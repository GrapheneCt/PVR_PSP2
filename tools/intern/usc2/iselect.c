/******************************************************************************
 * Name         : iselect.c
 * Title        : Miscellaneous Optimizations inc Global Move Elimination
 * Created      : Sept 2005
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
 * $Log: iselect.c $
 * ,
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"
#include "usedef.h"
#include <limits.h>

#include "usetab.h"

#include "sgxformatconvert.h"

/* Local declarations. */
static IMG_BOOL NegateTest(TEST_TYPE eOldTestType, PTEST_TYPE peNewTestType);
static
IMG_VOID CombineF32PacksBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PDGRAPH_STATE psDepState);
static IMG_BOOL HasSourceModifier(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx);
static IMG_VOID AppendToEvalList(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psEvalList, PINST psInst);
static IMG_VOID ReplaceFMOV(PINTERMEDIATE_STATE psState, PINST psInst);
static IMG_VOID ApplyArithmeticSimplificationsToList(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList);
static IMG_VOID WeakInstListAppendInstsOfType(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList, IOPCODE eOpcode);

static IMG_BOOL FindMSAPart(PINTERMEDIATE_STATE psState, PINST psInst, PINST psNextInst, IMG_BOOL bFirstIsSqr)
/*****************************************************************************
 FUNCTION	: FindMSAPart

 PURPOSE	: Find an instruction suitable to be combined into an MSA.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- First instruction.
			  psNextInst	- Second instruction.
			  bFirstSqr		- Does the first instruction do the squaring.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			psNextInst->eOpcode == IFMAD &&
			UseDefIsSingleSourceUse(psState, psNextInst, 2 /* uSrcIdx */, &psInst->asDest[0]) &&
			!IsAbsolute(psState, psNextInst, 2 /* uArgIdx */) &&
			EqualPredicates(psInst, psNextInst)
		)
	{
		if (bFirstIsSqr)
		{
			if (!IsNegated(psState, psNextInst, 2 /* uArgIdx */))
			{
				return IMG_TRUE;
			}
		}
		else
		{
			if (EqualFloatSrcs(psState, psNextInst, 0, psNextInst, 1))
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL FindMulX2(PINTERMEDIATE_STATE psState, PINST psInst, PINST psNextInst, IMG_PBOOL pbNegResult, IMG_PBOOL pbNeg2)
/*****************************************************************************
 FUNCTION	: FindMulX2

 PURPOSE	: Check for an instruction which multiplies the result of another
			  instruction by two.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- First instruction.
			  psNextInst	- Second instruction.
			  pbNegResult	- Returns TRUE if the result is negated.
			  pbNeg2		- Returns TRUE if the instruction multiplies by -2.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			psNextInst->eOpcode == IFMUL && 
			EqualPredicates(psInst, psNextInst)
	   )
	{
		IMG_UINT32	uSrc;

		uSrc = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);

		if (uSrc != USC_UNDEF &&
			psNextInst->asArg[1 - uSrc].uType == USEASM_REGTYPE_FPCONSTANT &&
			psNextInst->asArg[1 - uSrc].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT2)
		{
			*pbNegResult = IsNegated(psState, psNextInst, uSrc);
			*pbNeg2 = IsNegated(psState, psNextInst, 1 - uSrc);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL FindAdd(PINTERMEDIATE_STATE psState, PINST psInst, PINST psNextInst, IMG_PUINT32 puArg)
/*****************************************************************************
 FUNCTION	: FindAdd

 PURPOSE	: Check for an ADD which uses the result of a previous instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- First instruction.
			  psNextInst	- Second instruction.
			  puArg			- Returns the number of the other source.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			psNextInst->eOpcode == IFADD && 
			EqualPredicates(psNextInst, psInst)
	   )
	{
		IMG_UINT32	uSrc;

		uSrc = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);

		if (uSrc != USC_UNDEF && !psNextInst->u.psFloat->asSrcMod[uSrc].bAbsolute)
		{
			*puArg = 1 - uSrc;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsModifiableRegister(PARG	psArg)
/*****************************************************************************
 FUNCTION	: IsModifiableRegister

 PURPOSE	: Checks if a register is one whose liveness is tracked.

 PARAMETERS	: psArg		- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psArg->uType == USEASM_REGTYPE_TEMP ||
		    psArg->uType == USEASM_REGTYPE_OUTPUT ||
		    psArg->uType == USEASM_REGTYPE_PRIMATTR) ? IMG_TRUE : IMG_FALSE;
}

static IMG_BOOL TermsCancelOut(PINTERMEDIATE_STATE psState,
							   PINST psInst, 
							   PINST psNextInst, 
							   IMG_PUINT32 puRemainingTerm, 
							   IMG_PBOOL pbNegateRemainingTerm,
							   IMG_PBOOL pbDroppedTemporaries)
/*****************************************************************************
 FUNCTION	: TermsCancelOut

 PURPOSE	: Check for a case whether all but one of the terms in a sequence of
			  adds cancel out.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- Instruction sequence.
			  puRemainingTerm			- Number of the not canceled term
			  pbNegateRemainingTerm		- Sign of the not canceled term.
			  pbDroppedTemporaries		- Set to TRUE if the cancelled argument
										was a register whose liveness is
										tracked.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArg, uOtherArg, uFirstArg = 0;
	IMG_BOOL	abMatchOther[2] = {IMG_FALSE, IMG_FALSE};
	IMG_BOOL	bFirstArgNegate;
	PARG		psOtherSrc;

	/*
		Check that one of the arguments to the next instruction is
		the same as the destination of the first.
	*/
	uArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uArg == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	if (IsAbsolute(psState, psNextInst, uArg))
	{
		return IMG_FALSE;
	}
	uOtherArg = 1 - uArg;

	/*
		Check that the two instructions share a source.
	*/
	psOtherSrc = &psNextInst->asArg[uOtherArg];
	for (uArg = 0; uArg < 2; uArg++)
	{	
		PARG	psInstSrc = &psInst->asArg[uArg];

		if (psOtherSrc->uType == psInstSrc->uType &&
			psOtherSrc->uNumber == psInstSrc->uNumber &&
			IsAbsolute(psState, psNextInst, uOtherArg) == IsAbsolute(psState, psInst, uArg) &&
			psOtherSrc->uIndexType == USC_REGTYPE_NOINDEX &&
			psInstSrc->uIndexType == USC_REGTYPE_NOINDEX)
		{
			abMatchOther[uArg] = IMG_TRUE;
			uFirstArg = uArg;
		}
	}

	if ((!abMatchOther[0] && !abMatchOther[1]) || (abMatchOther[0] && abMatchOther[1]))
	{
		return IMG_FALSE;
	}

	/*
		Check that the terms cancel out.
	*/
	bFirstArgNegate = IsNegated(psState, psInst, uFirstArg);
	if (IsNegated(psState, psNextInst, 1 - uOtherArg))
	{
		bFirstArgNegate = (!bFirstArgNegate) ? IMG_TRUE : IMG_FALSE;
	}
	if (IsNegated(psState, psNextInst, uOtherArg) == bFirstArgNegate)
	{
		return IMG_FALSE;
	}

	/*
		Return information about the surviving term.
	*/
	*puRemainingTerm = 1 - uFirstArg;
	*pbNegateRemainingTerm = IsNegated(psState, psInst, 1 - uFirstArg);
	if (IsNegated(psState, psNextInst, 1 - uOtherArg))
	{
		*pbNegateRemainingTerm = (!(*pbNegateRemainingTerm)) ? IMG_TRUE : IMG_FALSE;
	}

	/*
		If dropping a reference to a modifable register then redo 
		dead code elimination for the whole program.
	*/
	if (IsModifiableRegister(&psNextInst->asArg[uFirstArg]))
	{
		*pbDroppedTemporaries = IMG_TRUE;
	}

	return IMG_TRUE;
}

static IMG_BOOL DoF32Test(PINTERMEDIATE_STATE	psState,
						  TEST_TYPE				eTest, 
						  IMG_FLOAT				fInput)
/*****************************************************************************
 FUNCTION	: DoF32Test

 PURPOSE	: Apply the test on an F32 test instruction to a floating point value.

 PARAMETERS	: psState	- Internal compiler state
			  eTest		- Test to apply.
			  fInput	- Value to test.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (eTest)
	{
		case TEST_TYPE_GTE_ZERO:	return (fInput >= 0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_GT_ZERO:		return (fInput >  0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_LTE_ZERO:	return (fInput <= 0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_LT_ZERO:		return (fInput <  0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_EQ_ZERO:		return (fInput == 0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_NEQ_ZERO:	return (fInput != 0.0f) ? IMG_TRUE : IMG_FALSE;
		case TEST_TYPE_ALWAYS_TRUE:	return IMG_TRUE;
		default: imgabort();
	}
}

static IMG_BOOL DoBitwiseTest(PINTERMEDIATE_STATE	psState,
							  TEST_TYPE				eTest, 
							  IMG_UINT32			uInput)
/*****************************************************************************
 FUNCTION	: DoBitwiseTest

 PURPOSE	: Apply the test on an bitwise test instruction to a integer value.

 PARAMETERS	: psState	- Internal compiler state
			  eTest		- Test to apply.
			  uInput	- Value to test.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_INT32	iInput = (IMG_INT32)uInput;

	switch (eTest)
	{
		case TEST_TYPE_GTE_ZERO:	
		case TEST_TYPE_SIGN_BIT_CLEAR:
		{
			return (iInput >= 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_GT_ZERO:
		{
			return (iInput > 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_LTE_ZERO:
		{
			return (iInput <= 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_LT_ZERO:		
		case TEST_TYPE_SIGN_BIT_SET:
		{
			return (iInput < 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_EQ_ZERO:	
		{
			return (iInput == 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_NEQ_ZERO:	
		{
			return (iInput != 0) ? IMG_TRUE : IMG_FALSE;
		}
		case TEST_TYPE_ALWAYS_TRUE:	
		{
			return IMG_TRUE;
		}
		default: imgabort();
	}
}

IMG_INTERNAL IMG_UINT32 LogicalNegateTest(PINTERMEDIATE_STATE psState, TEST_TYPE eTest)
/*****************************************************************************
 FUNCTION	: LogicalNegateTest

 PURPOSE	: Get the logical negation of a test type.

 PARAMETERS	: psState	- Internal compiler state
			  eTest		- Test to negate.

 RETURNS	: The logical negation of the sense of the test.
*****************************************************************************/
{
	switch (eTest)
	{
		case TEST_TYPE_EQ_ZERO:		return TEST_TYPE_NEQ_ZERO;
		case TEST_TYPE_NEQ_ZERO:	return TEST_TYPE_EQ_ZERO;
		case TEST_TYPE_GTE_ZERO:	return TEST_TYPE_LT_ZERO;
		case TEST_TYPE_LT_ZERO:		return TEST_TYPE_GTE_ZERO;
		case TEST_TYPE_LTE_ZERO:	return TEST_TYPE_GT_ZERO;
		case TEST_TYPE_GT_ZERO:		return TEST_TYPE_LTE_ZERO;
		default: imgabort();
	}
}

IMG_INTERNAL IMG_VOID LogicalNegateInstTest(PINTERMEDIATE_STATE psState, PINST psDefInst)
/*****************************************************************************
 FUNCTION	: LogicalNegateInstTest

 PURPOSE	: Modify a test instruction to do a test which is the logical negation
			  of the current test.

 PARAMETERS	: psState	- Internal compiler state
			  psDefInst	- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psDefInst->eOpcode == IVTEST)
	{
		psDefInst->u.psVec->sTest.eType = LogicalNegateTest(psState, psDefInst->u.psVec->sTest.eType);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		ASSERT(psDefInst->eOpcode == ITESTPRED);

		psDefInst->u.psTest->sTest.eType = LogicalNegateTest(psState, psDefInst->u.psTest->sTest.eType);
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_FLOAT ImmediateVectorToFloat(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, IMG_UINT32 uChanIdx)
/*****************************************************************************
 FUNCTION	: ImmediateVectorToFloat

 PURPOSE	: Get the value of a channel from a vector of immediates.

 PARAMETERS	: psState	- Internal compiler state
			  psInst	- Instruction.
			  uSrcIdx	- Index of the source argument which is a vector
						of immediates.
			  uChanIdx	- Channel to get.

 RETURNS	: The value of the channel in F32 format.
*****************************************************************************/
{
	IMG_UINT32	uImm;
	IMG_BOOL	bRet;

	bRet = GetImmediateVectorChan(psState, psInst, uSrcIdx, uChanIdx, &uImm);
	ASSERT(bRet);

	if (psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F16)
	{
		return ConvertF16ToF32(uImm);
	}
	else
	{
		ASSERT(psInst->u.psVec->aeSrcFmt[uSrcIdx] == UF_REGFORMAT_F32);
		return *(IMG_PFLOAT)&uImm;
	}
}

static IMG_FLOAT ApplyVectorSourceMod(IMG_FLOAT fValue, PVEC_SOURCE_MODIFIER psSourceModifier)
/*****************************************************************************
 FUNCTION	: ApplyVectorSourceMod

 PURPOSE	: Apply source modifiers to an F32 value.

 PARAMETERS	: fValue		- Value to apply source modifiers to.
			  psSourceModifiers
							- Source modifiers to apply.

 RETURNS	: The value after source modifiers have been applied.
*****************************************************************************/
{
	if (psSourceModifier->bAbs && fValue < 0.0f)
	{
		fValue = -fValue;
	}
	if (psSourceModifier->bNegate)
	{
		fValue = -fValue;
	}
	return fValue;
}

static IMG_BOOL EvaluateVTEST(PINTERMEDIATE_STATE	psState, 
							  PINST					psTestInst, 
							  IMG_UINT32			uTestSrc,
							  IMG_FLOAT				aafValues[][VECTOR_LENGTH],
							  IMG_PBOOL				pbTestResult)
/*****************************************************************************
 FUNCTION	: EvaluateVTEST

 PURPOSE	: Check for a vector TEST instruction which produces (per-channel)
			  either always true if one of the sources has a particular value
			  and always false if it has another.

 PARAMETERS	: psState				- Internal compiler state
			  psTestInst			- Vector TEST instruction.
			  uTestSrc				- Test source which has two possible values.
			  aafValues				- Per-channel the two possible values for the
									test source.
			  pbTestResult			- Returns TRUE if the test result is TRUE
									when the source has the first value and
									FALSE when it has the second value. Returns
									FALSE if the relationship is the inverse.

 RETURNS	: TRUE if the result of the instruction is determined by the value
			  of the specified source.
*****************************************************************************/
{
	IMG_BOOL	bFirstChan;
	IMG_BOOL	bTestResult;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uTestChansUsed;

	/*
		Check the instrution is an unpredicated TEST.
	*/
	if (
			!(
				psTestInst->eOpcode == IVTEST && 
				NoPredicate(psState, psTestInst) &&
				psTestInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT &&
				psTestInst->u.psVec->sTest.eTestOpcode == IVADD
			  )
	   )
	{
		return IMG_FALSE;
	}

	/*
		The other source to the TEST instruction must have a known value.
	*/
	if (!IsImmediateVectorSource(psState, psTestInst, 1 - uTestSrc))
	{
		return IMG_FALSE;
	}

	/*
		Get the mask of channels used from the ALU result by the test channel selector.
	*/
	uTestChansUsed = GetVTestChanSelMask(psState, psTestInst);

	bFirstChan = IMG_TRUE;
	bTestResult = USC_UNDEF;
	for (uChanIdx = 0; uChanIdx < VECTOR_LENGTH; uChanIdx++)
	{
		IMG_UINT32	uSrc;
		IMG_BOOL	abTestResult[2];

		/*
			Skip if this channel of the ALU result isn't used.
		*/
		if ((uTestChansUsed & (1U << uChanIdx)) == 0)
		{
			continue;
		}
		
		/*
			Get the result written to the predicate destination of the TEST instruction depending on
			the two possible values of the source channels.
		*/
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			IMG_FLOAT	afArgValue[2];
			IMG_FLOAT	fTestResult;
			IMG_UINT32	uArg;

			for (uArg = 0; uArg < 2; uArg++)
			{
				IMG_FLOAT	fValue;
				IMG_UINT32	uSrcChan = GetChan(psTestInst->u.psVec->auSwizzle[uArg], uChanIdx);

				if (uSrcChan >= USEASM_SWIZZLE_SEL_0 && uSrcChan <= USEASM_SWIZZLE_SEL_HALF)
				{
					return IMG_FALSE;
				}
				else 
				{
					ASSERT(uSrcChan >= USEASM_SWIZZLE_SEL_X && uSrcChan <= USEASM_SWIZZLE_SEL_W);
					if (uTestSrc == uArg)
					{
						/*
							This channel has two possible values depending on the result of the
							test on the proceeding instruction.
						*/
						fValue = aafValues[uSrc][uSrcChan];
					}
					else
					{
						/*
							Get the value of this channel from the TEST's source.
						*/
						fValue = ImmediateVectorToFloat(psState, psTestInst, uArg, uSrcChan);
					}
				}

				/*
					Apply any source modifier on the TEST's source.
				*/
				afArgValue[uArg] = ApplyVectorSourceMod(fValue, &psTestInst->u.psVec->asSrcMod[uArg]);
			}

			/*
				Get the result of the ALU instruction.
			*/
			switch (psTestInst->u.psVec->sTest.eTestOpcode)
			{
				case IVADD: fTestResult = afArgValue[0] + afArgValue[1]; break;
				default: imgabort();
			}

			/*
				Get the predicate result.
			*/
			abTestResult[uSrc] = DoF32Test(psState, psTestInst->u.psVec->sTest.eType, fTestResult);
		}

		/*
			Fail if the predicate result is constant.
		*/
		if (abTestResult[0] == abTestResult[1])
		{
			return IMG_FALSE;
		}
		/*
			Fail if the relation between the test on the MOVC instruction's source 0 and the
			result written to the TEST's predicate destination differs for different channels.
		*/
		if (!bFirstChan && bTestResult != abTestResult[0])
		{
			return IMG_FALSE;
		}
		bTestResult = abTestResult[0];
	}
	
	*pbTestResult = bTestResult;
	return IMG_TRUE;
}

static IMG_BOOL CombineVMOVC_VTESTNoPredicate(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: CombineVMOVC_VTESTNoPredicate

 PURPOSE	: Try to combine a VMOVC instruction whether both choices are constant
			  with a test on the result.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions to combine

 RETURNS	: TRUE if the instructions were combined; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uTestSrcFromMovcResult;
	IMG_UINT32	uTestSrcFromMovcResultArg;
	IMG_BOOL	bTestResult;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uUsedChans;
	IMG_FLOAT	aafValues[2][VECTOR_LENGTH];
	IMG_UINT32	uDest;
	TEST_TYPE	eMOVCTestType;

	ASSERT(psInst->eOpcode == IVMOVC);
	/*
		Check for:
			VMOVC	DEST, SRC0, IMMX, IMMY	(DEST = TEST(SRC0) ? IMMX : IMMY)
			VTEST	PDEST, DEST, IMMZ		(PDEST = TEST(DEST + IMMZ))
		->
			VTEST	PDEST, SRC0, #0			(PDEST = TEST(SRC0))
	*/

	/*
		Check the first instruction is an unpredicated conditional MOV.
	*/ 
	ASSERT(NoPredicate(psState, psInst));

	/*
		Check the two possible results of the VMOVC are vectors of immediate values.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (!IsImmediateVectorSource(psState, psInst, 1 + uArg))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check the result of the VMOVC is used in one of the TEST instruction's source and this
		is the only use.
	*/
	uTestSrcFromMovcResultArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uTestSrcFromMovcResultArg == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	ASSERT((uTestSrcFromMovcResultArg % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uTestSrcFromMovcResult = uTestSrcFromMovcResultArg / SOURCE_ARGUMENTS_PER_VECTOR;

	/*
		Check the all the channels referenced from this source are written
		by the MOVC.
	*/
	uUsedChans = GetLiveChansInArg(psState, psNextInst, uTestSrcFromMovcResultArg);
	if ((uUsedChans & ~psInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Get the two possible results of the MOVC for each channel.
	*/
	for (uChanIdx = 0; uChanIdx < VECTOR_LENGTH; uChanIdx++)
	{
		if ((psInst->auLiveChansInDest[0] & (1 << uChanIdx)) != 0)
		{
			IMG_UINT32	uMovcSrc;

			for (uMovcSrc = 0; uMovcSrc < 2; uMovcSrc++)
			{
				IMG_FLOAT	fValue;

				/*
					Get the value of this channel from the VMOVC's source.
				*/
				fValue = ImmediateVectorToFloat(psState, psInst, 1 + uMovcSrc, uChanIdx);	

				/*
					Apply any source modifier on the VMOVC's source.
				*/
				fValue = ApplyVectorSourceMod(fValue, &psInst->u.psVec->asSrcMod[1 + uMovcSrc]);

				aafValues[uMovcSrc][uChanIdx] = fValue;
			}
		}
	}

	/*
		Check the TEST instruction will produce (per-channel) either the same result as the
		MOVC test on source 0 or it's logical inverse.
	*/
	if (!EvaluateVTEST(psState, psNextInst, uTestSrcFromMovcResult, aafValues, &bTestResult))
	{
		return IMG_FALSE;
	}
	
	/*
		Convert the MOVC instruction to a TEST instruction.
	*/
	eMOVCTestType = psInst->u.psVec->eMOVCTestType;
	ModifyOpcode(psState, psInst, IVTEST);
	SetDestCount(psState, psInst, VTEST_PREDICATE_ONLY_DEST_COUNT);
	psInst->u.psVec->sTest.eTestOpcode = IVADD;
	psInst->u.psVec->sTest.eType = eMOVCTestType;
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NONE;

	/*
		Copy the predicate destination from the old TEST instruction to the MOVC instruction.
	*/
	ASSERT(psNextInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT);
	MoveDest(psState, psInst, VTEST_PREDICATE_DESTIDX, psNextInst, VTEST_PREDICATE_DESTIDX);

	for (uDest = VTEST_PREDICATE_DESTIDX+1; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		SetDestUnused(psState, psInst, uDest);
	}

	CopyPartiallyWrittenDest(psState, psInst, VTEST_PREDICATE_DESTIDX, psNextInst, VTEST_PREDICATE_DESTIDX);
	psInst->auDestMask[VTEST_PREDICATE_DESTIDX] = psNextInst->auDestMask[VTEST_PREDICATE_DESTIDX];
	psInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX] = psNextInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX];

	/*
		Copy the channel selector from the old TEST instruction to the MOVC instruction.
	*/
	psInst->u.psVec->sTest.eChanSel = psNextInst->u.psVec->sTest.eChanSel;

	/*
		Take the logical inversion of the MOVC test.
	*/
	if (!bTestResult)
	{
		psInst->u.psVec->sTest.eType = LogicalNegateTest(psState, psInst->u.psVec->sTest.eType);
	}

	/*
		Combine the swizzle on the VMOVC instruction's source 0 with the swizzle on the VMOVC instruction's
		result in the TEST instruction.
	*/
	psInst->u.psVec->auSwizzle[0] = 
		CombineSwizzles(psInst->u.psVec->auSwizzle[0], psNextInst->u.psVec->auSwizzle[uTestSrcFromMovcResult]);

	/*
		Make the first source to the new TEST the same as source 0 to the MOVC instruction
		and set the second source to immediate 0.0. 
	*/
	SetupImmediateSource(psInst, 1 /* uArgIdx */, 0.0f, psInst->u.psVec->aeSrcFmt[0]);

	return IMG_TRUE;
}

static IMG_BOOL CombineVTESTMASK_VTEST_NoPredicate(PINTERMEDIATE_STATE	psState,
													  PINST					psInst,
													  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineVTESTMASK_VTEST_NoPredicate

 PURPOSE	: Try to modify a VMOVCBIT instruction to VMOVC since the hardware
			  doesn't support the former instruction directly.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions to combine.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_FLOAT			aafValue[2][VECTOR_LENGTH];
	IMG_UINT32			uTestSrcFromMaskResultArg;
	IMG_UINT32			uTestSrcFromMaskResult;
	USEASM_TEST_MASK	eMaskType;
	IMG_UINT32			uSrc;
	IMG_UINT32			uUsedChans;
	IMG_BOOL			bTestResult;
	IMG_UINT32			uDest;

	/*
		Check the first instruction is an unpredicated VTESTMASK.
	*/ 
	ASSERT(psInst->eOpcode == IVTESTMASK);
	ASSERT(NoPredicate(psState, psInst));

	/*
		Check the second instruction is a trival TEST.
	*/
	if (psNextInst->eOpcode != IVTEST)
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Check the first instruction produces (per-channel) either 1.0f if the test on that
		channel is true or 0.0f if it is false.
	*/
	eMaskType = psInst->u.psVec->sTest.eMaskType;
	if (eMaskType != USEASM_TEST_MASK_NUM)
	{
		return IMG_FALSE;
	}

	/*
		Make arrays of the possible results of the TESTMASK for each channel.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		IMG_FLOAT	fValue = (uSrc == 0) ? 1.0f : 0.0f;
		IMG_UINT32	uChan;

		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			aafValue[uSrc][uChan] = fValue;
		}
	}

	/*
		Check exactly one of the sources to the TEST instruction is the result of the TESTMASK
		instruction and that is the only use.
	*/
	uTestSrcFromMaskResultArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uTestSrcFromMaskResultArg == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	ASSERT((uTestSrcFromMaskResultArg % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uTestSrcFromMaskResult = uTestSrcFromMaskResultArg / SOURCE_ARGUMENTS_PER_VECTOR;

	/*
		Check the all the channels referenced from this source are written
		by the MOVC.
	*/
	uUsedChans = GetLiveChansInArg(psState, psNextInst, uTestSrcFromMaskResultArg);
	if ((uUsedChans & ~psInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the TEST instruction will produce (per-channel) either the same result as the
		TESTMASK or it's logical inverse.
	*/
	if (!EvaluateVTEST(psState, psNextInst, uTestSrcFromMaskResult, aafValue, &bTestResult))
	{
		return IMG_FALSE;
	}

	/*
		Convert the TESTMASK instruction to a TEST instruction.
	*/
	ModifyOpcode(psState, psInst, IVTEST);
	SetDestCount(psState, psInst, VTEST_PREDICATE_ONLY_DEST_COUNT);

	/*
		Copy the predicate destination from the old TEST instruction to the TESTMASK instruction.
	*/
	ASSERT(psNextInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT);
	MoveDest(psState, psInst, VTEST_PREDICATE_DESTIDX, psNextInst, VTEST_PREDICATE_DESTIDX);

	for (uDest = VTEST_PREDICATE_DESTIDX+1; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
	{
		SetDestUnused(psState, psInst, uDest);
	}

	CopyPartiallyWrittenDest(psState, psInst, VTEST_PREDICATE_DESTIDX, psNextInst, VTEST_PREDICATE_DESTIDX);
	psInst->auDestMask[VTEST_PREDICATE_DESTIDX] = psNextInst->auDestMask[VTEST_PREDICATE_DESTIDX];
	psInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX] = psNextInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX];

	/*
		Copy the channel selector from the old TEST instruction to the TESTMASK instruction.
	*/
	psInst->u.psVec->sTest.eChanSel = psNextInst->u.psVec->sTest.eChanSel;

	/*
		Clear the test mask setting.
	*/
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NONE;

	/*
		Take the logical inversion of the TESTMASK test.
	*/
	if (!bTestResult)
	{
		psInst->u.psVec->sTest.eType = LogicalNegateTest(psState, psInst->u.psVec->sTest.eType);
	}

	/*
		Combine the swizzle on the TESTMASK instruction's sources with the swizzle on the TESTMASK instruction's
		result in the TEST instruction.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		psInst->u.psVec->auSwizzle[uSrc] = 
			CombineSwizzles(psInst->u.psVec->auSwizzle[uSrc], psNextInst->u.psVec->auSwizzle[uTestSrcFromMaskResult]);
	}

	return IMG_TRUE;
}

static IMG_BOOL IsConstArg(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   IMG_UINT32			uVecSrc,
						   IMG_UINT32			uConst)
/*****************************************************************************
 FUNCTION	: IsConstArg

 PURPOSE	: Check whether an instruction's source is a given constant.

 PARAMETERS	: psState	- Internal compiler state
			  psInst	- The two instructions.
			  uSrc		- The source of the instruction to check
			  uConst	- The constant to check for.

 RETURNS	: TRUE if the argument is, directly or indirectly, the constant.
*****************************************************************************/
{
	PINST psDefInst;
	PARG pVecArg;

	if (IsReplicatedImmediateVector(psState, psInst, uVecSrc, uConst))
	{
		return IMG_TRUE;
	}
	
	pVecArg = &psInst->asArg[uVecSrc*SOURCE_ARGUMENTS_PER_VECTOR];

	if (pVecArg->uType != USC_REGTYPE_UNUSEDSOURCE)
	{
		psDefInst = UseDefGetDefInst(psState, pVecArg->uType, pVecArg->uNumber, NULL /* puDestIdx */);
		
		if (psDefInst == NULL || psDefInst->eOpcode != IVMOV)
		{
			return IMG_FALSE;
		}

		if (!IsConstArg(psState, psDefInst, 0, uConst))
		{
			return IMG_FALSE;
		}

		if (psDefInst->apsOldDest[0] != NULL)
		{
			psDefInst = UseDefGetDefInst(psState, psDefInst->apsOldDest[0]->uType, psDefInst->apsOldDest[0]->uNumber, NULL /* puDestIdx */);

			if (psDefInst == NULL || psDefInst->eOpcode != IVMOV)
			{
				return IMG_FALSE;
			}

			if (!IsConstArg(psState, psDefInst, 0, uConst))
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uChansUsed;

		GetLiveChansInSourceSlot(psState, psInst, uVecSrc, &uChansUsed, NULL /* puChansUsedPostSwizzle */);

		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if ((uChansUsed & (1 << uChan)) != 0)
			{
				PARG pArg = &psInst->asArg[uVecSrc*SOURCE_ARGUMENTS_PER_VECTOR + uChan];

				psDefInst = UseDefGetDefInst(psState, pArg->uType, pArg->uNumber, NULL /* puDestIdx */);

				if (psDefInst == NULL || psDefInst->eOpcode != IVMOV)
				{
					return IMG_FALSE;
				}

				if (!IsConstArg(psState, psDefInst, 0, uConst))
				{
					return IMG_FALSE;
				}

				if (psDefInst->apsOldDest[0] != NULL)
				{
					psDefInst = UseDefGetDefInst(psState, psDefInst->apsOldDest[0]->uType, psDefInst->apsOldDest[0]->uNumber, NULL /* puDestIdx */);

					if (psDefInst == NULL || psDefInst->eOpcode != IVMOV)
					{
						return IMG_FALSE;
					}

					if (!IsConstArg(psState, psDefInst, 0, uConst))
					{
						return IMG_FALSE;
					}
				}
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CombineVTESTMASK_VMOVC_NoPredicate(PINTERMEDIATE_STATE	psState,
												   PINST				psInst,
												   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: CombineVTESTMASK_VMOVC_NoPredicate

 PURPOSE	: 

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	USEASM_TEST_MASK	eMaskType;
	IMG_UINT32			uSrc, uZeroVal, uOneVal;
	IMG_BOOL			bInvertTest;
	IMG_UINT32			uMOVCSrc;

	/*
		Check the first instruction is an unpredicated VTESTMASK.
	*/ 
	ASSERT(psInst->eOpcode == IVTESTMASK || psInst->eOpcode == IVTESTBYTEMASK);
	ASSERT(NoPredicate(psState, psInst));

	/*
		Check the second instruction is a conditional move.
	*/
	if (psInst->eOpcode == IVTESTMASK)
	{
		if (psNextInst->eOpcode != IVMOVC)
		{
			return IMG_FALSE;
		}

		/*
			Check the first instruction produces (per-channel) either 1.0f if the test on that
			channel is true or 0.0f if it is false.
		*/
		eMaskType = psInst->u.psVec->sTest.eMaskType;
		if (eMaskType != USEASM_TEST_MASK_NUM)
		{
			return IMG_FALSE;
		}

		uMOVCSrc = 0;
	}
	else
	{
		ASSERT(psInst->eOpcode == IVTESTBYTEMASK);
		if (psNextInst->eOpcode != IVMOVCU8_FLT)
		{
			return IMG_FALSE;
		}
		ASSERT(psInst->u.psVec->sTest.eMaskType == USEASM_TEST_MASK_BYTE);

		uMOVCSrc = 1;
	}
	if (!(psNextInst->u.psVec->eMOVCTestType == TEST_TYPE_EQ_ZERO || psNextInst->u.psVec->eMOVCTestType == TEST_TYPE_NEQ_ZERO))
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Check the result of the first instruction is only used as source 0 of the second instruction.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, uMOVCSrc, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}

	/*
		Check there are no source modifiers on source 0 of the second instruction.
	*/
	if (psNextInst->u.psVec->asSrcMod[0].bNegate || psNextInst->u.psVec->asSrcMod[0].bAbs)
	{
		return IMG_FALSE;
	}

	/*
		Check no constant values are swizzled into source 0.
	*/
	if (!IsNonConstantSwizzle(psNextInst->u.psVec->auSwizzle[0], psNextInst->auDestMask[0], NULL /* puMatchedSwizzle */))
	{
		return IMG_FALSE;
	}

	if (psInst->asDest[0].eFmt == UF_REGFORMAT_F16)
	{
		uZeroVal = FLOAT16_ZERO;
		uOneVal = FLOAT16_ONE;
	}
	else
	{
		uZeroVal = FLOAT32_ZERO;
		uOneVal = FLOAT32_ONE;
	}
	
	bInvertTest = (psNextInst->u.psVec->eMOVCTestType == TEST_TYPE_EQ_ZERO);
	/*
		Check the second argument to the MOVC is immediate 1.0f
		and the third argument to the MOVC is immediate 0.0f.
	*/
	if (IsConstArg(psState, psNextInst, 1 /* uSrcIdx */, uZeroVal) &&
		IsConstArg(psState, psNextInst, 2 /* uSrcIdx */, uOneVal) &&
		!psNextInst->u.psVec->asSrcMod[2].bNegate)
	{
		bInvertTest = !bInvertTest;
	}
	else if (!IsConstArg(psState, psNextInst, 1 /* uSrcIdx */, uOneVal) ||
			 !IsConstArg(psState, psNextInst, 2 /* uSrcIdx */, uZeroVal) ||
			 psNextInst->u.psVec->asSrcMod[1].bNegate)
	{
		return IMG_FALSE;
	}

	if (psInst->eOpcode == IVTESTBYTEMASK)
	{
		ModifyOpcode(psState, psInst, IVTESTMASK);
		SetDestCount(psState, psInst, 1 /* uNewDestCount */);
		psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;
	}

	/*
		Move the second instruction's destination to the first.
	*/
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	CopyPartiallyWrittenDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	psInst->auDestMask[0] = psNextInst->auDestMask[0];
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	/*
		Convert
			TESTMASK	T = test(op(X, Y)) ? 1 : 0
			MOVC		D = (T == 0) ? 0 : 1
		->
			TESTMASK	D = not(test(op(X, Y)) ? 1 : 0
	*/
	if (bInvertTest)
	{
		psInst->u.psVec->sTest.eType = LogicalNegateTest(psState, psInst->u.psVec->sTest.eType);
	}

	/*
		Combine the swizzle on the TESTMASK instruction's sources with the swizzle on the TESTMASK instruction's
		result in the MOVC instruction.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		psInst->u.psVec->auSwizzle[uSrc] = 
			CombineSwizzles(psInst->u.psVec->auSwizzle[uSrc], psNextInst->u.psVec->auSwizzle[0]);
	}

	return IMG_TRUE;
}

static IMG_BOOL CombineVMOVC_VMOVCBIT_NoPredicate(PINTERMEDIATE_STATE	psState,
												  PINST					psInst,
												  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineVMOVC_VMOVCBIT_NoPredicate

 PURPOSE	: Try to combine a VMOVC instruction with a VMOVCBIT instruction
			  using it's result where the VMOVC's test determines the
			  VMOVCBIT's result.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32			uChansFromSource0;
	IMG_BOOL			bFirstChan;
	IMG_BOOL			bInvertMOVCTest;
	IMG_UINT32			uChan;
	IMG_UINT32			uSrc;

	/*
		Check the first instruction is an unpredicated VMOVC or IVMOVCU8_FLT.
	*/ 
	ASSERT(psInst->eOpcode == IVMOVC || psInst->eOpcode == IVMOVCU8_FLT);
	ASSERT(NoPredicate(psState, psInst));

	/*
		Check the second instruction is a conditional move.
	*/
	if (psNextInst->eOpcode != IVMOVCBIT)
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Check the result of the first instruction is only used as source 0 of the second instruction.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}

	/*
		Check all the channels used by the second instruction are written by the first instruction.
	*/
	uChansFromSource0 = GetLiveChansInArg(psState, psNextInst, 0 /* uArg */);
	if ((uChansFromSource0 & ~psInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the second two sources to VMOVC are immediate.
	*/
	bFirstChan = IMG_TRUE;
	bInvertMOVCTest = IMG_FALSE;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((psInst->auDestMask[0] & (1 << uChan)) != 0)
		{
			IMG_BOOL	abBitResult[2];
			IMG_BOOL	bChanInvert;

			for (uSrc = 0; uSrc < 2; uSrc++)
			{
				IMG_UINT32	uChanValue;

				if (!GetImmediateVectorChan(psState, psInst, 1 + uSrc, uChan, &uChanValue))
				{
					return IMG_FALSE;
				}
				abBitResult[uSrc] = (uChanValue != 0) ? IMG_TRUE : IMG_FALSE;
			}

			if (abBitResult[0] == abBitResult[1])
			{
				return IMG_FALSE;
			}
			if (abBitResult[0])
			{
				bChanInvert = IMG_FALSE;
			}
			else
			{
				bChanInvert = IMG_TRUE;
			}
			if (bFirstChan)
			{
				bInvertMOVCTest = bChanInvert;
				bFirstChan = IMG_FALSE;
			}
			else
			{
				if (bInvertMOVCTest != bChanInvert)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Copy the destination of the second instruction to the first.
	*/
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	MovePartiallyWrittenDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	psInst->auDestMask[0] = psNextInst->auLiveChansInDest[0];

	CopyPredicate(psState, psInst, psNextInst);

	/*
		Copy the two conditional choices from the second instruction to the first.
	*/
	for (uSrc = 1; uSrc <= 2; uSrc++)
	{
		MoveVectorSource(psState, psInst, uSrc, psNextInst, uSrc, IMG_TRUE /* bMoveSourceModifier */);
	}

	if (bInvertMOVCTest)
	{
		psInst->u.psVec->eMOVCTestType = LogicalNegateTest(psState, psInst->u.psVec->eMOVCTestType);
	}

	return IMG_TRUE;
}

static IMG_BOOL CombineVTESTMASK_VMOVCBIT_NoPredicate(PINTERMEDIATE_STATE	psState,
													  PINST					psInst,
													  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineVTESTMASK_VMOVCBIT_NoPredicate

 PURPOSE	: Try to modify a VMOVCBIT instruction to VMOVC since the hardware
			  doesn't support the former instruction directly.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32			uChansFromSource0;

	/*
		Check the first instruction is an unpredicated VTESTMASK.
	*/ 
	ASSERT(psInst->eOpcode == IVTESTMASK);
	ASSERT(NoPredicate(psState, psInst));

	/*
		Check the second instruction is a conditional move.
	*/
	if (psNextInst->eOpcode != IVMOVCBIT)
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	ASSERT(psInst->u.psVec->sTest.eMaskType == USEASM_TEST_MASK_PREC ||
		   psInst->u.psVec->sTest.eMaskType == USEASM_TEST_MASK_NUM);

	/*
		Check the result of the first instruction is only used as source 0 of the second instruction.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}

	/*
		Check all the channels used by the second instruction are written by the first instruction.
	*/
	uChansFromSource0 = GetLiveChansInArg(psState, psNextInst, 0 /* uArg */);
	if ((uChansFromSource0 & ~psInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Change the first instruction to produce 1.0f on true and 0.0f on false.
	*/
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;

	/*
		Change the second instruction to use a floating point !=0 test.
	*/
	ModifyOpcode(psState, psNextInst, IVMOVC);
	psNextInst->u.psVec->eMOVCTestType = TEST_TYPE_NEQ_ZERO;

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL CombineNOT_Bitwise(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: CombineNOT_Bitwise

 PURPOSE	: Try to combine a NOT instruction whose result is used in a bitwise
			  instruction.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions to combine

 RETURNS	: TRUE if the instructions were combined; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	/*
		Check for:
			NOT			TEMP0, SRC
			BITWISE		DEST, TEMP1, TEMP0
		->
			BITWISE		DEST, TEMP1, ~TEMP0
	*/

	/*
		Check the first instruction is an unpredicated NOT instruction.
	*/ 
	if (!(psInst->eOpcode == INOT && NoPredicate(psState, psInst)))
	{
		return IMG_FALSE;
	}

	/*
		Check the second instruction is an unpredicated bitwise instruction.
	*/
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	if (g_psInstDesc[psNextInst->eOpcode].eType != INST_TYPE_BITWISE)
	{
		return IMG_FALSE;
	}
	if (psNextInst->eOpcode == INOT)
	{
		return IMG_FALSE;
	}

	/*
		Check the result of the first instruction is used only once in one of the 
		sources to the second instruction.
	*/
	uSrc = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);

	/*
		Check we aren't already using the logical negate source modifier.
	*/
	if (uSrc != 1 && psNextInst->u.psBitwise->bInvertSrc2)
	{
		return IMG_FALSE;
	}

	/*
		Make the first instruction into the combination of the effects of both instructions.
	*/
	SetOpcode(psState, psInst, psNextInst->eOpcode);
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
	MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psNextInst, 1 - uSrc);
	psInst->u.psBitwise->bInvertSrc2 = (!psNextInst->u.psBitwise->bInvertSrc2) ? IMG_TRUE : IMG_FALSE;

	return IMG_TRUE;
}

static IMG_FLOAT IsConstantValue(PARG psArg, IMG_PUINT32 puValue)
/*****************************************************************************
 FUNCTION	: IsConstantValue

 PURPOSE	: Checks if an argument has a constant value.

 PARAMETERS	: psArg				- Argument to check.
			  puValue			- Returns the constant value if it exists

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_FPCONSTANT)
	{
		switch (psArg->uNumber)
		{
			case EURASIA_USE_SPECIAL_CONSTANT_ZERO: *puValue = FLOAT32_ZERO; return IMG_TRUE;
			case EURASIA_USE_SPECIAL_CONSTANT_FLOAT1: *puValue = FLOAT32_ONE; return IMG_TRUE;
		}
	}
	else if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		*puValue = psArg->uNumber;
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CombineMovcTestNoPredicate(PINTERMEDIATE_STATE psState,
								PINST psInst, 
								PINST psNextInst,
								PUSC_LIST psNewMoveList)
/*****************************************************************************
 FUNCTION	: CombineMovcTestNoPredicate

 PURPOSE	: Try to combine a MOVC instruction whether both choices are constant
			  with a test on the result.

 PARAMETERS	: psState				- Internal compiler state
			  psInst, psNextInst	- The two instructions to combine
			  psNewMoveList			- If a new move instruction is generated
									then it is appended to this list.

 RETURNS	: TRUE if the instructions were combined; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_BOOL	abMatch[2] = {IMG_FALSE, IMG_FALSE};
	IMG_BOOL	abConst[2] = {IMG_FALSE, IMG_FALSE};
	IMG_UINT32	auTestInput[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
	IMG_UINT32	auMovcResult[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
	IMG_BOOL	abTestResult[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
	IOPCODE		eTestAluOpcode;
	IMG_BOOL	bTestMask;
	IOPCODE		eNewTestAluOpcode;
	IMG_UINT32	uNextInstArgCount;
	IMG_BOOL	bFloatTest;
	IMG_UINT32	uMatchSrcMask;

	ASSERT(psInst->eOpcode == IMOVC || psInst->eOpcode == IMOVC_I32);
	ASSERT(NoPredicate(psState, psInst));
	
	
	/*
		Check the next instruction is unpredicated.
	*/
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check for a TEST/TESTMASK
	*/
	if (psNextInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT && psNextInst->eOpcode == ITESTPRED)
	{
		eTestAluOpcode = psNextInst->u.psTest->eAluOpcode;
		bTestMask = IMG_FALSE;
	}
	else if (psNextInst->eOpcode == ITESTMASK)
	{
		eTestAluOpcode = psNextInst->u.psTest->eAluOpcode;
		bTestMask = IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
	/*
		Check the ALU opcode is one we know how to evaluate
		at compile time.
	*/
	if (
			eTestAluOpcode == IFADD ||
			eTestAluOpcode == IFMUL || 
			eTestAluOpcode == IFSUB || 
			eTestAluOpcode == IFMOV
		)
	{
		bFloatTest = IMG_TRUE;
	}
	else if (eTestAluOpcode == IOR)
	{
		bFloatTest = IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check that the two choices for the result of the MOVC are constant.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (!IsConstantValue(&psInst->asArg[1 + uArg], &auMovcResult[uArg]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check that the arguments to the test are either the result of the
		MOVC or constant.
	*/
	uMatchSrcMask = UseDefGetSourceUseMask(psState, psNextInst, &psInst->asDest[0]);
	if (uMatchSrcMask == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	uNextInstArgCount = g_psInstDesc[eTestAluOpcode].uDefaultArgumentCount;
	for (uArg = 0; uArg < uNextInstArgCount; uArg++)
	{
		PARG	psNextInstSrc = &psNextInst->asArg[uArg];

		if ((uMatchSrcMask & (1U << uArg)) != 0)
		{
			abMatch[uArg] = IMG_TRUE;
		}
		else
		{
			if (IsConstantValue(psNextInstSrc, &auTestInput[uArg]))
			{
				abConst[uArg] = IMG_TRUE;
			}
		}

		if (!(abMatch[uArg] || abConst[uArg]))
		{
			return IMG_FALSE;
		}
	}
		
	/*
		Calculate the result of the test depending on whether the test on 
		source 0 of the MOVC is true or false.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		IMG_UINT32	uTestArg;

		for (uTestArg = 0; uTestArg < uNextInstArgCount; uTestArg++)
		{
			if (abMatch[uTestArg])
			{
				auTestInput[uTestArg] = auMovcResult[uArg];
			}
		}

		if (bFloatTest)
		{
			IMG_FLOAT	fTestResult = 0.0f;
			IMG_FLOAT	afTestInput[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
			
			for (uTestArg = 0; uTestArg < uNextInstArgCount; uTestArg++)
			{
				afTestInput[uTestArg] = *(IMG_PFLOAT)&auTestInput[uTestArg];
			}

			switch (eTestAluOpcode)
			{
				case IFADD: fTestResult = afTestInput[0] + afTestInput[1]; break;
				case IFSUB: fTestResult = afTestInput[0] - afTestInput[1]; break;
				case IFMUL: fTestResult = afTestInput[0] * afTestInput[1]; break;
				case IFMOV: fTestResult = afTestInput[0]; break;
				default: imgabort();
			}
	
			abTestResult[uArg] = DoF32Test(psState, psNextInst->u.psTest->sTest.eType, fTestResult);
		}
		else
		{
			IMG_UINT32	uTestResult = 0U;

			switch (eTestAluOpcode)
			{
				case IOR: uTestResult = auTestInput[0] | auTestInput[1]; break;
				default: imgabort();
			}
			
			abTestResult[uArg] = DoBitwiseTest(psState, psNextInst->u.psTest->sTest.eType, uTestResult);
		}
	}

	/*
		Convert the MOVC into a TEST or TESTMASK.
	*/
	
	/*
		Copy the predicate/MASK destination to the new instruction.
	*/
	ASSERT(psNextInst->uDestCount == 1);
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);

	/*
		If the result of the test will always be either true or false then use
		a fixed test.
	*/
	if (abTestResult[0] == abTestResult[1])
	{
		SetOpcode(psState, psInst, IMOVPRED);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, abTestResult[0], UF_REGFORMAT_F32);
		AppendToList(psNewMoveList, &psInst->sTempListEntry);
	}
	else
	{
		TEST_TYPE	eNewTestType;

		/*
			Otherwise use the test from the MOVC.
		*/
		if (psInst->eOpcode == IMOVC)
		{
			eNewTestAluOpcode = IFMOV;
		}
		else
		{
			ASSERT(psInst->eOpcode == IMOVC_I32);

			eNewTestAluOpcode = IOR;
			SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
		}

		/*
			Take the logical inversion of the MOVC test.
		*/
		eNewTestType = psInst->u.psMovc->eTest;
		if (!abTestResult[0])
		{
			eNewTestType = LogicalNegateTest(psState, eNewTestType);
		}

		if (bTestMask)
		{
			SetOpcode(psState, psInst, ITESTMASK);
			psInst->u.psTest->eAluOpcode = eNewTestAluOpcode;

			ASSERT(psNextInst->u.psTest->sTest.eMaskType == USEASM_TEST_MASK_DWORD);
			psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
		}	
		else
		{
			SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		}
		psInst->u.psTest->eAluOpcode = eNewTestAluOpcode;
		psInst->u.psTest->sTest.eType = eNewTestType;

	}

	return IMG_TRUE;
}

static IMG_BOOL AllTestArgsHwConst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: AllTestArgsHwConst

 PURPOSE	: Are all the arguments to a TESTPRED/TESTMASK instruction hardware constants.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_TEST);
	for (uArg = 0; uArg < g_psInstDesc[psInst->u.psTest->eAluOpcode].uDefaultArgumentCount; uArg++)
	{
		PARG	psSrc = &psInst->asArg[uArg];

		if (psSrc->uType != USEASM_REGTYPE_FPCONSTANT && psSrc->uType != USEASM_REGTYPE_IMMEDIATE)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: InvertTest

 PURPOSE	: Generates the logical negation of a test.

 PARAMETERS	: psState			- Internal compiler state
			  uTest				- The test to invert.
			  pbMakeResultZero	- Set to TRUE on return if the caller should
								change the instruction so its result is zero.

 RETURNS	: The inverted test.
*****************************************************************************/
static IMG_VOID InvertTest(PINTERMEDIATE_STATE	psState,
						   PTEST_DETAILS		psTest,
						   IMG_PBOOL			pbMakeResultZero)
{
	/*
		If results of multiple channels are being combined, we need to swap AND and OR
		in addition to inverting the comparison for each channel, in order to get the
		correct inverted result:

		Org test: A && B && C && D			A || B || C || D
		Inverted: !(A && B && C && D)		!(A || B || C || D)
		 Same as: !A || !B || !C || !D		!A && !B && !C && !D
	*/
	switch (psTest->eChanSel)
	{
		case USEASM_TEST_CHANSEL_ANDALL:
		{
			psTest->eChanSel = USEASM_TEST_CHANSEL_ORALL;
			break;
		}
		case USEASM_TEST_CHANSEL_ORALL:
		{
			psTest->eChanSel = USEASM_TEST_CHANSEL_ANDALL;
			break;
		}
		case USEASM_TEST_CHANSEL_AND02:
		{
			psTest->eChanSel = USEASM_TEST_CHANSEL_OR02;
			break;
		}
		case USEASM_TEST_CHANSEL_OR02:
		{
			psTest->eChanSel = USEASM_TEST_CHANSEL_AND02;
			break;
		}
		default:
		{
			break;
		}
	}

	/*
		Check for always true cases.
	*/
	*pbMakeResultZero = IMG_FALSE;
	if (psTest->eType == TEST_TYPE_ALWAYS_TRUE)
	{
		psTest->eType = TEST_TYPE_NEQ_ZERO;
		*pbMakeResultZero = IMG_TRUE;
		return;
	}

	psTest->eType = LogicalNegateTest(psState, psTest->eType);
}

static IMG_BOOL FoldConstantTest(PINTERMEDIATE_STATE		psState,
								 PINST						psInst,
								 PINST						psDepInst)
/*****************************************************************************
 FUNCTION	: FoldConstantTest

 PURPOSE	: Check for a TEST instruction following by another TEST instruction
			  where the second instruction has a fixed result and is predicated
			  by the result of the first instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst, psDepInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG psDepInstPartialDest;

	ASSERT(psInst->eOpcode == ITESTPRED);
	ASSERT(psDepInst->eOpcode == ITESTPRED);

	/*
		Check the first instruction is an unpredicated TEST.
	*/
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}

	/* 
		Check for a move with test where the test result is constant and with a predicate that
		comes from the result of the first instruction. 

			Px = TEST(ALU_RESULT(...))
			Py = (if Px) ? TRUE : Pz						/ Py = (if Px) ? FALSE : Pz
		->
			Py = (if !Pz) ? TEST(ALU_RESULT(...)) : Pz		/ Py = (if Pz) ? not(TEST(ALU_RESULT(...))) : Pz
	*/
	psDepInstPartialDest = psDepInst->apsOldDest[TEST_PREDICATE_DESTIDX];
	if (
			psDepInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT &&
			(
				psDepInst->u.psTest->eAluOpcode == IFMOV || 
				psDepInst->u.psTest->eAluOpcode == IFADD || 
				psDepInst->u.psTest->eAluOpcode == IFSUB
			) &&
			AllTestArgsHwConst(psState, psDepInst) &&
			UseDefIsPredicateSourceOnly(psState, psDepInst, &psInst->asDest[TEST_PREDICATE_DESTIDX])
	   )
	{
		IMG_FLOAT	fValue = 0.0f;
		IMG_FLOAT	afValue[2];
		IMG_BOOL	bResult;
		IMG_BOOL	bInvertTest;
		IMG_BOOL	bMakeResultZero;
		IMG_UINT32	uArg;
		IMG_BOOL	bNegatePredicate;

		/*
			What is the fixed result of the second test.
		*/
		for (uArg = 0; uArg < g_psInstDesc[psDepInst->u.psTest->eAluOpcode].uDefaultArgumentCount; uArg++)
		{
			afValue[uArg] = GetHardwareConstantValue(psState, psDepInst->asArg[uArg].uNumber);
			if (IsNegated(psState, psDepInst, uArg))
			{
				afValue[uArg] = -afValue[uArg];
			}
		}
		switch (psDepInst->u.psTest->eAluOpcode)
		{
			case IFMOV: fValue = afValue[0]; break;
			case IFADD: fValue = afValue[0] + afValue[1]; break;
			case IFSUB: fValue = afValue[0] - afValue[1]; break;
			default: imgabort();
		}	
		bResult = DoF32Test(psState, psDepInst->u.psTest->sTest.eType, fValue);

		/*
			Change 
				pN = A op B						pN = A op B
				pM = pN ? T : pS				pM = pN ? F : pS
			=>								=>
				pM = (A op B) || pS				pM = !(A op B) && pS
			=>								=>
				pM = (!pS) ? (A op B) : pS		pM = pS ? !(A op B) : !pS
		*/
		if (bResult)
		{
			bInvertTest = IMG_FALSE;
			bNegatePredicate = IMG_TRUE;
			SetBit(psInst->auFlag, INST_PRED_NEG, 1);
		}
		else
		{
			bInvertTest = IMG_TRUE;
			bNegatePredicate = IMG_FALSE;
		}

		if (GetBit(psDepInst->auFlag, INST_PRED_NEG))
		{
			bInvertTest = (!bInvertTest) ? IMG_TRUE : IMG_FALSE;
		}
		if (bInvertTest)
		{
			InvertTest(psState, &psInst->u.psTest->sTest, &bMakeResultZero);
			if (bMakeResultZero)
			{
				psInst->u.psTest->eAluOpcode = IFMOV;
				SetSrc(psState, 
					   psInst, 
					   0 /* uSrcIdx */, 
					   USEASM_REGTYPE_FPCONSTANT, 
					   EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
					   UF_REGFORMAT_F32);
			}
		}

		/*
			Copy the predicate destination of the second instruction to the first instruction.
		*/
		MoveDest(psState, psInst, TEST_PREDICATE_DESTIDX, psDepInst, TEST_PREDICATE_DESTIDX);

		if (psDepInstPartialDest != NULL)
		{
			/*
				Copy the conditionally overwritten predicate from the second instruction to the
				first instruction.
			*/
			SetPartiallyWrittenDest(psState, psInst, TEST_PREDICATE_DESTIDX, psDepInstPartialDest);

			/*
				Set the predicate on the first instruction to the conditionally overwritten predicate
				from the second instruction.
			*/
			ASSERT(psDepInstPartialDest->uType == USEASM_REGTYPE_PREDICATE);
			SetPredicate(psState, psInst, psDepInstPartialDest->uNumber, bNegatePredicate); 
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL HasTestVariant(PINST psInst)
/*****************************************************************************
 FUNCTION	: HasTestVariant

 PURPOSE	: Checks if an opcode can have an attached test.

 PARAMETERS	: eOpcode	- Opcode to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IFRSQ:
		case IFRCP:
		case IFLOG:
		case IFEXP:
		{
			/*
				Check for a format conversion which isn't supported on a TEST instruction.
			*/
			if (psInst->asArg[0].eFmt != UF_REGFORMAT_F32)
			{
				return IMG_FALSE;
			}
			return IMG_TRUE;
		}
		case IFADD: 
		case IFSUBFLR:
		case IFDP:
		case IFMIN:
		case IFMAX:
		case IFDSX:
		case IFDSY:
		case IFMUL:
		case IFSUB: 
		{
			return IMG_TRUE;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVADD:
		case IVMUL:
		{
			return IMG_TRUE;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default: 
		{
			return IMG_FALSE;
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_BOOL IsTrivalVectorTest(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   PINST				psDepInst,
								   IMG_PUINT32			puNonZeroArg)
/*****************************************************************************
 FUNCTION	: IsTrivalVectorTest

 PURPOSE	: Checks for a TEST on a vector value where the TEST doesn't do
			  any extra calculations.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- The two instructions to check.
			  psDepInst	
			  puNonZeroArg		- Returns the argument to the TEST which is
								the result of the first instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check for:

			VADD	PDEST, SRC, (0, 0, 0, 0)
		or
			VADD	PDEST, (0, 0, 0, 0), SRC
	*/
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < 2; uArg++)
	{
		if (
				EqualArgs(&psInst->asDest[0], &psDepInst->asArg[uArg * SOURCE_ARGUMENTS_PER_VECTOR]) &&
				!psDepInst->u.psVec->asSrcMod[uArg].bAbs &&
				IsReplicatedImmediateVector(psState, psDepInst, 1 - uArg, 0 /* uImmValue */)
		   )
		{
			IMG_UINT32	uUsedChanMask;;

			/*
				Check all the channels used from the vector source are written by the earlier instructon.
			*/
			uUsedChanMask = GetLiveChansInArg(psState, psDepInst, uArg * SOURCE_ARGUMENTS_PER_VECTOR);
			if ((uUsedChanMask & ~psInst->auDestMask[0]) == 0)
			{
				*puNonZeroArg = uArg;
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL FoldSeperateTestNoPredicate(PINTERMEDIATE_STATE		psState,
											PINST						psInst,
											PINST						psDepInst)
/*****************************************************************************
 FUNCTION	: FoldSeperateTestNoPredicate

 PURPOSE	: Check if an instruction following by a TEST instruction using
			  its result can be combined into a single TEST instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst, psDepInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32 uArg;
	IMG_BOOL abNegateSrc[] = {IMG_FALSE, IMG_FALSE};
	IMG_BOOL bConvert;
	TEST_TYPE eTestType = TEST_TYPE_INVALID;
	IMG_BOOL bDisableWB = IMG_FALSE;
	IOPCODE eTestAluOpcode;
	IMG_BOOL bTestMask;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_UINT32 auNewSwizzle[2] = {USC_UNDEF, USC_UNDEF};
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(NoPredicate(psState, psInst));
	
	/*
		Check the first instruction isn't already a TEST and
		is suitable for the ALU operation of a TEST.
	*/
	if (
			!(
				psInst->uDestCount == 1 &&
				psInst->asDest[0].uType == USEASM_REGTYPE_TEMP &&
				HasTestVariant(psInst)
			 )
	   )
	{
		return IMG_FALSE;
	}

	/*
		Check for source modifiers (these aren't supported on test instructions) except
		where we can change ADD -> SUB.
	*/
	if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FLOAT)
	{
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			PFLOAT_SOURCE_MODIFIER	psMod = &psInst->u.psFloat->asSrcMod[uArg];

			if (psInst->eOpcode == IFADD && psMod->bNegate && !psMod->bAbsolute)
			{
				abNegateSrc[uArg] = IMG_TRUE;
				continue;
			}
			if (psMod->bNegate || psMod->bAbsolute)
			{
				return IMG_FALSE;
			}
		}
	}

	/* Check for TEST or TESTMASK */
	if (psDepInst->eOpcode == ITESTPRED && psDepInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		bTestMask = IMG_FALSE;
		eTestAluOpcode = psDepInst->u.psTest->eAluOpcode;
	}
	else
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psDepInst->eOpcode == IVTEST && psDepInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		bTestMask = IMG_FALSE;
		eTestAluOpcode = psDepInst->u.psVec->sTest.eTestOpcode;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (psDepInst->eOpcode == ITESTMASK)
	{
		bTestMask = IMG_TRUE;
		eTestAluOpcode = psDepInst->u.psTest->eAluOpcode;
	}
	else
	{
		return IMG_FALSE;
	}

	/* Check for a move with test. */
	bConvert = IMG_FALSE;
	if (eTestAluOpcode == IFMOV &&
		psDepInst->asArg[0].uType == psInst->asDest[0].uType &&
		psDepInst->asArg[0].uNumber == psInst->asDest[0].uNumber)
	{
		IMG_BOOL	bUsedInTestOnly;

		bUsedInTestOnly = UseDefIsSingleSourceUse(psState, psDepInst, 0 /* uSrcIdx */, &psInst->asDest[0]);

		if (bUsedInTestOnly || NoPredicate(psState, psDepInst))
		{
			eTestType = psDepInst->u.psTest->sTest.eType;
			bConvert = IMG_TRUE;
	
			bDisableWB = bUsedInTestOnly;
		}
	}
	/*
		Check for
				SUB		pN, 0, rM
	*/
	if (eTestAluOpcode == IFSUB &&
		psDepInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
		psDepInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
		psDepInst->asArg[1].uType == psInst->asDest[0].uType &&
		psDepInst->asArg[1].uNumber == psInst->asDest[0].uNumber &&
		psInst->eOpcode == IFADD)
	{
		IMG_BOOL	bUsedInTestOnly;

		bUsedInTestOnly = UseDefIsSingleSourceUse(psState, psDepInst, 1 /* uSrcIdx */, &psInst->asDest[0]);

		if (bUsedInTestOnly || NoPredicate(psState, psDepInst))
		{
			/*
				Apply the negation modifiers by changing the test.
			*/
			if (NegateTest(psDepInst->u.psTest->sTest.eType, &eTestType))
			{
				bConvert = IMG_TRUE;
	
				bDisableWB = bUsedInTestOnly;
			}
		}
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check for:

			VADD	PDEST, SRC, (0, 0, 0, 0)
		or
			VADD	PDEST, (0, 0, 0, 0), SRC
	*/
	if (eTestAluOpcode == IVADD)
	{
		IMG_UINT32	uNonZeroArg;

		if (IsTrivalVectorTest(psState, psInst, psDepInst, &uNonZeroArg))
		{
			IMG_UINT32	uPreSwizzleLiveChansInNonZeroArg;

			if (psDepInst->u.psVec->asSrcMod[uNonZeroArg].bNegate)
			{
				/*
					Apply the negate modifier on the TEST source by changing the sense of
					the TEST.
				*/
				if (!NegateTest(psDepInst->u.psVec->sTest.eType, &eTestType))
				{
					return IMG_FALSE;
				}
			}
			else
			{
				eTestType = psDepInst->u.psVec->sTest.eType;
			}

			/*
				Check the result of the first instruction is used only as a source to the TEST instruction.
			*/
			bDisableWB = 
				UseDefIsSingleSourceUse(psState, psDepInst, uNonZeroArg * SOURCE_ARGUMENTS_PER_VECTOR, &psInst->asDest[0]);
			if (!bDisableWB)
			{
				return IMG_FALSE;
			}

			/*
				Check we aren't trying to swizzle a constant into the result of the first instruction when it
				is used as a source to the TEST instruction.
			*/
			GetLiveChansInSourceSlot(psState, 
									 psDepInst, 
									 uNonZeroArg, 
									 &uPreSwizzleLiveChansInNonZeroArg, 
									 NULL /* puChansUsedPostSwizzle */);
			if (!IsNonConstantSwizzle(psDepInst->u.psVec->auSwizzle[uNonZeroArg], 
									  uPreSwizzleLiveChansInNonZeroArg, 
									  NULL /* puMatchedSwizzle */))
			{
				return IMG_FALSE;
			}

			if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
			{
				/*
					If the first instruction calculates an independent result per-channel apply any swizzle on the
					TEST instruction source by swizzling the sources to the first instruction.
				*/
				for (uArg = 0; uArg < 2; uArg++)
				{
					auNewSwizzle[uArg] = 
						CombineSwizzles(psInst->u.psVec->auSwizzle[uArg], psDepInst->u.psVec->auSwizzle[uNonZeroArg]);
				}
			}
			else
			{
				/*
					The first instruction broadcasts a scalar result to all channels of its destination so any
					swizzle on the TEST instruction source can be ignored.
				*/
				for (uArg = 0; uArg < 2; uArg++)
				{
					auNewSwizzle[uArg] = psInst->u.psVec->auSwizzle[uArg];
				}
			}

			/*
				Convert the two instructions to a single instruction.
			*/
			bConvert = IMG_TRUE;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		If both sources to the ADD are negated and the ADD result is only used for testing
		then (in effect) negate the result by changing the test.
	*/
	if (abNegateSrc[0] && abNegateSrc[1])
	{
		if (bDisableWB && NegateTest(eTestType, &eTestType))
		{
			abNegateSrc[0] = abNegateSrc[1] = IMG_FALSE;
		}
		else
		{
			bConvert = IMG_FALSE;
		}	
	}

	/*
		Check we don't need to write both the original destination of the non-TEST instruction
		and a unified store register with the mask result.
	*/
	if (bTestMask && !bDisableWB)
	{
		return IMG_FALSE;
	}

	if (bConvert)
	{
		/* Add the test to the first instruction. */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psDepInst->eOpcode == IVTEST)
		{
			IMG_UINT32	uDest;
			IOPCODE		eTestOpcode = psInst->eOpcode;

			ModifyOpcode(psState, psInst, IVTEST);
			SetDestCount(psState, psInst, VTEST_PREDICATE_ONLY_DEST_COUNT);
			
			for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
			{
				MoveDest(psState, psInst, uDest, psDepInst, uDest);
			}

			CopyPartiallyWrittenDest(psState, psInst, VTEST_PREDICATE_DESTIDX, psDepInst, VTEST_PREDICATE_DESTIDX);
			psInst->auDestMask[VTEST_PREDICATE_DESTIDX] = psDepInst->auDestMask[VTEST_PREDICATE_DESTIDX];
			psInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX] = psDepInst->auLiveChansInDest[VTEST_PREDICATE_DESTIDX];
			psInst->u.psVec->sTest.eTestOpcode = eTestOpcode;
			psInst->u.psVec->sTest.eType = eTestType;
			psInst->u.psVec->sTest.eChanSel = psDepInst->u.psVec->sTest.eChanSel;
			psInst->u.psVec->sTest.eMaskType = psDepInst->u.psVec->sTest.eMaskType;

			for (uArg = 0; uArg < 2; uArg++)
			{
				psInst->u.psVec->auSwizzle[uArg] = auNewSwizzle[uArg];
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			IOPCODE		eAluOpcode;
			IMG_UINT32	auSrcComponent[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];

			eAluOpcode = psInst->eOpcode;
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				ASSERT(uArg < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
				auSrcComponent[uArg] = GetComponentSelect(psState, psInst, uArg);
			}

			if (bTestMask)
			{
				SetOpcode(psState, psInst, ITESTMASK);
				MoveDest(psState, psInst, 0 /* uCopyToIdx */, psDepInst, 0 /* uCopyFromIdx */);
			}
			else
			{
				if (bDisableWB)
				{
					SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
				}
				else
				{
					SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
					MoveDest(psState, psInst, TEST_UNIFIEDSTORE_DESTIDX, psInst, 0 /* uCopyFromIdx */);
				}
				MoveDest(psState, psInst, TEST_PREDICATE_DESTIDX, psDepInst, TEST_PREDICATE_DESTIDX);
				CopyPartiallyWrittenDest(psState, psInst, TEST_PREDICATE_DESTIDX, psDepInst, TEST_PREDICATE_DESTIDX);
			}

			psInst->u.psTest->eAluOpcode = eAluOpcode;
			psInst->u.psTest->sTest.eType = eTestType;
			psInst->u.psTest->sTest.eChanSel = psDepInst->u.psTest->sTest.eChanSel;
			psInst->u.psTest->sTest.eMaskType = psDepInst->u.psTest->sTest.eMaskType;

			for (uArg = 0; uArg < g_psInstDesc[eAluOpcode].uDefaultArgumentCount; uArg++)
			{
				psInst->u.psTest->auSrcComponent[uArg] = auSrcComponent[uArg];
			}
		}

		CopyPredicate(psState, psInst, psDepInst);

		/* Remove source modifiers. */
		if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FLOAT)
		{
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				psInst->u.psFloat->asSrcMod[uArg].bNegate = IMG_FALSE;
			}
		}

		/*
			Apply a negation modifier by changing the opcode.
		*/
		if (abNegateSrc[0] || abNegateSrc[1])
		{
			ASSERT(!(abNegateSrc[0] && abNegateSrc[1]));
			if (abNegateSrc[0])
			{
				CommuteSrc01(psState, psInst);
			}

			ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_TEST);
			ASSERT(psInst->u.psTest->eAluOpcode == IFADD);
			psInst->u.psTest->eAluOpcode = IFSUB;
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX545)
static IMG_BOOL CombineMINMAXNoPredicate(PINTERMEDIATE_STATE		psState,
							  PINST						psInst,
							  PINST						psNextInst)
/*****************************************************************************
 FUNCTION	: CombineMINMAXNoPredicate

 PURPOSE	: Check if a MIN(MAX) instruction followed by a MAX(MIN) instruction
			  using the result can be combined into a single instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uFoundArg;
	
	ASSERT(psInst->eOpcode == IFMAX || psInst->eOpcode == IFMIN);
	ASSERT(psNextInst->eOpcode == IFMAX || psNextInst->eOpcode == IFMIN);
	ASSERT(NoPredicate(psState, psInst));

	if(!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP))
	{
		return IMG_FALSE;
	}
	
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check for the right opcodes.
	*/
	if (psNextInst->eOpcode == psInst->eOpcode)
	{
		return IMG_FALSE;
	}

	/*
		Check that the result of the first instruction is used as a source to
		the second instruction exactly once, without any source modifiers
		and this is the only place.
	*/
	uFoundArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uFoundArg == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	if (HasNegateOrAbsoluteModifier(psState, psNextInst, uFoundArg))
	{
		return IMG_FALSE;
	}

	if (psInst->eOpcode == IFMAX)
	{
		/*
			Convert

				MAX		rT, SRC1, SRC2
				MIN		rD, rT, SRC3

			->
				MAXMIN	rD, SRC1, SRC2, SRC3
		*/
		ModifyOpcode(psState, psInst, IFMINMAX);
		MoveFloatSrc(psState, psInst, 2, psNextInst, 1 - uFoundArg);
	}
	else
	{
		/*
			Convert

				MIN		rT, SRC1, SRC2
				MAX		rD, rT, SRC3

			->
				MINMAX	rD, SRC1, SRC3, SRC2
		*/
		ModifyOpcode(psState, psInst, IFMAXMIN);
		MoveFloatSrc(psState, psInst, 2, psInst, 1);
		MoveFloatSrc(psState, psInst, 1, psNextInst, 1 - uFoundArg);
	}
	MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);

	return IMG_TRUE;
}

static IMG_BOOL ConvertRSQRCPToSQRTNoPredicate(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: ConvertRSQRCPToSQRTNoPredicate

 PURPOSE	: Check if a RSQ instruction followed by a RCP instruction
			  using the result can be combined into a single instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check for an unpredicated RSQ.
	*/
	ASSERT(psInst->eOpcode == IFRSQ);
	ASSERT(psNextInst->eOpcode == IFRCP);
	ASSERT(NoPredicate(psState, psInst));
	
	if(!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS))
	{
		return IMG_FALSE;
	}
	
	/*
		Check the instruction has an F32 format register.
	*/
	if (
			psInst->asArg[0].eFmt != UF_REGFORMAT_F32 &&
			GetBit(psInst->auFlag, INST_TYPE_PRESERVE)
		)
	{
		return IMG_FALSE;
	}

	/*
		Check for unconditional, non-test RCP with F32 result and
		using the result of the first instruction without any
		source modifier.
	*/
	if (
			NoPredicate(psState, psNextInst) &&
			UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]) &&
			!HasNegateOrAbsoluteModifier(psState, psNextInst, 0)
	   )
	{
		/*
			RECIP(RSQRT(X)) -> SQRT(X)
		*/
		ModifyOpcode(psState, psInst, IFSQRT);
		MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);

		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_BOOL FoldTESTMASK_MOVC(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: FoldTESTMASK_MOVC

 PURPOSE	: Check if a TESTMASK followed by a MOVC_I32 using the result of
			  the TESTMASK can be converted to a single instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	TEST_TYPE	eTestType;

	ASSERT(psInst->eOpcode == ITESTMASK);
	ASSERT(psNextInst->eOpcode == IMOVC_I32);
	/*
		Check for an unpredicated TESTMASK with a trivial
		ALU operation.
	*/
	if (
			!(
				NoPredicate(psState, psInst) &&
				(
					(
						psInst->u.psTest->eAluOpcode == IFMOV &&
						psInst->asArg[0].eFmt == UF_REGFORMAT_F32
					) ||
					(
						psInst->u.psTest->eAluOpcode == IOR &&
						psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
						psInst->asArg[1].uNumber == 0
					)
				) &&
				psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check the test on the TESTMASK is supported on
		a MOVC.
	*/
	eTestType = psInst->u.psTest->sTest.eType;
	if (
			!(
					eTestType  == TEST_TYPE_EQ_ZERO ||
					eTestType  == TEST_TYPE_NEQ_ZERO ||
					eTestType  == TEST_TYPE_LT_ZERO ||
					eTestType  == TEST_TYPE_GTE_ZERO
			)
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is a MOVC_I32.
	*/
	if (
			!(
				(
					psNextInst->u.psMovc->eTest == TEST_TYPE_EQ_ZERO ||
					psNextInst->u.psMovc->eTest == TEST_TYPE_NEQ_ZERO
				)
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check the source 0 of the MOVC is the same as the TESTMASK destination and this
		is the only place where it's used.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}

	/*
		Convert the first instruction to a MOVC.
	*/
	if (psInst->u.psTest->eAluOpcode == IFMOV)
	{
		SetOpcode(psState, psInst, IMOVC);
	}
	else
	{
		ASSERT(psInst->u.psTest->eAluOpcode == IOR);
		SetOpcode(psState, psInst, IMOVC_I32);
	}

	/*
		Copy the predicate from the MOVC_I32.
	*/
	CopyPredicate(psState, psInst, psNextInst);

	/*
		Copy the destination and sources from the MOVC_I32.
	*/
	MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);
	CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	MoveSrc(psState, psInst, 1 /* uCopyToIdx */, psNextInst, 1 /* uCopyFromIdx */);
	MoveSrc(psState, psInst, 2 /* uCopyToIdx */, psNextInst, 2 /* uCopyFromIdx */);

	/*
		Clear the test-mask flag on the TEST.
	*/
	psInst->u.psMovc->eTest = eTestType;

	/*
		If the MOVC_I32 was testing for the TESTMASK result equal to 0 then
		take the logical inversion of the test.
	*/
	if (psNextInst->u.psMovc->eTest == TEST_TYPE_EQ_ZERO)
	{
		psInst->u.psMovc->eTest = LogicalNegateTest(psState, psInst->u.psMovc->eTest);
	}
	return IMG_TRUE;
}

static
IMG_BOOL FoldTESTMASK_TEST(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: FoldTESTMASK_TEST

 PURPOSE	: Check if a TESTMASK followed by a TEST using the result of
			  the TESTMASK can be converted to a single instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == ITESTMASK);
	ASSERT(psNextInst->eOpcode == ITESTPRED);
	
	/*
		Check for an unpredicated TESTMASK.
	*/
	if (
			!(
				NoPredicate(psState, psInst) &&
				psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check for a TEST with a trival ALU operation updating
		only a predicate.
	*/
	if (
			!(
				psNextInst->u.psTest->eAluOpcode == IOR &&
				psNextInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT &&
				(
					psNextInst->u.psTest->sTest.eType == TEST_TYPE_NEQ_ZERO ||
					psNextInst->u.psTest->sTest.eType == TEST_TYPE_EQ_ZERO
				) &&
				psNextInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
				psNextInst->asArg[1].uNumber == 0
			 )
		)
	{
		return IMG_FALSE;
	}
	/*
		Check the result tested comes from the result of the TESTMASK and
		this is the only use of the result.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}
	
	/*
		Convert the TESTMASK instruction to a TEST.
	*/
	ModifyOpcode(psState, psInst, ITESTPRED);
	SetDestCount(psState, psInst, TEST_PREDICATE_ONLY_DEST_COUNT /* uDestCount */);

	/*
		Copy the predicate destination from the TEST.
	*/
	MoveDest(psState, psInst, TEST_PREDICATE_DESTIDX, psNextInst, TEST_PREDICATE_DESTIDX);

	/*
		If TEST instruction conditionally updates its destination then copy the preserved destination.
	*/
	CopyPartiallyWrittenDest(psState, psInst, TEST_PREDICATE_DESTIDX, psNextInst, TEST_PREDICATE_DESTIDX);

	/*
		Copy any predicate source from the TEST.
	*/
	CopyPredicate(psState, psInst, psNextInst);

	/*
		Clear the TESTMASK flag.
	*/
	psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_NONE;

	/*
		If we've checking if the result of the TESTMASK is zero then
		take the logical inversion of test condition.
	*/
	if (psNextInst->u.psTest->sTest.eType == TEST_TYPE_EQ_ZERO)
	{
		psInst->u.psTest->sTest.eType = LogicalNegateTest(psState, psInst->u.psTest->sTest.eType);
	}

	return IMG_TRUE;
}

static IMG_BOOL UNPCKF32U8ToUNPCKF32O8NoPredicate(PINTERMEDIATE_STATE	psState,
									   PINST				psInst,
									   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: UNPCKF32U8ToUNPCKF32O8NoPredicate

 PURPOSE	: Try to convert an UNPCKF32U8 followed by a 2x-1 operation on
			  the result to UNPCKF32O8.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- UNPCKF32U8 instruction.
			  psNextInst	- Instruction which uses the first instruction's
							result.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArgFromDest;

	ASSERT(psInst->eOpcode == IUNPCKF32U8);
	ASSERT(psNextInst->eOpcode == IFMAD || psNextInst->eOpcode == IFADM);
	if(!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	
	/*
		Check the instruction is a conversion from fixed-point 8-bit to F32.
	*/
	if (!(psInst->u.psPck->bScale))
	{
		return IMG_FALSE;
	}

	/*
		Check that one of the second instruction's sources come from the
		UNPCKF32U8 result.
	*/
	uArgFromDest = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uArgFromDest == USC_UNDEF)
	{
		return IMG_FALSE;
	}
	if (uArgFromDest != 0 && uArgFromDest != 1)
	{
		return IMG_FALSE;
	}
	if (IsNegated(psState, psNextInst, uArgFromDest))
	{
		return IMG_FALSE;
	}

	/*
		Check if the second instruction does: MAD X, Y, 2, -1
	*/
	if (psNextInst->eOpcode == IFMAD)
	{
		PARG	psOtherArg = &psNextInst->asArg[1 - uArgFromDest];
		PARG	psArg2 = &psNextInst->asArg[2];

		if (psOtherArg->uType == USEASM_REGTYPE_FPCONSTANT &&
			psOtherArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT2 &&
			!IsNegated(psState, psNextInst, 1 - uArgFromDest) &&
			psArg2->uType == USEASM_REGTYPE_FPCONSTANT &&
			psArg2->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 &&
			IsNegated(psState, psNextInst, 2 /* uArgIdx */))
		{
			ModifyOpcode(psState, psInst, IUNPCKF32O8);
			MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);
			return IMG_TRUE;
		}
	}
	else
	/*
		Check if the second instruction does: ADM X, Y, -0.5, 2
	*/
	if (psNextInst->eOpcode == IFADM)
	{
		IMG_UINT32	uOtherArg = 1 - uArgFromDest;
		PARG		psOtherArg = &psNextInst->asArg[uOtherArg];
		PARG		psArg2 = &psNextInst->asArg[2];

		if (psOtherArg->uType == USEASM_REGTYPE_FPCONSTANT &&
			psOtherArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2 &&
			IsNegated(psState, psNextInst, uOtherArg) &&
			psArg2->uType == USEASM_REGTYPE_FPCONSTANT &&
			psArg2->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT2 &&
			!IsNegated(psState, psNextInst, 2))
		{
			ModifyOpcode(psState, psInst, IUNPCKF32O8);
			MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL FoldMINOrMAX_UNPCKF32U8NoPredicate(PINTERMEDIATE_STATE	psState,
										PINST				psInst,
										PINST				psNextInst,
										PUSC_LIST			psNewMoveList)
/*****************************************************************************
 FUNCTION	: FoldMINOrMAX_UNPCKF32U8NoPredicate

 PURPOSE	: Check for a MIN or MAX instruction followed by PCKU8F32 on the
			  result.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.
			  psNewMoveList			- If a new move instruction is generated
									then it is appended to this list.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IFMIN || psInst->eOpcode == IFMAX);
	ASSERT(psNextInst->eOpcode == IPCKU8F32);
	ASSERT(NoPredicate(psState, psInst));
	
	if (
			psInst->uDestCount > 0 &&
			NoPredicate(psState, psNextInst)
	   )
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < 2; uArg++)
		{
			PARG	psSrc = &psInst->asArg[uArg];

			if (psSrc->uType == USEASM_REGTYPE_FPCONSTANT &&
				(
					(
						psInst->eOpcode == IFMAX &&
						psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
					) ||	
					(
						psInst->eOpcode == IFMIN &&
						psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 &&
						!IsNegated(psState, psInst, uArg)
					)
				)
			   )
			{
				break;
			}
		}
		if (
				uArg < 2 &&
				/* Check the MIN/MAX destination is used only in the PCKU8F32. */
				UseDefGetSourceUseMask(psState, psNextInst, &psInst->asDest[0]) != USC_UNDEF &&
				!HasNegateOrAbsoluteModifier(psState, psInst, 1 - uArg)
		   )
		{
			if ((1 - uArg) != 0)
			{
				MoveFloatSrc(psState, psInst, 0 /* uCopyToIdx */, psInst, 1 - uArg /* uCopyFromIdx */);
			}
			if (psInst->asArg[0].eFmt == UF_REGFORMAT_F16)
			{
				ModifyOpcode(psState, psInst, IFMOV);
			}
			else
			{
				SetOpcode(psState, psInst, IMOV);
			}
			AppendToList(psNewMoveList, &psInst->sTempListEntry);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX545)
static IMG_BOOL FoldMINMAX_PCKU8F32NoPredicate(PINTERMEDIATE_STATE	psState,
								    PINST				psInst,
								    PINST				psNextInst,
									PUSC_LIST			psNewMoveList)
/*****************************************************************************
 FUNCTION	: FoldMINMAX_PCKU8F32NoPredicate

 PURPOSE	: Check for a MINMAX instruction followed by PCKU8F32 on the
			  result.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.
			  psNewMoveList			- If a new move instruction is generated
									then it is appended to this list.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IFMINMAX);
	ASSERT(psNextInst->eOpcode == IPCKU8F32);
	ASSERT(NoPredicate(psState, psInst));
	
	if (
			/*
				Check for

					MAXMIN	rDest, rSrc, 0, 1
			*/
			psInst->uDestCount > 0 &&
			!HasNegateOrAbsoluteModifier(psState, psInst, 1 /* uSrcIdx */) &&
			psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
			psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
			psInst->asArg[2].uType == USEASM_REGTYPE_FPCONSTANT &&
			psInst->asArg[2].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 &&
			!IsNegated(psState, psInst, 2 /* uArgIdx */) &&
			NoPredicate(psState, psNextInst) &&
			UseDefGetSourceUseMask(psState, psNextInst, &psInst->asDest[0]) != USC_UNDEF
	   )
	{
		MoveSrcAndModifiers(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
		if (psInst->asArg[0].eFmt == UF_REGFORMAT_F16)
		{
			ModifyOpcode(psState, psInst, IFMOV);
		}
		else
		{
			SetOpcode(psState, psInst, IMOV);
		}
		AppendToList(psNewMoveList, &psInst->sTempListEntry);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX545) */

static IMG_BOOL FoldMOVCXX_MULNoPredicate(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: FoldMOVCXX_MULNoPredicate

 PURPOSE	: Check for a MOVC instruction followed by MUL on the
			  result.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IMOVC || psInst->eOpcode == IMOVC_I32);
	ASSERT(psNextInst->eOpcode == IFMUL);
	ASSERT(NoPredicate(psState, psInst));
	
	if (
			psInst->asArg[1].uType == USEASM_REGTYPE_FPCONSTANT &&
			(
				psInst->asArg[1].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 || 
				psInst->asArg[1].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
			) &&
			psInst->asArg[2].uType == USEASM_REGTYPE_FPCONSTANT && 
			(
				psInst->asArg[2].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 || 
				psInst->asArg[2].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
			)
		)
	{
		/*
			Convert MOVC X, Y, 0, 1; MUL Z, X, C -> MOVC X, Y, 0, C 
		*/
		IMG_BOOL	abMatch[2];
		IMG_UINT32	uArg, uMatchMulSrcMask;

		/* Check where the result of the MOVC is used. */
		uMatchMulSrcMask = UseDefGetSourceUseMask(psState, psNextInst, &psInst->asDest[0]);
		if (uMatchMulSrcMask == USC_UNDEF)
		{
			return IMG_FALSE;
		}
		for (uArg = 0; uArg < 2; uArg++)
		{
			abMatch[uArg] = ((uMatchMulSrcMask & (1U << uArg)) != 0) ? IMG_TRUE : IMG_FALSE;
			if (abMatch[uArg])
			{
				if (IsNegated(psState, psNextInst, uArg))
				{
					return IMG_FALSE;
				}
			}
			else
			{
				if (HasNegateOrAbsoluteModifier(psState, psNextInst, uArg))
				{
					return IMG_FALSE;
				}
			}
		}

		CopyPredicate(psState, psInst, psNextInst);
		CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);
		for (uArg = 0; uArg < 2; uArg++)
		{
			IMG_UINT32	uSrcIdx = uArg + 1;
			PARG		psSrc = &psInst->asArg[uSrcIdx];

			if (psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
			{
				if (!(abMatch[0] && abMatch[1]))
				{
					if (abMatch[0])
					{
						CopySrc(psState, psInst, uSrcIdx, psNextInst, 1 /* uCopyFromIdx */);
					}
					else
					{
						ASSERT(abMatch[1]);
						CopySrc(psState, psInst, uSrcIdx, psNextInst, 0 /* uCopyFromIdx */);
					}
				}
			}
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL FoldMUL16_ADD16(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   PINST				psNextInst)
/*****************************************************************************
 FUNCTION	: FoldMUL16_ADD16

 PURPOSE	: Check for a MUL16 instruction followed by ADD16 on the
			  result and combine into a MAD16.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IFMUL16 );
	ASSERT(psNextInst->eOpcode == IFADD16);
	
	if (psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX)
	{
		if (EqualPredicates(psInst, psNextInst))
		{
			IMG_UINT32	uMatchArg;

			uMatchArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
			if (uMatchArg != USC_UNDEF && !IsAbsolute(psState, psNextInst, uMatchArg))
			{
				PFARITH16_PARAMS	psNextInstParams = psNextInst->u.psArith16;
				IMG_UINT32			uNonMatchArg = 1 - uMatchArg;

				ModifyOpcode(psState, psInst, IFMAD16);
				MoveDest(psState, psInst, 0 /* uCopyToIdx */, psNextInst, 0 /* uCopyFromIdx */);
				CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
				MoveSrc(psState, psInst, 2 /* uCopyToIdx */, psNextInst, uNonMatchArg);
				psInst->u.psArith16->aeSwizzle[2] = psNextInstParams->aeSwizzle[uNonMatchArg];
				psInst->u.psArith16->sFloat.asSrcMod[2] = psNextInstParams->sFloat.asSrcMod[uNonMatchArg];
				if (psNextInst->u.psArith16->sFloat.asSrcMod[1 - uNonMatchArg].bNegate)
				{
					InvertNegateModifier(psState, psInst, 0 /* uArgIdx */);
				}

				/*
					Apply a swizzle on the source to the ADD16 which is the result of the MUL16 to
					the sources of the MUL16.
				*/
				if (psNextInstParams->aeSwizzle[uMatchArg] != FMAD16_SWIZZLE_LOWHIGH)
				{
					IMG_UINT32			uMulArg;
					PFARITH16_PARAMS	psInstParams = psInst->u.psArith16;
					ASSERT(psNextInstParams->aeSwizzle[uMatchArg] != FMAD16_SWIZZLE_CVTFROMF32);
					for (uMulArg = 0; uMulArg < 2; uMulArg++)
					{
						psInstParams->aeSwizzle[uMulArg] =
							CombineFMad16Swizzle(psState,
												 psInstParams->aeSwizzle[uMulArg],
												 psNextInstParams->aeSwizzle[uMatchArg]);
					}
				}
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL FoldADD_ADDorSUB_ADD(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PINST					psNextInst)
{
	ASSERT(psInst->eOpcode == IFADD || psInst->eOpcode == IFSUB);
	ASSERT(psNextInst->eOpcode == IFADD);
	
	if (psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX)
	{
		if (
				/*
					Check the second ADD has the same predicate modes as the ADD.
				*/
				EqualPredicates(psNextInst, psInst) &&
				/*
					Check that both arguments is the same as the ADD destination and this
					is the only use of the ADD destination.
				*/
				UseDefGetSourceUseMask(psState, psNextInst, &psInst->asDest[0]) == 3 &&
				!IsAbsolute(psState, psNextInst, 0) &&
				!IsAbsolute(psState, psNextInst, 1) &&
				/*
					Check that the two sources have the same negate modifier setting.
				*/
				IsNegated(psState, psNextInst, 0) == IsNegated(psState, psNextInst, 1)
		   )
		{
			IMG_BOOL	bNegate;

			bNegate = IMG_FALSE;
			if (IsNegated(psState, psNextInst, 0) && IsNegated(psState, psNextInst, 1))
			{
				bNegate = IMG_TRUE;
			}

			/*
				Convert
					ADD		T, A, B				ADM		D, A, B, 2  (D=(A+B)*2)
					ADD		D, T, T		->
			*/
			if (psInst->eOpcode == IFSUB)
			{
				InvertNegateModifier(psState, psInst, 1 /* uArgIdx */);
			}
			ModifyOpcode(psState, psInst, IFADM);
			MoveDest(psState, psInst, 0 /* uDestIdx */, psNextInst, 0 /* uDestIdx */);
			CopyPartiallyWrittenDest (psState, psInst, 0, psNextInst, 0);
			SetSrc(psState, 
				   psInst, 
				   2 /* uSrcIdx */, 
				   USEASM_REGTYPE_FPCONSTANT, 
				   EURASIA_USE_SPECIAL_CONSTANT_FLOAT2, 
				   UF_REGFORMAT_F32);
			psInst->u.psFloat->asSrcMod[2].bNegate = bNegate;

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_BOOL FoldVMUL_VADD(PINTERMEDIATE_STATE		psState,
							  PINST						psInst,
							  PINST						psNextInst)
/*****************************************************************************
 FUNCTION	: FoldVMUL_VADD

 PURPOSE	: Check for a VMUL instruction followed by VADD on the
			  result and try to combine into a VMAD.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The two instructions to check.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uMatchedArg;
	IMG_UINT32	uMatchedVecArg;
	PARG		psMatchedArg;
	IMG_UINT32	uLiveChansInVecArg;
	IMG_UINT32	uPreSwizzleLiveChansInVecArg;
	IMG_UINT32	uChan;

	ASSERT(psInst->eOpcode == IVMUL);

	/*
		Check the dependent instruction is an ADD.
	*/
	if (psNextInst->eOpcode != IVADD)
	{
		return IMG_FALSE;
	}

	/*
		Check both the ADD and the MUL have the same predicate.
	*/
	if (!EqualPredicates(psNextInst, psInst))
	{
		return IMG_FALSE;
	}

	/*
		Check only one argument to the ADD comes from the result of the MUL.
	*/
	uMatchedArg = UseDefGetSingleSourceUse(psState, psNextInst, &psInst->asDest[0]);
	if (uMatchedArg == USC_UNDEF)
	{
		return IMG_FALSE;
	}

	ASSERT((uMatchedArg % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uMatchedVecArg = uMatchedArg / SOURCE_ARGUMENTS_PER_VECTOR;
	psMatchedArg = &psNextInst->asArg[uMatchedArg];

	/*
		Check the result of the MUL doesn't have an absolute source modifier when used in
		the ADD.
	*/
	if (psNextInst->u.psVec->asSrcMod[uMatchedVecArg].bAbs)
	{
		return IMG_FALSE;
	}

	/*
		Get the channels referenced by the ADD instruction from its source which is written
		by the MUL.
	*/
	GetLiveChansInSourceSlot(psState, 
							 psNextInst, 
							 uMatchedVecArg, 
							 &uPreSwizzleLiveChansInVecArg, 
							 &uLiveChansInVecArg);

	/*
		Check all the channels used from the ADD source are written by the MUL.
	*/
	if (uLiveChansInVecArg != psInst->auDestMask[0])
	{
		return IMG_FALSE;
	}

	/*
		Check none of the swizzle selectors used on the ADD source are 2 or 0.5. If the swizzle
		selectors are 1 or 0 then we can generate the correct value in the combined instruction
		by setting the corresponding swizzle selector on the first two sources to 1 or 0.
	*/
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uPreSwizzleLiveChansInVecArg & (1U << uChan)) != 0)
		{
			IMG_UINT32	uSel;

			uSel = GetChan(psNextInst->u.psVec->auSwizzle[uMatchedVecArg], uChan);
			if (uSel == USEASM_SWIZZLE_SEL_2 || uSel == USEASM_SWIZZLE_SEL_HALF)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Change the first instruction to a MAD.
	*/
	ModifyOpcode(psState, psInst, IVMAD);

	/*
		Copy the ADD's destination to the MAD.
	*/
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	CopyPartiallyWrittenDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	psInst->auDestMask[0] = psNextInst->auDestMask[0];

	/*
		Copy the source to the ADD which isn't the result of the MUL into the MAD instruction.
	*/
	MoveVectorSource(psState, 
					 psInst, 
					 2 /* uDestSlot */, 
					 psNextInst, 
					 1 - uMatchedVecArg /* uSrcSlot */, 
					 IMG_TRUE /* bMoveSourceModifier */);

	/*
		Handle a negate modifier on the ADD source by inverting the negate modifier on one of the sources
		to the multiply.
	*/
	if (psNextInst->u.psVec->asSrcMod[uMatchedVecArg].bNegate)
	{
		psInst->u.psVec->asSrcMod[0].bNegate = (IMG_BOOL) !psInst->u.psVec->asSrcMod[0].bNegate;
	}

	/*
		Handle a swizzle on the ADD source by applying the swizzle to both of the multiply sources.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		psInst->u.psVec->auSwizzle[uArg] = 
			CombineSwizzles(psInst->u.psVec->auSwizzle[uArg], psNextInst->u.psVec->auSwizzle[uMatchedVecArg]);
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL FoldMUL_ADDOrMUL(PINTERMEDIATE_STATE	psState,
								 PINST					psInst,
								 PINST					psNextInst)
{
	ASSERT(psInst->eOpcode == IFMUL);
	
	if (psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX)
	{
		IMG_BOOL	bFirstIsSqr;
		IMG_UINT32	uArg = 0;
		IMG_BOOL	bNeg2;
		IMG_BOOL	bNegResult;

		/*
			Is the first MUL suitable for the first part of an MSA.
		*/
		if (EqualFloatSrcs(psState,psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */))
		{
			bFirstIsSqr = IMG_TRUE;
		}
		else
		{
			bFirstIsSqr = IMG_FALSE;
		}

		/*
			Check for an instruction which we can combine with the current
			instruction into MSA instruction.
		*/
		if (FindMSAPart(psState, psInst, psNextInst, bFirstIsSqr))
		{
			ModifyOpcode(psState, psInst, IFMSA);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			if (bFirstIsSqr)
			{
				MoveFloatSrc(psState, psInst, 1, psNextInst, 0);
				MoveFloatSrc(psState, psInst, 2, psNextInst, 1);
			}
			else
			{
				MoveFloatSrc(psState, psInst, 2, psInst, 1);
				MoveFloatSrc(psState, psInst, 1, psInst, 0);
				MoveFloatSrc(psState, psInst, 0, psNextInst, 1);

				if (IsNegated(psState, psNextInst, 2))
				{
					InvertNegateModifier(psState, psInst, 1 /* uArgIdx */);
				}
			}
			return IMG_TRUE;
		}
		/*
			Check for an instruction which multiplies the result of this one
			by two.

			MUL		T, A, B			-> ADM D, A, A, B   (D = (A + A) * B = AB + AB = 2AB)
			MUL		D, 2, T			
		*/
		if (FindMulX2(psState, psInst, psNextInst, &bNegResult, &bNeg2))
		{
			IMG_UINT32	uDuplicateSrc;
			
			ModifyOpcode(psState, psInst, IFADM);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);

			if (CanUseSource0(psState, psInst->psBlock->psOwner->psFunc, &psInst->asArg[0]))
			{
				uDuplicateSrc = 0;
			}
			else
			{
				uDuplicateSrc = 1;
			}

			MoveFloatSrc(psState, psInst, 2, psInst, 1 - uDuplicateSrc);
			if (bNegResult != bNeg2)
			{
				InvertNegateModifier(psState, psInst, 2 /* uArgIdx */);
			}
			CopySrcAndModifiers(psState, psInst, 1 - uDuplicateSrc, psInst, uDuplicateSrc);
			return IMG_TRUE;
		}
		/*
			Check for an ADD instruction uses the result of this instruction.
		*/
		if (FindAdd(psState, psInst, psNextInst, &uArg))
		{
			ModifyOpcode(psState, psInst, IFMAD);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			MoveFloatSrc(psState, psInst, 2 /* uMoveToSrcIdx */, psNextInst, uArg /* uMoveFromSrcIdx */);
			if (IsNegated(psState, psNextInst, 1 - uArg))
			{
				InvertNegateModifier(psState, psInst, 0 /* uArgIdx */);
			}
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL FoldADD_ADD(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
							PINST				psNextInst,
							PUSC_LIST			psNewMoveList,
							IMG_PBOOL			pbExtraMoves,
							IMG_PBOOL			pbExtraF16Moves,
							IMG_PBOOL			pbDroppedTemporaries)
/*****************************************************************************
 FUNCTION	: FoldADD_ADD

 PURPOSE	: 

 PARAMETERS	: 

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IFADD);
	ASSERT(psNextInst->eOpcode == IFADD);
	
	if (
		psInst->uDestCount > 0 &&
		NoPredicate(psState, psInst) &&
		psNextInst->uDestCount > 0 &&
		NoPredicate(psState, psNextInst)
		&& (psState->uCompilerFlags & UF_OPENCL) == 0
	   )

	{
		IMG_UINT32	uRemainingTerm;
		IMG_BOOL	bNegateRemainingTerm;	

		if (TermsCancelOut(psState, psInst, psNextInst, &uRemainingTerm, &bNegateRemainingTerm, pbDroppedTemporaries))
		{
			UF_REGFORMAT			eRemainingArgFmt = psInst->asArg[uRemainingTerm].eFmt;
			FLOAT_SOURCE_MODIFIER	sRemainingMod = psInst->u.psFloat->asSrcMod[uRemainingTerm];

			sRemainingMod.bNegate = bNegateRemainingTerm;

			*pbExtraMoves = IMG_TRUE;
			if (eRemainingArgFmt == UF_REGFORMAT_F16)
			{
				*pbExtraF16Moves = IMG_TRUE;
			}

			if (uRemainingTerm != 0)
			{
				MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, uRemainingTerm);
			}
			if (sRemainingMod.bNegate || 
				sRemainingMod.bAbsolute)
			{
				SetOpcode(psState, psInst, IFMOV);
			}
			else if (eRemainingArgFmt == UF_REGFORMAT_F16)
			{
				SetOpcode(psState, psInst, IUNPCKF32F16);
			}
			else
			{
				SetOpcode(psState, psInst, IMOV);
			}
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);

			if (psInst->eOpcode == IFMOV)
			{
				psInst->u.psFloat->asSrcMod[0] = sRemainingMod;
			}

			if (psInst->eOpcode == IUNPCKF32F16)
			{
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, sRemainingMod.uComponent);
			}

			AppendToList(psNewMoveList, &psInst->sTempListEntry);
			
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL SimplifyAddressConversionNoPredicate(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: SimplifyAddressConversionNoPredicate

 PURPOSE	: Try to simplify a conversion from F32->I32/U32 followed by a
			  conversion from I32/U32 to a format suitable for addressing
			  into register arrays or constant buffers in memory.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- F32->I32/U32 conversion instruction.
			  psNextInst		- I32/U32 to address conversion instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check the first instruction is an unpredicated float to integer conversion.
	*/
	ASSERT(psInst->eOpcode == ICVTFLT2INT);
	ASSERT(psNextInst->eOpcode == ICVTINT2ADDR);
	ASSERT(NoPredicate(psState, psInst));
	
	/*
		Check the second instruction is an unpredicated integer to address conversion.
	*/
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Check the destination of the first instruction is the source to the second instruction
		and this is the only use of the destination.
	*/
	if (!UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]))
	{
		return IMG_FALSE;
	}

	/*
		Merge the two instructions into a conversion in a single step.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		if (psInst->u.psCvtFlt2Int->bSigned)
		{
			SetOpcode(psState, psInst, IVPCKS16FLT);
		}
		else
		{
			SetOpcode(psState, psInst, IVPCKU16FLT);
		}

		psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psInst->u.psVec->aeSrcFmt[0] = psInst->asArg[0].eFmt;

		MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		SetSrcUnused(psState, psInst, 0);
		SetSrcUnused(psState, psInst, 2);
		SetSrcUnused(psState, psInst, 3);
		SetSrcUnused(psState, psInst, 4);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		if (psInst->u.psCvtFlt2Int->bSigned)
		{
			SetOpcode(psState, psInst, IPCKS16F32);
		}
		else
		{
			SetOpcode(psState, psInst, IPCKU16F32);
		}

		SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
	}
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);

	return IMG_TRUE;
}

static IMG_BOOL OptimizeInstSeqNoPredicate(PINTERMEDIATE_STATE psState,
											PINST psInst, 
											PINST psNextInst, 
											PUSC_LIST psNewMoveList,
											IMG_PBOOL pbExtraMoves,
											IMG_PBOOL pbRecheckSourceModifiers)
/*****************************************************************************
 FUNCTION	: OptimizeInstSeq

 PURPOSE	: Try to optimize a dependent sequence of instructions. Where first
 			  Instruction do not have predicate
 			  (updates its destination unconditionally)

 PARAMETERS	: 

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Merge an instruction together with a TEST instruction using the result.
	*/
	if (FoldSeperateTestNoPredicate(psState, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	
	switch (psInst->eOpcode)
	{
		case ICVTFLT2INT:
		{
			if(psNextInst->eOpcode == ICVTINT2ADDR)
			{
				if (SimplifyAddressConversionNoPredicate(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case IFMIN:
		case IFMAX:
		{
			#if defined(SUPPORT_SGX545)
			/*
				Combine a MAX(MIN) instruction followed by a MIN(MAX) instruction into a MAXMIN(MINMAX) instruction.
			*/
			if(psNextInst->eOpcode == IFMAX || psNextInst->eOpcode == IFMIN)
			{
				if (CombineMINMAXNoPredicate(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			if(psNextInst->eOpcode == IPCKU8F32)
			{
				/*
					Check for a max or min followed by a pack on the result.
				*/
				if (FoldMINOrMAX_UNPCKF32U8NoPredicate(psState, psInst, psNextInst, psNewMoveList))
				{
					*pbExtraMoves = IMG_TRUE;
					return IMG_FALSE;
				}
			}
			return IMG_FALSE;
		}
		case IFRSQ:
		{
			#if defined(SUPPORT_SGX545)
			/*
				Combine RECIP(RSQRT(X)) -> SQRT(X)
			*/
			if(psNextInst->eOpcode == IFRCP)
			{
				if (ConvertRSQRCPToSQRTNoPredicate(psState, psInst, psNextInst))
				{
					return IMG_TRUE;	
				}
			}
			#endif /* defined(SUPPORT_SGX545) */
			return IMG_FALSE;
		}
		case IMOVC:
		case IMOVC_I32:
		{
			/*
				Check for a MOVC with constant arguments followed by a MUL on the result.
			*/
			if(psNextInst->eOpcode == IFMUL)
			{
				if (FoldMOVCXX_MULNoPredicate(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			else
			/*
				Check for a MOVC with constant arguments followed by a test on the
				result.
			*/
			if (CombineMovcTestNoPredicate(psState, psInst, psNextInst, psNewMoveList))
			{
				/*
					Changing the instruction might give new opportunities to remove
					moves since TEST has less bank restrictions.
				*/
				*pbExtraMoves = IMG_TRUE;
				*pbRecheckSourceModifiers = IMG_TRUE;
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		#if defined(SUPPORT_SGX545)
		case IFMINMAX:
		{
			if(psNextInst->eOpcode == IPCKU8F32)
			{
				if (FoldMINMAX_PCKU8F32NoPredicate(psState, psInst, psNextInst, psNewMoveList))
				{
					*pbExtraMoves = IMG_TRUE;
					return IMG_FALSE;
				}
			}
			return IMG_FALSE;
		}
		#endif /* defined(SUPPORT_SGX545) */
		case IUNPCKF32U8:
		{
			/*
				Convert UNPCKF32U8 followed by 2x-1 on the result to UNPCKF32O8.
			*/
			if(psNextInst->eOpcode == IFMAD || psNextInst->eOpcode == IFADM)
			{
				if (UNPCKF32U8ToUNPCKF32O8NoPredicate(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVMOVC:
		{
			if (CombineVMOVC_VTESTNoPredicate(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			/* Fallthrough */
		}
		case IVMOVCU8_FLT:
		{
			if (CombineVMOVC_VMOVCBIT_NoPredicate(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case IVTESTMASK:
		{
			/* This optimization might modify the instructions but doesn't merge them. */
			CombineVTESTMASK_VMOVCBIT_NoPredicate(psState, psInst, psNextInst);

			if (CombineVTESTMASK_VMOVC_NoPredicate(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			if (CombineVTESTMASK_VTEST_NoPredicate(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			return IMG_FALSE; 
		}
		case IVTESTBYTEMASK:
		{
			if (CombineVTESTMASK_VMOVC_NoPredicate(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default:
			break;
	}
	return IMG_FALSE;
}

static IMG_BOOL OptimizeInstSeqPredicateDontCare(PINTERMEDIATE_STATE psState,
														     PINST psInst, 
														     PINST psNextInst, 
															 PUSC_LIST psNewMoveList,
														     IMG_PBOOL pbExtraMoves,
														     IMG_PBOOL pbExtraF16Moves,
														     IMG_PBOOL pbDroppedTemporaries)
/*****************************************************************************
 FUNCTION	: OptimizeInstSeqPredicateDontCare

 PURPOSE	: Try to optimize a dependent sequence of instructions. Where 
 			  First intruction can have predicate 
 			  (updates its destination conditionally/unconditionally)

 PARAMETERS	: 

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case ITESTPRED:
		{
			/*
				Merge two TEST instructions computing the same result together.
			*/
			if(psNextInst->eOpcode == ITESTPRED)
			{
				/*
					Combine a TEST instruction with another TEST instruction with a constant result
					and predicated by the first TEST.
				*/
				if (FoldConstantTest(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case IFADD:
			/*
				Check for a sequence of adds where some of the terms cancel out.
			*/
			if(psNextInst->eOpcode == IFADD)
			{
				if (FoldADD_ADD(psState, 
								psInst,
								psNextInst, 
								psNewMoveList, 
								pbExtraMoves, 
								pbExtraF16Moves, 
								pbDroppedTemporaries))
				{
					return IMG_TRUE;
				}
			}
			/* Intentional fall through */
		case IFSUB:
		{
			/*
				Convert ADD followed by a ADD or ADD into ADM.
			*/
			if(psNextInst->eOpcode == IFADD)
			{
				if (FoldADD_ADDorSUB_ADD(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case IFMUL16:
		{
			/*
				Convert IFMUL16 T, A, B; IFADD D, T, C -> IFMAD16 D, A, B, C
			*/
			if(psNextInst->eOpcode == IFADD16)
			{
				if (FoldMUL16_ADD16(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}

			return IMG_FALSE;
		}
		case ITESTMASK:
		{
			/*
				Check for a TESTMASK following by a TEST on the TESTMASK's destination register.
			*/
			if(psNextInst->eOpcode == ITESTPRED)
			{
				if (FoldTESTMASK_TEST(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			else
			/*
				Check for a TESTMASK following by a MOVC_I32 using the TESTMASK result to choose
				one of the other two sources.
			*/
			if(psNextInst->eOpcode == IMOVC_I32)
			{
				if (FoldTESTMASK_MOVC(psState, psInst, psNextInst))
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case IFMUL:
		{
			/*
				Convert MUL followed by ADD or MUL into various instructions.
			*/
			if (FoldMUL_ADDOrMUL(psState, psInst, psNextInst))
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVMUL:
		{
			/*
				Combine VMUL followed by VADD into VMAD.
			*/
			return FoldVMUL_VADD(psState, psInst, psNextInst);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		case INOT:
		{
			return CombineNOT_Bitwise(psState, psInst, psNextInst);
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL OptimizeInstructionSequence(PINTERMEDIATE_STATE psState,
											PINST psInst, 
											PINST psNextInst, 
											PUSC_LIST psNewMoveList,
											IMG_PBOOL pbExtraMoves,
											IMG_PBOOL pbExtraF16Moves,
											IMG_PBOOL pbDroppedTemporaries,
											IMG_PBOOL pbRecheckSourceModifiers)
/*****************************************************************************
 FUNCTION	: OptimizeInstructionSequence

 PURPOSE	: Try to optimize a dependent sequence of instructions.

 PARAMETERS	: 

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_BOOL bResult;
	
	if(NoPredicate(psState, psInst))
	{
		bResult = OptimizeInstSeqNoPredicate(psState, 
														 psInst, 
														 psNextInst,
														 psNewMoveList,
														 pbExtraMoves,
														 pbRecheckSourceModifiers);
		if(bResult == IMG_TRUE)
		{
			return IMG_TRUE;
		}
	}
	bResult = OptimizeInstSeqPredicateDontCare(psState, 
														 psInst, 
														 psNextInst, 
														 psNewMoveList,
														 pbExtraMoves,
														 pbExtraF16Moves,
														 pbDroppedTemporaries);
	return bResult;
}

IMG_INTERNAL
IMG_VOID ExpandUnsupportedInstructions(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: ExpandUnsupportedInstructions

 PURPOSE	: Expand some instructions not supported directly by the hardware.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IOPCODE		eOpcode;
		IMG_VOID (*pfExpand)(PINTERMEDIATE_STATE psState, PINST psInst);
		IMG_UINT32	uFeatureFlag;
	} asExpansion[] = 
	{
		{ICVTFLT2INT,	ExpandCVTFLT2INTInstruction,			0},
		{ICVTINT2ADDR,	ExpandCVTINT2ADDRInstruction,			0},
		{IFTRC,			ExpandFTRCInstruction,					0},
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		{IVTRC,			ExpandVTRCInstruction,					0},
		{IVMOVCBIT,		ExpandVMOVCBITInstruction,				0},
		{IVPCKFLTU8,	ExpandInvalidVectorPackToFloat,			0},
		{IVPCKFLTS8,	ExpandInvalidVectorPackToFloat,			0},
		{IVPCKFLTU16,	ExpandInvalidVectorPackToFloat,			0},
		{IVPCKFLTS16,	ExpandInvalidVectorPackToFloat,			0},
		{IVPCKU8FLT,	ExpandInvalidVectorPackToFixedPoint,	0},
		{IVPCKS8FLT,	ExpandInvalidVectorPackToFixedPoint,	0},
		{ICVTFLT2ADDR,	ExpandCVTFLT2ADDRInstruction,			0},
		{IPCKU8U8,		ExpandPCKU8U8Inst,						SGX_FEATURE_FLAGS_USE_VEC34},
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	IMG_UINT32	uExpandTypeIdx;
	IMG_BOOL	bExpandedInstruction;

	bExpandedInstruction = IMG_FALSE;
	for (uExpandTypeIdx = 0; uExpandTypeIdx < (sizeof(asExpansion) / sizeof(asExpansion[0])); uExpandTypeIdx++)
	{
		IOPCODE			eExpandOpcode = asExpansion[uExpandTypeIdx].eOpcode;
		IMG_UINT32		uFeatureFlag = asExpansion[uExpandTypeIdx].uFeatureFlag;

		if (uFeatureFlag != 0 && (psState->psTargetFeatures->ui32Flags & uFeatureFlag) == 0)
		{
			continue;
		}

		if (!SafeListIsEmpty(&psState->asOpcodeLists[eExpandOpcode]))
		{
			bExpandedInstruction = IMG_TRUE;
		}

		ForAllInstructionsOfType(psState, eExpandOpcode, asExpansion[uExpandTypeIdx].pfExpand);
	}

	if (bExpandedInstruction)
	{
		TESTONLY_PRINT_INTERMEDIATE("Expanded Unsupported Instructions", "exp_unsup", IMG_FALSE);
	}
}

static
IMG_BOOL AllPredInstsAlreadyOutput(PINTERMEDIATE_STATE	psState,
								   PDGRAPH_STATE		psDepState,
								   PINST				psInst, 
								   PARG					asPred, 
								   IMG_UINT32			uPredCount)
/*****************************************************************************
 FUNCTION	: AllPredInstsAlreadyOutput
    
 PURPOSE	: Check whether all instructions required for a given instruction have already been output
			  or who, if they are predicated, use one of a set of predicates.

 PARAMETERS	: psState		- Compiler state.
			  psDepState	- Dependency state
			  psInst		- Instruction to check
			  asPred		- Array of allowed predicates.
			  uPredCount	- Length of the array of allowed predicates.

 RETURNS	: TRUE if they could all be output.
*****************************************************************************/
{
	IMG_UINT32	uInst;
	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		if (ArrayGet(psState, psDepState->psInstructions, uInst) != NULL &&
			GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, uInst))
		{
			PINST psRequiredInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uInst);

			if (psRequiredInst->uPredCount != 0)
			{
				IMG_UINT32	uIndex, uPred;
				IMG_BOOL	bMatchesPred = IMG_FALSE;

				for (uPred = 0; uPred < psRequiredInst->uPredCount; ++uPred)
				{
					if (psRequiredInst->apsPredSrc[uPred] == 0)
					{
						continue;
					}

					for (uIndex = 0; uIndex < uPredCount; ++uIndex)
					{
						if (asPred[uIndex].uType == USEASM_REGTYPE_PREDICATE)
						{
							if (psRequiredInst->apsPredSrc[uPred]->uNumber == asPred[uIndex].uNumber)
							{
								bMatchesPred = IMG_TRUE;
							}
						}
					}
				}

				if (!bMatchesPred)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL CouldOutputAllDependentInsts(PINTERMEDIATE_STATE psState, PDGRAPH_STATE psDepState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CouldOutputAllDependantInsts
    
 PURPOSE	: Whether all instructions dependent on this one could be output
			  without outputting one with a predicate that is not written to
			  by this one.

 PARAMETERS	: psState		- Compiler state.
			  psDepState	- Dependency state
			  psInst		- Instruction to check

 RETURNS	: TRUE if they could all be output.
*****************************************************************************/
{
	IMG_UINT32				uInst;
	ADJACENCY_LIST_ITERATOR	sIter;

	PADJACENCY_LIST psAdjacentList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, psInst->uId);
	for (uInst = FirstAdjacent(psAdjacentList, &sIter); !IsLastAdjacent(&sIter); uInst = NextAdjacent(&sIter))
	{
		PINST psDepInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uInst);

		if (!AllPredInstsAlreadyOutput(psState, psDepState, psDepInst, psInst->asDest, psInst->uDestCount))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ShouldInsertInstNow(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psBlock,
							 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ShouldInsertInstNow
    
 PURPOSE	: Check whether an instruction should be output now (into a half filled block).
			  False if the instruction movement would be larger than the limit (if there is one).

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to insert instruction into.
			  psInst	- The instruction to insert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
	{
		if ((psInst->uId >= psBlock->uInstCount && (psInst->uId - psBlock->uInstCount > psState->uMaxInstMovement)) ||
			(psInst->uId < psBlock->uInstCount && (psBlock->uInstCount - psInst->uId > psState->uMaxInstMovement)))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID ScheduleForPredRegPressureBP(PINTERMEDIATE_STATE	psState,
									  PCODEBLOCK			psBlock,
									  IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: ScheduleForPredRegPressureBP
    
 PURPOSE	: Schedule instructions in a block to minimise predicate register pressure.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block in which to find instructions.
			  pvNULL	- not used.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE	psDepState;	
	PINST			psBestInst, psInst;
	PARG			psLiveReg[4];
	IMG_UINT32		uIndex;
	IMG_BOOL		bRegLive;

	PVR_UNREFERENCED_PARAMETER(pvNULL);
	
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE);
	ClearBlockInsts(psState, psBlock);

	for (uIndex = 0; uIndex < 4; ++uIndex)
	{
		psLiveReg[uIndex] = NULL;
	}

	bRegLive = IMG_FALSE;
	
	for (psBestInst = GetNextAvailable(psDepState, NULL); psBestInst != NULL; psBestInst = GetNextAvailable(psDepState, NULL))
	{
		IMG_UINT32  uDest;
		IMG_BOOL	bFoundOne, bNotMatchingLiveReg;

		bFoundOne = IMG_TRUE;
		bNotMatchingLiveReg = IMG_FALSE;

		if (bRegLive)
		{
			/* Check whether the next instruction uses the live predicate register */
			for (uDest = 0; uDest < psBestInst->uDestCount; ++uDest)
			{
				if (psBestInst->asDest[uDest].uType == USEASM_REGTYPE_PREDICATE)
				{
					bFoundOne = IMG_FALSE;
					bNotMatchingLiveReg = IMG_TRUE;
				}
			}

			if (psBestInst->uPredCount != 0)
			{
				IMG_UINT32	uPred;
				IMG_BOOL	bMatchesALiveReg = IMG_FALSE;

				for (uPred = 0; uPred < psBestInst->uPredCount; ++uPred)
				{
					if (psBestInst->apsPredSrc[uPred] == NULL)
					{
						continue;
					}

					for (uIndex = 0; uIndex < 4; ++uIndex)
					{
						if (psLiveReg[uIndex] != NULL && psBestInst->apsPredSrc[uPred]->uNumber == psLiveReg[uIndex]->uNumber)
						{
							bMatchesALiveReg = IMG_TRUE;
						}
					}
				}

				if (!bMatchesALiveReg)
				{
					bNotMatchingLiveReg = IMG_TRUE;
					bFoundOne = IMG_FALSE;
				}
			}
		}

		/* If so, output that instruction next. */
		if (bNotMatchingLiveReg)
		{
			/* If the next instruction doesn't use the live predicate register, find one which does. */
			IMG_BOOL bFoundPredicatedInst = IMG_FALSE;

			for (psInst = GetNextAvailable(psDepState, psBestInst); psInst != NULL; psInst = GetNextAvailable(psDepState, psInst))
			{
				IMG_UINT32 uPred;
				
				if (!ShouldInsertInstNow(psState, psBlock, psInst))
				{
					continue;
				}

				for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
				{
					if (psInst->apsPredSrc[uPred] == NULL)
					{
						continue;
					}

					for (uIndex = 0; uIndex < 4; ++uIndex)
					{
						if (psLiveReg[uIndex] != NULL && 
							psInst->apsPredSrc[uPred]->uNumber == psLiveReg[uIndex]->uNumber)
						{
							psBestInst = psInst;
							bFoundPredicatedInst = IMG_TRUE;
							bFoundOne = IMG_TRUE;
							break;
						}
					}
				}

				if (bFoundPredicatedInst)
				{
					break;
				}
			}

			if (!bFoundPredicatedInst)
			{
				/*
					If no instruction can be output that uses the live register, try to find one which is
					a requirement of an instruction that is.
				*/
				IMG_BOOL bFoundRequiredInst = IMG_FALSE;
				for (psInst = GetNextAvailable(psDepState, NULL); psInst != NULL; psInst = GetNextAvailable(psDepState, psInst))
				{
					if (!ShouldInsertInstNow(psState, psBlock, psInst))
					{
						continue;
					}

					for (uIndex = 0; uIndex < 4; ++uIndex)
					{
						if (psLiveReg[uIndex] != NULL)
						{
							PUSC_LIST_ENTRY	psUseListEntry;
							PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, psLiveReg[uIndex]->uType, psLiveReg[uIndex]->uNumber);

							for (psUseListEntry = psUseDef->sList.psHead; 
								psUseListEntry != NULL;
								psUseListEntry = psUseListEntry->psNext)
							{
								PINST	psUseInst;
								PUSEDEF	psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

								if (psUse == psUseDef->psDef)
								{
									continue;
								}

								psUseInst = psUse->u.psInst;

								if (GraphGet(psState, psDepState->psClosedDepGraph, psUseInst->uId, psInst->uId))
								{
									psBestInst = psInst;
									bFoundRequiredInst = IMG_TRUE;
									bFoundOne = IMG_TRUE;
									break;
								}
							}

							if (bFoundRequiredInst)
							{
								break;
							}
						}
					}

					if (bFoundRequiredInst)
					{
						break;
					}
				}
			}
		}

		/*
			If we have not found an instruction that matches the live predicate register
			output one which does not make any more live, or one whose dependent instructions
			can all be output now without making more predicate registers live.
		*/
		if ((!bRegLive || !bFoundOne) &&
			GetNextAvailable(psDepState, psBestInst) != NULL)
		{
			for (psInst = psBestInst; psInst != NULL; psInst = GetNextAvailable(psDepState, psInst))
			{
				IMG_BOOL bUpdatesPredReg = IMG_FALSE;
				
				if (!ShouldInsertInstNow(psState, psBlock, psInst))
				{
					continue;
				}

				for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
				{
					if (psInst->asDest[uDest].uType == USEASM_REGTYPE_PREDICATE)
					{
						bUpdatesPredReg = IMG_TRUE;
						break;
					}
				}

				if (!bUpdatesPredReg || CouldOutputAllDependentInsts(psState, psDepState, psInst))
				{
					psBestInst = psInst;
					break;
				}
			}
		}
		
		for (uDest = 0; uDest < psBestInst->uDestCount; ++uDest)
		{
			if (psBestInst->asDest[uDest].uType == USEASM_REGTYPE_PREDICATE)
			{
				psLiveReg[uDest] = &psBestInst->asDest[uDest];
				bRegLive = IMG_TRUE;
			}
		}
		
		RemoveInstruction(psDepState, psBestInst);
		InsertInstBefore(psState, psBlock, psBestInst, NULL);
	}

	FreeBlockDGraphState(psState, psBlock);
}


IMG_INTERNAL
IMG_VOID ScheduleForPredRegPressure(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ScheduleForPredRegPressure

 PURPOSE	: Schedule instructions to minimise predicate register pressure

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the optimisation to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, ScheduleForPredRegPressureBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Schedule for pred reg pressure", "pred_reg_scheduling", IMG_FALSE);
}


IMG_INTERNAL 
IMG_VOID GenerateNonEfoInstructionsBP(PINTERMEDIATE_STATE	psState,
									  PCODEBLOCK			psBlock,
									  IMG_PVOID				pvNull)
/*****************************************************************************
 FUNCTION	: GenerateNonEfoInstructionsBP

 PURPOSE	: Try to optimize a basic block by generating MSA and ADM
			  instructions from other arithmetic instructions.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Basic block to optimize.
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uInst;
	IMG_BOOL		bChanges;
	IMG_UINT32		uNextInst;
	IMG_BOOL		bExtraMoves;
	IMG_BOOL		bExtraF16Moves;
	PDGRAPH_STATE	psDepState;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	do
	{
		IMG_BOOL		bDroppedTemporaries;
		PINST			psFirstInst;
		USC_LIST		sNewMoveList;
		IMG_BOOL		bRecheckSourceModifiers;

		psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
		ComputeClosedDependencyGraph(psDepState, IMG_FALSE);

		bChanges = IMG_FALSE;
		bExtraMoves = IMG_FALSE;
		bExtraF16Moves = IMG_FALSE;
		InitializeList(&sNewMoveList);
		bDroppedTemporaries = IMG_FALSE;
		bRecheckSourceModifiers = IMG_FALSE;
	
		/* Combine instructions into MSA or ADM instructions. */
		for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
		{
			PINST		psInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uInst);
			PINST		psNextInst;
			IMG_BOOL	bMergeDependencies;
			const IMG_UINT32 uInstMainDep = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, 
																 				psDepState->psMainDep,
																 				uInst);

			if (psInst == NULL ||
				uInstMainDep == USC_MAINDEPENDENCY_NONE ||
				uInstMainDep == USC_MAINDEPENDENCY_MULTIPLE)
			{
				continue;
			}

			uNextInst = uInstMainDep;

			psNextInst = ArrayGet(psState, psDepState->psInstructions, uNextInst);
	
			if (psNextInst == NULL)
			{
				continue;
			}

			bMergeDependencies = OptimizeInstructionSequence(psState,
															 psInst, 
															 psNextInst, 
															 &sNewMoveList,
															 &bExtraMoves,
															 &bExtraF16Moves,
															 &bDroppedTemporaries, 
															 &bRecheckSourceModifiers);
			if (bMergeDependencies)
			{
				MergeInstructions(psDepState, uInst, uNextInst, IMG_TRUE);
				RemoveInst(psState, psBlock, psNextInst);
				FreeInst(psState, psNextInst);
				bChanges = IMG_TRUE;
			}
		}

		/*
			Look for cases where we can do conversions from U8/S8/O8 -> F32 in parallel.
		*/
		if (!(bChanges || bExtraMoves || bExtraF16Moves))
		{
			CombineF32PacksBlock(psState, psBlock, psDepState);
		}

		/*
			Recontruct the block.
		*/
		ClearBlockInsts(psState, psBlock);
			
		while ((psFirstInst = GetNextAvailable(psDepState, NULL)) != NULL)
		{
			InsertInstBefore(psState, 
							 psBlock, 
							 psFirstInst, 
							 NULL);

			RemoveInstruction(psDepState, psFirstInst);
		}
	
		/* Release graph memory */
		FreeBlockDGraphState(psState, psBlock);

		if (bChanges)
		{
			if (bDroppedTemporaries)
			{
				UseDefDropUnusedTemps(psState);
			}
		}

		if (bRecheckSourceModifiers)
		{
			SAFE_LIST_ITERATOR	sIter;

			InstListIteratorInitialize(psState, IFMOV, &sIter);
			for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
			{
				PINST	psFMOVInst;

				psFMOVInst = InstListIteratorCurrent(&sIter);
				AppendToList(&sNewMoveList, &psFMOVInst->sTempListEntry);
			}
			InstListIteratorFinalise(&sIter);
		}
		EliminateMovesFromList(psState, &sNewMoveList);

		if (!bExtraF16Moves)
		{
			bExtraF16Moves = PromoteFARITH16ToF32(psState, psBlock);
		}

		if (bExtraF16Moves)
		{
			EliminateF16MovesBP(psState, psBlock, NULL);
		}
	}
	while (bChanges || bExtraMoves || bExtraF16Moves);
}

static IMG_BOOL HasSourceModifier(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: HasSourceModifier

 PURPOSE	: Checks if an argument has a source modifier that means it can't
			  be a source to a move.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.	
			  uArgIdx		- Index of the argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG psArg = &psInst->asArg[uArgIdx];

	if (HasNegateOrAbsoluteModifier(psState, psInst, uArgIdx))
	{
		return IMG_TRUE;
	}
	if (HasF16F32SelectInst(psInst) && psArg->eFmt == UF_REGFORMAT_F16)
	{
		return IMG_TRUE;
	}
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_FARITH16)
	{
		if (psInst->u.psArith16->aeSwizzle[uArgIdx] != FMAD16_SWIZZLE_LOWHIGH)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

typedef enum
{
	ARITHMETIC_PRECISION_F32 = 0,
	ARITHMETIC_PRECISION_F16,
	ARITHMETIC_PRECISION_COUNT
} ARITHMETIC_PRECISION, *PARITHMETIC_PRECISION;

typedef enum
{
	ARITHMETIC_OP_MOV = 0,
	ARITHMETIC_OP_ADD,
	ARITHMETIC_OP_SUB,
	ARITHMETIC_OP_MUL,
	ARITHMETIC_OP_MAD,
	ARITHMETIC_OP_COUNT,
	ARITHMETIC_OP_UNDEF
} ARITHMETIC_OP;

typedef enum
{
	ARITHMETIC_CONSTANT_ZERO,
	ARITHMETIC_CONSTANT_ONE,
	ARITHMETIC_CONSTANT_MINUSONE,
	ARITHMETIC_CONSTANT_TWO,
	ARITHMETIC_CONSTANT_COUNT,
	ARITHMETIC_CONSTANT_UNDEF
} ARITHMETIC_CONSTANT;

typedef struct _ARITHMETIC_ARGUMENT
{
	IMG_UINT32				uOrigArgIdx;
	IMG_BOOL				bNegate;
	ARITHMETIC_CONSTANT		eConstant;
} ARITHMETIC_ARGUMENT, *PARITHMETIC_ARGUMENT;

static ARITHMETIC_OP SimplifyArithmeticOp(ARITHMETIC_OP eOp, PARITHMETIC_ARGUMENT apsArgs[])
/*****************************************************************************
 FUNCTION	: SimplifyArithmeticOp

 PURPOSE	: Apply simple arithmetic rules to simplify an expression.

 PARAMETERS	: eOp			- The current operation.
			  apsArgs		- Information about any constant arguments
							to the operation.

 RETURNS	: The simplified operation.
*****************************************************************************/
{
	if (eOp == ARITHMETIC_OP_MAD)
	{
		/* A*B+0 -> A*B */
		if (apsArgs[2]->eConstant == ARITHMETIC_CONSTANT_ZERO)
		{
			eOp = ARITHMETIC_OP_MUL;
		}
		/* A*0+B/0*A+B -> B */
		else if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ZERO || apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ZERO)
		{
			eOp = ARITHMETIC_OP_MOV;
			apsArgs[0] = apsArgs[2];
		}
		/* 1*A+B -> A+B  / -1*A+B -> (-A)+B */
		else if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ONE || apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
		{
			eOp = ARITHMETIC_OP_ADD;
			if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
			{
				apsArgs[1]->bNegate = (!apsArgs[1]->bNegate) ? IMG_TRUE : IMG_FALSE;
			}
			apsArgs[0] = apsArgs[1];
			apsArgs[1] = apsArgs[2];
		}
		/* A*1+B -> A+B  / A*-1+B -> (-A)+B */
		else if (apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ONE || apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
		{
			eOp = ARITHMETIC_OP_ADD;
			if (apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
			{
				apsArgs[0]->bNegate = (!apsArgs[0]->bNegate) ? IMG_TRUE : IMG_FALSE;
			}
			apsArgs[1] = apsArgs[2];
		}
	}

	if (eOp == ARITHMETIC_OP_ADD || eOp == ARITHMETIC_OP_SUB)
	{
		/* A+0 -> A */
		if (apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ZERO)
		{
			eOp = ARITHMETIC_OP_MOV;
		}
		/* 0+A -> A */
		else if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ZERO)
		{
			apsArgs[0] = apsArgs[1];
			if (eOp == ARITHMETIC_OP_SUB)
			{
				apsArgs[0]->bNegate = (!apsArgs[0]->bNegate) ? IMG_TRUE : IMG_FALSE;
			}
			eOp = ARITHMETIC_OP_MOV;
		}
		/* 1+1 -> 2 */
		else if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ONE && 
				 apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ONE &&
				 eOp == ARITHMETIC_OP_ADD)
		{
			eOp = ARITHMETIC_OP_MOV;
			apsArgs[0]->uOrigArgIdx = USC_UNDEF;
			apsArgs[0]->eConstant = ARITHMETIC_CONSTANT_TWO;
			apsArgs[0]->bNegate = IMG_FALSE;
		}
	}

	if (eOp == ARITHMETIC_OP_MUL)
	{		
		/* 0*A/A*0 -> 0 */
		if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ZERO || apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ZERO)
		{
			eOp = ARITHMETIC_OP_MOV;
			apsArgs[0]->uOrigArgIdx = USC_UNDEF;
			apsArgs[0]->eConstant = ARITHMETIC_CONSTANT_ZERO;
			apsArgs[0]->bNegate = IMG_FALSE;
		}
		/* 1*A -> A */
		else if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_ONE || apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
		{
			eOp = ARITHMETIC_OP_MOV;
			if (apsArgs[0]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
			{
				apsArgs[1]->bNegate = (!apsArgs[1]->bNegate) ? IMG_TRUE : IMG_FALSE;
			}
			apsArgs[0] = apsArgs[1];
		}
		/* A*1 -> A */
		else if (apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_ONE || apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
		{
			eOp = ARITHMETIC_OP_MOV;
			if (apsArgs[1]->eConstant == ARITHMETIC_CONSTANT_MINUSONE)
			{
				apsArgs[0]->bNegate = (!apsArgs[0]->bNegate) ? IMG_TRUE : IMG_FALSE;
			}
		}
	}	

	return eOp;
}

static ARITHMETIC_OP GetArithmeticOpType(IOPCODE eOpcode, PARITHMETIC_PRECISION pePrec)
/*****************************************************************************
 FUNCTION	: GetArithmeticOpType

 PURPOSE	: Convert an instruction opcode to an arithmetic operation type.

 PARAMETERS	: eOpcode		- Opcode.
			  pePrec		- Returns the precision of the operation.

 RETURNS	: The operation.
*****************************************************************************/
{
	switch (eOpcode)
	{
		case IFMUL:		*pePrec = ARITHMETIC_PRECISION_F32; return ARITHMETIC_OP_MUL;
		case IFMUL16:	*pePrec = ARITHMETIC_PRECISION_F16; return ARITHMETIC_OP_MUL;
		case IFADD:		*pePrec = ARITHMETIC_PRECISION_F32; return ARITHMETIC_OP_ADD;
		case IFSUB:		*pePrec = ARITHMETIC_PRECISION_F32; return ARITHMETIC_OP_SUB;
		case IFADD16:	*pePrec = ARITHMETIC_PRECISION_F16; return ARITHMETIC_OP_ADD;
		case IFMAD:		*pePrec = ARITHMETIC_PRECISION_F32; return ARITHMETIC_OP_MAD;
		case IFMAD16:	*pePrec = ARITHMETIC_PRECISION_F16; return ARITHMETIC_OP_MAD;
		default:											return ARITHMETIC_OP_UNDEF;
	}
}

static ARITHMETIC_CONSTANT GetConstant(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uArg, IMG_UINT32 uOneConstNum)
/*****************************************************************************
 FUNCTION	: GetConstant

 PURPOSE	: Checks for a constant instruction argument.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uArg			- Argument to check.
			  uOneConstNum	- Number of the hardware constant which contains
							one for the instruction's operation type.

 RETURNS	: The constant.
*****************************************************************************/
{
	PARG	psArg = &psInst->asArg[uArg];

	if (psArg->uType == USEASM_REGTYPE_FPCONSTANT && psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO)
	{
		return ARITHMETIC_CONSTANT_ZERO;
	}
	else if (psArg->uType == USEASM_REGTYPE_IMMEDIATE && psArg->uNumber == 0)
	{
		return ARITHMETIC_CONSTANT_ZERO;
	}
	else if (psArg->uType == USEASM_REGTYPE_FPCONSTANT && psArg->uNumber == uOneConstNum)
	{
		if (IsNegated(psState, psInst, uArg))
		{
			return ARITHMETIC_CONSTANT_MINUSONE;
		}
		else
		{
			return ARITHMETIC_CONSTANT_ONE;
		}
	}
	return ARITHMETIC_CONSTANT_UNDEF;
}

static IMG_BOOL SimplifyInst(PINTERMEDIATE_STATE	psState, 
							 PINST					psInst)
/*****************************************************************************
 FUNCTION	: SimplifyInst

 PURPOSE	: Checks if an arithmetic instruction could be simplified.

 PARAMETERS	: psState			- Compiler state
			  psInst			- Instruction to try and simplify.

 RETURNS	: TRUE if the arithmetic instruction was converted to a move.
*****************************************************************************/
{
	IMG_UINT32				uArg;
	IMG_UINT32				uOneConstNum;
	IMG_BOOL				bNewMoves;
	PARITHMETIC_ARGUMENT	apsArgs[USC_MAXIMUM_NONCALL_ARG_COUNT];
	ARITHMETIC_ARGUMENT		asArgs[USC_MAXIMUM_NONCALL_ARG_COUNT];
	IMG_BOOL				bImmediateArgs;
	ARITHMETIC_OP			eOpType;
	ARITHMETIC_OP			eNewOpType;
	ARITHMETIC_PRECISION	eOpPrecision;
	IOPCODE					eNewOpcode;
	IOPCODE					eOldOpcode;
	IMG_UINT32				uUsedSourceMask;
	IMG_UINT32				uOldArgCount;
	IMG_UINT32				uNewArgCount;
	static const IOPCODE	aeNewOpcode[ARITHMETIC_OP_COUNT][ARITHMETIC_PRECISION_COUNT] = 
	{
								/* ARITHMETIC_PRECISION_F32		ARITHMETIC_PRECISION_F16 */
		/* ARITHMETIC_OP_MOV */	{			IFMOV,					IFMOV16,				},
		/* ARITHMETIC_OP_ADD */	{			IFADD,					IFADD16,				},
		/* ARITHMETIC_OP_SUB */	{			IINVALID,				IINVALID,				},
		/* ARITHMETIC_OP_MUL */	{			IFMUL,					IFMUL16,				},
		/* ARITHMETIC_OP_MAD */	{			IFMAD,					IFMAD16,				},
	};
	static const struct
	{
		IMG_UINT32	uType;
		IMG_UINT32	uNumber;
	} asConsts[ARITHMETIC_CONSTANT_COUNT][ARITHMETIC_PRECISION_COUNT] =
	{
		/* ARITHMETIC_CONSTANT_ZERO */		
		{
			{USEASM_REGTYPE_FPCONSTANT, EURASIA_USE_SPECIAL_CONSTANT_ZERO},			/* ARITHMETIC_PRECISION_F32 */
			{USEASM_REGTYPE_FPCONSTANT, EURASIA_USE_SPECIAL_CONSTANT_ZERO},			/* ARITHMETIC_PRECISION_F16 */
		},
		/* ARITHMETIC_CONSTANT_ONE */
		{
			{USEASM_REGTYPE_FPCONSTANT, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1},		/* ARITHMETIC_PRECISION_F32 */
			{USEASM_REGTYPE_FPCONSTANT, EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE},	/* ARITHMETIC_PRECISION_F16 */
		},
		/* ARITHMETIC_CONSTANT_MINUSONE */
		{
			{USEASM_REGTYPE_IMMEDIATE,	FLOAT32_MINUSONE},							/* ARITHMETIC_PRECISION_F32 */
			{USEASM_REGTYPE_IMMEDIATE,	FLOAT16_MINUSONE_REPLICATED},				/* ARITHMETIC_PRECISION_F16 */
		},
		/* ARITHMETIC_CONSTANT_TWO */
		{
			{USEASM_REGTYPE_FPCONSTANT, EURASIA_USE_SPECIAL_CONSTANT_FLOAT2},		/* ARITHMETIC_PRECISION_F32 */
			{USEASM_REGTYPE_IMMEDIATE,	FLOAT16_TWO_REPLICATED},					/* ARITHMETIC_PRECISION_F16 */
		},
	};

	/*
		Convert the instruction's opcode to an arithmetic operation type (ADD, MUL, etc).
	*/
	if (psInst->eOpcode == ITESTPRED || psInst->eOpcode == ITESTMASK)
	{
		eOldOpcode = psInst->u.psTest->eAluOpcode;
	}
	else
	{
		eOldOpcode = psInst->eOpcode;
	}
	eOpType = GetArithmeticOpType(eOldOpcode, &eOpPrecision);
	if (eOpType == ARITHMETIC_OP_UNDEF)
	{
		return IMG_FALSE;
	}

	if (eOpPrecision == ARITHMETIC_PRECISION_F16)
	{
		uOneConstNum = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
	}
	else
	{
		uOneConstNum = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
	}

	uOldArgCount = psInst->uArgumentCount;

	/*
		Check if any arguments to the instruction are immediate values.
	*/
	bImmediateArgs = IMG_FALSE;
	ASSERT(uOldArgCount <= (sizeof(asArgs) / sizeof(asArgs[0])));
	for (uArg = 0; uArg < uOldArgCount; uArg++)
	{
		asArgs[uArg].eConstant = GetConstant(psState, psInst, uArg, uOneConstNum);
		if (asArgs[uArg].eConstant != ARITHMETIC_CONSTANT_UNDEF)
		{
			bImmediateArgs = IMG_TRUE;
		}
		asArgs[uArg].uOrigArgIdx = uArg;
		asArgs[uArg].bNegate = IMG_FALSE;

		apsArgs[uArg] = &asArgs[uArg];
	}

	if (!bImmediateArgs)
	{
		return IMG_FALSE;
	}

	/*
		Apply simple arithmetic rules to simplify the expression.
	*/
	eNewOpType = SimplifyArithmeticOp(eOpType, apsArgs);
	if (eNewOpType == eOpType)
	{
		return IMG_FALSE;
	}

	/*
		Convert the simplified operation type back to an instruction opcode.
	*/
	ASSERT(eNewOpType < ARITHMETIC_OP_COUNT);
	eNewOpcode = aeNewOpcode[eNewOpType][eOpPrecision];
	ASSERT(eNewOpcode != IINVALID);

	uNewArgCount = g_psInstDesc[eNewOpcode].uDefaultArgumentCount;

	/*
		Check which sources to the expressions are still used in the simplified form.
	*/
	uUsedSourceMask = 0;
	for (uArg = 0; uArg < uNewArgCount; uArg++)
	{
		PARITHMETIC_ARGUMENT	psArithArg = apsArgs[uArg];
		
		if (psArithArg->uOrigArgIdx != USC_UNDEF)
		{
			uUsedSourceMask |= (1U << uArg);
		}
	}

	/*
		Create a new array of instruction arguments.
	*/
	for (uArg = 0; uArg < uNewArgCount; uArg++)
	{
		PARITHMETIC_ARGUMENT	psArithArg = apsArgs[uArg];

		if (psArithArg->uOrigArgIdx != uArg)
		{
			if (psArithArg->uOrigArgIdx == USC_UNDEF)
			{
				IMG_UINT32		uArgType;
				IMG_UINT32		uArgNumber;
				UF_REGFORMAT	eArgFmt;

				ASSERT(psArithArg->eConstant < (sizeof(asConsts) / sizeof(asConsts[0])));
	
				uArgType = asConsts[psArithArg->eConstant][eOpPrecision].uType;
				uArgNumber = asConsts[psArithArg->eConstant][eOpPrecision].uNumber;
				eArgFmt = (eOpPrecision == ARITHMETIC_PRECISION_F32) ? UF_REGFORMAT_F32 : UF_REGFORMAT_F16;

				SetSrc(psState, psInst, uArg, uArgType, uArgNumber, eArgFmt);
			}
			else 
			{
				ASSERT(psArithArg->uOrigArgIdx > uArg);
				MoveSrcAndModifiers(psState, psInst, uArg, psInst, psArithArg->uOrigArgIdx);
			}
		}
	}

	/*
		Update the instruction's opcode.
	*/
	if (psInst->eOpcode == ITESTPRED || psInst->eOpcode == ITESTMASK)
	{
		psInst->u.psTest->eAluOpcode = eNewOpcode;

		/*
			Apply a negate modifier by changing the test type e.g.
				-X <= 0		->		X >= 0
		*/
		ASSERT(uNewArgCount == 1);
		if (apsArgs[0]->bNegate)
		{
			IMG_BOOL	bRet;

			bRet = NegateTest(psInst->u.psTest->sTest.eType, &psInst->u.psTest->sTest.eType);
			ASSERT(bRet);
		}
	}
	else
	{
		ModifyOpcode(psState, psInst, eNewOpcode);

		for (uArg = 0; uArg < uNewArgCount; uArg++)
		{
			if (apsArgs[uArg]->bNegate)
			{
				InvertNegateModifier(psState, psInst, uArg);
			}
		}
	}

	/*
		Check if simplifying the expression has generated a straight copy of a source
		argument. We may now be able to replace the destination by the source argument and
		remove the instruction altogether.
	*/
	bNewMoves = IMG_FALSE;
	if (eNewOpType == ARITHMETIC_OP_MOV)
	{
		bNewMoves = IMG_TRUE;
	}

	/*
		Convert a move with source modifier to an ordinary MOV if the
		source modifier has disappeared.
	*/
	if (
			bNewMoves &&
			(
				psInst->eOpcode == IFMOV || 
				psInst->eOpcode == IFMOV16
			) &&
			(
				!HasSourceModifier(psState, psInst, 0 /* uArgIdx */) ||
				(
					psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
					psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
				)
			)
	   )
	{
		SetOpcode(psState, psInst, IMOV);
	}

	return bNewMoves;
}

static IMG_BOOL CanChangeTestForSourceMod(PINTERMEDIATE_STATE psState,
										  PINST psTestInst,
										  IMG_BOOL bNegate,
										  IMG_BOOL bAbsolute)
/*****************************************************************************
 FUNCTION	: CanChangeTestForSourceMod

 PURPOSE	: Can an argument which uses a source modifier be substituted into
			  a test instruction by changing the instruction parameter.

 PARAMETERS	: psState		- Internal compiler state
			  psTestInst	- Instruction to substitute into.
			  bNegate		- If TRUE substitute a negate source modifier.
			  bAbsolute		- If TRUE substitute an absolute source modifier.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (bAbsolute)
	{
		return IMG_FALSE;
	}
	ASSERT(bNegate);
	if (
			psTestInst->u.psTest->eAluOpcode == IFADD ||
			psTestInst->u.psTest->eAluOpcode == IFSUB ||
			psTestInst->u.psTest->eAluOpcode == IFMOV
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID ChangeTestForSourceMod(PINTERMEDIATE_STATE psState,
									   PINST psTestInst, 
									   IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: ChangeTestForSourceMod

 PURPOSE	: Change a test instruction when the source modifiers on an argument are
			  changed.

 PARAMETERS	: psState		- Internal compiler state
			  psTestInst	- Instruction to change.
			  uArg			- Argument number that is substituted.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IOPCODE	eAluOpcode;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	ASSERT(g_psInstDesc[psTestInst->eOpcode].eType == INST_TYPE_TEST);
	eAluOpcode = psTestInst->u.psTest->eAluOpcode;
	ASSERT(eAluOpcode == IFADD || eAluOpcode == IFSUB || eAluOpcode == IFMOV);

	if (eAluOpcode == IFMOV)
	{
		ASSERT(uArg == 0);

		psTestInst->u.psTest->eAluOpcode = IFSUB;
		MoveSrc(psState, psTestInst, 1 /* uMoveToSrcIdx */, psTestInst, 0 /* uMoveFromSrcIdx */);
		SetSrc(psState, 
			   psTestInst, 
			   0 /* uSrcIdx */, 
			   USEASM_REGTYPE_FPCONSTANT, 
			   EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
			   UF_REGFORMAT_F32);
	}
	else if (eAluOpcode == IFADD)
	{
		if (uArg == 0)
		{
			CommuteSrc01(psState, psTestInst);
		}
		psTestInst->u.psTest->eAluOpcode = IFSUB;
	}
	else
	{
		ASSERT(eAluOpcode == IFSUB);
		psTestInst->u.psTest->eAluOpcode = IFADD;
		if (uArg == 0)
		{
			IMG_BOOL		bRet;

			/*
				Convert for example -A - B >= 0 -> -(A + B) >= 0 -> (A + B) <= 0
			*/
			bRet = NegateTest(psTestInst->u.psTest->sTest.eType, &psTestInst->u.psTest->sTest.eType);
			ASSERT(bRet);
		}
		else
		{
			/*
				Otherwise convert A - (-B) => A+B
			*/
		}
	}
}

/*****************************************************************************
 FUNCTION	: OverwritesIndexDest

 PURPOSE	: Check if an index used in an argument is overwritten by an
			  instruction's destination.

 PARAMETERS	: psDest		- Instruction destination.
			  psArg			- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL OverwritesIndexDest(PARG psDest, PARG psArg)
{
	if (
			psDest->uType == psArg->uIndexType &&
			psDest->uNumber == psArg->uIndexNumber
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: OverwritesIndex

 PURPOSE	: Check if an index used in an argument is overwritten by an
			  instruction.

 PARAMETERS	: psInst		- Instruction.
			  psArg			- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL OverwritesIndex(PINST psInst, PARG psArg)
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (OverwritesIndexDest(psDest, psArg))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsReplacableDest(PINST		psInst,
								 IMG_UINT32	uDestIdx)
/*****************************************************************************
 FUNCTION	: IsReplacableDest

 PURPOSE	: .

 PARAMETERS	: psInst			- Instruction to check.
			  uDestIdx			- Index of the destination to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (g_psInstDesc[psInst->eOpcode].uFlags & (DESC_FLAGS_TEXTURESAMPLE | DESC_FLAGS_MULTIPLEDEST))
	{
		if (psInst->uDestCount > 1)
		{
			return IMG_FALSE;
		}
	}
	else
	{
		if (uDestIdx > 0)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
PINST GetEarliestInst(PINTERMEDIATE_STATE psState, PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: GetEarliestInst

 PURPOSE	: Get the instruction from a pair which occurs first in a block.

 PARAMETERS	: psState			- Compiler state.
			  psInst1, psInst2	- The instructions to compare.

 RETURNS	: The earlier instruction.
*****************************************************************************/
{
	if (psInst1 == NULL)
	{
		return psInst2;
	}
	if (psInst2 == NULL)
	{
		return psInst1;
	}
	ASSERT(psInst1->psBlock == psInst2->psBlock);
	if (psInst1->uBlockIndex < psInst2->uBlockIndex)
	{
		return psInst1;
	}
	else
	{
		return psInst2;
	}
}

static
PINST GetLatestInst(PINTERMEDIATE_STATE psState, PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: GetLatestInst

 PURPOSE	: Get the instruction from a pair which occurs last in a block.

 PARAMETERS	: psState			- Compiler state.
			  psInst1, psInst2	- The instructions to compare.

 RETURNS	: The later instruction.
*****************************************************************************/
{
	if (psInst1 == NULL)
	{
		return psInst2;
	}
	if (psInst2 == NULL)
	{
		return psInst1;
	}
	ASSERT(psInst1->psBlock == psInst2->psBlock);
	if (psInst1->uBlockIndex > psInst2->uBlockIndex)
	{
		return psInst1;
	}
	else
	{
		return psInst2;
	}
}

typedef struct _NONTEMP_WORK_LIST_ITEM
{
	USC_LIST_ENTRY	sListEntry;
	IMG_UINT32		uTempNum;
} NONTEMP_WORK_LIST_ITEM, *PNONTEMP_WORK_LIST_ITEM;

typedef struct _INST_RANGE
{
	PINST	psFirst;
	PINST	psLast;
} INST_RANGE, *PINST_RANGE;

static
IMG_VOID AddToNonTempWorkList(PINTERMEDIATE_STATE	psState, 
							  PUSC_LIST				psWorkList, 
							  IMG_UINT32			uTempNum)
{
	PUSC_LIST_ENTRY			psListEntry;
	PNONTEMP_WORK_LIST_ITEM	psNewItem;

	for (psListEntry = psWorkList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PNONTEMP_WORK_LIST_ITEM	psItem;

		psItem = IMG_CONTAINING_RECORD(psListEntry, PNONTEMP_WORK_LIST_ITEM, sListEntry);
		if (psItem->uTempNum == uTempNum)
		{
			return;
		}
	}

	psNewItem = UscAlloc(psState, sizeof(*psNewItem));
	psNewItem->uTempNum = uTempNum;
	AppendToList(psWorkList, &psNewItem->sListEntry);
}

static
IMG_VOID UpdateRefRange(PINTERMEDIATE_STATE psState, PINST_RANGE psRange, PINST psRef)
/*****************************************************************************
 FUNCTION	: UpdateRefRange

 PURPOSE	: Expand a range of instructions.

 PARAMETERS	: psState			- Compiler state.
			  psRange			- Range to expand.
			  psRef				- The instruction to add to the range.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRange->psFirst = GetEarliestInst(psState, psRange->psFirst, psRef);
	psRange->psLast = GetLatestInst(psState, psRange->psLast, psRef);
}

static
IMG_BOOL IsValidIndexType(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: IsValidIndexType

 PURPOSE	: Check if an argument can be substituted for an dynamic index.

 PARAMETERS	: psState				- Compiler state.
			  psArg					- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Don't allow a source containing a dynamic index to itself be indexed.
	*/
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	if (
			psArg->uType != USEASM_REGTYPE_TEMP &&
			psArg->uType != USC_REGTYPE_REGARRAY && 
			psArg->uType != USEASM_REGTYPE_IMMEDIATE
		)
	{
		return IMG_FALSE;
	}
	if (IsModifiableRegisterArray(psState, psArg->uType, psArg->uNumber))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL BaseCombineNonTempRegister(PINTERMEDIATE_STATE	psState,
									PCODEBLOCK			psBlock,
									PUSC_LIST			psWorkList,
									PINST				psMovInst,
									PARG				psMovDest,
									IMG_UINT32			uMovSrcRegNum,
									IMG_BOOL			bCheckOnly,
									PINST_RANGE			psRange)
/*****************************************************************************
 FUNCTION	: BaseCombineNonTempRegister

 PURPOSE	: Try to replace a temporary register by a non-temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block within which to do the replacement.
			  psWorkList.
			  psMovDest			- Non-temporary register.
			  uMovSrcRegNum		- Temporary register to replace.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  psRange			- Updating with the range of instructions in the
								block where the non-temporary register is used.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psMovSrcUseDef;
	PUSEDEF			psMovSrcDef;
	PINST			psDefInst;
	IMG_UINT32		uDefDestIdx;
	PARG			psOldDest;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/*
		Get the USE-DEF chain for the move's source.
	*/
	psMovSrcUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uMovSrcRegNum);
	psMovSrcDef = psMovSrcUseDef->psDef;
	if (psMovSrcDef == NULL)
	{
		/*
			The move source is uninitialized.
		*/
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	ASSERT(UseDefIsDef(psMovSrcDef));
	if (psMovSrcDef->eType != DEF_TYPE_INST)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	psDefInst = psMovSrcDef->u.psInst;
	uDefDestIdx = psMovSrcDef->uLocation;

	/*
		Is the non-temporary register suitable as a destination for the instruction which defines
		the move's source?
	*/
	if (!CanUseDest(psState, psDefInst, uDefDestIdx, psMovDest->uType, psMovDest->uIndexType))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if (!IsReplacableDest(psDefInst, uDefDestIdx))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Is the instruction which defines the move's source in a different block.
	*/
	if (psDefInst->psBlock != psBlock)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Is the move's source partially/conditionally written?
	*/
	if (psMovDest->uType != USEASM_REGTYPE_TEMP)
	{
		psOldDest = psDefInst->apsOldDest[uDefDestIdx];
		if (psOldDest != NULL)
		{
			if (psOldDest->uType != USEASM_REGTYPE_TEMP)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			/*
				Also replace the source for the non-written channels by the non-temporary register.
			*/
			AddToNonTempWorkList(psState, psWorkList, psOldDest->uNumber);
		}
	}

	if (!bCheckOnly)
	{
		/*
			Update the instruction which defines the move's source so it now defines the
			non-temporary register.
		*/
		SetDestFromArg(psState, psDefInst, uDefDestIdx, psMovDest);
		psDefInst->auDestMask[uDefDestIdx] = 
			GetMinimalDestinationMask(psState, psDefInst, uDefDestIdx, psDefInst->auDestMask[uDefDestIdx]);
	}

	/*
		Update the range over which the non-temporary register will now be referenced to include the
		instruction defining the move's source.
	*/
	if (psRange != NULL)
	{
		UpdateRefRange(psState, psRange, psDefInst);
	}

	for (psListEntry = psMovSrcUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;
		PINST	psUseInst;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		/*
			Skip the define of the move source.
		*/
		if (psUse == psMovSrcUseDef->psDef)
		{
			continue;
		}

		ASSERT(UseDefIsUse(psUse));

		/*
			Skip the reference to the source register which is in the move instruction itself.
		*/
		if (psUse->eType == USE_TYPE_SRC && psUse->u.psInst == psMovInst)
		{
			continue;
		}

		/*
			Don't replace if the move's source is used in other blocks.
		*/
		if (!(psUse->eType >= USE_TYPE_FIRSTINSTUSE && psUse->eType <= USE_TYPE_LASTINSTUSE))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		psUseInst = psUse->u.psInst;
		if (psUseInst->psBlock != psBlock)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}

		/* Check the move's destination can be substituted by a temporary register used as an index. */
		if (psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_OLDDESTIDX)
		{
			if (!IsValidIndexType(psState, psMovDest))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}

		/*
			Check the non-temporary registers is compatible with source register bank restrictions on
			this use.
		*/
		if (psUse->eType == USE_TYPE_SRC)
		{
			if (!CanUseSrc(psState, psUseInst, psUse->uLocation, psMovDest->uType, psMovDest->uIndexType))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}

		/*
			If the move's source is used as the source for non-written channels in a
			predicated instruction then also replace the destination.
		*/
		if (psUse->eType == USE_TYPE_OLDDEST)
		{
			PARG	psDest;
			
			ASSERT(psUse->uLocation < psUseInst->uDestCount);
			psDest = &psUseInst->asDest[psUse->uLocation];
			if (psDest->uType != USEASM_REGTYPE_TEMP)
			{
				AddToNonTempWorkList(psState, psWorkList, psDest->uNumber);
			}
		}

		/*
			Replace the use of the move's source by the non-temporary register.
		*/
		if (!bCheckOnly)
		{
			SetUseFromArg(psState, psUse, psMovDest);
		}

		/*
			Update the range over which the non-temporary register will now be referenced to include
			the instruction using the move's source.
		*/
		if (psRange != NULL)
		{
			UpdateRefRange(psState, psRange, psUseInst);
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL DoCombineRegistersBackwards(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psBlock,
									 PUSC_LIST				psWorkList,
									 PINST					psMovInst,
									 PARG					psMovDest,
									 IMG_BOOL				bCheckOnly)
{
	INST_RANGE		sRefRange;
	PINST_RANGE		psRefRange;
	PUSC_LIST_ENTRY	psListEntry;

	if (bCheckOnly && psMovDest->uType != USEASM_REGTYPE_TEMP)
	{
		sRefRange.psFirst = sRefRange.psLast = NULL;
		psRefRange = &sRefRange;
	}
	else
	{
		psRefRange = NULL;
	}

	for (psListEntry = psWorkList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PNONTEMP_WORK_LIST_ITEM	psItem;

		psItem = IMG_CONTAINING_RECORD(psListEntry, PNONTEMP_WORK_LIST_ITEM, sListEntry);
		if (!BaseCombineNonTempRegister(psState,
				 					    psBlock,
										psWorkList,
										psMovInst,
										psMovDest,
										psItem->uTempNum,
										bCheckOnly,
										psRefRange))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	if (bCheckOnly && psMovDest->uType != USEASM_REGTYPE_TEMP)
	{
		PINST	psInst;

		psInst = psRefRange->psFirst; 
		for (;;)
		{
			/*
				Check for overwriting the non-temporary register.
			*/
			if (psInst != psMovInst && GetChannelsWrittenInArg(psInst, psMovDest, NULL) != 0)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			/*
				Check for overwriting any index used in the non-temporary register.	
			*/
			if (OverwritesIndex(psInst, psMovDest))
			{	
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}	

			if (psInst == psRefRange->psLast)
			{
				break;
			}
			psInst = psInst->psNext;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL CombineRegistersBackwards(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   PINST				psMovInst,
								   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: CombineRegistersBackwards

 PURPOSE	: Try to replace a move source by a move destination; where the move
			  destination might not be a temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block within which to do the replacement.
			  psMovDest			- Non-temporary register.
			  psMovSrc			- Temporary register to replace.
			  bCheckOnly		- If TRUE just check if replacement is possible.

 RETURNS	: TRUE if replacement is possible.
*****************************************************************************/
{
	USC_LIST		sWorkList;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;
	IMG_BOOL		bRet;
	ARG				sMovDest;
	PARG			psMovDest;
	PARG			psMovSrc = &psMovInst->asArg[0];

	if (!NoPredicate(psState, psMovInst))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	sMovDest = psMovInst->asDest[0];
	psMovDest = &sMovDest;
	if (!bCheckOnly)
	{
		SetDestUnused(psState, psMovInst, 0 /* uDestIdx */);
	}
	
	if (psMovSrc->uType != USEASM_REGTYPE_TEMP)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if (psMovDest->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	InitializeList(&sWorkList);
	AddToNonTempWorkList(psState, &sWorkList, psMovSrc->uNumber);

	bRet = DoCombineRegistersBackwards(psState, psBlock, &sWorkList, psMovInst, psMovDest, bCheckOnly);

	for (psListEntry = sWorkList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PNONTEMP_WORK_LIST_ITEM	psItem;

		psNextListEntry = psListEntry->psNext;
		psItem = IMG_CONTAINING_RECORD(psListEntry, PNONTEMP_WORK_LIST_ITEM, sListEntry);
		UscFree(psState, psItem);
	}

	if (!bCheckOnly)
	{
		RemoveInst(psState, psBlock, psMovInst);
		FreeInst(psState, psMovInst);
	}

	return bRet;
}

static IMG_VOID SetPredicateSourceToFixedValue(PINTERMEDIATE_STATE	psState, 
											   PINST				psInst, 
											   IMG_BOOL				bFixedValue,
											   PWEAK_INST_LIST		psEvalList)
/*****************************************************************************
 FUNCTION	: SetPredicateSourceToFixedValue

 PURPOSE	: Update an instruction when it's predicate is known to have a
			  fixed value.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to modify.
			  bFixedValue	- Fixed value of the predicate source.
			  psEvalList	- Append modified/new instructions to this list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	/*
		Apply the predicate negation flag on the instruction.
	*/
	if (GetBit(psInst->auFlag, INST_PRED_NEG))
	{
		bFixedValue = (!bFixedValue) ? IMG_TRUE : IMG_FALSE;
	}

	if (bFixedValue)
	{
		/*
			Set the instruction as unconditionally executeed.
		*/
		ClearPredicates(psState, psInst);

		/*
			Clear any references to the previous values of conditionally overwritten
			destinations.
		*/
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			if (GetPreservedChansInPartialDest(psState, psInst, uDestIdx) == 0)
			{
				SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psPartialDest */);
			}
		}

		if (psEvalList != NULL)
		{
			/*
				Reconsider the instruction for move elimination now that it has no predicate.
			*/
			AppendToEvalList(psState, psEvalList, psInst);
		}
	}
	else
	{
		PINST	psPrevInst = psInst->psPrev;

		/*	
			Convert the instruction to a series of moves copying the overwritten value of
			the destinations to the original instruction destinations.
		*/
		CopyPartiallyOverwrittenData(psState, psInst);

		if (psEvalList != NULL)
		{
			PINST	psMovInst;

			/*
				Consider the new move instructions for move elimination.
			*/
			psMovInst = psPrevInst != NULL ? psPrevInst->psNext : psInst->psBlock->psBody;
			for (; psMovInst != psInst; psMovInst = psMovInst->psNext)
			{
				AppendToEvalList(psState, psEvalList, psMovInst);
			}
		}

		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
	}
}

static IMG_BOOL NegateAtDefiningTest(PINTERMEDIATE_STATE	psState, 
									 PUSEDEF_CHAIN			psPredUseDef, 
									 PINST					psUseInst,
									 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: NegateAtDefiningTest

 PURPOSE	: Try to modify the instruction defining a predicate so the logical
			  negation of the previous value is generated.

 PARAMETERS	: psState		- Compiler state.
			  psPredUseDef	- Uses of the predicate to negate.
			  psUseInst		- Instruction where the predicate is used.
			  bCheckOnly	- If TRUE just check if the modifications are 
							possible.

 RETURNS	: TRUE if the defining instruction was modified.
*****************************************************************************/
{
	PINST			psDefInst;
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uNonPredDestCount;

	/*
		Get the instruction which defines the predicate.
	*/
	psDefInst = UseDefGetDefInstFromChain(psPredUseDef, NULL /* puDestIdx */);
	if (psDefInst == NULL)
	{
		return IMG_FALSE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psDefInst->eOpcode == IVTEST)
	{
		IMG_UINT32	uPredDestCount;
		IMG_UINT32	uDest;

		uPredDestCount = 0;
		uNonPredDestCount = 0;
		for (uDest = 0; uDest < psDefInst->uDestCount; uDest++)
		{
			PARG	psDest = &psDefInst->asDest[uDest];
			if (psDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				uPredDestCount++;
			}
			else if (psDest->uType != USC_REGTYPE_UNUSEDDEST)
			{
				uNonPredDestCount++;
			}
		}
		/*	
			Don't modify the instruction if it defines several
			predicates.
		*/
		if (uPredDestCount != 1)
		{
			return IMG_FALSE;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (psDefInst->eOpcode == ITESTPRED)
	{
		if (psDefInst->uDestCount == TEST_MAXIMUM_DEST_COUNT)
		{	
			uNonPredDestCount = 1;
		}
		else
		{
			ASSERT(psDefInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT);
			uNonPredDestCount = 0;
		}
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check that the predicate is only used in one instruction.
	*/
	for (psListEntry = psPredUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psPredUseDef->psDef)
		{
			continue;
		}
		if (!UseDefIsInstUseDef(psUse))
		{
			return IMG_FALSE;
		}
		if (psUse->u.psInst != psUseInst)
		{
			return IMG_FALSE;
		}
	}

	/*
		If the defining instruction is predicated then check whether the source predicate can
		also be negated.
	*/
	if (psDefInst->uPredCount != 0)
	{
		PARG			psOldDest;
		PUSEDEF_CHAIN	psSubPredUseDef;
		IMG_UINT32		uSubPredNum;

		if (psDefInst->uPredCount > 1 || GetPredicatePerChan(psState, psDefInst))
		{
			return IMG_FALSE;
		}

		/*
			Don't modifying the defining instruction's predicate if it also
			controls the update of a temporary register.
		*/
		if (uNonPredDestCount != 0)
		{
			return IMG_FALSE;
		}

		ASSERT(psDefInst->apsPredSrc[0]->uType == USEASM_REGTYPE_PREDICATE);
		uSubPredNum = psDefInst->apsPredSrc[0]->uNumber;

		/*
			Get the source for the conditionally overwritten data is the same
			as the source predicate.
		*/
		psOldDest = psDefInst->apsOldDest[TEST_PREDICATE_DESTIDX];
		if (psOldDest == NULL || !EqualArgs(psOldDest, psDefInst->apsPredSrc[0]))
		{
			return IMG_FALSE;
		}

		/*
			Check if the instruction which defines the other predicate can be modified.
		*/
		psSubPredUseDef = UseDefGet(psState, USEASM_REGTYPE_PREDICATE, uSubPredNum);
		if (!NegateAtDefiningTest(psState, psSubPredUseDef, psDefInst, bCheckOnly))
		{
			return IMG_FALSE;
		}

		/*	
			Invert the negate modifier on the source predicate e.g.

				P0 = A op B
				P1 = if P0 then C op D else P0
				   = (A op B) && (C op D)
			=>
				P0 = not(A op B)
				P1 = if !P0 then not(C op D) else P0
				   = not(A op B) || not(C op D)
				   = not((A op B) && (C op D))
		*/
		if (!bCheckOnly)
		{
			SetBit(psDefInst->auFlag, INST_PRED_NEG, !GetBit(psDefInst->auFlag, INST_PRED_NEG));
		}
	}

	/*
		Change the instruction defining the predicate to generate the logical negation of its current
		result.
	*/
	if (!bCheckOnly)
	{
		LogicalNegateInstTest(psState, psDefInst);
	}

	return IMG_TRUE;
}

static IMG_BOOL EliminatePredicateMove(PINTERMEDIATE_STATE	psState,
									   PINST				psMoveInst,
									   IMG_UINT32			uPredNum,
									   IMG_UINT32			uReplacementPredNum,
									   IMG_BOOL				bReplacementPredNegate,
									   IMG_BOOL				bFixedValue,
									   PWEAK_INST_LIST		psEvalList)
/*****************************************************************************
 FUNCTION	: EliminatePredicateMove

 PURPOSE	: Update an instruction when it's predicate is known to have a
			  fixed value.

 PARAMETERS	: psState		- Compiler state.
			  psMoveInst	- Instruction defining the predicate to replace.
			  uPredNum		- Predicate to replace.
			  uReplacementPredNum
							- If non-USC_UNDEF then predicate to replace by.
							  If USC_UNDEF then replace the predicate by a 
							  fixed value.
			  bReplacementPredNegate
							- Negation modifier for a replacement predicate.
			  bFixedValue	- Fixed value to replace by.
			  psEvalList	- Append new/modified move instructions to this
							list.

 RETURNS	: TRUE if all references to the predicate were replaced.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psPredToReplaceUseDef;
	PUSEDEF_CHAIN	psReplacementPredUseDef;
	PUSEDEF			psPredToReplaceDef;
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;

	psPredToReplaceUseDef = UseDefGet(psState, USEASM_REGTYPE_PREDICATE, uPredNum);

	if (uReplacementPredNum != USC_UNDEF)
	{
		psReplacementPredUseDef = UseDefGet(psState, USEASM_REGTYPE_PREDICATE, uReplacementPredNum);
	}
	else
	{
		ASSERT(!bReplacementPredNegate);
		psReplacementPredUseDef = NULL;
	}

	psPredToReplaceDef = psPredToReplaceUseDef->psDef;
	ASSERT(UseDefIsDef(psPredToReplaceDef));

	if (uReplacementPredNum == USC_UNDEF)
	{
		/*
			We don't support substituting a fixed value for a predicate which is used in
			the driver epilog.
		*/
		for (psListEntry = psPredToReplaceUseDef->sList.psHead; 
			 psListEntry != NULL; 
			 psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

			if (psUse->eType == USE_TYPE_FIXEDREG)
			{
				return IMG_FALSE;
			}	
		}
	}
	else if (bReplacementPredNegate)
	{
		IMG_BOOL	bInvalidModifier;

		/*
			Check for use locations which don't support a negation modifier on
			a predicate source.
		*/
		bInvalidModifier = IMG_FALSE;
		for (psListEntry = psPredToReplaceUseDef->sList.psHead; 
			 psListEntry != NULL; 
			 psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

			if (psUse->eType == USE_TYPE_SRC && psUse->u.psInst->eOpcode != IMOVPRED)
			{
				bInvalidModifier = IMG_TRUE;
				break;
			}
			if (psUse->eType == USE_TYPE_FIXEDREG || psUse->eType == USE_TYPE_OLDDEST)
			{
				bInvalidModifier = IMG_TRUE;
				break;
			}
		}

		if (bInvalidModifier)
		{
			/*
				Check the source predicate is used in only one place (the MOVPRED instruction we are trying
				to remove).
			*/
			if (!IsTwoElementList(&psReplacementPredUseDef->sList))
			{
				return IMG_FALSE;
			}

			/*
				Check if the modifier can be moved into the defining instruction.
			*/
			if (!NegateAtDefiningTest(psState, psReplacementPredUseDef, psMoveInst, IMG_FALSE /* bCheckOnly */))
			{
				return IMG_FALSE;
			}

			/*
				Remove the logical negate modifier on the MOVPRED instruction.
			*/
			bReplacementPredNegate = IMG_FALSE;
		}
	}

	for (psListEntry = psPredToReplaceUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		/*
			Skip the define of the predicate at the MOVPRED instruction.
		*/
		if (psUse == psPredToReplaceUseDef->psDef)
		{
			continue;
		}

		if (uReplacementPredNum == USC_UNDEF)
		{
			switch (psUse->eType)
			{
				case USE_TYPE_COND:
				{
					/*
						note this sets the bBlockStructureChanged flag of the CFG, so calls
						to Dominates etc. will abort, thinking the dominance info needs
						updating - and it does!
					*/
					SwitchConditionalBlockToUnconditional(psState, psUse->u.psBlock, bFixedValue);

					/*
						Modifying the flow control structure might have invalidated the next list entry.
					*/
					psNextListEntry = psPredToReplaceUseDef->sList.psHead;
					break;
				}
				case USE_TYPE_PREDICATE:
				{
					SetPredicateSourceToFixedValue(psState, psUse->u.psInst, bFixedValue, psEvalList);

					/*
						Modifying the instruction might have invalidated the next list entry.
					*/
					psNextListEntry = psPredToReplaceUseDef->sList.psHead;
					break;
				}
				case USE_TYPE_OLDDEST:
				{
					SetPartialDest(psState, 
								   psUse->u.psInst, 
								   psUse->uLocation, 
								   USC_REGTYPE_BOOLEAN, 
								   bFixedValue, 
								   UF_REGFORMAT_F32);
					break;
				}
				case USE_TYPE_SRC:
				{
					SetSrc(psState, psUse->u.psInst, psUse->uLocation, USC_REGTYPE_BOOLEAN, bFixedValue, UF_REGFORMAT_F32);
					break;
				}
				default: imgabort();
			}
		}
		else
		{
			switch (psUse->eType)
			{
				case USE_TYPE_COND:
				{
					PCODEBLOCK	psBlock = psUse->u.psBlock;
					
					ASSERT(psBlock->eType == CBTYPE_COND);

					SetConditionalBlockPredicate(psState, psBlock, uReplacementPredNum);

					/*
						If substituting the logical negation of a predicate then swap the
						true and false successors.
					*/
					if (bReplacementPredNegate)
					{
						ExchangeConditionalSuccessors(psState, psBlock);
					}
					break;
				}
				case USE_TYPE_PREDICATE:
				{
					PINST		psUseInst = psUse->u.psInst;
					IMG_BOOL	bInstPredNegate;

					bInstPredNegate = (GetBit(psUseInst->auFlag, INST_PRED_NEG)) ? IMG_TRUE : IMG_FALSE;
					if (bReplacementPredNegate)
					{
						bInstPredNegate = (!bInstPredNegate) ? IMG_TRUE : IMG_FALSE;
					}

					SetPredicate(psState, psUseInst, uReplacementPredNum, bInstPredNegate);
					break;
				}
				case USE_TYPE_FIXEDREG:
				{
					PFIXED_REG_DATA	psFixedReg = psUse->u.psFixedReg;
					IMG_UINT32		uFixedRegIdx = psUse->uLocation;

					ASSERT(!bReplacementPredNegate);

					UseDefDropFixedRegUse(psState, psFixedReg, uFixedRegIdx);
					psFixedReg->auVRegNum[uFixedRegIdx] = uReplacementPredNum;
					UseDefAddFixedRegUse(psState, psFixedReg, uFixedRegIdx);
					break;
				}
				case USE_TYPE_SRC:
				{
					PINST	psUseInst = psUse->u.psInst;

					SetSrc(psState, 
						   psUseInst, 
						   psUse->uLocation, 
						   USEASM_REGTYPE_PREDICATE, 
						   uReplacementPredNum, 
						   UF_REGFORMAT_F32);
					if (bReplacementPredNegate)
					{
						ASSERT(psUseInst->eOpcode == IMOVPRED);
						psUseInst->u.psMovp->bNegate = (!psUseInst->u.psMovp->bNegate) ? IMG_TRUE : IMG_FALSE;
					}
					break;
				}
				case USE_TYPE_OLDDEST:
				{
					ASSERT(!bReplacementPredNegate);
					SetPartialDest(psState, 
								   psUse->u.psInst, 
								   psUse->uLocation, 
								   USEASM_REGTYPE_PREDICATE, 
								   uReplacementPredNum, 
								   UF_REGFORMAT_F32);
					break;
				}
				default: imgabort();
			}
		}
	}

	return IMG_TRUE;
}

typedef struct
{
	FLOAT_SOURCE_MODIFIER	sSourceMod;
	PWEAK_INST_LIST			psEvalList;
	FMAD16_SWIZZLE			eF16Swizzle;
} ELIMINATE_GLOBAL_MOVE_CONTEXT;

static IMG_BOOL CheckSourceModifierOnArg(PINTERMEDIATE_STATE			psState,
										 PINST							psInst,
										 IMG_UINT32						uNewArg,
										 IMG_UINT32						uOldArg,
										 PSOURCE_VECTOR					psReplaceMask,
										 PFLOAT_SOURCE_MODIFIER			psNewSrcMod)
/*****************************************************************************
 FUNCTION	: CheckSourceModifierOnArg

 PURPOSE	: Check if changing the source modifiers on an argument would still
			  be valid.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to check.
			  uNewArg				- New position of the argument.
			  uOldArg				- Current position of the argument.
			  psReplaceMask			- Mask of instruction sources to modify.
			  psNewSrcMod			- Source modifier to apply before the existing
									modifiers.

 RETURNS	: TRUE if the new modifiers are valid.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psOldMod;
	FLOAT_SOURCE_MODIFIER	sNewMod;

	psOldMod = GetFloatMod(psState, psInst, uOldArg);
	if (psOldMod != NULL)
	{
		sNewMod = *psOldMod;
	}
	else
	{
		sNewMod.bNegate = sNewMod.bAbsolute = IMG_FALSE;
	}

	if (CheckSourceVector(psReplaceMask, uOldArg))
	{
		GetCombinedSourceModifiers(&sNewMod, psNewSrcMod);
	}

	/*
		Check if the destination accepts source modifiers (if that is required).
	*/
	if (psInst->eOpcode == ITESTPRED)
	{
		/*
			TEST instructions can't have source modifiers but check if we can
			accomodate them by changing the ALU operation (e.g. ADD -> SUB)
		*/
		if (!CanChangeTestForSourceMod(psState, psInst, sNewMod.bNegate, sNewMod.bAbsolute))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		if (!CanHaveSourceModifier(psState, psInst, uNewArg, sNewMod.bNegate, sNewMod.bAbsolute))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CheckCommutableSourceModifiers(PINTERMEDIATE_STATE				psState, 
											   PINST							psInst, 
											   PCSOURCE_ARGUMENT_PAIR			psCommutableSources,
											   PSOURCE_VECTOR					psReplaceMask,
											   PFLOAT_SOURCE_MODIFIER			psNewSrcMod,
											   IMG_BOOL							bCommute)
/*****************************************************************************
 FUNCTION	: CheckCommutableSourceModifiers

 PURPOSE	: Check if changing the source modifiers on a pair of commutable sources
			  would still be valid.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to check.
			  psCommutableSources	- Pair of instruction sources which commute.
			  psReplaceMask			- Mask of instruction sources to modify.
			  psNewSrcMod			- Source modifier to apply before the existing
									modifiers.
			  bCommute				- If FALSE then check the arguments in the original
									order. If TRUE then check swapped.

 RETURNS	: TRUE if the modifiers on the pair of arguments are still valid.
*****************************************************************************/
{
	if (!CheckSourceModifierOnArg(psState, 
								  psInst, 
								  bCommute ? psCommutableSources->uSecondSource : psCommutableSources->uFirstSource, 
								  psCommutableSources->uFirstSource, 
								  psReplaceMask, 
								  psNewSrcMod))
	{
		return IMG_FALSE;
	}
	if (!CheckSourceModifierOnArg(psState, 
								  psInst, 
								  bCommute ? psCommutableSources->uFirstSource : psCommutableSources->uSecondSource, 
								  psCommutableSources->uSecondSource, 
								  psReplaceMask, 
								  psNewSrcMod))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}	

static IMG_BOOL CheckSourceModifiers(PINTERMEDIATE_STATE			psState, 
									 PINST							psInst, 
									 PSOURCE_VECTOR					psReplaceMask,
									 PFLOAT_SOURCE_MODIFIER			psNewSrcMod,
									 IMG_BOOL						bCheckOnly)
/*****************************************************************************
 FUNCTION	: CheckSourceModifiers

 PURPOSE	: Check if an instruction would still be valid if a source modifier
			  applied in a seperate move instruction are substituted into
			  the instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to check.
			  psReplaceMask			- Mask of instruction sources which are
									the destination of the move instruction.
			  psNewSrcMod			- Source modifier applied by the move
									instruction.
			  bCheckOnly			- Just check if replacement is possible.

 RETURNS	: TRUE if the instruction was still valid.
*****************************************************************************/
{
	PCSOURCE_ARGUMENT_PAIR	psCommutableSources = g_psInstDesc[psInst->eOpcode].psCommutableSources;
	IMG_BOOL				bCommute;
	IMG_UINT32				uArg;

	/*
		Check if the move doesn't apply any source modifier.
	*/
	if (!(psNewSrcMod->bNegate || psNewSrcMod->bAbsolute))
	{
		return IMG_TRUE;
	}

	/*
		For a pair of commutable sources: check if the source modifiers are
		valid in either possible order.
	*/
	bCommute = IMG_FALSE;
	if (psCommutableSources != NULL)
	{
		if (!CheckCommutableSourceModifiers(psState,
										    psInst,
										    psCommutableSources,
										    psReplaceMask,
										    psNewSrcMod,
										    IMG_FALSE /* bCommute */))
		{
			if (!CheckCommutableSourceModifiers(psState,
												psInst,
												psCommutableSources,
												psReplaceMask,
												psNewSrcMod,
												IMG_TRUE /* bCommute */))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			bCommute = IMG_TRUE;
		}
	}
	
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		/*
			Ignore arguments we aren't replacing.
		*/
		if (!CheckSourceVector(psReplaceMask, uArg))
		{
			continue;
		}

		/*
			Don't check the pair of commutable arguments.
		*/
		if (psCommutableSources != NULL)
		{
			if (uArg == psCommutableSources->uFirstSource || uArg == psCommutableSources->uSecondSource)
			{
				continue;
			}
		}

		/*
			Is the new source modifier valid for this source?
		*/
		if (!CheckSourceModifierOnArg(psState, psInst, uArg, uArg, psReplaceMask, psNewSrcMod))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	/*
		Add the source modifiers
	*/
	if (!bCheckOnly)
	{
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			/*
				Ignore arguments we aren't replacing.
			*/
			if (!CheckSourceVector(psReplaceMask, uArg))
			{
				continue;
			}

			/*
				Combine the source modifiers from the move instruction with 
				the current source modifiers.
			*/
			if (psInst->eOpcode == ITESTPRED)
			{
				ASSERT(!psNewSrcMod->bAbsolute && psNewSrcMod->bNegate);
				ChangeTestForSourceMod(psState, psInst, uArg);
			}
			else
			{
				CombineInstSourceModifiers(psState, psInst, uArg, psNewSrcMod);
			}
		}

		if (bCommute)
		{
			SwapInstSources(psState, psInst, psCommutableSources->uFirstSource, psCommutableSources->uSecondSource);	
		}

		/*
			Check if the instruction could now be simplified after changing the
			source modifiers.
		*/
		if (psInst->eOpcode == IFMOV && 
			!HasSourceModifier(psState, psInst, 0 /* uArg */))
		{
			SetOpcode(psState, psInst, IMOV);
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL GlobalReplaceArguments(PINTERMEDIATE_STATE			psState,
									   PCODEBLOCK					psCodeBlock,
									   PINST						psInst,
									   PARGUMENT_REFS				psArgRefs,
									   PARG							psRegA,
									   PARG							psRegB,
									   IMG_PVOID					pvContext,
									   IMG_BOOL						bCheckOnly)
/*****************************************************************************
 FUNCTION	: GlobalReplaceArguments

 PURPOSE	: Modify an instruction whose arguments are being replaced to
			  avoid bank restrictions.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block containing the instruction whose
									arguments are being replaced.
			  uArgMask				- Mask of arguments being replaced.
			  psRegA				- Replacement argument.
			  bCheckOnly			- Just check if replacement is possible.

 RETURNS	: FALSE if the move destination couldn't be replaced.
*****************************************************************************/
{
	IMG_UINT32						uArg;
	ELIMINATE_GLOBAL_MOVE_CONTEXT*	psCtx = (ELIMINATE_GLOBAL_MOVE_CONTEXT*)pvContext;

	PVR_UNREFERENCED_PARAMETER(psCodeBlock);

	if (psArgRefs->bUsedAsIndex || psArgRefs->bUsedAsPartialDest)
	{
		/*
			Source modifiers can't be moved from a move instruction to the use of the destination as an index
			so fail.
		*/
		if (psCtx->eF16Swizzle != FMAD16_SWIZZLE_LOWHIGH || 
			psCtx->sSourceMod.bNegate || 
			psCtx->sSourceMod.bAbsolute ||
			psRegB->eFmt == UF_REGFORMAT_F16)
		{
			return IMG_FALSE;
		}
	}

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		/*
			Ignore arguments we aren't replacing.
		*/
		if (!CheckSourceVector(&psArgRefs->sSourceVector, uArg))
		{
			continue;
		}

		/*
			Check if its possible to substitute an F16 swizzle into an instruction. On the
			second pass modify the instruction to do the substitute.
		*/
		if (psCtx->eF16Swizzle != FMAD16_SWIZZLE_LOWHIGH && 
			!SubstituteFMad16Swizzle(psState, psInst, psCtx->eF16Swizzle, uArg, bCheckOnly))
		{
			return IMG_FALSE;
		}

		/*
			Clear component and format flags when substituting a register that
			doesn't use format control.
		*/
		ASSERT(!(bCheckOnly && psRegB->eFmt != UF_REGFORMAT_UNTYPED && psInst->asArg[uArg].eFmt != psRegB->eFmt));

		if (!bCheckOnly)
		{
			if (
					InstUsesF16FmtControl(psInst) &&
					psRegB->eFmt == UF_REGFORMAT_F16 &&
					!TypeUsesFormatControl(psInst->asArg[uArg].uType)
			   )
			{
				psInst->asArg[uArg].eFmt = UF_REGFORMAT_F32;

				SetComponentSelect(psState, psInst, uArg, 0 /* uComponent */);

				/*
					Replace the F16 form of ONE by the F32 form.
				*/
				if (psInst->asArg[uArg].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
				{
					psInst->asArg[uArg].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				}
			}
		}
	}

	/*
		Check if negate/absolute source modifiers can be moved from the move instruction to this 
		instruction.
	*/
	if (!CheckSourceModifiers(psState, psInst, &psArgRefs->sSourceVector, &psCtx->sSourceMod, bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Append the modified instruction to the list of instructions to check for 
		arithmetic simplifications.
	*/
	if (!bCheckOnly && psCtx->psEvalList != NULL)
	{
		AppendToEvalList(psState, psCtx->psEvalList, psInst);
	}

	/*
		Check if we could replace a PCKF16F16 by a simplier instruction.
	*/
	if (
			!bCheckOnly &&
			psInst->eOpcode == IPCKF16F16 &&
			psInst->auDestMask[0] == USC_DESTMASK_FULL &&
			psRegA->uType == USEASM_REGTYPE_FPCONSTANT
	   )
	{
	
		if (
				psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
				(
					psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO || 
					psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1
				) &&
				psInst->asArg[1].uType == psInst->asArg[0].uType &&
				psInst->asArg[1].uNumber == psInst->asArg[0].uNumber
			)
		{
			SetOpcode(psState, psInst, IMOV);
		}
	}

	return IMG_TRUE;
}

typedef struct _REPLACE_MASKED_WORK_ITEM
{
	USC_LIST_ENTRY	sListEntry;
	/* Temporary register number to replace. */
	IMG_UINT32		uTempNum;
	/* Mask of channels to replace. */
	IMG_UINT32		uChanMask;
} REPLACE_MASKED_WORK_ITEM, *PREPLACE_MASKED_WORK_ITEM;

static
PREPLACE_MASKED_WORK_ITEM CreateMaskedReplaceWorkItem(PINTERMEDIATE_STATE	psState,
													  PUSC_LIST				psWorkList,
													  IMG_UINT32			uTempNum,
													  IMG_UINT32			uReplaceMask)
/*****************************************************************************
 FUNCTION	: CreateMaskedReplaceWorkItem

 PURPOSE	: Add an individual temporary register to the list of registers to
			  replace a mask of channels by another register.

 PARAMETERS	: psState				- Compiler state.
			  psWorkList			- List to add to.
			  uTempNum				- Register number to replace.
			  uReplaceMask			- Mask of channels to replace. 

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREPLACE_MASKED_WORK_ITEM	psNewWork;

	psNewWork = UscAlloc(psState, sizeof(*psNewWork));
	psNewWork->uTempNum = uTempNum;
	psNewWork->uChanMask = uReplaceMask;
	AppendToList(psWorkList, &psNewWork->sListEntry);
	return psNewWork;
}

static
IMG_BOOL IsSamePred(PINST				psInst,
					PARG*				apsMatchPredSrc,
					IMG_UINT32			uPredCount,
					IMG_BOOL			bMatchPredNegate,
					IMG_BOOL			bMatchPredPerChan)
/*****************************************************************************
 FUNCTION	: IsSamePred

 PURPOSE    : Check for a matching predicate.

 INPUT		: psInst			- Compare the source predicate from this instruction.
			  apsMatchPredSrc	- The array of predicates to compare
			  uPredCount		- Length of the array of predicates to compare
			  bMatchPredNegate	- Predicate negate flag must also match this.
			  bMatchPredPerChan	- Predicate per channel flag must also match this.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uPredSrc, uPred;
	IMG_BOOL	bPredNegate, bPredPerChan;

	if (psInst->uPredCount != uPredCount)
	{
		return IMG_FALSE;
	}
	
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		IMG_UINT32 uMatchPredNum = USC_PREDREG_NONE;

		GetPredicate(psInst, &uPredSrc, &bPredNegate, &bPredPerChan, uPred);
		if (apsMatchPredSrc[uPred] != NULL)
		{
			uMatchPredNum = apsMatchPredSrc[uPred]->uNumber;
		}
		
		if (uPredSrc != uMatchPredNum ||
			bPredNegate != bMatchPredNegate ||
			bPredPerChan != bMatchPredPerChan)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL BaseReplaceAllUsesMasked(PINTERMEDIATE_STATE			psState,
								  PUSC_LIST						psWorkList,
								  IMG_UINT32					uMovDestTempNum,
								  IMG_UINT32					uMovDestMask,
								  PARG*							apsPredSrc,
								  IMG_UINT32					uPredCount,
								  IMG_BOOL						bPredNegate,
								  IMG_BOOL						bPredPerChan,
								  PFN_MASKED_REPLACE			pfnReplace,
								  IMG_PVOID						pvContext,
								  IMG_BOOL						bCheckOnly)
/*****************************************************************************
 FUNCTION	: BaseReplaceAllUsesMasked

 PURPOSE	: Try to replace a mask of channels within a single register.

 PARAMETERS	: psState				- Compiler state.
			  psWorkList			- List of registers to replace. If the current
									register is the old value of a partially updated
									destination then the new destination is added with the
									mask of channels copied from the old destination.
			  uMovDestTempNum		- Register number to replace.
			  uMovDestMask			- Mask of channels to replace.
			  psMovSrc				- Register to replace by.
			  uMovSrcMask			- Mask of channels to replace by.
			  pfnReplace			- Callback for checking whether a
									replacement is valid at an instruction.
			  pvContext				- Callback context.
			  bCheckOnly			- If TRUE just check if replacement is possible.

 RETURNS	: TRUE if replacement is possible.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF			psMovDestDef;
	PUSEDEF_CHAIN	psMovDestUseDef;

	/*
		Get the chain of uses of the move's destination.
	*/
	psMovDestUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uMovDestTempNum);

	psMovDestDef = psMovDestUseDef->psDef;
	ASSERT(UseDefIsDef(psMovDestDef));

	psListEntry = psMovDestUseDef->sList.psHead;
	while (psListEntry != NULL)
	{
		SOURCE_VECTOR	sSourceVector = {0};
		IMG_BOOL		bReplaceSources;
		PINST			psUseInst;
		IMG_BOOL		bRet;

		psUseInst = NULL;
		bReplaceSources = IMG_FALSE;
		while (psListEntry != NULL)
		{
			PUSEDEF		psUse;

			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

			/*
				Skip the define of the move destination.
			*/
			if (psUse == psMovDestUseDef->psDef)
			{
				psListEntry = psListEntry->psNext;
				continue;
			}

			ASSERT(UseDefIsUse(psUse));

			/*
				Don't do the replace if the move result is used in a switch or in the driver epilog.
			*/
			if (psUse->eType == USE_TYPE_FIXEDREG || 
				psUse->eType == USE_TYPE_SWITCH ||
				psUse->eType == USE_TYPE_FUNCOUTPUT)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			/*
				Only predicates are valid as sources for conditional flow control blocks.
			*/
			ASSERT(psUse->eType != USE_TYPE_COND);

			/*
				Accumulate all the uses within one instruction.
			*/
			if (psUseInst != NULL && psUseInst != psUse->u.psInst)
			{
				break;
			}
			psUseInst = psUse->u.psInst;
			psListEntry = psListEntry->psNext;

			/*
				Fail if the move destination is used as an index.
			*/
			if (psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_OLDDESTIDX)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			/*
				Fail if the write is predicated.
			*/
			if (NoPredicate(psState, psUseInst) && apsPredSrc == IMG_NULL)
			{
				/* this instruction has no predicate and we are looking for no predicate */
			}
			else if (apsPredSrc != IMG_NULL && IsSamePred(psUseInst, apsPredSrc, uPredCount, bPredNegate, bPredPerChan))
			{
				/* this instruction has same predicate as we are looking for */
			}
			else if (apsPredSrc != IMG_NULL || psUse->eType != USE_TYPE_SRC)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			/*
				Check for using the move destination as the preserved data in a masked or predicated
				write.
			*/
			if (psUse->eType == USE_TYPE_OLDDEST)
			{
				IMG_UINT32	uPreservedMask;

				/*
					Get the channels in the instruction's destination which are copied from the move's destination.
				*/
				uPreservedMask = uMovDestMask & ~psUseInst->auDestMask[psUse->uLocation];

				if (uPreservedMask != 0)
				{
					PARG psDest = &psUseInst->asDest[psUse->uLocation];

					if (psDest->uType != USEASM_REGTYPE_TEMP)
					{
						ASSERT(bCheckOnly);
						return IMG_FALSE;
					}

					/*
						Add the instruction's destination to the list of temporaries to replace.
					*/
					CreateMaskedReplaceWorkItem(psState, psWorkList, psDest->uNumber, uPreservedMask);

					if (!bCheckOnly)
					{
						/*
							Flag that the channels replaced by the move source are no longer in use.
						*/
						psUseInst->auLiveChansInDest[psUse->uLocation] &= ~uPreservedMask;

						/*
							Check if all the channels not overwritten have been replaced by the move source.
						*/
						if (GetPreservedChansInPartialDest(psState, psUseInst, psUse->uLocation) == 0)
						{
							SetPartiallyWrittenDest(psState, psUseInst, psUse->uLocation, NULL /* psPartialDest */);
						}
					}
				}
			}
			else
			{
				IMG_UINT32	uUsedChanMask;

				ASSERT(psUse->eType == USE_TYPE_SRC);

				uUsedChanMask = GetLiveChansInArg(psState, psUseInst, psUse->uLocation);

				/*
					Check if the instruction source uses the mask of channels we are replacing.
				*/
				if ((uUsedChanMask & uMovDestMask) != 0)
				{
					/*
						Check if the instruction source also uses the mask of channels we are not
						replacing.
					*/
					if ((uUsedChanMask & ~uMovDestMask) != 0)
					{	
						ASSERT(bCheckOnly);
						return IMG_FALSE;
					}

					/*
						Build up a mask of the instruction sources we are trying to replace.
					*/
					if (!bReplaceSources)
					{
						InitSourceVector(psState, psUse->u.psInst, &sSourceVector);
						bReplaceSources = IMG_TRUE;
					}
					AddToSourceVector(&sSourceVector, psUse->uLocation);
				}
			}

			if (psListEntry == NULL)
			{
				break;
			}
		}

		if (psUseInst == NULL)
		{
			continue;
		}

		/*
			Call the callback function to check if a replacement is valid for this instruction.
		*/
		if (bReplaceSources)
		{
			bRet = pfnReplace(psState, psUseInst, &sSourceVector, pvContext, bCheckOnly);
			DeinitSourceVector(psState, &sSourceVector);
	
			if (!bRet)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL	
IMG_BOOL DoReplaceAllUsesMasked(PINTERMEDIATE_STATE			psState,
								PARG						psMovDest,
								IMG_UINT32					uMovDestMask,
								PARG*						apsPredSrc,
								IMG_UINT32					uPredCount,
								IMG_BOOL					bPredNegate,
								IMG_BOOL					bPredPerChan,
								PFN_MASKED_REPLACE			pfnReplace,
								IMG_PVOID					pvContext,
								IMG_BOOL					bCheckOnly)
/*****************************************************************************
 FUNCTION	: DoReplaceAllUsesMasked

 PURPOSE	: Try to replace a mask of channels within a register.

 PARAMETERS	: psState				- Compiler state.
			  psMovDest				- Register to replace.
			  uMovDestTempNum		- Register number to replace.
			  apsPredSrc			- Predicates on the source to be replaced.
			  uPredCount			- Number of predicates on the source to be replaced.
			  pfnReplace			- Callback for checking whether a
									replacement is valid at an instruction.
			  psMovSrc				- Register number to replace by.
			  uMovSrcMask			- Mask of replacement channels.
			  pvContext				- Callback context.
			  bCheckOnly			- If TRUE just check if replacement is possible.

 RETURNS	: TRUE if replacement is possible.
*****************************************************************************/
{
	USC_LIST		sWorkList;
	PUSC_LIST_ENTRY	psListEntry;

	InitializeList(&sWorkList);
	
	ASSERT(psMovDest->uType == USEASM_REGTYPE_TEMP);
	CreateMaskedReplaceWorkItem(psState, &sWorkList, psMovDest->uNumber, uMovDestMask);

	while ((psListEntry = RemoveListHead(&sWorkList)) != NULL)
	{
		PREPLACE_MASKED_WORK_ITEM	psWorkItem;
		IMG_BOOL					bRet;

		psWorkItem = IMG_CONTAINING_RECORD(psListEntry, PREPLACE_MASKED_WORK_ITEM, sListEntry);

		bRet = 
			BaseReplaceAllUsesMasked(psState, 
								     &sWorkList,
									 psWorkItem->uTempNum,
									 psWorkItem->uChanMask,
									 apsPredSrc,
									 uPredCount,
									 bPredNegate,
									 bPredPerChan,
									 pfnReplace,
									 pvContext,
								     bCheckOnly);
		UscFree(psState, psWorkItem);

		if (!bRet)
		{
			while ((psListEntry = RemoveListHead(&sWorkList)) != NULL)
			{
				PREPLACE_MASKED_WORK_ITEM	psWorkItemToFree;

				psWorkItemToFree = IMG_CONTAINING_RECORD(psListEntry, PREPLACE_MASKED_WORK_ITEM, sListEntry);
				UscFree(psState, psWorkItemToFree);
			}

			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ReplaceAllUsesMasked(PINTERMEDIATE_STATE			psState,
							  PARG							psMovDest,
							  IMG_UINT32					uMovDestMask,
							  PARG*							apsPredSrc,
							  IMG_UINT32					uPredCount,
							  IMG_BOOL						bPredNegate,
							  IMG_BOOL						bPredPerChan,
							  PFN_MASKED_REPLACE			pfnReplace,
							  IMG_PVOID						pvContext,
							  IMG_BOOL						bCheckOnly)
/*****************************************************************************
 FUNCTION	: ReplaceAllUsesMasked

 PURPOSE	: Try to replace a mask of channels within a register.

 PARAMETERS	: psState				- Compiler state.
			  psMovDest				- Register to replace.
			  uMovDestTempNum		- Register number to replace.
			  pfnReplace			- Callback for checking whether a
									replacement is valid at an instruction.
			  pvContext				- Callback context.

 RETURNS	: TRUE if the register was replaced.
********* ********************************************************************/
{
	IMG_BOOL	bRet;

	/*
		Check if replacement is possible.
	*/
	if (!DoReplaceAllUsesMasked(psState,
								psMovDest,
								uMovDestMask,
								apsPredSrc,
								uPredCount,
								bPredNegate,
								bPredPerChan,
								pfnReplace,
								pvContext,
								IMG_TRUE /* bCheckOnly */))
	{
		return IMG_FALSE;
	}

	if (bCheckOnly)
	{
		return IMG_TRUE;
	}

	/*
		Do the replacement.
	*/
	bRet = DoReplaceAllUsesMasked(psState,
								  psMovDest,
								  uMovDestMask,
								  apsPredSrc,
								  uPredCount,
								  bPredNegate,
								  bPredPerChan,
								  pfnReplace,
								  pvContext,
								  IMG_FALSE /* bCheckOnly */);
	ASSERT(bRet);

	return IMG_TRUE;
}

static
IMG_BOOL ReplaceAllUses(PINTERMEDIATE_STATE			psState, 
						PARG						psRegA,
						PARG						psRegB,
						PFN_GLOBAL_REPLACE_ARGUMENT	pfnArgReplace,
						IMG_PVOID					pvContext,
						IMG_BOOL					bCheckOnly,
						PUSEDEF_CHAIN				psUseDef)
{
	PUSC_LIST_ENTRY	psListEntry;

	psListEntry = psUseDef->sList.psHead;
	while (psListEntry != NULL)
	{
		ARGUMENT_REFS	sRefs = {{0}, IMG_FALSE, IMG_FALSE};
		PINST			psUseInst;
		IMG_BOOL		bRet;

		psUseInst = NULL;
		while (psListEntry != NULL)
		{
			PUSEDEF		psUse;

			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psUse == psUseDef->psDef)
			{
				psListEntry = psListEntry->psNext;
				break;
			}
			if (psUse->eType == USE_TYPE_FIXEDREG || psUse->eType == USE_TYPE_SWITCH || psUse->eType == USE_TYPE_FUNCOUTPUT)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			ASSERT(psUse->eType != USE_TYPE_COND);
			if (psUseInst != NULL && psUseInst != psUse->u.psInst)
			{
				break;
			}

			if (psUseInst == NULL)
			{
				sRefs.bUsedAsIndex = IMG_FALSE;
				sRefs.bUsedAsPartialDest = IMG_FALSE;
				InitSourceVector(psState, psUse->u.psInst, &sRefs.sSourceVector);
			}
			psUseInst = psUse->u.psInst;

			psListEntry = psListEntry->psNext;

			/*
				Check if the argument is a valid substitute for a temporary used
				as a dynamic index.
			*/
			if (psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_OLDDESTIDX)
			{
				if (!IsValidIndexType(psState, psRegA))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
			}

			/*
				Build up a mask of the uses of the mov destination in this instruction.
			*/
			switch (psUse->eType)
			{
				case USE_TYPE_DESTIDX:
				case USE_TYPE_SRCIDX:
				case USE_TYPE_OLDDESTIDX:
				{
					sRefs.bUsedAsIndex = IMG_TRUE;
					break;
				}
				case USE_TYPE_OLDDEST:
				{
					sRefs.bUsedAsPartialDest = IMG_TRUE;
					break;
				}
				case USE_TYPE_SRC:
				{
					AddToSourceVector(&sRefs.sSourceVector, psUse->uLocation);
					break;
				}
				default: imgabort();
			}

			if (!bCheckOnly)
			{
				/*
					Replace the use of the move destination by the move source.
				*/
				UseDefSubstUse(psState, psUseDef, psUse, psRegA);
			}
		}

		if (psUseInst == NULL)
		{
			continue;
		}

		bRet = pfnArgReplace(psState,
							 psUseInst->psBlock,
							 psUseInst,
							 &sRefs,
							 psRegA,
							 psRegB,
							 pvContext,
							 bCheckOnly);
		DeinitSourceVector(psState, &sRefs.sSourceVector);
		if (!bRet)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}

		if (!bCheckOnly)
		{
			#ifdef SRC_DEBUG
			if( (psState->uCurSrcLine != UNDEFINED_SOURCE_LINE) && 
				(psUseInst->uAssociatedSrcLine == psState->uTotalLines) )
			{
				DecrementCostCounter(psState, (psUseInst->uAssociatedSrcLine), 1);
				psUseInst->uAssociatedSrcLine = psState->uCurSrcLine;
				IncrementCostCounter(psState, (psUseInst->uAssociatedSrcLine), 1);
			}	
			#endif /* SRC_DEBUG */
		}
	}

	return IMG_TRUE;
}

IMG_BOOL IMG_INTERNAL DoGlobalMoveReplace(PINTERMEDIATE_STATE			psState, 
										  PCODEBLOCK					psBlock,
										  PINST							psMoveInst,
										  PARG							psRegA,
										  PARG							psRegB,
										  PFN_GLOBAL_REPLACE_ARGUMENT	pfnArgReplace,
										  IMG_PVOID						pvContext)
/*****************************************************************************
 FUNCTION	: DoGlobalMoveReplace

 PURPOSE	: Eliminate a move from a block where the move result is used after
			  the block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block containing the move.
			  psMoveInst		- Move instruction.
			  psRegA			- Move source.
			  psRegB			- Move destination.
			  pfnArgReplace		- Function to update instructions after replacement.

 RETURNS	: TRUE if the move was eliminated.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSEDEF			psDef;

	/*
		Don't replace register types we don't have USE-DEF information for.
	*/
	if (psRegB->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	/*
		Get the list of uses of the move destination.
	*/
	psUseDef = UseDefGet(psState, psRegB->uType, psRegB->uNumber);
	ASSERT(psUseDef != NULL);

	ASSERT(psUseDef->psDef != NULL);
	psDef = psUseDef->psDef;
	ASSERT(psDef->eType == DEF_TYPE_INST);
	ASSERT(psDef->u.psInst == psMoveInst);
	ASSERT(psDef->uLocation == 0);

	/*
		If the move source is a modifiable type that we haven't put in SSA form then don't
		try and replace the destination.
	*/
	if (psRegA->uType == USEASM_REGTYPE_LINK || IsModifiableRegisterArray(psState, psRegA->uType, psRegA->uNumber))
	{
		return IMG_FALSE;
	}

	if (!ReplaceAllUses(psState, 
						psRegA, 
						psRegB, 
						pfnArgReplace, 
						pvContext, 
						IMG_TRUE /* bCheckOnly */,
						psUseDef))
	{
		return IMG_FALSE;
	}

	#ifdef SRC_DEBUG
	psState->uCurSrcLine =  psMoveInst->uAssociatedSrcLine;
	#endif /* SRC_DEBUG */

	ReplaceAllUses(psState, 
				   psRegA, 
				   psRegB, 
				   pfnArgReplace, 
				   pvContext, 
				   IMG_FALSE /* bCheckOnly */,
				   psUseDef);

	#ifdef SRC_DEBUG
	psState->uCurSrcLine = UNDEFINED_SOURCE_LINE;
	#endif /* SRC_DEBUG */

	RemoveInst(psState, psBlock, psMoveInst);
	FreeInst(psState, psMoveInst);
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL CanReplaceTempWithAnyGPI(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uTempRegNum,
								  PINST*				ppsDefineInst, 
								  PINST*				ppsLastUseInst,
								  IMG_BOOL				bAllowGroupedReg)
/*****************************************************************************
 FUNCTION	: CanReplaceTempWithAnyGPI
    
 PURPOSE	: Checks if its possible to replace a temporary everywhere by an internal register (any).

 PARAMETERS	: psState			- Compiler state.
			  uTempRegNum		- Temporary to replace.
			  ppsDefineInst		- If not NULL, used to return the instruction that defines
								  the temp register, if returning TRUE.
			  ppsLastUseInst	- If not NULL, used to return the instruction which is the 
								  last use of the register, if returning TRUE.
			  bAllowGroupedReg	- Whether to avoid allocating GPIs for registers in groups.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psDefInst, psLastUseInst, psDeschedInst;
	PCODEBLOCK		psDefineBlock;
	UF_REGFORMAT	eDefFmt;
	
	/*
		Check if this temporary is part of a group of registers which require consecutive
		hardware register numbers then it can't be replaced by an internal register.
	*/
	if (!bAllowGroupedReg && IsNodeInGroup(FindRegisterGroup(psState, uTempRegNum)))
	{
		return IMG_FALSE;
	}

	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempRegNum);
	
	/*
		If this temporary is not used return FALSE.
		If this temporary is defined in the driver prolog then it can't be replaced by an
		internal register.
	*/
	if (psUseDef == NULL || psUseDef->psDef == NULL || psUseDef->psDef->eType != DEF_TYPE_INST)
	{
		return IMG_FALSE;
	}
	
	psDefInst = psUseDef->psDef->u.psInst;
	psDefineBlock = psDefInst->psBlock;
	eDefFmt = psDefInst->asDest[psUseDef->psDef->uLocation].eFmt;

	if (
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psDefInst->eOpcode == IVDUAL || 
#endif
		!CanUseDest(psState, psDefInst, psUseDef->psDef->uLocation, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}

	if (RequiresGradients(psDefInst) && (psState->uNumDynamicBranches > 0))
	{
		return IMG_FALSE;
	}

	if (g_psInstDesc[psDefInst->eOpcode].eType == INST_TYPE_PCK &&
		psDefInst->auDestMask[psUseDef->psDef->uLocation] != USC_ALL_CHAN_MASK)
	{
		return IMG_FALSE;
	}

	if (eDefFmt == UF_REGFORMAT_C10 || eDefFmt == UF_REGFORMAT_U8 || eDefFmt == UF_REGFORMAT_F16)
	{
		/* Some of the fixed point arithmetic instructions have different behaviour when their sources/destinations are GPIs */
		return IMG_FALSE;
	}

	psLastUseInst = NULL;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse;
		PINST		psReader;
		IMG_UINT32	uReaderArg;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		/*
			Check for using the temporary other than in a source argument.
		*/
		if (psUse->eType != USE_TYPE_SRC)
		{
			return IMG_FALSE;
		}
		psReader = psUse->u.psInst;
		/*
			Check for using the temporary in a different block.
		*/
		if (psReader->psBlock != psDefineBlock)
		{
			return IMG_FALSE;
		}
		if (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_34293)
		{
			if (g_psInstDesc[psReader->eOpcode].uFlags & (DESC_FLAGS_MEMSTORE | DESC_FLAGS_MEMLOAD))
			{
				return IMG_FALSE;
			}
		}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			These instructions can't have internal register sources.
		*/
		if (psReader->eOpcode == IVDSX || psReader->eOpcode == IVDSY)
		{
			return IMG_FALSE;
		}
		
		/* 
			Vector pack instructions with F32 sources require both or neither of the arguments to be GPI.
			This is difficult to ensure, so just disallow GPIs for those instructions.
		*/
		if ((g_psInstDesc[psReader->eOpcode].uFlags & DESC_FLAGS_VECTORPACK) != 0)
		{
			return IMG_FALSE;
		}
#endif

		uReaderArg = psUse->uLocation;

		/*
			Check for instructions which don't support an internal register as an argument.
		*/
		if (!CanUseSrc(psState, psReader, uReaderArg, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX))
		{
			return IMG_FALSE;
		}
		
		/*
			Keep track of the last place in the block where the temporary is used.
		*/
		if (psLastUseInst == NULL || psLastUseInst->uBlockIndex < psReader->uBlockIndex)
		{
			psLastUseInst = psReader;
		}
	}
	
	if (ppsDefineInst != NULL)
	{
		*ppsDefineInst = psDefInst;
	}
	if (ppsLastUseInst != NULL)
	{
		*ppsLastUseInst = psLastUseInst;
	}

	/*
		Check for no uses of the temporary register in the block.
	*/
	if (psLastUseInst == NULL)
	{
		return IMG_TRUE;
	}

	/*
		Check if the interval contains a deschedule.
	*/
	if (IsDeschedAfterInst(psDefInst))
	{
		return IMG_FALSE;
	}

	for (psDeschedInst = psDefInst->psNext; psDeschedInst != psLastUseInst; psDeschedInst = psDeschedInst->psNext)
	{
		if (IsDeschedBeforeInst(psState, psDeschedInst) || IsDeschedAfterInst(psDeschedInst))
		{
			return IMG_FALSE;
		}
	}

	if (IsDeschedBeforeInst(psState, psLastUseInst))
	{
		return IMG_FALSE;
	}	
	
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL CanReplaceTempWithGPI(PINTERMEDIATE_STATE	psState,
							   IMG_UINT32			uTempRegNum, 
							   IMG_UINT32			uGPIRegNum,
							   IMG_BOOL				bAllowGroupedReg)
/*****************************************************************************
 FUNCTION	: CanReplaceTempWithGPI
    
 PURPOSE	: Checks if its possible to replace a temporary everywhere by a given internal register.

 PARAMETERS	: psState			- Compiler state.
			  uTempRegNum		- Temporary to replace.
			  uGPIRegNum		- Internal register to replace by.
			  bAllowGroupedReg	- Whether to avoid allocating GPIs for registers in groups.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{ 
	PINST psDefineInst, psLastUseInst;

	ASSERT(UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempRegNum) != NULL);

	/* Check if the temporary could be replaced with a GPI */
	if (!CanReplaceTempWithAnyGPI(psState, uTempRegNum, &psDefineInst, &psLastUseInst, bAllowGroupedReg))
	{
		return IMG_FALSE;
	}

	/*
		Check if the interval where the temporary is live contains either a deschedule or another
		use of the same internal register.
	*/
	if (UseDefIsIRegLiveInInternal(psState, uGPIRegNum, psDefineInst, psLastUseInst))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

IMG_BOOL IMG_INTERNAL EliminateGlobalMove(PINTERMEDIATE_STATE		psState, 
										  PCODEBLOCK				psBlock,
										  PINST						psMoveInst,
										  PARG						psRegA,	
										  PARG						psRegB,
										  PWEAK_INST_LIST			psEvalList)
/*****************************************************************************
 FUNCTION	: EliminateGlobalMove

 PURPOSE	: Eliminate a move from a block where the move result is used after
			  the block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block containing the move.
			  psMoveInst		- Move instruction.
			  psRegA			- Move source.
			  psRegB			- Move destination.
			  psEvalList		- If non-NULL then modified instructions are
								appended to this list.

 RETURNS	: TRUE if the move was eliminated.
*****************************************************************************/
{
	ELIMINATE_GLOBAL_MOVE_CONTEXT	sCtx;

	/*
		Initialise the context for the function that replaces instruction arguments.
	*/
	sCtx.psEvalList = psEvalList;

	/*
		Move with a negate or absolute modifier?
	*/
	if (psMoveInst->eOpcode == IFMOV16 || psMoveInst->eOpcode == IFMOV)
	{
		if (psMoveInst->eOpcode == IFMOV16)
		{
			sCtx.sSourceMod = psMoveInst->u.psArith16->sFloat.asSrcMod[0];
		}
		else
		{
			sCtx.sSourceMod = psMoveInst->u.psFloat->asSrcMod[0];
		}
	}
	else
	{
		sCtx.sSourceMod.bNegate = IMG_FALSE;
		sCtx.sSourceMod.bAbsolute = IMG_FALSE;
	}

	/*
		Move with a swizzle on F16 data?
	*/
	sCtx.eF16Swizzle = FMAD16_SWIZZLE_LOWHIGH;
	if (psMoveInst->eOpcode == IFMOV16)
	{
		sCtx.eF16Swizzle = psMoveInst->u.psArith16->aeSwizzle[0];
	}

	return DoGlobalMoveReplace(psState, 
							   psBlock, 
							   psMoveInst, 
							   psRegA, 
							   psRegB, 
							   GlobalReplaceArguments, 
							   (IMG_PVOID)&sCtx);	
}

#if 0
static IMG_VOID RemovePredicateWrite(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psBlock,
									 PINST					psInst)
/*****************************************************************************
 FUNCTION	: RemovePredicateWrite

 PURPOSE	: Change an instructions so it doesn't write to a predicate.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block that contains the instruction.
			  psInst			- TEST instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		If no references to the predicate remain then remove the write.
	*/
	if (psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		RemoveInst(psState, psBlock, psInst);
		FreeInst(psState, psInst);
	}
	else
	{
		IMG_UINT32	auSrcComponent[TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT];
		IOPCODE		eAluOpcode = psInst->u.psTest->eAluOpcode;
		IMG_UINT32	uArg;

		/*
			Save component selects on any F16 sources to the TEST.
		*/
		ASSERT(g_psInstDesc[eAluOpcode].uDefaultArgumentCount < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
		for (uArg = 0; uArg < g_psInstDesc[eAluOpcode].uDefaultArgumentCount; uArg++)
		{
			auSrcComponent[uArg] = GetComponentSelect(psState, psInst, uArg);
		}
		
		/*
			Switch to the pure ALU form of the instruction.
		*/
		psInst->asDest[0] = psInst->asDest[TEST_UNIFIEDSTORE_DESTIDX];
		SetOpcodeAndDestCount(psState, psInst, eAluOpcode, 1 /* uDestCount */);

		/*
			Restore component selects on the new instruction.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			SetComponentSelect(psState, psInst, uArg, auSrcComponent[uArg]);
		}

		if (psInst->eOpcode == IFMOV && !HasSourceModifier(psState, psInst, 0 /* uArgIdx */))
		{
			SetOpcode(psState, psInst, IMOV);
		}
	}
}
#endif

static
IMG_BOOL IsFixedResultTest(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   IMG_PBOOL			pbFixedResult)
/*****************************************************************************
 FUNCTION	: IsFixedResultTest

 PURPOSE	: Checks for a TEST instruction with a compile-time known result.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  pbFixedResult		- Returns the fixed value the predicate
								result is set to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	 if (
			psInst->eOpcode == ITESTPRED &&
			(
				psInst->u.psTest->eAluOpcode == IFMOV || 
				psInst->u.psTest->eAluOpcode == IFADD || 
				psInst->u.psTest->eAluOpcode == IFSUB
			) &&
			AllTestArgsHwConst(psState, psInst)
		)
	{
		IMG_FLOAT	afValue[2], fValue = 0.0f;
		IMG_UINT32	uArg;

		/*
			Get the result of the test.
		*/
		for (uArg = 0; uArg < g_psInstDesc[psInst->u.psTest->eAluOpcode].uDefaultArgumentCount; uArg++)
		{
			PARG	psSrc = &psInst->asArg[uArg];

			if (psSrc->uType == USEASM_REGTYPE_FPCONSTANT)
			{
				afValue[uArg] = GetHardwareConstantValue(psState, psSrc->uNumber);
			}
			else
			{
				ASSERT(psSrc->uType == USEASM_REGTYPE_IMMEDIATE);
				afValue[uArg] = *(IMG_PFLOAT)&psSrc->uNumber;
			}
			if (IsNegated(psState, psInst, uArg))
			{
				afValue[uArg] = -afValue[uArg];
			}
		}
		switch (psInst->u.psTest->eAluOpcode)
		{
			case IFMOV: fValue = afValue[0]; break;
			case IFADD: fValue = afValue[0] + afValue[1]; break;
			case IFSUB: fValue = afValue[0] - afValue[1]; break;
			default: imgabort();
		}
		*pbFixedResult = DoF32Test(psState, psInst->u.psTest->sTest.eType, fValue);

		return IMG_TRUE;
	 }
	 if (
			psInst->eOpcode == ITESTPRED &&
			(
				psInst->u.psTest->eAluOpcode == IOR ||
				psInst->u.psTest->eAluOpcode == IAND
			) &&
			AllTestArgsHwConst(psState, psInst)
		)
	 {
		IMG_UINT32		uResult;
		IMG_UINT32		auValue[2];
		IMG_UINT32		uArg;

		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			PARG	psSrc = &psInst->asArg[uArg];

			if (psSrc->uType == USEASM_REGTYPE_FPCONSTANT)
			{
				auValue[uArg] = GetHardwareConstantValueu(psState, psSrc->uNumber);
			}
			else
			{
				ASSERT(psSrc->uType == USEASM_REGTYPE_IMMEDIATE);
				auValue[uArg] = psSrc->uNumber;
			}
		}

		 if (psInst->u.psTest->eAluOpcode == IOR)
		 {
			 uResult = auValue[0] | auValue[1];
		 }
		 else
		 {
			 ASSERT(psInst->u.psTest->eAluOpcode == IAND);
			 uResult = auValue[0] & auValue[1];
		 }

		 *pbFixedResult = DoBitwiseTest(psState, psInst->u.psTest->sTest.eType, uResult);

		 return IMG_TRUE;
	 }
	 return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef struct _VMOV_REPLACE_CONTEXT_
{
	IMG_UINT32			uReplaceChansMask;
	IMG_UINT32			uSwizArg;
	PARG				asChanArg;
	PARG				psVecArg;
	VEC_SOURCE_MODIFIER	sSrcMod;
	IMG_UINT32			uSrcSwizzle;
	IMG_BOOL			bScalarSources;
	UF_REGFORMAT		eDestFormat;
	UF_REGFORMAT		eSourceFormat;
} VMOV_REPLACE_CONTEXT, *PVMOV_REPLACE_CONTEXT;

static IMG_BOOL VMOVReplaceArguments(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PSOURCE_VECTOR			psSourceVector,
									 IMG_PVOID				pvContext,
									 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: VMOVReplaceArguments

 PURPOSE	: Modify an instruction whose arguments are being replaced to
			  avoid bank restrictions.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction whose arguments are to be
									replaced.
			  psSourceVector		- Mask of sources to replace.
			  bCheckOnly			- Just check if replacement is possible.

 RETURNS	: FALSE if the move destination couldn't be replaced.
*****************************************************************************/
{
	IMG_UINT32				uArg;
	PVMOV_REPLACE_CONTEXT	psContext = (PVMOV_REPLACE_CONTEXT)pvContext;

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		IMG_UINT32	uLiveChansMask;
		IMG_UINT32	uChan;
		IMG_UINT32	uSwizArg;

		/*
			Ignore arguments we aren't replacing.
		*/
		if (!CheckSourceVector(psSourceVector, uArg))
		{
			continue;
		}

		/*
			Get the mask of channels used by the instruction from the vector
			sized source.
		*/
		uLiveChansMask = GetLiveChansInArg(psState, psInst, uArg);
		if ((uLiveChansMask & ~psContext->uReplaceChansMask) != 0)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}


		/*
			Texture sample instructions don't support swizzles or source modifiers.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0 || 
			psInst->eOpcode == IDELTA ||
			psInst->eOpcode == ICALL)
		{
			if (psContext->bScalarSources ||
				psContext->sSrcMod.bNegate ||
				psContext->sSrcMod.bAbs ||
				psContext->eDestFormat != psContext->eSourceFormat ||
				!CompareSwizzles(psContext->uSrcSwizzle, USEASM_SWIZZLE(X, Y, Z, W), uLiveChansMask))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			if (!bCheckOnly)
			{
				SetSrcFromArg(psState, psInst, uArg, psContext->psVecArg);
			}
			continue;
		}

		/*
			We should only be substituting into a vector sized argument to
			a vector instruction.
		*/
		ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0);
		ASSERT((uArg % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
		uSwizArg = uArg / SOURCE_ARGUMENTS_PER_VECTOR;

		/*
			Don't substitute a negate modifier into a source where some of the channels have constants
			swizzled into them since the negate modifier will then modify the constants too. Absolute
			is okay because all of the constant swizzle selectors are positive.
		*/
		if (psContext->sSrcMod.bNegate)
		{
			IMG_UINT32	uChansUsedPreSwizzle;

			GetLiveChansInSourceSlot(psState, psInst, uSwizArg, &uChansUsedPreSwizzle, NULL /* puChansUsedPostSwizzle */);
			if (!IsNonConstantSwizzle(psInst->u.psVec->auSwizzle[uSwizArg], uChansUsedPreSwizzle, NULL /* puMatchedSwizzle */))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}

		if (!bCheckOnly)
		{
			/*
				Combine the source modifiers on the current instruction with
				those on the VMOV instruction.
			*/
			CombineSourceModifiers(&psContext->sSrcMod, 
								   &psInst->u.psVec->asSrcMod[uSwizArg], 
								   &psInst->u.psVec->asSrcMod[uSwizArg]);

			/*	
				Combine the source swizzle on the current instruction with
				the swizzle on the VMOV instruction.
			*/
			psInst->u.psVec->auSwizzle[uSwizArg] = 
				CombineSwizzles(psContext->uSrcSwizzle, psInst->u.psVec->auSwizzle[uSwizArg]);

			/*
				Copy the source format.
			*/
			ASSERT(psInst->u.psVec->aeSrcFmt[uSwizArg] == psContext->eDestFormat);
			psInst->u.psVec->aeSrcFmt[uSwizArg] = psContext->eSourceFormat;

			if (psContext->bScalarSources)
			{
				IMG_UINT32	uVecLen;

				/*
					Clear the vector sized argument to the instruction.
				*/
				SetSrcUnused(psState, psInst, uArg);
	
				/*
					Get the number of 32-bit scalar registers corresponding
					to a vector register.
				*/
				if (psInst->u.psVec->aeSrcFmt[uSwizArg] == UF_REGFORMAT_F16)
				{
					uVecLen = SCALAR_REGISTERS_PER_F16_VECTOR;
				}
				else
				{
					ASSERT(psInst->u.psVec->aeSrcFmt[uSwizArg] == UF_REGFORMAT_F32);
					uVecLen = SCALAR_REGISTERS_PER_F32_VECTOR;
				}

				/*
					Replace by the vector of scalar arguments to the VMOV instruction.
				*/
				for (uChan = 0; uChan < uVecLen; uChan++)
				{
					PARG	psChanArg = &psInst->asArg[uArg + 1 + uChan];

					ASSERT(psChanArg->uType == USC_REGTYPE_UNUSEDSOURCE || psChanArg->uType == USC_REGTYPE_DUMMY);
					SetSrcFromArg(psState, psInst, uArg + 1 + uChan, &psContext->asChanArg[uChan]);
				}
			}
			else
			{
				SetSrcFromArg(psState, psInst, uArg, psContext->psVecArg);
			}
		}
	}

	if (!bCheckOnly)
	{
		if (psInst->eOpcode == IUNPCKVEC && psContext->bScalarSources)
		{
			if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
			{
				IMG_UINT32	uChanSel;

				uChanSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_X_CHAN);
				if (uChanSel >= USEASM_SWIZZLE_SEL_X && uChanSel <= USEASM_SWIZZLE_SEL_W)
				{
					/* Only convert UNPCKVEC -> MOV if the source is aligned right. */
					if ((psInst->asArg[1 + uChanSel].uType != USEASM_REGTYPE_FPCONSTANT) ||
						((psInst->asArg[1 + uChanSel].uNumber % (1U << QWORD_SIZE_LOG2)) == 0))
					{
						MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 + uChanSel /* uCopyFromSrcIdx */);
						SetOpcode(psState, psInst, IMOV);
					}
				}
			}
			else
			{
				IMG_UINT32	uXChanSel, uYChanSel;
				IMG_BOOL	bXUsed, bYUsed;

				ASSERT(psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);

				uXChanSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_X_CHAN);
				uYChanSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_Y_CHAN);
				bXUsed = (psInst->auLiveChansInDest[0] & USC_XY_CHAN_MASK) != 0;
				bYUsed = (psInst->auLiveChansInDest[0] & USC_ZW_CHAN_MASK) != 0;
				if ((!bXUsed || uXChanSel == USEASM_SWIZZLE_SEL_X) && (!bYUsed || uYChanSel == USEASM_SWIZZLE_SEL_Y))
				{
					/* Only convert UNPCKVEC -> MOV if the source is aligned right. */
					if ((psInst->asArg[1].uType != USEASM_REGTYPE_FPCONSTANT) ||
						((psInst->asArg[1].uNumber % (1U << QWORD_SIZE_LOG2)) == 0))
					{
						MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
						SetOpcode(psState, psInst, IMOV);
					}
				}
				else if ((!bXUsed || uXChanSel == USEASM_SWIZZLE_SEL_Z) && (!bYUsed || uYChanSel == USEASM_SWIZZLE_SEL_W))
				{
					/* Only convert UNPCKVEC -> MOV if the source is aligned right. */
					if ((psInst->asArg[2].uType != USEASM_REGTYPE_FPCONSTANT) ||
						((psInst->asArg[2].uNumber % (1U << QWORD_SIZE_LOG2)) == 0))
					{
						MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 2 /* uCopyFromSrcIdx */);
						SetOpcode(psState, psInst, IMOV);
					}
				}
			}
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL DefIsBetweenInsts(PINTERMEDIATE_STATE psState, PARG pArg, PINST psLowerInst, PINST psUpperInst)
/*****************************************************************************
 FUNCTION	: DefIsBetweenInsts

 PURPOSE	: Check whether the definition of a register is in between two instructions
			  (psLowerInst, psUpperInst]

 PARAMETERS	: psState				- Compiler state.
			  pArg					- Argument whose definition is searched for.
			  psUpperInst			- Do not look higher than this instruction (but include it).
			  psLowerInst			- Do not look lower than this instruction (don't include it).

 RETURNS	: TRUE if an instruction was found in between the two given that
			  writes to the register.
*****************************************************************************/
{
	PINST psDefInst;

	PUSEDEF_CHAIN psUseDef = UseDefGet(psState, pArg->uType, pArg->uNumber);
	if ((psUseDef == NULL) || (psUseDef->psDef == NULL))
	{
		return IMG_FALSE;
	}

	if (psUseDef->psDef->eType != DEF_TYPE_INST)	
	{
		return IMG_FALSE;
	}
	
	psDefInst = psUseDef->psDef->u.psInst;

	if (psDefInst->psBlock != psLowerInst->psBlock ||
		psDefInst->psBlock != psUpperInst->psBlock)
	{
		return IMG_FALSE;
	}

	return (psDefInst->uBlockIndex >= psUpperInst->uBlockIndex &&
			psDefInst->uBlockIndex < psLowerInst->uBlockIndex);
}

static
IMG_BOOL MoveInstUp(PINTERMEDIATE_STATE psState, PINST psMovingInst, PINST psTargetInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: MoveInstUp

 PURPOSE	: Try to move an instruction up, past another instruction.

 PARAMETERS	: psState				- Compiler state.
			  psMovingInst			- Instruction to move up.
			  psTargetInst			- Instruction to move past.

 RETURNS	: TRUE if the move was successful.
*****************************************************************************/
{
	IMG_UINT32  uSrc, uDest;

	for (uSrc = 0; uSrc < psMovingInst->uArgumentCount; ++uSrc)
	{			
		if (psMovingInst->asArg[uSrc].uType == USC_REGTYPE_UNUSEDSOURCE) 
		{
			continue;
		}

		if (DefIsBetweenInsts(psState, &psMovingInst->asArg[uSrc], psMovingInst, psTargetInst))
		{
			return IMG_FALSE;
		}
	}

	for (uDest = 0; uDest < psMovingInst->uDestCount; ++uDest)
	{
		if (psMovingInst->apsOldDest[uDest] == NULL)
		{
			continue;
		}

		if (DefIsBetweenInsts(psState, psMovingInst->apsOldDest[uDest], psMovingInst, psTargetInst))
		{
			return IMG_FALSE;
		}
	}
	
	/* Move the instruction up */
	if (!bCheckOnly)
	{
		RemoveInst(psState, psMovingInst->psBlock, psMovingInst);
		InsertInstBefore(psState, psTargetInst->psBlock, psMovingInst, psTargetInst);
	}

	return IMG_TRUE;
}

static IMG_BOOL ReplaceVMOVDestination(PINTERMEDIATE_STATE psState, PINST psVMOVInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: ReplaceVMOVDestination

 PURPOSE	: Try to replace the source of a VMOV instruction by its destination.

 PARAMETERS	: psState				- Compiler state.
			  psVMOVInst			- VMOV instruction.

 RETURNS	: FALSE if the move source couldn't be replaced.
*****************************************************************************/
{
	ARG				sVMOVSrc;
	PARG			psVMOVDest;
	PARG			psVMOVOldDest, psDefInstOldDest;
	PUSEDEF_CHAIN	psVMOVSrcUseDef;
	IMG_UINT32		uSrcSwizzle = psVMOVInst->u.psVec->auSwizzle[0];
	IMG_BOOL		bSourceSwizzle;
	IMG_UINT32		uChansUsedFromVMOVSrc;
	PINST			psDefInst;
	IMG_UINT32		uDefDestIdx;
	IMG_BOOL		bSourceModifier;
	PINST			psInstBeforeMoved;
	PINST			psInstMoved;

	/*
		Skip moves between non-intermediate registers.
	*/
	sVMOVSrc = psVMOVInst->asArg[0];
	if (sVMOVSrc.uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	psVMOVDest = &psVMOVInst->asDest[0];
	if (psVMOVDest->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	/*
		Skip predicated moves.
	*/
	if (!NoPredicate(psState, psVMOVInst))
	{
		return IMG_FALSE;
	}

	/*
		Skip moves with a negate or absolute source modifier.
	*/
	if (psVMOVInst->u.psVec->asSrcMod[0].bNegate || psVMOVInst->u.psVec->asSrcMod[0].bAbs)
	{
		return IMG_FALSE;
	}

	/*
		Check if the move swizzles constant data into any of the channels of the result. In this case
		we can't apply the move's swizzle by modifying the swizzles on the instruction defining
		the move's source.
	*/
	if (!IsNonConstantSwizzle(uSrcSwizzle, psVMOVInst->auDestMask[0], NULL /* puMatchedSwizzle */))
	{
		return IMG_FALSE;
	}

	/*
		Get the list of defines and uses of the move source.
	*/
	psVMOVSrcUseDef = UseDefGet(psState, sVMOVSrc.uType, sVMOVSrc.uNumber);

	/*
		Get the instruction where the move source is defined.
	*/
	psDefInst = UseDefGetDefInstFromChain(psVMOVSrcUseDef, &uDefDestIdx);
	if (psDefInst == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Check if the VMOV is applying a swizzle or a format conversion.
	*/
	bSourceModifier = IMG_FALSE;
	bSourceSwizzle = IMG_FALSE;
	if (psVMOVDest->eFmt != sVMOVSrc.eFmt)
	{
		bSourceModifier = IMG_TRUE;
	}
	if (!CompareSwizzles(uSrcSwizzle, USEASM_SWIZZLE(X, Y, Z, W), psVMOVInst->auDestMask[0]))
	{
		bSourceSwizzle = IMG_TRUE;
		bSourceModifier = IMG_TRUE;
	}

	if (bSourceModifier)
	{
		/*
			Check the instruction defining the VMOV instruction's source can apply a format
			conversion to its destination and can apply a swizzle to the destination by
			swizzling its sources.
		*/
		if ((g_psInstDesc[psDefInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) == 0)
		{
			return IMG_FALSE;
		}
		/*
			Changing the source swizzle of a TEST instruction would require changing the
			predicate channel select.
		*/
		if (psDefInst->eOpcode == IVTEST)
		{
			return IMG_FALSE;
		}
		/*
			No swizzle is available on the mask of bytes used from source 0.
		*/
		if (psDefInst->eOpcode == IVMOVCU8_FLT)
		{
			return IMG_FALSE;
		}
	}

	psDefInstOldDest = psDefInst->apsOldDest[0];
	if (psDefInstOldDest != NULL)
	{
		/*
			Ensure the destination and old dest of the instruction will match after removing
			the VMOV.
		*/
		if (psDefInstOldDest->eFmt != psVMOVInst->asDest[0].eFmt)
		{
			return IMG_FALSE;
		}

		/*
			Ensure if we need to modify the swizzles of the instruction defining the VMOV 
			instruction's source that the source is completely defined there.
		*/
		if (bSourceSwizzle)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check the instruction defining the VMOV instruction's source is not writing
		any channels which aren't used in that source.
	*/
	uChansUsedFromVMOVSrc = GetLiveChansInArg(psState, psVMOVInst, 0 /* uArg */);
	if (psDefInst->auDestMask[uDefDestIdx] != uChansUsedFromVMOVSrc)
	{
		return IMG_FALSE;
	}
	
	/*
		Check if the VMOV instruction is copying the unwritten channels in its destination
		from another intermediate register that the intermediate register is defined
		before the instruction defining the VMOV instruction's source.
	*/
	psInstMoved = NULL;
	psInstBeforeMoved = NULL;
	psVMOVOldDest = psVMOVInst->apsOldDest[0];
	if (psVMOVOldDest != NULL)
	{
		PINST	psOldDestDefInst;

		/*
			Check if the instruction defining the VMOV's source is already copying data from a
			different intermediate register into unwritten channel in its destination.
		*/
		if (psDefInst->apsOldDest[uDefDestIdx] != NULL)
		{
			return IMG_FALSE;
		}

		/*
			Check if the instruction defining the VMOV old destination is either before the instruction
			defining the VMOV source or can be moved before it.
		*/
		if (psVMOVOldDest->uType != USEASM_REGTYPE_TEMP)
		{
			return IMG_FALSE;
		}
		psOldDestDefInst = UseDefGetDefInst(psState, psVMOVOldDest->uType, psVMOVOldDest->uNumber, NULL /* puDestIdx */);
		if (psOldDestDefInst != NULL)
		{
			if (psOldDestDefInst->psBlock != psDefInst->psBlock)
			{
				return IMG_FALSE;
			}

			if (psOldDestDefInst->uBlockIndex >= psDefInst->uBlockIndex)
			{
				psInstBeforeMoved = psOldDestDefInst->psPrev;

				if (!MoveInstUp(psState, psOldDestDefInst, psDefInst, bCheckOnly))
				{
					return IMG_FALSE;
				}
	
				psInstMoved = psOldDestDefInst;
			}
		}
	}

	/*
		Check if the VMOV instruction's source is also used as a source to other instructions.
	*/
	if (!UseDefIsSingleSourceUse(psState, psVMOVInst, 0 /* uSrcIdx */, &sVMOVSrc))
	{
		VMOV_REPLACE_CONTEXT	sContext;
		IMG_UINT32				uInverseSwizzle;

		/*
			Check if the VMOV instruction is reducing the precision of its source but
			other instructions still need the higher precision result.
		*/
		if (psVMOVDest->eFmt == UF_REGFORMAT_F16 && sVMOVSrc.eFmt == UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
	
		if ((g_psInstDesc[psDefInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
		{
			IMG_UINT32	auSrcChan[VECTOR_LENGTH];
			IMG_UINT32	uChan;

			/*
				Record for each channel in the VMOV instruction's source the first channel in the
				destination it is copied to.
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				auSrcChan[uChan] = USC_UNDEF;
			}
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psVMOVInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					USEASM_SWIZZLE_SEL	eSrcChan;

					eSrcChan = GetChan(uSrcSwizzle, uChan);
	
					ASSERT(eSrcChan >= USEASM_SWIZZLE_SEL_X && eSrcChan <= USEASM_SWIZZLE_SEL_W);
					
					if (auSrcChan[eSrcChan - USEASM_SWIZZLE_SEL_X] == USC_UNDEF)
					{
						auSrcChan[eSrcChan - USEASM_SWIZZLE_SEL_X] = uChan;
					}	
				}	
			}

			/*	
				Make a swizzle which inverts the VMOV instruction's source swizzle. This is always possible cause
				we checked above that the instruction defining the VMOV instruction's source doesn't write any
				channels which aren't used by the VMOV.
			*/
			uInverseSwizzle = 0;
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psDefInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					IMG_UINT32	uSrcChan;

					uSrcChan = auSrcChan[uChan];
					ASSERT(uSrcChan != USC_UNDEF);
					uInverseSwizzle = SetChan(uInverseSwizzle, uChan, USEASM_SWIZZLE_SEL_X + uSrcChan);
				}
			}
		}
		else
		{
			IMG_UINT32	uChan;

			/*
				The result of the instruction defining the VMOV's source is a scalar value broadcast to all
				channels so choose one channel from the new destination mask to use in all the other
				references to the VMOV instruction's source.
			*/
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psVMOVInst->auDestMask[0] & (1 << uChan)) != 0)
				{
					break;
				}
			}
			ASSERT(uChan < VECTOR_LENGTH);

			uInverseSwizzle = g_auReplicateSwizzles[uChan];
		}
		
		sContext.uReplaceChansMask = psDefInst->auDestMask[0];
		sContext.asChanArg = NULL;
		sContext.psVecArg = psVMOVDest;
		sContext.sSrcMod = psVMOVInst->u.psVec->asSrcMod[0];
		sContext.uSrcSwizzle = uInverseSwizzle;
		sContext.bScalarSources = IMG_FALSE;
		sContext.eSourceFormat = psVMOVDest->eFmt;
		sContext.eDestFormat = sVMOVSrc.eFmt;

		/*
			Check the format/swizzle can be modified at all the places where the VMOV instruction's
			source is used.
		*/
		if (!ReplaceAllUsesMasked(psState,
								  &sVMOVSrc,
								  psDefInst->auDestMask[0],
								  psDefInst->apsPredSrc, 
								  psDefInst->uPredCount,
								  GetPredicateNegate(psState, psDefInst),
								  GetPredicatePerChan(psState, psDefInst),
								  VMOVReplaceArguments,
								  &sContext,
								  bCheckOnly))
		{
			if (!bCheckOnly && psInstMoved != NULL)
			{
				/* Move the instruction back to where it was to avoid increase reg pressure. */
				RemoveInst(psState, psInstMoved->psBlock, psInstMoved);
				InsertInstAfter(psState, psInstBeforeMoved->psBlock, psInstMoved, psInstBeforeMoved);
			}
			return IMG_FALSE;
		}

		/*
			If the instruction defining the VMOV instruction's source copies data from another register into
			the channels of its destination that it doesn't write with the ALU result then insert extra
			instructions to perform the copies.
		*/
		if (!bCheckOnly)
		{
			CopyPartiallyOverwrittenData(psState, psDefInst);
		}
	}
	
	if (bCheckOnly)
	{
		return IMG_TRUE;
	}
		
	/*
		Apply the swizzle on the VMOV instruction's source by swizzling the sources of the defining
		instruction.
	*/
	if (bSourceSwizzle && (g_psInstDesc[psDefInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
	{
		IMG_UINT32	uSlot;

		for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psDefInst); uSlot++)
		{
			psDefInst->u.psVec->auSwizzle[uSlot] = CombineSwizzles(psDefInst->u.psVec->auSwizzle[uSlot], uSrcSwizzle);
		}
	}

	/*
		Move the VMOV instruction's destination to the destination of the instruction which
		defines the VMOV instruction's source.
	*/
	if (psVMOVInst->apsOldDest[0] != NULL)
	{
		CopyPartiallyWrittenDest(psState, psDefInst, uDefDestIdx, psVMOVInst, 0 /* uMoveFromDestIdx */);
	}
	MoveDest(psState, psDefInst, uDefDestIdx, psVMOVInst, 0 /* uMoveFromDestIdx */);
	psDefInst->auDestMask[uDefDestIdx] = psVMOVInst->auDestMask[0];
	psDefInst->auLiveChansInDest[uDefDestIdx] = psVMOVInst->auLiveChansInDest[0];

	/*
		Drop the VMOV instruction.
	*/
	RemoveInst(psState, psVMOVInst->psBlock, psVMOVInst);
	FreeInst(psState, psVMOVInst);
	
	return IMG_TRUE;
}

static IMG_BOOL TryEliminateVMOV(PINTERMEDIATE_STATE psState,  PINST psVMOVInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: TryEliminateVMOV

 PURPOSE	: Try to replace the destination of a VMOV instruction by its source.

 PARAMETERS	: psState				- Compiler state.
			  psVMOVInst			- VMOV instruction.
			  bCheckOnly			- If TRUE, don't modify/remove the instruction, 
									  just check whether it's possible to

 RETURNS	: FALSE if the move destination couldn't be replaced.
*****************************************************************************/
{
	IMG_UINT32				uChan;
	IMG_UINT32				uLiveChansMask;
	VMOV_REPLACE_CONTEXT	sContext;
	PARG					psSrcArg;
	IMG_UINT32				uVecLen;
	static const IMG_UINT32	auF32ChanToMask[] = {USC_X_CHAN_MASK, USC_Y_CHAN_MASK, USC_Z_CHAN_MASK, USC_W_CHAN_MASK};
	static const IMG_UINT32	auF16ChanToMask[] = {USC_XY_CHAN_MASK, USC_ZW_CHAN_MASK};
	static const USEASM_SWIZZLE_SEL	aeChanSel[] = 
	{
		USEASM_SWIZZLE_SEL_X, 
		USEASM_SWIZZLE_SEL_Y,
		USEASM_SWIZZLE_SEL_Z,
		USEASM_SWIZZLE_SEL_W,
	};
	IMG_UINT32 const *		auChanToMask;

	/*
		If the VMOV's source argument and the source for unwritten channels in the destination
		are the same then simplify the instruction.

			VMOV	rM.X[=rN], rN
		->
			VMOV	rM, rN 
	*/
	if (
			psVMOVInst->apsOldDest[0] != NULL && 
			EqualArgs(psVMOVInst->apsOldDest[0], &psVMOVInst->asArg[0]) &&
			!psVMOVInst->u.psVec->asSrcMod[0].bNegate &&
			!psVMOVInst->u.psVec->asSrcMod[0].bAbs
		)
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uNewSwizzle;

		uNewSwizzle = psVMOVInst->u.psVec->auSwizzle[0];
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if ((psVMOVInst->auLiveChansInDest[0] & (1U << uChan)) != 0 && (psVMOVInst->auDestMask[0] & (1U << uChan)) == 0)
			{
				uNewSwizzle = SetChan(uNewSwizzle, uChan, aeChanSel[uChan]);
			}
		}
		psVMOVInst->u.psVec->auSwizzle[0] = uNewSwizzle;

		psVMOVInst->auDestMask[0] = psVMOVInst->auLiveChansInDest[0];
		SetPartiallyWrittenDest(psState, psVMOVInst, 0 /* uDestIdx */, NULL /* psPartialDest */);

		if (!NoPredicate(psState, psVMOVInst))
		{
			ClearPredicates(psState, psVMOVInst);
		}
	}

	/*
		Get the mask of channels used from the instruction's
		source argument.
	*/
	uLiveChansMask = GetLiveChansInArg(psState, psVMOVInst, 0);

	/*
		Check for a move of a vector of scalar registers.
	*/
	if (psVMOVInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		IMG_UINT32	uReferencedScalarCount;

		/*
			Get the number of 32-bit scalar registers corresponding
			to a vector register.
		*/
		if (psVMOVInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
		{
			uVecLen = SCALAR_REGISTERS_PER_F16_VECTOR;
			auChanToMask = auF16ChanToMask;
		}
		else
		{
			ASSERT(psVMOVInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32);
			uVecLen = SCALAR_REGISTERS_PER_F32_VECTOR;
			auChanToMask = auF32ChanToMask;
		}

		/*	
			Check the scalar registers moved by the instruction all have the
			same type.
		*/
		psSrcArg = NULL;
		uReferencedScalarCount = 0;
		for (uChan = 0; uChan < uVecLen; uChan++)
		{
			if (uLiveChansMask & auChanToMask[uChan])
			{	
				PARG	psScalarChan = &psVMOVInst->asArg[1 + uChan];

				/*
					Check if the scalar registers are elements of a non-constant, indexable array. 
				*/
				if (IsModifiableRegisterArray(psState, psScalarChan->uType, psScalarChan->uNumber))
				{
					return IMG_FALSE;
				}

				if (psSrcArg == NULL)
				{	
					psSrcArg = psScalarChan;
				}
				else
				{
					if (psSrcArg->uType != psScalarChan->uType)
					{
						return IMG_FALSE;
					}
				}
				uReferencedScalarCount++;
			}
		}
		
		sContext.bScalarSources = IMG_TRUE;
	}
	else
	{
		/*
			Replace the destination by another vector register.
		*/
		sContext.bScalarSources = IMG_FALSE;
		psSrcArg = &psVMOVInst->asArg[0];
	}

	/*
		Simple case: no format conversion, no source modifier and no swizzle.
	*/
	if (
			!sContext.bScalarSources && 
			psVMOVInst->asDest[0].eFmt == psVMOVInst->asArg[0].eFmt &&
			(psVMOVInst->auLiveChansInDest[0] & ~psVMOVInst->auDestMask[0]) == 0 &&
			!psVMOVInst->u.psVec->asSrcMod[0].bNegate &&
			!psVMOVInst->u.psVec->asSrcMod[0].bAbs &&
			NoPredicate(psState, psVMOVInst) &&
			CompareSwizzles(psVMOVInst->u.psVec->auSwizzle[0], USEASM_SWIZZLE(X, Y, Z, W), psVMOVInst->auLiveChansInDest[0])
		)
	{
		IMG_BOOL	bExcludePrimaryEpilog;

		bExcludePrimaryEpilog = IMG_TRUE;
		if (psVMOVInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
		{
			bExcludePrimaryEpilog = IMG_FALSE;
		}

		/*
			For all uses of the destination: substitute by the source.
		*/
		if (UseDefSubstituteIntermediateRegister(psState, 
												 &psVMOVInst->asDest[0],
												 &psVMOVInst->asArg[0],
												 #if defined(SRC_DEBUG)
												 psVMOVInst->uAssociatedSrcLine,
 												 #endif /* defined(SRC_DEBUG) */
												 bExcludePrimaryEpilog,
												 IMG_TRUE /* bExcludeSecondaryEpilog */,
												 bCheckOnly))
		{
			return IMG_TRUE;
		}
	}

	/*
		Set up the context for VMOVReplaceArguments.
	*/
	sContext.uReplaceChansMask = psVMOVInst->auDestMask[0];
	sContext.asChanArg = &psVMOVInst->asArg[1];
	sContext.psVecArg = &psVMOVInst->asArg[0];
	sContext.sSrcMod = psVMOVInst->u.psVec->asSrcMod[0];
	sContext.uSrcSwizzle = psVMOVInst->u.psVec->auSwizzle[0];
	sContext.eDestFormat = psVMOVInst->asDest[0].eFmt;
	sContext.eSourceFormat = psVMOVInst->u.psVec->aeSrcFmt[0];

	/*
		Try and replace the VMOV's destination by the sources.
	*/
	return ReplaceAllUsesMasked(psState,
								&psVMOVInst->asDest[0],
								psVMOVInst->auDestMask[0],
								psVMOVInst->apsPredSrc,
								psVMOVInst->uPredCount,
								GetPredicateNegate(psState, psVMOVInst),
								GetPredicatePerChan(psState, psVMOVInst),
								VMOVReplaceArguments,
								&sContext,
								bCheckOnly);	
}

IMG_INTERNAL
IMG_BOOL EliminateVMOV(PINTERMEDIATE_STATE psState,  PINST psVMOVInst, IMG_BOOL bCheckOnly)
/*****************************************************************************
 FUNCTION	: EliminateVMOV

 PURPOSE	: Try to replace the destination of a VMOV instruction by its source.

 PARAMETERS	: psState				- Compiler state.
			  psVMOVInst			- VMOV instruction.
			  bCheckOnly			- If TRUE, don't modify/remove the instruction, 
									  just check whether it's possible to

 RETURNS	: FALSE if the move destination couldn't be replaced.
*****************************************************************************/
{
	if (TryEliminateVMOV(psState, psVMOVInst, bCheckOnly))
	{
		if (!bCheckOnly)
		{
			CopyPartiallyOverwrittenData(psState, psVMOVInst);

			RemoveInst(psState, psVMOVInst->psBlock, psVMOVInst);
			FreeInst(psState, psVMOVInst);
		}

		return IMG_TRUE;
	}
	return ReplaceVMOVDestination(psState, psVMOVInst, bCheckOnly);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */


IMG_INTERNAL 
IMG_VOID EliminateMovesFromGPI(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: EliminateMovesFromGPI

 PURPOSE	: Eliminate moves from a GPI to a temp.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR sIter;

	InstListIteratorInitialize(psState, IMOV, &sIter);
	for (; InstListIteratorContinue(&sIter);)
	{
		PINST psInst = InstListIteratorCurrent(&sIter);
		InstListIteratorNext(&sIter);

		if (
				psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL &&
				psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL &&
				(NoPredicate(psState, psInst) || psInst->apsOldDest[0] == NULL) &&
				(
					psInst->asDest[0].eFmt == psInst->asArg[0].eFmt ||
					(
						psInst->asArg[0].eFmt == UF_REGFORMAT_U8 &&
						psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
						psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
						psInst->asArg[0].uNumber == 0
					)
				)
			)
		{
			ASSERT(psInst->uDestCount == 1);
			/*
				If the register is live out of the block try doing the replace globally.
			*/
			if (psInst->asDest[0].uType != USEASM_REGTYPE_INDEX &&
				psInst->asDest[0].uType != USC_REGTYPE_REGARRAY &&
				CanReplaceTempWithGPI(psState, psInst->asDest[0].uNumber, psInst->asArg[0].uNumber, IMG_FALSE))
			{
				EliminateGlobalMove(psState, psInst->psBlock, psInst, &psInst->asArg[0], &psInst->asDest[0], NULL /* psEvalList */);
			}
		}
	}
	InstListIteratorFinalise(&sIter);

	TESTONLY_PRINT_INTERMEDIATE("Eliminate Moves from GPI", "gpi_mov_elim", IMG_FALSE);
}

static
IMG_VOID BaseReplaceMOV(PINTERMEDIATE_STATE psState, PINST psInst, PWEAK_INST_LIST psEvalList)
/*****************************************************************************
 FUNCTION	: BaseReplaceMOV

 PURPOSE	: Try to get rid of a move instruction by either replacing the source
			  by the destination or vice-versa. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- MOV Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bValidFormat;

	ASSERT(psInst->eOpcode == IMOV || psInst->eOpcode == ISETL || psInst->eOpcode == ISAVL);
	ASSERT(psInst->uDestCount == 1);

	/*
		Don't try to replace internal registers.
	*/
	if (psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}

	/*
		Check if the move is predicated and the data copied into the destination
		if the predicate is false is initialized.
	*/
	if (!NoPredicate(psState, psInst) && psInst->apsOldDest[0] != NULL)
	{
		return;
	}

	/*
		Don't try to replace 'casts' of data from one format to another.
	*/
	bValidFormat = IMG_FALSE;
	if (psInst->asDest[0].eFmt == psInst->asArg[0].eFmt)
	{
		bValidFormat = IMG_TRUE;
	}
	if (
			psInst->asArg[0].eFmt == UF_REGFORMAT_U8 &&
			psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
			psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[0].uNumber == 0
		)
	{
		bValidFormat = IMG_TRUE;
	}
	if (!bValidFormat)
	{
		return;
	}

	/*
		Just drop a move with exactly the same source and destination.
	*/
	if (EqualArgs(&psInst->asDest[0], &psInst->asArg[0]))
	{
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
		return;
	}

	/*
		Try to replace the destination everywhere by the source.
	*/
	if (psInst->asDest[0].uType != USEASM_REGTYPE_INDEX &&
		psInst->asDest[0].uType != USC_REGTYPE_REGARRAY &&
		EliminateGlobalMove(psState, psInst->psBlock, psInst, &psInst->asArg[0], &psInst->asDest[0], psEvalList))
	{
		return;
	}

	/*
		Try to modify the instruction defining the source to define the
		destination instead.
	*/
	if (CombineRegistersBackwards(psState, psInst->psBlock, psInst, IMG_TRUE /* bCheckOnly */))
	{
		CombineRegistersBackwards(psState, psInst->psBlock, psInst, IMG_FALSE /* bCheckOnly */);
		return;
	}
}

IMG_INTERNAL
IMG_VOID ReplaceMOV(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ReplaceMOV

 PURPOSE	: Try to get rid of a move instruction by either replacing the source
			  by the destination or vice-versa. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- MOV Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	BaseReplaceMOV(psState, psInst, NULL /* psEvalList */);
}

static
IMG_VOID BaseReplaceFMOV(PINTERMEDIATE_STATE psState, PINST psInst, PWEAK_INST_LIST psEvalList)
/*****************************************************************************
 FUNCTION	: BaseReplaceFMOV

 PURPOSE	: Try to get rid of an FMOV instruction by replacing the destination
			  by the source and applying the negation/absolute modifier at
			  each use of the destination. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- FMOV Instruction.
			  psEvalList	- If non-NULL then instructions whose sources have been modified are
							appended to this list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IFMOV);

	if (!NoPredicate(psState, psInst))
	{
		return;
	}
	if (psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}
	if (psInst->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return;
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}
	if (psInst->asArg[0].eFmt != UF_REGFORMAT_F32)
	{
		return;
	}

	EliminateGlobalMove(psState, psInst->psBlock, psInst, &psInst->asArg[0], &psInst->asDest[0], psEvalList);
}

static
IMG_VOID ReplaceFMOV(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ReplaceFMOV

 PURPOSE	: Try to get rid of an FMOV instruction by replacing the destination
			  by the source and applying the negation/absolute modifier at
			  each use of the destination. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- FMOV Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	BaseReplaceFMOV(psState, psInst, NULL /* psEvalList */);
}

static
IMG_VOID BaseReplaceMOVPRED(PINTERMEDIATE_STATE psState, PINST psInst, PWEAK_INST_LIST psEvalList)
/*****************************************************************************
 FUNCTION	: BaseReplaceMOVPRED

 PURPOSE	: Try to get rid of a MOVPRED instruction by replacing the destination
			  by the source. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- MOVPRED Instruction.
			  psEvalList	- New/modified move instructions are appended to
							this list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psPredSrc = &psInst->asArg[0];
	PARG		psPredDest = &psInst->asDest[0];
	IMG_BOOL	bSrcNegate = psInst->u.psMovp->bNegate;
	IMG_BOOL	bReplacedAllUses;

	if (!NoPredicate(psState, psInst))
	{
		if (psPredSrc->uType == USC_REGTYPE_BOOLEAN)
		{
			PARG*		apsPredSrc = psInst->apsPredSrc;
			PARG		psPredOldDest = psInst->apsOldDest[TEST_PREDICATE_DESTIDX];
			IMG_BOOL	bFixedResult;
			IMG_BOOL	bPredNeg = GetBit(psInst->auFlag, INST_PRED_NEG) ? IMG_TRUE : IMG_FALSE;

			bFixedResult = psPredSrc->uNumber ? IMG_TRUE : IMG_FALSE;
			if (bSrcNegate)
			{
				bFixedResult = !bFixedResult;
			}

			/*
				If the TEST instruction doesn't change its destination then we can replace the
				destination everywhere by the conditionally overwritten destination.

					q = IF p THEN true ELSE p	->	q = IF p THEN true ELSE false		-> q = p
					q = IF p THEN false ELSE p	->	q = IF p THEN false ELSE false 		-> q = false
					q = IF !p THEN false ELSE p	->	q = IF !p THEN false ELSE true		-> q = p
					q = IF !p THEN true ELSE p	->	q = IF !p THEN true ELSE true		-> q = true
			*/
			if (psPredOldDest != NULL && EqualArgs(psPredOldDest, apsPredSrc[0]))
			{
				if ((bFixedResult && !bPredNeg) || (!bFixedResult && bPredNeg))
				{
					MovePartialDestToSrc(psState, psInst, 0 /* uSrcIdx */, psInst, 0 /* uDestIdx */);
				}
				else
				{
					SetSrc(psState, psInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, bFixedResult, UF_REGFORMAT_F32);
				}

				ClearPredicates(psState, psInst);
			}
		}
	}

	if (!NoPredicate(psState, psInst))
	{
		return;
	}

	if (psPredSrc->uType == USC_REGTYPE_BOOLEAN)
	{
		IMG_BOOL	bFixedValue;

		bFixedValue =  psPredSrc->uNumber ? IMG_TRUE : IMG_FALSE;
		if (bSrcNegate)
		{
			bFixedValue = (!bFixedValue) ? IMG_TRUE : IMG_FALSE;
		}

		bReplacedAllUses = EliminatePredicateMove(psState,
												  psInst,
												  psPredDest->uNumber,
												  USC_UNDEF /* uReplacementPredNum */,
												  IMG_FALSE /* bSourcePredNegate */,
												  bFixedValue,
												  psEvalList);
	}
	else
	{
		ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
		bReplacedAllUses = EliminatePredicateMove(psState,
												  psInst,
												  psPredDest->uNumber,
												  psPredSrc->uNumber,
												  bSrcNegate,
												  IMG_FALSE /* bFixedValue */,
												  psEvalList);
	}
	if (bReplacedAllUses)
	{
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
	}
}

static
IMG_VOID ReplaceMOVPRED(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ReplaceMOVPRED

 PURPOSE	: Try to get rid of an FMOV instruction by replacing the destination
			  by the source. On success the instruction is freed.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- MOVPRED Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	BaseReplaceMOVPRED(psState, psInst, NULL /* psEvalList */);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ReplaceVMOV(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ReplaceVMOV

 PURPOSE	: Try to replace the destination of a VMOV instruction by its source.

 PARAMETERS	: psState				- Compiler state.
			  psVMOVInst			- VMOV instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	EliminateVMOV(psState, psInst, IMG_FALSE /* bCheckOnly */);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID EliminateMovesFromList(PINTERMEDIATE_STATE psState, PUSC_LIST psList)
/*****************************************************************************
 FUNCTION	: EliminateMovesFromList

 PURPOSE	: For all the move instructions in a list try to replace the move
			  destination by the source.

 PARAMETERS	: psState		- Compiler state.
			  psList		- List of instructions.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	WEAK_INST_LIST	sEvalList;

	WeakInstListInitialize(&sEvalList);
	while ((psListEntry = RemoveListHead(psList)) != NULL)
	{
		PINST	psMovInst;

		psMovInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sTempListEntry);
		switch (psMovInst->eOpcode)
		{
			case IMOV:
			{
				PWEAK_INST_LIST	psEvalList;

				if (psMovInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE ||
					psMovInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT)
				{
					psEvalList = &sEvalList;
				}
				else
				{
					psEvalList = NULL;
				}
				BaseReplaceMOV(psState, psMovInst, psEvalList); 
				break;
			}
			case IUNPCKF32F16:
			{
				IMG_BOOL bNewMoves;
				EliminateF16Move(psState, psMovInst, &bNewMoves);
				break;
			}
			case IFMOV:
			{
				ReplaceFMOV(psState, psMovInst);
				break;
			}
			case IMOVPRED:
			{
				ReplaceMOVPRED(psState, psMovInst);
				break;
			}
			default: imgabort();
		}
	}

	ApplyArithmeticSimplificationsToList(psState, &sEvalList);
}

IMG_INTERNAL
IMG_VOID EliminateMoves(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: EliminateMoves

 PURPOSE	: For all the move instructions in the program try to replace the move
			  destination by the source.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IOPCODE		eOpcode;
		IMG_VOID	(*pfnReplace)(PINTERMEDIATE_STATE, PINST);
	} asMoveTypes[] =
	{
		{IMOV,		ReplaceMOV},
		{IMOVPRED,	ReplaceMOVPRED},
		{IFMOV,		ReplaceFMOV},
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		{IVMOV,		ReplaceVMOV},
		{IVMOV,		ReplaceVMOV},
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	IMG_UINT32	uMoveType;

	for (uMoveType = 0; uMoveType < (sizeof(asMoveTypes) / sizeof(asMoveTypes[0])); uMoveType++)
	{
		ForAllInstructionsOfType(psState, asMoveTypes[uMoveType].eOpcode, asMoveTypes[uMoveType].pfnReplace);
	}
}

static PINST MakeReplacementMove(PINTERMEDIATE_STATE	psState, 
								 PINST					psInst, 
								 IOPCODE				eMovType,
								 IMG_UINT32				uDest, 
								 PINST*					ppsFirstMoveInst, 
								 PINST*					ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: MakeReplacementMove

 PURPOSE	: Create a move instruction writing to one of the destinations of
			  an existing instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Existing instruction.
			  eMovType			- Type of move instruction to generate.
			  uDest				- Index of the destination.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: The created move instruction.
*****************************************************************************/
{
	PINST	psMoveInst;

	psMoveInst = AllocateInst(psState, psInst);
	
	SetOpcode(psState, psMoveInst, eMovType);
		
	MoveDest(psState, psMoveInst, 0 /* uMoveToDestIdx */, psInst, uDest);
	CopyPredicate(psState, psMoveInst, psInst);
	CopyPartiallyWrittenDest(psState, psMoveInst, 0 /* uMoveToDestIdx */, psInst, uDest);
	psMoveInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDest];
		
	InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst);

	if ((*ppsFirstMoveInst) == NULL)
	{
		(*ppsFirstMoveInst) = psMoveInst;
	}
	*ppsLastMoveInst = psMoveInst;

	return psMoveInst;
}

static 
IMG_VOID MakeReplacementImmediateMove(PINTERMEDIATE_STATE	psState, 
									  PINST					psInst, 
									  IMG_UINT32			uDest, 
									  IMG_UINT32			uImmediateValue,
									  PINST*				ppsFirstMoveInst,
									  PINST*				ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: MakeReplacementMove

 PURPOSE	: Create a move instruction writing an immediate value to one of 
			  the destinations of an existing instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Existing instruction.
			  uDest				- Index of the destination.
			  uImmediateValue	- Immediate value to write.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMoveInst;

	psMoveInst = MakeReplacementMove(psState, psInst, IMOV, uDest, ppsFirstMoveInst, ppsLastMoveInst);
	SetSrc(psState, psMoveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, uImmediateValue, UF_REGFORMAT_F32);
}

static 
IMG_VOID MakeReplacementSourceCopy(PINTERMEDIATE_STATE	psState, 
								   PINST				psInst, 
								   IMG_UINT32			uDest, 
								   IMG_UINT32			uSrcToCopy,
								   PINST*				ppsFirstMoveInst,
								   PINST*				ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: MakeReplacementMove

 PURPOSE	: Create a move instruction copying an existing instruction's
			  source argument to one of the same instruction's destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Existing instruction.
			  uDest				- Index of the destination.
			  uSrcToCopy		- Index of the source to copy from.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMoveInst;

	psMoveInst = MakeReplacementMove(psState, psInst, IMOV, uDest, ppsFirstMoveInst, ppsLastMoveInst);
	MoveSrc(psState, psMoveInst, 0 /* uMoveToSrcIdx */, psInst, uSrcToCopy);
}

static
IMG_BOOL SimplifyFMOV(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyFMOV

 PURPOSE	: Apply arithmetic rules to simplify an FMOV instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- FMOV instruction.

 RETURNS	: TRUE if the instruction was converted to a simple move.
*****************************************************************************/
{
	/*
		Convert a move with source modifier to an ordinary MOV if the
		source modifier has disappeared.
	*/
	if (
			(
				psInst->eOpcode == IFMOV || 
				psInst->eOpcode == IFMOV16
			) &&
			!HasSourceModifier(psState, psInst, 0 /* uArgIdx */)
	   )
	{
		SetOpcode(psState, psInst, IMOV);
		return IMG_TRUE;
	}

	/*
		Simplify a move with source modifier where the source
		register is a hardware constant.
	*/
	if (psInst->eOpcode == IFMOV &&
		psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT)
	{
		PFLOAT_SOURCE_MODIFIER	psMod = &psInst->u.psFloat->asSrcMod[0];

		/*
			Hardware constants are never negative so we can remove the
			absolute modifier.
		*/
		if (psMod->bAbsolute)
		{
			psMod->bAbsolute = IMG_FALSE;
		}
		/*
			Remove a negation of zero.
		*/
		if (psMod->bNegate &&
			psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO)
		{
			psMod->bNegate = IMG_FALSE;
		}

		/*
			If all source modifiers have been removed try simplifying the
			instruction.
		*/
		if (!psMod->bNegate)
		{
			SetOpcode(psState, psInst, IMOV);
			return IMG_TRUE;
		}		
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL SimplifyTESTPRED(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyTESTPRED

 PURPOSE	: Apply arithmetic rules to simplify a TESTPRED instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- TESTPRED instruction.

 RETURNS	: TRUE if the instruction was converted to a simple move.
*****************************************************************************/
{
	IMG_BOOL	bFixedResult;

	ASSERT(psInst->eOpcode == ITESTPRED);
	if (psInst->u.psTest->eAluOpcode == IFADD || psInst->u.psTest->eAluOpcode == IFSUB)
	{
		IMG_UINT32	uSrc;

		for (uSrc = 0; uSrc < g_psInstDesc[psInst->u.psTest->eAluOpcode].uDefaultArgumentCount; uSrc++)
		{
			PARG		psSrc = &psInst->asArg[uSrc];
			IMG_BOOL	bZero;

			bZero = IMG_FALSE;
			if (psSrc->uType == USEASM_REGTYPE_IMMEDIATE && psSrc->uNumber == 0)
			{
				bZero = IMG_TRUE;
			}
			if (psSrc->uType == USEASM_REGTYPE_FPCONSTANT && psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO)
			{
				bZero = IMG_TRUE;
			}

			if (bZero && !(uSrc == 0 && psInst->u.psTest->eAluOpcode == IFSUB))
			{
				if (uSrc == 0)
				{
					MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
				}
				psInst->u.psTest->eAluOpcode = IFMOV;
				break;
			}
		}
	}

	if (IsFixedResultTest(psState, psInst, &bFixedResult) && psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		SetOpcode(psState, psInst, IMOVPRED);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, bFixedResult, UF_REGFORMAT_F32);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL ApplyTESTChannelSelect(PINTERMEDIATE_STATE psState, VTEST_CHANSEL eChanSel, IMG_BOOL abChanResult[VECTOR_LENGTH])
/*****************************************************************************
 FUNCTION	: ApplyTESTChannelSelect

 PURPOSE	: Applies the channel select operator from a TEST instruction to
			  the per-channel test results to generate a single result.

 PARAMETERS	: psState			- Compiler state.
			  eChanSel			- Test channel select.
			  abChanResult		- Per-channel test results.

 RETURNS	: The final instruction result.
*****************************************************************************/
{
	switch (eChanSel)
	{
		case VTEST_CHANSEL_ANDALL:
		{
			IMG_UINT32	uChan;

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if (!abChanResult[uChan])
				{
					return IMG_FALSE;
				}
			}
			return IMG_TRUE;
		}
		case VTEST_CHANSEL_ORALL:
		{
			IMG_UINT32	uChan;

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if (abChanResult[uChan])
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case VTEST_CHANSEL_XCHAN:
		{
			return abChanResult[USC_X_CHAN];
		}
		default: imgabort();
	}
}

static
IMG_VOID MakeReplacementMOVPRED(PINTERMEDIATE_STATE	psState,
								PINST				psInst,
								IMG_UINT32			uDest,
								IMG_BOOL			bFixedResult,
								PINST*				ppsFirstMoveInst, 
								PINST*				ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: MakeReplacementMOVPRED

 PURPOSE	: Create a move instruction copying a boolean constant to one of 
			  the destinations of an existing instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Existing instruction.
			  uDest				- Index of the destination.
			  bFixedResult		- Boolean constant.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMOVPREDInst;

	psMOVPREDInst = MakeReplacementMove(psState, 
										psInst, 
										IMOVPRED /* uMovType */, 
										uDest, 
										ppsFirstMoveInst, 
										ppsLastMoveInst);

	/* 
		The MOVPRED instruction automatically gets the same predicates as psInst.
		But MOVPRED doesn't allow per-channel predicatesm, so instead pick the predicate
		relevant for the destination we're copying. 
	*/
	if (GetBit(psInst->auFlag, INST_PRED_PERCHAN))
	{
		IMG_UINT32	uPredSrc;
		IMG_BOOL	bPredNegate, bPredPerChan;

		ClearPredicates(psState, psMOVPREDInst);
		SetBit(psMOVPREDInst->auFlag, INST_PRED_PERCHAN, 0);

		GetPredicate(psInst, &uPredSrc, &bPredNegate, &bPredPerChan, uDest);
		ASSERT(bPredPerChan);

		SetPredicateAtIndex(psState, psMOVPREDInst, uPredSrc, bPredNegate, 0);
	}

	SetSrc(psState, psMOVPREDInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, bFixedResult, UF_REGFORMAT_F32);
}

static
IMG_BOOL SimplifyVTEST(PINTERMEDIATE_STATE psState, PINST psInst, PINST* ppsFirstMoveInst, PINST* ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: SimplifyVTEST

 PURPOSE	: Try to simplify an VTEST instruction where all of the sources
			  are immediate values.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- VTEST instruction.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: TRUE if the VTEST instruction was simplified to a series of moves.
*****************************************************************************/
{
	IMG_UINT32			uSrc;
	IMG_UINT32			uChan;
	IMG_BOOL			abChanResult[VECTOR_LENGTH];

	ASSERT(psInst->eOpcode == IVTEST);

	/*
		Don't simplify instructions writing both a predicate result and a
		temporary register result.
	*/
	if (psInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}

	/*
		Check for an ALU opcode we know how to evaluate.
	*/
	if (psInst->u.psVec->sTest.eTestOpcode != IVADD)
	{
		return IMG_FALSE;
	}

	/*
		Check the values of both sources are known at compile-time.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		if (!IsImmediateVectorSource(psState, psInst, uSrc))
		{
			return IMG_FALSE;
		}
	}

	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_FLOAT	afSrcChanValue[2];
		IMG_FLOAT	fChanResult;

		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			IMG_UINT32	uChanValue;
			IMG_FLOAT	fChanValue;
			IMG_BOOL	bRet;

			/*
				Get the value of this source channel applying any swizzle.
			*/
			bRet = GetImmediateVectorChan(psState, psInst, uSrc, uChan, &uChanValue);
			ASSERT(bRet);

			if (psInst->u.psVec->aeSrcFmt[uSrc] == UF_REGFORMAT_F32)
			{
				fChanValue = *(IMG_PFLOAT)&uChanValue;
			}
			else
			{
				ASSERT(psInst->u.psVec->aeSrcFmt[uSrc] == UF_REGFORMAT_F16);
				fChanValue = ConvertF16ToF32(uChanValue);
			}

			/*
				Apply any source modifier on the instruction.
			*/
			if (psInst->u.psVec->asSrcMod[uSrc].bAbs)
			{
				if (fChanValue < 0.0f)
				{
					fChanValue = -fChanValue;
				}
			}
			if (psInst->u.psVec->asSrcMod[uSrc].bNegate)
			{
				fChanValue = -fChanValue;
			}

			afSrcChanValue[uSrc] = fChanValue; 
		}

		/*
			Do the TEST instruction ALU calculation.
		*/
		fChanResult = afSrcChanValue[0] + afSrcChanValue[1];

		/*
			Generate the TRUE/FALSE result for this channel from the ALU result.
		*/
		abChanResult[uChan] = DoF32Test(psState, psInst->u.psVec->sTest.eType, fChanResult);
	}

	/*
		Replace the VTEST by instructions copying a constant into the destination.
	*/
	if (psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN)
	{
		IMG_UINT32	uPredIdx;

		/*
			Copy the per-channel result into the corresponding destination.
		*/
		for (uPredIdx = 0; uPredIdx < psInst->uDestCount; uPredIdx++)
		{
			PARG	psDest = &psInst->asDest[uPredIdx];

			if (psDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				MakeReplacementMOVPRED(psState, psInst, uPredIdx, abChanResult[uPredIdx], ppsFirstMoveInst, ppsLastMoveInst);
			}
			else
			{
				ASSERT(psDest->uType == USC_REGTYPE_UNUSEDDEST);
			}
		}
	}
	else
	{
		IMG_BOOL	bFixedResult;
		IMG_UINT32	uPredIdx;

		/*
			Combine the per-channel results using the channel selector.
		*/
		bFixedResult = ApplyTESTChannelSelect(psState, psInst->u.psVec->sTest.eChanSel, abChanResult);

		/*
			Copy the combined result into the first destination.
		*/
		MakeReplacementMOVPRED(psState, psInst, 0 /* uDest */, bFixedResult, ppsFirstMoveInst, ppsLastMoveInst);

		/*
			The other destinations should be unused.
		*/
		for (uPredIdx = 1; uPredIdx < psInst->uDestCount; uPredIdx++)
		{
			PARG	psDest = &psInst->asDest[uPredIdx];
			
			ASSERT(psDest->uType == USC_REGTYPE_UNUSEDDEST);
		}
	}

	/*
		Free the VTEST instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
	
	return IMG_TRUE;
}

static 
IMG_BOOL IsVectorZero(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSlot)
/*****************************************************************************
 FUNCTION	: IsVectorZero

 PURPOSE	: Check if a source to a vector instruction is a vector of immediate 0.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSlot				- Source to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	UF_REGFORMAT	eSrcFormat = psInst->u.psVec->aeSrcFmt[uSlot];

	if (!IsImmediateVectorSource(psState, psInst, uSlot))
	{
		return IMG_FALSE;
	}
	if (eSrcFormat == UF_REGFORMAT_F32)
	{
		return IsReplicatedImmediateVector(psState, psInst, uSlot, FLOAT32_ZERO);
	}
	else
	{
		ASSERT(eSrcFormat == UF_REGFORMAT_F16);
		return IsReplicatedImmediateVector(psState, psInst, uSlot, FLOAT16_ZERO);
	}
}

static 
IMG_BOOL IsVectorOne(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSlot, IMG_PBOOL pbNegated)
/*****************************************************************************
 FUNCTION	: IsVectorOne

 PURPOSE	: Check if a source to a vector instruction is a vector of immediate 1.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSlot				- Source to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	UF_REGFORMAT	eSrcFormat = psInst->u.psVec->aeSrcFmt[uSlot];

	if (!IsImmediateVectorSource(psState, psInst, uSlot))
	{
		return IMG_FALSE;
	}
	*pbNegated = psInst->u.psVec->asSrcMod[uSlot].bNegate;
	if (eSrcFormat == UF_REGFORMAT_F32)
	{
		return IsReplicatedImmediateVector(psState, psInst, uSlot, FLOAT32_ONE);
	}
	else
	{
		ASSERT(eSrcFormat == UF_REGFORMAT_F16);
		return IsReplicatedImmediateVector(psState, psInst, uSlot, FLOAT16_ONE);
	}
}

static
IMG_BOOL SimplifyVADD(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyVADD

 PURPOSE	: Apply arithmetic simplifications to a vector add instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	IMG_BOOL	bZeroArg;
	
	/*
		VADD	DEST, SRC0, #0		-> VMOV	DEST, SRC0
		VADD	DEST, #0, SRC0
	*/

	bZeroArg = IMG_FALSE;
	if (IsVectorZero(psState, psInst, 0 /* uArgIdx */))
	{
		MoveVectorSource(psState, psInst, 0 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */, IMG_TRUE /* bMoveSourceModifier */);
		bZeroArg = IMG_TRUE;
	}
	else if (IsVectorZero(psState, psInst, 1 /* uArgIdx */))
	{
		bZeroArg = IMG_TRUE;
	}

	if (bZeroArg)
	{
		ModifyOpcode(psState, psInst, IVMOV);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL SimplifyVMULVMAD(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyVMULVMAD

 PURPOSE	: Apply arithmetic simplifications to a vector multiply or
			  multiply-add instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	/*
		VMAD	DEST, SRC0, SRC1, #0	-> VMUL DEST, SRC0, SRC1
	*/
	if (psInst->eOpcode == IVMAD && IsVectorZero(psState, psInst, 2 /* uArgIdx */))
	{
		ModifyOpcode(psState, psInst, IVMUL);
	}

	/*
		VMAD	DEST, #1, SRC1, SRC2	-> VADD	DEST, SRC1, SRC2
		VMAD	DEST, SRC1, #1, SRC2

		VMUL	DEST, #1, SRC1			-> VMOV DEST, SRC1
		VMUL	DEST, SRC1, #1
	*/
	if (psInst->eOpcode == IVMAD || psInst->eOpcode == IVMUL)
	{
		IMG_BOOL	bOneArg;
		IMG_BOOL	bNegated;

		bOneArg = IMG_FALSE;
		if (IsVectorOne(psState, psInst, 0 /* uArgIdx */, &bNegated))
		{
			MoveVectorSource(psState, psInst, 0 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */, IMG_TRUE /* bMoveSourceModifier */);
			bOneArg = IMG_TRUE;
		}
		else if (IsVectorOne(psState, psInst, 1 /* uArgIdx */, &bNegated))
		{
			bOneArg = IMG_TRUE;
		}

		if (bOneArg)
		{
			if (bNegated)
			{
				psInst->u.psVec->asSrcMod[0].bNegate = !psInst->u.psVec->asSrcMod[0].bNegate;
			}
			if (psInst->eOpcode == IVMAD)
			{
				MoveVectorSource(psState, 
								 psInst, 
								 1 /* uDestArgIdx */, 
								 psInst, 
								 2 /* uSrcArgIdx */,
								 IMG_TRUE /* bMoveSourceModifier */);
				ModifyOpcode(psState, psInst, IVADD);
				return SimplifyVADD(psState, psInst);
			}
			else
			{
				ModifyOpcode(psState, psInst, IVMOV);
				return IMG_TRUE;
			}
		}
	}
	/*
		VMAD	DEST, #0, SRC1, SRC2	-> VMOV	DEST, SRC2
		VMAD	DEST, SRC1, #0, SRC2

		VMUL	DEST, #0, SRC1			-> VMOV DEST, #0
		VMUL	DEST, SRC1, #0
	*/
	if (psInst->eOpcode == IVMAD || psInst->eOpcode == IVMUL)
	{
		if (IsVectorZero(psState, psInst, 0 /* uArgIdx */) || IsVectorZero(psState, psInst, 1 /* uArgIdx */))
		{
			if (psInst->eOpcode == IVMAD)
			{	
				MoveVectorSource(psState, 
								 psInst, 
								 0 /* uDestArgIdx */, 
								 psInst, 
								 2 /* uSrcArgIdx */, 
								 IMG_TRUE /* bMoveSourceModifier */);
			}
			else
			{
				psInst->u.psVec->aeSrcFmt[0] = psInst->asDest[0].eFmt;
				psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
				SetImmediateVector(psState, psInst, 0 /* uSrcIdx */, 0 /* uImm */);	
			}
			ModifyOpcode(psState, psInst, IVMOV);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_BOOL GetImmediateValue(PINTERMEDIATE_STATE psState, PARG psSrc, IMG_PUINT32 puValue)
{
	switch(psSrc->uType)
	{
		case USEASM_REGTYPE_FPCONSTANT:
		{
			*puValue = GetHardwareConstantValueu(psState, psSrc->uNumber);
			return IMG_TRUE;
		}
		case USEASM_REGTYPE_IMMEDIATE:
		{
			*puValue = psSrc->uNumber;
			return IMG_TRUE;
		}
		default:
		{
			*puValue = 0;
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL SimplifyMOVC(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyMOVC

 PURPOSE	: Try to simplify an MOVC/MOVC_I32 instruction where the source is
			  an immediate value.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- MOVC/MOVC_I32 instruction.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	PARG		psCondSrc;
	IMG_UINT32	uValue;
	IMG_BOOL	bResult;

	ASSERT(psInst->eOpcode == IMOVC || psInst->eOpcode == IMOVC_I32);
	psCondSrc = &psInst->asArg[0];

	/*
		Simplify a conditional move where the first argument is known.
	*/
	ASSERT(!IsNegated(psState, psInst, 0 /* uArgIdx */));
	if (!GetImmediateValue(psState, psCondSrc, &uValue))
	{
		return IMG_FALSE;
	}

	/*
		Do the MOVC test.
	*/
	if (psInst->eOpcode == IMOVC)
	{
		IMG_FLOAT	fValue;

		fValue = *(IMG_PFLOAT)&uValue;
		bResult = DoF32Test(psState, psInst->u.psMovc->eTest, fValue);
	}
	else
	{
		ASSERT(psInst->eOpcode == IMOVC_I32);
		bResult = DoBitwiseTest(psState, psInst->u.psMovc->eTest, uValue);
	}

	if (bResult)
	{
		/*
			Do an unconditional move of the second argument.
		*/
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
	}
	else
	{
		/*
			Do an unconditional move of the third argument.
		*/
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 2 /* uCopyFromSrcIdx */);
	}
	SetOpcode(psState, psInst, IMOV);

	return IMG_TRUE;
}

#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_BOOL SimplifyIMA32(PINTERMEDIATE_STATE psState, PINST psInst, PINST* ppsFirstMoveInst, PINST* ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: SimplifyIMA32

 PURPOSE	: Try to simplify an IMA32 instruction where some/all of the sources
			  are immediate values.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- IMA32 instruction.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: TRUE if the IMA32 instruction was simplified to a series of moves.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_INT64	ai64ArgValue[IMA32_SOURCE_COUNT];
	IMG_UINT32	uDest;
	IMG_BOOL	abArgKnownValue[IMA32_SOURCE_COUNT];
	IMG_BOOL	bSimplified;
	IMG_UINT32	uDestCount;

	ASSERT(psInst->eOpcode == IIMA32);
	ASSERT(psInst->uDestCount <= 2);

	uDestCount = psInst->uDestCount;
	if (uDestCount == 2 && psInst->auLiveChansInDest[1] == 0)
	{
		uDestCount = 1;
	}

	if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if any of the source arguments are immediates.
	*/
	for (uArg = 0; uArg < IMA32_SOURCE_COUNT; uArg++)
	{
		PARG		psArg = &psInst->asArg[uArg];
		IMG_UINT32	uImmValue;

		abArgKnownValue[uArg] = GetImmediateValue(psState, psArg, &uImmValue);
		if (!abArgKnownValue[uArg])
		{
			continue;
		}

		/*
			Sign-extend the instruction source from 32-bits to 64-bits.
		*/
		if (psInst->u.psIma32->bSigned)
		{
			ai64ArgValue[uArg] = (IMG_INT64)(IMG_INT32)uImmValue;
		}
		else
		{
			ai64ArgValue[uArg] = uImmValue;
		}

		/*
			Apply any instruction negate source modifier.
		*/
		if (psInst->u.psIma32->abNegate[uArg])
		{
			ai64ArgValue[uArg] = -ai64ArgValue[uArg];
		}
	}

	/*
		If all the source arguments are immediate values then calculate the instruction
		result.
	*/
	bSimplified = IMG_FALSE;
	if (abArgKnownValue[0] && abArgKnownValue[1] && abArgKnownValue[2])
	{
		IMG_UINT64	ui64Result;
		IMG_INT64	i64Result;

		i64Result = ai64ArgValue[0] * ai64ArgValue[1] + ai64ArgValue[2];
		ui64Result = (IMG_UINT64)i64Result;

		/*
			Make move instructions copying the immediate instruction result
			into each of the destinations.
		*/
		for (uDest = 0; uDest < psInst->uDestCount; uDest++)
		{	
			IMG_UINT32	uDestValue;

			if (psInst->auLiveChansInDest[uDest] == 0)
			{
				continue;
			}

			uDestValue = (IMG_UINT32)(ui64Result >> (((IMG_UINT64)uDest) * ((IMG_UINT64)32)));

			MakeReplacementImmediateMove(psState, 
										 psInst, 
										 uDest, 
										 uDestValue,
										 ppsFirstMoveInst,
										 ppsLastMoveInst);
		}

		bSimplified = IMG_TRUE;
	}
	/*
		Otherwise check for:

			IMA32	DEST, SRC, #1, #0
		or	IMA32	DEST, #1, SRC, #0
	*/
	else if (abArgKnownValue[2] && ai64ArgValue[2] == 0 && (!psInst->u.psIma32->bSigned || uDestCount == 1))
	{
		IMG_UINT32	uArg;

		/*
			Check if one of the first two sources is immediate #0.
		*/
		for (uArg = 0; uArg < 2; uArg++)
		{
			if (abArgKnownValue[1 - uArg] && ai64ArgValue[1 - uArg] == 1 && !psInst->u.psIma32->abNegate[uArg])
			{
				break;
			}
		}

		if (uArg < 2)
		{
			/*
				Copy the non-immediate source directly into the destination for the first 32-bits of the IMA32
				result.
			*/
			MakeReplacementSourceCopy(psState, 
									  psInst, 
									  0 /* uDest */, 
									  uArg, 
									  ppsFirstMoveInst, 
									  ppsLastMoveInst);

			/*	
				Set the last 32-bits of the result to zero.
			*/
			if (uDestCount > 1)
			{
				MakeReplacementImmediateMove(psState, 
											 psInst, 
											 1 /* uDest */, 
											 0 /* uImmediateValue */,
											 ppsFirstMoveInst,
											 ppsLastMoveInst);
			}

			bSimplified = IMG_TRUE;
		}
	}

	if (bSimplified)
	{
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
	}

	return bSimplified;
}
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL SimplifyUNPCKF32U16(PINTERMEDIATE_STATE psState, PINST psInst, PINST* ppsFirstMoveInst, PINST* ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: SimplifyUNPCKF32U16

 PURPOSE	: Try to simplify an UNPCKF32U16 instruction where the source is
			  an immediate value.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- UNPCKF32U16 instruction.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: TRUE if the UNPCKF32U16 instruction was simplified to a move.
*****************************************************************************/
{
	IMG_UINT32	uArgValue;
	IMG_UINT32	uResult;
	IMG_UINT32	uComponent;
	IMG_FLOAT	fResult;

	if (!GetImmediateValue(psState, &psInst->asArg[0], &uArgValue))
	{
		return IMG_FALSE;
	}

	uComponent = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);
	uArgValue >>= (uComponent * BITS_PER_BYTE);
	uArgValue &= ((1 << (WORD_SIZE * BITS_PER_BYTE)) - 1);

	fResult = (IMG_FLOAT)uArgValue;
	if (psInst->u.psPck->bScale)
	{
		fResult /= 65535;
	}

	uResult = *(IMG_PUINT32)&fResult;

	MakeReplacementImmediateMove(psState, 
								 psInst, 
								 0 /* uDest */, 
								 uResult,
								 ppsFirstMoveInst,
								 ppsLastMoveInst);

	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);

	return IMG_TRUE;
}

static IMG_BOOL SimplifyShift(PINTERMEDIATE_STATE psState, PINST psInst, PINST* ppsFirstMoveInst, PINST* ppsLastMoveInst)
/*****************************************************************************
 FUNCTION	: SimplifyShift

 PURPOSE	: Try to simplify a SHL/SHR instruction where some of the sources are
			  immediate values.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- SHL/SHR instruction.
			  ppsFirstMoveInst	- On return points to the move instruction for
								the first instruction destination.
			  ppsLastMoveInst	- On return points to the move instruction for
								the last instruction destination.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	IMG_UINT32	auArgValue[2];
	IMG_BOOL	abArgKnownValue[2];
	IMG_UINT32	uArg;
	IMG_BOOL	bSimplified;

	for (uArg = 0; uArg < 2; uArg++)
	{
		abArgKnownValue[uArg] = GetImmediateValue(psState, &psInst->asArg[uArg], &auArgValue[uArg]);
	}

	bSimplified = IMG_FALSE;
	if (abArgKnownValue[0] && abArgKnownValue[1])
	{
		IMG_UINT32	uResult;

		switch (psInst->eOpcode)
		{
			case ISHR: uResult = auArgValue[0] >> auArgValue[1]; break;
			case ISHL: uResult = auArgValue[0] << auArgValue[1]; break;
			default: imgabort();
		}

		MakeReplacementImmediateMove(psState, 
									 psInst, 
									 0 /* uDest */, 
									 uResult,
									 ppsFirstMoveInst,
									 ppsLastMoveInst);
		bSimplified = IMG_TRUE;
	}
	/*	
		SHR/SHL	DEST, SRC, #0 -> MOV DEST, SRC
	*/
	else if (abArgKnownValue[1] && auArgValue[1] == 0)
	{
		MakeReplacementSourceCopy(psState, 
								  psInst, 
								  0 /* uDest */, 
								  0 /* uSrcToCopy */, 
								  ppsFirstMoveInst, 
								  ppsLastMoveInst);
		bSimplified = IMG_TRUE;
	}
	/*
		SHR/SHL	DEST, #0, SRC -> MOV DEST, #0
	*/
	else if (abArgKnownValue[0] && auArgValue[0] == 0)
	{
		MakeReplacementImmediateMove(psState, 
									 psInst, 
									 0 /* uDest */, 
									 0 /* uImmediateValue */,
									 ppsFirstMoveInst,
									 ppsLastMoveInst);
		bSimplified = IMG_TRUE;
	}

	if (bSimplified)
	{
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
	}

	return bSimplified;
}

static
IMG_BOOL SimplifyPCKF16F16(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SimplifyPCKF16F16

 PURPOSE	: Try to simplify a PCKF16F16 instruction where some of the sources are
			  immediate values.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- PCKF16F16 instruction.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == IPCKF16F16);

	if (
			psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
			(
				psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO || 
				psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE
			)
		)
	{
		if (
				psInst->auDestMask[0] == USC_ALL_CHAN_MASK &&
				psInst->asArg[1].uType == psInst->asArg[0].uType &&
				psInst->asArg[1].uNumber == psInst->asArg[0].uNumber
			)
		{
			SetOpcode(psState, psInst, IMOV);
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

#define INST_INEVALLIST		INST_LOCAL0

static
IMG_VOID AppendToEvalList(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psEvalList, PINST psInst)
/*****************************************************************************
 FUNCTION	: AppendToEvalList

 PURPOSE	: Append an instruction to the list of instructions to be
			  considered for arithmetic simplification.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_INEVALLIST) == 0)
	{
		SetBit(psInst->auFlag, INST_INEVALLIST, 1);
		WeakInstListAppend(psState, psEvalList, psInst);
	}
}

static
IMG_VOID ApplyArithmeticSimplificationsToList(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList)
{
	PINST	psEvalInst;

	while ((psEvalInst = WeakInstListRemoveHead(psState, psList)) != NULL)
	{
		PINST	psFirstMoveInst, psLastMoveInst;

		SetBit(psEvalInst->auFlag, INST_INEVALLIST, 0);

		/*
			Apply simplifications to this instruction.
		*/
		psFirstMoveInst = psLastMoveInst = NULL;
		switch (psEvalInst->eOpcode)
		{
			case ITESTPRED:
			{
				SimplifyInst(psState, psEvalInst);
				if (SimplifyTESTPRED(psState, psEvalInst))
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			case IVTEST:
			{
				SimplifyVTEST(psState, psEvalInst, &psFirstMoveInst, &psLastMoveInst);
				break;
			}
			case IVMUL:
			case IVMAD:
			case IVADD:
			{
				IMG_BOOL	bToMove;

				if (psEvalInst->eOpcode == IVMAD || psEvalInst->eOpcode == IVMUL)
				{
					bToMove = SimplifyVMULVMAD(psState, psEvalInst);
				}
				else
				{
					ASSERT(psEvalInst->eOpcode == IVADD);
					bToMove = SimplifyVADD(psState, psEvalInst);
				}
				if (bToMove)
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			case IMOVC:
			case IMOVC_I32:
			{
				if (SimplifyMOVC(psState, psEvalInst))
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			case IFMOV:
			case IFMOV16:
			{
				if (SimplifyFMOV(psState, psEvalInst))
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			case IFADD:
			case IFMUL:
			case IFMAD:
			case IFMUL16:
			case IFADD16:
			case IFMAD16:
			{
				if (SimplifyInst(psState, psEvalInst))
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			case IUNPCKF32U8:
			{
				PARG	psPckSrc = &psEvalInst->asArg[0];

				if (psPckSrc->uType == USEASM_REGTYPE_IMMEDIATE && psPckSrc->uNumber == 0)
				{
					SetOpcode(psState, psEvalInst, IMOV);
					psEvalInst->asArg[0].eFmt = UF_REGFORMAT_F32;
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			case ISHR:
			case ISHL:
			{
				SimplifyShift(psState, psEvalInst, &psFirstMoveInst, &psLastMoveInst);
				break;
			}
			case IUNPCKF32U16:
			{
				SimplifyUNPCKF32U16(psState, psEvalInst, &psFirstMoveInst, &psLastMoveInst);
				break;
			}
			case IPCKF16F16:
			{
				if (SimplifyPCKF16F16(psState, psEvalInst))
				{
					psFirstMoveInst = psLastMoveInst = psEvalInst;
				}
				break;
			}
			#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			case IIMA32:
			{
				SimplifyIMA32(psState, psEvalInst, &psFirstMoveInst, &psLastMoveInst);
				break;
			}
			#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			case IMOV:
			{
				psFirstMoveInst = psLastMoveInst = psEvalInst;
				break;
			}
			default:
			{
				break;
			}
		}

		/*
			If the instruction was simplified to a move of a constant then replace the
			destination everywhere by the constant. Instructions whose sources were
			modified are appended to the list of instructions to be evaluated.
		*/
		if (psFirstMoveInst != NULL)
		{
			IMG_BOOL	bExit;
			PINST		psMoveInst;

			psMoveInst = psFirstMoveInst;
			do
			{
				PINST	psNextMoveInst;

				bExit = IMG_FALSE;
				if (psMoveInst == psLastMoveInst)
				{
					bExit = IMG_TRUE;
				}
				psNextMoveInst = psMoveInst->psNext;

				switch (psMoveInst->eOpcode)
				{
					case IMOV:
					{
						BaseReplaceMOV(psState, psMoveInst, psList);
						break;
					}
					case IMOVPRED:
					{
						BaseReplaceMOVPRED(psState, psMoveInst, psList);
						break;
					}
					case IFMOV:
					{
						BaseReplaceFMOV(psState, psMoveInst, psList);
						break;
					}
					case IFMOV16:
					{
						IMG_BOOL	bNewMoves;
						ReplaceFMOV16(psState, psMoveInst, &bNewMoves, psList);
						break;
					}
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					case IVMOV:
					{
						ReplaceVMOV(psState, psMoveInst);
						break;
					}
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					default:
					{
						imgabort();
					}
				}

				psMoveInst = psNextMoveInst;
			} while (!bExit);
		}
	}

	UseDefDropUnusedTemps(psState);
}

static
IMG_VOID WeakInstListAppendInstsOfType(PINTERMEDIATE_STATE psState, PWEAK_INST_LIST psList, IOPCODE eOpcode)
{
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, eOpcode, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psEvalInst;

		psEvalInst = InstListIteratorCurrent(&sIter);
		AppendToEvalList(psState, psList, psEvalInst);
	}
	InstListIteratorFinalise(&sIter);
}

IMG_INTERNAL
IMG_VOID ArithmeticSimplification(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ArithmeticSimplification

 PURPOSE	: Apply arithmetic rules to simplify instructions in the program.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IOPCODE	aeEvaluableOpcodes[] =
	{
		ITESTPRED,
		IFMOV,
		IFMUL,
		IFADD,
		IFMAD,
		IFMOV16,
		IFMUL16,
		IFADD16,
		IFMAD16,
		IMOVC,
		IMOVC_I32,
		IUNPCKF32U8,
		ISHL,
		ISHR,
		IUNPCKF32U16,
		#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IIMA32,
		#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IVTEST,
		IVMUL,
		IVADD,
		IVMAD,
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	WEAK_INST_LIST	sEvalList;
	IMG_UINT32		uEvalType;

	/*
		Initialize the list of instructions to be considered.
	*/
	WeakInstListInitialize(&sEvalList);

	/*
		Add all the instructions which we have simplification rules for.
	*/
	for (uEvalType = 0; uEvalType < (sizeof(aeEvaluableOpcodes) / sizeof(aeEvaluableOpcodes[0])); uEvalType++)
	{
		WeakInstListAppendInstsOfType(psState, &sEvalList, aeEvaluableOpcodes[uEvalType]);
	}

	ApplyArithmeticSimplificationsToList(psState, &sEvalList);

	MergeAllBasicBlocks(psState);
}

IMG_INTERNAL
IMG_VOID EliminateMovesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: EliminateMovesBP

 PURPOSE	: For all the move instructions in a block try to replace the move
			  destination by the source.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;
	PINST	psNextInst;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		switch (psInst->eOpcode)
		{
			case IMOV: 
			{
				ReplaceMOV(psState, psInst); 
				break;
			}
			case IFMOV: 
			{
				ReplaceFMOV(psState, psInst); 
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID EliminateMovesAndFuncs(PINTERMEDIATE_STATE psState)
/******************************************************************************
 FUNCTION		: EliminateMovesAndFuncs

 DESCRIPTION	: Eliminates moves across the program; also tries to remove
				  functions made empty by move elimination (since this includes
				  removing no-longer-needed ISETL/ISAVL from functions)

 PARAMETERS		: psState	- Compiler intermediate state

 RETURNS		: Nothing
******************************************************************************/
{
	EliminateMoves(psState);
	MergeAllBasicBlocks(psState);
}

/*****************************************************************************
 FUNCTION	: NegateTest

 PURPOSE	: Generates a new test when the value being tested has been negated.

 PARAMETERS	: eOldTestType		- The test to negate.
			  peNewTestType		- Returns the new test.

 RETURNS	: TRUE if the test condition is a recognized one.
*****************************************************************************/
static IMG_BOOL NegateTest(TEST_TYPE eOldTestType, PTEST_TYPE peNewTestType)
{
	switch (eOldTestType)
	{
		/* GT -> LT */ case TEST_TYPE_GT_ZERO:	*peNewTestType = TEST_TYPE_LT_ZERO; break;
		/* GE -> LE */ case TEST_TYPE_GTE_ZERO:	*peNewTestType = TEST_TYPE_LTE_ZERO; break;
		/* EQ -> EQ */ case TEST_TYPE_EQ_ZERO:	*peNewTestType = TEST_TYPE_EQ_ZERO; break;
		/* LT -> GT */ case TEST_TYPE_LT_ZERO:	*peNewTestType = TEST_TYPE_GT_ZERO; break;
		/* LE -> GE */ case TEST_TYPE_LTE_ZERO:	*peNewTestType = TEST_TYPE_GTE_ZERO; break;
		/* NE -> NE */ case TEST_TYPE_NEQ_ZERO:	*peNewTestType = TEST_TYPE_NEQ_ZERO; break;
		default: return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsUnmergeableInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsUnmergeableInst

 PURPOSE	: Check for an instruction which we shouldn't try to merge
			  with other instructions computing the same result.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Don't merge an instruction writing a clipplane P flag.
	*/
	if (psInst->eOpcode == IFDPC)
	{
		return IMG_TRUE;
	}

	#if defined(OUTPUT_USPBIN)
	/*
		Don't merge USP texture samples.
	*/
	if	(
			(psInst->eOpcode >= ISMP_USP) && 
			(psInst->eOpcode <= ISMP_USP_NDR)
		)
	{
		return IMG_TRUE;
	}
	#endif /* #if defined(OUTPUT_USPBIN) */

	/*
		Don't merge instructions reading/writing local memory. Local memory doesn't have the SSA property
		so this avoids needing to keep track of read-after-write dependencies when merging instructions.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE) != 0)
	{
		return IMG_TRUE;
	}

	/*
		Don't merge instructions reading/writing intermediate registers which aren't in SSA form. 
	*/
	if (IsInstReferencingNonSSAData(psState, psInst))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_BOOL LogicalCombine(PINST psInst)
/*****************************************************************************
 FUNCTION	: LogicalCombine

 PURPOSE	: Check for an instruction which calculates the logical conjunction
			  or disjunction of its own result and another predicate.

 PARAMETERS	: psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInst->apsOldDest[0] == NULL)
	{
		return IMG_FALSE;
	}
	if (psInst->uPredCount != 1)
	{
		return IMG_FALSE;
	}
	if (!EqualArgs(psInst->apsOldDest[0], psInst->apsPredSrc[0]))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsTestIdempotent(PINTERMEDIATE_STATE psState, PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: IsTestIdempotent

 PURPOSE	: Check for case where two instructions which aren't structurally
			  equal compute the same result because of the idempotency of
			  conjunction.

 PARAMETERS	: psState			- Compiler state.
			  psInst1, psInst2	- Instructions to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check both instructions are defining only a predicate.
	*/
	if (!(psInst1->uDestCount == 1 && psInst1->asDest[0].uType == USEASM_REGTYPE_PREDICATE))
	{
		return IMG_FALSE;
	}
	if (!(psInst2->uDestCount == 1 && psInst2->asDest[0].uType == USEASM_REGTYPE_PREDICATE))
	{
		return IMG_FALSE;
	}

	/*
		Check the second instruction is calculating the conjuction of it's result and the
		result of the first instruction i.e.

			INST1	P = test(A)
		(P)	INST2	Q[=P] = test(B)

			<=>

			P = test(A)
			Q = if P then test(B) else P
	*/
	if (NoPredicate(psState, psInst2))
	{
		return IMG_FALSE;
	}
	if (!EqualArgs(&psInst1->asDest[0], psInst2->apsPredSrc[0]))
	{
		return IMG_FALSE;
	}
	if (!LogicalCombine(psInst2))
	{
		return IMG_FALSE;
	}

	/*
		Check the first instruction is either unconditional or is itself calculating the conjunction/disjunction of another
		predicate and its own result.

			P = if R then test(A) else R 
			Q = if P then test(A) else P
		=>
			P = R && test(A)

			Q = P && test(A)
			Q = R && test(A) && test(A)
			Q = R && test(A)
			Q = P
	*/
	if (!NoPredicate(psState, psInst1))
	{
		if (!LogicalCombine(psInst1))
		{
			return IMG_FALSE;
		}
		/*
			Check both instructions are either both conjunctions or both disjunctions.
		*/
		if (GetBit(psInst1->auFlag, INST_PRED_NEG) != GetBit(psInst2->auFlag, INST_PRED_NEG))
		{
			return IMG_FALSE;
		}
	}
			
	return IMG_TRUE;
}

static
IMG_INT32 CompareInstructions(PINTERMEDIATE_STATE psState, PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: CompareInstructions

 PURPOSE	: Compares two instructions.

 PARAMETERS	: psState			- Compiler state.
			  psInst1, psInst2	- Instructions to compare.

 RETURNS	: -ve		if the first instruction is less than the second.
			  0			if the first instruction is equal to the second.
			  +ve		if the first instruction is greater than the second.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;
	IMG_INT32	iCmp;

	if ((psInst1->eOpcode == IPCKF16F16) &&
		(psInst2->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE) &&
		(psInst2->asArg[1].uNumber == 0) &&
		(psInst2->auDestMask[0] == 3) &&
		(psInst1->uArgumentCount == 2) &&
		(psInst2->uArgumentCount == 2) &&
		(psInst1->uDestCount == 1) &&
		(psInst2->uDestCount == 1))
	{
		/* instruction 2 doesn't use its second argument so so long as its
		first argument is identical to inst 1 we can use inst 1 instead of it */
		const PCK_PARAMS *psPck1 = psInst1->u.psPck;
		const PCK_PARAMS *psPck2 = psInst2->u.psPck;

		iCmp = (IMG_INT32)(psPck1->auComponent[0] - psPck2->auComponent[0]);
		if (iCmp != 0)
		{
			return iCmp;
		}

		iCmp = psPck1->bScale - psPck2->bScale;
		if (iCmp != 0)
		{
			return iCmp;
		}

		if ((psInst1->auDestMask[0] & psInst2->auDestMask[0]) != psInst2->auDestMask[0])
		{
			return -1;
		}

		iCmp = (IMG_INT32)(psInst1->asDest[0].uType - psInst2->asDest[0].uType);
		if (iCmp != 0)
		{
			return iCmp;
		}

		if (psInst1->asDest[0].uType != USC_REGTYPE_UNUSEDDEST)
		{
			ASSERT(psInst2->asDest[0].uType != USC_REGTYPE_UNUSEDDEST);

			iCmp = psInst1->asDest[0].eFmt - psInst2->asDest[0].eFmt;
			if (iCmp != 0)
			{
				return iCmp;
			}
		}

		iCmp = CompareArgs(&psInst1->asArg[0], &psInst2->asArg[0]);
		if (iCmp != 0)
		{
			return iCmp;
		}
	}
	else
	{
		iCmp = CompareInstructionParameters(psState, psInst1, psInst2);
		if (iCmp != 0)
		{
			return iCmp;
		}

		iCmp = (IMG_INT32)(psInst1->uDestCount - psInst2->uDestCount);
		if (iCmp != 0)
		{
			return iCmp;
		}

		for (uDestIdx = 0; uDestIdx < psInst1->uDestCount; uDestIdx++)
		{
			iCmp = (IMG_INT32)(psInst1->auDestMask[uDestIdx] - psInst2->auDestMask[uDestIdx]);
			if (iCmp != 0)
			{
				return iCmp;
			}

			iCmp = (IMG_INT32)(psInst1->asDest[uDestIdx].uType - psInst2->asDest[uDestIdx].uType);
			if (iCmp != 0)
			{
				return iCmp;
			}

			if (psInst1->asDest[uDestIdx].uType != USC_REGTYPE_UNUSEDDEST)
			{
				ASSERT(psInst2->asDest[uDestIdx].uType != USC_REGTYPE_UNUSEDDEST);

				iCmp = psInst1->asDest[uDestIdx].eFmt - psInst2->asDest[uDestIdx].eFmt;
				if (iCmp != 0)
				{
					return iCmp;
				}
			}
		}

		iCmp = (IMG_INT32)(psInst1->uArgumentCount - psInst2->uArgumentCount);
		if (iCmp != 0)
		{
			return iCmp;
		}

		for (uSrcIdx = 0; uSrcIdx < psInst1->uArgumentCount; uSrcIdx++)
		{
			iCmp = CompareArgs(&psInst1->asArg[uSrcIdx], &psInst2->asArg[uSrcIdx]);
			if (iCmp != 0)
			{
				return iCmp;
			}
		}
	}

	/*
		Check for the case where we have
			
			P = test(A)
			Q = if P then test(A) else P / if !P then test(A) else P
			  = P && test(A) / P || test(A) 
			  = test(A) && test(A) / test(A) || test(A)
			  = test(A)
	*/
	if (IsTestIdempotent(psState, psInst1, psInst2))
	{
		return 0;
	}
	else
	{
		/*
			Check the sources for unwritten channels in each instruction
			destination are the same.
		*/
		for (uDestIdx = 0; uDestIdx < psInst1->uDestCount; uDestIdx++)
		{
			PARG		psOldDest1 = psInst1->apsOldDest[uDestIdx];
			PARG		psOldDest2 = psInst2->apsOldDest[uDestIdx];
			IMG_BOOL	bOldDestPresent1;
			IMG_BOOL	bOldDestPresent2;

			bOldDestPresent1 = (psOldDest1 != NULL) ? IMG_TRUE : IMG_FALSE;
			bOldDestPresent2 = (psOldDest2 != NULL) ? IMG_TRUE : IMG_FALSE;
			iCmp = bOldDestPresent1 - bOldDestPresent2;
			if (iCmp != 0)
			{
				return iCmp;
			}
	
			if (bOldDestPresent1)
			{
				iCmp = CompareArgs(psOldDest1, psOldDest2);
				if (iCmp != 0)
				{
					return iCmp;
				}
			}
		}

		/*
			Check both instructions are guarded by the same predicate.
		*/
		iCmp = ComparePredicates(psInst1, psInst2);
		if (iCmp != 0)
		{
			return iCmp;
		}
	}

	return 0;
}

static
PINST InsertInstInMergeList(PINTERMEDIATE_STATE psState, 
							PUSC_LIST psMergeList, 
							PINST psInstToInsert,
							IMG_BOOL bInsert)
/*****************************************************************************
 FUNCTION	: InsertInstInMergeList

 PURPOSE	: Checks a sorted list of instructions for an instruction identical
			  to the supplied instruction. If no identical instruction is found
			  then optionally inserts the supplied instruction in the list, 
			  keeping the list sorted.

 PARAMETERS	: psState			- Compiler state.
			  psMergeList		- Sorted list of instructions.
			  psInsertToInsert	- Instruction to insert.
			  bInsert			- Whether to insert the instruction into the list,
								  if an identical one is not found

 RETURNS	: A pointer to the identical instruction already in the list if one
			  was found or NULL if the instruction is unique.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psMergeList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINST		psInst;
		IMG_INT32	iCmp;

		psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
		iCmp = CompareInstructions(psState, psInst, psInstToInsert);
		if (iCmp == 0)
		{
			return psInst;
		}
		if (iCmp > 0)
		{
			break;
		}
	}
	if (bInsert)
	{
		InsertInList(psMergeList, &psInstToInsert->sAvailableListEntry, psListEntry);
	}

	return NULL;
}

static
IMG_VOID ReplaceIdenticalResult(PINTERMEDIATE_STATE psState, 
								PINST				psInstToRetain, 
								PINST				psInstToRemove)
/*****************************************************************************
 FUNCTION	: ReplaceIdenticalResult

 PURPOSE	: Where two instruction compute the same result replace all the
			  uses of the destination of one instruction by the destination
			  of the other.

 PARAMETERS	: psState			- Compiler state.
			  psInstToRetain	- The instruction to leave in place.
			  psInstToRemove	- The instruction whose result is to be
								replaced everywhere by the result of the
								other instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	/*
		Replace the later instruction in the block by the earlier instruction.
	*/
	ASSERT(psInstToRetain->uId < psInstToRemove->uId);

	/*
		Replace all the uses of the later instructions by the result of the earlier instruction.
	*/
	for (uDestIdx = 0; uDestIdx < psInstToRemove->uDestCount; uDestIdx++)
	{
		PARG		psDestToRemove = &psInstToRemove->asDest[uDestIdx];
		PARG		psDestToRetain = &psInstToRetain->asDest[uDestIdx];
		IMG_BOOL	bRet;

		ASSERT(psDestToRemove->uType == psDestToRetain->uType);

		/*
			Skip destinations not written by the instruction.
		*/
		if (psDestToRemove->uType != USEASM_REGTYPE_TEMP && psDestToRemove->uType != USEASM_REGTYPE_PREDICATE)
		{
			ASSERT(psDestToRemove->uType == USC_REGTYPE_UNUSEDDEST);
			continue;
		}

		/*	
			Combine the masks of channels used from each instruction's destination.
		*/
		psInstToRetain->auLiveChansInDest[uDestIdx] |= psInstToRemove->auLiveChansInDest[uDestIdx];
		
		/*
			Replace the result of the later instruction by the result of the
			earlier instruction.
		*/
		bRet = 
			UseDefSubstituteIntermediateRegister(psState,
												 psDestToRemove,
												 psDestToRetain,
												 #if defined(SRC_DEBUG)
												 UNDEFINED_SOURCE_LINE,
												 #endif /* defined(SRC_DEBUG) */
												 IMG_FALSE /* bExcludePrimaryEpilog */,
												 IMG_FALSE /* bExcludeSecondaryEpilog */,
												 IMG_FALSE /* bCheckOnly */);
		ASSERT(bRet);
	}

	/*
		Remove the duplicate instruction from the block and free it.
	*/
	RemoveInst(psState, psInstToRemove->psBlock, psInstToRemove);
	FreeInst(psState, psInstToRemove);
}

IMG_INTERNAL 
IMG_VOID MergeIdenticalInstructionsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: MergeIdenticalInstructionsBP

 PURPOSE	: Merge identical instructions.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block in which to merge instructions
			  pvNull			- IMG_PVOID to fit callback signature; unused

 RETURNS	: None
*****************************************************************************/
{
	PUSC_LIST		asMergeList;
	IMG_UINT32		uId;
	IMG_UINT32		uOpType;
	PINST			psInst;
	PINST			psNextInst;

	PVR_UNREFERENCED_PARAMETER(pvNull);

	/*
		PCKF16F16		DEST.XYZW, A, B
			<=>
		PCKF16F16		DEST.XY, A, __
			<=>
		PCKF16F16		DEST.ZW, B, __
	*/

	asMergeList = UscAlloc(psState, sizeof(asMergeList[0]) * IOPCODE_MAX);
	for (uOpType = 0; uOpType < IOPCODE_MAX; uOpType++)
	{
		InitializeList(&asMergeList[uOpType]);
	}

	/*
		Make a list of the potential candidates for merging.
	*/
	for (psInst = psBlock->psBody, uId = 0; psInst; psInst = psNextInst, uId++)
	{
		psNextInst = psInst->psNext;

		/*
			If the first two source arguments to the instruction can go in either order then put them
			in a fixed (but arbitary) order.
		*/
		if (
				InstSource01Commute(psState, psInst) &&
				(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0
		   )
		{
			PARG		psSrc1 = &psInst->asArg[0];
			PARG		psSrc2 = &psInst->asArg[1];
			IMG_INT32	iCmp;

			iCmp = CompareArgs(psSrc1, psSrc2);
			if (iCmp == 0)
			{
				iCmp = CompareInstructionArgumentParameters(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);
			}
			if (iCmp < 0)
			{
				CommuteSrc01(psState, psInst);
			}
		}

		psInst->uId = uId;
		if (!IsUnmergeableInst(psState, psInst))
		{
			PUSC_LIST	psMergeList;
			PINST		psIdenticalInst;

			/*
				Look for an identical instruction in the merge list. If one isn't found then
				insert in the merge list, keeping it sorted.
			*/
			psMergeList = &asMergeList[psInst->eOpcode];
			psIdenticalInst = InsertInstInMergeList(psState, psMergeList, psInst, IMG_TRUE);
			if (psIdenticalInst != NULL)
			{
				ReplaceIdenticalResult(psState, psIdenticalInst, psInst);
			}
		}
	}

	UscFree(psState, asMergeList);
}

static IMG_BOOL ReplaceConstantArg(PARG					psArg,
								   IMG_PUINT32			puNewType,
								   IMG_PUINT32			puNewNumber)
/*****************************************************************************
 FUNCTION	: ReplaceConstantArg

 PURPOSE	: Update an argument for an instruction which is being moved to the
			  secondary update program.

 PARAMETERS	: psArg				- Argument to update.
			  puNewType			- New register type,
			  puNewNumber		- New register number.

 RETURNS	: TRUE if argument needs updating.
*****************************************************************************/
{
	*puNewType = psArg->uType;
	*puNewNumber = psArg->uNumber;

	if (psArg->uType == USEASM_REGTYPE_SECATTR)
	{
		/*
			Secondary attributes are aliased to primary attributes in the secondary
			update program.
		*/
		*puNewType = USEASM_REGTYPE_PRIMATTR;
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID ReplaceConstantSource(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: ReplaceConstantSource

 PURPOSE	: Update a source argument for an instruction which is being moved to the
			  secondary update program.

 PARAMETERS	: psState			- Compiler state.
			  psCState			- Module state.
			  psInst			- Instruction to update.
			  uSrcIdx			- Source to update.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uNewType, uNewNumber;
	PARG		psSrc = &psInst->asArg[uSrcIdx];

	if (ReplaceConstantArg(psSrc, &uNewType, &uNewNumber))
	{
		SetSrc(psState, psInst, uSrcIdx, uNewType, uNewNumber, psSrc->eFmt);
	}
}

static IMG_BOOL IsConstantRegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber, PVREGISTER psRegister)
/******************************************************************************
 FUNCTION	: IsConstantRegister

 PURPOSE	: Check if an intermediate register has the same value
			  regardless of the current thread.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Intermediate register to check.
			  uNumber
			  psRegister

 RETURNS	: TRUE or FALSE.
******************************************************************************/
{
	/*
		Directly constant register types.
	*/
	if (uType == USEASM_REGTYPE_IMMEDIATE ||
		uType == USEASM_REGTYPE_FPCONSTANT ||
		uType == USEASM_REGTYPE_SECATTR)
	{
		return IMG_TRUE;
	}

	/*
		The dependent read counter parameter isn't set up until the InsertWaits phase
		at the very end of compilation.
	*/
	if (uType == USEASM_REGTYPE_DRC)
	{
		return IMG_TRUE;
	}

	/*
		Ignore sources not used by the instruction.
	*/
	if (uType == USC_REGTYPE_UNUSEDSOURCE || uType == USC_REGTYPE_NOINDEX)
	{
		return IMG_TRUE;
	}

	/*
		Check for a temporary register containing constant data in the channels
		read by the instruction.
	*/
	if (uType == USEASM_REGTYPE_TEMP)
	{
		PUSEDEF_CHAIN	psArgUseDef;
		PUSEDEF			psArgDef;

		psArgUseDef = psRegister->psUseDefChain;
		psArgDef = psArgUseDef->psDef;

		if (psArgDef != NULL)
		{
			if ((psArgDef->eType == DEF_TYPE_INST) && ((psArgDef->u.psInst->psBlock->psOwner->psFunc) == psState->psSecAttrProg))
			{
				return IMG_TRUE;
			}
			if (psArgDef->eType == DEF_TYPE_FIXEDREG && !psArgDef->u.psFixedReg->bPrimary)
			{
				return IMG_TRUE;
			}
		}
	}
	/*
		Check for an array containing constant data.
	*/
	if (uType == USC_REGTYPE_REGARRAY)
	{
		PUSC_VEC_ARRAY_REG	psArray;

		ASSERT(uNumber < psState->uNumVecArrayRegs);
		psArray = psState->apsVecArrayReg[uNumber];
		if (psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR || 
			psArray->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR)
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL IMG_BOOL IsConstantArgument(PINTERMEDIATE_STATE	psState,
										 PARG					psArg)
/******************************************************************************
 FUNCTION	: IsConstantArgument

 PURPOSE	: Check if an instruction's source argument has the same value
			  regardless of the current thread.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Index of the argument to be checked.

 RETURNS	: TRUE or FALSE.
******************************************************************************/
{
	/*
		Check the source register is uniform.
	*/
	if (!IsConstantRegister(psState, psArg->uType, psArg->uNumber, psArg->psRegister))
	{
		return IMG_FALSE;
	}
	/*
		Check any source register used to index into an array is uniform.
	*/
	if (!IsConstantRegister(psState, psArg->uIndexType, psArg->uIndexNumber, psArg->psIndexRegister))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: GlobalSimpleReplaceArguments
    
 PURPOSE	: Replace occurences of a temporary in an instruction by
			  another register.

 PARAMETERS	: psState			- Internal compiler state
			  psNextInst		- Instruction to change.
			  psSourceVector	- Mask of source arguments to change.
			  pvContext			- Points to the replacement argument.
			  bCheckOnly		- If TRUE only check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static
IMG_BOOL GlobalSimpleReplaceArguments(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  PSOURCE_VECTOR		psSourceVector,
									  IMG_PVOID				pvContext,
									  IMG_BOOL				bCheckOnly)
{
	PARG psArg = (PARG)pvContext;	
	IMG_UINT32 uSrcIdx;

	/*
		Replace the source arguments in the mask.
	*/
	if (!bCheckOnly)
	{
		for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
		{
			if (CheckSourceVector(psSourceVector, uSrcIdx))
			{
				SetSrcFromArg(psState, psInst, uSrcIdx, psArg);
			}
		}
	}

	return IMG_TRUE;
}

static IMG_VOID CopySecAttrToTemp(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psBlock,
								  PINST					psInst,
								  IMG_UINT32			uDestIdx,
								  IMG_BOOL				bOldDestConstant,
								  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								  IMG_BOOL				bVectorDest,
								  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								  PARG					psSecAttr,
								  PINST					psInsertBeforeInst,
								  IMG_PBOOL				pbInsertedPacks)
/******************************************************************************
 FUNCTION	: CopySecAttrToTemp

 PURPOSE	: Generate instructions to copy from a secondary attribute to the
			  original destination of an instruction.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Code block to add the instructions to.
			  psInst				- Instruction containing the original destination.
			  uDestIdx				- Original destination.
			  bOldDestConstant		- TRUE if the old value of a partially written
									destination was also constant.
			  psSecAttr				- Temporary register to copy from.
			  psInsertBeforeInst	- Point to insert the move instructions.
			  pbInsertedPacks		- Returns TRUE if pack instructions where
									inserted.

 RETURNS	: Nothing.
******************************************************************************/
{
	IMG_UINT32		uDestMask;
	IMG_UINT32		uLiveChansInDest;
	IMG_BOOL		bReplaced;

	ASSERT(uDestIdx < psInst->uDestCount);
	uDestMask = psInst->auDestMask[uDestIdx];
	uLiveChansInDest = psInst->auLiveChansInDest[uDestIdx];

	if (bOldDestConstant)
	{
		bReplaced = 
			UseDefSubstituteIntermediateRegister(psState,
												 &psInst->asDest[uDestIdx],
												 psSecAttr,
												 #if defined(SRC_DEBUG)
												 psInst->uAssociatedSrcLine,
												 #endif /* defined(SRC_DEBUG) */
												 IMG_FALSE /* bExcludePrimaryEpilog */,
												 IMG_FALSE /* bExcludeSecondaryEpilog */,
												 IMG_FALSE /* bCheckOnly */);
	}
	else
	{
		bReplaced = ReplaceAllUsesMasked(psState, 
										 &psInst->asDest[uDestIdx], 
										 uDestMask, 
										 psInst->apsPredSrc,
										 psInst->uPredCount,
										 GetPredicateNegate(psState, psInst),
										 GetPredicatePerChan(psState, psInst),
										 GlobalSimpleReplaceArguments, 
										 psSecAttr, 
										 IMG_FALSE /* bCheckOnly */);
		if (bReplaced)
		{
			psInst->auLiveChansInDest[uDestIdx] &= ~uDestMask;
			CopyPartiallyOverwrittenDataInBlock(psState, psBlock, psInst, psInsertBeforeInst);
		}
	}

	if (bReplaced)
	{
		return;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (bVectorDest)
	{
		PINST		psVMOVInst;

		psVMOVInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psVMOVInst, IVMOV);
		psVMOVInst->auLiveChansInDest[0] = uLiveChansInDest;
		MoveDest(psState, psVMOVInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
		if (!bOldDestConstant)
		{
			psVMOVInst->auDestMask[0] = uDestMask;
			CopyPartiallyWrittenDest(psState, psVMOVInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
		}
		else
		{
			psVMOVInst->auDestMask[0] = uLiveChansInDest;
		}
		SetupVectorSource(psState, psVMOVInst, 0 /* uSrcIdx */, psSecAttr);
		psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		InsertInstBefore(psState, psBlock, psVMOVInst, psInsertBeforeInst);	
	}
	else 
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (bOldDestConstant)
	{
		PINST	psMOVInst;

		psMOVInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psMOVInst, IMOV);
		psMOVInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDestIdx];
		MoveDest(psState, psMOVInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
		CopyPartiallyWrittenDest(psState, psMOVInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
		SetSrcFromArg(psState, psMOVInst, 0 /* uSrcIdx */, psSecAttr);
		InsertInstBefore(psState, psBlock, psMOVInst, psInsertBeforeInst);			
	}
	else
	{
		PINST	psSOPWMInst;

		psSOPWMInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psSOPWMInst, ISOPWM);

		psSOPWMInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psSOPWMInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
		psSOPWMInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		psSOPWMInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
		psSOPWMInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psSOPWMInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

		psSOPWMInst->auDestMask[0] = uDestMask;
		psSOPWMInst->auLiveChansInDest[0] = uLiveChansInDest;

		CopyPartiallyWrittenDest(psState, psSOPWMInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);

		MoveDest(psState, psSOPWMInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);

		SetSrcFromArg(psState, psSOPWMInst, 0 /* uSrcIdx */, psSecAttr);

		psSOPWMInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psSOPWMInst->asArg[1].uNumber = 0;

		InsertInstBefore(psState, psBlock, psSOPWMInst, psInsertBeforeInst);

		*pbInsertedPacks = IMG_TRUE;
	}
}

IMG_INTERNAL
IMG_BOOL	CheckAndUpdateInstSARegisterCount(PINTERMEDIATE_STATE	psState, 
											  IMG_UINT32			uSrcRegCount, 
											  IMG_UINT32			uResultRegCount,
											  IMG_BOOL				bCheckOnly)
/******************************************************************************
 FUNCTION	: CheckAndUpdateInstSARegisterCount

 PURPOSE	: Checks and updates the count of maxmium number of SA regsiters 
			  used by any instruction in sceondary update program

 PARAMETERS	: psState		  - Compiler state.
			  uSrcRegCount    - Number of instruction sources that requires 
							    sa registers
			  uResultRegCount - Number of result registers required by an 
							    instruction out of sa update program
			  bCheckOnly	  - If TRUE, only checks that enough sa registers 
								are available

 RETURNS	: IMG_TRUE if enough sa registers are available
******************************************************************************/
{
	IMG_UINT32	uMinRegsRequiredByInst = 0;
	IMG_UINT32	uTempsReservedAlready = 0;
	IMG_UINT32	uInstTempSrcCount = 0;

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
	{
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			uMinRegsRequiredByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_USPBIN - UF_MINIMUM_HARDWARE_TEMPORARIES_USP_SMP;
		}
		#if defined(OUTPUT_USCHW)
		else
		{
			uMinRegsRequiredByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_USCHW - UF_MINIMUM_HARDWARE_TEMPORARIES_SMP;
		}
		#else
		else
		{
			imgabort();
		}
		#endif /* defined(OUTPUT_USCHW) */
		#else
		uMinRegsRequiredByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_USCHW - UF_MINIMUM_HARDWARE_TEMPORARIES_SMP;
		#endif /* defined(OUTPUT_USPBIN) */

		uTempsReservedAlready = psState->sSAProg.uNumSecAttrsReservedForTemps;
		uInstTempSrcCount = uSrcRegCount;
	}

	/*
		Registers required for instruction results + minimum required by 
		instruction + instructions sources + registers hold for constants - 
		registers already reserved for temporaries
	*/
	if ((uResultRegCount + uMinRegsRequiredByInst + uInstTempSrcCount + psState->sSAProg.uConstSecAttrCount - uTempsReservedAlready) > psState->sSAProg.uInRegisterConstantLimit)
	{
		return IMG_FALSE;
	}

	if(!bCheckOnly)
	{
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
		{
			if (psState->sSAProg.uMaxTempRegsUsedByAnyInst < (uMinRegsRequiredByInst + uSrcRegCount))
			{
				psState->sSAProg.uMaxTempRegsUsedByAnyInst = uMinRegsRequiredByInst + uSrcRegCount;
			}
		}
	}
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

#define FIRST_TWO_SLOTS_MASK			((1U << (SOURCE_ARGUMENTS_PER_VECTOR * 2)) - 1)

static IMG_BOOL LiftMultiplyFromVMAD(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psSABlock,
									 PINST					psInst,
									 IMG_UINT32				uConstArgMask,
									 IMG_BOOL				bAllConst,
									 IMG_UINT32				uTempUsedByInst,
									 IMG_UINT32				uNumResultRegistersRequired)
/******************************************************************************
 FUNCTION	: LiftMultiplyFromVMAD

 PURPOSE	: Checks for a vector MAD instruction where the multiply part of the
			  instruction can be lifted into the secondary update program.

 PARAMETERS	: psState		- Compiler state.
			  psSABlock		-
			  psInst		- Instruction to try and modify.
			  uConstArgMask	- Mask of constant sources to the instruction.
			  bAllConst		- TRUE if all the sources to the instruction are
							constant.
			  uTempUsedByInst
			  uNumResultRegistersRequired	
							- Number of scalar registers required to hold the
							result of the instruction.

 RETURNS	: IMG_TRUE if the multiply was lifted.
******************************************************************************/
{
	PINST			psMulInst;
	ARG				sNewSecAttr;
	UF_REGFORMAT	eResultFmt;
	IMG_UINT32		uSlot;
	IMG_UINT32		uArg;

	if (psInst->eOpcode != IVMAD)
	{
		return IMG_FALSE;
	}

	/*
		Check only the first two sources to the instruction are constant.
	*/
	if (bAllConst)
	{
		return IMG_FALSE;
	}
	if ((uConstArgMask & FIRST_TWO_SLOTS_MASK) != FIRST_TWO_SLOTS_MASK)
	{
		return IMG_FALSE;
	}

	/*
		Check the destination and all the sources to the instruction are F32. This avoids problems
		where we don't know what the precision of the intermediate result should be but making it
		F32 where the other sources are F16 would reduce performance.
	*/
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}
	for (uSlot = 0; uSlot < 3; uSlot++)
	{
		if (psInst->u.psVec->aeSrcFmt[uSlot] != UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check there are sufficent secondary attributes to move the instruction.
	*/
	if (!CheckAndUpdateInstSARegisterCount(psState, uTempUsedByInst, uNumResultRegistersRequired, IMG_TRUE))
	{
		return IMG_FALSE;
	}
	
	/*
		Get a new variable to hold the result of the multiply.
	*/
	eResultFmt = psInst->asDest[0].eFmt;
	AddNewSAProgResult(psState, 
					   eResultFmt, 
					   IMG_TRUE /* bVector */, 
					   uNumResultRegistersRequired,
					   &sNewSecAttr);

	/*
		Generate the MUL instruction.
	*/
	psMulInst = AllocateInst(psState, psInst);

	SetOpcode(psState, psMulInst, IVMUL);
	SetDestFromArg(psState, psMulInst, 0 /* uDestIdx */, &sNewSecAttr);
	psMulInst->auLiveChansInDest[0] = psInst->auDestMask[0] & psInst->auLiveChansInDest[0];
	psMulInst->auDestMask[0] = psMulInst->auLiveChansInDest[0];
	MoveVectorSource(psState, 
					 psMulInst, 
					 0 /* uMoveToSrcIdx */, 
					 psInst, 
					 0 /* uMoveFromSrcIdx */, 
					 IMG_TRUE /* bMoveSourceModifier */);
	MoveVectorSource(psState, 
					 psMulInst, 
					 1 /* uMoveToSrcIdx */, 
					 psInst, 
					 1 /* uMoveFromSrcIdx */, 
					 IMG_TRUE /* bMoveSourceModifier */);
	for (uArg = 0; uArg < psMulInst->uArgumentCount; uArg++)
	{
		ReplaceConstantSource(psState, psMulInst, uArg);
	}
	InsertInstBefore(psState, psSABlock, psMulInst, NULL);

	/*
		Replace the MAD by an ADD using the result of the MUL.
	*/
	MoveVectorSource(psState, 
					 psInst, 
					 0 /* uMoveToArgIdx */, 
					 psInst, 
					 2 /* uMoveFromArgIdx */,
					 IMG_TRUE /* bMoveSourceModifier */);
	SetupVectorSource(psState, psInst, 1 /* uArgIdx */, &sNewSecAttr);
	psInst->u.psVec->asSrcMod[1].bNegate = IMG_FALSE;
	psInst->u.psVec->asSrcMod[1].bAbs = IMG_FALSE;
	psInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	ModifyOpcode(psState, psInst, IVADD);

	/*
		Update information about the number of secondary attribute registers used.
	*/
	CheckAndUpdateInstSARegisterCount(psState, uTempUsedByInst, uNumResultRegistersRequired, IMG_FALSE);

	return IMG_TRUE;
}

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL AlreadyExtracted(PINTERMEDIATE_STATE	psState,
								 PINST					psInst,
								 PUSC_LIST				asSecUpdateList,
								 PINST*					psExtractedInst)
								 
/******************************************************************************
 FUNCTION	: AlreadyExtracted

 PURPOSE	: Check whether a given instruction (constant calculation) has already been extracted.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to extract.
			  asSecUpdateList	- List of all instructions already extracted.
			  psExtractedInst	- Will be used to return the instruction already extracted
								  that is identical to psInst.

 RETURNS	: TRUE if the instruction was already extracted.
******************************************************************************/
{
	/*
		If the first two source arguments to the instruction can go in either order then put them
		in a fixed (but arbitary) order.
	*/
	if (
		InstSource01Commute(psState, psInst) &&
		(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0
		)
	{
		PARG		psSrc1 = &psInst->asArg[0];
		PARG		psSrc2 = &psInst->asArg[1];
		IMG_INT32	iCmp;

		iCmp = CompareArgs(psSrc1, psSrc2);
		if (iCmp == 0)
		{
			iCmp = CompareInstructionArgumentParameters(psState, psInst, 0 /* uArg1Idx */, psInst, 1 /* uArg2Idx */);
		}
		if (iCmp < 0)
		{
			CommuteSrc01(psState, psInst);
		}
	}
	
	if (!IsUnmergeableInst(psState, psInst))
	{
		PUSC_LIST	psSecUpdateList;
		PINST		psIdenticalInst;

		/*
			Look for an identical instruction in the secondary update list. If one isn't found then
			insert in the list, keeping it sorted.
		*/
		psSecUpdateList = &asSecUpdateList[psInst->eOpcode];
		psIdenticalInst = InsertInstInMergeList(psState, psSecUpdateList, psInst, IMG_FALSE);
		if (psIdenticalInst != NULL)
		{
			*psExtractedInst = psIdenticalInst;
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL ExtractConstantCalculationsInst(PINTERMEDIATE_STATE		psState,
											    PCODEBLOCK				psBlock,
												PCODEBLOCK				psSABlock,
												PINST					psInst,
												IMG_PBOOL				pbInsertPacks,
												PUSC_LIST				asSecUpdateList)
/******************************************************************************
 FUNCTION	: ExtractConstantCalculationsInst

 PURPOSE	: Try to move an instruction to the secondary update program.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Code block containing the instruction.
			  psSABlock			- Code block in SA program to which instruction
								  should be moved
			  psCState			- Constant extraction state.
			  psInst			- Instruction to check.
			  pbInsertPacks		- Set to TRUE on return if new PACK instructions
								  were added.
			  asSecUpdateList	- Ordered list of all instructions already extracted.

 RETURNS	: TRUE if the instruction was moved to the secondary update program
******************************************************************************/
{
	IMG_BOOL	bAllConst;
	IMG_UINT32	uArg;
	ARG			asNewSecAttr[USC_MAX_NONCALL_DEST_COUNT];
	IMG_UINT32	auDestNumResultRegistersRequired[USC_MAX_NONCALL_DEST_COUNT];
	IMG_BOOL	abOldDestConstant[USC_MAX_NONCALL_DEST_COUNT];
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uConstArgMask;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL	abVectorDest[USC_MAX_NONCALL_DEST_COUNT];
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	IMG_UINT32	uNumResultRegistersRequired;
	IMG_UINT32	uTempUsedByInst = 0;
	IMG_BOOL	bSampleInst = IMG_FALSE;
	PINST		psInsertBeforeInst, psExtractedInst;

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
	{
		#if defined(OUTPUT_USPBIN)
		if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			if (psInst->eOpcode >= ISMP_USP && psInst->eOpcode <= ISMP_USP_NDR)
			{
				uTempUsedByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_USP_SMP;
				bSampleInst = IMG_TRUE;
			}
		}
		#if defined(OUTPUT_USCHW)
		else
		{
			if (psInst->eOpcode >= ISMP && psInst->eOpcode <= ISMPGRAD)
			{
				uTempUsedByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_SMP;
				bSampleInst = IMG_TRUE;
			}
		}
		#else
		else
		{
			imgabort();
		}
		#endif /* defined(OUTPUT_USCHW) */
		#else
		if (psInst->eOpcode >= ISMP && psInst->eOpcode <= ISMPGRAD)
		{
			uTempUsedByInst = UF_MINIMUM_HARDWARE_TEMPORARIES_SMP;
			bSampleInst = IMG_TRUE;
		}
		#endif /* defined(OUTPUT_USPBIN) */
	}

	/*
		Skip instructions which implicitly use some data which varies between different instances
		in their calculation.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_IMPLICITLYNONUNIFORM) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Don't move instructions writing predicates.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Don't move predicated instructions.
	*/
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}

	/*
		Don't move instructions which require cross-instance calculations.
	*/
	if (RequiresGradients(psInst))
	{
		return IMG_FALSE;
	}

	/*
		Unless the conditionals for the predecessor of this block are constant (which we don't check) then
		a delta instruction must be considered nonconstant.
	*/
	if (psInst->eOpcode == IDELTA)
	{
		return IMG_FALSE;
	}

	/*
		Don't move instructions writing a clipplane P-flag.
	*/
	if (
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psInst->eOpcode == IVDP_CP ||
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			psInst->eOpcode == IFDPC
	   )
	{
		return IMG_FALSE;
	}

	#if defined(OUTPUT_USPBIN)
	/*
		Don't move USP texture samples as the USP can't handle expansion of texture samples
		in the secondary update program.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
	{
		return IMG_FALSE;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	uNumResultRegistersRequired = 0;
	ASSERT(psInst->uDestCount <= USC_MAX_NONCALL_DEST_COUNT);
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		/*
			Ignore unwritten destinations.
		*/
		if (psDest->uType == USC_REGTYPE_UNUSEDDEST)
		{
			continue;
		}

		/*
			Don't move instructions that perform dynamically indexed writes.
		*/
		if	(psDest->uIndexType != USC_REGTYPE_NOINDEX)
		{
			return IMG_FALSE;
		}

		/*
			Skip instructions not writing temporary registers.
		*/
		if (psDest->uType != USEASM_REGTYPE_TEMP)
		{
			return IMG_FALSE;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Is this an instruction writing a 128-bit or 64-bit sized register?
		*/
		abVectorDest[uDestIdx] = IMG_FALSE;
		if (InstHasVectorDest(psState, psInst, uDestIdx))
		{
			abVectorDest[uDestIdx] = IMG_TRUE;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Keep track of how many registers will be needed to hold the results of this
			instruction as an output from the secondary update program.
		*/
		auDestNumResultRegistersRequired[uDestIdx] = 
			GetSAProgNumResultRegisters(psState, 
										#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									    abVectorDest[uDestIdx],
										#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
										psDest->eFmt);
		uNumResultRegistersRequired += auDestNumResultRegistersRequired[uDestIdx];
	}

	/*
		Check if all arguments are constants.
	*/
	bAllConst = IMG_TRUE;
	uConstArgMask = 0;
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (GetLiveChansInArg(psState, psInst, uArg) == 0)
		{
			uConstArgMask |= (1U << uArg);
			continue;
		}
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
		{
			if(!bSampleInst)
			{
				uTempUsedByInst++;
			}
		}
		if (IsConstantArgument(psState, &psInst->asArg[uArg]))
		{
			uConstArgMask |= (1U << uArg);
		}
		else
		{
			bAllConst = IMG_FALSE;
		}
	}
	
	/*
		Check for a MAD instruction with the first two arguments constants. We can extract the multiply part
		of the MAD to the secondary load program which avoids a move since constant arguments can be accessed
		in both sources in the secondary program.
	*/
	if (
			psInst->eOpcode == IFMAD &&
			uConstArgMask == 3 &&
			CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[2]) &&
			CheckAndUpdateInstSARegisterCount(psState, 2, 1, IMG_TRUE)
	   )
	{
		PINST		psMulInst;
		ARG			sNewSecAttr;

		/*
			Get a new variable to hold the result of the multiply.
		*/
		AddNewSAProgResult(psState, 
						   UF_REGFORMAT_F32, 
						   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						   IMG_FALSE /* bVector */, 
						   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						   1 /* uNumResultRegisters */,
						   &sNewSecAttr);

		/*
			Generate the MUL instruction.
		*/
		psMulInst = AllocateInst(psState, psInst);

		SetOpcode(psState, psMulInst, IFMUL);
		SetDestFromArg(psState, psMulInst, 0 /* uDestIdx */, &sNewSecAttr);
		MoveFloatSrc(psState, psMulInst, 0 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
		MoveFloatSrc(psState, psMulInst, 1 /* uDestArgIdx */, psInst, 1 /* uSrcArgIdx */);
		ReplaceConstantSource(psState, psMulInst, 0 /* uSrcIdx */);
		ReplaceConstantSource(psState, psMulInst, 1 /* uSrcIdx */);
		InsertInstBefore(psState, psSABlock, psMulInst, NULL);

		/*
			Replace the MAD by an ADD using the result of the MUL.
		*/
		MoveFloatSrc(psState, psInst, 0 /* uDestArgIdx */, psInst, 2 /* uSrcArgIdx */);
		SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, &sNewSecAttr);
		InitPerArgumentParameters(psState, psInst, 1 /* uArgIdx */);
		ModifyOpcode(psState, psInst, IFADD);

		/*
			Update maximum number of regs used by any instruction for cores 
			where temporary registers cannot be allocated for the secondary 
			update program.
		*/
		CheckAndUpdateInstSARegisterCount(psState, 2, 1, IMG_FALSE);

		/*
			Treat the residual ADD instruction as a normal, non-constant
			instruction.
		*/
		return IMG_FALSE;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Similarly for the case of a vector MAD.
	*/
	if (LiftMultiplyFromVMAD(psState,
							 psSABlock,
							 psInst,
							 uConstArgMask,
							 bAllConst,
							 uTempUsedByInst,
							 uNumResultRegistersRequired))
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Not a constant instruction.
	*/
	if (!bAllConst)
	{
		return IMG_FALSE;
	}
	
	/*
		Don't extract 'move-type' instructions since we aren't saving anything in that case.
	*/
	if (psInst->eOpcode == IPCKF16F16 &&
		psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
		psInst->asArg[1].uNumber == 0 &&
		psInst->uDestCount == 1 &&
		psInst->auDestMask[0] == 12 &&
		GetComponentSelect(psState, psInst, 0 /* uArgIdx */) == 0)
	{
		// moving low bits to high bits so ok to extract
		if (psInst->apsOldDest[0] != NULL &&
			!IsConstantArgument(psState, psInst->apsOldDest[0]))
		{
			return IMG_FALSE;
		}
	}
	else if (psInst->eOpcode == IPCKF16F16 &&
		psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
		psInst->asArg[1].uNumber == 0 &&
		psInst->uDestCount == 1 &&
		psInst->auDestMask[0] == 3 &&
		GetComponentSelect(psState, psInst, 0 /* uArgIdx */) == 2)
	{
		// moving high bits to low bits so ok to extract
		if (psInst->apsOldDest[0] != NULL &&
			!IsConstantArgument(psState, psInst->apsOldDest[0]))
		{
			return IMG_FALSE;
		}
	}
	else if (psInst->eOpcode == IPCKF16F16 &&
		psInst->uDestCount == 1 &&
		psInst->auDestMask[0] == 15)
	{
		// broadcasting a result so extract out
		if (psInst->apsOldDest[0] != NULL &&
			!IsConstantArgument(psState, psInst->apsOldDest[0]))
		{
			return IMG_FALSE;
		}
	}
	else if (psInst->eOpcode == IPCKF16F16 &&
		psInst->apsOldDest[0] != NULL &&
		IsConstantArgument(psState, psInst->apsOldDest[0]))
	{
		/* old dest has already been hoisted out so hoist this too */
	}
	else if (psInst->eOpcode == IMOV || psInst->eOpcode == IPCKF16F16)
	{
		return IMG_FALSE;
	}
	
	/* Try to find an identical instruction already extracted. */
	if (AlreadyExtracted(psState, psInst, asSecUpdateList, &psExtractedInst))
	{
		/*
			Remove the instruction from it's current block.
		*/
		psInsertBeforeInst = psInst->psNext;
		RemoveInst(psState, psBlock, psInst);

		for (uDestIdx = 0; uDestIdx < psExtractedInst->uDestCount; uDestIdx++)
		{
			PARG psDest, psOldDest;

			psDest = &psInst->asDest[uDestIdx];

			/*
				Ignore unwritten destinations.
			*/
			if (psDest->uType == USC_REGTYPE_UNUSEDDEST)
			{
				continue;
			}

			ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);

			/*
				Update the preserved values of conditionally/partially written destinations.
			*/
			psOldDest = psInst->apsOldDest[uDestIdx];

			abOldDestConstant[uDestIdx] = IMG_TRUE;
			if (psOldDest != NULL)
			{
				if (!IsConstantArgument(psState, psOldDest))
				{
					abOldDestConstant[uDestIdx] = IMG_FALSE;
				}
			}

			asNewSecAttr[uDestIdx] = psExtractedInst->asDest[uDestIdx];

			/*
			Generate new moves in the primary program to replace the instruction.
			*/
			CopySecAttrToTemp(psState, 
				psBlock,
				psInst,
				uDestIdx,
				abOldDestConstant[uDestIdx],
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				abVectorDest[uDestIdx],
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				&asNewSecAttr[uDestIdx],
				psInsertBeforeInst,
				pbInsertPacks);
		}

		FreeInst(psState, psInst);
		
		return IMG_TRUE;
	}

	/*
		Check for enough secondary attributes to contain the extra results written in the secondary
		update program.
	*/
	if(!CheckAndUpdateInstSARegisterCount(psState, uTempUsedByInst, uNumResultRegistersRequired, IMG_TRUE))
	{
		return IMG_FALSE;
	}
	else
	{
		/*
			Update maximum number of regs used by any instruction for cores 
			where temporary registers cannot be allocated for the secondary 
			update program.
		*/
		CheckAndUpdateInstSARegisterCount(psState, uTempUsedByInst, uNumResultRegistersRequired, IMG_FALSE);
	}
	
	if (!IsUnmergeableInst(psState, psInst))
	{
		/*
		Insert in the list of instructions in the secondary update, keeping it sorted.
		*/
		PUSC_LIST psSecUpdateList = &asSecUpdateList[psInst->eOpcode];
		PINST psIdenticalInst = InsertInstInMergeList(psState, psSecUpdateList, psInst, IMG_TRUE /*insert*/);
		ASSERT(psIdenticalInst == NULL);
	}

	/*
		Move the instruction to the secondary update program.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG					psDest = &psInst->asDest[uDestIdx];

		/*
			Ignore unwritten destinations.
		*/
		if (psDest->uType == USC_REGTYPE_UNUSEDDEST)
		{
			continue;
		}

		ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);

		/*
			Grab a new variable to hold this destination as an output from the
			secondary update program.
		*/
		AddNewSAProgResult(psState, 
						   psDest->eFmt, 
						   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						   abVectorDest[uDestIdx], 
						   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						   auDestNumResultRegistersRequired[uDestIdx],
						   &asNewSecAttr[uDestIdx]);
	}

	/*
		Remove the instruction from it's current block and append it to the secondary
		update program block.
	*/
	psInsertBeforeInst = psInst->psNext;
	RemoveInst(psState, psBlock, psInst);
	AppendInst(psState, psSABlock, psInst);

	/*
		Update the instruction's source arguments for the move to the secondary
		update program.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (GetLiveChansInArg(psState, psInst, uArg) != 0)
		{
			ReplaceConstantSource(psState, psInst, uArg);
		}
	}

	/*
		Update the preserved values of conditionally/partially written destinations.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psOldDest = psInst->apsOldDest[uDestIdx];

		abOldDestConstant[uDestIdx] = IMG_TRUE;
		if (psOldDest != NULL)
		{
			if (IsConstantArgument(psState, psOldDest))
			{
				IMG_UINT32	uNewType, uNewNumber;

				if (ReplaceConstantArg(psOldDest, &uNewType, &uNewNumber))
				{
					SetPartialDest(psState, psInst, uDestIdx, uNewType, uNewNumber, psOldDest->eFmt);
				}
			}
			else
			{
				abOldDestConstant[uDestIdx] = IMG_FALSE;
			}
		}
	}

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG psDest;

		psDest = &psInst->asDest[uDestIdx];

		/*
			Ignore unwritten destinations.
		*/
		if (psDest->uType == USC_REGTYPE_UNUSEDDEST)
		{
			continue;
		}

		ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);

		/*
			Generate new moves in the primary program to replace the instruction.
		*/
		CopySecAttrToTemp(psState, 
						  psBlock,
						  psInst,
						  uDestIdx,
						  abOldDestConstant[uDestIdx],
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  abVectorDest[uDestIdx],
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  &asNewSecAttr[uDestIdx],
						  psInsertBeforeInst,
						  pbInsertPacks);


		/*
			Replace the instruction's destination.
		*/
		SetDestFromArg(psState, psInst, uDestIdx, &asNewSecAttr[uDestIdx]);

		/*
			If the partially overwritten destination isn't constant then clear it in the instruction
			copied to the secondary program. Masking will be handled by the generated move instructions
			in the primary program.
		*/
		if (!abOldDestConstant[uDestIdx])
		{
			SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psPartialDest */);
			psInst->auLiveChansInDest[uDestIdx] = psInst->auDestMask[uDestIdx];
		}
	}

	return IMG_TRUE;
}

static IMG_VOID ExtractConstantCalculationsBP(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psBlock,
											  IMG_PVOID				vSecUpdateData)
/*****************************************************************************
 FUNCTION	: ExtractConstantCalculationsBP

 PURPOSE	: Try to move instructions from a block to the secondary update
			  program. Includes appropriate calls to DCE, IntegerOptimize and
			  EliminateMoves if instructions were moved or packs added.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Code block to examine for suitable instructions.
			  vSecUpdateData	- Ordered list of all instructions already extracted.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;
	PINST		psNextInst;
	IMG_BOOL	bReplacedInsts = IMG_FALSE;
	IMG_BOOL	bInsertedPacks = IMG_FALSE;
	PUSC_LIST	asSecUpdateList = (PUSC_LIST)vSecUpdateData;

	/*
		Don't check instructions already in the secondary update program.
	*/
	if (psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		return;
	}

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		/*
			Check for reaching the limit of secondary attributes we are allowed to use for constants.
		*/
		if (psState->sSAProg.uConstSecAttrCount == psState->sSAProg.uInRegisterConstantLimit)
		{
			break;
		}

		psNextInst = psInst->psNext;

		/*
			Try and move this instruction to the secondary program.
			For now we move all instructions into the same block; the possibility of extracting
			calculations that use (i.e. constant) flow control is left for another day.
		*/
		ASSERT (psState->psSecAttrProg->sCfg.psEntry == psState->psSecAttrProg->sCfg.psExit);
		if (ExtractConstantCalculationsInst(psState, psBlock, psState->psSecAttrProg->sCfg.psExit, psInst, &bInsertedPacks, asSecUpdateList))
		{
			bReplacedInsts = IMG_TRUE;
		}
	}

	if (bReplacedInsts)
	{
		IMG_BOOL	bChanges = IMG_FALSE;

		do
		{
			if (bInsertedPacks)
			{
				IMG_BOOL bNewMoves;

				bChanges = IntegerOptimize(psState, psBlock, &bNewMoves);
			}
		
			/*
				Try and remove the new moves we just inserted.
			*/
			EliminateMovesBP(psState, psBlock);
		} while (bChanges);
	}
}

static
IMG_UINT32 GetSAOutputUsage(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psSAOutput)
/*****************************************************************************
 FUNCTION	: GetSAOutputUsage

 PURPOSE	: Get the mask of channels used in the primary program from an
			  intermediate register defined in the secondary program.

 PARAMETERS	: psState			- Compiler state.
			  psSAOutput		- Intermediate register.

 RETURNS	: The mask of channels.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uUsedChanMask;

	uUsedChanMask = 0;
	for (psListEntry = psSAOutput->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		IMG_BOOL	bPrimary = IMG_FALSE;

		switch (psUse->eType)
		{
			case USE_TYPE_SRC:
			case USE_TYPE_SRCIDX:
			case USE_TYPE_OLDDEST:
			case USE_TYPE_OLDDESTIDX:
			case USE_TYPE_DESTIDX:
			{
				bPrimary = IMG_FALSE;
				if ((psUse->u.psInst->psBlock->psOwner->psFunc) != psState->psSecAttrProg)
				{
					bPrimary = IMG_TRUE;
				}
				break;
			}
			case USE_TYPE_SWITCH:
			{
				bPrimary = IMG_FALSE;
				if (psUse->u.psBlock->psOwner->psFunc != psState->psSecAttrProg)
				{
					bPrimary = IMG_TRUE;
				}
				break;
			}
			case USE_TYPE_FIXEDREG:
			{
				bPrimary = psUse->u.psFixedReg->bPrimary;
				break;
			}
			case USE_TYPE_FUNCOUTPUT:
			{
				bPrimary = IMG_TRUE;
				break;
			}
			case DEF_TYPE_FIXEDREG:
			{
				ASSERT(!psUse->u.psFixedReg->bPrimary);
				bPrimary = IMG_FALSE;
				break;
			}
			case DEF_TYPE_INST:
			{
				ASSERT(psUse->u.psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg);
				bPrimary = IMG_FALSE;
				break;
			}
			default: imgabort();
		}

		if (bPrimary)
		{
			uUsedChanMask |= GetUseChanMask(psState, psUse);
			if (uUsedChanMask == USC_ALL_CHAN_MASK)
			{
				break;
			}
		}
	}
	return uUsedChanMask;
}

IMG_INTERNAL IMG_VOID ExtractConstantCalculations(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ExtractConstantCalculations

 PURPOSE	: Move parts of the program that do calculations on constant values
			  to the secondary update task.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_LIVESET		psSAOutputs;
	PUSC_LIST_ENTRY			psListEntry;
	PUSC_LIST_ENTRY			psNextListEntry;
	IMG_UINT32				uSAIdx;
	IMG_UINT32				uNewNumReservedSecAttrs;
	IMG_BOOL				bSARegCountSet = IMG_FALSE;
	IMG_UINT32				uArrayIdx;
	PUSC_LIST				asSecUpdateList;
	IMG_UINT32				uOpType;

	/*
		Cores with a unified temp/pa allocation don't support allocating temporaries for the
		secondary update program so we must reserve enough registers to be able to complete
		register allocation for any instructions which we might move to the secondary
		update program.
	*/
	uNewNumReservedSecAttrs = 0;
	
	/* Keep a list of all instructions that were extracted, so we can reuse them, if pos. */
	asSecUpdateList = UscAlloc(psState, sizeof(asSecUpdateList[0]) * IOPCODE_MAX);
	for (uOpType = 0; uOpType < IOPCODE_MAX; uOpType++)
	{
		InitializeList(&asSecUpdateList[uOpType]);
	}

	/*
		Try to move instructions from all basic blocks to the secondary update program.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, ExtractConstantCalculationsBP, IMG_FALSE, asSecUpdateList);

	UscFree(psState, asSecUpdateList);

	/*
		No instructions were moved to the secondary update program so nothing more to do.
	*/
	if (psState->sSAProg.uNumResults == 0)
	{
		goto exitfunc;
	}

	/*
		Clear the set of registers live at the end of the secondary update program.
	*/
	psSAOutputs = &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut;
	ClearRegLiveSet(psState, psSAOutputs);

	/*
		Set all registers not available for use by us as live out of the secondary update
		program. 
	*/
	for (uSAIdx = 0; uSAIdx < psState->psSAOffsets->uInRegisterConstantOffset; uSAIdx++)
	{
		SetRegisterLiveMask(psState,
							psSAOutputs,
							USEASM_REGTYPE_PRIMATTR,
							uSAIdx,
							0 /* uArrayOffset */,
							USC_ALL_CHAN_MASK);
	}
	/*
		Set all ranges of secondary attributes accessed with dynamic indices as live
		out of the secondary update program.
	*/
	for (uArrayIdx = 0; uArrayIdx < psState->uNumVecArrayRegs; uArrayIdx++)
	{	
		PUSC_VEC_ARRAY_REG psArray;	psArray = psState->apsVecArrayReg[uArrayIdx];	
		if (psArray->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR ||
			psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR)	
		{
			for (uSAIdx = 0; uSAIdx < psArray->uRegs; uSAIdx++)
			{
				SetRegisterLiveMask(psState,				
									psSAOutputs,				
									USEASM_REGTYPE_TEMP,				
									psArray->uBaseReg + uSAIdx,				
									0 /* uArrayOffset */,				
									USC_ALL_CHAN_MASK);
			}
		}
	}

	/*
		Where a secondary update program result is used in the main program add it to the set of
		registers live on exit from the secondary update program.
	*/
	for (psListEntry = psState->sSAProg.sResultsList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);
		if (!psResult->bPartOfRange)
		{
			IMG_UINT32		uTempRegNum = psResult->psFixedReg->auVRegNum[0];
			PUSEDEF_CHAIN	psTemp = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempRegNum);
			IMG_UINT32		uChansUsedInPrimary;
		
			uChansUsedInPrimary = GetSAOutputUsage(psState, psTemp);
			SetRegisterLiveMask(psState, 
								psSAOutputs,
								USEASM_REGTYPE_TEMP,
								uTempRegNum,
								0 /* uArrayOffset */,
								uChansUsedInPrimary);
		}
	}
	
	/*
		Recalculate kill flags in the secondary loading block.
	*/
	if (psState->psSecAttrProg->sCfg.psEntry != psState->psSecAttrProg->sCfg.psExit)
	{
		DoSALiveness(psState);
	}
		
	MergeBasicBlocks(psState, psState->psSecAttrProg); //might delete it!

	/*
		For any secondary update program results where all the instructions which use them
		have also been moved to the secondary update program remove them from the list of
		results.
	*/
	for (psListEntry = psState->sSAProg.sResultsList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psNextListEntry)
	{
		PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);

		psNextListEntry = psListEntry->psNext;

		if (psResult->eType == SAPROG_RESULT_TYPE_CALC)
		{
			SetChansUsedFromSAProgResult(psState, psResult);
		}
	}

	/*
		Update maximum number of regs used by any instruction for cores 
		where temporary registers cannot be allocated for the secondary 
		update program.
	*/
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
	{
		if (psState->sSAProg.uMaxTempRegsUsedByAnyInst > psState->sSAProg.uNumSecAttrsReservedForTemps)
		{
			uNewNumReservedSecAttrs = psState->sSAProg.uMaxTempRegsUsedByAnyInst - psState->sSAProg.uNumSecAttrsReservedForTemps;
		}
	}

	/*
		Now instructions have been moved to the secondary update program we must reserve
		enough secondary attributes to be able to do register allocation.
	*/
	if (uNewNumReservedSecAttrs > 0)
	{
		ASSERT(psState->sSAProg.uInRegisterConstantLimit >= uNewNumReservedSecAttrs);
		psState->sSAProg.uInRegisterConstantLimit -= uNewNumReservedSecAttrs;

		psState->sSAProg.uNumSecAttrsReservedForTemps += uNewNumReservedSecAttrs;
	}

	bSARegCountSet = IMG_TRUE;
	
	/* Free memory */
exitfunc:

	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS)
	{
		if (!bSARegCountSet)
		{
			/*
				Is there any instruction moved to secondary update program 
				before extracting constant calculations.
			*/
			if (psState->sSAProg.uMaxTempRegsUsedByAnyInst > psState->sSAProg.uNumSecAttrsReservedForTemps)
			{
				uNewNumReservedSecAttrs = psState->sSAProg.uMaxTempRegsUsedByAnyInst - psState->sSAProg.uNumSecAttrsReservedForTemps;
			}

			if (uNewNumReservedSecAttrs > 0)
			{
				ASSERT(psState->sSAProg.uInRegisterConstantLimit >= uNewNumReservedSecAttrs);
				psState->sSAProg.uInRegisterConstantLimit -= uNewNumReservedSecAttrs;

				psState->sSAProg.uNumSecAttrsReservedForTemps += uNewNumReservedSecAttrs;
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: ReplaceF32ByF16Source

 PURPOSE    : Replace uses of an F32 format source by another source in F16
			  format.

 INPUT		: psState			- Compiler state.
			  uInst				- Instruction which generates the source.
			  uSrcType			- Source to replace.
			  uSrcNum
			  bCheckOnly		- TRUE if the function should only check if
								the replacement is possible.
			  uReplaceType		- Replacement source.
			  uReplaceNum
			  uReplaceComponent

 RETURNS	: IMG_TRUE if the source can be replaced; IMG_FALSE otherwise.
*****************************************************************************/
static IMG_BOOL ReplaceF32ByF16Source(PINTERMEDIATE_STATE	psState, 
									  IMG_UINT32			uSrcType, 
									  IMG_UINT32			uSrcNum,
									  IMG_BOOL				bCheckOnly,
									  IMG_UINT32			uReplaceType,
									  IMG_UINT32			uReplaceNum,
									  IMG_UINT32			uReplaceComponent)
{
	PUSEDEF_CHAIN			psUseDef;
	PUSEDEF					psDef;
	PUSC_LIST_ENTRY			psListEntry;
	PUSC_LIST_ENTRY			psNextListEntry;

	psUseDef = UseDefGet(psState, uSrcType, uSrcNum);
	if (psUseDef == NULL)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	ASSERT(psUseDef->psDef != NULL);
	psDef = psUseDef->psDef;
	ASSERT(UseDefIsDef(psDef));

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF		psUse;
		PINST		psDepInst;
		IMG_UINT32	uArg;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		ASSERT(UseDefIsUse(psUse));

		/*
			Check for a use not in an instruction source.
		*/
		if (psUse->eType != USE_TYPE_SRC)
		{
			return IMG_FALSE;
		}
		psDepInst = psUse->u.psInst;
		uArg = psUse->uLocation;

		/*
			Check if the instruction can accept a register in multiple formats.
		*/
		if (!HasF16F32SelectInst(psDepInst))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		/*
			Check for the hardware bug whereby an f16 format register can't appear
			in source 0 of an EFO.
		*/
		if (uArg == 0 && 
			psDepInst->eOpcode == IEFO && 
			(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697) != 0)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		/*
			If not checking then replace the source.
		*/
		if (!bCheckOnly)
		{
			SetSrc(psState, psDepInst, uArg, uReplaceType, uReplaceNum, UF_REGFORMAT_F16);
			SetComponentSelect(psState, psDepInst, uArg, uReplaceComponent);
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CombineF32PacksBlock

 PURPOSE    : Combine two unpacks to f32 format where the unpack instructions
			  share the same source.

 INPUT		: psState			- Compiler state.
			  psCodeBlock		- Block to combine instructions within.

 RETURNS	: .
*****************************************************************************/
static
IMG_VOID CombineF32PacksBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PDGRAPH_STATE psDepState)
{
	IMG_UINT32		uInstA;
	
	if (psState->bInvariantShader == IMG_TRUE)
	{
		/* 
			This optimization is not invariance safe as it can modify F32 to F16 
			in one shader but not in another shader.
		*/
		return;
	}

	for (uInstA = 0; uInstA < psDepState->uBlockInstructionCount; uInstA++)
	{
		PINST		psInstA = ArrayGet(psState, psDepState->psInstructions, uInstA);
		IMG_UINT32	uInstB;

		/*
			Check for a non-predicated pack instruction.
		*/
		if (
				!(
					psInstA != NULL &&
					(
						psInstA->eOpcode == IUNPCKF32U8 || 
						psInstA->eOpcode == IUNPCKF32S8 || 
						psInstA->eOpcode == IUNPCKF32O8 || 
						psInstA->eOpcode == IUNPCKF32C10
					) &&
					NoPredicate(psState, psInstA) &&
					psInstA->asDest[0].uType == USEASM_REGTYPE_TEMP &&
					psInstA->asArg[0].uIndexType == USC_REGTYPE_NOINDEX
				  )
			)
		{
			continue;
		}
		/*
			Check all the receiving instructions can accept f16 format sources.
		*/
		if (!ReplaceF32ByF16Source(psState, psInstA->asDest[0].uType, psInstA->asDest[0].uNumber, IMG_TRUE, 0, 0, 0))
		{
			continue;
		}
		/*
			Find another instruction to combine with.
		*/
		for (uInstB = uInstA + 1; uInstB < psDepState->uBlockInstructionCount; uInstB++)
		{
			PINST		psInstB = ArrayGet(psState, psDepState->psInstructions, uInstB);
			IMG_UINT32	uNewRegister;

			/*
				Check for a pack of the same format, without a dependency on the first pack.
			*/
			if (
					!(
						psInstB != NULL &&
						!GraphGet(psState, psDepState->psClosedDepGraph, uInstB, uInstA) &&
						!GraphGet(psState, psDepState->psClosedDepGraph, uInstA, uInstB) &&
						psInstB->eOpcode == psInstA->eOpcode &&
						psInstB->u.psPck->bScale == psInstA->u.psPck->bScale &&
						NoPredicate(psState, psInstB)
					 )
			   )
			{
				continue;
			}
			/*
				Check all the receiving instructions can accept f16 sources.
			*/
			if (!ReplaceF32ByF16Source(psState, psInstB->asDest[0].uType, psInstB->asDest[0].uNumber, IMG_TRUE, 0, 0, 0))
			{
				continue;
			}

			/*
				Get a new temporary register that will hold the two unpacked results.
			*/
			uNewRegister = GetNextRegister(psState);

			/*
				Replace uses of the two old destination registers by the new register.
			*/
			ReplaceF32ByF16Source(psState, 
								  psInstA->asDest[0].uType, 
								  psInstA->asDest[0].uNumber, 
								  IMG_FALSE, 
								  USEASM_REGTYPE_TEMP, 
								  uNewRegister, 
								  0);
			ReplaceF32ByF16Source(psState, 
								  psInstB->asDest[0].uType, 
								  psInstB->asDest[0].uNumber, 
								  IMG_FALSE, 
								  USEASM_REGTYPE_TEMP, 
								  uNewRegister, 
								  2);

			/*
				Switch the opcode to unpack to f16 format.
			*/
			switch (psInstA->eOpcode)
			{
				case IUNPCKF32U8: ModifyOpcode(psState, psInstA, IUNPCKF16U8); break;
				case IUNPCKF32S8: ModifyOpcode(psState, psInstA, IUNPCKF16S8); break;
				case IUNPCKF32O8: ModifyOpcode(psState, psInstA, IUNPCKF16O8); break;
				case IUNPCKF32C10: ModifyOpcode(psState, psInstA, IUNPCKF16C10); break;
				default: imgabort();
			}
			SetDest(psState, psInstA, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewRegister, UF_REGFORMAT_F16);
			MoveSrc(psState, psInstA, 1 /* uCopyToSrcIdx */, psInstB, 0 /* uCopyFromSrcIdx */);
			SetPCKComponent(psState, psInstA, 1 /* uArgIdx */, GetPCKComponent(psState, psInstB, 0 /* uArgIdx */));

			/*
				Remove the combined instruction.
			*/
			MergeInstructions(psDepState, uInstA, uInstB, IMG_TRUE);
			RemoveInst(psState, psCodeBlock, psInstB);
			FreeInst(psState, psInstB);
			break;
		}
	}
}

static
IMG_BOOL IsMatchPred(PINST					psInst,
					 IMG_UINT32				uMatchPredSrc,
					 IMG_BOOL				bMatchPredNegate,
					 IMG_BOOL				bMatchPredPerChan)
/*****************************************************************************
 FUNCTION	: IsMatchPred

 PURPOSE    : Check for either an unconditional predicate or a matching predicate.

 INPUT		: psInst			- Compare the source predicate from this instruction.
			  uMatchPredSrc		- Second predicate to compare
			  bMatchPredNegate	- Predicate negate flag must also match this.
			  bMatchPredPerChan	- Predicate per channel flag must also match this.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uPredSrc;
	IMG_BOOL	bPredNegate, bPredPerChan;
	
	if (psInst->uPredCount > 1)
	{
		return IMG_FALSE;
	}

	GetPredicate(psInst, &uPredSrc, &bPredNegate, &bPredPerChan, 0);
	if (uPredSrc == USC_PREDREG_NONE)
	{
		return IMG_TRUE;
	}
	if (uPredSrc == uMatchPredSrc &&
		bPredNegate == bMatchPredNegate &&
		bPredPerChan == bMatchPredPerChan)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CanPredicate(PINTERMEDIATE_STATE psState,
							 PINST psInst)
/*****************************************************************************
 FUNCTION	: CanPredicate

 PURPOSE    : Test whether an instruction can be predicated

 INPUT		: psState	 - Internal compiler state
              psInst     - Instruction to test

 RETURNS	: IMG_TRUE iff psInst takes a predicate
*****************************************************************************/
{
	IMG_UINT32 eOpcode = psInst->eOpcode;


	if (psInst->eOpcode == ISMPGRAD
#if defined(OUTPUT_USPBIN)
		|| psInst->eOpcode == ISMP_USP_NDR
		|| psInst->eOpcode == ISMPBIAS_USP
		|| psInst->eOpcode == ISMPUNPACK_USP
		|| psInst->eOpcode == ISMPGRAD_USP
		|| psInst->eOpcode == ISMP_USP_NDR
#endif
		|| psInst->eOpcode == ISMP
		|| psInst->eOpcode == ISMPGRAD
		|| psInst->eOpcode == ISMPREPLACE
		|| psInst->eOpcode == ISMPBIAS)
	{
		if (psState->ePredicationLevel < UF_PREDLVL_NESTED_SAMPLES)
		{
			/* can only predicate samples if the predication level is high enough */
			return IMG_FALSE;
		}
		else if (psInst->u.psSmp->bTextureArray)
		{
			/* cannot predicate dynamically-calculated samples */
			return IMG_FALSE;
		}
	}

	if (eOpcode == ILDAB ||
	eOpcode == ILDAW ||
	eOpcode == ILDAD ||
	eOpcode == ILDLD ||
	eOpcode == ISTAB ||
	eOpcode == ISTARRC10 ||
	eOpcode == ISTARRF16 ||
	eOpcode == ISTARRF32 ||
	eOpcode == ISTARRU8 ||
	eOpcode == ISTAW ||
	eOpcode == ISTAD ||
	eOpcode == ISTLB ||
	eOpcode == ISTLD ||
	eOpcode == ISTLW ||
	eOpcode == ILDTD
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	 || eOpcode == ILDAQ
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	)
	{
		/* cannot current predicate load/store instructions safely*/
		return IMG_FALSE;
	}

	if (psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25355)
	{
		if (
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			eOpcode == ILDAQ ||
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			eOpcode == ISPILLREAD ||
			eOpcode == ISMP || /* ACL These 4 added in usc2 (?!)... */
			eOpcode == ISMPBIAS ||
			eOpcode == ISMPGRAD ||
			eOpcode == ISMPREPLACE)
		{
			return IMG_FALSE;
		}
	}

	#if defined(SUPPORT_SGX545)
	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33442) != 0)
	{
		if (eOpcode == IIDIV32 || eOpcode == IIMA32)
		{
			return IMG_FALSE;
		}
	}
	#endif /* defined(SUPPORT_SGX545) */

	return g_psInstDesc[eOpcode].bCanPredicate;
}

static
IMG_VOID SetDeltaMov(PINTERMEDIATE_STATE psState, PINST psMOVInst, PINST psDeltaInst)
/******************************************************************************
 FUNCTION		: DeltaToMov

 DESCRIPTION	: Set up a move instruction suitable for copying the data selected
				  by a delta instruction.

 PARAMETERS		: psState		- Compiler state.
				  psMOVInst		- MOV instruction to set up.
				  psDeltaInst	- Delta function.

 RETURNS		: Nothing.
******************************************************************************/
{
	if (psDeltaInst->asDest[0].uType  == USEASM_REGTYPE_TEMP)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psDeltaInst->u.psDelta->bVector)
		{
			SetOpcode(psState, psMOVInst, IVMOV);
			psMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psMOVInst->u.psVec->aeSrcFmt[0] = psDeltaInst->asDest[0].eFmt;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetOpcode(psState, psMOVInst, IMOV);
		}
	}
	else
	{
		SetOpcode(psState, psMOVInst, IMOVPRED);
	}
}

static
IMG_VOID ConvertConstantDeltaToMov(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   PINST				psDeltaInst,
								   IMG_UINT32			auPredIdx[])
/******************************************************************************
 FUNCTION		: ConvertConstantDeltaToMov

 DESCRIPTION	: Check for cases which can be optimized for a delta function in a block
				  where two predecessor edges point back to the same conditional.

 PARAMETERS		: psState		- Compiler state.
				  psBlock		- Block to insert the moves into.
				  psDeltaInst	- Delta function.
				  auPredIdx		- Indices of the two predecessor edges.

 RETURNS		: Nothing.
******************************************************************************/
{
	IMG_BOOL	abBooleanValue[2];
	IMG_UINT32	uMovDestPredNum;
	IMG_UINT32	uIdx;
	PINST		psMovInst;

	/*
		Skip delta instructions not choosing between predicates.
	*/
	if (psDeltaInst->asDest[0].uType != USEASM_REGTYPE_PREDICATE)
	{
		return;
	}

	/*
		Check the source arguments to the delta instruction corresponding to
		each arm of the conditional are constants.
	*/
	for (uIdx = 0; uIdx < 2; uIdx++)
	{		
		PARG	psDeltaArg = &psDeltaInst->asArg[auPredIdx[uIdx]];

		if (psDeltaArg->uType != USC_REGTYPE_BOOLEAN)
		{
			return;
		}
		abBooleanValue[uIdx] = psDeltaArg->uNumber ? IMG_TRUE : IMG_FALSE;
	}

	/*
		Allocate a new predicate register.
	*/
	uMovDestPredNum = GetNextPredicateRegister(psState);

	/*
		Replace the delta instruction by a MOV.
	*/
	psMovInst = AllocateInst(psState, psDeltaInst);
	SetOpcode(psState, psMovInst, IMOVPRED);
	SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_PREDICATE, uMovDestPredNum, UF_REGFORMAT_F32);

	if (abBooleanValue[0] == abBooleanValue[1])
	{
		SetSrc(psState, psMovInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, abBooleanValue[0], UF_REGFORMAT_F32);
	}
	else
	{
		SetSrcFromArg(psState, psMovInst, 0 /* uSrcIdx */, &psBlock->u.sCond.sPredSrc);
		psMovInst->u.psMovp->bNegate = (!abBooleanValue[0]) ? IMG_TRUE : IMG_FALSE;
	}
	AppendInst(psState, psBlock, psMovInst);

	if (psDeltaInst->uArgumentCount == 2)
	{
		/*
			Convert the DELTA instruction to a simple MOV.
		*/
		SetOpcode(psState, psDeltaInst, IMOVPRED);
		SetSrc(psState, psDeltaInst, 0 /* uSrcIdx */, USEASM_REGTYPE_PREDICATE, uMovDestPredNum, UF_REGFORMAT_F32);
	}
	else
	{
		/*
			Replace both of the sources to the delta instruction by the result of the MOV.
		*/
		SetSrc(psState, psDeltaInst, auPredIdx[0], USEASM_REGTYPE_PREDICATE, uMovDestPredNum, UF_REGFORMAT_F32);
		SetSrc(psState, psDeltaInst, auPredIdx[1], USEASM_REGTYPE_PREDICATE, uMovDestPredNum, UF_REGFORMAT_F32);
	}
}

static
IMG_VOID ConvertDeltaInstructionsToPredCopy(PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCond)
/******************************************************************************
 FUNCTION		: ConvertDeltaInstructionsToPredCopy

 DESCRIPTION	: Check for cases where the two arms of a conditional go to a
				  successor block which chooses between two constant values for
				  a predicate based on which arm of the conditional was taken.

 PARAMETERS		: psState		- Compiler state.
				  psCond		- Conditional to check.

 RETURNS		: Nothing.
******************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;
	IMG_UINT32		uSucc;
	IMG_UINT32		auPredIdx[2];
	PCODEBLOCK		psIPostDom = psCond->psIPostDom;

	ASSERT(psCond->uNumSuccs == 2);
	for (uSucc = 0; uSucc < 2; uSucc++)
	{
		PCODEBLOCK	psCondArm;

		psCondArm = psCond->asSuccs[uSucc].psDest;
		if (psCondArm == psIPostDom)
		{
			auPredIdx[uSucc] = psCond->asSuccs[uSucc].uDestIdx;
		}
		else
		{
			if (psCondArm->uNumPreds != 1 || psCondArm == psCondArm->psOwner->psEntry)
			{
				return;
			}
			if (psCondArm->eType != CBTYPE_UNCOND)
			{
				return;
			}
			ASSERT(psCondArm->uNumSuccs == 1);
			if (psCondArm->asSuccs[0].psDest != psCond->psIPostDom)
			{
				return;
			}

			auPredIdx[uSucc] = psCondArm->asSuccs[0].uDestIdx;
		}
	}

	for (psListEntry = psIPostDom->sDeltaInstList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;

		psNextListEntry = psListEntry->psNext;
		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;

		ASSERT(psDeltaInst->eOpcode == IDELTA);
		ASSERT(psDeltaInst->psBlock == psIPostDom);

		ConvertConstantDeltaToMov(psState, psCond, psDeltaInst, auPredIdx);
	}
}

static
IMG_UINT32 GetConditionalEdge(PCODEBLOCK psSrc, IMG_UINT32 uArm)
/******************************************************************************
 FUNCTION		: GetConditionalEdge

 DESCRIPTION	: Get the index in the predecessor array of the common successor of a 
				  conditional corresponding to one arm of the conditional.

 PARAMETERS		: psSrc			- Conditional.
				  uArm			- Conditional arm.

 RETURNS		: Nothing.
******************************************************************************/
{
	if (psSrc->asSuccs[uArm].psDest == psSrc->psIPostDom)
	{
		return psSrc->asSuccs[uArm].uDestIdx;
	}
	else
	{
		PCODEBLOCK psBlock = psSrc->asSuccs[uArm].psDest;

		while (psBlock->asSuccs[0].psDest != psSrc->psIPostDom)
		{
			if (psBlock->psIPostDom == NULL || psBlock->psIPostDom->asSuccs == NULL)
			{
				break;
			}
			if (psSrc->psIPostDom->uNumPreds != 1)
			{
				break;
			}
			psBlock = psBlock->psIPostDom;
		}

		return psBlock->asSuccs[0].uDestIdx;
	}
}

typedef struct _DELTA_FLATTEN
{
	IMG_BOOL	abDuplicate[2];
	IMG_BOOL	abFirstDuplicate[2];

	IMG_BOOL	abNonTemporary[2];

	IMG_BOOL	abDefinedInsideArm[2];
	IMG_BOOL	abConditionalWriteAvailable[2];

	IMG_BOOL	abInsertMoveOne[2];
	IMG_BOOL	abInsertMoveBoth[2];
	IMG_UINT32	uCondWriteArm;
} DELTA_FLATTEN, *PDELTA_FLATTEN;

static
IMG_BOOL IsDuplicatedDeltaSource(PCODEBLOCK				psSucc, 
								 PUSC_LIST_ENTRY		psStop,
								 PARG					psSrc, 
								 IMG_UINT32				uEdge,
								 IMG_PUINT32			puFirstOccurance)
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uDeltaInst;

	*puFirstOccurance = USC_UNDEF;
	for (psListEntry = psSucc->sDeltaInstList.psHead, uDeltaInst = 0; 
		 psListEntry != psStop; 
		 psListEntry = psListEntry->psNext, uDeltaInst++)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;
		PARG			psDeltaSrc;

		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;
		psDeltaSrc = &psDeltaInst->asArg[uEdge];

		if (EqualArgs(psDeltaSrc, psSrc))
		{
			*puFirstOccurance = uDeltaInst;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID CanFlattenDeltaInstructions(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psSrc,
									 IMG_UINT32				auEdgeToSucc[],
									 IMG_UINT32				auOneArmInstCount[],
									 IMG_UINT32				auBothArmsInstCount[],
									 PDELTA_FLATTEN*		pasFlat)
{
	PCODEBLOCK		psSucc = psSrc->psIPostDom;
	IMG_UINT32		uArm;
	PDELTA_FLATTEN	asFlat;
	IMG_UINT32		uDeltaInstCount;
	IMG_UINT32		uDeltaInst;
	PUSC_LIST_ENTRY	psListEntry;

	/*
		Initialize return data.
	*/
	for (uArm = 0; uArm < 2; uArm++)
	{
		auOneArmInstCount[uArm] = 0;
		auBothArmsInstCount[uArm] = 0;
	}

	/*
		Count the number of delta instructions in the successor of the conditional.
	*/
	uDeltaInstCount = 0;
	for (psListEntry = psSucc->sDeltaInstList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		uDeltaInstCount++;
	}

	/*
		Allocate an array to record what instruction to generate from each delta instruction when
		flattening the conditional.
	*/
	*pasFlat = asFlat = UscAlloc(psState, sizeof(asFlat[0]) * uDeltaInstCount);
	memset(asFlat, 0, sizeof(asFlat[0]) * uDeltaInstCount);

	/*
		For each delta instruction in the successor...
	*/
	for (psListEntry = psSucc->sDeltaInstList.psHead, uDeltaInst = 0; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext, uDeltaInst++)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;
		PDELTA_FLATTEN	psFlat = &asFlat[uDeltaInst];

		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;

		for (uArm = 0; uArm < 2; uArm++)
		{
			IMG_UINT32		uEdgeToSucc = auEdgeToSucc[uArm];
			IMG_UINT32		uOtherEdge = auEdgeToSucc[1 - uArm];
			PCODEBLOCK		psArm = psSrc->asSuccs[uArm].psDest;
			IMG_UINT32		uFirstDuplicate;
			PARG			psDeltaSrc = &psDeltaInst->asArg[uEdgeToSucc];
			PARG			psOtherDeltaSrc = &psDeltaInst->asArg[uOtherEdge];
			PINST			psDefInst;
			IMG_UINT32		uDefDestIdx;
			PUSEDEF_CHAIN	psDeltaSrcUseDef;
			IMG_BOOL		bDefinedInSecondary;

			/*
				Find the instruction defining this source.
			*/
			psDeltaSrcUseDef = NULL;
			if (psDeltaSrc->uType == USEASM_REGTYPE_TEMP || psDeltaSrc->uType == USEASM_REGTYPE_PREDICATE)
			{
				psDeltaSrcUseDef = UseDefGet(psState, psDeltaSrc->uType, psDeltaSrc->uNumber);
			}
			bDefinedInSecondary = IMG_FALSE;
			psDefInst = NULL;
			uDefDestIdx = USC_UNDEF;
			if (psDeltaSrcUseDef != NULL)
			{
				PUSEDEF	psDeltaSrcDef;

				psDeltaSrcDef = psDeltaSrcUseDef->psDef;
				if (psDeltaSrcDef != NULL)
				{
					if (psDeltaSrcDef->eType == DEF_TYPE_INST)
					{
						psDefInst = psDeltaSrcDef->u.psInst;
						uDefDestIdx = psDeltaSrcDef->uLocation;
					}

					/*
						Check if the source is defined in the secondary program.
					*/
					if (psDeltaSrcDef->eType == DEF_TYPE_FIXEDREG && !psDeltaSrcDef->u.psFixedReg->bPrimary)
					{
						bDefinedInSecondary = IMG_TRUE;
					}
					if (psDefInst != NULL && (psDefInst->psBlock->psOwner->psFunc == psState->psSecAttrProg))
					{
						bDefinedInSecondary = IMG_TRUE;
					}
				}
			}

			/*
				Check if the source is either a constant or will be replaced by a secondary attribute
				later on.
			*/
			psFlat->abNonTemporary[uArm] = IMG_FALSE;
			if (psDeltaSrcUseDef == NULL || bDefinedInSecondary)
			{
				psFlat->abNonTemporary[uArm] = IMG_TRUE;
			}


			/*
				Check if this source argument already occured as a source to a previous delta instruction.
			*/
			if (IsDuplicatedDeltaSource(psSucc, 
										psListEntry, 
										psDeltaSrc, 
										uEdgeToSucc,
										&uFirstDuplicate))
			{
				psFlat->abDuplicate[uArm] = IMG_TRUE;
				psFlat->abFirstDuplicate[uArm] = IMG_FALSE;

				/*
					Record for the earlier occurence of this source that it has a later duplicate.
				*/
				asFlat[uFirstDuplicate].abDuplicate[uArm] = IMG_TRUE;
				asFlat[uFirstDuplicate].abFirstDuplicate[uArm] = IMG_TRUE;
			}
			else
			{
				psFlat->abDuplicate[uArm] = IMG_FALSE;
			}

			/*
				Check if this source is defined by an instruction inside the conditional
				arm.
			*/
			psFlat->abDefinedInsideArm[uArm] = IMG_FALSE;
			if (!psFlat->abDuplicate[uArm] && psArm != psSrc->psIPostDom)
			{
				if (psDefInst != NULL && UnconditionallyFollows(psState, psArm, psDefInst->psBlock))
				{
					PARG	psOldDest;

					psFlat->abDefinedInsideArm[uArm] = IMG_TRUE;

					psOldDest = psDefInst->apsOldDest[uDefDestIdx];
					if (psOldDest == NULL || EqualArgs(psOldDest, psOtherDeltaSrc))
					{
						psFlat->abConditionalWriteAvailable[uArm] = IMG_TRUE;
					}
				}
			}
		}
	}

	for (uDeltaInst = 0; uDeltaInst < uDeltaInstCount; uDeltaInst++)
	{
		PDELTA_FLATTEN	psFlat = &asFlat[uDeltaInst];
		IMG_BOOL		abCondWrite[2];

		for (uArm = 0; uArm < 2; uArm++)
		{
			if ((psFlat->abDuplicate[uArm] && !psFlat->abFirstDuplicate[uArm]) || !psFlat->abDefinedInsideArm[uArm])
			{
				psFlat->abInsertMoveOne[uArm] = IMG_TRUE;
				auOneArmInstCount[uArm]++;
				if (!psFlat->abNonTemporary[uArm])
				{
					auOneArmInstCount[uArm]++;
				}
			}
		}

		for (uArm = 0; uArm < 2; uArm++)
		{
			abCondWrite[uArm] = psFlat->abDefinedInsideArm[uArm] && 
								psFlat->abConditionalWriteAvailable[uArm] &&
								!psFlat->abDuplicate[uArm];
		}

		/*
			If at least one source to the delta instruction is defined inside one of the conditional
			arms then 
		*/
		if (abCondWrite[1])
		{
			psFlat->uCondWriteArm = 1;
		}
		else if (abCondWrite[0] && !psFlat->abDefinedInsideArm[1] && !psFlat->abDuplicate[1])
		{
			psFlat->uCondWriteArm = 0;
		}
		else
		{
			psFlat->abInsertMoveBoth[1] = IMG_TRUE;
			psFlat->uCondWriteArm = 1;

			auBothArmsInstCount[1]++;
			if (psFlat->abNonTemporary[0] && psFlat->abNonTemporary[1])
			{
				auBothArmsInstCount[0]++;
			}
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static 
IMG_VOID GetMovcSwizzle(PINTERMEDIATE_STATE	psState,
						PINST				psTestInst,
						PINST				psDeltaInst,
						UF_REGFORMAT		eTestFmt,
						IMG_UINT32			*auSwizzle)
/******************************************************************************
 FUNCTION		: GetMovcSwizzle

 DESCRIPTION	: Find the swizzle required on a MovC instruction.

 PARAMETERS		: psState		- Compiler state.
				  psTestInst	- Instruction to convert into MovC
				  psDeltaInst	- The delta instruction to convert into a MOVC
				  eTestFmt		- the datatype of the test
				  auSwizzle		- reference to a variable that will receive MOVC 
								  test swizzle

 RETURNS		: Nothing.
******************************************************************************/
{
	ASSERT(psTestInst != IMG_NULL);

	*auSwizzle = USEASM_SWIZZLE (X, X, X, X);

	if (
			psTestInst->eOpcode == IVTEST && 
			psTestInst->u.psVec->sTest.eTestOpcode == IVADD && 
			psDeltaInst->asDest[0].uType  == USEASM_REGTYPE_TEMP && 
			eTestFmt == psDeltaInst->asDest[0].eFmt && 
			!psTestInst->u.psVec->asSrcMod [0].bAbs && 
			!psTestInst->u.psVec->asSrcMod [0].bNegate
		)
	{
		VTEST_CHANSEL eChanSel = psTestInst->u.psVec->sTest.eChanSel;

		if (eChanSel == VTEST_CHANSEL_XCHAN)
		{
			/* only selecting 1 channel, but need to broadcast conditional
			   across all channels for it to work with IVMOVC */

			*auSwizzle = USEASM_SWIZZLE(X, X, X, X);
		}
	}
}

static
PINST CanMovCBecomeMAdd(PINTERMEDIATE_STATE	psState,
						   PINST				psTestInst,
						   PINST				psDeltaInst,
						   IMG_UINT32			uArmEdge,
						   IMG_UINT32			*puIncrementIdx)
/******************************************************************************
 FUNCTION		: CanMovCBecomeMAdd

 DESCRIPTION	: Returns whether a conditional with a test&delta instruction
                  can become TESTMASK and MADD instructions

 PARAMETERS		: psState		- Current compilation context
				  psTestInst	- The Test/VTest instruction for the conditional
				  psDeltaInst	- The delta instruction to convert into a MADD
				  uArmEdge		- Index of Delta source

 RETURNS		: true if test and delta can become a single MOVC instruction
******************************************************************************/
{
	IMG_UINT32	uDestIdx, uSrc, uOldDestIdx;
	PINST		psDefInst, psOldDefInst;

	PVR_UNREFERENCED_PARAMETER(psState);
	
	if (psDeltaInst->uArgumentCount != 2)
	{
		return IMG_NULL;
	}

	ASSERT(uArmEdge == 0 || uArmEdge == 1);
	
	psDefInst = UseDefGetDefInst(psState, psDeltaInst->asArg[uArmEdge].uType, psDeltaInst->asArg[uArmEdge].uNumber, &uDestIdx);
	psOldDefInst = UseDefGetDefInst(psState, psDeltaInst->asArg[1-uArmEdge].uType, psDeltaInst->asArg[1-uArmEdge].uNumber, &uOldDestIdx);

	if (psTestInst == IMG_NULL ||
		psDefInst == IMG_NULL || 
		uDestIdx != 0 ||
		psDefInst->eOpcode != IVADD ||
		psOldDefInst == IMG_NULL || 
		uOldDestIdx != 0 ||
		psOldDefInst->eOpcode != IVMOV ||
		psOldDefInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE ||
		psTestInst->asArg[0].eFmt != psDefInst->asArg[0].eFmt ||
		(
			psDefInst->asArg[0].eFmt != UF_REGFORMAT_F16 &&
			psDefInst->asArg[0].eFmt != UF_REGFORMAT_F32
		)
		)
	{
		return IMG_NULL;
	}

	if (psOldDefInst->asArg[0].uType == psDefInst->asArg[0].uType &&
		psOldDefInst->asArg[0].uNumber == psDefInst->asArg[0].uNumber)
	{
		uSrc = 0;
		*puIncrementIdx = 5;
	}
	else if (psDeltaInst->asArg[0].uType == psDefInst->asArg[5].uType &&
		     psDeltaInst->asArg[0].uNumber == psDefInst->asArg[5].uNumber)
	{
		uSrc = 5;
		*puIncrementIdx = 0;
	}
	else
	{
		return IMG_NULL;
	}
	/* check swizzles etc*/

	if (!CompareSwizzles (psDefInst->u.psVec->auSwizzle[uSrc], USEASM_SWIZZLE (X, Y, Z, W), psDefInst->auLiveChansInDest [0]) ||
		psDefInst->u.psVec->asSrcMod[uSrc].bAbs ||
		psDefInst->u.psVec->asSrcMod[uSrc].bNegate)
	{
		return IMG_NULL;
	}

	return psDefInst;
}
#endif /* #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_BOOL CanTestBecomeMovC(PINTERMEDIATE_STATE	psState,
						   PINST				psTestInst,
						   PINST				psDeltaInst,
						   PTEST_TYPE			peTestMode,
						   UF_REGFORMAT			eTestFmt)
/******************************************************************************
 FUNCTION		: CanTestBecomeMovC

 DESCRIPTION	: Returns whether a conditional with a test&delta instruction
                  can become a single MOVC instruction.

 PARAMETERS		: psState		- Current compilation context
				  psTestInst	- The Test/VTest instruction for the conditional
				  psDeltaInst	- The delta instruction to convert into a MOVC
				  peTestMode	- reference to a variable that will receive MOVC test
				  eTestFmt		- the datatype of the test

 RETURNS		: true if test and delta can become a single MOVC instruction
******************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if (psTestInst == IMG_NULL)
	{
		return IMG_FALSE;
	}
	else if (psTestInst->eOpcode == ITESTPRED
		 && (psTestInst->u.psTest->eAluOpcode == IFSUB
		  || psTestInst->u.psTest->eAluOpcode == IFMOV)
		 && psTestInst->u.psTest->auSrcComponent [0] == 0
		 && psDeltaInst->asDest[0].uType  == USEASM_REGTYPE_TEMP
		 && 
		 (
			eTestFmt == psDeltaInst->asDest[0].eFmt ||
			(
				eTestFmt == UF_REGFORMAT_F32 && 
				psDeltaInst->asDest[0].eFmt == UF_REGFORMAT_F16 &&
				psDeltaInst->auLiveChansInDest[0] == 15
			)
		 ))
	{
		if (psDeltaInst->asDest[0].eFmt == UF_REGFORMAT_F16
			&& psDeltaInst->auLiveChansInDest[0] != 3)
		{
			/* this is a 16-bit vector conditional, which can't be done with an f16 MOVC */
			return IMG_FALSE;
		}
		if (psTestInst->u.psTest->eAluOpcode == IFSUB)
		{
			if ((psTestInst->asArg [1].uNumber != EURASIA_USE_SPECIAL_CONSTANT_ZERO1 &&
				psTestInst->asArg [1].uNumber != EURASIA_USE_SPECIAL_CONSTANT_ZERO)
				|| psTestInst->asArg [1].uType != USEASM_REGTYPE_FPCONSTANT)
			{
				/* not comparing with 0 */
				return IMG_FALSE;
			}
		}

		*peTestMode = psTestInst->u.psTest->sTest.eType;

		if (*peTestMode == TEST_TYPE_GT_ZERO || *peTestMode == TEST_TYPE_LTE_ZERO)
		{
			/* condition not supported by MOVC */
			return IMG_FALSE;
		}
	}
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	else if (
				psTestInst->eOpcode == IVTEST && 
				psTestInst->u.psVec->sTest.eTestOpcode == IVADD && 
				psDeltaInst->asDest[0].uType  == USEASM_REGTYPE_TEMP &&
				eTestFmt == psDeltaInst->asDest[0].eFmt && 
				!psTestInst->u.psVec->asSrcMod [0].bAbs && 
				!psTestInst->u.psVec->asSrcMod [0].bNegate
			)
	{
		if (psTestInst->u.psVec->sTest.eTestOpcode == IVADD)
		{
			if ((psTestInst->asArg [SOURCE_ARGUMENTS_PER_VECTOR+1].uNumber != EURASIA_USE_SPECIAL_CONSTANT_ZERO1 && 
				psTestInst->asArg [SOURCE_ARGUMENTS_PER_VECTOR+1].uNumber != EURASIA_USE_SPECIAL_CONSTANT_ZERO)
			 || psTestInst->asArg [SOURCE_ARGUMENTS_PER_VECTOR+1].uType != USEASM_REGTYPE_FPCONSTANT)
			{
				/* not comparing with 0 */
				return IMG_FALSE;
			}
		}

		*peTestMode = psTestInst->u.psVec->sTest.eType;

		if (psTestInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN ||
			psTestInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_ORALL ||
			psTestInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_ANDALL)
		{
			/* combining multiple channels into 1 condition */
			if (!psTestInst->u.psVec->auSwizzle [0] == USEASM_SWIZZLE (X, X, X, X) &&
				!psTestInst->u.psVec->auSwizzle [0] == USEASM_SWIZZLE (Y, Y, Y, Y) &&
				!psTestInst->u.psVec->auSwizzle [0] == USEASM_SWIZZLE (Z, Z, Z, Z) &&
				!psTestInst->u.psVec->auSwizzle [0] == USEASM_SWIZZLE (W, W, W, W))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			ASSERT(psTestInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_XCHAN);
		}
	}
#endif
	else
	{
		return IMG_FALSE;
	}

	if (*peTestMode == TEST_TYPE_EQ_ZERO
		|| *peTestMode == TEST_TYPE_NEQ_ZERO
		|| *peTestMode == TEST_TYPE_GT_ZERO
		|| *peTestMode == TEST_TYPE_GTE_ZERO
		|| *peTestMode == TEST_TYPE_LT_ZERO
		|| *peTestMode == TEST_TYPE_LTE_ZERO)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


static
IMG_VOID InsertDeltaMOVC(PINTERMEDIATE_STATE	psState, 
						 PINST					psDeltaInst, 
						 IMG_UINT32				uArmEdge,
						 PCODEBLOCK				psSrc,
						 IMG_UINT32				uPredSrc,
						 IMG_BOOL				bPredNegate,
						 PARG					*ppsPreviousRegister,
						 PINST					psTestInst,
						 PINST*					ppsLastTestInst)
/******************************************************************************
 FUNCTION		: InsertDeltaMOVC

 DESCRIPTION	: This function converts DELTA instructions into MOVC instructions
			      The first arm of a delta becomes a MOV instruction, the second
				  becomes a MOVC instruction.

 PARAMETERS		: psState	- Current compilation context
				  psDeltaInst - The delta instruction to convert into a MOVC
				  uArmEdge  - the arm of the delta instruction to process now
				  psSrc     - the basic block of the conditional
				  uPredSrc  - the predicate of the conditional
				  bPredNegate - the negate flag of the predicate of the conditional
				  pTestInst - the TEST or TP instruction generating the predicate
				  ppsLastTestInst - the new test instruction generated by this MOVC

 RETURNS		: Nothing.
******************************************************************************/
{
	PINST		psMOVCInst;
	IMG_UINT32	uMOVTempNum;
	PARG		psDeltaDest = &psDeltaInst->asDest[0];

	/*
		Create a MOV instruction suitable for copying the sourec arguments to the
		DELTA instruction.
	*/
	psMOVCInst = AllocateInst(psState, psDeltaInst);
	SetDeltaMov(psState, psMOVCInst, psDeltaInst);

	/*
		Allocate a temporary register for the result of the MOV.
	*/
	if (psDeltaDest->uType == USEASM_REGTYPE_TEMP)
	{
		uMOVTempNum = GetNextRegister(psState);
	}
	else
	{
		ASSERT(psDeltaDest->uType == USEASM_REGTYPE_PREDICATE);
		uMOVTempNum = GetNextPredicateRegister(psState);
	}

	/*
		Set the destination of the MOV to the newly allocated temporary register.
	*/
	SetDest(psState, psMOVCInst, 0 /* uDestIdx */, psDeltaDest->uType, uMOVTempNum, psDeltaDest->eFmt);
	psMOVCInst->auLiveChansInDest[0] = psDeltaInst->auLiveChansInDest[0];
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psMOVCInst->eOpcode == IVMOV)
	{
		psMOVCInst->auDestMask[0] = psDeltaInst->auLiveChansInDest[0];
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (*ppsPreviousRegister)
	{
		TEST_TYPE eTestMode;
		UF_REGFORMAT eTestFmt = UF_REGFORMAT_INVALID;

		if (psTestInst)
		{
			IMG_UINT32 uComponent;
			for (uComponent=0; uComponent<psTestInst->uArgumentCount; uComponent++)
			{
				if (psTestInst->asArg[uComponent].uType != USC_REGTYPE_UNUSEDSOURCE)
				{
					eTestFmt = psTestInst->asArg[uComponent].eFmt;
					break;
				}
			}
		}

		/* 
		   This is a conditional move, so set the other arm of the CMOV to 
		   the previous register value 
		*/
		if (psTestInst == IMG_NULL || /* can't use MOVC - no test instruction */
			(psTestInst->eOpcode != ITESTPRED
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			&& psTestInst->eOpcode != IVTEST
#endif
			) ||
			(eTestFmt == UF_REGFORMAT_F16 && psTestInst->eOpcode == ITESTPRED) ||
			eTestFmt == UF_REGFORMAT_INVALID ||
			(
				eTestFmt != psDeltaInst->asDest[0].eFmt &&
				!(
					eTestFmt == UF_REGFORMAT_F32 && 
					psDeltaInst->asDest[0].eFmt == UF_REGFORMAT_F16
				)
			) ||
			psDeltaDest->uType == USEASM_REGTYPE_PREDICATE)
		{ 
			/*
				Can't use a MOVC instruction
				Copy the predicate used to choose between the two arms of the conditional.
			*/
			SetPredicate(psState, psMOVCInst, uPredSrc, bPredNegate);

			SetPartiallyWrittenDest(psState, 
									psMOVCInst, 
									0,  
									*ppsPreviousRegister);
			/*
				Copy the source to the DELTA instruction corresponding to this arm of the
				conditional to the MOV instruction.
			*/
			MoveSrc(psState, psMOVCInst, 0 /* uSrcIdx */, psDeltaInst, uArmEdge);
		}
		else
		{
			IMG_UINT32 uSrcSize = 1, uComponent, uTrueSrc=1, uFalseSrc=2;
			IMG_BOOL bNegate = IMG_FALSE;
			IMG_BOOL bCanBeMAdd = IMG_FALSE;

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			IMG_UINT32 uIncrementSrc;
			PINST psIncrementInst = CanMovCBecomeMAdd(psState, psTestInst, psDeltaInst, uArmEdge, &uIncrementSrc);
			IMG_UINT32  auSwizzle = USEASM_SWIZZLE (X, X, X, X);

			bCanBeMAdd = (IMG_BOOL)(!bPredNegate && psIncrementInst != IMG_NULL);
			
			/* use a MOVC instruction */
			if (psTestInst->eOpcode == IVTEST) {
				if (bCanBeMAdd)
				{
					SetOpcode (psState, psMOVCInst, IVMAD);
				}
				else
				{
					SetOpcode (psState, psMOVCInst, IVMOVC);
				}
				uSrcSize = 5;
			}
			else
#endif
			{
				SetOpcode (psState, psMOVCInst, IMOVC);
			}

			if (CanTestBecomeMovC(psState, psTestInst, psDeltaInst, &eTestMode, eTestFmt) && 
				!bCanBeMAdd)
			{
				for (uComponent=0; uComponent<uSrcSize; uComponent++) 
				{
					CopySrc (psState, psMOVCInst, uComponent, psTestInst, uComponent);
				}

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psMOVCInst->eOpcode == IVMOVC)
				{
					IMG_UINT32 uChan, uSel;

					ASSERT(psTestInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_XCHAN);

					uSel = GetChan(psTestInst->u.psVec->auSwizzle[0], 0);
					psMOVCInst->u.psVec->auSwizzle[0] = 0;

					for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
					{
						psMOVCInst->u.psVec->auSwizzle[0] = SetChan(psMOVCInst->u.psVec->auSwizzle[0], uChan, uSel);
					}
				}
				#endif

				/* Only .tz, .t!z, .tn or .tp|z are valid tests for movc */
				if (eTestMode == TEST_TYPE_GT_ZERO)
				{
					eTestMode = TEST_TYPE_LT_ZERO;
					bNegate = IMG_TRUE;
				}
				else if (eTestMode == TEST_TYPE_LTE_ZERO)
				{
					eTestMode = TEST_TYPE_GTE_ZERO;
					bNegate = IMG_TRUE;
				}
				if (bPredNegate)
				{
					uTrueSrc = 2; uFalseSrc = 1;
				}

				/*
					Copy the source to the DELTA instruction corresponding to this arm of the
					conditional to the MOV instruction.
				*/
				MoveSrc(psState, psMOVCInst, uTrueSrc*uSrcSize, psDeltaInst, uArmEdge);
			}
			else
			{
				/* this IDELTA requires a TESTMASK and MOVC */
				IMG_UINT32	uTESTMASKTempNum;
				PINST psTESTMASKInst;
				
				/*
					Create an instruction suitable for doing a compare with zero
					This is the comparison component of the test instruction
				*/
				psTESTMASKInst = AllocateInst(psState, psDeltaInst);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psTestInst->eOpcode == IVTEST)
				{
					GetMovcSwizzle(psState, psTestInst, psDeltaInst, eTestFmt, &auSwizzle);

					SetOpcode (psState, psTESTMASKInst, IVTESTMASK);
					psTESTMASKInst->u.psVec->sTest.eTestOpcode = psTestInst->u.psVec->sTest.eTestOpcode;
					psTESTMASKInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;
					psTESTMASKInst->u.psVec->sTest.eType = psTestInst->u.psVec->sTest.eType;
					psTESTMASKInst->u.psVec->aeSrcFmt [0] = psTestInst->u.psVec->aeSrcFmt [0];
					psTESTMASKInst->u.psVec->aeSrcFmt [1] = psTestInst->u.psVec->aeSrcFmt [1];
					psTESTMASKInst->u.psVec->auSwizzle [0] = psTestInst->u.psVec->auSwizzle [0];
					psTESTMASKInst->u.psVec->auSwizzle [1] = psTestInst->u.psVec->auSwizzle [1];
					psTESTMASKInst->u.psVec->asSrcMod [0] = psTestInst->u.psVec->asSrcMod [0];
					psTESTMASKInst->u.psVec->asSrcMod [1] = psTestInst->u.psVec->asSrcMod [1];
					psTESTMASKInst->u.psVec->asSrcMod [1].bNegate = psTestInst->u.psVec->asSrcMod [1].bNegate;
					psTESTMASKInst->u.psVec->asSrcMod [1].bAbs = psTestInst->u.psVec->asSrcMod [1].bAbs;
					psTESTMASKInst->u.psVec->asSrcMod [0].bNegate = psTestInst->u.psVec->asSrcMod [0].bNegate;
					psTESTMASKInst->u.psVec->asSrcMod [0].bAbs = psTestInst->u.psVec->asSrcMod [0].bAbs;

					psMOVCInst->u.psVec->auSwizzle [0] = auSwizzle;

					psTESTMASKInst->auDestMask[0] = psTESTMASKInst->auLiveChansInDest[0] = USC_X_CHAN_MASK;
				}
				else
	#endif
				{
					bCanBeMAdd = IMG_FALSE; // can't do an I32 MADD
					SetOpcode (psState, psTESTMASKInst, ITESTMASK);
					SetOpcode (psState, psMOVCInst, IMOVC_I32);
					psTESTMASKInst->u.psTest->eAluOpcode =  psTestInst->u.psTest->eAluOpcode;
					psTESTMASKInst->u.psTest->auSrcComponent [0] = psTestInst->u.psTest->auSrcComponent [0];
					psTESTMASKInst->u.psTest->auSrcComponent [1] = psTestInst->u.psTest->auSrcComponent [1];
					psTESTMASKInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
					psTESTMASKInst->u.psTest->sTest.eType = psTestInst->u.psTest->sTest.eType;

					psTESTMASKInst->auDestMask[0] = psTESTMASKInst->auLiveChansInDest[0] = USC_ALL_CHAN_MASK;
				}
				uTESTMASKTempNum = GetNextRegister(psState);

				SetDest (psState, psTESTMASKInst, 0, USEASM_REGTYPE_TEMP, uTESTMASKTempNum, eTestFmt);

				eTestMode = TEST_TYPE_NEQ_ZERO;
				if (bPredNegate)
				{
					ASSERT(!bCanBeMAdd);
					eTestMode = TEST_TYPE_EQ_ZERO;
				}

				if (
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					psTESTMASKInst->eOpcode == IVMOV || 
	#endif
					psTESTMASKInst->eOpcode == IFMOV ||
					psTESTMASKInst->eOpcode == IMOV)
				{
					for (uComponent=0; uComponent<uSrcSize; uComponent++) 
					{
						CopySrc (psState, psTESTMASKInst, uComponent,	psTestInst, uComponent);
					}
				}
				else
				{
					for (uComponent=0; uComponent<uSrcSize; uComponent++) {
						CopySrc (psState, psTESTMASKInst, uComponent,	      psTestInst, 0*uSrcSize + uComponent);
						CopySrc (psState, psTESTMASKInst, uComponent+uSrcSize, psTestInst, 1*uSrcSize + uComponent);
					}
				}

				if (*ppsLastTestInst &&
					CompareInstructions(psState, psTESTMASKInst, *ppsLastTestInst) == 0)
				{
					/* the test mask instruction is an exact copy of the last one generated
					so just use the last one */
					FreeInst(psState, psTESTMASKInst);

					SetSrcFromArg(psState, psMOVCInst, (IMG_UINT32)bCanBeMAdd*uSrcSize, &(*ppsLastTestInst)->asDest[0]);
				}
				else
				{
					AppendInst(psState, psSrc, psTESTMASKInst);

					SetSrc (psState, psMOVCInst, (IMG_UINT32)bCanBeMAdd*uSrcSize, USEASM_REGTYPE_TEMP, uTESTMASKTempNum, eTestFmt);

					*ppsLastTestInst = psTESTMASKInst;
				}
				
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (bCanBeMAdd)
				{
					IMG_UINT32 uOneValue;
					if(psIncrementInst->asDest[0].eFmt == UF_REGFORMAT_F16)
					{
						uOneValue = FLOAT16_ONE;
					}
					else
					{
						ASSERT(psIncrementInst->asDest[0].eFmt == UF_REGFORMAT_F32);
						uOneValue = FLOAT32_ONE;
					}
					uFalseSrc = 0;
					if(IsReplicatedImmediateVector(psState, psIncrementInst, uIncrementSrc/uSrcSize, uOneValue))
					{
						SetOpcode(psState, psMOVCInst, IVADD);
					}
					else
					{
						for (uComponent=0; uComponent<uSrcSize; uComponent++) 
						{
							CopySrc(psState, psMOVCInst, 2*uSrcSize+uComponent, psIncrementInst, uIncrementSrc+uComponent);
						}
					}
					psMOVCInst->u.psVec->aeSrcFmt [0] = psDeltaDest->eFmt;
					psMOVCInst->u.psVec->aeSrcFmt [1] = psTestInst->u.psVec->aeSrcFmt [0];
					psMOVCInst->u.psVec->aeSrcFmt [2] = psIncrementInst->u.psVec->aeSrcFmt [1];
					psMOVCInst->u.psVec->auSwizzle [0] = USEASM_SWIZZLE (X, Y, Z, W);
					psMOVCInst->u.psVec->auSwizzle [1] = auSwizzle;
					psMOVCInst->u.psVec->auSwizzle [2] = psIncrementInst->u.psVec->auSwizzle [uIncrementSrc/uSrcSize];
					psMOVCInst->u.psVec->eMOVCTestType = eTestMode;
					psMOVCInst->u.psVec->asSrcMod [1].bAbs = IMG_FALSE;
					psMOVCInst->u.psVec->asSrcMod [1].bNegate = bNegate;
					psMOVCInst->u.psVec->asSrcMod [2] = psIncrementInst->u.psVec->asSrcMod [uIncrementSrc/uSrcSize];
					psMOVCInst->auDestMask [0] = psIncrementInst->auDestMask [0];
				}
				else
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				{
					/*
						Copy the source to the DELTA instruction corresponding to this arm of the
						conditional to the MOV instruction.
					*/
					MoveSrc(psState, psMOVCInst, uTrueSrc*uSrcSize, psDeltaInst, uArmEdge);
				}
			}
			
			SetSrc(psState, psMOVCInst, uFalseSrc*uSrcSize, (*ppsPreviousRegister)->uType, (*ppsPreviousRegister)->uNumber, (*ppsPreviousRegister)->eFmt);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psMOVCInst->eOpcode == IVMAD || psMOVCInst->eOpcode == IVADD)
			{
				/* MADD is already done in bCanBeMAdd above */
			}
			else if (psTestInst->eOpcode == IVTEST) 
			{
				psMOVCInst->u.psVec->aeSrcFmt [0] = psTestInst->u.psVec->aeSrcFmt [0];
				psMOVCInst->u.psVec->aeSrcFmt [1] = psDeltaDest->eFmt;
				psMOVCInst->u.psVec->auSwizzle [1] = USEASM_SWIZZLE (X, Y, Z, W);
				psMOVCInst->u.psVec->aeSrcFmt [2] = psDeltaDest->eFmt;
				psMOVCInst->u.psVec->auSwizzle [2] = USEASM_SWIZZLE (X, Y, Z, W);
				psMOVCInst->u.psVec->eMOVCTestType = eTestMode;
				psMOVCInst->u.psVec->asSrcMod [0].bAbs = IMG_FALSE;
				psMOVCInst->u.psVec->asSrcMod [0].bNegate = bNegate;
			}
			else
#endif
			{
				psMOVCInst->u.psMovc->eTest = eTestMode;
				ASSERT (!bNegate);
			}
		}
	}
	else
	{
		/*
			Copy the source to the DELTA instruction corresponding to this arm of the
			conditional to the MOV instruction.
		*/
		MoveSrc(psState, psMOVCInst, 0 /* uSrcIdx */, psDeltaInst, uArmEdge);
	}

	*ppsPreviousRegister = &psMOVCInst->asDest [0];

	/*
		Set the result of the MOV as the source to the DELTA instruction.
	*/
	SetSrc(psState, psDeltaInst, uArmEdge, psDeltaDest->uType, uMOVTempNum, psDeltaDest->eFmt);

	/*
		Add the MOV instruction to the end of the predecessor of the two arms of the conditional.
	*/
	AppendInst(psState, psSrc, psMOVCInst);
}

static
IMG_VOID InsertDeltaCopy(PINTERMEDIATE_STATE	psState, 
						 PINST					psDeltaInst, 
						 IMG_UINT32				uArmEdge,
						 PCODEBLOCK				psSrc,
						 IMG_UINT32				uPredSrc,
						 IMG_BOOL				bPredNegate)
/******************************************************************************
 FUNCTION		: InsertDeltaCopy

 DESCRIPTION	: 

 PARAMETERS		: psState	- Current compilation context
				  psDeltaInst
				  uArmEdge
				  psSrc
				  uPredSrc
				  bPredNegate

 RETURNS		: Nothing.
******************************************************************************/
{
	PINST		psMOVInst;
	IMG_UINT32	uMOVTempNum;
	PARG		psDeltaDest = &psDeltaInst->asDest[0];

	/*
		Create a MOV instruction suitable for copying the sourec arguments to the
		DELTA instruction.
	*/
	psMOVInst = AllocateInst(psState, psDeltaInst);
	SetDeltaMov(psState, psMOVInst, psDeltaInst);

	/*
		Copy the source to the DELTA instruction corresponding to this arm of the
		conditional to the MOV instruction.
	*/
	MoveSrc(psState, psMOVInst, 0 /* uSrcIdx */, psDeltaInst, uArmEdge);

	/*
		Allocate a temporary register for the result of the MOV.
	*/
	if (psDeltaDest->uType == USEASM_REGTYPE_TEMP)
	{
		uMOVTempNum = GetNextRegister(psState);
	}
	else
	{
		ASSERT(psDeltaDest->uType == USEASM_REGTYPE_PREDICATE);
		uMOVTempNum = GetNextPredicateRegister(psState);
	}

	/*
		Set the destination of the MOV to the newly allocated temporary register.
	*/
	SetDest(psState, psMOVInst, 0 /* uDestIdx */, psDeltaDest->uType, uMOVTempNum, psDeltaDest->eFmt);
	psMOVInst->auLiveChansInDest[0] = psDeltaInst->auLiveChansInDest[0];
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psMOVInst->eOpcode == IVMOV)
	{
		psMOVInst->auDestMask[0] = psDeltaInst->auLiveChansInDest[0];
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Set the result of the MOV as the source to the DELTA instruction.
	*/
	SetSrc(psState, psDeltaInst, uArmEdge, psDeltaDest->uType, uMOVTempNum, psDeltaDest->eFmt);

	/*
		Copy the predicate used to choose between the two arms of the conditional.
	*/
	SetPredicate(psState, psMOVInst, uPredSrc, bPredNegate);

	/*
		Add the MOV instruction to the end of the predecessor of the two arms of the conditional.
	*/
	AppendInst(psState, psSrc, psMOVInst);
}

static
IMG_VOID FlattenMOVCDeltaInstructions(PINTERMEDIATE_STATE psState,
								  PCODEBLOCK			psSrc,
								  IMG_UINT32			auEdgeToSucc[],
								  IMG_UINT32			uPredSrc,
								  PDELTA_FLATTEN		asDeltaFlat,
								  PINST					psTestInst)
/******************************************************************************
 FUNCTION		: FlattenMOVCDeltaInstructions

 DESCRIPTION	: Convert delta instructions in the successor of a conditional
				  block to conditional moves

 PARAMETERS		: psState		- Current compilation context
				  psSrc			- Conditional block.
				  auEdgeToSucc	- Entry in the conditional's common successor's
								predecessor array which are from each arm of
								the conditional.
				  asDeltaFlat	- Information about how to flatten the
								delta instructions.
				  abFlatten		- For each arm of the conditional: whether it
								is to be flattened to predicates.
				  uArmToDrop	- If both arms of the conditional are to be
								flattened then the conditional's successor
								which be converted to an unconditional edge.				  
			      psTestInst	- The TESTPRED or VTEST instruction generating 
				                the predicate

 RETURNS		: Nothing.
******************************************************************************/
{
	PCODEBLOCK		psSucc = psSrc->psIPostDom;
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;
	IMG_UINT32		uDeltaInst;
	PINST 			psNewTestInst = NULL;

	for (psListEntry = psSucc->sDeltaInstList.psHead, uDeltaInst = 0; 
		 psListEntry != NULL; 
		 psListEntry = psNextListEntry, uDeltaInst++)
	{
		PDELTA_PARAMS	psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		PINST			psDeltaInst = psDeltaParams->psInst;
		PDELTA_FLATTEN	psFlat = &asDeltaFlat[uDeltaInst];

		IMG_UINT32	uCondWriteArm = psFlat->uCondWriteArm;
		PARG		psPreviousTemp = IMG_NULL;


		psNextListEntry = psListEntry->psNext;

		if (psDeltaInst->uArgumentCount == 2 && 
			CompareArgs(&psDeltaInst->asArg[0], &psDeltaInst->asArg[1]) == 0)
		{
			InsertDeltaCopy(psState,
							psDeltaInst,
							auEdgeToSucc[1-uCondWriteArm],
							psSrc,
							uPredSrc,
							(1-uCondWriteArm == 0) ? IMG_FALSE : IMG_TRUE /* bPredNegate */);
		}
		else
		{
			InsertDeltaMOVC(psState,
							psDeltaInst,
							auEdgeToSucc[1-uCondWriteArm],
							psSrc,
							uPredSrc,
							(1-uCondWriteArm == 0) ? IMG_FALSE : IMG_TRUE /* bPredNegate */,
							&psPreviousTemp,
							psTestInst,
							&psNewTestInst);

			InsertDeltaMOVC(psState,
							psDeltaInst,
							auEdgeToSucc[uCondWriteArm],
							psSrc,
							uPredSrc,
							(uCondWriteArm == 0) ? IMG_FALSE : IMG_TRUE /* bPredNegate */,
							&psPreviousTemp,
							psTestInst,
							&psNewTestInst);

			ASSERT (psDeltaInst->asArg [auEdgeToSucc[1-uCondWriteArm]].uNumber != psPreviousTemp->uNumber);
		}
		DropPredecessorFromDeltaInstruction(psState, psDeltaInst, auEdgeToSucc[1-uCondWriteArm]);
	}
}


static
IMG_VOID FlattenDeltaInstructions(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psSrc,
								  IMG_UINT32			auEdgeToSucc[],
								  IMG_UINT32			uPredSrc,
								  PDELTA_FLATTEN		asDeltaFlat,
								  IMG_BOOL				abFlatten[],
								  IMG_UINT32			uArmToDrop)
/******************************************************************************
 FUNCTION		: FlattenDeltaInstructions

 DESCRIPTION	: Convert delta instructions in the successor of a conditional
				  block to predicated moves.

 PARAMETERS		: psState		- Current compilation context
				  psSrc			- Conditional block.
				  auEdgeToSucc	- Entry in the conditional's common successor's
								predecessor array which are from each arm of
								the conditional.
				  asDeltaFlat	- Information about how to flatten the
								delta instructions.
				  abFlatten		- For each arm of the conditional: whether it
								is to be flattened to predicates.
				  uArmToDrop	- If both arms of the conditional are to be
								flattened then the conditional's successor
								which be converted to an unconditional edge.

 RETURNS		: Nothing.
******************************************************************************/
{
	PCODEBLOCK		psSucc = psSrc->psIPostDom;
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;
	IMG_UINT32		uDeltaInst;

	for (psListEntry = psSucc->sDeltaInstList.psHead, uDeltaInst = 0; 
		 psListEntry != NULL; 
		 psListEntry = psNextListEntry, uDeltaInst++)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;
		PDELTA_FLATTEN	psFlat = &asDeltaFlat[uDeltaInst];

		psNextListEntry = psListEntry->psNext;
		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;

		if ((abFlatten[0] || psSrc->asSuccs[0].psDest == psSrc->psIPostDom) && 
			(abFlatten[1] || psSrc->asSuccs[1].psDest == psSrc->psIPostDom))
		{
			IMG_UINT32	uCondWriteArm = psFlat->uCondWriteArm;
			IMG_UINT32	uThisEdge = auEdgeToSucc[uCondWriteArm];
			IMG_UINT32	uOtherEdge = auEdgeToSucc[1 - uCondWriteArm];
			PARG		psOtherEdgeDeltaSrc = &psDeltaInst->asArg[uOtherEdge];
			PINST		psCondWriteInst;
			IMG_UINT32	uCondWriteDestIdx;
			IMG_UINT32	uArm;
			PARG		psCondWriteOldDest;

			for (uArm = 0; uArm < 2; uArm++)
			{
				if (psFlat->abInsertMoveBoth[uArm])
				{
					InsertDeltaCopy(psState,
									psDeltaInst,
									auEdgeToSucc[uArm],
									psSrc,
									uPredSrc,
									(uArm == 0) ? IMG_FALSE : IMG_TRUE /* bPredNegate */);
				}
			}

			psCondWriteInst = UseDefGetDefInst(psState, 
											   psDeltaInst->asArg[uThisEdge].uType,
											   psDeltaInst->asArg[uThisEdge].uNumber,
											   &uCondWriteDestIdx);

			ASSERT(psCondWriteInst != NULL);
			psCondWriteOldDest = psCondWriteInst->apsOldDest[uCondWriteDestIdx];
			if (psCondWriteOldDest == NULL)
			{
				SetPartiallyWrittenDest(psState, 
										psCondWriteInst, 
										uCondWriteDestIdx,  
										psOtherEdgeDeltaSrc);
			}
			else
			{
				ASSERT(EqualArgs(psCondWriteInst->apsOldDest[uCondWriteDestIdx], psOtherEdgeDeltaSrc));
			}

			if (uArmToDrop == uCondWriteArm)
			{
				MoveSrc(psState, psDeltaInst, uOtherEdge, psDeltaInst, uThisEdge);
			}
			DropPredecessorFromDeltaInstruction(psState, psDeltaInst, auEdgeToSucc[uArmToDrop]);
		}
		else
		{
			IMG_UINT32	uArm;

			for (uArm = 0; uArm < 2; uArm++)
			{
				if (abFlatten[uArm] && psFlat->abInsertMoveOne[uArm])
				{
					InsertDeltaCopy(psState,
									psDeltaInst,
									auEdgeToSucc[uArm],
									psSrc,
									uPredSrc,
									(uArm == 0) ? IMG_FALSE : IMG_TRUE /* bPredNegate */);
				}
			}
		}
	}
}

static
IMG_VOID SetupConditionalWrite(PINTERMEDIATE_STATE	psState, 
							   PCODEBLOCK			psDest, 
							   IMG_UINT32			uPredSrc, 
							   IMG_BOOL				bPredNegate, 
							   PINST				psInst,
							   IMG_UINT32			uDestIdx)
/*****************************************************************************
 FUNCTION	: SetupConditionalWrite

 PURPOSE    : 

 PARAMETERS	: psState		- Compiler intermediate state
			  psDest		- Destination for the instructions from the
							flattened block.
			  psSrc			- Block we are flattening.
			  uPredSrc		- Predicate to set on the instruction.
			  bPredNegate
			  psInst		- Instruction to modify.
			  uDestIdx		- Destination to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG			psDestArg = &psInst->asDest[uDestIdx];
	PARG			psOldDestArg = psInst->apsOldDest[uDestIdx];

	/*
		Skip unwritten destinations.
	*/
	if (psDestArg->uType == USC_REGTYPE_UNUSEDDEST)
	{
		return;
	}

	if (!(psDestArg->uType == USEASM_REGTYPE_TEMP || psDestArg->uType == USEASM_REGTYPE_PREDICATE))
	{
		if (psOldDestArg == NULL)
		{
			/*
				The destination isn't a register type in SSA form so copy the existing destination to the
				conditionally overwritten destination.
			*/
			SetPartiallyWrittenDest(psState, psInst, uDestIdx, psDestArg);
		}
		else if (!EqualArgs(psOldDestArg, psDestArg) && NoPredicate(psState, psInst))
		{
			IMG_UINT32		uMaskTemp;
			UF_REGFORMAT	eMaskTempFmt = psDestArg->eFmt;
			PINST			psMOVInst;

			/*
				The instruction already has a old destination. If this for a partial destination write mask
				then set a fresh temporary as the destination then (conditionally) copy from the fresh temporary
				to the original destination.
			*/

			uMaskTemp = GetNextRegister(psState);

			/*
					ARRAY[i].X = ....
					ARRAY[i].Y = OLDDEST.Y
				->
					TEMP.X = ...
					TEMP.Y = OLDDEST.Y
					(if PREDICATE) ARRAY[i] = TEMP else ARRAY[i] = ARRAY[i]
			*/
			psMOVInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psMOVInst, IMOV);
			psMOVInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDestIdx];
			MoveDest(psState, psMOVInst, 0 /* uDestIdx */, psInst, uDestIdx);
			SetPartiallyWrittenDest(psState, psMOVInst, 0 /* uDestIdx */, &psMOVInst->asDest[0]);
			SetSrc(psState, psMOVInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uMaskTemp, eMaskTempFmt);
			SetPredicate(psState, psMOVInst, uPredSrc, bPredNegate);
			AppendInst(psState, psDest, psMOVInst);

			SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_TEMP, uMaskTemp, eMaskTempFmt);
		}
		return;
	}
}

static
IMG_VOID MovePredicateInsts(PINTERMEDIATE_STATE psState,
							PCODEBLOCK psSrc,
							PCODEBLOCK psDest,
							IMG_UINT32 uPredSrc,
							IMG_BOOL bNegate)
/*****************************************************************************
 FUNCTION	: MovePredicateInsts

 PURPOSE    : Move instructions from a codeblock to another and predicate them

 PARAMETERS	: psState		- Compiler intermediate state
			  psSrc			- Block from which to remove instructions
			  psDest		- Block into which to put instructions (predicated)
			  uPredSrc		- Predicate source to apply to instructions moved
			  bNegate		- Negation flag

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst, psNextInst;

	/* Keep a "combined predicate": a predicate register that is a combination
	   of uPredSrc (with bNegate) and another predicate already found in the
	   basic block being predicated: uCombinedPredicateSource (with 
	   uCombinedPredicateNegate)
	*/
	IMG_BOOL bGotACombinedPredicate = IMG_FALSE;
	IMG_BOOL bCombinedPredicateNegate = IMG_FALSE;
	IMG_BOOL bCombinedPredicatePerChan = IMG_FALSE;
	IMG_UINT32 uCombinedPredicateSource = 0;
	IMG_UINT32 uCombinedPredicate = 0;
	IMG_BOOL bGotLastTestPredicate = IMG_FALSE;
	IMG_UINT32 uLastTestPredicate = 0;
	IMG_BOOL bLastTestPredicateConst = IMG_FALSE;

	/*
		Check for an empty conditional.
	*/
	if (psSrc == psDest->psIPostDom)
	{
		return;
	}

	ASSERT (!IsCall(psState, psSrc) && !IsCall(psState, psDest));

	/* 
	   Set predicate on all instructions.
	*/
	for (psInst = psSrc->psBody; psInst; psInst = psNextInst)
	{
		IMG_UINT32	uDestIdx;

		ASSERT(psInst->eOpcode != IDELTA);

		if ((psInst->eOpcode == ITESTPRED
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			|| psInst->eOpcode == IVTEST
#endif
			) 
			&& psInst->asDest [0].uType == USEASM_REGTYPE_PREDICATE
		    && IsMatchPred(psInst, uPredSrc, bNegate, IMG_FALSE))
		{
			/* This is probably going to be a predicate test operation where this predicate
			   register should be combined with the outer predicate. So, we pre-initialize the
			   predicate register to false to make it easier to combine.
			   */
			PUSEDEF_CHAIN psPredicateUseDef = UseDefGet (psState, USEASM_REGTYPE_PREDICATE, psInst->asDest [0].uNumber);
			if (psPredicateUseDef != IMG_NULL
				&& !UseDefIsReferencedOutsideBlock (psState, psPredicateUseDef, psSrc))
			{
				bGotLastTestPredicate = IMG_TRUE;
				uLastTestPredicate = psInst->asDest [0].uNumber;
				bLastTestPredicateConst = IMG_FALSE;

				SetPartialDest (psState, psInst, 0, USC_REGTYPE_BOOLEAN, bLastTestPredicateConst, UF_REGFORMAT_F32);
			}
		}


		/* Move into new block */
		psNextInst = psInst->psNext;
		RemoveInst(psState, psSrc, psInst);
		AppendInst(psState, psDest, psInst);

		if ((psState->ePredicationLevel == UF_PREDLVL_NESTED_SAMPLES) && !CanPredicate(psState, psInst))
		{
			/* 
				can't predicate a sample operation, so just leave the sample
				instruction unpredicated.
			*/
		}
		else if (IsMatchPred(psInst, uPredSrc, bNegate, IMG_FALSE))
		{
			/* it is trivial to attach the predicate to this instruction */
			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				if (psInst->asDest[uDestIdx].uType == USEASM_REGTYPE_FPINTERNAL)
				{
					/* can't make internal registers conditionally writable,
					   it messes up live value analysis
					   */
				}
				else
				{
					SetupConditionalWrite(psState, 
										  psDest, 
										  uPredSrc,
										  bNegate, 
										  psInst, 
										  uDestIdx);
				}
			}

			SetPredicate(psState, psInst, uPredSrc, bNegate);
		}
		else
		{
			IMG_UINT32	uOtherPredSrc;
			IMG_BOOL	bOtherPredNegate, bOtherPredPerChan;

			GetPredicate(psInst, &uOtherPredSrc, &bOtherPredNegate, &bOtherPredPerChan, 0);

			if (!bGotACombinedPredicate ||
				uOtherPredSrc != uCombinedPredicateSource ||
				bOtherPredNegate != bCombinedPredicateNegate ||
				bOtherPredPerChan != bCombinedPredicatePerChan)
			{
				if (bGotLastTestPredicate && uOtherPredSrc == uLastTestPredicate)
				{
					/* predicate is already initialized to constant value */
					bCombinedPredicateNegate = bOtherPredNegate;
					uCombinedPredicateSource = uOtherPredSrc;
					bGotACombinedPredicate = IMG_TRUE;
					if (bOtherPredNegate != bLastTestPredicateConst)
					{
						/* need to over-write inner predicate if outer conditional is false */
						PINST psCombinePredInst;

						bLastTestPredicateConst = bOtherPredNegate;

						uCombinedPredicate = GetNextPredicateRegister (psState);

						psCombinePredInst = AllocateInst (psState, IMG_NULL);
						SetOpcode (psState, psCombinePredInst, IMOVPRED);
						SetPartialDest (psState, psCombinePredInst, 0, USEASM_REGTYPE_PREDICATE, uOtherPredSrc, UF_REGFORMAT_F32);
						SetDest (psState, psCombinePredInst, 0, USEASM_REGTYPE_PREDICATE, uCombinedPredicate, UF_REGFORMAT_F32);
						SetSrc (psState, psCombinePredInst, 0, USC_REGTYPE_BOOLEAN, bOtherPredNegate, UF_REGFORMAT_F32);
						SetupConditionalWrite(psState, 
											  psDest, 
											  uPredSrc,
											  bNegate, 
											  psCombinePredInst, 
											  0);
						SetPredicate(psState, psCombinePredInst, uPredSrc, (!bNegate) ? IMG_TRUE : IMG_FALSE);
						InsertInstBefore(psState, psDest, psCombinePredInst, psInst);
					}
					else
					{
						/* predicate is already initialized to correct constant value */
						uCombinedPredicate = uOtherPredSrc;
					}
				}
				else
				{
					PINST psCombinePredInst;
					/* need to calculate a new combined predicate */
					bCombinedPredicateNegate = bOtherPredNegate;
					uCombinedPredicateSource = uOtherPredSrc;
					uCombinedPredicate = GetNextPredicateRegister (psState);
					bGotACombinedPredicate = IMG_TRUE;

					psCombinePredInst = AllocateInst (psState, IMG_NULL);
					SetOpcode (psState, psCombinePredInst, IMOVPRED);
					SetPartialDest (psState, psCombinePredInst, 0, USEASM_REGTYPE_PREDICATE, uOtherPredSrc, UF_REGFORMAT_F32);
					SetDest (psState, psCombinePredInst, 0, USEASM_REGTYPE_PREDICATE, uCombinedPredicate, UF_REGFORMAT_F32);
					SetSrc (psState, psCombinePredInst, 0, USC_REGTYPE_BOOLEAN, bOtherPredNegate, UF_REGFORMAT_F32);
					SetupConditionalWrite(psState, 
										  psDest, 
										  uPredSrc,
										  bNegate, 
										  psCombinePredInst, 
										  0);
					SetPredicate(psState, psCombinePredInst, uPredSrc, (!bNegate) ? IMG_TRUE : IMG_FALSE);
					InsertInstBefore(psState, psDest, psCombinePredInst, psInst);
				}
			}

			/* attach a combined predicate to this instruction */
			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				if (psInst->asDest[uDestIdx].uType == USEASM_REGTYPE_FPINTERNAL)
				{
					/* can't make internal registers conditionally writable,
					   it messes up live value analysis
					   */
				}
				else
				{
					SetupConditionalWrite(psState, 
										  psDest, 
										  uCombinedPredicate,
										  bCombinedPredicateNegate, 
										  psInst, 
										  uDestIdx);
				}
			}

			SetPredicate(psState, psInst, uCombinedPredicate, bCombinedPredicateNegate);

		}
	}
}

static
IMG_VOID MoveMOVCInsts(PINTERMEDIATE_STATE	psState,
					   PCODEBLOCK			psSrc,
					   PCODEBLOCK			psDest)
/*****************************************************************************
 FUNCTION	: MoveMOVCInsts

 PURPOSE    : Move instructions from a codeblock to another and make ready for MOVC

 PARAMETERS	: psState		- Compiler intermediate state
			  psSrc			- Block from which to remove instructions
			  psDest		- Block into which to put instructions (predicated)
			  uPredSrc		- Predicate source to apply to instructions moved
			  bNegate		- Negation flag

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst, psNextInst;

	/*
		Check for an empty conditional.
	*/
	if (psSrc == psDest->psIPostDom)
	{
		return;
	}

	ASSERT (!IsCall(psState, psSrc) && !IsCall(psState, psDest));

	/* 
	   Move all instructions.
	*/
	for (psInst = psSrc->psBody; psInst; psInst = psNextInst)
	{
		ASSERT(psInst->eOpcode != IDELTA);

		/* Move into new block */
		psNextInst = psInst->psNext;
		RemoveInst(psState, psSrc, psInst);
		AppendInst(psState, psDest, psInst);
	}
}

static
IMG_BOOL InstructionsCanBePredicated(PINTERMEDIATE_STATE	psState, 
									 PCODEBLOCK				psBlock, 
									 IMG_UINT32				uPredSrc, 
									 IMG_BOOL				bPredNegate)
/*****************************************************************************
 FUNCTION	: InstructionsCanBePredicated

 PURPOSE    : Test whether all instructions in a block could be predicated
			  with a given predicate.

 INPUT		: psState		- Internal compiler state
			  psBlock		- Block to check instructions of
			  uPredSrc		- Number of the predicate to use
			  bPredNegate	- Whether the predicate to use is negated

 RETURNS	: IMG_TRUE or IMG_FALSE
*****************************************************************************/
{
	PINST psInst;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uDestIdx;

		if (!CanPredicate(psState, psInst))
		{
			return IMG_FALSE;
		}
		if (!IsMatchPred(psInst, uPredSrc, bPredNegate, IMG_FALSE))
		{
			if (psState->ePredicationLevel < UF_PREDLVL_NESTED)
			{
				return IMG_FALSE;
			}
		}

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];

			if (psDest->uType == USEASM_REGTYPE_PREDICATE && psDest->uNumber == uPredSrc)
			{
				return IMG_FALSE;
			}
		}

		if (psInst->eOpcode == IDELTA)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CanFlatten(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBranch,
						   IMG_UINT32			uArm,
						   IMG_PUINT32			puNumInsts)
/*****************************************************************************
 FUNCTION	: CanFlatten

 PURPOSE    : Test whether it's worthwhile to flatten one arm of a conditional

 INPUT		: psState		- Internal compiler state
			  psBranch		- The conditional block.
			  uArm			- True/false successor (of conditional) to consider

 OUTPUT		: puNumInsts	- if TRUE, how many instructions would be flattened

 RETURNS	: IMG_TRUE if its worth flattening the branch, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uPredSrc;
	IMG_BOOL	bPredNegate;
	PCODEBLOCK	psPredicatedBlock;
	PCODEBLOCK	psArm = psBranch->asSuccs[uArm].psDest;

	/*
		Return early if the conditional contains no code.
	*/
	if (psArm == psBranch->psIPostDom)
	{
		if (puNumInsts) *puNumInsts = 0;
		return IMG_TRUE;
	}

	if (psArm->eType != CBTYPE_UNCOND || !UnconditionallyFollows(psState, psArm, psBranch->psIPostDom))
	{
		return IMG_FALSE; //nested flow control
	}
	if (psArm->uNumPreds != 1) 
	{
		return IMG_FALSE; //can't incorporate into psBranch!
	}
	ASSERT (psArm->asPreds[0].psDest == psBranch);
	ASSERT (psBranch->eType == CBTYPE_COND);
	
	ASSERT(psBranch->u.sCond.sPredSrc.uType == USEASM_REGTYPE_PREDICATE);
	uPredSrc = psBranch->u.sCond.sPredSrc.uNumber;
	if (psBranch->asSuccs[0].psDest == psArm)
	{
		bPredNegate = IMG_FALSE;
	}
	else
	{
		ASSERT (psBranch->asSuccs[1].psDest == psArm);
		bPredNegate = IMG_TRUE;
	}
	
	//At present, call blocks (ICALLs) are never predicated
	if (IsCall(psState, psArm)) 
	{
		return IMG_FALSE;
	}
	
	/* 
	   Test whether instructions support predication,
	   aren't already predicated,
	   don't clobber the predicate
	*/
	if (puNumInsts != NULL) 
	{
		*puNumInsts = 0;
	}

	psPredicatedBlock = psArm;
	while (psPredicatedBlock != psBranch->psIPostDom)
	{
		if (!InstructionsCanBePredicated(psState, psPredicatedBlock, uPredSrc, bPredNegate))
		{
			return IMG_FALSE;
		}
		
		/* Record the number of instructions */
		if (puNumInsts != NULL) 
		{
			*puNumInsts += psPredicatedBlock->uInstCount;
		}
		
		ASSERT(psPredicatedBlock->eType == CBTYPE_UNCOND);
		ASSERT(psPredicatedBlock->uNumSuccs == 1);

		psPredicatedBlock = psPredicatedBlock->asSuccs[0].psDest;
	}

	return IMG_TRUE;
}

static IMG_BOOL CanFlattenMOVC(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBranch,
						   IMG_UINT32			uArm)
/*****************************************************************************
 FUNCTION	: CanFlattenMOVC

 PURPOSE    : Test whether it's worthwhile use MOVC instructions instead of
			  a conditional. Assumes CanFlatten has already returned true

 INPUT		: psState		- Internal compiler state
			  psBranch		- The conditional block.
			  uArm			- True/false successor (of conditional) to consider

 OUTPUT		: puNumInsts	- if TRUE, how many instructions would be flattened

 RETURNS	: IMG_TRUE if its worth flattening the branch, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_INT32		iSaving;
	PINST			psInst;
	PCODEBLOCK		psPredicatedBlock;
	PCODEBLOCK		psArm = psBranch->asSuccs[uArm].psDest;
	PCODEBLOCK		psSucc = psBranch->psIPostDom;
	PUSC_LIST_ENTRY	psListEntry;

	/*
		Return early if the conditional contains no code.
	*/
	if (psArm == psBranch->psIPostDom)
	{
		return IMG_TRUE;
	}

	ASSERT (psArm->asPreds[0].psDest == psBranch);
	ASSERT (psBranch->eType == CBTYPE_COND);
	
	ASSERT(psBranch->u.sCond.sPredSrc.uType == USEASM_REGTYPE_PREDICATE);
	
	//At present, call blocks (ICALLs) are never predicated
	if (IsCall(psState, psArm)) 
	{
		return IMG_FALSE;
	}
	
	/* 
	    This is an estimate of the relative saving of doing a MOVC vs doing
		a predicated block. iSaving is the number of instructions saved by
		doing MOVC instead of a predicated block
	*/
	if (psState->ePredicationLevel == UF_PREDLVL_LARGE_MOVC)
	{
		/* 
			Allow large blocks for MOVC instructions. This is altering the
			heuristics by using a compiler option.
		*/
		iSaving = 6; 
	}
	else
	{
		iSaving = 1; // save 1 instruction converting ITEST to IMOVC
	}

	psPredicatedBlock = psArm;
	while (psPredicatedBlock != psBranch->psIPostDom)
	{
		for (psInst = psPredicatedBlock->psBody; psInst != NULL; psInst = psInst->psNext)
		{
			IMG_UINT32	uDestIdx;

			if (psInst->eOpcode == IDELTA)
			{
				return IMG_FALSE;
			}

			if (psInst->eOpcode == IMOV
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				&& psInst->eOpcode == IVMOV			
#endif
				)
			{
				/* save 1 instruction for every MOV inside a conditional block if doing MOVC */
				iSaving++;
			}

			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				PARG	psDest = &psInst->asDest[uDestIdx];
				/* can't conditional-move to a predicate */

				if (psDest->uType == USEASM_REGTYPE_PREDICATE)
				{
					return IMG_FALSE;
				}
			}
		}

		ASSERT(psPredicatedBlock->eType == CBTYPE_UNCOND);
		ASSERT(psPredicatedBlock->uNumSuccs == 1);

		psPredicatedBlock = psPredicatedBlock->asSuccs[0].psDest;
	}

	/*
		For each delta instruction in the successor...
	*/
	for (psListEntry = psSucc->sDeltaInstList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;
		IMG_UINT32		uSrc;
		IMG_BOOL		bCanBeMADD = IMG_FALSE;

		/*
			If we convert a conditional into a MOVC, then we need a MOVC for each delta instruction
		*/
		--iSaving;

		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;

		if (psDeltaInst->asDest[0].uType == USEASM_REGTYPE_PREDICATE)
		{
			/* can't MOVC to a predicate */
			return IMG_FALSE;
		}

		/*
			If an argument of the delta is not defined in the relevant block then it is a constant
			and therefore a MOV would be inserted if the block was predicated, but the MOV is not
			needed if we do a MOVC.
		*/
		for (uSrc = 0; uSrc < psDeltaInst->uArgumentCount; ++uSrc)
		{
			PINST psDefInst = 
				UseDefGetDefInst(psState, psDeltaInst->asArg[uSrc].uType, psDeltaInst->asArg[uSrc].uNumber, NULL /* puDestIdx */);

			if (psDefInst == NULL || 
				(
					!UnconditionallyFollows(psState, psBranch->asSuccs[0].psDest, psDefInst->psBlock) &&
					!UnconditionallyFollows(psState, psBranch->asSuccs[1].psDest, psDefInst->psBlock) &&
					(psDefInst->psBlock != psBranch)
				)
				)
			{
				/*
					An argument to the delta isn't in the conditional block, and so a MOV is being
					removed when doing a MOVC operation
				*/
				++iSaving;
				bCanBeMADD = IMG_FALSE;
			}
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			else if (psDefInst->eOpcode == IVADD &&
				     (
						(psDefInst->psBlock == psBranch->asSuccs[0].psDest) ||
						(psDefInst->psBlock == psBranch->asSuccs[1].psDest)
					 )
					)
			{
				/*
					This is a conditional ADD instruction which can be turned into a TESTMASK
					and an ADD, or MADD. This is treated as a MOVC for the purposes of this
					function.
				*/
				bCanBeMADD = IMG_TRUE;
			}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		}

		if (bCanBeMADD)
		{
			++iSaving;
		}
	}

	if (iSaving < 0)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

typedef enum {FLATTEN_NOCHANGE, FLATTEN_NOCONTROLCHANGE, FLATTEN_CONTROLCHANGED} FLATTEN_RESULT;

static FLATTEN_RESULT FlattenConditional(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uALUMax)
/******************************************************************************
 FUNCTION		: FlattenConditional

 DESCRIPTION	: Flatten a conditional block and its successor(s), or not, as
				  is worthwhile.

 PARAMETERS		: psState	- Current compilation context
				  psBlock	- Conditional block to flatten
				  uALUMax	- Max number (ALU) instructions to flatten, per arm

 RETURNS		: TRUE if the conditional was (at least partly) flattened
******************************************************************************/
{
	IMG_BOOL		abCanFlatten[2];
	IMG_BOOL		abCanMOVC[2];
	IMG_UINT32		auNumInsts[2];
	IMG_BOOL		abFlatten[2];
	PDELTA_FLATTEN	asDeltaFlat;
	IMG_UINT32		auEdgeToSucc[2];
	IMG_UINT32		auNumDeltaInstsOneArm[2];
	IMG_UINT32		auNumDeltaInstsBothArms[2];
	IMG_UINT32		uArm;
	PINST			psTestInst = IMG_NULL;
	IMG_BOOL		bControlChanged = IMG_FALSE;

	/*
		Check for cases where delta functions in the post-dominator of the conditional
		could be converted to copies of the source predicate to the conditional.
	*/
	ConvertDeltaInstructionsToPredCopy(psState, psBlock);

	for (uArm = 0; uArm < 2; uArm++)
	{
		/*
			Get the edge by which this arm of the conditional arrives at the
			post-dominator of the conditional.
		*/
		auEdgeToSucc[uArm] = GetConditionalEdge(psBlock, uArm);

		/*
			Check if it's possible to flatten this arm of the conditional and the
			number of instructions in the conditional arm.
		*/
		abCanFlatten[uArm] = CanFlatten(psState, psBlock, uArm, &auNumInsts[uArm]);

		/*
			Check if it's possible to, instead of predicating all the instructions
			in this arm of the conditional, converting the DELTA instruction after
			the conditional into a conditional move
		*/
		abCanMOVC[uArm] = (abCanFlatten[uArm] && CanFlattenMOVC(psState, psBlock, uArm)) ? IMG_TRUE : IMG_FALSE;

		/* 
			Check if one of the branches is a loop branch. If so, do nothing
		*/
		if (Dominates (psState, psBlock->asSuccs [uArm].psDest, psBlock))
		{
			return FLATTEN_NOCHANGE;
		}
	}

	//can we do anything useful?
	if (!abCanFlatten[0] && !abCanFlatten[1]) 
	{
		return FLATTEN_NOCHANGE;
	}

	/*
		Check what extra instructions we would need to insert to remove delta instructions
		from the successor of the conditional arms.
	*/
	CanFlattenDeltaInstructions(psState,
								psBlock,
								auEdgeToSucc,
								auNumDeltaInstsOneArm,
								auNumDeltaInstsBothArms,
								&asDeltaFlat);

	/*
		Check if the number of predicated instructions is small enough to make it worthwhile to
		flatten the conditionals.
	*/
	if (abCanMOVC[0] && abCanMOVC[1])
	{
		IMG_UINT32	uDestIdx;
		USEDEF_TYPE sUseDefType;
		IMG_UINT32	uUseSrcIdx;
		IMG_PVOID	pvPredUse;

		psTestInst = UseDefGetDefInst(psState,
										USEASM_REGTYPE_PREDICATE,
										psBlock->u.sCond.sPredSrc.uNumber,
										&uDestIdx);
		/* check that there is a single test instruction that
		   can be converted into a MOVC
		*/
		if(psTestInst == IMG_NULL
			|| psState->ePredicationLevel < UF_PREDLVL_LARGE_BLOCKS_MOVC
			|| (psTestInst->eOpcode != ITESTPRED
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			    && psTestInst->eOpcode != IVTEST
#endif
				)
			|| uDestIdx != 0
			|| (psTestInst->uDestCount == 2 && psTestInst->asDest[1].uType != USEASM_REGTYPE_PREDICATE)
			|| psTestInst->uPredCount != 0
			|| psTestInst->apsOldDest [0] != IMG_NULL
			|| !UseDefGetSingleRegisterUse(psState, 
									psBlock->u.sCond.sPredSrc.psRegister, 
									&pvPredUse,
									&sUseDefType,
									&uUseSrcIdx)
			|| sUseDefType != USE_TYPE_COND
			|| pvPredUse != psBlock
			|| (psTestInst->eOpcode == ITESTPRED
			      && psTestInst->u.psTest->eAluOpcode != IFSUB
			      && psTestInst->u.psTest->eAluOpcode != IFADD
			      && psTestInst->u.psTest->eAluOpcode != IFMOV)
									)
		{
			psTestInst = IMG_NULL;
			abCanMOVC[0] = abCanMOVC[1] = IMG_FALSE;
		}
		else
		{
			abFlatten[0] = abFlatten[1] = IMG_TRUE;
		}
	}	
	if ((abCanMOVC[0] && abCanMOVC[1]) ||
		   (abCanFlatten[0] && 
			abCanFlatten[1] && 
			(auNumInsts[0] + auNumDeltaInstsBothArms[0]) <= uALUMax &&
			(auNumInsts[1] + auNumDeltaInstsBothArms[1]) <= uALUMax)
		)
	{
		abFlatten[0] = abFlatten[1] = IMG_TRUE;
	}
	else
	{
		abFlatten[0] = abFlatten[1] = IMG_FALSE;
		abCanMOVC[0] = abCanMOVC[1] = IMG_FALSE;
		for (uArm = 0; uArm < 2; uArm++)
		{	
			if (abCanFlatten[uArm] && (auNumInsts[uArm] + auNumDeltaInstsOneArm[uArm]) <= uALUMax)
			{
				abFlatten[uArm] = IMG_TRUE;
				break;
			}
		}
	}
	
	if (abCanMOVC[0] && abCanMOVC[1])
	{
		PCODEBLOCK psPostDomBlock = psBlock->psIPostDom;

		ASSERT(abFlatten[0] && abFlatten[1]);

		/*
			Move instructions from each conditional arm into the block
		*/
		for (uArm = 0; uArm < 2; uArm++)
		{
			if (abFlatten[uArm])
			{
				PCODEBLOCK psFlattenBlock = psBlock->asSuccs[uArm].psDest;

				while (psFlattenBlock != psPostDomBlock)
				{
					MoveMOVCInsts(psState, psFlattenBlock, psBlock);
					
					ASSERT(psFlattenBlock->eType == CBTYPE_UNCOND);
					ASSERT(psFlattenBlock->uNumSuccs == 1);

					psFlattenBlock = psFlattenBlock->asSuccs[0].psDest;
				}
			}
		}

		/*
			Convert delta instructions into conditional moves
		*/
		FlattenMOVCDeltaInstructions(psState,
									 psBlock,
									 auEdgeToSucc,
									 psBlock->u.sCond.sPredSrc.uNumber,
									 asDeltaFlat,
									 psTestInst);

	}
	else if (abFlatten[0] || abFlatten[1])
	{
		PCODEBLOCK psPostDomBlock = psBlock->psIPostDom;

		/*
			Move instructions from each conditional arm into the block and set them as predicated.
		*/
		for (uArm = 0; uArm < 2; uArm++)
		{
			if (abFlatten[uArm])
			{
				PCODEBLOCK psFlattenBlock = psBlock->asSuccs[uArm].psDest;

				while (psFlattenBlock != psPostDomBlock)
				{
					MovePredicateInsts(psState, 
									   psFlattenBlock, 
									   psBlock,
									   psBlock->u.sCond.sPredSrc.uNumber, 
									   (uArm == 1) /* bNegate */);

					ASSERT(psFlattenBlock->eType == CBTYPE_UNCOND);
					ASSERT(psFlattenBlock->uNumSuccs == 1);
					
					psFlattenBlock = psFlattenBlock->asSuccs[0].psDest;
				}
			}
		}

		/*
			Convert delta instructions into the conditional arm's successor to predicated moves.
		*/
		FlattenDeltaInstructions(psState,
								 psBlock,
								 auEdgeToSucc,
								 psBlock->u.sCond.sPredSrc.uNumber,
								 asDeltaFlat,
								 abFlatten,
								 0 /* uArmToDrop */);
	}

	if (abFlatten[0] || abFlatten[1])
	{
		/*
			Replace the empty flow control block in flattened conditional arms by a direct edge
			to the post-dominator of the conditional.
		*/
		for (uArm = 0; uArm < 2; uArm++)
		{
			if (abFlatten[uArm] 
				&& psBlock->asSuccs[uArm].psDest->eType == CBTYPE_UNCOND 
				&& UnconditionallyFollows(psState, psBlock->asSuccs[uArm].psDest->asSuccs[0].psDest, psBlock->psIPostDom))
			{
				bControlChanged = IMG_TRUE;
				ReplaceEdge(psState, psBlock, uArm, psBlock->psIPostDom);
			}
		}

		/*
			If both arms of the conditional have been flattened then make the current block unconditional.
		*/
		if (psBlock->asSuccs[0].psDest == psBlock->asSuccs[1].psDest)
		{
			bControlChanged = IMG_TRUE;
			MergeIdenticalSuccessors(psState, psBlock, 1 /* uSuccToRetain */);
		}
	}

	if (asDeltaFlat != NULL)
	{
		UscFree(psState, asDeltaFlat);
	}

	if (bControlChanged) 
	{
		return FLATTEN_CONTROLCHANGED;
	}
	else if (abFlatten[0] || abFlatten[1])
	{
		return FLATTEN_NOCONTROLCHANGE;
	}
	else
	{
		return FLATTEN_NOCHANGE;
	}
}

static FLATTEN_RESULT FlattenConditionalsDomTree(PINTERMEDIATE_STATE psState,
										   PCODEBLOCK psTopBlock,
										   IMG_UINT32 uALUMax,
										   UF_PREDICATIONLEVEL ePredicationLevel,
										   PFUNC psFunc)
/******************************************************************************
 FUNCTION		: FlattenConditionalsDomTree

 DESCRIPTION	: Traverse the dominator tree, flattening conditionals as
				  appropriate. (We do this on the dominator tree to ensure we
				  flatten any inner/nested conditionals BEFORE outer ones.)

 PARAMETERS		: psState	 - Current compilation context
				  psTopBlock - Block to traverse, and then consider flattening
				  uALUMax	  - Max number (ALU) instructions to flatten, per arm

 RETURNS		: whether this block, or any dominated block, was flattened
******************************************************************************/
{
	/**
	   Recursive implementation.

	   To be removed when usc2 is stable
	 **/
	IMG_UINT32 uChild;
	IMG_UINT32 uNumChildren = psTopBlock->uNumDomChildren;
	FLATTEN_RESULT eFlattenResult = FLATTEN_NOCHANGE;

	//recurse first
	for (uChild = 0; uChild < uNumChildren; uChild++)
	{
		eFlattenResult = FlattenConditionalsDomTree(psState, psTopBlock->apsDomChildren[uChild], uALUMax, ePredicationLevel, psFunc);
		if (eFlattenResult == FLATTEN_CONTROLCHANGED)
		{
			return FLATTEN_CONTROLCHANGED;
		}
	}

	//now consider this node.
	if (psTopBlock->eType == CBTYPE_COND)
	{
		eFlattenResult = FlattenConditional(psState, psTopBlock, uALUMax);
	}
	return eFlattenResult;
	/**
	   End of recursive implementation
	 **/

#if 0 // DO NOT REMOVE
	/**
	   Iterative implementation

	   DO NOT REMOVE

	   Disabled until usc2 is stable.
	 **/
	IMG_BOOL bRet = IMG_FALSE;
	PUSC_PAIR psPair = NULL;
	PUSC_STACK psStack = StackMake(psState, sizeof(USC_PAIR));
	
	if (psTopBlock == NULL)
		return bRet;

	/* Initialise the block stack */
	psPair = StackPush(psState, psStack, NULL);
	ASSERT(psPair != NULL);
	psPair->pvLeft = (IMG_PVOID)psTopBlock;
	psPair->pvRight = (IMG_PVOID)IMG_FALSE;
	
	while(!StackEmpty(psStack))
	{
		PCODEBLOCK psBlock;
		IMG_BOOL bSeen;
		IMG_UINT32 uNumChildren = 0;
		
		psPair = StackTop(psStack);
		ASSERT(psPair != NULL);
		psBlock = (PCODEBLOCK)psPair->pvLeft;
		bSeen = (IMG_BOOL)psPair->pvRight;
		ASSERT(psBlock != NULL);

		StackPop(psState, psStack);

		uNumChildren = psBlock->uNumDomChildren;
		if ((!bSeen) && (uNumChildren > 0))
		{
			IMG_UINT32 uIdx;

			/* Mark parent as seen */
			psPair = StackPush(psState, psStack, NULL);
			ASSERT(psPair != NULL);
			psPair->pvLeft = (IMG_PVOID)psBlock;
			psPair->pvRight = (IMG_PVOID)IMG_TRUE;

			/*
			  If the block has children, process them first, in the
			  order that they appear in the block.
			*/
			for (uIdx = uNumChildren; uIdx > 0; --uIdx)
			{
				const IMG_UINT32 uChild = uNumChildren - uIdx;
				PCODEBLOCK psChild = psBlock->apsDomChildren[uChild];

				psPair = StackPush(psState, psStack, NULL);
				ASSERT(psPair != NULL);
				psPair->pvLeft = (IMG_PVOID)psChild;
				psPair->pvRight = (IMG_PVOID)IMG_FALSE;
			}

			/* Now process the children before the parent */
			continue;
		}

		/* Process this block */
		if (psBlock->eType == CBTYPE_COND &&
			FlattenConditional(psState, psBlock, uALUMax))
		{
			bRet = IMG_TRUE;
		}
	}

	/* Release the stack */
	ASSERT(StackEmpty(psStack));
	StackDelete(psState, psStack);

	/* Done */
	return bRet;
	/**
	   End of iterative implementation
	 **/
#endif
}

static
IMG_VOID RemovePredicatesBP(PINTERMEDIATE_STATE	psState,
							PCODEBLOCK			psBlock,
							IMG_PVOID			pvNULL)
/*****************************************************************************
 FUNCTION	: RemovePredicatesBP

 PURPOSE    : Remove predicates from instructions in a given block, where possible.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;
	
	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->uPredCount != 0)
		{
			IMG_UINT32  uDest;
			IMG_BOOL	bAllUsesPredicated = IMG_TRUE;

			for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
			{
				PUSC_LIST_ENTRY	psUseListEntry;
				PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, psInst->asDest[uDest].uType, psInst->asDest[uDest].uNumber);

				if (psUseDef == NULL)
				{
					bAllUsesPredicated = IMG_FALSE;
					break;
				}

				for (psUseListEntry = psUseDef->sList.psHead; 
					psUseListEntry != NULL;
					psUseListEntry = psUseListEntry->psNext)
				{
					PUSEDEF	psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
					PINST	psUseInst;

					if (psUse == psUseDef->psDef)
					{
						continue;
					}
					
					psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
					psUseInst = UseDefGetInst(psUse);

					if (psUseInst == NULL)
					{
						bAllUsesPredicated = IMG_FALSE;
						break;
					}

					if (psUse->eType == USE_TYPE_OLDDEST)
					{
						/* 
							In the situation

							p MOV r2[=r3] r4
							p MOV r5[=r2] r6

							it would not be right to remove the predicate on the first instruction.
						*/
						bAllUsesPredicated = IMG_FALSE;
						break;
					}

					if (!EqualPredicates(psUseInst, psInst))
					{
						bAllUsesPredicated = IMG_FALSE;
						break;
					}
				}

				if (!bAllUsesPredicated)
				{
					break;
				}
			}

			if (bAllUsesPredicated)
			{
				ClearPredicates(psState, psInst);

				for (uDest = 0; uDest < psInst->uDestCount; ++uDest)
				{
					if (psInst->auDestMask[uDest] == USC_ALL_CHAN_MASK)
					{
						SetPartiallyWrittenDest(psState, psInst, uDest, NULL);
					}
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID RemovePredicates(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: RemovePredicates

 PURPOSE    : Remove predicates from instructions, where possible.

 PARAMETERS	: psState - Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply the optimisation to all basic blocks.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, RemovePredicatesBP, IMG_FALSE /* CALLS */, NULL /* pvUserData */);

	/*	
		Show the updated intermediate code.
	*/
	TESTONLY_PRINT_INTERMEDIATE("Remove Predicates", "remove_preds", IMG_FALSE);
}

IMG_INTERNAL
IMG_BOOL FlattenProgramConditionals(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FlattenProgramConditionals

 PURPOSE    : (Consider) flatten(ing) any/all conditionals in the program

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Whether anything was changed.
*****************************************************************************/
{
	PFUNC			psFunc, psFuncNext;
	IMG_BOOL		bChanged;
	FLATTEN_RESULT	eFlattenResult;

	bChanged = IMG_FALSE;

	if(psState->ePredicationLevel == UF_PREDLVL_AUTO)
	{
		/* In future, we should put heuristics in here based on analysis of code
		   and measured results from real hardware. Until then, this is the default
		   */
#if 1
		psState->ePredicationLevel = UF_PREDLVL_LARGE_BLOCKS_MOVC;
#else
		psState->ePredicationLevel = UF_PREDLVL_SINGLE_INST;
#endif
		#if defined (EXECPRED)
		if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS)  && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
		{
			psState->uMaxConditionalALUInstsToFlatten = USC_MAXALUINSTSTOFLATTEN_EXECPRED;
		}
		else
		#endif /* #if defined (EXECPRED) */
		{
			psState->uMaxConditionalALUInstsToFlatten = USC_MAXALUINSTSTOFLATTEN_DEFAULT;			
		}
	}
	
	/* Clamp this mode to a single instruction - as per description */
	if (psState->ePredicationLevel ==  UF_PREDLVL_SINGLE_INST)
	{
		psState->uMaxConditionalALUInstsToFlatten = 1;
	}

	MergeAllBasicBlocks(psState);

	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFuncNext)
	{	
		psFuncNext = psFunc->psFnNestOuter;

		eFlattenResult = FlattenConditionalsDomTree(psState, psFunc->sCfg.psEntry, psState->uMaxConditionalALUInstsToFlatten, psState->ePredicationLevel, psFunc);

		if (eFlattenResult != FLATTEN_NOCHANGE)
		{
			bChanged = IMG_TRUE;
			if (eFlattenResult == FLATTEN_CONTROLCHANGED)
			{
				MergeBasicBlocks(psState, psFunc);
				/* Start again. This function may have been deleted in MergeBasicBlocks. */
				psFuncNext = psState->psFnInnermost;
			}
		}
	}
	
	return bChanged;
}

static
IMG_VOID OptimizePredicateCombineInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: OptimizePredicateCombineInst

 PURPOSE    : Optimise an instruction doing a logical combine on two predicates.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to optimize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psOldDest;
	PARG		psSrc;
	PARG		psPredSrc;
	PARG		apsSrc[2];
	IMG_BOOL	abSrcNegate[2];
	IMG_UINT32	uSrc;
	IMG_BOOL	abUsedOnce[2];
	PINST		apsDefInst[2];
	IMG_UINT32	auDefDestIdx[2];
	IMG_UINT32	uFirst;
	IMG_UINT32	uLast;
	PINST		psLastInst;
	IMG_UINT32	uLastDestIdx;
	enum
	{
		COMBOP_AND,
		COMBOP_OR,
	} eCombOp;

	/*
		Check for a conditional move choosing between two predicate moves.
	*/
	if (NoPredicate(psState, psInst))
	{
		return;
	}
	psPredSrc = psInst->apsPredSrc[0];

	ASSERT(psInst->uDestCount == 1);
	if (psInst->apsOldDest[0] == NULL)
	{
		return;
	}
	psOldDest = psInst->apsOldDest[0];
	psSrc = &psInst->asArg[0];

	/*
		Set the first input to the and/or operation.
	*/
	apsSrc[0] = psPredSrc;
	abSrcNegate[0] = GetBit(psInst->auFlag, INST_PRED_NEG) ? IMG_TRUE : IMG_FALSE;

	/*
		Check one of the conditional sources is a constant.
	*/
	if (psOldDest->uType == USC_REGTYPE_BOOLEAN)
	{	
		if (psSrc->uType != USEASM_REGTYPE_PREDICATE)
		{
			return;
		}

		apsSrc[1] = psSrc;
		abSrcNegate[1] = psInst->u.psMovp->bNegate;

		/*
			IF a THEN b ELSE true		-> (!A) or B
			IF a THEN b ELSE false		-> A and B
		*/
		if (psOldDest->uNumber)
		{
			eCombOp = COMBOP_OR;
			abSrcNegate[0] = !abSrcNegate[0];
		}
		else
		{
			eCombOp = COMBOP_AND;
		}
	}
	else if (psSrc->uType == USC_REGTYPE_BOOLEAN)
	{
		if (psOldDest->uType != USEASM_REGTYPE_PREDICATE)
		{
			return;
		}

		apsSrc[1] = psOldDest;
		abSrcNegate[1] = IMG_FALSE;

		/*
			IF a THEN true ELSE b		-> A or B
			IF a THEN false ELSE b		-> (!A) and B
		*/
		if (psSrc->uNumber)
		{
			eCombOp = COMBOP_OR;
		}
		else
		{
			eCombOp = COMBOP_AND;
			abSrcNegate[0] = !abSrcNegate[0];
		}
	}
	else
	{
		return;
	}

	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		IMG_PVOID	pvUse;
		USEDEF_TYPE	eUseType;
		IMG_UINT32	uUseIdx;

		/*
			Check if the combine operation source is only used here.
		*/
		abUsedOnce[uSrc] = UseDefGetSingleRegisterUse(psState, apsSrc[uSrc]->psRegister, &pvUse, &eUseType, &uUseIdx);
	
		/*
			Get the instruction defining the source.
		*/
		apsDefInst[uSrc] = UseDefGetDefInstFromChain(apsSrc[uSrc]->psRegister->psUseDefChain, &auDefDestIdx[uSrc]);
		if (apsDefInst[uSrc] == NULL)
		{
			return;
		}

		/*	
			Check if we can apply a negate modifier by changing the instruction defining the source.
		*/
		if (abSrcNegate[uSrc])
		{
			if (!NegateAtDefiningTest(psState, apsSrc[uSrc]->psRegister->psUseDefChain, psInst, IMG_TRUE /* bCheckOnly */))
			{
				return;
			}
		}
	}

	/*
		Check that the two instructions defining each of the sources are executed in
		a defined order.
	*/
	if (apsDefInst[0]->psBlock == apsDefInst[1]->psBlock)
	{
		if (apsDefInst[0]->uBlockIndex == apsDefInst[1]->uBlockIndex)
		{
			return;
		}
		else if (apsDefInst[0]->uBlockIndex < apsDefInst[1]->uBlockIndex)
		{
			uFirst = 0;
		}
		else
		{
			uFirst = 1;
		}
	}
	else
	{
		if (PostDominated(psState, apsDefInst[0]->psBlock, apsDefInst[1]->psBlock))
		{
			uFirst = 0;
		}
		else if (PostDominated(psState, apsDefInst[1]->psBlock, apsDefInst[0]->psBlock))
		{
			uFirst = 1;
		}
		else
		{
			return;
		}
	}

	/*
		Get the source defined by the last instruction to be executed.
	*/
	uLast = 1 - uFirst;

	/*
		Check the last source is used only in the combine. Then we can move the define of
		the combine result to the last instruction.
	*/
	if (!abUsedOnce[uLast])
	{
		return;
	}

	psLastInst = apsDefInst[uLast];
	uLastDestIdx = auDefDestIdx[uLast];

	/*
		Check the last instruction is unpredicated.
	*/
	if (!NoPredicate(psState, psLastInst))
	{
		return;
	}
	ASSERT(psLastInst->auDestMask[uLastDestIdx] == USC_ALL_CHAN_MASK);

	/*
		Update instructions so the sources to the combine now contain the negation
		of their original values.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		if (abSrcNegate[uSrc])
		{
			IMG_BOOL	bRet;

			bRet = 
				NegateAtDefiningTest(psState, apsSrc[uSrc]->psRegister->psUseDefChain, psInst, IMG_FALSE /* bCheckOnly */);
			ASSERT(bRet);
		}
	}

	/*
		Convert

			 TEST		A, ...
			 TEST		B, ...
			 AND		C, A, B						 OR		C, A, B
		->
			 TEST		A, ....
		(A)  TEST		B[=A], ....				(!A) TEST	B[=A], ...
	*/
	MoveDest(psState, psLastInst, uLastDestIdx, psInst, 0 /* uMoveFromDestIdx */);

	SetPredicate(psState, psLastInst, apsSrc[uFirst]->uNumber, eCombOp == COMBOP_AND ? IMG_FALSE : IMG_TRUE);
	SetPartiallyWrittenDest(psState, psLastInst, uLastDestIdx, apsSrc[uFirst]);

	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID OptimizePredicateCombines(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: OptimizePredicateCombineInst

 PURPOSE    : Optimise an instructions in the program doing logical combines of two predicates.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ForAllInstructionsOfType(psState, IMOVPRED, OptimizePredicateCombineInst);

	TESTONLY_PRINT_INTERMEDIATE("Optimise Predicate Combines", "pred_comb_opt", IMG_FALSE);
}

#if defined(OUTPUT_USPBIN)
/*****************************************************************************
 FUNCTION	: GetUSPDestFmtChannelOrder

 PURPOSE    : Get an array representing the order of channels in a USP sample
			  destination format.

 PARAMETERS	: psState	- Compiler State
			  eFormat	- Register format.

 RETURNS	: The channel order.
*****************************************************************************/
static IMG_UINT32 const* GetUSPDestFmtChannelOrder(PINTERMEDIATE_STATE	psState,
												   UF_REGFORMAT			eFormat)
{
	static const IMG_UINT32 auRGBAChanOrder[] = {0, 1, 2, 3};
	static const IMG_UINT32 auBGRAChanOrder[] = {2, 1, 0, 3};

	PVR_UNREFERENCED_PARAMETER(psState);

	if (eFormat == UF_REGFORMAT_F16 || eFormat == UF_REGFORMAT_F32)
	{
		return auRGBAChanOrder;
	}
	else
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA)
		{
			return auRGBAChanOrder;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			return auBGRAChanOrder;
		}
	}
}

/*****************************************************************************
 FUNCTION	: ComponentToChannel

 PURPOSE    : Maps a component of a vector in a given format to an index into a
			  vector of registers and a byte offset in the register.

 PARAMETERS	: psState		- Compiler State
			  uComponent	- Component.
			  puDestIdx		- Returns the register index.
			  puChannel		- Returns the byte offset.
			  eFormat		- Register format.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID ComponentToChannel(PINTERMEDIATE_STATE	psState,
								   IMG_UINT32			uComponent,
								   IMG_PUINT32			puDestIdx,
								   IMG_PUINT32			puChannel,
								   UF_REGFORMAT			eFormat)
{
	switch (eFormat)
	{
		case UF_REGFORMAT_F32:
		{
			*puDestIdx = uComponent;
			*puChannel = 0;
			break;
		}
		case UF_REGFORMAT_F16:
		{
			*puDestIdx = uComponent >> 1;
			*puChannel = (uComponent & 1) << 1;
			break;
		}
		case UF_REGFORMAT_U8:
		case UF_REGFORMAT_C10:
		{
			*puDestIdx = 0;
			*puChannel = uComponent;
			break;
		}
		default: imgabort();
	}
}

/*****************************************************************************
 FUNCTION	: ChannelMaskToComponentMask

 PURPOSE    : Maps a mask of bytes used from an array of registers in a given
			  format to a mask of components

 PARAMETERS	: psState		- Compiler State
			  uDestIdx		- Register offset.
			  uChanMask		- Mask of bytes from the register.
			  eFormat		- Register format.

 RETURNS	: The mask of components.
*****************************************************************************/
static IMG_UINT32 ChannelMaskToComponentMask(PINTERMEDIATE_STATE psState,
											 IMG_UINT32 uDestIdx, 
											 IMG_UINT32 uChanMask, 
											 UF_REGFORMAT eFormat)
{
	switch (eFormat)
	{
		case UF_REGFORMAT_F32:
		{
			ASSERT(uDestIdx < CHANS_PER_REGISTER);
			return (1U << uDestIdx);
		}
		case UF_REGFORMAT_F16:
		{
			IMG_UINT32	uCompMask;

			uCompMask = 0;
			if (uChanMask & USC_XY_CHAN_MASK)
			{
				uCompMask |= USC_X_CHAN_MASK;
			}
			if (uChanMask & USC_ZW_CHAN_MASK)
			{
				uCompMask |= USC_Y_CHAN_MASK;
			}

			ASSERT((uDestIdx << 1) < CHANS_PER_REGISTER);
			return uCompMask << (uDestIdx << 1);
		}
		case UF_REGFORMAT_U8:
		case UF_REGFORMAT_C10:
		{
			ASSERT(uDestIdx == 0);
			return uChanMask;
		}
		default: imgabort();
	}
}

static IMG_VOID AppendToMoveList(PUSC_LIST psMoveList, PINST psInst)
/*****************************************************************************
 FUNCTION	: AppendToMoveList

 PURPOSE    : Updates a PCK instruction when the format of the source data
			  changes.

 PARAMETERS	: psMoveList		- List of move instructions.
			  psInst			- Instruction to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_MODIFIEDINST) == 0)
	{
		AppendToList(psMoveList, &psInst->sAvailableListEntry);
		SetBit(psInst->auFlag, INST_MODIFIEDINST, 1);
	}
}

/*****************************************************************************
 FUNCTION	: SetPackSourceFormat

 PURPOSE    : Updates a PCK instruction when the format of the source data
			  changes.

 PARAMETERS	: psState		- Compiler State
			  psInst		- Instruction to change.
			  eNewFormat	- New source format.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetPackSourceFormat(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									UF_REGFORMAT		eNewFormat,
									PUSC_LIST			psMoveList)
{
	switch (psInst->eOpcode)
	{
		case IPCKU8F16:
		case IPCKU8F32:
		{
			switch (eNewFormat)
			{
				case UF_REGFORMAT_U8:
				{
					ModifyOpcode(psState, psInst, IPCKU8U8);
					psInst->u.psPck->bScale = IMG_FALSE;
					break;
				}
				case UF_REGFORMAT_C10:
				{
					SetOpcode(psState, psInst, ISOPWM);
					psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
					psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
					psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
					psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
					psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
					psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
					break;
				}
				case UF_REGFORMAT_F16:
				{
					ModifyOpcode(psState, psInst, IPCKU8F16);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_F32:
				{
					ModifyOpcode(psState, psInst, IPCKU8F32);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}

		case IPCKC10C10:
		case IPCKC10F16:
		case IPCKC10F32:
		{
			switch (eNewFormat)
			{
				case UF_REGFORMAT_U8:
				{
					SetOpcode(psState, psInst, ISOPWM);
					psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
					psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
					psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
					psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
					psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
					psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
					break;
				}
				case UF_REGFORMAT_C10:
				{
					ModifyOpcode(psState, psInst, IPCKC10C10);
					psInst->u.psPck->bScale = IMG_FALSE;
					break;
				}
				case UF_REGFORMAT_F16:
				{
					ModifyOpcode(psState, psInst, IPCKC10F16);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_F32:
				{
					ModifyOpcode(psState, psInst, IPCKC10F32);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}

		case IUNPCKF16U8:
		case IUNPCKF16C10:
		case IPCKF16F32:
		{
			switch (eNewFormat)
			{
				case UF_REGFORMAT_U8:
				{
					ModifyOpcode(psState, psInst, IUNPCKF16U8);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_C10:
				{
					ModifyOpcode(psState, psInst, IUNPCKF16C10);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_F16:
				{
					AppendToMoveList(psMoveList, psInst);

					ModifyOpcode(psState, psInst, IPCKF16F16);
					psInst->u.psPck->bScale = IMG_FALSE;
					break;
				}
				case UF_REGFORMAT_F32:
				{
					AppendToMoveList(psMoveList, psInst);

					ModifyOpcode(psState, psInst, IPCKF16F32);
					psInst->u.psPck->bScale = IMG_FALSE;
					break;
				}
				default:
				{
					break;
				}
			}
			break;
		}

		case IUNPCKF32U8:
		case IUNPCKF32C10:
		case IUNPCKF32F16:
		{
			switch (eNewFormat)
			{
				case UF_REGFORMAT_U8:
				{
					ModifyOpcode(psState, psInst, IUNPCKF32U8);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_C10:
				{
					ModifyOpcode(psState, psInst, IUNPCKF32C10);
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				}
				case UF_REGFORMAT_F16:
				{
					AppendToMoveList(psMoveList, psInst);

					ModifyOpcode(psState, psInst, IUNPCKF32F16);
					psInst->u.psPck->bScale = IMG_FALSE;
					break;
				}
				case UF_REGFORMAT_F32:
				{
					AppendToMoveList(psMoveList, psInst);

					SetOpcode(psState, psInst, IMOV);
					break;
				}
				default:
				{
					break;
				}
			}
			break;	
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVPCKFLTU8:
		case IVPCKFLTC10:
		{
			switch (eNewFormat)
			{
				case UF_REGFORMAT_U8: ModifyOpcode(psState, psInst, IVPCKFLTU8); break;
				case UF_REGFORMAT_C10: ModifyOpcode(psState, psInst, IVPCKFLTC10); break;
				default: imgabort();
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		default:
		{
			break;
		}
	}
}

/*****************************************************************************
 FUNCTION	: OptimiseUSPSampleDestFmt

 PURPOSE    : Override the destination format for USP sample instructions
			  to minimise the unpacking work that will be required.

 PARAMETERS	: psState		- Compiler State
			  psSmpInst		- The USP sample instruction to optimise
			  psMoveList	- Instructions which are converted to moves as
							a consequence of changing the USP sample format
							are appended to this list.

 RETURNS	: TRUE if a USP smp instruction was modified.
*****************************************************************************/
static IMG_BOOL OptimiseUSPSampleDestFmt(PINTERMEDIATE_STATE	psState,
										 PINST					psSmpInst,
										 PUSC_LIST				psMoveList)
{
	PARG					psSmpUnpackDest;
	IMG_UINT32				uSmpUnpackDestCount;
	IMG_UINT32				uDestIdx;
	IMG_UINT32				uChanIdx;
	IMG_UINT32				uAltFmtIdx;
	UF_REGFORMAT			eBestAltFmt;
	IMG_UINT32				uInvalidAltFormats;
	IMG_UINT32				uBestAltFmtUseCount;
	IMG_UINT32				uSharedAltDestFmts;
	IMG_UINT32				auSharedAltDestFmtUseCounts[UF_REGFORMAT_COUNT];
	IMG_UINT32				auAltDestFmts[CHANS_PER_REGISTER];
	IMG_UINT32				auAltDestFmtUseCount[CHANS_PER_REGISTER][UF_REGFORMAT_COUNT];
	IMG_UINT32				uNewSmpUnpackDestNum;
	IMG_UINT32				uNewSmpUnpackDestCount;
	IMG_UINT32				uNewSmpDestNum;
	IMG_UINT32 const*		auOldFmtChanOrder;
	IMG_UINT32 const*		auNewFmtChanOrder;
	PSMP_USP_PARAMS			psUSPSample;
	PINST					psSmpUnpackInst;
	PSMPUNPACK_PARAMS		psSmpUnpackParams;

	psUSPSample = &psSmpInst->u.psSmp->sUSPSample;

	ASSERT(psUSPSample->psSampleUnpack != NULL);

	psSmpUnpackInst = psUSPSample->psSampleUnpack;
	psSmpUnpackParams = psSmpUnpackInst->u.psSmpUnpack;

	ASSERT(psSmpUnpackInst->eOpcode == ISMPUNPACK_USP);
	ASSERT(psSmpUnpackParams->psTextureSample == psSmpInst);
	ASSERT(psSmpUnpackParams->uSmpID == psUSPSample->uSmpID);
		
	/*
		If the sample is performing a partial write, and isn't replacing
		all the live channels, we cannot alter the destination format.
	*/
	uSmpUnpackDestCount	= psSmpUnpackInst->uDestCount;
	psSmpUnpackDest		= &psSmpUnpackInst->asDest[0];

	if	(psSmpUnpackParams->uChanMask != USC_DESTMASK_FULL)
	{
		for	(uDestIdx = 0; uDestIdx < uSmpUnpackDestCount; uDestIdx++)
		{
			IMG_UINT32	uDestLive;
			IMG_UINT32	uDestMask;

			uDestMask	= psSmpUnpackInst->auDestMask[uDestIdx];
			uDestLive	= psSmpUnpackInst->auLiveChansInDest[uDestIdx];

			if	(uDestLive & ~uDestMask)
			{
				return IMG_FALSE;	
			}
		}
	}

	/*
		results, and determine which alternate formats could be used for each
		of them.
	*/
	for	(uDestIdx = 0; uDestIdx < uSmpUnpackDestCount; uDestIdx++)
	{
		auAltDestFmts[uDestIdx] = 0;

		for	(uAltFmtIdx = 0; uAltFmtIdx < UF_REGFORMAT_COUNT; uAltFmtIdx++)
		{
			auAltDestFmtUseCount[uDestIdx][uAltFmtIdx] = 0;
		}
	}

	uInvalidAltFormats = 0;

	for (uDestIdx = 0; uDestIdx < uSmpUnpackDestCount; uDestIdx++)
	{
		PUSEDEF_CHAIN	psSmpDestUseDef;
		PUSC_LIST_ENTRY	psListEntry;

		psSmpDestUseDef = UseDefGet(psState, psSmpUnpackDest[uDestIdx].uType, psSmpUnpackDest[uDestIdx].uNumber);
		ASSERT(psSmpDestUseDef != NULL);
		for (psListEntry = psSmpDestUseDef->sList.psHead;
			 psListEntry != NULL;
			 psListEntry = psListEntry->psNext)
		{
			PUSEDEF		psUse;
			PINST		psInst;
			IMG_UINT32	uSrcIdx;
			PARG		psArg;
			IMG_UINT32	uChansUsed;

			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			
			/*
				Skip the define at the sample instruction.
			*/
			if (psUse == psSmpDestUseDef->psDef)
			{
				continue;
			}

			if (psUse->eType != USE_TYPE_SRC)
			{
				/*
					Leave the sample instruction unchanged if we encounter a write to
					one of the USP sample destination registers
				*/
				return IMG_FALSE;
			}

			psInst = psUse->u.psInst;
			uSrcIdx = psUse->uLocation;
			psArg = &psInst->asArg[uSrcIdx];

			uChansUsed	= GetLiveChansInArg(psState, psInst, uSrcIdx);

			if	((psSmpUnpackInst->auDestMask[uDestIdx] & uChansUsed) == 0)
			{
				continue;		
			}

			/*
				Determine whether this instruction could source the sample unpacked
				data in any different formats, without changing it's result.
			*/
			switch (psInst->eOpcode)
			{
				case IPCKU8F16:
				case IPCKU8F32:
				{
					/*
						We cannot change the source format if this instruction
						combines sample unpacked data with any other.
					*/
					if	(g_auSetBitCount[psInst->auDestMask[0]] > 1)
					{
						return IMG_FALSE;	
					}

					/*
						Flag that we could sample to U8 instead
					*/
					auAltDestFmts[uDestIdx] |= 1 << UF_REGFORMAT_U8;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_U8]++;
					break;
				}

				case IPCKC10C10:
				case IPCKC10F16:
				case IPCKC10F32:
				{
					IMG_UINT32 uDestMask	= psInst->auDestMask[0];
					/*
						We cannot change the source format if this instruction
						combines sample unpacked data with any other.
					*/
					if	(g_auSetBitCount[uDestMask] > 1)
					{
						return IMG_FALSE;
					}

					/*
						If we choose to sample unpack to U8, then a PCKC10C10
						gets replaced with a SOPWM (since there is no
						PCKC10U8 instruction). SOPWM can only copy
						channel data straight between the source and dest,
						it cannot move it's location within a register.

						Therefore, PCKC10C10 must be performing a direct
						copy too, otherwise we cannot change it's source
						format.
					*/
					if	(psArg->eFmt == UF_REGFORMAT_C10)
					{
						if	((IMG_UINT32)(0x1 << GetPCKComponent(psState, psInst, uSrcIdx)) != uDestMask)
						{
							return IMG_FALSE;
						}
					}

					/*
						C10 and U8 and interchangeable, so we allow both as
						alternate formats when we see a pack to C10.

						NB: This is potentially wrong, since going to U8 could	
							result in loss of precision. However, it does give
							better code.
					*/
					auAltDestFmts[uDestIdx] |= (1U << UF_REGFORMAT_C10) |
											   (1U << UF_REGFORMAT_U8);

					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_C10]++;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_U8]++;
					break;
				}

				case IUNPCKF16U8:
				case IUNPCKF16C10:
				case IPCKF16F32:
				{
					/*
						We cannot change the source format if this instruction
						combines sample unpacked data with any other.
					*/
					if	(g_auSetBitCount[psInst->auDestMask[0]] > 2)
					{
						return IMG_FALSE;	
					}

					/*
						Flag that we could sample unpack to F16 instead
					*/
					auAltDestFmts[uDestIdx] |= 1 << UF_REGFORMAT_F16;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_F16]++;
					break;
				}

				case IUNPCKF32U8:
				case IUNPCKF32C10:
				case IUNPCKF32F16:
				{
					/*
						We cannot change the source format if this instruction
						combines sample unpacked data with any other.
					*/
					if	(g_auSetBitCount[psInst->auDestMask[0]] != 4)
					{
						return IMG_FALSE;	
					}

					/*
						Flag that we could sample unpack to F16 instead
					*/
					auAltDestFmts[uDestIdx] |= 1 << UF_REGFORMAT_F16;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_F16]++;
					break;	
				}

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				case IVPCKFLTU8:
				{
					UF_REGFORMAT	eDestFmt;

					/*
						Flag that we could sample to floating point instead.
					*/
					ASSERT(psInst->uDestCount == 1);
					eDestFmt = psInst->asDest[0].eFmt;
					ASSERT(eDestFmt == UF_REGFORMAT_F16 || eDestFmt == UF_REGFORMAT_F32);

					auAltDestFmts[uDestIdx] |= 1 << eDestFmt;
					auAltDestFmtUseCount[uDestIdx][eDestFmt]++;
					break;
				}
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

				case ISOPWM:
				case ISOP2:
				case ISOP3:
				case ILRP1:
				case IFPMA:
				case IFPADD8:
				case IFPSUB8:
				case IFPDOT:
				{
					/*
						Colour instructions can source data as either U8 or C10
					*/
					auAltDestFmts[uDestIdx] |= (1U << UF_REGFORMAT_U8) |
											   (1U << UF_REGFORMAT_C10);

					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_U8]++;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_C10]++;

					/*
						Colour instructions can only support U8 or C10 data
						directly. If we choose any other format then additional
						PACKs will have to be inserted.
					*/
					uInvalidAltFormats |= (1U << UF_REGFORMAT_F16) |
										  (1U << UF_REGFORMAT_F32);

					break;
				}

				default:
				{
					/*
						Check if this instruction can accept a register in either F16
						or F32 formats.
					*/
					if	(!HasF16F32SelectInst(psInst))
					{
						return IMG_FALSE;
					}

					/*
						Check for the hardware bug whereby an f16 format register can't appear
						in source 0 of an EFO.
					*/
					if (uSrcIdx == 0 && 
						psInst->eOpcode == IEFO && 
						(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697) != 0)
					{
						return IMG_FALSE;
					}

					/*
						This instruction can source the sampled data in either F16 or F32
						format
					*/
					auAltDestFmts[uDestIdx] |= (1U << UF_REGFORMAT_F16) |
											   (1U << UF_REGFORMAT_F32);

					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_F16]++;
					auAltDestFmtUseCount[uDestIdx][UF_REGFORMAT_F32]++;

					/*
						Float instructions can only support F16 or F32 data
						directly. If we choose any other format then additional
						PACKs will have to be inserted.
					*/
					uInvalidAltFormats |= (1U << UF_REGFORMAT_U8) |
										  (1U << UF_REGFORMAT_C10);
					break;
				}
			}
		}
	}

	/*
		Restrict the set of alternate formats to those that are a equal
		or higher precision to that of the texture

		NB: As a sneaky optimisation, force C10 to be downgraded to U8, 
			as this can lead to better code. For the same reason, don't
			allow U8 to be upgraded to C10, since C10 data generally
			needs to be allocated an IReg, which can also lead to less
			efficient code compared to U8.
	*/
	uSharedAltDestFmts = 0;

	switch (psUSPSample->eTexPrecision)
	{
		case UF_REGFORMAT_U8:
		{
			uSharedAltDestFmts = (1U << UF_REGFORMAT_U8) |
								 (1U << UF_REGFORMAT_F16) |
								 (1U << UF_REGFORMAT_F32);
			break;
		}
		case UF_REGFORMAT_C10:
		{
			uSharedAltDestFmts = (1U << UF_REGFORMAT_U8) |
								 (1U << UF_REGFORMAT_F16) |
								 (1U << UF_REGFORMAT_F32);
			break;
		}
		case UF_REGFORMAT_F16:
		{
			uSharedAltDestFmts = (1U << UF_REGFORMAT_F16) |
								 (1U << UF_REGFORMAT_F32);
			break;
		}
		case UF_REGFORMAT_F32:
		{
			uSharedAltDestFmts = (1U << UF_REGFORMAT_F32);
			break;
		}
		default:
		{
			imgabort();
			break;
		}
	}

	/*
		Remove alternate formats that cannot be used (due to instruction restrictions)
		without adding extra instructions
	*/
	uSharedAltDestFmts &= ~uInvalidAltFormats;

	/*
		Currently, the USP only supports the same destination-register format
		for all sampled channels (although the interface allows otherwise). 
		
		Make sure that there is at-least one alternate format for all channels
		being sampled.
	*/
	for	(uAltFmtIdx = 0; uAltFmtIdx < UF_REGFORMAT_COUNT; uAltFmtIdx++)
	{
		auSharedAltDestFmtUseCounts[uAltFmtIdx] = 0;
	}

	for	(uDestIdx = 0; uDestIdx < uSmpUnpackDestCount; uDestIdx++)
	{
		if	(!psSmpUnpackInst->auLiveChansInDest[uDestIdx])
		{
			continue;
		}

		uSharedAltDestFmts &= auAltDestFmts[uDestIdx];
		if	(!uSharedAltDestFmts)
		{
			return IMG_FALSE;
		}

		for	(uAltFmtIdx = 0; uAltFmtIdx < UF_REGFORMAT_COUNT; uAltFmtIdx++)
		{
			auSharedAltDestFmtUseCounts[uAltFmtIdx] += auAltDestFmtUseCount[uDestIdx][uAltFmtIdx];
		}
	}

	/*
		Choose the best alternate format for the Smp Unpack destination.

		NB: We check the alternate formats in increasing precision,
			as the unpacking cost for F32 is highest.
	*/
	eBestAltFmt			= psSmpUnpackDest->eFmt;
	uBestAltFmtUseCount = 0;

	for	(uAltFmtIdx = UF_REGFORMAT_COUNT - 1; uAltFmtIdx != UINT_MAX; uAltFmtIdx--)
	{
		if	((uSharedAltDestFmts & (1U << uAltFmtIdx)) == 0)
		{
			continue;
		}
		if	(auSharedAltDestFmtUseCounts[uAltFmtIdx] > uBestAltFmtUseCount)
		{
			eBestAltFmt			= (UF_REGFORMAT)uAltFmtIdx;
			uBestAltFmtUseCount = auSharedAltDestFmtUseCounts[uAltFmtIdx];
		}
	}

	if	(eBestAltFmt == psSmpUnpackDest->eFmt)
	{
		return IMG_FALSE;	
	}

	/*
		Allocate new temporary registers to represent the new destination of
		the sample unpack instruction.

		NB: Currently, we never choose C10 as the new dest format.
	*/
	uNewSmpUnpackDestCount = 0;

	switch (eBestAltFmt)
	{
		case UF_REGFORMAT_U8:
		{
			uNewSmpUnpackDestCount = 1;
			break;
		}
		case UF_REGFORMAT_F16:
		{
			uNewSmpUnpackDestCount = 2;
			break;
		}
		case UF_REGFORMAT_F32:
		{
			uNewSmpUnpackDestCount = 4;
			break;
		}
		default:
		{
			imgabort();
			break;
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT) != 0)
	{
		if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL && 
			psUSPSample->bNonDependent)
		{
			PPIXELSHADER_INPUT		psInput;
			UNIFLEX_TEXLOAD_FORMAT	eFormatConvert;

			switch(eBestAltFmt)
			{
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_C10:
				{
					eFormatConvert = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
					break;
				}

				case UF_REGFORMAT_F32:
				{
					eFormatConvert = UNIFLEX_TEXLOAD_FORMAT_F32;
					break;
				}

				case UF_REGFORMAT_F16:
				{
					eFormatConvert = UNIFLEX_TEXLOAD_FORMAT_F16;
					break;
				}

				default:
				{
					eFormatConvert = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
					break;
				}
			}

			/*
				Add an entry for the newly selected format to the list of pixel 
				shader inputs representing the non-dependent samples.
			*/
			psInput = 
				AddOrCreateNonDependentTextureSample(psState,
													 psSmpInst->u.psSmp->uTextureStage,
													 0 /* uChunk */,
													 psUSPSample->uNDRTexCoord,
													 psSmpInst->u.psSmp->uDimensionality,
													 psUSPSample->bProjected,
													 uNewSmpUnpackDestCount, 
													 uNewSmpUnpackDestCount,
													 UF_REGFORMAT_UNTYPED,
													 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
													 IMG_FALSE /* bVector */,
													 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
													 eFormatConvert);

			/* Update the source register */
			if(psSmpUnpackDest->eFmt != eBestAltFmt)
			{
				IMG_UINT32	uSrcIdx;

				for (uSrcIdx = 0; uSrcIdx < uNewSmpUnpackDestCount; uSrcIdx++)
				{
					SetSrc(psState, 
						   psSmpInst, 
						   NON_DEP_SMP_TFDATA_ARG_START + uSrcIdx, 
						   USEASM_REGTYPE_TEMP, 
						   psInput->psFixedReg->auVRegNum[uSrcIdx], 
						   UF_REGFORMAT_UNTYPED);
				}
				for (; uSrcIdx < NON_DEP_SMP_TFDATA_ARG_MAX_COUNT; uSrcIdx++)
				{
					SetSrcUnused(psState, 
								 psSmpInst,
								 NON_DEP_SMP_TFDATA_ARG_START + uSrcIdx);
				}
			}

		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */

	uNewSmpUnpackDestNum = GetNextRegisterCount(psState, uNewSmpUnpackDestCount);

	/*
		If we are switching between U8/C10 and F16/F32 formats, swap the
		red and blue channels in the destination write-mask and swizzle.
	*/
	auOldFmtChanOrder = GetUSPDestFmtChannelOrder(psState, psSmpUnpackDest->eFmt);
	auNewFmtChanOrder = GetUSPDestFmtChannelOrder(psState, eBestAltFmt);

	if	(auOldFmtChanOrder != auNewFmtChanOrder)
	{
		IMG_UINT32	uOldChanSwizzle;
		IMG_UINT32	uOldChanMask;
		IMG_UINT32	uChan;

		uOldChanMask		= psSmpUnpackParams->uChanMask;
		uOldChanSwizzle		= psUSPSample->uChanSwizzle;

		psSmpUnpackParams->uChanMask = 0;
		psUSPSample->uChanSwizzle = 0;

		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uMaskBit;
			IMG_UINT32	uSwizSel;

			uMaskBit = (uOldChanMask >> auOldFmtChanOrder[uChan]) & 0x1;
			uSwizSel = (uOldChanSwizzle >> (auOldFmtChanOrder[uChan] * 3)) & 0x7;

			psSmpUnpackParams->uChanMask |= (uMaskBit << auNewFmtChanOrder[uChan]);
			psUSPSample->uChanSwizzle |= (uSwizSel << (auNewFmtChanOrder[uChan] * 3));
		}
	}

	/*
		Modify instructions that reference the sample destination
		registers to use the new format and location.
	*/
	for (uDestIdx = 0; uDestIdx < uSmpUnpackDestCount; uDestIdx++)
	{
		PUSEDEF_CHAIN	psSmpDestUseDef;
		PUSC_LIST_ENTRY	psListEntry;
		PUSC_LIST_ENTRY	psNextListEntry;

		psSmpDestUseDef = UseDefGet(psState, psSmpUnpackDest[uDestIdx].uType, psSmpUnpackDest[uDestIdx].uNumber);
		ASSERT(psSmpDestUseDef != NULL);
		for (psListEntry = psSmpDestUseDef->sList.psHead;
			 psListEntry != NULL;
			 psListEntry = psNextListEntry)
		{
			PUSEDEF		psUse;
			PINST		psInst;
			IMG_UINT32	uSrcIdx;
			IMG_UINT32	uChansUsed;
			IMG_UINT32	uCompMask;
			IMG_UINT32	uCompIdx;
			IMG_UINT32	uNewDestIdx;

			psNextListEntry = psListEntry->psNext;
			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

			if (psUse == psSmpDestUseDef->psDef)
			{
				continue;
			}

			ASSERT(psUse->eType == USE_TYPE_SRC);
	
			psInst = psUse->u.psInst;
			uSrcIdx = psUse->uLocation;

			uChansUsed = GetLiveChansInArg(psState, 
										   psInst, 
										   uSrcIdx);

			if	((psSmpInst->auDestMask[uDestIdx] & uChansUsed) == 0)
			{
				continue;		
			}

			/*
				Should only be referencing the results of the texture sample.
			*/
			ASSERT((uChansUsed & ~psSmpInst->auDestMask[uDestIdx]) == 0);

			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (
					(
						psInst->eOpcode == IVPCKFLTU8 || 
						psInst->eOpcode == IVPCKFLTC10
					) &&
					(
						eBestAltFmt == UF_REGFORMAT_F16 ||
						eBestAltFmt == UF_REGFORMAT_F32
					)
				)
			{
				IMG_UINT32	uChan;

				ModifyOpcode(psState, psInst, IVMOV);
				psInst->u.psVec->bPackScale = IMG_FALSE;
				
				SetSrcUnused(psState, psInst, 0 /* uSrcIdx */);

				psInst->u.psVec->aeSrcFmt[0] = eBestAltFmt;
				for (uChan = 0; uChan < uNewSmpUnpackDestCount; uChan++)
				{
					SetSrc(psState, psInst, 1 + uChan, USEASM_REGTYPE_TEMP, uNewSmpUnpackDestNum + uChan, eBestAltFmt);
				}

				for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
				{
					USEASM_SWIZZLE_SEL	eSel;

					eSel = GetChan(psInst->u.psVec->auSwizzle[0], uChan);
					if (!g_asSwizzleSel[eSel].bIsConstant)
					{
						IMG_UINT32	uSrcChan;

						uSrcChan = g_asSwizzleSel[eSel].uSrcChan;
						uSrcChan = auOldFmtChanOrder[uSrcChan];
						uSrcChan = auNewFmtChanOrder[uSrcChan];
						eSel = g_aeChanToSwizzleSel[uSrcChan];
					}
					psInst->u.psVec->auSwizzle[0] = SetChan(psInst->u.psVec->auSwizzle[0], uChan, eSel);
				}

				continue;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

			/*
				Map the mask of bytes used to a mask of components used.
			*/
			uCompMask = ChannelMaskToComponentMask(psState, uDestIdx, uChansUsed, psSmpUnpackDest->eFmt);
			if (g_aiSingleComponent[uCompMask] == -1)
			{
				/*
					If multiple components are used then we can't be changing either the number
					of registers or the channel order.
				*/
				ASSERT(auOldFmtChanOrder == auNewFmtChanOrder);
				ASSERT(uNewSmpUnpackDestCount == 1);
				ASSERT(uSmpUnpackDestCount == 1);

				uNewDestIdx = 0;
				uCompIdx = 0;
			}
			else
			{
				uCompIdx = (IMG_UINT32)g_aiSingleComponent[uCompMask];

				/*
					Swap the component from the old channel order to the new.
				*/
				uCompIdx = auOldFmtChanOrder[uCompIdx];
				uCompIdx = auNewFmtChanOrder[uCompIdx];

				/*
					Map the component index to an index into the destination registers
					and a byte select.
				*/
				ComponentToChannel(psState, uCompIdx, &uNewDestIdx, &uCompIdx, eBestAltFmt);
			}

			/*
				Update the source register to use the new SMP destination.
			*/
			SetSrc(psState, psInst, uSrcIdx, USEASM_REGTYPE_TEMP, uNewSmpUnpackDestNum + uNewDestIdx, eBestAltFmt); 
			SetComponentSelect(psState, psInst, uSrcIdx, uCompIdx);

			/*
				Change the PACK/UNPACK instruction opcode to match the
				new source format
			*/
			SetPackSourceFormat(psState, psInst, eBestAltFmt, psMoveList);
		}
	}

	/*
		Modify the USP sample unpack instruction to account for the chosen format

		NB: The channel mask should reflect the live channels only, so it's
			OK for the destination masks and live-channels to be set the
			same here.
	*/	
	SetDestCount(psState, psSmpUnpackInst, uNewSmpUnpackDestCount);
	uNewSmpDestNum = GetNextRegisterCount(psState, uNewSmpUnpackDestCount);
	SetDestCount(psState, psSmpInst, UF_MAX_CHUNKS_PER_TEXTURE + uNewSmpUnpackDestCount);	
	for	(uDestIdx = 0; uDestIdx < uNewSmpUnpackDestCount; uDestIdx++)
	{
		psSmpUnpackInst->auDestMask[uDestIdx] = 0; 
		psSmpUnpackInst->auLiveChansInDest[uDestIdx] = 0;
		#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
		if (psState->uFlags & USC_FLAGS_INITIALISEREGSONFIRSTWRITE)
		{			
			psSmpUnpackInst->u.psSmpUnpack->auInitialisedChansInDest[uDestIdx] = 0;
		}
		#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */
	}

	for	(uChanIdx = 0; uChanIdx < CHANS_PER_REGISTER; uChanIdx++)
	{
		IMG_UINT32	uCompIdx;
		IMG_UINT32	uCompMask;

		if	((psSmpUnpackParams->uChanMask & (1U << uChanIdx)) == 0)
		{
			continue;
		}

		switch (eBestAltFmt)
		{
			case UF_REGFORMAT_U8:
			case UF_REGFORMAT_C10:
			{
				uDestIdx	= 0;
				uCompIdx	= uChanIdx;
				uCompMask	= 0x1;
				break;
			}
			case UF_REGFORMAT_F16:
			{
				uDestIdx	= uChanIdx >> 1;
				uCompIdx	= (uChanIdx & 1) * 2;
				uCompMask	= 0x3;
				break;
			}
			case UF_REGFORMAT_F32:
			{
				uDestIdx	= uChanIdx;
				uCompIdx	= 0;
				uCompMask	= 0xF;
				break;
			}
			default:
			{
				uDestIdx	= 0;
				uCompIdx	= 0;
				uCompMask	= 0;
				break;
			}
		}

		psSmpUnpackInst->auDestMask[uDestIdx] |= uCompMask << uCompIdx;
		psSmpUnpackInst->auLiveChansInDest[uDestIdx] |= uCompMask << uCompIdx;
	}

	/*
		Update the SMP unpack instruction's destinations.
	*/
	for (uDestIdx = 0; uDestIdx < uNewSmpUnpackDestCount; uDestIdx++)
	{	
		SetDest(psState, psSmpUnpackInst, uDestIdx, USEASM_REGTYPE_TEMP, uNewSmpUnpackDestNum + uDestIdx, eBestAltFmt);

		SetDest(psState, 
				psSmpInst, 
				UF_MAX_CHUNKS_PER_TEXTURE + uDestIdx, 
				USEASM_REGTYPE_TEMP, 
				uNewSmpDestNum + uDestIdx, 
				eBestAltFmt);
		psSmpInst->auDestMask[UF_MAX_CHUNKS_PER_TEXTURE + uDestIdx] = USC_XYZW_CHAN_MASK;

		SetSrc(psState, 
			   psSmpUnpackInst, 
			   UF_MAX_CHUNKS_PER_TEXTURE + uDestIdx, 
			   USEASM_REGTYPE_TEMP, 
			   uNewSmpDestNum + uDestIdx, 
			   eBestAltFmt);
	}

	for (uDestIdx = (UF_MAX_CHUNKS_PER_TEXTURE + uNewSmpUnpackDestCount); uDestIdx < (UF_MAX_CHUNKS_PER_TEXTURE + UF_MAX_CHUNKS_PER_TEXTURE); uDestIdx++)
	{
		SetSrcUnused(psState, psSmpUnpackInst, uDestIdx);
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: OptimiseUSPSamplesBP

 PURPOSE    : BLOCK_PROC to try to optimise the destination format for all
	USP pseudo samples in the block.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL
IMG_VOID OptimiseUSPSamplesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
{
	PINST			psInst;
	USC_LIST		sMoveList;
	PUSC_LIST_ENTRY	psListEntry;

	PVR_UNREFERENCED_PARAMETER(pvNull);

	/*
		Try to optimise the destination format for all USP pseudo samples
		in the block
	*/
	InitializeList(&sMoveList);
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		IOPCODE	eOpcode;

		eOpcode = psInst->eOpcode;

		if	((eOpcode >= ISMP_USP) && (eOpcode <= ISMP_USP_NDR))
		{
			OptimiseUSPSampleDestFmt(psState, psInst, &sMoveList);
		}
	}

	/*
		Optimizing the samples generated new MOV instructions. Try
		to remove them.
	*/
	while ((psListEntry = RemoveListHead(&sMoveList)) != NULL)
	{
		PINST		psMoveInst;
		IMG_BOOL	bNewMoves;

		psMoveInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
		SetBit(psMoveInst->auFlag, INST_MODIFIEDINST, 0);

		switch (psMoveInst->eOpcode)
		{
			case IMOV:
			{
				EliminateGlobalMove(psState, 
									psBlock, 
									psMoveInst, 
									&psMoveInst->asArg[0], 
									&psMoveInst->asDest[0], 
									NULL /* psEvalList */);
				break;
			}
			case IUNPCKF32F16:
			case IPCKF16F16:
			case IPCKF16F32:
			{
				EliminateF16Move(psState, psMoveInst, &bNewMoves);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}
#endif /* #if defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
static IMG_BOOL CheckSamplerUseInst(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									PCSOURCE_VECTOR		psUsedSources,
									IMG_PVOID			pvCheckOnly)
/*****************************************************************************
 FUNCTION	: CheckSamplerUseInst

 PURPOSE    : Checks if some of the sources to an instruction could be replaced
			  by F16 format data.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  psUsedSources		- Instruction sources which are to be replaced
								by F16 data.
			  pvCheckOnly		- Points to a boolean. If TRUE then the instruction
								is modified so the arguments can be F16.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_PBOOL	pbCheckOnly = (IMG_PBOOL)pvCheckOnly;
	IMG_BOOL	bCheckOnly = *pbCheckOnly;

	if (psInst->eOpcode == IPCKU8F32)
	{
		/*
			Change the source format by changing the opcode providing all
			the arguments are being replaced.
		*/
		if (!(g_abSingleBitSet[psInst->auDestMask[0]] || CheckSourceVectorRange(psUsedSources, PCK_SOURCE0, PCK_SOURCE1)))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		if (!bCheckOnly)
		{
			ModifyOpcode(psState, psInst, IPCKU8F16);
		}
	}
	else
	{
		/*
			Check if the instruction can accept a register in multiple formats.
		*/
		if (!HasF16F32SelectInst(psInst))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		/*
			Check for the hardware bug whereby an f16 format register can't appear
			in source 0 of an EFO.
		*/
		if (
				CheckSourceVector(psUsedSources, EFO_UNIFIEDSTORE_SOURCE0) &&
				psInst->eOpcode == IEFO && 
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697) != 0
			)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckSamplerUseForPrecisionReduce

 PURPOSE    : Check all uses of a sampler result to see if the sampler result
			  can be changed to F16.

 PARAMETERS	: psState			- Compiler state.
			  psUseDefChain		- Sampler result to check.
			  uNewDestNum		- F16 destination register number.
			  uNewDestComponent	- F16 destination register component.
			  bCheckOnly		- If TRUE just check don't replace registers.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL CheckSamplerUseForPrecisionReduce(PINTERMEDIATE_STATE	psState, 
												  IMG_UINT32			uDestCount,
												  IMG_PUINT32			puOldDestNums,
												  IMG_PUINT32			puNewDestNums,
												  IMG_PUINT32			puNewDestComps,
												  IMG_BOOL				bCheckOnly)
{
	PUSEDEF_CHAIN	apsOldDests[SMP_MAX_SAMPLE_RESULT_DWORDS];
	IMG_UINT32		uDestIdx;

	/*
		Get lists of uses/defines for each of the SMP destinations.
	*/
	ASSERT(uDestCount <= SMP_MAX_SAMPLE_RESULT_DWORDS);
	for (uDestIdx = 0; uDestIdx < uDestCount; uDestIdx++)
	{
		apsOldDests[uDestIdx] = UseDefGet(psState, USEASM_REGTYPE_TEMP, puOldDestNums[uDestIdx]);
	}

	/*
		For all of the instructions uses any of the SMP destinations as sources then they can
		accept F16 format arguments.
	*/
	if (!UseDefForAllUsesOfSet(psState, uDestCount, apsOldDests, CheckSamplerUseInst, &bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	if (!bCheckOnly)
	{
		for (uDestIdx = 0; uDestIdx < uDestCount; uDestIdx++)
		{
			PUSC_LIST_ENTRY	psListEntry, psNextListEntry;
			PUSEDEF_CHAIN	psOldDest = apsOldDests[uDestIdx];

			if (psOldDest == NULL)
			{
				continue;
			}

			for (psListEntry = psOldDest->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
			{
				PUSEDEF		psUse;
				PINST		psInst;
				IMG_UINT32	uArg;

				psNextListEntry = psListEntry->psNext;
				psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
				if (psUse == psOldDest->psDef)
				{
					continue;
				}
				ASSERT(psUse->eType == USE_TYPE_SRC);
				psInst = psUse->u.psInst;
				uArg = psUse->uLocation;

				/*
					Replace with a use of the F16 format result.
				*/
				SetSrc(psState, psInst, uArg, USEASM_REGTYPE_TEMP, puNewDestNums[uDestIdx], UF_REGFORMAT_F16);
				SetComponentSelect(psState, psInst, uArg, puNewDestComps[uDestIdx]);
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL ReduceSampleResultPrecisionForDependentTextureSample(PINTERMEDIATE_STATE	psState,
															  PINST					psInst,
															  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: ReduceSampleResultPrecisionForDependentTextureSample

 PURPOSE    : Check if the  the precision of the result of a dependent texture
			  sample can be reduced from F32 to F16.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  bCheckOnly	- Just check if the change is possible.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uNewDestCount = UINT_MAX;
	IMG_UINT32	puNewDestRegNums[SMP_MAX_SAMPLE_RESULT_DWORDS];
	IMG_UINT32	puNewDestComponents[SMP_MAX_SAMPLE_RESULT_DWORDS];
	IMG_UINT32	puOldDestRegNums[SMP_MAX_SAMPLE_RESULT_DWORDS];
	IMG_UINT32	puAllocatedDests[SMP_MAX_SAMPLE_RESULT_DWORDS];
	IMG_UINT32	puNewLiveChansInDest[SMP_MAX_SAMPLE_RESULT_DWORDS];

	/*
		Don't replace predicated instructions since we would need to replace other instructions
		which write the destination.
	*/
	if (!NoPredicate(psState, psInst))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Don't replace texture samples returning texture sample parameters.
	*/
	if (psInst->u.psSmp->eReturnData != SMP_RETURNDATA_NORMAL)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Get new register for the F16 format destinations and create a new
		group for them.
	*/
	if (!bCheckOnly)
	{
		ASSERT(psInst->uDestCount <= SMP_MAX_SAMPLE_RESULT_DWORDS);

		uNewDestCount = (psInst->uDestCount + 1) / 2;
		ASSERT(uNewDestCount <= SMP_MAX_SAMPLE_RESULT_DWORDS);

		for (uDestIdx = 0; uDestIdx < uNewDestCount; uDestIdx++)
		{
			puAllocatedDests[uDestIdx] = GetNextRegister(psState);
			puNewLiveChansInDest[uDestIdx] = 0;
		}
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			IMG_UINT32	uSrcChan;
			PUNIFLEX_TEXTURE_PARAMETERS	psTexParams;

			psTexParams = &psState->psSAOffsets->asTextureParameters[psInst->u.psSmp->uTextureStage];
			if (psTexParams->sFormat.bTFSwapRedAndBlue && !psTexParams->bGamma)
			{
				uSrcChan = g_puSwapRedAndBlueChan[uDestIdx];
			}
			else
			{
				uSrcChan = uDestIdx;
			}

			ASSERT((uSrcChan / 2) < uNewDestCount);
			puNewDestRegNums[uDestIdx] = puAllocatedDests[uSrcChan / 2];
			puNewDestComponents[uDestIdx] = (uSrcChan % 2) * 2;

			if (psInst->auLiveChansInDest[uDestIdx] != 0)
			{
				puNewLiveChansInDest[uSrcChan / 2] |= (USC_XY_CHAN_MASK << puNewDestComponents[uDestIdx]);
			}
		}
	}

	/*
		Put the current destination register numbers into an array.
	*/
	ASSERT(psInst->uDestCount <= SMP_MAX_SAMPLE_RESULT_DWORDS);
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (psDest->uType != USEASM_REGTYPE_TEMP)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		puOldDestRegNums[uDestIdx] = psDest->uNumber;
	}

	/*
		Check all the places that use the result of the sampler.
	*/
	if (!CheckSamplerUseForPrecisionReduce(psState, 
										   psInst->uDestCount,
										   puOldDestRegNums,
										   puNewDestRegNums,
										   puNewDestComponents,
										   bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	if (!bCheckOnly)
	{
		SetDestCount(psState, psInst, uNewDestCount);

		for (uDestIdx = 0; uDestIdx < uNewDestCount; uDestIdx++)
		{
			SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_TEMP, puAllocatedDests[uDestIdx], UF_REGFORMAT_F16);
			psInst->auLiveChansInDest[uDestIdx] = puNewLiveChansInDest[uDestIdx];
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ReduceSampleResultPrecisionForSampler

 PURPOSE    : Try to reduce the precision of a texture sample result from F32
			  to F16.

 PARAMETERS	: psState		- Compiler state.
			  uSamplerIdx	- Index of the sampler to reduce the precision for.
			  bCheckOnly	- Just check if the change is possible.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL ReduceSampleResultPrecisionForSampler(PINTERMEDIATE_STATE	psState,
													  IMG_UINT32			uSamplerIdx,
													  IMG_BOOL				bCheckOnly)
{
	static const IOPCODE	aeSmpOpcodes[] = {ISMP, ISMPBIAS, ISMPREPLACE, ISMPGRAD};
	IMG_UINT32				uOpcodeIdx;

	/*
		Check everywhere where the sampler is used in a dependent read
		that we can replace F32-format registers by F16 format.
	*/
	for (uOpcodeIdx = 0; uOpcodeIdx < (sizeof(aeSmpOpcodes) / sizeof(aeSmpOpcodes[0])); uOpcodeIdx++)
	{
		IOPCODE				eOpcode = aeSmpOpcodes[uOpcodeIdx];
		SAFE_LIST_ITERATOR	sIter;
		IMG_BOOL			bFailed;

		InstListIteratorInitialize(psState, eOpcode, &sIter);
		bFailed = IMG_FALSE;
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST	psSmpInst;

			psSmpInst = InstListIteratorCurrent(&sIter);
			if (psSmpInst->u.psSmp->uTextureStage == uSamplerIdx)
			{
				if (!ReduceSampleResultPrecisionForDependentTextureSample(psState, psSmpInst, bCheckOnly))
				{
					ASSERT(bCheckOnly);
					bFailed = IMG_TRUE;
					break;
				}
			}
		}
		InstListIteratorFinalise(&sIter);

		if (bFailed)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check everywhere where the sample is used in a non-dependent read.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PUSC_LIST_ENTRY		psInputListEntry;
		PPIXELSHADER_STATE	psPS;

		psPS = psState->sShader.psPS;
		
		for (psInputListEntry = psPS->sPixelShaderInputs.psHead;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext)
		{
			PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);

			if (psInput->sLoad.uTexture == uSamplerIdx)
			{
				IMG_UINT32	uRegIdx;
				IMG_UINT32	uOldTempCount;
				IMG_UINT32	puOldTempNums[SMP_MAX_SAMPLE_RESULT_DWORDS];
				IMG_UINT32	puNewTempNums[SMP_MAX_SAMPLE_RESULT_DWORDS];
				IMG_UINT32	puNewTempComponents[SMP_MAX_SAMPLE_RESULT_DWORDS];

				ASSERT(psInput->sLoad.uNumAttributes <= SMP_MAX_SAMPLE_RESULT_DWORDS);

				/*
					Set up an array with the temporary register numbers currently written
					by the non-dependent texture sample.
				*/
				uOldTempCount = psInput->sLoad.uNumAttributes;
				for (uRegIdx = 0; uRegIdx < psInput->sLoad.uNumAttributes; uRegIdx++)
				{
					puOldTempNums[uRegIdx] = psInput->psFixedReg->auVRegNum[uRegIdx];
				}

				if (!bCheckOnly)
				{
					IMG_UINT32		uFirstNewDestNum;
					IMG_UINT32		uOldNumAttributes;
					PFIXED_REG_DATA	psFixedReg;

					/*
						Reduce the number of registers written by the texture sample.
					*/
					uOldNumAttributes = psInput->sLoad.uNumAttributes;
					psInput->sLoad.uNumAttributes = (psInput->sLoad.uNumAttributes + 1) / 2;

					/*
						Get new registers for the results of the texture sample unpacked to
						F16 precision.
					*/
					uFirstNewDestNum = GetNextRegisterCount(psState, psInput->sLoad.uNumAttributes);

					/*
						Update the set of registers which contain the result of the texture sample.
					*/
					psFixedReg = psInput->psFixedReg;
					ASSERT(psFixedReg->uConsecutiveRegsCount == uOldNumAttributes);
					for (uRegIdx = 0; uRegIdx < uOldNumAttributes; uRegIdx++)
					{
						UseDefDropFixedRegDef(psState, psFixedReg, uRegIdx);
					}
					UscFree(psState, psFixedReg->auVRegNum);

					psFixedReg->uConsecutiveRegsCount = psInput->sLoad.uNumAttributes;
					psFixedReg->auVRegNum = 
						UscAlloc(psState, sizeof(psFixedReg->auVRegNum[0]) * psInput->sLoad.uNumAttributes);
					for (uRegIdx = 0; uRegIdx < psInput->sLoad.uNumAttributes; uRegIdx++)
					{
						psFixedReg->auVRegNum[uRegIdx] = uFirstNewDestNum + uRegIdx;
						UseDefAddFixedRegDef(psState, psFixedReg, uRegIdx);
					}

					/*
						Calculate the new temporary register numbers and channels
						within the registers.
					*/
					for (uRegIdx = 0; uRegIdx < uOldNumAttributes; uRegIdx++)
					{
						IMG_UINT32	uSrcChan;
						PUNIFLEX_TEXTURE_PARAMETERS	psTexParams;

						psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
						if (psTexParams->sFormat.bTFSwapRedAndBlue && !psTexParams->bGamma)
						{
							uSrcChan = g_puSwapRedAndBlueChan[uRegIdx];
						}
						else	
						{
							uSrcChan = uRegIdx;
						}

						puNewTempNums[uRegIdx] = uFirstNewDestNum + uSrcChan / 2;
						puNewTempComponents[uRegIdx] = (uSrcChan % 2) * 2;
					}
				}
				
				/*
					Check everywhere where the result of the texture sample is used.
				*/
				if (!CheckSamplerUseForPrecisionReduce(psState, 
													   uOldTempCount,
													   puOldTempNums,
													   puNewTempNums,
													   puNewTempComponents,
													   bCheckOnly))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
			}
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: ReduceSampleResultPrecision

 PURPOSE    : Try and reduce the precision of sampler results from F32 to F16.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID ReduceSampleResultPrecision(PINTERMEDIATE_STATE	psState)
{
	IMG_UINT32	uSamplerIdx;

	for (uSamplerIdx = 0; uSamplerIdx < psState->psSAOffsets->uTextureCount; uSamplerIdx++)
	{
		if (
				GetBit(psState->auTextureUnpackFormatSelectedMask, uSamplerIdx) != 0 &&
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat == UNIFLEX_TEXTURE_UNPACK_FORMAT_F32
		   )
		{
			IMG_UINT32	uChan;
			IMG_BOOL	bHighPrecision;
			PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];

			/*
				Check if the texture has any channels with greater than F16 precision.
			*/
			bHighPrecision = IMG_FALSE;
			for (uChan = 0; uChan < 4; uChan++)
			{
				USC_CHANNELFORM	eForm = psTexParams->sFormat.peChannelForm[uChan];

				if (eForm == USC_CHANNELFORM_F32 ||
					eForm == USC_CHANNELFORM_DF32 ||
					eForm == USC_CHANNELFORM_U16 ||
					eForm == USC_CHANNELFORM_S16)
				{
					bHighPrecision = IMG_TRUE;
					break;
				}
			}
			if (bHighPrecision)
			{
				continue;
			}

			/*
				Check all the places where the sampler result is used can accept F16 format
				registers.
			*/
			if (!ReduceSampleResultPrecisionForSampler(psState, uSamplerIdx, IMG_TRUE))
			{
				continue;
			}

			/*
				Do the replacement.
			*/
			ReduceSampleResultPrecisionForSampler(psState, uSamplerIdx, IMG_FALSE);
			psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_F16;
		}
	}
}
#endif /* defined(OUTPUT_USCHW) */

typedef struct
{
	IMG_PUINT32		puGtzRegs;
	IMG_PUINT32		puLtoRegs;
} REMOVE_SATURATIONS_CONTEXT, *PREMOVE_SATURATIONS_CONTEXT;

#if defined(OUTPUT_USCHW)
/*****************************************************************************
 FUNCTION	: IsSaturatedSample

 PURPOSE    : Check if the result of a texture sample is known to be between 0 and 1.

 PARAMETERS	: psState		- Compiler state.
			  uSamplerIdx	- Sampler to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL IsSaturatedSample(PINTERMEDIATE_STATE	psState,
								  IMG_UINT32			uSamplerIdx)
{
	if (psState->asTextureUnpackFormat[uSamplerIdx].bNormalise)
	{
		IMG_UINT32			uChanIdx;
		PUNIFLEX_TEXFORM	psTexform = &psState->psSAOffsets->asTextureParameters[uSamplerIdx].sFormat;

		for (uChanIdx = 0; uChanIdx < CHANNELS_PER_INPUT_REGISTER; uChanIdx++)
		{
			USC_CHANNELFORM	eChannelForm = psTexform->peChannelForm[uChanIdx];

			if (eChannelForm != USC_CHANNELFORM_U8 &&
				eChannelForm != USC_CHANNELFORM_U16 &&
				eChannelForm != USC_CHANNELFORM_ZERO &&
				eChannelForm != USC_CHANNELFORM_ONE)
			{
				return IMG_FALSE;
			}
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}
#endif /* defined(OUTPUT_USCHW) */

/*****************************************************************************
 FUNCTION	: IsSaturatedArgument

 PURPOSE    : Check the known-bounds status of an instruction source.

 PARAMETERS	: psState		- Compiler state.
			  psCtx			- Per-stage context.
			  psArg			- Argument to check.
			  pbGtz			- Return the known bounds for the instruction argument.
			  pbLto

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID IsSaturatedArgument(PINTERMEDIATE_STATE				psState,
									PREMOVE_SATURATIONS_CONTEXT		psCtx,
									PARG							psArg,
									IMG_BOOL						*pbGtz,
									IMG_BOOL						*pbLto)
{
	PVR_UNREFERENCED_PARAMETER(psState);

	switch (psArg->uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			ASSERT(psArg->eFmt == UF_REGFORMAT_F16 || psArg->eFmt == UF_REGFORMAT_F32);
			*pbGtz = (GetBit(psCtx->puGtzRegs, psArg->uNumber)) ? IMG_TRUE : IMG_FALSE;
			*pbLto = (GetBit(psCtx->puLtoRegs, psArg->uNumber)) ? IMG_TRUE : IMG_FALSE;
			return;
		}	
		default:
		{
			*pbGtz = *pbLto = IMG_FALSE;
			return;
		}
	}
}

/*****************************************************************************
 FUNCTION	: UpdateSaturatedResult

 PURPOSE    : Update the known-bounds status of an instruction result.

 PARAMETERS	: psState		- Compiler state.
			  psCtx			- Per-stage context.
			  psInst		- Instruction to update the destination of.
			  bGtz			- Known bounds for the instruction result.
			  bLto

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID UpdateSaturatedResult(PINTERMEDIATE_STATE			psState,
									  PREMOVE_SATURATIONS_CONTEXT	psCtx,
									  PINST							psInst,
									  IMG_BOOL						bGtz,
									  IMG_BOOL						bLto)
{	
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psDest = &psInst->asDest[uDestIdx];

		if (psDest->uType == USEASM_REGTYPE_TEMP)
		{
			if (!NoPredicate(psState, psInst))
			{
				PARG	psOldDest = psInst->apsOldDest[uDestIdx];

				/*
					If the destination is conditionally updated then combine the greater-than-zero/less-than-one
					flags for the instruction result and for the old value of the destination.
				*/
				if (psOldDest != NULL)
				{
					if (!(psOldDest->uType == USEASM_REGTYPE_TEMP && GetBit(psCtx->puGtzRegs, psOldDest->uNumber)))
					{
						bGtz = IMG_FALSE;
					}
					if (!(psOldDest->uType == USEASM_REGTYPE_TEMP && GetBit(psCtx->puLtoRegs, psOldDest->uNumber)))
					{
						bLto = IMG_FALSE;
					}
				}
			}

			if (bGtz)
			{
				SetBit(psCtx->puGtzRegs, psDest->uNumber, 1);
			}
			else
			{
				SetBit(psCtx->puGtzRegs, psDest->uNumber, 0);
			}

			if (bLto)
			{
				SetBit(psCtx->puLtoRegs, psDest->uNumber, 1);
			}
			else
			{
				SetBit(psCtx->puLtoRegs, psDest->uNumber, 0);
			}
		}
	}
}

#if defined(SUPPORT_SGX545)
/*****************************************************************************
 FUNCTION	: TryRemoveSaturate_545

 PURPOSE    : Try to simplify a FMAXMIN instruction if it has one non-constant
			  source with known bounds.

 PARAMETERS	: psState		- Compiler state.
			  psCtx			- Per-stage context.
			  psInst		- Instruction to try and remove.
			  psNewMoveList	- If any new move or format conversion instructions
							where generated then they are appended to this list.

 RETURNS	: TRUE if the known bounds for the destination were updated.
*****************************************************************************/
static IMG_BOOL TryRemoveSaturate_545(PINTERMEDIATE_STATE			psState, 
									  PREMOVE_SATURATIONS_CONTEXT	psCtx,
									  PINST							psInst,
									  PUSC_LIST						psNewMoveList)
{
	IMG_BOOL	bGtz, bLto;

	/*
		Check for

			FMAXMIN		rDest, 0, rSrc, 1
	*/
	if (HasNegateOrAbsoluteModifier(psState, psInst, 1 /* uArgIdx */))
	{
		return IMG_FALSE;
	}
	if (
			!(
				psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
				psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
				!IsNegated(psState, psInst, 0 /* uArgIdx */)
			 )
	  )
	{
		return IMG_FALSE;
	}
	if (
			!(
				psInst->asArg[2].uType == USEASM_REGTYPE_FPCONSTANT &&
				psInst->asArg[2].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 &&
				!IsNegated(psState, psInst, 2 /* uArgIdx */)
			 )
	   )
	{
		return IMG_FALSE;
	}

	/*
		Check if the non-constant argument has known bounds.
	*/
	IsSaturatedArgument(psState, psCtx, &psInst->asArg[1], &bGtz, &bLto);

	if (bGtz && bLto)
	{
		/*
			FMAXMIN		rDest, 0, rSrc, 1

				->

			MOV			rDest, rSrc
		*/
		MoveSrcAndModifiers(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
		if (psInst->asArg[0].eFmt == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uSrcComponent;

			uSrcComponent = GetComponentSelect(psState, psInst, 0 /* uArgIdx */);
			SetOpcode(psState, psInst, IUNPCKF32F16);
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uSrcComponent);
		}
		else
		{
			SetOpcode(psState, psInst, IMOV);
		}
		AppendToList(psNewMoveList, &psInst->sTempListEntry);
	}
	else if (bLto)
	{
		/*
			FMAXMIN		rDest, 0, rSrc, 1

				->

			FMAX		rDest, 0, rSrc
		*/
		SetOpcode(psState, psInst, IFMAX);
	}
	else if (bGtz)
	{
		/*
			FMAXMIN		rDest, 0, rSrc, 1

				->

			FMIN		rDest, 1, rSrc
		*/
		SetOpcode(psState, psInst, IFMIN);
		SetSrc(psState, 
			   psInst, 
			   0 /* uSrcIdx */, 
			   USEASM_REGTYPE_FPCONSTANT, 
			   EURASIA_USE_SPECIAL_CONSTANT_FLOAT1, 
			   UF_REGFORMAT_F32);
	}
	
	/*
		Flag the result has having known bounds.
	*/
	UpdateSaturatedResult(psState, psCtx, psInst, IMG_TRUE, IMG_TRUE);

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX545) */

/*****************************************************************************
 FUNCTION	: TryRemoveSaturate

 PURPOSE    : Try to convert a MIN/MAX instruction to a MOV if it has one
			  non-constant argument with known bounds.

 PARAMETERS	: psState		- Compiler state.
			  psCtx			- Per-stage context.
			  psInst		- Instruction to try and remove.
			  psNewMoveList	- If any new moves where generated then they are
							appended to this list.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID TryRemoveSaturate(PINTERMEDIATE_STATE			psState, 
								  PREMOVE_SATURATIONS_CONTEXT	psCtx,
								  PINST							psInst,
								  PUSC_LIST						psNewMoveList)
{
	IMG_UINT32	uArg;
	IMG_UINT32	uConstant;
	IMG_BOOL	bGtz, bLto;
	
	if (psInst->eOpcode == IFMAX)
	{
		uConstant = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
	}
	else
	{
		uConstant = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
	}

	/*
		Check for a constant-bound.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psInst->asArg[uArg].uType == USEASM_REGTYPE_FPCONSTANT &&
			psInst->asArg[uArg].uNumber == uConstant &&
			!IsNegated(psState, psInst, uArg))
		{
			break;
		}
	}
	if (uArg == 2 || HasNegateOrAbsoluteModifier(psState, psInst, 1 - uArg))
	{
		UpdateSaturatedResult(psState, psCtx, psInst, IMG_FALSE, IMG_FALSE);
		return;
	}
	
	/*
		Check if the non-constant argument has known bounds.
	*/
	IsSaturatedArgument(psState, psCtx, &psInst->asArg[1 - uArg], &bGtz, &bLto);
	
	if (
			(bLto && psInst->eOpcode == IFMIN) ||
			(bGtz && psInst->eOpcode == IFMAX)
	   )
	{
		/*
			Convert the instruction to a move.
		*/
		if (uArg != 1)
		{
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 - uArg /* uCopyFromSrcIdx */);
		}
		if (psInst->asArg[0].eFmt == UF_REGFORMAT_F16)
		{
			SetOpcode(psState, psInst, IFMOV);
		}
		else
		{
			SetOpcode(psState, psInst, IMOV);
		}
		AppendToList(psNewMoveList, &psInst->sTempListEntry);
	}
	else
	{
		/*
			Flag that the result of the MIN/MAX has known bounds.
		*/
		if (psInst->eOpcode == IFMIN)
		{
			bLto = IMG_TRUE;
		}
		else
		{
			bGtz = IMG_TRUE;
		}
	}

	UpdateSaturatedResult(psState, psCtx, psInst, bGtz, bLto);
}

static IMG_BOOL TrySimplifyTest(PINTERMEDIATE_STATE			psState,
								PREMOVE_SATURATIONS_CONTEXT	psCtx,
								PINST						psInst)
/*****************************************************************************
 FUNCTION	: TrySimplifyTest

 PURPOSE    : Try to simplify a TEST instruction where the result of the tested
			  ALU operation has known bounds.

 PARAMETERS	: psState		- Compiler state.
			  psCtx			- Per-stage context.
			  psInst		- Instruction to try and remove.

 RETURNS	: TRUE if the TEST instruction was removed or simplified.
*****************************************************************************/
{
	IMG_BOOL	bGtz, bLto;
	PARG		psPredSrc;
	PARG		psPartialDest;
	IMG_BOOL	bFixedResult;

	/*
		Check for a TEST instruction for >= 0 or < 0.
	*/
	ASSERT(psInst->eOpcode == ITESTPRED);
	if (psInst->u.psTest->eAluOpcode != IFMOV)
	{
		return IMG_FALSE;
	}
	if (psInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}
	if (!(psInst->u.psTest->sTest.eType == TEST_TYPE_LT_ZERO || psInst->u.psTest->sTest.eType == TEST_TYPE_GTE_ZERO))
	{
		return IMG_FALSE;
	}
	/*
		Check the TEST result is always >= 0.
	*/
	if (IsNegated(psState, psInst, 0 /* uArgIdx */))
	{
		return IMG_FALSE;
	}
	IsSaturatedArgument(psState, psCtx, &psInst->asArg[0], &bGtz, &bLto);
	if (!bGtz)
	{
		return IMG_FALSE;
	}

	/*
		Simplify the TEST by making the argument a constant.
	*/
	bFixedResult = (psInst->u.psTest->sTest.eType == TEST_TYPE_LT_ZERO) ? IMG_FALSE : IMG_TRUE;
	SetOpcode(psState, psInst, IMOVPRED);
	SetSrc(psState, psInst, 0 /* uSrcIdx */, USC_REGTYPE_BOOLEAN, bFixedResult, UF_REGFORMAT_F32);

	/*
		Check if we can also remove the instruction's predicate.
	*/
	psPartialDest = psInst->apsOldDest[TEST_PREDICATE_DESTIDX];
	if (psInst->uPredCount == 1 && psPartialDest != NULL && EqualArgs(psInst->apsPredSrc[0], psPartialDest))
	{
		IMG_BOOL	bPredSrcNeg;
		
		psPredSrc = psInst->apsPredSrc[0];
		/*	
			Four possible cases:
				pS = (if pQ) ? F : pQ	-> !pQ && pQ		-> F
				pS = (if !pQ) ? F : pQ	-> pQ && pQ			-> pQ
				pS = (if pQ) T : pQ		-> pQ || pQ			-> pQ
				pS = (if !pQ) T : pQ	-> !pQ || pQ		-> T
		*/

		/* Get the fixed result of the test. */
		bFixedResult = (psInst->u.psTest->sTest.eType == TEST_TYPE_LT_ZERO) ? IMG_FALSE : IMG_TRUE;
		/* Get the negate flag on the predicate source. */
		bPredSrcNeg = (GetBit(psInst->auFlag, INST_PRED_NEG) ? IMG_TRUE : IMG_FALSE);

		if (bFixedResult == bPredSrcNeg)
		{
			SetSrc(psState, psInst, 0 /* uSrcIdx */, psPredSrc->uType, psPredSrc->uNumber, UF_REGFORMAT_F32);
		}
		ClearPredicates(psState, psInst);
		SetPartiallyWrittenDest(psState, psInst, TEST_PREDICATE_DESTIDX, NULL /* psPartialDest */);
	}

	return IMG_TRUE;
}

static
IMG_VOID FlagSaturatedPixelShaderInputs(PINTERMEDIATE_STATE			psState,
										PREMOVE_SATURATIONS_CONTEXT	psCtx)
/*****************************************************************************
 FUNCTION	: FlagSaturatedPixelShaderInputs

 PURPOSE    : For temporary registers which hold pixel shader inputs flag
			  those whose values are known to be between 0 and 1.

 PARAMETERS	: psState	- Compiler intermediate state
			  psCtx		- Stage state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psInputListEntry;

	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}

	for (psInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
		 psInputListEntry != NULL;
		 psInputListEntry = psInputListEntry->psNext)
	{
		PPIXELSHADER_INPUT		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
		PUNIFLEX_TEXTURE_LOAD	psTextureLoad = &psInput->sLoad;
		IMG_BOOL				bSaturated;

		bSaturated = IMG_FALSE;

		/*
			The result of iterating v0 or v1 is satured.
		*/
		if (
				psTextureLoad->uTexture == UNIFLEX_TEXTURE_NONE &&
				psTextureLoad->eIterationType == UNIFLEX_ITERATION_TYPE_COLOUR
		   )
		{
			bSaturated = IMG_TRUE;
		}

		#if defined(OUTPUT_USCHW)
		/*
			Check for a texture sample with a saturated result.
		*/
		if (
				psTextureLoad->uTexture != UNIFLEX_TEXTURE_NONE &&
				IsSaturatedSample(psState, psTextureLoad->uTexture)
		   )
		{
			bSaturated = IMG_TRUE;
		}
		#endif /* defined(OUTPUT_USCHW) */

		/*
			Mark the registers containing the pixel shader input as saturated.
		*/
		if (bSaturated)
		{
			IMG_UINT32	uRegIdx;

			for (uRegIdx = 0; uRegIdx < psInput->sLoad.uNumAttributes; uRegIdx++)
			{
				IMG_UINT32	uRegNum = psInput->psFixedReg->auVRegNum[uRegIdx];

				SetBit(psCtx->puLtoRegs, uRegNum, 1);
				SetBit(psCtx->puGtzRegs, uRegNum, 1);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID RemoveUnnecessarySaturationsBP(PINTERMEDIATE_STATE	psState,
										PCODEBLOCK			psBlock,
										IMG_PVOID			pvNull)
/*****************************************************************************
 FUNCTION	: RemoveUnnecessarySaturationsBP

 PURPOSE    : BLOCK_PROC, tries to remove unnecessary MIN and MAX instructions

 PARAMETERS	: psState	- Compiler intermediate state
			  psBlock	- Block to examine (call on all blocks in any order)
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: Nothing.
*****************************************************************************/
{
	REMOVE_SATURATIONS_CONTEXT	sCtx;
	PINST						psInst, psNextInst;
	USC_LIST					sNewMoveList;

	PVR_UNREFERENCED_PARAMETER(pvNull);

	InitializeList(&sNewMoveList);

	sCtx.puLtoRegs = UscAlloc(psState, UINTS_TO_SPAN_BITS(psState->uNumRegisters) * sizeof(IMG_UINT32));
	memset(sCtx.puLtoRegs, 0, UINTS_TO_SPAN_BITS(psState->uNumRegisters) * sizeof(IMG_UINT32));

	sCtx.puGtzRegs = UscAlloc(psState, UINTS_TO_SPAN_BITS(psState->uNumRegisters) * sizeof(IMG_UINT32));
	memset(sCtx.puGtzRegs, 0, UINTS_TO_SPAN_BITS(psState->uNumRegisters) * sizeof(IMG_UINT32));

	/*
		Flag temporary registers which hold saturated pixel shader inputs. These registers are never
		written inside the shader so we can do this for every block.
	*/
	FlagSaturatedPixelShaderInputs(psState, &sCtx);

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		switch (psInst->eOpcode)
		{	
			case IFMAX:
			case IFMIN:
			{
				TryRemoveSaturate(psState, &sCtx, psInst, &sNewMoveList);
				break;
			}
			#if defined(SUPPORT_SGX545)
			case IFMINMAX:
			{
				if (!TryRemoveSaturate_545(psState, &sCtx, psInst, &sNewMoveList))
				{
					/*
						Couldn't match the FMAXMIN instruction so flag the result
						as having unknown bounds.
					*/
					UpdateSaturatedResult(psState, &sCtx, psInst, IMG_FALSE, IMG_FALSE);
				}
				break;
			}
			#endif /* defined(SUPPORT_SGX545) */
			case IFMUL:
			{
				IMG_BOOL	bSrc0Gtz, bSrc0Lto;
				IMG_BOOL	bSrc1Gtz, bSrc1Lto;
				IMG_BOOL	bResultGtz, bResultLto;

				/*
					Check for possible bounds on the arguments.
				*/
				IsSaturatedArgument(psState, &sCtx, &psInst->asArg[0], &bSrc0Gtz, &bSrc0Lto);
				IsSaturatedArgument(psState, &sCtx, &psInst->asArg[1], &bSrc1Gtz, &bSrc1Lto);

				/*
					If the arguments are both >=0 (<=1) then flag the result as >=0 (<=1).
				*/
				bResultGtz = bResultLto = IMG_FALSE;
				if (!IsNegated(psState, psInst, 0 /* uArgIdx */) && !IsNegated(psState, psInst, 1 /* uArgIdx */))
				{
					bResultGtz = (bSrc0Gtz && bSrc1Gtz) ? IMG_TRUE : IMG_FALSE;
					bResultLto = (bSrc0Lto && bSrc1Lto) ? IMG_TRUE : IMG_FALSE;
				}
				UpdateSaturatedResult(psState, &sCtx, psInst, bResultGtz, bResultLto);
				break;
			}
			case ITESTPRED:
			{
				if (TrySimplifyTest(psState, &sCtx, psInst))
				{
					AppendToList(&sNewMoveList, &psInst->sTempListEntry);
				}
				else
				{
					UpdateSaturatedResult(psState, &sCtx, psInst, IMG_FALSE, IMG_FALSE);
				}
				break;
			}
			case IUNPCKF32U16:
			case IUNPCKF32U8:
			{
				/*
					Flag that the result of unpacking from an unsigned format is saturated.
				*/
				UpdateSaturatedResult(psState, &sCtx, psInst, IMG_TRUE, IMG_TRUE);
				break;
			}
			#if defined(OUTPUT_USCHW)
			case ISMP:
			case ISMPBIAS:
			case ISMPREPLACE:
			case ISMPGRAD:
			{
				IMG_BOOL	bSmpLto, bSmpGtz;
				
				/*
					Check for a sample from a texture which contains saturated values.
				*/
				bSmpLto = bSmpGtz = IMG_FALSE;
				if (IsSaturatedSample(psState, psInst->u.psSmp->uTextureStage))
				{
					bSmpLto = bSmpGtz = IMG_TRUE;
				}

				UpdateSaturatedResult(psState, &sCtx, psInst, bSmpLto, bSmpGtz);
				break;
			}
			#endif /* defined(OUTPUT_USCHW) */
			default:
			{
				/*
					For an unknown instruction mark the result as potentially unsaturated.
				*/
				UpdateSaturatedResult(psState, &sCtx, psInst, IMG_FALSE, IMG_FALSE);
				break;
			}
		}
	}

	UscFree(psState, sCtx.puGtzRegs);
	UscFree(psState, sCtx.puLtoRegs);

	/*
		Where we converted redundant saturations to moves: replace the move destination by the source.
	*/
	EliminateMovesFromList(psState, &sNewMoveList);
}

static
IMG_BOOL FindSecondPredicatedMove(PINTERMEDIATE_STATE	psState,
								  PINST					psFirstMoveInst,
								  PINST*				ppsSecondMoveInst,
								  IOPCODE				eMoveOpcode)
/******************************************************************************
 FUNCTION	: FindSecondPredicatedMove

 PURPOSE    : Look for a move instruction conditionally overwriting the destination
			  to another instruction.

 PARAMETERS	: psState			- Compiler state.
			  psFirstMoveInst	- First move instruction in the pair.
			  ppsSecondMoveInst	- On success returns a pointer to the second move
								instruction.
			  eMoveOpcode		- Expected opcode of a move instruction.

 RETURNS	: TRUE if a move instruction was found.
******************************************************************************/
{
	PINST		psSecondMoveInst;
	USEDEF_TYPE	eFirstMoveUseType;
	IMG_UINT32	uFirstMoveUseLocation;

	/*
		Check if the destination to the first move instruction is used in only
		one other instruction.
	*/
	if (!UseDefGetSingleUse(psState, 
							&psFirstMoveInst->asDest[0], 
							&psSecondMoveInst, 
							&eFirstMoveUseType, 
							&uFirstMoveUseLocation))
	{
		return IMG_FALSE;
	}

	/*
		Check the using instruction is a move.
	*/
	if (psSecondMoveInst->eOpcode != eMoveOpcode)
	{
		return IMG_FALSE;
	}

	/*
		Check the destination to the first move instruction is used as the source
		for a conditionally overwritten destination.
	*/
	if (eFirstMoveUseType != USE_TYPE_OLDDEST)
	{
		return IMG_FALSE;
	}
	if (uFirstMoveUseLocation != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the two instructions have mutually exclusive predicates.
	*/
	if (NoPredicate(psState, psSecondMoveInst))
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psFirstMoveInst))
	{
		IMG_UINT32	uPred;

		if (psFirstMoveInst->uPredCount != psSecondMoveInst->uPredCount)
		{
			return IMG_FALSE;
		}
		for (uPred = 0; uPred < psFirstMoveInst->uPredCount; uPred++)
		{
			PARG	psSecondMovePredSrc = psSecondMoveInst->apsPredSrc[uPred];
			PARG	psFirstMovePredSrc = psFirstMoveInst->apsPredSrc[uPred];

			if (psSecondMovePredSrc->uNumber != psFirstMovePredSrc->uNumber)
			{
				return IMG_FALSE;
			}
		}
		if (GetBit(psSecondMoveInst->auFlag, INST_PRED_NEG) == GetBit(psFirstMoveInst->auFlag, INST_PRED_NEG))
		{
			return IMG_FALSE;
		}
		if (GetBit(psSecondMoveInst->auFlag, INST_PRED_PERCHAN) != GetBit(psFirstMoveInst->auFlag, INST_PRED_PERCHAN))
		{
			return IMG_FALSE;
		}
	}

	*ppsSecondMoveInst = psSecondMoveInst;

	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID MovePartialDestToVectorSource(PINTERMEDIATE_STATE	psState,
									   PINST				psMoveToInst,
									   IMG_UINT32			uMoveToSlotIdx,
									   PINST				psMoveFromInst,
									   IMG_UINT32			uMoveFromDestIdx)
/******************************************************************************
 FUNCTION	: MovePartialDestToVectorSource

 PURPOSE    : Moves the partial destination from one instruction to a 
			  source to another.

 PARAMETERS	: psState			- Compiler state.
			  psMoveToInst		- Instruction to move to.
			  uMoveToSlotIdx	- Vector source to move to.
			  psMoveFromInst	- Instruction to move from.
			  uMoveFromDestIdx	- Partial destination to move.

 RETURNS	: Nothing.
******************************************************************************/
{
	MovePartialDestToSrc(psState, psMoveToInst, uMoveToSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR, psMoveFromInst, uMoveFromDestIdx);
	psMoveToInst->u.psVec->auSwizzle[uMoveToSlotIdx] = USEASM_SWIZZLE(X, Y, Z, W);
	psMoveToInst->u.psVec->aeSrcFmt[uMoveToSlotIdx] = psMoveToInst->asArg[uMoveToSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR].eFmt;
}

static
IMG_BOOL ConvertPredicatedVectorMoveToMovc(PINTERMEDIATE_STATE	psState,
										   PINST				psFirstMoveInst)
/******************************************************************************
 FUNCTION	: ConvertPredicatedVectorMoveToMovc

 PURPOSE    : Tries to convert a pair of predicated VMOV instructions
			  writing the same register to a single VMOVC.

 PARAMETERS	: psState			- Compiler state.
			  psFirstMoveInst	- First move instruction in the pair.

 RETURNS	: TRUE if the move instruction was converted.
******************************************************************************/
{
	IMG_BOOL			bPair;
	PINST				psPredicateWriterInst;
	PINST				psMovcInst;
	PINST				psSecondMoveInst;
	IMG_UINT32			uPredAsMaskRegNum;
	PINST				psTestMaskInst;
	IMG_UINT32			uSrc;
	IMG_UINT32			uTestMaskSwizzle;
	PINST				psPredicateSrcInst;
	IMG_UINT32			uChan;

	ASSERT(psFirstMoveInst->eOpcode == IVMOV);

	if (psFirstMoveInst->asDest[0].uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	/*
		Check the first move instruction doesn't also have a partial destination mask as
		well as a predicate.
	*/
	if ((psFirstMoveInst->auLiveChansInDest[0] & ~psFirstMoveInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Look for another move instruction with the destination of first move used as
		the source for unwritten channels in the second move's destination.
	*/
	if (FindSecondPredicatedMove(psState, psFirstMoveInst, &psSecondMoveInst, IVMOV))
	{
		/*
			Check the second move instruction doesn't have a partial destination write mask.
		*/
		if ((psSecondMoveInst->auLiveChansInDest[0] & ~psSecondMoveInst->auDestMask[0]) != 0)
		{
			return IMG_FALSE;
		}

		/*
			Check the source to the second move instruction can be safely moved
			backwards to the location of the first move instruction.
		*/
		if (IsNonSSAVectorSource(psState, psSecondMoveInst, 0 /* uSrc */))
		{
			return IMG_FALSE;
		}

		if (psSecondMoveInst->auDestMask[0] != psFirstMoveInst->auDestMask[0])
		{
			return IMG_FALSE;
		}

		bPair = IMG_TRUE;
	}
	else if (!NoPredicate(psState, psFirstMoveInst) && psFirstMoveInst->apsOldDest[0] != NULL)
	{
		bPair = IMG_FALSE;
		psSecondMoveInst = NULL;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Get USE-DEF information for the predicate source to the two MOV instructions.
	*/
	if (psSecondMoveInst != NULL)
	{
		psPredicateSrcInst = psSecondMoveInst;
	}
	else
	{
		psPredicateSrcInst = psFirstMoveInst;
	}
	if (GetBit(psPredicateSrcInst->auFlag, INST_PRED_PERCHAN))
	{
		IMG_UINT32	uPred;

		ASSERT(psPredicateSrcInst->uPredCount == VECTOR_LENGTH);

		psPredicateWriterInst = NULL;
		for (uPred = 0; uPred < psPredicateSrcInst->uPredCount; uPred++)
		{
			if ((psFirstMoveInst->auDestMask[0] & (1 << uPred)) != 0)
			{
				PARG			psPredicateSrc;
				PUSEDEF_CHAIN	psPredicateSrcUseDef;
				PINST			psChanPredicateWriterInst;
				IMG_UINT32		uChanPredicateWriterDestIdx;

				psPredicateSrc = psPredicateSrcInst->apsPredSrc[uPred];
				psPredicateSrcUseDef = psPredicateSrc->psRegister->psUseDefChain;

				/*	
					Get the instruction defining the predicate for this destination channel.
				*/
				psChanPredicateWriterInst = UseDefGetDefInstFromChain(psPredicateSrcUseDef, &uChanPredicateWriterDestIdx);
				if (psChanPredicateWriterInst == NULL)
				{
					return IMG_FALSE;
				}

				/*
					Check the ordering of the predicate sources to the VMOV and the predicate destinations
					to the VTEST are the same.
				*/
				if (uChanPredicateWriterDestIdx != uPred)
				{
					return IMG_FALSE;
				}

				/*
					Check all the instruction predicates are defined by the same instruction.
				*/
				if (psPredicateWriterInst != NULL && psPredicateWriterInst != psChanPredicateWriterInst)
				{
					return IMG_FALSE;
				}
				psPredicateWriterInst = psChanPredicateWriterInst;
			}
		}
	}
	else
	{
		PARG			psPredicateSrc;
		PUSEDEF_CHAIN	psPredicateSrcUseDef;

		ASSERT(psPredicateSrcInst->uPredCount == 1);
		psPredicateSrc = psPredicateSrcInst->apsPredSrc[0];
		psPredicateSrcUseDef = psPredicateSrc->psRegister->psUseDefChain;

		/*
			Find the TEST instruction that writes the predicate source for the MOV instructions.
		*/
		psPredicateWriterInst = UseDefGetDefInstFromChain(psPredicateSrcUseDef, NULL /* puDestIdx */);
		if (psPredicateWriterInst == NULL)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check if the TEST instruction is suitable for converting to VTESTMASK.	
	*/
	if (psPredicateWriterInst->eOpcode != IVTEST)
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psPredicateWriterInst))
	{
		return IMG_FALSE;
	}
	if (psPredicateWriterInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}

	if (GetBit(psPredicateSrcInst->auFlag, INST_PRED_PERCHAN))
	{
		if (psPredicateWriterInst->u.psVec->sTest.eChanSel != VTEST_CHANSEL_PERCHAN)
		{
			return IMG_FALSE;
		}
		uTestMaskSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
	}
	else
	{
		if (psPredicateWriterInst->u.psVec->sTest.eChanSel != VTEST_CHANSEL_XCHAN)
		{
			return IMG_FALSE;
		}
		uTestMaskSwizzle = USEASM_SWIZZLE(X, X, X, X);
	}

	/*
		Create a VMOVCU8_FLT instruction.
	*/
	psMovcInst = AllocateInst(psState, psFirstMoveInst);
	SetOpcode(psState, psMovcInst, IVMOVC);
	psMovcInst->u.psVec->eMOVCTestType = TEST_TYPE_NEQ_ZERO;
	psMovcInst->auDestMask[0] = psFirstMoveInst->auDestMask[0];
	psMovcInst->auLiveChansInDest[0] = psFirstMoveInst->auLiveChansInDest[0];
	if (bPair)
	{
		/*
			Convert
				(p0)	VMOV	D1, A
				(!p0)	VMOV	D1[=D2], B
			->
				VMOVC	MOV D1, .., A, B
		*/
		MoveDest(psState, psMovcInst, 0 /* uMoveToDestIdx */, psSecondMoveInst, 0 /* uMoveFromSrcIdx */);
		if (GetBit(psSecondMoveInst->auFlag, INST_PRED_NEG))
		{
			MoveVectorSource(psState, 
							 psMovcInst, 
							 2 /* uMoveToIdx */, 
							 psSecondMoveInst, 
							 0 /* uMoveFromIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
			MoveVectorSource(psState, 
							 psMovcInst, 
							 1 /* uMoveToIdx */, 
							 psFirstMoveInst, 
							 0 /* uMoveFromIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
		}
		else
		{
			MoveVectorSource(psState, 
							 psMovcInst, 
							 1 /* uMoveToIdx */, 
							 psSecondMoveInst, 
							 0 /* uMoveFromIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
			MoveVectorSource(psState, 
							 psMovcInst, 
							 2 /* uMoveToIdx */, 
							 psFirstMoveInst, 
							 0 /* uMoveFromIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
		}

		/*
			Insert the MOVC at the same point as the second move instruction.
		*/
		InsertInstBefore(psState, psSecondMoveInst->psBlock, psMovcInst, psSecondMoveInst);
	}
	else
	{
		/*
			Convert
				(p0)	VMOV D1[=B], A
			->
				VMOVC	D1, .., A, B
		*/
		MoveDest(psState, psMovcInst, 0 /* uMoveToDestIdx */, psFirstMoveInst, 0 /* uMoveFromSrcIdx */);
		if (GetBit(psFirstMoveInst->auFlag, INST_PRED_NEG))
		{
			MoveVectorSource(psState, 
							 psMovcInst, 
							 2 /* uMoveToIdx */, 
							 psFirstMoveInst, 
							 0 /* uMoveFromSrcIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
			MovePartialDestToVectorSource(psState, 
										  psMovcInst, 
										  1 /* uMoveToIdx */, 
										  psFirstMoveInst, 
										  0 /* uMoveFromDestIdx */);
		}
		else
		{
			MoveVectorSource(psState, 
							 psMovcInst, 
							 1 /* uMoveToIdx */, 
							 psFirstMoveInst, 
							 0 /* uMoveFromSrcIdx */, 
							 IMG_TRUE /* bMoveSourceModifier */);
			MovePartialDestToVectorSource(psState, 
										  psMovcInst, 
										  2 /* uMoveToIdx */, 
										  psFirstMoveInst, 
										  0 /* uMoveFromDestIdx */);
		}

		/*
			Insert the VMOVC at the same point as the first move instruction.
		*/
		InsertInstBefore(psState, psFirstMoveInst->psBlock, psMovcInst, psFirstMoveInst);
	}

	/*
		Get a temporary register for the VTESTMASK destination.
	*/
	uPredAsMaskRegNum = GetNextRegister(psState);

	/*
		Use the VTESTBYTEMASK result to chose one of the other two
		sources to the VMOVC.
	*/
	SetSrc(psState, psMovcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPredAsMaskRegNum, psMovcInst->asDest[0].eFmt);
	psMovcInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMovcInst->u.psVec->aeSrcFmt[0] = psMovcInst->asDest[0].eFmt;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		SetSrcUnused(psState, psMovcInst, 1 + uChan);
	}

	/*
		Copy the TEST instruction to a TESTMASK instruction.
	*/
	psTestMaskInst = CopyInst(psState, psPredicateWriterInst);
	ModifyOpcode(psState, psTestMaskInst, IVTESTMASK);
	SetDestCount(psState, psTestMaskInst, 1 /* uNewDestCount */);
	SetDest(psState, psTestMaskInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPredAsMaskRegNum, psMovcInst->asDest[0].eFmt);
	psTestMaskInst->auLiveChansInDest[0] = psMovcInst->auDestMask[0];
	psTestMaskInst->auDestMask[0] = psMovcInst->auDestMask[0];

	/*
		Where the VTEST instruction selected a particular channel which was tested to produce the predicate result -
		for the VTESTMASK instruction swizzle the sources to get the same effect.
	*/
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psTestMaskInst); uSrc++)
	{
		psTestMaskInst->u.psVec->auSwizzle[uSrc] = 
			CombineSwizzles(psTestMaskInst->u.psVec->auSwizzle[uSrc], uTestMaskSwizzle);
	}

	/*
		Setup the TESTMASK instruction to produce, per-channel, 1.0f if the test conditional is true and
		0.0f it is false.
	*/
	psTestMaskInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_INVALID;
	psTestMaskInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;

	/*
		Insert before the VTEST instruction in the block.
	*/
	InsertInstBefore(psState, psPredicateWriterInst->psBlock, psTestMaskInst, psPredicateWriterInst);

	/*
		Remove and free the move instructions.
	*/
	RemoveInst(psState, psFirstMoveInst->psBlock, psFirstMoveInst);
	FreeInst(psState, psFirstMoveInst);
	if (psSecondMoveInst)
	{
		RemoveInst(psState, psSecondMoveInst->psBlock, psSecondMoveInst);
		FreeInst(psState, psSecondMoveInst);
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_BOOL ConvertPredicatedMoveToMovc(PINTERMEDIATE_STATE	psState,
									 PINST					psFirstMoveInst)
/******************************************************************************
 FUNCTION	: ConvertPredicatedMoveToMovc

 PURPOSE    : Tries to convert a pair of predicated MOV instructions
			  writing the same register to a single MOVC.

 PARAMETERS	: psState			- Compiler state.
			  psFirstMoveInst	- First move instruction in the pair.

 RETURNS	: TRUE if the move instruction was converted.
******************************************************************************/
{
	IMG_BOOL		bPair;
	PARG			psPredicateSrc;
	PUSEDEF_CHAIN	psPredicateSrcUseDef;
	PUSEDEF			psPredicateDef;
	PINST			psPredicateWriterInst;
	PINST			psMovcInst;
	PINST			psSecondMoveInst;
	IMG_UINT32		uPredAsMaskRegNum;
	PINST			psTestMaskInst;
	IMG_UINT32		uArgumentCount;
	IMG_UINT32		uArg;

	ASSERT(psFirstMoveInst->eOpcode == IMOV);

	if (psFirstMoveInst->asDest[0].uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	if (psFirstMoveInst->asDest[0].eFmt == UF_REGFORMAT_C10)
	{
		return IMG_FALSE;
	}
	if (!(psFirstMoveInst->asArg[0].uType == USEASM_REGTYPE_TEMP ||
		  psFirstMoveInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE ||
		  psFirstMoveInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT))
	{
		return IMG_FALSE;
	}
	if (FindSecondPredicatedMove(psState, psFirstMoveInst, &psSecondMoveInst, IMOV))
	{
		ASSERT(psFirstMoveInst->auLiveChansInDest[0] == psSecondMoveInst->auLiveChansInDest[0]);
		bPair = IMG_TRUE;
	}
	else if (!NoPredicate(psState, psFirstMoveInst) && psFirstMoveInst->apsOldDest[0] != NULL)
	{
		bPair = IMG_FALSE;
		psSecondMoveInst = NULL;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Get USE-DEF information for the predicate source to the two MOV instructions.
	*/
	ASSERT((psFirstMoveInst == NULL  || psFirstMoveInst->uPredCount < 2) &&
		   (psSecondMoveInst == NULL || psSecondMoveInst->uPredCount < 2));

	if (psSecondMoveInst != NULL)
	{
		psPredicateSrc = psSecondMoveInst->apsPredSrc[0];
	}
	else
	{
		psPredicateSrc = psFirstMoveInst->apsPredSrc[0];
	}
	psPredicateSrcUseDef = UseDefGet(psState, psPredicateSrc->uType, psPredicateSrc->uNumber);

	/*
		Find the TEST instruction that writes the predicate source for the MOV instructions.
	*/
	ASSERT(psPredicateSrcUseDef->psDef != NULL);
	psPredicateDef = psPredicateSrcUseDef->psDef;
	if (psPredicateDef->eType != DEF_TYPE_INST)
	{
		return IMG_FALSE;
	}
	psPredicateWriterInst = psPredicateDef->u.psInst;
	
	/*
		Check if the TEST instruction is suitable for converting to TESTMASK.	
	*/
	if (psPredicateWriterInst->eOpcode != ITESTPRED)
	{
		return IMG_FALSE;
	}
	ASSERT(psPredicateDef->uLocation == TEST_PREDICATE_DESTIDX);
	if (!NoPredicate(psState, psPredicateWriterInst))
	{
		return IMG_FALSE;
	}
	if (psPredicateWriterInst->u.psTest->eAluOpcode == IFPADD8 || 
		psPredicateWriterInst->u.psTest->eAluOpcode == IFPSUB8)
	{
		return IMG_FALSE;
	}
	if (psPredicateWriterInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}

	/*
		Create a MOVC instruction.
	*/
	psMovcInst = AllocateInst(psState, psFirstMoveInst);
	SetOpcode(psState, psMovcInst, IMOVC_I32);
	psMovcInst->u.psMovc->eTest = TEST_TYPE_NEQ_ZERO;
	psMovcInst->auLiveChansInDest[0] = psFirstMoveInst->auLiveChansInDest[0];
	if (bPair)
	{
		/*
			Convert
				(p0)	MOV	D1, A
				(!p0)	MOV	D1[=D2], B
			->
				MOVC	MOV D1, .., A, B
		*/
		MoveDest(psState, psMovcInst, 0 /* uMoveToDestIdx */, psSecondMoveInst, 0 /* uMoveFromSrcIdx */);
		if (GetBit(psSecondMoveInst->auFlag, INST_PRED_NEG))
		{
			MoveSrc(psState, psMovcInst, 2 /* uMoveToIdx */, psSecondMoveInst, 0 /* uMoveFromIdx */);
			MoveSrc(psState, psMovcInst, 1 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromIdx */);
		}
		else
		{
			MoveSrc(psState, psMovcInst, 1 /* uMoveToIdx */, psSecondMoveInst, 0 /* uMoveFromIdx */);
			MoveSrc(psState, psMovcInst, 2 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromIdx */);
		}

		/*
			Insert the MOVC at the same point as the second move instruction.
		*/
		InsertInstBefore(psState, psSecondMoveInst->psBlock, psMovcInst, psSecondMoveInst);
	}
	else
	{
		/*
			Convert
				(p0)	MOV D1[=B], A
			->
				MOVC	D1, .., A, B
		*/
		MoveDest(psState, psMovcInst, 0 /* uMoveToDestIdx */, psFirstMoveInst, 0 /* uMoveFromSrcIdx */);
		if (GetBit(psFirstMoveInst->auFlag, INST_PRED_NEG))
		{
			MoveSrc(psState, psMovcInst, 2 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromSrcIdx */);
			MovePartialDestToSrc(psState, psMovcInst, 1 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromDestIdx */);
		}
		else
		{
			MoveSrc(psState, psMovcInst, 1 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromSrcIdx */);
			MovePartialDestToSrc(psState, psMovcInst, 2 /* uMoveToIdx */, psFirstMoveInst, 0 /* uMoveFromDestIdx */);
		}

		/*
			Insert the MOVC at the same point as the first move instruction.
		*/
		InsertInstBefore(psState, psFirstMoveInst->psBlock, psMovcInst, psFirstMoveInst);
	}

	/*
		Get a temporary register for the TESTMASK destination.
	*/
	uPredAsMaskRegNum = GetNextRegister(psState);

	/*
		Use the TESTMASK result to chose one of the other two
		sources to the MOVC.
	*/
	SetSrc(psState, psMovcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPredAsMaskRegNum, UF_REGFORMAT_F32);

	/*
		Copy the TEST instruction to a TESTMASK instruction.
	*/
	psTestMaskInst = AllocateInst(psState, psPredicateWriterInst);
	SetOpcode(psState, psTestMaskInst, ITESTMASK);
	SetDest(psState, psTestMaskInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPredAsMaskRegNum, UF_REGFORMAT_F32);

	/*
		Copy the TEST sources.
	*/
	uArgumentCount = psPredicateWriterInst->uArgumentCount;
	ASSERT(uArgumentCount == g_psInstDesc[ITESTMASK].uDefaultArgumentCount);
	for (uArg = 0; uArg < uArgumentCount; uArg++)
	{
		IMG_UINT32	uComponent;

		CopySrc(psState, psTestMaskInst, uArg, psPredicateWriterInst, uArg);

		uComponent = GetComponentSelect(psState, psPredicateWriterInst, uArg);
		SetComponentSelect(psState, 
						   psTestMaskInst, 
						   uArg, 
						   uComponent);
	}

	psTestMaskInst->u.psTest->eAluOpcode = psPredicateWriterInst->u.psTest->eAluOpcode;
	psTestMaskInst->u.psTest->sTest.eType = psPredicateWriterInst->u.psTest->sTest.eType;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psTestMaskInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psTestMaskInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
	}

	/*
		Insert before the TEST instruction in the block.
	*/
	InsertInstBefore(psState, psPredicateWriterInst->psBlock, psTestMaskInst, psPredicateWriterInst);

	/*
		Remove and free the move instructions.
	*/
	RemoveInst(psState, psFirstMoveInst->psBlock, psFirstMoveInst);
	FreeInst(psState, psFirstMoveInst);
	if (psSecondMoveInst)
	{
		RemoveInst(psState, psSecondMoveInst->psBlock, psSecondMoveInst);
		FreeInst(psState, psSecondMoveInst);
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID ConvertPredicatedMovesToMovc(PINTERMEDIATE_STATE	psState)
/******************************************************************************
 FUNCTION	: ConvertPredicatedMovesToMovcBlock

 PURPOSE    : Tries to convert pairs of predicated MOV instructions
			  writing the same register to a single MOVC.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
******************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;
	IMG_BOOL			bChanges;

	bChanges = IMG_FALSE;
	InstListIteratorInitialize(psState, IMOV, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psMOVInst;

		psMOVInst = InstListIteratorCurrent(&sIter);

		if (ConvertPredicatedMoveToMovc(psState, psMOVInst))
		{
			bChanges = IMG_TRUE;
		}
	}
	InstListIteratorFinalise(&sIter);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	InstListIteratorInitialize(psState, IVMOV, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psMOVInst;

		psMOVInst = InstListIteratorCurrent(&sIter);

		if (ConvertPredicatedVectorMoveToMovc(psState, psMOVInst))
		{
			bChanges = IMG_TRUE;
		}
	}
	InstListIteratorFinalise(&sIter);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (bChanges)
	{
		UseDefDropUnusedTemps(psState);

		TESTONLY_PRINT_INTERMEDIATE("Generate conditional moves", "movtomovc", IMG_TRUE);
	}
}

typedef struct _CONST_REGISTER
{
	/*
		Bank and number of the register.
	*/
	IMG_UINT32	uType;
	IMG_UINT32	uNumber;
	/*
		Number of references to this constant register.
	*/
	IMG_UINT32	uUseCount;
	/*
		Points to the MOV instruction which moves this constant
		register into a temporary.
	*/
	PINST		psMoveInst;
} CONST_REGISTER, *PCONST_REGISTER;

typedef struct _FIXBANK_GLOBALCTX
{
	/*
		Number of bit entries in the auTempsWithOutputHwType 
		array.
	*/
	IMG_UINT32		uNumTemps;
	/*
		Bit-array. For each temporary register the corresponding
		bit is set if this register will be mapped to an output
		register in the hardware code.
	*/
	IMG_PUINT32		auTempsWithOutputHwType;
} FIXBANK_GLOBALCTX, *PFIXBANK_GLOBALCTX;

typedef struct _FIXBANK_CTX
{
	/*
		Number of entries in the psConstRegisters array.
	*/
	IMG_UINT32		uNumConstRegisters;
	/*
		Information about each reference to a constant register
		in an instruction where its register bank isn't
		supported.
	*/
	PCONST_REGISTER	psConstRegisters;
	/*
		Set to TRUE on the first pass if there is an invalid
		register bank not fixed.
	*/
	IMG_BOOL		bNeedSecondPass;
} FIXBANK_CTX, *PFIXBANK_CTX;

static
IMG_UINT32 FixInvalidSourceBanks_GetCReg(PINTERMEDIATE_STATE	psState,
										 PFIXBANK_CTX			psCtx,
										 PARG					psArg)
/*****************************************************************************
 FUNCTION	: FixInvalidSourceBanks_GetCReg

 PURPOSE    : Get the structure that records references to a constant register
			  used in an instruction which doesn't support its register bank.

 PARAMETERS	: psState	- Compiler state.
			  psCtx		- Stage context.
			  psArg		- Argument to get the CONST_REGISTER structure for.

 RETURNS	: The index of the CONST_REGISTER structure.
*****************************************************************************/
{
	IMG_UINT32		uRegIdx;
	PCONST_REGISTER	psCReg;

	ASSERT(psArg->uType == USEASM_REGTYPE_SECATTR ||
		   psArg->uType == USEASM_REGTYPE_FPCONSTANT ||
		   psArg->uType == USEASM_REGTYPE_IMMEDIATE ||
		   psArg->uType == USEASM_REGTYPE_TEMP);

	/*
		Check for an existing reference to the constant register.
	*/
	for (uRegIdx = 0; uRegIdx < psCtx->uNumConstRegisters; uRegIdx++)
	{
		psCReg = &psCtx->psConstRegisters[uRegIdx];

		if (psCReg->uType == psArg->uType && psCReg->uNumber == psArg->uNumber)
		{
			return uRegIdx;
		}
	}

	/*
		Add another entry to the array.
	*/

	IncreaseArraySizeInPlace(psState,
				sizeof(psCtx->psConstRegisters[0]) * psCtx->uNumConstRegisters,
				sizeof(psCtx->psConstRegisters[0]) * (psCtx->uNumConstRegisters + 1),
				(IMG_PVOID*)&psCtx->psConstRegisters);

	/*
		Initialize the last entry to point to the register.
	*/
	psCReg = &psCtx->psConstRegisters[psCtx->uNumConstRegisters];
	psCReg->uType = psArg->uType;
	psCReg->uNumber = psArg->uNumber;
	psCReg->uUseCount = 0;
	psCReg->psMoveInst = NULL;

	psCtx->uNumConstRegisters++;

	return psCtx->uNumConstRegisters - 1;
}

static
IMG_BOOL IsValidSource(PINTERMEDIATE_STATE	psState,
					   PINST				psInst,
					   IMG_UINT32			uSrcIdx,
					   IMG_UINT32			uArgType,
					   IMG_UINT32			uArgNumber,
					   IMG_UINT32			uArgIndexType,
					   IMG_BOOL				bArgNegate,
					   IMG_BOOL				bArgAbs,
					   IMG_UINT32			uArgComponent)
/*****************************************************************************
 FUNCTION	: IsValidSource

 PURPOSE    : Check if an instruction source is valid.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check for.
			  uSrcIdx		- Index of the source to check.
			  uArgType		- Details of the argument to check.
			  uArgNumber
			  uArgIndexType
			  bArgNegate
			  bArgAbs
			  uArgComponent

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uHwType;

	uHwType = GetPrecolouredRegisterType(psState, psInst, uArgType, uArgNumber);

	/*
		Check if the register bank is valid.
	*/
	if (!CanUseSrc(psState, psInst, uSrcIdx, uHwType, uArgIndexType))
	{
		return IMG_FALSE;
	}
	/*
		Check if an immediate value is valid.
	*/
	if (uArgType == USEASM_REGTYPE_IMMEDIATE)
	{
		if (!IsImmediateSourceValid(psState, psInst, uSrcIdx, uArgComponent, uArgNumber))
		{
			return IMG_FALSE;
		}
	}
	/*
		Check if a source modifier is valid.
	*/
	if (!CanHaveSourceModifier(psState, psInst, uSrcIdx, bArgNegate, bArgAbs))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsValidSourceArg(PINTERMEDIATE_STATE		psState,
						  PINST						psInst,
						  IMG_UINT32				uSrcIdx,
						  PINST						psArgInst,
						  IMG_UINT32				uArgIdx)
/*****************************************************************************
 FUNCTION	: IsValidSourceArg

 PURPOSE    : Check if an instruction source is valid.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check for.
			  uSrcIdx		- Index of the source to check.
			  psArg			- Argument to check.
			  psArgMod		- Modifiers on the argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG					psArg = &psArgInst->asArg[uArgIdx];
	PFLOAT_SOURCE_MODIFIER	psArgMod;
	IMG_BOOL				bNegate;
	IMG_BOOL				bAbsolute;

	psArgMod = GetFloatMod(psState, psArgInst, uArgIdx);

	if (psArgMod != NULL)
	{
		bNegate = psArgMod->bNegate;
		bAbsolute = psArgMod->bAbsolute;
	}
	else
	{
		bNegate = IMG_FALSE;
		bAbsolute = IMG_FALSE;
	}

	return IsValidSource(psState,
						 psInst,
						 uSrcIdx,
						 psArg->uType,
						 psArg->uNumber,
						 psArg->uIndexType,
						 bNegate,
						 bAbsolute,
						 GetComponentSelect(psState, psInst, uArgIdx));
}

static
IMG_VOID AddMoveForInvalidSourceBank(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psBlock,	
									 PINST					psInst,
									 IMG_UINT32				uInstArg,
									 PCONST_REGISTER		psCReg)
/*****************************************************************************
 FUNCTION	: AddMoveForInvalidSourceBank

 PURPOSE    : Insert a MOV to fix an invalid source register bank used in
			  an instruction.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to add the MOV to.
			  psInst	- Instruction to insert the MOV before.
			  uInstArg	- Source for the MOV.
			  psCReg	- Either NULL or the structure for a reference
						to a constant register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uTemp;
	PINST			psMoveInst;
	PARG			psArg = &psInst->asArg[uInstArg];
	UF_REGFORMAT	eArgFmt = psArg->eFmt;

	if (psCReg == NULL || psCReg->psMoveInst == NULL)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			/*
				On cores with the vector instruction set a MOV instruction can't access hardware
				registers with these types/alignments.
			*/
			ASSERT(!(psArg->uType == USEASM_REGTYPE_FPINTERNAL && (psArg->uNumber % 4) != 0));
			ASSERT(!(psArg->uType == USEASM_REGTYPE_FPCONSTANT && (psArg->uNumber % 2) != 0));
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Get a temporary register for the MOV destination.
		*/
		uTemp = GetNextRegister(psState);

		/*
			Create the MOV instruction.
		*/
		psMoveInst = AllocateInst(psState, psInst);
		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE && psArg->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			SetOpcode(psState, psMoveInst, ILIMM);
		}
		else
		{
			SetOpcode(psState, psMoveInst, IMOV);
		}
		SetDest(psState, psMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp, eArgFmt);
		MoveSrc(psState, psMoveInst, 0 /* uCopyToIdx */, psInst, uInstArg);
		InsertInstBefore(psState, psBlock, psMoveInst, psInst);

		ASSERT(IsValidSourceArg(psState, psMoveInst, 0, psMoveInst, 0));

		if (psCReg != NULL)
		{
			psCReg->psMoveInst = psMoveInst;
		}
	}
	else
	{
		ASSERT(psCReg->psMoveInst->asDest[0].uType == USEASM_REGTYPE_TEMP);

		/*
			Reuse the destination of the existing MOV instruction.
		*/
		uTemp = psCReg->psMoveInst->asDest[0].uNumber;
	}

	/*
		Replace the instruction argument by the destination of the MOV.
	*/
	SetSrc(psState, psInst, uInstArg, USEASM_REGTYPE_TEMP, uTemp, eArgFmt);
}

static
IMG_UINT32 CheckForHardwareConstants(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 IMG_UINT32				uValue,
									 IMG_PBOOL				pbNegate)
/*****************************************************************************
 FUNCTION	: CheckForHardwareConstants

 PURPOSE    : Check for a hardware constant with the same value as an immediate.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uValue		- Value of the immediate to check for.
			  pbNegate		- On output returns TRUE if a hardware constant
							with the same value as the negation of the 
							immediate was found.

 RETURNS	: The index of the hardware constant with the same value as the
			  immediate or USC_UNDEF if no equivalent hardware constant exists.
*****************************************************************************/
{
	IMG_UINT32	uConstIdx;

	/*
		Check for a hardware constants with exactly the same value.
	*/
	uConstIdx = FindHardwareConstant(psState, uValue);
	if (uConstIdx != USC_UNDEF)
	{
		*pbNegate = IMG_FALSE;
		return uConstIdx;
	}

	/*
		Check for an instruction interprets the negate modifier as a FLOAT32 negate.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_FLOAT32NEG)
	{
		IMG_FLOAT	fValue;

		/*
			Get the immediate interpreted as float.
		*/
		fValue = *(IMG_PFLOAT)&uValue;
		
		if (fValue < 0.0f)
		{
			/*
				Check for a hardware constant with the same value as the
				negation of the immediate.
			*/
			fValue = -fValue;
			uConstIdx = FindHardwareConstant(psState, *(IMG_PUINT32)&fValue);
			if (uConstIdx != USC_UNDEF)
			{
				/*
					Use this hardware constant and set the negate source
					modifier on the argument.
				*/
				*pbNegate = IMG_TRUE;
				return uConstIdx;
			}
		}
	}
	
	return USC_UNDEF;
}

static
IMG_BOOL IsHwConstSourceValid(PINTERMEDIATE_STATE	psState, 
							  PINST					psInst, 
							  IMG_UINT32			uFinalArgIdx, 
							  IMG_UINT32			uOrigArgIdx,
							  IMG_UINT32			uConstIdx,
							  IMG_BOOL				bNegateConst)
/*****************************************************************************
 FUNCTION	: IsHwConstSourceValid

 PURPOSE    : Check if a hardware constant is valid as a source to an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uFinalArgIdx	- Source to check.
			  uOrigArgIdx	- Modifier for the source. 
			  uConstIdx		- Hardware constant number.
			  bNegateConst	- TRUE if a negate modifier must be used when
							substituting the hardware constant.

 RETURNS	: TRUE if the hardware constant is valid.
*****************************************************************************/
{
	PFLOAT_SOURCE_MODIFIER	psArgMod;
	IMG_BOOL				bArgNegate;
	IMG_BOOL				bArgAbsolute;

	psArgMod = GetFloatMod(psState, psInst, uOrigArgIdx);
	if (psArgMod != NULL)
	{
		bArgNegate = psArgMod->bNegate;
		if (bNegateConst)
		{
			bArgNegate = (!bArgNegate) ? IMG_TRUE : IMG_FALSE;
		}
		bArgAbsolute = psArgMod->bAbsolute;
	}
	else
	{
		ASSERT(!bNegateConst);
		bArgNegate = IMG_FALSE;
		bArgAbsolute = IMG_FALSE;
	}

	return IsValidSource(psState, 
						 psInst, 
						 uFinalArgIdx, 
						 USEASM_REGTYPE_FPCONSTANT, 
						 uConstIdx,
						 USC_REGTYPE_NOINDEX,
						 bArgNegate,
					     bArgAbsolute,
						 GetComponentSelect(psState, psInst, uOrigArgIdx));
}

static
IMG_BOOL ReplaceImmediateByHwConst(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: ReplaceImmediateByHwConst

 PURPOSE    : Check if an immediate source argument can be replaced by an
			  equivalent hardware constant.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  uArgIdx		- Argument index.

 RETURNS	: TRUE if the immediate was replaced.
*****************************************************************************/
{
	PARG		psArg = &psInst->asArg[uArgIdx];
	IMG_UINT32	uConstIdx;
	IMG_BOOL	bNegate;
	IMG_BOOL	bCommuteSources = IMG_FALSE;
	IMG_BOOL	bReplaceImmediate;

	/*
		Only replace immediate arguments.
	*/
	if (psArg->uType != USEASM_REGTYPE_IMMEDIATE)
	{
		return IMG_FALSE;
	}

	/*
		Check for a hardware constant with the same value as the immediate.
	*/
	uConstIdx = CheckForHardwareConstants(psState, psInst, psArg->uNumber, &bNegate);
	if (uConstIdx == USC_UNDEF)
	{
		return IMG_FALSE;
	}

	/*
		Check if a hardware constant can be used in this source.
	*/
	bReplaceImmediate = IMG_FALSE;
	if (IsHwConstSourceValid(psState, psInst, uArgIdx, uArgIdx, uConstIdx, bNegate))
	{
		bReplaceImmediate = IMG_TRUE;
	}
	/*
		If the first two source arguments to the instruction commute then check whether the existing source 1
		would be valid in source 0 and hardware constants are valid in source 1.
	*/
	else if (
				uArgIdx == 0 &&
				InstSource01Commute(psState, psInst) &&
				IsHwConstSourceValid(psState, psInst, 1 /* uFinalArgIdx */, 0 /* uOrigArgIdx */, uConstIdx, bNegate) &&
				IsValidSourceArg(psState, psInst, 0 /* uSrcIdx */, psInst, 1 /* uArgIdx */)
			)
	{
		bReplaceImmediate = IMG_TRUE;
		bCommuteSources = IMG_TRUE;
	}

	if (bReplaceImmediate)
	{
		/*
			Substitute the hardware constant.
		*/
		SetSrc(psState, psInst, uArgIdx, USEASM_REGTYPE_FPCONSTANT, uConstIdx, UF_REGFORMAT_F32);
		if (bNegate && !IsAbsolute(psState, psInst, uArgIdx))
		{
			InvertNegateModifier(psState, psInst, uArgIdx);
		}

		if (bCommuteSources)
		{
			CommuteSrc01(psState, psInst);
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL CollapseMultipleMoves(PARG psArg)
/*****************************************************************************
 FUNCTION	: CollapseMultipleMoves

 PURPOSE    : Check if the register bank of an instruction argument is one
			  where multiple occurances of the arguments can be replaced
			  by the result of a single move.

 PARAMETERS	: psArg		- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return ((
				psArg->uType == USEASM_REGTYPE_SECATTR ||
				psArg->uType == USEASM_REGTYPE_FPCONSTANT ||
				psArg->uType == USEASM_REGTYPE_IMMEDIATE ||
				psArg->uType == USEASM_REGTYPE_TEMP
			) &&
			psArg->uIndexType == USC_REGTYPE_NOINDEX) ? IMG_TRUE : IMG_FALSE;
}	

static
IMG_BOOL CouldAlsoReplaceSrc1(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CouldAlsoReplaceSrc1

 PURPOSE    : Check for an instruction where either source 0 or source 1 could be
			  replaced by a temporary register to give the instruction valid register banks.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check for an instruction where the first two sources can be swapped without
		affecting the instruction result.
	*/
	if (!InstSource01Commute(psState, psInst))
	{
		return IMG_FALSE;
	}
	/*
		Check if the existing source 0 would have a valid register bank and source modifiers if it
		was moved to source 1.
	*/
	if (!IsValidSourceArg(psState, psInst, 1 /* uSrcIdx */, psInst, 0 /* uArgIdx */))
	{
		return IMG_FALSE;
	}
	/*
		Check the source modifier on source 1 would be valid if it was moved to source 0.
	*/
	if (!CanHaveSourceModifier(psState, psInst, 0, IsNegated(psState, psInst, 1), IsAbsolute(psState, psInst, 1)))
	{
		return IMG_FALSE;
	}
	/*
		Check the type of source 1 is suitable for replacing multiple occurances of a source by the result of
		a single MOV.
	*/
	if (!CollapseMultipleMoves(&psInst->asArg[1]))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_VOID ReplaceSourcesByTemporaries(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psBlock,
									 PFIXBANK_CTX			psCtx,
									 PINST					psInst,
									 IMG_UINT32				uPass,
									 IMG_UINT32				uInvalidSourceMask)
/*****************************************************************************
 FUNCTION	: ReplaceSourcesByTemporaries

 PURPOSE    : Replace source arguments to an instruction by temporary registers
			  containing the same data.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  psCtx			- Stage context.
			  psInst		- Instruction to replace arguments to.
			  uPass
			  uInvalidSourceMask
							- Bit mask of source arguments to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		PARG	psArg = &psInst->asArg[uArg];

		if ((uInvalidSourceMask & (1U << uArg)) == 0)
		{
			continue;
		}

		/*
			Should be able to replace any source by a temporary register.
		*/
		ASSERT(CanUseSrc(psState, psInst, uArg, USEASM_REGTYPE_TEMP, USC_REGTYPE_NOINDEX));

		/*
			Replace unreferenced arguments by a temporary register but don't insert
			a MOV.
		*/
		if (GetLiveChansInArg(psState, psInst, uArg) == 0)
		{
			SetSrc(psState, psInst, uArg, USEASM_REGTYPE_TEMP, GetNextRegister(psState), psArg->eFmt);
			continue;
		}

		/*
			Is the invalid source bank one where we can try to collapse the MOVs needed for
			multiple instructions.
		*/
		if (CollapseMultipleMoves(psArg))
		{
			IMG_UINT32 uCReg1Idx = FixInvalidSourceBanks_GetCReg(psState, psCtx, psArg);
			PCONST_REGISTER	psCReg1;
			PCONST_REGISTER	psCReg2 = NULL;

			/*
				Could we also replace a different source by a temporary.
			*/
			if (uArg == 0 && CouldAlsoReplaceSrc1(psState, psInst))
			{
				IMG_UINT32	uCReg2Idx;

				uCReg2Idx = FixInvalidSourceBanks_GetCReg(psState, psCtx, &psInst->asArg[1]);
				psCReg2 = &psCtx->psConstRegisters[uCReg2Idx];
			}
			psCReg1 = &psCtx->psConstRegisters[uCReg1Idx];

			if (uPass == 0)
			{
				/*
					Record how frequently each constant is used in an argument where its
					register bank is supported.
				*/
				psCReg1->uUseCount++;
				if (psCReg2 != NULL)
				{
					psCReg2->uUseCount++;
				}
				psCtx->bNeedSecondPass = IMG_TRUE;
			}
			else
			{
				/*
					Replace the other source either if there is already a temporary register
					which contains its value or it is used more frequently in subsequent
					instructions.
				*/
				if (
						psCReg1->psMoveInst == NULL &&
						psCReg2 != NULL &&
						(
							psCReg2->psMoveInst != NULL ||
							psCReg2->uUseCount > psCReg1->uUseCount
						)
				   )
				{	
					CommuteSrc01(psState, psInst);

					AddMoveForInvalidSourceBank(psState, psBlock, psInst, 0 /* uInstArg */, psCReg2);
				}	
				else
				{
					AddMoveForInvalidSourceBank(psState, psBlock, psInst, uArg, psCReg1);
				}
			}
		}
		else
		{
			/*
				Add a MOV from the original source register to a temporary and replace
				the source by the temporary.
			*/
			ASSERT(uPass == 0);
			AddMoveForInvalidSourceBank(psState, psBlock, psInst, uArg, NULL);
		}
	}
}

typedef struct _CHECK_SOURCE_ARG_CONTEXT
{
	PFIXBANK_CTX	psCtx;
	IMG_PUINT32		puInvalidSourceMask;
} CHECK_SOURCE_ARG_CONTEXT, *PCHECK_SOURCE_ARG_CONTEXT;

static
IMG_VOID CheckSourceArg(PINTERMEDIATE_STATE	psState,
						PINST				psInst,
						IMG_BOOL			bDest,
					    IMG_UINT32			uGroupStart,
						IMG_UINT32			uGroupCount,
						HWREG_ALIGNMENT		eGroupAlign,
						IMG_PVOID			pvContext)
/*****************************************************************************
 FUNCTION	: CheckSourceArg

 PURPOSE    : Check whether a source argument to an instruction has a hardware
			  register bank supported by that instruction.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  bDest		- Ignored.
			  uGroupStart
			  uGroupCount
			  eGroupAlign
			  pvContext

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCHECK_SOURCE_ARG_CONTEXT	psContext = (PCHECK_SOURCE_ARG_CONTEXT)pvContext;
	PARG						psArg = &psInst->asArg[uGroupStart];

	PVR_UNREFERENCED_PARAMETER(bDest);

	/*
		Groups of arguments with special restrictions on the hardware register numbers assigned
		have already had their register banks checked (and if necessary fixed).
	*/
	if (uGroupCount > 1 || eGroupAlign != HWREG_ALIGNMENT_NONE)
	{
		return;
	}
	if (psArg->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		return;
	}
	if (!IsValidSourceArg(psState, psInst, uGroupStart, psInst, uGroupStart))
	{
		(*psContext->puInvalidSourceMask) |= (1U << uGroupStart);
	}
}

static
IMG_BOOL ReplaceConstantBySecAttr(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psBlock,
								  PINST					psInst,
								  IMG_UINT32			uArg)
/*****************************************************************************
 FUNCTION	: ReplaceConstantBySecAttr

 PURPOSE	: Replace an instruction argument which is a hardware constant or
			  an immediate by a secondary attribute with the same value.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Flow control block which contains the instruction.
			  psInst			- Instruction whose argument is to be replaced.
			  uArg				- Index of the argument to replace.

 RETURNS	: TRUE if the constant was replaced.
*****************************************************************************/
{
	PARG		psArg = &psInst->asArg[uArg];
	IMG_UINT32	uSAConstType;
	IMG_BOOL	bReplaceImmediate;
	IMG_BOOL	bCommuteSources;

	/*
		Within the SA-update program, SA-registers are aliased as PA's.	Any
		constants we add must be reffered to as a PA register in that case.
	*/
	if	(psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		uSAConstType = USEASM_REGTYPE_PRIMATTR;	
	}	
	else	
	{
		uSAConstType = USEASM_REGTYPE_SECATTR;
	}

	if (!(psArg->uType == USEASM_REGTYPE_FPCONSTANT || psArg->uType == USEASM_REGTYPE_IMMEDIATE))
	{
		return IMG_FALSE;
	}

	bReplaceImmediate = IMG_FALSE;
	bCommuteSources = IMG_FALSE;
	/*
		Check if secondary attributes are valid in this source argument.
	*/
	if (CanUseSrc(psState, psInst, uArg, uSAConstType, USC_REGTYPE_NOINDEX))
	{
		bReplaceImmediate = IMG_TRUE;
	}
	/*
		If the first two source arguments to the instruction commute then check whether the existing source 1
		would be valid in source 0 and secondary attribute are valid in source 1.
	*/
	else if (
				uArg == 0 &&
				InstSource01Commute(psState, psInst) &&
				CanUseSrc(psState, psInst, 1 /* uArg */, uSAConstType, USC_REGTYPE_NOINDEX) &&
				IsValidSourceArg(psState, psInst, 0 /* uSrcIdx */, psInst, 1 /* uArgIdx */)
			)
	{
		bReplaceImmediate = IMG_TRUE;
		bCommuteSources = IMG_TRUE;
	}
	
	if (bReplaceImmediate)
	{
		IMG_UINT32	uValue;

		ASSERT(psArg->uIndexType == USC_REGTYPE_NOINDEX);

		if (psArg->uType == USEASM_REGTYPE_FPCONSTANT)
		{
			uValue = GetHardwareConstantValueu(psState, psArg->uNumber);
		}
		else
		{
			ASSERT(psArg->uType == USEASM_REGTYPE_IMMEDIATE);
			uValue = psArg->uNumber;
		}

		if (AddStaticSecAttr(psState, uValue, NULL, NULL))
		{
			IMG_UINT32	 uSecAttrType, uSecAttrNum;
			UF_REGFORMAT eSecAttrFmt;

			/*	
				Add a new secondary attribute with the same value as the hardware constant/immediate.
			*/
			AddStaticSecAttr(psState, uValue, &uSecAttrType, &uSecAttrNum);

			/*
				Replace the argument by the secondary attribute.
			*/
			if (bCommuteSources)
			{
				CommuteSrc01(psState, psInst);
				uArg = 1;
			}

			eSecAttrFmt = psInst->asArg[uArg].eFmt;
			if (InstUsesF16FmtControl(psInst))
			{
				eSecAttrFmt = UF_REGFORMAT_F32;
			}
			if (HasC10FmtControl(psInst))
			{
				eSecAttrFmt = UF_REGFORMAT_U8;
			}

			SetSrc(psState, psInst, uArg, uSecAttrType, uSecAttrNum, eSecAttrFmt);

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID FixInvalidSourceBanksInst(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   PFIXBANK_CTX			psCtx,
								   PINST				psInst,
								   IMG_UINT32			uPass)
/*****************************************************************************
 FUNCTION	: FixInvalidSourceBanksInst

 PURPOSE    : Check and fix an invalid source register banks to an instruction.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block that contains the instruction.
			  psCtx		- Stage context.
			  psInst	- Instruction to fix.
			  uPass		- On pass 0 fix non-constant registers and record
						information about which constant registers are used
						in an instruction which doesn't support their register
						banks.
						On pass 1 fix constant registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32					uArg;
	IMG_UINT32					uInvalidSourceMask;
	CHECK_SOURCE_ARG_CONTEXT	sCtx;
	PCSOURCE_ARGUMENT_PAIR		psCommutableSources;
	SOP3_COMMUTE_MODE			eSOP3CommuteMode = SOP3_COMMUTE_MODE_INVALID;

	/*
		Check which sources to the instruction use unsupported register banks.
	*/
	uInvalidSourceMask = 0;
	sCtx.puInvalidSourceMask = &uInvalidSourceMask;
	sCtx.psCtx = psCtx;
	ProcessSourceRegisterGroups(psState, psInst, CheckSourceArg, (IMG_PVOID)&sCtx);

	/*
		Is there nothing to do.
	*/
	if (uInvalidSourceMask == 0)
	{
		return;
	}

	/*
		Can we avoid adding moves by swapping the sources to the instruction.
	*/
	if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3)
	{
		psCommutableSources = GetCommutableSOP3Sources(psState, psInst, &eSOP3CommuteMode);
	}
	else
	{
		psCommutableSources = g_psInstDesc[psInst->eOpcode].psCommutableSources;
	}
	if (
			psCommutableSources != NULL &&
			(uInvalidSourceMask & (1U << psCommutableSources->uFirstSource)) != 0 &&
			(uInvalidSourceMask & (1U << psCommutableSources->uSecondSource)) == 0 &&
			IsValidSourceArg(psState, psInst, psCommutableSources->uFirstSource, psInst, psCommutableSources->uSecondSource) &&
			IsValidSourceArg(psState, psInst, psCommutableSources->uSecondSource, psInst, psCommutableSources->uFirstSource)
		)
	{
		uInvalidSourceMask &= ~(1U << psCommutableSources->uFirstSource);

		if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3)
		{
			CommuteSOP3Sources(psState, psInst, eSOP3CommuteMode);
		}
		else
		{
			SwapInstSources(psState, psInst, psCommutableSources->uFirstSource, psCommutableSources->uSecondSource);	
		}
	}
	if (uInvalidSourceMask == 0)
	{
		return;
	}

	/*
		Can we avoid adding MOVs by replacing immediates by hardware constants.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (
				(uInvalidSourceMask & (1U << uArg)) != 0 && 
				ReplaceImmediateByHwConst(psState, psInst, uArg)
		   )
		{
			uInvalidSourceMask &= ~(1U << uArg);
		}
	}

	/*
		Can we avoid adding MOVs by replacing immediates/hardware constants by secondary
		attributes.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (
				(uInvalidSourceMask & (1U << uArg)) != 0 && 
				ReplaceConstantBySecAttr(psState, psBlock, psInst, uArg)
		   )
		{
			uInvalidSourceMask &= ~(1U << uArg);
		}
	}

	/*
		Check for cases where a floating point arithmetic instruction could be converted to the
		EFO instruction with supported source register banks.
	*/
	if (uInvalidSourceMask != 0 && ConvertInstToEfo(psState, psInst))
	{
		return;
	}

	/*
		Otherwise replace invalid sources by temporaries containing the same value.
	*/
	ReplaceSourcesByTemporaries(psState, psBlock, psCtx, psInst, uPass, uInvalidSourceMask);
}

static
IMG_VOID FixInvalidSourceBanksBP(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psBlock,
								 IMG_PVOID				pvNull)
/*****************************************************************************
 FUNCTION	: FixInvalidSourceBanksBP

 PURPOSE    : Check and fix an invalid source register banks to instructions
			  in a basic block.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	FIXBANK_CTX			sCtx;
	PINST				psInst;
	IMG_UINT32			uPass;

	PVR_UNREFERENCED_PARAMETER(pvNull);

	sCtx.bNeedSecondPass = IMG_FALSE;
	sCtx.uNumConstRegisters = 0;
	sCtx.psConstRegisters = NULL;

	for (uPass = 0; uPass < 2; uPass++)
	{
		PINST	psNextInst;
		for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
		{
			psNextInst = psInst->psNext;
			FixInvalidSourceBanksInst(psState, psBlock, &sCtx, psInst, uPass);
		}
		if (!sCtx.bNeedSecondPass)
		{
			break;
		}
	}

	if (sCtx.psConstRegisters != NULL)
	{
		UscFree(psState, sCtx.psConstRegisters);
	}
}

IMG_INTERNAL
IMG_VOID FixInvalidSourceBanks(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixInvalidSourceBanks

 PURPOSE    : Check and fix an invalid source register banks to instructions
			  in the program.

 PARAMETERS	: psState	- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	DoOnAllBasicBlocks(psState, ANY_ORDER, FixInvalidSourceBanksBP, IMG_FALSE, NULL /* pvUserData */);
}
#if 0
/***
 * Local Constant Propagation
 ***/

/* Data-type stored in the tree used for constant proopagation. */ 
typedef struct _CP_TREE_PAIR_
{
    PARG psFrom;
    PARG psTo;
    IMG_UINT32 uInstNum;
} CP_TREE_PAIR, *PCP_TREE_PAIR;

/*****************************************************************************
 FUNCTION	: CPTreeCmpFn

 PURPOSE	: Compare nodes in the copy propagation tree

 PARAMETERS	: pvLeft      - One of the nodes to be compared
              pvRight     - The other node to be compared

 OUTPUT     : None

 RETURNS	: Negative if pvLeft < pvRight, 0 if they are equal and positive
 			  if pvLeft > pvRight

*****************************************************************************/
static
IMG_INT32 CPTreeCmpFn(IMG_VOID* pvLeft, IMG_VOID* pvRight)
{
	/* Cast to the right types */
    PCP_TREE_PAIR psPair1 = NULL;
    PCP_TREE_PAIR psPair2 = NULL;

    psPair1 = (PCP_TREE_PAIR) pvLeft;
    psPair2 = (PCP_TREE_PAIR) pvRight;

    if(psPair1->psFrom->uType != psPair2->psFrom->uType)
    {
        return psPair1->psFrom->uType - psPair2->psFrom->uType;
    }

    if(psPair1->psFrom->uNumber != psPair2->psFrom->uNumber)
    {
        return psPair1->psFrom->uNumber - psPair2->psFrom->uNumber;
    }

    /* If everything (in the key) is the same, return 0 */
    return 0;
}

/*****************************************************************************
 FUNCTION	: BlockConstantPropagation

 PURPOSE	: Perform copy propagation at a basic block level

 PARAMETERS	: psState     - Compiler state
              psBlock     - Code block

 OUTPUT     : None

 RETURNS	: IMG_TRUE iff any change was made.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL BlockConstantPropagation(PINTERMEDIATE_STATE psState,
				   				  PCODEBLOCK psBlock)
{
    INST *psCurInst = NULL;
	PUSC_TREE psTree = NULL;
	USC_STACK* psNewRegStack = NULL;
	USC_COMPARE_FN pfnCmp = CPTreeCmpFn;
	IMG_BOOL bModified = IMG_FALSE;
    IMG_UINT32 uInstNum = 1;

	/* Set up a tree. */
	psTree = UscTreeMake(psState, sizeof(CP_TREE_PAIR), pfnCmp);

	/* Set up the stack of newly allocated registers */
	psNewRegStack = UscStackMake(psState, sizeof(PARG));

    /* Loop over all the instructions in the basic block */
    for (psCurInst = psBlock->psBody;
		 psCurInst != NULL;
		 psCurInst = GET_NEXT_INST(psCurInst))
	{        
        IMG_UINT32 i = 0;

		IMG_UINT32 uNumArgs = 0;
        IMG_UINT32 uNumRegs = 0;
        IMG_UINT32 uNumDests = 0;
        CP_TREE_PAIR sPair;
        
		sPair.psFrom = NULL;
        sPair.psTo = NULL;
        sPair.uInstNum = uInstNum;

        uNumArgs = g_psInstDesc[psCurInst->eOpcode].uArgumentCount;

        /* If there is also a predicated source, do another loop so we can deal with it */
        uNumRegs = uNumArgs;
        if(psCurInst->uPredSrc != USC_PREDREG_NONE)
        {
            uNumRegs += 1;
        }

		/*
          Loop over each of the arguments to the instruction looking for a
          replacement we can make.
        */
        for(i = 0; i < uNumRegs; i++)
        {
            /* Search the tree for a mapping from this register to a replacement one */
            CP_TREE_PAIR sTmpPair;
            PCP_TREE_PAIR psFromData = NULL;
            PCP_TREE_PAIR psToData = NULL;
			PARG psPredArg = NULL; 

            /* 
               If we have dealt with all the non-predicate arguments, deal with the
               predicate one (if one exists)
            */
            if(i == uNumArgs)
            {
				psPredArg = UscAlloc(psState, sizeof(ARG));

                psPredArg->uNumber = psCurInst->uPredSrc;
                psPredArg->uType = USEASM_REGTYPE_PREDICATE;

                sTmpPair.psFrom = psPredArg;
            }
            else
            {
                sTmpPair.psFrom = &psCurInst->asArg[i];
            }

            sTmpPair.psTo = NULL;
            psFromData = (PCP_TREE_PAIR)UscTreeGetPtr(psTree, &sTmpPair);
            sTmpPair.psFrom = NULL;

            if (psPredArg)
			{
				UscFree(psState, psPredArg);
			}

            /* 
              Now we have a mapping from psFrom, check when psTo was last written.
              If it is after psFrom then the replacement is invalid
            */
            if(psFromData)
            {
                sTmpPair.psFrom = psFromData->psTo;
                sTmpPair.psTo = NULL;

                /* 
                   It is possible for psFromData->psTo to be NULL if the only
                   node found is actually an invalid one. In which
                   case we do nothing. 
                */
                if(psFromData->psTo)
                {
                    psToData = UscTreeGetPtr(psTree, &sTmpPair);

                    /* 
                      If there is a valid node in the tree, check that it was written
                      before psFromData. If there isn't a matching node then we can
                      just make the substitution anyway, as all it means is that
                      there has not been a write of psFromData->psTo in the whole
                      block, let alone before psFromData was written to.
                    */
                    if(!psToData || psToData->uInstNum < psFromData->uInstNum)
                    {
                        /* If we find a replacement, update the instruction */
                        if(i == uNumArgs)
                        {
                            /* Update the predicate register */
                            psCurInst->uPredSrc = psFromData->psTo->uNumber;
                        }
                        else
                        {
                            psCurInst->asArg[i] = *psFromData->psTo;
                        } 
                        
                        bModified = IMG_TRUE;
                    }
                }
            }
        }

        /* 
           Now consider the destination(s). We need to mark any node which uses 
           these as invalid. To do this we simply add a special invalidating
           node to the tree, which will cause the nodes which use it to
           become invalid.
        */
    

        /*
           If the instruction is a move then sTmpPair->psTo will be filled with
           the source of the move. If it is not a move the sTmpPair->psTo will
           remain NULL
        */
        IsMove((PINST) psCurInst, &(sPair.psFrom), &(sPair.psTo));
        
        /* If there is also a predicated destination, do another loop so we can deal with it */
        uNumDests = psCurInst->uDestCount;
        if(psCurInst->uPredDest != USC_PREDREG_NONE)
        {
            uNumDests += 1;
        }      
        
        for(i = 0; i < uNumDests; i++)
        {
            PCP_TREE_PAIR psData = NULL;
            PARG psPredArg = NULL;
            
            /* 
               If we have dealt with all the non-predicate desinations, deal with the
               predicate one (if one exists)
            */
            if(i == uNumArgs)
            {
                psPredArg = UscAlloc(psState, sizeof(ARG)); 
                psPredArg->uNumber = psCurInst->uPredDest;
                psPredArg->uType = USEASM_REGTYPE_PREDICATE;

                sPair.psFrom = psPredArg;
            }
            else
            {
                sPair.psFrom = &psCurInst->asDest[i];
            }

            psData = UscTreeGetPtr(psTree, &sPair);

            /* If there is a valid mapping in the tree from this register, invalidate/replace it */
            if(psData)
            {
                if(sPair.psTo)
                {
                    psData->psTo = sPair.psTo;
                }
                else
                {
                    psData->psTo = NULL;
                }

				if (psPredArg)
				{
					UscFree(psState, psPredArg);
				}
            }
            else
            {
                /* Add the new node into the tree */
                UscTreeAdd(psState, psTree, &sPair);
				if (psPredArg)
				{
					UscStackPush(psState, psNewRegStack, &psPredArg);
				}
            }
		}

        /* Update the instruction count */
        uInstNum += 1;
    }

	/* Delete all created predicate registers */
	while (!UscStackEmpty(psNewRegStack))
	{
		PARG* ppsReg = (PARG*)(UscStackTop(psNewRegStack));
		PARG psPred = *ppsReg;

		UscStackPop(psState, psNewRegStack);
		UscFree(psState, psPred);
	}

	/* Delete the tree and stack*/
	UscTreeDelete(psState, psTree);
	UscStackDelete(psState, psNewRegStack);

    return bModified;
} 

/*****************************************************************************
 FUNCTION	: LocalConstPropBP

 PURPOSE	: LocalConstant propagation wrapper

 PARAMETERS	: psState     - USC Compiler state
              psBlock     - USC Code block

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID LocalConstPropBP(PINTERMEDIATE_STATE psUscState,
						  PCODEBLOCK psUscBlock,
						  IMG_PVOID pvData)
{
	PVR_UNREFERENCED_PARAMETER(pvData);

	BlockConstantPropagation((PINTERMEDIATE_STATE)psUscState, (PCODEBLOCK)psUscBlock);
}

/*****************************************************************************
 FUNCTION	: LocalConstProp

 PURPOSE	: Local constant propagation

 PARAMETERS	: psState     - USC Compiler state
              psBlock     - USC Code block

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID LocalConstProp(PINTERMEDIATE_STATE psUscState)
{
	DoOnAllBasicBlocks(psUscState, ANY_ORDER, LocalConstPropBP, IMG_FALSE, NULL);
}
#endif
/******************************************************************************
 End of file (iselect.c)
******************************************************************************/
