/******************************************************************************
 * Name         : groupinst.c
 * Title        : Instruction grouping and other MOE setup
 * Created      : May 2005
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * $Log: groupinst.c $
 * 
 * Fixed assert on instructions partially writing an index register.
 *  --- Revision Logs Removed --- 
 * of 4.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"

#define MOE_OPERAND_DEST			(0)
#define MOE_OPERAND_DEST_MASK		(1U << MOE_OPERAND_DEST)
#define MOE_OPERAND_SRC0			(1)
#define MOE_OPERAND_SRC0_MASK		(1U << MOE_OPERAND_SRC0)
#define MOE_OPERAND_SRC1			(2)
#define MOE_OPERAND_SRC1_MASK		(1U << MOE_OPERAND_SRC1)
#define MOE_OPERAND_SRC2			(3)
#define MOE_OPERAND_SRC2_MASK		(1U << MOE_OPERAND_SRC2)
#define MOE_OPERAND_SOURCES_MASK	(MOE_OPERAND_SRC0_MASK | MOE_OPERAND_SRC1_MASK | MOE_OPERAND_SRC2_MASK)

typedef struct _SMLSI_STATE
{
	IMG_BOOL	pbInvalid[4];
	MOE_DATA	psMoeData[4];
} SMLSI_STATE, *PSMLSI_STATE;

typedef struct _SMBO_STATE
{
	IMG_BOOL	pbInvalidBaseOffsets[USC_MAX_MOE_OPERANDS];
	IMG_UINT32	puBaseOffsets[USC_MAX_MOE_OPERANDS];
} SMBO_STATE, *PSMBO_STATE;

/*
	Possible states for the MOE flags affecting whether U8/C10 
	or F16/F32 format selects are enabled for colour instructions
	and EFO instruction respectively.
*/
typedef enum
{
	FORMAT_CONTROL_FLAG_ON,
	FORMAT_CONTROL_FLAG_OFF,
	FORMAT_CONTROL_FLAG_UNDEFINED,
} FORMAT_CONTROL_FLAG;

/*
	Format select flags supported by the MOE. 
*/
typedef enum
{
	FORMAT_CONTROL_FLAG_TYPE_EFO = 0,
	FORMAT_CONTROL_FLAG_TYPE_COLOUR = 1,
	FORMAT_CONTROL_FLAG_TYPE_COUNT = 2,
} FORMAT_CONTROL_FLAG_TYPE;

typedef struct _SETFC_STATE
{
	FORMAT_CONTROL_FLAG_TYPE	aeFlag[FORMAT_CONTROL_FLAG_TYPE_COUNT];
} SETFC_STATE, *PSETFC_STATE;

/* USC_SMSLI_STATE_DEFAULT_VALUE: The default SMLSI state */
#define USC_SMSLI_STATE_DEFAULT_VALUE \
   { \
		{IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE}, 					/* pbInvalid */	\
		{USC_MOE_DATA_DEFAULT_VALUE, USC_MOE_DATA_DEFAULT_VALUE,		/* psMoeData */	\
				USC_MOE_DATA_DEFAULT_VALUE, USC_MOE_DATA_DEFAULT_VALUE}, \
   }

static const SMLSI_STATE sDefaultSmsliState = USC_SMSLI_STATE_DEFAULT_VALUE;

/*
   DEFAULT_GROUP_TRIGGER: The number of instructions normally needed to form a group.
 */
#define DEFAULT_GROUP_TRIGGER (3)

/*
   MIN_GROUP_TRIGGER: The absolute minimum number of instructions needed to form a
   group.
 */
#define MIN_GROUP_TRIGGER (2)

/*
   USC_MAX_SMALL_INCREMENT_INC: Maximum increment when using reduced grouping
   (selected by cli option -mingroup).
*/
#define USC_MAX_SMALL_INCREMENT_INC (1)

/*
   USC_MIN_SMALL_INCREMENT_INC: Minimum increment when using reduced grouping.
   (selected by cli option -mingroup).
*/
#define USC_MIN_SMALL_INCREMENT_INC (0)

/*
  GROUPINST_STATE: Instruction grouping data.
*/
typedef struct _GROUPINST_FUNCSTATE
{
	/* TRUE if a call to the function has been encountered in the program so far. */
	IMG_BOOL		bUsed;
	/* Accumulated MOE state at the start of the function. */
	IMG_PVOID		pvInitialState;
} GROUPINST_FUNCSTATE, *PGROUPINST_FUNCSTATE;

typedef struct _GROUPINST_STATE
{
	/* Points to state for each function. */
	PGROUPINST_FUNCSTATE	asFunc;

	/* asInstGroupData: Instructions linked by opcodes */
	PINST asInstGroup[IOPCODE_MAX];

	/* UseDef record */
	USEDEF_RECORD sUseDef;
} GROUPINST_STATE, *PGROUPINST_STATE;

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef struct
{
	/*
		If TRUE this is a repeat of dotproduct (VDP_CP/VDP_GPI) instructions.
	*/
	IMG_BOOL		bDotProduct;
	/*
		Increment mode to use for the repeat.
	*/
	SVEC_REPEATMODE	eRepeatMode;
} SINGLEISSUE_VECTOR_STATE, *PSINGLEISSUE_VECTOR_STATE;
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

typedef struct
{
	/*
		If TRUE this instruction can use MOE increments
		directly specified in the instruction encoding.
	*/
	IMG_BOOL	bInstMoeIncrement;
	/*
		If TRUE this instruction only uses MOE increments
		from the instruction encoding.
	*/
	IMG_BOOL	bNeverUseMoe;

	/*
		Mode and increment/swizzle needed to group
		each operand.
	*/
	MOE_DATA	psMoeData[USC_MAX_MOE_OPERANDS];
	/*
		Set to TRUE if each instruction in the group
		has a different predicate.
	*/
	IMG_BOOL	bPredModN;
	/*
		Set to TRUE if the instruction is a dotproduct.
	*/
	IMG_BOOL	bDotProduct;
	/*
		Set to TRUE if the instruction is a double dotproduct.
	*/
	IMG_BOOL	bDoubleDotProduct;

	/*
		First instruction in the repeat.
	*/
	PINST		psFirstInst;
	/*
		Last instruction in the repeat.
	*/
	PINST		psLastInst;

	/*
		Count of the operands (including the destination)
		used by instructions in the repeat.
	*/
	IMG_UINT32	uArgCount;
	/*
		Bit mask of operands not used by the instructions
		in the repeat. Indexed by instruction operand index.
	*/
	IMG_UINT32	uDontCareMask;
	/*
		Bit mask of operands which are in the special
		constant bank and point to the start of a
		run of constants with the same value.
	*/
	IMG_UINT32	uSpecialConstMask;
	/*
		Length of the run of constants with the
		same value.
	*/
	IMG_UINT32	uSpecialConstGroupLimit;

	/*
		Offset to use for the dotproduct destination
		in a repeated instruction.
	*/
	IMG_UINT32	uNewDPDestNum;

	/*
		Count of instructions in the repeat.
	*/
	IMG_UINT32	uInstCount;

	/*
		A pointer to each instruction in the repeat.
	*/
	PINST		apsInst[EURASIA_USE_MAXIMUM_REPEAT];

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		TRUE if this repeat is for single-issue vector
		instruction.
	*/
	IMG_BOOL					bSVecInst;

	/*
		Increment data that will be only used for repeats 
		that are not using moe mode.
	*/
	MOE_DATA	psIncData[USC_MAX_MOE_OPERANDS];

	/*
		State specific to making repeats from the single-issue vector
		instructions.
	*/
	SINGLEISSUE_VECTOR_STATE	sVec;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} CANDIDATE_GROUP_DATA, *PCANDIDATE_GROUP_DATA;

IMG_INTERNAL
IMG_BOOL TypeUsesFormatControl(IMG_UINT32		uType)
/*****************************************************************************
 FUNCTION	: TypeUsesFormatControl

 PURPOSE	: Checks if a register is one that is affected by the format control.

 PARAMETERS	: uType		- The type to check.

 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_TEMP ||
		uType == USEASM_REGTYPE_PRIMATTR ||
		uType == USEASM_REGTYPE_SECATTR ||
		uType == USEASM_REGTYPE_OUTPUT ||
		uType == USEASM_REGTYPE_FPINTERNAL ||
		uType == USC_REGTYPE_REGARRAY)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL GetMOEDestIdx(PINST psInst)
/*****************************************************************************
 FUNCTION	: GetMOEDestIdx

 PURPOSE	: Check for an instruction not affected by the destination MOE
			  state.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uMOEDestIdx;
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0)
	{
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
		{
			uMOEDestIdx = VTEST_UNIFIEDSTORE_DESTIDX;
		}
		else
		{
			uMOEDestIdx = TEST_UNIFIEDSTORE_DESTIDX;
		}
	}
	else
	{
		uMOEDestIdx = 0;
	}
	if (uMOEDestIdx >= psInst->uDestCount)
	{
		return USC_UNDEF;
	}
	return uMOEDestIdx;
}

static
IMG_BOOL HasMOEDest(PINST psInst)
/*****************************************************************************
 FUNCTION	: HasMOEDest

 PURPOSE	: Check for an instruction not affected by the destination MOE
			  state.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (GetMOEDestIdx(psInst) != USC_UNDEF) ? IMG_TRUE : IMG_FALSE;
}

static IMG_BOOL Src0AliasesDest(PINTERMEDIATE_STATE		psState,
								PINST					psInst)
/*****************************************************************************
 FUNCTION	: Src0AliasesDest

 PURPOSE	: Check for an instruction where source 0 and the destination
			  must use the same MOE state.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDestMask;

	PVR_UNREFERENCED_PARAMETER(psState);

	/*
		No aliasing for an instruction with no destinations.
	*/
	if (!HasMOEDest(psInst))
	{
		return IMG_FALSE;
	}

	#if defined(OUTPUT_USPBIN)
	/*
		The USP will handle masking for these texture sample instructions
		when it expands them.
	*/
	if	(
			(
				(psInst->eOpcode >= ISMP_USP) &&
				(psInst->eOpcode <= ISMP_USP_NDR)
			) ||  
			(psInst->eOpcode == ISMPUNPACK_USP)
		)
	{
		return IMG_FALSE;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		This instruction supports masking on the destination without requiring the MOE state for
		the destination and source 0 to be the same.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKNOSRC0) != 0)
	{
		return IMG_FALSE;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			if (psInst->auDestMask[uDestIdx] != 0 && psInst->auDestMask[uDestIdx] != USC_ALL_CHAN_MASK)
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	uDestMask = psInst->auDestMask[0];
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST)
	{
		ASSERT(psInst->uDestCount == 1 || psInst->uDestCount == 2);

		if (psInst->uDestCount > 1 && (psInst->auDestMask[1] & USC_RED_CHAN_MASK) != 0)
		{
			uDestMask |= USC_ALPHA_CHAN_MASK;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	return (uDestMask != USC_ALL_CHAN_MASK) ? IMG_TRUE : IMG_FALSE;
}

static IMG_BOOL ConvertIncrementToSwizzle(PINTERMEDIATE_STATE	psState,
										  PMOE_DATA				psMoeData,
										  IMG_UINT32			uGroupCount)
/*****************************************************************************
 FUNCTION	: ConvertIncrementToSwizzle

 PURPOSE	: Check if an argument which has been repeated using MOE increment
			  mode to MOE swizzle mode.

 PARAMETERS	: psState		- Compiler state.
			  psMoeData		- MOE state.
			  uGroupCount	- Count of instructions in the repeat.

 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	IMG_INT32	iIncrement;
	IMG_UINT32	uIteration;

	ASSERT(psMoeData->eOperandMode == MOE_INCREMENT);

	iIncrement = psMoeData->u.iIncrement;

	/*
		Check if the existing increments can be represented using
		swizzle mode.
	*/
	if (uGroupCount >= USC_MAX_SWIZZLE_SLOTS)
	{
		return IMG_FALSE;
	}
	if ((labs(iIncrement) * uGroupCount) > USC_MAX_SWIZZLE_INC)
	{
		return IMG_FALSE;
	}

	/*
		Fill out the swizzle entries from the increment.
	*/
	psMoeData->eOperandMode = MOE_SWIZZLE;
	if (iIncrement >= 0)
	{
		for (uIteration = 0; uIteration <= uGroupCount; uIteration++)
		{
			psMoeData->u.s.auSwizzle[uIteration] = iIncrement * uIteration;
		}
		psMoeData->u.s.uLargest = iIncrement * uGroupCount;
	}
	else
	{
		for (uIteration = 0; uIteration <= uGroupCount; uIteration++)
		{
			psMoeData->u.s.auSwizzle[uIteration] = (-iIncrement) * (uGroupCount - uIteration);
		}
		psMoeData->u.s.uLargest = (-iIncrement) * uGroupCount;
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckArgSwizzleMode(IMG_UINT32	uFirstRegNumber,
									IMG_UINT32	uNextRegNumber,
									IMG_UINT32	uIterationIndex,
									PMOE_DATA	psMoeData)
/*****************************************************************************
 FUNCTION	: CheckArgSwizzleMode

 PURPOSE	: Check if an instruction argument could be included in an existing
			  repeat which is already using MOE swizzle mode.

 PARAMETERS	: uFirstRegNumber	- Register number for the argument to the
								first instruction.
			  uNextRegNumber	- Register number for the argument to the
								instruction we are trying to add to the
								repeat.
			  uIterationIndex	- Index of the MOE iteration to check for.
			  psMoeData			- Existing MOE state for the argument.

 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	IMG_INT32 iJump;
	IMG_UINT32 uPrevOffset;
	IMG_UINT32 uPrevLargest;
	IMG_UINT32 uNewLargest;
	IMG_UINT32 uNewSwizzle;
	IMG_UINT32 uBaseRegister;
	IMG_UINT32 uNewBaseReg;
	IMG_UINT32 uNewOffset;
	IMG_UINT32 uOffsetDiff;
	IMG_UINT32 uAbsJump;
	IMG_UINT32 i;

	/* First operand is in swizzle mode */

	/* Get the distance between the previous and the current instructions */
	iJump = uNextRegNumber - uFirstRegNumber;

	/* The previous largest increment, from the last swizzle value */
	uPrevLargest = psMoeData->u.s.uLargest;

	/* The previous offset from the first swizzle value (at index 0) */
	uPrevOffset = psMoeData->u.s.auSwizzle[0];

	/* Calculate the base register from the previous offset */
	uBaseRegister = uFirstRegNumber - uPrevOffset;

	/*
	   The non-negative jump (the distance from the first to the current register)
	*/
	uAbsJump = (iJump < 0) ? -iJump : iJump;

	/* Make sure there is a slot available */
	if (uIterationIndex >= USC_MAX_SWIZZLE_SLOTS)
	{
		IMG_UINT32	uSlot;
		IMG_UINT32	uSwizzleSel;

		/*
			We are reusing a slot that is already used by
			a previous iteration so the swizzle selections
			match for both iterations.
		*/
		if (uNextRegNumber < uBaseRegister)
		{
			return IMG_FALSE;
		}
		uSlot = uIterationIndex % USC_MAX_SWIZZLE_SLOTS;
		uSwizzleSel = uNextRegNumber - uBaseRegister;
		if (psMoeData->u.s.auSwizzle[uSlot] != uSwizzleSel)
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}

	/*
	   The new base register and difference to be added to
	   the swizzle values
	*/
	if (uNextRegNumber < uBaseRegister)
	{
		uNewBaseReg = uNextRegNumber;
		uNewOffset = uAbsJump;
		uOffsetDiff = uNewOffset - uPrevOffset;
	}
	else
	{
		uNewBaseReg = uBaseRegister;
		uNewOffset = uPrevOffset;
		uOffsetDiff = 0;
	}

	/* The new swizzle */
	uNewSwizzle = uNextRegNumber - uNewBaseReg;

	/* The new largest swizzle */
	uNewLargest = uPrevLargest + uOffsetDiff;
	if (uNewLargest < uNewSwizzle)
	{
		uNewLargest = uNewSwizzle;
	}

	/* Test whether swizzle mode could continue */
	if (USC_MIN_SWIZZLE_INC <= (IMG_INT32)uNewOffset &&
		(IMG_INT32)uNewOffset <= USC_MAX_SWIZZLE_INC &&
		USC_MIN_SWIZZLE_INC <= (IMG_INT32)uNewLargest &&
		(IMG_INT32)uNewLargest <= USC_MAX_SWIZZLE_INC)
	{
		/* Swizzlable */

		/*
		   Store the largest distance.
		*/
		psMoeData->u.s.uLargest = uNewLargest;

		if (uNewOffset != uPrevOffset)
		{
			/* Rebuild the swizzle values array */
			for (i = 0; i < uIterationIndex; i++)
			{
				/* Get the old swizzle increment */
				psMoeData->u.s.auSwizzle[i] += uOffsetDiff;
			}
		}

		/* Store the last swizzle */
		psMoeData->u.s.auSwizzle[uIterationIndex] = uNewSwizzle;

		/* Can include this instruction argument in the group. */
		return IMG_TRUE;
	}
	else
	{
		/* Unswizzlable */
		return IMG_FALSE;
	}
}

static IMG_BOOL CheckArg(PINTERMEDIATE_STATE psState,
						 PCANDIDATE_GROUP_DATA psGroup,
						 PARG psFirstArg,
						 PARG psNextArg,
						 IMG_UINT32 uArg,
						 IMG_UINT32 uIterationIndex,
						 PMOE_DATA  psMoeData)
/*****************************************************************************
 FUNCTION	: CheckArg

 PURPOSE	: Check if two instruction arguments could be grouped together.

 PARAMETERS	: psFirstInst				- First instruction in the group
			  psFirstArg				- The argument from the first instruction in the group
			  psNextArg					- The argument from the candidate instruction.
              uArg                      - The index of the argument (0: dest, 1..n: src0..srcn)
			  uIterationIndex			-
 INPUT/OUTPUT:
			  psMoeData       			- MOE data for instructions already grouped.
 OUTPUT:
              pbSwizzle                 - Whether an operand switches to swizzle mode.

 RETURNS	: IMG_TRUE if the arguments could be grouped; IMG_FALSE if they couldn't.
*****************************************************************************/
{
	IMG_INT32	iMinIncrement, iMaxIncrement;
	IMG_INT32	iMinSwizzle, iMaxSwizzle;
	IMG_UINT32	uFirstRegNum, uNextRegNum;

	/* Instructions with different arguments can't be grouped. */
	if (psNextArg->uType != psFirstArg->uType ||
		psNextArg->uIndexType != psFirstArg->uIndexType ||
		psNextArg->uIndexNumber != psFirstArg->uIndexNumber ||
		(psNextArg->eFmt != psFirstArg->eFmt))
	{
		return IMG_FALSE;
	}

	uFirstRegNum = psFirstArg->uNumber;
	uNextRegNum = psNextArg->uNumber;

	/*
		Immediate arguments aren't affected by the MOE increments.
		Index registers used as destinations aren't affected by MOE increments
	*/
	if (psFirstArg->uType == USEASM_REGTYPE_IMMEDIATE ||
		(psFirstArg->uType == USEASM_REGTYPE_INDEX && uArg == 0))
	{
		if (uFirstRegNum != uNextRegNum)
		{
			return IMG_FALSE;
		}
		psMoeData->eOperandMode = MOE_INCREMENT;
		psMoeData->u.iIncrement = 0;

		return IMG_TRUE;
	}

	/* Set the minimum and maximum increments and swizzles for each chip */
	if (psGroup->bInstMoeIncrement)
	{
		/* SGX545 increments and swizzles */
		iMinIncrement = USC_MIN_SMALL_INCREMENT_INC;
		iMaxIncrement = USC_MAX_SMALL_INCREMENT_INC;

		iMinSwizzle = USC_MIN_SWIZZLE_INC;
		iMaxSwizzle = USC_MAX_SWIZZLE_INC;
	}
	else
	{
		/* Other chip increments and swizzles */
		iMinIncrement = USC_MIN_INCREMENT_INC;
		iMaxIncrement = USC_MAX_INCREMENT_INC;

		iMinSwizzle = USC_MIN_SWIZZLE_INC;
		iMaxSwizzle = USC_MAX_SWIZZLE_INC;
	}

	/*
	   Two instructions which have all their arguments of
	   the same type can always be grouped by changing the
	   macro operation expander increments or swizzles.
	*/
	if (uIterationIndex == 1)
	{
		/*
		   First and second instructions in the group.
		   Set mode to increment.
		*/
		psMoeData->eOperandMode = MOE_INCREMENT;
		psMoeData->u.iIncrement = uNextRegNum - uFirstRegNum;

		/*
			Check for an increment outside the ranges supported by the hardware.
		*/
		if (psMoeData->u.iIncrement < iMinIncrement ||
			psMoeData->u.iIncrement > iMaxIncrement)
		{
			IMG_INT32 iIncr = psMoeData->u.iIncrement;
			IMG_UINT32 uAbsIncr;
			uAbsIncr = (iIncr < 0) ? -iIncr : iIncr;

			/* Check for a swizzle outside the range */
			if (uAbsIncr < (IMG_UINT32)iMinSwizzle ||
				uAbsIncr > (IMG_UINT32)iMaxSwizzle)
			{
				return IMG_FALSE;
			}
			else
			{
				IMG_UINT32 uSwizzle1, uSwizzle2;
				IMG_UINT32 uLargest;

				/* Switch to swizzle mode */
				psMoeData->eOperandMode = MOE_SWIZZLE;

				/* Work out the swizzles */
				if (iIncr < 0)
				{
					/* Negative swizzle */
					uSwizzle1 = uAbsIncr;
					uSwizzle2 = 0;
				}
				else
				{
					/* Positive swizzle */
					uSwizzle1 = 0;
					uSwizzle2 = uAbsIncr;
				}
				psMoeData->u.s.auSwizzle[0] = uSwizzle1;
				psMoeData->u.s.auSwizzle[1] = uSwizzle2;

				uLargest = max(uSwizzle1, uSwizzle2);
				psMoeData->u.s.uLargest = uLargest;
			}
		}
	}
	else
	{
		/* Instructions after the first two in the group */

		/* Get the distance between the previous and the current instructions */
		IMG_INT32 iJump = uNextRegNum - uFirstRegNum;

		switch (psMoeData->eOperandMode)
		{
			case MOE_INCREMENT:
			{
				/* First operand is in increment mode */
				if (iJump != ((IMG_INT32)uIterationIndex * psMoeData->u.iIncrement))
				{
					/*
					   Increments differ so can't continue this group in
					   increment mode, try to switch to swizzle.
					*/
					if (!ConvertIncrementToSwizzle(psState, psMoeData, uIterationIndex - 1))
					{
						return IMG_FALSE;
					}
					if (!CheckArgSwizzleMode(uFirstRegNum, uNextRegNum, uIterationIndex, psMoeData))
					{
						/* Can't switch to swizzle mode so end the group */
						return IMG_FALSE;
					}
				}
				break;
			}
			case MOE_SWIZZLE:
			{
				if (!CheckArgSwizzleMode(uFirstRegNum, uNextRegNum, uIterationIndex, psMoeData))
				{
					/*
						Can't add this instruction to the group.
					*/
					return IMG_FALSE;
				}
				break;
			}
			default:
			{
				/*
				   Operand is not in increment or swizzle mode.
				   End grouping.
				*/
				return IMG_FALSE;
			}
		}
	}

	/*
	   The instructions group.
	*/
	return IMG_TRUE;
}

static IMG_BOOL IsDoubleDotProduct(PINST psInst, IMG_PBOOL pbLast, IMG_BOOL bRotateArgs)
/*****************************************************************************
 FUNCTION	: IsDoubleDotProduct

 PURPOSE	: Is this instruction part of a double dot-product.

 PARAMETERS	: psInst		- Instruction to check.
			  pbLast		- Set to TRUE if this is the last instruction.

 RETURNS	: True or false.
*****************************************************************************/
{
	if (!bRotateArgs &&
		!(psInst->u.psEfo->eM0Src0 == SRC0 &&
		  psInst->u.psEfo->eM0Src1 == SRC1 &&
		  psInst->u.psEfo->eM1Src0 == SRC0 &&
		  psInst->u.psEfo->eM1Src1 == SRC2))
	{
		return IMG_FALSE;
	}
	if (bRotateArgs &&
		!(psInst->u.psEfo->eM0Src0 == SRC0 &&
		  psInst->u.psEfo->eM0Src1 == SRC2 &&
		  psInst->u.psEfo->eM1Src0 == SRC1 &&
		  psInst->u.psEfo->eM1Src1 == SRC2))
	{
		return IMG_FALSE;
	}
	if (!(psInst->u.psEfo->eA0Src0 == M0 &&
		  psInst->u.psEfo->eA0Src1 == I0 &&
		  psInst->u.psEfo->eA1Src0 == I1 &&
		  psInst->u.psEfo->eA1Src1 == M1))
	{
		return IMG_FALSE;
	}
	if (psInst->u.psEfo->bA0RightNeg || psInst->u.psEfo->bA1LeftNeg)
	{
		return IMG_FALSE;
	}
	if (psInst->u.psEfo->bWriteI0 &&
		psInst->u.psEfo->bWriteI1 &&
		psInst->u.psEfo->eI0Src == A0 &&
		psInst->u.psEfo->eI1Src == A1 &&
		psInst->u.psEfo->bIgnoreDest)
	{
		return IMG_TRUE;
	}
	if (!psInst->u.psEfo->bIgnoreDest &&
		!(psInst->u.psEfo->bWriteI0 && psInst->u.psEfo->bWriteI1) &&
		(psInst->u.psEfo->bWriteI0 || psInst->u.psEfo->bWriteI1) &&
		(psInst->u.psEfo->eDSrc == A0 || psInst->u.psEfo->eDSrc == A1))
	{
		EFO_SRC	eOtherSrc = psInst->u.psEfo->bWriteI0 ? psInst->u.psEfo->eI0Src : psInst->u.psEfo->eI1Src;

		if ((eOtherSrc == A0 || eOtherSrc == A1) && (psInst->u.psEfo->eDSrc != eOtherSrc))
		{
			*pbLast = IMG_TRUE;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckPredicateSource(PINTERMEDIATE_STATE	psState,
									 PCANDIDATE_GROUP_DATA	psGroup, 
									 PINST					psFirstInst, 
									 PINST					psNextInst, 
									 IMG_UINT32				uIterationIndex)
/*****************************************************************************
 FUNCTION	: CheckPredicateSource

 PURPOSE	: Check whether two instructions have compatible source predicates
			  allowing them both to be part of the same repeat.

 PARAMETERS	: psState		- Compiler state.
			  psGroup		- Information about the repeat. If this is the
							first instruction to be added then bPredNMod4 flag
							is set up.
			  psFirstInst	- First instruction in the repeat.
			  psNextInst	- Subsequent instruction in the repeat.
			  uIterationIndex
							- Index of the subsequent instruction.

 RETURNS	: TRUE if the instructions can be part of a repeat.
*****************************************************************************/
{
	IMG_UINT32	uFirstInstPredSrc, uNextInstPredSrc;
	IMG_BOOL	bFirstInstPredNeg, bNextInstPredNeg;
	IMG_BOOL	bFirstInstPredPerChan, bNextInstPredPerChan;

	if (psFirstInst->uPredCount != psNextInst->uPredCount)
	{
		return IMG_FALSE;
	}

	if (psFirstInst->uPredCount == 0)
	{
		return IMG_TRUE;
	}

	GetPredicate(psFirstInst, &uFirstInstPredSrc, &bFirstInstPredNeg, &bFirstInstPredPerChan, 0);
	GetPredicate(psNextInst, &uNextInstPredSrc, &bNextInstPredNeg, &bNextInstPredPerChan, 0);

	if (bFirstInstPredPerChan || bNextInstPredPerChan)
	{
		return IMG_FALSE;
	}

	ASSERT(psFirstInst->uPredCount == 1 && psNextInst->uPredCount == 1);
	
	if (bFirstInstPredNeg != bNextInstPredNeg)
	{
		return IMG_FALSE;
	}

	if (uIterationIndex == 1)
	{
		/*
			For the first repeat set the predicate mode - either all the predicates must be equal or
			the predicate numbers must equal the iteration number mod 4.
		*/
		if (uNextInstPredSrc == uFirstInstPredSrc)
		{
			psGroup->bPredModN = IMG_FALSE;
		}
		else if (!GetBit(psFirstInst->auFlag, INST_PRED_NEG) &&
				 uFirstInstPredSrc == 0 &&
				 uNextInstPredSrc == 1 &&
				 GetInstPredicateSupport(psState, psFirstInst) == INST_PRED_EXTENDED)
		{
			psGroup->bPredModN = IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Check that this instruction uses the same predicate modes as the previous ones.
		*/
		if (psGroup->bPredModN)
		{
			if (uNextInstPredSrc != (uIterationIndex % 4))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if (uNextInstPredSrc != uFirstInstPredSrc)
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL GetVDPSingleDestinationChannel(PINTERMEDIATE_STATE	psState,
										PINST				psInst,
										IMG_PUINT32			puWrittenChannel)
/*****************************************************************************
 FUNCTION	: GetVDPSingleDestinationChannel

 PURPOSE	: Get the single channel (if there is just one) written by a
			  single-issue vector dotproduct instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  puWrittenChannel
							- Returns the index of the channel written in
							the array of instruction destinations.

 RETURNS	: TRUE if the instruction only writes one channel; FALSE if
			  it writes more than one.
*****************************************************************************/
{
	IMG_UINT32	uWrittenChannel;
	IMG_UINT32	uDestIdx;

	/*
		Look for an instruction 
	*/
	uWrittenChannel = USC_UNDEF;
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->auDestMask[uDestIdx] != 0)
		{
			/*
				Instructions which are part of a repeat can
				only write one channel.
			*/
			if (uWrittenChannel != USC_UNDEF)
			{
				return IMG_FALSE;
			}
			uWrittenChannel = uDestIdx;
		}
	}
	ASSERT(uWrittenChannel != USC_UNDEF);

	*puWrittenChannel = uWrittenChannel;
	return IMG_TRUE;
}

static IMG_BOOL SetupVDPInstGroup(PINTERMEDIATE_STATE	psState,
								  PCANDIDATE_GROUP_DATA	psGroup,
								  PINST					psFirstInst)
/*****************************************************************************
 FUNCTION	: SetupVDPInstGroup

 PURPOSE	: Initializes information specific to making a repeated instruction
			  from single-issue dotproducts.

 PARAMETERS	: psState		- Compiler state.
			  psGroup		- Information about the repeated instruction.
			  psFirstInst	- First instruction in the repeat.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	/*
		Check the first instruction is writing the first channel only in
		its vector destination.
	*/
	if (!(GetVDPSingleDestinationChannel(psState, psFirstInst, &uDestIdx) && uDestIdx == 0))
	{
		return IMG_FALSE;
	}

	/*
		Set default repeat mode.
	*/
	psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_USEMOE;

	/*
		A repeat formed from dotproduct instructions.
	*/
	psGroup->sVec.bDotProduct = IMG_TRUE;

	return IMG_TRUE;
}

static IMG_VOID SetupVMADInstGroup(PCANDIDATE_GROUP_DATA	psGroup)
/*****************************************************************************
 FUNCTION	: SetupMADInstGroup

 PURPOSE	: Initializes information specific to making a repeated instruction
			  from single-issue vector mad.

 PARAMETERS	: psGroup		- Information about the repeated instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Set default repeat mode.
	*/
	psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_USEMOE;

	/*
		Not a repeat formed from dotproduct instructions.
	*/
	psGroup->sVec.bDotProduct = IMG_FALSE;
}

static IMG_BOOL CheckInstGroupVDP_NonMOEDest(PINTERMEDIATE_STATE	psState,
											 PINST					psFirstInst,
											 PINST					psNextInst,
											 IMG_UINT32				uIterationIndex)
/*****************************************************************************
 FUNCTION	: CheckInstGroupVDP_NonMOEDest

 PURPOSE	: Checks if an instruction's destination is suitable for adding to
			  repeated single-issue vector instruction using non-MOE mode.

 PARAMETERS	: psState		- Compiler state.
			  psFirstInst	- First instruction in the repeat.
			  psNextInst	- Instruction we are trying to add to the repeat.
			  uIterationIndex
							- Iteration to add the instruction at.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uWrittenChannel;
	PARG		psFirstDest = &psFirstInst->asDest[0];
	PARG		psNextDest = &psNextInst->asDest[0];
	IMG_UINT32	uChannelDifference;

	/*
		Check the new instruction is writing only a single channel in its vector
		destination and get the index of the written channel.
	*/
	if (!GetVDPSingleDestinationChannel(psState, psNextInst, &uWrittenChannel))
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions are writing the same type of destination.
	*/
	if (psFirstDest->uType != psNextDest->uType)
	{
		return IMG_FALSE;
	}

	/*
		Check the channels written by each iteration are consecutive 32-bit channels of
		an 128-bit  vector.
	*/
	if (psFirstDest->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		/*
			For GPI registers the register number is in 128-bit units.
		*/
		if (psFirstDest->uNumber != psNextDest->uNumber)
		{
			return IMG_FALSE;
		}
		uChannelDifference = uWrittenChannel;
	}
	else
	{
		IMG_UINT32	uChannelBase;

		/*
			For other register types the register nubmer is in 64-bit units.
		*/
		if (psFirstDest->uNumber > psNextDest->uNumber)
		{
			return IMG_FALSE;
		}
		uChannelBase = (psNextDest->uNumber - psFirstDest->uNumber) * (QWORD_SIZE / LONG_SIZE);
		uChannelDifference = uChannelBase + uWrittenChannel;
	}

	/*
		The channel written must the same as the current iteration.
	*/
	if (uChannelDifference != uIterationIndex)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckInstGroupVDP(PINTERMEDIATE_STATE	psState,
								  PCANDIDATE_GROUP_DATA	psGroup,
								  PINST					psFirstInst,
								  PINST					psNextInst,
								  IMG_UINT32			uIterationIndex,
								  PMOE_DATA				psIncData)
/*****************************************************************************
 FUNCTION	: CheckInstGroupVDP

 PURPOSE	: Checks if an instruction is suitable for adding to
			  repeated single-issue vector instruction.

 PARAMETERS	: psState		- Compiler state.
			  psGroup		- Information about the repeated instruction.
			  psFirstInst	- First instruction in the repeat.
			  psNextInst	- Instruction we are trying to add to the repeat.
			  uIterationIndex
							- Iteration to add the instruction at.
			  psIncData		- MOE state needed to group the operands.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSrcIdx;

	/*
		Check both instructions use the same opcode.
	*/
	if (psNextInst->eOpcode != psFirstInst->eOpcode)
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions use the same predicate.
	*/
	if (!CheckPredicateSource(psState, psGroup, psFirstInst, psNextInst, uIterationIndex))
	{
		return IMG_FALSE;
	}

	/*
		Get both instructions uses the same SKIPINVALID flag.
	*/
	if (GetBit(psFirstInst->auFlag, INST_SKIPINV) != GetBit(psNextInst->auFlag, INST_SKIPINV))
	{
		return IMG_FALSE;
	}

	/*
		Check if we can add the instruction to this repeat using non-MOE mode for the
		destination.
	*/
	if (!CheckInstGroupVDP_NonMOEDest(psState, psFirstInst, psNextInst, uIterationIndex))
	{
		return IMG_FALSE;
	}

	/*
		For VDP_CP check the difference in the written clipplane P-flag is the same
		as the iteration.
	*/
	if (psFirstInst->eOpcode == IVDP_CP)
	{
		if ((psFirstInst->u.psVec->uClipPlaneOutput + uIterationIndex) != psNextInst->u.psVec->uClipPlaneOutput)
		{
			return IMG_FALSE;
		}
	}

	for (uSrcIdx = 0; uSrcIdx < 2; uSrcIdx++)
	{
		IMG_UINT32	uVectorBaseArg = VEC_MOESOURCE_ARGINDEX(uSrcIdx);

		/*
			Check both instructions are using the same swizzles.
		*/
		if (psNextInst->u.psVec->auSwizzle[uSrcIdx] != psFirstInst->u.psVec->auSwizzle[uSrcIdx])
		{
			return IMG_FALSE;
		}
		/*
			Check both instructions are using the same source modifiers.
		*/
		if (psNextInst->u.psVec->asSrcMod[uSrcIdx].bNegate != psFirstInst->u.psVec->asSrcMod[uSrcIdx].bNegate ||
			psNextInst->u.psVec->asSrcMod[uSrcIdx].bAbs != psFirstInst->u.psVec->asSrcMod[uSrcIdx].bAbs)
		{
			return IMG_FALSE;
		}
		if (!CheckArg(psState,
					  psGroup,
					  &psFirstInst->asArg[uVectorBaseArg],
					  &psNextInst->asArg[uVectorBaseArg],
					  1 + uSrcIdx /* uArg */,
					  uIterationIndex,
					  &psIncData[1 + uSrcIdx]))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckInstGroupVMAD(PINTERMEDIATE_STATE		psState, 
								   PCANDIDATE_GROUP_DATA	psGroup, 
								   PINST					psFirstInst, 
								   PINST					psNextInst, 
								   IMG_UINT32				uIterationIndex, 
								   PMOE_DATA				psIncData)
/*****************************************************************************
 FUNCTION	: CheckInstGroupVMAD

 PURPOSE	: Checks if an instruction is suitable for adding to
			  repeated single-issue vector instruction.

 PARAMETERS	: psState		- Compiler state.
			  psGroup		- Information about the repeated instruction.
			  psFirstInst	- First instruction in the repeat.
			  psNextInst	- Instruction we are trying to add to the repeat.
			  uIterationIndex
							- Iteration to add the instruction at.
			  psIncData		- MOE state needed to group the operands.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSrcIdx;
	IMG_UINT32	uSrcCount;
	IMG_UINT32	uDestIdx;

	/*
		Check both instructions use the same opcode.
	*/
	if (psNextInst->eOpcode != psFirstInst->eOpcode)
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions use the same predicate.
	*/
	if (!CheckPredicateSource(psState, psGroup, psFirstInst, psNextInst, uIterationIndex))
	{
		return IMG_FALSE;
	}

	/*
		Get both instructions uses the same SKIPINVALID flag.
	*/
	if (GetBit(psFirstInst->auFlag, INST_SKIPINV) != GetBit(psNextInst->auFlag, INST_SKIPINV))
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions have the same destination masks.
	*/
	if (psFirstInst->uDestCount != psNextInst->uDestCount)
	{
		return IMG_FALSE;
	}
	for (uDestIdx = 0; uDestIdx < psFirstInst->uDestCount; uDestIdx++)
	{
		if (psFirstInst->auDestMask[uDestIdx] != psNextInst->auDestMask[uDestIdx])
		{
			return IMG_FALSE;
		}
	}

	/*
		Check whether the destination register number for the new
		instruction can be added to the repeat.
	*/
	if (!CheckArg(psState,
				  psGroup,
				  &psFirstInst->asDest[0],
				  &psNextInst->asDest[0],
				  0 /* uArg */,
				  uIterationIndex,
				  &psIncData[0]))
	{
		return IMG_FALSE;
	}

	if (psFirstInst->eOpcode == IVMAD4)
	{
		uSrcCount = 3;
	}
	else
	{
		ASSERT(psFirstInst->eOpcode == IVMUL3 || psFirstInst->eOpcode == IVADD3);
		uSrcCount = 2;
	}

	for (uSrcIdx = 0; uSrcIdx < uSrcCount; uSrcIdx++)
	{
		IMG_UINT32	uVectorBaseArg = VEC_MOESOURCE_ARGINDEX(uSrcIdx);

		/*
			Check both instructions are using the same swizzles.
		*/
		if (psNextInst->u.psVec->auSwizzle[uSrcIdx] != psFirstInst->u.psVec->auSwizzle[uSrcIdx])
		{
			return IMG_FALSE;
		}
		/*
			Check both instructions are using the same source modifiers.
		*/
		if (psNextInst->u.psVec->asSrcMod[uSrcIdx].bNegate != psFirstInst->u.psVec->asSrcMod[uSrcIdx].bNegate ||
			psNextInst->u.psVec->asSrcMod[uSrcIdx].bAbs != psFirstInst->u.psVec->asSrcMod[uSrcIdx].bAbs)
		{
			return IMG_FALSE;
		}
		if (!CheckArg(psState,
					  psGroup,
					  &psFirstInst->asArg[uVectorBaseArg],
					  &psNextInst->asArg[uVectorBaseArg],
					  1 + uSrcIdx /* uArg */,
					  uIterationIndex, 
					  &psIncData[1 + uSrcIdx]))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL TryAddToGroup(PINTERMEDIATE_STATE	psState,
							  PCANDIDATE_GROUP_DATA	psGroup,
							  IMG_UINT32			uIterationIndex,
							  PINST					psFirstInst,
							  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CheckInstGroup

 PURPOSE	: Check if two instructions could be grouped together.

 PARAMETERS	: psState					- The compiler state
			  psGroup					- Information about the group. On success the
										MOE state required for the group is updated.
			  psNextInst				- The candidate instruction to be checked
			  uIterationIndex			- MOE iteration number for the candidate instruction.
              

 RETURNS	: IMG_TRUE if the instructions could be grouped; IMG_FALSE if they couldn't.

 NOTES		: If swizzle mode is selected for an operand r, the swizzle array is
			  is constructed for a base register which is chosen as the smallest
			  register in the sequence. The first value v in the swizzle array is
              the offset which must be removed from the first register r to calculate
			  the number of the base register (r-v).
*****************************************************************************/
{
	IMG_UINT32 i;
	IMG_UINT32 uMoeNumSrcArgs = g_psInstDesc[psFirstInst->eOpcode].uMoeArgumentCount;
	IMG_UINT32 uNumSrcArgs = psFirstInst->uArgumentCount;
	PMOE_DATA psMoeData = psGroup->psMoeData;
	MOE_DATA asNewMoeData[USC_MAX_MOE_OPERANDS];

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if (IsInstReferingShaderResult(psState, psNextInst))
		{
			/*
				Instruction is referencing a shader result so it can not be
				grouped with other instructions.
			*/
			return IMG_FALSE;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Do special checks for single-issue vector instruction.
	*/
	if (psGroup->bSVecInst)
	{
		if (psGroup->sVec.bDotProduct)
		{
			return CheckInstGroupVDP(psState, psGroup, psFirstInst, psNextInst, uIterationIndex, psGroup->psIncData);
		}
		else
		{
			return CheckInstGroupVMAD(psState, psGroup, psFirstInst, psNextInst, uIterationIndex, psGroup->psIncData);
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* Instructions with different opcodes can't be grouped. */
	if (!EqualInstructionModes(psState, psFirstInst, psNextInst))
	{
		return IMG_FALSE;
	}

	/* Check operands, getting MOE data if operand works */

	/*
		Set the default values of the MOE state needed for the new repeat from the 
		MOE state for the existing repeat.
	*/
	memcpy(asNewMoeData, psMoeData, sizeof(asNewMoeData[0]) * USC_MAX_MOE_OPERANDS);


	/*
		Check any predicate source to the instruction.
	*/
	if (!CheckPredicateSource(psState, psGroup, psFirstInst, psNextInst, uIterationIndex))
	{
		return IMG_FALSE;
	}

	/*
		The predicate destination for a test must be the same as the iteration number.
	*/
	if ((g_psInstDesc[psFirstInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0 && !psGroup->bDotProduct)
	{
		PARG	psPredDest;
		PARG	psFirstPredDest;

		if (g_psInstDesc[psNextInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
		{
			ASSERT(psNextInst->uDestCount >= VTEST_PREDICATE_DESTIDX);
			psPredDest = &psNextInst->asDest[VTEST_PREDICATE_DESTIDX];
			ASSERT(psPredDest->uType == USEASM_REGTYPE_PREDICATE);

			ASSERT(psFirstInst->uDestCount >= VTEST_PREDICATE_DESTIDX);
			psFirstPredDest = &psFirstInst->asDest[VTEST_PREDICATE_DESTIDX];
			ASSERT(psFirstPredDest->uType == USEASM_REGTYPE_PREDICATE);
		}
		else
		{
			ASSERT(psNextInst->uDestCount >= TEST_PREDICATE_DESTIDX);
			psPredDest = &psNextInst->asDest[TEST_PREDICATE_DESTIDX];
			ASSERT(psPredDest->uType == USEASM_REGTYPE_PREDICATE);

			ASSERT(psFirstInst->uDestCount >= TEST_PREDICATE_DESTIDX);
			psFirstPredDest = &psFirstInst->asDest[TEST_PREDICATE_DESTIDX];
			ASSERT(psFirstPredDest->uType == USEASM_REGTYPE_PREDICATE);
		}

		if (psPredDest->uNumber != (psFirstPredDest->uNumber + uIterationIndex))
		{
			return IMG_FALSE;
		}
	}

	/* Test destination if not a dotproduct */
	{	
		IMG_UINT32	uMOEDestIdx = GetMOEDestIdx(psFirstInst);

		if (uMOEDestIdx != USC_UNDEF)
		{
			if (!CheckArg(psState,
						  psGroup,
						  &psFirstInst->asDest[uMOEDestIdx],
						  &psNextInst->asDest[uMOEDestIdx],
						  0,
						  uIterationIndex,
						  &asNewMoeData[0]))
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		For texture samples all the destinations must have consecutive register numbers so it's enough to check
		that the numbers for the first destination can be made into a repeat.

		For FDDP the extra destination (always an internal register) isn't written until the last iteration
		anyway so ignore it.

		For other instructions the extra destinations represent fixed parameters which aren't affected by the MOE so 
		check they are exactly equal.
	*/
	if (
			(g_psInstDesc[psFirstInst->eOpcode].uFlags & (DESC_FLAGS_TEXTURESAMPLE | DESC_FLAGS_MULTIPLEDEST)) == 0 &&
			!psGroup->bDotProduct &&
			!psGroup->bDoubleDotProduct
	   )
	{
		IMG_UINT32	uDestIdx;

		ASSERT(psFirstInst->uDestCount == psNextInst->uDestCount);
		for (uDestIdx = 1; uDestIdx < psNextInst->uDestCount; uDestIdx++)
		{
			if (!EqualArgs(&psFirstInst->asDest[uDestIdx], &psNextInst->asDest[uDestIdx]))
			{
				return IMG_FALSE;
			}
		}
	}

	/* Test source operands */
	for (i = 1; i < USC_MAX_MOE_OPERANDS; i++)
	{
		IMG_UINT32	uInstArg;

		uInstArg = MoeArgToInstArg(psState, psFirstInst, i - 1);
		if (uInstArg == DESC_ARGREMAP_DONTCARE || (uInstArg & DESC_ARGREMAP_ALIASMASK) != 0)
		{
			continue;
		}

		ASSERT(uInstArg < psFirstInst->uArgumentCount);
		if (!CheckArg(psState,
					  psGroup,
					  &psFirstInst->asArg[uInstArg],
					  &psNextInst->asArg[uInstArg],
					  i,
					  uIterationIndex,
					  &asNewMoeData[i]))
		{
			return IMG_FALSE;
		}
	}

	/*
		For extra sources whose register numbers can't be altered by the
		MOE just check that they are exactly equal for both instructions.
	*/
	for (i = uMoeNumSrcArgs; i < uNumSrcArgs; i++)
	{
		if (!EqualArgs(&psFirstInst->asArg[i], &psNextInst->asArg[i]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Update the MOE state with the settings required to include this instruction
		in the repeat.
	*/
	memcpy(psMoeData, asNewMoeData, sizeof(asNewMoeData[0]) * USC_MAX_MOE_OPERANDS);

	return IMG_TRUE;
}

static IMG_UINT32 GetMaxAddressableRegNum(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_BOOL				bDest,
										  IMG_UINT32			uArgIdx,
										  PARG					psArg,
										  IMG_BOOL				bFormatControl,
										  IMG_BOOL				bF16FormatControl)
/*****************************************************************************
 FUNCTION	: GetMaxAddressableRegNum

 PURPOSE	: Gets the maximum register number which can be directly encoded
			  in an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  bDest				- Is this the destination of an instruction.
			  uArgIdx			- Index of the source or destination to check.
			  psArg				- Points to the source or destination to check.
			  bFormatControl	- Whether the top-bit of the register number
								is used to select the format.
			  bF16FormatControl	- Whether the top-bit of the register number
								selects between F16 and F32 format.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegisterNumberFieldBitCount;
	IMG_UINT32	uMaxAddressableRegNum;
	IMG_BOOL	bTopBitIsFormatSel;

	PVR_UNREFERENCED_PARAMETER(uArgIdx);
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVMOVC_I32 || psInst->eOpcode == IVMOVCU8)
	{
		uMaxAddressableRegNum = SGXVEC_USE0_VMOVC_INT_REGISTER_NUMBER_MAX - 1;

		if (psArg->uType == USEASM_REGTYPE_TEMP)
		{
			uMaxAddressableRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;
		}

		return uMaxAddressableRegNum;
	}
	#else
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Does the top-bit of the register number choose between two different 
		interpretations of the register contents? In this case the maximum register
		number is reduced.
	*/
	bTopBitIsFormatSel = IMG_FALSE;
	if (bFormatControl && TypeUsesFormatControl(psArg->uType) && !(bDest && bF16FormatControl))
	{
		bTopBitIsFormatSel = IMG_TRUE;
	}

	/*
		Indexed arguments have special restrictions on the maximum register number.
	*/
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		if (bTopBitIsFormatSel)
		{
			return EURASIA_USE_FCINDEX_MAXIMUM_OFFSET;
		}
		else
		{
			return EURASIA_USE_INDEX_MAXIMUM_OFFSET;
		}
	}

	/*
		Get the width of the register number field in this instruction.
	*/
	if (bDest)
	{
		uRegisterNumberFieldBitCount = g_psInstDesc[psInst->eOpcode].uDestRegisterNumberBitCount;
	}
	else
	{
		uRegisterNumberFieldBitCount = g_psInstDesc[psInst->eOpcode].uSourceRegisterNumberBitCount;
	}

	/*
		Does the top-bit of the register number select between alternate interpretations of the
		register contents.
	*/
	if (bTopBitIsFormatSel)
	{	
		uRegisterNumberFieldBitCount--;
	}	

	/*
		What's the maximum register number that can be used in this
		instruction?
	*/
	uMaxAddressableRegNum = (1U << uRegisterNumberFieldBitCount) - 1;

	/*
		The top registers in the temporary bank are aliases for the
		internal registers.
	*/
	if (psArg->uType == USEASM_REGTYPE_TEMP)
	{
		uMaxAddressableRegNum -= EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Some vector instructions have special restrictions on one of their sources.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_WIDEVECTORSOURCE) != 0 && !bDest && uArgIdx == 1)
	{
		IMG_UINT32	uWideRegisterNumberMax;

		uWideRegisterNumberMax = SGXVEC_USE_VEC2_WIDE_USSOURCE_REGISTER_NUMBER_MAX;
		if (!(psInst->eOpcode == IVDUAL && !IsVDUALUnifiedStoreVectorSource(psState, psInst)))
		{
			uWideRegisterNumberMax >>= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		}
		uMaxAddressableRegNum = min(uMaxAddressableRegNum, uWideRegisterNumberMax);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	return uMaxAddressableRegNum;
}

static IMG_VOID GetArgumentFormatControlRequirements(PINTERMEDIATE_STATE	psState,
													 PINST					psInst,
													 IMG_BOOL				bDest,
													 IMG_UINT32				uArgIdx,
													 PARG					psArg,
													 IMG_BOOL				bF16FmtControl,
													 IMG_PBOOL				pbNeedsFmtCtrlOff,
													 IMG_PBOOL				pbNeedsFmtCtrlOn,
													 IMG_PBOOL				pbNeedsFmtCtrlDefined)
/*****************************************************************************
 FUNCTION	: GetArgumentFormatControlRequirements

 PURPOSE	: Get the state of the format control required by an instruction
			  argument.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  bDest				- TRUE if the argument is the instruction
								destination.
			  uArgIdx			- Index of the argument to check.
			  bF16FmtControl	- TRUE if the format control for this instruction
								selects between F16 and F32 format registers.
			  pbNeedsFmtCtrlOff	- Set to TRUE if the argument requires the
								format control to be off.
			  pbNeedsFmtCtrlOn	- Set to TRUE if the argument requires the
								format control to be on.
			  pbNeedsFmtCtrlDefined
								- Set to TRUE if the argument requires the
								format control to have a defined state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (TypeUsesFormatControl(psArg->uType))
	{
		UF_REGFORMAT	eFormatCmtFmt;

		if (bF16FmtControl)
		{
			eFormatCmtFmt = UF_REGFORMAT_F16;
		}
		else
		{
			eFormatCmtFmt = UF_REGFORMAT_C10;
		}

		if (psArg->eFmt == eFormatCmtFmt)
		{
			/*
				If this argument is in the alternate format for the instruction then
				the MOE format control flag must be on.
			*/
			*pbNeedsFmtCtrlOn = IMG_TRUE;
		}
		else
		{
			IMG_UINT32		uMaxRegNum;

			/*
				If the register number for this argument is larger than the maximum number
				which is addressable when the MOE format control flag is on then switch
				the flag off. 
			*/
			uMaxRegNum = GetMaxAddressableRegNum(psState,
												 psInst, 
												 bDest,
												 uArgIdx,
												 psArg,
												 IMG_TRUE /* bFormatControl */, 
												 bF16FmtControl);
			if (psArg->uNumber > uMaxRegNum)
			{
				*pbNeedsFmtCtrlOff = IMG_TRUE;
			}
		}

		/*
			For the fpinternal register bank the setting of the format control flag affects
			the encoding of the register number so it must have a defined value.
		*/
		if (pbNeedsFmtCtrlDefined != NULL)
		{
			if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
			{
				*pbNeedsFmtCtrlDefined = IMG_TRUE;
			}
		}
	}
}

static IMG_VOID GetFormatControlRequirements(PINTERMEDIATE_STATE	psState,
											 PINST					psInst,
											 IMG_PBOOL				pbNeedsFmtCtrlOff,
											 IMG_PBOOL				pbNeedsFmtCtrlOn,
											 IMG_PBOOL				pbNeedsFmtCtrlDefined)
/*****************************************************************************
 FUNCTION	: GetFormatControlRequirements

 PURPOSE	: Get the state of the format control required by an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  pbNeedsFmtCtrlOff	- Set to TRUE if the instruction requires the
								format control to be off.
			  pbNeedsFmtCtrlOn	- Set to TRUE if the instruction requires the
								format control to be on.
			  pbNeedsFmtCtrlDefined
								- Set to TRUE if the argument requires the
								format control to have a defined state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uMoeArg;
	IMG_BOOL		bF16FmtControl;

	*pbNeedsFmtCtrlOff = IMG_FALSE;
	*pbNeedsFmtCtrlOn = IMG_FALSE;
	if (pbNeedsFmtCtrlDefined != NULL)
	{
		*pbNeedsFmtCtrlDefined = IMG_FALSE;
	}

	/*
		Get the type of alternate format for this instruction.
	*/
	if (psInst->eOpcode != IEFO)
	{
		bF16FmtControl = IMG_FALSE;
	}
	else
	{
		bF16FmtControl = IMG_TRUE;
	}

	/*
		Check if the destination needs the format control either on or off.
	*/
	if (psInst->eOpcode != IEFO && psInst->uDestCount > 0)
	{
		GetArgumentFormatControlRequirements(psState,
											 psInst,
											 IMG_TRUE /* bDest */,
											 0 /* uArgIdx */,
											 &psInst->asDest[0],
											 bF16FmtControl,
											 pbNeedsFmtCtrlOff,
											 pbNeedsFmtCtrlOn,
											 pbNeedsFmtCtrlDefined);
	}

	/*
		For each source argument check whether it needs the format control either off or on.
	*/
	for (uMoeArg = 0; uMoeArg < (USC_MAX_MOE_OPERANDS - 1); uMoeArg++)
	{
		IMG_UINT32	uInstArg;

		uInstArg = MoeArgToInstArg(psState, psInst, uMoeArg);
		ASSERT(uInstArg != DESC_ARGREMAP_INVALID);

		/*
			Skip unreferenced arguments.
		*/
		if (uInstArg == DESC_ARGREMAP_DONTCARE || (uInstArg & DESC_ARGREMAP_ALIASMASK) != 0)
		{
			continue;
		}

		GetArgumentFormatControlRequirements(psState,
											 psInst,
											 IMG_FALSE /* bDest */,
											 uInstArg,
											 &psInst->asArg[uInstArg],
											 bF16FmtControl,
											 pbNeedsFmtCtrlOff,
											 pbNeedsFmtCtrlOn,
											 pbNeedsFmtCtrlDefined);
	}
}

static
IMG_VOID InsertSetfcInstruction(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psCodeBlock,
								PINST				psInsertBeforeInst,
								PSETFC_STATE		psMoeState)
/*****************************************************************************
 FUNCTION	: InsertSetfcInstruction

 PURPOSE	: Add a SETFC instruction to a block.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- The block to insert the instruction in.
			  psInsertBeforeInst	- Instruction to insert the SETFC instruction
									immediately before.
			  psMoeState			- State to set the format control enables to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psSetfcInst;
	IMG_UINT32	uArg;

	ASSERT(psState->psMainProg->sCfg.psEntry);//->uType == CBTYPE_BASIC);

	psSetfcInst = AllocateInst(psState, psInsertBeforeInst);

	SetOpcode(psState, psSetfcInst, ISETFC);
	psSetfcInst->asDest[0].uType = USC_REGTYPE_DUMMY;
	psSetfcInst->asDest[0].uNumber = 0;
	for (uArg = 0; uArg < FORMAT_CONTROL_FLAG_TYPE_COUNT; uArg++)
	{
		PARG	psArg = &psSetfcInst->asArg[uArg];

		psArg->uType = USEASM_REGTYPE_IMMEDIATE;
		if (psMoeState->aeFlag[uArg] == FORMAT_CONTROL_FLAG_ON)
		{
			psArg->uNumber = 1;
		}
		else
		{
			ASSERT(psMoeState->aeFlag[uArg] == FORMAT_CONTROL_FLAG_OFF);
			psArg->uNumber = 0;
		}
	}
	InsertInstBefore(psState, psCodeBlock, psSetfcInst, psInsertBeforeInst);
}

static IMG_VOID CheckFormatControl(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psCodeBlock,
								   PINST					psFirstInst,
								   PSETFC_STATE				psMoeState,
								   IMG_BOOL					bInsertInst)
/*****************************************************************************
 FUNCTION	: CheckFormatControl

 PURPOSE	: Check if a SETFC instruction is needed before a instruction.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- The block containing the instruction to be checked.
			  psFirstInst			- The instruction to be checked.
			  psMoeState			- The current state of the MOE
			  bInsertInst			- Whether to actually insert a SETFC instr (or just model state)

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psFirstInst->eOpcode == IEFO || HasC10FmtControl(psFirstInst))
	{
		IMG_BOOL					bNeedsFmtCtrlOn;
		IMG_BOOL					bNeedsFmtCtrlOff;
		IMG_BOOL					bNeedsFmtCtrlDefined;
		IMG_BOOL					bNeedsSetFc = IMG_FALSE;
		FORMAT_CONTROL_FLAG_TYPE	eType;
		FORMAT_CONTROL_FLAG_TYPE	eOtherType;
		FORMAT_CONTROL_FLAG			eFlag;

		/*
			Check if the instruction needs the format control either on or off.
		*/
		GetFormatControlRequirements(psState, psFirstInst, &bNeedsFmtCtrlOff, &bNeedsFmtCtrlOn, &bNeedsFmtCtrlDefined);
		ASSERT(!(bNeedsFmtCtrlOn && bNeedsFmtCtrlOff));

		/*
			Check the format control flag used by this instruction type.
		*/
		if (psFirstInst->eOpcode == IEFO)
		{
			eType = FORMAT_CONTROL_FLAG_TYPE_EFO;
			eOtherType = FORMAT_CONTROL_FLAG_TYPE_COLOUR;
		}
		else
		{
			eType = FORMAT_CONTROL_FLAG_TYPE_COLOUR;
			eOtherType = FORMAT_CONTROL_FLAG_TYPE_EFO;
		}

		/*
			If this instruction needs a particular state of the format control check
			if either the current state is undefined or doesn't match the required
			state.
		*/
		eFlag = psMoeState->aeFlag[eType];
		if ((bNeedsFmtCtrlOn || bNeedsFmtCtrlOff || bNeedsFmtCtrlDefined) && eFlag == FORMAT_CONTROL_FLAG_UNDEFINED)
		{
			bNeedsSetFc = IMG_TRUE;
		}
		if ((bNeedsFmtCtrlOn && eFlag == FORMAT_CONTROL_FLAG_OFF) || (bNeedsFmtCtrlOff && eFlag == FORMAT_CONTROL_FLAG_ON))
		{
			bNeedsSetFc = IMG_TRUE;
		}

		/*
			Insert a setfc instruction if required.
		*/
		if (bNeedsSetFc)
		{
			static const FORMAT_CONTROL_FLAG aeDefaultFlag[FORMAT_CONTROL_FLAG_TYPE_COUNT] =
			{
				FORMAT_CONTROL_FLAG_OFF,	/* FORMAT_CONTROL_FLAG_TYPE_EFO */
				FORMAT_CONTROL_FLAG_ON,		/* FORMAT_CONTROL_FLAG_TYPE_COLOUR */
			};

			/*
				Set the new state required by this instruction.
			*/
			if (bNeedsFmtCtrlOn)
			{
				psMoeState->aeFlag[eType] = FORMAT_CONTROL_FLAG_ON;
			}
			else if (bNeedsFmtCtrlOff)
			{
				psMoeState->aeFlag[eType] = FORMAT_CONTROL_FLAG_OFF;
			}
			else
			{
				ASSERT(bNeedsFmtCtrlDefined);
				psMoeState->aeFlag[eType] = aeDefaultFlag[eType];
			}

			/*
				If the other format control flag has an undefined state then
				set it to the default value.
			*/
			if (psMoeState->aeFlag[eOtherType] == FORMAT_CONTROL_FLAG_UNDEFINED)
			{
				psMoeState->aeFlag[eOtherType] = aeDefaultFlag[eType];
			}

			if (bInsertInst)
			{
				/*
					Add a SETFC instruction before the current instruction to set the
					required state of the format control flags.
				*/
				InsertSetfcInstruction(psState, psCodeBlock, psFirstInst, psMoeState);
			}
		}
		if (bInsertInst)
		{
			/*
				For instructions which use the format control flags record what
				the flag's value's will be when the instruction is executed.
			*/
			if (psMoeState->aeFlag[eType] == FORMAT_CONTROL_FLAG_ON)
			{
				SetBit(psFirstInst->auFlag, INST_FORMAT_SELECT, 1);
			}
		}
	}
}

static
IMG_VOID MoeToInst(PINTERMEDIATE_STATE psState,
				   const SMLSI_STATE* psMoeState,
				   PINST psInst)
/*****************************************************************************
 FUNCTION	: MoeToInst

 PURPOSE	: Make an instruction to set the MOE operand modes.

 INPUT		: psState		- Compiler state
			  psMoeState	- The moe state to be set

 OUTPUT		: pInst			- The instruction  to set to the MOE values.

 RETURNS	: Nothing.

 DESCRIPTION: Sets psInst to an SMLSI setting the MOE operand modes.
*****************************************************************************/
{
   IMG_UINT32 indx;

   SetOpcode(psState, psInst, ISMLSI);
   psInst->asDest[0].uType = USC_REGTYPE_DUMMY;

   /*
	  Set increment/swizzle values for each operand

	  For operand i, psInst->asArg[i] is the operand value
	  and pSInst->asArg[i+4] enables(=1)/disables(=0) swizzle
	  mode.
   */

	for (indx = 0; indx < 4; indx++)
	{
		PARG	psIncrArg = &psInst->asArg[indx + 0];
		PARG	psModeArg = &psInst->asArg[indx + 4];

		psModeArg->uType = USEASM_REGTYPE_IMMEDIATE;

		if (psMoeState->psMoeData[indx].eOperandMode == MOE_SWIZZLE)
		{
			IMG_UINT32	uSwizzle;
			IMG_UINT32	uSlot;

			uSwizzle = 0;
			for (uSlot = 0; uSlot < USC_MAX_SWIZZLE_SLOTS; uSlot++)
			{
				IMG_UINT32	uSel;

				uSel = psMoeState->psMoeData[indx].u.s.auSwizzle[uSlot];
				uSwizzle |= (uSel << (USEASM_SWIZZLE_FIELD_SIZE * uSlot));
			}
			psIncrArg->uNumber = uSwizzle;

			psIncrArg->uType = USEASM_REGTYPE_SWIZZLE;
			psModeArg->uNumber = 1;
		}
		else
		{
			psIncrArg->uNumber = psMoeState->psMoeData[indx].u.iIncrement;

			psIncrArg->uType = USEASM_REGTYPE_IMMEDIATE;
			psModeArg->uNumber = 0;
		}
	}
}

static IMG_BOOL EqualMoeData(const MOE_DATA* psFst, const MOE_DATA* psSnd, IMG_UINT32 uNum)
/*****************************************************************************
 FUNCTION	: EqualMoeData

 PURPOSE	: Compare an array of MOE_DATA structures for equality (same mode, same value)

 INPUT		: psFst, psSnd		- The first strutures in the arrays of MOE data to compare
		      uNum				- The number of strutures to compare.

 RETURNS	: IMG_TRUE if the structures are equal, IMG_FALSE otherwise
*****************************************************************************/
{
	IMG_UINT32 indx;

	for (indx = 0; indx < uNum; indx ++)
	{
		const MOE_DATA*	psFstArg = &psFst[indx];
		const MOE_DATA* psSndArg = &psSnd[indx];

		if (psFstArg->eOperandMode != psSndArg->eOperandMode)
		{
			return IMG_FALSE;
		}
		if (psFstArg->eOperandMode == MOE_INCREMENT)
		{
			if (psFstArg->u.iIncrement != psSndArg->u.iIncrement)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			IMG_UINT32	uSlot;

			for (uSlot = 0; uSlot < USC_MAX_SWIZZLE_SLOTS; uSlot++)
			{
				if (psFstArg->u.s.auSwizzle[uSlot] != psSndArg->u.s.auSwizzle[uSlot])
				{
					return IMG_FALSE;
				}
			}
		}
	}

	return IMG_TRUE;
}

static IMG_VOID ResetMoeOpState(PSMLSI_STATE psState)
/*****************************************************************************
 FUNCTION	: ResetMoeOpState

 PURPOSE	: Reset the MOE operand modes to the default values (increment 1 mode).
			  Clear operand invalid flags.

 INPUT		: psState		- The MOE state to reset

 RETURNS	: Nothing
*****************************************************************************/
{
	MOE_DATA sTemp = USC_MOE_DATA_DEFAULT_VALUE;
	IMG_UINT32 indx;

	for (indx = 0; indx < USC_MAX_MOE_OPERANDS; indx++)
	{
		psState->pbInvalid[indx] = IMG_FALSE;
		memcpy(&psState->psMoeData[indx], &sTemp, sizeof(psState->psMoeData[indx]));
	}
}

static
IMG_UINT32 RemapMoeArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: RemapMoeArg

 PURPOSE	: Convert between an MOE argument number and the corresponding
			  argument in the intermediate format.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to convert for.
			  uArg			- The MOE argument number.

 RETURNS	: The argument number for the intermediate instruction.
*****************************************************************************/
{
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP) != 0 &&
		(psInst->eOpcode == IFMIN || psInst->eOpcode == IFMAX))
	{
		static const IMG_UINT32	puMinRemap[] = {0, 0, 1};
		static const IMG_UINT32	puMaxRemap[] = {0, 1, 0};
		if (psInst->eOpcode == IFMIN)
		{
			return puMinRemap[uArg];
		}
		else
		{
			return puMaxRemap[uArg];
		}
	}
	else if (psInst->eOpcode == ITESTPRED || psInst->eOpcode == ITESTMASK)
	{
		if (psInst->u.psTest->eAluOpcode == IFPADD8 || 
			psInst->u.psTest->eAluOpcode == IFPSUB8 ||
			psInst->u.psTest->eAluOpcode == IIADD8 || 
			psInst->u.psTest->eAluOpcode == IISUB8 ||
			psInst->u.psTest->eAluOpcode == IIADDU8 || 
			psInst->u.psTest->eAluOpcode == IISUBU8)
		{
			/*
				Remap:
					MOE source 0 -> Instruction source 1
					MOE source 1 -> Instruction source 0
					MOE source 2 -> Unused
			*/
			static const IMG_UINT32 auFPTestRemap[] = {1, 0, DESC_ARGREMAP_DONTCARE};

			return auFPTestRemap[uArg];
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		else if (psInst->u.psTest->eAluOpcode == IIADDU32 ||
				 psInst->u.psTest->eAluOpcode == IIADD32 ||
				 psInst->u.psTest->eAluOpcode == IISUBU32 ||
				 psInst->u.psTest->eAluOpcode == IISUB32)
		{
			/*
				Remap:
					MOE source 0 -> Instruction source 0
					MOE source 1 -> Unused
					MOE source 2 -> Instruction source 1
			*/
			static const IMG_UINT32 auInt32TestRemap[] = {0, DESC_ARGREMAP_DONTCARE, 1};

			return auInt32TestRemap[uArg];
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		else
		{
			return g_psInstDesc[psInst->eOpcode].puMoeArgRemap[uArg];
		}
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	else if (psInst->eOpcode == IFPTESTPRED_VEC || psInst->eOpcode == IFPTESTMASK_VEC)
	{
		/*
			Remap:
				MOE source 0 -> Instruction vector source 1
				MOE source 1 -> Instruction vector source 0
				MOE source 2 -> Unused
		*/
		static const IMG_UINT32 auFPTestRemap_Vec[] = {C10VEC_MOESOURCE_ARGINDEX(1), C10VEC_MOESOURCE_ARGINDEX(0), DESC_ARGREMAP_DONTCARE};

		ASSERT(psInst->u.psTest->eAluOpcode == IFPADD8_VEC || psInst->u.psTest->eAluOpcode == IFPSUB8_VEC);

		return auFPTestRemap_Vec[uArg];
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	else
	{
		return g_psInstDesc[psInst->eOpcode].puMoeArgRemap[uArg];
	}
}

IMG_INTERNAL
IMG_UINT32 MoeArgToInstArg(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: MoeArgToInstArg

 PURPOSE	: Convert between an MOE argument number and the corresponding
			  argument in the intermediate format.

 PARAMETERS	: psInst		- Instruction to convert for.
			  uArg			- The MOE argument number.

 RETURNS	: The argument number for the intermediate instruction.
*****************************************************************************/
{
	if (uArg == 0 && Src0AliasesDest(psState, psInst))
	{
		ASSERT(RemapMoeArg(psState, psInst, uArg) == DESC_ARGREMAP_DONTCARE);
		return DESC_ARGREMAP_DESTALIAS;
	}
	return RemapMoeArg(psState, psInst, uArg);
}

/*
	Information about a possible location for inserting an SMLSI
	instruction. When an MOE repeat is created the location
	of the instruction is recorded in this structure. Then we
	accumulate information about the restrictions on the MOE
	state which could be set by an SMLSI instruction inserted
	at the site based on the instructions in between it and
	the current instruction.
*/
typedef struct
{
	/*
		Pointer to the site to insert the SMLSI instruction
		after.
	*/
	PINST		psLocation;

	/*
		Minimum register number used by instructions between the
		site and the current instruction for each MOE operand.
	*/
	IMG_UINT32	auMinOperandNum[USC_MAX_MOE_OPERANDS];

	/*
		For each MOE operand either USC_UNDEF or the index of a previous
		MOE operand. In the latter case both MOE operands require the
		same state.
	*/
	IMG_UINT32	auArgAlias[USC_MAX_MOE_OPERANDS];
#if defined(OUTPUT_USPBIN)
	/*
		For each MOE operand either IMG_FALSE or IMG_TRUE if MOE operand 
		is a Shader Result Reference.
	*/
	IMG_BOOL	abShaderResultRef[USC_MAX_MOE_OPERANDS];
#endif	/* defined(OUTPUT_USPBIN) */
	IMG_BOOL	bInCompatibleInst;
} MOE_SMLSI_SITE, *PMOE_SMLSI_SITE;

static
IMG_VOID GetArgumentsAffectedByMOESwizzles(PINTERMEDIATE_STATE	psState,
										   PINST				psInst,
										   IMG_BOOL				bAdjustToPerInstIncrements,
										   PARG					apsArgs[],
										   IMG_UINT32			auArgIdxs[],
										   IMG_UINT32			auArgAliases[])
/*****************************************************************************
 FUNCTION	: GetArgumentsAffectedByMOESwizzles

 PURPOSE	: Fills out an array of the instruction arguments which are affected
			  by the state for MOE operand.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to get the arguments for.
			  bAdjustToPerInstIncrements
								- If TRUE don't return any arguments for
								instructions which support MOE increments
								directly encoded in the instruction.
			  apsArg			- Returns pointers to the instruction
								argument corresponding to each MOE operand.
			  auArgIdxs			- Returns the indexes (into either the destination or
								source arrays) corresponding to each MOE operand.
			  auArgAliases		- For each MOE operand either USC_UNDEF or
								the index of another MOE operand. In the latter
								case both operands must have exactly the same
								MOE state.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uMoeArg;
	IMG_UINT32	uReferencedInstSrcs = 0;

	PVR_UNREFERENCED_PARAMETER(bAdjustToPerInstIncrements);

	/*
		Initialize return data.
	*/
	for (uMoeArg = 0; uMoeArg < USC_MAX_MOE_OPERANDS; uMoeArg++)
	{
		apsArgs[uMoeArg] = NULL;
		auArgAliases[uMoeArg] = USC_UNDEF;
		if (auArgIdxs != NULL)
		{
			auArgIdxs[uMoeArg] = USC_UNDEF;
		}
	}

	#if defined(SUPPORT_SGX545)
	if (bAdjustToPerInstIncrements)
	{
		/*
			If the instruction supports MOE increments directly encoded in the
			instruction then we can switch that mode on to ignore the global
			MOE increments/swizzles.
		*/
		ASSERT(GetBit(psInst->auFlag, INST_MOE));
		if (SupportsPerInstMoeIncrements(psState, psInst))
		{
			return;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	/* Go through the MOE operands, changing any which are in swizzle mode */
	for (uMoeArg = 0; uMoeArg < USC_MAX_MOE_OPERANDS; uMoeArg++)
	{
		PARG	psArg;

		/* Get the operand number */
		if (uMoeArg == 0)
		{
			IMG_UINT32	uMOEDestIdx = GetMOEDestIdx(psInst);

			/*
				Skip an unused destination.
			*/
			if (uMOEDestIdx == USC_UNDEF)
			{
				continue;
			}

			/* Get the destination operand */
			psArg = &psInst->asDest[uMOEDestIdx];
			if (auArgIdxs != NULL)
			{
				auArgIdxs[0] = 0;
			}
		}
		else
		{
			IMG_UINT32	uSrcArg;

			uSrcArg = MoeArgToInstArg(psState, psInst, uMoeArg - 1);
			ASSERT(uSrcArg != DESC_ARGREMAP_INVALID);

			/*
				Skip MOE arguments not referenced by the instruction.
			*/
			if (uSrcArg == DESC_ARGREMAP_DONTCARE)
			{
				continue;
			}

			if ((uSrcArg & DESC_ARGREMAP_ALIASMASK) != 0)
			{
				IMG_UINT32	uAliasArg = DESC_ARGREMAP_GETALIASOP(uSrcArg);

				ASSERT(uAliasArg < uMoeArg);

				/*
					Update this MOE operand using the same argument as the
					aliased operand.
				*/
				psArg = apsArgs[uAliasArg];

				/*
					Don't return any state for this operand if the operand
					it aliases isn't affected by the MOE.
				*/
				if (psArg == NULL)
				{
					continue;
				}

				/* 
					Flag that this MOE operand requires exactly the same MOE state as
					its alias.
				*/
				auArgAliases[uMoeArg] = uAliasArg;
			}
			else
			{
				/* Each instruction argument must correspond to only one MOE operand. */
				ASSERT((uReferencedInstSrcs & (1U << uSrcArg)) == 0);
				uReferencedInstSrcs |= (1U << uSrcArg);
	
				/* Get the source operand */
				ASSERT(uSrcArg < psInst->uArgumentCount);
				psArg = &psInst->asArg[uSrcArg];
				if (auArgIdxs != NULL)
				{
					auArgIdxs[uMoeArg] = uSrcArg;
				}
			}
		}

		/*
			Immediate arguments aren't affected by the MOE swizzle/increments.
		*/
		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE ||
			psArg->uType == USC_REGTYPE_UNUSEDSOURCE)
		{
			continue;
		}
		/* Index registers aren't subject to MOE offset when being written */
		if (uMoeArg == 0 && psArg->uType == USEASM_REGTYPE_INDEX)
		{
			continue;
		}

		/*
			Store the instruction argument corresponding to MOE operand.
		*/
		apsArgs[uMoeArg] = psArg;
	}
}

static
IMG_VOID UpdateSMLSISiteConstraints(PINTERMEDIATE_STATE	psState,
									PMOE_SMLSI_SITE		psSite,
									PINST				psInst)
/*****************************************************************************
 FUNCTION	: UpdateSMLSISiteConstraints

 PURPOSE	: Update information about which MOE states are compatible with
			  a range of instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSite			- Information about to.
			  psInst			- Instruction to whose information is to be
								added.

 RETURNS	: None.
*****************************************************************************/
{
	PARG		apsArgs[USC_MAX_MOE_OPERANDS];
	IMG_UINT32	auArgAlias[USC_MAX_MOE_OPERANDS];	
	IMG_UINT32	uMOEArg;

	if (GetBit(psInst->auFlag, INST_NOEMIT))
	{
		psSite->bInCompatibleInst |= IMG_TRUE;
		return;
	}
	GetArgumentsAffectedByMOESwizzles(psState, 
									  psInst, 
									  IMG_TRUE /* bAdjustToPerInstIncrements */, 
									  apsArgs, 
									  NULL /* auArgIdxs */,
									  auArgAlias);

	for (uMOEArg = 0; uMOEArg < USC_MAX_MOE_OPERANDS; uMOEArg++)
	{
		/*
			Copy information about MOE operands which require the
			same state.
		*/
		if (psSite->auArgAlias[uMOEArg] == USC_UNDEF)
		{
			psSite->auArgAlias[uMOEArg] = auArgAlias[uMOEArg];
		}
		else
		{
			ASSERT(auArgAlias[uMOEArg] == USC_UNDEF || psSite->auArgAlias[uMOEArg] == auArgAlias[uMOEArg]);
		}

		if (apsArgs[uMOEArg] != NULL)
		{
			IMG_UINT32	uRegNum;
			
			uRegNum = apsArgs[uMOEArg]->uNumber;

			/*
				Update the minimum register number used with this MOE operand.
			*/
			psSite->auMinOperandNum[uMOEArg] = min(psSite->auMinOperandNum[uMOEArg], uRegNum);
		}
#if defined(OUTPUT_USPBIN)		
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			if(uMOEArg == 0)
			{
				/*
					Skip an unused destination.
				*/
				if (psInst->uDestCount > 0)
				{
					psSite->abShaderResultRef[uMOEArg] |= IsInstDestShaderResult(psState, psInst);				
				}
			}
			else
			{
				IMG_UINT32	uInstArg;
				uInstArg = MoeArgToInstArg(psState, psInst, uMOEArg - 1);
				if (uInstArg == DESC_ARGREMAP_DONTCARE || (uInstArg & DESC_ARGREMAP_ALIASMASK) != 0)
				{
					continue;
				}
				psSite->abShaderResultRef[uMOEArg] |= IsInstSrcShaderResultRef(psState, psInst, uInstArg);
			}
		}
#endif	/* defined(OUTPUT_USPBIN) */
	}
}

static IMG_INT32 GetRegisterOffsetAtIteration(PINTERMEDIATE_STATE	psState,
											  PMOE_DATA				psMoeData,
											  IMG_UINT32			uIteration)
/*****************************************************************************
 FUNCTION	: GetRegisterOffsetAtIteration

 PURPOSE	: Get the offset applied by the MOE to a register number on a
			  particular iteration.

 PARAMETERS	: psState		- Compiler state.
			  psMoeData		- MOE state for the argument.
			  uIteration	- Iteration to check the offset for.

 RETURNS	: The signed offset.
*****************************************************************************/
{
	if (psMoeData->eOperandMode == MOE_INCREMENT)
	{
		return psMoeData->u.iIncrement * (IMG_INT32)uIteration;
	}
	else
	{
		IMG_UINT32	uSel;

		ASSERT(psMoeData->eOperandMode == MOE_SWIZZLE);

		uSel = psMoeData->u.s.auSwizzle[uIteration % USC_MAX_SWIZZLE_SLOTS];
		return (IMG_INT32)uSel;
	}
}

static
IMG_VOID AdjustRegisterNumbersForMOESwizzle(PINTERMEDIATE_STATE	psState,
											PMOE_DATA			asMoeData,
											PINST				psFirstInst,
											PINST				psLastInst,
											IMG_BOOL			bAdjustToPerInstIncrements,
											IMG_BOOL			bIgnoreDest)
/*****************************************************************************
 FUNCTION	: AdjustRegisterNumbersForMOESwizzle

 PURPOSE	: Adjust the register numbers of arguments to instructions in a
			  range so when they are executed while a given MOE state is
			  setup the post-MOE register numbers are the same as the
			  pre-adjusted ones.

 PARAMETERS	: psState			- Compiler state.
			  asMoeData			- MOE state to adjust for.
			  psFirstInst		- First instruction in the range.
			  psLastInst		- First instruction after the range.

 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;
	IMG_UINT32	uMOEArg;
	IMG_UINT32	auRegisterNumberAdjust[USC_MAX_MOE_OPERANDS];

	PVR_UNREFERENCED_PARAMETER(bAdjustToPerInstIncrements);

	for (uMOEArg = 0; uMOEArg < USC_MAX_MOE_OPERANDS; uMOEArg++)
	{
		auRegisterNumberAdjust[uMOEArg] = 
			GetRegisterOffsetAtIteration(psState, &asMoeData[uMOEArg], 0 /* uIteration */);
	}

	for (psInst = psFirstInst; psInst != psLastInst; psInst = psInst->psNext)
	{
		PARG		apsArgs[USC_MAX_MOE_OPERANDS];
		IMG_UINT32	auArgAlias[USC_MAX_MOE_OPERANDS];

		#if defined(SUPPORT_SGX545)
		/*
			For an instruction which support MOE increments encoded directly in the
			instruction switch that mode on (even though the instruction isn't
			repeated) so the global MOE increments/swizzles are ignored.
		*/
		if (bAdjustToPerInstIncrements && SupportsPerInstMoeIncrements(psState, psInst))
		{
			ASSERT(GetBit(psInst->auFlag, INST_MOE) == 1);
			SetBit(psInst->auFlag, INST_MOE, 0);
			continue;
		}
		#endif /* defined(SUPPORT_SGX545) */

		GetArgumentsAffectedByMOESwizzles(psState, 
										  psInst, 
										  bAdjustToPerInstIncrements, 
										  apsArgs, 
										  NULL /* uArgIdxs */,
										  auArgAlias);

		/*
			Clear the state for the destination.
		*/
		if (bIgnoreDest)
		{
			for (uMOEArg = 0; uMOEArg < USC_MAX_MOE_OPERANDS; uMOEArg++)
			{
				ASSERT(auArgAlias[uMOEArg] != 0);
			}
			apsArgs[0] = NULL;
		}

		for (uMOEArg = 0; uMOEArg < USC_MAX_MOE_OPERANDS; uMOEArg++)
		{
			IMG_UINT32	uAdjustForInst;

			if (auArgAlias[uMOEArg] != USC_UNDEF)
			{
				ASSERT(auRegisterNumberAdjust[uMOEArg] == auRegisterNumberAdjust[auArgAlias[uMOEArg]]);
				continue;
			}
			if (apsArgs[uMOEArg] == NULL)
			{
				continue;
			}

			/*
				Subtract the MOE increment on iteration 0 from the register number encoded in
				the instruction. Then when the hardware adds the increment when executing
				the instruction we'll get back to the original register number.
			*/
			uAdjustForInst = auRegisterNumberAdjust[uMOEArg];
			ASSERT(apsArgs[uMOEArg]->uNumber >= uAdjustForInst);
			apsArgs[uMOEArg]->uNumber -= uAdjustForInst;
		}
	}
}

IMG_INTERNAL
IMG_BOOL IsValidSwapInst(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsInvalidSwapInst

 PURPOSE	: Check if an instruction can be moved around (and is valid for InstsCanSwap)

 PARAMETERS	: psInst				- Instruction to test.

 RETURNS	: IMG_TRUE if the instruction can be moved; IMG_FALSE otherwise.
*****************************************************************************/
{
	if (psInst == NULL)
	{
		return IMG_FALSE;
	}

	/* Don't move tests or repeated instructions */
	if (psInst->uRepeat > 0)
	{
		return IMG_FALSE;
	}

	/* Test by opcode */
	switch (psInst->eOpcode)
	{
		/* These instructions can't be moved */

		case IBR:
		case ICALL:
		case ILABEL:
		case ILAPC:

		case IIDF:
		case IWDF:

		case ISMLSI:
		case ISMR:
		case ISETFC:
		case ISMBO:
		case IDRVPADDING:

		#if defined(OUTPUT_USPBIN)
		case ISMP_USP:
		case ISMPBIAS_USP:
		case ISMPREPLACE_USP:
		case ISMPGRAD_USP:
		case ISMP_USP_NDR:
		#endif /* defined(OUTPUT_USPBIN) */
		{
			return IMG_FALSE;
		}

		/* These instructions can sometimes be moved */
		case ILDAB:
		case ILDLB:
		case ILDAW:
		case ILDLW:
		case ILDAD:
		case ILDLD:

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case ILDAQ:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		#if defined(SUPPORT_SGX545)
		case IELDD:
		case IELDQ:
		#endif /* defined(SUPPORT_SGX545) */

		case ISTAB:
		case ISTLB:
		case ISTAW:
		case ISTLW:
		case ISTAD:
		case ISTLD:

		case ISPILLREAD:
		case ISPILLWRITE:
		{
			if (GetBit(psInst->auFlag, INST_FETCH))
				return IMG_FALSE;
		}

		default:
			break;
	}

	return IMG_TRUE;
}

static
IMG_UINT32 UpdateLiveChansInDestForInst(PINTERMEDIATE_STATE psState, 
										PINST				psInst, 
										IMG_UINT32			uLiveChansInDest, 
										PARG				psArg,
										IMG_UINT32			uNewLiveChansInArg)
/*****************************************************************************
 FUNCTION	: UpdateLiveChansInDestForInst

 PURPOSE	: When moving an instruction to form an MOE repeat update an
			  intermediate instruction.

 PARAMETERS	: psState		- Compiler state
			  psInst		- Instruction to update.
			  uLiveChansInDest
							- Mask of channels live in the argument immediately
							after the instruction to update.
			  psArg			- The destination of the moving instruction to
							update.
			  uNewLiveChansInArg
							- Mask of channels written in the destination by
							the moving instruction.

 RETURNS	: The mask of channels live in the argument immediately before the
			  instruction to update.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psInstDest = &psInst->asDest[uDestIdx];
		PARG	psInstOldDest = psInst->apsOldDest[uDestIdx];

		if (psInstDest->uType == psArg->uType && psInstDest->uNumberPreMoe == psArg->uNumberPreMoe)
		{
			IMG_UINT32	uNewLiveChansInDest;

			if (psInstOldDest != NULL)
			{
				ASSERT(psInstOldDest->uType == psArg->uType);
				ASSERT(psInstOldDest->uNumberPreMoe == psArg->uNumberPreMoe);

				uNewLiveChansInDest = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);
			}
			else
			{
				uNewLiveChansInDest = 0;
			}
			uNewLiveChansInDest |= uNewLiveChansInArg;

			ASSERT((psInst->auLiveChansInDest[uDestIdx] & uNewLiveChansInArg) == 0);
			psInst->auLiveChansInDest[uDestIdx] |= uNewLiveChansInArg;
			if (psInst->apsOldDest[uDestIdx] == NULL)
			{
				SetPartiallyWrittenDest(psState, psInst, uDestIdx, psInstDest);
			}

			uLiveChansInDest = uNewLiveChansInDest;
		}
	}

	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		PARG	psSrc = &psInst->asArg[uSrcIdx];

		if (psSrc->uType == psArg->uType && psSrc->uNumberPreMoe == psArg->uNumberPreMoe)
		{
			IMG_UINT32	uLiveChansInArg;

			uLiveChansInArg = GetLiveChansInArg(psState, psInst, uSrcIdx);
			ASSERT((uLiveChansInArg & uNewLiveChansInArg) == 0);
			uLiveChansInDest |= uLiveChansInArg;
		}
	}

	return uLiveChansInDest;
}

static
IMG_VOID UpdateLiveChansInDest(PINTERMEDIATE_STATE psState, PINST psInstToMove, PINST psNewInstLocation)
/*****************************************************************************
 FUNCTION	: UpdateLiveChansInDest

 PURPOSE	: Update the recorded masks of channels used from the destination
			  of each instruction when moving two instructions together to
			  form an MOE repeat.

 PARAMETERS	: psState		- Compiler state
			  psInstToMove	- Instruction which is moving.
			  psNewInstLocation
							- Location of the MOE repeat.

 RETURNS	: Nothing
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInstToMove->uDestCount; uDestIdx++)
	{
		IMG_UINT32	uLiveChansInDest;
		IMG_UINT32	uChansWritten;
		PARG		psDest;
		PINST		psInst;

		uLiveChansInDest = psInstToMove->auLiveChansInDest[uDestIdx];
		uChansWritten = psInstToMove->auDestMask[uDestIdx] & uLiveChansInDest;
		psDest = &psInstToMove->asDest[uDestIdx];

		if (psDest->uType == USC_REGTYPE_DUMMY)
		{
			/*
				Skip instruction destinations which aren't real registers.
			*/
			continue;
		}

		/*
			Update the live channels for the destinations of instructions in between the
			new and old locations.
		*/
		for (psInst = psInstToMove->psPrev; psInst != psNewInstLocation; psInst = psInst->psPrev)
		{	
			uLiveChansInDest = 
				UpdateLiveChansInDestForInst(psState, psInst, uLiveChansInDest, psDest, uChansWritten);
		}

		/*
			Record the new mask of channels live in the moving instruction's destination.
		*/
		psInstToMove->auLiveChansInDest[uDestIdx] = uLiveChansInDest;

		/*
			Update the moving instruction if it is no longer completely defining the destination.
		*/
		if ((uLiveChansInDest & ~psInstToMove->auDestMask[uDestIdx]) != 0)
		{
			if (psInstToMove->apsOldDest[uDestIdx] == NULL)
			{
				SetPartiallyWrittenDest(psState, psInstToMove, uDestIdx, psDest);
			}
		}
	}
}

static
IMG_VOID FreeRepeatGroup(PINTERMEDIATE_STATE psState, PREPEAT_GROUP *ppsElem)
/*****************************************************************************
 FUNCTION	: FreeRepeatGroup

 PURPOSE	: Release memory used for a repeat group

 PARAMETERS	: psState		- Compiler state
			  psElem		- Element to free

 RETURNS	: Nothing
*****************************************************************************/
{
	if (ppsElem != NULL)
	{
		_UscFree(psState, (IMG_PVOID*)ppsElem);
	}
}

static
PREPEAT_GROUP AllocRepeatGroup(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AllocRepeatGroup

 PURPOSE	: Allocate and initialise a repeat group

 PARAMETERS	: psState		- Compiler state

 RETURNS	: A dynamically allocated, initialised repeat group
*****************************************************************************/
{
	PREPEAT_GROUP psRet = UscAlloc(psState, sizeof(REPEAT_GROUP));
	memset(psRet, 0, sizeof(REPEAT_GROUP));

	return psRet;
}

static IMG_BOOL IsReplicatedHardwareConstant(IMG_UINT32				uNumber,
											 IMG_PUINT32			puReplicateCount)
/*****************************************************************************
 FUNCTION	: IsReplicatedHardwareConstant

 PURPOSE	: Checks for a hardware constants where the next N hardware constants
			  have the same value.

 PARAMETERS	: uNumber		- Index of the constant to check.
			  puReplicateCount
							- Returns the length of the range of replicated constants.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (uNumber)
	{
		case EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE:
		{
			*puReplicateCount = min(*puReplicateCount, 2);
			return IMG_TRUE;
		}
		case EURASIA_USE_SPECIAL_CONSTANT_ZERO:
		case EURASIA_USE_SPECIAL_CONSTANT_FLOAT1:
		case EURASIA_USE_SPECIAL_CONSTANT_INT32ONE:
		case EURASIA_USE_SPECIAL_CONSTANT_S16ONE:
		{
			*puReplicateCount = min(*puReplicateCount, 3);
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef enum
{
	MOE_MODE_COMPLEX,
	MOE_MODE_NOINCREMENT,
	MOE_MODE_INCREMENT,
} MOE_MODE;

static MOE_MODE CheckIncrementMode(PMOE_DATA	psMoeData,
								   PARG			psArg)
/*****************************************************************************
 FUNCTION	: CheckIncrementMode

 PURPOSE	: Check if an increment/swizzle state is one of two simple forms:
			  either no increment or increment by a fixed value.

 PARAMETERS	: psMoeData			- MOE state to check.
			  psArg				- Corresponding source/destination argument to
								the instruction.

 RETURNS	: The MOE increment mode.
*****************************************************************************/
{
	IMG_INT32	iIncrementStep;

	if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		/* Increment by one GPI register (=128 bit vector). */
		iIncrementStep = 1;
	}
	else
	{
		/* Increment by two unified store registers (=2x64 bit vector) */
		iIncrementStep = 2;
	}

	if (psMoeData->eOperandMode != MOE_INCREMENT)
	{
		return MOE_MODE_COMPLEX;
	}
	if (psMoeData->u.iIncrement == 0)
	{
		return MOE_MODE_NOINCREMENT;
	}
	else if (psMoeData->u.iIncrement == iIncrementStep)
	{
		return MOE_MODE_INCREMENT;
	}
	else
	{
		return MOE_MODE_COMPLEX;
	}
}

static IMG_BOOL 
CheckSingleIssueVectorIncrementMode(PCANDIDATE_GROUP_DATA	psGroup, 
									PINST					psInst)
/*****************************************************************************
 FUNCTION	: CheckSingleIssueVectorIncrementMode

 PURPOSE	: Checks the repeat/increment mode to use for a repeated single-issue
			  vector instruction.

 PARAMETERS	: psGroup		- Information about the repeated instruction.
			  psInst		- Instruction to check repeat mode for.

 RETURNS	: TRUE if the vector instruction can be repeated.
*****************************************************************************/
{
	PMOE_DATA	psDest = &psGroup->psIncData[0];
	PMOE_DATA	psUSSrc = &psGroup->psIncData[1];
	PMOE_DATA	psGPISrc = &psGroup->psIncData[2];
	MOE_MODE	eUnifiedStoreSrcMode;
	MOE_MODE	eGPISrcMode;
	PINST		psFirstInst = psGroup->psFirstInst;

	if (psGroup->sVec.bDotProduct)
	{
		/*
			We already checked while finding the instructions to include in the repeat that
			the destination register numbers were incrementing in the right pattern.
		*/
	}
	else
	{
		/*
			Check the destination increases by an 128-bit vector on each iteration.
		*/
		if (CheckIncrementMode(psDest, &psFirstInst->asDest[0]) != MOE_MODE_INCREMENT)
		{
			return IMG_FALSE;
		}
	}

	/*
		Single issue vector instructions never use MOE state for the
		destination or source 0.
	*/
	psGroup->uDontCareMask |= MOE_OPERAND_DEST_MASK | MOE_OPERAND_SRC0_MASK;

	/*
		Check for both the unified store and the GPI source whether
		they have either no increment or increment by a 128-bit vector on each iteration.
	*/
	eUnifiedStoreSrcMode = CheckIncrementMode(psUSSrc, &psFirstInst->asArg[VEC_MOESOURCE_ARGINDEX(0)]);
	eGPISrcMode = CheckIncrementMode(psGPISrc, &psFirstInst->asArg[VEC_MOESOURCE_ARGINDEX(1)]);

	/*
		VMAD has two GPI sources: both must have the same
		increment mode.
	*/
	if (psInst->eOpcode == IVMAD4)
	{
		PMOE_DATA	psGPISrc2 = &psGroup->psIncData[3];
		MOE_MODE	eGPISrc2Mode;

		eGPISrc2Mode = CheckIncrementMode(psGPISrc2, &psFirstInst->asArg[VEC_MOESOURCE_ARGINDEX(2)]);
		if (eGPISrc2Mode != eGPISrcMode)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check if pattern of the register numbers for the unified store and GPI sources
		match one of the instruction modes.
	*/
	if (eGPISrcMode == MOE_MODE_INCREMENT && eUnifiedStoreSrcMode == MOE_MODE_INCREMENT)
	{
		/* Increment both the unified store and GPI sources. */
		psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_INCREMENTBOTH;
	}
	else if (eGPISrcMode == MOE_MODE_INCREMENT && eUnifiedStoreSrcMode == MOE_MODE_NOINCREMENT)
	{
		/* Increment just the GPI source. */
		psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_INCREMENTGPI;
	}
	else if (eGPISrcMode == MOE_MODE_NOINCREMENT && eUnifiedStoreSrcMode == MOE_MODE_INCREMENT)
	{
		/* Increment just the unified store source. */
		psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_INCREMENTUS;
	}
	else if (eGPISrcMode == MOE_MODE_NOINCREMENT)
	{
		/* Use the MOE state for the unified store source and don't increment the GPI source. */
		psGroup->sVec.eRepeatMode = SVEC_REPEATMODE_USEMOE;
	}
	else
	{
		return IMG_FALSE;
	}

	if (psGroup->sVec.eRepeatMode == SVEC_REPEATMODE_USEMOE)
	{
		/*
			Set both MOE source 1 and source 2 to the state required
			for the unified store source.
		*/
		psGroup->uArgCount = 4;
		psGroup->psMoeData[2] = *psUSSrc;
		psGroup->psMoeData[3] = *psUSSrc;
	}
	else
	{
		/*
			Ignore the MOE state for both source 1 and source 2.
		*/
		psGroup->uDontCareMask |= MOE_OPERAND_SRC1_MASK | MOE_OPERAND_SRC2_MASK;
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL RemapIncrements(PINTERMEDIATE_STATE		psState,
								PINST					psInst,
								PCANDIDATE_GROUP_DATA	psGroup)
/*****************************************************************************
 FUNCTION	: RemapIncrements

 PURPOSE	: Remaps the per-argument increments/swizzle to correspond to the MOE.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to remap for.
			  psGroup				- On return updated with information about
									the arguments.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOpIdx;    // Operand index

	/*
		Initialize return arguments.
	*/
	psGroup->uSpecialConstMask = 0;
	psGroup->uDontCareMask = 0;
	psGroup->uArgCount = 1;

	/*
		Where some arguments point to a range of hardware constants
		with the same value the size of the range.
	*/
	psGroup->uSpecialConstGroupLimit = 3;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psGroup->bSVecInst)
	{
		/*
			Check if we can use one of the special single
			issue vector increment modes.
		*/
		return CheckSingleIssueVectorIncrementMode(psGroup, psInst);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		For each of the arguments in the MOE order.
	*/
	for (uOpIdx = 1; uOpIdx < USC_MAX_MOE_OPERANDS; uOpIdx++)
	{
		IMG_UINT32 uInstArg;
		IMG_UINT32 uArgIdx = uOpIdx - 1; // Source operand index

		/*
			Map the MOE argument to the index of an argument to the
			instruction or a special value.
		*/
		uInstArg = MoeArgToInstArg(psState, psInst, uArgIdx);
		if (uInstArg == DESC_ARGREMAP_DONTCARE)
		{
			/*
				Add to the mask of MOE arguments we are
				ignoring.
			*/
			psGroup->uDontCareMask |= (1U << uOpIdx);
		}
		else if ((uInstArg & DESC_ARGREMAP_ALIASMASK) != 0)
		{
			IMG_UINT32	uAliasArg = DESC_ARGREMAP_GETALIASOP(uInstArg);

			/*
				Use the same MOE state for this argument and its alias.
			*/
			psGroup->psMoeData[uOpIdx] = psGroup->psMoeData[uAliasArg];

			psGroup->uArgCount = max(psGroup->uArgCount, uOpIdx + 1);
		}
		else
		{
			PARG		psArg;
			PMOE_DATA	psMoeData = &psGroup->psMoeData[uOpIdx];

			ASSERT(uInstArg < psInst->uArgumentCount);
			psArg = &psInst->asArg[uInstArg];

			if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
			{
				/*
					Immediate arguments aren't modified by the MOE. We already
					checked that all the instructions in the repeat are using
					the same immediate value so just ignore the MOE state
					for this argument.
				*/
				ASSERT(psMoeData->eOperandMode == MOE_INCREMENT);
				ASSERT(psMoeData->u.iIncrement == 0);
				psGroup->uDontCareMask |= (1U << uOpIdx);
				continue;
			}

			/*
				Flag arguments which point to the start of a range of hardware
				constants all of which have the same value.
			*/
			if (psArg->uType == USEASM_REGTYPE_FPCONSTANT &&
				psMoeData->eOperandMode == MOE_INCREMENT &&
				psMoeData->u.iIncrement == 0 &&
				IsReplicatedHardwareConstant(psArg->uNumber, &psGroup->uSpecialConstGroupLimit))
			{
				psGroup->uSpecialConstMask |= (1U << uOpIdx);
			}

			/*
				Track how many MOE arguments are actually referenced.
			*/
			psGroup->uArgCount = max(psGroup->uArgCount, uOpIdx + 1);
		}
	}

	return IMG_TRUE;
}

#if defined(SUPPORT_SGX545)
static IMG_BOOL CheckPerInstMoeIncrements(PCANDIDATE_GROUP_DATA	psGroup)
/*****************************************************************************
 FUNCTION	: CheckPerInstMoeIncrements

 PURPOSE	: Check if a set of instructions can be grouped using per-instruction
			  increment.

 PARAMETERS	: psGroup		- Details of the group we are trying to create.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uMOEDestIdx = GetMOEDestIdx(psGroup->psFirstInst);

	/* Ignore the destination if we aren't writing. */
	if (uMOEDestIdx != USC_UNDEF)
	{
		if (psGroup->bDotProduct || psGroup->bDoubleDotProduct)
		{
			IMG_UINT32	uDestNumber;

			uDestNumber = psGroup->psFirstInst->asDest[uMOEDestIdx].uNumber;

			/*
				A dot-product only writes on the last iteration
				and with per-instruction increments

					LAST_DEST = ENCODED_DEST + REPEAT_COUNT

				so check the encoded destination wouldn't
				have to be negative.
			*/
			if (uDestNumber < (psGroup->uInstCount - 1))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				The increment for the destination must be one.
			*/
			if (psGroup->psMoeData[0].eOperandMode == MOE_SWIZZLE)
			{
				return IMG_FALSE;
			}
			if (psGroup->psMoeData[0].u.iIncrement != 1)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		The increment for the source arguments must 0 or 1.
	*/
	for (uArg = 1; uArg < psGroup->uArgCount; uArg++)
	{
		if (psGroup->psMoeData[uArg].eOperandMode == MOE_SWIZZLE)
		{
			return IMG_FALSE;
		}
		if (!(psGroup->psMoeData[uArg].u.iIncrement >= 0 &&
			  psGroup->psMoeData[uArg].u.iIncrement <= 1))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX545) */

static IMG_BOOL NeedSMSLIForDPDestination(PINTERMEDIATE_STATE	psState,
										  PSMLSI_STATE			psMoeState,
										  PCANDIDATE_GROUP_DATA	psGroup,
										  IMG_UINT32			uLastIteration,
										  IMG_PUINT32			puNewDPDestNum)
/*****************************************************************************
 FUNCTION	: NeedSMSLIForDPDestination

 PURPOSE	: Checks if a dot-product grouped instruction can write to the
			  right register given the current MOE state.

 PARAMETERS	: psState		- Compiler state.
			  psMoeState	- Existing MOE state.
			  psGroup		- Information about the set of instruction we are
							trying to group.
			  uLastIteration
							- Index of the last iteration.
			  puNewDPDestNum
							- On return the encoded number for the dot-product
							destination.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_INT32	iDestNumber;
	PMOE_DATA	psMoeData;
	IMG_UINT32	uMOEDestIdx;

	/*
		Need an SMLSI if the MOE state for the destination is unknown.
	*/
	if (psMoeState->pbInvalid[0])
	{
		return IMG_TRUE;
	}

	/*
		For a dotproduct a writeback is only done on the last iteration so check that we can
		encode the required destination register number.
	*/
	psMoeData = &psMoeState->psMoeData[0];
	uMOEDestIdx = GetMOEDestIdx(psGroup->psFirstInst);

	iDestNumber = (IMG_INT32)psGroup->psFirstInst->asDest[uMOEDestIdx].uNumber;
	iDestNumber -= GetRegisterOffsetAtIteration(psState, psMoeData, uLastIteration);
	if (iDestNumber < 0 || iDestNumber >= (IMG_INT32)EURASIA_USE_REGISTER_NUMBER_MAX)
	{
		return IMG_TRUE;
	}

	*puNewDPDestNum = (IMG_UINT32)iDestNumber;
	return IMG_FALSE;
}

static IMG_BOOL IsMOEStateCompatibleWithInstructionRange(PINTERMEDIATE_STATE	psState,
														 PMOE_DATA				psMoeData,
														 PMOE_SMLSI_SITE		psSiteData)
/*****************************************************************************
 FUNCTION	: IsMOEStateCompatibleWithInstructionRange

 PURPOSE	: Checks if the register numbers of the arguments to each instruction
			  in a range can be encoded given a particular MOE state.

 PARAMETERS	: psState			- Compiler state.
			  psMoeData			- MOE increments/swizzle state.
			  psSiteData		- Information about the instructions in the
								range.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMoeArg;

	if(psSiteData->bInCompatibleInst == IMG_TRUE)
	{
		return IMG_FALSE;
	}
	/*
		For each MOE operand if we want to use swizzle mode and the increment applied
		by the swizzle on the first iteration is N then we'll need to subtract N
		from the register number for any instruction argument which uses the same
		operand so the post-MOE register numbers remain the same.
	*/

	for (uMoeArg = 0; uMoeArg < USC_MAX_MOE_OPERANDS; uMoeArg++)
	{
		IMG_UINT32	uMOEOffset = GetRegisterOffsetAtIteration(psState, &psMoeData[uMoeArg], 0 /* uIteration */);

		/*
			If the range contains instructions where different MOE operands correspond
			to the same argument that the adjustment for each operand is the same.
		*/
		if (psSiteData->auArgAlias[uMoeArg] != USC_UNDEF)
		{
			IMG_UINT32	uAliasArg = psSiteData->auArgAlias[uMoeArg];
			IMG_UINT32	uAliasOffset = GetRegisterOffsetAtIteration(psState, &psMoeData[uAliasArg], 0 /* uIteration */);

			if (uMOEOffset != uAliasOffset)
			{
				return IMG_FALSE;
			}
		}

		/*
			Check for all the instructions in between the last repeated instructions and here
			if the register number for any instruction is less than the adjustment needed
		*/
		if (uMOEOffset > psSiteData->auMinOperandNum[uMoeArg])
		{
			return IMG_FALSE;
		}
#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			if(psSiteData->abShaderResultRef[uMoeArg] == IMG_TRUE)
			{
				/* 
					Some MOE argument is contaning a Shader Result reference
				*/
				return IMG_FALSE;
			}
		}
#endif /* defined(OUTPUT_USPBIN) */
	}
	
	return IMG_TRUE;
}

static IMG_BOOL CheckExistingMoeState(PINTERMEDIATE_STATE	psState,
									  PSMLSI_STATE			psMoeState,
									  PCANDIDATE_GROUP_DATA	psGroup)
/*****************************************************************************
 FUNCTION	: CheckExistingMoeState

 PURPOSE	: Checks if a group of instructions can be formed into a repeat
			  given the existing MOE state.

 PARAMETERS	: psState		- Compiler state.
			  psMoeState	- Existing MOE state.
			  psGroup		- Information about the set of instruction we are
							trying to group.

 RETURNS	: TRUE if these instructions can't be grouped using the current
			  MOE state.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	uArg = 0;
	/* Ignore the destination if we aren't writing. */
	if (!HasMOEDest(psGroup->psFirstInst))
	{
		ASSERT((g_psInstDesc[psGroup->psFirstInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0);
		uArg = 1;
	}
	else if (psGroup->bDotProduct || psGroup->bDoubleDotProduct)
	{
		/* Destination gets special treatment for a dotproduct */
		if (NeedSMSLIForDPDestination(psState, psMoeState, psGroup,
									  psGroup->uInstCount - 1,
									  &psGroup->uNewDPDestNum))
		{
			return IMG_TRUE;
		}
		uArg = 1;
	}
	
	for ( ; uArg < psGroup->uArgCount; uArg++)
	{
		PMOE_DATA	psMoeData;

		/*
			Ignore arguments not referenced by the instruction.
		*/
		if (psGroup->uDontCareMask & (1U << uArg))
		{
			continue;
		}
		/*
			Need an SMLSI if the MOE state is unknown.
		*/
		if (psMoeState->pbInvalid[uArg])
		{
			return IMG_TRUE;
		}

		psMoeData = &psMoeState->psMoeData[uArg];
		if (psGroup->uSpecialConstMask & (1U << uArg))
		{
			IMG_UINT32	uMaxOffset;

			/*
				For a special constant (e.g. c48) the next 2 or 4 registers
				hold the same value so if the existing MOE increment is
				0 or 1 we don't need an SMLSI.
			*/
			if (psMoeData->eOperandMode == MOE_INCREMENT)
			{
				if (psMoeData->u.iIncrement < 0)
				{
					return IMG_TRUE;
				}
				uMaxOffset = psMoeData->u.iIncrement * (psGroup->uInstCount - 1);
			}
			else
			{
				IMG_UINT32	uGroup;

				uMaxOffset = 0;
				for (uGroup = 0; uGroup < psGroup->uInstCount; uGroup++)
				{
					IMG_UINT32	uSel;

					uSel = psMoeData->u.s.auSwizzle[uGroup];
					uMaxOffset = max(uMaxOffset, uSel);
				}
			}

			if (uMaxOffset > psGroup->uSpecialConstGroupLimit)
			{
				return IMG_TRUE;
			}
		}
		else
		{
			/*
				Check if the existing MOE state is the same as that needed for this
				argument.
			*/
			if (!EqualMoeData(&psGroup->psMoeData[uArg], psMoeData, 1))
			{
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL SwitchToRepeatSpecialConstArg(PINTERMEDIATE_STATE	psState,
											  PCANDIDATE_GROUP_DATA	psGroup,
											  PMOE_DATA				psCurrentMoeData, 
											  IMG_UINT32			uLastIterationInMask, 
											  IMG_UINT32			uRepeatMask)
/*****************************************************************************
 FUNCTION	: SwitchToRepeatSpecialConstArg

 PURPOSE	: Check if switching to a repeat mask for an MOE repeat would be
			  compatible with an argument in the special constant register
			  bank.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- MOE repeat we are trying to create.
			  psCurrentMoeState	- Currently set MOE state.
			  uLastIterationInMask
								- Highest bit set in the repeat mask.
			  uRepeatMask		- Repeat mask to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uIteration;

	/*
		For each iteration in the mask check the offset to the register number
		wouldn't go outside the block of special constants with the same
		value.
	*/
	for (uIteration = 0; uIteration <= uLastIterationInMask; uIteration++)
	{
		if ((uRepeatMask & (1U << uIteration)) != 0)
		{
			IMG_INT32	iRegisterNumberOffset;

			iRegisterNumberOffset = GetRegisterOffsetAtIteration(psState, psCurrentMoeData, uIteration);
			if (iRegisterNumberOffset < 0 || iRegisterNumberOffset > (IMG_INT32)psGroup->uSpecialConstGroupLimit)
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL SwitchToRepeatArg(PINTERMEDIATE_STATE	psState,
								  PCANDIDATE_GROUP_DATA	psGroup,
								  IMG_UINT32			uArg,
								  PMOE_DATA				psCurrentMoeData,
								  IMG_UINT32			uMaxRepeatMaskLength,
								  IMG_PUINT32			puRepeatMask,
								  IMG_PUINT32			puLastIterationInMask)
/*****************************************************************************
 FUNCTION	: SwitchToRepeatArg

 PURPOSE	: Check if there is a repeat mask which, in conjunction with the
			  currently set MOE state, would give the sequence of register numbers
			  required for an argument in a repeat.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Details of the group we are trying to create.
			  psCurrentMoeState	- Currently set MOE state.
			  uMaxRepeatMaskLength
								- Maximum length of the repeat mask.
			  puRepeatMask		- Returns the repeat mask (if it exists).
		      puLastIterationInMask	
								- Returns the highest set bit in the mask.

 RETURNS	: TRUE if a repeat mask exists.
*****************************************************************************/
{
	IMG_UINT32	uIteration;
	IMG_UINT32	uMaskChan;
	IMG_UINT32	uRepeatMask;
	PMOE_DATA	psGroupMoeData = &psGroup->psMoeData[uArg];

	uRepeatMask = 0;

	uMaskChan = 0;
	for (uIteration = 0; uIteration < psGroup->uInstCount; uIteration++)
	{
		IMG_UINT32	uRequiredOffset;
		IMG_BOOL	bFoundMatchingChannel;

		/*
			Get the offset to the register number required by the repeat on
			this iteration.
		*/
		uRequiredOffset = GetRegisterOffsetAtIteration(psState, psGroupMoeData, uIteration);

		/*
			Check for an iteration which would give the same offset when used with
			the current MOE state.
		*/
		bFoundMatchingChannel = IMG_FALSE;
		for (; uMaskChan < uMaxRepeatMaskLength; uMaskChan++)
		{
			IMG_UINT32	uCurrentOffset = GetRegisterOffsetAtIteration(psState, psCurrentMoeData, uMaskChan);

			if (uCurrentOffset == uRequiredOffset)
			{
				bFoundMatchingChannel = IMG_TRUE;
				break;
			}
		}
		if (!bFoundMatchingChannel)
		{
			return IMG_FALSE;
		}

		/*
			Add the found channel to the repeat mask.
		*/
		uRepeatMask |= (1U << uMaskChan);
		uMaskChan++;
	}

	*puRepeatMask = uRepeatMask;
	*puLastIterationInMask = uMaskChan - 1;
	return IMG_TRUE;
}

static IMG_BOOL SwitchToRepeatMask(PINTERMEDIATE_STATE		psState,
								   PSMLSI_STATE				psMoeState,
								   PCANDIDATE_GROUP_DATA	psGroup,
								   IMG_PUINT32				puRepeatMask)
/*****************************************************************************
 FUNCTION	: SwitchToRepeatMask

 PURPOSE	: Check if using a repeat mask (rather than a repeat count) for
			  a group of instructions would avoid having to insert an SMLSI
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psMoeState	- Existing MOE state.
			  psGroup		- Information about the set of instruction we are
							trying to group.
			  piMoePrevInc	-
			  puRepeatMask	- On success returns the repeat mask to use.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uRepeatMask;
	IMG_UINT32	uMaxRepeatCount;
	IMG_UINT32	uMaxRepeatMaskLength;
	IMG_UINT32	uLastIterationInMask;

	/* Check if this group can use a repeat mask; not available on SGX545. */
	if (!CanUseRepeatMask(psState, psGroup->psFirstInst))
	{
		return IMG_FALSE;
	}

	/*
		If the group has a different predicate for each iteration then we can't switch to
		a mask without changing the predicate numbers.
	*/
	if (psGroup->bPredModN)
	{
		return IMG_FALSE;
	}

	/*
		Convert the maximum repeat count to a mask (one bit per iteration).
	*/
	uMaxRepeatCount = GetMaxRepeatCountInst(psState, psGroup->psFirstInst);
	if (uMaxRepeatCount >= 16)
	{
		uMaxRepeatMaskLength = 4;
	}
	else if (uMaxRepeatCount >= 8)
	{
		uMaxRepeatMaskLength = 3;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check the bit width of the repeat mask supported by
		this instruction can fit all the iterations with at
		least one gap.
	*/
	if (uMaxRepeatMaskLength < (psGroup->uInstCount - 1))
	{
		return IMG_FALSE;
	}

	uRepeatMask = USC_UNDEF;
	uLastIterationInMask = USC_UNDEF;
	for (uArg = 0; uArg < psGroup->uArgCount; uArg++)
	{
		PMOE_DATA	psCurrentMoeData;
		IMG_UINT32	uArgRepeatMask;
		IMG_UINT32	uArgLastIterationInMask;

		/*
			Ignore an unwritten destination. Handle
			dotproduct destinations specially since
			they are only written on the
			last iteration.
		*/
		if (uArg == 0 &&
			(!HasMOEDest(psGroup->psFirstInst) ||
			 psGroup->bDotProduct ||
			 psGroup->bDoubleDotProduct))
		{
			continue;
		}
		/*
			Ignore MOE arguments not used by the instruction.
		*/
		if (psGroup->uDontCareMask & (1U << uArg))
		{
			continue;
		}
		/*
			We'll always need an SMLSI if the MOE state is unknown.
		*/
		if (psMoeState->pbInvalid[uArg])
		{
			return IMG_FALSE;
		}
		/*
			Check arguments using the special constant bank in another
			pass.
		*/
		if (psGroup->uSpecialConstMask & (1U << uArg))
		{
			continue;
		}

		/*
			Look for a repeat mask which would give the right sequence
			of register numbers for this argument when using the
			currently set MOE state.
		*/
		psCurrentMoeData = &psMoeState->psMoeData[uArg];
		if (!SwitchToRepeatArg(psState,
							   psGroup, 
							   uArg,
							   psCurrentMoeData, 
							   uMaxRepeatMaskLength, 
							   &uArgRepeatMask,
							   &uArgLastIterationInMask))
		{
			return IMG_FALSE;
		}

		if (uRepeatMask == USC_UNDEF)
		{
			uRepeatMask = uArgRepeatMask;
			uLastIterationInMask = uArgLastIterationInMask;
		}
		else
		{
			/*
				Check each argument can use the same repeat mask.
			*/
			if (uRepeatMask != uArgRepeatMask)
			{
				return IMG_FALSE;
			}
			ASSERT(uLastIterationInMask == uArgLastIterationInMask);
		}
	}

	/*
		Check for any arguments using the special constant bank if for any iteration the
		register offset would take the register number outside the range of special
		constants with the same value.
	*/
	for (uArg = 0; uArg < psGroup->uArgCount; uArg++)
	{
		if (psGroup->uSpecialConstMask & (1U << uArg))
		{
			PMOE_DATA	psCurrentMoeData;

			psCurrentMoeData = &psMoeState->psMoeData[uArg];
			if (!SwitchToRepeatSpecialConstArg(psState, psGroup, psCurrentMoeData, uLastIterationInMask, uRepeatMask))
			{
				return IMG_FALSE;
			}
		}
	}

	if (HasMOEDest(psGroup->psFirstInst) && (psGroup->bDotProduct || psGroup->bDoubleDotProduct))
	{
		IMG_UINT32	uNewDPDestNum;

		/*
			Check if we can set up the instruction so we write to the right register
			number on the last iteration.
		*/
		if (NeedSMSLIForDPDestination(psState, psMoeState, psGroup, uLastIterationInMask, &uNewDPDestNum))
		{
			return IMG_FALSE;
		}
		else
		{
			psGroup->uNewDPDestNum = uNewDPDestNum;
		}
	}

	*puRepeatMask = uRepeatMask;
	return IMG_TRUE;
}

static IMG_BOOL IsNonZeroSwizzle(PMOE_DATA	psMoeData,
								 IMG_UINT32	uCount)
/*****************************************************************************
 FUNCTION	: IsNonZeroSwizzle

 PURPOSE	: Check if the MOE state for any argument is swizzle mode with the
			  selector for the first iteration not equal to zero.

 PARAMETERS	: psMoeData		- Points to an array of per-argument MOE state.
			  uCount		- Count of arguments to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < uCount; uArg++)
	{
		if (psMoeData[uArg].eOperandMode == MOE_SWIZZLE &&
			psMoeData[uArg].u.s.auSwizzle[0] != 0)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckIncrements(PINTERMEDIATE_STATE		psState,
								PSMLSI_STATE			psMoeState,
								PCANDIDATE_GROUP_DATA	psGroup,
								PMOE_SMLSI_SITE			psSiteData,
								IMG_PBOOL				pbUseMoeSite,
								IMG_PBOOL				pbUseMoe,
								IMG_PBOOL				pbNeedSmlsi,
								IMG_PBOOL				pbUseMask,
								IMG_PUINT32				puRepeatMask)
/*****************************************************************************
 FUNCTION	: CheckIncrements

 PURPOSE	: Check if we should create a group of instructions and whether
			  an SMLSI instruction needs to be inserted before.

 PARAMETERS	: psState			- Compiler state.
			  psMoeState		- Current MOE state.
			  psGroup			- Information about the set of instructions
								we are trying to group.
			  uMinTrigger		-
			  bLastWasGrouped
			  pbUseMoe			- On return FALSE to use per-instruction increments.
								or TRUE to use MOE increments.
			  pbNeedSmlsi		- On return TRUE if an SMSLI must be inserted before
								the group.
			  pbUseMask			- On return TRUE to use a repeat mask.
			  puRepeatMask		- On return the repeat mask to use.

 RETURNS	: TRUE if the instructions should be grouped.
*****************************************************************************/
{
	IMG_UINT32	uMinTrigger;

	/*
		Initialize return data.
	*/
	*pbUseMoe = IMG_TRUE;
	*pbNeedSmlsi = IMG_FALSE;
	*pbUseMask = IMG_FALSE;
	*puRepeatMask = 0;
	*pbUseMoeSite = IMG_FALSE;

	#if defined(SUPPORT_SGX545)
	if (psGroup->bInstMoeIncrement)
	{
		/*
			Check if we can create a group using the per-instruction increments without
			needing an SMLSI.
		*/
		if (CheckPerInstMoeIncrements(psGroup))
		{
			*pbUseMoe = IMG_FALSE;
			return IMG_TRUE;
		}

		/*
			Some instructions don't support MOE increments so don't group if MOE increments
			are required to make a repeat.
		*/
		if (psGroup->bNeverUseMoe)
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	/*
		The test instruction must always use a mask.
	*/
	*pbUseMask = IMG_FALSE;
	if (UseRepeatMaskOnly(psState, psGroup->psFirstInst))
	{
		*pbUseMask = IMG_TRUE;
		*puRepeatMask = (1U << psGroup->uInstCount) - 1;
	}

	/*
		If some of the instructions between the point where the current MOE state
		was set up and here aren't compatible with the current state then don't bother
		trying to use it since we'd need to insert an SMLSI anyway.
	*/
	if (
			psSiteData->psLocation == NULL || 
			IsMOEStateCompatibleWithInstructionRange(psState, psMoeState->psMoeData, psSiteData)
	   )
	{
		/*
			Check if the current instructions can be grouped using the existing MOE state.
		*/	
		if (!CheckExistingMoeState(psState,
								   psMoeState,
								   psGroup))
		{
			return IMG_TRUE;
		}

		/*
			Check if we could get away without an smlsi instruction by inserting gaps in the mask.
		*/
		if (!(*pbUseMask))
		{
			if (SwitchToRepeatMask(psState,
								   psMoeState,
								   psGroup,
								   puRepeatMask))
			{
				*pbUseMask = IMG_TRUE;
				return IMG_TRUE;
			}
		}
	}

	/*
		We'll definitely need to insert an SMLSI instruction.
	*/
	*pbNeedSmlsi = IMG_TRUE;

	/*
		Can we insert the SMLSI instruction after the last repeated
		instruction?
	*/
	*pbUseMoeSite = IMG_TRUE;
	if (psSiteData->psLocation == NULL)
	{
		/* No repeated instructions generated yet. */
		*pbUseMoeSite = IMG_FALSE;
	}
	else
	{
		*pbUseMoeSite = IsMOEStateCompatibleWithInstructionRange(psState, psGroup->psMoeData, psSiteData);
	}

	/*
		We must form a group for a set of DP/DDP instructions.
	*/
	if (psGroup->bDotProduct || psGroup->bDoubleDotProduct)
	{
		return IMG_TRUE;
	}

	/*
		If we can insert an SMLSI instruction for free then reduce the
		number of instructions required to trigger a repeat.
	*/
	uMinTrigger = (*pbUseMoeSite) ? MIN_GROUP_TRIGGER : DEFAULT_GROUP_TRIGGER;
	if (psGroup->uInstCount >= uMinTrigger)
	{
		return IMG_TRUE;
	}

	/*
		Don't form a group.
	*/
	return IMG_FALSE;
}

static IMG_VOID FindPotentialGroup(PINTERMEDIATE_STATE		psState,
								   PCANDIDATE_GROUP_DATA	psGroup,
								   PINST					psFirstInstInGroup,
								   PUSEDEF_RECORD			psUseDef)
/*****************************************************************************
 FUNCTION	: FindPotentialGroup

 PURPOSE	: Check if we should create a group of instructions and whether
			  an SMLSI instruction needs to be inserted before.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Information about the set of instructions
								we are trying to group.
			  psFirstInstInGroup
								- Instruction to find others to group with.
			  psUseDef			- Preallocated USE/DEF record to use.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psCandidateInst;
	PINST		psFirstInstNotInUseDefRecord;
	IMG_UINT32	uMaxRepeatCount;
	IMG_BOOL	bExcludeRAW;
	/*
		The registers defined in the group so far.
	*/
	REGISTER_USEDEF	sGroupDef;

	uMaxRepeatCount = GetMaxRepeatCountInst(psState, psFirstInstInGroup);

	psCandidateInst = psFirstInstInGroup;
	psFirstInstNotInUseDefRecord = psFirstInstInGroup->psNext;

	/*
	   psUseDef: Use-def record for instructions between the instructions which form a group.
	*/
	ClearUseDef(psState, psUseDef);

	/*
		Check for cases where we can't make groups which include an instruction
		which defines a register then another instruction which uses the same
		register.
		
		For asynchronous writeback instructions this is because we need a WDF
		between the define and the use.

		For per-instance mode there is a big pipeline stall between the define
		and the use.
	*/
	bExcludeRAW = IMG_FALSE;
	if (IsAsyncWBInst(psState, psFirstInstInGroup))
	{
		bExcludeRAW = IMG_TRUE;
	}
	if (psState->uNumDynamicBranches > 0)
	{
		bExcludeRAW = IMG_TRUE;
	}

	/*
		Initialise a USE/DEF record to track register written in instructions added to the group.
	*/
	if (bExcludeRAW)
	{
		InitRegUseDef(&sGroupDef);
		InstDef(psState, psFirstInstInGroup, &sGroupDef);
	}

	/*
		Run through instructions until no more fit in group.

			psCurrInst: The instruction to be tested.
			psFirstInst: The first instruction in the group
			psPrevInst: The last instruction to succeed.

	   First a candidate is found: If a dotproduct, the candidate is always
	   psCurrInst. Otherwise, the list of instructions beginning with
	   psCurrInst is searched for an instruction with the same opcode as
	   psFirstInst and which could be moved to the end of the group. (The end
	   of the group of instructions is taken to be marked by psFirstInst, with
	   any other instructions in the group ignored.)

	   When a candidate is found, it is passed to CheckInstGroup for inclusion
	   in the group. If this fails, the group ends otherwise the instruction
	   is marked for removal and the process is repeated with the instruction
	   following the candidate in the list.

	   Flag bContiguous is used so that processing a single block of
	   instructions does not mean that each instruction in the block is tested
	   to see if it can be moved.

	   Efficiency of the search depends on the instructions listed in
	   IsValidSwapInst. The more isntructions rejected by IsValidSwapInst, the
	   sooner the search and grouping will stop.

	   At the end, psNextInst points to the last instruction (psCurrInt) to be
	   tested.
	*/
	for (;;)
	{
		PINST		psIntermediateInst;
		IMG_BOOL	bValidSwapInst;

		/*
			Advance to the next candidate instruction (but not if handling a dotproduct).
		*/
		psCandidateInst = psCandidateInst->psRepeatGroup->psNext;

		/*
			No suitable instructions left?
		*/
		if (psCandidateInst == NULL)
		{
			break;
		}

		if (psCandidateInst->eOpcode != psFirstInstInGroup->eOpcode)
		{
			continue;
		}
		ASSERT(IsValidSwapInst(psCandidateInst));

		/*
			Accumulate a USE/DEF record for all the instructions between the
			current candidate and the instructions it will be in a repeat with.
		*/
		bValidSwapInst = IMG_TRUE;
		for (psIntermediateInst = psFirstInstNotInUseDefRecord;
			 psIntermediateInst != psCandidateInst;
			 psIntermediateInst = psIntermediateInst->psNext)
		{
			/*
				Skip instructions which have already been included in a group.
			*/
			if (psIntermediateInst->psRepeatGroup == NULL)
			{
				continue;
			}

			/*
				Check for an instruction which a future instruction can never be moved
				backwards over. If we find one then we can stop looking.
			*/
			if (!IsValidSwapInst(psIntermediateInst))
			{
				bValidSwapInst = IMG_FALSE;
				break;
			}

			/* Instruction can be moved past, update use-def record */
			ComputeInstUseDef(psState, psUseDef, psIntermediateInst, psIntermediateInst);
		}
		if (!bValidSwapInst)
		{
			break;
		}

		/*
			Save the first instruction after the group not included in the USE/DEF record.
		*/
		psFirstInstNotInUseDefRecord = psCandidateInst;

		/*
		   Check none of the sources used by the candidate instruction are defined by an
		   intermediate instruction.
		*/
		if (InstUseDefined(psState, &psUseDef->sDef, psCandidateInst))
		{
			continue;
		}
		/*
			Check none of the registers defined by the candidate instruction are used by an
			intermediate instruction.
		*/
		if (InstDefReferenced(psState, &psUseDef->sUse, psCandidateInst))
		{
			continue;
		}
		/*
			Check none of the registers defined by the candidate instruction are defined by an
			intermediate instruction.
		*/
		if (InstDefReferenced(psState, &psUseDef->sDef, psCandidateInst))
		{
			continue;
		}

		if (bExcludeRAW)
		{
			/*
				Check that no register used by this instruction is defined by 
				an earlier instruction in the group.
			*/
			if (InstUseDefined(psState, &sGroupDef, psCandidateInst))
			{
				break;
			}
		}

		/*
			Can the instruction be added to this group using any possible MOE increment/swizzle
			state?
		*/
		if (TryAddToGroup(psState, psGroup, psGroup->uInstCount, psFirstInstInGroup, psCandidateInst))
		{
			/*
				Don't add instructions included in the group
				to the USE/DEF record.
			*/
			psFirstInstNotInUseDefRecord = psCandidateInst->psNext;

			/* Instruction will be removed */
			psGroup->apsInst[psGroup->uInstCount] = psCandidateInst;

			/*
				Add the registers efined by this instruction to the overall set for the group.
			*/
			if (bExcludeRAW)
			{
				InstDef(psState, psCandidateInst, &sGroupDef);
			}

			/* Increment the group count */
			psGroup->uInstCount++;
			if (psGroup->uInstCount >= uMaxRepeatCount)
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	/*
		Free memory used to track registers used/defined in the group.
	*/
	if (bExcludeRAW)
	{
		ClearRegUseDef(psState, &sGroupDef);
	}
}

static IMG_VOID GetDotProductMOEState(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  PCANDIDATE_GROUP_DATA	psGroup)
/*****************************************************************************
 FUNCTION	: GetDotProductMOEState

 PURPOSE	: Get the MOE state required for a dotproduct instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Dotproduct instruction.
			  psGroup			- Updated with the required MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIteration;
	IMG_UINT32	uArg;
	IMG_UINT32	uArgCountPerIteration;
	IMG_UINT32	uMOEArgStart;

	if (psInst->eOpcode == IFDDP_RPT)
	{
		uArgCountPerIteration = FDDP_PER_ITERATION_ARGUMENT_COUNT;
		uMOEArgStart = 1;
	}
	else
	{
		uArgCountPerIteration = FDP_RPT_PER_ITERATION_ARGUMENT_COUNT;
		uMOEArgStart = 2;
	}

	psGroup->uInstCount = psInst->u.psFdp->uRepeatCount;

	for (uIteration = 1; uIteration < psInst->u.psFdp->uRepeatCount; uIteration++)
	{
		for (uArg = 0; uArg < uArgCountPerIteration; uArg++)
		{
			PARG		psFirstArg = &psInst->asArg[uArg];
			PARG		psNextArg = &psInst->asArg[uIteration * uArgCountPerIteration + uArg];
			IMG_BOOL	bRet;

			bRet = CheckArg(psState,
							psGroup,
							psFirstArg,
							psNextArg,
							uArg,
							uIteration,
							&psGroup->psMoeData[uMOEArgStart + uArg]);
			ASSERT(bRet);
		}
	}
}

static IMG_BOOL GroupInstructions(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psCodeBlock,
								  PINST					psFirstInst,
								  PSMLSI_STATE			psMoeState,
								  PUSEDEF_RECORD		psUseDef,
								  PMOE_SMLSI_SITE		psSiteData,
								  IMG_BOOL				bInsert)
/*****************************************************************************
 FUNCTION	: GroupInstructions

 PURPOSE	: Check if an instruction could be grouped together with those immediately following it.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- The block containing the instruction to be grouped.
			  psFirstInst			- The instruction to be checked for grouping.
			  psMoeState			- The current state of the MOE increments.
			  bLastWasGrouped		- Whether the instructions preceding psFirstInst was grouped.
			  uMinTrigger			- The minimum number of instruction needed to create group .
			  psUseDef				- A use-def record for use by the routine.
			  bInsert				- If TRUE, actually group the instructions
									  and insert MOE commands; if FALSE, just
									  update the MOE state.

 OUTPUT:      ppsMoeSite			- Where to add any SMLSI instruction.
			  pbHasMoeControl		- True if the function (hypothetically)
										added any MOE control instruction.

 RETURNS	: IMG_TRUE iff a group is formed.

 DESCRIPTION: Collapse a group of one or more instructions, beginning with psFirstInst to
		      a single instruction, pointed to be psFirstInst. May emit a SMLSI
			  instruction, if so the SMLSI is added immediately before psFirstInst.
*****************************************************************************/
{
	IMG_UINT32 uRepeatMask = 0; 	/* The repeat mask */
	IMG_BOOL bUseMask;
	IMG_BOOL bNeedSmlsi;
	IMG_UINT32 uArg;
	IMG_BOOL bUseMoe = IMG_TRUE;
	CANDIDATE_GROUP_DATA sGroup = {0 };
	IMG_BOOL bUseMoeSite;

	/*
		Check for an instruction which can never be repeated.
	*/
	if (!CanRepeatInst(psState, psFirstInst))
	{
		return IMG_FALSE;
	}

	/*
	  Test instructions with repeats can only write the same predicate
	  register as the iteration number.
	*/
	if ((g_psInstDesc[psFirstInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0 &&
		psFirstInst->u.psTest->eAluOpcode != IFDP)
	{
		PARG	psPredDest;
		
		ASSERT(psFirstInst->uDestCount >= TEST_PREDICATE_DESTIDX);
		psPredDest = &psFirstInst->asDest[TEST_PREDICATE_DESTIDX];
		ASSERT(psPredDest->uType == USEASM_REGTYPE_PREDICATE);
		if (psPredDest->uNumber != 0)
		{
			return IMG_FALSE;
		}
	}

	/*
		Initialize information about the candidate group.
	*/
	for (uArg = 0; uArg < (sizeof(sGroup.psMoeData) / sizeof(sGroup.psMoeData[0])); uArg++)
	{
		sGroup.psMoeData[uArg].eOperandMode = MOE_INCREMENT;
		sGroup.psMoeData[uArg].u.iIncrement = 1;

        #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		sGroup.psIncData[uArg].eOperandMode = MOE_INCREMENT;
		sGroup.psIncData[uArg].u.iIncrement = 1;
        #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	sGroup.bPredModN = IMG_FALSE;

	/* Check for the dotproduct special case. */
	sGroup.bDotProduct = IMG_FALSE;
	sGroup.bDoubleDotProduct = IMG_FALSE;
	sGroup.uNewDPDestNum = USC_UNDEF;
	if (
			psFirstInst->eOpcode == IFDP_RPT || 
			psFirstInst->eOpcode == IFDPC_RPT || 
			psFirstInst->eOpcode == IFDP_RPT_TESTPRED ||
			psFirstInst->eOpcode == IFDP_RPT_TESTMASK
	   )
	{
		sGroup.bDotProduct = IMG_TRUE;
	}
	/* Check for the double dotproduct special case. */
	if (psFirstInst->eOpcode == IFDDP_RPT)
	{
		sGroup.bDoubleDotProduct = IMG_TRUE;
	}

	/* Check for no emit */
	if (GetBit(psFirstInst->auFlag, INST_NOEMIT))
	{
		return IMG_FALSE;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Initialize special state for single issue vector instructions.
	*/
	sGroup.bSVecInst = IMG_FALSE;
	if (psFirstInst->eOpcode == IVDP_CP || psFirstInst->eOpcode == IVDP_GPI || psFirstInst->eOpcode == IVDP3_GPI)
	{
		sGroup.bSVecInst = IMG_TRUE;
		if (!SetupVDPInstGroup(psState, &sGroup, psFirstInst))
		{
			return IMG_FALSE;
		}
	}
	else if (psFirstInst->eOpcode == IVADD3 || psFirstInst->eOpcode == IVMUL3 || psFirstInst->eOpcode == IVMAD4)
	{
		sGroup.bSVecInst = IMG_TRUE;
		SetupVMADInstGroup(&sGroup);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if(IsInstReferingShaderResult(psState, psFirstInst))
		{
			/*
				Instruction is referencing a shader result so it can not be
				grouped with other instructions.
			*/
			return IMG_FALSE;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/* Set capabilities */
	sGroup.bInstMoeIncrement = IMG_FALSE;
	sGroup.bNeverUseMoe = IMG_FALSE;

	#if defined (SUPPORT_SGX545)
	/*
	   Check for SGX545 repeats:
	   Increments of 0 or 1 specified by the instruction or standard swizzle mode.
	   Currently, only supported in sgx545.
	*/
	if (SupportsPerInstMoeIncrements(psState, psFirstInst))
	{
		sGroup.bInstMoeIncrement = IMG_TRUE;
	}

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES)
	{
		if (g_psInstDesc[psFirstInst->eOpcode].uFlags & DESC_FLAGS_FARITH16)
		{
			sGroup.bNeverUseMoe = IMG_TRUE;
		}
	}
	#endif /* #if defined(SUPPORT_SGX545) */

	/* bUseMask: Whether to use repeat mask. */
	bUseMask = IMG_FALSE;

	/*
		Initialize the list of instructions in the group.
	*/
	sGroup.uInstCount = 1;
	sGroup.apsInst[0] = psFirstInst;

	if (sGroup.bDotProduct || sGroup.bDoubleDotProduct)
	{
		/*
			For a dotproduct a single intermediate instruction represents all the hardware instructions
			in the repeat. So just extract the MOE state we need to set up from the instruction.
		*/
		GetDotProductMOEState(psState, psFirstInst, &sGroup);
		sGroup.uInstCount = psFirstInst->u.psFdp->uRepeatCount;
	}
	else
	{
		/*
			Look for other instructions which can be grouped together with this one.
		*/
		FindPotentialGroup(psState, &sGroup, psFirstInst, psUseDef);
	}

	/* Return if no instructions can be grouped with this one. */
	if (sGroup.uInstCount == 1)
	{
		return IMG_FALSE;
	}

	sGroup.psFirstInst = psFirstInst;
	sGroup.psLastInst = sGroup.apsInst[sGroup.uInstCount - 1];

	/* Switch the order of the MOE increments around to match the expansion of the instructions. */
	if (!RemapIncrements(psState,
						 psFirstInst,
						 &sGroup))
	{
		return IMG_FALSE;
	}

	/*
		Check if we should create a group and whether we need to insert an SMLSI.
	*/
	if (!CheckIncrements(psState,
						 psMoeState,
						 &sGroup,
						 psSiteData,
						 &bUseMoeSite,
						 &bUseMoe,
						 &bNeedSmlsi,
						 &bUseMask,
						 &uRepeatMask))
	{
		/* Not going to continue with the group, clean up and bail out */
		return IMG_FALSE;
	}

	/* Remove the instructions which have been grouped with the first one. */
	if (!sGroup.bDotProduct && !sGroup.bDoubleDotProduct)
	{
		IMG_UINT32	uInst;
		PINST		psLastGroupChild;

		psLastGroupChild = psFirstInst;
		for (uInst = 1; uInst < sGroup.uInstCount; uInst++)
		{
			PINST psInst = sGroup.apsInst[uInst];
			PINST psRepeatNext = psInst->psRepeatGroup->psNext;
			PINST psRepeatPrev = psInst->psRepeatGroup->psPrev;

			/*
				Remove from the per-opcode lists.
			*/
			if (psRepeatNext != NULL)
				psRepeatNext->psRepeatGroup->psPrev = psRepeatPrev;
			if (psRepeatPrev != NULL)
				psRepeatPrev->psRepeatGroup->psNext = psRepeatNext;

			if (bInsert)
			{
				UpdateLiveChansInDest(psState, psInst, psFirstInst);

				/*
					Remove the grouped instruction from the basic block and add it
					to a list linked from the main instruction.
				*/
				SetAsGroupChild(psState, psFirstInst, psLastGroupChild, psInst);
				psLastGroupChild = psInst;
			}
			else
			{
				/*
					First pass - don't actually modify block. Hence, rather
					than removing the instruction from the block, we mark
					it as "would have been removed" by setting its
					psRepeatGroup to NULL....
				*/
				FreeRepeatGroup(psState, &psInst->psRepeatGroup);
			}
		}
	}

	/* Update the instruction flags with the repeat count/mask */
	if (bInsert)
	{
		if (bUseMask)
		{
			psFirstInst->uMask = uRepeatMask;
			psFirstInst->uRepeat = 0;
		}
		else
		{
			psFirstInst->uMask = 0;
			psFirstInst->uRepeat = sGroup.uInstCount;
		}

		/* Update the predicate mode if we are repeating a predicated instruction. */
		if (sGroup.bPredModN)
		{
			PARG	psPredSrc = psFirstInst->apsPredSrc[0];

			ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
			psPredSrc->uNumber = USC_PREDREG_PNMOD4;
		}

		/*
		   Calculate the destination register for dotproducts
		*/
		if (sGroup.bDotProduct || sGroup.bDoubleDotProduct)
		{
			IMG_UINT32	uMOEDestIdx;

			uMOEDestIdx = GetMOEDestIdx(sGroup.psFirstInst);
			if (uMOEDestIdx != USC_UNDEF)
			{
				PARG	psFirstInstMOEDest;

				psFirstInstMOEDest = &psFirstInst->asDest[uMOEDestIdx];
				
				/*
					If per-instruction MOE increments are used then
					the destination increment is always 1.
				*/
				if (!bUseMoe)
				{
					ASSERT(psFirstInstMOEDest->uNumber >= (sGroup.uInstCount - 1));
					psFirstInstMOEDest->uNumber -= sGroup.uInstCount - 1;
				}
				/*
					Otherwise adjust the destination register number
					for the currently set MOE state.
				*/
				else if (!bNeedSmlsi)
				{
					psFirstInstMOEDest->uNumber = sGroup.uNewDPDestNum;
					ASSERT(psFirstInstMOEDest->uNumber <= EURASIA_USE_REGISTER_NUMBER_MAX);
				}
			}
		}

		if (bUseMoe)
		{
			IMG_BOOL	bIgnoreDest;

			/*
				For dotproducts we already updated the destination register number to
				a value taking into account the MOE state.
			*/
			bIgnoreDest = IMG_FALSE;
			if (sGroup.bDotProduct)
			{
				bIgnoreDest = IMG_TRUE;
			}

			/*
				If we used MOE swizzle mode for any arguments then we might need to
				adjust the register numbers encoded in the instruction.
			*/
			AdjustRegisterNumbersForMOESwizzle(psState, 
											   sGroup.psMoeData, 
											   psFirstInst, 
											   psFirstInst->psNext,
											   IMG_FALSE /* bAdjustToPerInstIncrements */,
											   bIgnoreDest);
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			For single-issue vector instructions set the increment mode encoded
			in the instruction.
		*/
		if (sGroup.bSVecInst)
		{
			psFirstInst->u.psVec->eRepeatMode = sGroup.sVec.eRepeatMode;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	/* Set the instruction moe flag - even if !bInsert, as will be read by RecalcInstRegs */
	SetBit(psFirstInst->auFlag, INST_MOE, bUseMoe ? 1 : 0);

	/*
		Check for a previous repeated instruction but we aren't going to make use
		of the MOE site directly after it.
	*/
	if (psSiteData->psLocation != NULL && (!bUseMoeSite || !bNeedSmlsi))
	{
		/*
			Check if the MOE state used by the previous repeat is compatible with the
			instructions in between it and this instruction.
		*/
		if (IsMOEStateCompatibleWithInstructionRange(psState, psMoeState->psMoeData, psSiteData))
		{
			if (IsNonZeroSwizzle(psMoeState->psMoeData, USC_MAX_MOE_OPERANDS))
			{
				if (bInsert)
				{
					/*
						Modify the register numbers of the instructions inbetween for the previous
						repeat's MOE state.
					*/
					AdjustRegisterNumbersForMOESwizzle(psState, 
													   psMoeState->psMoeData, 
													   psSiteData->psLocation->psNext, 
													   psFirstInst,
													   IMG_TRUE /* bAdjustToPerInstIncrements */,
													   IMG_FALSE /* bIgnoreDest */);
				}
			}
		}
		else
		{
			PINST	psSMLSIResetInst;

			/*
				If the current repeat is using the MOE state set up before the previous repeat 
				then we've already checked the state is compatible with the instructions in between.
			*/
			ASSERT(bNeedSmlsi || !bUseMoe);

			/*
				Insert an SMLSI directly after the previous repeated instruction
				to reset the MOE state to the default
			*/
			ResetMoeOpState(psMoeState);
			if (bInsert)
			{
				psSMLSIResetInst = AllocateInst(psState, NULL);
				MoeToInst(psState, psMoeState, psSMLSIResetInst);
				InsertInstBefore(psState, psCodeBlock, psSMLSIResetInst, psSiteData->psLocation->psNext);
			}
		}
	}

	/* Insert an smia instruction if necessary. */
	if (bUseMoe && bNeedSmlsi)
	{
		MOE_DATA sZeroData = {MOE_INCREMENT, {0}};   /* n.b. sTemp CANNOT be {MOE_INCREMENT, 1} */
		MOE_DATA sDefaultData = USC_MOE_DATA_DEFAULT_VALUE;

		/* Set increment/swizzle values for each operand */
		for (uArg = 0; uArg < sGroup.uArgCount; uArg++)
		{
			if ((sGroup.bDotProduct || sGroup.bDoubleDotProduct) && uArg == 0)
			{
				/* Zero out the MOE data for the destination */
				memcpy(&sGroup.psMoeData[uArg], &sZeroData, sizeof(sZeroData));
			}
			else if (sGroup.uSpecialConstMask & (1U << uArg))
			{
				/* Zero out the MOE data for source which are special constants */
				memcpy(&sGroup.psMoeData[uArg], &sZeroData, sizeof(sZeroData));
			}
			else
			{
				if ((sGroup.uDontCareMask & (1U << uArg)))
				{
					/* Set MOE operands not referenced by the instruction to default values. */
					memcpy(&sGroup.psMoeData[uArg], &sDefaultData, sizeof(sDefaultData));
				}
			}
		}
		/* Handle any extra operands */
		for (; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			if (psMoeState->pbInvalid[uArg])
			{
				/* Reset invalid source operands */
				memcpy(&sGroup.psMoeData[uArg], &sDefaultData, sizeof(sDefaultData));
			}
		}

		/* Update the MOE state with the new data */
		for(uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			memcpy(&psMoeState->psMoeData[uArg],
				   &sGroup.psMoeData[uArg],
				   sizeof(psMoeState->psMoeData[uArg]));
			psMoeState->pbInvalid[uArg] = IMG_FALSE;
		}

		if (bInsert)
		{
			PINST psSmiaInst;

			/* Form the SMLSI instruction */
			#ifdef SRC_DEBUG
			ASSERT(psFirstInst != IMG_NULL);
			#endif
			psSmiaInst = AllocateInst(psState, psFirstInst);

			MoeToInst(psState, psMoeState, psSmiaInst);

			if (bUseMoeSite)
			{
				ASSERT(psSiteData->psLocation != NULL);

				/* Add the SMLSI to the code block immediately after the last repeated instruction. */
				InsertInstBefore(psState, psCodeBlock, psSmiaInst, psSiteData->psLocation->psNext);

				/*
					If any of the MOE operands are using swizzle mode with a non-zero increment on
					iteration 0 then they affect the execution of non-repeated instructions so go
					through the instructions between the last repeat and the current repeat and
					adjust the register numbers they use to compensate.
				*/
				if (IsNonZeroSwizzle(psMoeState->psMoeData, USC_MAX_MOE_OPERANDS))
				{
					AdjustRegisterNumbersForMOESwizzle(psState, 
													   psMoeState->psMoeData, 
													   psSmiaInst->psNext, 
													   psFirstInst,
													   IMG_TRUE /* bAdjustToPerInstIncrements */,
													   IMG_FALSE /* bIgnoreDest */);
				}
			}
			else
			{
				/* Add the SMLSI to the code block immediate before the current instruction. */
				InsertInstBefore(psState, psCodeBlock, psSmiaInst, psFirstInst);
			}
		}
	}
	else if (bInsert && !bUseMoe)
	{
		ASSERT(sGroup.bInstMoeIncrement);
		ASSERT(sGroup.psMoeData[0].u.iIncrement == 1);

		/* if don't need to use MOE control, set up the instruction increment */
		for (uArg = 1; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			IMG_INT32	iIncrement;

			ASSERT(sGroup.psMoeData[uArg].eOperandMode == MOE_INCREMENT);

			iIncrement = sGroup.psMoeData[uArg].u.iIncrement;

			ASSERT(iIncrement == 0 || iIncrement == 1);

			if (iIncrement == 0)
			{
				SetBit(psFirstInst->puSrcInc, uArg - 1, 0);
			}
			else
			{
				SetBit(psFirstInst->puSrcInc, uArg - 1, 1);
			}
		}
	}

	/*
		Start collecting information about whether the next
		repeated instruction can insert any SMLSI needed
		after this instruction.
	*/
	psSiteData->psLocation = psFirstInst;
	psSiteData->bInCompatibleInst = IMG_FALSE;
	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psSiteData->auMinOperandNum[uArg] = USC_UNDEF;
		psSiteData->auArgAlias[uArg] = USC_UNDEF;
#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			psSiteData->abShaderResultRef[uArg] = IMG_FALSE;
		}
#endif	/* defined(OUTPUT_USPBIN) */
	}

	return IMG_TRUE;
}

static IMG_VOID SaveRegNumsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: SaveRegNumsBP

 PURPOSE	: Save the register numbers for each source and destination into
			  another field.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to process.
			  pvNull			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(pvNull);

	/*
		Save the old register numbers before we adjust them for the MOE.
	*/
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		IMG_UINT32	i;

		for (i = 0; i < psInst->uDestCount; i++)
		{
			psInst->asDest[i].uNumberPreMoe = psInst->asDest[i].uNumber;
			if (psInst->apsOldDest[i] != NULL)
			{
				psInst->apsOldDest[i]->uNumberPreMoe = psInst->apsOldDest[i]->uNumber;
			}
		}
		for (i = 0; i < psInst->uArgumentCount; i++)
		{
			psInst->asArg[i].uNumberPreMoe = psInst->asArg[i].uNumber;
		}
	}
}

static IMG_VOID InstUsesFormatControl(PINST		psInst,
									  IMG_PBOOL	pbFmtControl,
									  IMG_PBOOL	pbF16FmtControl)
/*****************************************************************************
 FUNCTION	: InstUsesFormatControl

 PURPOSE	: Checks if an instruction will use the top-bit of the register
			  number to select the register format.

 PARAMETERS	: psInst		- Instruction to check.
			  pbFmtControl	- Returns TRUE if the instruction uses format
							control.
			  pbF16FmtControl
							- Returns TRUE if the format control selects
							between F16 and F32 register formats.

 RETURNS	: Nothing.
*****************************************************************************/
{
	*pbFmtControl = IMG_FALSE;
	if (InstUsesF16FmtControl(psInst) || HasC10FmtControl(psInst))
	{
		IMG_UINT32		uArg;
		UF_REGFORMAT	eFmt;

		if (InstUsesF16FmtControl(psInst))
		{
			eFmt = UF_REGFORMAT_F16;
			*pbF16FmtControl = IMG_TRUE;
		}
		else
		{
			eFmt = UF_REGFORMAT_C10;
			*pbF16FmtControl = IMG_FALSE;
		}

		if (eFmt == UF_REGFORMAT_C10 && psInst->uDestCount > 0 && psInst->asDest[0].eFmt == eFmt)
		{
			*pbFmtControl = IMG_TRUE;
			return;
		}
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			if (psInst->asArg[uArg].eFmt == eFmt)
			{
				*pbFmtControl = IMG_TRUE;
				return;
			}
		}
	}
}

typedef struct _SMBO_ARG_DATA
{
	/*
		The minimum and maximum possible values for the MOE
		base offset for this argument.
	*/
	IMG_UINT32	uMinBaseOffset;
	IMG_UINT32	uMaxBaseOffset;
	/*
		TRUE if the MOE base offset for this argument can have
		any value.
	*/
	IMG_UINT32	bDontCare;

	/*
		Either USC_UNDEF or the index of another MOE operand. If another
		operand then both operands must have exactly the same base
		offset.
	*/
	IMG_UINT32	uAliasArg;
} SMBO_ARG_DATA, *PSMBO_ARG_DATA;

static IMG_VOID GetBaseOffsetRange(PINTERMEDIATE_STATE	psState,
								   PINST				psFirstInst,
								   PINST*				ppsRangeEnd,
								   PSMBO_ARG_DATA		psArgData)
/*****************************************************************************
 FUNCTION	: GetBaseOffsetRange

 PURPOSE	: Gets the range of instructions which can use a single base
			  offset setting.

 PARAMETERS	: psState				- Compiler state.
			  psFirstInst			- Instruction to start the range from.
			  bFormatControl		- TRUE if using the top-bit of the register
									number to select the register format is on
									for this instruction.
			  ppsRangeEnd			- Returns the first instruction not in the
									range.
			  auRegNumMin			- For each argument returns the range of
			  auRegNumMax			register numbers used in the argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	PINST		psInst;

	*ppsRangeEnd = psFirstInst->psNext;

	/*
		Initialize the return data for each argument.
	*/
	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psArgData[uArg].bDontCare = IMG_TRUE;
		psArgData[uArg].uAliasArg = USC_UNDEF;
	}

	/*
		Find all the instructions which can share a single base offset setting.
	*/
	for (psInst = psFirstInst; psInst != NULL; psInst = psInst->psNext)
	{
		SMBO_ARG_DATA	asNewArgData[USC_MAX_MOE_OPERANDS];
		IMG_BOOL		bFormatControl;
		IMG_BOOL		bF16FormatControl;
		PARG			apsArgs[USC_MAX_MOE_OPERANDS];
		IMG_UINT32		auArgIdxs[USC_MAX_MOE_OPERANDS];
		IMG_UINT32		auArgAliases[USC_MAX_MOE_OPERANDS];

		InstUsesFormatControl(psInst, &bFormatControl, &bF16FormatControl);

		for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			/*
				Default to no change in the data for this argument.
			*/
			asNewArgData[uArg] = psArgData[uArg];
		}
	
		/*
			Get the mapping of MOE operands to sources/destinations of this
			instruction.
		*/
		GetArgumentsAffectedByMOESwizzles(psState, 
										  psInst, 
										  IMG_FALSE /* bAdjustToPerInstIncrements */, 
										  apsArgs, 
										  auArgIdxs,
										  auArgAliases);

		for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			PSMBO_ARG_DATA	psOldArgData = &psArgData[uArg];
			PSMBO_ARG_DATA	psNewArgData = &asNewArgData[uArg];
			SMBO_ARG_DATA	sInstArgData;
			PARG			psArg = apsArgs[uArg];
			IMG_UINT32		uMaxAddressableRegNum;
			IMG_UINT32		uArgAlias = auArgAliases[uArg];
			IMG_BOOL		bDest = (uArg == 0) ? IMG_TRUE : IMG_FALSE;

			/*
				Check for an MOE operand which requires the same MOE state as
				a previous operand.
			*/
			if (uArgAlias != USC_UNDEF)
			{
				/*
					For simplicity don't include in a range both instructions which require the same base offset setting
					for multiple MOE operands and instructions which directly reference the aliased operands.
				*/
				if (
						!psOldArgData->bDontCare || 
						(
							psOldArgData->uAliasArg != USC_UNDEF && 
							psOldArgData->uAliasArg != uArgAlias
						)
				   )
				{
					return;
				}
	
				/*
					Flag that this MOE operand must be given exactly the same base
					offset as the previous operand.
				*/
				psNewArgData->uAliasArg = uArgAlias;

				continue;
			}

			/*
				Skip MOE operands not used by the instruction.
			*/
			if (psArg == NULL)
			{
				continue;
			}

			/*
				For simplicity don't include in a range both instructions which require the same base offset setting
				for multiple MOE operands and instructions which directly reference the aliased operands.
			*/
			if (psOldArgData->uAliasArg != USC_UNDEF)
			{
				return;
			}

			/*
				Get the maximum register number which can be directly encoded by this
				instruction.
			*/
			uMaxAddressableRegNum = 
				GetMaxAddressableRegNum(psState,
										psInst, 
										bDest, 
										auArgIdxs[uArg],
										psArg,
										bFormatControl, 
										bF16FormatControl);

			/*
				Get the maximum and minimum base offsets which
				can be used with this instruction argument.
			*/
			sInstArgData.uMaxBaseOffset = psArg->uNumber;
			if (psArg->uNumber < uMaxAddressableRegNum)
			{
				sInstArgData.uMinBaseOffset = 0;
			}
			else
			{
				sInstArgData.uMinBaseOffset = psArg->uNumber - uMaxAddressableRegNum;
			}
			ASSERT(sInstArgData.uMinBaseOffset <= sInstArgData.uMaxBaseOffset);

			if (psOldArgData->bDontCare)
			{
				/*
					If this argument wasn't previously referenced in the range then set all
					the data from this instruction.
				*/
				psNewArgData->bDontCare = IMG_FALSE;
				psNewArgData->uMinBaseOffset = sInstArgData.uMinBaseOffset;
				psNewArgData->uMaxBaseOffset = sInstArgData.uMaxBaseOffset;
			}
			else
			{
				/*
					Finish the range if the range of register numbers is too large to be
					handled by a single base offset setting.
				*/
				if (sInstArgData.uMaxBaseOffset < psNewArgData->uMinBaseOffset ||
					sInstArgData.uMinBaseOffset > psNewArgData->uMaxBaseOffset)
				{
					return;
				}

				/*
					Store the updated data for this argument.
				*/
				psNewArgData->uMinBaseOffset = max(psNewArgData->uMinBaseOffset, sInstArgData.uMinBaseOffset);
				psNewArgData->uMaxBaseOffset = min(psNewArgData->uMaxBaseOffset, sInstArgData.uMaxBaseOffset);
			}
		}

		/*
			We are including this instruction in the range so update the information about
			register numbers used.
		*/
		for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			psArgData[uArg] = asNewArgData[uArg];
		}

		/*
			Set the first instruction not in the range.
		*/
		*ppsRangeEnd = psInst->psNext;
	}
}

static IMG_VOID AdjustArgumentByBaseOffset(PINTERMEDIATE_STATE	psState,
										   PSMBO_STATE			psMoeState,
										   PARG					psArg,
										   IMG_UINT32			uMOEArg)
/*****************************************************************************
 FUNCTION	: AdjustArgumentByBaseOffset

 PURPOSE	: Adjust an instruction argument for the currently set MOE base
			  offset.

 PARAMETERS	: psState			- Compiler state.
			  psMoeState		- Base offset state.
			  psArg				- Argument to adjust.
			  uMOEArg			- MOE operand used by this argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArgBaseOffset;

	uArgBaseOffset = psMoeState->puBaseOffsets[uMOEArg];
	ASSERT(psArg->uNumber >= uArgBaseOffset);
	psArg->uNumber -= uArgBaseOffset;
}

static IMG_VOID CheckBaseOffset(PINTERMEDIATE_STATE psState,
								PCODEBLOCK psBlock,
								PINST psFirstInst,
								PSMBO_STATE psMoeState,
								PINST* ppsNextInst, //*always* set
								IMG_BOOL bInsertInst)
/*****************************************************************************
 FUNCTION	: CheckBaseOffset

 PURPOSE	: Insert any SMBO instruction needed by a range of instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block which contains the instructions to check.
			  psFirstInst		- First instruction in the range.
			  psMoeState		- Current base offset state.
			  ppsNextInst		- Returns the first instruction after the range
								checked.
			  bInsertInst		- If TRUE then the SMBO is actually inserted.
								  If FALSE then only the state is updated.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL		bNeedSMBO = IMG_FALSE;
	IMG_UINT32		uArg;
	IMG_UINT32		puNewBaseOffset[USC_MAX_MOE_OPERANDS];
	PINST			psRangeStart, psRangeEnd;
	PINST			psInst;
	SMBO_ARG_DATA	asRangeArgData[USC_MAX_MOE_OPERANDS];

	/*
		Get the range of instruction to set a single set of base offset values for and
		the range of register numbers used in the instructions.
	*/
	GetBaseOffsetRange(psState, 
					   psFirstInst, 
					   &psRangeEnd, 
					   asRangeArgData);
	psRangeStart = psFirstInst;
	*ppsNextInst = psRangeEnd;

	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		PSMBO_ARG_DATA	psArgData = &asRangeArgData[uArg];

		/*
			Set the default for the new base offset from the
			current state.
		*/
		if (psMoeState->pbInvalidBaseOffsets[uArg])
		{
			puNewBaseOffset[uArg] = 0;
		}
		else
		{
			puNewBaseOffset[uArg] = psMoeState->puBaseOffsets[uArg];
		}

		/*
			Check for an operand which must have the same base offset as another
			operand.
		*/
		if (psArgData->uAliasArg != USC_UNDEF)
		{
			IMG_UINT32	uAliasArg;

			uAliasArg = psArgData->uAliasArg;
			ASSERT(uAliasArg < USC_MAX_MOE_OPERANDS);
			ASSERT(!asRangeArgData[uAliasArg].bDontCare);

			ASSERT(psArgData->bDontCare);

			puNewBaseOffset[uArg] = puNewBaseOffset[uAliasArg];
			if (psMoeState->pbInvalidBaseOffsets[uArg] ||
				psMoeState->puBaseOffsets[uArg] != psMoeState->puBaseOffsets[uAliasArg])
			{
				bNeedSMBO = IMG_TRUE;
			}
		}

		/*
			Skip MOE arguments not used by an instruction in
			the range.
		*/
		if (psArgData->bDontCare)
		{
			continue;
		}
		ASSERT(psArgData->uMinBaseOffset <= psArgData->uMaxBaseOffset);
		ASSERT(psArgData->uMaxBaseOffset < EURASIA_USE_NUM_UNIFIED_REGISTERS);

		/*
			Can the register number for this argument be encoded given
			the current base offset setting?
		*/
		if (
				psMoeState->pbInvalidBaseOffsets[uArg] ||
				psMoeState->puBaseOffsets[uArg] < psArgData->uMinBaseOffset ||
				psMoeState->puBaseOffsets[uArg] > psArgData->uMaxBaseOffset
		   )
		{
			bNeedSMBO = IMG_TRUE;
			/*
				If possible set the base offset to 0 to avoid having to insert
				more SMBOs before future instructions which use lower register
				numbers.
			*/
			if (psArgData->uMinBaseOffset == 0)
			{
				puNewBaseOffset[uArg] = 0;
			}
			else
			{
				puNewBaseOffset[uArg] = psArgData->uMinBaseOffset;
			}
		}
	}

	/*
		Insert an SMBO instruction before the current instruction to set the MOE
		base offsets.
	*/
	if (bNeedSMBO)
	{
		for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			psMoeState->pbInvalidBaseOffsets[uArg] = IMG_FALSE;
			psMoeState->puBaseOffsets[uArg] = puNewBaseOffset[uArg];
		}

		if (bInsertInst)
		{
			PINST psSmboInst;
			psSmboInst = AllocateInst(psState, psFirstInst);

			SetOpcodeAndDestCount(psState, psSmboInst, ISMBO, 0 /* uDestCount */);

			for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
			{
				psSmboInst->asArg[uArg].uType = USEASM_REGTYPE_IMMEDIATE;
				psSmboInst->asArg[uArg].uNumber = puNewBaseOffset[uArg];

			}

			/* Initialise instruction group data. */
			FreeRepeatGroup(psState, &psSmboInst->psRepeatGroup);
			InsertInstBefore(psState, psBlock, psSmboInst, psFirstInst);
		}
	}

	if (bInsertInst)
	{
		/*
			Adjust all the registers number down by the currently set
			base offset values.
		*/
		for (psInst = psRangeStart; psInst != psRangeEnd; psInst = psInst->psNext)
		{
			PARG		apsArgs[USC_MAX_MOE_OPERANDS];
			IMG_UINT32	auArgAliases[USC_MAX_MOE_OPERANDS];

			if (GetBit(psInst->auFlag, INST_NOEMIT))
			{
				continue;
			}

			/*
				Get the mapping of MOE operands to sources/destinations of this
				instruction.
			*/
			GetArgumentsAffectedByMOESwizzles(psState, 
											  psInst, 
											  IMG_FALSE /* bAdjustToPerInstIncrements */, 
											  apsArgs, 
											  NULL /* uArgIdxs */,
											  auArgAliases);

			for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
			{
				IMG_UINT32	uArgAlias = auArgAliases[uArg];
				PARG		psArg = apsArgs[uArg];

				/*
					Where a single argument to the instruction corresponds to multiple
					MOE operands adjust the register number only for the first operand.
				*/
				if (uArgAlias != USC_UNDEF)
				{
					ASSERT(asRangeArgData[uArg].uAliasArg == uArgAlias);
					ASSERT(psMoeState->puBaseOffsets[uArgAlias] == psMoeState->puBaseOffsets[uArg]);
					continue;
				}

				/*
					Skip MOE operands not used by this instruction.
				*/
				if (psArg == NULL)
				{
					continue;
				}

				/*
					Subtract the MOE base offset from the register number.
				*/
				AdjustArgumentByBaseOffset(psState, psMoeState, psArg, uArg);
			}
		}
	}
}

#if defined(OUTPUT_USPBIN)
/*****************************************************************************
 FUNCTION	: RecordSetfcStateForBlock

 PURPOSE	: Record the MOE format control state at the start of a basic-block

 PARAMETERS	: psBlock		- The block in which to record the MOE state
			  psMoeState	- MOE state at the start of the block

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID RecordSetfcStateForBlock(PCODEBLOCK psBlock,
								  PSETFC_STATE psMoeState)
{
	if (psMoeState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_EFO] == FORMAT_CONTROL_FLAG_ON)
	{
		psBlock->bEfoFmtCtl = IMG_TRUE;
	}
	else
	{
		psBlock->bEfoFmtCtl = IMG_FALSE;
	}
	if (psMoeState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_COLOUR] == FORMAT_CONTROL_FLAG_ON)
	{
		psBlock->bColFmtCtl = IMG_TRUE;
	}
	else
	{
		psBlock->bColFmtCtl = IMG_FALSE;
	}
}

/*****************************************************************************
 FUNCTION	: RecordSmboStateForBlock

 PURPOSE	: Record the MOE base offsets state at the start of a basic-block

 PARAMETERS	: psBlock		- The block in which to record the MOE state
			  psMoeState	- MOE state at the start of the block

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID RecordSmboStateForBlock(PCODEBLOCK psBlock,
								 PSMBO_STATE psMoeState)
{
	IMG_UINT32	uArg;

	for	(uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psBlock->auMoeBaseOffset[uArg]	= psMoeState->puBaseOffsets[uArg];
	}
}

/*****************************************************************************
 FUNCTION	: RecordSmlsiStateForBlock

 PURPOSE	: Record the MOE increment state at the start of a basic-block

 PARAMETERS	: psBlock		- The block in which to record the MOE state
			  psMoeState	- MOE state at the start of the block

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID RecordSmlsiStateForBlock(PCODEBLOCK psBlock,
								  PSMLSI_STATE psMoeState)
{
	IMG_UINT32	uArg;

	for	(uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psBlock->asMoeIncSwiz[uArg]		= psMoeState->psMoeData[uArg];
	}
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID FreeGroupInstState(PINTERMEDIATE_STATE psState,
							PGROUPINST_STATE psGIState)
/*****************************************************************************
 FUNCTION	: FreeGroupInstState

 PURPOSE	: Finalise and free instruction grouping data

 PARAMETERS	: psState		- Compiler state.

 OUTPUT		: ppsGIState	- Instruction grouping state

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psGIState->asFunc);
	ClearUseDef(psState, &psGIState->sUseDef);
	UscFree(psState, psGIState);
}

static IMG_UINT32 CheckPossibleSwizzle(PINTERMEDIATE_STATE psState,
									   IMG_UINT32 uArgCount,
									   IMG_UINT32 puArgMap[][USC_MAX_MOE_OPERANDS],
									   IMG_UINT32 puArgMin[],
									   IMG_UINT32 uInstCount)
/*****************************************************************************
 FUNCTION	: CheckPossibleSwizzle

 PURPOSE	: Check if a sequence of register number could be generated by
			  an MOE increment or swizzle.

 PARAMETERS	: uArgCount			- Count of the arguments to check.
			  puArgMap			- Register numbers for each argument.
			  puArgMin			- Minimum register number for each argument.
			  uInstCount		- Sequence length.

 RETURNS	: The maximum sequence length.
*****************************************************************************/
{
	IMG_UINT32	uArg, uMaxRepeat;
	IMG_INT32	iMinIncrement, iMaxIncrement;

#if !defined(SUPPORT_SGX545)
	PVR_UNREFERENCED_PARAMETER(psState);
#endif

#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PER_INST_MOE_INCREMENTS)
	{
		uMaxRepeat = SGX545_USE1_FLOAT_RCOUNT_MAXIMUM;

		iMinIncrement = USC_MIN_SMALL_INCREMENT_INC;
		iMaxIncrement = USC_MAX_SMALL_INCREMENT_INC;
	}
	else
#endif /* defined(SUPPORT_SGX545) */
	{
		uMaxRepeat = EURASIA_USE_MAXIMUM_REPEAT;

		iMinIncrement = USC_MIN_INCREMENT_INC;
		iMaxIncrement = USC_MAX_INCREMENT_INC;
	}
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		IMG_UINT32	uInst, uIncrMax, uSwizMax;
		IMG_INT32	piGap[USC_MAX_MOE_OPERANDS];

		/*
			Check if an increment is possible - if the first register number is M then we
			need G so the nth register number is M + Gn
		*/
		piGap[uArg] = puArgMap[1][uArg] - puArgMap[0][uArg];
		if (piGap[uArg] > iMaxIncrement || piGap[uArg] < iMinIncrement)
		{
			uIncrMax = 0;
		}
		else
		{
			for (uInst = 2; uInst < uInstCount; uInst++)
			{
				if (puArgMap[uInst][uArg] != ((piGap[uArg] * uInst) + puArgMap[0][uArg]))
				{
					break;
				}
			}
			uIncrMax = min(uInst, uInstCount);
		}

		/*
			Check if a swizzle is possible - the maximum range of register numbers needs
			to be 3 or less.
		*/
		for (uInst = 0; uInst < min(uInstCount, 4); uInst++)
		{
			if ((puArgMap[uInst][uArg] - puArgMin[uArg]) > USC_MAX_SWIZZLE_INC)
			{
				break;
			}
		}
		uSwizMax = min(uInst, uInstCount);

		/*
			The overall repeat is maximum available through either increment or swizzle mode.
		*/
		uMaxRepeat = min(uMaxRepeat, max(uIncrMax, uSwizMax));
	}

	return uMaxRepeat;
}

static IMG_UINT32 GetMOERegisterNumber(PINTERMEDIATE_STATE psState,
									   PINST psNextInst,
									   IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: GetMOERegisterNumber

 PURPOSE	: Get the register number for an argument as it would be encoded
			  in the hardware instruction.

 PARAMETERS	: psNextInst			- Instruction to get the register number from.
			  uArg					- Argument index to get the register number from.

 RETURNS	: The register number.
*****************************************************************************/
{
	IMG_UINT32	uNumber;
	PARG		psArg = &psNextInst->asArg[uArg];

	if (psArg->eFmt == UF_REGFORMAT_F16)
	{
		IMG_UINT32	uComponent;

		uNumber = psArg->uNumber << EURASIA_USE_FMTF16_REGNUM_SHIFT;
		uComponent = GetComponentSelect(psState, psNextInst, uArg);
		ASSERT(uComponent == 0 || uComponent == 2);
		if (uComponent == 2)
		{
			uNumber |= EURASIA_USE_FMTF16_SELECTHIGH;
		}
	}
	else
	{
		uNumber = psArg->uNumber;
	}
	return uNumber;
}

static IMG_BOOL EqualArgModes(PINTERMEDIATE_STATE	psState, 
							  PINST					psInst1, 
							  IMG_UINT32			uArg1Idx, 
							  PINST					psInst2, 
							  IMG_UINT32			uArg2Idx)
/*****************************************************************************
 FUNCTION	: EqualArgModes

 PURPOSE	: Check if two arguments are equal apart from the register number.

 PARAMETERS	: psState			- Compiler state.
			  psInst1			- First argument to compare.
			  uArg1Idx
			  psInst2			- Second argument to compare.
			  uArg2Idx

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG					psArg1 = &psInst1->asArg[uArg1Idx];
	PARG					psArg2 = &psInst2->asArg[uArg2Idx];
	PFLOAT_SOURCE_MODIFIER	psMod1;
	PFLOAT_SOURCE_MODIFIER	psMod2;

	psMod1 = GetFloatMod(psState, psInst1, uArg1Idx);
	psMod2 = GetFloatMod(psState, psInst2, uArg2Idx);

	if (psArg1->uType != psArg2->uType ||
		psArg1->uIndexType != psArg2->uIndexType ||
		psArg1->uIndexNumber != psArg2->uIndexNumber ||
		psArg1->uIndexStrideInBytes != psArg2->uIndexStrideInBytes ||
		psArg1->eFmt != psArg2->eFmt)
	{
		return IMG_FALSE;
	}
	if (psMod1->bNegate != psMod2->bNegate || psMod1->bAbsolute != psMod2->bAbsolute)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static IMG_VOID CopyArgumentToDP(PINTERMEDIATE_STATE	psState,
								 PINST					psDPInst,
								 IMG_UINT32				uDestArg,
								 PINST					psSrcInst,
								 IMG_UINT32				uSrcArg)
/*****************************************************************************
 FUNCTION	: CopyArgumentToDP

 PURPOSE	: Copy an argument from a floating point instruction to a FDP_RPT/FDDP
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDPInst			- Instruction to copy to.
			  uDestArg			- Argument to copy to.
			  psSrcInst			- Instruction to copy from.
			  uSrcArg			- Argument to copy from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	MoveSrc(psState, psDPInst, uDestArg, psSrcInst, uSrcArg);
	SetComponentSelect(psState, psDPInst, uDestArg, GetComponentSelect(psState, psSrcInst, uSrcArg));
}

static IMG_VOID CopyNegateAbsoluteModifierToFDP(PINTERMEDIATE_STATE psState, 
												PINST				psDPInst, 
												IMG_UINT32			uArgCountPerIteration, 
												PINST				psFloatInst)
/*****************************************************************************
 FUNCTION	: CopyNegateAbsoluteModifierToFDP

 PURPOSE	: Copy the negate and absolute source modifiers for each argument from
			  a floating point instruction to a FDP_RPT or FDDP instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDPInst			- Instruction to copy to.
			  uArgCountPerIteration
								- Number of source arguments read on each iteration.
			  psFloatInst		- Instruction to copy from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < uArgCountPerIteration; uArg++)
	{
		PFLOAT_SOURCE_MODIFIER	psMod = GetFloatMod(psState, psFloatInst, uArg);
	
		psDPInst->u.psFdp->abNegate[uArg] = psMod->bNegate;
		psDPInst->u.psFdp->abAbsolute[uArg] = psMod->bAbsolute;
	}
}

static IMG_BOOL GenerateFDP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psFirstInst, PINST* ppsNextInst)
/*****************************************************************************
 FUNCTION	: GenerateFDP

 PURPOSE	: Generate dotproduct instructions.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Block containing the first instruction.
			  psInst				- First instruction in the DP sequence.
			  ppsNextInst			- Updated with the first valid instruction
									after psInst.

 RETURNS	: TRUE if a dotproduct was generated.
*****************************************************************************/
{
	PINST		psNextInst, psPrevInst;
	IMG_UINT32	uDotCount;
	IMG_UINT32	uRepeatCount;
	IMG_UINT32	uArg;
	IMG_BOOL	abSwapArgs[EURASIA_USE_MAXIMUM_REPEAT];
	PARG		psAccumSrc;
	PINST		psFDPInst;
	IMG_UINT32	uInst;
	PINST		psNextNextInst;
	IMG_UINT32	puArgMap[EURASIA_USE_MAXIMUM_REPEAT][USC_MAX_MOE_OPERANDS];
	IMG_UINT32	puArgMin[USC_MAX_MOE_OPERANDS];

	uDotCount = 1;
	abSwapArgs[0] = IMG_FALSE;

	/*
		Get the register numbers for the first instruction.
	*/
	for (uArg = 0; uArg < FDP_RPT_PER_ITERATION_ARGUMENT_COUNT; uArg++)
	{
		puArgMap[0][uArg] = puArgMin[uArg] = GetMOERegisterNumber(psState, psFirstInst, uArg);
	}

	for (psNextInst = psFirstInst->psNext, psPrevInst = psFirstInst;
		 psNextInst != NULL;
		 psPrevInst = psNextInst, psNextInst = psNextInst->psNext)
	{
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			if(IsInstReferingShaderResult(psState, psNextInst))
			{
				break;
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */

		/*
			Check for different instruction modes.
		*/
		if (!(psNextInst->eOpcode == IFMAD && EqualPredicates(psNextInst, psFirstInst)))
		{
			break;
		}

		/*
			Check the arguments are of the same type and have the same modes.
		*/
		if (EqualArgModes(psState, psNextInst, 0 /* uArgIdx */, psFirstInst, 0 /* uArgIdx */) &&
			EqualArgModes(psState, psNextInst, 1 /* uArgIdx */, psFirstInst, 1 /* uArgIdx */))
		{
			abSwapArgs[uDotCount] = IMG_FALSE;
		}
		else if (EqualArgModes(psState, psNextInst, 0 /* uArgIdx */, psFirstInst, 1 /* uArgIdx */) &&
				 EqualArgModes(psState, psNextInst, 1 /* uArgIdx */, psFirstInst, 0 /* uArgIdx */))
		{
			abSwapArgs[uDotCount] = IMG_TRUE;
		}
		else
		{
			break;
		}

		/*
			Check the third source to theMAD instruction is the same as the 
			result of the previous dotproduct stage.
		*/
		psAccumSrc = &psNextInst->asArg[2];
		if (!EqualArgs(psAccumSrc, &psPrevInst->asDest[0]))
		{
			break;
		}

		/*
			Check the third source to the next MAD instruction has no source modifiers.
		*/
		if (HasNegateOrAbsoluteModifier(psState, psNextInst, 2 /* uArgIdx */))
		{
			break;
		}

		/*
			Check the result of the previous dotproduct stage is used only here.
		*/
		if (!UseDefIsSingleSourceRegisterUse(psState, psNextInst, 2 /* uSrcIdx */))
		{
			break;
		}

		/*
			Save the register numbers.
		*/
		for (uArg = 0; uArg < FDP_RPT_PER_ITERATION_ARGUMENT_COUNT; uArg++)
		{
			IMG_UINT32	uNumber;
			IMG_UINT32	uSrcArg;

			if (abSwapArgs[uDotCount])
			{
				uSrcArg = 1 - uArg;
			}
			else
			{
				uSrcArg = uArg;
			}
			uNumber = GetMOERegisterNumber(psState, psNextInst, uSrcArg);

			puArgMap[uDotCount][uArg] = uNumber;
			puArgMin[uArg] = min(puArgMin[uArg], uNumber);
		}
		uDotCount++;
		if (uDotCount == EURASIA_USE_MAXIMUM_REPEAT)
		{
			break;
		}
	}
	if (uDotCount <= 1)
	{
		return IMG_FALSE;
	}

	/*
		Check if we could group the dotproducts using the MOE.
	*/
	uRepeatCount = 
		CheckPossibleSwizzle(psState, 
							 FDP_RPT_PER_ITERATION_ARGUMENT_COUNT, 
							 puArgMap, 
							 puArgMin, 
							 uDotCount);
	if (uRepeatCount <= 1)
	{
		return IMG_FALSE;
	}	

	/*
		Create the new dotproduct instruction.
	*/
	psFDPInst = AllocateInst(psState, psFirstInst);
	SetOpcode(psState, psFDPInst, IFDP_RPT);

	/*
		Insert before the first instruction in the sequence.
	*/
	InsertInstBefore(psState, psBlock, psFDPInst, psFirstInst);

	#if defined(SRC_DEBUG)
	/*
		Flag that this instruction will cost the same number of cycles as the number of
		iterations in the repeat.
	*/
	IncrementCostCounter(psState, psFDPInst->uAssociatedSrcLine, uRepeatCount - 1);
	#endif /* defined(SRC_DEBUG) */

	/*
		Copy the source predicate from the first instruction in the sequence.
	*/
	CopyPredicate(psState, psFDPInst, psFirstInst);

	/*
		Set the number of iterations.	
	*/
	psFDPInst->u.psFdp->uRepeatCount = uRepeatCount;

	/*
		Copy the source modifiers from the first instruction in the sequence. 
	*/
	CopyNegateAbsoluteModifierToFDP(psState, psFDPInst, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT, psFirstInst);

	/*
		Copy the SKIPINVALID flag from the first instruction in the sequence. Because the result of each instruction
		is only used in the next instruction in the dotproduct sequence the SKIPINV flag must be the same for all
		instructions.
	*/
	SetBit(psFDPInst->auFlag, INST_SKIPINV, GetBit(psFirstInst->auFlag, INST_SKIPINV));

	for (uInst = 0, psNextInst = psFirstInst;
		 uInst < uRepeatCount;
		 uInst++, psNextInst = psNextNextInst)
	{
		IMG_UINT32	uIterationArgBase;

		psNextNextInst = psNextInst->psNext;

		ASSERT(GetBit(psFDPInst->auFlag, INST_SKIPINV) == GetBit(psNextInst->auFlag, INST_SKIPINV));

		uIterationArgBase = uInst * FDP_RPT_PER_ITERATION_ARGUMENT_COUNT;

		/*
			Copy the source arguments from the instruction for this iteration
			of the dotproduct.
		*/
		if (abSwapArgs[uInst])
		{
			ASSERT(uInst > 0);
			CopyArgumentToDP(psState, psFDPInst, uIterationArgBase + 0, psNextInst, 1 /* uSrcArg */);
			CopyArgumentToDP(psState, psFDPInst, uIterationArgBase + 1, psNextInst, 0 /* uSrcArg */);
		}
		else
		{
			CopyArgumentToDP(psState, psFDPInst, uIterationArgBase + 0, psNextInst, 0 /* uSrcArg */);
			CopyArgumentToDP(psState, psFDPInst, uIterationArgBase + 1, psNextInst, 1 /* uSrcArg */);
		}

		/*
			Copy the dotproduct destination from the last instruction in the sequence.
		*/
		if (uInst == (uRepeatCount - 1))
		{
			MoveDest(psState, psFDPInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		}

		/*
			Free the old instruction.
		*/
		if ((*ppsNextInst) == psNextInst)
		{
			(*ppsNextInst) = psNextInst->psNext;
		}
		RemoveInst(psState, psBlock, psNextInst);
		FreeInst(psState, psNextInst);
	}

	return IMG_TRUE;
}

static IMG_BOOL GenerateFDDP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psFirstInst, PINST* ppsNextInst)
/*****************************************************************************
 FUNCTION	: GenerateFDDP

 PURPOSE	: Generate double dotproduct instructions.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Block containing the first instruction.
			  psInst				- First instruction in the DDP sequence.
			  ppsNextInst			- Updated with the first valid instruction
									after psInst.

 RETURNS	: TRUE if a double dotproduct was generated.
*****************************************************************************/
{
	EFO_SRC						eOtherSrc;
	IMG_UINT32					uIRegDestNum;
	IMG_BOOL					bRotateArgs = IMG_FALSE;
	PINST						psNextInst;
	IMG_BOOL					bLast = IMG_FALSE;
	IMG_UINT32					uDotCount;
	IMG_BOOL					bSwapArgs = IMG_FALSE;
	IMG_UINT32					uArg;
	PINST						psFDDPInst;
	PINST						psNextNextInst;
	IMG_UINT32					uInst;
	IMG_UINT32 const*			puArgRemap;
	IMG_UINT32					puArgMap[EURASIA_USE_MAXIMUM_REPEAT][USC_MAX_MOE_OPERANDS];
	IMG_UINT32					puArgMin[USC_MAX_MOE_OPERANDS];
	static const IMG_UINT32		auNoRotateNoSwap[FDDP_PER_ITERATION_ARGUMENT_COUNT] = {0, 1, 2};
	static const IMG_UINT32		auRotateNoSwap[FDDP_PER_ITERATION_ARGUMENT_COUNT]	= {2, 0, 1};
	static const IMG_UINT32		auNoRotateSwap[FDDP_PER_ITERATION_ARGUMENT_COUNT]	= {0, 2, 1};
	static const IMG_UINT32		auRotateSwap[FDDP_PER_ITERATION_ARGUMENT_COUNT]		= {2, 1, 0};

	#if defined(OUTPUT_USPBIN)
	if(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if(IsInstReferingShaderResult(psState, psFirstInst))
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Check that the EFO is suitable to be the first stage in a DDP.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) == 0 &&
		psFirstInst->u.psEfo->bIgnoreDest &&
		psFirstInst->u.psEfo->bWriteI0 &&
		psFirstInst->u.psEfo->bWriteI1 &&
		psFirstInst->u.psEfo->eI0Src == M0 &&
		psFirstInst->u.psEfo->eI1Src == M1 &&
		psFirstInst->u.psEfo->eM0Src0 == SRC0 &&
		psFirstInst->u.psEfo->eM0Src1 == SRC1 &&
		psFirstInst->u.psEfo->eM1Src0 == SRC0 &&
		psFirstInst->u.psEfo->eM1Src1 == SRC2)
	{
	}
	else if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NEW_EFO_OPTIONS) != 0 &&
			 psFirstInst->u.psEfo->bIgnoreDest &&
			 psFirstInst->u.psEfo->bWriteI0 &&
			 psFirstInst->u.psEfo->bWriteI1 &&
			 psFirstInst->u.psEfo->eI0Src == M0 &&
			 psFirstInst->u.psEfo->eI1Src == M1 &&
			 psFirstInst->u.psEfo->eM0Src0 == SRC0 &&
			 psFirstInst->u.psEfo->eM0Src1 == SRC2 &&
			 psFirstInst->u.psEfo->eM1Src0 == SRC1 &&
			 psFirstInst->u.psEfo->eM1Src1 == SRC2 &&
			 CanUseSource0(psState, psBlock->psOwner->psFunc, &psFirstInst->asArg[2]))
	{
		bRotateArgs = IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Save the register numbers for the first instruction.
	*/
	for (uArg = 0; uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT; uArg++)
	{
		puArgMap[0][uArg] = puArgMin[uArg] = GetMOERegisterNumber(psState, psFirstInst, uArg);
	}

	uDotCount = 1;
	for (psNextInst = psFirstInst->psNext; psNextInst != NULL; psNextInst = psNextInst->psNext)
	{
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			if(IsInstReferingShaderResult(psState, psNextInst))
			{
				break;
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */

		if (psNextInst->eOpcode != IEFO)
		{
			break;
		}

		/*
			Check for different argument types or modes.
		*/
		for (uArg = 0; uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT; uArg++)
		{
			if (!EqualArgModes(psState, psNextInst, uArg, psFirstInst, uArg))
			{
				break;
			}
		}
		if (uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT)
		{
			break;
		}

		/*
			Check for a different instruction predicate.
		*/
		if (!EqualPredicates(psNextInst, psFirstInst))
		{
			break;
		}
		/*
			Check that the EFO has the right modes to be the next stage in a DDP.
		*/
		if (!IsDoubleDotProduct(psNextInst, &bLast, bRotateArgs))
		{
			break;
		}
		/*
			Save the register numbers.
		*/
		for (uArg = 0; uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT; uArg++)
		{
			IMG_UINT32	uNumber;

			uNumber = GetMOERegisterNumber(psState, psNextInst, uArg);

			puArgMap[uDotCount][uArg] = uNumber;
			puArgMin[uArg] = min(puArgMin[uArg], uNumber);
		}
		uDotCount++;
		if (uDotCount == FDDP_MAXIMUM_REPEAT)
		{
			break;
		}
		if (bLast)
		{
			break;
		}
	}
	if (!bLast || uDotCount <= 1)
	{
		return IMG_FALSE;
	}

	/* Check if we need to swap the arguments to a double dotproduct. */
	eOtherSrc = psNextInst->u.psEfo->bWriteI0 ? psNextInst->u.psEfo->eI0Src : psNextInst->u.psEfo->eI1Src;
	if (eOtherSrc == A0)
	{
		ASSERT(psNextInst->u.psEfo->eDSrc == A1);
		bSwapArgs = IMG_TRUE;
	}

	uIRegDestNum = psNextInst->u.psEfo->bWriteI0 ? 0 : 1;

	/*
		Check if we can group the EFOs into a DDP using the MOE.
	*/
	if (CheckPossibleSwizzle(psState, FDDP_PER_ITERATION_ARGUMENT_COUNT, puArgMap, puArgMin, uDotCount) != uDotCount)
	{
		return IMG_FALSE;
	}

	if (bRotateArgs)
	{
		puArgRemap = bSwapArgs ? auRotateSwap : auRotateNoSwap;
	}
	else
	{
		puArgRemap = bSwapArgs ? auNoRotateSwap : auNoRotateNoSwap;
	}

	/*
		Create the new dotproduct instruction.
	*/
	psFDDPInst = AllocateInst(psState, psFirstInst);
	SetOpcodeAndDestCount(psState, psFDDPInst, IFDDP_RPT, 2 /* uNewDestCount */);

	/*
		Insert before the first instruction in the sequence.
	*/
	InsertInstBefore(psState, psBlock, psFDDPInst, psFirstInst);

	#if defined(SRC_DEBUG)
	/*
		Flag that this instruction will cost the same number of cycles as the number of
		iterations in the repeat.
	*/
	IncrementCostCounter(psState, psFDDPInst->uAssociatedSrcLine, uDotCount - 1);
	#endif /* defined(SRC_DEBUG) */

	/*
		Copy the source predicate from the first instruction in the sequence.
	*/
	CopyPredicate(psState, psFDDPInst, psFirstInst);

	/*
		Set the number of iterations.	
	*/
	psFDDPInst->u.psFdp->uRepeatCount = uDotCount;

	/*
		Copy the source modifiers from the first instruction in the sequence. 
	*/
	for (uArg = 0; uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT; uArg++)
	{
		PFLOAT_SOURCE_MODIFIER	psMod = GetFloatMod(psState, psFirstInst, puArgRemap[uArg]);
	
		psFDDPInst->u.psFdp->abNegate[uArg] = psMod->bNegate;
		psFDDPInst->u.psFdp->abAbsolute[uArg] = psMod->bAbsolute;
	}

	/*
		Copy the destination for the first dot-product result from the
		last instruction in the sequence.
	*/
	MoveDest(psState, psFDDPInst, 0 /* uMoveDestIdx */, psNextInst, EFO_US_DEST /* uMoveFromDestIdx */);
	psFDDPInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[EFO_US_DEST];

	/*
		Add the internal register for the second dot-product result
		as an extra destination.
	*/
	SetDest(psState, psFDDPInst, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIRegDestNum, UF_REGFORMAT_F32);
	psFDDPInst->auLiveChansInDest[1] = USC_ALL_CHAN_MASK;

	/*
		Copy the SKIPINVALID flag from the first instruction in the sequence.
	*/
	SetBit(psFDDPInst->auFlag, INST_SKIPINV, GetBit(psFirstInst->auFlag, INST_SKIPINV));

	for (uInst = 0, psNextInst = psFirstInst; uInst < uDotCount; uInst++, psNextInst = psNextNextInst)
	{
		IMG_UINT32	uIterationArgBase;

		psNextNextInst = psNextInst->psNext;

		ASSERT(GetBit(psFDDPInst->auFlag, INST_SKIPINV) == GetBit(psNextInst->auFlag, INST_SKIPINV));

		uIterationArgBase = + uInst * FDDP_PER_ITERATION_ARGUMENT_COUNT;

		/*
			Copy the source arguments from the instruction for this iteration
			of the double dotproduct.
		*/	
		for (uArg = 0; uArg < FDDP_PER_ITERATION_ARGUMENT_COUNT; uArg++)
		{
			CopyArgumentToDP(psState, psFDDPInst, uIterationArgBase + uArg, psNextInst, puArgRemap[uArg] /* uSrcArg */);
		}

		/*
			Free the old instruction.
		*/
		if ((*ppsNextInst) == psNextInst)
		{
			(*ppsNextInst) = psNextInst->psNext;
		}
		RemoveInst(psState, psBlock, psNextInst);
		FreeInst(psState, psNextInst);
	}

	return IMG_TRUE;
}

static IMG_BOOL GenerateDotProducts(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psInst, PINST* ppsNextInst)
/*****************************************************************************
 FUNCTION	: GenerateDotProducts

 PURPOSE	: Generate dotproduct/double dotproduct instructions.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Block containing the first instruction.
			  psInst				- First instruction in the DP/DDP sequence.
			  ppsNextInst			- Updated with the first valid instruction
									after psInst.

 RETURNS	: TRUE if a dotproduct was generated and the instructions in
			  sequence were freed.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* No scalar dot-product instructions on this core. */
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if(IsInstReferingShaderResult(psState, psInst))
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	if (psInst->eOpcode == IFMUL)
	{
		return GenerateFDP(psState, psBlock, psInst, ppsNextInst);
	}
	/* Check for the double dotproduct special case. */
	if (psInst->eOpcode == IEFO)
	{
		return GenerateFDDP(psState, psBlock, psInst, ppsNextInst);
	}
	return IMG_FALSE;
}

static
IMG_VOID InitGroupInstState(PINTERMEDIATE_STATE psState,
							PGROUPINST_STATE *ppsGIState)
/*****************************************************************************
 FUNCTION	: InitGroupInstState

 PURPOSE	: Allocate and initialise instruction grouping data

 PARAMETERS	: psState		- Compiler state.

 OUTPUT		: ppsGIState	- Instruction grouping state

 RETURNS	: Nothing.
*****************************************************************************/
{
	PGROUPINST_STATE psGIState;
	IMG_UINT32 uLabel;

	/* Allocate the state */
	psGIState = (PGROUPINST_STATE)UscAlloc(psState, sizeof(GROUPINST_STATE));

	/* Allocate state for each function. */
	psGIState->asFunc = UscAlloc(psState, psState->uMaxLabel * sizeof(psGIState->asFunc[0]));

	/* Initialise the data */
	for (uLabel = 0; uLabel < psState->uMaxLabel; uLabel++)
	{
		psGIState->asFunc[uLabel].bUsed = IMG_FALSE;
		psGIState->asFunc[uLabel].pvInitialState = NULL;
	}

	InitUseDef(&psGIState->sUseDef);

	/* Done */
	*ppsGIState = psGIState;
}

static IMG_VOID SetHardwareConst(PARG		psArg,
								 IMG_UINT32	uNumber)
/*****************************************************************************
 FUNCTION	: SetHardwareConst

 PURPOSE	: Set up an instruction argument to point to a hardware constant.

 PARAMETERS	: psArg		- Argument to initialize.
			  uNumber	- Index of the hardware constant.

 RETURNS	: Nothing.
*****************************************************************************/
{
	InitInstArg(psArg);
	psArg->uType = USEASM_REGTYPE_FPCONSTANT;
	psArg->uNumber = uNumber;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID ShiftVecPCKInstruction(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ShiftVecPCKInstruction

 PURPOSE	: Update a PCK instruction for the limitations of the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to shift.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMaxConverts;
	IMG_UINT32	uChan;
	IMG_UINT32	uWriteMask;
	IMG_INT32	iWrittenChan;

	/*
		For some combinations of PCK source and destination formats the
		hardware only supports writing the X channel. Up to this point we've
		representing these instructions as writing any channel (but only
		one). Now change the instruction so the scalar register corresponding
		to the written channel is the first destination and update the
		swizzle to match.
	*/

	/*
		Get the maximum number of channels we can convert in a single
		instruction.
	*/
	uMaxConverts = GetPackMaximumSimultaneousConversions(psState, psInst);
	
	/*
		Convert the write mask from bytes to F32 or F16 channels.
	*/
	uWriteMask = 0;
	if (psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
	{
		for (uChan = 0; uChan < psInst->uDestCount; uChan++)
		{
			if (psInst->auDestMask[uChan] != 0)
			{
				uWriteMask |= (1U << uChan);
			}
		}
	}
	else
	{
		ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_F16);
		for (uChan = 0; uChan < psInst->uDestCount; uChan++)
		{
			if ((psInst->auDestMask[uChan] & USC_DESTMASK_LOW) != 0)
			{
				uWriteMask |= (1U << ((uChan * 2) + 0));
			}
			if ((psInst->auDestMask[uChan] & USC_DESTMASK_HIGH) != 0)
			{
				uWriteMask |= (1U << ((uChan * 2) + 1));
			}
		}
	}

	/*
		Nothing more to do if we aren't trying to convert more channels than
		the hardware supports.
	*/
	if ((uWriteMask & ~((1U << uMaxConverts) - 1)) == 0)
	{
		return;
	}

	iWrittenChan = g_aiSingleComponent[uWriteMask];
	ASSERT(iWrittenChan != -1);

	/*
		Move the destination for the written channel to the first destination.
	*/
	ASSERT(psInst->asDest[iWrittenChan].uType != USEASM_REGTYPE_FPINTERNAL);
	if (psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
	{
		psInst->asDest[0] = psInst->asDest[iWrittenChan];
		psInst->auDestMask[0] = psInst->auDestMask[iWrittenChan];
		psInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[iWrittenChan];
	}
	else
	{
		ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_F16);
		ASSERT(uMaxConverts >= 2);

		psInst->asDest[0] = psInst->asDest[iWrittenChan / 2];
		psInst->auDestMask[0] = psInst->auDestMask[iWrittenChan / 2];
		psInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[iWrittenChan / 2];
	}
	SetDestCount(psState, psInst, 1 /* uNewDestCount */);

	/*
		Shift the swizzle selector corresponding to the written channel to the first
		slot in the swizzle.
	*/
	switch (GetChan(psInst->u.psVec->auSwizzle[0], iWrittenChan))
	{
		case USEASM_SWIZZLE_SEL_X: psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
		case USEASM_SWIZZLE_SEL_Y: psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
		case USEASM_SWIZZLE_SEL_Z: psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, Z, Z, Z); break;
		case USEASM_SWIZZLE_SEL_W: psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W); break;
	}
}

static IMG_VOID ExpandVecPCKInstruction(PINTERMEDIATE_STATE psState, PINST psOrigInst)
/*****************************************************************************
 FUNCTION	: ExpandVecPCKInstruction

 PURPOSE	: In the hardware a PCK instruction with an F32 format, unified store
			  source actually reads the X and Y channels from SRC1 and the
			  Z and W channels from SRC2. So expand the number of instruction
			  sources to reflect this arrangement. This simplifies the MOE
			  set up code.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IOPCODE			eNewOpcode;
	IMG_UINT32		uSwizzle;
	IMG_UINT32		uArg;
	IMG_UINT32		uSrc0Base;
	IMG_UINT32		uSrc1Base;

	/*
		Expand only PCK instructions with an F32 format, VEC3 or VEC4 source in
		the unified store.
	*/
	if (
			psOrigInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16 ||
			psOrigInst->asArg[1].uType == USEASM_REGTYPE_FPINTERNAL ||
			(
				GetLiveChansInArg(psState, psOrigInst, 1 + USC_Z_CHAN) == 0 &&
				GetLiveChansInArg(psState, psOrigInst, 1 + USC_W_CHAN) == 0
			)
		)
	{
		return;
	}
	ASSERT(psOrigInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);

	/*
		Get the new opcode.
	*/
	switch (psOrigInst->eOpcode)
	{
		case IVPCKU8FLT: eNewOpcode = IVPCKU8FLT_EXP; break;
		case IVPCKC10FLT: eNewOpcode = IVPCKC10FLT_EXP; break;
		case IVPCKS16FLT: eNewOpcode = IVPCKS16FLT_EXP; break;
		case IVPCKU16FLT: eNewOpcode = IVPCKU16FLT_EXP; break;
		case IVPCKFLTFLT: eNewOpcode = IVPCKFLTFLT_EXP; break;
		default: imgabort();
	}

	/*
		Save the current instruction swizzle.
	*/
	uSwizzle = psOrigInst->u.psVec->auSwizzle[0];

	/*
		Change to the new opcode.
	*/
	ModifyOpcode(psState, psOrigInst, eNewOpcode);

	/*
		Set up information about the two source arguments.
	*/
	psOrigInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psOrigInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
	psOrigInst->u.psVec->uPackSwizzle = uSwizzle;

	psOrigInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	psOrigInst->u.psVec->aeSrcFmt[1] = UF_REGFORMAT_F32;

	uSrc0Base = 0 * SOURCE_ARGUMENTS_PER_VECTOR;
	uSrc1Base = 1 * SOURCE_ARGUMENTS_PER_VECTOR;

	ASSERT(psOrigInst->asArg[uSrc0Base].uType == USC_REGTYPE_UNUSEDSOURCE);
	psOrigInst->asArg[uSrc1Base].uType = USC_REGTYPE_UNUSEDSOURCE;

	for (uArg = 0; uArg <= 1; uArg++)
	{
		PARG	psOrigArg = &psOrigInst->asArg[uSrc0Base + 1 + USC_Z_CHAN + uArg];

		/*
			Copy the Z and W components of the first source to the
			X and Y components of the second source.
		*/
		psOrigInst->asArg[uSrc1Base + 1 + uArg] = *psOrigArg;

		/*
			Clear the Z and W components of the first source.
		*/
		InitInstArg(psOrigArg);
		psOrigArg->uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	/*
		Clear the Z and W components of the second source.
	*/
	for (uArg = 0; uArg <= 1; uArg++)
	{
		psOrigInst->asArg[uSrc1Base + 1 + USC_Z_CHAN + uArg].uType = USC_REGTYPE_UNUSEDSOURCE;
	}
}

static IMG_VOID NormaliseVMOVSwizzle(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: NormaliseVMOVSwizzle

 PURPOSE	: If a VMOV instruction accesses the Z or W channels but not X
			  or Y then increase the source register number so the channels
			  it was accessed become the X and Y channels.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to normalise.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uNewSwizzle;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uChanMask;

	/*
		Don't normalise if X or Y are accessed.
	*/
	if (GetLiveChansInArg(psState, psInst, 1 + USC_X_CHAN) != 0 ||
		GetLiveChansInArg(psState, psInst, 1 + USC_Y_CHAN) != 0)
	{
		return;
	}

	/*
		Calculate a new swizzle by replacing selectors of Z by X and
		W by Y.
	*/
	uNewSwizzle = 0;
	uChanMask = 0;
	for (uChanIdx = 0; uChanIdx < psInst->uDestCount; uChanIdx++)
	{
		if ((psInst->auDestMask[uChanIdx] & psInst->auLiveChansInDest[uChanIdx]) != 0)
		{
			IMG_UINT32	uSel;

			uSel = GetChan(psInst->u.psVec->auSwizzle[0], uChanIdx);
			if (uSel == USEASM_SWIZZLE_SEL_Z)
			{
				uSel = USEASM_SWIZZLE_SEL_X;
			}
			else
			{
				ASSERT(uSel == USEASM_SWIZZLE_SEL_W);
				uSel = USEASM_SWIZZLE_SEL_Y;
			}
			uNewSwizzle |= (uSel << (USEASM_SWIZZLE_FIELD_SIZE * uChanIdx));

			/*
				Record which channels are used from the swizzle.
			*/
			uChanMask |= (1U << uChanIdx);
		}
	}

	/*
		Check the new swizzle is supported by the VMOV instruction.
	*/
	if (!IsSwizzleSupported(psState, psInst, psInst->eOpcode, 0 /* uSwizzleSlotIdx */, uNewSwizzle, uChanMask, &uNewSwizzle))
	{
		return;
	}

	/*
		Update the swizzle in the instruction.
	*/
	psInst->u.psVec->auSwizzle[0] = uNewSwizzle;

	/*
		Shift the source registers so the data selected by the Z and W channels are
		now selected by the X and Y channels.
	*/
	for (uChanIdx = 0; uChanIdx < 2; uChanIdx++)
	{
		PARG	psOrigArg = &psInst->asArg[1 + USC_Z_CHAN + uChanIdx];

		psInst->asArg[1 + uChanIdx] = *psOrigArg;

		InitInstArg(psOrigArg);
		psOrigArg->uType = USC_REGTYPE_UNUSEDSOURCE;
	}
}

static IMG_VOID ExpandVecMovInstruction(PINTERMEDIATE_STATE psState, PINST psOrigInst)
/*****************************************************************************
 FUNCTION	: ExpandVecMovInstruction

 PURPOSE	: Expand a vector move instruction which isn't supported directly
			  by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bConvertToVPCK = IMG_FALSE;
	IMG_UINT32	uDestIdx;

	/*
		Expand a vector MOV with an F32 format source/destination and reading
		the Z or W channels from a unified store source.
	*/
	if (
			psOrigInst->asDest[0].eFmt == UF_REGFORMAT_F32 &&
			psOrigInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL
	   )
	{
		/*
			If only Z and W are used then try to change to using X and Y
			by increasing the source register number.
		*/
		NormaliseVMOVSwizzle(psState, psOrigInst);	

		if (GetLiveChansInArg(psState, psOrigInst, 1 + USC_Z_CHAN) != 0 ||
			GetLiveChansInArg(psState, psOrigInst, 1 + USC_W_CHAN) != 0)
		{
			bConvertToVPCK = IMG_TRUE;
		}
	}

	/*
		Expand a VMOV converting between different formats.
	*/
	if (psOrigInst->asDest[0].eFmt != psOrigInst->u.psVec->aeSrcFmt[0])
	{
		bConvertToVPCK = IMG_TRUE;
	}

	/*
		Expand a VMOV writing subcomponents of a 32-bit register.
	*/
	for (uDestIdx = 0; uDestIdx < psOrigInst->uDestCount; uDestIdx++)
	{
		IMG_UINT32	uChansToPreserve;

		uChansToPreserve = psOrigInst->auLiveChansInDest[uDestIdx] & ~psOrigInst->auDestMask[uDestIdx];
		if (uChansToPreserve != 0 && uChansToPreserve != USC_XYZW_CHAN_MASK)
		{
			bConvertToVPCK = IMG_TRUE;
			break;
		}
	}

	/*
		Expand a vector MOV if the swizzle isn't supported by the hardware
		VMOV.
	*/
	if (!bConvertToVPCK)
	{
		IMG_UINT32	uNewSwizzle;
		IMG_UINT32	uLiveChans;

		/*
			Get the mask of channels accessed from the source before the current swizzle
			is applied.
		*/
		GetLiveChansInSourceSlot(psState, psOrigInst, 0 /* uSlotIdx */, &uLiveChans, NULL /* puChansUsedPostSwizzle */);

		/*
			Check if the swizzle (or an equivalent) is supported by the hardware VMOV instruction.
		*/
		if (!IsSwizzleSupported(psState, 
								psOrigInst,
							    IVMOV_EXP, 
								0 /* uSwizzleSlotIdx */, 
								psOrigInst->u.psVec->auSwizzle[0], 
								uLiveChans, 
								&uNewSwizzle))
		{
			bConvertToVPCK = IMG_TRUE;
		}
		else
		{
			psOrigInst->u.psVec->auSwizzle[0] = uNewSwizzle;
		}
	}

	/*
		Convert the instruction to a VPCK.
	*/
	if (bConvertToVPCK)
	{
		ModifyOpcode(psState, psOrigInst, IVPCKFLTFLT);

		ExpandVecPCKInstruction(psState, psOrigInst);
	}
	else
	{
		ModifyOpcode(psState, psOrigInst, IVMOV_EXP);

		for (uDestIdx = 0; uDestIdx < psOrigInst->uDestCount; uDestIdx++)
		{
			if (psOrigInst->auDestMask[uDestIdx] != 0)
			{
				psOrigInst->auDestMask[uDestIdx] = USC_XYZW_CHAN_MASK;
			}
		}
	}
}

static IMG_VOID ExpandNonFltPCKInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psOrigPCKInst)
/*****************************************************************************
 FUNCTION	: ExpandNonFltPCKInstruction

 PURPOSE	: Expand a PCKU16U16 or PCKS16S8 instruction to the equivalent
			  for a vector core.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block containing the instruction to
								expand.
			  psInst			- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	auChanWriteMask[2];
	IMG_UINT32	uSrcIdx;
	IMG_UINT32	uChan;
	IOPCODE		eNewOpcode;

	switch (psOrigPCKInst->eOpcode)
	{
		case IUNPCKU16U16: eNewOpcode = IVPCKU16U16; break;
		case IUNPCKS16S8: eNewOpcode = IVPCKS16S8; break;
		case IUNPCKU16U8: eNewOpcode = IVPCKU16U8; break;
		default: imgabort();
	}

	for (uChan = 0; uChan < 2; uChan++)
	{
		auChanWriteMask[uChan] = psOrigPCKInst->auDestMask[0] & (USC_XY_CHAN_MASK << (uChan * 2));
	}

	uSrcIdx = 0;
	for (uChan = 0; uChan < 2; uChan++)
	{
		IMG_UINT32	uChanDestMask;
		PINST		psPCKInst;
		IMG_UINT32	uOrigPCKComponent;

		/*
			Get the bytes written in this destination channel.
		*/
		uChanDestMask = auChanWriteMask[uChan];
		if (uChanDestMask == 0)
		{
			continue;
		}

		psPCKInst = AllocateInst(psState, psOrigPCKInst);

		SetOpcode(psState, psPCKInst, eNewOpcode);
		InsertInstBefore(psState, psCodeBlock, psPCKInst, psOrigPCKInst);

		/*
			Copy the old PCK's destination to the new instruction.
		*/
		SetDestFromArg(psState, psPCKInst, 0 /* uDestIdx */, &psOrigPCKInst->asDest[0]);

		/*
			Flag we are not completely defining the instructon if either
			(i) Either the original instruction didn't completely define it's destination.
			(ii) Or this is the second instruction in the expanded sequence (so we need to preserve
			the channels written by the first instruction).
		*/
		if (psOrigPCKInst->apsOldDest[0] != NULL || (uChan == 1 && auChanWriteMask[0] != 0))
		{
			SetPartiallyWrittenDest(psState, psPCKInst, 0 /* uCopyToDestIdx */, &psPCKInst->asDest[0]);
		}

		/*
			Copy the old PCK's predicate to the new instruction.
		*/
		CopyPredicate(psState, psPCKInst, psOrigPCKInst);

		/*
			Copy the SKIPINVALID flag from the old PCK to the new instruction.
		*/
		SetBit(psPCKInst->auFlag, INST_SKIPINV, GetBit(psOrigPCKInst->auFlag, INST_SKIPINV));

		/*
			Copy the NOEMIT flag from the old PCK to the new instruction.
		*/
		SetBit(psPCKInst->auFlag, INST_NOEMIT, GetBit(psOrigPCKInst->auFlag, INST_NOEMIT));

		/* Unused by this instruction. */
		psPCKInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_INVALID;

		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			/*
				Copy information about which shader results are read/written by this instruction.
			*/
			psPCKInst->uShaderResultHWOperands = psOrigPCKInst->uShaderResultHWOperands;
		}
		#endif /* defined(OUTPUT_USPBIN) */

		psPCKInst->auDestMask[0] = uChanDestMask;
		psPCKInst->auLiveChansInDest[0] = psOrigPCKInst->auLiveChansInDest[0];
		if (uChan == 0)
		{
			psPCKInst->auLiveChansInDest[0] &= ~auChanWriteMask[1];
		}

		/*
			Copy the source register which the old PCK wrote to the current channel in
			the destination.
		*/
		CopySrc(psState, psPCKInst, 0, psOrigPCKInst, uSrcIdx);

		/*
			Convert the component select on the source register to a swizzle.
		*/
		uOrigPCKComponent = GetPCKComponent(psState, psOrigPCKInst, uSrcIdx);
		if (psOrigPCKInst->eOpcode == IUNPCKU16U16)
		{
			switch (uOrigPCKComponent)
			{
				case 0: psPCKInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
				case 1: imgabort();
				case 2: psPCKInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
				case 3: imgabort();
			}
		}
		else
		{
			psPCKInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uOrigPCKComponent];
		}

		uSrcIdx = (uSrcIdx + 1) % 2;
	}

	RemoveInst(psState, psCodeBlock, psOrigPCKInst);
	FreeInst(psState, psOrigPCKInst);
}

static IMG_VOID ExpandIMOVC_I32Instruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psOrigMovcInst)
/*****************************************************************************
 FUNCTION	: ExpandIMOVC_I32Instruction

 PURPOSE	: Expand a IMOVC_I32 instruction to the equivalent
			  for a vector core.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block containing the instruction to
								expand.
			  psOrigMovcInst	- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrcIdx;
	PINST		psVmovcInst;

	psVmovcInst = AllocateInst(psState, psOrigMovcInst);
	SetOpcode(psState, psVmovcInst, IVMOVC_I32);

	/*
		Copy the old MOVC's destination to the new instruction.
	*/
	psVmovcInst->asDest[0] = psOrigMovcInst->asDest[0];

	/*
		Copy the old MOVC's predicate to the new instruction.
	*/
	CopyPredicate(psState, psVmovcInst, psOrigMovcInst);

	/*
		Copy the SKIPINVALID flag from the old MOVC to the new instruction.
	*/
	SetBit(psVmovcInst->auFlag, INST_SKIPINV, GetBit(psOrigMovcInst->auFlag, INST_SKIPINV));

	/*
		Copy the NOEMIT flag from the old MOVC to the new instruction.
	*/
	SetBit(psVmovcInst->auFlag, INST_NOEMIT, GetBit(psOrigMovcInst->auFlag, INST_NOEMIT));

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/*
			Copy information about which shader results are read/written by this instruction.
		*/
		psVmovcInst->uShaderResultHWOperands = psOrigMovcInst->uShaderResultHWOperands;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	psVmovcInst->auDestMask[0] = psOrigMovcInst->auDestMask[0];

	psVmovcInst->auLiveChansInDest[0] = psOrigMovcInst->auLiveChansInDest[0];

	/*
		Copy test from the old MOVC instruction.
	*/
	*psVmovcInst->u.psMovc = *psOrigMovcInst->u.psMovc;

	for (uSrcIdx = 0; uSrcIdx < 3; uSrcIdx++)
	{
		/*
			Copy the source register which the old MOVC wrote to the current channel in
			the destination.
		*/
		psVmovcInst->asArg[uSrcIdx] = psOrigMovcInst->asArg[uSrcIdx];
	}

	InsertInstBefore(psState, psCodeBlock, psVmovcInst, psOrigMovcInst->psNext);

	RemoveInst(psState, psCodeBlock, psOrigMovcInst);
	FreeInst(psState, psOrigMovcInst);
}

static IMG_BOOL IsValidSingleIssueSource(PINTERMEDIATE_STATE	psState,
										 IMG_UINT32				uNewSlotIdx,
										 IOPCODE				eSingleIssueOpcode,
										 IMG_UINT32				uChansUsed,
										 PINST					psInst,
										 IMG_UINT32				uOldSlotIdx,
										 IMG_PUINT32			puNewSwizzle)
/*****************************************************************************
 FUNCTION	: IsValidSingleIssueSource

 PURPOSE	: Checks whether a source to an instruction would be valid as a
			  source to a single-issue GPI oriented instruction.

 PARAMETERS	: psState			- Compiler state.
			  uNewSlotIdx		- Source slot to check.
			  eSingleIssueOpcode	
								- Single-issue instruction type.
			  uChansUsed		- Mask of the channels referenced from the source.
			  psInst			- Instruction to which the source is currently
								an argument.
			  uOldSlotIdx		- Source slot currently used.
			  puNewSwizzle		- Returns this swizzle to use with the source.

 RETURNS	: TRUE if the source is valid.
*****************************************************************************/
{
	PARG		psArg = &psInst->asArg[VEC_MOESOURCE_ARGINDEX(uOldSlotIdx)];
	IMG_UINT32	uSwizzle = psInst->u.psVec->auSwizzle[uOldSlotIdx];

	if (uNewSlotIdx == 0)
	{
		/*
			The unified store source can't be F16 format.
		*/
		if (psArg->eFmt != UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			The other sources must be GPI registers.
		*/
		if (psArg->uType != USEASM_REGTYPE_FPINTERNAL)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check the swizzle on this source is supported by the single-issue instruction.
	*/
	if (!IsSwizzleSupported(psState, psInst, eSingleIssueOpcode, uNewSlotIdx, uSwizzle, uChansUsed, puNewSwizzle))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static IMG_BOOL TryConvertToSingleIssue(PINTERMEDIATE_STATE	psState,
									    PINST				psInst)
/*****************************************************************************
 FUNCTION	: TryConvertToSingleIssue

 PURPOSE	: Try and convert an instruction to its single-issue GPI-oriented variant. 
			  These have some extra restrictions on the arguments but can be grouped into an MOE repeat.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.

 RETURNS	: TRUE if the instruction was converted.
*****************************************************************************/
{
	IMG_UINT32	uChansUsed;
	IOPCODE		eNewOpcode;
	IMG_UINT32	i;
	IMG_UINT32	uWrittenDestCount;

	uWrittenDestCount = psInst->uDestCount;
	while (uWrittenDestCount > 0)
	{
		if (psInst->auDestMask[uWrittenDestCount - 1] != 0)
		{
			break;
		}
		uWrittenDestCount--;
	}

	/*
		Get the single-issue version of this opcode.
	*/
	switch (psInst->eOpcode)
	{
		case IVDP:
		{
			uChansUsed = USC_XYZW_CHAN_MASK;	
			eNewOpcode = IVDP_GPI; 
			break;
		}
		case IVDP3:	
		{
			uChansUsed = USC_XYZ_CHAN_MASK;		
			eNewOpcode = IVDP3_GPI; 

			/*
				The hardware doesn't support writing to the W component of the destination from
				a single-issue VDP3 instruction.
			*/
			if (uWrittenDestCount > 3)
			{
				return IMG_FALSE;
			}
			break;
		}
		case IVMAD:
		{
			uChansUsed = USC_XYZW_CHAN_MASK;
			eNewOpcode = IVMAD4;
			break;
		}
		case IVADD:
		{
			if (uWrittenDestCount > 3)
			{
				/*
					We can support a repeated VADD by using VMAD with a dummy second source
					with a swizzle of 1111. But this swizzle is only supported on a VEC3 VMAD.
				*/
				return IMG_FALSE;
			}
			uChansUsed = USC_XYZ_CHAN_MASK;
			eNewOpcode = IVADD3;
			break;
		}
		case IVMUL:
		{
			if (uWrittenDestCount > 3)
			{
				/*
					We can support a repeated VADD by using VMAD with a dummy third source
					with a swizzle of 0000. But this swizzle is only supported on a VEC3 VMAD.
				*/
				return IMG_FALSE;
			}
			uChansUsed = USC_XYZ_CHAN_MASK;
			eNewOpcode = IVMUL3;
			break;
		}
		default: 
		{
			return IMG_FALSE;
		}
	}

	/*
		The single-issue instructions only support F32 sources/destinations.
	*/
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}

	/*
		Check the sources would still be legal if the instruction was converted to the single-issue
		variant. The first two sources to the single-issue all commute so try also with the first
		two sources swapped.
	*/
	for (i = 0; i < 2; i++)
	{
		IMG_UINT32	auNewSwizzles[VECTOR_MAX_SOURCE_SLOT_COUNT];
		IMG_BOOL	bValidSources;
		IMG_UINT32	uOldSlotIdx;

		bValidSources = IMG_TRUE;
		for (uOldSlotIdx = 0; uOldSlotIdx < GetSwizzleSlotCount(psState, psInst); uOldSlotIdx++)
		{
			IMG_UINT32	uNewSlotIdx;

			if (i == 0)
			{
				uNewSlotIdx = uOldSlotIdx;
			}
			else
			{
				switch (uOldSlotIdx)
				{
					case 0: uNewSlotIdx = 1; break;
					case 1: uNewSlotIdx = 0; break;
					default: uNewSlotIdx = uOldSlotIdx; break;
				}
			}

			if (!IsValidSingleIssueSource(psState, 
										  uNewSlotIdx,
										  eNewOpcode,
										  uChansUsed,
										  psInst,
										  uOldSlotIdx,
										  &auNewSwizzles[uNewSlotIdx]))
			{
				bValidSources = IMG_FALSE;
				break;
			}
		}

		if (bValidSources)
		{
			if (i == 1)
			{
				SwapVectorSources(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);
			}
			ModifyOpcode(psState, psInst, eNewOpcode);
			memcpy(psInst->u.psVec->auSwizzle, auNewSwizzles, sizeof(psInst->u.psVec->auSwizzle));
			return IMG_TRUE;
		}
	}
	
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID SetFloatHardwareConst(PINST psInst, IMG_UINT32 uArgIdx, IMG_UINT32 uConstNum)
/*****************************************************************************
 FUNCTION	: SetFloatHardwareConst

 PURPOSE	: Set a hardware constant as an argument to a floating point instruction.

 PARAMETERS	: psInst		- Instruction to modify.
			  uArgIdx		- Index of the argument to modify.
			  uConstNum		- Index of the constant to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetHardwareConst(&psInst->asArg[uArgIdx], uConstNum);
	psInst->u.psFloat->asSrcMod[uArgIdx].bNegate = IMG_FALSE;
	psInst->u.psFloat->asSrcMod[uArgIdx].bAbsolute = IMG_FALSE;
	psInst->u.psFloat->asSrcMod[uArgIdx].uComponent = 0;
}

static IMG_VOID SetFMAD16Constant(PINST psInst, IMG_UINT32 uArgIdx, IMG_UINT32 uConstNum)
/*****************************************************************************
 FUNCTION	: SetFMAD16Constant

 PURPOSE	: Set a hardware constant as an argument to a FMAD16 instruction.

 PARAMETERS	: psInst		- Instruction to modify.
			  uArgIdx		- Index of the argument to modify.
			  uConstNum		- Index of the constant to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetHardwareConst(&psInst->asArg[uArgIdx], uConstNum);
	psInst->u.psArith16->aeSwizzle[uArgIdx] = FMAD16_SWIZZLE_LOWHIGH;
	psInst->u.psArith16->sFloat.asSrcMod[uArgIdx].bNegate = IMG_FALSE;
	psInst->u.psArith16->sFloat.asSrcMod[uArgIdx].bAbsolute = IMG_FALSE;
	psInst->u.psArith16->sFloat.asSrcMod[uArgIdx].uComponent = 0;
}

static IMG_VOID ExpandMacroInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandMacroInstruction

 PURPOSE	: Expand instructions which don't have an exact equivalent in the
			  hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IFMOV:
		{
			/*
				Similarly for an FMOV expand it to either FDP or FMAD.
			*/
			if (!CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[0]))
			{
				ModifyOpcode(psState, psInst, IFDP);
				SetFloatHardwareConst(psInst, 1 /* uArgIdx */, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1);
			}
			else
			{
				ModifyOpcode(psState, psInst, IFMAD);
				SetFloatHardwareConst(psInst, 1 /* uArgIdx */, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1);
				SetFloatHardwareConst(psInst, 2 /* uArgIdx */, EURASIA_USE_SPECIAL_CONSTANT_ZERO);
			}
			break;
		}
		case ITESTPRED:
		case ITESTMASK:
		{
			if (psInst->u.psTest->eAluOpcode == IFMOV)
			{
				psInst->u.psTest->eAluOpcode = IFADD;
				SetHardwareConst(&psInst->asArg[1], EURASIA_USE_SPECIAL_CONSTANT_ZERO);
				InitPerArgumentParameters(psState, psInst, 1 /* uArgIdx */);
			}
			break;
		}
		case INOT:
		{
			ASSERT(!psInst->u.psBitwise->bInvertSrc2);

			/*
					NOT		DEST, SRC
				->
					XOR		DEST, SRC, #0xFFFFFFFF
			*/
			ModifyOpcode(psState, psInst, IXOR);
			SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0xFFFFFFFF /* uNumber */, UF_REGFORMAT_F32);
			break;
		}
		case IFMUL:
		{
			/*
				For a FMUL instruction expand it to either FDP (if neither source can go in source 0
				of an FMAD) or FMAD otherwise.
			*/
			if (!CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[0]) &&
				!CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[1]))
			{
				ModifyOpcode(psState, psInst, IFDP);
			}
			else
			{
				ModifyOpcode(psState, psInst, IFMAD);
				if (!CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[0]))
				{
					CommuteSrc01(psState, psInst);
				}
				SetFloatHardwareConst(psInst, 2 /* uArgIdx */, EURASIA_USE_SPECIAL_CONSTANT_ZERO);
				InvertNegateModifier(psState, psInst, 2);
			}
			break;
		}
		case IFADD:
		case IFSUB:
		{
			if (psInst->eOpcode == IFSUB)
			{
				InvertNegateModifier(psState, psInst, 1 /* uArgIdx */);
			}

			/*
				ADD D, A, B -> MAD D, A, 1, B
			*/
			ModifyOpcode(psState, psInst, IFMAD);
			MoveFloatSrc(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */);
			SetFloatHardwareConst(psInst, 1 /* uArgIdx */, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1);
			break;
		}
		case IFMOV16:
		{
			/*
				MOV16 D, A -> MAD16 D, A, 1, 0
			*/
			ModifyOpcode(psState, psInst, IFMAD16);

			SetFMAD16Constant(psInst, 1, EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE);
			SetFMAD16Constant(psInst, 2, EURASIA_USE_SPECIAL_CONSTANT_ZERO);
			break;
		}
		case IFADD16:
		{
			/*
				ADD16 D, A, B -> MAD16 D, A, 1, B
			*/
			ModifyOpcode(psState, psInst, IFMAD16);

			MoveFloatSrc(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */);

			SetFMAD16Constant(psInst, 1, EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE);
			break;
		}
		case IFMUL16:
		{
			/*
				MUL16 D, A, B -> MAD16 D, A, B, 0
			*/
			ModifyOpcode(psState, psInst, IFMAD16);

			SetFMAD16Constant(psInst, 2, EURASIA_USE_SPECIAL_CONSTANT_ZERO);
			break;
		}
		#if defined(SUPPORT_SGX545)
		case IFMIN:
		{
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP)
			{
				/* D = min(X, Y) -> D = min(max(X, X), Y) */
				ModifyOpcode(psState, psInst, IFMINMAX);
				CopySrcAndModifiers(psState, psInst, 2 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */);
				CopySrcAndModifiers(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
			}
			break;
		}
		case IFMAX:
		{
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP)
			{
				/* D = max(X, Y) -> D = max(min(X, X), Y) */
				ModifyOpcode(psState, psInst, IFMAXMIN);
				CopySrcAndModifiers(psState, psInst, 2 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
			}
			break;
		}
		#endif /* defined(SUPPORT_SGX545) */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IUNPCKU16U16:
		case IUNPCKS16S8:
		case IUNPCKU16U8:
		{
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
			{
				ExpandNonFltPCKInstruction(psState, psCodeBlock, psInst);
			}
			break;
		}
		case IMOVC_I32:
		{
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
			{
				ExpandIMOVC_I32Instruction(psState, psCodeBlock, psInst);
			}
			break;
		}
		case IVMOV:
		{
			ExpandVecMovInstruction(psState, psInst);
			break;
		}
		case IVMOVC:
		case IVMOVCU8_FLT:
		{
			if (psInst->u.psVec->eMOVCTestType == TEST_TYPE_GTE_ZERO)
			{
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LT_ZERO;
				SwapVectorSources(psState, psInst, 1 /* uArg1Idx */, psInst, 2 /* uArg2Idx */);
			}
			else if (psInst->u.psVec->eMOVCTestType == TEST_TYPE_GT_ZERO) 
			{
				psInst->u.psVec->eMOVCTestType = TEST_TYPE_LTE_ZERO;
				SwapVectorSources(psState, psInst, 1 /* uArg1Idx */, psInst, 2 /* uArg2Idx */);
			}
			break;
		}
		case IVPCKU8FLT:
		case IVPCKC10FLT:
		case IVPCKS16FLT:
		case IVPCKU16FLT:
		case IVPCKFLTFLT:
		{
			ExpandVecPCKInstruction(psState, psInst);
			break;
		}
		case IVPCKFLTU8:
		case IVPCKFLTS8:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		{
			ShiftVecPCKInstruction(psState, psInst);
			break;
		}
		case IVDP2:
		{
			ModifyOpcode(psState, psInst, IVDP3_GPI);
			psInst->u.psVec->auSwizzle[0] = SetChan(psInst->u.psVec->auSwizzle[0], USC_Z_CHAN, USEASM_SWIZZLE_SEL_0);
			psInst->u.psVec->auSwizzle[1] = SetChan(psInst->u.psVec->auSwizzle[1], USC_Z_CHAN, USEASM_SWIZZLE_SEL_1);
			break;
		}
		case IVDP3:
		{
			/*
				First try converting the instruction to the single-issue GPI vec3 dotproduct.
			*/
			if (!TryConvertToSingleIssue(psState, psInst))
			{
				/*
					Convert 
						VDP3 A, B.xyz, C.xyz
					->
						VDP	 A, B.xyz0, C.xyz1
				*/
				ModifyOpcode(psState, psInst, IVDP);

				psInst->u.psVec->auSwizzle[0] &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * USC_W_CHAN));
				psInst->u.psVec->auSwizzle[0] |=  (USEASM_SWIZZLE_SEL_0 << (USEASM_SWIZZLE_FIELD_SIZE * USC_W_CHAN));
				ASSERT(IsSwizzleSupported(psState,
										  psInst,
										  psInst->eOpcode, 
										  0 /* uSwizzleSlotIdx */, 
										  psInst->u.psVec->auSwizzle[0],
										  USC_XYZW_CHAN_MASK,
										  NULL /* puMatchedSwizzle */));

				psInst->u.psVec->auSwizzle[1] &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * USC_W_CHAN));
				psInst->u.psVec->auSwizzle[1] |=  (USEASM_SWIZZLE_SEL_1 << (USEASM_SWIZZLE_FIELD_SIZE * USC_W_CHAN));
				ASSERT(IsSwizzleSupported(psState, 
										  psInst,
										  psInst->eOpcode, 
										  1 /* uSwizzleSlotIdx */, 
										  psInst->u.psVec->auSwizzle[1],
										  USC_XYZW_CHAN_MASK,
										  NULL /* puMatchedSwizzle */));
			}
			break;
		}
		case IVDP:
		case IVMAD:
		case IVADD:
		case IVMUL:
		{
			/*
				Try converting to a variant of this instruction which can
				be grouped into an MOE repeat.
			*/
			(IMG_VOID)TryConvertToSingleIssue(psState, psInst);
			break;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default:
		{
			break;
		}
	}
}

static
IMG_VOID GenerateExtraDPCIteration(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   PINST				psOrigDPCInst)
/*****************************************************************************
 FUNCTION	: GenerateExtraDPCIteration

 PURPOSE	: Add extra instructions so a DPC instruction has a minimum of
			  two iterations.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the DPC instruction.
			  psOrigDPCInst		- DPC instruction to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psNewDPCInst;
	IMG_UINT32	uIteration;

	/*
		Create a new DPC instruction.
	*/
	psNewDPCInst = AllocateInst(psState, psOrigDPCInst);
	SetOpcode(psState, psNewDPCInst, IFDPC_RPT);
	InsertInstBefore(psState, psBlock, psNewDPCInst, psOrigDPCInst);

	/*
		Copy the instruction parameters from the original instruction.
	*/
	SetBit(psNewDPCInst->auFlag, INST_SKIPINV, GetBit(psOrigDPCInst->auFlag, INST_SKIPINV));
	CopyNegateAbsoluteModifierToFDP(psState, psNewDPCInst, FDP_RPT_PER_ITERATION_ARGUMENT_COUNT, psOrigDPCInst);

	/*
		Do two dotproduct iterations (as required by the hardware).
	*/
	psNewDPCInst->u.psFdp->uRepeatCount = 2;

	/*
		Copy the clipplane P-flag destination.
	*/
	psNewDPCInst->u.psFdp->uClipPlane = psOrigDPCInst->u.psDpc->uClipplane;

	#if defined(SRC_DEBUG)
	/*
		Add an extra cycle for the second DPC iteration.
	*/
	IncrementCostCounter(psState, psNewDPCInst->uAssociatedSrcLine, 1);
	#endif /* defined(SRC_DEBUG) */

	/*
		Copy the dotproduct destination.
	*/
	MoveDest(psState, psNewDPCInst, 0, psOrigDPCInst, 0);

	/*
		Setup the calculation for the first iteration to

			FIRST_DPC_SOURCE * ONE

		where FIRST_DPC_SOURCE contains the clipplane result.

		Setup the calculation for the second iteration to

			SECOND_DPC_SOURCE * ONE + (first iteration)

		where SECOND_DPC_SOURCE is a temporary register containing zero. The
		two DPC source register have been given consecutive register numbers
		so we can always form a repeat from the two iterations.
	*/
	for (uIteration = 0; uIteration < 2; uIteration++)
	{
		IMG_UINT32	uArgBase;
		PARG		psSecondArg;

		uArgBase = uIteration * FDP_RPT_PER_ITERATION_ARGUMENT_COUNT;

		CopyArgumentToDP(psState, psNewDPCInst, uArgBase + 0 /* uDestArg */, psOrigDPCInst, uIteration /* uSrcArg */);

		psSecondArg = &psNewDPCInst->asArg[uArgBase + 1];
		psSecondArg->uType = USEASM_REGTYPE_FPCONSTANT;
		psSecondArg->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
		SetComponentSelect(psState, psNewDPCInst, uArgBase + 1 /* uDestArg */, 0 /* uComponent */);
	}

	/*	
		Free the DPC instruction.
	*/
	RemoveInst(psState, psBlock, psOrigDPCInst);
	FreeInst(psState, psOrigDPCInst);
}

static
IMG_BOOL OptimizePFlagCalc(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBlock,
						   PINST				psDPCInst)
/*****************************************************************************
 FUNCTION	: OptimizePFlagCalc

 PURPOSE	: Try to combine an instruction writing a P flag with a dotproduct
			  instruction writing its source.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the DPC instruction.
			  psOrigDPCInst		- DPC instruction to try and optimize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_USEDEF	sDPCUse;
	REGISTER_USEDEF	sDPCDef;
	PINST			psPrevInst;
	IMG_BOOL		bOptimized;
	PARG			psClipValue = &psDPCInst->asArg[0];

	/*
		Check the DPC doesn't have any modifiers on its source argument.
	*/
	if (
			!(
				psClipValue->uIndexType == USC_REGTYPE_NOINDEX &&
				!psDPCInst->u.psDpc->sFloat.asSrcMod[0].bNegate &&
				!psDPCInst->u.psDpc->sFloat.asSrcMod[0].bAbsolute 
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check this is the last use of the DPC source.
	*/
	if (!UseDefIsSingleSourceRegisterUse(psState, psDPCInst, 0 /* uSrcIdx */))
	{
		return IMG_FALSE;
	}

	/*
		Set up a record of the registers used by the DPC instruction.
	*/
	InitRegUseDef(&sDPCUse);
	InstUse(psState, psDPCInst, &sDPCUse);

	/*
		The second DPC source is only needed if we don't manage to optimize the DPC together
		with a previous dot-product so clear its entry in the use-def record.
	*/
	ReduceRegUseDef(psState, 
					&sDPCUse, 
					psDPCInst->asArg[1].uType, 
					psDPCInst->asArg[1].uNumber, 
					USC_DESTMASK_FULL);

	/*
		Set up a record of the registers defined by the DPC instruction.
	*/
	InitRegUseDef(&sDPCDef);
	InstDef(psState, psDPCInst, &sDPCDef);

	bOptimized = IMG_FALSE;
	for (psPrevInst = psDPCInst->psPrev; psPrevInst != NULL; psPrevInst = psPrevInst->psPrev)
	{
		REGISTER_USEDEF		sPrevInstDef, sPrevInstUse;
		IMG_BOOL			bNotDisjoint;

		/*
			Check for a repeated DP instruction writing the DPC instruction's source.
		*/
		if (psPrevInst->eOpcode == IFDP_RPT &&
			psPrevInst->u.psFdp->uRepeatCount > 1 &&
			NoPredicate(psState, psPrevInst) &&
			psPrevInst->asDest[0].uType == psClipValue->uType &&
			psPrevInst->asDest[0].uNumber == psClipValue->uNumber &&
			psPrevInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX)
		{
			/*	
				Switch the previous instruction to write a clipplane result.
			*/
			ModifyOpcode(psState, psPrevInst, IFDPC_RPT);

			/*
				Copy the DPC's destination to the DP. We've already checked there are no instructions using the
				DPC's result in between.
			*/
			psPrevInst->asDest[0] = psDPCInst->asDest[0];

			/*
				Copy the clipplane destination from the DPC to the repeated dotproduct.
			*/
			psPrevInst->u.psFdp->uClipPlane = psDPCInst->u.psDpc->uClipplane;

			/*
				Remove and free the DPC instruction.
			*/
			RemoveInst(psState, psBlock, psDPCInst);
			FreeInst(psState, psDPCInst);

			bOptimized = IMG_TRUE;
			break;
		}

		/* Calculate register def */
		InitRegUseDef(&sPrevInstDef);
		InstDef(psState, psPrevInst, &sPrevInstDef);

		/*
			Check if this instruction defines any register used or defined by the DPC.
		*/
		bNotDisjoint = DisjointUseDef(psState, &sDPCUse, &sPrevInstDef);
		if (bNotDisjoint)
		{
			bNotDisjoint = DisjointUseDef(psState, &sDPCDef, &sPrevInstDef);
		}
		ClearRegUseDef(psState, &sPrevInstDef);
		if (!bNotDisjoint)
		{
			break;
		}

		/* Calculate register use */
		InitRegUseDef(&sPrevInstUse);
		InstUse(psState, psPrevInst, &sPrevInstUse);

		/*
			Check if this instruction uses any register either used or defined by the DPC.
		*/
		bNotDisjoint = DisjointUseDef(psState, &sDPCUse, &sPrevInstUse);
		if (bNotDisjoint)
		{
			bNotDisjoint = DisjointUseDef(psState, &sDPCDef, &sPrevInstUse);
		}
		ClearRegUseDef(psState, &sPrevInstUse);
		if (!bNotDisjoint)
		{
			break;
		}
	}

	/*
		Free the USE/DEF information for the DPC instruction.
	*/
	ClearRegUseDef(psState, &sDPCDef);
	ClearRegUseDef(psState, &sDPCUse);

	return bOptimized;
}

static IMG_BOOL MergeDPWithTest(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psBlock,
								PINST				psInst)
/*****************************************************************************
 FUNCTION	: MergeDPWithTest

 PURPOSE	: Try to merge a dotproduct instruction together with a test on
			  the result.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psInst			- Instruction to try to merge.

 RETURNS	: TRUE if the instruction was merged.
*****************************************************************************/
{
	PINST		psPrevInst;
	IMG_BOOL	bTestMask;
	IOPCODE		eNewOpcode;
	IMG_BOOL	bKilledTestSource;

	/*
		Check the current instruction is a TEST or a TESTMASK.
	*/
	if (psInst->eOpcode == ITESTPRED && psInst->u.psTest->eAluOpcode == IFMOV)
	{
		bTestMask = IMG_FALSE;
	}
	else if (psInst->eOpcode == ITESTMASK && psInst->u.psTest->eAluOpcode == IFMOV)
	{
		bTestMask = IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
	/*
		Check the previous instruction is a dotproduct writing the source
		to the test.
	*/
	if (psInst->psPrev == NULL)
	{
		return IMG_FALSE;
	}
	psPrevInst = psInst->psPrev;
	if (
			!(
				psPrevInst->eOpcode == IFDP_RPT &&
				psPrevInst->asDest[0].uType == psInst->asArg[0].uType &&
				psPrevInst->asDest[0].uNumber == psInst->asArg[0].uNumber
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Either the result of the dotproduct is only used here or the
		TEST instruction doesn't do a writeback.
	*/
	bKilledTestSource = UseDefIsSingleSourceRegisterUse(psState, psInst, 0 /* uSrcIdx */);
	if (!bKilledTestSource && (bTestMask || psInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT))
	{
		return IMG_FALSE;
	}
	/*
		No source modifiers on TEST instructions.
	*/
	if (
			psPrevInst->u.psFdp->abNegate[0] ||
			psPrevInst->u.psFdp->abAbsolute[0] ||
			psPrevInst->u.psFdp->abNegate[1] ||
			psPrevInst->u.psFdp->abAbsolute[1]
	   )
	{
		return IMG_FALSE;
	}
	/*
		DP with test is broken on some cores
	*/
	if (!bTestMask && (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_23164) != 0)
	{
		return IMG_FALSE;
	}


	/*
		Check for too many repeats for a TEST instruction with a dot-product ALU op.
	*/
	if (psPrevInst->u.psFdp->uRepeatCount > EURASIA_USE1_TEST_MAX_REPEAT)
	{
		return IMG_FALSE;
	}

	/*
		Convert the dotproduct to a TEST.
	*/
	eNewOpcode = bTestMask ? IFDP_RPT_TESTMASK : IFDP_RPT_TESTPRED;
	ModifyOpcode(psState, psPrevInst, eNewOpcode);

	/*
		Copy the condition to test for from the old TEST instruction.
	*/
	psPrevInst->u.psFdp->sTest = psInst->u.psTest->sTest;

	/*
		If the DP result isn't only used in the old TEST instruction then enable the TEST
		instruction's unified store writeback.
	*/
	if (!bKilledTestSource)
	{
		ASSERT(!bTestMask);
		ASSERT(psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT);
		SetDestCount(psState, psPrevInst, TEST_MAXIMUM_DEST_COUNT);

		/* Set the unified store destination to the old DP destination. */
		MoveDest(psState, psPrevInst, TEST_UNIFIEDSTORE_DESTIDX, psPrevInst, 0);

		/* Copy the predicate destination from the old TEST instruction. */
		MoveDest(psState, psPrevInst, TEST_PREDICATE_DESTIDX, psInst, TEST_PREDICATE_DESTIDX);
	}
	else
	{
		/*
			Otherwise copy the TEST/TESTMASK's destination to the dotproduct.
		*/
		MoveDest(psState, psPrevInst, 0, psInst, 0);
	}
	
	/*
		Free the TEST instruction.
	*/
	RemoveInst(psState, psBlock, psInst);
	FreeInst(psState, psInst);

	return IMG_TRUE;
}

static IMG_VOID GenerateDotProdBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
	/*
		Generate DP/DDP instructions.
	*/
{
	PINST	 psInst, psNextInst;
	IMG_BOOL bDeadInstructions;
	PVR_UNREFERENCED_PARAMETER (pvNull);

	bDeadInstructions = IMG_FALSE;
	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;

		/*
			Try to combine this instruction with following ones into a DOTPRODUCT
		*/
		if (GenerateDotProducts(psState, psBlock, psInst, &psNextInst))
		{
			continue;
		}

		/*
			Check if we can convert a test on the result of the dotproduct into a
			test instruction with DP alu opcode.
		*/
		if (MergeDPWithTest(psState, psBlock, psInst))
		{
			continue;
		}

		/*
			Try to optimize DPC instructions.
		*/
		if (psInst->eOpcode == IFDPC)
		{
			if (!OptimizePFlagCalc(psState, psBlock, psInst))
			{
				GenerateExtraDPCIteration(psState, psBlock, psInst);
			}
			else
			{
				bDeadInstructions = IMG_TRUE;
			}
			continue;
		}

		/*
			Expand intermediate instructions which don't have an exact equivalent in the
			hardware.
		*/
		ExpandMacroInstruction(psState, psBlock, psInst);
	}

	/*
		Optimizing a DPC instruction might now mean some instructions have unused results.
	*/
	if (bDeadInstructions)
	{
		DeadCodeEliminationBP(psState, psBlock, (IMG_PVOID)(IMG_UINTPTR_T)IMG_TRUE);
	}
}

static IMG_VOID AdjustToMOEUnitsInstArg(PINTERMEDIATE_STATE	psState,
									    PINST				psInst,
									    IMG_BOOL			bDest,
									    IMG_UINT32			uArgIdx,
									    IMG_UINT32			uGroupCount,
									    HWREG_ALIGNMENT		eGroupAlign,
									    IMG_PVOID			pvContext)
/*****************************************************************************
 FUNCTION	: AdjustToMOEUnitsInstArg

 PURPOSE	: Convert a register number to the units of the MOE state.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to which the argument is a
								source or a destination.
			  bDest				- TRUE if the argument is a destination.
								  FALSE if the argument is a source.
			  uArgIdx			- Argument.
			  uGroupCount		- Ignored.
			  eGroupAlign		- Ignored.
			  pvContext			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMOEUnitsInBytesLog2;
	PARG		psArg;
	
	PVR_UNREFERENCED_PARAMETER(uGroupCount);
	PVR_UNREFERENCED_PARAMETER(eGroupAlign);
	PVR_UNREFERENCED_PARAMETER(pvContext);
	
	if (bDest)
	{
		ASSERT(uArgIdx < psInst->uDestCount);
		psArg = &psInst->asDest[uArgIdx];
	}
	else
	{
		ASSERT(uArgIdx < psInst->uArgumentCount);
		psArg = &psInst->asArg[uArgIdx];
	}

	if (psArg->uType == USEASM_REGTYPE_IMMEDIATE || 
		psArg->uType == USEASM_REGTYPE_INDEX || 
		psArg->uType == USEASM_REGTYPE_PREDICATE)
	{
		return;
	}

	uMOEUnitsInBytesLog2 = GetMOEUnitsLog2(psState, psInst, bDest, uArgIdx);

	if (uMOEUnitsInBytesLog2 > LONG_SIZE_LOG2)
	{
		IMG_UINT32	uAdjustLog2 = uMOEUnitsInBytesLog2 - LONG_SIZE_LOG2;

		ASSERT((psArg->uNumber % (1U << uAdjustLog2)) == 0);
		psArg->uNumber >>= uAdjustLog2;
	}
	else if (uMOEUnitsInBytesLog2 < LONG_SIZE_LOG2)
	{
		IMG_UINT32	uAdjustLog2 = LONG_SIZE_LOG2 - uMOEUnitsInBytesLog2;
		IMG_UINT32	uCompInRegUnits;
		IMG_UINT32	uComponent;

		ASSERT(!bDest);

		psArg->uNumber <<= uAdjustLog2;

		uComponent = GetComponentSelect(psState, psInst, uArgIdx);

		uCompInRegUnits = uComponent >> uMOEUnitsInBytesLog2;

		uComponent &= ((1U << uMOEUnitsInBytesLog2) - 1);
		ASSERT(uComponent == 0);

		psArg->uNumber += uCompInRegUnits;
	}
}

static IMG_VOID AdjustToMOEUnitsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvIgnored)
/*****************************************************************************
 FUNCTION	: AdjustToMOEUnitsBP

 PURPOSE	: Convert register numbers to the units of the MOE state.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to process.
			  pvIgnored			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvIgnored);

	/*
		Convert register numbers to the units of the MOE state.
	*/
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		if (GetBit(psInst->auFlag, INST_NOEMIT))
		{
			continue;
		}

		ProcessSourceRegisterGroups(psState, psInst, AdjustToMOEUnitsInstArg, NULL /* pvContext */);
		ProcessDestRegisterGroups(psState, psInst, AdjustToMOEUnitsInstArg, NULL /* pvContext */);
	}
}

/* Is format control ever required to be on or off? */
typedef struct
{
	IMG_BOOL	bEfoFmtCtrlEverOff, bEfoFmtCtrlEverOn;
	IMG_BOOL	bColFmtCtrlEverOff, bColFmtCtrlEverOn;
	IMG_BOOL	bFormatChangeAfterFeedback;
} FMT_CTRL_EVER, *PFMT_CTRL_EVER;

static IMG_VOID SearchFormatControlBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvFmtCtrl)
/*****************************************************************************
 FUNCTION	: SearchFormatControlBP

 PURPOSE	: Checks if a block contains any instructions that require the MOE
			  format control to be either on or off.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to process.
			  pvIgnored			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFMT_CTRL_EVER	psFmtCtrl = (PFMT_CTRL_EVER)pvFmtCtrl;
	PINST			psInst;
	IMG_BOOL		bNeedControl = IMG_FALSE;

	/*
		now look for needing a SETFC, but not in the SA program.
	*/
	if (psBlock->psOwner->psFunc == psState->psSecAttrProg) return;

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		IMG_BOOL bNeedsFmtCtrlOff, bNeedsFmtCtrlOn;
		if (!(psInst->eOpcode == IEFO || HasC10FmtControl(psInst)))
		{
			continue;
		}
		GetFormatControlRequirements(psState, psInst, &bNeedsFmtCtrlOff, &bNeedsFmtCtrlOn, NULL);
		if (psInst->eOpcode == IEFO)
		{
			if (bNeedsFmtCtrlOff)
			{
				psFmtCtrl->bEfoFmtCtrlEverOff = IMG_TRUE;
				bNeedControl = IMG_TRUE;
			}
			if (bNeedsFmtCtrlOn)
			{
				psFmtCtrl->bEfoFmtCtrlEverOn = IMG_TRUE;
				bNeedControl = IMG_TRUE;
			}
		}
		else
		{
			if (bNeedsFmtCtrlOff)
			{
				psFmtCtrl->bColFmtCtrlEverOff = IMG_TRUE;
				bNeedControl = IMG_TRUE;
			}
			if (bNeedsFmtCtrlOn)
			{
				psFmtCtrl->bColFmtCtrlEverOn = IMG_TRUE;
				bNeedControl = IMG_TRUE;
			}
		}
	}

	/* Check whether this block is after a feedback phase */
	if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && bNeedControl)
	{
		IMG_UINT32 uBlock;
		for (uBlock = 0; uBlock < psBlock->psOwner->uNumBlocks; uBlock++)
		{
			PCODEBLOCK psPrevBlock = psBlock->psOwner->apsAllBlocks[uBlock];	
			if (psPrevBlock == psBlock)
			{
				break;
			}

			if (psPrevBlock == psState->psPreFeedbackDriverEpilogBlock)
			{
				psFmtCtrl->bFormatChangeAfterFeedback = IMG_TRUE;
				break;
			}
		}
	}
}

static IMG_VOID CheckBaseOffsetBasicBlock(PINTERMEDIATE_STATE	psState,
										  PGROUPINST_STATE		psGIState,
										  PCODEBLOCK			psBlock,
										  IMG_PVOID				pvMoeState,
										  IMG_BOOL				bInsert)
/*****************************************************************************
 FUNCTION	: CheckBaseOffsetBasicBlock

 PURPOSE	: Inserts any SMBO instructions needed by a block.

 PARAMETERS	: psState			- Compiler state.
			  psGIState			- Module state.
			  psBlock			- Block to process.
			  psMoeState		- On entry the MOE state before the first
								  instruction.
								  On exit the MOE state after the last
								  instruction.
			  bInsert			- If TRUE actually insert SMBO instructions.
								  If FALSE only update the MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;
	PINST		psNextInst;
	PSMBO_STATE	psMoeState = (PSMBO_STATE)pvMoeState;

	PVR_UNREFERENCED_PARAMETER(psGIState);

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/*
			For the patcher record the MOE base offset state before the first instruction.
		*/
		RecordSmboStateForBlock(psBlock, psMoeState);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Use the MOE base offset feature to access registers whose numbers are
		too large to be reached directly.
	*/
	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		CheckBaseOffset(psState, psBlock, psInst, psMoeState, &psNextInst, bInsert);
	}
}

static IMG_VOID CheckFormatControlBasicBlock(PINTERMEDIATE_STATE	psState,
											 PGROUPINST_STATE		psGIState,
											 PCODEBLOCK				psBlock,
											 IMG_PVOID				pvMoeState,
											 IMG_BOOL				bInsert)
/*****************************************************************************
 FUNCTION	: CheckFormatControlBasicBlock

 PURPOSE	: Inserts any SETFC instructions needed by a block.

 PARAMETERS	: psState			- Compiler state.
			  psGIState			- Module state.
			  psBlock			- Block to process.
			  psMoeState		- On entry the MOE state before the first
								  instruction.
								  On exit the MOE state after the last
								  instruction.
			  bInsert			- If TRUE actually insert SETFC instructions.
								  If FALSE only update the MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInst;
	PSETFC_STATE	psMoeState = (PSETFC_STATE)pvMoeState;

	PVR_UNREFERENCED_PARAMETER(psGIState);

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/*
			For the patcher record the MOE format control state before the first instruction.
		*/
		RecordSetfcStateForBlock(psBlock, psMoeState);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		/* Check for needing a SETFC instruction. */
		CheckFormatControl(psState, psBlock, psInst, psMoeState, bInsert);
	}
}

static IMG_VOID GroupInstructionsBasicBlock(PINTERMEDIATE_STATE	psState, 
											PGROUPINST_STATE	psGIState, 
											PCODEBLOCK			psBlock, 
											IMG_PVOID			pvMoeState, 
											IMG_BOOL			bInsert)
/*****************************************************************************
 FUNCTION	: GroupInstructionsBlock

 PURPOSE	: Generate repeating instructions for a block of code.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Code block.
			  pvMoeState		- PSMLSI_STATE to MOE state before the block.
			  bInsert			- If TRUE, actually inserts MOE instructions;
								  if FALSE, only updates the MOE state.

 RETURNS	: Nothing, but sets psMoeState to state at end.
*****************************************************************************/
{
	PINST psInst;
	PINST* psInstGroup;
	MOE_SMLSI_SITE	sMoeSite;
	PSMLSI_STATE psMoeState = (PSMLSI_STATE)pvMoeState;
	
	/* Record the MOE state at the start of this block */
	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		RecordSmlsiStateForBlock(psBlock, psMoeState);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Prepare each instruction for repeat grouping by initialising
		the repeat group data and linking instructions with the same
		opcode so that GroupInstructions doesn't have to search
		through the code-block.
	*/
	memset(psGIState->asInstGroup, (IMG_UINT32)(IMG_UINTPTR_T)NULL, sizeof(psGIState->asInstGroup));
	psInstGroup = psGIState->asInstGroup;
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		PINST psPrevInst;
		PINST* ppsInstGroup;

		if (GetBit(psInst->auFlag, INST_NOEMIT))
		{
			continue;
		}

		/* Initialise instruction data. */
		if (psInst->psRepeatGroup != NULL)
		{
			memset(psInst->psRepeatGroup, 0, sizeof(REPEAT_GROUP));
		}
		else
		{
			psInst->psRepeatGroup = AllocRepeatGroup(psState);
		}

		if (psInst->eOpcode == IFMAD &&
			psInst->asArg[1].uType == USEASM_REGTYPE_FPCONSTANT &&
			psInst->asArg[1].uNumberPreMoe == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 &&
			!IsNegated(psState, psInst, 1 /* uArgIdx */))
		{
			ppsInstGroup = &psInstGroup[IFADD];
		}
		else if (psInst->eOpcode == IFMAD &&
				 psInst->asArg[2].uType == USEASM_REGTYPE_FPCONSTANT &&
				 psInst->asArg[2].uNumberPreMoe == EURASIA_USE_SPECIAL_CONSTANT_ZERO)
		{
			ppsInstGroup = &psInstGroup[IFMUL];
		}
		else
		{
			ppsInstGroup = &psInstGroup[psInst->eOpcode];
		}

		/* Link together instructions with the same opcode. */
		psPrevInst = *ppsInstGroup;
		if (psPrevInst != NULL)
		{
			psInst->psRepeatGroup->psPrev = psPrevInst;
			psPrevInst->psRepeatGroup->psNext = psInst;
		}
		*ppsInstGroup = psInst;
	}

	/*
		Reset information about a possible location to insert an SMLSI costing
		zero cycles.
	*/
	sMoeSite.psLocation = NULL;

	/* Group instructions */
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		ASSERT(psInst->eOpcode != ISMLSI);

		if (GetBit(psInst->auFlag, INST_NOEMIT)
				/* no psRepeatGroup, but still need to RecalcInstRegs */
			|| psInst->psRepeatGroup)
		{
			IMG_BOOL	bCreatedRepeat;

			/* Check if we could group this instruction with those following. */
			bCreatedRepeat = 
				GroupInstructions(psState, 
								  psBlock, 
								  psInst,
								  psMoeState, 
								  &(psGIState->sUseDef),
								  &sMoeSite,
								  bInsert);

			/*
			   At this point, the assumption is that GroupInstructions ended
			   by collapsing a group of one or more instruction to one instruction
			   possibly adding an smlsi immediately before the instruction.
			*/

			if (!bCreatedRepeat)
			{
				psBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;
				/*
					Record the constraints on what MOE state can be loaded by
					an SMLSI instruction inserted after the last repeat.
				*/
				if (sMoeSite.psLocation != NULL)
				{
					UpdateSMLSISiteConstraints(psState, &sMoeSite, psInst);
				}
			}
		}
		/* Remove repeat group if we definitely don't need it any more */
		if (bInsert) FreeRepeatGroup(psState, &psInst->psRepeatGroup);

	}

	/*
		If the state at the end of a block has non-zero offsets for
		the first iteration for any operand then reset it to the
		default. This means successor blocks can rely on the MOE
		state not affecting non-repeated instructions even if the
		MOE state is unknown because the block has multiple
		predecessors.
	*/
	if (IsNonZeroSwizzle(psMoeState->psMoeData, USC_MAX_MOE_OPERANDS))
	{
		ResetMoeOpState(psMoeState);

		if (bInsert)
		{
			PINST psSMLSIInst = AllocateInst(psState, NULL);

			ASSERT(sMoeSite.psLocation != NULL);

			MoeToInst(psState, psMoeState, psSMLSIInst);
			InsertInstBefore(psState, psBlock, psSMLSIInst, sMoeSite.psLocation->psNext);
		}
	}
}

typedef struct _CALC_STATE
/* captures all state needed for GroupInstsDF dataflow function */
{
	PGROUPINST_STATE	psGIState;

	/*
		Size in bytes of a state structure.
	*/
	IMG_UINT32			uStateSize;

	/*
		MOE state at the start of the main program.
	*/
	IMG_PVOID			pvInitial;

	/*
		MOE state that is set after a function call returns.
	*/
	IMG_PVOID			pvPostFuncState;

	/*
		Writes the default MOE state into the state structure pointed to by the argument.
	*/
	IMG_VOID			(*pfSetDefault)(IMG_PVOID);

	/*
		Merges the second MOE state into the first.
	*/
	IMG_VOID			(*pfMerge)(IMG_PVOID, IMG_PVOID);

	/*
		Insert MOE control instructions into a basic block.
	*/
	IMG_VOID			(*pfProcessBasicBlock)(PINTERMEDIATE_STATE, PGROUPINST_STATE, PCODEBLOCK, IMG_PVOID, IMG_BOOL);

	/*
		Insert an extra MOE control instruction needed to reset the MOE state in a function
		epilog.
	*/
	IMG_VOID			(*pfProcessFuncEnd)(PINTERMEDIATE_STATE, PFUNC, struct _CALC_STATE*, IMG_PVOID);

	/*
		If TRUE, actually insert MOE instructions; if FALSE, only compute MOE state
		(only affects behaviour by being passed to callbacks, apart from that data
		for functions is ONLY accumulated if FALSE)
	*/
	IMG_BOOL			bInsert;
} CALC_STATE, *PCALC_STATE;

static IMG_BOOL GroupInstsDF(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvResult, IMG_PVOID *ppvArgs, IMG_PVOID pvCalcState)
{
	PCALC_STATE psCalcState = (PCALC_STATE)pvCalcState;
	PGROUPINST_STATE psGIState = psCalcState->psGIState;
	IMG_UINT32 uPred;
	IMG_PVOID pvBlockState;

	/*
		Allocate space for the state before the block.
	*/
	pvBlockState = UscAlloc(psState, psCalcState->uStateSize);

	/*
		1. compute value at block entry by merging predecessors
	*/
	if (psBlock == psBlock->psOwner->psEntry)
	{
		memcpy(pvBlockState, psCalcState->pvInitial, psCalcState->uStateSize);
		uPred = 0;
	}
	else if (psBlock->uNumPreds == 0)
	{
		//no predecessors - must be unreachable exit (silly program)
		ASSERT (psBlock == psBlock->psOwner->psExit);
		psCalcState->pfSetDefault(pvBlockState); //doesn't really matter...
		uPred = 0;
	}
	else if (
				(psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && 
				psState->psPreFeedbackDriverEpilogBlock == psBlock->asPreds[0].psDest
			)
	{
		ASSERT (psState->psPreFeedbackDriverEpilogBlock->eType == CBTYPE_UNCOND);
		ASSERT (psBlock->uNumPreds == 1);
		// state after resumption is simply the default state.
		psCalcState->pfSetDefault(pvBlockState);
		uPred = 1; // (Ignore state at end of pre-feedback stage)
	}
	else if ((psState->uFlags2 & USC_FLAGS2_SPLITCALC) && (psState->psPreSplitBlock == psBlock->asPreds[0].psDest))
	{
		ASSERT (psState->psPreSplitBlock->eType == CBTYPE_UNCOND);
		ASSERT (psBlock->uNumPreds == 1);
		// state after resumption is simply the default state.
		psCalcState->pfSetDefault(pvBlockState);
		uPred = 1; // (Ignore state at end of pre-split stage)
	}
	else
	{
		memcpy(pvBlockState, ppvArgs[0], psCalcState->uStateSize);
		uPred = 1;
	}

	for ( /*uPred set already*/ ; uPred < psBlock->uNumPreds; uPred++) 
	{
		psCalcState->pfMerge(pvBlockState, ppvArgs[uPred]);
	}

	//2. process instructions in body
	if (IsCall(psState, psBlock))
	{
		IMG_UINT32 uLabel = psBlock->psBody->u.psCall->psTarget->uLabel;
		ASSERT (psBlock->psOwner->psFunc != psState->psSecAttrProg);
		/*
			Are we gathering data on CALLs?
			if bInsert, we aren't, as gathered already; however, possible that bInsert is FALSE
			but we still aren't, 'coz we're just scanning through a function body while trying to
			insert instructions only into caller (and not callee)...
		*/
		if (!psCalcState->bInsert)
		{
			PGROUPINST_FUNCSTATE	psFuncState = &psGIState->asFunc[uLabel];

			/*
				Accumulate the possible states before the function.
			*/
			if (psFuncState->bUsed)
			{
				psCalcState->pfMerge(psFuncState->pvInitialState, pvBlockState);
			}
			else
			{
				psFuncState->bUsed = IMG_TRUE;
				memcpy(psFuncState->pvInitialState, pvBlockState, psCalcState->uStateSize);
			}
		}

		/*
			The state after the function will be reset to a fixed value.
		*/
		if (psCalcState->pvPostFuncState != NULL)
		{
			memcpy(pvBlockState, psCalcState->pvPostFuncState, psCalcState->uStateSize);
		}
		else
		{
			psCalcState->pfSetDefault(pvBlockState);
		}
	}
	else
	{
		/*
			Process instructions in the block. Updates pvBlockState to the state after the
			last instruction.
		*/
		psCalcState->pfProcessBasicBlock(psState, psGIState, psBlock, pvBlockState, psCalcState->bInsert);
	}

	{
		/*
			check to see if the result at the end of the block has changed.
			(certainly true on the first pass!). If so, will re-process successors;
			hence, this'll handle loops, which'll get a reset at the beginning to
			deal with the merge of loop entry/repeat states (if necessary!).
		*/
		IMG_BOOL bDifferent = memcmp(pvResult, pvBlockState, psCalcState->uStateSize) ? IMG_TRUE : IMG_FALSE;
		ASSERT (!bDifferent || !psCalcState->bInsert);

		/*
			If the result at the end of the block has changed update the stored
			state for the block.
		*/
		if (bDifferent)
		{
			memcpy(pvResult, pvBlockState, psCalcState->uStateSize);
		}

		/*
			Free space for the old state.
		*/
		UscFree(psState, pvBlockState);

		return bDifferent;
	}
}

static IMG_PVOID DoGroupInsts(PINTERMEDIATE_STATE	psState,
							  PFUNC					psFunc,
							  IMG_PVOID				pvInitial,
							  PGROUPINST_STATE		psGIState,
							  PCALC_STATE			psCalcState)
{
	IMG_PVOID asBlockEnds;
	IMG_UINT32 uBlockIdx;

	asBlockEnds = UscAlloc(psState, psFunc->sCfg.uNumBlocks * psCalcState->uStateSize);
	for (uBlockIdx = 0; uBlockIdx < psFunc->sCfg.uNumBlocks; uBlockIdx++)
	{
		IMG_PVOID	pvBlockState;

		pvBlockState = (IMG_PVOID)((IMG_PCHAR)asBlockEnds + uBlockIdx * psCalcState->uStateSize);
		psCalcState->pfSetDefault(pvBlockState);
	}

	psCalcState->psGIState = psGIState;
	psCalcState->pvInitial = pvInitial;

	/*
		First pass: only compute the MOE state (don't insert any instructions),
		iterating until a fixpoint is reached. (The existing GroupInstructions
		etc. code breaks if passed a block which it's already *modified*; since
		iterating a dataflow function to a fixpoint may process some blocks
		multiple times, setting bInsert to FALSE forces GroupInstructions not
		to modify any blocks)
	*/
	psCalcState->bInsert = IMG_FALSE;
	DoDataflow(psState, psFunc, IMG_TRUE /*forwards*/,
		psCalcState->uStateSize, asBlockEnds, GroupInstsDF, psCalcState);

	/*
		Second pass: actually insert/modify the blocks according to the states
		computed by the first pass.
		(Thus, on each block the dataflow function should always return the
		same state as it did in the final iteration of the first pass, i.e.
		such that the first pass correctly predicted the outcome of the second;
		hence no block should have to be processed more than once, avoiding
		the problem with GroupInstructions above. TODO, if GroupInstructions
		etc. were reimplemented to be able to process blocks it had already
		modified, we could do everything in one pass and remove the bInsert
		distinction.)
	*/
	psCalcState->bInsert = IMG_TRUE;
	DoDataflow(psState, psFunc, IMG_TRUE /*forwards*/,
		psCalcState->uStateSize, asBlockEnds, GroupInstsDF, psCalcState);

	//desired result is state at end of fn. However, to avoid a copy, we'll return the whole lot.
	return asBlockEnds;
}

static
IMG_VOID DoMOEPass(PINTERMEDIATE_STATE	psState,
				   PGROUPINST_STATE		psGIState,
				   PCALC_STATE			psCalcState,
				   IMG_PVOID			pvMainInitialState)
/*****************************************************************************
 FUNCTION	: CheckFeaturesUsedBP

 PURPOSE	: BLOCK_PROC to check which MOE features could be used.

 PARAMETERS	: psState				- Compiler state.
			  pvGIState				- PGROUPINST_STATE to Instruction grouping data.

 RETURNS	: Nothing, but may set the element of pvGIState->abCallUsesSetFc
	and abCallUsesBaseOffset for the function containing the current block.
*****************************************************************************/
{
	PGROUPINST_FUNCSTATE	psMainFuncState;
	PFUNC					psFunc;
	IMG_UINT32				uLabel;

	/*
		For each function allocate memory to hold the MOE state before
		the start of the function.
	*/
	for (uLabel = 0; uLabel < psState->uMaxLabel; uLabel++)
	{
		PGROUPINST_FUNCSTATE	psFuncState;

		psFuncState = &psGIState->asFunc[uLabel];

		psFuncState->pvInitialState = UscAlloc(psState, psCalcState->uStateSize);
		psFuncState->bUsed = IMG_FALSE;
	}

	/*
		Initialize the MOE state before the main function.
	*/
	psMainFuncState = &psGIState->asFunc[psState->psMainProg->uLabel];
	if (pvMainInitialState != NULL)
	{
		memcpy(psMainFuncState->pvInitialState, pvMainInitialState, psCalcState->uStateSize);
	}
	else
	{
		psCalcState->pfSetDefault(psMainFuncState->pvInitialState);
	}
	psMainFuncState->bUsed = IMG_TRUE;

	/*
		Initialize the MOE state before the secondary update program.
	*/
	if (psState->psSecAttrProg)
	{
		PGROUPINST_FUNCSTATE	psSecAttrProgFuncState;

		psSecAttrProgFuncState = &psGIState->asFunc[psState->psSecAttrProg->uLabel];
		psCalcState->pfSetDefault(psSecAttrProgFuncState->pvInitialState);

		psSecAttrProgFuncState->bUsed = IMG_TRUE;
	}

	/*
		Do the MOE pass for all functions.
	*/
	for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
	{
		IMG_PVOID				asAll;
		PGROUPINST_FUNCSTATE	psFuncState = &psGIState->asFunc[psFunc->uLabel];

		ASSERT (psFuncState->bUsed);

		/*
			Do the MOE pass for this function returning the MOE state after each block.
		*/
		asAll = DoGroupInsts(psState, psFunc, psFuncState->pvInitialState, psGIState, psCalcState);

		/*
			Do any work needed based on the MOE state at the end of the function.
		*/
		if (psCalcState->pfProcessFuncEnd != NULL)
		{
			psCalcState->pfProcessFuncEnd(psState, psFunc, psCalcState, asAll);
		}

		/*
			Free block end states.
		*/
		UscFree(psState, asAll);
	}

	/*
		Free memory allocated for the initial MOE state for each function.
	*/
	for (uLabel = 0; uLabel < psState->uMaxLabel; uLabel++)
	{
		PGROUPINST_FUNCSTATE	psFuncState;

		psFuncState = &psGIState->asFunc[uLabel];

		UscFree(psState, psFuncState->pvInitialState);
	}
}


static
IMG_VOID SMLSI_ProcessFuncEnd(PINTERMEDIATE_STATE	psState,
							  PFUNC					psFunc,
							  PCALC_STATE			psCalcState,
							  IMG_PVOID				pvAll)
/*****************************************************************************
 FUNCTION	: SMLSI_ProcessFuncEnd

 PURPOSE	: Do any work needed after generating repeating instructions in a function.

 PARAMETERS	: psState			- Compiler state.
			  psFunc			- Function that was just processed.
			  pvAll				- MOE state after each block in the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSMLSI_STATE	asAll = (PSMLSI_STATE)pvAll;
	PCODEBLOCK		psResetBlock;
	PSMLSI_STATE	psEndState;
	IMG_UINT32		uArg;

	PVR_UNREFERENCED_PARAMETER(psCalcState);

	if (psFunc == psState->psMainProg)
	{
		/*
			If this flag is set then the MOE state after the program can be anything.
		*/
		if (psState->uCompilerFlags & UF_DONTRESETMOEAFTERPROGRAM)
		{
			return;
		}

		/*
			Insert instructions to restore a default state at the end of the program. If splitting
			for feedback is on then the end of the program (for this purpose) is the point where
			feedback is done.
		*/
		if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
		{
			psResetBlock = psState->psPreFeedbackBlock;
		}
		else
		{
			psResetBlock = psState->psMainProg->sCfg.psExit;
		}
	}
	else
	{
		/*
			Reset the MOE state at the function exit.
		*/
		psResetBlock = psFunc->sCfg.psExit;
	}

	/*
		Get the MOE state at the end of the function.
	*/
	psEndState = &asAll[psResetBlock->uIdx];

	/*
		Check for needing to reset the MOE state.
	*/
	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		PMOE_DATA	psMoeData;
		psMoeData = &psEndState->psMoeData[uArg];
		if (psFunc == psState->psMainProg)
		{
			/*
				Check for swizzle mode with the first channel selection non-zero.
			*/
			if (
					psMoeData->eOperandMode == MOE_SWIZZLE &&
					psMoeData->u.s.auSwizzle[0] != 0
			   )
			{
				break;
			}
		}
		else
		{
			/*
				Check for multiple possible MOE states.
			*/
			if (psEndState->pbInvalid[uArg])
			{
				break;
			}
			/*
				Check for the MOE state different to the default state.
			*/
			if (!EqualMoeData(psMoeData, &sDefaultSmsliState.psMoeData[uArg], 1))
			{
				break;
			}
		}
	}

	if (uArg < USC_MAX_MOE_OPERANDS)
	{
		/*
			Insert an instruction to reset the increments back to their defaults.
		*/
		PINST	psSmlsiInst;

		ResetMoeOpState(psEndState);

		/* Make the reset instruction */
		psSmlsiInst = AllocateInst(psState, IMG_NULL);

		MoeToInst(psState,psEndState, psSmlsiInst);

		/* Add it to the epilogue */
		AppendInst(psState, psResetBlock, psSmlsiInst);
	}
}

static
IMG_VOID SMLSI_Merge(IMG_PVOID	pvDest,
					 IMG_PVOID	pvSrc)
/*****************************************************************************
 FUNCTION	: SMLSI_Merge

 PURPOSE	: Merge two possible MOE states together.

 PARAMETERS	: pvDest			- On entry: the first state to merge.
								  On exit: the merged states.
			  pvSrc				- The second state to merge.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 i;
	PSMLSI_STATE psDest = (PSMLSI_STATE)pvDest;
	PSMLSI_STATE psSrc = (PSMLSI_STATE)pvSrc;

	for (i = 0; i < USC_MAX_MOE_OPERANDS; i++)
	{
		if (!EqualMoeData(&psDest->psMoeData[i], &psSrc->psMoeData[i], 1) ||
			psSrc->pbInvalid[i])
		{
			/* If the states don't match then mark the destination as invalid. */
			psDest->pbInvalid[i] = IMG_TRUE;
		}
	}
}

static IMG_VOID SMLSI_SetDefault(IMG_PVOID			pvState)
/*****************************************************************************
 FUNCTION	: SMLSI_SetDefault

 PURPOSE	: Copy the default MOE state.

 PARAMETERS	: pvState	- Updated with the default MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ResetMoeOpState((PSMLSI_STATE)pvState);
}

static
IMG_VOID InsertSmlsi(PINTERMEDIATE_STATE	psState,
					 PGROUPINST_STATE		psGIState)
/*****************************************************************************
 FUNCTION	: InsertSmlsi

 PURPOSE	: Generate repeating instructions.

 PARAMETERS	: psState			- Compiler state.
			  psGIState			- Module state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	CALC_STATE		sCalcState;

	sCalcState.psGIState = psGIState;

	sCalcState.uStateSize = sizeof(SMLSI_STATE);
	sCalcState.pvInitial = NULL;
	sCalcState.pvPostFuncState = NULL;

	sCalcState.pfProcessBasicBlock = GroupInstructionsBasicBlock;
	sCalcState.pfSetDefault = SMLSI_SetDefault;
	sCalcState.pfMerge = SMLSI_Merge;
	sCalcState.pfProcessFuncEnd = SMLSI_ProcessFuncEnd;

	DoMOEPass(psState, psGIState, &sCalcState, NULL);
}

static
IMG_VOID SMBO_SetDefault(IMG_PVOID			pvState)
/*****************************************************************************
 FUNCTION	: SMBO_SetDefault

 PURPOSE	: Copy the default MOE state.

 PARAMETERS	: pvState	- Updated with the default MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSMBO_STATE	psState	= (PSMBO_STATE)pvState;
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psState->pbInvalidBaseOffsets[uArg] = IMG_FALSE;
		psState->puBaseOffsets[uArg] = 0;
	}
}

static
IMG_VOID SMBO_Merge(IMG_PVOID	pvDest,
					IMG_PVOID	pvSrc)
/*****************************************************************************
 FUNCTION	: SMBO_Merge

 PURPOSE	: Merge two possible MOE states together.

 PARAMETERS	: pvDest			- On entry: the first state to merge.
								  On exit: the merged states.
			  pvSrc				- The second state to merge.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSMBO_STATE	psDest = (PSMBO_STATE)pvDest;
	PSMBO_STATE	psSrc = (PSMBO_STATE)pvSrc;
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		if (psDest->puBaseOffsets[uArg] != psSrc->puBaseOffsets[uArg] || psSrc->pbInvalidBaseOffsets[uArg])
		{
			psDest->pbInvalidBaseOffsets[uArg] = IMG_TRUE;
		}
	}
}

static
IMG_VOID SMBO_ProcessFuncEnd(PINTERMEDIATE_STATE	psState,
							 PFUNC					psFunc,
							 PCALC_STATE			psCalcState,
							 IMG_PVOID				pvAll)
/*****************************************************************************
 FUNCTION	: SMBO_ProcessFuncEnd

 PURPOSE	: Do any work needed after inserting SMBO instructions in a function.

 PARAMETERS	: psState			- Compiler state.
			  psFunc			- Function that was just processed.
			  pvAll				- MOE state after each block in the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSMBO_STATE	asAll = (PSMBO_STATE)pvAll;
	IMG_UINT32 uArg;
	PCODEBLOCK psResetBlock;
	PSMBO_STATE psEndState;
	IMG_BOOL bResetBaseOffset;

	PVR_UNREFERENCED_PARAMETER(psCalcState);

	if (psFunc == psState->psMainProg)
	{
		/*
			If this flag is set then the base offset at the end of the program can
			have any value.
		*/
		if (psState->uCompilerFlags & UF_DONTRESETMOEAFTERPROGRAM)
		{
			return;
		}

		/*
			Insert instructions to restore a default state at the end of the program. If splitting
			for feedback is on then the end of the program is the point where feedback is done.
		*/
		if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
		{
			psResetBlock = psState->psPreFeedbackBlock;
		}
		else
		{
			psResetBlock = psState->psMainProg->sCfg.psExit;
		}
	}
	else if (psFunc == psState->psSecAttrProg)
	{
		/*
			The base offsets at the end of the secondary update program can have
			any value.
		*/
		return;
	}
	else
	{
		/*
			Reset the MOE state at the function exit.
		*/
		psResetBlock = psFunc->sCfg.psExit;
	}

	psEndState = &asAll[psResetBlock->uIdx];

	/*
		Check for invalid values in the MOE state.
	*/
	bResetBaseOffset = IMG_FALSE;
	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		if (psEndState->pbInvalidBaseOffsets[uArg] || psEndState->puBaseOffsets[uArg] != 0)
		{
			bResetBaseOffset = IMG_TRUE;
		}
	}

	if (bResetBaseOffset)
	{
		/*
			Insert an instruction to reset the base offsets back to their
			default value.
		*/
		PINST	psSmboInst;

		psSmboInst = AllocateInst(psState, IMG_NULL);

		SetOpcodeAndDestCount(psState, psSmboInst, ISMBO, 0 /* uDestCount */);

		for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
		{
			psSmboInst->asArg[uArg].uType = USEASM_REGTYPE_IMMEDIATE;
			psSmboInst->asArg[uArg].uNumber = 0;
		}

		AppendInst(psState, psResetBlock, psSmboInst);
	}
}

static
IMG_VOID InsertSmbo(PINTERMEDIATE_STATE	psState,
					PGROUPINST_STATE	psGIState)
/*****************************************************************************
 FUNCTION	: InsertSmbo

 PURPOSE	: Insert SMBO instructions where instructions are using registers
			  numbers larger than directly addressable by the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psGIState			- Modulate state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	CALC_STATE		sCalcState;

	sCalcState.psGIState = psGIState;

	sCalcState.uStateSize = sizeof(SMBO_STATE);
	sCalcState.pvInitial = NULL;
	sCalcState.pvPostFuncState = NULL;

	sCalcState.pfProcessBasicBlock = CheckBaseOffsetBasicBlock;
	sCalcState.pfSetDefault = SMBO_SetDefault;
	sCalcState.pfMerge = SMBO_Merge;
	sCalcState.pfProcessFuncEnd = SMBO_ProcessFuncEnd;

	DoMOEPass(psState, psGIState, &sCalcState, NULL);
}

static
IMG_VOID SETFC_SetDefault(IMG_PVOID			pvState)
/*****************************************************************************
 FUNCTION	: SETFC_SetDefault

 PURPOSE	: Copy the default MOE state.

 PARAMETERS	: pvState	- Updated with the default MOE state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSETFC_STATE	psState = (PSETFC_STATE)pvState;

	/*
		Set the initial value of the format control for
		EFO instructions.
	*/
	#if EURASIA_USE_MOE_INITIAL_EFO_FORMAT_CONTROL
	psState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_EFO] = FORMAT_CONTROL_FLAG_ON;
	#else /* EURASIA_USE_MOE_INITIAL_EFO_FORMAT_CONTROL */
	psState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_EFO] = FORMAT_CONTROL_FLAG_OFF;
	#endif /* EURASIA_USE_MOE_INITIAL_EFO_FORMAT_CONTROL */

	/*
		Set the initial value of the format control for U8/C10
		instructions.
	*/
	#if EURASIA_USE_MOE_INITIAL_COLOUR_FORMAT_CONTROL
	psState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_COLOUR] = FORMAT_CONTROL_FLAG_ON;
	#else /* EURASIA_USE_MOE_INITIAL_COLOUR_FORMAT_CONTROL */
	psState->aeFlag[FORMAT_CONTROL_FLAG_TYPE_COLOUR] = FORMAT_CONTROL_FLAG_OFF;
	#endif /* EURASIA_USE_MOE_INITIAL_COLOUR_FORMAT_CONTROL */
}

static
IMG_VOID SETFC_Merge(IMG_PVOID	pvDest,
					 IMG_PVOID	pvSrc)
/*****************************************************************************
 FUNCTION	: SETFC_Merge

 PURPOSE	: Merge two possible MOE states together.

 PARAMETERS	: pvDest			- On entry: the first state to merge.
								  On exit: the merged states.
			  pvSrc				- The second state to merge.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSETFC_STATE	psDest = (PSETFC_STATE)pvDest;
	PSETFC_STATE	psSrc = (PSETFC_STATE)pvSrc;
	IMG_UINT32		uArg;

	/*
		Set the format control flags to invalid if
		their value in the two states differs.
	*/
	for (uArg = 0; uArg < FORMAT_CONTROL_FLAG_TYPE_COUNT; uArg++)
	{
		if (psDest->aeFlag[uArg] != psSrc->aeFlag[uArg])
		{
			psDest->aeFlag[uArg] = FORMAT_CONTROL_FLAG_UNDEFINED;
		}
	}
}

static
IMG_VOID SETFC_ProcessFuncEnd(PINTERMEDIATE_STATE	psState,
							  PFUNC					psFunc,
							  PCALC_STATE			psCalcState,
							  IMG_PVOID				pvAll)
/*****************************************************************************
 FUNCTION	: SETFC_ProcessFuncEnd

 PURPOSE	: Do any work needed after inserting SETFC instructions in a function.

 PARAMETERS	: psState			- Compiler state.
			  psFunc			- Function that was just processed.
			  pvAll				- MOE state after each block in the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSETFC_STATE	asAll = (PSETFC_STATE)pvAll;
	PCODEBLOCK		psResetBlock;
	PSETFC_STATE	psEndState;
	PSETFC_STATE	psDefaultState;
	IMG_BOOL		bResetSetfcState;
	IMG_UINT32		uArg;

	if (psFunc == psState->psMainProg || psFunc == psState->psSecAttrProg)
	{ 
		/*
			Code executing after the main function or the secondary update
			program won't assume anything about the MOE format control state.
		*/
		return;
	}
	else
	{
		/*
			Reset the MOE state at the function exit.
		*/
		psResetBlock = psFunc->sCfg.psExit;
	}

	/*
		Get the MOE state at the end of the function.
	*/
	psEndState = &asAll[psResetBlock->uIdx];

	/*
		Check if the MOE differs from the default.
	*/
	bResetSetfcState = IMG_FALSE;
	psDefaultState = (PSETFC_STATE)psCalcState->pvPostFuncState;
	for (uArg = 0; uArg < FORMAT_CONTROL_FLAG_TYPE_COUNT; uArg++)
	{
		if (psEndState->aeFlag[uArg] != psDefaultState->aeFlag[uArg])
		{
			bResetSetfcState = IMG_TRUE;
		}
	}

	/*
		If the MOE state differs from the default then insert an instruction at the
		end of the block to reset it.
	*/
	if (bResetSetfcState)
	{
		InsertSetfcInstruction(psState, 
							   psResetBlock, 
							   NULL, 
							   psDefaultState);
	}
}

static
IMG_VOID InsertSetfc(PINTERMEDIATE_STATE	psState,
					 PGROUPINST_STATE		psGIState)
/*****************************************************************************
 FUNCTION	: InsertSetfc

 PURPOSE	: Insert SETFC instructions where required.

 PARAMETERS	: psState			- Compiler state.
			  psGIState			- Module state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	CALC_STATE	sCalcState;
	IMG_BOOL	bInsertInitialSETFC = IMG_FALSE;

	/*
		Check what states of the format control are required by the program.
	*/
	FMT_CTRL_EVER sFmtCtrl = { IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE };
	SETFC_STATE sInitialState;
	DoOnAllBasicBlocks(psState, ANY_ORDER, SearchFormatControlBP, IMG_FALSE, &sFmtCtrl);

	/*
		Get the initial state of the two format control bits set up by the hardware
		at the start of the program.
	*/
	SETFC_SetDefault(&sInitialState);

	/*
		Check if only one setting of the EFO format control is used
		in the program.
	*/
	if (
			!(sFmtCtrl.bEfoFmtCtrlEverOn && sFmtCtrl.bEfoFmtCtrlEverOff) &&
			(sFmtCtrl.bEfoFmtCtrlEverOn || sFmtCtrl.bEfoFmtCtrlEverOff) &&
			!sFmtCtrl.bFormatChangeAfterFeedback
	   )
	{
		FORMAT_CONTROL_FLAG	eEfoFmtCtrlSetting;

		if (sFmtCtrl.bEfoFmtCtrlEverOn)
		{
			eEfoFmtCtrlSetting = FORMAT_CONTROL_FLAG_ON;
		}
		else
		{
			eEfoFmtCtrlSetting = FORMAT_CONTROL_FLAG_OFF;
		}

		/*
			If the required setting doesn't match the default set by the
			hardware then insert an initial SETFC instruction.
		*/
		if (sInitialState.aeFlag[FORMAT_CONTROL_FLAG_TYPE_EFO] != eEfoFmtCtrlSetting)
		{
			bInsertInitialSETFC = IMG_TRUE;
			sInitialState.aeFlag[FORMAT_CONTROL_FLAG_TYPE_EFO] = eEfoFmtCtrlSetting;

		}
	}

	/*
		Check if only one setting of the colour format control is used
		in the program.
	*/
	if (
			!(sFmtCtrl.bColFmtCtrlEverOn && sFmtCtrl.bColFmtCtrlEverOff) &&
			(sFmtCtrl.bColFmtCtrlEverOn || sFmtCtrl.bColFmtCtrlEverOff) &&
			!sFmtCtrl.bFormatChangeAfterFeedback
	   )
	{
		FORMAT_CONTROL_FLAG	eColFmtCtrlSetting;

		if (sFmtCtrl.bColFmtCtrlEverOn)
		{
			eColFmtCtrlSetting = FORMAT_CONTROL_FLAG_ON;
		}
		else
		{
			eColFmtCtrlSetting = FORMAT_CONTROL_FLAG_OFF;
		}

		/*
			If the required setting doesn't match the default set by the
			hardware then insert an initial SETFC instruction.
		*/
		if (sInitialState.aeFlag[FORMAT_CONTROL_FLAG_TYPE_COLOUR] != eColFmtCtrlSetting)
		{
			bInsertInitialSETFC = IMG_TRUE;
			sInitialState.aeFlag[FORMAT_CONTROL_FLAG_TYPE_COLOUR] = eColFmtCtrlSetting;
		}
	}

	/*
		Insert SETFC instructions.
	*/
	sCalcState.psGIState = psGIState;

	sCalcState.uStateSize = sizeof(SETFC_STATE);
	sCalcState.pvInitial = NULL;
	sCalcState.pvPostFuncState = &sInitialState;

	sCalcState.pfProcessBasicBlock = CheckFormatControlBasicBlock;
	sCalcState.pfSetDefault = SETFC_SetDefault;
	sCalcState.pfMerge = SETFC_Merge;
	sCalcState.pfProcessFuncEnd = SETFC_ProcessFuncEnd;

	DoMOEPass(psState, psGIState, &sCalcState, &sInitialState);

	/* Insert an instruction at the program entrypoint to establish the initial format control state. */
	if (bInsertInitialSETFC)
	{
		InsertSetfcInstruction(psState, 
							   psState->psMainProg->sCfg.psEntry, 
							   psState->psMainProg->sCfg.psEntry->psBody,
							   &sInitialState);
	}
}

IMG_INTERNAL
IMG_VOID GroupInstructionsProgram(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: GroupInstructionsProgram

 PURPOSE	: Generate repeating instructions for a program.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PGROUPINST_STATE psGIState = NULL;

	/* Allocate the groupinst state. */
	InitGroupInstState(psState, &psGIState);

	DoOnAllBasicBlocks(psState, ANY_ORDER, GenerateDotProdBP, IMG_FALSE, NULL);
	TESTONLY_PRINT_INTERMEDIATE("Generate DP/DDP", "gen_ddp", IMG_FALSE);

	/*
		Flag that the interpretation of register number changes once the
		MOE state is applied.
	*/
	psState->uFlags |= USC_FLAGS_MOESTAGE;

	/*
		Copy ARG.uNumber to ARG.uNumberPreMoe for each instruction source and
		destination. After this point ARG.uNumber is the register to encode
		directly in the instruction, which will be equal to ARG.uNumberPreMoe after
		the hardware applies the MOE state.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, SaveRegNumsBP, IMG_FALSE/*CALLs*/, NULL);

	/*
		Change register numbers to be in the same units as the MOE increments/base offset/etc.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, AdjustToMOEUnitsBP, IMG_FALSE, NULL);

	/*
		Generate repeated instructions.
	*/
	InsertSmlsi(psState, psGIState);
	TESTONLY_PRINT_INTERMEDIATE("Generate Repeats", "generate_repeats", IMG_FALSE);

	/*
		Insert SMBO instructions where the register numbers used in instructions
		are larger than that directly supported by the hardware.
	*/
	InsertSmbo(psState, psGIState);
	TESTONLY_PRINT_INTERMEDIATE("Insert SMBO", "insert_smbo", IMG_FALSE);

	/*
		Insert SETFC instructions where instructions require them.
	*/
	InsertSetfc(psState, psGIState);
	TESTONLY_PRINT_INTERMEDIATE("Insert SETFC", "insert_setfc", IMG_FALSE);

	/* Free the allocated memory */
	FreeGroupInstState(psState, psGIState);
}
