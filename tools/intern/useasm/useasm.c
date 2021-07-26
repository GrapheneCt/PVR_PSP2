/******************************************************************************
 * Name         : useasm.c
 * Title        : USE Compiler
 * Author       : John Russell
 * Created      : Jan 2002
 *
 * Copyright    : 2002-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: useasm.c $
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sgxsupport.h"

#include "sgxdefs.h"

#include "use.h"
#include "useasm.h"
#include "usetab.h"

#ifndef min
#define min(a,b) a>b?b:a
#endif
#ifndef max
#define max(a,b) a>b?a:b
#endif 


/* Zero and one sources. */
static const USE_REGISTER g_sSourceZero = {EURASIA_USE_SPECIAL_CONSTANT_ZERO, USEASM_REGTYPE_FPCONSTANT, 0, USEREG_INDEX_NONE};
static const USE_REGISTER g_sSourceOne = {EURASIA_USE_SPECIAL_CONSTANT_FLOAT1, USEASM_REGTYPE_FPCONSTANT, 0, USEREG_INDEX_NONE};
/*
static const USE_REGISTER g_sSourceImmZero = {0, USEASM_REGTYPE_IMMEDIATE, 0, USEREG_INDEX_NONE};
static const USE_REGISTER g_sSourceInt8One = {EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0, USEREG_INDEX_NONE};
static const USE_REGISTER g_sSourceInt16One = {EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, USEASM_REGTYPE_FPCONSTANT, 0, USEREG_INDEX_NONE};
static const USE_REGISTER g_sSourceImmOne = {0xFFFFFFFF, USEASM_REGTYPE_IMMEDIATE, 0, USEREG_INDEX_NONE};
*/

#if defined(USER)
#define USEASM_ERRMSG(X)			psContext->pfnAssemblerError X
#else /* defined(USER) */
#if 1/*defined(DEBUG)*/
#define USEASM_ERRMSG(X)			psContext->pfnAssemblerError(psContext->pvContext, NULL, NULL)
#else /* defined(DEBUG) */
#define USEASM_ERRMSG(X)			PVR_UNREFERENCED_PARAMETER(psContext)
#endif /* defined(DEBUG) */
#endif /* defined(USER) */

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 FUNCTION	: CheckIdentitySwizzle

 PURPOSE	: Check that a source to an instruction is an identity swizzle.

 PARAMETERS	: psInst			- Input instruction.
			  uArg				- Input argument which contains the swizzle.
			  bVec4				- TRUE if the instruction is vec4; otherwise it is vec3.
			  psContext			- USEASM context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID CheckIdentitySwizzle(PUSE_INST			psInst,
									 IMG_UINT32			uArg,
									 IMG_BOOL			bVec4,
									 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uSwizzle = psInst->asArg[uArg].uNumber;
	IMG_UINT32	uIdentitySwizzle = USEASM_SWIZZLE(X, Y, Z, W);

	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_SWIZZLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
	}

	if (!bVec4)
	{
		/*
			Don't compare the W channel.
		*/
		uSwizzle &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * 3));
		uIdentitySwizzle &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * 3));
	}

	if (uSwizzle != uIdentitySwizzle)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be an identity swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
	}
}

static IMG_VOID CheckVectorOrScalarIdentitySwizzle(PUSEASM_CONTEXT	psContext,
												  PUSE_INST			psInst,
												  IMG_UINT32		uInArg,
												  IMG_BOOL			bVector,
												  IMG_BOOL			bVec4)
/*****************************************************************************
 FUNCTION	: CheckVectorOrScalarIdentitySwizzle

 PURPOSE	: Checks that an input instruction argument uses an indentity
			  swizzle.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Input instruction.
			  uInArg			- Input argument to check.
			  bVector			- TRUE if the instruction is vector.
			  bVec4				- TRUE if the instruction is vec4.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (bVector)
	{
		CheckIdentitySwizzle(psInst, 
							 uInArg + 1,
							 bVec4,
							 psContext);
	}
	else
	{
		IMG_UINT32	uSrcComponent = 
			(psInst->asArg[uInArg].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;

		if (uSrcComponent != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"A component select is invalid for argument %d to this instruction", uInArg));
		}
	}
}

static IMG_BOOL CheckSwizzleArgument(PUSE_INST			psInst,
									 IMG_UINT32			uArg,
									 PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: CheckSwizzleArgument

 PURPOSE	: Checks if an input argument is a swizzle.

 PARAMETERS	: psInst			- Input instruction.
			  uArg				- Input argument which contains the swizzle.
			  psContext			- Assembler context.

 RETURNS	: TRUE if the argument is a swizzle.
*****************************************************************************/
{
	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_SWIZZLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
		return IMG_FALSE;
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle can't be accessed in indexed mode"));
		return IMG_FALSE;
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
		return IMG_FALSE;
	}
	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 FUNCTION	: SetLabelAddress

 PURPOSE	: Set the address of a label into an intsruction.

 PARAMETERS	: psContext			- Assembler callbacks.
			  uOp				- Type of instruction.
			  uOffset			- Offset of the label.
			  puCode			- Address of the instruction.
			  bSyncEnd			- Is this a label with the syncend modifier.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetLabelAddress(PCSGX_CORE_DESC psTarget,
								PUSEASM_CONTEXT psContext,
								PUSE_INST psInst,
								IMG_UINT32 uOp,
								IMG_UINT32 uOffset,
								IMG_UINT32 uCodeOffset,
								IMG_PUINT32 puBaseInst,
								IMG_PUINT32 puCode,
								IMG_BOOL bSyncEnd)
{
	IMG_UINT32	uPCMax = (1 << NumberOfProgramCounterBits(psTarget)) - 1;

	/* Used only for error messages. */
	PVR_UNREFERENCED_PARAMETER(psInst);

	if (uOp == LABEL_REF_BR)
	{
		if (!HasNoInstructionPairing(psTarget))
		{
			if (bSyncEnd && (uOffset & 1))
			{	
				USEASM_ERRMSG((psContext->pvContext,
							   psInst,
							   "The destination label for a branch with the syncend modifier "
							   "must be double instruction aligned "
							   "(try adding a .align2 directive before the label)"));
			}
		}
		uOffset -= (IMG_UINT32)((puCode - puBaseInst) / 2);

		if (((IMG_INT32)uOffset < -(IMG_INT32)((uPCMax + 1) / 2)) ||
			((IMG_INT32)uOffset >= (IMG_INT32)((uPCMax + 1) / 2)))
		{
			USEASM_ERRMSG((psContext->pvContext,
						   psInst,
						   "Branch destination is out of range "
						   "(it should be within the range [%d, %d])",
						   -(IMG_INT32)((uPCMax + 1) / 2),
						   (IMG_INT32)((uPCMax + 1) / 2)));
		}
	}
	else
	{
		uOffset += uCodeOffset;
		if (uOffset > uPCMax)
		{
			USEASM_ERRMSG((psContext->pvContext,
						   psInst,
						   "Branch destination is out of range "
						   "(it should be within the range [0, %d])",
						   uPCMax));
		}
		if (!HasNoInstructionPairing(psTarget))
		{
			if (bSyncEnd && (uOffset & 1))
			{
				USEASM_ERRMSG((psContext->pvContext,
							   psInst,
							   "The destination label for a branch with the syncend modifier "
							   "must be double instruction aligned "
							   "(try adding a .align2 directive before the label)"));
			}
		}
	}
	if (uOp == LABEL_REF_BA || uOp == LABEL_REF_BR ||
		uOp == LABEL_REF_UBA || uOp == LABEL_REF_UBR)
	{
		IMG_UINT32	uPCMask = (~uPCMax) << EURASIA_USE0_BRANCH_OFFSET_SHIFT;

		puCode[0] &= uPCMask;
		puCode[0] |= (uOffset << EURASIA_USE0_BRANCH_OFFSET_SHIFT) & ~uPCMask;
	}
	else if (uOp == LABEL_REF_LIMM ||
			 uOp == LABEL_REF_ULIMM)
	{
		puCode[1] &= (EURASIA_USE1_LIMM_IMM3126_CLRMSK & EURASIA_USE1_LIMM_IMM2521_CLRMSK);
		puCode[0] &= EURASIA_USE0_LIMM_IMML21_CLRMSK;
		puCode[0] |= (((uOffset >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT) &
					  ~EURASIA_USE0_LIMM_IMML21_CLRMSK);
		puCode[1] |= (((uOffset >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT) &
					  ~EURASIA_USE1_LIMM_IMM2521_CLRMSK);
		puCode[1] |= (((uOffset >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) &
					  ~EURASIA_USE1_LIMM_IMM3126_CLRMSK);

		/*
		  Notify interested parties of a LADDR label reference.
		*/
#if defined(USER)
		if (psContext->pfnLADDRNotify != NULL)
		{
			psContext->pfnLADDRNotify((IMG_UINT32)((puCode - puBaseInst) / 2UL));
		}
#endif /* defined(USER) */
	}
}

static IMG_BOOL FixupLabelAddresses(PCSGX_CORE_DESC psTarget,
									IMG_UINT32 uLabelAddress,
									IMG_UINT32 uLabel,
									IMG_PUINT32 puBaseInst,
									IMG_UINT32 uCodeOffset,
									PUSEASM_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: FixupLabelAddresses

 PURPOSE	: Fixes forward references to labels.

 PARAMETERS	: uLabelAddress			- The address of the label.
			  uLabel				- The number of the label.
			  psInst				- The label instruction.
			  puBaseInst			- The start of the instruction memory.
			  uCodeOffset			- Starting offset of the code.
			  ppsContext			- Data about referenced labels.

 RETURNS	: Nothing.
*****************************************************************************/
{
	LABEL_CONTEXT* psLabelState = (LABEL_CONTEXT*)psContext->pvLabelState;
	IMG_UINT32 i;
	IMG_UINT32 uOldSize;

	if (psLabelState == NULL)
	{
		return IMG_TRUE;
	}

	uOldSize = sizeof(LABEL_CONTEXT) + (psLabelState->uLabelReferenceCount - 1) * sizeof(LABEL_REFERENCE);

	for (i = 0; i < psLabelState->uLabelReferenceCount; )
	{
		if (psLabelState->psLabelReferences[i].uLabel == uLabel)
		{
			IMG_UINT32 uOffset = uLabelAddress + psLabelState->psLabelReferences[i].uLabelOffset;
			IMG_UINT32 uOp = psLabelState->psLabelReferences[i].uOp;
			IMG_PUINT32 puCode = psLabelState->psLabelReferences[i].puOffset;
			PUSE_INST psSrcInst = psLabelState->psLabelReferences[i].psInst;

			SetLabelAddress(psTarget, 
							psContext, 
							psSrcInst, 
							uOp, 
							uOffset, 
							uCodeOffset, 
							puBaseInst, 
							puCode, 
							psLabelState->psLabelReferences[i].bSyncEnd);

			psLabelState->psLabelReferences[i] = psLabelState->psLabelReferences[psLabelState->uLabelReferenceCount - 1];
			psLabelState->uLabelReferenceCount--;
		}
		else
		{
			i++;
		}
	}
	if (psLabelState->uLabelReferenceCount == 0)
	{
		(IMG_VOID)psContext->pfnRealloc(psContext->pvContext, psContext->pvLabelState, 0, uOldSize);
		psContext->pvLabelState = NULL;
	}
	else
	{
		IMG_UINT32 uCount;

		uCount = psLabelState->uLabelReferenceCount;
		psContext->pvLabelState = 
			psContext->pfnRealloc(psContext->pvContext, 
								  psContext->pvLabelState, 
								  sizeof(LABEL_CONTEXT) + (uCount - 1) * sizeof(LABEL_REFERENCE), 
							      uOldSize);
		if (psContext->pvLabelState == NULL)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL GetLabelAddress(PCSGX_CORE_DESC psTarget,
								PUSE_INST psInst,
								IMG_UINT32 uLabel,
								IMG_UINT32 uCodeOffset,
								IMG_PUINT32 puBaseOffset,
								IMG_PUINT32 puOffset,
								IMG_UINT32 uLabelOffset,
								IMG_UINT32 uOp,
								IMG_BOOL bSyncEnd,
								PUSEASM_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: GetLabelAddress

 PURPOSE	: Get the address of a label.

 PARAMETERS	: psInst			- Instruction which references to the label.
			  dwLabel			- Number of the label to get the address for.
			  pdwOffset			- Offset of the instruction which will branch to the label

 RETURNS	: Nothing..
*****************************************************************************/
{
	IMG_UINT32 uAddress;
	IMG_UINT32 uCount;
	LABEL_CONTEXT* psLabelState;

	if ((uAddress = psContext->pfnGetLabelAddress(psContext->pvContext, uLabel)) != USE_UNDEF)
	{
		uAddress += uLabelOffset;
		SetLabelAddress(psTarget, psContext, psInst, uOp,
						uAddress, uCodeOffset,
						puBaseOffset, puOffset, bSyncEnd);
		return IMG_TRUE;
	}

	if (psContext->pvLabelState == NULL)
	{
		uCount = 0;
		psContext->pvLabelState = psContext->pfnRealloc(psContext->pvContext,
														NULL,
														sizeof(LABEL_CONTEXT),
														0);
	}
	else
	{
		IMG_UINT32 uOldSize;

		uCount = ((LABEL_CONTEXT*)psContext->pvLabelState)->uLabelReferenceCount;
		uOldSize = sizeof(LABEL_CONTEXT) + (uCount - 1) * sizeof(LABEL_REFERENCE);

		psContext->pvLabelState = psContext->pfnRealloc(psContext->pvContext,
														psContext->pvLabelState, 
														uOldSize + sizeof(LABEL_REFERENCE),
														uOldSize);
	}
	if (psContext->pvLabelState == NULL)
	{
		return IMG_FALSE;
	}

	psLabelState = (LABEL_CONTEXT*)psContext->pvLabelState;
	psLabelState->psLabelReferences[uCount].uLabel = uLabel;
	psLabelState->psLabelReferences[uCount].puOffset = puOffset;
	psLabelState->psLabelReferences[uCount].psInst = psInst;
	psLabelState->psLabelReferences[uCount].uOp = uOp;
	psLabelState->psLabelReferences[uCount].bSyncEnd = bSyncEnd;
	psLabelState->psLabelReferences[uCount].uLabelOffset = uLabelOffset;
	psLabelState->uLabelReferenceCount = uCount + 1;
	return IMG_TRUE;
}

static IMG_VOID CheckArgFlags(PUSEASM_CONTEXT psContext,
							  PUSE_INST psInst,
							  IMG_UINT32 uArgument,
							  IMG_UINT32 uValidFlags)
/*****************************************************************************
 FUNCTION	: CheckArgFlags

 PURPOSE	: Check the flags supplied with an argument for validity.

 PARAMETERS	: psInst			- Instruction to check.
			  uArgument			- Argument to check.
			  uValidFlag		- Flags that are valid for this argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uInvalidFlags = psInst->asArg[uArgument].uFlags & ~uValidFlags;
	if (uInvalidFlags == 0)
	{
		return;
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_NEGATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".neg is invalid for argument %d to this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_ABSOLUTE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".abs is invalid for argument %d to this instruction", uArgument));
	}
	if (uInvalidFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A component select is invalid for argument %d to this instruction", uArgument));
	}
	if (uInvalidFlags & ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A bytemask is invalid for argument %d to this instruction", uArgument));
	}
	if ((uInvalidFlags & USEASM_ARGFLAGS_REQUIRESGPI2) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".requirespi2 is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_INVERT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".invert is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_LOW)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".low is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_HIGH)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".high is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_ALPHA)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".alpha is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".comp is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_DISABLEWB)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Disable writeback is invalid for argument %d for this instruction",
					   uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_FMTF32)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".flt32 is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_FMTF16)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".flt16 is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_FMTC10)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".c10 is invalid for argument %d for this instruction", uArgument));
	}
	if (uInvalidFlags & USEASM_ARGFLAGS_FMTU8)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".u8 is invalid for argument %d for this instruction", uArgument));
	}
	if ((uInvalidFlags & USEASM_ARGFLAGS_REQUIREMOE) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   ".requiremoe is invalid for argument %d for this instruction", uArgument));
	}
	if ((uInvalidFlags & USEASM_ARGFLAGS_NOT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "~ is invalid for argument %d for this instruction", uArgument));
	}
	if (uValidFlags & (USEASM_ARGFLAGS_FMTF32 |
					   USEASM_ARGFLAGS_FMTF16 |
					   USEASM_ARGFLAGS_FMTC10 |
					   USEASM_ARGFLAGS_FMTU8))
	{
		IMG_UINT32	uFmtFlags;

		uFmtFlags = psInst->asArg[uArgument].uFlags;
		uFmtFlags &= (USEASM_ARGFLAGS_FMTF32 | USEASM_ARGFLAGS_FMTF16 |
					  USEASM_ARGFLAGS_FMTC10 | USEASM_ARGFLAGS_FMTU8);
		switch (uFmtFlags)
		{
			case 0:
			case USEASM_ARGFLAGS_FMTF32:
			case USEASM_ARGFLAGS_FMTF16:
			case USEASM_ARGFLAGS_FMTC10:
			case USEASM_ARGFLAGS_FMTU8:
			{
				break;
			}
			default:
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "The modifiers .flt32, .flt16, .c10 and .u8 are mutually exclusive"));
				break;
			}
		}
	}
	if (uInvalidFlags & ~USEASM_ARGFLAGS_MAD16SWZ_CLRMSK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle is invalid for argument %d for this instruction", uArgument));
	}
}

static IMG_VOID CheckFlags(PUSEASM_CONTEXT psContext, 
						   PUSE_INST psInst, 
						   IMG_UINT32 uValidFlags1, 
						   IMG_UINT32 uValidFlags2,
						   IMG_UINT32 uValidFlags3)
/*****************************************************************************
 FUNCTION	: CheckFlags

 PURPOSE	: Check the flags supplied with an instruction for validity.

 PARAMETERS	: psInst			- Instruction to check.
			  uValidFlags1
			  uValidFlags2		
			  uValidFlags3		- Flags that are valid for this instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uInvalidFlags1 = psInst->uFlags1 & ~uValidFlags1;
	IMG_UINT32 uInvalidFlags2 = psInst->uFlags2 & ~uValidFlags2;
	IMG_UINT32 uInvalidFlags3 = psInst->uFlags3 & ~uValidFlags3;
	if (uInvalidFlags1 == 0 && uInvalidFlags2 == 0 && uInvalidFlags3 == 0)
	{
		return;
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_SKIPINVALID)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".skipinv is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".i8 is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_SYNCSTART)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".syncs is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".nosched is invalid for this instruction"));
	}
	if (uInvalidFlags1 & (~USEASM_OPFLAGS1_REPEAT_CLRMSK))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".repeat is invalid for this instruction"));
	}
	if (((uInvalidFlags1 & (~USEASM_OPFLAGS1_MASK_CLRMSK)) >> USEASM_OPFLAGS1_MASK_SHIFT) > 1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is invalid for this instruction"));
	}
	if (uInvalidFlags1 & (~USEASM_OPFLAGS1_PRED_CLRMSK))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A predicate is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_END)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".end is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_SYNCEND)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".synce is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_SAVELINK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".savelink is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_PARTIAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".partial is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_MOVC_FMTMASK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".mask is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_MONITOR)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".mon is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_ABS)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".abs is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_ROUNDENABLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".renable is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_MAINISSUE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction can't be part of a co-issued pair"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_MOVC_FMTF16)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".flt16 is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_PREINCREMENT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "preincrement is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_POSTINCREMENT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "postincrement is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_RANGEENABLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "rangeneable is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_TESTENABLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "testenable is invalid for this instruction"));
	}
	if (uInvalidFlags1 & USEASM_OPFLAGS1_FETCHENABLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".fetch is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_ONCEONLY)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".onceonly is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_BYPASSCACHE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".bpcache is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_FORCELINEFILL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".fcfill is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_TASKSTART)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".tasks is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_TASKEND)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".taske is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_SCALE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".scale is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_INCSGN)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "pre-/post-increment is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_SAT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".sat is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_MOVC_FMTI16)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".i16 is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_MOVC_FMTI32)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".i32 is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_MOVC_FMTF32)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".flt is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".i10 is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_TYPEPRESERVE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".tpres is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_C10)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".c10 is invalid for this instruction"));
	}
	if (uInvalidFlags2 & USEASM_OPFLAGS2_TOGGLEOUTFILES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".tog is invalid for this instruction"));
	}
	if (uInvalidFlags2 & (USEASM_OPFLAGS2_PERINSTMOE | 
						  USEASM_OPFLAGS2_PERINSTMOE_INCS0 |
						  USEASM_OPFLAGS2_PERINSTMOE_INCS1 |
						  USEASM_OPFLAGS2_PERINSTMOE_INCS2))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".incs is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_PCFF32)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".pcf is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_PCFF16)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".pcff16 is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_SAMPLEDATA)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".rsd is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_SAMPLEINFO)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".sinf is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_SYNCENDNOTTAKEN)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".syncend is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_TRIGGER)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".trigger is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_TWOPART)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".twopart is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_THREEPART)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".threepart is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_CUT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".cut is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_BYPASSL1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".bypassl1 is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_GPIEXT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".gpiext is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_GLOBAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".global is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_DM_NOMATCH)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".dm_nomatch is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_NOREAD)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".noread is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_PWAIT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".pwait is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_SAMPLEDATAANDINFO)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".irsd is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_MINPACK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".minp is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_SAMPLEMASK)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".smsk is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_FREEP)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".freep is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_ALLINSTANCES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".allinst is invalid for this instruction"));
	}
	if (uInvalidFlags3 & USEASM_OPFLAGS3_ANYINSTANCES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".anyinst is invalid for this instruction"));
	}
}

IMG_INTERNAL IMG_UINT32 FloatToCstIndex(IMG_FLOAT f)
/*****************************************************************************
 FUNCTION	: FloatToCstIndex

 PURPOSE	: Look for a hardware constant which matches a floating point value.

 PARAMETERS	: f				- Floating point value.

 RETURNS	: The index of the matching constant or -1 if none matched.
*****************************************************************************/
{
	IMG_UINT32 i;

	/*
		Return these constants in preference since the increment doesn't
		have to be set to zero in repeated instructions.
	*/
	if (f == 0)
	{
		return EURASIA_USE_SPECIAL_CONSTANT_ZERO;
	}
	if (f == 1)
	{
		return EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
	}

	for (i = 0; i < (sizeof(g_pfHardwareConstants) / sizeof(g_pfHardwareConstants[0])); i++)
	{
		if (f == g_pfHardwareConstants[i])
		{
			return i;
		}
	}

	return USE_UNDEF;
}

#if defined(USER)
static IMG_PCHAR GetRegisterTypeName(PUSE_REGISTER	psRegister)
/*****************************************************************************
 FUNCTION	: GetRegisterTypeName

 PURPOSE	: Get a string representing a register type.

 PARAMETERS	: psRegister		- Register to get the type name of.

 RETURNS	: The string representing the type.
*****************************************************************************/
{
	if (psRegister->uType >= 0 && psRegister->uType < USEASM_REGTYPE_MAXIMUM)
	{
		return pszTypeToString[psRegister->uType];
	}
	else
	{
		return "invalid";
	}
}
#endif /* defined(USER) */

typedef enum
{
	USEASM_HWARG_DESTINATION,
	USEASM_HWARG_SOURCE0,
	USEASM_HWARG_SOURCE1,
	USEASM_HWARG_SOURCE2,
} USEASM_HWARG;

#if defined(USER)
static const IMG_PCHAR g_apszHwArgName[] =
{
	"destination",	/* USEASM_HWARG_DESTINATION */
	"source 0",		/* USEASM_HWARG_SOURCE0 */
	"source 1",		/* USEASM_HWARG_SOURCE1 */
	"source 2",		/* USEASM_HWARG_SOURCE2 */
};
#endif /* defined(USER) */

static IMG_BOOL GPIRegistersUseTempBank(PCSGX_CORE_DESC	psTarget,
										USEASM_HWARG	eArgument,
										IMG_BOOL		bAllowExtended)
/*****************************************************************************
 FUNCTION	: GPIRegistersUseTempBank

 PURPOSE	: Checks if internal registers in an instruction argument are
			  encoded as temporary registers with large register numbers.

 PARAMETERS	: psTarget		- Compilation target.
			  eArgument		- Index of the argument.
			  bAllowExtended
							- TRUE if this instruction argument allows the
							extended bank set.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psTarget);

	#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
	/*
		This feature replaces the dedicated internal register bank.
	*/
	if (SupportsIndexTwoBanks(psTarget))
	{
		return IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */

	/*
		The destination, source 1 and source 2 have a dedicated internal
		register bank if the extended bank set is allowed.
	*/
	if (!bAllowExtended || eArgument == USEASM_HWARG_SOURCE0)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_UINT32 EncodeIndex(PUSEASM_CONTEXT psContext, PUSE_INST psInst, PUSE_REGISTER psRegister, IMG_UINT32 uFieldLength, PCSGX_CORE_DESC psTarget)
/*****************************************************************************
 FUNCTION	: EncodeIndex

 PURPOSE	: Generate the encoding for an indexed argument.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  psRegister		- Register to encode for.
			  uFieldLength		- Number of bits available for encoding the
								index.
			  psTarget			- Assembly target.			  

 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32	uEncodedNonOffset;
	IMG_UINT32	uBank;
	IMG_UINT32	uOffsetFieldLength;
	IMG_UINT32	uOffsetMax;

	/* Used only for error messages. */
	PVR_UNREFERENCED_PARAMETER(psInst);

	/*
		Convert the USEASM register type to the encoded index bank.
	*/
	switch (psRegister->uType)
	{
		case USEASM_REGTYPE_TEMP: uBank = EURASIA_USE_INDEX_BANK_TEMP; break;
		case USEASM_REGTYPE_OUTPUT: uBank = EURASIA_USE_INDEX_BANK_OUTPUT; break;
		case USEASM_REGTYPE_PRIMATTR: uBank = EURASIA_USE_INDEX_BANK_PRIMATTR; break;
		case USEASM_REGTYPE_SECATTR: uBank = EURASIA_USE_INDEX_BANK_SECATTR; break;
		default: 
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Register type %s is not indexable", 
				GetRegisterTypeName(psRegister))); 
			return 0;
		}
	}

	/*
		Set up the part of the field not corresponding to the register number.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
	if (SupportsIndexTwoBanks(psTarget))
	{
		uEncodedNonOffset = (uBank << SGXVEC_USE_INDEX_NONOFFSET_BANK_SHIFT);

		uOffsetFieldLength = uFieldLength - SGXVEC_USE_INDEX_NONOFFSET_NUM_BITS;
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
	{
		PVR_UNREFERENCED_PARAMETER(psTarget);

		uEncodedNonOffset = (uBank << EURASIA_USE_INDEX_NONOFFSET_BANK_SHIFT);
		if (psRegister->uIndex == USEREG_INDEX_H)
		{
			uEncodedNonOffset |= EURASIA_USE_INDEX_NONOFFSET_IDXSEL;
		}

		uOffsetFieldLength = uFieldLength - EURASIA_USE_INDEX_NONOFFSET_NUM_BITS;
	}

	/*
		Check the register number is in the range supported.
	*/
	uOffsetMax = (1 << uOffsetFieldLength) - 1;
	if (psRegister->uNumber > uOffsetMax)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Offset into indexed register bank is too large (maximum is %d)",
			uOffsetMax));
	}

	/*
		Combine the offset and non-offset parts of the encoded index.
	*/
	return (uEncodedNonOffset << uOffsetFieldLength) | psRegister->uNumber;
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_VOID SetVectorRegisterNumber(PUSEASM_CONTEXT	psContext,
										PUSE_INST		psInst,
										USEASM_HWARG	eHwArg,
										PUSE_REGISTER	psRegister,
										IMG_UINT32		uEncodedNumber,
										IMG_UINT32		uNumberFieldShift,
										IMG_PUINT32		puInst0)
/*****************************************************************************
 FUNCTION	: SetVectorRegisterNumber

 PURPOSE	: 

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  pszSourceName
			  psRegister		- Register to encode for.
			  uEncodedNumber	- Encoded register number to set.
			  uNumberFieldShift	- Offset of the register number field in the
								hardware instruction.
			  puInst0			- Points to the hardware instruction to encode.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(eHwArg);
	PVR_UNREFERENCED_PARAMETER(psInst);

	if (psRegister->uType == USEASM_REGTYPE_GLOBAL || 
		psRegister->uType == USEASM_REGTYPE_FPCONSTANT ||
		psRegister->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		/*
			In this case the MSB isn't decoded - we've already checked the register number is in the
			right range.
		*/
	}
	else
	{
		/*
			Check the register number has the required alignment.
		*/
		if (uEncodedNumber & ((1 << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) - 1))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "The register number for %s to %s must be a multiple of %d", 
						   g_apszHwArgName[eHwArg], OpcodeName(psInst->uOpcode),
						   1 << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT));
		}

		/*
			If the LSBs of the register number aren't present in the field 
			(rather than just ignored) then shift the register number down.
		*/
		uEncodedNumber >>= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
	}

	/*
		Set the register number into the instruction.
	*/
	*puInst0 |= (uEncodedNumber << uNumberFieldShift);
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

static IMG_VOID EncodeDestBank(PUSEASM_CONTEXT		psContext,
							   PUSE_INST			psInst,
							   IMG_BOOL				bAllowExtended,
							   IMG_UINT32			uBankExtension,
							   IMG_PUINT32			puInst1,
							   PCSGX_CORE_DESC		psTarget,
							   IMG_BOOL				bBankOnly,
							   PUSE_REGISTER		psRegister)
{
	IMG_UINT32	uBank;
	IMG_BOOL	bExtendedBank;
	IMG_BOOL	bInvalidDestBank;

	/* Used for error messages only. */
	PVR_UNREFERENCED_PARAMETER(psInst);

	bInvalidDestBank = IMG_FALSE;
	if (psRegister->uIndex != USEREG_INDEX_NONE)
	{
		if (bBankOnly)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Indexed registers aren't a valid destination type for %s", 
					OpcodeName(psInst->uOpcode)));
			return;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
		if (SupportsIndexTwoBanks(psTarget))
		{
			switch (psRegister->uIndex)
			{
				case USEREG_INDEX_L: 
				{
					uBank = SGXVEC_USE1_D1STDBANK_INDEXED_IL;
					bExtendedBank = IMG_FALSE;
					break;
				}
				case USEREG_INDEX_H: 
				{
					uBank = SGXVEC_USE1_D1EXTBANK_INDEXED_IH;
					bExtendedBank = IMG_TRUE;
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid index register"));
					return;
				}
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
		{
			uBank = EURASIA_USE1_D1STDBANK_INDEXED;
			bExtendedBank = IMG_FALSE;
		}
	}
	else
	{
		switch (psRegister->uType)
		{
			case USEASM_REGTYPE_TEMP: 
			{
				uBank = EURASIA_USE1_D1STDBANK_TEMP; 
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_OUTPUT: 
			{
				uBank = EURASIA_USE1_D1STDBANK_OUTPUT; 
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR: 
			{
				uBank = EURASIA_USE1_D1STDBANK_PRIMATTR;
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_SECATTR: 
			{
				uBank = EURASIA_USE1_D1EXTBANK_SECATTR;
				bExtendedBank = IMG_TRUE;
				break;
			}
			case USEASM_REGTYPE_INDEX: 
			{
				uBank = EURASIA_USE1_D1EXTBANK_INDEX; 
				bExtendedBank = IMG_TRUE;
				break;
			}
			case USEASM_REGTYPE_FPINTERNAL: 
			{
				if (GPIRegistersUseTempBank(psTarget, USEASM_HWARG_DESTINATION, bAllowExtended))
				{
					uBank = EURASIA_USE1_D1STDBANK_TEMP; 
					bExtendedBank = IMG_FALSE;
	
					if (bBankOnly)
					{
						bInvalidDestBank = IMG_TRUE;
					}
				}
				else
				{
					uBank = EURASIA_USE1_D1EXTBANK_FPINTERNAL; 
					bExtendedBank = IMG_TRUE;
				}
				break;
			}
			case USEASM_REGTYPE_GLOBAL:
			{
				if (bBankOnly)
				{
					bInvalidDestBank = IMG_TRUE;
				}
	
				uBank = EURASIA_USE1_D1EXTBANK_SPECIAL;
				bExtendedBank = IMG_TRUE;
				break;
			}
			case USEASM_REGTYPE_IMMEDIATE:
			case USEASM_REGTYPE_FPCONSTANT:
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Destination must be a modifiable register")); 
				return;
			}
			default: 
			{
				bInvalidDestBank = IMG_TRUE;

				uBank = USE_UNDEF;
				bExtendedBank = IMG_FALSE;
				break;
			}
		}
	}
		
	/*
		Are extended destination banks supported on this instruction?
	*/
	if (!bAllowExtended && bExtendedBank)
	{
		bInvalidDestBank = IMG_TRUE;
	}

	if (bInvalidDestBank)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s is not a valid destination type for %s", 
				GetRegisterTypeName(psRegister), OpcodeName(psInst->uOpcode))); 
	}

	*puInst1 |= (uBank << EURASIA_USE1_D1BANK_SHIFT);
	if (bExtendedBank)
	{
		*puInst1 |= uBankExtension;
	}
}

static IMG_BOOL IsValidSrc0Bank(PUSE_REGISTER		psRegister,
								IMG_PUINT32			puBank,
								IMG_PBOOL			pbExtendedBank)
/*****************************************************************************
 FUNCTION	: IsValidSrc0Bank

 PURPOSE	: Check if a register bank is valid for source 0 and gets the
			  encoded bank.

 PARAMETERS	: psRegister		- Register to check.
			  puBank			- If non-NULL returns the encoded bank.
			  pbExtendedBank	- If non-NULL returns the extended bank flag.

 RETURNS	: TRUE if the register bank is valid in source 0.
*****************************************************************************/
{
	IMG_UINT32	uBank;
	IMG_BOOL	bExtendedBank;

	/*
		Set default return values.
	*/
	if (puBank != NULL)
	{
		*puBank = USE_UNDEF;
	}
	if (pbExtendedBank != NULL)
	{
		*pbExtendedBank = IMG_FALSE;
	}


	/*
		Check for invalid banks.
	*/
	if (psRegister->uIndex != USEREG_INDEX_NONE)
	{
		return IMG_FALSE;
	}

	/*
		Translate the USEASM register type to the bank encoding for
		source 0.
	*/
	switch (psRegister->uType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			uBank = EURASIA_USE1_S0STDBANK_TEMP;
			bExtendedBank = IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			/*
				Internal registers are encoded as large temporary registers.
			*/
			uBank = EURASIA_USE1_S0STDBANK_TEMP;
			bExtendedBank = IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_PRIMATTR: 
		{
			uBank = EURASIA_USE1_S0STDBANK_PRIMATTR;
			bExtendedBank = IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_OUTPUT:
		{
			uBank = EURASIA_USE1_S0EXTBANK_OUTPUT;
			bExtendedBank = IMG_TRUE;
			break;
		}
		case USEASM_REGTYPE_SECATTR: 
		{
			uBank = EURASIA_USE1_S0EXTBANK_SECATTR;
			bExtendedBank = IMG_TRUE;
			break;
		}
		default: 
		{
			return IMG_FALSE;
		}
	}

	if (puBank != NULL)
	{
		*puBank = uBank;
	}
	if (pbExtendedBank != NULL)
	{
		*pbExtendedBank = bExtendedBank;
	}

	return IMG_TRUE;
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_BOOL IsValidSrc2MappingToSrc0Bank(PUSEASM_CONTEXT psContext, PUSE_INST psInst, PUSE_REGISTER psRegister)
/*****************************************************************************
 FUNCTION	: IsValidSrc2MappingToSrc0Bank

 PURPOSE	: Check if a register bank is valid for source 2 mapped to source 0
			  and prints an error message if it isn't.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction.
			  psRegister		- Register to check.

 RETURNS	: TRUE if the register bank is valid.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psInst);
	if (IsValidSrc0Bank(psRegister, NULL /* puBank */, NULL /* pbExtended */))
	{
		return IMG_TRUE;
	}
	if (psRegister->uType == USEASM_REGTYPE_FPCONSTANT)
	{
		return IMG_TRUE;
	}
	USEASM_ERRMSG((psContext->pvContext, 
				  psInst,
				  "For %s the register type of source 2 must be temporary, primary attribute, secondary attribute, output "
				  "or hardware constant",
				  OpcodeName(psInst->uOpcode)));
	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
					
static IMG_VOID EncodeSrc0Bank(PUSEASM_CONTEXT		psContext,
							   PUSE_INST			psInst,
							   IMG_BOOL				bAllowExtended,
							   IMG_PUINT32			puInst1,
							   IMG_UINT32			uBankExtension,
							   IMG_UINT32			uArg)
/*****************************************************************************
 FUNCTION	: EncodeSrc0Bank

 PURPOSE	: Encode the source 0 bank for an instruction.

 PARAMETERS	: psContext		- USEASM context.
			  psInst		- Instruction whose argument is to be encoded.
			  bAllowExtended
							- TRUE if extended source 0 banks are allowed for
							this instruction.
			  puInst1		- Points to the hardware instruction to be encoded.
			  uBankExtension
							- Bit position of the extended bank select flag.
			  uArg			- Index of the argument to be encoded.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSE_REGISTER	psRegister = &psInst->asArg[uArg];
	IMG_UINT32		uBank;
	IMG_BOOL		bExtendedBank;
	IMG_BOOL		bInvalidBank = IMG_FALSE;

	/* Used for error messages only. */
	PVR_UNREFERENCED_PARAMETER(psInst);

	/*
		Check if this is a valid register bank for source 0 and get
		the encoded bank.
	*/
	if (!IsValidSrc0Bank(psRegister, &uBank, &bExtendedBank))
	{
		bInvalidBank = IMG_TRUE;
	}

	/*
		Check for using the extended bank set in an instruction which doesn't
		support it.
	*/
	if (!bAllowExtended && bExtendedBank)
	{
		bInvalidBank = IMG_TRUE;
	}

	/*
		Print a message on an unsupported register bank.
	*/
	if (bInvalidBank)
	{
		if (psRegister->uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d can't be indexed", uArg));
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d type can't be %s for this instruction", 
				uArg, GetRegisterTypeName(psRegister)));
		}
	}

	/*
		Set the encoded bank into the instruction.
	*/
	*puInst1 |= (uBank << EURASIA_USE1_S0BANK_SHIFT);
	if (bExtendedBank)
	{
		*puInst1 |= uBankExtension;
	}
}

static IMG_VOID EncodeSrc12Bank(PUSEASM_CONTEXT	psContext,
								PUSE_INST		psInst,
								IMG_UINT32		uArg,
								IMG_BOOL		bAllowExtended,
								IMG_UINT32		uBankExtension,
								USEASM_HWARG	eHwArgument,
								IMG_PUINT32		puInst0,
								IMG_PUINT32		puInst1,
								PCSGX_CORE_DESC	psTarget,
								IMG_BOOL		bBankOnly)
/*****************************************************************************
 FUNCTION	: EncodeSrc12Bank

 PURPOSE	: Encode the source 1 or source 2 bank for an instruction.

 PARAMETERS	: psContext		- USEASM context.
			  psInst		- Instruction whose argument is to be encoded.
			  uArg			- Index of the argument to be encoded.
			  bAllowExtended
							- TRUE if extended source 1/2 banks are allowed for
							this instruction.
			  uBankExtension
							- Bit position of the extended bank select flag.
			  eHwArgument	- Index of the argument to encoded for.
			  puInst0		- Points to the hardware instruction to be encoded.
			  puInst1		
			  psTarget		- Assembly target.
			  bBankOnly		- TRUE if only the bank is to be encoded. The
							hardware argument doesn't have any associated
							register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSE_REGISTER	psRegister = &psInst->asArg[uArg];
	IMG_UINT32		uBank;
	IMG_BOOL		bExtendedBank;
	IMG_BOOL		bInvalidBank = IMG_FALSE;

	if (psRegister->uIndex != USEREG_INDEX_NONE)
	{
		if (bBankOnly)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d can't indexed", uArg));
			return;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS)
		if (SupportsIndexTwoBanks(psTarget))
		{
			switch (psRegister->uIndex)
			{
				case USEREG_INDEX_L: 
				{
					uBank = SGXVEC_USE0_S1EXTBANK_INDEXED_IL;
					bExtendedBank = IMG_TRUE;
					break;
				}
				case USEREG_INDEX_H: 
				{
					uBank = SGXVEC_USE0_S1EXTBANK_INDEXED_IH;
					bExtendedBank = IMG_TRUE;
					break;
				}
				default: IMG_ABORT();
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_TWO_INDEX_BANKS) */
		{
			uBank = EURASIA_USE0_S1EXTBANK_INDEXED;
			bExtendedBank = IMG_TRUE;
		}
	}
	else
	{
		switch (psRegister->uType)
		{
			case USEASM_REGTYPE_TEMP:
			{	
				uBank = EURASIA_USE0_S1STDBANK_TEMP;
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_OUTPUT:
			{
				uBank = EURASIA_USE0_S1STDBANK_OUTPUT;
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				uBank = EURASIA_USE0_S1STDBANK_PRIMATTR;
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_SECATTR:
			{
				uBank = EURASIA_USE0_S1STDBANK_SECATTR;
				bExtendedBank = IMG_FALSE;
				break;
			}
			case USEASM_REGTYPE_FPCONSTANT:
			{
				uBank = EURASIA_USE0_S1EXTBANK_SPECIAL;
				bExtendedBank = IMG_TRUE;
				break;
			}
			case USEASM_REGTYPE_FPINTERNAL:
			{
				if (GPIRegistersUseTempBank(psTarget, eHwArgument, bAllowExtended))
				{
					if (bBankOnly)
					{
						bInvalidBank = IMG_TRUE;
					}
	
					uBank = EURASIA_USE0_S1STDBANK_TEMP;
					bExtendedBank = IMG_FALSE;
				}
				else
				{
					uBank = EURASIA_USE0_S1EXTBANK_FPINTERNAL;
					bExtendedBank = IMG_TRUE;
				}
				break; 
			}
			case USEASM_REGTYPE_IMMEDIATE:
			{
				uBank = EURASIA_USE0_S1EXTBANK_IMMEDIATE;
				bExtendedBank = IMG_TRUE;
				break; 
			}
			case USEASM_REGTYPE_GLOBAL:
			{
				if (bBankOnly)
				{
					bInvalidBank = IMG_TRUE;
				}
				uBank = EURASIA_USE0_S1EXTBANK_SPECIAL;
				bExtendedBank = IMG_TRUE;
				break;
			}
			default:
			{
				bInvalidBank = IMG_TRUE;
				uBank = USE_UNDEF;
				bExtendedBank = IMG_FALSE;
				break;
			}
		}
	}

	if (!bAllowExtended && bExtendedBank)
	{
		bInvalidBank = IMG_TRUE;
	}

	if (bInvalidBank)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d type can't be %s", uArg, GetRegisterTypeName(psRegister)));
	}

	switch (eHwArgument)
	{
		case USEASM_HWARG_SOURCE1: 
		{
			*puInst0 |= (uBank << EURASIA_USE0_S1BANK_SHIFT);
			break;
		}
		case USEASM_HWARG_SOURCE2: 
		{
			*puInst0 |= (uBank << EURASIA_USE0_S2BANK_SHIFT);
			break;
		}
		default: IMG_ABORT();
	}
	if (bExtendedBank)
	{
		*puInst1 |= uBankExtension;
	}
}

static IMG_UINT32 CheckAndEncodeGlobalRegisterNumber(PUSEASM_CONTEXT	psContext,
													 PUSE_INST			psInst,
													 PUSE_REGISTER		psRegister,
													 USEASM_HWARG		eArgument,
													 IMG_BOOL			bBitwise,
													 IMG_UINT32			uNumberFieldLength,
													 PCSGX_CORE_DESC		psTarget)
/*****************************************************************************
 FUNCTION	: CheckAndEncodeGlobalRegisterNumber

 PURPOSE	: Check special restrictions on global registers and return the
			  encoded form of the register number.

 PARAMETERS	: psContext			- USEASM context.
			  psInst			- Instruction whose argument is being encoded.
			  psRegister		- Register to encode.
			  eArgument			- Index of the hardware argument to encode for.
			  bBitwise			- TRUE if instruction is a bitwise instrction.

 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32	uRegisterNumberMax;

	/* Used for error messages only. */
	PVR_UNREFERENCED_PARAMETER(psInst);

	/*
		The global register bank is encoded as the SPECIAL register bank with the 7th bit of the
		register number selecting between the global registers and the constant registers. So check the
		register number field is wide enough to include the 7th bit,
	*/
	if (EURASIA_USE_SPECIAL_INTERNALDATA >= (1U << uNumberFieldLength))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s type can't be global for this instruction", 
			g_apszHwArgName[eArgument]));
	}

	uRegisterNumberMax = min(EURASIA_USE_GLOBAL_BANK_SIZE, 1U << uNumberFieldLength);
	if (psRegister->uNumber >= uRegisterNumberMax)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid global register specified (g%d)", psRegister->uNumber));
	}

	/*
		Check special restrictions on where specific global registers can be accessed.
	*/
	if (
			eArgument != USEASM_HWARG_SOURCE2 &&
			eArgument != USEASM_HWARG_DESTINATION &&
			(
				psRegister->uNumber >= EURASIA_USE_SPECIAL_BANK_TASKQUEUEDATA0 &&
				psRegister->uNumber <= EURASIA_USE_SPECIAL_BANK_TASKQUEUEDATA6
			)
	   )
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Task queue data (g%d to g%d) can be accessed in src2 only",
				EURASIA_USE_SPECIAL_BANK_TASKQUEUEDATA0, EURASIA_USE_SPECIAL_BANK_TASKQUEUEDATA6));
	}

	if (!bBitwise && IsInvalidSpecialRegisterForNonBitwiseInstructions(psTarget, psRegister->uNumber))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "g%d can only "
				"be accessed in bitwise instruction", 
				psRegister->uNumber));
	}

	return psRegister->uNumber | EURASIA_USE_SPECIAL_INTERNALDATA;
}

static IMG_UINT32 CheckSignedImmediate(PUSEASM_CONTEXT	psContext,
									   PUSE_INST		psInst,
									   IMG_UINT32		uImm,
									   IMG_PCHAR		pszSrc,
									   IMG_UINT32		uNumberFieldSize)
/*****************************************************************************
 FUNCTION	: CheckSignedImmediate

 PURPOSE	: Check an immediate is in the right range for a source interpreted
			  as signed.

 PARAMETERS	: psContext					- USEASM context.
			  uImm						- Immediate value.

 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32	uImmMask = (1 << uNumberFieldSize) - 1;
	IMG_INT32	iMinImm = -(IMG_INT32)(1 << (uNumberFieldSize - 1));
	IMG_INT32	iMaxImm = (IMG_INT32)((1 << (uNumberFieldSize - 1)) - 1);

	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(pszSrc);

	if ((IMG_INT32)uImm > iMaxImm)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s immediate value is too large (was %d; maximum %d)",
					pszSrc, uImm, iMinImm));
	}
	else if ((IMG_INT32)uImm < iMinImm)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s immediate value is too small (was %d; minimum -%d)",
					pszSrc, uImm, -iMaxImm));
	}
	return uImm & uImmMask;
}

static IMG_UINT32 CheckAndEncodeImmediate(PUSEASM_CONTEXT	psContext,
										  PUSE_INST			psInst,
										  IMG_BOOL			bSigned,
										  PUSE_REGISTER		psRegister,
										  IMG_UINT32		uNumberFieldSize)
{
	if (bSigned)
	{
		return CheckSignedImmediate(psContext, psInst, psRegister->uNumber, "Source 1", uNumberFieldSize);
	}
	else
	{
		IMG_UINT32	uMaxImmediate = (1 << uNumberFieldSize) - 1;

		if ((psRegister->uNumber & ~uMaxImmediate) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Source 1 immediate value is too large (was %u; maximum %d)", 
				psRegister->uNumber, uMaxImmediate));
		}
		return psRegister->uNumber;
	}
}

static IMG_UINT32 CheckAndEncodeRegisterNumber(PUSEASM_CONTEXT	psContext,
											   PUSE_INST		psInst,
											   IMG_BOOL			bAllowExtended,
											   IMG_BOOL			bFmtSelect,
											   IMG_UINT32		uFmtFlag,
											   PCSGX_CORE_DESC	psTarget,
											   IMG_UINT32		uNumberFieldLength,
											   IMG_UINT32		uMaxRegNum,
											   IMG_BOOL			bIgnoresLSBsForIRegs,
											   USEASM_HWARG		eArgument,
											   IMG_BOOL			bBitwise,
											   IMG_BOOL			bSigned,
											   PUSE_REGISTER	psRegister)
/*****************************************************************************
 FUNCTION	: CheckAndEncodeRegisterNumber

 PURPOSE	: Generate the encoding for a destination argument.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  puEncodedNumber	- Returns the encoded form of the destination
								register number.
			  puInst1			- Points to the second dword of the instruction.
			  bFmtSelect		- If TRUE then the destination can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uNumTempsMappedToFPI;
	IMG_UINT32		uEncodedNumber;

	uNumTempsMappedToFPI = EURASIA_USE_NUM_TEMPS_MAPPED_TO_FPI;
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (bIgnoresLSBsForIRegs)
	{
		uNumTempsMappedToFPI <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
	}
	#else /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	PVR_UNREFERENCED_PARAMETER(bIgnoresLSBsForIRegs);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC344) */

	/*
		Reduce the maximum register number if the top bit is
		used for format control.
	*/
	if (psRegister->uType != USEASM_REGTYPE_GLOBAL &&
		psRegister->uType != USEASM_REGTYPE_FPCONSTANT &&
		psRegister->uType != USEASM_REGTYPE_IMMEDIATE)
	{
		if (bFmtSelect)
		{
			assert(uMaxRegNum == (IMG_UINT32)((1 << uNumberFieldLength) - 1));
			uMaxRegNum >>= 1;

			uNumberFieldLength--;
		}
	}
	
	if (psRegister->uIndex == USEREG_INDEX_NONE)
	{
		IMG_UINT32	uMaxEncodedRegNum;

		uMaxEncodedRegNum = (1 << uNumberFieldLength) - 1;

		switch (psRegister->uType)
		{
			case USEASM_REGTYPE_TEMP: 
			{
				IMG_UINT32	uMaxTempRegNum;

				/*
					If the register bank is temporary then the last few register numbers are aliases
					for the internal registers. So reduce the maximum register number accordingly.
				*/
				uMaxTempRegNum = uMaxEncodedRegNum - uNumTempsMappedToFPI;
				uMaxTempRegNum = min(uMaxTempRegNum, uMaxRegNum);
				if (psRegister->uNumber > uMaxTempRegNum)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid temporary register specified (r%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			case USEASM_REGTYPE_OUTPUT:
			{
				if (psRegister->uNumber > uMaxRegNum)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid output register specified (o%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				if (psRegister->uNumber > uMaxRegNum)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid primary attribute register specified (pa%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			case USEASM_REGTYPE_SECATTR:
			{
				if (psRegister->uNumber > uMaxRegNum)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid secondary attribute register specified (sa%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			case USEASM_REGTYPE_INDEX:
			{
				if (!(psRegister->uNumber == 1 || psRegister->uNumber == 2 || psRegister->uNumber == 3))
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid index mask specified (was i%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			case USEASM_REGTYPE_GLOBAL:
			{
				uEncodedNumber = 
					CheckAndEncodeGlobalRegisterNumber(psContext, 
													   psInst, 
													   psRegister, 
													   eArgument, 
													   bBitwise,
													   uNumberFieldLength,
													   psTarget);
				break;
			}
			case USEASM_REGTYPE_FPINTERNAL:
			{
				if (GPIRegistersUseTempBank(psTarget, eArgument, bAllowExtended))
				{
					IMG_UINT32	uNumber = psRegister->uNumber;

					#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
					if (bIgnoresLSBsForIRegs)
					{
						uNumber <<= SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
					}
					#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

					if (uNumber >= uNumTempsMappedToFPI)
					{
						USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid internal register specified (i%d)", psRegister->uNumber));
					}

					uEncodedNumber = uMaxEncodedRegNum + 1 - uNumTempsMappedToFPI + uNumber;
				}
				else
				{
					if (psRegister->uNumber > uMaxRegNum)
					{
						USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid fp internal register specified (fp%d)", psRegister->uNumber));
					}
					uEncodedNumber = psRegister->uNumber;
				}
				break;
			}
			case USEASM_REGTYPE_IMMEDIATE:
			{
				uEncodedNumber = CheckAndEncodeImmediate(psContext, psInst, bSigned, psRegister, uNumberFieldLength);
				break;
			}
			case USEASM_REGTYPE_FPCONSTANT:
			{
				if (psRegister->uNumber >= EURASIA_USE_FPCONSTANT_BANK_SIZE)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid constant register specified (c%d)", psRegister->uNumber));
				}
				uEncodedNumber = psRegister->uNumber;
				break;
			}
			default: 
			{
				uEncodedNumber = 0;
				break;
			}
		}
	}
	else
	{
		uEncodedNumber = EncodeIndex(psContext, psInst, psRegister, uNumberFieldLength, psTarget);
	}

	if (psRegister->uType != USEASM_REGTYPE_GLOBAL &&
		psRegister->uType != USEASM_REGTYPE_FPCONSTANT &&
		psRegister->uType != USEASM_REGTYPE_IMMEDIATE)
	{
		if (bFmtSelect && (psRegister->uFlags & uFmtFlag))
		{
			uEncodedNumber |= EURASIA_USE_FMTSELECT;
		}
	}

	return uEncodedNumber;
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_VOID BaseEncodeArgumentDoubleRegisters(PUSEASM_CONTEXT	psContext,
												  PUSE_INST			psInst,
												  IMG_BOOL			bAllowExtended,
												  IMG_UINT32		uBankExtension,
												  IMG_PUINT32		puInst0,
												  IMG_PUINT32		puInst1,
												  IMG_UINT32		uNumberFieldLength,
												  IMG_UINT32		uNumberFieldShift,
												  IMG_UINT32		uMaxUSRegNum,
												  USEASM_HWARG		eArgument,
												  IMG_UINT32		uArg,
												  PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: BaseEncodeArgumentDoubleRegisters

 PURPOSE	: Generate the encoding for a argument to an instruction
			  which reads/writes two registers.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  puInst0			- Points to the first dword of the instruction.
			  puInst1			- Points to the second dword of the instruction.
			  bFmtSelect		- If TRUE then the destination can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  uNumberFieldLength
								- Number of bits in the instruction field for the
								register number.
			  uNumberFieldShift	- Start of the instruction field for the
								register number.
			  uMaxUSRegNum		- Maximum register number which can be used for
								source registers in the unified store.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSE_REGISTER	psRegister = &psInst->asArg[uArg];
	IMG_UINT32		uEncodedNumber;

	/*
		For global and fpconstant register types the register number field is (in effect) aligned so any unencodable
		bits are at the top. So check that we have enough space to encode the top-bit which selects between the
		global and special constant banks.
	*/
	if (psRegister->uType == USEASM_REGTYPE_GLOBAL && EURASIA_USE_SPECIAL_INTERNALDATA > (1U << (uNumberFieldLength - 2)))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s type can't be global for this instruction", 
			g_apszHwArgName[eArgument]));
		return;
	}
	/*
		Similarly for immediate arguments the MSB isn't decoded so reduce the effective length of the number field
		so we check the immediate value is in the right range.
	*/
	if (psRegister->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		uNumberFieldLength--;
	}

	/*
		Encode the register bank.
	*/
	switch (eArgument)
	{
		case USEASM_HWARG_DESTINATION:
		{
			EncodeDestBank(psContext,
						   psInst,
						   bAllowExtended,
						   uBankExtension,
						   puInst1,
						   psTarget,
						   IMG_FALSE /* bBankOnly */,
						   psRegister);
			break;
		}
		case USEASM_HWARG_SOURCE0:
		{
			EncodeSrc0Bank(psContext,
						   psInst,
						   bAllowExtended,
						   puInst1,
						   uBankExtension,
						   uArg);
			break;
		}
		case USEASM_HWARG_SOURCE1:
		case USEASM_HWARG_SOURCE2:
		{
			EncodeSrc12Bank(psContext,
							psInst,
							uArg,
							bAllowExtended,
							uBankExtension,
							eArgument,
							puInst0,
							puInst1,
							psTarget,
							IMG_FALSE /* bBankOnly */);
			break;
		}
	}

	/*
		Get the encoded register number.
	*/
	uEncodedNumber =
		CheckAndEncodeRegisterNumber(psContext,
									 psInst,
									 bAllowExtended,
									 IMG_FALSE /* bFmtSelect */,
									 0 /* uFmtFlag */,
									 psTarget,
									 uNumberFieldLength,
									 uMaxUSRegNum,
									 IMG_TRUE /* bIgnoresLSBForIRegs */,
									 eArgument,
									 IMG_FALSE /* bBitwise */,
									 IMG_FALSE /* bSigned */,
									 psRegister);

	/*
		Set the encoded register number into the instruction.
	*/
	SetVectorRegisterNumber(psContext,
							psInst,
							eArgument,
							psRegister,
							uEncodedNumber,
							uNumberFieldShift,
							puInst0);	
}

static IMG_VOID EncodeWideUSSourceDoubleRegisters(PUSEASM_CONTEXT	psContext,
												  PUSE_INST			psInst,
												  IMG_BOOL			bAllowExtended,
												  IMG_UINT32		uBankExtension,
												  IMG_PUINT32		puInst0,
												  IMG_PUINT32		puInst1,
												  IMG_UINT32		uNumberFieldLength,
												  IMG_UINT32		uNumberFieldShift,
												  USEASM_HWARG		eArgument,
												  IMG_UINT32		uArg,
												  PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeWideUSSourceDoubleRegisters

 PURPOSE	: Generate the encoding for a argument to an instruction
			  which reads a 128-bit wide register.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  puInst0			- Points to the first dword of the instruction.
			  puInst1			- Points to the second dword of the instruction.
			  bFmtSelect		- If TRUE then the destination can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  uNumberFieldShift	- Start of the instruction field for the destination
								register number.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	BaseEncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  bAllowExtended,
									  uBankExtension,
									  puInst0,
									  puInst1,
									  uNumberFieldLength,
									  uNumberFieldShift,
									  SGXVEC_USE_VEC2_WIDE_USSOURCE_REGISTER_NUMBER_MAX,
									  eArgument,
									  uArg,
									  psTarget);
}

static IMG_VOID EncodeArgumentDoubleRegisters(PUSEASM_CONTEXT	psContext,
											  PUSE_INST			psInst,
											  IMG_BOOL			bAllowExtended,
											  IMG_UINT32		uBankExtension,
											  IMG_PUINT32		puInst0,
											  IMG_PUINT32		puInst1,
											  IMG_UINT32		uNumberFieldLength,
											  IMG_UINT32		uNumberFieldShift,
											  USEASM_HWARG		eArgument,
											  IMG_UINT32		uArg,
											  PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeArgumentDoubleRegisters

 PURPOSE	: Generate the encoding for a argument to an instruction
			  which reads/writes two registers.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  puInst0			- Points to the first dword of the instruction.
			  puInst1			- Points to the second dword of the instruction.
			  bFmtSelect		- If TRUE then the destination can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  uNumberFieldShift	- Start of the instruction field for the destination
								register number.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	BaseEncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  bAllowExtended,
									  uBankExtension,
									  puInst0,
									  puInst1,
									  uNumberFieldLength,
									  uNumberFieldShift,
									  (1 << uNumberFieldLength) - 1,
									  eArgument,
									  uArg,
									  psTarget);
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

static IMG_VOID EncodeArgument(PUSEASM_CONTEXT	psContext,
							   PUSE_INST		psInst,
							   USEASM_HWARG		eHwArg,
							   IMG_UINT32		uInputArg,
							   IMG_BOOL			bAllowExtended,
							   IMG_UINT32		uBankExtension,
							   IMG_BOOL			bSigned,
							   IMG_PUINT32		puInst0,
							   IMG_PUINT32		puInst1,
							   IMG_BOOL			bBitwise,
							   IMG_BOOL			bFmtSelect,
							   IMG_UINT32		uFmtFlag,
							   PCSGX_CORE_DESC	psTarget,
							   IMG_UINT32		uNumberFieldShift,
							   IMG_UINT32		uNumberFieldLength)
/*****************************************************************************
 FUNCTION	: EncodeArgument

 PURPOSE	: Encode a source argument to an instruction.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  eHwArg			- Hardware argument to encode for.
			  uInputArg			- Argument to the input instruction to encode from.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  uBankExtension	- Encoded banke xtension flag.
			  bSigned			- If TRUE then immediate arguments are interpreted
								as signed.
			  puInst0			- Points to the first dword of the instruction.
			  puInst1			- Points to the second dword of the instruction.
			  bBitwise			- If TRUE then the argument is for a bitwise instruction.
			  bFmtSelect		- If TRUE then the argument can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  psTarget			- Assembly target.
			  uNumberFieldShift	- Start of the instruction field for the argument
								register number.
			  uNumberFieldLength
								- Number of bits in the instruction register number
								field.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSE_REGISTER	psInputArg = &psInst->asArg[uInputArg];
	IMG_UINT32		uEncodedNumber;

	/*
		Encode the source register bank.
	*/
	switch (eHwArg)
	{
		case USEASM_HWARG_DESTINATION:
		{
			EncodeDestBank(psContext,
						   psInst,
						   bAllowExtended,
						   EURASIA_USE1_DBEXT,
						   puInst1,
						   psTarget,
						   IMG_FALSE /* bBankOnly */,
						   psInputArg);
			break;
		}
		case USEASM_HWARG_SOURCE0:
		{
			EncodeSrc0Bank(psContext,
						   psInst,
						   bAllowExtended,
						   puInst1,
						   uBankExtension,
						   uInputArg);
			break;
		}
		case USEASM_HWARG_SOURCE1:
		case USEASM_HWARG_SOURCE2:
		{
			EncodeSrc12Bank(psContext,
							psInst,
							uInputArg,
							bAllowExtended,
							uBankExtension,
							eHwArg,
							puInst0,
							puInst1,
							psTarget,
							IMG_FALSE /* bBankOnly */);
			break;
		}
		default: IMG_ABORT();
	}

	/*
		Get the encoded form of the register number.
	*/
	uEncodedNumber =
		CheckAndEncodeRegisterNumber(psContext,
									 psInst,
									 bAllowExtended,
									 bFmtSelect,
									 uFmtFlag,
									 psTarget,
									 uNumberFieldLength,
									 (1 << uNumberFieldLength) - 1,
									 IMG_FALSE /* bIgnoresLSBsForIRegs */,
									 eHwArg,
									 bBitwise,
									 bSigned,
									 psInputArg);

	/*
		Set the register number into the encoded instruction.
	*/
	(*puInst0) |= (uEncodedNumber << uNumberFieldShift);
}

static IMG_VOID EncodeDest(PUSEASM_CONTEXT	psContext,
						   PUSE_INST		psInst,
						   IMG_BOOL			bAllowExtended,
						   IMG_PUINT32		puInst0,
						   IMG_PUINT32		puInst1,
						   IMG_BOOL			bFmtSelect,
						   IMG_UINT32		uFmtFlag,
						   PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeDest

 PURPOSE	: Generate the encoding for a destination argument.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  bAllowExtended	- Are extended banks allowed for this instruction.
			  puInst0			- Points to the first dword of the instruction.
			  puInst1			- Points to the second dword of the instruction.
			  bFmtSelect		- If TRUE then the destination can have multiple
								formats.
			  uFmtFlag			- Value of the flag used to select an alternate
								format.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	EncodeArgument(psContext,
				   psInst,
				   USEASM_HWARG_DESTINATION,
				   0 /* uInputArg */,
				   bAllowExtended,
				   EURASIA_USE1_DBEXT /* uBankExtension */,
				   IMG_FALSE /* bSigned */,
				   puInst0,
				   puInst1,
				   IMG_FALSE /* bBitwise */,
				   bFmtSelect,
				   uFmtFlag,
				   psTarget,
				   EURASIA_USE0_DST_SHIFT,
				   EURASIA_USE_REGISTER_NUMBER_BITS);
}

static IMG_VOID EncodeSrc0(PUSEASM_CONTEXT	psContext,
						   PUSE_INST		psInst,
						   IMG_UINT32		uArg,
						   IMG_BOOL			bAllowExtended,
						   IMG_PUINT32		puInst0,
						   IMG_PUINT32		puInst1,
						   IMG_UINT32		uBankExtension,
						   IMG_BOOL			bFmtSelect,
						   IMG_UINT32		uFmtFlag,
						   PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeSrc0

 PURPOSE	: Generate the encoding for a source argument in position 0.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  uArg				- The argument to the instruction which goes in position 1.
			  bAllowExtended	- Are extended bank accesses allowed for this instruction.
			  puInst0			- Destination for the encoded source argument.
			  puInst1
			  uBankExtension	- Value of the flag in the instruction encoding to 
								indicate an extended bank.
			  bFmtSelect		- TRUE if the source argument can have two formats.
			  uFmtFlag			- Value of the flag that indicates an alternate format.

 RETURNS	: The encoded value.
*****************************************************************************/
{
	EncodeArgument(psContext,
				   psInst,
				   USEASM_HWARG_SOURCE0,
				   uArg,
				   bAllowExtended,
				   uBankExtension,
				   IMG_FALSE /* bSigned */,
				   puInst0,
				   puInst1,
				   IMG_FALSE /* bBitwise */,
				   bFmtSelect,
				   uFmtFlag,
				   psTarget,
				   EURASIA_USE0_SRC0_SHIFT,
				   EURASIA_USE_REGISTER_NUMBER_BITS);
}

static IMG_VOID EncodeSrc1(PUSEASM_CONTEXT psContext,
						   PUSE_INST psInst,
						   IMG_UINT32 uArg,
						   IMG_BOOL bAllowExtended,
						   IMG_UINT32 uBankExtension,
						   IMG_BOOL bSigned,
						   IMG_PUINT32 puInst0,
						   IMG_PUINT32 puInst1,
						   IMG_BOOL bBitwise,
						   IMG_BOOL bFmtSelect,
						   IMG_UINT32 uFmtFlag,
						   PCSGX_CORE_DESC psTarget)
/*****************************************************************************
 FUNCTION	: EncodeSrc1

 PURPOSE	: Generate the encoding for a source argument in position 1.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  uArg				- The argument to the instruction which goes in position 1.
			  bAllowExtended	- Are extended bank accesses allowed for this instruction.
			  uBankExtension	- Value of the flag in the instruction encoding to 
								indicate an extended bank.
			  bSigned			- TRUE if an immediate argument is interpreted as signed.
			  puInst0			- Destination for the encoded source argument.
			  puInst1
			  bBitwise			- TRUE if the instruction is bitwise.
			  bFmtSelect		- TRUE if the source argument can have two formats.
			  uFmtFlag			- Value of the flag that indicates an alternate format.
			  bIRegsInvalid		- 

 RETURNS	: The encoded value.
*****************************************************************************/
{
	EncodeArgument(psContext,
				   psInst,
				   USEASM_HWARG_SOURCE1,
				   uArg,
				   bAllowExtended,
				   uBankExtension,
				   bSigned,
				   puInst0,
				   puInst1,
				   bBitwise,
				   bFmtSelect,
				   uFmtFlag,
				   psTarget,
				   EURASIA_USE0_SRC1_SHIFT,
				   EURASIA_USE_REGISTER_NUMBER_BITS);
}

static IMG_VOID EncodeSrc2(PUSEASM_CONTEXT	psContext,
						   PUSE_INST		psInst,
						   IMG_UINT32		uArg,
						   IMG_BOOL			bAllowExtended,
						   IMG_UINT32		uBankExtension,
						   IMG_BOOL			bSigned,
						   IMG_PUINT32		puInst0,
						   IMG_PUINT32		puInst1,
						   IMG_BOOL			bBitwise,
						   IMG_BOOL			bFmtSelect,
						   IMG_UINT32		uFmtFlag,
						   PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeSrc2

 PURPOSE	: Generate the encoding for a source argument in position 2.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode for.
			  uArg				- The argument to the instruction which goes in position 2.
			  bAllowExtended	- Are extended bank accesses allowed for this instruction.
			  uBankExtension	- Value of the flag in the instruction encoding to 
								indicate an extended bank.
			  bSigned			- TRUE if an immediate argument is interpreted as signed.
			  puInst0			- Destination for the encoded source argument.
			  puInst1
			  bBitwise			- TRUE if the instruction is bitwise.
			  bFmtSelect		- TRUE if the source argument can have two formats.
			  uFmtFlag			- Value of the flag that indicates an alternate format.

 RETURNS	: The encoded value.
*****************************************************************************/
{
	EncodeArgument(psContext,
				   psInst,
				   USEASM_HWARG_SOURCE2,
				   uArg,
				   bAllowExtended,
				   uBankExtension,
				   bSigned,
				   puInst0,
				   puInst1,
				   bBitwise,
				   bFmtSelect,
				   uFmtFlag,
				   psTarget,
				   EURASIA_USE0_SRC2_SHIFT,
				   EURASIA_USE_REGISTER_NUMBER_BITS);
}


static IMG_VOID EncodeUnusedSource(IMG_UINT32 uArg, IMG_PUINT32 puInst0, IMG_PUINT32 puInst1)
/*****************************************************************************
 FUNCTION	: EncodeUnusedSource

 PURPOSE	: Encode an unused source.

 PARAMETERS	: uArg				- Argument which is unused.
			  puInst0, puInst1	- Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	assert(uArg == 1 || uArg == 2);
	if (uArg == 1)
	{
		*puInst0 |= (EURASIA_USE0_S1EXTBANK_IMMEDIATE << EURASIA_USE0_S1BANK_SHIFT);
		*puInst1 |= EURASIA_USE1_S1BEXT;
	}
	else
	{
		*puInst0 |= (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);
		*puInst1 |= EURASIA_USE1_S2BEXT;
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_UINT32 EncodeShortVectorPredicate(PUSEASM_CONTEXT psContext, PUSE_INST psInst)
/*****************************************************************************
 FUNCTION	: EncodeShortVectorPredicate
    
 PURPOSE	: Generate the encoding for an instruction predicate.

 PARAMETERS	: psInst			- Instruction to encode the predicate for.
			  
 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32 dwPredicate = (psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT;

	switch (dwPredicate)
	{
		case USEASM_PRED_NONE: return SGXVEC_USE_VEC_SVPRED_NONE;
		case USEASM_PRED_P0: return SGXVEC_USE_VEC_SVPRED_P0;
		case USEASM_PRED_NEGP0: return SGXVEC_USE_VEC_SVPRED_NOTP0;
		case USEASM_PRED_PN: return SGXVEC_USE_VEC_SVPRED_PERCHAN;
		default: 
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid predicate type for this instruction")); 
			return 0;
		}
	}
}

static IMG_UINT32 EncodeExtendedVectorPredicate(PUSEASM_CONTEXT psContext, PUSE_INST psInst)
/*****************************************************************************
 FUNCTION	: EncodeExtendedVectorPredicate
    
 PURPOSE	: Generate the encoding for an instruction predicate.

 PARAMETERS	: psInst			- Instruction to encode the predicate for.
			  
 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32 dwPredicate = (psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT;

	switch (dwPredicate)
	{
		case USEASM_PRED_NONE: return SGXVEC_USE_VEC_EVPRED_NONE;
		case USEASM_PRED_P0: return SGXVEC_USE_VEC_EVPRED_P0;
		case USEASM_PRED_P1: return SGXVEC_USE_VEC_EVPRED_P1;
		case USEASM_PRED_P2: return SGXVEC_USE_VEC_EVPRED_P2;
		case USEASM_PRED_NEGP0: return SGXVEC_USE_VEC_EVPRED_NOTP0;
		case USEASM_PRED_NEGP1: return SGXVEC_USE_VEC_EVPRED_NOTP1;
		case USEASM_PRED_NEGP2: return SGXVEC_USE_VEC_EVPRED_NOTP2;
		case USEASM_PRED_PN: return SGXVEC_USE_VEC_EVPRED_PERCHAN;
		default: 
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid predicate type for this instruction")); 
			return 0;
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

static IMG_UINT32 EncodePredicate(PUSEASM_CONTEXT psContext, PUSE_INST psInst, IMG_BOOL bShort)
/*****************************************************************************
 FUNCTION	: EncodePredicate
    
 PURPOSE	: Generate the encoding for an instruction predicate.

 PARAMETERS	: psInst			- Instruction to encode the predicate for.
			  bShort			- TRUE if the instruction only accepts the short predicate form.
			  
 RETURNS	: The encoded value.
*****************************************************************************/
{
	IMG_UINT32 dwPredicate = (psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT;
	if (bShort)
	{
		switch (dwPredicate)
		{
			case USEASM_PRED_NONE: return EURASIA_USE1_SPRED_ALWAYS;
			case USEASM_PRED_P0: return EURASIA_USE1_SPRED_P0;
			case USEASM_PRED_P1: return EURASIA_USE1_SPRED_P1;
			case USEASM_PRED_NEGP0: return EURASIA_USE1_SPRED_NOTP0;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid predicate type for this instruction")); break;
		}
	}
	else
	{
		switch (dwPredicate)
		{
			case USEASM_PRED_NONE: return EURASIA_USE1_EPRED_ALWAYS;
			case USEASM_PRED_P0: return EURASIA_USE1_EPRED_P0;
			case USEASM_PRED_P1: return EURASIA_USE1_EPRED_P1;
			case USEASM_PRED_NEGP0: return EURASIA_USE1_EPRED_NOTP0;
			case USEASM_PRED_NEGP1: return EURASIA_USE1_EPRED_NOTP1;
			case USEASM_PRED_P2: return EURASIA_USE1_EPRED_P2;
			case USEASM_PRED_P3: return EURASIA_USE1_EPRED_P3;
			case USEASM_PRED_PN: return EURASIA_USE1_EPRED_PNMOD4;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid predicate type for this instruction")); break;
		}
	}
	return 0;
}

static IMG_UINT32 EncodeFloatMod(IMG_UINT32		uFlags)
/*****************************************************************************
 FUNCTION	: EncodeFloatMod

 PURPOSE	: Generate the encoding for the potential modifiers to an argument to a floating point instruction.

 PARAMETERS	: uFlags			- The flags from the argument to encode the modifiers for.

 RETURNS	: The encoded value.
*****************************************************************************/
{
	return ((uFlags & USEASM_ARGFLAGS_NEGATE) ? EURASIA_USE_SRCMOD_NEGATE : 0) |
		   ((uFlags & USEASM_ARGFLAGS_ABSOLUTE) ? EURASIA_USE_SRCMOD_ABSOLUTE : 0);
}

/*****************************************************************************
 FUNCTION	: AssembleDot34
    
 PURPOSE	: Encode an integer dot3 or dot4 instructiom.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Instruction to encode.
			  puInst			- Destination for the encoding.
			  psTarget			- Assembly target.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID AssembleDot34(PUSEASM_CONTEXT psContext, 
							  PUSE_INST psInst, 
							  IMG_PUINT32 puInst, 
							  PCSGX_CORE_DESC psTarget)
{
	IMG_BOOL bSeperateAlphaScale = FALSE;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;

	/*
		Is U8/C10 format select on arguments turned on?
	*/
	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
		(~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MAINISSUE, 0, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_DOT3DOT4 << EURASIA_USE1_OP_SHIFT) |
				 (EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d",
				EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Get the encoded opcode. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_U8DOT3:
		case USEASM_OP_U8DOT3OFF:
		case USEASM_OP_U16DOT3:
		case USEASM_OP_U16DOT3OFF: puInst[1] |= (EURASIA_USE1_DOT34_OPSEL_DOT3 << EURASIA_USE1_DOT34_OPSEL_SHIFT); break;
		case USEASM_OP_U8DOT4:
		case USEASM_OP_U8DOT4OFF:
		case USEASM_OP_U16DOT4:
		case USEASM_OP_U16DOT4OFF: puInst[1] |= (EURASIA_USE1_DOT34_OPSEL_DOT4 << EURASIA_USE1_DOT34_OPSEL_SHIFT); break;
		default: break;
	}
	/* Get instruction modes. */
	if (psInst->uOpcode == USEASM_OP_U16DOT3 || psInst->uOpcode == USEASM_OP_U16DOT4 ||
		psInst->uOpcode == USEASM_OP_U16DOT3OFF || psInst->uOpcode == USEASM_OP_U16DOT4OFF)
	{
		puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_ONEWAY << EURASIA_USE1_DOT34_ASEL1_SHIFT);
		if (psInst->uOpcode == USEASM_OP_U16DOT3OFF || psInst->uOpcode == USEASM_OP_U16DOT4OFF)
		{
			puInst[1] |= EURASIA_USE1_DOT34_OFF;
		}
		bSeperateAlphaScale = TRUE;
	}
	else
	{
		if (!(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE))
		{
			puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_ZERO << EURASIA_USE1_DOT34_ASEL1_SHIFT) |
						  (EURASIA_USE1_DOT34_ASEL2_FOURWAY << EURASIA_USE1_DOT34_ASEL2_SHIFT);
		}
		else
		{
			PUSE_INST psCoissue = psInst->psNext;
			if (psInst->uOpcode == USEASM_OP_U8DOT4 || psInst->uOpcode == USEASM_OP_U8DOT4OFF)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Integer dot4 can't be coissued"));
			}
			if (psCoissue == NULL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Coissued instruction with no alpha operation"));
			}
			else
			{
				switch (psCoissue->uOpcode)
				{
					case USEASM_OP_AINTRP1:
					case USEASM_OP_AINTRP2:
					{
						IMG_UINT32 uAMod2 = 0;
						if (psCoissue->uOpcode == USEASM_OP_AINTRP1)
						{
							puInst[1] |= (EURASIA_USE1_DOT34_AOP_INTERPOLATE1 << EURASIA_USE1_DOT34_AOP_SHIFT);
						}
						else
						{
							puInst[1] |= (EURASIA_USE1_DOT34_AOP_INTERPOLATE2 << EURASIA_USE1_DOT34_AOP_SHIFT);
						}
						CheckArgFlags(psContext, psCoissue, 0, uValidArgFlags);
						EncodeSrc0(psContext, psCoissue, 0, FALSE, puInst + 0, puInst + 1, 0, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
						if (psCoissue->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to aintrp1 must be an integer source selection"));
						}
						if (psCoissue->asArg[1].uFlags != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to aintrp1"));
						}
						if (psCoissue->asArg[1].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						switch (psCoissue->asArg[1].uNumber)
						{
							case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_ZERO << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_SRC1ALPHA << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_SRC2ALPHA << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid integer source for the second argument to aintrp1"));
						}
						if (psCoissue->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to aintrp1 must be an integer source selection"));
						}
						if ((psCoissue->asArg[2].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "Only the complement flag is valid for the third argument to aintrp1"));
						}
						if (psCoissue->asArg[2].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						if (psCoissue->asArg[2].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
						{
							uAMod2 = EURASIA_USE1_DOT34_AMOD2_COMPLEMENT;
						}
						switch (psCoissue->asArg[2].uNumber)
						{
							case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_ZERO << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							case USEASM_INTSRCSEL_ONE: 
							{
								puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_ZERO << EURASIA_USE1_DOT34_ASEL2_SHIFT);
								uAMod2 ^= EURASIA_USE1_DOT34_AMOD2_COMPLEMENT;
								break;
							}
							case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_SRC1ALPHA << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_SRC2ALPHA << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid integer source for the third argument to aintrp1"));
						}
						puInst[1] |= (uAMod2 << EURASIA_USE1_DOT34_AMOD2_SHIFT);
						break;
					}
					case USEASM_OP_AADD:
					case USEASM_OP_ASUB:
					{
						IMG_UINT32 uAMod1, uAMod2;
						IMG_UINT32 uASrc1Mod;

						if (psCoissue->uOpcode == USEASM_OP_AADD)
						{
							puInst[1] |= (EURASIA_USE1_DOT34_AOP_ADD << EURASIA_USE1_DOT34_AOP_SHIFT);
						}
						else
						{
							puInst[1] |= (EURASIA_USE1_DOT34_AOP_SUBTRACT << EURASIA_USE1_DOT34_AOP_SHIFT);
						}

						if (psCoissue->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to aadd/asub must be an integer source selection"));
						}
						if (psCoissue->asArg[0].uFlags != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the first argument to aadd/asub"));
						}
						if (psCoissue->asArg[0].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						switch (psCoissue->asArg[0].uNumber)
						{
							case USEASM_INTSRCSEL_AX1: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X1 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
							case USEASM_INTSRCSEL_AX2: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X2 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
							case USEASM_INTSRCSEL_AX4: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X4 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
							case USEASM_INTSRCSEL_AX8: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X8 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
							default: USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to aadd/asub must be an alpha scale factor"));
						}

						if (psCoissue->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to aadd/asub must be an integer source selection"));
						}
						if ((psCoissue->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "Only the complement flag is valid for the second argument to aadd/asub"));
						}
						if (psCoissue->asArg[1].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						if (psCoissue->asArg[1].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
						{
							uAMod1 = EURASIA_USE1_DOT34_AMOD1_COMPLEMENT;
						}
						else
						{
							uAMod1 = EURASIA_USE1_DOT34_AMOD1_NONE;
						}
						switch (psCoissue->asArg[1].uNumber)
						{
							case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_ZERO << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							case USEASM_INTSRCSEL_ONE:
							{
								puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_ZERO << EURASIA_USE1_DOT34_ASEL1_SHIFT);
								uAMod1 ^= EURASIA_USE1_DOT34_AMOD1_COMPLEMENT;
								break;
							}
							case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_SRC1ALPHA << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL1_SRC2ALPHA << EURASIA_USE1_DOT34_ASEL1_SHIFT); break;
							default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to aadd/asub - one, zero, s1a, s2a are valid choices"));
						}
						puInst[1] |= (uAMod1 << EURASIA_USE1_DOT34_AMOD1_SHIFT);

						if (psCoissue->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to aadd/asub must be an integer source selection"));
						}
						if ((psCoissue->asArg[2].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "Only the complement flag is valid for the third argument to aadd/asub"));
						}
						if (psCoissue->asArg[2].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						if (psCoissue->asArg[2].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
						{
							uASrc1Mod = EURASIA_USE0_DOT34_ASRC1MOD_COMPLEMENT;
						}
						else
						{
							uASrc1Mod = EURASIA_USE0_DOT34_ASRC1MOD_NONE;
						}
						if (psCoissue->asArg[2].uNumber != USEASM_INTSRCSEL_SRC1ALPHA)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to aadd/asub must be s1a"));
						}
						puInst[0] |= (uASrc1Mod << EURASIA_USE0_DOT34_ASRC1MOD_SHIFT);

						if (psCoissue->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to aadd/asub must be an integer source selection"));
						}
						if ((psCoissue->asArg[3].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "Only the complement flag is valid for the fourth argument to aadd/asub"));
						}
						if (psCoissue->asArg[3].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						if (psCoissue->asArg[3].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
						{
							uAMod2 = EURASIA_USE1_DOT34_AMOD2_COMPLEMENT;
						}
						else
						{
							uAMod2 = EURASIA_USE1_DOT34_AMOD2_NONE;
						}
						switch (psCoissue->asArg[3].uNumber)
						{
							case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_ZERO << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							case USEASM_INTSRCSEL_ONE:
							{
								puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_ZERO << EURASIA_USE1_DOT34_ASEL2_SHIFT);
								uAMod2 ^= EURASIA_USE1_DOT34_AMOD2_COMPLEMENT;
								break;
							}
							case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_SRC1ALPHA << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_DOT34_ASEL2_SRC2ALPHA << EURASIA_USE1_DOT34_ASEL2_SHIFT); break;
							default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to aadd/asub - one, zero, s1a, s2a are valid choices"));
						}
						puInst[1] |= (uAMod2 << EURASIA_USE1_DOT34_AMOD2_SHIFT);

						if (psCoissue->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to aadd/asub must be an integer source selection"));
						}
						if (psCoissue->asArg[4].uFlags  != 0)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to aadd/asub"));
						}
						if (psCoissue->asArg[4].uIndex != USEREG_INDEX_NONE)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
						}
						if (psCoissue->asArg[4].uNumber != USEASM_INTSRCSEL_SRC2ALPHA)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to aadd/asub must be s2a"));
						}
						break;
					}
					default: break;
				}
			}
			bSeperateAlphaScale = TRUE;
		}
		if (psInst->uOpcode == USEASM_OP_U8DOT3OFF || psInst->uOpcode == USEASM_OP_U8DOT4OFF)
		{
			puInst[1] |= EURASIA_USE1_DOT34_OFF;
		}
	}

	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to dot3/dot4 must be an integer source selection"));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to dot3/dot4"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
	}
	switch (psInst->asArg[1].uNumber)
	{
		case USEASM_INTSRCSEL_CX1: puInst[1] |= (EURASIA_USE1_DOT34_CSCALE_X1 << EURASIA_USE1_DOT34_CSCALE_SHIFT); break;
		case USEASM_INTSRCSEL_CX2: puInst[1] |= (EURASIA_USE1_DOT34_CSCALE_X2 << EURASIA_USE1_DOT34_CSCALE_SHIFT); break;
		case USEASM_INTSRCSEL_CX4: puInst[1] |= (EURASIA_USE1_DOT34_CSCALE_X4 << EURASIA_USE1_DOT34_CSCALE_SHIFT); break;
		case USEASM_INTSRCSEL_CX8: puInst[1] |= (EURASIA_USE1_DOT34_CSCALE_X8 << EURASIA_USE1_DOT34_CSCALE_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to dot3/dot4 must be a colour scale factor"));
	}

	if (!bSeperateAlphaScale)
	{
		if (psInst->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to dot3/dot4 must be an integer source selection"));
		}
		if (psInst->asArg[2].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the third argument to dot3/dot4"));
		}
		if (psInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selection is not indexable"));
		}
		switch (psInst->asArg[2].uNumber)
		{
			case USEASM_INTSRCSEL_AX1: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X1 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
			case USEASM_INTSRCSEL_AX2: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X2 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
			case USEASM_INTSRCSEL_AX4: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X4 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
			case USEASM_INTSRCSEL_AX8: puInst[0] |= (EURASIA_USE0_DOT34_ASCALE_X8 << EURASIA_USE0_DOT34_ASCALE_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to dot3/dot4 must be a alpha scale factor"));
		}
	}

	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_COMPLEMENT | uValidArgFlags);
	EncodeSrc1(psContext, psInst, 3, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_DOT34_SRC1MOD_COMPLEMENT << EURASIA_USE1_DOT34_SRC1MOD_SHIFT);
	}
	CheckArgFlags(psContext, psInst, 4, USEASM_ARGFLAGS_COMPLEMENT | uValidArgFlags);
	EncodeSrc2(psContext, psInst, 4, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_DOT34_SRC2MOD_COMPLEMENT << EURASIA_USE1_DOT34_SRC2MOD_SHIFT);
	}
}

/*****************************************************************************
 FUNCTION	: CheckBytemask

 PURPOSE	: Checks the consistency of the bytemask with the destination.

 PARAMETERS	: psInst			- The instruction to check.
			  uByteMask			- The bytemask.
			  bIRegsValid		- Indicates if partial masks on IRegs should
								  be allowed.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID CheckByteMask(PUSEASM_CONTEXT	psContext,
							  PUSE_INST			psInst,
							  IMG_UINT32		uByteMask,
							  IMG_UINT32		bIRegsValid)
{
	if	(uByteMask != 0xF)
	{
		if (psInst->asArg[0].uType == USEASM_REGTYPE_INDEX ||
			psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .bytemask1111 is supported when the destination is indexed, or an index register"));
		}

		if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
		{
			if (!bIRegsValid)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .bytemask1111 is supported when the destination is an internal register"));
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeFloatRepeats

 PURPOSE	: Encode the repeat count/mask for a floating point instruction.

 PARAMETERS	: psTarget				- Processor target.
			  puInst				- Destination for the encoded repeats.
			  psInst				- Source instruction.
			  uOp					- Instruction opcode.
			  psContext				- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFloatRepeats(PCSGX_CORE_DESC psTarget, IMG_PUINT32 puInst, PUSE_INST psInst, IMG_UINT32 uOp, PUSEASM_CONTEXT psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	PVR_UNREFERENCED_PARAMETER(psContext);
	PVR_UNREFERENCED_PARAMETER(uOp);
	PVR_UNREFERENCED_PARAMETER(psTarget);

	/*
		For instruction writing clipplane P flags check they have at least
		two iterations.
	*/
	if (psInst->uOpcode == USEASM_OP_FDPC || psInst->uOpcode == USEASM_OP_FDDPC)
	{
		IMG_BOOL	bTwoIterations;

		bTwoIterations = IMG_TRUE;
		if (uRptCount == 0)
		{
			/*
				Check repeat mask.
			*/
			if (uMask == 1 || uMask == 2 || uMask == 4 || uMask == 8)
			{
				bTwoIterations = IMG_FALSE;
			}
		}
		else
		{
			/*
				Check repeat count.
			*/
			if (uRptCount == 1)
			{
				bTwoIterations = IMG_FALSE;
			}
		}

		if (!bTwoIterations)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"A %s instruction must have at least two iterations", OpcodeName(psInst->uOpcode)));
		}
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	if (SupportsFmad16Swizzles(psTarget) && uOp == EURASIA_USE1_OP_FARITH16)
	{
		if (uRptCount > 0)
		{
			if (uRptCount > SGX545_USE1_FARITH16_RCNT_MAXIMUM)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count for fmad16 on this processor is %d", SGX545_USE1_FARITH16_RCNT_MAXIMUM));
			}
		}
		else
		{
			switch (uMask)
			{
				case 1:
				{
					uRptCount = 1;
					break;
				}
				case 3:
				{
					uRptCount = 2;
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The only repeat masks supported on floating point instructions on this processor are .x, .xy, .xyz and .xyzw"));
					uRptCount = 1;
					break;
				}
			}
		}
		puInst[1] |= ((uRptCount - 1) << SGX545_USE1_FARITH16_RCNT_SHIFT);
		if (!(psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE) && uRptCount != 1)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "MOE increments aren't supported on fmad16 on this processor"));
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS0)
		{
			puInst[1] |= SGX545_USE1_FLOAT_S0INC;
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS1)
		{
			puInst[1] |= SGX545_USE1_FLOAT_S1INC;
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS2)
		{
			puInst[1] |= SGX545_USE1_FLOAT_S2INC;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) && defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) && defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */
	#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
	if (IsPerInstMoeIncrements(psTarget) && uOp != EURASIA_USE1_OP_FSCALAR)
	{
		if (uRptCount > 0)
		{
			if (uRptCount > SGX545_USE1_FLOAT_RCOUNT_MAXIMUM)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count for floating point instructions on this processor is %d", SGX545_USE1_FLOAT_RCOUNT_MAXIMUM));
			}
		}
		else
		{
			switch (uMask)
			{
				case 1:
				{
					uRptCount = 1;
					break;
				}
				case 3:
				{
					uRptCount = 2;
					break;
				}
				case 7:
				{
					uRptCount = 3;
					break;
				}
				case 15:
				{
					uRptCount = 4;
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The only repeat masks supported on floating point instructions on this processor are .x, .xy, .xyz and .xyzw"));
					uRptCount = 1;
					break;
				}
			}
		}
		puInst[1] |= ((uRptCount - 1) << SGX545_USE1_FLOAT_RCNT_SHIFT);
		if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE)
		{
			if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS0)
			{
				puInst[1] |= SGX545_USE1_FLOAT_S0INC;
			}
			if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS1)
			{
				puInst[1] |= SGX545_USE1_FLOAT_S1INC;
			}
			if (psInst->uFlags2 & USEASM_OPFLAGS2_PERINSTMOE_INCS2)
			{
				puInst[1] |= SGX545_USE1_FLOAT_S2INC;
			}
		}
		else
		{
			puInst[1] |= SGX545_USE1_FLOAT_MOE;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
	{
		if (uRptCount > 0)
		{
			puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
		}
		else
		{
			puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_BOOL EncodeTestInstruction_VecAluOp(PUSE_INST	psInst,
											   IMG_UINT32	uSrcStartIndex,
											   IMG_PUINT32	puInst,
											   IMG_PBOOL	pbVectorAluOp)
/*****************************************************************************
 FUNCTION	: EncodeTestInstruction_VecAluOp

 PURPOSE	: Encodes an F16/F32 ALU opcode for a TEST instruction.

 PARAMETERS	: psInst			- Instruction.
			  uSrcStartIndex	- Index of the start of the sources in the
								argument array.
			  puInst			- Destination for the encoded instruction.
			  pbVectorAluOp		- On return TRUE if the ALU opcode is a
								vector instruction.

 RETURNS	: TRUE if the ALU opcode was an F32/F16 instruction.
*****************************************************************************/
{
	IMG_UINT32	uAluOp;
	IMG_UINT32	uPrec;
	IMG_UINT32	uOpcode;

	uOpcode = psInst->uOpcode;

	/*
		Change ADD -> SUB to support a negate modifier on source 2.
	*/
	if (uOpcode == USEASM_OP_VF16ADD || 
		uOpcode == USEASM_OP_VADD ||
		uOpcode == USEASM_OP_VF16SUB ||
		uOpcode == USEASM_OP_VSUB)
	{
		if ((psInst->asArg[uSrcStartIndex + 2].uFlags & USEASM_ARGFLAGS_NEGATE) != 0)
		{
			switch (uOpcode)
			{
				case USEASM_OP_VF16ADD: uOpcode = USEASM_OP_VF16SUB; break;
				case USEASM_OP_VADD: uOpcode = USEASM_OP_VSUB; break;
				case USEASM_OP_VF16SUB: uOpcode = USEASM_OP_VF16ADD; break;
				case USEASM_OP_VSUB: uOpcode = USEASM_OP_VADD; break;
				default: IMG_ABORT();
			}
		}
	}

	switch (uOpcode)
	{
		case USEASM_OP_VF16ADD:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VADD;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16FRC:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VFRC;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16MIN:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VMIN;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16MAX:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VMAX;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16DSX:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VDSX;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16DSY:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VDSY;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16MUL:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VMUL;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16SUB:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VSUB;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VF16DP:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_VDP;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			break;
		}
		case USEASM_OP_VRCP:
		case USEASM_OP_VRSQ:
		case USEASM_OP_VLOG:
		case USEASM_OP_VEXP:
		{
			switch (psInst->uOpcode)
			{
				case USEASM_OP_VRCP: uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_FRCP; break;
				case USEASM_OP_VRSQ: uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_FRSQ; break;
				case USEASM_OP_VLOG: uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_FLOG; break;
				case USEASM_OP_VEXP: uAluOp = SGXVEC_USE0_TEST_ALUOP_F16_FEXP; break;
				default: IMG_ABORT();
			}

			if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_FMTF16)
			{
				uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F16;
			}
			else
			{
				uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			}
			break;
		}
		case USEASM_OP_VADD:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VADD;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VFRC:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VFRC;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VDP:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VDP;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VMIN:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VMIN;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VMAX:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VMAX;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VDSX:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VDSX;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VDSY:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VDSY;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VMUL:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VMUL;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		case USEASM_OP_VSUB:
		{
			uAluOp = SGXVEC_USE0_TEST_ALUOP_F32_VSUB;
			uPrec = SGXVEC_USE1_TEST_PREC_ALUFLOAT_F32;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	puInst[0] |= (uAluOp << EURASIA_USE0_TEST_ALUOP_SHIFT);
	puInst[1] |= (uPrec << SGXVEC_USE1_TEST_PREC_SHIFT);

	*pbVectorAluOp = IMG_TRUE;

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

static IMG_BOOL EncodeTestInstruction_FloatAluOp(PUSE_INST		psInst,
												 IMG_PUINT32	puInst,
												 IMG_PBOOL		pbSFASEL)
/*****************************************************************************
 FUNCTION	: EncodeTestInstruction_FloatAluOp

 PURPOSE	: Encodes an F32 ALU opcode for a TEST instruction.

 PARAMETERS	: psInst			- Instruction.
			  puInst			- Destination for the encoded instruction.
			  pbSFASEL			- On return TRUE if the ALU opcode has per-source
								F16/F32 selects.

 RETURNS	: TRUE if the ALU opcode was an F32 instruction.
*****************************************************************************/
{
	IMG_UINT32	uAluOp;

	switch (psInst->uOpcode)
	{
		case USEASM_OP_FMOV:
		case USEASM_OP_FADD: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_ADD;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FFRC: 
		case USEASM_OP_FSUBFLR:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_FRC;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FRCP: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_RCP;
			break;
		}
		case USEASM_OP_FRSQ: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_RSQ;
			break;
		}
		case USEASM_OP_FLOG: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_LOG;
			break;
		}
		case USEASM_OP_FEXP: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_EXP;
			break;
		}
		case USEASM_OP_FDP3:
		case USEASM_OP_FDP4:
		case USEASM_OP_FDP: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_DP;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FMIN: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_MIN;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FMAX: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_MAX;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FDSX: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_DSX;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FDSY: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_DSY;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FMUL: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_MUL;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FSUB: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_F32_SUB;
			*pbSFASEL = IMG_TRUE;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	puInst[0] |= (uAluOp << EURASIA_USE0_TEST_ALUOP_SHIFT);
	puInst[0] |= (EURASIA_USE0_TEST_ALUSEL_F32 << EURASIA_USE0_TEST_ALUSEL_SHIFT);

	return IMG_TRUE;
}

static IMG_VOID EncodeTestInstruction_AluOp(PCSGX_CORE_DESC	psTarget,
											PUSE_INST		psInst,
											IMG_UINT32		uSrcStartArgIndex,
											IMG_PUINT32		puInst,
											PUSEASM_CONTEXT	psContext,
											IMG_PBOOL		pbSFASEL,
											IMG_PBOOL		pbCFASEL,
											IMG_PBOOL		pbSigned,
											IMG_PBOOL		pbRestrictedSource1Banks,
											IMG_PBOOL		pbRestrictedSource2Banks,
											IMG_PBOOL		pbVectorAluOp,
											IMG_PBOOL		pbAllowSatModifier,
											IMG_PBOOL		pbBitwise,
											IMG_PUINT32		puAluSel)
/*****************************************************************************
 FUNCTION	: EncodeTestInstruction_AluOp

 PURPOSE	: Encodes an ALU opcode for a TEST instruction.

 PARAMETERS	: psTarget			- Target core for assembly.
		      psInst			- Instruction.
			  uSrcStartIndex	- Index of the start of the sources in the
								argument array.
			  puInst			- Destination for the encoded instruction.
			  psContext			- Assembly context.
			  pbSFASEL			- On return TRUE if the ALU opcode has per-source
								F16/F32 selects.
			  pbCFASEL			- On return TRUE if the ALU opcode has per-source
								U8/C10 selects.
			  pbSigned			- On return TRUE if the ALU opcode interprets
								immediate arguments as TRUE.
			  pbRestrictedSource1Banks
								- On return TRUE if the ALU opcode restricts which
								register banks can be used in source 1.
			  pbRestrictedSource2Banks
								- On return TRUE if the ALU opcode restricts which
								register banks can be used in source 2.
			  pbVectorAluOp		- On return TRUE if the ALU opcode is a vector
								instruction.
			  pbAllowSatModifier
								- On return TRUE if the ALU opcode supports the SAT
								modifier.
			  pbBitwise			- On return TRUE if the ALU opcode is a bitwise
								instruction.
			  puAluSel			- On return the ALU type selected.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uAluOp;
	IMG_UINT32	uAluSel;

	PVR_UNREFERENCED_PARAMETER(psTarget);
	PVR_UNREFERENCED_PARAMETER(uSrcStartArgIndex);

	*pbSFASEL = IMG_FALSE;
	*pbCFASEL = IMG_FALSE;
	*pbSigned = IMG_FALSE;
	*pbRestrictedSource1Banks = IMG_FALSE;
	*pbRestrictedSource2Banks = IMG_FALSE;
	*pbVectorAluOp = IMG_FALSE;
	*pbAllowSatModifier = IMG_FALSE;
	*pbBitwise = IMG_FALSE;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		if (EncodeTestInstruction_VecAluOp(psInst, uSrcStartArgIndex, puInst, pbVectorAluOp))
		{
			*puAluSel = EURASIA_USE0_TEST_ALUSEL_F32;
			return;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		if (EncodeTestInstruction_FloatAluOp(psInst, puInst, pbSFASEL))
		{
			*puAluSel = EURASIA_USE0_TEST_ALUSEL_F32;
			return;
		}
	}

	switch (psInst->uOpcode)
	{
		case USEASM_OP_AND: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_AND;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_OR: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_OR;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_XOR: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_XOR;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_SHL: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_SHL;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_SHR: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_SHR;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_ROL: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_ROL;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_ASR: 
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_BITWISE_ASR;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_BITWISE;
			break;
		}
		case USEASM_OP_IADD16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IADD;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbSigned = TRUE;
			break;
		}
		case USEASM_OP_ISUB16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_ISUB;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbSigned = TRUE;
			break;
		}
		case USEASM_OP_IMUL16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IMUL;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbSigned = TRUE;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_IADDU16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IADDU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			break;
		}
		case USEASM_OP_ISUBU16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_ISUBU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			break;
		}
		case USEASM_OP_IMULU16:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IMULU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_IADD32:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IADD32;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbSigned = TRUE;
			#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32)
			if (SupportsTESTSUB32(psTarget))
			{
				*pbRestrictedSource1Banks = IMG_TRUE;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32) */
			break;
		}
		case USEASM_OP_IADDU32:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I16_IADDU32;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32)
			if (SupportsTESTSUB32(psTarget))
			{
				*pbRestrictedSource1Banks = IMG_TRUE;
			}
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32) */
			break;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32)
		case USEASM_OP_ISUB32:
		{
			if (!SupportsTESTSUB32(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Conditional code update isn't available on this instruction")); 
			}
			uAluOp = SGXVEC_USE0_TEST_ALUOP_I16_SUB32;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbSigned = IMG_TRUE;
			*pbRestrictedSource1Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_ISUBU32:
		{
			if (!SupportsTESTSUB32(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Conditional code update isn't available on this instruction")); 
			}
			uAluOp = SGXVEC_USE0_TEST_ALUOP_I16_SUBU32;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I16;
			*pbRestrictedSource1Banks = IMG_TRUE;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SUB32) */
		case USEASM_OP_IADD8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_ADD;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_ISUB8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_SUB;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_IADDU8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_ADDU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_ISUBU8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_SUBU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_IMUL8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_MUL;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			break;
		}
		case USEASM_OP_IMULU8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_MULU;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			break;
		}
		case USEASM_OP_FPMUL8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_FPMUL;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbCFASEL = IMG_TRUE;
			break;
		}
		case USEASM_OP_FPADD8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_FPADD;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbCFASEL = IMG_TRUE;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		case USEASM_OP_FPSUB8:
		{
			uAluOp = EURASIA_USE0_TEST_ALUOP_I8_FPSUB;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbCFASEL = IMG_TRUE;
			*pbRestrictedSource2Banks = IMG_TRUE;
			break;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_TEST_SABLND)
		case USEASM_OP_SABLND:
		{
			if (!SupportsTESTSABLND(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Conditional code update isn't available on this instruction")); 
			}

			uAluOp = SGXVEC_USE0_TEST_ALUOP_I8_SABLND;
			uAluSel = EURASIA_USE0_TEST_ALUSEL_I8;
			*pbCFASEL = IMG_TRUE;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_TEST_SABLND) */
		default: 
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Conditional code update isn't available on this instruction")); 
			uAluOp = 0;
			uAluSel = 0;
			break;
		}
	}

	puInst[0] |= (uAluOp << EURASIA_USE0_TEST_ALUOP_SHIFT);
	*puAluSel = uAluSel;
	
	if (uAluSel == EURASIA_USE0_TEST_ALUSEL_I16 || uAluSel == EURASIA_USE0_TEST_ALUSEL_I8)
	{
		*pbAllowSatModifier = IMG_TRUE;
	}

	if (uAluSel == EURASIA_USE0_TEST_ALUSEL_BITWISE)
	{
		*pbBitwise = IMG_TRUE;
	}
}

static IMG_VOID EncodeTestInstruction_Repeat(PCSGX_CORE_DESC		psTarget,
											 PUSE_INST			psInst,
											 IMG_UINT32			uTestMask,
											 IMG_PUINT32		puInst,
											 PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: EncodeTestInstruction_Repeat

 PURPOSE	: Encodes the repeat count for a TEST instruction.

 PARAMETERS	: psTarget			- Target core for assembly.
			  psInst			- Instruction.
			  uTestMask			- Test mask setting for the instruction.
			  puInst			- Destination for the encoded instruction.
			  psContext			- Assembly context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	/* Override repeat mask for fdp3 and fdp4 */
	if (psInst->uOpcode == USEASM_OP_FDP3 || psInst->uOpcode == USEASM_OP_FDP4)
	{
		if (uMask != 0x1)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is not valid for this instruction"));
		}
		uMask = (psInst->uOpcode == USEASM_OP_FDP3) ? 0x7 : 0xF;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		if (uMask != 0x1 && uMask != 0x0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask isn't valid for this instruction"));
		}
		if (uRptCount > 0)
		{
			if (uRptCount > SGXVEC_USE1_TEST_RCNT_MAXIMUM)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count for this instruction is %d",
					SGXVEC_USE1_TEST_RCNT_MAXIMUM));
			}
			puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_TEST_RCNT_SHIFT);
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		if (uRptCount > 0)
		{
			if (uTestMask == USEASM_TEST_MASK_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat count is not valid for this instruction"));
			}
			else
			{
				if (uRptCount > EURASIA_USE1_TEST_MAX_REPEAT)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count for this instruction is %d", EURASIA_USE1_TEST_MAX_REPEAT));
				}
				puInst[1] |= (((1 << uRptCount) - 1) << EURASIA_USE1_TEST_PREDMASK_SHIFT);
			}
		}
		else
		{
			if (
					FixBRN23062(psTarget) && 
					uMask != 0x1 &&
					uTestMask == USEASM_TEST_MASK_NONE &&
					psInst->uOpcode != USEASM_OP_FDP && 
					psInst->uOpcode != USEASM_OP_FDP3 &&
					psInst->uOpcode != USEASM_OP_FDP4
			   )
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A repeated test instruction isn't supported on this processor"));
			}
			if (
					FixBRN23164(psTarget) &&
					uMask != 0x1 &&
					uTestMask == USEASM_TEST_MASK_NONE &&
					(
						psInst->uOpcode == USEASM_OP_FDP ||
						psInst->uOpcode == USEASM_OP_FDP3 ||
						psInst->uOpcode == USEASM_OP_FDP4
					)
			   )
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A repeated dotproduct with test isn't supported on this processor"));
			}

			puInst[1] |= uMask << EURASIA_USE1_TEST_PREDMASK_SHIFT;
		}
	}

	if (uTestMask == USEASM_TEST_MASK_NONE)
	{
		if (psInst->uOpcode != USEASM_OP_FDP && 
			psInst->uOpcode != USEASM_OP_FDP3 &&
			psInst->uOpcode != USEASM_OP_FDP4)
		{
			if ((uMask != 0x1 || uRptCount > 1) && psInst->asArg[1].uNumber != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "For a test instruction with a repeat mask the predicate destination must be p0"));
			}
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
static IMG_VOID EncodeTestInstructionPrec(PUSEASM_CONTEXT	psContext,
										  PUSE_INST			psInst,
										  IMG_UINT32		uAluSel,
										  IMG_PUINT32		puInst,
										  IMG_PUINT32		puValidFlags1,
										  IMG_PUINT32		puValidFlags2)
/*****************************************************************************
 FUNCTION	: EncodeTestInstructionPrec

 PURPOSE	: Encode the precision modifier on a TEST instruction.

 PARAMETERS	: psContext		- Assembler context.
			  psInst		- Input instruction.
			  uAluSel		- Type of ALU instruction used with the test.
			  puInst		- Destination for the encoded instruction.
			  puValidFlags1	- Updated with the available precisions given
			  puValidFlags2   the ALU instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (uAluSel)
	{
		case EURASIA_USE0_TEST_ALUSEL_I8:
		{
			*puValidFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8;
			*puValidFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI10;

			if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
			{
				if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The .i8 and .i10 flags can't be specified together"));
				}

				puInst[1] |= (SGXVEC_USE1_TEST_PREC_ALUI8_I8 << SGXVEC_USE1_TEST_PREC_SHIFT);
			}
			else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
			{
				puInst[1] |= (SGXVEC_USE1_TEST_PREC_ALUI8_I10 << SGXVEC_USE1_TEST_PREC_SHIFT);
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "For a %s instruction with test and the .msk flag either .i8 or .i10 flags must be present", OpcodeName(psInst->uOpcode)));
			}
			break;
		}
		case EURASIA_USE0_TEST_ALUSEL_I16:
		{
			*puValidFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16 | USEASM_OPFLAGS2_MOVC_FMTI32;

			if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI16)
			{
				if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI32)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The .i16 and .i32 flags can't be specified together"));
				}

				puInst[1] |= (SGXVEC_USE1_TEST_PREC_ALUI16_I16 << SGXVEC_USE1_TEST_PREC_SHIFT);
			}
			else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI32)
			{
				puInst[1] |= (SGXVEC_USE1_TEST_PREC_ALUI16_I32 << SGXVEC_USE1_TEST_PREC_SHIFT);
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "For a %s instruction with test and the .msk flag either .i16 or .i32 flags must be present", OpcodeName(psInst->uOpcode)));
			}
			break;
		}
		case EURASIA_USE0_TEST_ALUSEL_F32:
		{
			/*
				The PREC field has already been encoded.
			*/
			break;
		}
		case EURASIA_USE0_TEST_ALUSEL_BITWISE:
		{
			puInst[1] |= (SGXVEC_USE1_TEST_PREC_ALUBITWISE_BITWISE << SGXVEC_USE1_TEST_PREC_SHIFT);
			break;
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

/*****************************************************************************
 FUNCTION	: CheckTestSourceBankRestrictions

 PURPOSE	: Checks a source in cases where the ALU OP restricts that
			  sources to the register banks valid in source 0.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Input instruction.
			  psSource			- Source to check.
			  pszSource			- Name of the source to check.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID CheckTestSourceBankRestrictions(PUSEASM_CONTEXT	psContext,
												PUSE_INST		psInst,
												PUSE_REGISTER	psSource,
												IMG_PCHAR		pszSource)
{
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(pszSource);

	if (!IsValidSrc0Bank(psSource, NULL, NULL))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "For %s with test the type of %s must be "
					   "temporary, output, primary attribute, secondary attribute or internal",
					   OpcodeName(psInst->uOpcode),
					   pszSource));
	}
}

/*****************************************************************************
 FUNCTION	: EncodeTestInstruction

 PURPOSE	: Encode a test instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeTestInstruction(PCSGX_CORE_DESC psTarget,
									  PUSE_INST psInst, 
									  IMG_PUINT32 puInst,
									  PUSEASM_CONTEXT psContext)
{
	IMG_UINT32 uTestMask = (psInst->uTest & ~USEASM_TEST_MASK_CLRMSK) >> USEASM_TEST_MASK_SHIFT;
	IMG_BOOL bBitwise = FALSE;
	IMG_UINT32 uSrc1ArgIndex, uSrc2ArgIndex;
	IMG_BOOL bSigned = FALSE;
	IMG_BOOL bCFASEL = IMG_FALSE;
	IMG_BOOL bSFASEL = IMG_FALSE;
	IMG_BOOL bRestrictedSource1Banks = IMG_FALSE;
	IMG_BOOL bRestrictedSource2Banks = IMG_FALSE;
	IMG_BOOL bAllowSatModifier = IMG_FALSE;
	IMG_UINT32 uFmtControlFlag = 0;
	IMG_UINT32 uValidArg1Flags = 0;
	IMG_UINT32 uValidArg2Flags = 0;
	IMG_UINT32 uValidDestFlags = USEASM_ARGFLAGS_DISABLEWB;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidFlags1;
	IMG_UINT32 uValidFlags2;
	IMG_UINT32 uChanSel = (psInst->uTest & ~USEASM_TEST_CHANSEL_CLRMSK) >> USEASM_TEST_CHANSEL_SHIFT;
	IMG_BOOL bVectorAluOp;
	IMG_UINT32 uAluSel;

	/* Check instruction validity. */
	uValidFlags1 = USEASM_OPFLAGS1_SKIPINVALID | 
				   USEASM_OPFLAGS1_SYNCSTART |
				   (~USEASM_OPFLAGS1_MASK_CLRMSK) | 
				   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
				   (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				   USEASM_OPFLAGS1_PARTIAL | 
				   USEASM_OPFLAGS1_TESTENABLE;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	uValidFlags2 = USEASM_OPFLAGS2_ONCEONLY | USEASM_OPFLAGS2_SAT;

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags2 & USEASM_OPFLAGS2_ONCEONLY) ? EURASIA_USE1_TEST_ONCEONLY : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_TEST_NOPAIR_NOSCHED : 0);
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
	{
		if (IsEnhancedNoSched(psTarget))
		{
			puInst[1] |= EURASIA_USE1_TEST_NOPAIR_SAT;
		}
		else
		{
			puInst[1] |= EURASIA_USE1_TEST_SAT;
		}
	}
	if (psInst->uFlags1 & USEASM_OPFLAGS1_PARTIAL)
	{
		puInst[1] |= EURASIA_USE1_TEST_PARTIAL;
	}
	if (uTestMask != USEASM_TEST_MASK_NONE)
	{
		puInst[1] |= (EURASIA_USE1_OP_TESTMASK << EURASIA_USE1_OP_SHIFT);
		uSrc1ArgIndex = 1;
	}
	else
	{
		puInst[1] |= (EURASIA_USE1_OP_TEST << EURASIA_USE1_OP_SHIFT);
		uSrc1ArgIndex = 2;
	}

	/* Encode the test opcode. */
	EncodeTestInstruction_AluOp(psTarget, 
								psInst, 
								uSrc1ArgIndex,
								puInst, 
								psContext, 
								&bSFASEL, 
								&bCFASEL, 
								&bSigned, 
								&bRestrictedSource1Banks,
								&bRestrictedSource2Banks,
								&bVectorAluOp,
								&bAllowSatModifier,
								&bBitwise,
								&uAluSel);
	puInst[0] |= (uAluSel << EURASIA_USE0_TEST_ALUSEL_SHIFT);
	
	/*
		The SAT flag is only valid for certain ALU opcodes.
	*/
	if (!bAllowSatModifier && (psInst->uFlags2 & USEASM_OPFLAGS2_SAT))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Saturation modifier is only available on 16-bit integer instructions with test"));
	}

	/*
		The partial flag is only valid for bitwise ALU opcodes.
	*/
	if (!bBitwise && (psInst->uFlags1 & USEASM_OPFLAGS1_PARTIAL))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Partial modifier is only available on bitwise instructions"));
	}

	/*
		What's the index of the second source in the argument array?
	*/
	if (OpcodeArgumentCount(psInst->uOpcode) >= 3)
	{
		if (bVectorAluOp)
		{
			uSrc2ArgIndex = uSrc1ArgIndex + 2;
		}
		else
		{
			uSrc2ArgIndex = uSrc1ArgIndex + 1;
		}
	}
	else if (psInst->uOpcode == USEASM_OP_FFRC)
	{
		uSrc2ArgIndex = uSrc1ArgIndex;
	}
	else
	{
		uSrc2ArgIndex = USE_UNDEF;
	}

	/* Encode repeat count and mask */
	EncodeTestInstruction_Repeat(psTarget, psInst, uTestMask, puInst, psContext);

	/* Encode test type. */
	puInst[1] |= ((psInst->uTest & ~USEASM_TEST_SIGN_CLRMSK) >> USEASM_TEST_SIGN_SHIFT) << EURASIA_USE1_TEST_STST_SHIFT;
	puInst[1] |= ((psInst->uTest & ~USEASM_TEST_ZERO_CLRMSK) >> USEASM_TEST_ZERO_SHIFT) << EURASIA_USE1_TEST_ZTST_SHIFT;
	puInst[1] |= (psInst->uTest & USEASM_TEST_CRCOMB_AND) ? EURASIA_USE1_TEST_CRCOMB_AND : 0;
	if (uTestMask == USEASM_TEST_MASK_NONE)
	{
		IMG_UINT32	uChanCC;

		switch (uChanSel)
		{
			case USEASM_TEST_CHANSEL_C0: uChanCC = EURASIA_USE1_TEST_CHANCC_SELECT0; break;
			case USEASM_TEST_CHANSEL_C1: uChanCC = EURASIA_USE1_TEST_CHANCC_SELECT1; break;
			case USEASM_TEST_CHANSEL_C2: uChanCC = EURASIA_USE1_TEST_CHANCC_SELECT2; break;
			case USEASM_TEST_CHANSEL_C3: uChanCC = EURASIA_USE1_TEST_CHANCC_SELECT3; break;
			case USEASM_TEST_CHANSEL_ANDALL: uChanCC = EURASIA_USE1_TEST_CHANCC_ANDALL; break;
			case USEASM_TEST_CHANSEL_ORALL: uChanCC = EURASIA_USE1_TEST_CHANCC_ORALL; break;
			case USEASM_TEST_CHANSEL_AND02: 
			{
				#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
				if (bVectorAluOp)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, ".and02 isn't supported on this instruction"));
				}
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
				uChanCC = EURASIA_USE1_TEST_CHANCC_AND02; 
				break;
			}
			case USEASM_TEST_CHANSEL_OR02: 
			{
				#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
				if (bVectorAluOp)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, ".or02 isn't supported on this instruction"));
				}
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
				uChanCC = EURASIA_USE1_TEST_CHANCC_OR02; 
				break;
			}
			case USEASM_TEST_CHANSEL_PERCHAN:
			{
				#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
				if (bVectorAluOp)
				{
					uChanCC = SGXVEC_USE1_TEST_CHANCC_PERCHAN;
				}
				else
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, ".cperchan isn't supported on this instruction"));
					uChanCC = 0;
				}
				break;
			}
			default:
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Unknown channel combine option"));
				uChanCC = 0;
				break;
			}
		}

		puInst[1] |= (uChanCC << EURASIA_USE1_TEST_CHANCC_SHIFT);
	}
	else
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (SupportsVEC34(psTarget))
		{
			switch (uTestMask)
			{
				case USEASM_TEST_MASK_BYTE: 
				{
					puInst[1] |= (SGXVEC_USE1_TESTMASK_TSTTYPE_8BITMASK << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				case USEASM_TEST_MASK_PREC: 
				{
					puInst[1] |= (SGXVEC_USE1_TESTMASK_TSTTYPE_PRECMASK << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				case USEASM_TEST_MASK_NUM: 
				{
					if (uAluSel != EURASIA_USE0_TEST_ALUSEL_F32)
					{
						USEASM_ERRMSG((psContext->pvContext, psInst, ".num is only supported by floating point vector "
							"ALU instructions"));
					}
					puInst[1] |= (SGXVEC_USE1_TESTMASK_TSTTYPE_NUMMASK << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Unknown test mask type"));
					break;
				}
			}

			if (uTestMask == USEASM_TEST_MASK_PREC || uTestMask == USEASM_TEST_MASK_NUM)
			{
				EncodeTestInstructionPrec(psContext,
										  psInst,
										  uAluSel,
										  puInst,
										  &uValidFlags1,
										  &uValidFlags2);
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			switch (uTestMask)
			{
				case USEASM_TEST_MASK_BYTE: 
				{
					puInst[1] |= (EURASIA_USE1_TESTMASK_TSTTYPE_4CHAN << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				case USEASM_TEST_MASK_WORD: 
				{
					puInst[1] |= (EURASIA_USE1_TESTMASK_TSTTYPE_2CHAN << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				case USEASM_TEST_MASK_DWORD: 
				{
					puInst[1] |= (EURASIA_USE1_TESTMASK_TSTTYPE_1CHAN << EURASIA_USE1_TESTMASK_TSTTYPE_SHIFT); 
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Unknown test mask type"));
					break;
				}
			}
		}
	}
	/* Encode predicate destination. */
	if (uTestMask == USEASM_TEST_MASK_NONE)
	{
		if (psInst->asArg[1].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the predicate destination for a test"));
		}
		if (psInst->asArg[1].uType != USEASM_REGTYPE_PREDICATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second destination for a test instruction must be a predicate register"));
		}
		if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Predicate registers are not indexable"));
		}
		if (psInst->asArg[1].uNumber >= EURASIA_USE_PREDICATE_BANK_SIZE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum predicate register number is %u", EURASIA_USE_PREDICATE_BANK_SIZE - 1));
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (
				uChanSel == USEASM_TEST_CHANSEL_PERCHAN &&
				psInst->asArg[1].uNumber != 0
		   )
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "For a %s instruction with test and per-channel result the predicate destination must be p0",
						   OpcodeName(psInst->uOpcode)));
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		puInst[1] |= (psInst->asArg[1].uNumber << EURASIA_USE1_TEST_PDST_SHIFT);
	}
	/* Is the destination disabled. */
	if (!(psInst->asArg[0].uFlags & USEASM_ARGFLAGS_DISABLEWB)) 
	{
		puInst[0] |= EURASIA_USE0_TEST_WBEN;
	}

	/*
		Check if U8/C10 or F16/F32 is enabled.
	*/
	if (bCFASEL || bSFASEL)
	{		
		if (bCFASEL)
		{
			uFmtControlFlag = USEASM_ARGFLAGS_FMTC10;
		}
		else
		{
			uFmtControlFlag = USEASM_ARGFLAGS_FMTF16;
		}
		uValidArg1Flags = uFmtControlFlag;
		uValidArg2Flags = uFmtControlFlag;
		uValidDestFlags |= uFmtControlFlag;

		if (
				(
					bCFASEL &&
					(psInst->asArg[0].uFlags & uFmtControlFlag)
				) ||
				(psInst->asArg[uSrc1ArgIndex].uFlags & uFmtControlFlag) ||
				(
					uSrc2ArgIndex != USE_UNDEF && 
					(psInst->asArg[uSrc2ArgIndex].uFlags & uFmtControlFlag)
				)
		   )
		{
			bFmtControl = IMG_TRUE;
			puInst[1] |= EURASIA_USE1_TEST_SFASEL;
		}
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	/*
		Encode the negate source modifier.	
	*/
	if (SupportsVEC34(psTarget) &&
		uAluSel == EURASIA_USE0_TEST_ALUSEL_F32)
	{
		if (	
				uSrc1ArgIndex != USE_UNDEF &&
				(psInst->asArg[uSrc1ArgIndex].uFlags & USEASM_ARGFLAGS_NEGATE)
		   )
		{
			uValidArg1Flags |= USEASM_ARGFLAGS_NEGATE;
			puInst[1] |= SGXVEC_USE1_TEST_NEGATESRC1;
		}

		/*
			VADD/VSUB support a negate source modifier source 2 by changing
			the ALU opcode.
		*/
		if (
				psInst->uOpcode == USEASM_OP_VADD ||
				psInst->uOpcode == USEASM_OP_VF16ADD ||
				psInst->uOpcode == USEASM_OP_VSUB ||
				psInst->uOpcode == USEASM_OP_VF16SUB
		   )
		{
			uValidArg2Flags |= USEASM_ARGFLAGS_NEGATE;
		}
	}

	/*
		For these instructions the FLTF16 flag on the source argument
		chooses between the F16 and F32 ALU versions.
	*/
	if (
			SupportsVEC34(psTarget) &&
			(
				psInst->uOpcode == USEASM_OP_VRCP ||
				psInst->uOpcode == USEASM_OP_VRSQ ||
				psInst->uOpcode == USEASM_OP_VLOG ||
				psInst->uOpcode == USEASM_OP_VEXP
			)
		)
	{
		uValidArg1Flags |= USEASM_ARGFLAGS_FMTF16;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/*
		Check for an unsupported modifiers on the instruction.
	*/
	CheckFlags(psContext, psInst, uValidFlags1, uValidFlags2, 0);

	/* Encode instruction sources. */
	CheckArgFlags(psContext, psInst, 0, uValidDestFlags);
	if (!(psInst->asArg[0].uFlags & USEASM_ARGFLAGS_DISABLEWB))
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (bVectorAluOp)
		{
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_DBEXT,
										  puInst + 0,
										  puInst + 1,
										  SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
										  EURASIA_USE0_DST_SHIFT,
										  USEASM_HWARG_DESTINATION,
										  0 /* uArg */,
										  psTarget);
			
			if (uTestMask == USEASM_TEST_MASK_BYTE && psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "%s is not a valid destination type for %s", 
					GetRegisterTypeName(&psInst->asArg[0]), OpcodeName(psInst->uOpcode))); 
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			/*
				From the bug tracker:

				The test mask generated by the TM instruction is always 8 bits per channel for 
				both 8 and 10 bit data types
			*/
			if (
					FixBRN27984(psTarget) &&
					uTestMask != USEASM_TEST_MASK_NONE &&
					bCFASEL &&
					(psInst->asArg[0].uFlags & USEASM_ARGFLAGS_FMTC10) != 0
			    )
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A C10 format destination isn't supported for the "
					"TESTMASK instruction on this core"));
			}

			EncodeDest(psContext, 
					   psInst, 
					   IMG_TRUE /* bAllowExtended */, 
					   puInst + 0, 
					   puInst + 1, 
					   bFmtControl, 
					   uFmtControlFlag, 
					   psTarget);
		}
	}
	else
	{
		/*
			Set the destination to a read-only special register if writebacks
			are disabled.
		*/
		puInst[1] |= (EURASIA_USE1_D1EXTBANK_SPECIAL << EURASIA_USE1_D1BANK_SHIFT) | 
					 EURASIA_USE1_DBEXT;
		puInst[0] |= (0 << EURASIA_USE0_DST_SHIFT);
	}

	/*
		Check extra restrictions on the source register banks imposed by the ALU opcode.
	*/
	if (bRestrictedSource1Banks)
	{
		CheckTestSourceBankRestrictions(psContext, psInst, &psInst->asArg[uSrc1ArgIndex], "source 1");
	}
	if (bRestrictedSource2Banks)
	{
		CheckTestSourceBankRestrictions(psContext, psInst, &psInst->asArg[uSrc2ArgIndex], "source 2");
	}

	CheckArgFlags(psContext, psInst, uSrc1ArgIndex, uValidArg1Flags);
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (bVectorAluOp)
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S1BEXT,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
									  EURASIA_USE0_SRC1_SHIFT,
									  USEASM_HWARG_SOURCE1,
									  uSrc1ArgIndex,
									  psTarget);

		CheckIdentitySwizzle(psInst, uSrc1ArgIndex + 1, IMG_TRUE /* bVec4 */, psContext);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		EncodeSrc1(psContext, 
				   psInst, 
				   uSrc1ArgIndex, 
				   IMG_TRUE /* bAllowExtended */, 
				   EURASIA_USE1_S1BEXT, 
				   bSigned, 
				   puInst + 0, 
				   puInst + 1, 
				   bBitwise, 
				   bFmtControl, 
				   uFmtControlFlag,
				   psTarget);
	}
	if (uSrc2ArgIndex != USE_UNDEF)
	{
		CheckArgFlags(psContext, psInst, uSrc2ArgIndex, uValidArg2Flags);
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (bVectorAluOp)
		{
			if (IsValidSrc2MappingToSrc0Bank(psContext, psInst, &psInst->asArg[uSrc2ArgIndex]))
			{
				EncodeArgumentDoubleRegisters(psContext,
											  psInst,
											  IMG_TRUE /* bAllowExtended */,
											  EURASIA_USE1_S2BEXT,
											  puInst + 0,
											  puInst + 1,
											  SGXVEC_USE_TEST_VEC_REGISTER_FIELD_LENGTH,
											  EURASIA_USE0_SRC2_SHIFT,
											  USEASM_HWARG_SOURCE2,
											  uSrc2ArgIndex,
											  psTarget);
			}

			if (CheckSwizzleArgument(psInst, uSrc2ArgIndex + 1, psContext))
			{
				PUSE_REGISTER	psSwizzleArg = &psInst->asArg[uSrc2ArgIndex + 1];
				if (psSwizzleArg->uNumber == USEASM_SWIZZLE(X, X, X, X))
				{
					puInst[1] |= SGXVEC_USE1_TEST_VSCOMP;
				}
				else if (psSwizzleArg->uNumber != USEASM_SWIZZLE(X, Y, Z, W))
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Only swizzle(xyzw) or swizzle(xxxx) is valid for argument %d to a %s instruction", uSrc2ArgIndex + 1, OpcodeName(psInst->uOpcode)));
				}
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			EncodeSrc2(psContext, 
					   psInst,
					   uSrc2ArgIndex, 
					   IMG_TRUE, 
					   EURASIA_USE1_S2BEXT, 
					   bSigned, 
					   puInst + 0, 
					   puInst + 1, 
					   bBitwise, 
					   bFmtControl, 
					   uFmtControlFlag,
					   psTarget);
		}
	}
	else
	{
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeFloatInstruction

 PURPOSE	: Encode a floating point instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFloatInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uOp, uOp2, uValidFlags1, uValidFlags2, uArgValidFlags = 0, uArgCount, uGroup = 0;
	IMG_BOOL	bSFASEL = IMG_FALSE;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Get the opcode. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_FMAD: uOp = EURASIA_USE1_OP_FARITH; uOp2 = EURASIA_USE1_FLOAT_OP2_MAD; uArgCount = 3; break;
		case USEASM_OP_FADM: uOp = EURASIA_USE1_OP_FARITH; uOp2 = EURASIA_USE1_FLOAT_OP2_ADM; uArgCount = 3; break;
		case USEASM_OP_FMSA: uOp = EURASIA_USE1_OP_FARITH; uOp2 = EURASIA_USE1_FLOAT_OP2_MSA; uArgCount = 3; break;
		case USEASM_OP_FSUBFLR: uOp = EURASIA_USE1_OP_FARITH; uOp2 = EURASIA_USE1_FLOAT_OP2_FRC; uArgCount = 2; break;
		case USEASM_OP_FRCP: uOp = EURASIA_USE1_OP_FSCALAR; uOp2 = EURASIA_USE1_FLOAT_OP2_RCP; uArgCount = 1; break;
		case USEASM_OP_FRSQ: uOp = EURASIA_USE1_OP_FSCALAR; uOp2 = EURASIA_USE1_FLOAT_OP2_RSQ; uArgCount = 1; break;
		case USEASM_OP_FLOG: uOp = EURASIA_USE1_OP_FSCALAR; uOp2 = EURASIA_USE1_FLOAT_OP2_LOG; uArgCount = 1; break;
		case USEASM_OP_FEXP: uOp = EURASIA_USE1_OP_FSCALAR; uOp2 = EURASIA_USE1_FLOAT_OP2_EXP; uArgCount = 1; break;
		case USEASM_OP_FDP: 
		{
			uOp = EURASIA_USE1_OP_FDOTPRODUCT; 
			uOp2 = EURASIA_USE1_FLOAT_OP2_DP; 
			uArgCount = 2; 
			break;
		}
		case USEASM_OP_FMIN: uOp = EURASIA_USE1_OP_FMINMAX; uOp2 = EURASIA_USE1_FLOAT_OP2_MIN; uArgCount = 2; break;
		case USEASM_OP_FMAX: uOp = EURASIA_USE1_OP_FMINMAX; uOp2 = EURASIA_USE1_FLOAT_OP2_MAX; uArgCount = 2; break;
		case USEASM_OP_FDSX: uOp = EURASIA_USE1_OP_FGRADIENT; uOp2 = EURASIA_USE1_FLOAT_OP2_DSX; uArgCount = 2; break;
		case USEASM_OP_FDSY: uOp = EURASIA_USE1_OP_FGRADIENT; uOp2 = EURASIA_USE1_FLOAT_OP2_DSY; uArgCount = 2; break;
		case USEASM_OP_FMAD16: uOp = EURASIA_USE1_OP_FARITH16; uOp2 = EURASIA_USE1_FLOAT_OP2_FMAD16; uArgCount = 3; break;
		#if defined(SUPPORT_SGX_FEATURE_USE_FCLAMP)
		case USEASM_OP_FMINMAX: 
		{
			if (!IsFClampInstruction(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "fminmax isn't supported on this processor"));
			}
			uOp = EURASIA_USE1_OP_FMINMAX; 
			uOp2 = SGX545_USE1_FLOAT_OP2_MINMAX; 
			uArgCount = 3; 
			break;
		}
		case USEASM_OP_FMAXMIN: 
		{
			if (!IsFClampInstruction(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "fmaxmin isn't supported on this processor"));
			}
			uOp = EURASIA_USE1_OP_FMINMAX; 
			uOp2 = SGX545_USE1_FLOAT_OP2_MAXMIN; 
			uArgCount = 3; 
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FCLAMP) */
		#if defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS)
		case USEASM_OP_FSQRT:
		{
			if (!IsSqrtSinCosInstruction(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "fsqrt isn't supported on this processor"));
			}
			uOp = EURASIA_USE1_OP_FSCALAR;
			uOp2 = SGX545_USE1_FLOAT_OP2_SQRT;
			uArgCount = 1;
			uGroup = SGX545_USE1_FSCALAR_GROUP;
			break;
		}
		case USEASM_OP_FSIN:
		{
			if (!IsSqrtSinCosInstruction(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "fsin isn't supported on this processor"));
			}
			uOp = EURASIA_USE1_OP_FSCALAR;
			uOp2 = SGX545_USE1_FLOAT_OP2_SIN;
			uArgCount = 1;
			uGroup = SGX545_USE1_FSCALAR_GROUP;
			break;
		}
		case USEASM_OP_FCOS:
		{
			if (!IsSqrtSinCosInstruction(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "fcos isn't supported on this processor"));
			}
			uOp = EURASIA_USE1_OP_FSCALAR;
			uOp2 = SGX545_USE1_FLOAT_OP2_COS;
			uArgCount = 1;
			uGroup = SGX545_USE1_FSCALAR_GROUP;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS) */
		default: IMG_ABORT();
	}
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (uOp << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(uOp2 << EURASIA_USE1_FLOAT_OP2_SHIFT) |
				uGroup;
	/* Encode repeat count and mask */
	EncodeFloatRepeats(psTarget, puInst, psInst, uOp, psContext);
	/* Check instruction validity. */
	uValidFlags1 = 0;
	if (!(SupportsFmad16Swizzles(psTarget) && psInst->uOpcode == USEASM_OP_FMAD16))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_END;
	}
	uValidFlags2 = 0;
	if (uOp == EURASIA_USE1_OP_FSCALAR)
	{
		uValidFlags2 |= USEASM_OPFLAGS2_TYPEPRESERVE;
	}
	if (IsPerInstMoeIncrements(psTarget) && uOp != EURASIA_USE1_OP_FSCALAR)
	{
		uValidFlags2 |= USEASM_OPFLAGS2_PERINSTMOE | 
					   USEASM_OPFLAGS2_PERINSTMOE_INCS0 |
					   USEASM_OPFLAGS2_PERINSTMOE_INCS1 |
					   USEASM_OPFLAGS2_PERINSTMOE_INCS2;
	}
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
		USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
		(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK) | uValidFlags1, uValidFlags2, 0);
	if (uOp == EURASIA_USE1_OP_FSCALAR)
	{
		IMG_UINT32 uSrcComp = (psInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;

		if (psInst->uFlags2 & USEASM_OPFLAGS2_TYPEPRESERVE)
		{
			puInst[1] |= EURASIA_USE1_FSCALAR_TYPEPRESERVE;
		}

		if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTC10)
		{
			puInst[1] |= (EURASIA_USE1_FSCALAR_DTYPE_C10 << EURASIA_USE1_FSCALAR_DTYPE_SHIFT);
			puInst[1] |= (uSrcComp << EURASIA_USE1_FSCALAR_CHANSEL_SHIFT);
		}
		else if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTF16)
		{
			puInst[1] |= (EURASIA_USE1_FSCALAR_DTYPE_F16 << EURASIA_USE1_FSCALAR_DTYPE_SHIFT);
			if (uSrcComp == 0)
			{
				puInst[1] |= (EURASIA_USE1_FSCALAR_CHANSEL_LOWWORD << EURASIA_USE1_FSCALAR_CHANSEL_SHIFT);
			}
			else if (uSrcComp == 2)
			{
				puInst[1] |= (EURASIA_USE1_FSCALAR_CHANSEL_HIGHWORD << EURASIA_USE1_FSCALAR_CHANSEL_SHIFT);
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .0 and .2 are valid component selects for an f16 source argument to a scalar operation"));
			}
		}
		else
		{
			puInst[1] |= (EURASIA_USE1_FSCALAR_DTYPE_F32 << EURASIA_USE1_FSCALAR_DTYPE_SHIFT);
			if (uSrcComp != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .0 is a valid component selects for an f32 source argument to a scalar operation"));
			}
		}
	}
	else if (uOp != EURASIA_USE1_OP_FARITH16)
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < uArgCount; uArg++)
		{
			if (psInst->asArg[1 + uArg].uFlags & USEASM_ARGFLAGS_FMTF16)
			{
				bSFASEL = IMG_TRUE;
			}
		}
		if (bSFASEL)
		{
			puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
			uArgValidFlags = USEASM_ARGFLAGS_FMTF16;
		}
	}
	
	/* Encode the banks and offsets of the sources and destination. */
	if (psInst->uOpcode == USEASM_OP_FMAD || psInst->uOpcode == USEASM_OP_FADM || psInst->uOpcode == USEASM_OP_FMSA ||
		psInst->uOpcode == USEASM_OP_FMAD16 ||
		psInst->uOpcode == USEASM_OP_FMINMAX || psInst->uOpcode == USEASM_OP_FMAXMIN)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES)
		/* Encode fmad16 swizzles. */
		if (SupportsFmad16Swizzles(psTarget) && psInst->uOpcode == USEASM_OP_FMAD16)
		{
			IMG_UINT32	uArg;

			uArgValidFlags |= (~USEASM_ARGFLAGS_MAD16SWZ_CLRMSK);

			for (uArg = 0; uArg < 3; uArg++)
			{
				IMG_UINT32	uArgSwizzle;
				
				uArgSwizzle =
					(psInst->asArg[1 + uArg].uFlags & ~USEASM_ARGFLAGS_MAD16SWZ_CLRMSK) >> USEASM_ARGFLAGS_MAD16SWZ_SHIFT;
				switch (uArg)
				{
					case 0: puInst[1] |= (uArgSwizzle << SGX545_USE1_FARITH16_SRC0SWZ_SHIFT); break;
					case 1:
					{
						IMG_UINT32	uLow, uHigh;

						uLow = ((uArgSwizzle >> SGX545_USE1_FARITH16_SRC1SWZL_INTERNALSHIFT) << SGX545_USE1_FARITH16_SRC1SWZL_SHIFT);
						puInst[1] |= (uLow & ~SGX545_USE1_FARITH16_SRC1SWZL_CLRMSK);
						uHigh = ((uArgSwizzle >> SGX545_USE1_FARITH16_SRC1SWZH_INTERNALSHIFT) << SGX545_USE1_FARITH16_SRC1SWZH_SHIFT);
						puInst[1] |= (uHigh & ~SGX545_USE1_FARITH16_SRC1SWZH_CLRMSK);
						break;
					}
					case 2: puInst[1] |= (uArgSwizzle << SGX545_USE1_FARITH16_SRC2SWZ_SHIFT); break;
				}
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FMAD16_SWIZZLES) */

		CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
		EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
		EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget); 
		/* Encode the source modifiers. */
		#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
		if (IsPerInstMoeIncrements(psTarget))
		{
			IMG_UINT32 uArg2Flags = psInst->asArg[2].uFlags;
			IMG_UINT32 uArg3Flags = psInst->asArg[3].uFlags;
			if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_NEGATE)
			{
				if (psInst->uOpcode == USEASM_OP_FMAD || psInst->uOpcode == USEASM_OP_FMAD16)
				{
					uArg2Flags ^= USEASM_ARGFLAGS_NEGATE;
				}
				else if (psInst->uOpcode == USEASM_OP_FADM)
				{
					/* (-A + B) * C -> (A - B) * -C */
					uArg2Flags ^= USEASM_ARGFLAGS_NEGATE;
					uArg3Flags ^= USEASM_ARGFLAGS_NEGATE;
				}
				else if (psInst->uOpcode == USEASM_OP_FMSA)
				{
					/* Source 0 is squared so a negate flag is ignored. */
				}
				else
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "A negate flag on source 0 isn't supported on this instruction and processor"));
				}
			}
			if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
			{
				puInst[1] |= SGX545_USE1_FLOAT_SRC0ABS;
			}
			puInst[1] |= (EncodeFloatMod(uArg2Flags) << EURASIA_USE1_SRC1MOD_SHIFT) |
						 (EncodeFloatMod(uArg3Flags) << EURASIA_USE1_SRC2MOD_SHIFT);
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
		{
			puInst[1] |= (EncodeFloatMod(psInst->asArg[1].uFlags) << EURASIA_USE1_SRC0MOD_SHIFT) |
						 (EncodeFloatMod(psInst->asArg[2].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT) |
						 (EncodeFloatMod(psInst->asArg[3].uFlags) << EURASIA_USE1_SRC2MOD_SHIFT);
		}
	}
	else if (psInst->uOpcode == USEASM_OP_FDP || psInst->uOpcode == USEASM_OP_FMIN || 
			 psInst->uOpcode == USEASM_OP_FMAX || psInst->uOpcode == USEASM_OP_FSUBFLR ||
			 psInst->uOpcode == USEASM_OP_FDSX || psInst->uOpcode == USEASM_OP_FDSY)
	{
		CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
		EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget); 
		/* Encode the source modifiers. */
		puInst[1] |= (EncodeFloatMod(psInst->asArg[1].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT) |
					 (EncodeFloatMod(psInst->asArg[2].uFlags) << EURASIA_USE1_SRC2MOD_SHIFT);
	}
	else
	{
		if (uOp == EURASIA_USE1_OP_FSCALAR)
		{
			CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | 
									 USEASM_ARGFLAGS_FMTC10 | USEASM_ARGFLAGS_FMTF16 |
									 (~USEASM_ARGFLAGS_COMP_CLRMSK));
		}
		else
		{
			CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
		}
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
		/* Encode the source modifiers. */
		puInst[1] |= (EncodeFloatMod(psInst->asArg[1].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT);
	}
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
}

#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE)
/*****************************************************************************
 FUNCTION	: EncodeNormaliseInstruction

 PURPOSE	: Encode a normalise instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeNormaliseInstruction(PCSGX_CORE_DESC		psTarget,
										   PUSE_INST			psInst, 
										   IMG_PUINT32			puInst,
										   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uDRCArg;
	IMG_UINT32	uSrcModArg;
	IMG_UINT32	uSrcMod;
	IMG_UINT32	uArg;

	if (!SupportsFnormalise(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The normalise instruction isn't supported on this core"));
	}
	if (
			psInst->uOpcode == USEASM_OP_FNRM32 &&
			(
				FixBRN25580(psTarget) ||
				FixBRN25582(psTarget)
			)
	    )
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "fnrm32 isn't supported on this core"));
	}

	CheckFlags(psContext, 
			   psInst, 
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_SYNCSTART | 
			   USEASM_OPFLAGS1_NOSCHED | 
			   (~USEASM_OPFLAGS1_PRED_CLRMSK), 
			   0,
			   0);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (SGX540_USE1_OP_FNRM << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0);

	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_PRIMATTR &&
		psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for a normalise must be the primary attribute bank or the temporary bank"));
	}
	EncodeDest(psContext, psInst, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);

	if (psInst->uOpcode == USEASM_OP_FNRM16)
	{
		IMG_UINT32	uChan;

		puInst[1] |= SGX540_USE1_FNRM_F16DP;

		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 2, 0);

		EncodeSrc1(psContext,
				   psInst,
				   1,
				   IMG_TRUE,
				   EURASIA_USE1_S1BEXT,
				   IMG_FALSE,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE,
				   IMG_FALSE,
				   0,
				   psTarget);
		EncodeSrc2(psContext,
				   psInst,
				   2,
				   IMG_TRUE,
				   EURASIA_USE1_S2BEXT,
				   IMG_FALSE,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE,
				   IMG_FALSE,
				   0,
				   psTarget);

		if (psInst->asArg[3].uType != USEASM_REGTYPE_SWIZZLE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to a fnrm16 instruction must be a swizzle"));
		}
		if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A swizzle can't be accessed in indexed mode"));
		}
		if (psInst->asArg[3].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
		}
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uChanSel;
			
			uChanSel = (psInst->asArg[3].uNumber >> (uChan * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;
			
			if (uChanSel == USEASM_SWIZZLE_SEL_2 || uChanSel == USEASM_SWIZZLE_SEL_HALF)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid swizzle component select"));
			}
			switch (uChan)
			{
				case 0:
				{
					puInst[1] |= (uChanSel << SGX540_USE1_FNRM_F16C0SWIZ_SHIFT);
					break;
				}
				case 1:
				{
					puInst[1] |= (uChanSel << SGX540_USE1_FNRM_F16C1SWIZ_SHIFT);
					break;
				}
				case 2:
				{
					puInst[0] |= (uChanSel << SGX540_USE0_FNRM_F16C2SWIZ_SHIFT);
					break;
				}
				case 3:
				{
					puInst[0] |= (uChanSel << SGX540_USE0_FNRM_F16C3SWIZ_SHIFT);
					break;
				}
			}
		}

		uSrcModArg = 4;
	}
	else
	{
		IMG_BOOL	bSFASEL;

		/*
			Check if we need format select for this instruction.
		*/
		bSFASEL = IMG_FALSE;
		for (uArg = 0; uArg < 3; uArg++)
		{
			if (psInst->asArg[1 + uArg].uFlags & USEASM_ARGFLAGS_FMTF16)
			{
				bSFASEL = IMG_TRUE;
			}
		}
		if (bSFASEL)
		{
			puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
		}

		CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_FMTF16);
		CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_FMTF16);
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_FMTF16);

		EncodeSrc0(psContext,
				   psInst,
				   1,
				   IMG_TRUE,
				   puInst + 0,
				   puInst + 1,
				   EURASIA_USE1_S0BEXT,
				   bSFASEL,
				   USEASM_ARGFLAGS_FMTF16,
				   psTarget);
		EncodeSrc1(psContext,
				   psInst,
				   2,
				   IMG_TRUE,
				   EURASIA_USE1_S1BEXT,
				   IMG_FALSE,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE,
				   bSFASEL,
				   USEASM_ARGFLAGS_FMTF16,
				   psTarget);
		EncodeSrc2(psContext,
				   psInst,
				   3,
				   IMG_TRUE,
				   EURASIA_USE1_S2BEXT,
				   IMG_FALSE,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE,
				   bSFASEL,
				   USEASM_ARGFLAGS_FMTF16,
				   psTarget);

		uSrcModArg = 4;
	}

	uSrcMod = 0;
	for (uArg = 0; uArg < 2; uArg++)
	{
		PUSE_REGISTER	psArg = &psInst->asArg[uSrcModArg + uArg];
		IMG_UINT32		uIntSrcSel = (uArg == 0) ? USEASM_INTSRCSEL_SRCNEG : USEASM_INTSRCSEL_SRCABS;
		IMG_UINT32		uFlag = (uArg == 0) ? EURASIA_USE_SRCMOD_NEGATE : EURASIA_USE_SRCMOD_ABSOLUTE;

		if (psArg->uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "Argument %d to %s should be an integer source selector",
						   uSrcModArg + uArg,
						   OpcodeName(psInst->uOpcode)));
		}
		if (psArg->uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "No flags are valid for argument %d to %s",
						   uSrcModArg + uArg,
						   OpcodeName(psInst->uOpcode)));
		}
		if (psArg->uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "Integer source selectors aren't indexable"));
		}
		if (psArg->uNumber == uIntSrcSel)
		{
			uSrcMod |= uFlag;
		}
		else if (psArg->uNumber != USEASM_INTSRCSEL_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "Invalid choice for argument %d to %s",
						   uSrcModArg + uArg, 
						   OpcodeName(psInst->uOpcode)));
		}
	}
	puInst[1] |= (uSrcMod << SGX540_USE1_FNRM_SRCMOD_SHIFT);
	uDRCArg = uSrcModArg + 2;

	/* Encode the dependent read counter argument. */
	if (psInst->asArg[uDRCArg].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The last argument to a normalise instruction must be in the dependent read counter bank"));
	}
	if (psInst->asArg[uDRCArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, 
					   "A dependent read counter register can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uDRCArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "No flags are valid for a dependent read counter register"));
	}
	if (psInst->asArg[uDRCArg].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The maximum number of a drc register is %d",
					   EURASIA_USE_DRC_BANK_SIZE - 1));
	}
	puInst[1] |= psInst->asArg[uDRCArg].uNumber << SGX540_USE1_FNRM_DRCSEL_SHIFT;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) */

/*****************************************************************************
 FUNCTION	: EncodeSOP2Instruction

 PURPOSE	: Encode a SOP2 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSOP2Instruction(PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext,
									  PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32 uCMod1, uCMod2;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
		(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MAINISSUE, 0, 0);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SOP2 << EURASIA_USE1_OP_SHIFT) |
				 (EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}

	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);

	if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to sop2 should be an integer source selector"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fourth argument to sop2"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	if (psInst->asArg[3].uNumber == USEASM_INTSRCSEL_COMP)
	{
		puInst[0] |= (EURASIA_USE0_SOP2_SRC1MOD_COMPLEMENT << EURASIA_USE0_SOP2_SRC1MOD_SHIFT);
	}
	else if (psInst->asArg[3].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choices for the fourth argument to sop2 - valid choices are comp or none"));
	}

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop2 should be an integer source selector"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the fifth argument to sop2"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod1 = EURASIA_USE1_SOP2_CMOD1_COMPLEMENT;
	}
	else
	{
		uCMod1 = EURASIA_USE1_SOP2_CMOD1_NONE;
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_ZERO << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_ZERO << EURASIA_USE1_SOP2_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP2_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP2_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_SRC1 << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_SRC1ALPHA << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_SRC2 << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_SRC2ALPHA << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2SCALE: puInst[1] |= (EURASIA_USE1_SOP2_CSEL1_SRC2DESTX2 << EURASIA_USE1_SOP2_CSEL1_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fifth argument to sop2 - valid choices are zero, one, asat, rasat, s1, s1a, s2, s2a or s2scale")); break;
	}
	puInst[1] |= (uCMod1 << EURASIA_USE1_SOP2_CMOD1_SHIFT);

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to sop2 should be an integer source selector"));
	}
	if ((psInst->asArg[5].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the sixth argument to sop2"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod2 = EURASIA_USE1_SOP2_CMOD2_COMPLEMENT;
	}
	else
	{
		uCMod2 = EURASIA_USE1_SOP2_CMOD2_NONE;
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_ZERO << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_ZERO << EURASIA_USE1_SOP2_CSEL2_SHIFT);
			uCMod2 ^= EURASIA_USE1_SOP2_CMOD2_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2_CSEL2_SHIFT);
			uCMod2 ^= EURASIA_USE1_SOP2_CMOD2_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_SRC1 << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_SRC1ALPHA << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_SRC2 << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_SRC2ALPHA << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_ZEROS: puInst[1] |= (EURASIA_USE1_SOP2_CSEL2_ZEROSRC2MINUSHALF << EURASIA_USE1_SOP2_CSEL2_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the sixth argument to sop2 - valid choices are zero, one, asat, rasat, s1, s1a, s2, s2a or zeros")); break;
	}
	puInst[1] |= (uCMod2 << EURASIA_USE1_SOP2_CMOD2_SHIFT);

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to sop2 should be an integer source selector"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to sop2"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_ADD: puInst[0] |= (EURASIA_USE0_SOP2_COP_ADD << EURASIA_USE0_SOP2_COP_SHIFT); break;
		case USEASM_INTSRCSEL_SUB: puInst[0] |= (EURASIA_USE0_SOP2_COP_SUB << EURASIA_USE0_SOP2_COP_SHIFT); break;
		case USEASM_INTSRCSEL_MIN: puInst[0] |= (EURASIA_USE0_SOP2_COP_MIN << EURASIA_USE0_SOP2_COP_SHIFT); break;
		case USEASM_INTSRCSEL_MAX: puInst[0] |= (EURASIA_USE0_SOP2_COP_MAX << EURASIA_USE0_SOP2_COP_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the seventh argument to sop2 - valid choices are add, sub, min or max")); break;
	}	

	if (!(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE) || (psInst->psNext == NULL))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "sop2 must be coissued"));
	}
	else
	{
		PUSE_INST psCoissueInst = psInst->psNext;
		IMG_UINT32 uAMod1, uAMod2;

		if (psCoissueInst->uOpcode != USEASM_OP_ASOP2)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "sop2 must be coissued with asop2"));
		}

		if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to asop2 should be an integer source selector"));
		}
		if (psCoissueInst->asArg[0].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the first argument to asop2"));
		}
		if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
		}
		if (psCoissueInst->asArg[0].uNumber == USEASM_INTSRCSEL_COMP)
		{
			puInst[0] |= (EURASIA_USE0_SOP2_ASRC1MOD_COMPLEMENT << EURASIA_USE0_SOP2_ASRC1MOD_SHIFT);
		}

		if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to asop2 should be an integer source selector"));
		}
		if ((psCoissueInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the second argument to asop2"));
		}
		if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
		}
		if (psCoissueInst->asArg[1].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uAMod1 = EURASIA_USE1_SOP2_AMOD1_COMPLEMENT;
		}
		else
		{
			uAMod1 = EURASIA_USE1_SOP2_AMOD1_NONE;
		}
		switch (psCoissueInst->asArg[1].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2_ASEL1_ZERO << EURASIA_USE1_SOP2_ASEL1_SHIFT); break;
			case USEASM_INTSRCSEL_ONE:
			{
				puInst[1] |= (EURASIA_USE1_SOP2_ASEL1_ZERO << EURASIA_USE1_SOP2_ASEL1_SHIFT);
				uAMod1 ^= EURASIA_USE1_SOP2_AMOD1_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_ASEL1_SRC1ALPHA << EURASIA_USE1_SOP2_ASEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_ASEL1_SRC2ALPHA << EURASIA_USE1_SOP2_ASEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2SCALE: puInst[1] |= (EURASIA_USE1_SOP2_ASEL1_SRC2ALPHAX2 << EURASIA_USE1_SOP2_ASEL1_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to asop2 - valid choices are zero, one, s1a, s2a, s2scale")); break;
		}
		puInst[1] |= (uAMod1 << EURASIA_USE1_SOP2_AMOD1_SHIFT);

		if (psCoissueInst->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to asop2 should be an integer source selector"));
		}
		if ((psCoissueInst->asArg[2].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the third argument to asop2"));
		}
		if (psCoissueInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
		}
		if (psCoissueInst->asArg[2].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uAMod2 = EURASIA_USE1_SOP2_AMOD2_COMPLEMENT;
		}
		else
		{
			uAMod2 = EURASIA_USE1_SOP2_AMOD2_NONE;
		}
		switch (psCoissueInst->asArg[2].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2_ASEL2_ZERO << EURASIA_USE1_SOP2_ASEL2_SHIFT); break;
			case USEASM_INTSRCSEL_ONE:
			{
				puInst[1] |= (EURASIA_USE1_SOP2_ASEL2_ZERO << EURASIA_USE1_SOP2_ASEL2_SHIFT);
				uAMod2 ^= EURASIA_USE1_SOP2_AMOD2_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_ASEL2_SRC1ALPHA << EURASIA_USE1_SOP2_ASEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2_ASEL2_SRC2ALPHA << EURASIA_USE1_SOP2_ASEL2_SHIFT); break;
			case USEASM_INTSRCSEL_ZEROS: puInst[1] |= (EURASIA_USE1_SOP2_ASEL2_ZEROSRC2MINUSHALF << EURASIA_USE1_SOP2_ASEL2_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the third argument to asop2 - valid choices are zero, one, s1a, s2a, zeros")); break;
		}
		puInst[1] |= (uAMod2 << EURASIA_USE1_SOP2_AMOD2_SHIFT);

		if (psCoissueInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to asop2 should be an integer source selector"));
		}
		if (psCoissueInst->asArg[3].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fourth argument to asop2"));
		}
		if (psCoissueInst->asArg[3].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
		}
		switch (psCoissueInst->asArg[3].uNumber)
		{
			case USEASM_INTSRCSEL_ADD: puInst[0] |= (EURASIA_USE0_SOP2_AOP_ADD << EURASIA_USE0_SOP2_AOP_SHIFT); break;
			case USEASM_INTSRCSEL_SUB: puInst[0] |= (EURASIA_USE0_SOP2_AOP_SUB << EURASIA_USE0_SOP2_AOP_SHIFT); break;
			case USEASM_INTSRCSEL_MIN: puInst[0] |= (EURASIA_USE0_SOP2_AOP_MIN << EURASIA_USE0_SOP2_AOP_SHIFT); break;
			case USEASM_INTSRCSEL_MAX: puInst[0] |= (EURASIA_USE0_SOP2_AOP_MAX << EURASIA_USE0_SOP2_AOP_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fourth argument to asop2 - valid choices are add, sub, min and max")); break;
		}

		if (psCoissueInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop2 should be an integer source selector"));
		}
		if (psCoissueInst->asArg[4].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to sop2"));
		}
		if (psCoissueInst->asArg[4].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
		}
		if (psCoissueInst->asArg[4].uNumber == USEASM_INTSRCSEL_NEG)
		{
			puInst[0] |= (EURASIA_USE0_SOP2_ADSTMOD_NEGATE << EURASIA_USE0_SOP2_ADSTMOD_SHIFT);
		}
		else if (psCoissueInst->asArg[4].uNumber != USEASM_INTSRCSEL_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fifth argument to asop2 - valid choices are neg or none"));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodePackInstruction

 PURPOSE	: Encode a PACK instruction.

 PARAMETERS	: psTarget			- Assembly target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodePackInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst,
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32 uDestFmt;
	IMG_UINT32 uSrcFmt;
	IMG_UINT32 uByteMask = (psInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK) >> USEASM_ARGFLAGS_BYTEMSK_SHIFT;
	IMG_UINT32 uSrc0Comp = (psInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	IMG_UINT32 uSrc1Comp = (psInst->asArg[2].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	IMG_BOOL bSigned = FALSE;
	IMG_BOOL bNoScale = FALSE;
	IMG_BOOL bRound = FALSE;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
		USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
		USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) | USEASM_OPFLAGS1_ROUNDENABLE, USEASM_OPFLAGS2_SCALE, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[1] = (EURASIA_USE1_OP_PCKUNPCK << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_PCK_NOSCHED : 0);
	puInst[0] = ((psInst->uFlags2 & USEASM_OPFLAGS2_SCALE) ? EURASIA_USE0_PCK_SCALE : 0);
	/* Encode round-enable. */
	#if defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES)
	puInst[0] |= ((psInst->uFlags1 & USEASM_OPFLAGS1_ROUNDENABLE) ? SGX545_USE0_PCK_ROUNDENABLE : 0);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES) */
	/* Encode repeat count and mask */
	if (uRptCount > 0)
	{
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
	}
	else
	{
		puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
	}
	/* Encode the source and destination formats. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_PCKF16F32: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; bNoScale = TRUE; bRound = TRUE; break;
		case USEASM_OP_PCKF16F16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; bNoScale = TRUE; bRound = TRUE; break;
		case USEASM_OP_PCKF16U16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; break;
		case USEASM_OP_PCKF16S16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; break;
		case USEASM_OP_PCKU16F32: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKU16F16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKU16U16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKU16S16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_PCKS16F32: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKS16F16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKS16U16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKS16S16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_PCKU8F32: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKU8F16: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKU8U16: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKU8S16: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_PCKS8F32: uDestFmt = EURASIA_USE1_PCK_FMT_S8; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKS8F16: uDestFmt = EURASIA_USE1_PCK_FMT_S8; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKS8U16: uDestFmt = EURASIA_USE1_PCK_FMT_S8; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKS8S16: uDestFmt = EURASIA_USE1_PCK_FMT_S8; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_PCKO8F32: uDestFmt = EURASIA_USE1_PCK_FMT_O8; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKO8F16: uDestFmt = EURASIA_USE1_PCK_FMT_O8; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKO8U16: uDestFmt = EURASIA_USE1_PCK_FMT_O8; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKO8S16: uDestFmt = EURASIA_USE1_PCK_FMT_O8; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_PCKC10F32: uDestFmt = EURASIA_USE1_PCK_FMT_C10; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; break;
		case USEASM_OP_PCKC10F16: uDestFmt = EURASIA_USE1_PCK_FMT_C10; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_PCKC10U16: uDestFmt = EURASIA_USE1_PCK_FMT_C10; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_PCKC10S16: uDestFmt = EURASIA_USE1_PCK_FMT_C10; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKF32F32: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_F32; bNoScale = TRUE; bRound = IMG_TRUE; break;
		case USEASM_OP_UNPCKF32F16: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; bNoScale = TRUE; bRound = IMG_TRUE; break;
		case USEASM_OP_UNPCKF32U16: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; break;
		case USEASM_OP_UNPCKF32S16: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; break;
		case USEASM_OP_UNPCKF32U8: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; break;
		case USEASM_OP_UNPCKF32S8: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_S8; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF32O8: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_O8; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF32C10: uDestFmt = EURASIA_USE1_PCK_FMT_F32; uSrcFmt = EURASIA_USE1_PCK_FMT_C10; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF16F16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; bNoScale = TRUE; bRound = TRUE; break;
		case USEASM_OP_UNPCKF16U16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; break;
		case USEASM_OP_UNPCKF16S16: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF16U8: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; break;
		case USEASM_OP_UNPCKF16S8: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_S8; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF16O8: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_O8; bSigned = TRUE; break;
		case USEASM_OP_UNPCKF16C10: uDestFmt = EURASIA_USE1_PCK_FMT_F16; uSrcFmt = EURASIA_USE1_PCK_FMT_C10; bSigned = TRUE; break;
		case USEASM_OP_UNPCKU16F16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_UNPCKU16U16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU16S16: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU16U8: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU16S8: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_S8; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU16O8: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_O8; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU16C10: uDestFmt = EURASIA_USE1_PCK_FMT_U16; uSrcFmt = EURASIA_USE1_PCK_FMT_C10; bSigned = TRUE; break;
		case USEASM_OP_UNPCKS16F16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_F16; break;
		case USEASM_OP_UNPCKS16U16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_U16; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS16S16: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_S16; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS16U8: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS16S8: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_S8; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS16O8: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_O8; bSigned = TRUE; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS16C10: uDestFmt = EURASIA_USE1_PCK_FMT_S16; uSrcFmt = EURASIA_USE1_PCK_FMT_C10; bSigned = TRUE; break;
		case USEASM_OP_UNPCKU8U8: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKU8S8: uDestFmt = EURASIA_USE1_PCK_FMT_U8; uSrcFmt = EURASIA_USE1_PCK_FMT_S8; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKS8U8: uDestFmt = EURASIA_USE1_PCK_FMT_S8; uSrcFmt = EURASIA_USE1_PCK_FMT_U8; bNoScale = TRUE; break;
		case USEASM_OP_UNPCKC10C10: uDestFmt = EURASIA_USE1_PCK_FMT_C10; uSrcFmt = EURASIA_USE1_PCK_FMT_C10; break;
		default: IMG_ABORT();
	}
	if (bNoScale && (psInst->uFlags2 & USEASM_OPFLAGS2_SCALE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The scale flag is only valid when converting between integer and floating point formats"));
	}
	if (!bRound && (psInst->uFlags1 & USEASM_OPFLAGS1_ROUNDENABLE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The renable flag is only valid when converting between floating point formats"));
	}
	if (!HasPackMultipleRoundModes(psTarget) && (psInst->uFlags1 & USEASM_OPFLAGS1_ROUNDENABLE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The renable flag isn't supported on this processor"));
	}

	puInst[1] |= (uDestFmt << EURASIA_USE1_PCK_DSTF_SHIFT);
	puInst[1] |= (uSrcFmt << EURASIA_USE1_PCK_SRCF_SHIFT);
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 1, ~USEASM_ARGFLAGS_COMP_CLRMSK);
	CheckArgFlags(psContext, psInst, 2, ~USEASM_ARGFLAGS_COMP_CLRMSK);
	CheckArgFlags(psContext, psInst, 0, ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK);
	EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Check the consistency of the bytemask with the destination register type. */
	CheckByteMask(psContext, psInst, uByteMask, uDestFmt == EURASIA_USE1_PCK_FMT_C10);
	/* Check the consistency of the bytemask and the destination bytemask. */
	switch (uDestFmt)
	{
		case EURASIA_USE1_PCK_FMT_U16:
		case EURASIA_USE1_PCK_FMT_S16:
		case EURASIA_USE1_PCK_FMT_F16:
		{
			if (uByteMask != 0xf && uByteMask != 0xc && uByteMask != 0x3)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid bytemask specified (legal values are .bytemask1111, .bytemask0011, .bytemask1100)"));
			}
			break;
		}
		case EURASIA_USE1_PCK_FMT_F32:
		{
			if (uByteMask != 0xf)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid bytemask specified (legal values are .bytemask1111)"));
			}
			break;
		}
	}
	/* Encode destination bytemask and source components. */
	puInst[1] |= (uByteMask << EURASIA_USE1_PCK_DMASK_SHIFT);
	puInst[0] |= (uSrc0Comp << EURASIA_USE0_PCK_S1SCSEL_SHIFT) |
				 (uSrc1Comp << EURASIA_USE0_PCK_S2SCSEL_SHIFT);
	/* Encode rounding mode. */
	if (bRound || (!bNoScale && (psInst->uFlags2 & USEASM_OPFLAGS2_SCALE) && psInst->uOpcode != USEASM_OP_UNPCKC10C10))
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES)
		IMG_UINT32 uRMode;
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES) */
		if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to a pack instruction should be an integer source selector"));
		}
		if (psInst->asArg[3].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an integer source selector"));
		}
		if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector can't be indexed"));
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES)
		switch (psInst->asArg[3].uNumber)
		{
			case USEASM_INTSRCSEL_ROUNDDOWN: uRMode = SGX545_USE0_PCK_RMODE_DOWN; break;
			case USEASM_INTSRCSEL_ROUNDUP: uRMode = SGX545_USE0_PCK_RMODE_UP; break;
			case USEASM_INTSRCSEL_ROUNDNEAREST: uRMode = SGX545_USE0_PCK_RMODE_NEAREST; break;
			case USEASM_INTSRCSEL_ROUNDZERO: uRMode = SGX545_USE0_PCK_RMODE_ZERO; break;
			default:
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Unsupported rounding mode"));
				uRMode = 0;
				break;
			}
		}
		if (!HasPackMultipleRoundModes(psTarget) && uRMode != SGX545_USE0_PCK_RMODE_NEAREST)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only nearest rounding is supported on this processor"));
		}
		puInst[0] |= (uRMode << SGX545_USE0_PCK_RMODE_SHIFT);
		#else /* defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES) */
		if (psInst->asArg[3].uNumber != USEASM_INTSRCSEL_ROUNDNEAREST)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only nearest rounding is supported on this processor"));
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_PACK_MULTIPLE_ROUNDING_MODES) */
	}

	/*
		Check special restrictions on an instruction loading a C10 vec4 into an internal register.
	*/
	if ((psInst->uFlags2 & USEASM_OPFLAGS2_SCALE) != 0 && psInst->uOpcode == USEASM_OP_UNPCKC10C10)
	{
		if (uByteMask != 0xf)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A PCKC10C10 with the scale flag doesn't support a destination write mask"));
		}
		if (psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A PCKC10C10 with the scale flag only supports an internal register as a destination"));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeBitwiseInstruction

 PURPOSE	: Encode a bitwise instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeBitwiseInstruction(PCSGX_CORE_DESC 		psTarget,
										 PUSE_INST				psInst, 
										 IMG_PUINT32			puInst,
										 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uOpcode, uOp2, uRot = 0;
	IMG_UINT32 uInvert = (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_INVERT);
	IMG_UINT32 uSrc2Immediate = 0;
	IMG_UINT32 uValidFlags;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	/*
		If the second argument is immediate then try to rotate it to fit in 16 bits.
	*/
	if ((psInst->asArg[2].uType == USEASM_REGTYPE_IMMEDIATE) && (psInst->uOpcode != USEASM_OP_RLP))
	{
		if (psInst->uFlags1 & USEASM_OPFLAGS1_PARTIAL)
		{
			uSrc2Immediate = psInst->asArg[2].uNumber;
		}
		else
		{
			IMG_UINT32	uPass;
			for (uPass = 0; uPass < 2; uPass++)
			{
				uRot = 0;
				uSrc2Immediate = psInst->asArg[2].uNumber;
				if (uPass == 1)
				{
					uSrc2Immediate = ~uSrc2Immediate;
					uInvert = !uInvert;
				}
				while (uSrc2Immediate > EURASIA_USE_BITWISE_MAXIMUM_UNROTATED_IMMEDIATE)
				{
					uRot++;
					if (uRot > EURASIA_USE1_BITWISE_SRC2ROT_MAXIMUM)
					{
						if (uPass == 1)
						{
							USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate source argument to a bitwise instruction must be equal to "
								"a 16-bit value rotated upto %d places within a 32-bit word", EURASIA_USE1_BITWISE_SRC2ROT_MAXIMUM));
						}
						break;
					}
					uSrc2Immediate = (uSrc2Immediate >> 1) | ((uSrc2Immediate & 0x1) << 31);
				}
				if (uRot < 32)
				{
					break;
				}
			}
		}
	}

	uValidFlags = 
		USEASM_OPFLAGS1_SKIPINVALID | 
		USEASM_OPFLAGS1_END | 
		(~USEASM_OPFLAGS1_PRED_CLRMSK) |
		(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
		USEASM_OPFLAGS1_SYNCSTART | 
		USEASM_OPFLAGS1_PARTIAL;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (!NoRepeatMaskOnBitwiseInstructions(psTarget))
	{
		uValidFlags |= (~USEASM_OPFLAGS1_MASK_CLRMSK);
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);

	/* Get the hardware opcode for this instruction. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_AND: uOpcode = EURASIA_USE1_OP_ANDOR; uOp2 = EURASIA_USE1_BITWISE_OP2_AND; break;
		case USEASM_OP_OR: uOpcode = EURASIA_USE1_OP_ANDOR; uOp2 = EURASIA_USE1_BITWISE_OP2_OR; break;
		case USEASM_OP_XOR: uOpcode = EURASIA_USE1_OP_XOR; uOp2 = EURASIA_USE1_BITWISE_OP2_XOR; break;
		case USEASM_OP_SHL: uOpcode = EURASIA_USE1_OP_SHLROL; uOp2 = EURASIA_USE1_BITWISE_OP2_SHL; break;
		case USEASM_OP_SHR: uOpcode = EURASIA_USE1_OP_SHRASR; uOp2 = EURASIA_USE1_BITWISE_OP2_SHR; break;
		case USEASM_OP_ASR: uOpcode = EURASIA_USE1_OP_SHRASR; uOp2 = EURASIA_USE1_BITWISE_OP2_ASR; break;
		case USEASM_OP_ROL: uOpcode = EURASIA_USE1_OP_SHLROL; uOp2 = EURASIA_USE1_BITWISE_OP2_ROL; break;
		case USEASM_OP_RLP: uOpcode = EURASIA_USE1_OP_RLP; uOp2 = EURASIA_USE1_BITWISE_OP2_RLP; break;
		default: IMG_ABORT();
	}
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (uOpcode << EURASIA_USE1_OP_SHIFT) |
				 (EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				 (uOp2 << EURASIA_USE1_BITWISE_OP2_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_PARTIAL) ? EURASIA_USE1_BITWISE_PARTIAL : 0) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_BITWISE_NOSCHED : 0) |
				 (uInvert ? EURASIA_USE1_BITWISE_SRC2INV : 0) |
				 (uRot << EURASIA_USE1_BITWISE_SRC2ROT_SHIFT);
	/* Encode repeat count and mask */
	#if defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK)
	if (NoRepeatMaskOnBitwiseInstructions(psTarget))
	{
		if (uRptCount > 0)
		{
			puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_BITWISE_RCOUNT_SHIFT);
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BITWISE_NO_REPEAT_MASK) */
	{
		if (uRptCount > 0)
		{
			puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
		}
		else
		{
			puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
		}
	}
	/* Encode the banks and offsets of the sources and destination. */
	if (psInst->uOpcode == USEASM_OP_RLP)
	{
		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 0, 0);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, TRUE, IMG_FALSE, 0, psTarget);
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		if (psInst->asArg[2].uType != USEASM_REGTYPE_PREDICATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to rlp must be a predicate"));
		}
		if (psInst->asArg[2].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the predicate argument to rlp"));
		}
		if (psInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Predicate registers can't be indexed"));
		}
		if (psInst->asArg[2].uNumber >= EURASIA_USE_PREDICATE_BANK_SIZE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum predicate number is %u", EURASIA_USE_PREDICATE_BANK_SIZE - 1));
		}
		puInst[0] |= psInst->asArg[2].uNumber << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
	}
	else
	{
		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 0, 0);
		CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_INVERT);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, TRUE, IMG_FALSE, 0, psTarget);
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		if (psInst->asArg[2].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, TRUE, IMG_FALSE, 0, psTarget);
		}
		else
		{
			puInst[1] |= EURASIA_USE1_S2BEXT;
			puInst[0] |= (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);
			puInst[0] |= ((uSrc2Immediate & 0x007F) >> 0) << EURASIA_USE0_SRC2_SHIFT;
			puInst[0] |= ((uSrc2Immediate & 0x3F80) >> 7) << EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_SHIFT;
			puInst[1] |= ((uSrc2Immediate & 0xC000) >> 14) << EURASIA_USE1_BITWISE_SRC2IEXTH_SHIFT;
		}
	}
}

static IMG_VOID IntSrcSel(PUSE_REGISTER psArg, IMG_UINT32 uSel, IMG_BOOL bComplement)
/*****************************************************************************
 FUNCTION	: IntSrcSel

 PURPOSE	: Initialize an integer source selector argument.

 PARAMETERS	: psArg			- Argument to initialize.
			  uSel			- Type of source selector.
			  bComplement	- If TRUE apply a complement to the source
							selector.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psArg->uType = USEASM_REGTYPE_INTSRCSEL;
	psArg->uNumber = uSel;
	psArg->uIndex = USEREG_INDEX_NONE;
	psArg->uFlags = bComplement ? USEASM_ARGFLAGS_COMPLEMENT : 0;
}

/*****************************************************************************
 FUNCTION	: ExpandMacros

 PURPOSE	: Expand instruction macros into their final form.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  psTemp			- Written with the expanded instruction.

 RETURNS	: Nothing.
*****************************************************************************/
static PUSE_INST ExpandMacros(PCSGX_CORE_DESC	psTarget, 
							  PUSE_INST			psInst, 
							  PUSE_INST			psTemp,
							  IMG_PUINT32		puRptCount,
							  PUSEASM_CONTEXT	psContext)
{
	/* Convert fmul a, b, c to fmad a, b, c, #0.0f */
	if (psInst->uOpcode == USEASM_OP_FMUL)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMAD;
		psTemp->asArg[3] = g_sSourceZero;
		return psTemp;
	}
	/* Convert fadd a, b, c to fmad a, b, #1.0f, c */
	else if (psInst->uOpcode == USEASM_OP_FADD)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMAD;
		psTemp->asArg[3] = psTemp->asArg[2];
		psTemp->asArg[2] = g_sSourceOne;
		return psTemp;
	}
	/* Convert fsub a, b, c to fmad a, b, #1.0f, -c */
	else if (psInst->uOpcode == USEASM_OP_FSUB)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMAD;
		psTemp->asArg[3] = psTemp->asArg[2];
		psTemp->asArg[3].uFlags ^= USEASM_ARGFLAGS_NEGATE;
		psTemp->asArg[2] = g_sSourceOne;
		return psTemp;
	}
	/* Convert single-issue fmov a, b to fmad a, b, #1.0f, #0.0f  */
	else if (psInst->uOpcode == USEASM_OP_FMOV)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMAD;
		psTemp->asArg[2] = g_sSourceOne;
		psTemp->asArg[3] = g_sSourceZero;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IADD16 || psInst->uOpcode == USEASM_OP_ISUB16 ||
			 psInst->uOpcode == USEASM_OP_IADDU16 || psInst->uOpcode == USEASM_OP_ISUBU16)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_IMA16;
		psTemp->asArg[3] = psInst->asArg[2];
		if (psInst->uOpcode == USEASM_OP_ISUB16 || psInst->uOpcode == USEASM_OP_ISUBU16)
		{
			psTemp->asArg[3].uFlags ^= USEASM_ARGFLAGS_NEGATE;
		}
		psTemp->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[2].uNumber = 1;
		psTemp->asArg[2].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[2].uFlags = 0;

		if (psInst->uOpcode == USEASM_OP_IADD16 || psInst->uOpcode == USEASM_OP_ISUB16)
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
		}
		else
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
		}

		psTemp->asArg[4].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[4].uFlags = 0;
		psTemp->asArg[4].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[4].uNumber = USEASM_INTSRCSEL_U16;

		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uFlags = 0;
		psTemp->asArg[5].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_U16;

		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IMUL16 || psInst->uOpcode == USEASM_OP_IMULU16)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_IMA16;
		psTemp->asArg[3].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[3].uNumber = 0;
		psTemp->asArg[3].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[3].uFlags = 0;
		
		if (psInst->uOpcode == USEASM_OP_IMUL16)
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
		}
		else
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
		}

		psTemp->asArg[4].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[4].uFlags = 0;
		psTemp->asArg[4].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[4].uNumber = USEASM_INTSRCSEL_U16;

		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uFlags = 0;
		psTemp->asArg[5].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_U16;

		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IADD32 || psInst->uOpcode == USEASM_OP_IADDU32)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_IMAE;
		psTemp->asArg[3] = psInst->asArg[2];
		psTemp->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[2].uNumber = 1;
		psTemp->asArg[2].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[2].uFlags = USEASM_ARGFLAGS_LOW;

		if (psInst->uOpcode == USEASM_OP_IADD32)
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
		}
		else
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
		}

		psTemp->asArg[4].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[4].uFlags = 0;
		psTemp->asArg[4].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[4].uNumber = USEASM_INTSRCSEL_U32;

		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uFlags = 0;
		psTemp->asArg[5].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_NONE;

		psTemp->asArg[6].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[6].uFlags = 0;
		psTemp->asArg[6].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[6].uNumber = USEASM_INTSRCSEL_NONE;

		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_ISUB32 ||
			 psInst->uOpcode == USEASM_OP_ISUBU32)
	{
		*psTemp = *psInst;

		psTemp->uOpcode = USEASM_OP_IMA32;

		psTemp->uFlags3 |= USEASM_OPFLAGS3_STEP1;

		if (psInst->uOpcode == USEASM_OP_ISUB32)
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_SIGNED;
		}
		else
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_UNSIGNED;
		}

		/*
			No carry-out destination.
		*/
		UseAsmInitReg(&psTemp->asArg[1]);
		psTemp->asArg[1].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[1].uNumber = USEASM_INTSRCSEL_NONE;

		/*
			Convert
				ISUB32/ISUBU32		DEST, SRC1, SRC2
			->
				IMA32				DEST, NONE, SRC1, #1, SRC2, NONE
		*/
		psTemp->asArg[2] = psInst->asArg[1];

		UseAsmInitReg(&psTemp->asArg[3]);
		psTemp->asArg[3].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[3].uNumber = 1;

		psTemp->asArg[4] = psInst->asArg[2];
		psTemp->asArg[4].uFlags ^= USEASM_ARGFLAGS_NEGATE;

		/*
			No carry-in source.
		*/
		UseAsmInitReg(&psTemp->asArg[5]);
		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_NONE;

		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IADD8 || 
			 psInst->uOpcode == USEASM_OP_ISUB8 ||
			 psInst->uOpcode == USEASM_OP_IADDU8 || 
			 psInst->uOpcode == USEASM_OP_ISUBU8)
	{
		*psTemp = *psInst;
		if (psInst->uOpcode == USEASM_OP_IADDU8 || 
			psInst->uOpcode == USEASM_OP_ISUBU8)
		{
			psTemp->uFlags2 |= USEASM_OPFLAGS2_SAT;
		}
		psTemp->uOpcode = USEASM_OP_IMA8;
		if (psInst->uOpcode == USEASM_OP_ISUB8 || psInst->uOpcode == USEASM_OP_ISUBU8)
		{
			psTemp->asArg[1] = psInst->asArg[2];
			psTemp->asArg[3] = psInst->asArg[1];	
			psTemp->asArg[1].uFlags |= USEASM_ARGFLAGS_NEGATE;
		}
		else
		{
			psTemp->asArg[3] = psTemp->asArg[2];
		}
		psTemp->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[2].uNumber = 1;
		psTemp->asArg[2].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[2].uFlags = 0;
		psTemp->asArg[4].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[4].uNumber = USEASM_INTSRCSEL_SRC0;
		psTemp->asArg[4].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[4].uFlags = 0;
		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_SRC1;
		psTemp->asArg[5].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[5].uFlags = 0;
		psTemp->asArg[6].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[6].uNumber = USEASM_INTSRCSEL_SRC2;
		psTemp->asArg[6].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[6].uFlags = 0;
		psTemp->asArg[7].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[7].uNumber = USEASM_INTSRCSEL_SRC0ALPHA;
		psTemp->asArg[7].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[7].uFlags = 0;
		psTemp->asArg[8].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[8].uNumber = USEASM_INTSRCSEL_SRC1ALPHA;
		psTemp->asArg[8].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[8].uFlags = 0;
		psTemp->asArg[9].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[9].uNumber = USEASM_INTSRCSEL_SRC2ALPHA;
		psTemp->asArg[9].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[9].uFlags = 0;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IMUL8 || psInst->uOpcode == USEASM_OP_IMULU8)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "imul8/imulu8 is only valid with an attached test"));
	}
	else if (psInst->uOpcode == USEASM_OP_FPADD8 || psInst->uOpcode == USEASM_OP_FPSUB8)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "fpadd8/fpsub8 is only valid with an attached test"));
	}
	else if (psInst->uOpcode == USEASM_OP_FPMUL8)
	{
		*psTemp = *psInst;
		psTemp->asArg[3].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[3].uNumber = USEASM_INTSRCSEL_SRC1;
		psTemp->asArg[3].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[3].uFlags = 0;
		psTemp->asArg[4].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[4].uNumber = USEASM_INTSRCSEL_ZERO;
		psTemp->asArg[4].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[4].uFlags = 0;
		psTemp->asArg[5].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[5].uNumber = USEASM_INTSRCSEL_ADD;
		psTemp->asArg[5].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[5].uFlags = 0;
		psTemp->asArg[6].uType = USEASM_REGTYPE_INTSRCSEL;
		psTemp->asArg[6].uNumber = USEASM_INTSRCSEL_ADD;
		psTemp->asArg[6].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[6].uFlags = 0;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_SABLND)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_SOP2WM;
		/* SEL1 */
		IntSrcSel(&psTemp->asArg[3], USEASM_INTSRCSEL_SRC1ALPHA, IMG_FALSE /* bComplement */);
		/* SEL2 */
		IntSrcSel(&psTemp->asArg[4], USEASM_INTSRCSEL_SRC1ALPHA, IMG_TRUE /* bComplement */);
		/* COP */
		IntSrcSel(&psTemp->asArg[5], USEASM_INTSRCSEL_ADD, IMG_FALSE /* bComplement */);
		/* AOP */
		IntSrcSel(&psTemp->asArg[6], USEASM_INTSRCSEL_ADD, IMG_FALSE /* bComplement */);
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_SDM)
	{
		if (HasNewEfoOptions(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't available on processors with the new EFO options"));
		}
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_EFO;
		psTemp->asArg[4].uNumber |= EURASIA_USE1_EFO_WI0EN |
								  EURASIA_USE1_EFO_WI1EN |
								  (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT) |
								  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
								  (EURASIA_USE1_EFO_ISRC_I0M0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_SDA)
	{
		if (HasNewEfoOptions(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't available on processors with the new EFO options"));
		}
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_EFO;
		psTemp->asArg[4].uNumber |= EURASIA_USE1_EFO_WI0EN |
								  EURASIA_USE1_EFO_WI1EN |
								  (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT) |
								  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
								  (EURASIA_USE1_EFO_ISRC_I0A0_I1A1 << EURASIA_USE1_EFO_ISRC_SHIFT);
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_AMM || psInst->uOpcode == USEASM_OP_SMM)
	{
		if (HasNewEfoOptions(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't available on processors with the new EFO options"));
		}
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_EFO;
		psTemp->asArg[4].uNumber |= EURASIA_USE1_EFO_WI0EN |
								  EURASIA_USE1_EFO_WI1EN |
								  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
								  (EURASIA_USE1_EFO_ASRC_M0SRC2_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT) |
								  (EURASIA_USE1_EFO_ISRC_I0A0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
		psTemp->asArg[4].uNumber = (psInst->uOpcode == USEASM_OP_SMM) ? EURASIA_USE1_EFO_A1LNEG : 0;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_AMS || psInst->uOpcode == USEASM_OP_SMS)
	{
		if (HasNewEfoOptions(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't available on processors with the new EFO options"));
		}
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_EFO;
		psTemp->asArg[4].uNumber |= EURASIA_USE1_EFO_WI0EN |
								  EURASIA_USE1_EFO_WI1EN |
								  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC0 << EURASIA_USE1_EFO_MSRC_SHIFT) |
								  (EURASIA_USE1_EFO_ASRC_M0SRC2_I1I0 << EURASIA_USE1_EFO_ASRC_SHIFT) |
								  (EURASIA_USE1_EFO_ISRC_I0A0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT);
		psTemp->asArg[4].uNumber = (psInst->uOpcode == USEASM_OP_SMS) ? EURASIA_USE1_EFO_A1LNEG : 0;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_DMA)
	{
		if (HasNewEfoOptions(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't available on processors with the new EFO options"));
		}
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_EFO;
		psTemp->asArg[4].uNumber |= EURASIA_USE1_EFO_WI0EN |
								  EURASIA_USE1_EFO_WI1EN |
								  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
								  (EURASIA_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT) |
								  (EURASIA_USE1_EFO_ISRC_I0A0_I1A1 << EURASIA_USE1_EFO_ISRC_SHIFT);
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FDP3 || psInst->uOpcode == USEASM_OP_FDP4)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FDP;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_REPEAT_CLRMSK;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT) != USEREG_MASK_X)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A mask is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_MASK_CLRMSK;
		(*puRptCount) = (psInst->uOpcode == USEASM_OP_FDP3 ? 3 : 4);
		psTemp->uFlags1 |= (*puRptCount) << USEASM_OPFLAGS1_REPEAT_SHIFT;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FDPC3 || psInst->uOpcode == USEASM_OP_FDPC4)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FDPC;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_REPEAT_CLRMSK;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT) != USEREG_MASK_X)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A mask is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_MASK_CLRMSK;
		(*puRptCount) = (psInst->uOpcode == USEASM_OP_FDPC3 ? 3 : 4);
		psTemp->uFlags1 |= (*puRptCount) << USEASM_OPFLAGS1_REPEAT_SHIFT;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FDDP3 || psInst->uOpcode == USEASM_OP_FDDP4)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FDDP;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_REPEAT_CLRMSK;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT) != USEREG_MASK_X)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A mask is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_MASK_CLRMSK;
		(*puRptCount) = (psInst->uOpcode == USEASM_OP_FDDP3 ? 3 : 4);
		psTemp->uFlags1 |= (*puRptCount) << USEASM_OPFLAGS1_REPEAT_SHIFT;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FDDPC3 || psInst->uOpcode == USEASM_OP_FDDPC4)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FDDPC;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_REPEAT_CLRMSK;
		if (((psTemp->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT) != USEREG_MASK_X)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A mask is not valid on a dotproduct instruction"));
		}
		psTemp->uFlags1 &= USEASM_OPFLAGS1_MASK_CLRMSK;
		(*puRptCount) = (psInst->uOpcode == USEASM_OP_FDDPC3 ? 3 : 4);
		psTemp->uFlags1 |= (*puRptCount) << USEASM_OPFLAGS1_REPEAT_SHIFT;
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FFRC)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FSUBFLR;
		psTemp->asArg[2] = psTemp->asArg[1];
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_IMOV16)
	{
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_UNPCKU16U16;
		psTemp->asArg[0].uFlags |= USEASM_ARGFLAGS_BYTEMSK_PRESENT;
		if (psTemp->asArg[0].uFlags & USEASM_ARGFLAGS_HIGH)
		{
			psTemp->asArg[0].uFlags |= (0xc << USEASM_ARGFLAGS_BYTEMSK_SHIFT);
		}
		else if (psTemp->asArg[0].uFlags & USEASM_ARGFLAGS_LOW)
		{
			psTemp->asArg[0].uFlags |= (0x3 << USEASM_ARGFLAGS_BYTEMSK_SHIFT);
		}
		else
		{
			psTemp->asArg[0].uFlags |= (0xF << USEASM_ARGFLAGS_BYTEMSK_SHIFT);
		}
		psTemp->asArg[0].uFlags &= ~(USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
		if (psTemp->asArg[1].uFlags & USEASM_ARGFLAGS_HIGH)
		{
			psTemp->asArg[1].uFlags |= (2 << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psTemp->asArg[1].uFlags &= ~(USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
		if (psTemp->asArg[2].uFlags & USEASM_ARGFLAGS_HIGH)
		{
			psTemp->asArg[2].uFlags |= (2 << USEASM_ARGFLAGS_COMP_SHIFT);
		}
		psTemp->asArg[2].uFlags &= ~(USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
		return psTemp;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_FCLAMP)
	else if (psInst->uOpcode == USEASM_OP_FMIN && IsFClampInstruction(psTarget))
	{
		/* D = min(X, Y) -> D = min(max(X, X), Y) */
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMINMAX;
		psTemp->asArg[3] = psTemp->asArg[2];
		psTemp->asArg[2] = psTemp->asArg[1];
		return psTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FMAX && IsFClampInstruction(psTarget))
	{
		/* D = max(X, Y) -> D = max(min(X, X), Y) */
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_FMAXMIN;
		psTemp->asArg[3] = psTemp->asArg[1];
		return psTemp;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_FCLAMP) */
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	else if (SupportsVEC34(psTarget) &&
			 psInst->uOpcode == USEASM_OP_MOV &&
			 psInst->asArg[0].uType != USEASM_REGTYPE_LINK &&
			 psInst->asArg[1].uType != USEASM_REGTYPE_LINK)
	{
		/* MOV D, S -> OR D, S, #0 */
		*psTemp = *psInst;
		psTemp->uOpcode = USEASM_OP_OR;
		psTemp->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psTemp->asArg[2].uNumber = 0;
		psTemp->asArg[2].uIndex = USEREG_INDEX_NONE;
		psTemp->asArg[2].uFlags = 0;
		return psTemp;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		
	return psInst;
}

#if defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
/*****************************************************************************
 FUNCTION	: CheckIMA32AsDualIssue

 PURPOSE	: Check that an IMA32 instruction has the right modes to be part
			  of a co-issued part.

 PARAMETERS	: psInst			- Instruction to check.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID CheckIMA32AsDualIssue(PUSE_INST				psInst, 
									  PUSEASM_CONTEXT	psContext)
{
	/* The second argument is the destination for the high bits of the result. It must be none for a coissued ima32. */
	if (
			!(
				psInst->asArg[1].uType == USEASM_REGTYPE_INTSRCSEL &&
				psInst->asArg[1].uNumber == USEASM_INTSRCSEL_NONE
			 )
	   )
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Ima32 as part of a co-issued instruction pair can only write one destination."));
	}
	if (
			!(
				psInst->asArg[5].uType == USEASM_REGTYPE_INTSRCSEL &&
				psInst->asArg[5].uNumber == USEASM_INTSRCSEL_NONE
			 )
	   )
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Ima32 as part of a co-issued instruction pair doesn't support carry-in or carry-out."));
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueInstruction

 PURPOSE	: Encode a dual issue instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueInstruction(PCSGX_CORE_DESC	psTarget,
										   PUSE_INST		psInst, 
										   IMG_PUINT32		puInst, 
										   PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32 uOp1, uOp1Ext, uOp2;
	const IMG_UINT32* puResourceTableOp1;
	IMG_BOOL bInst0UsesSrc0, bInst0UsesSrc2;
	IMG_BOOL bSFASEL = IMG_FALSE;
	IMG_UINT32 uArgValidFlags = 0;
	IMG_UINT32 uSrc0, uSrc1, uSrc2, uCoSrc0, uCoSrc1, uCoSrc2;
	PUSE_INST psCoissueInst;
	IMG_UINT32 uOp1SrcModFlag;
	static const IMG_UINT32 ppuResourceTable[][16] = 
	{
		/* FMAD	FMAD16 FMSA	FFRC FCLMP FRCP	FRSQ FEXP FLOG FSIN FCOS  FADD   FSUB  FMUL IMA32 MOV */
		{   1,	  0,	0,	  1,  0,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   1,	 1,   1},		/* FMAD */
		{   1,	  1,	1,	  1,  1,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   1,	 1,   1},		/* MOV */
		{   0,	  0,	0,	  1,  0,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   0,	 1,   1},		/* FADM */
		{   0,	  0,	0,	  1,  0,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   0,	 1,   1},		/* FMSA */
		{   0,	  0,	0,	  0,  0,     1,  1,	   1,	1,	 1,	 1,	   0,	   0,   0,	 1,   1},		/* FDDP */
		{   0,	  0,	0,	  0,  0,     1,  1,	   1,	1,	 1,	 1,	   0,	   0,   1,	 0,   1},		/* FCLMP */
		{   1,	  1,	1,	  1,  0,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   1,	 0,   1},		/* IMA32 */
		{   0,	  0,	0,	  1,  0,     1,  1,	   1,	1,	 1,	 1,	   1,	   1,   0,	 1,   1},		/* FSSQ */
	};

	if (!HasDualIssue(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Dual-issue isn't supported on this processor"));
		return;
	}

	psCoissueInst = psInst->psNext;   // The instruction co-issued with psInst.

	CheckFlags(psContext, 
			   psInst, 
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_SYNCSTART | 
			   USEASM_OPFLAGS1_NOSCHED |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK) |
			   USEASM_OPFLAGS1_MAINISSUE, 
			   0,
			   0);
	puInst[1] = (SGX545_USE1_OP_FDUAL << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, IMG_TRUE) << SGX545_USE1_FDUAL_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGX545_USE1_FDUAL_NOSCHED : 0);
	puInst[0] = 0;

	if (!(psInst->uOpcode == USEASM_OP_MOV || psInst->uOpcode == USEASM_OP_IMA32))
	{
		IMG_UINT32	uArg;
	
		for (uArg = 1; uArg < OpcodeArgumentCount(psInst->uOpcode); uArg++)
		{
			if (psInst->asArg[uArg].uFlags & USEASM_ARGFLAGS_FMTF16)
			{
				bSFASEL = IMG_TRUE;
			}
		}
		if (bSFASEL)
		{
			puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
			uArgValidFlags = USEASM_ARGFLAGS_FMTF16;
		}
	}

	/* Encode repeat count and mask */
	if (uRptCount > 0)
	{
		if (uRptCount > SGX545_USE1_FDUAL_RCNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count on a dual-issue instruction is %d", SGX545_USE1_FDUAL_RCNT_MAX));
		}
		puInst[1] |= ((uRptCount - 1) << SGX545_USE1_FDUAL_RCNT_SHIFT);
	}
	else
	{
		if (uMask == 3)
		{
			puInst[1] |= (1 << SGX545_USE1_FDUAL_RCNT_SHIFT);
		}
		else if (uMask != 1)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only masks of .x and .xy are supported on dual-issued instruction"));
		}
	}

	/*
		Encode the first opcode.
	*/
	uOp1SrcModFlag = USEASM_ARGFLAGS_NEGATE;
	switch (psInst->uOpcode)
	{
		case USEASM_OP_FMAD:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_STD_FMAD;
			uOp1Ext = 0;
			puResourceTableOp1 = ppuResourceTable[0];
			break;
		}
		case USEASM_OP_MOV:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_STD_MOV;
			uOp1Ext = 0;
			puResourceTableOp1 = ppuResourceTable[1];

			/*
				No negate modifier on MOV sources.
			*/
			uOp1SrcModFlag = 0;
			break;
		}
		case USEASM_OP_FADM:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_STD_FADM;
			uOp1Ext = 0;
			puResourceTableOp1 = ppuResourceTable[2];
			break;
		}
		case USEASM_OP_FMSA:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_STD_FMSA;
			uOp1Ext = 0;
			puResourceTableOp1 = ppuResourceTable[3];
			break;
		}
		case USEASM_OP_FDDP:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_EXT_FDDP;
			uOp1Ext = SGX545_USE1_FDUAL_OP1EXT;
			puResourceTableOp1 = ppuResourceTable[4];
			break;
		}
		case USEASM_OP_FMINMAX:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_EXT_FCLMP;
			uOp1Ext = SGX545_USE1_FDUAL_OP1EXT;
			puResourceTableOp1 = ppuResourceTable[5];
			break;
		}
		case USEASM_OP_IMA32:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_EXT_IMA32;
			uOp1Ext = SGX545_USE1_FDUAL_OP1EXT;
			puResourceTableOp1 = ppuResourceTable[6];

			CheckIMA32AsDualIssue(psInst, psContext);
			break;
		}
		case USEASM_OP_FSSQ:
		{
			uOp1 = SGX545_USE1_FDUAL_OP1_EXT_FSSQ;
			uOp1Ext = SGX545_USE1_FDUAL_OP1EXT;
			puResourceTableOp1 = ppuResourceTable[7];
			break;
		}	
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't supported as the first in a pair of coissued opcodes"));
			uOp1 = uOp1Ext = 0;
			puResourceTableOp1 = NULL;
			break;
		}
	}
	puInst[1] |= (uOp1 << SGX545_USE1_FDUAL_OP1_SHIFT) | uOp1Ext;

	/*
		Encode the destination to the first opcode.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);

	/*
		Check the second destination for DDP.
	*/
	if (psInst->uOpcode == USEASM_OP_FDDP)
	{
		if (!(psInst->asArg[1].uType == USEASM_REGTYPE_FPINTERNAL && 
			  psInst->asArg[1].uNumber == SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM &&
			  psInst->asArg[1].uIndex == USEREG_INDEX_NONE))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"The second destination for fddp as part of a co-issued instruction must be i%d", 
				SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM));
		}
	}

	/*
		Encode the sources to the first opcode.
	*/
	if (psInst->uOpcode == USEASM_OP_FSSQ)
	{
		uSrc0 = USE_UNDEF;
		uSrc1 = 1;
		uSrc2 = 2;

		bInst0UsesSrc0 = IMG_FALSE;
		bInst0UsesSrc2 = IMG_TRUE;
	}
	else if (psInst->uOpcode == USEASM_OP_MOV)
	{
		uSrc0 = USE_UNDEF;
		uSrc1 = 1;
		uSrc2 = 2;

		bInst0UsesSrc0 = bInst0UsesSrc2 = IMG_FALSE;
	}
	else 
	{
		if (psInst->uOpcode == USEASM_OP_FDDP ||
			psInst->uOpcode == USEASM_OP_IMA32)
		{
			uSrc0 = 2;
			uSrc1 = 3;
			uSrc2 = 4;
		}
		else
		{
			uSrc0 = 1;
			uSrc1 = 2;
			uSrc2 = 3;
		}

		bInst0UsesSrc0 = bInst0UsesSrc2 = IMG_TRUE;
	}

	if (bInst0UsesSrc0)
	{
		CheckArgFlags(psContext, psInst, uSrc0, uArgValidFlags);
		EncodeSrc0(psContext, psInst, uSrc0, IMG_FALSE, puInst + 0, puInst + 1, 0, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	}

	CheckArgFlags(psContext, psInst, uSrc1, uOp1SrcModFlag | uArgValidFlags);
	EncodeSrc1(psContext, psInst, uSrc1, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	if (psInst->asArg[uSrc1].uFlags & uOp1SrcModFlag)
	{
		puInst[1] |= SGX545_USE1_FDUAL_S1NEG;
	}

	if (bInst0UsesSrc2)
	{
		CheckArgFlags(psContext, psInst, uSrc2, 
					  uOp1SrcModFlag | uArgValidFlags);
		EncodeSrc2(psContext, psInst, uSrc2, IMG_TRUE, EURASIA_USE1_S2BEXT, 
				   IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, bSFASEL, 
				   USEASM_ARGFLAGS_FMTF16, psTarget);
		if (psInst->asArg[uSrc2].uFlags & uOp1SrcModFlag)
		{
			puInst[1] |= SGX545_USE1_FDUAL_S2NEG;
		}
	}

	/*
		Encode the second instruction's opcode.
	*/
	uCoSrc0 = uCoSrc1 = uCoSrc2 = USE_UNDEF;
	switch (psCoissueInst->uOpcode)
	{
		case USEASM_OP_FMAD: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FMAD; 
			uCoSrc0 = 1;
			uCoSrc1 = 2;
			uCoSrc2 = 3;
			break;
		}
		case USEASM_OP_FMAD16: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FMAD16;
			uCoSrc0 = 1;
			uCoSrc1 = 2;
			uCoSrc2 = 3;
			break;
		}
		case USEASM_OP_FMSA:
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FMSA;
			uCoSrc0 = 1;
			uCoSrc1 = 2;
			uCoSrc2 = 3;
			break;
		}
		case USEASM_OP_FFRC: 
		case USEASM_OP_FSUBFLR:
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FFRC; 
			uCoSrc1 = 1;
			/*
				For FFRC make the primary source 1
				and source 2 the same.
			*/
			if (psCoissueInst->uOpcode == USEASM_OP_FFRC)
			{
				uCoSrc2 = 1;
			}
			else
			{
				uCoSrc2 = 2;
			}
			break;
		}
		case USEASM_OP_FMAXMIN: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FCLMP; 
			uCoSrc0 = 1;
			uCoSrc1 = 2;
			uCoSrc2 = 3;
			break;
		}
		case USEASM_OP_FRCP: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FRCP; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FRSQ: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FRSQ; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FEXP: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FEXP; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FLOG: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FLOG; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FSIN: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FSIN; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FCOS: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FCOS; 
			uCoSrc1 = 1;
			break;
		}
		case USEASM_OP_FADD: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FADD; 
			uCoSrc1 = 1;
			uCoSrc2 = 2;
			break;
		}
		case USEASM_OP_FSUB: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FSUB; 
			uCoSrc1 = 1;
			uCoSrc2 = 2;
			break;
		}
		case USEASM_OP_FMUL: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_FMUL; 
			uCoSrc1 = 1;
			uCoSrc2 = 2;
			break;
		}
		case USEASM_OP_IMA32: 
		{
			if (FixBRN30898(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "IMA32 isn't supported as a secondary operation on this core"));
			}

			uOp2 = SGX545_USE1_FDUAL_OP2_IMA32; 
			
			CheckIMA32AsDualIssue(psCoissueInst, psContext);

			uCoSrc0 = 2;
			uCoSrc1 = 3;
			uCoSrc2 = 4;
			break;
		}
		case USEASM_OP_MOV: 
		{
			uOp2 = SGX545_USE1_FDUAL_OP2_MOV; 
			uCoSrc1 = 1;
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "This instruction isn't supported as the second in a pair of coissued opcodes"));
			uOp2 = 0;
			break;
		}
	}
	puInst[1] |= (uOp2 << SGX545_USE1_FDUAL_OP2_SHIFT);

	/*
		Check resource constraints.
	*/
	if (!puResourceTableOp1[uOp2])
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "This pair of instructions aren't supported as coissued opcodes"));
	}

	/*
		Encode the second instruction's destination.
	*/
	CheckArgFlags(psContext, psCoissueInst, 0, 0);
	if (uCoSrc0 != USE_UNDEF)
	{
		CheckArgFlags(psContext, psCoissueInst, uCoSrc0, uArgValidFlags);
	}
	if (psCoissueInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL &&
		psCoissueInst->asArg[0].uIndex == USEREG_INDEX_NONE)
	{
		IMG_BOOL	bSrc0EqualsDest = IMG_FALSE;

		if (psCoissueInst->asArg[0].uNumber > SGX545_USE1_FDUAL_OP2DSTS0_MAXIMUM_INTERNAL_REGISTER)
		{
			USEASM_ERRMSG((psContext->pvContext, psCoissueInst, 
				"The maximum register number for the destination of the co-issued instruction is %d",
				SGX545_USE1_FDUAL_OP2DSTS0_MAXIMUM_INTERNAL_REGISTER));
		}
		else
		{
			if (uCoSrc0 != USE_UNDEF)
			{
				CheckArgFlags(psContext, psCoissueInst, uCoSrc0, uArgValidFlags);
				if (psCoissueInst->asArg[uCoSrc0].uType == psCoissueInst->asArg[0].uType &&
					psCoissueInst->asArg[uCoSrc0].uNumber == psCoissueInst->asArg[0].uNumber &&
					psCoissueInst->asArg[uCoSrc0].uIndex == psCoissueInst->asArg[0].uIndex)
				{
					bSrc0EqualsDest = IMG_TRUE;
				}
			}

			switch (psCoissueInst->asArg[0].uNumber)
			{
				case 0: 
				{
					if (bSrc0EqualsDest || uCoSrc0 == USE_UNDEF)
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2DSTS0_STD_I0 << SGX545_USE1_FDUAL_OP2DSTS0_SHIFT); 
					}
					else
					{
						if (!bInst0UsesSrc0)
						{
							/*
								The primary instruction doesn't use source 0 so encode our source there.
							*/
							EncodeSrc0(psContext, 
									   psCoissueInst, 
									   uCoSrc0, 
									   IMG_FALSE, 
									   puInst + 0, 
									   puInst + 0, 
									   0, 
									   bSFASEL, 
									   USEASM_ARGFLAGS_FMTF16,
									   psTarget);

							puInst[1] |= (SGX545_USE1_FDUAL_OP2DSTS0_EXT_I0SRC0 <<  SGX545_USE1_FDUAL_OP2DSTS0_SHIFT) |
										 SGX545_USE1_FDUAL_OP2DEXTS0EXT; 
						}
						else if (psCoissueInst->asArg[uCoSrc0].uType == psInst->asArg[uSrc0].uType &&
								 psCoissueInst->asArg[uCoSrc0].uNumber == psInst->asArg[uSrc0].uNumber &&
								 psCoissueInst->asArg[uCoSrc0].uIndex == psInst->asArg[uSrc0].uIndex)
						{
							puInst[1] |= (SGX545_USE1_FDUAL_OP2DSTS0_EXT_I0SRC0 <<  SGX545_USE1_FDUAL_OP2DSTS0_SHIFT) |
										 SGX545_USE1_FDUAL_OP2DEXTS0EXT; 
						}
						else
						{
							USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "Source 0 of the coissued instruction and source 0 of the main instruction must be the same"));
						}
					}
					break;
				}
				case 1: 
				{
					if (!(bSrc0EqualsDest || uCoSrc0 == USE_UNDEF))
					{
						USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "The destination and source 0 of the coissued instruction must be the same"));
					}
					puInst[1] |= (SGX545_USE1_FDUAL_OP2DSTS0_STD_I1 <<  SGX545_USE1_FDUAL_OP2DSTS0_SHIFT); 
					break;
				}
				case 2: 
				{
					if (!(bSrc0EqualsDest || uCoSrc0 == USE_UNDEF))
					{
						USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "The destination and source 0 of the coissued instruction must be the same"));
					}
					puInst[1] |= (SGX545_USE1_FDUAL_OP2DSTS0_EXT_I2 <<  SGX545_USE1_FDUAL_OP2DSTS0_SHIFT) |
								 SGX545_USE1_FDUAL_OP2DEXTS0EXT; 
					break;
				}
			}
		}
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "The destination for the co-issued instruction must be an internal register"));
	}

	if (psCoissueInst->uOpcode != USEASM_OP_MOV)
	{
		if (uCoSrc1 != USE_UNDEF)
		{
			CheckArgFlags(psContext, psCoissueInst, uCoSrc1, uArgValidFlags);
			if (psCoissueInst->asArg[uCoSrc1].uType == USEASM_REGTYPE_FPINTERNAL &&
				psCoissueInst->asArg[uCoSrc1].uNumber <= 2 &&
				psCoissueInst->asArg[uCoSrc1].uIndex == USEREG_INDEX_NONE)
			{
				switch (psCoissueInst->asArg[uCoSrc1].uNumber)
				{
					case 0:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_I0 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
						break;
					}
					case 1:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_I1 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
						break;
					}
					case 2:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_I2 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
						break;
					}
				}
			}
			else if (psCoissueInst->asArg[uCoSrc1].uType == psInst->asArg[uSrc1].uType &&
					 psCoissueInst->asArg[uCoSrc1].uNumber == psInst->asArg[uSrc1].uNumber &&
					 psCoissueInst->asArg[uCoSrc1].uIndex == psInst->asArg[uSrc1].uIndex)
			{
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_SRC1 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "Only i0, i1, i2 or the main instruction's source 1 are valid for source 1 of a co-issued instruction"));
			}
		}

		if (uCoSrc2 != USE_UNDEF)
		{
			CheckArgFlags(psContext, psCoissueInst, uCoSrc2, uArgValidFlags);
			if (psCoissueInst->asArg[uCoSrc2].uType == USEASM_REGTYPE_FPINTERNAL &&
				psCoissueInst->asArg[uCoSrc2].uNumber <= 2 &&
				psCoissueInst->asArg[uCoSrc2].uIndex == USEREG_INDEX_NONE)
			{
				switch (psCoissueInst->asArg[uCoSrc2].uNumber)
				{
					case 0:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S2_STD_I0 << SGX545_USE1_FDUAL_OP2S2_SHIFT);
						break;
					}
					case 1:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S2_STD_I1 << SGX545_USE1_FDUAL_OP2S2_SHIFT);
						break;
					}
					case 2:
					{
						puInst[1] |= (SGX545_USE1_FDUAL_OP2S2_EXT_I2 << SGX545_USE1_FDUAL_OP2S2_SHIFT) |
								     SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
						break;
					}
				}
			}
			else if (bInst0UsesSrc2 &&
					 psCoissueInst->asArg[uCoSrc2].uType == psInst->asArg[uSrc2].uType &&
					 psCoissueInst->asArg[uCoSrc2].uNumber == psInst->asArg[uSrc2].uNumber &&
					 psCoissueInst->asArg[uCoSrc2].uIndex == psInst->asArg[uSrc2].uIndex)
			{
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S2_EXT_SRC2 << SGX545_USE1_FDUAL_OP2S2_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else if (!bInst0UsesSrc2)
			{	
				EncodeSrc2(psContext, psCoissueInst, uCoSrc2, IMG_TRUE, EURASIA_USE1_S2BEXT, 
						   IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, bSFASEL, 
						   USEASM_ARGFLAGS_FMTF16, psTarget);
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S2_EXT_SRC2 << SGX545_USE1_FDUAL_OP2S2_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psCoissueInst, "Only i0, i1, i2 or the main instruction's source 2 are valid for source 2 of a co-issued instruction"));
			}
		}
	}
	else
	{
		/* psCoIssueInst->uOpcode == USEASM_OP_MOV */

		CheckArgFlags(psContext, psCoissueInst, uCoSrc1, 0);
		if (psCoissueInst->asArg[uCoSrc1].uType == USEASM_REGTYPE_FPINTERNAL &&
			psCoissueInst->asArg[uCoSrc1].uNumber <= 2 &&
			psCoissueInst->asArg[uCoSrc1].uIndex == USEREG_INDEX_NONE)
		{
			switch (psCoissueInst->asArg[uCoSrc1].uNumber)
			{
				case 0:
				{
					puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I0 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
					break;
				}
				case 1:
				{
					puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I1 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
					break;
				}
				case 2:
				{
					puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCSTD_I2 << SGX545_USE1_FDUAL_OP2S1_SHIFT);
					break;
				}
			}
		}
		else
		{
			if (psCoissueInst->asArg[uCoSrc1].uNumber == USEASM_INTSRCSEL_SRC1 &&
				psCoissueInst->asArg[uCoSrc1].uType == USEASM_REGTYPE_INTSRCSEL &&
				psCoissueInst->asArg[uCoSrc1].uIndex == USEREG_INDEX_NONE &&
				psCoissueInst->asArg[uCoSrc1].uFlags == 0)
			{
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC1 << SGX545_USE1_FDUAL_OP2S1_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else if (bInst0UsesSrc2 &&
					 psCoissueInst->asArg[uCoSrc1].uNumber == USEASM_INTSRCSEL_SRC2 &&
					 psCoissueInst->asArg[uCoSrc1].uType == USEASM_REGTYPE_INTSRCSEL &&
					 psCoissueInst->asArg[uCoSrc1].uIndex == USEREG_INDEX_NONE &&
					 psCoissueInst->asArg[uCoSrc1].uFlags == 0)
			{
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC2 << SGX545_USE1_FDUAL_OP2S1_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else if (bInst0UsesSrc0 &&
					 psCoissueInst->asArg[uCoSrc1].uNumber == USEASM_INTSRCSEL_SRC0 &&
					 psCoissueInst->asArg[uCoSrc1].uType == USEASM_REGTYPE_INTSRCSEL &&
					 psCoissueInst->asArg[uCoSrc1].uIndex == USEREG_INDEX_NONE &&
					 psCoissueInst->asArg[uCoSrc1].uFlags == 0)
			{
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC0 << SGX545_USE1_FDUAL_OP2S1_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else if (!bInst0UsesSrc2)
			{	
				EncodeSrc2(psContext, psCoissueInst, uCoSrc1, IMG_TRUE, EURASIA_USE1_S2BEXT, 
						   IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, bSFASEL, 
						   USEASM_ARGFLAGS_FMTF16, psTarget);
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC2 << SGX545_USE1_FDUAL_OP2S1_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else if (!bInst0UsesSrc0)
			{
				EncodeSrc0(psContext, psCoissueInst, uCoSrc1, IMG_FALSE, puInst + 0, puInst + 1, 0, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
				puInst[1] |= (SGX545_USE1_FDUAL_OP2S1_MSRCEXT_SRC0 << SGX545_USE1_FDUAL_OP2S1_SHIFT) |
							 SGX545_USE1_FDUAL_O2S2EXT_MSRCEXT;
			}
			else
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The source to a co-issued move instruction can be i0, i1, i2 or any of the sources to the main instruction"));
			}
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */


/*****************************************************************************
 FUNCTION	: EncodeFDPCInstruction

 PURPOSE	: Encode a FDPC instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFDPCInstruction(PCSGX_CORE_DESC		psTarget, 
									  PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_BOOL bSFASEL = IMG_FALSE;
	IMG_UINT32 uArgValidFlags = 0;
	IMG_UINT32 uValidFlags2;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check instruction validity. */
	uValidFlags2 = 0;
	if (IsPerInstMoeIncrements(psTarget))
	{
		uValidFlags2 |= USEASM_OPFLAGS2_PERINSTMOE | 
					    USEASM_OPFLAGS2_PERINSTMOE_INCS0 |
					    USEASM_OPFLAGS2_PERINSTMOE_INCS1 |
					    USEASM_OPFLAGS2_PERINSTMOE_INCS2;
	}
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
			   USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), uValidFlags2, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_FDOTPRODUCT << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(EURASIA_USE1_FLOAT_OP2_DP << EURASIA_USE1_FLOAT_OP2_SHIFT);
	/* Encode repeat count and mask */
	EncodeFloatRepeats(psTarget, puInst, psInst, EURASIA_USE1_OP_FDOTPRODUCT, psContext);

	/* Encode format select. */
	if ((psInst->asArg[2].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[3].uFlags & USEASM_ARGFLAGS_FMTF16))
	{
		bSFASEL = IMG_TRUE;
		puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
	}
	uArgValidFlags = USEASM_ARGFLAGS_FMTF16;

	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget); 
	/* Encode the source modifiers. */
	puInst[1] |= (EncodeFloatMod(psInst->asArg[2].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT) |
				 (EncodeFloatMod(psInst->asArg[3].uFlags) << EURASIA_USE1_SRC2MOD_SHIFT);
	/* Encode the destination. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Encode the clipplane to update. */
	if (psInst->asArg[1].uType != USEASM_REGTYPE_CLIPPLANE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to a fdpc instruction must be the clip plane to update"));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a clipplane argument"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A clip plane argument is not indexable"));
	}
	if (psInst->asArg[1].uNumber >= EURASIA_MAX_MTE_CLIP_PLANES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum clip plane number is %d", EURASIA_MAX_MTE_CLIP_PLANES - 1));
	}
	puInst[0] |= EURASIA_USE0_DP_CLIPENABLE |
				 (psInst->asArg[1].uNumber << EURASIA_USE0_DP_CLIPPLANEUPDATE_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodeFDDPInstruction

 PURPOSE	: Encode a FDDP instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFDDPInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_BOOL bSFASEL = IMG_FALSE;
	IMG_UINT32 uArgValidFlags = 0;
	IMG_UINT32 uValidFlags2;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check instruction validity. */
	uValidFlags2 = 0;
	if (IsPerInstMoeIncrements(psTarget))
	{
		uValidFlags2 |= USEASM_OPFLAGS2_PERINSTMOE | 
					    USEASM_OPFLAGS2_PERINSTMOE_INCS0 |
					    USEASM_OPFLAGS2_PERINSTMOE_INCS1 |
					    USEASM_OPFLAGS2_PERINSTMOE_INCS2;
	}
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
			   USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), uValidFlags2, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_FDOTPRODUCT << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(EURASIA_USE1_FLOAT_OP2_DDP << EURASIA_USE1_FLOAT_OP2_SHIFT);
	/* Encode repeat count and mask */
	EncodeFloatRepeats(psTarget, puInst, psInst, EURASIA_USE1_OP_FDOTPRODUCT, psContext);
	
	/* Encode format select. */
	if ((psInst->asArg[2].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[3].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[4].uFlags & USEASM_ARGFLAGS_FMTF16))
	{
		bSFASEL = IMG_TRUE;
		puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
	}
	uArgValidFlags = USEASM_ARGFLAGS_FMTF16;
	
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
	CheckArgFlags(psContext, psInst, 4, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE | uArgValidFlags);
	EncodeSrc0(psContext, psInst, 2, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc1(psContext, psInst, 3, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc2(psContext, psInst, 4, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget); 
	/* Encode the source modifiers. */
	#if defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS)
	if (IsPerInstMoeIncrements(psTarget))
	{
		IMG_UINT32 uArg3Flags = psInst->asArg[3].uFlags;
		IMG_UINT32 uArg4Flags = psInst->asArg[4].uFlags;
		if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			uArg3Flags ^= USEASM_ARGFLAGS_NEGATE;
			uArg4Flags ^= USEASM_ARGFLAGS_NEGATE;
		}
		if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
		{
			puInst[1] |= SGX545_USE1_FLOAT_SRC0ABS;
		}
		puInst[1] |= (EncodeFloatMod(uArg3Flags) << EURASIA_USE1_SRC1MOD_SHIFT) |
					 (EncodeFloatMod(uArg4Flags) << EURASIA_USE1_SRC2MOD_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_PER_INST_MOE_INCREMENTS) */
	{
		puInst[1] |= (EncodeFloatMod(psInst->asArg[2].uFlags) << EURASIA_USE1_SRC0MOD_SHIFT) |
					 (EncodeFloatMod(psInst->asArg[3].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT) |
					 (EncodeFloatMod(psInst->asArg[4].uFlags) << EURASIA_USE1_SRC2MOD_SHIFT);
	}
	/* Encode the destination. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Encode the destination for the second dotproduct. */
	if (psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to a fddp instruction must be the internal register to update"));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an internal register register"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Internal registers are not indexable"));
	}
	if (psInst->asArg[1].uNumber >= 2)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only i0 and i1 may be destinations for this instruction"));
	}
	if (psInst->asArg[1].uNumber == 0)
	{
		puInst[0] |= EURASIA_USE1_DOTPRODUCT_UPDATEI0;
	}
	else
	{
		puInst[1] |= EURASIA_USE1_DOTPRODUCT_UPDATEI1;
	}
}

/*****************************************************************************
 FUNCTION	: EncodeFDDPCInstruction

 PURPOSE	: Encode a FDDPC instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFDDPCInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst,
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uClip;
	IMG_BOOL bSFASEL = IMG_FALSE;
	IMG_UINT32 uArgValidFlags = 0;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Scalar dot-product instructions aren't supported on this core"));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check instruction validity. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
			   USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), 0, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_FDOTPRODUCT << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(EURASIA_USE1_FLOAT_OP2_DDPC << EURASIA_USE1_FLOAT_OP2_SHIFT);
	/* Encode repeat count and mask */
	EncodeFloatRepeats(psTarget, puInst, psInst, EURASIA_USE1_OP_FDOTPRODUCT, psContext);
	
	/* Encode format select. */
	if ((psInst->asArg[4].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[5].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[6].uFlags & USEASM_ARGFLAGS_FMTF16))
	{
		bSFASEL = IMG_TRUE;
		puInst[1] |= EURASIA_USE1_FLOAT_SFASEL;
	}
	uArgValidFlags = USEASM_ARGFLAGS_FMTF16;
	
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 4, uArgValidFlags);
	CheckArgFlags(psContext, psInst, 5, uArgValidFlags);
	CheckArgFlags(psContext, psInst, 6, uArgValidFlags);
	EncodeSrc0(psContext, psInst, 4, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc1(psContext, psInst, 5, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc2(psContext, psInst, 6, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bSFASEL, USEASM_ARGFLAGS_FMTF16, psTarget); 
	/* Encode the destination. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Encode the destination for the second dotproduct. */
	if (psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to a fddpc instruction must be the internal register to update"));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an internal register register"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Internal registers are not indexable"));
	}
	if (psInst->asArg[1].uNumber >= 2)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only i0 and i1 may be destinations for this instruction"));
	}
	if (psInst->asArg[1].uNumber == 0)
	{
		puInst[0] |= EURASIA_USE1_DOTPRODUCT_UPDATEI0;
	}
	else
	{
		puInst[1] |= EURASIA_USE1_DOTPRODUCT_UPDATEI1;
	}
	/* Encode the clipplane to update. */
	if (psInst->asArg[2].uType != USEASM_REGTYPE_CLIPPLANE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to a fddpc instruction must be the clip plane to update"));
	}
	if (psInst->asArg[2].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a clipplane argument"));
	}
	if (psInst->asArg[2].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A clip plane argument is not indexable"));
	}
	if (psInst->asArg[2].uNumber >= EURASIA_MAX_MTE_CLIP_PLANES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum clip plane number is %d", EURASIA_MAX_MTE_CLIP_PLANES - 1));
	}
	if (psInst->asArg[3].uType != USEASM_REGTYPE_CLIPPLANE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to a fdpc instruction must be the clip plane to update"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a clipplane argument"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A clip plane argument is not indexable"));
	}
	if (psInst->asArg[3].uNumber >= EURASIA_MAX_MTE_CLIP_PLANES)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum clip plane number is %d", EURASIA_MAX_MTE_CLIP_PLANES - 1));
	}
	uClip = (psInst->asArg[2].uNumber << 0) | (psInst->asArg[3].uNumber << 3);
	puInst[1] |= (uClip << EURASIA_USE1_DDPC_CLIPPLANEUPDATE_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodeMOVCTest

 PURPOSE	: Encode a MOVC test.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded test.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMOVCTest(PUSE_INST		psInst,
							   IMG_PUINT32		puInst,
							   PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uSignTest = (psInst->uTest & ~USEASM_TEST_SIGN_CLRMSK) >> USEASM_TEST_SIGN_SHIFT;
	IMG_UINT32 uZeroTest = (psInst->uTest & ~USEASM_TEST_ZERO_CLRMSK) >> USEASM_TEST_ZERO_SHIFT;
	IMG_UINT32 uCombineOp = (psInst->uTest & USEASM_TEST_CRCOMB_AND);

	if (uSignTest != USEASM_TEST_SIGN_TRUE)
	{
		puInst[1] |= EURASIA_USE1_MOVC_TESTCCEXT;

		if (uSignTest == USEASM_TEST_SIGN_POSITIVE &&
			uZeroTest == USEASM_TEST_ZERO_ZERO && 
			uCombineOp == USEASM_TEST_CRCOMB_OR)
		{
			puInst[1] |= (EURASIA_USE1_MOVC_TESTCC_EXTNONSIGNED << EURASIA_USE1_MOVC_TESTCC_SHIFT);
		}
		else if (uSignTest == USEASM_TEST_SIGN_NEGATIVE &&
				 uZeroTest == USEASM_TEST_ZERO_NONZERO && 
				 uCombineOp == USEASM_TEST_CRCOMB_AND)
		{
			puInst[1] |= (EURASIA_USE1_MOVC_TESTCC_EXTSIGNED << EURASIA_USE1_MOVC_TESTCC_SHIFT);
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .tz, .t!z, .tn or .tp|z are valid tests for movc"));
		}
	}
	else
	{
		if (uZeroTest == USEASM_TEST_ZERO_ZERO)
		{
			puInst[1] |= (EURASIA_USE1_MOVC_TESTCC_STDZERO << EURASIA_USE1_MOVC_TESTCC_SHIFT);
		}
		else
		{
			puInst[1] |= (EURASIA_USE1_MOVC_TESTCC_STDNONZERO << EURASIA_USE1_MOVC_TESTCC_SHIFT);
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeMovcInstruction

 PURPOSE	: Encode a MOVC instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.U.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMovcInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32 uValidFlags1 = 0;
	IMG_UINT32 uValidFlags2 = 0;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	if (!(psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A movc instruction must include a test (.tz, .t!z, .ts or .tp)"));
	}
	if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
	{
		if (FixBRN21713(psTarget) && psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "On this processor MOVC on C10 format data must have a destination in the internal register bank"));
		}
	}
	uValidFlags1 |= USEASM_OPFLAGS1_MOVC_FMTI8;
	uValidFlags2 |= USEASM_OPFLAGS2_MOVC_FMTI16 |
				    USEASM_OPFLAGS2_MOVC_FMTI32 | 
				    USEASM_OPFLAGS2_MOVC_FMTF32 |
					USEASM_OPFLAGS2_MOVC_FMTI10;
	
	/* Check instruction validity. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART | 
			(~USEASM_OPFLAGS1_MASK_CLRMSK) | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK) | 
			USEASM_OPFLAGS1_NOSCHED |
			USEASM_OPFLAGS1_END |
			USEASM_OPFLAGS1_TESTENABLE | uValidFlags1,  uValidFlags2, 0);
	/* Encode opcode, predicate and instruction flags. */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
		 		(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0);
	/* Encode repeat count and mask */
	if (uRptCount > 0)
	{
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
	}
	else
	{
		puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
	}
	/* Encode the sources. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);

	/* Encode the test */
	EncodeMOVCTest(psInst, puInst, psContext);

	/* Encode the instruction modes. */
	if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
	{
		puInst[1] |= (EURASIA_USE1_MOVC_TSTDTYPE_INT8 << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI16)
	{
		puInst[1] |= (EURASIA_USE1_MOVC_TSTDTYPE_INT16 << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI32)
	{
		puInst[1] |= (EURASIA_USE1_MOVC_TSTDTYPE_INT32 << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTF32)
	{
		puInst[1] |= (EURASIA_USE1_MOVC_TSTDTYPE_FLOAT << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
	{
		puInst[1] |= (EURASIA_USE1_MOVC_TSTDTYPE_INT10 << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A movc instruction must specify the data type (.i8, .i16, .i32, .flt)"));
	}
}

/*****************************************************************************
 FUNCTION	: EncodeEfoInstruction

 PURPOSE	: Encode an EFO instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeEfoInstruction(PCSGX_CORE_DESC			psTarget,
									 PUSE_INST				psInst, 
									 IMG_PUINT32			puInst,
									 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uValidArgFlags = 0;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_BOOL bSrc0BExt, bSrc2BExt;
	IMG_UINT32 uSrc0BExt, uSrc2BExt;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uValidFlags3;

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
		return;
	}	
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check instruction validity. */
	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_GPIEXT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK), 0, uValidFlags3);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_EFO << EURASIA_USE1_OP_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(psInst->asArg[4].uNumber);
	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		uValidArgFlags = USEASM_ARGFLAGS_NEGATE;

		bSrc0BExt = bSrc2BExt = IMG_TRUE;
		uSrc0BExt = SGX545_USE1_EFO_S0BEXT;
		uSrc2BExt = SGX545_USE1_EFO_S2BEXT;

		puInst[1] |= EncodePredicate(psContext, psInst, IMG_TRUE) << SGX545_USE1_EFO_SPRED_SHIFT;

		if (psInst->uFlags3 & USEASM_OPFLAGS3_GPIEXT)
		{
			puInst[1] |= SGX545_USE1_EFO_GPIROW;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	{
		uValidArgFlags = USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE;

		bSrc0BExt = bSrc2BExt = IMG_FALSE;
		uSrc0BExt = uSrc2BExt = 0;

		puInst[1] |= EncodePredicate(psContext, psInst, IMG_FALSE) << EURASIA_USE1_EPRED_SHIFT;
	}
	/* Encode repeat count and mask */
	if (uRptCount != 0)
	{
		if (uRptCount > EURASIA_USE1_EFO_RCOUNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum repeat count for an EFO instruction is %d (was %d)",	EURASIA_USE1_EFO_RCOUNT_MAX, uRptCount));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_EFO_RCOUNT_SHIFT);
	}

	/*
		Encode format select.
	*/
	uValidArgFlags |= USEASM_ARGFLAGS_FMTF16;
	if ((psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[2].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->asArg[3].uFlags & USEASM_ARGFLAGS_FMTF16) ||
		(psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT))
	{
		bFmtControl = IMG_TRUE;
	}
	
	/* Check if a workaround is required for a hardware bug. */
	if (FixBRN21697(psTarget))
	{
		if ((psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTF16) &&
			psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL &&
			psInst->asArg[1].uNumber != 0 &&
			(psInst->asArg[4].uNumber & EURASIA_USE1_EFO_WI0EN) &&
			(psInst->asArg[4].uNumber & EURASIA_USE1_EFO_WI0EN))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The instruction is affected by BRN21697 (i0 and i1 are written, and source 0 is in f16 format and has a non-zero register number)"));
		}
	}
	if (FixBRN23461(psTarget))
	{
		/*
			In an EFO instruction where MSRC = 3 and ASRC = 1:
				m0 = s2*s1
				m1 = s0*s0
				a0 = m0+s2
				a1 = i1+i0

			The USE will erroneously convert source arguments from f16 to f32 before feeding the floating point ALU's, 
			resulting in wrong results for all outputs. 
		*/
		IMG_UINT32	uASrc = (psInst->asArg[4].uNumber & ~EURASIA_USE1_EFO_ASRC_CLRMSK) >> EURASIA_USE1_EFO_ASRC_SHIFT;
		IMG_UINT32	uMSrc = (psInst->asArg[4].uNumber & ~EURASIA_USE1_EFO_MSRC_CLRMSK) >> EURASIA_USE1_EFO_MSRC_SHIFT;

		if (uMSrc == EURASIA_USE1_EFO_MSRC_SRC1SRC2_SRC0SRC0 &&
			uASrc == EURASIA_USE1_EFO_ASRC_M0SRC2_I1I0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The instruction is affected by BRN23461 (a0=m0+src2, a1=i1+i0, m0=src1*src2, m1=src0*src0)"));
		}
	}
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	CheckArgFlags(psContext, psInst, 3, uValidArgFlags);
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeSrc0(psContext, psInst, 1, bSrc0BExt, puInst + 0, puInst + 1, uSrc0BExt, bFmtControl, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc1(psContext, psInst, 2, FALSE, 0, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTF16, psTarget);
	EncodeSrc2(psContext, psInst, 3, bSrc2BExt, uSrc2BExt, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTF16, psTarget); 
	EncodeDest(psContext, psInst, FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Encode the source modifiers. */
	#if defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS)
	if (HasNewEfoOptions(psTarget))
	{
		if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGX545_USE1_EFO_SRC0NEG;
		}
		if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGX545_USE1_EFO_SRC1NEG;
		}
		if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGX545_USE1_EFO_SRC2NEG;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NEW_EFO_OPTIONS) */
	{
		puInst[1] |= (EncodeFloatMod(psInst->asArg[1].uFlags) << EURASIA_USE1_SRC0MOD_SHIFT) |
	 				 (EncodeFloatMod(psInst->asArg[2].uFlags) << EURASIA_USE1_SRC1MOD_SHIFT) |
					 (EncodeFloatMod(psInst->asArg[3].uFlags) << EURASIA_USE1_SRC2MOD_SHIFT);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSOPWMInstruction

 PURPOSE	: Encode a SOPWM instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSOPWMInstruction(PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext,
									   PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32 uMod1, uMod2, uWriteMask;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				USEASM_OPFLAGS1_NOSCHED,  0, 0);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SOPWM << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Check repeats */
	if (uMask != 0x1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is not valid for this instruction"));
	}

	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[0].uFlags & USEASM_ARGFLAGS_BYTEMSK_PRESENT)
	{
		IMG_UINT32 uByteMask = (psInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_BYTEMSK_CLRMSK) >> USEASM_ARGFLAGS_BYTEMSK_SHIFT;

		CheckByteMask(psContext, psInst, uByteMask, IMG_TRUE);
				
		uWriteMask = ((uByteMask & 0x8) ? EURASIA_USE1_SOP2WM_WRITEMASK_W : 0) |
					 ((uByteMask & 0x4) ? EURASIA_USE1_SOP2WM_WRITEMASK_Z : 0) |
					 ((uByteMask & 0x2) ? EURASIA_USE1_SOP2WM_WRITEMASK_Y : 0) |
					 ((uByteMask & 0x1) ? EURASIA_USE1_SOP2WM_WRITEMASK_X : 0);
	}
	else
	{
		uWriteMask = EURASIA_USE1_SOP2WM_WRITEMASK_XYZW;
	}
	puInst[1] |= (uWriteMask << EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT);

	if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to sop2wm should be an integer source selector"));
	}
	if ((psInst->asArg[3].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the fourth argument to sop2wm"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uMod1 = EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT;
	}
	else
	{
		uMod1 = EURASIA_USE1_SOP2WM_MOD1_NONE;
	}
	switch (psInst->asArg[3].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_ZERO << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_ZERO << EURASIA_USE1_SOP2WM_SEL1_SHIFT);
			uMod1 ^= EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2WM_SEL1_SHIFT);
			uMod1 ^= EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_SRC1 << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_SRC1ALPHA << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_SRC2 << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL1_SRC2ALPHA << EURASIA_USE1_SOP2WM_SEL1_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to sop2wm is not a valid selector for sopwm"));
	}
	puInst[1] |= (uMod1 << EURASIA_USE1_SOP2WM_MOD1_SHIFT);

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop2wm should be an integer source selector"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the fifth argument to sop2wm"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uMod2 = EURASIA_USE1_SOP2WM_MOD2_COMPLEMENT;
	}
	else
	{
		uMod2 = EURASIA_USE1_SOP2WM_MOD2_NONE;
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_ZERO << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_ZERO << EURASIA_USE1_SOP2WM_SEL2_SHIFT);
			uMod2 ^= EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP2WM_SEL2_SHIFT);
			uMod2 ^= EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_SRC1 << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_SRC1ALPHA << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_SRC2 << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP2WM_SEL2_SRC2ALPHA << EURASIA_USE1_SOP2WM_SEL2_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop2wm is not a valid selector for sopwm"));
	}
	puInst[1] |= (uMod2 << EURASIA_USE1_SOP2WM_MOD2_SHIFT);

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to sop2wm should be an integer source selector"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to sop2wm"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP2WM_COP_ADD << EURASIA_USE1_SOP2WM_COP_SHIFT); break;
		case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP2WM_COP_SUB << EURASIA_USE1_SOP2WM_COP_SHIFT); break;
		case USEASM_INTSRCSEL_MIN: puInst[1] |= (EURASIA_USE1_SOP2WM_COP_MIN << EURASIA_USE1_SOP2WM_COP_SHIFT); break;
		case USEASM_INTSRCSEL_MAX: puInst[1] |= (EURASIA_USE1_SOP2WM_COP_MAX << EURASIA_USE1_SOP2WM_COP_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to sop2wm is not a valid operation for sopwm"));
	}

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to sop2wm should be an integer source selector"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to sop2wm"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Integer source selectors aren't indexable"));
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP2WM_AOP_ADD << EURASIA_USE1_SOP2WM_AOP_SHIFT); break;
		case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP2WM_AOP_SUB << EURASIA_USE1_SOP2WM_AOP_SHIFT); break;
		case USEASM_INTSRCSEL_MIN: puInst[1] |= (EURASIA_USE1_SOP2WM_AOP_MIN << EURASIA_USE1_SOP2WM_AOP_SHIFT); break;
		case USEASM_INTSRCSEL_MAX: puInst[1] |= (EURASIA_USE1_SOP2WM_AOP_MAX << EURASIA_USE1_SOP2WM_AOP_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to sop2wm is not a valid operation for sopwm"));
	}
}

/*****************************************************************************
 FUNCTION	: EncodeLRP1Instruction

 PURPOSE	: Encode a LRP1 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLRP1Instruction(PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext,
									  PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32 uASel1 = 0;
	IMG_UINT32 uCOp;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32 uAMod1;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MAINISSUE, 0, 0);
				
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SOP3 << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Check repeats */
	if (uMask != 0x1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is not valid for this instruction"));
	}

	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 3, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to lrp1 must be an integer source selection"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the fifth argument to lrp1"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCOp = EURASIA_USE1_SOP3_COP_INTERPOLATE2;
	}
	else
	{
		uCOp = EURASIA_USE1_SOP3_COP_INTERPOLATE1;
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO; break;
		case USEASM_INTSRCSEL_ONE:
		{
			uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO;
			uCOp = (uCOp == EURASIA_USE1_SOP3_COP_INTERPOLATE1) ? EURASIA_USE1_SOP3_COP_INTERPOLATE2 : EURASIA_USE1_SOP3_COP_INTERPOLATE1;
			break;
		}
		case USEASM_INTSRCSEL_SRC1: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10SRC1; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fifth argument to lrp1 - valid choices are zero, one, s1"));
	}
	puInst[1] |= (uCOp << EURASIA_USE1_SOP3_COP_SHIFT);

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to lrp1 must be an integer source selection"));
	}
	if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uAMod1 = EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
	}
	else
	{
		uAMod1 = EURASIA_USE1_SOP3_AMOD1_NONE;
	}
	if ((psInst->asArg[5].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the sixth argument to lrp1"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO; break;
		case USEASM_INTSRCSEL_ONE:
		{
			uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO; 
			uAMod1 ^= EURASIA_USE1_SOP3_AMOD1_COMPLEMENT; 
			break;
		}
		case USEASM_INTSRCSEL_SRC2: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11SRC2; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the sixth argument to lrp1 - valid choices are zero or s2"));
	}
	puInst[1] |= (uAMod1 << EURASIA_USE1_SOP3_AMOD1_SHIFT);

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to lrp1 must be an integer source selection"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to lrp1"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_DESTMOD_CS0SRC0ALPHA << EURASIA_USE1_SOP3_DESTMOD_SHIFT); break;
		case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_DESTMOD_CS0SRC0 << EURASIA_USE1_SOP3_DESTMOD_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the seventh argument to lrp1 - valid choices are s0a or s0"));
	}

	puInst[1] |= (uASel1 << EURASIA_USE1_SOP3_ASEL1_SHIFT);

	if (!(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "lrp1 must be coissued"));
	}
	if (psInst->psNext == NULL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "lrp1 must be coisssued"));
	}
	else
	{
		PUSE_INST psCoissueInst = psInst->psNext;
		IMG_UINT32 uCMod1, uCMod2;

		if (psCoissueInst->uOpcode != USEASM_OP_ASOP)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "lrp1 must be coissued with asop"));
		}

		if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to asop must be an integer source selector"));
		}
		if ((psCoissueInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the first argument to asop"));
		}
		if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
		}
		if (psCoissueInst->asArg[0].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uCMod1 = EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
		}
		else
		{
			uCMod1 = EURASIA_USE1_SOP3_CMOD1_NONE;
		}
		switch (psCoissueInst->asArg[0].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_ONE:
			{
				puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT);
				uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_RSRCALPHASAT:
			{
				puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT);
				uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the first argument to asop - valid choices are zero, one, asat, rasat, s0, s0a, s1, s1a, s2, s2a"));
		}
		puInst[1] |= (uCMod1 << EURASIA_USE1_SOP3_CMOD1_SHIFT);

		if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to asop must be an integer source selector"));
		}
		if ((psCoissueInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the second argument to asop"));
		}
		if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
		}
		if (psCoissueInst->asArg[1].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uCMod2 = EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
		}
		else
		{
			uCMod2 = EURASIA_USE1_SOP3_CMOD2_NONE;
		}
		switch (psCoissueInst->asArg[1].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_ZERO << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_ONE:
			{
				puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_ZERO << EURASIA_USE1_SOP3_CSEL2_SHIFT);
				uCMod2 ^= EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_RSRCALPHASAT:
			{
				puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL2_SHIFT);
				uCMod2 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC0 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC0ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC1 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC1ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC2 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC2ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to asop - valid choices are zero, one, asat, rasat, s0, s0a, s1, s1a, s2, s2a"));
		}
		puInst[1] |= (uCMod2 << EURASIA_USE1_SOP3_CMOD2_SHIFT);

		if (psCoissueInst->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to asop must be an integer source selector"));
		}
		if (psCoissueInst->asArg[2].uFlags  != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the third argument to asop"));
		}
		if (psCoissueInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
		}
		switch (psCoissueInst->asArg[2].uNumber)
		{
			case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP3_AOP_ADD << EURASIA_USE1_SOP3_AOP_SHIFT); break;
			case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP3_AOP_SUB << EURASIA_USE1_SOP3_AOP_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the third argument to asop - valid choices are add or sub"));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeLRP2Instruction

 PURPOSE	: Encode a LRP2 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLRP2Instruction(PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext,
									  PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32 uCMod1, uCMod2;
	IMG_UINT32 uCS1, uCS2;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MAINISSUE, 0, 0);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SOP3 << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Check repeats */
	if (uMask != 0x1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is not valid for this instruction"));
	}

	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 3, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop3 must be an integer source selection"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the fifth argument to sop3"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod1 = EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
	}
	else
	{
		uCMod1 = EURASIA_USE1_SOP3_CMOD1_NONE;
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fifth argument to lrp2 - valid choices are zero, one, mina, rminsa, s0, s0a, s1, s1a, s2 and s2a")); break;
	}
	puInst[1] |= (uCMod1 << EURASIA_USE1_SOP3_CMOD1_SHIFT);

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to lrp2 must be an integer source selection"));
	}
	if ((psInst->asArg[5].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the sixth argument to lrp2"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod2 = EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
	}
	else
	{
		uCMod2 = EURASIA_USE1_SOP3_CMOD2_NONE;
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_ZERO << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_ZERO << EURASIA_USE1_SOP3_CSEL2_SHIFT);
			uCMod2 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL2_SHIFT);
			uCMod2 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC0 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC0ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC1 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC1ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC2 << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL2_SRC2ALPHA << EURASIA_USE1_SOP3_CSEL2_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the sixth argument to lrp2 - valid choices are one, zero, mina, rmins, s0, s0a, s1, s1a, s2 and s2a"));
	}
	puInst[1] |= (uCMod2 << EURASIA_USE1_SOP3_CMOD2_SHIFT);

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to lrp2 must be an integer source selection"));
	}
	if ((psInst->asArg[6].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the seventh argument to lrp2"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	uCS1 = psInst->asArg[6].uNumber;

	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to lrp2 must be an integer source selection"));
	}
	if ((psInst->asArg[7].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the .comp flag is valid for the eighth argument to lrp2"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	uCS2 = psInst->asArg[7].uNumber;

	if (uCS1 == USEASM_INTSRCSEL_SRC1 && !(psInst->asArg[6].uFlags & USEASM_ARGFLAGS_COMPLEMENT))
	{
		if (uCS2 != USEASM_INTSRCSEL_SRC2)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is s1 then the eighth argument must be s2"));
		}
		if (psInst->asArg[7].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			puInst[1] |= (EURASIA_USE1_SOP3_DESTMOD_COMPLEMENTSRC2 << EURASIA_USE1_SOP3_DESTMOD_SHIFT);
		}
		puInst[1] |= (EURASIA_USE1_SOP3_COP_INTERPOLATE1 << EURASIA_USE1_SOP3_COP_SHIFT);
	}
	else if (uCS1 == USEASM_INTSRCSEL_SRC1 && (psInst->asArg[6].uFlags & USEASM_ARGFLAGS_COMPLEMENT))
	{
		if (uCS2 != USEASM_INTSRCSEL_SRC2)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is s1.comp then the eighth argument must be s2"));
		}
		if (psInst->asArg[7].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is s1.comp then eighth argument can't be s2.comp"));
		}
		puInst[1] |= (EURASIA_USE1_SOP3_COP_INTERPOLATE2 << EURASIA_USE1_SOP3_COP_SHIFT) |
					 (EURASIA_USE1_SOP3_DESTMOD_CS1SRC1 << EURASIA_USE1_SOP3_DESTMOD_SHIFT);
	}
	else if (uCS1 == USEASM_INTSRCSEL_ONE)
	{
		if (psInst->asArg[6].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is one then the complement flag can't be set"));
		}
		if (uCS2 != USEASM_INTSRCSEL_SRC2)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is s1.comp then the eighth argument must be s2"));
		}
		if (psInst->asArg[7].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "If the seventh argument to lrp2 is s1.comp then eighth argument can't be s2.comp"));
		}
		puInst[1] |= (EURASIA_USE1_SOP3_COP_INTERPOLATE2 << EURASIA_USE1_SOP3_COP_SHIFT);
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the seventh argument to lrp2 - valid choices are s1, s2 and one"));
	}

	if (!(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE) || (psInst->psNext == NULL))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "lrp2 must be coissued"));
	}
	else
	{
		PUSE_INST psCoissueInst = psInst->psNext;
		IMG_UINT32 uASel1 = 0;
		IMG_UINT32 uAOp;
		IMG_UINT32 uAMod1;

		/* Check the coissued operation. */
		if (psCoissueInst->uOpcode != USEASM_OP_ALRP)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "lrp2 must be coissued with alrp"));
		}

		/* Encode ASEL10 */
		if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to alrp must be an integer source selector"));
		}
		if ((psCoissueInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the first argument to alrp"));
		}
		if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
		}
		if (psCoissueInst->asArg[0].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uAOp = EURASIA_USE1_SOP3_AOP_INTERPOLATE2;
		}
		else
		{
			uAOp = EURASIA_USE1_SOP3_AOP_INTERPOLATE1;
		}
		switch (psCoissueInst->asArg[0].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO; break;
			case USEASM_INTSRCSEL_ONE:
			{
				uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO;
				uAOp = (uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE1) ? EURASIA_USE1_SOP3_AOP_INTERPOLATE2 : EURASIA_USE1_SOP3_AOP_INTERPOLATE1;
				break;
			}
			case USEASM_INTSRCSEL_SRC1ALPHA: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10SRC1; break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only zero, one and s1a are valid choices for the first argument to alrp")); break;
		}

		/* Encode ASEL11 */
		if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to alrp must be an integer source selector"));
		}
		if ((psCoissueInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the second argument to alrp"));
		}
		if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
		}
		if (psCoissueInst->asArg[1].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
		{
			uAMod1 = EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
		}
		else
		{
			uAMod1 = EURASIA_USE1_SOP3_AMOD1_NONE;
		}
		switch (psCoissueInst->asArg[1].uNumber)
		{
			case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO; break;
			case USEASM_INTSRCSEL_ONE:
			{
				uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO;
				uAMod1 ^= EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
				break;
			}
			case USEASM_INTSRCSEL_SRC2ALPHA: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11SRC2; break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only zero, one and s2a are valid choices for the second argument to alrp")); break;
		}

		puInst[1] |= (uASel1 << EURASIA_USE1_SOP3_ASEL1_SHIFT) |
					 (uAOp << EURASIA_USE1_SOP3_AOP_SHIFT) |
					 (uAMod1 << EURASIA_USE1_SOP3_AMOD1_SHIFT);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSOP3Instruction

 PURPOSE	: Encode a SOP3 instruction.

 PARAMETERS	: psTarget			- Assembly target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSOP3Instruction(PCSGX_CORE_DESC	psTarget,
									  PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uCSel2;
	IMG_UINT32 uCMod1, uCMod2;
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MAINISSUE, 0, 0);
	/* Check repeats */
	if (uMask != 0x1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask is not valid for this instruction"));
	}

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SOP3 << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "sop3 can't be repeated"));
	}
			
	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, uValidArgFlags);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 3, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to sop3 must be an integer source selection"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the fifth argument to sop3"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod1 = EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
	}
	else
	{
		uCMod1 = EURASIA_USE1_SOP3_CMOD1_NONE;
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_ONE:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_ZERO << EURASIA_USE1_SOP3_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_MINSRC1A1MSRC2A << EURASIA_USE1_SOP3_CSEL1_SHIFT);
			uCMod1 ^= EURASIA_USE1_SOP3_CMOD1_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC0ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC1ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2 << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_CSEL1_SRC2ALPHA << EURASIA_USE1_SOP3_CSEL1_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the fifth argument to sop3 - valid choices are zero, one, mina, rmina, s0, s0a, s1, s1a, s2, s2a"));
	}
	puInst[1] |= (uCMod1 << EURASIA_USE1_SOP3_CMOD1_SHIFT);

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to sop3 must be an integer source selection"));
	}
	if ((psInst->asArg[5].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the sixth argument to sop3"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		uCMod2 = EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
	}
	else
	{
		uCMod2 = EURASIA_USE1_SOP3_CMOD2_NONE;
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_ZERO: 
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_ZERO; 
			break;
		}
		case USEASM_INTSRCSEL_ONE:
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_ZERO;
			uCMod2 ^= EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRCALPHASAT: 
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A; 
			break;
		}
		case USEASM_INTSRCSEL_RSRCALPHASAT:
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A;
			uCMod2 ^= EURASIA_USE1_SOP3_CMOD2_COMPLEMENT;
			break;
		}
		case USEASM_INTSRCSEL_SRC0: 
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC0; 
			break;
		}
		case USEASM_INTSRCSEL_SRC0ALPHA: 
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC0ALPHA; 
			break;
		}
		case USEASM_INTSRCSEL_SRC1: 
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC1; 
			break;
		}
		case USEASM_INTSRCSEL_SRC1ALPHA:
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC1ALPHA; 
			break;
		}
		case USEASM_INTSRCSEL_SRC2:
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC2; 
			break;
		}
		case USEASM_INTSRCSEL_SRC2ALPHA:
		{
			uCSel2 = EURASIA_USE1_SOP3_CSEL2_SRC2ALPHA; 
			break;
		}
		default: 
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the sixth argument to sop3 - valid choices are zero, one, mina, rmina, s0, s0a, s1, s1a, s2, s2a"));
			uCSel2 = 0;
		}
	}
	puInst[1] |= (uCSel2 << EURASIA_USE1_SOP3_CSEL2_SHIFT);
	puInst[1] |= (uCMod2 << EURASIA_USE1_SOP3_CMOD2_SHIFT);

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to sop3 must be an integer source selection"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to sop3"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP3_COP_ADD << EURASIA_USE1_SOP3_COP_SHIFT); break;
		case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP3_COP_SUB << EURASIA_USE1_SOP3_COP_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the seventh argument to sop3 - valid choices are add or sub")); break;
	}

	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to sop3 must be an integer source selection"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to sop3"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	switch (psInst->asArg[7].uNumber)
	{
		case USEASM_INTSRCSEL_NONE:	break;
		case USEASM_INTSRCSEL_NEG: puInst[1] |= (EURASIA_USE1_SOP3_DESTMOD_NEGATE << EURASIA_USE1_SOP3_DESTMOD_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for eighth argument to sop3 - valid choices are none or neg")); break;
	}

	if (!(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "sop3 must be coissued"));
	}
	else
	{
		PUSE_INST psCoissueInst = psInst->psNext;
		if (psCoissueInst == NULL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "sop3 without a coissued instruction"));
		}
		if ((psCoissueInst->uOpcode != USEASM_OP_ALRP) && (psInst->asArg[7].uNumber == USEASM_INTSRCSEL_NEG))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "sop3 with a negate destination modifier must be coissued with alrp"));
		}
		if (psCoissueInst->uOpcode == USEASM_OP_ALRP)
		{
			IMG_UINT32 uASel1 = 0;
			IMG_UINT32 uAOp;
			IMG_UINT32 uAMod1;

			/* Encode ASEL10 */
			if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to alrp must be an integer source selector"));
			}
			if ((psCoissueInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the first argument to alrp"));
			}
			if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			if (psCoissueInst->asArg[0].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
			{
				uAOp = EURASIA_USE1_SOP3_AOP_INTERPOLATE2;
			}
			else
			{
				uAOp = EURASIA_USE1_SOP3_AOP_INTERPOLATE1;
			}
			switch (psCoissueInst->asArg[0].uNumber)
			{
				case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO; break;
				case USEASM_INTSRCSEL_ONE: 
				{
					uASel1 |= EURASIA_USE1_SOP3_ASEL1_10ZERO;
					uAOp = (uAOp == EURASIA_USE1_SOP3_AOP_INTERPOLATE1) ? EURASIA_USE1_SOP3_AOP_INTERPOLATE2 : EURASIA_USE1_SOP3_AOP_INTERPOLATE1; 
					break;
				}
				case USEASM_INTSRCSEL_SRC1ALPHA: uASel1 |= EURASIA_USE1_SOP3_ASEL1_10SRC1; break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the first argument to alrp - valid choices are zero, one and s1a")); break;
			}
			/* Encode ASEL10 */
			if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to alrp must be an integer source selector"));
			}
			if ((psCoissueInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the second argument to alrp"));
			}
			if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			if (psCoissueInst->asArg[1].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
			}
			else
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_NONE;
			}
			switch (psCoissueInst->asArg[1].uNumber)
			{
				case USEASM_INTSRCSEL_ZERO: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO; break;
				case USEASM_INTSRCSEL_ONE:
				{
					uASel1 |= EURASIA_USE1_SOP3_ASEL1_11ZERO;
					uAMod1 ^= EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
					break;
				}
				case USEASM_INTSRCSEL_SRC2ALPHA: uASel1 |= EURASIA_USE1_SOP3_ASEL1_11SRC2; break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the first argument to alrp - valid choices are zero, one and s2a"));
			}
			puInst[1] |= (uAMod1 << EURASIA_USE1_SOP3_AMOD1_SHIFT) |
						 (uASel1 << EURASIA_USE1_SOP3_ASEL1_SHIFT) |
						 (uAOp << EURASIA_USE1_SOP3_AOP_SHIFT);
		}
		else if (psCoissueInst->uOpcode == USEASM_OP_ASOP)
		{
			IMG_UINT32 uAMod1;

			if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to asop must be an integer source selector"));
			}
			if ((psCoissueInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the first argument to asop"));
			}
			if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			if (psCoissueInst->asArg[0].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
			}
			else
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_NONE;
			}
			switch (psCoissueInst->asArg[0].uNumber)
			{
				case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_ZERO << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_ONE:
				{
					puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_ZERO << EURASIA_USE1_SOP3_ASEL1_SHIFT);
					uAMod1 ^= EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
					break;
				}
				case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC0ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC1ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC2ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the first argument to asop - valid choices are zero, one, s0a, s1a or s2a")); break;
			}
			puInst[1] |= (uAMod1 << EURASIA_USE1_SOP3_AMOD1_SHIFT);

			if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to asop must be an integer source selector"));
			}
			if (psCoissueInst->asArg[1].uFlags != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to asop"));
			}
			if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			switch (psCoissueInst->asArg[1].uNumber)
			{
				case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP3_AOP_ADD << EURASIA_USE1_SOP3_AOP_SHIFT); break;
				case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP3_AOP_SUB << EURASIA_USE1_SOP3_AOP_SHIFT); break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to asop - valid choices are add or sub")); break;
			}
		}
		else if (psCoissueInst->uOpcode == USEASM_OP_ARSOP)
		{
			IMG_UINT32 uAMod1;

			/*
				Check for BRN25060:

					COP = add/subtract  AND
					AOP = add/subtract AND
					DESTMOD = 1  AND
					CMOD2  = 1 AND
					CSEL2 != 001
			*/
			if (FixBRN25060(psTarget))
			{
				if (uCMod2 == EURASIA_USE1_SOP3_CMOD2_COMPLEMENT &&
					uCSel2 != EURASIA_USE1_SOP3_CSEL2_MINSRC1A1MSRC2A)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "SOP3 with ARSOP and a complement modifier on CSEL2 is broken on this core"));
				}
			}

			if (psCoissueInst->asArg[0].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to arsop must be an integer source selector"));
			}
			if ((psCoissueInst->asArg[0].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is valid for the first argument to arsop"));
			}
			if (psCoissueInst->asArg[0].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			/* Set the DESTMOD bit for arsop */
			puInst[1] |= (EURASIA_USE1_SOP3_DESTMOD_NEGATE << EURASIA_USE1_SOP3_DESTMOD_SHIFT);
			if (psCoissueInst->asArg[0].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
			}
			else
			{
				uAMod1 = EURASIA_USE1_SOP3_AMOD1_NONE;
			}
			switch (psCoissueInst->asArg[0].uNumber)
			{
				case USEASM_INTSRCSEL_ZERO: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_ZERO << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_ONE:
				{
					puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_ZERO << EURASIA_USE1_SOP3_ASEL1_SHIFT);
					uAMod1 ^= EURASIA_USE1_SOP3_AMOD1_COMPLEMENT;
					break;
				}
				case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC0ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC1ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_SOP3_ASEL1_SRC2ALPHA << EURASIA_USE1_SOP3_ASEL1_SHIFT); break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the first argument to arsop - valid choices are zero, one, s0a, s1a or s2a")); break;
			}
			puInst[1] |= (uAMod1 << EURASIA_USE1_SOP3_AMOD1_SHIFT);

			if (psCoissueInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to arsop must be an integer source selector"));
			}
			if (psCoissueInst->asArg[1].uFlags != 0)
			{	
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to arsop"));
			}
			if (psCoissueInst->asArg[1].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
			}
			switch (psCoissueInst->asArg[1].uNumber)
			{
				case USEASM_INTSRCSEL_ADD: puInst[1] |= (EURASIA_USE1_SOP3_AOP_ADD << EURASIA_USE1_SOP3_AOP_SHIFT); break;
				case USEASM_INTSRCSEL_SUB: puInst[1] |= (EURASIA_USE1_SOP3_AOP_SUB << EURASIA_USE1_SOP3_AOP_SHIFT); break;
				default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the second argument to arsop - valid choices are add or sub")); break;
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeFPMAInstruction

 PURPOSE	: Encode a FPMA instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFPMAInstruction(PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext,
									  PCSGX_CORE_DESC	psTarget)
{
	IMG_BOOL bFmtControl = IMG_FALSE;
	IMG_UINT32 uValidArgFlags = 0;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	if (psInst->uFlags2 & USEASM_OPFLAGS2_FORMATSELECT)
	{
		bFmtControl = IMG_TRUE;
		uValidArgFlags = USEASM_ARGFLAGS_FMTC10;
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | 
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 
				USEASM_OPFLAGS2_SAT, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0) |
				((psInst->uFlags2 & USEASM_OPFLAGS2_SAT) ? EURASIA_USE1_IMA8_SAT : 0);
	if (psInst->uOpcode == USEASM_OP_IMA8)
	{
		puInst[1] |= (EURASIA_USE1_OP_IMA8 << EURASIA_USE1_OP_SHIFT);
	}
	else
	{
		puInst[1] |= (EURASIA_USE1_OP_FPMA << EURASIA_USE1_OP_SHIFT);
	}
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	CheckArgFlags(psContext, psInst, 0, uValidArgFlags);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | uValidArgFlags);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 2, uValidArgFlags);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);
	CheckArgFlags(psContext, psInst, 3, uValidArgFlags);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, bFmtControl, USEASM_ARGFLAGS_FMTC10, psTarget);

	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		puInst[1] |= EURASIA_USE1_IMA8_NEGS0;
	}

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[4].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the fifth argument to ima8"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_CMOD0_COMPLEMENT << EURASIA_USE1_IMA8_CMOD0_SHIFT);
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_SRC0: puInst[1] |= (EURASIA_USE1_IMA8_CSEL0_SRC0 << EURASIA_USE1_IMA8_CSEL0_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_IMA8_CSEL0_SRC1 << EURASIA_USE1_IMA8_CSEL0_SHIFT); break;
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_CSEL0_SRC0ALPHA << EURASIA_USE1_IMA8_CSEL0_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_CSEL0_SRC1ALPHA << EURASIA_USE1_IMA8_CSEL0_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only s0, s1, s0a and s1a are valid choices for the fifth argument to ima8"));
	}

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[5].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the sixth argument to ima8"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_CMOD1_COMPLEMENT << EURASIA_USE1_IMA8_CMOD1_SHIFT);
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_SRC1: puInst[1] |= (EURASIA_USE1_IMA8_CSEL1_SRC1 << EURASIA_USE1_IMA8_CSEL1_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_CSEL1_SRC1ALPHA << EURASIA_USE1_IMA8_CSEL1_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only s1 and s1a are valid choices for the sixth argument to ima8"));
	}

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[6].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the seventh argument to ima8"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[6].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_CMOD2_COMPLEMENT << EURASIA_USE1_IMA8_CMOD2_SHIFT);
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_SRC2: puInst[1] |= (EURASIA_USE1_IMA8_CSEL2_SRC2 << EURASIA_USE1_IMA8_CSEL2_SHIFT); break;
		case USEASM_INTSRCSEL_SRC2ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_CSEL2_SRC2ALPHA << EURASIA_USE1_IMA8_CSEL2_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only s2 and s2a are valid choices for the seventh argument to ima8"));
	}

	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[7].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the eighth argument to ima8"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[7].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_AMOD0_COMPLEMENT << EURASIA_USE1_IMA8_AMOD0_SHIFT);
	}
	switch (psInst->asArg[7].uNumber)
	{
		case USEASM_INTSRCSEL_SRC0ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_ASEL0_SRC0ALPHA << EURASIA_USE1_IMA8_ASEL0_SHIFT); break;
		case USEASM_INTSRCSEL_SRC1ALPHA: puInst[1] |= (EURASIA_USE1_IMA8_ASEL0_SRC1ALPHA << EURASIA_USE1_IMA8_ASEL0_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only s0a and s1a are valid choices for the eighth argument to ima8"));
	}

	if (psInst->asArg[8].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[8].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the nineth argument to ima8"));
	}
	if (psInst->asArg[8].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[8].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_AMOD1_COMPLEMENT << EURASIA_USE1_IMA8_AMOD1_SHIFT);
	}
	if (psInst->asArg[8].uNumber != USEASM_INTSRCSEL_SRC1ALPHA)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only s1a is a valid choice for the nineth argument to ima8"));
	}

	if (psInst->asArg[9].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The tenth argument to ima8 must be an integer source selection"));
	}
	if ((psInst->asArg[9].uFlags & ~USEASM_ARGFLAGS_COMPLEMENT) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only .comp is a valid flag for the tenth argument to ima8"));
	}
	if (psInst->asArg[9].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector is not indexable"));
	}
	if (psInst->asArg[9].uFlags & USEASM_ARGFLAGS_COMPLEMENT)
	{
		puInst[1] |= (EURASIA_USE1_IMA8_AMOD2_COMPLEMENT << EURASIA_USE1_IMA8_AMOD2_SHIFT);
	}
	if (psInst->asArg[9].uNumber != USEASM_INTSRCSEL_SRC2ALPHA)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only s2a is a valid choice for the tenth argument to ima8"));
	}
}

/*****************************************************************************
 FUNCTION	: EncodeIMA16Instruction

 PURPOSE	: Encode a IMA16 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeIMA16Instruction(PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext,
									   PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32 uSrc1Fmt;
	IMG_UINT32 uSrc2Fmt;
	IMG_BOOL bS1Signed = FALSE, bS2Signed = FALSE;
	IMG_UINT32 uDestShift;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_ABS |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 
				USEASM_OPFLAGS2_SIGNED | USEASM_OPFLAGS2_UNSIGNED |
				(~USEASM_OPFLAGS2_RSHIFT_CLRMSK) | USEASM_OPFLAGS2_SAT, 0);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_IMA16 << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->asArg[3].uFlags & USEASM_ARGFLAGS_NEGATE) ? EURASIA_USE1_IMA16_S2NEG : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode destination shift. */
	uDestShift = (psInst->uFlags2 & ~USEASM_OPFLAGS2_RSHIFT_CLRMSK) >> USEASM_OPFLAGS2_RSHIFT_SHIFT;
	if (uDestShift > EURASIA_USE1_IMA16_ORSHIFT_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum destination shift for an ima16 instruction is %d", EURASIA_USE1_IMA16_ORSHIFT_MAX));
	}
	puInst[1] |= (uDestShift << EURASIA_USE1_IMA16_ORSHIFT_SHIFT);
	/* Encode destination modifier. */
	if (psInst->asArg[0].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
	{
		puInst[1] |= EURASIA_USE1_IMA16_ABS;
	}
	/* Encode instruction mode. */
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SIGNED)
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_UNSIGNED)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .s and .u can be specified on an instruction"));
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
		{
			puInst[1] |= (EURASIA_USE1_IMA16_MODE_SGNSAT << EURASIA_USE1_IMA16_MODE_SHIFT);
		}
		else
		{
			puInst[1] |= (EURASIA_USE1_IMA16_MODE_SGNNOSAT << EURASIA_USE1_IMA16_MODE_SHIFT);
		}
	}
	else
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
		{
			puInst[1] |= (EURASIA_USE1_IMA16_MODE_UNSGNSAT << EURASIA_USE1_IMA16_MODE_SHIFT);
		}
		else
		{
			puInst[1] |= (EURASIA_USE1_IMA16_MODE_UNSGNNOSAT << EURASIA_USE1_IMA16_MODE_SHIFT);
		}
	}
	/* Get the source formats. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to ima16 must be a data format"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to ima16 isn't indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to ima16"));
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U16: uSrc1Fmt = EURASIA_USE1_IMA16_SFORM_16BIT; bS1Signed = TRUE; break;
		case USEASM_INTSRCSEL_U8: uSrc1Fmt = EURASIA_USE1_IMA16_SFORM_8BITZEROEXTEND; break;
		case USEASM_INTSRCSEL_S8: uSrc1Fmt = EURASIA_USE1_IMA16_SFORM_8BITSGNEXTEND; bS1Signed = TRUE; break;
		case USEASM_INTSRCSEL_O8: uSrc1Fmt = EURASIA_USE1_IMA16_SFORM_8BITOFFSET; bS1Signed = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid source data format for ima16 - valid choices are u16, u8, s8 or off8")); uSrc1Fmt = 0;
	}
	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ima16 must be a data format"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ima16 isn't indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to ima16"));
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_U16: uSrc2Fmt = EURASIA_USE1_IMA16_SFORM_16BIT; bS2Signed = TRUE; break;
		case USEASM_INTSRCSEL_U8: uSrc2Fmt = EURASIA_USE1_IMA16_SFORM_8BITZEROEXTEND; break;
		case USEASM_INTSRCSEL_S8: uSrc2Fmt = EURASIA_USE1_IMA16_SFORM_8BITSGNEXTEND; bS2Signed = TRUE; break;
		case USEASM_INTSRCSEL_O8: uSrc2Fmt = EURASIA_USE1_IMA16_SFORM_8BITOFFSET; bS2Signed = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid source data format for ima16 - valid choices are u16, u8, s8 or off8")); uSrc2Fmt = 0;
	}
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 1, 0);
	if (uSrc1Fmt == EURASIA_USE1_IMA16_SFORM_16BIT)
	{
		CheckArgFlags(psContext, psInst, 2, 0);
	}
	else
	{
		CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	}
	if (uSrc2Fmt == EURASIA_USE1_IMA16_SFORM_16BIT)
	{
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE);
	}
	else
	{
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	}
	CheckArgFlags(psContext, psInst, 0, USEASM_ARGFLAGS_ABSOLUTE);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, IMG_FALSE, 0, psTarget);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bS1Signed, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bS2Signed, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	/* Encode the source formats */
	puInst[1] |= uSrc1Fmt << EURASIA_USE1_IMA16_S1FORM_SHIFT;
	puInst[1] |= uSrc2Fmt << EURASIA_USE1_IMA16_S2FORM_SHIFT;
	/* Encode the high-low select. */
	if (uSrc1Fmt != EURASIA_USE1_IMA16_SFORM_16BIT && psInst->asArg[2].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		if (!((psInst->asArg[2].uFlags & USEASM_ARGFLAGS_HIGH) || 
			  (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_LOW)))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Second argument to ima16 is 8-bit but neither .low nor .high are specified"));
		}
		puInst[1] |= (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_HIGH) ? EURASIA_USE1_IMA16_SEL1H_UPPER8 : 0;
	}
	if (uSrc2Fmt != EURASIA_USE1_IMA16_SFORM_16BIT && psInst->asArg[3].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		if (!((psInst->asArg[3].uFlags & USEASM_ARGFLAGS_HIGH) || 
			  (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_LOW)))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Third argument to ima16 is 8-bit but neither .low nor .high are specified"));
		}
		puInst[1] |= (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_HIGH) ? EURASIA_USE1_IMA16_SEL2H_UPPER8 : 0;
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
static IMG_VOID EncodeBRN33442_Predicate(PCSGX_CORE_DESC	psTarget,
										 PUSE_INST			psInst,
										 IMG_PUINT32		puInst,
										 PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: EncodeBRN33442_Predicate

 PURPOSE	: Encode the predicate for instructions affected by BRN33442 (IMA32
			  and IDIV).

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (FixBRN33442(psTarget))
	{
		puInst[1] |= (EncodePredicate(psContext, psInst, IMG_TRUE /* bShort */) << EURASIA_USE1_SPRED_SHIFT);
	}
	else
	{
		puInst[1] |= (EncodePredicate(psContext, psInst, IMG_FALSE /* bShort */) << EURASIA_USE1_EPRED_SHIFT);
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) || defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */

#if defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
/*****************************************************************************
 FUNCTION	: EncodeIDIVInstruction

 PURPOSE	: Encode a IDIV instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeIDIVInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst,
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	if (!IsIDIVInstruction(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The idiv instruction isn't available on this processor"));
	}
	if (FixBRN27005(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The idiv instruction is non-functional on this core"));
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
				(~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 0, 0);
	/* Encode opcode and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_32BITINT << EURASIA_USE1_OP_SHIFT) |
				(SGX545_USE1_32BITINT_OP2_IDIV << EURASIA_USE1_32BITINT_OP2_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);

	/* Encode predicate. */
	EncodeBRN33442_Predicate(psTarget, psInst, puInst, psContext);

	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		if (FixBRN26570(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat count isn't supported on %s on this core", OpcodeName(psInst->uOpcode)));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode the destination register. */
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for IDIV can't be indexed"));
	}
	if (psInst->asArg[0].uType != USEASM_REGTYPE_PRIMATTR &&
		psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for IDIV must be the primary attribute bank or the temporary bank"));
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		puInst[1] |= SGX545_USE1_IDIV_DBANK_TEMP;
	}
	else
	{
		puInst[1] |= SGX545_USE1_IDIV_DBANK_PRIMATTR;
	}
	puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
	/* Encode the source registers. */
	EncodeSrc1(psContext, psInst, 1, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	EncodeSrc2(psContext, psInst, 2, IMG_TRUE, EURASIA_USE1_S2BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	/* Encode the drc select. */
	if (psInst->asArg[3].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to an IDIV instruction must be in the dependent read counter bank"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A dependent read counter register can't be accessed in indexed mode"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a dependent read counter register"));
	}
	if (psInst->asArg[3].uNumber > SGX545_USE1_IDIV_DRCSEL_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum number of a drc register is %d", SGX545_USE1_IDIV_DRCSEL_MAX));
	}
	puInst[1] |= psInst->asArg[3].uNumber << SGX545_USE1_IDIV_DRCSEL_SHIFT;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */

#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD)
/*****************************************************************************
 FUNCTION	: EncodeIMA32Instruction

 PURPOSE	: Encode a IMA32 instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeIMA32Instruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst,
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_BOOL bSigned;
	IMG_UINT32 uSrc1Negate;
	IMG_UINT32 uValidFlags3;
	if (!IsIMA32Instruction(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The ima32 instruction isn't available on this processor"));
	}

	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
	if (IMA32Is32x16Plus32(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_STEP1 | USEASM_OPFLAGS3_STEP2;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
				(~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 
				USEASM_OPFLAGS2_SIGNED | USEASM_OPFLAGS2_UNSIGNED, uValidFlags3);
	
	/* Encode opcode and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_32BITINT << EURASIA_USE1_OP_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);

	/* Encode predicate. */
	EncodeBRN33442_Predicate(psTarget, psInst, puInst, psContext);

	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode destination. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, IMG_TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	
	/* Encode the destination for the upper 32 bits. */
	if (psInst->asArg[1].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
		if (IMA32Is32x16Plus32(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A second destination for IMA32 isn't supported on this core"));
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */

		puInst[1] |= EURASIA_USE1_IMA32_GPIWEN;
		if (psInst->asArg[1].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second destination of IMA32"));
		}
		if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second destination of IMA32 can't be indexed"));
		}
		if (psInst->asArg[1].uNumber > EURASIA_USE1_IMA32_GPISRC_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum register number for the second destination of IMA32 is %d", EURASIA_USE1_IMA32_GPISRC_MAX));
		}
		puInst[1] |= (psInst->asArg[1].uNumber << EURASIA_USE1_IMA32_GPISRC_SHIFT);
	}
	else
	{
		if (psInst->asArg[1].uType != USEASM_REGTYPE_INTSRCSEL ||
			psInst->asArg[1].uNumber != USEASM_INTSRCSEL_NONE ||
			psInst->asArg[1].uFlags != 0 ||
			psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only an internal register or nothing are valid as a destination for the upper 32-bits of the IMA32 result"));
		}
	}
	/* Encode the instruction modes. */
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SIGNED)
	{
		bSigned = IMG_TRUE;
		if (psInst->uFlags2 & USEASM_OPFLAGS2_UNSIGNED)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .s and .u can be specified on an instruction"));	
		}
		puInst[1] |= EURASIA_USE1_IMA32_SGN;
	}
	else
	{
		bSigned = IMG_FALSE;
	}
	/* Encode sources. */
	CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_NEGATE);
	EncodeSrc0(psContext, psInst, 2, IMG_TRUE, puInst + 0, puInst + 1, EURASIA_USE1_IMA32_S0BEXT, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE);
	EncodeSrc1(psContext, psInst, 3, IMG_TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 4, USEASM_ARGFLAGS_NEGATE);
	EncodeSrc2(psContext, psInst, 4, IMG_TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	/* Encode carry-in. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ima32 must be an integer source select"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ima32 isn't indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to ima32"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_CINENABLE)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
		if (IMA32Is32x16Plus32(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_STEP2))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A carry-in isn't supported on step2"));
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */

		if (psInst->asArg[6].uType != USEASM_REGTYPE_FPINTERNAL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to ima32 must be an internal register"));
		}
		if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to ima32 isn't indexable"));
		}
		if (psInst->asArg[6].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to ima32"));
		}	
		switch (psInst->asArg[6].uNumber)
		{
			case 0: puInst[1] |= (EURASIA_USE1_IMA32_CIEN_I0_BIT0 << EURASIA_USE1_IMA32_CIEN_SHIFT); break;
			case 1: puInst[1] |= (EURASIA_USE1_IMA32_CIEN_I1_BIT0 << EURASIA_USE1_IMA32_CIEN_SHIFT); break;
			case 2: puInst[1] |= (EURASIA_USE1_IMA32_CIEN_I2_BIT0 << EURASIA_USE1_IMA32_CIEN_SHIFT); break;
			default: USEASM_ERRMSG((psContext->pvContext, psInst, "Only i0, i1 or i2 are valid for the seventh argument to ima32"));
		}
	}
	else if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_NONE)
	{
		puInst[1] |= (EURASIA_USE1_IMA32_CIEN_DISABLED << EURASIA_USE1_IMA32_CIEN_SHIFT);
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only cie and nothing are valid for the sixth argument to ima32"));
	}
	
	/* Apply a negate on source 0 by negating source 1. */
	uSrc1Negate = psInst->asArg[3].uFlags & USEASM_ARGFLAGS_NEGATE;
	if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		uSrc1Negate ^= USEASM_ARGFLAGS_NEGATE;
	}
	/* Encode source modes. */
	if (uSrc1Negate & USEASM_ARGFLAGS_NEGATE)
	{
		puInst[1] |= EURASIA_USE1_IMA32_NEGS1;
	}
	if (psInst->asArg[4].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		puInst[1] |= EURASIA_USE1_IMA32_NEGS2;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32)
	if (IMA32Is32x16Plus32(psTarget))
	{
		/*
			Encode the STEP select.
		*/
		if (psInst->uFlags3 & USEASM_OPFLAGS3_STEP1)
		{
			puInst[1] |= (SGXVEC_USE1_IMA32_STEP_STEP1 << SGXVEC_USE1_IMA32_STEP_SHIFT);
		}
		else if (psInst->uFlags3 & USEASM_OPFLAGS3_STEP2)
		{
			puInst[1] |= (SGXVEC_USE1_IMA32_STEP_STEP2 << SGXVEC_USE1_IMA32_STEP_SHIFT);
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Either the .step1 or .step2 flags must be set on IMA32"));
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IMA32_32X16_PLUS_32) */
	{
		puInst[1] |= (EURASIA_USE1_32BITINT_OP2_IMA32 << EURASIA_USE1_32BITINT_OP2_SHIFT);
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) */

/*****************************************************************************
 FUNCTION	: EncodeIMAEInstruction

 PURPOSE	: Encode a IMAE instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeIMAEInstruction(PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext,
									  PCSGX_CORE_DESC		psTarget)
{
	IMG_BOOL bS1Signed = FALSE, bS2Signed = FALSE;
	IMG_UINT32 uSrc2Fmt;
	IMG_UINT32 uDestShift;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | 
			(~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 
			USEASM_OPFLAGS2_SIGNED | USEASM_OPFLAGS2_UNSIGNED | USEASM_OPFLAGS2_SAT |
			(~USEASM_OPFLAGS2_RSHIFT_CLRMSK), 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_IMAE << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > 8)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is 8"));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode destination shift. */
	uDestShift = (psInst->uFlags2 & ~USEASM_OPFLAGS2_RSHIFT_CLRMSK) >> USEASM_OPFLAGS2_RSHIFT_SHIFT;
	if (uDestShift > EURASIA_USE1_IMAE_ORSHIFT_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum destination shift for an imae instruction is %d", EURASIA_USE1_IMAE_ORSHIFT_MAX));
	}
	puInst[1] |= (uDestShift << EURASIA_USE1_IMAE_ORSHIFT_SHIFT);
	/* Encode instruction modes. */
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SIGNED)
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_UNSIGNED)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .s and .u can be specified on an instruction"));
		}
		puInst[1] |= EURASIA_USE1_IMAE_SIGNED;
		bS1Signed = TRUE;
	}
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SAT)
	{
		puInst[1] |= EURASIA_USE1_IMAE_SATURATE;
	}

	/* Encode source 2 format. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to imae must be a data format"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to imae isn't indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to imae"));
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U32: uSrc2Fmt = EURASIA_USE1_IMAE_SRC2TYPE_32BIT; bS2Signed = TRUE; break;
		case USEASM_INTSRCSEL_Z16: uSrc2Fmt = EURASIA_USE1_IMAE_SRC2TYPE_16BITZEXTEND; break;
		case USEASM_INTSRCSEL_S16: uSrc2Fmt = EURASIA_USE1_IMAE_SRC2TYPE_16BITSEXTEND; bS2Signed = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid data format for imae")); uSrc2Fmt = 0; break;
	}
	puInst[1] |= (uSrc2Fmt << EURASIA_USE1_IMAE_SRC2TYPE_SHIFT);
	/* Encode high/low word selects. */
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_HIGH)
	{
		puInst[1] |= EURASIA_USE1_IMAE_SRC0H_SELECTHIGH;
	}
	else if (!(psInst->asArg[1].uFlags & USEASM_ARGFLAGS_LOW) && psInst->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "One of .low or .high should be specified for the first source to imae"));
	}
	if (psInst->asArg[2].uFlags & USEASM_ARGFLAGS_HIGH)
	{
		puInst[1] |= EURASIA_USE1_IMAE_SRC1H_SELECTHIGH;
	}
	else if (!(psInst->asArg[2].uFlags & USEASM_ARGFLAGS_LOW) && psInst->asArg[2].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "One of .low or .high should be specified for the second source to imae"));
	}
	if (uSrc2Fmt != EURASIA_USE1_IMAE_SRC2TYPE_32BIT)
	{
		if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_HIGH)
		{
			puInst[1] |= EURASIA_USE1_IMAE_SRC2H_SELECTHIGH;
		}
		else if (!(psInst->asArg[3].uFlags & USEASM_ARGFLAGS_LOW))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "One of .low or .high should be specified for the third source to imae"));
		}
	}
	else
	{
		if ((psInst->asArg[3].uFlags & (USEASM_ARGFLAGS_LOW | USEASM_ARGFLAGS_HIGH)) != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, ".low and .high are meaningless for a 32-bit source"));
		}
	}
	/* Encode carry out. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to imae must be coe or nothing"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to imae isn't indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to imae"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_COUTENABLE)
	{
		puInst[1] |= EURASIA_USE1_IMAE_CARRYOUTENABLE;
	}
	else if (psInst->asArg[5].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only coe or nothing are valid choices for the sixth argument to imae"));
	}
	/* Encode carry in. */
	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to imae must be cie or nothing"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to imae isn't indexable"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to imae"));
	}
	if (psInst->asArg[6].uNumber == USEASM_INTSRCSEL_CINENABLE)
	{
		puInst[1] |= EURASIA_USE1_IMAE_CARRYINENABLE;
	}
	else if (psInst->asArg[6].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only cie or nothing are valid choices for the seventh argument to imae"));
	}
	/* Encode carry source/dest */
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_COUTENABLE ||
		psInst->asArg[6].uNumber == USEASM_INTSRCSEL_CINENABLE)
	{
		if (psInst->asArg[7].uType != USEASM_REGTYPE_FPINTERNAL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth source to imae should be in the floating point internal bank"));
		}
		if (psInst->asArg[7].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the eighth source to imae"));
		}
		if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth source to imae can't be indexed"));
		}
		if (psInst->asArg[7].uNumber > EURASIA_USE1_IMAE_CISRC_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum register number for the eighth source to imae is %d", EURASIA_USE1_IMAE_CISRC_MAX));
		}
		if (psInst->asArg[7].uNumber == 0)
		{
			puInst[1] |= (EURASIA_USE1_IMAE_CISRC_I0 << EURASIA_USE1_IMAE_CISRC_SHIFT);
		}
		else if (psInst->asArg[7].uNumber == 1)
		{
			puInst[1] |= (EURASIA_USE1_IMAE_CISRC_I1 << EURASIA_USE1_IMAE_CISRC_SHIFT);
		}
	}
	/* Encode instruction sources. */
	CheckArgFlags(psContext, psInst, 0, USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bS1Signed, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_HIGH | USEASM_ARGFLAGS_LOW);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bS2Signed, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeADIFInstruction

 PURPOSE	: Encode a ADIF instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeADIFInstruction(PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext,
									  PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
			  (~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 0, 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_ADIFFIRVBILIN << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_ADIFFIRVBILIN_OPSEL_ADIF << EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an integer instruction is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode the banks and offsets of the sources and destination. */
	if (psInst->uOpcode == USEASM_OP_ADIF)
	{
		CheckArgFlags(psContext, psInst, 0, 0);
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 1, 0);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 2, 0);
		EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		CheckArgFlags(psContext, psInst, 0, 0);
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_LOW | USEASM_ARGFLAGS_HIGH);
		EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 2, 0);
		EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 3, 0);
		EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
			
		/* For the sum variant encode the low/high word selects. */
		puInst[1] |= 
			    EURASIA_USE1_ADIF_SUM |
				((psInst->asArg[1].uFlags & USEASM_ARGFLAGS_HIGH) ? EURASIA_USE1_ADIF_SRC0H_SELECTHIGH : 0);			
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSSUM16Instruction

 PURPOSE	: Encode a SSUM16 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSSUM16Instruction(PCSGX_CORE_DESC		psTarget,
										PUSE_INST			psInst, 
										IMG_PUINT32			puInst,
										PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uDestShift;

	if (!IsHighPrecisionFIR(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "ssum16 isn't available on this processor"));
	}

	/* Check the instruction flags. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
				(~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, (~USEASM_OPFLAGS2_RSHIFT_CLRMSK), 0);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_ADIFFIRVBILIN << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SSUM16 << EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	/* Encode destination shift. */
	uDestShift = (psInst->uFlags2 & ~USEASM_OPFLAGS2_RSHIFT_CLRMSK) >> USEASM_OPFLAGS2_RSHIFT_SHIFT;
	if (uDestShift > EURASIA_USE1_SSUM16_RSHIFT_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum destination shift for an ima16 instruction is %d", EURASIA_USE1_SSUM16_RSHIFT_MAX));
	}
	puInst[1] |= (uDestShift << EURASIA_USE1_SSUM16_RSHIFT_SHIFT);
	/* Encode repeat count */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_SSUM16_RCOUNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on an SSUM16 instruction is %d", EURASIA_USE1_SSUM16_RCOUNT_MAX));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode the banks and offsets of the sources and destination. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);

	/* Encode instruction modes. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to ssum16 must be an integer source selector"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to ssum16"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[4].uNumber == USEASM_INTSRCSEL_IDST)
	{
		puInst[1] |= EURASIA_USE1_SSUM16_IDST;
	}
	else if (psInst->asArg[4].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only idst or nothing is valid as a choice for the fifth argument to ssum16"));
	}

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to ssum16 must be an integer source selector"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to ssum16"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_IADD)
	{
		puInst[1] |= EURASIA_USE1_SSUM16_IADD;
	}
	else if (psInst->asArg[5].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only iadd or nothing is valid as a choice for the sixth argument to ssum16"));
	}

	if (psInst->asArg[6].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to ssum16 must be an integer source selector"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to ssum16"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An internal register isn't indexable"));
	}
	if (psInst->asArg[6].uNumber == 0)
	{
		puInst[1] |= EURASIA_USE1_SSUM16_ISEL_I0;
	}
	else if (psInst->asArg[6].uNumber == 1)
	{
		puInst[1] |= EURASIA_USE1_SSUM16_ISEL_I1;
	}
	else if (psInst->asArg[6].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only i0 or r1 valid as a choice for the seventh argument to ssum16"));
	}

	/*
		Encode the rounding mode.
	*/
	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to ssum16 must be an integer source selector"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the eighth argument to ssum16"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	switch (psInst->asArg[7].uNumber)
	{
		case USEASM_INTSRCSEL_ROUNDDOWN:
		{
			puInst[1] |= (EURASIA_USE1_SSUM16_RMODE_DOWN << EURASIA_USE1_SSUM16_RMODE_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_ROUNDNEAREST:
		{
			puInst[1] |= (EURASIA_USE1_SSUM16_RMODE_NEAREST << EURASIA_USE1_SSUM16_RMODE_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_ROUNDUP:
		{
			puInst[1] |= (EURASIA_USE1_SSUM16_RMODE_UP << EURASIA_USE1_SSUM16_RMODE_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only iadd or nothing is valid as a choice for the eighth argument to ssum16"));
		}
	}

	/*
		Encode the source format.
	*/
	if (psInst->asArg[8].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to ssum16 must be an integer source selector"));
	}
	if (psInst->asArg[8].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the nineth argument to ssum16"));
	}
	if (psInst->asArg[8].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	switch (psInst->asArg[8].uNumber)
	{
		case USEASM_INTSRCSEL_U8:
		{
			puInst[1] |= (EURASIA_USE1_SSUM16_SRCFORMAT_U8 << EURASIA_USE1_SSUM16_SRCFORMAT_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_S8:
		{
			puInst[1] |= (EURASIA_USE1_SSUM16_SRCFORMAT_S8 << EURASIA_USE1_SSUM16_SRCFORMAT_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only u8 or s8 are valid as a choice for the nineth argument to ssum16"));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeFIRVInstruction

 PURPOSE	: Encode a FIRV instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.U.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFIRVInstruction(PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext,
									  PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_BOOL bSigned = FALSE;

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), 0, 0);
	/* Encode opcode and instruction modes. */
	puInst[1] = (EURASIA_USE1_OP_ADIFFIRVBILIN << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_ADIFFIRVBILIN_OPSEL_FIRV << EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0) |
				(!(psInst->asArg[0].uFlags & USEASM_ARGFLAGS_DISABLEWB) ? EURASIA_USE1_FIRV_WBENABLE : 0);
	puInst[0] = 0;
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on firv is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}

	/* Check restrictions on source0/source1 register types. */
	if (psInst->asArg[1].uType != psInst->asArg[2].uType ||
		psInst->asArg[1].uIndex != psInst->asArg[2].uIndex)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register banks for source 0 and source 1 to firv must be the same"));
	}
	if (!(psInst->asArg[1].uType == USEASM_REGTYPE_TEMP ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_OUTPUT ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_PRIMATTR ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_SECATTR) ||
		psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register bank for firv source 0 must be either temporary, output, "
			"primary attribute or secondary attribute and non-indexed"));
	}

	/* Encode source format. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firv must specify the source data format"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to firv"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firv isn't indexable"));
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U8: puInst[1] |= EURASIA_USE1_FIRV_SRCFORMAT_U8 << EURASIA_USE1_FIRV_SRCFORMAT_SHIFT; break;
		case USEASM_INTSRCSEL_S8: puInst[1] |= EURASIA_USE1_FIRV_SRCFORMAT_S8 << EURASIA_USE1_FIRV_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		case USEASM_INTSRCSEL_O8: puInst[1] |= EURASIA_USE1_FIRV_SRCFORMAT_O8 << EURASIA_USE1_FIRV_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firv must specify the source data format")); break;
	}

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to firv"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_PLUSSRC2DOT2)
	{
		puInst[1] |= EURASIA_USE1_FIRV_COEFADD;
	}
	else if (psInst->asArg[5].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only +src2.2 or nothing is valid as a choice for the sixth argument to firv"));
	}

	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to firv"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[6].uNumber == USEASM_INTSRCSEL_IADD)
	{
		puInst[1] |= EURASIA_USE1_IREGSRC;
	}
	else if (psInst->asArg[6].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only iadd or nothing is valid as a choice for the seventh argument to firv"));
	}

	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the eighth argument to firv"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[7].uNumber == USEASM_INTSRCSEL_SCALE)
	{
		puInst[1] |= EURASIA_USE1_FIRV_SHIFTEN;
	}
	else if (psInst->asArg[7].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only scale or nothing is valid as a choice for the eighth argument to firv"));
	}

	/* Encode the destination and source arguments. */
	CheckArgFlags(psContext, psInst, 0, USEASM_ARGFLAGS_DISABLEWB);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	puInst[0] |= (psInst->asArg[1].uNumber << EURASIA_USE0_SRC0_SHIFT);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeFIRVHInstruction

 PURPOSE	: Encode a FIRVH instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFIRVHInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_BOOL bSigned = FALSE;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	if (!IsHighPrecisionFIR(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "firvh isn't available on this processor"));
	}

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED |
			(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), 0, 0);
	/* Encode opcode and instruction modes. */
	puInst[1] = (EURASIA_USE1_OP_ADIFFIRVBILIN << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_ADIFFIRVBILIN_OPSEL_FIRV << EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT) |
				(EURASIA_USE1_FIRV_SRCFORMAT_FIRVH << EURASIA_USE1_FIRV_SRCFORMAT_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0) |
				(!(psInst->asArg[0].uFlags & USEASM_ARGFLAGS_DISABLEWB) ? EURASIA_USE1_FIRV_WBENABLE : 0);
	puInst[0] = 0;
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on firvh is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}

	/* Check restrictions on source0 register types. */
	if (psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register bank for firvh source 0 must be the internal registers"));
	}

	if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to firv"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[3].uNumber == USEASM_INTSRCSEL_PLUSSRC2DOT2)
	{
		puInst[1] |= EURASIA_USE1_FIRV_COEFADD;
	}
	else if (psInst->asArg[3].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only +src2.2 or nothing is valid as a choice for the fourth argument to firv"));
	}

	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to firv"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[4].uNumber == USEASM_INTSRCSEL_IADD)
	{
		puInst[1] |= EURASIA_USE1_IREGSRC;
	}
	else if (psInst->asArg[4].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only iadd or nothing is valid as a choice for the fifth argument to firv"));
	}

	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to firv must be an integer source selector"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the eighth argument to firv"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector isn't indexable"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_SCALE)
	{
		puInst[1] |= EURASIA_USE1_FIRV_SHIFTEN;
	}
	else if (psInst->asArg[5].uNumber != USEASM_INTSRCSEL_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only scale or nothing is valid as a choice for the sixth argument to firv"));
	}

	/* Encode the destination and source arguments. */
	CheckArgFlags(psContext, psInst, 0, USEASM_ARGFLAGS_DISABLEWB);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	puInst[0] |= (psInst->asArg[1].uNumber << EURASIA_USE0_SRC0_SHIFT);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeBILINInstruction

 PURPOSE	: Encode a BILIN instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeBILINInstruction(PUSE_INST		psInst, 
									   IMG_PUINT32		puInst,
									   PUSEASM_CONTEXT	psContext,
									   PCSGX_CORE_DESC	psTarget)
{
	IMG_BOOL bSigned = FALSE;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 0, 0);
	/* Encode opcode and instruction modes. */
	puInst[1] = (EURASIA_USE1_OP_ADIFFIRVBILIN << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_ADIFFIRVBILIN_OPSEL_BILIN << EURASIA_USE1_ADIFFIRVBILIN_OPSEL_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	puInst[0] = 0;
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_INT_RCOUNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on bilin is %d", EURASIA_USE1_INT_RCOUNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_INT_RCOUNT_SHIFT);
	}
	/* Encode source format. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to bilin must specify the filtering operation"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to bilin isn't indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to bilin"));
	}
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U8: puInst[1] |= (EURASIA_USE1_BILIN_SRCFORMAT_U8 << EURASIA_USE1_BILIN_SRCFORMAT_SHIFT); break;
		case USEASM_INTSRCSEL_S8: puInst[1] |= (EURASIA_USE1_BILIN_SRCFORMAT_S8 << EURASIA_USE1_BILIN_SRCFORMAT_SHIFT); bSigned = TRUE; break;
		case USEASM_INTSRCSEL_O8: puInst[1] |= (EURASIA_USE1_BILIN_SRCFORMAT_O8 << EURASIA_USE1_BILIN_SRCFORMAT_SHIFT); bSigned = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to bilin must specify the source data format"));
	}
	/* Encode planar/interleaved select. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to bilin must specify the source data format"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to bilin isn't indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the sixth argument to bilin"));
	}
	switch (psInst->asArg[5].uNumber)
	{
		case USEASM_INTSRCSEL_INTERLEAVED: puInst[1] |= EURASIA_USE1_BILIN_INTERLEAVED; break;
		case USEASM_INTSRCSEL_PLANAR: break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to bilin must specify either interleaved or planar"));
	}
	/* Encode src channel select. */
	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to bilin must specify the source channel select"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to bilin isn't indexable"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the seventh argument to bilin"));
	}
	switch (psInst->asArg[6].uNumber)
	{
		case USEASM_INTSRCSEL_SRC01: puInst[1] |= (EURASIA_USE1_BILIN_SRCCHANSEL_01 << EURASIA_USE1_BILIN_SRCCHANSEL_SHIFT); break;
		case USEASM_INTSRCSEL_SRC23: puInst[1] |= (EURASIA_USE1_BILIN_SRCCHANSEL_23 << EURASIA_USE1_BILIN_SRCCHANSEL_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to bilin must specify either src01 or src23"));
	}
	/* Encode dest channel select. */
	if (psInst->asArg[7].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to bilin must specify the destination channel select"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to bilin isn't indexable"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the eighth argument to bilin"));
	}
	switch (psInst->asArg[7].uNumber)
	{
		case USEASM_INTSRCSEL_DST01: puInst[1] |= (EURASIA_USE1_BILIN_DESTCHANSEL_01 << EURASIA_USE1_BILIN_DESTCHANSEL_SHIFT); break;
		case USEASM_INTSRCSEL_DST23: puInst[1] |= (EURASIA_USE1_BILIN_DESTCHANSEL_23 << EURASIA_USE1_BILIN_DESTCHANSEL_SHIFT); break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to bilin must specify either src01 or src23"));
	}
	/* Encode rounding select. */
	if (psInst->asArg[8].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to bilin must specify the rounding"));
	}
	if (psInst->asArg[8].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to bilin isn't indexable"));
	}
	if (psInst->asArg[8].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the nineth argument to bilin"));
	}
	switch (psInst->asArg[8].uNumber)
	{
		case USEASM_INTSRCSEL_RND: puInst[1] |= EURASIA_USE1_BILIN_RND; break;
		case USEASM_INTSRCSEL_NONE: break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to bilin must specify either rnd or nothing"));
	}

	/* Encode the destination and source arguments. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc0(psContext, psInst, 1, FALSE, puInst + 0, puInst + 1, 0, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeFIRHInstruction

 PURPOSE	: Encode a FIRH instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFIRHInstruction(PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext,
									  PCSGX_CORE_DESC	psTarget)
{
	IMG_BOOL bSigned = FALSE;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, 0, 0);
	/* Encode opcode and instruction modes. */
	puInst[1] = (EURASIA_USE1_OP_FIRH << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0);
	puInst[0] = 0;
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_FIRH_RCOUNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on firh is %d", EURASIA_USE1_FIRH_RCOUNT_MAX));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_FIRH_RCOUNT_SHIFT);
	}

	/* Check restrictions on source0/source1 register types. */
	if (psInst->asArg[1].uType != psInst->asArg[2].uType ||
		psInst->asArg[1].uIndex != psInst->asArg[2].uIndex)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register banks for source 0 and source 1 to firh must be the same"));
	}
	if (!(psInst->asArg[1].uType == USEASM_REGTYPE_TEMP ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_OUTPUT ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_PRIMATTR ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_SECATTR) ||
		psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register bank for firh source 0 must be either temporary, output, "
			"primary attribute or secondary attribute and non-indexed"));
	}

	/* Encode the source format. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firh must specify a source data format"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firh isn't indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to firh"));
	}
	/* Encode source format. */
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U8: puInst[1] |= EURASIA_USE1_FIRH_SRCFORMAT_U8 << EURASIA_USE1_FIRH_SRCFORMAT_SHIFT; break;
		case USEASM_INTSRCSEL_S8: puInst[1] |= EURASIA_USE1_FIRH_SRCFORMAT_S8 << EURASIA_USE1_FIRH_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		case USEASM_INTSRCSEL_O8: puInst[1] |= EURASIA_USE1_FIRH_SRCFORMAT_O8 << EURASIA_USE1_FIRH_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firh must specify a source data format"));
	}

	/* Encode the edgemode select. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_IMMEDIATE ||
		psInst->asArg[5].uNumber >= EURASIA_USE1_FIRH_EDGEMODE_RESERVED)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to firh must be one of rep, msc or mdc"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	puInst[1] |= (psInst->asArg[5].uNumber << EURASIA_USE1_FIRH_EDGEMODE_SHIFT);

	/* Encode the filter coefficent selection. */
	if (psInst->asArg[6].uType != USEASM_REGTYPE_FILTERCOEFF)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to firh must be a filter coefficent set"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Filter coefficent sets aren't indexable"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a filter coefficent select"));
	}
	if (psInst->asArg[6].uNumber > EURASIA_USE1_FIRH_COEFSEL_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum filter coefficent set is %d", EURASIA_USE1_FIRH_COEFSEL_MAX));
	}
	puInst[1] |= (psInst->asArg[6].uNumber << EURASIA_USE1_FIRH_COEFSEL_SHIFT);

	/* Encode the offset to filter centre. */
	if (psInst->asArg[7].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to firh must be an immediate value"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	if (((IMG_INT32)psInst->asArg[7].uNumber < EURASIA_USE1_FIRH_SOFF_MIN) ||
		((IMG_INT32)psInst->asArg[7].uNumber > EURASIA_USE1_FIRH_SOFF_MAX))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The range of the eighth argument to firh is %d to %d", 
				EURASIA_USE1_FIRH_SOFF_MIN, EURASIA_USE1_FIRH_SOFF_MAX));
	}
	puInst[1] |= (((psInst->asArg[7].uNumber >> 2) << EURASIA_USE1_FIRH_SOFFH_SHIFT) & ~EURASIA_USE1_FIRH_SOFFH_CLRMSK) | 
				 (((psInst->asArg[7].uNumber >> 0) << EURASIA_USE1_FIRH_SOFFL_SHIFT) & ~EURASIA_USE1_FIRH_SOFFL_CLRMSK) |
				 (((psInst->asArg[7].uNumber >> 4) << EURASIA_USE1_FIRH_SOFFS_SHIFT) & ~EURASIA_USE1_FIRH_SOFFS_CLRMSK);

	/* Encode the packing register offset. */
	if (psInst->asArg[8].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to firh must be an immediate value"));
	}
	if (psInst->asArg[8].uIndex != USEREG_INDEX_NONE)
	{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[8].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	if (psInst->asArg[8].uNumber > EURASIA_USE1_FIRH_POFF_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The range of the nineth argument to firh is 0 to %d", EURASIA_USE1_FIRH_POFF_MAX));
	}
	puInst[1] |= (psInst->asArg[8].uNumber << EURASIA_USE1_FIRH_POFF_SHIFT);

	/* Encode the destination and source arguments. */
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	/*
		Source 0 always has the same bank as source 1.
	*/
	puInst[0] |= (psInst->asArg[1].uNumber << EURASIA_USE0_SRC0_SHIFT);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeFIRHHInstruction

 PURPOSE	: Encode a FIRHH instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeFIRHHInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_BOOL bSigned = FALSE;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	if (!IsHighPrecisionFIR(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "firhh isn't available on this processor"));
	}

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_END | (~USEASM_OPFLAGS1_PRED_CLRMSK) |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | USEASM_OPFLAGS1_NOSCHED, EURASIA_USE1_FIRHH_MOEPOFF, 0);
	/* Encode opcode and instruction modes. */
	puInst[1] = (EURASIA_USE1_OP_FIRH << EURASIA_USE1_OP_SHIFT) |
				EURASIA_USE1_FIRHH_HIPRECISION |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_SPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_INT_NOSCHED : 0) |
				((psInst->uFlags2 & USEASM_OPFLAGS2_MOEPOFF) ? EURASIA_USE1_FIRHH_MOEPOFF : 0);
	puInst[0] = 0;
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (uRptCount > EURASIA_USE1_FIRHH_RCOUNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Maximum repeat count on firhh is %d", EURASIA_USE1_FIRHH_RCOUNT_MAX));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_FIRHH_RCOUNT_SHIFT);
	}

	/* Check restrictions on source0/source1 register types. */
	if (psInst->asArg[1].uType != psInst->asArg[2].uType ||
		psInst->asArg[1].uIndex != psInst->asArg[2].uIndex)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register banks for source 0 and source 1 to firhh must be the same"));
	}
	if (!(psInst->asArg[1].uType == USEASM_REGTYPE_TEMP ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_OUTPUT ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_PRIMATTR ||
		  psInst->asArg[1].uType == USEASM_REGTYPE_SECATTR) ||
		psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register bank for firhh source 0 must be either temporary, output, "
				"primary attribute or secondary attribute and non-indexed"));
	}

	/* Encode the source format. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firhh must specify a source data format"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firhh isn't indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to firhh"));
	}
	/* Encode source format. */
	switch (psInst->asArg[4].uNumber)
	{
		case USEASM_INTSRCSEL_U8: puInst[1] |= EURASIA_USE1_FIRHH_SRCFORMAT_U8 << EURASIA_USE1_FIRHH_SRCFORMAT_SHIFT; break;
		case USEASM_INTSRCSEL_S8: puInst[1] |= EURASIA_USE1_FIRHH_SRCFORMAT_S8 << EURASIA_USE1_FIRHH_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		case USEASM_INTSRCSEL_O8: puInst[1] |= EURASIA_USE1_FIRHH_SRCFORMAT_O8 << EURASIA_USE1_FIRHH_SRCFORMAT_SHIFT; bSigned = TRUE; break;
		default: USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to firhh must specify a source data format"));
	}

	/* Encode the edgemode select. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_IMMEDIATE ||
		psInst->asArg[5].uNumber >= EURASIA_USE1_FIRHH_EDGEMODE_RESERVED)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to firhh must be one of rep, msc or mdc"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	puInst[1] |= (psInst->asArg[5].uNumber << EURASIA_USE1_FIRHH_EDGEMODE_SHIFT);

	/* Encode the filter coefficent selection. */
	if (psInst->asArg[6].uType != USEASM_REGTYPE_FILTERCOEFF)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to firhh must be a filter coefficent set"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Filter coefficent sets aren't indexable"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a filter coefficent select"));
	}
	if (psInst->asArg[6].uNumber > EURASIA_USE1_FIRHH_COEFSEL_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum filter coefficent set is %d", EURASIA_USE1_FIRHH_COEFSEL_MAX));
	}
	puInst[1] |= (psInst->asArg[6].uNumber << EURASIA_USE1_FIRHH_COEFSEL_SHIFT);

	/* Encode the offset to filter centre. */
	if (psInst->asArg[7].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The eighth argument to firhh must be an immediate value"));
	}
	if (psInst->asArg[7].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[7].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	if (((IMG_INT32)psInst->asArg[7].uNumber < EURASIA_USE1_FIRHH_SOFF_MIN) ||
		((IMG_INT32)psInst->asArg[7].uNumber > EURASIA_USE1_FIRHH_SOFF_MAX))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The range of the eighth argument to firhh is %d to %d", 
				EURASIA_USE1_FIRHH_SOFF_MIN, EURASIA_USE1_FIRHH_SOFF_MAX));
	}
	puInst[1] |= (((psInst->asArg[7].uNumber >> 2) << EURASIA_USE1_FIRHH_SOFFH_SHIFT) & ~EURASIA_USE1_FIRHH_SOFFH_CLRMSK) | 
				 (((psInst->asArg[7].uNumber >> 0) << EURASIA_USE1_FIRHH_SOFFL_SHIFT) & ~EURASIA_USE1_FIRHH_SOFFL_CLRMSK) |
				 (((psInst->asArg[7].uNumber >> 4) << EURASIA_USE1_FIRHH_SOFFS_SHIFT) & ~EURASIA_USE1_FIRHH_SOFFS_CLRMSK);

	/* Encode the packing register offset. */
	if (psInst->asArg[8].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The nineth argument to firhh must be an immediate value"));
	}
	if (psInst->asArg[8].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[8].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	if (psInst->asArg[8].uNumber > EURASIA_USE1_FIRHH_POFF_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The range of the nineth argument to firhh is 0 to %d", EURASIA_USE1_FIRHH_POFF_MAX));
	}
	puInst[1] |= (psInst->asArg[8].uNumber << EURASIA_USE1_FIRHH_POFF_SHIFT);

	/* Encode the destination and source arguments. */
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination argument to firhh must be an internal register"));
	}
	puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
	CheckArgFlags(psContext, psInst, 1, 0);
	puInst[0] |= (psInst->asArg[1].uNumber << EURASIA_USE0_SRC0_SHIFT);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, bSigned, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
}

static IMG_VOID EncodeSMPInstruction_RepeatCount(PCSGX_CORE_DESC		psTarget,
												 PUSE_INST			psInst, 
												 IMG_PUINT32 		puInst,
												 PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: EncodeSMPInstruction_RepeatCount

 PURPOSE	: Encode the repeat mask/repeat count field in an SMP instruction.

 PARAMETERS	: psTarget			- Target for assembly.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	PVR_UNREFERENCED_PARAMETER(psContext);
	PVR_UNREFERENCED_PARAMETER(psTarget);

	/* Check for core bugs. */
	if (FixBRN26570(psTarget) || FixBRN26681(psTarget))
	{
		if (uRptCount > 0 || uMask != 1)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat count isn't supported on %s on this core",
				OpcodeName(psInst->uOpcode)));
		}
	}

	/* Encode repeat count and mask */
	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_REDUCEDREPEATCOUNT)
	if (ReducedSMPRepeatCount(psTarget))
	{
		if (uRptCount > 0)
		{	
			if (uRptCount > EURASIA_USE1_SMP_RMSKCNT_MAXCOUNT)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "The maximum repeat count for %s on this core is %d",
							   OpcodeName(psInst->uOpcode),
							   EURASIA_USE1_SMP_RMSKCNT_MAXCOUNT));
			}
			puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_SMP_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_SMP_NO_REPEAT_MASK)
		else if (NoRepeatMaskOnSMPInstructions(psTarget))
		{
			if (uMask != USEREG_MASK_X)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat mask isn't supported on this instruction"));
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_NO_REPEAT_MASK) */
		else
		{
			if (uMask & ~EURASIA_USE1_SMP_RMSKCNT_MAXMASK)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "Valid repeat masks for %s on this core are .x, .y and .xy",
							   OpcodeName(psInst->uOpcode)));
			}
			puInst[1] |= (uMask << EURASIA_USE1_SMP_RMSKCNT_SHIFT);
		}
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_REDUCEDREPEATCOUNT) */
	{
		if (uRptCount > 0)
		{	
			puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
		}
		else
		{
			puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
		}
	}
}

#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE)
typedef enum
{
	RAWSAMPLEMODE_NONE,
	RAWSAMPLEMODE_DATA,
	RAWSAMPLEMODE_INFO,
	RAWSAMPLEMODE_BOTH,
} RAWSAMPLEMODE;

static IMG_VOID EncodeSMPInstruction_RawSampleMode(PCSGX_CORE_DESC	psTarget,
												   PUSE_INST		psInst, 
												   IMG_PUINT32 		puInst,
												   PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: EncodeSMPInstruction_RawSampleMode

 PURPOSE	: Encode the raw sample/sample info mode field in an SMP instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (SupportsRawSample(psTarget))
	{
		RAWSAMPLEMODE	eMode;

		if (psInst->uFlags3 & USEASM_OPFLAGS3_SAMPLEDATAANDINFO)
		{
			if (psInst->uFlags3 & (USEASM_OPFLAGS3_SAMPLEINFO | USEASM_OPFLAGS3_SAMPLEDATA))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .irsd, .rsd and .sinf can be specified on an instruction"));
			}
			#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH)
			if (SupportsRawSampleBoth(psTarget))
			{
				eMode = RAWSAMPLEMODE_BOTH;
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) */
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, ".irsd isn't supported on this core"));
				eMode = RAWSAMPLEMODE_NONE;
			}
		}
		else if (psInst->uFlags3 & USEASM_OPFLAGS3_SAMPLEDATA)
		{
			if (psInst->uFlags3 & USEASM_OPFLAGS3_SAMPLEINFO)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .rsd and .sinf can be specified on an instruction"));
			}
			eMode = RAWSAMPLEMODE_DATA;
		}
		else if (psInst->uFlags3 & USEASM_OPFLAGS3_SAMPLEINFO)
		{
			eMode = RAWSAMPLEMODE_INFO;
		}
		else
		{
			eMode = RAWSAMPLEMODE_NONE;
		}

		#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH)
		if (SupportsRawSampleBoth(psTarget))
		{
			IMG_UINT32	uSbMode;

			switch (eMode)
			{
				case RAWSAMPLEMODE_NONE: uSbMode = SGXVEC_USE1_SMP_SBMODE_NONE; break;
				case RAWSAMPLEMODE_DATA: uSbMode = SGXVEC_USE1_SMP_SBMODE_RAWDATA; break;
				case RAWSAMPLEMODE_INFO: uSbMode = SGXVEC_USE1_SMP_SBMODE_COEFFS; break;
				case RAWSAMPLEMODE_BOTH: uSbMode = SGXVEC_USE1_SMP_SBMODE_BOTH; break;
				default: IMG_ABORT();
			}
			puInst[1] |= (uSbMode << SGXVEC_USE1_SMP_SBMODE_SHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) */
		#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) && defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH)
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) && defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH) */
		#if defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH)
		{
			IMG_UINT32	uSrd;

			switch (eMode)
			{
				case RAWSAMPLEMODE_NONE: uSrd = SGX545_USE1_SMP_SRD_NONE; break;
				case RAWSAMPLEMODE_DATA: uSrd = SGX545_USE1_SMP_SRD_RAWDATA; break;
				case RAWSAMPLEMODE_INFO: uSrd = SGX545_USE1_SMP_SRD_COEFFS; break;
				default: IMG_ABORT();
			}
			puInst[1] |= (uSrd << SGX545_USE1_SMP_SRD_SHIFT);
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_NOT_RAWSAMPLEBOTH) */
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE) */

/*****************************************************************************
 FUNCTION	: EncodeSMPInstruction

 PURPOSE	: Encode a SMP instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSMPInstruction(PCSGX_CORE_DESC		psTarget,
									 PUSE_INST			psInst, 
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32		uCDim, uLodMode, uDrcArg, uValidFlags1, uValidFlags3;
	PUSE_REGISTER	psFormatConvertArg;

	/*
		Check for BRN25355: no predicated texture samples.
	*/
	if (FixBRN25355(psTarget))
	{
		IMG_UINT32 uPredicate = (psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT;

		if (uPredicate != USEASM_PRED_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "A %s instruction can't be predicated on this core",
						   OpcodeName(psInst->uOpcode)));
		}
	}
	
	/* Check instruction validity. */
	uValidFlags1 = (USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
					(~USEASM_OPFLAGS1_MASK_CLRMSK) | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
					(~USEASM_OPFLAGS1_PRED_CLRMSK));
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_TAG_PCF)
	if (SupportsPCF(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_PCFF32 | USEASM_OPFLAGS3_PCFF16;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_PCF) */
	#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE)
	if (SupportsRawSample(psTarget))
	{
		#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH)
		if (SupportsRawSampleBoth(psTarget))
		{
			uValidFlags3 |= USEASM_OPFLAGS3_SAMPLEDATAANDINFO;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLEBOTH) */
		uValidFlags3 |= USEASM_OPFLAGS3_SAMPLEDATA | USEASM_OPFLAGS3_SAMPLEINFO;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE) */
	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
	if (SupportsSMPResultFormatConvert(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_MINPACK;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
	CheckFlags(psContext, psInst, uValidFlags1, 0, uValidFlags3);

	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SMP << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_SMP_NOSCHED : 0);
	
	/*
		Encode the repeat count/repeat mask.
	*/
	EncodeSMPInstruction_RepeatCount(psTarget, psInst, puInst, psContext);

	#if defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE)
	/*
		Encode the raw sample/sample info return mode.
	*/
	EncodeSMPInstruction_RawSampleMode(psTarget, psInst, puInst, psContext);
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_RAWSAMPLE) */

	#if defined(SUPPORT_SGX_FEATURE_TAG_PCF)
	/*
		Encode the PCF mode.
	*/
	if (SupportsPCF(psTarget))
	{
		IMG_UINT32	uPCFMode;

		if (psInst->uFlags3 & USEASM_OPFLAGS3_PCFF32)
		{
			if (psInst->uFlags3 & USEASM_OPFLAGS3_PCFF16)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Only one of .pcf and .pcff16 can be specified on an instruction"));
			}
			uPCFMode = SGX545_USE1_SMP_PCF_F32;
		}
		else if (psInst->uFlags3 & USEASM_OPFLAGS3_PCFF16)
		{
			uPCFMode = SGX545_USE1_SMP_PCF_F16;
		}
		else
		{
			uPCFMode = SGX545_USE1_SMP_PCF_NONE;
		}

		puInst[1] |= (uPCFMode << SGX545_USE1_SMP_PCF_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_TAG_PCF) */

	#if defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
	/*
		Encode the MINP flag.
	*/
	if (SupportsSMPResultFormatConvert(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_MINPACK))
	{
		puInst[1] |= SGXVEC_USE1_SMP_MINPACK;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */

	/* Encode the coordinate dimension. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_SMP1D: case USEASM_OP_SMP1DBIAS: case USEASM_OP_SMP1DREPLACE: case USEASM_OP_SMP1DGRAD: uCDim = EURASIA_USE1_SMP_CDIM_U; break;
		case USEASM_OP_SMP2D: case USEASM_OP_SMP2DBIAS: case USEASM_OP_SMP2DREPLACE: case USEASM_OP_SMP2DGRAD: uCDim = EURASIA_USE1_SMP_CDIM_UV; break;
		case USEASM_OP_SMP3D: case USEASM_OP_SMP3DBIAS: case USEASM_OP_SMP3DREPLACE: case USEASM_OP_SMP3DGRAD: uCDim = EURASIA_USE1_SMP_CDIM_UVS; break;
		default: IMG_ABORT();
	}
	puInst[1] |= uCDim << EURASIA_USE1_SMP_CDIM_SHIFT;
	/* Encode the lod mode. */
	switch (psInst->uOpcode)
	{
		case USEASM_OP_SMP1D: case USEASM_OP_SMP2D: case USEASM_OP_SMP3D: uLodMode = EURASIA_USE1_SMP_LODM_NONE; break;
		case USEASM_OP_SMP1DBIAS: case USEASM_OP_SMP2DBIAS: case USEASM_OP_SMP3DBIAS: uLodMode = EURASIA_USE1_SMP_LODM_BIAS; break;
		case USEASM_OP_SMP1DREPLACE: case USEASM_OP_SMP2DREPLACE: case USEASM_OP_SMP3DREPLACE: uLodMode = EURASIA_USE1_SMP_LODM_REPLACE; break;
		case USEASM_OP_SMP1DGRAD: case USEASM_OP_SMP2DGRAD: case USEASM_OP_SMP3DGRAD: uLodMode = EURASIA_USE1_SMP_LODM_GRADIENTS; break;
		default: IMG_ABORT();
	}
	puInst[1] |= uLodMode << EURASIA_USE1_SMP_LODM_SHIFT;
	
	/*
		Encode the bank and offset of the coordinate source.
	*/
	CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_FMTF16 | USEASM_ARGFLAGS_FMTC10 | USEASM_ARGFLAGS_FMTF32);
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S0BEXT,
								      puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_SMP_SRC_FIELD_LENGTH,
									  EURASIA_USE0_SRC0_SHIFT,
									  USEASM_HWARG_SOURCE0,
									  1 /* uArg */,
									  psTarget);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		EncodeSrc0(psContext, 
				   psInst, 
				   1 /* uArg */, 
				   IMG_TRUE /* bAllowExtended */, 
				   puInst + 0, 
				   puInst + 1, 
				   EURASIA_USE1_S0BEXT, 
				   IMG_FALSE /* bFmtSelect */, 
				   0 /* uFmtFlag */, 
				   psTarget);
	}

	/*
		Encode the bank and offset of the state source.
	*/
	CheckArgFlags(psContext, psInst, 2, 0);
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (SupportsVEC34(psTarget))
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S1BEXT,
								      puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_SMP_SRC_FIELD_LENGTH,
									  EURASIA_USE0_SRC1_SHIFT,
									  USEASM_HWARG_SOURCE1,
									  2 /* uArg */,
									  psTarget);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		EncodeSrc1(psContext, 
				   psInst, 
				   2 /* uArg */, 
				   IMG_TRUE /* bAllowExtended */, 
				   EURASIA_USE1_S1BEXT, 
				   IMG_FALSE /* bSigned */, 
				   puInst + 0, 
				   puInst + 1, 
				   IMG_FALSE /* bBitwise */, 
				   IMG_FALSE /* bFmtSelect */, 
				   0 /* uFmtFlag */, 
				   psTarget);
	}

	/*
		Encode the LOD-BIAS/LOD-REPLACE/GRADIENTS source.
	*/
	if (uLodMode == EURASIA_USE1_SMP_LODM_NONE)
	{
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
		uDrcArg = 3;
	}
	else
	{
		CheckArgFlags(psContext, psInst, 3, 0);
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (SupportsVEC34(psTarget))
		{
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_S2BEXT,
										  puInst + 0,
										  puInst + 1,
										  SGXVEC_USE_SMP_SRC_FIELD_LENGTH,
										  EURASIA_USE0_SRC2_SHIFT,
										  USEASM_HWARG_SOURCE2,
										  3 /* uArg */,
										  psTarget);
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			EncodeSrc2(psContext, 
					   psInst, 
					   3 /* uArg */, 
					   IMG_TRUE /* bAllowExtended */, 
					   EURASIA_USE1_S2BEXT, 
					   IMG_FALSE /* bSigned */, 
					   puInst + 0, 
					   puInst + 1, 
					   IMG_FALSE /* bBitwise */, 
					   IMG_FALSE /* bFmtSelect */, 
					   0 /* uFmtFlag */, 
					   psTarget);
		}
		uDrcArg = 4;
	}


	/*
		Encode the destination bank and offset.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for an SMP can't be indexed"));
	}
	if (psInst->asArg[0].uType != USEASM_REGTYPE_PRIMATTR &&
		psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for a SMP must be the primary attribute bank or the temporary bank"));
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		puInst[1] |= EURASIA_USE1_SMP_DBANK_TEMP;
		if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid temporary register specified (r%d)", psInst->asArg[0].uNumber));
		}
	}
	else
	{
		puInst[1] |= EURASIA_USE1_SMP_DBANK_PRIMATTR;
		if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid primary attribute register specified (pa%d)", psInst->asArg[0].uNumber));
		}
	}
	puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);

	/* Encode the coordinate information type. */
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTF16)
	{
		puInst[1] |= (EURASIA_USE1_SMP_CTYPE_F16 << EURASIA_USE1_SMP_CTYPE_SHIFT);
	}
	else if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTC10)
	{
		puInst[1] |= (EURASIA_USE1_SMP_CTYPE_C10 << EURASIA_USE1_SMP_CTYPE_SHIFT);
		#if defined(SUPPORT_SGX_FEATURE_TAG_PCF)
		if (SupportsPCF(psTarget) && (psInst->uFlags3 & (USEASM_OPFLAGS3_PCFF32 | USEASM_OPFLAGS3_PCFF16)))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "C10 format texture coordinates aren't supported with PCF"));
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_TAG_PCF) */
	}
	else
	{
		puInst[1] |= (EURASIA_USE1_SMP_CTYPE_F32 << EURASIA_USE1_SMP_CTYPE_SHIFT);
	}
	/* Encode the dependent read counter argument. */
	if (psInst->asArg[uDrcArg].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to a sampler instruction must be in the dependent read counter bank"));
	}
	if (psInst->asArg[uDrcArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A dependent read counter register can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uDrcArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a dependent read counter register"));
	}
	if (psInst->asArg[uDrcArg].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum number of a drc register is %u", EURASIA_USE_DRC_BANK_SIZE - 1));
	}
	puInst[1] |= psInst->asArg[uDrcArg].uNumber << EURASIA_USE1_SMP_DRCSEL_SHIFT;

	/*
		Encode the format conversion argument.
	*/
	psFormatConvertArg = &psInst->asArg[uDrcArg + 1];
	if (
			psFormatConvertArg->uType != USEASM_REGTYPE_INTSRCSEL ||
			psFormatConvertArg->uIndex != USEREG_INDEX_NONE ||
			psFormatConvertArg->uFlags != 0 ||
			(
				psFormatConvertArg->uNumber != USEASM_INTSRCSEL_NONE &&
				psFormatConvertArg->uNumber != USEASM_INTSRCSEL_F16 &&
				psFormatConvertArg->uNumber != USEASM_INTSRCSEL_F32
			)
		)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for argument %d to %s are none, f16 and f32", uDrcArg + 1, OpcodeName(psInst->uOpcode)));
	}
	else
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
		if (SupportsSMPResultFormatConvert(psTarget))
		{
			IMG_UINT32	uFormatConvert;
			switch (psFormatConvertArg->uNumber)
			{
				case USEASM_INTSRCSEL_NONE: uFormatConvert = SGXVEC_USE1_SMP_FCONV_NONE; break;
				case USEASM_INTSRCSEL_F16: uFormatConvert = SGXVEC_USE1_SMP_FCONV_F16; break;
				case USEASM_INTSRCSEL_F32: uFormatConvert = SGXVEC_USE1_SMP_FCONV_F32; break;
				default: IMG_ABORT();
			}
			puInst[1] |= (uFormatConvert << SGXVEC_USE1_SMP_FCONV_SHIFT);
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
		{
			if (psFormatConvertArg->uNumber != USEASM_INTSRCSEL_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "On this core SMP result format conversion isn't supported"));
			}
		}
	}
}

static IMG_VOID EncodeLDSTCacheFlags(PCSGX_CORE_DESC		psTarget,
									 PUSE_INST			psInst,
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext)
/*****************************************************************************
 FUNCTION	: EncodeLDSTCacheFlags

 PURPOSE	: Encode the cache bypass/forcefill flags for LD/ST instructions.

 PARAMETERS	: psTarget
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/* Check cache flags validity. */
	if ((psInst->uFlags2 & USEASM_OPFLAGS2_BYPASSCACHE) && (psInst->uFlags2 & USEASM_OPFLAGS2_FORCELINEFILL))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".bpcache and .fcfill can't be specified together"));
	}
	else if ((psInst->uFlags2 & USEASM_OPFLAGS2_BYPASSCACHE) && (psInst->uFlags3 & USEASM_OPFLAGS3_BYPASSL1))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".bpcache and .bypassl1 can't be specified together"));
	}
	else if ((psInst->uFlags2 & USEASM_OPFLAGS2_FORCELINEFILL) && (psInst->uFlags3 & USEASM_OPFLAGS3_BYPASSL1))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, ".fcfill and .bypassl1 can't be specified together"));
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
	if (SupportsLDSTExtendedCacheModes(psTarget))
	{
		IMG_UINT32	uDCCTLEXT;

		#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
		if (psInst->uOpcode == USEASM_OP_ELDD ||
			psInst->uOpcode == USEASM_OP_ELDQ)
		{
			uDCCTLEXT = SGX545_USE1_EXTLD_DCCTLEXT;
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */
		{
			uDCCTLEXT = EURASIA_USE1_LDST_DCCTLEXT;
		}

		if (psInst->uFlags2 & USEASM_OPFLAGS2_BYPASSCACHE)
		{
			puInst[1] |= EURASIA_USE1_LDST_DCCTL_STDBYPASSL1L2;
		}
		else if (psInst->uFlags3 & USEASM_OPFLAGS3_BYPASSL1)
		{
			puInst[1] |= EURASIA_USE1_LDST_DCCTL_EXTBYPASSL1 | uDCCTLEXT;
		}
		else if (psInst->uFlags2 & USEASM_OPFLAGS2_FORCELINEFILL)
		{
			puInst[1] |= EURASIA_USE1_LDST_DCCTL_EXTFCFILL | uDCCTLEXT;
		}
	}
	else
	#else /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
	PVR_UNREFERENCED_PARAMETER(psTarget);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
	{
		if (psInst->uFlags2 & USEASM_OPFLAGS2_FORCELINEFILL)
		{
			puInst[1] |= EURASIA_USE1_LDST_FCLFILL;
		}
		if (psInst->uFlags2 & USEASM_OPFLAGS2_BYPASSCACHE)
		{
			puInst[1] |= EURASIA_USE1_LDST_BPCACHE;
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeLDSTInstruction

 PURPOSE	: Encode a LD or ST instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLDSTInstruction(PCSGX_CORE_DESC	psTarget,
									  PUSE_INST			psInst,
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	/* LD DEST, BASE, OFFSET, [LIMIT], DRCSEL/ST BASE, OFFSET, SOURCE */
	IMG_UINT32 uAddressMode;
	IMG_UINT32 uDataType;
	IMG_UINT32 uOpcode;
	IMG_UINT32 uValidFlags;
	IMG_UINT32 uValidFlags2;
	IMG_UINT32 uValidFlags3;

	#if defined(FIX_HW_BRN_27259)
	/*
		***************************************** BRN27359 **********************************

		Description (from the bugtracker):
		A LD or ST instruction with pre/post increment and src0 coming from a primary attribute 
		register performs a range check to make sure the src0 is within range so it can be pre/post 
		incremented. This check is incorrectly performed against a quarter of the stride and may 
		disable the instruction.

		Unfortunately we can't check for instructions which are affected by this bug because
		the number of primary attribute registers allocated isn't supplied as an input to
		the assembler.

		*************************************************************************************
	*/
	#endif /* defined(FIX_HW_BRN_27259) */

	#if defined(FIX_HW_BRN_28009)
	/*
		***************************************** BRN28009 **********************************

		Description (from the bugtracker):
		The ST instruction updates src0 (the destination for the store) with a new address after 
		each iteration when MOEEXP = 1. This process only works correctly for the first 3 iterations, 
		which means the destination will be stuck at the value used for the 4th iteration. 

		Workaround (from the bugtracker):
		Use ST.fetch instead of ST.repeat, or set MOE DST increment to 0

		Unfortunately we can't check for instructions affected by this bug because we don't know
		what the state of the MOE increments will be when the instruction is executed.
		*************************************************************************************
	*/
	#endif /* defined(FIX_HW_BRN_28009) */

	switch (psInst->uOpcode)
	{
		case USEASM_OP_LDAB:
		case USEASM_OP_LDAW:
		case USEASM_OP_LDAD:
		case USEASM_OP_LDAQ:
		case USEASM_OP_LDLB: 
		case USEASM_OP_LDLW: 
		case USEASM_OP_LDLD: 
		case USEASM_OP_LDLQ:
		case USEASM_OP_LDTB: 
		case USEASM_OP_LDTW: 
		case USEASM_OP_LDTD: 
		case USEASM_OP_LDTQ:
		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		case USEASM_OP_LDATOMIC:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
		{
			uOpcode = EURASIA_USE1_OP_LD;
			break;
		}
		case USEASM_OP_STAB:
		case USEASM_OP_STAW:
		case USEASM_OP_STAD:
		case USEASM_OP_STAQ:
		case USEASM_OP_STLB: 
		case USEASM_OP_STLW: 
		case USEASM_OP_STLD:
		case USEASM_OP_STLQ:
		case USEASM_OP_STTB: 
		case USEASM_OP_STTW: 
		case USEASM_OP_STTD:
		case USEASM_OP_STTQ:
		{
			uOpcode = EURASIA_USE1_OP_ST;
			break;
		}
		default: IMG_ABORT();
	}
	switch (psInst->uOpcode)
	{
		case USEASM_OP_LDAB: 
		case USEASM_OP_LDAW: 
		case USEASM_OP_LDAD:
		case USEASM_OP_LDAQ:
		case USEASM_OP_STAB:
		case USEASM_OP_STAW:
		case USEASM_OP_STAD: 
		case USEASM_OP_STAQ:
		{
			uAddressMode = EURASIA_USE1_LDST_AMODE_ABSOLUTE;
			break;
		}
		case USEASM_OP_LDLB: 
		case USEASM_OP_LDLW: 
		case USEASM_OP_LDLD:
		case USEASM_OP_LDLQ:
		case USEASM_OP_STLB:
		case USEASM_OP_STLW:
		case USEASM_OP_STLD:
		case USEASM_OP_STLQ:
		{
			uAddressMode = EURASIA_USE1_LDST_AMODE_LOCAL;
			break;
		}
		case USEASM_OP_LDTB: 
		case USEASM_OP_LDTW: 
		case USEASM_OP_LDTD:
		case USEASM_OP_LDTQ:
		case USEASM_OP_STTB:
		case USEASM_OP_STTW:
		case USEASM_OP_STTD:
		case USEASM_OP_STTQ:
		{
			uAddressMode = EURASIA_USE1_LDST_AMODE_TILED;
			break;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		case USEASM_OP_LDATOMIC:
		{
			uAddressMode = SGXVEC_USE1_LD_AMODE_ATOMIC;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
		default: IMG_ABORT();
	}
	switch (psInst->uOpcode)
	{
		case USEASM_OP_LDAB:
		case USEASM_OP_STAB:
		case USEASM_OP_LDLB: 
		case USEASM_OP_STLB:
		case USEASM_OP_LDTB: 
		case USEASM_OP_STTB:
		{
			uDataType = EURASIA_USE1_LDST_DTYPE_8BIT;
			break;
		}
		case USEASM_OP_LDAW: 
		case USEASM_OP_STAW:
		case USEASM_OP_LDLW: 
		case USEASM_OP_STLW:
		case USEASM_OP_LDTW:
		case USEASM_OP_STTW:
		{
			uDataType = EURASIA_USE1_LDST_DTYPE_16BIT;
			break;
		}
		case USEASM_OP_LDAD:
		case USEASM_OP_STAD:
		case USEASM_OP_LDLD:
		case USEASM_OP_LDTD:
		case USEASM_OP_STTD: 
		case USEASM_OP_STLD: 
		{
			uDataType = EURASIA_USE1_LDST_DTYPE_32BIT;
			break;
		}
		case USEASM_OP_LDAQ:
		case USEASM_OP_STAQ:
		case USEASM_OP_LDLQ:
		case USEASM_OP_LDTQ:
		case USEASM_OP_STTQ: 
		case USEASM_OP_STLQ: 
		{
			#if defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD)
			if (SupportsQwordLDST(psTarget))
			{
				uDataType = SGXVEC_USE1_LDST_DTYPE_QWORD;
			}
			else
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_QWORD) */
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "%s isn't supported on this core", 
					OpcodeName(psInst->uOpcode)));
				uDataType = USE_UNDEF;
			}
			break;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		case USEASM_OP_LDATOMIC:
		{
			/* The data type isn't used by this variant of the instruction. */
			uDataType = USE_UNDEF;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
		default: IMG_ABORT();
	}
		
	/* Check instruction validity. */
	uValidFlags = USEASM_OPFLAGS1_SKIPINVALID | 
				  USEASM_OPFLAGS1_SYNCSTART | 
				  USEASM_OPFLAGS1_SKIPINVALID | 
				  (~USEASM_OPFLAGS1_PRED_CLRMSK) |  
				  USEASM_OPFLAGS1_PREINCREMENT |
				  USEASM_OPFLAGS1_POSTINCREMENT |
				  USEASM_OPFLAGS1_FETCHENABLE |
				  (~USEASM_OPFLAGS1_REPEAT_CLRMSK);
	uValidFlags2 = USEASM_OPFLAGS2_BYPASSCACHE | 
				   USEASM_OPFLAGS2_FORCELINEFILL |
				   USEASM_OPFLAGS2_INCSGN;
	if (IsEnhancedNoSched(psTarget))
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
		if (!(SupportsExtendedLoad(psTarget) && uOpcode == EURASIA_USE1_OP_LD))
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */
		{
			uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
		}
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (psInst->uOpcode != USEASM_OP_LDATOMIC)
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
	{
		uValidFlags |= USEASM_OPFLAGS1_RANGEENABLE;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (psInst->uOpcode == USEASM_OP_LDATOMIC && uRptCount > 1)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A repeat count is invalid for an ldatomic instructon"));
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */

	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
	if (SupportsLDSTExtendedCacheModes(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_BYPASSL1;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
	#if defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE)
	if (uOpcode == EURASIA_USE1_OP_ST && SupportsSTNoReadBeforeWrite(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_NOREAD;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE) */
	CheckFlags(psContext, psInst, uValidFlags, uValidFlags2, uValidFlags3);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = 0;
	puInst[1] = (uOpcode << EURASIA_USE1_OP_SHIFT) |
			    (EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_RANGEENABLE) ? EURASIA_USE1_LDST_RANGEENABLE : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_LDST_NOSCHED : 0) |
				(uAddressMode << EURASIA_USE1_LDST_AMODE_SHIFT);

	/*
		Set the LD/ST data type.
	*/
	#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
	if (!(psInst->uOpcode == USEASM_OP_LDATOMIC))
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
	{
		 puInst[1] |= (uDataType << EURASIA_USE1_LDST_DTYPE_SHIFT);
	}

	/*
		Check for BRN25355: no predicated LDs.
	*/
#ifdef NOT_YET
	if (FixBRN25355(psTarget) && uOpcode == EURASIA_USE1_OP_LD)
	{
		IMG_UINT32 uPredicate = (psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT;

		if (uPredicate != USEASM_PRED_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "A %s instruction can't be predicated on this core",
						   OpcodeName(psInst->uOpcode)));
		}
	}
#endif /* NOT_YET */

	/* Encode cache bypass flags. */
	EncodeLDSTCacheFlags(psTarget, psInst, puInst, psContext);
	/* Encode repeat count. */
	if (
			#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
			psInst->uOpcode != USEASM_OP_LDATOMIC && 
			#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
			uRptCount > 0
	   )
	{
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT);
	}
	/* Check increment mode and source types. */
	if (uAddressMode == EURASIA_USE1_LDST_AMODE_TILED)
	{
		if (((uOpcode == EURASIA_USE1_OP_ST) && psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE) ||
			((uOpcode == EURASIA_USE1_OP_LD) && psInst->asArg[2].uType == USEASM_REGTYPE_IMMEDIATE))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Source 1 can't be immediate for a tiled address mode load or store"));
		}
		if ((psInst->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT) || (psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Pre/post-increment can't be specified for a tiled address mode load or store"));
		}
	}
	/*
		Check for BRN24895:
			Local address mode
			Not pre-/post-increment
			Immediate offset
	*/
	if (
			FixBRN24895(psTarget) && 
			uAddressMode == EURASIA_USE1_LDST_AMODE_LOCAL &&
			!(psInst->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT) &&
			!(psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT) &&
			(
				(uOpcode == EURASIA_USE1_OP_ST && psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE) ||
				(uOpcode == EURASIA_USE1_OP_LD && psInst->asArg[2].uType == USEASM_REGTYPE_IMMEDIATE)
			)
		)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "%s can't be used with non-pre/post increment and an immediate offset on this core",
					   OpcodeName(psInst->uOpcode)));
	}
	/* Encode increment mode. */
	if ((psInst->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT) &&
		(psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A load/store can't be both pre- and post-increment"));
	}
	if (psInst->uFlags1 & USEASM_OPFLAGS1_PREINCREMENT)
	{
		puInst[1] |= EURASIA_USE1_LDST_IMODE_PRE << EURASIA_USE1_LDST_IMODE_SHIFT;
	}
	else if (psInst->uFlags1 & USEASM_OPFLAGS1_POSTINCREMENT)
	{
		puInst[1] |= EURASIA_USE1_LDST_IMODE_POST << EURASIA_USE1_LDST_IMODE_SHIFT;
	}
	/* Encode the moeexpand flag. */
	if ((psInst->uFlags1 & USEASM_OPFLAGS1_FETCHENABLE) == 0)
	{
		puInst[1] |= EURASIA_USE1_LDST_MOEEXPAND;
	}
	/* Encode the increment sign flag. */
	if (psInst->uFlags2 & USEASM_OPFLAGS2_INCSGN)
	{
		puInst[1] |= EURASIA_USE1_LDST_INCSGN;
	}
	/* Encode instruction sources. */
	if (uOpcode == EURASIA_USE1_OP_ST)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE)
		/*
			Encode the no read before write flag.
		*/
		if (SupportsSTNoReadBeforeWrite(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_NOREAD))
		{
			puInst[1] |= SGX545_USE1_ST_NORDBEF;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_ST_NO_READ_BEFORE_WRITE) */
		CheckArgFlags(psContext, psInst, 0, 0);
		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 2, 0);
		EncodeSrc0(psContext, psInst, 0, TRUE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, IMG_FALSE, 0, psTarget);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		CheckArgFlags(psContext, psInst, 0, 0);
		if (psInst->asArg[0].uType != USEASM_REGTYPE_PRIMATTR &&
			psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for an LD must be the primary attribute bank or the temporary bank"));
		}
		if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for an LD can't be indexed"));
		}
		if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
		{
			puInst[1] |= EURASIA_USE1_LDST_DBANK_TEMP;
			if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid temporary register specified (r%d)", psInst->asArg[0].uNumber));
			}
		}
		else
		{
			puInst[1] |= EURASIA_USE1_LDST_DBANK_PRIMATTR;
			if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid primary attribute register specified (pa%d)", psInst->asArg[0].uNumber));
			}
		}
		puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 2, 0);
		EncodeSrc0(psContext, psInst, 1, TRUE, puInst + 0, puInst + 1, EURASIA_USE1_S0BEXT, IMG_FALSE, 0, psTarget);
		EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		if (
				#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
				psInst->uOpcode == USEASM_OP_LDATOMIC ||
				#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
				(psInst->uFlags1 & USEASM_OPFLAGS1_RANGEENABLE) != 0
		   )
		{
			EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);			
		}
		else
		{
			EncodeUnusedSource(2, puInst + 0, puInst + 1);
		}
		/* Encode the dependent read counter argument. */
		if (psInst->asArg[4].uType != USEASM_REGTYPE_DRC)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The last argument to a load instruction must be in the dependent read counter bank"));
		}
		if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A dependent read counter register can't be accessed in indexed mode"));
		}
		if (psInst->asArg[4].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a dependent read counter register"));
		}
		if (psInst->asArg[4].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum number of a drc register is %u", EURASIA_USE_DRC_BANK_SIZE - 1));
		}
		puInst[1] |= psInst->asArg[4].uNumber << EURASIA_USE1_LDST_DRCSEL_SHIFT;

		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		if (psInst->uOpcode == USEASM_OP_LDATOMIC)
		{
			PUSE_REGISTER	psAtomicOpArg = &psInst->asArg[5];

			if (psAtomicOpArg->uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to an ldatomic instruction must be an atomic operation"));
			}
			if (psAtomicOpArg->uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An atomic operation can't be accessed in indexed mode"));
			}
			if (psAtomicOpArg->uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an atomic operation"));
			}
			switch (psAtomicOpArg->uNumber)
			{
				case USEASM_INTSRCSEL_ADD: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_ADD << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_SUB: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_SUB << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_XCHG: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_XCHG << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_UMIN: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_MIN << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_UMAX: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_MAX << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_IMIN: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_MIN << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					puInst[1] |= SGXVEC_USE1_LD_ATOMIC_DATASGN;
					break;
				}
				case USEASM_INTSRCSEL_IMAX: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_MAX << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					puInst[1] |= SGXVEC_USE1_LD_ATOMIC_DATASGN;
					break;
				}
				case USEASM_INTSRCSEL_OR: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_OR << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_AND: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_AND << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				case USEASM_INTSRCSEL_XOR: 
				{
					puInst[1] |= (SGXVEC_USE1_LD_ATOMIC_OP_XOR << SGXVEC_USE1_LD_ATOMIC_OP_SHIFT);
					break;
				}
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid choice for the atomic operation argument to ldatomic. The valid choices are add, sub, xchg, umin, umax, imin, imax, and, or and xor."));
					break;
				}
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */
	}
}

#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
/*****************************************************************************
 FUNCTION	: EncodeELDInstruction

 PURPOSE	: Encode an extended LD instruction.

 PARAMETERS	: psTarget
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeELDInstruction(PCSGX_CORE_DESC		psTarget,
									 PUSE_INST			psInst,
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uValidFlags3;

	if (!SupportsExtendedLoad(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "This processor doesn't support the extended load instruction"));
	}
	/* Encode opcode, predicate and instruction flags */
	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES)
	if (SupportsLDSTExtendedCacheModes(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_BYPASSL1;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDST_EXTENDED_CACHE_MODES) */
	CheckFlags(psContext, 
			   psInst, 
			   USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART | USEASM_OPFLAGS1_FETCHENABLE | (~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), 
			   USEASM_OPFLAGS2_BYPASSCACHE | USEASM_OPFLAGS2_FORCELINEFILL, 
			   uValidFlags3);
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_LD << EURASIA_USE1_OP_SHIFT) |
				SGX545_USE1_LDST_EXTENDED |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0);
	/* Encode cache bypass flags. */
	EncodeLDSTCacheFlags(psTarget, psInst, puInst, psContext);
	/* Encode repeat count. */
	if (uRptCount > 0)
	{
		if (!(psInst->uFlags1 & USEASM_OPFLAGS1_FETCHENABLE))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "eldd/eldq only supports fetch mode"));
		}
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT);
	}
	/* Encode the data size. */
	if (psInst->uOpcode == USEASM_OP_ELDQ)
	{
		puInst[1] |= SGX545_USE1_EXTLD_DTYPE_128BIT;
	}
	else
	{
		puInst[1] |= SGX545_USE1_EXTLD_DTYPE_32BIT;
	}
	/* Encode destination register. */
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_PRIMATTR &&
		psInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for an LD must be the primary attribute bank or the temporary bank"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for an LD can't be indexed"));
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		puInst[1] |= SGX545_USE1_EXTLD_DBANK_TEMP;
	}
	else
	{
		puInst[1] |= SGX545_USE1_EXTLD_DBANK_PRIMATTR;
	}
	puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
	/* Encode base source. */
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc1(psContext, psInst, 1, IMG_FALSE, 0, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	/* Encode offset source. */
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc0(psContext, psInst, 2, IMG_FALSE, puInst + 0, puInst + 1, 0, IMG_FALSE, 0, psTarget);
	/* Encode extra offset. */
	if (psInst->asArg[3].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only an immediate value is supported for the fourth argument to eldd/eldq"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are supported for the fourth argument to eldd/eldq"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Immediate values can't be indexed"));
	}
	if (psInst->asArg[3].uNumber > SGX545_USE1_EXTLD_OFFSET_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum value for the offset to eldd/eldq is %d", SGX545_USE1_EXTLD_OFFSET_MAX));
	}
	puInst[1] |= 
			(((psInst->asArg[3].uNumber >> SGX545_USE1_EXTLD_OFFSET0_INTERNALSHIFT) << SGX545_USE1_EXTLD_OFFSET0_SHIFT) & ~SGX545_USE1_EXTLD_OFFSET0_CLRMSK) |
			(((psInst->asArg[3].uNumber >> SGX545_USE1_EXTLD_OFFSET1_INTERNALSHIFT) << SGX545_USE1_EXTLD_OFFSET1_SHIFT) & ~SGX545_USE1_EXTLD_OFFSET1_CLRMSK);
	/* Encode range. */
	CheckArgFlags(psContext, psInst, 4, 0);
	EncodeSrc2(psContext, psInst, 4, IMG_FALSE, 0, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	/* Encode the dependent read counter argument. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The last argument to a load instruction must be in the dependent read counter bank"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A dependent read counter register can't be accessed in indexed mode"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a dependent read counter register"));
	}
	if (psInst->asArg[5].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum number of a drc register is %d", EURASIA_USE_DRC_BANK_SIZE - 1));
	}
	puInst[1] |= psInst->asArg[5].uNumber << EURASIA_USE1_LDST_DRCSEL_SHIFT;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */

/*****************************************************************************
 FUNCTION	: EncodeLAPCInstruction

 PURPOSE	: Encode a LAPC instruction.

 PARAMETERS	: psTarget			- Compilation target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLAPCInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags1, uValidFlags3;

	/* Move link-register into pc */
	uValidFlags1 = USEASM_OPFLAGS1_SYNCEND | (~USEASM_OPFLAGS1_PRED_CLRMSK);
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
	if (SupportsExtSyncEnd(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_SYNCENDNOTTAKEN;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
	if (SupportsBranchPwait(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_PWAIT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */

	if (((psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT) == USEASM_PRED_PN)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Pn isn't a valid predicate for branch instructions"));
	}
	CheckFlags(psContext, psInst, uValidFlags1, 0, uValidFlags3);
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCEND) ? EURASIA_USE1_FLOWCTRL_SYNCEND : 0) |
				(EURASIA_USE1_FLOWCTRL_OP2_LAPC << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);
	if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		puInst[1] |= EURASIA_USE1_FLOWCTRL_NOSCHED;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
	if (SupportsExtSyncEnd(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_SYNCENDNOTTAKEN))
	{
		if (psInst->uFlags1 & USEASM_OPFLAGS1_SYNCEND)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, ".syncent and .synce can't be specified together"));
		}
		puInst[1] |= EURASIA_USE1_FLOWCTRL_SYNCEXT | EURASIA_USE1_FLOWCTRL_SYNCEND;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */

	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
	if (SupportsBranchPwait(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_PWAIT))
	{
		puInst[1] |= SGXVEC_USE1_FLOWCTRL_PWAIT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */
}

#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION)
/*****************************************************************************
 FUNCTION	: EncodeBEXCEPTIONInstruction

 PURPOSE	: Encode a BEXCEPTION instruction.

 PARAMETERS	: psTarget			- Compilation target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeBEXCEPTIONInstruction(PCSGX_CORE_DESC		psTarget,
										    PUSE_INST			psInst, 
										    IMG_PUINT32			puInst,
										    PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags1;

	if (!SupportsBranchException(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "bexception isn't supported on this core"));
	}

	/* Exception check. */
	uValidFlags1 = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	
	CheckFlags(psContext, psInst, uValidFlags1, 0, 0);
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_FLOWCTRL_OP2_BA << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
				EURASIA_USE1_BRANCH_SAVELINK |
				EURASIA_USE1_FLOWCTRL_EXCEPTION;
	if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		puInst[1] |= EURASIA_USE1_FLOWCTRL_NOSCHED;
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION) */

/*****************************************************************************
 FUNCTION	: EncodeBranchInstruction

 PURPOSE	: Encode a BA or BR instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.
			  bExtendedAlu		- Does this processor support the extended ALU.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL EncodeBranchInstruction(PCSGX_CORE_DESC		psTarget,
										PUSE_INST			psInst, 
										IMG_PUINT32			puInst,
										PUSEASM_CONTEXT		psContext,
										IMG_UINT32			uCodeOffset,
										IMG_PUINT32			puBaseInst)
{
	IMG_BOOL	bSyncEnd = (psInst->uFlags1 & USEASM_OPFLAGS1_SYNCEND) ? TRUE : FALSE;
	IMG_UINT32	uLabelOffset;
	IMG_UINT32	uValidFlags;
	IMG_UINT32	uValidFlags3;
	IMG_UINT32	uOp2;
	IMG_UINT32	uLabelType, uRefType;
	USEASM_PRED	ePredicate = ((psInst->uFlags1 & ~USEASM_OPFLAGS1_PRED_CLRMSK) >> USEASM_OPFLAGS1_PRED_SHIFT);

	/*
		Get the parameters which vary depending on this branch type.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_BA: 
		{
			uOp2 = EURASIA_USE1_FLOWCTRL_OP2_BA; 
			uRefType = LABEL_REF_UBA; 
			uLabelType = LABEL_REF_BA; 
			break;
		}
		case USEASM_OP_BR: 
		{
			uOp2 = EURASIA_USE1_FLOWCTRL_OP2_BR; 
			uRefType = LABEL_REF_UBR; 
			uLabelType = LABEL_REF_BR; 
			break;
		}
		default: IMG_ABORT();
	}

	/* Branch to absolute or relative address. */
	uValidFlags = USEASM_OPFLAGS1_SYNCEND | USEASM_OPFLAGS1_SAVELINK | (~USEASM_OPFLAGS1_PRED_CLRMSK);
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	if (NumberOfMonitorsSupported(psTarget) > 0)
	{
		uValidFlags |= USEASM_OPFLAGS1_MONITOR;
	}

	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
	if (SupportsExtSyncEnd(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_SYNCENDNOTTAKEN;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
	if (SupportsBranchPwait(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_PWAIT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES)
	if (SupportsAllAnyInstancesFlagOnBranchInstruction(psTarget) && !FixBRN29643(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_ALLINSTANCES | USEASM_OPFLAGS3_ANYINSTANCES;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES) */

	CheckFlags(psContext, psInst, uValidFlags, 0, uValidFlags3);

	if (ePredicate == USEASM_PRED_PN)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Pn isn't a valid predicate for branch instructions"));
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES)
	if ((psInst->uFlags3 & (USEASM_OPFLAGS3_ALLINSTANCES | USEASM_OPFLAGS3_ANYINSTANCES)) != 0)
	{
		if (ePredicate != USEASM_PRED_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A %s instruction can't be predicated and use the .anyinst/.allinst "
				"flags", OpcodeName(psInst->uOpcode)));
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES) */

	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCEND) ? EURASIA_USE1_FLOWCTRL_SYNCEND : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SAVELINK) ? EURASIA_USE1_BRANCH_SAVELINK : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_FLOWCTRL_NOSCHED : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_MONITOR) ? SGX545_USE1_BRANCH_MONITOR : 0) |
				(uOp2 << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);

	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND)
	if (SupportsExtSyncEnd(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_SYNCENDNOTTAKEN))
	{
		if (psInst->uFlags1 & USEASM_OPFLAGS1_SYNCEND)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, ".syncent and .synce can't be specified together"));
		}
		puInst[1] |= EURASIA_USE1_FLOWCTRL_SYNCEXT | EURASIA_USE1_FLOWCTRL_SYNCEND;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXTSYNCEND) */

	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT)
	if (SupportsBranchPwait(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_PWAIT))
	{
		puInst[1] |= SGXVEC_USE1_FLOWCTRL_PWAIT;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_PWAIT) */

	#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES)
	if (
			SupportsAllAnyInstancesFlagOnBranchInstruction(psTarget) && 
			!FixBRN29643(psTarget)
	   )
	{
		if ((psInst->uFlags3 & USEASM_OPFLAGS3_ALLINSTANCES) != 0)
		{
			puInst[0] |= SGXVEC_USE0_FLOWCTRL_ALLINSTANCES;
		}
		if ((psInst->uFlags3 & USEASM_OPFLAGS3_ANYINSTANCES) != 0)
		{
			puInst[0] |= SGXVEC_USE0_FLOWCTRL_ANYINSTANCES;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_ALL_ANY_INSTANCES) */

	if (psInst->asArg[0].uType != USEASM_REGTYPE_LABEL &&
		psInst->asArg[0].uType != USEASM_REGTYPE_LABEL_WITH_OFFSET &&
		psInst->asArg[0].uType != USEASM_REGTYPE_REF)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The argument to a branch must be a label or a reference"));
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_LABEL_WITH_OFFSET)
	{
		uLabelOffset = psInst->asArg[0].uFlags;
	}
	else
	{
		uLabelOffset = 0;
	}
	/*
	  Encode the target address, either by embedding a reference or by
	  looking up a label.
	*/
	if (psInst->asArg[0].uType == USEASM_REGTYPE_REF)
	{
		/* Embed the reference */
		IMG_UINT32 uTarget = psInst->asArg[0].uNumber;

		SetLabelAddress(psTarget, psContext, psInst, uRefType,
						uTarget,
						uCodeOffset, puBaseInst, puInst,
						bSyncEnd);

		return IMG_TRUE;
	}
	else
	{
		if (!GetLabelAddress(psTarget, psInst, psInst->asArg[0].uNumber,
							 uCodeOffset, puBaseInst, puInst, uLabelOffset,
							 uLabelType,
							 bSyncEnd, psContext))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: EncodeIDFInstruction

 PURPOSE	: Encode an IDF instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeIDFInstruction(PCSGX_CORE_DESC		psTarget,
									 PUSE_INST			psInst, 
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext)
{
	/* Issue data fence */
	IMG_UINT32	uValidFlags;
	IMG_UINT32	uDataPath;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID)
	if (SupportSkipInvalidOnIDFInstruction(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_SKIPINVALID;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID) */
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to idf must be a drc register"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A drc register is not indexable"));
	}
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a drc register"));
	}
	if (psInst->asArg[0].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum drc register number is %u", EURASIA_USE_DRC_BANK_SIZE - 1));
	}
	if (psInst->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to idf must be a data path selector"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A data path selector is not indexable"));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a data path selector"));
	}
	uDataPath = psInst->asArg[1].uNumber;
	if (!(uDataPath == EURASIA_USE1_IDF_PATH_ST || uDataPath == EURASIA_USE1_IDF_PATH_PIXELBE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid data path selector"));
	}
	if (FixBRN23960(psTarget) && psInst->asArg[0].uNumber != 0 && psInst->asArg[1].uNumber == EURASIA_USE1_IDF_PATH_PIXELBE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only drc0 can be used with PBE data fences on this core"));
	}
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
			    (EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_IDF << EURASIA_USE1_OTHER_OP2_SHIFT) |	
				(psInst->asArg[0].uNumber << EURASIA_USE1_IDFWDF_DRCSEL_SHIFT) |
				(uDataPath << EURASIA_USE1_IDF_PATH_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_IDFWDF_NOSCHED : 0);
	#if defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID)
	if (SupportSkipInvalidOnIDFInstruction(psTarget) && (psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID))
	{
		puInst[1] |= EURASIA_USE1_SKIPINV;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDF_SUPPORTS_SKIPINVALID) */
}

/*****************************************************************************
 FUNCTION	: EncodeWDFInstruction

 PURPOSE	: Encode an WDF instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeWDFInstruction(PCSGX_CORE_DESC		psTarget,
									 PUSE_INST			psInst,
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext)
{
	/* Wait for data fence to return */
	IMG_UINT32	uValidFlags;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_DRC)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The argument to wdf must be a drc register"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A drc register is not indexable"));
	}
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a drc register"));
	}
	if (psInst->asArg[0].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum drc register number is %u", EURASIA_USE_DRC_BANK_SIZE - 1));
	}
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_WDF << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(psInst->asArg[0].uNumber << EURASIA_USE1_IDFWDF_DRCSEL_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_IDFWDF_NOSCHED : 0);
}

static IMG_VOID EncodeEmitInstructionSource(PUSEASM_CONTEXT	psContext,
											PUSE_INST		psInst,
											IMG_UINT32		uInputSource,
											USEASM_HWARG	eHwSource,
											IMG_PUINT32		puInst,
											PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeEmitInstructionSource

 PURPOSE	: Encode an emit instruction source.

 PARAMETERS	: psContext			- Assembler context.
			  psInst			- Input instruction.
			  uInputSource		- Index of the input source to encode.
			  eHwSource			- Hardware source to encode.
			  puInst			- Written with the encoded instruction.
			  psTarget			- Assembler target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	/*
		For cores with the vector instructions the sources to emit are always
		64-bit registers.
	*/
	if (SupportsVEC34(psTarget))
	{
		IMG_UINT32	uBankExtension;
		IMG_UINT32	uNumberFieldShift;

		switch (eHwSource)
		{
			case USEASM_HWARG_SOURCE0: 
			{
				uBankExtension = EURASIA_USE1_EMIT_S0BEXT; 
				uNumberFieldShift = EURASIA_USE0_SRC0_SHIFT; 
				break;
			}
			case USEASM_HWARG_SOURCE1: 
			{
				uBankExtension = EURASIA_USE1_S1BEXT; 
				uNumberFieldShift = EURASIA_USE0_SRC1_SHIFT;
				break;
			}
			case USEASM_HWARG_SOURCE2:
			{
				uBankExtension = EURASIA_USE1_S2BEXT; 
				uNumberFieldShift = EURASIA_USE0_SRC2_SHIFT;
				break;
			}
			default: IMG_ABORT();
		}

		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  uBankExtension,
								      puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_EMIT_SRC_FIELD_LENGTH,
									  uNumberFieldShift,
									  eHwSource,
									  uInputSource /* uArg */,
									  psTarget);

	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
	{
		switch (eHwSource)
		{
			case USEASM_HWARG_SOURCE0:
			{
				EncodeSrc0(psContext, 
						   psInst, 
						   uInputSource, 
						   IMG_TRUE /* bAllowExtended */, 
						   puInst + 0, 
						   puInst + 1, 
						   EURASIA_USE1_EMIT_S0BEXT, 
						   IMG_FALSE /* bFmtSelect */, 
						   0 /* uAltFmtFlag */, 
						   psTarget);
				break;
			}
			case USEASM_HWARG_SOURCE1:
			{
				EncodeSrc1(psContext, 
						   psInst, 
						   uInputSource, 
						   TRUE, 
						   EURASIA_USE1_S1BEXT, 
						   IMG_FALSE /* bSigned */, 
						   puInst + 0, 
						   puInst + 1, 
						   IMG_FALSE /* bBitwise */, 
						   IMG_FALSE /* bFmtSelect */, 
						   0 /* uAltFmtFlag */, 
						   psTarget);
		
				break;
			}
			case USEASM_HWARG_SOURCE2:
			{
				EncodeSrc2(psContext, 
						   psInst, 
						   uInputSource, 
						   TRUE, 
						   EURASIA_USE1_S2BEXT, 
						   IMG_FALSE /* bSigned */, 
						   puInst + 0, 
						   puInst + 1, 
						   IMG_FALSE /* bBitwise */, 
						   IMG_FALSE /* bFmtSelect */, 
						   0 /* uAltFmtFlag */, 
						   psTarget);
				break;
			}
			default: IMG_ABORT();
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeEmitInstruction

 PURPOSE	: Encode an emit instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeEmitInstruction(PCSGX_CORE_DESC	psTarget,
									  PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uTarget;
	IMG_UINT32 uNextArg = 0;
	IMG_UINT32 uSideBand109To96 = 0;
	IMG_UINT32 uValidFlags1;
	IMG_UINT32 uValidFlags2;
	IMG_UINT32 uValidFlags3;
	IMG_BOOL bMTETarget = IMG_FALSE;

	uValidFlags1 = USEASM_OPFLAGS1_END;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags1 |= USEASM_OPFLAGS1_NOSCHED;
	}
	uValidFlags2 = 0;
	if (psInst->uOpcode == USEASM_OP_EMITPDS)
	{
		uValidFlags2 |= USEASM_OPFLAGS2_TASKSTART | USEASM_OPFLAGS2_TASKEND;
	}
	uValidFlags3 = USEASM_OPFLAGS3_FREEP;
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		if (psInst->uOpcode == USEASM_OP_EMITMTEVERTEX ||
			psInst->uOpcode == USEASM_OP_EMITVCBVERTEX)
		{
			uValidFlags3 |= USEASM_OPFLAGS3_TWOPART;
		}
		if (psInst->uOpcode == USEASM_OP_EMITMTEVERTEX)
		{
			uValidFlags3 |= USEASM_OPFLAGS3_CUT;
		}
		if (psInst->uOpcode == USEASM_OP_EMITMTESTATE ||
			psInst->uOpcode == USEASM_OP_EMITVCBSTATE)
		{
			uValidFlags3 |= USEASM_OPFLAGS3_THREEPART;
		}
		if (psInst->uOpcode == USEASM_OP_EMITVCBSTATE ||
			psInst->uOpcode == USEASM_OP_EMITVCBVERTEX)
		{
			uValidFlags3 &= ~USEASM_OPFLAGS3_FREEP;
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	CheckFlags(psContext, psInst, uValidFlags1, uValidFlags2, uValidFlags3);

	switch (psInst->uOpcode)
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		case USEASM_OP_EMITPIXEL: 
		{
			if (!SupportsVEC34(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, 
							   psInst, 
							   "%s isn't supported on this core", 
							   OpcodeName(psInst->uOpcode)));
			}
			uTarget = EURASIA_USE1_EMIT_TARGET_PIXELBE;
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		case USEASM_OP_EMITPIXEL1: 
		case USEASM_OP_EMITPIXEL2: 
		{
			if (SupportsVEC34(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, 
							   psInst, 
							   "%s isn't supported on this core", 
							   OpcodeName(psInst->uOpcode)));
			}
			uTarget = EURASIA_USE1_EMIT_TARGET_PIXELBE; 
			break;
		}
		case USEASM_OP_EMITSTATE: 
		case USEASM_OP_EMITMTESTATE:
		case USEASM_OP_EMITVERTEX:
		case USEASM_OP_EMITMTEVERTEX:
		case USEASM_OP_EMITPRIMITIVE: uTarget = EURASIA_USE1_EMIT_TARGET_MTE; bMTETarget = IMG_TRUE; break;
		case USEASM_OP_EMITPDS: uTarget = EURASIA_USE1_EMIT_TARGET_PDS; break;
		#if defined(SUPPORT_SGX_FEATURE_VCB)
		case USEASM_OP_EMITVCBSTATE:
		case USEASM_OP_EMITVCBVERTEX: uTarget = SGX545_USE1_EMIT_TARGET_VCB; bMTETarget = IMG_TRUE; break;
		#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
		default: IMG_ABORT();
	}
	/* Emit data */
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				(uTarget << EURASIA_USE1_EMIT_TARGET_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_EMIT_NOSCHED : 0);
	puInst[0] = ((psInst->uFlags3 & USEASM_OPFLAGS3_FREEP) ? EURASIA_USE0_EMIT_FREEP : 0);
	if (uTarget == EURASIA_USE1_EMIT_TARGET_PDS)
	{
		puInst[1] |= ((psInst->uFlags2 & USEASM_OPFLAGS2_TASKSTART) ? EURASIA_USE1_EMIT_PDSCTRL_TASKS : 0) |
					 ((psInst->uFlags2 & USEASM_OPFLAGS2_TASKEND) ? EURASIA_USE1_EMIT_PDSCTRL_TASKE : 0);
	}
	else if (bMTETarget)
	{
		#if defined(SUPPORT_SGX_FEATURE_VCB)
		if (SupportsVCB(psTarget))
		{
			switch (psInst->uOpcode)
			{
				case USEASM_OP_EMITMTESTATE: 
				case USEASM_OP_EMITVCBSTATE:
					puInst[1] |= (SGX545_USE1_EMIT_VCB_EMITTYPE_STATE << SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT);
					break;
				case USEASM_OP_EMITMTEVERTEX: 
				case USEASM_OP_EMITVCBVERTEX:
					puInst[1] |= (SGX545_USE1_EMIT_VCB_EMITTYPE_VERTEX << SGX545_USE1_EMIT_VCB_EMITTYPE_SHIFT);
					break;
				default:
					USEASM_ERRMSG((psContext->pvContext, psInst,
								   "%s isn't supported on this processor",
								   OpcodeName(psInst->uOpcode)));
					break;
			}
		}
		else
		#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
		{
			switch (psInst->uOpcode)
			{
				case USEASM_OP_EMITSTATE: puInst[1] |= (EURASIA_USE1_EMIT_MTECTRL_STATE << EURASIA_USE1_EMIT_MTECTRL_SHIFT); break;
				case USEASM_OP_EMITVERTEX: puInst[1] |= (EURASIA_USE1_EMIT_MTECTRL_VERTEX << EURASIA_USE1_EMIT_MTECTRL_SHIFT); break;
				case USEASM_OP_EMITPRIMITIVE: puInst[1] |= (EURASIA_USE1_EMIT_MTECTRL_PRIMITIVE << EURASIA_USE1_EMIT_MTECTRL_SHIFT); break;
				default:
					USEASM_ERRMSG((psContext->pvContext, psInst,
								   "%s isn't supported on this processor",
								   OpcodeName(psInst->uOpcode)));
					break;
			}
		}

		#if defined(SUPPORT_SGX_FEATURE_VCB)
		if (SupportsVCB(psTarget))
		{
			if (psInst->uOpcode == USEASM_OP_EMITMTESTATE ||
				psInst->uOpcode == USEASM_OP_EMITVCBSTATE)
			{
				if (psInst->uFlags3 & USEASM_OPFLAGS3_THREEPART)
				{
					puInst[1] |= SGX545_USE1_EMIT_STATE_THREEPART;
				}
			}
			if (psInst->uOpcode == USEASM_OP_EMITMTEVERTEX || 
				psInst->uOpcode == USEASM_OP_EMITVCBVERTEX)
			{
				if (psInst->uFlags3 & USEASM_OPFLAGS3_TWOPART)
				{
					puInst[1] |= SGX545_USE1_EMIT_VERTEX_TWOPART;
				}
			}
			if (psInst->uOpcode == USEASM_OP_EMITMTEVERTEX)
			{
				if (psInst->uFlags3 & USEASM_OPFLAGS3_CUT)
				{
					puInst[1] |= SGX545_USE1_EMIT_MTE_CUT;
				}
			}
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	}
	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to an emit must be an immediate value"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}	
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		if (psInst->asArg[0].uNumber > SGX545_USE1_EMIT_INCP_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum immediate value for the first argument to an emit is %d", SGX545_USE1_EMIT_INCP_MAX));
		}
		puInst[1] |= ((psInst->asArg[0].uNumber << EURASIA_USE1_EMIT_INCP_SHIFT) & ~EURASIA_USE1_EMIT_INCP_CLRMSK);
		puInst[1] |= ((psInst->asArg[0].uNumber >> SGX545_USE1_EMIT_INCPEXT_INTERNALSHIFT) << SGX545_USE1_EMIT_INCPEXT_SHIFT);
	}
	else
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	{
		if (psInst->asArg[0].uNumber > EURASIA_USE1_EMIT_INCP_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum immediate value for the first argument to an emit is %d", EURASIA_USE1_EMIT_INCP_MAX));
		}
		puInst[1] |= (psInst->asArg[0].uNumber << EURASIA_USE1_EMIT_INCP_SHIFT);
	}
	uNextArg = 1;
	if (psInst->uOpcode == USEASM_OP_EMITPIXEL1 || 
		psInst->uOpcode == USEASM_OP_EMITPIXEL2 ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL ||
		psInst->uOpcode == USEASM_OP_EMITPDS)
	{
		EncodeEmitInstructionSource(psContext, psInst, uNextArg, USEASM_HWARG_SOURCE0, puInst, psTarget);
		uNextArg++;
	}
	if (psInst->uOpcode == USEASM_OP_EMITPIXEL1 || 
		psInst->uOpcode == USEASM_OP_EMITPIXEL2 ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL ||
		psInst->uOpcode == USEASM_OP_EMITPDS ||
		psInst->uOpcode == USEASM_OP_EMITSTATE ||
		psInst->uOpcode == USEASM_OP_EMITPRIMITIVE ||
		psInst->uOpcode == USEASM_OP_EMITMTESTATE ||
		psInst->uOpcode == USEASM_OP_EMITMTEVERTEX)
	{
		EncodeEmitInstructionSource(psContext, psInst, uNextArg, USEASM_HWARG_SOURCE1, puInst, psTarget);
		uNextArg++;
	}
	else
	{
		EncodeUnusedSource(1, puInst + 0, puInst + 1);
	}
	if (psInst->uOpcode == USEASM_OP_EMITPDS ||
		psInst->uOpcode == USEASM_OP_EMITVERTEX ||
		psInst->uOpcode == USEASM_OP_EMITPRIMITIVE ||
		psInst->uOpcode == USEASM_OP_EMITVCBVERTEX ||
		psInst->uOpcode == USEASM_OP_EMITVCBSTATE ||
		psInst->uOpcode == USEASM_OP_EMITMTESTATE ||
		psInst->uOpcode == USEASM_OP_EMITMTEVERTEX ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL2 ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL)
	{
		EncodeEmitInstructionSource(psContext, psInst, uNextArg, USEASM_HWARG_SOURCE2, puInst, psTarget);
		uNextArg++;
	}
	else
	{
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
	}
	if (psInst->uOpcode == USEASM_OP_EMITPIXEL ||
		psInst->uOpcode == USEASM_OP_EMITPIXEL2 ||
		psInst->uOpcode == USEASM_OP_EMITPDS ||
		psInst->uOpcode == USEASM_OP_EMITPRIMITIVE)
	{
		if (psInst->asArg[uNextArg].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The last argument to an emit must be an immediate value"));
		}
		if (psInst->asArg[uNextArg].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
		}	
		if (psInst->asArg[uNextArg].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
		}
		uSideBand109To96 = psInst->asArg[uNextArg].uNumber;
	}
	else if (psInst->uOpcode == USEASM_OP_EMITPIXEL1)
	{
		#if defined(SUPPORT_SGX545)
		if (psTarget->eCoreType == SGX_CORE_ID_545)
		{
			uSideBand109To96 = SGX545_PIXELBE2S2_TWOEMITS;
		}
		else
		#endif /* defined(SUPPORT_SGX545) */
		{
			uSideBand109To96 = EURASIA_PIXELBE1SB_TWOEMITS;
		}
	}
	puInst[1] |= (((uSideBand109To96 >> 12) << EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK) |
				 (((uSideBand109To96 >> 6) << EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT) & ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK);
	puInst[0] |= (((uSideBand109To96 >> 0) << EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT) & ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK);
}

/*****************************************************************************
 FUNCTION	: EncodeSMOAInstruction

 PURPOSE	: Encode a SMOA instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSMOAInstruction(PCSGX_CORE_DESC	psTarget,
									  PUSE_INST			psInst, 
									  IMG_PUINT32		puInst,
									  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uValidFlags;
	IMG_UINT32	i;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		if (!IsLoadMOEStateFromRegisters(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Loading the MOE state from registers isn't supported on this processor."));
		}
		puInst[0] = 0;
		puInst[1] = EURASIA_USE1_MOECTRL_REGDAT;
		EncodeSrc1(psContext, psInst, 0, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
		EncodeSrc2(psContext, psInst, 1, IMG_TRUE, EURASIA_USE1_S2BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa must be an immediate value", i));
			}
			if ((IMG_INT32)psInst->asArg[i].uNumber < -512 || (IMG_INT32)psInst->asArg[i].uNumber > 511)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa is out of range (valid range is [-512..511])", i));
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa can't be indexed", i));
			}
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to smia", i));
			}
		}
		for (i = 4; i < 8; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_ADDRESSMODE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa must be an address mode", i));
			}
			if (psInst->asArg[i].uNumber > EURASIA_USE1_MOECTRL_ADDRESSMODE_MIRROR)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa is not a legal value (should be one of none, repeat, clamp or mirror)", i));
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smoa can't be indexed", i));
			}
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to smoa", i));
			}
		}
		puInst[0] = 
			((psInst->asArg[3].uNumber << EURASIA_USE0_MOECTRL_SMOA_S2OFFSET_SHIFT) & ~EURASIA_USE0_MOECTRL_SMOA_S2OFFSET_CLRMSK) |
			((psInst->asArg[2].uNumber << EURASIA_USE0_MOECTRL_SMOA_S1OFFSET_SHIFT) & ~EURASIA_USE0_MOECTRL_SMOA_S1OFFSET_CLRMSK) |
			((psInst->asArg[1].uNumber << EURASIA_USE0_MOECTRL_SMOA_S0OFFSET_SHIFT) & ~EURASIA_USE0_MOECTRL_SMOA_S0OFFSET_CLRMSK) |
			((psInst->asArg[0].uNumber << EURASIA_USE0_MOECTRL_SMOA_DOFFSET_SHIFT) & ~EURASIA_USE0_MOECTRL_SMOA_DOFFSET_CLRMSK);
		puInst[1] = (((psInst->asArg[0].uNumber >> 2) << EURASIA_USE1_MOECTRL_SMOA_DOFFSET_SHIFT) & ~EURASIA_USE1_MOECTRL_SMOA_DOFFSET_CLRMSK) |
					((psInst->asArg[7].uNumber << EURASIA_USE1_MOECTRL_SMOA_S2AM_SHIFT) & ~EURASIA_USE1_MOECTRL_SMOA_S2AM_CLRMSK) |
					((psInst->asArg[6].uNumber << EURASIA_USE1_MOECTRL_SMOA_S1AM_SHIFT) & ~EURASIA_USE1_MOECTRL_SMOA_S1AM_CLRMSK) |
					((psInst->asArg[5].uNumber << EURASIA_USE1_MOECTRL_SMOA_S0AM_SHIFT) & ~EURASIA_USE1_MOECTRL_SMOA_S0AM_CLRMSK) |
					((psInst->asArg[4].uNumber << EURASIA_USE1_MOECTRL_SMOA_DAM_SHIFT) & ~EURASIA_USE1_MOECTRL_SMOA_DAM_CLRMSK);
	}
	puInst[1] |= (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				 (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				 (EURASIA_USE1_MOECTRL_OP2_SMOA << EURASIA_USE1_MOECTRL_OP2_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_MOECTRL_NOSCHED : 0);
}

/*****************************************************************************
 FUNCTION	: EncodeSMLSIInstruction

 PURPOSE	: Encode a SMLSI instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSMLSIInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	IMG_UINT32	i;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE && psInst->asArg[0].uType != USEASM_REGTYPE_SWIZZLE)
	{
		if (!IsLoadMOEStateFromRegisters(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Loading the MOE state from registers isn't supported on this processor"));
		}
		puInst[0] = 0;
		puInst[1] = EURASIA_USE1_MOECTRL_REGDAT;
		EncodeSrc1(psContext, psInst, 0, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
		EncodeSrc2(psContext, psInst, 1, IMG_TRUE, EURASIA_USE1_S2BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		IMG_UINT32		auInc[4];

		for (i = 0; i < 4; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE &&
				psInst->asArg[i].uType != USEASM_REGTYPE_SWIZZLE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi must be an immediate value or a swizzle", i));
			}
			if (psInst->asArg[i].uType == USEASM_REGTYPE_SWIZZLE)
			{
				IMG_UINT32	uChan;

				auInc[i] = 0;
				for (uChan = 0; uChan < 4; uChan++)
				{
					IMG_UINT32	uChanSel;
					
					uChanSel = (psInst->asArg[i].uNumber >> (uChan * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;

					if (uChanSel > USEASM_SWIZZLE_SEL_W)
					{
						USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d uses component selects other than "
							"x, y, z and w", i));
					}
					auInc[i] |= (uChanSel << (uChan * EURASIA_USE_MOESWIZZLE_FIELD_SIZE));
				}

				if (auInc[i] > EURASIA_USE0_MOECTRL_SMLSI_SWIZ_MAX)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi isn't a valid swizzle", i));
				}
			}
			else
			{
				if (
						(IMG_INT32)psInst->asArg[i].uNumber < EURASIA_USE0_MOECTRL_SMLSI_INC_MIN || 
						(IMG_INT32)psInst->asArg[i].uNumber > EURASIA_USE0_MOECTRL_SMLSI_INC_MAX
				   )
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi is out of range (valid range is [%d..%d])", i, EURASIA_USE0_MOECTRL_SMLSI_INC_MIN, EURASIA_USE0_MOECTRL_SMLSI_INC_MAX));
				}
				auInc[i] = psInst->asArg[i].uNumber;
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi can't be indexed", i));
			}
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to smlsi", i));
			}
		}
		for (i = 4; i < 8; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi must be an immediate value", i));
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi can't be indexed", i));
			}
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to smlsi", i));
			}
		}
		for (i = 8; i < 11; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi must be an immediate value", i));
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi can't be indexed", i));
			}	
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to smlsi", i));
			}
			if (psInst->asArg[i].uNumber > EURASIA_USE1_MOECTRL_SMLSI_LIMIT_MAX)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi is out of range (valid values are 0, 4, 8... %d)", i, EURASIA_USE1_MOECTRL_SMLSI_LIMIT_MAX));
			}
			if ((psInst->asArg[i].uNumber & (EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN - 1)) != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smlsi should be a multiple of %d", i, EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN));
			}
		}
		puInst[0] = 
				((auInc[3] << EURASIA_USE0_MOECTRL_SMLSI_S2INC_SHIFT) & ~EURASIA_USE0_MOECTRL_SMLSI_S2INC_CLRMSK) |
				((auInc[2] << EURASIA_USE0_MOECTRL_SMLSI_S1INC_SHIFT) & ~EURASIA_USE0_MOECTRL_SMLSI_S1INC_CLRMSK) |
				((auInc[1] << EURASIA_USE0_MOECTRL_SMLSI_S0INC_SHIFT) & ~EURASIA_USE0_MOECTRL_SMLSI_S0INC_CLRMSK) |
				((auInc[0] << EURASIA_USE0_MOECTRL_SMLSI_DINC_SHIFT) & ~EURASIA_USE0_MOECTRL_SMLSI_DINC_CLRMSK);
		puInst[1] = (psInst->asArg[4].uNumber ? EURASIA_USE1_MOECTRL_SMLSI_DUSESWIZ : 0) |
				    (psInst->asArg[5].uNumber ? EURASIA_USE1_MOECTRL_SMLSI_S0USESWIZ : 0) |
					(psInst->asArg[6].uNumber ? EURASIA_USE1_MOECTRL_SMLSI_S1USESWIZ : 0) |
					(psInst->asArg[7].uNumber ? EURASIA_USE1_MOECTRL_SMLSI_S2USESWIZ : 0);
		puInst[1] |= 
					((psInst->asArg[8].uNumber / EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN) << EURASIA_USE1_MOECTRL_SMLSI_TLIMIT_SHIFT) |
					((psInst->asArg[9].uNumber / EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN) << EURASIA_USE1_MOECTRL_SMLSI_PLIMIT_SHIFT) |
					((psInst->asArg[10].uNumber / EURASIA_USE1_MOECTRL_SMLSI_LIMIT_GRAN) << EURASIA_USE1_MOECTRL_SMLSI_SLIMIT_SHIFT);
	
	}
	puInst[1] |= (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				 (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_MOECTRL_NOSCHED : 0);
	puInst[1] |= EURASIA_USE1_MOECTRL_OP2_SMLSI << EURASIA_USE1_MOECTRL_OP2_SHIFT;
}

#if defined(SUPPORT_SGX_FEATURE_USE_CFI)
/*****************************************************************************
 FUNCTION	: EncodeCFIInstruction

 PURPOSE	: Encode a CFI instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeCFIInstruction(PCSGX_CORE_DESC			psTarget,
									 PUSE_INST				psInst,
									 IMG_PUINT32			puInst,
									 PUSEASM_CONTEXT		psContext)
{
	if (!SupportsCFI(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The %s instruction isn't supported on this core", 
					   OpcodeName(psInst->uOpcode)));
	}

	/*
		Check the instruction doesn't have any unexpected modifiers.
	*/
	CheckFlags(psContext, 
			   psInst, 
			   USEASM_OPFLAGS1_NOSCHED, 
			   0, 
			   USEASM_OPFLAGS3_GLOBAL | USEASM_OPFLAGS3_DM_NOMATCH);

	/*
		Set up the invariant parts of the instruction.
	*/
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(SGX545_USE1_OTHER2_OP2_CFI << EURASIA_USE1_OTHER2_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				EURASIA_USE1_SPECIAL_OPCAT_EXTRA;

	/*
		Check for instruction modifiers.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		puInst[1] |= EURASIA_USE1_NOSCHED;
	}
	if (psInst->uFlags3 & USEASM_OPFLAGS3_GLOBAL)
	{
		puInst[1] |= SGX545_USE1_OTHER2_CFI_GLOBAL;
	}
	if (psInst->uFlags3 & USEASM_OPFLAGS3_DM_NOMATCH)
	{
		puInst[1] |= SGX545_USE1_OTHER2_CFI_DM_NOMATCH;
	}

	switch (psInst->uOpcode)
	{
		case USEASM_OP_CF: puInst[1] |= SGX545_USE1_OTHER2_CFI_FLUSH; break;
		case USEASM_OP_CI: puInst[1] |= SGX545_USE1_OTHER2_CFI_INVALIDATE; break;
		case USEASM_OP_CFI: puInst[1] |= SGX545_USE1_OTHER2_CFI_FLUSH | SGX545_USE1_OTHER2_CFI_INVALIDATE; break;
		default: break;
	}

	/*
		Encode hardware source 1 from source 0 to the instruction. This is the
		data master to flush for.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeSrc1(psContext,
			   psInst,
			   0,
			   IMG_TRUE,
			   EURASIA_USE1_S1BEXT,
			   IMG_FALSE,
			   puInst + 0,
			   puInst + 1,
			   IMG_FALSE,
			   IMG_FALSE,
			   0, 
			   psTarget);

	/*
		Encode hardware source 2 from source 1 to the instruction. This is the
		address to flush for.
	*/
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc2(psContext,
			   psInst,
			   1,
			   IMG_TRUE,
			   EURASIA_USE1_S2BEXT,
			   IMG_FALSE,
			   puInst + 0,
			   puInst + 1,
			   IMG_FALSE,
			   IMG_FALSE,
			   0,
			   psTarget);

	/*
		Encode the cache levels to flush from source 2 to the instruction.
	*/
	if (psInst->asArg[2].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The second argument to %s must be an immediate value", 
					   OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[2].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "No flags are valid for the second argument to %s", 
					   OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[2].uIndex != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "An index isn't valid for the second argument to %s", 
					   OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[2].uNumber > SGX545_USE1_OTHER2_CFI_LEVEL_MAX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The maximum value for the second argument to %s is %d", 
					   OpcodeName(psInst->uOpcode),
					   SGX545_USE1_OTHER2_CFI_LEVEL_MAX));
	}
	puInst[1] |= (psInst->asArg[2].uNumber << SGX545_USE1_OTHER2_CFI_LEVEL_SHIFT);
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_CFI) */

#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
/*****************************************************************************
 FUNCTION	: EncodeMOESTInstruction

 PURPOSE	: Encode an MOEST instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMOESTInstruction(PCSGX_CORE_DESC			psTarget,
									   PUSE_INST				psInst,
									   IMG_PUINT32				puInst,
									   PUSEASM_CONTEXT			psContext)
{
	IMG_UINT32	uValidFlags;

	if (!SupportsMOEStore(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The moest instruction isn't supported on this core"));
	}

	uValidFlags = USEASM_OPFLAGS1_SKIPINVALID;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);

	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
			    (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_MOECTRL_NOSCHED : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				(SGX545_USE1_MOECTRL_OP2_MOEST << EURASIA_USE1_MOECTRL_OP2_SHIFT);

	EncodeDest(psContext, psInst, IMG_TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);

	if (psInst->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to moest must be an immediate value"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to moest can't be indexed"));
	}	
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to moest"));
	}
	if (psInst->asArg[1].uNumber > SGX545_USE1_MOECTRL_MOEST_PARAM_MAXIMUM)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum value for the second argument to moest is %d",
			SGX545_USE1_MOECTRL_MOEST_PARAM_MAXIMUM));
	}
	puInst[1] |= (psInst->asArg[1].uNumber << SGX545_USE1_MOECTRL_MOEST_PARAM_SHIFT);
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */

/*****************************************************************************
 FUNCTION	: EncodeMOEInstruction

 PURPOSE	: Encode an MOE instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMOEInstruction(PCSGX_CORE_DESC			psTarget,
									 PUSE_INST				psInst,
									 IMG_PUINT32			puInst,
									 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uOpcode;
	IMG_UINT32 uValidFlags;
	IMG_UINT32 i;

	#if defined(USER)
	IMG_PCHAR pszOpcode;

	switch (psInst->uOpcode)
	{
		case USEASM_OP_SMR: pszOpcode = "smr"; break;
		case USEASM_OP_SMBO: pszOpcode = "smbo"; break;
		case USEASM_OP_IMO: pszOpcode = "imo"; break;
		default: pszOpcode = NULL; break;
	}
	#endif

	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE && psInst->uOpcode != USEASM_OP_IMO)
	{
		if (!IsLoadMOEStateFromRegisters(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Loading the MOE state from registers isn't supported on this core"));
		}
		puInst[0] = 0;
		puInst[1] = EURASIA_USE1_MOECTRL_REGDAT;
		EncodeSrc1(psContext, psInst, 0, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
		EncodeSrc2(psContext, psInst, 1, IMG_TRUE, EURASIA_USE1_S2BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to smr/smbo/imo must be an immediate value", i));
			}
			if (psInst->uOpcode == USEASM_OP_IMO)
			{
				if ((IMG_INT32)psInst->asArg[i].uNumber < EURASIA_USE1_MOECTRL_IMO_INC_MIN || 
					(IMG_INT32)psInst->asArg[i].uNumber > EURASIA_USE1_MOECTRL_IMO_INC_MAX)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to imo is out of range (valid range is [%d..%d])", i, EURASIA_USE1_MOECTRL_IMO_INC_MIN, EURASIA_USE1_MOECTRL_IMO_INC_MAX));
				}
			}
			else
			{
				if (psInst->asArg[i].uNumber > EURASIA_USE1_MOECTRL_OFFSETRANGE_MAX)
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s is out of range (valid range is [0..%d])", i, pszOpcode, EURASIA_USE1_MOECTRL_OFFSETRANGE_MAX));
				}	
			}
			if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s can't be indexed", i, pszOpcode));
			}
			if (psInst->asArg[i].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to %s", i, pszOpcode));
			}
		}
		puInst[0] = 
				((psInst->asArg[3].uNumber << EURASIA_USE0_MOECTRL_SMR_S2RANGE_SHIFT) & ~EURASIA_USE0_MOECTRL_SMR_S2RANGE_CLRMSK) |
				((psInst->asArg[2].uNumber << EURASIA_USE0_MOECTRL_SMR_S1RANGE_SHIFT) & ~EURASIA_USE0_MOECTRL_SMR_S1RANGE_CLRMSK) |
				((psInst->asArg[1].uNumber << EURASIA_USE0_MOECTRL_SMR_S0RANGE_SHIFT) & ~EURASIA_USE0_MOECTRL_SMR_S0RANGE_CLRMSK);
		puInst[1] = (((psInst->asArg[1].uNumber >> 8) << EURASIA_USE1_MOECTRL_SMR_S0RANGE_SHIFT) & ~EURASIA_USE1_MOECTRL_SMR_S0RANGE_CLRMSK) |
					((psInst->asArg[0].uNumber << EURASIA_USE1_MOECTRL_SMR_DRANGE_SHIFT) & ~EURASIA_USE1_MOECTRL_SMR_DRANGE_CLRMSK);
	}
	puInst[1] |= (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				 (EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				 ((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_MOECTRL_NOSCHED : 0);
	switch (psInst->uOpcode)
	{
		case USEASM_OP_SMR: uOpcode = EURASIA_USE1_MOECTRL_OP2_SMR; break;
		case USEASM_OP_SMBO: uOpcode = EURASIA_USE1_MOECTRL_OP2_SMBO; break;
		case USEASM_OP_IMO: uOpcode = EURASIA_USE1_MOECTRL_OP2_IMO; break;
		default: IMG_ABORT();
	}
	puInst[1] |= uOpcode << EURASIA_USE1_MOECTRL_OP2_SHIFT;
}

/*****************************************************************************
 FUNCTION	: EncodeSETFCInstruction

 PURPOSE	: Encode n SETFC instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSETFCInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uValidFlags;
	IMG_UINT32 i;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	for (i = 0; i < 2; i++)
	{
		if (psInst->asArg[i].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to setfc must be an immediate value", i));
		}
		if (psInst->asArg[i].uNumber != 0 && psInst->asArg[i].uNumber != 1)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to setfc is must be either #0 or #1", i));
		}
		if (psInst->asArg[i].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to setfc can't be indexed", i));
		}
		if (psInst->asArg[i].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for argument %d to setfc", i));
		}
	}
	puInst[0] = ((psInst->asArg[0].uNumber == 1) ? EURASIA_USE0_MOECTRL_SETFC_EFO_SELFMTCTL : 0) |
				((psInst->asArg[1].uNumber == 1) ? EURASIA_USE0_MOECTRL_SETFC_COL_SETFMTCTL : 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_MOECTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_MOECTRL_OP2_SETFC << EURASIA_USE1_MOECTRL_OP2_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_MOECTRL_NOSCHED : 0);
}

static IMG_VOID EncodeLIMMInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst, 
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext,
									  IMG_UINT32			uCodeOffset,
									  IMG_PUINT32			puBaseInst)
/*****************************************************************************
 FUNCTION	: EncodeLIMMInstruction

 PURPOSE	: Encode a LIMM (load immediate) instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uImmediate;

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | (~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_END |
				USEASM_OPFLAGS1_NOSCHED, 0, 0);

	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_LIMM_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_LIMM_NOSCHED : 0);

	if (psInst->asArg[1].uType == USEASM_REGTYPE_LABEL ||
		psInst->asArg[1].uType == USEASM_REGTYPE_LABEL_WITH_OFFSET)
	{
		IMG_UINT32 uLabelOffset = 0;
		if (psInst->asArg[1].uType == USEASM_REGTYPE_LABEL_WITH_OFFSET)
		{
			uLabelOffset = psInst->asArg[1].uFlags;
		}
		CheckArgFlags(psContext, psInst, 1, 0);
		GetLabelAddress(psTarget, psInst, psInst->asArg[1].uNumber, uCodeOffset, puBaseInst, puInst, uLabelOffset, LABEL_REF_LIMM, FALSE, psContext);
	}
	else
	{
		if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			uImmediate = psInst->asArg[1].uNumber;
		}
		else if (psInst->asArg[1].uType == USEASM_REGTYPE_REF)
		{
			/* Encode references as an immediate value */
			uImmediate = psInst->asArg[1].uNumber;
		}
		else
		{
			uImmediate = *(IMG_PUINT32)(g_pfHardwareConstants + psInst->asArg[1].uNumber);
		}
		CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_INVERT);
		if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_INVERT)
		{
			uImmediate = ~uImmediate;
		}
		puInst[0] |= (((uImmediate & 0x001FFFFF) >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT);
		puInst[1] |= (((uImmediate & 0xFC000000) >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT) |
					 (((uImmediate & 0x03E00000) >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT);
	}

	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
}

/*****************************************************************************
 FUNCTION	: EncodeMOVInstruction

 PURPOSE	: Encode a MOV instruction.

 PARAMETERS	: psTarget			- Compile target.
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMOVInstruction(PCSGX_CORE_DESC			psTarget,
									 PUSE_INST				psInst, 
									 IMG_PUINT32			puInst,
									 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	/* Special move form: Move a register into the link-pointer register. */
	if (psInst->asArg[0].uType == USEASM_REGTYPE_LINK)
	{
		IMG_UINT32	uValidFlags;
		uValidFlags = (~USEASM_OPFLAGS1_PRED_CLRMSK);
		if (IsEnhancedNoSched(psTarget))
		{
			uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
		}
		CheckFlags(psContext, psInst, uValidFlags, 0, 0);
		puInst[0] = 0;
		puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_FLOWCTRL_OP2_SETL << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
					(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_FLOWCTRL_NOSCHED : 0);

		/* Check for any argument modifier flags on the destination. */
		CheckArgFlags(psContext, psInst, 0, 0);

		/* Check for any argument modifier flags on the source. */
		CheckArgFlags(psContext, psInst, 1, 0);
		/* Encode the source. */
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	}
	/* Special move form: Move the link-pointer register into another register. */
	else if (psInst->asArg[1].uType == USEASM_REGTYPE_LINK)
	{
		IMG_UINT32	uValidFlags;
		uValidFlags = (~USEASM_OPFLAGS1_PRED_CLRMSK);
		if (IsEnhancedNoSched(psTarget))
		{
			uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
		}
		CheckFlags(psContext, psInst, uValidFlags, 0, 0);
		puInst[0] = 0;
		puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
					(EURASIA_USE1_FLOWCTRL_OP2_SAVL << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
					(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_FLOWCTRL_NOSCHED : 0);

		/* Check for any argument modifier flags on the destination. */
		CheckArgFlags(psContext, psInst, 0, 0);
		/* Encode the SAVL destination. */
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		/* Check for any argument modifier flags on the source. */
		CheckArgFlags(psContext, psInst, 1, 0);
	}
	else
	{
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		if (SupportsVEC34(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core", OpcodeName(psInst->uOpcode)));
			return;
		}	
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

		/* Check instruction validity. */
		CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_SYNCSTART |
					USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
					(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK), 0, 0);
		/* Encode opcode, predicate and instruction flags */
		puInst[0] = 0;
		puInst[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
					(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
					((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
					(EURASIA_USE1_MOVC_TSTDTYPE_UNCOND << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
		if (uRptCount > 0)
		{
			puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
		}
		else
		{
			puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
		}
		/* Encode the banks and offsets of the source and destination. */
		CheckArgFlags(psContext, psInst, 1, 0);
		CheckArgFlags(psContext, psInst, 0, 0);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeMutexInstruction

 PURPOSE	: Encode a mutex instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMutexInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst,
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
{
	/* Enter/leave critical section. */
	IMG_UINT32	uValidFlags;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	if (psInst->uOpcode == USEASM_OP_LOCK)
	{
		puInst[0] = EURASIA_USE0_LOCKRELEASE_ACTION_LOCK;
	}
	else
	{
		puInst[0] = EURASIA_USE0_LOCKRELEASE_ACTION_RELEASE;
	}
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_LOCKRELEASE << EURASIA_USE1_OTHER_OP2_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_LOCKRELEASE_NOSCHED : 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The argument to %s must be an immediate",
					   OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "No flags are valid for the argument to %s",
					   OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, 
					   "An immediate value can't be indexed"));
	}
	if (psInst->asArg[0].uNumber >= NumberOfMutexesSupported(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Only %d mutex(es) are supported on this processor",
					   NumberOfMutexesSupported(psTarget)));
	}
	puInst[0] |= (psInst->asArg[0].uNumber << SGX545_USE0_LOCKRELEASE_MUTEXNUM_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodeLDRSTRGlobalRegisterNumber

 PURPOSE	: Encode the global register number argument to a LDR or STR instruction.

 PARAMETERS	: puInst			- Written with the encoded instruction.
			  uRegNum			- Register number.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLDRSTRGlobalRegisterNumber(PCSGX_CORE_DESC		psTarget,
												 PUSE_INST			psInst,
												 IMG_PUINT32		puInst,
												 IMG_UINT32			uRegNum,
												 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uMaxRegNum;

	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(psTarget);

	/*	
		Get the maximum immediate value supported.
	*/
	uMaxRegNum = EURASIA_USE_LDRSTR_GLOBREG_MAX_NUM;
	#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE)
	if (SupportsLDRSTRExtendedImmediate(psTarget))
	{
		uMaxRegNum = SGXVEC_USE_LDRSTR_GLOBREG_MAX_NUM;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE) */

	/*
		Get the supplied immediate is in range.
	*/
	if (uRegNum > uMaxRegNum)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum immediate value for the %s global register number is #0x%.X", OpcodeName(psInst->uOpcode), uMaxRegNum));
	}

	/*
		Encode the source register bank.
	*/
	puInst[1] |= EURASIA_USE1_S2BEXT;
	puInst[0] |= (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT);

	/*
		Encode the immediate value.
	*/
	puInst[0] |= ((uRegNum >> EURASIA_USE0_LDRSTR_SRC2EXT_INTERNALSHIFT) << EURASIA_USE0_LDRSTR_SRC2EXT_SHIFT) & ~EURASIA_USE0_LDRSTR_SRC2EXT_CLRMSK;
	puInst[0] |= ((uRegNum >> 0) << EURASIA_USE0_SRC2_SHIFT) & ~EURASIA_USE0_SRC2_CLRMSK;
	#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE)
	if (SupportsLDRSTRExtendedImmediate(psTarget))
	{
		puInst[1] |= ((uRegNum >> SGXVEC_USE1_LDRSTR_SRC2EXT2_INTERNALSHIFT) << SGXVEC_USE1_LDRSTR_SRC2EXT2_SHIFT) & ~SGXVEC_USE1_LDRSTR_SRC2EXT2_CLRMSK;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_EXTENDED_IMMEDIATE) */
}

/*****************************************************************************
 FUNCTION	: EncodeLDRSTRInstruction

 PURPOSE	: Encode a LDR or STR instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeLDRSTRInstruction(PCSGX_CORE_DESC		psTarget,
									    PUSE_INST			psInst,
									    IMG_PUINT32			puInst,
									    PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	uValidFlags = USEASM_OPFLAGS1_SKIPINVALID;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT)
	if (SupportsLDRSTRRepeat(psTarget))
	{
		uValidFlags |= (~USEASM_OPFLAGS1_REPEAT_CLRMSK);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT) */
	#if defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE)
	if (SupportsSTRPredicate(psTarget) && psInst->uOpcode == USEASM_OP_STR)
	{
		uValidFlags |= (~USEASM_OPFLAGS1_PRED_CLRMSK);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE) */
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_LDRSTR << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_LDRSTR_NOSCHED : 0);
	puInst[0] = 0;

	#if defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT)
	/*
		Encode repeat count.
	*/
	if (SupportsLDRSTRRepeat(psTarget))
	{
		IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

		if (uRptCount > 0)
		{
			puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_LDRSTR_RCNT_SHIFT);
		}
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_LDRSTR_REPEAT) */

	#if defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE)
	/*
		Encode STR predicate.
	*/
	if (SupportsSTRPredicate(psTarget) && psInst->uOpcode == USEASM_OP_STR)
	{
		puInst[1] |= (EncodePredicate(psContext, psInst, IMG_TRUE /* bShort */) << SGXVEC_USE1_LDRSTR_STR_SPRED_SHIFT);
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_STR_PREDICATE) */

	if (psInst->uOpcode == USEASM_OP_STR)
	{
		puInst[1] |= EURASIA_USE1_LDRSTR_DSEL_STORE;
		CheckArgFlags(psContext, psInst, 0, 0);
		if (psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			EncodeLDRSTRGlobalRegisterNumber(psTarget, psInst, puInst, psInst->asArg[0].uNumber, psContext);
		}
		else
		{
			EncodeSrc2(psContext, psInst, 0, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		}
		CheckArgFlags(psContext, psInst, 1, 0);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		puInst[1] |= EURASIA_USE1_LDRSTR_DSEL_LOAD;
		CheckArgFlags(psContext, psInst, 0, 0);
		if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for ldr can't be indexed"));
		}
		if (psInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
		{
			puInst[1] |= EURASIA_USE1_LDRSTR_DBANK_TEMP;
			if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid temporary register specified (r%d)", psInst->asArg[0].uNumber));
			}
			puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
		}
		else if (psInst->asArg[0].uType == USEASM_REGTYPE_PRIMATTR)
		{
			puInst[1] |= EURASIA_USE1_LDRSTR_DBANK_PRIMATTR;
			if (psInst->asArg[0].uNumber >= EURASIA_USE_REGISTER_NUMBER_MAX)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid primary attribute register specified (pa%d)", psInst->asArg[0].uNumber));
			}
			puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_DST_SHIFT);
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for ldr must be in either temporary or primary attribute banks"));
		}
		CheckArgFlags(psContext, psInst, 1, 0);
		if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			EncodeLDRSTRGlobalRegisterNumber(psTarget, psInst, puInst, psInst->asArg[1].uNumber, psContext);
		}
		else
		{
			EncodeSrc2(psContext, psInst, 1, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		}
		if (psInst->asArg[2].uType != USEASM_REGTYPE_DRC)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to ldr must be a drc register"));
		}
		if (psInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A drc register is not indexable"));
		}
		if (psInst->asArg[2].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a drc register"));
		}
		if (psInst->asArg[2].uNumber >= EURASIA_USE_DRC_BANK_SIZE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum drc register number is %u", EURASIA_USE_DRC_BANK_SIZE - 1));
		}
		puInst[1] |= (psInst->asArg[2].uNumber << EURASIA_USE1_LDRSTR_DRCSEL_SHIFT);
	}
}

/*****************************************************************************
 FUNCTION	: EncodePTOFFPCOEFFInstruction

 PURPOSE	: Encode a PTOFF or PCOEFF instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodePTOFFPCOEFFInstruction(PCSGX_CORE_DESC			psTarget,
											 PUSE_INST				psInst,
											 IMG_PUINT32			puInst,
											 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	uValidFlags = USEASM_OPFLAGS1_END;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_VISTEST_OP2_PTCTRL << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_VISTEST_PTCTRL_NOSCHED : 0);
	puInst[0] = 0;
	if (psInst->uOpcode == USEASM_OP_PCOEFF)
	{
		puInst[1] |= (EURASIA_USE1_VISTEST_PTCTRL_TYPE_PLANE << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT);
		CheckArgFlags(psContext, psInst, 0, 0);
		EncodeSrc0(psContext, psInst, 0, TRUE, puInst + 0, puInst + 1, EURASIA_USE1_VISTEST_PTCTRL_S0BEXT, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 1, 0);
		EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
		CheckArgFlags(psContext, psInst, 2, 0);
		EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	}
	else
	{
		puInst[1] |= (EURASIA_USE1_VISTEST_PTCTRL_TYPE_PTOFF << EURASIA_USE1_VISTEST_PTCTRL_TYPE_SHIFT);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeATST8Instruction

 PURPOSE	: Encode a ATST8 instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeATST8Instruction(PUSE_INST			psInst, 
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext,
									   PCSGX_CORE_DESC		psTarget)
{
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SYNCSTART | USEASM_OPFLAGS1_END |
				USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_PRED_CLRMSK), USEASM_OPFLAGS2_C10, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_VISTEST_OP2_ATST8 << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_VISTEST_ATST8_SYNCS : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0);
	puInst[0] = 0;
	if (psInst->uFlags2 & USEASM_OPFLAGS2_C10)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_ATST8_C10;
	}
	if (psInst->asArg[1].uType != USEASM_REGTYPE_PREDICATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination argument to atst8 must be a predicate register"));
	}
	if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A predicate register is not indexable"));
	}
	if ((psInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_DISABLEWB) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags except disable writeback are valid for a predicate register"));
	}
	if (psInst->asArg[1].uNumber >= EURASIA_USE_PREDICATE_BANK_SIZE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum predicate register number is %u", EURASIA_USE_PREDICATE_BANK_SIZE - 1));
	}
	puInst[1] |= (psInst->asArg[1].uNumber << EURASIA_USE1_VISTEST_ATST8_PDST_SHIFT);
	if (!(psInst->asArg[1].uFlags & USEASM_ARGFLAGS_DISABLEWB))
	{
		puInst[1] |= EURASIA_USE1_VISTEST_ATST8_PDSTENABLE;
	}
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext, psInst, FALSE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc0(psContext, psInst, 2, TRUE, puInst + 0, puInst + 1, EURASIA_USE1_VISTEST_ATST8_S0BEXT, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 3, 0);
	EncodeSrc1(psContext, psInst, 3, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 4, 0);
	EncodeSrc2(psContext, psInst, 4, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	/* Encode the twosided flag. */
	if (psInst->asArg[5].uType != USEASM_REGTYPE_INTSRCSEL ||
		(psInst->asArg[5].uNumber != USEASM_INTSRCSEL_TWOSIDED &&
		 psInst->asArg[5].uNumber != USEASM_INTSRCSEL_NONE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sixth argument to atst8 must be one of twosided or none"));
	}
	if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector value is not indexable"));
	}
	if (psInst->asArg[5].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an integer source selector"));
	}
	if (psInst->asArg[5].uNumber == USEASM_INTSRCSEL_TWOSIDED)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_ATST8_TWOSIDED;
	}
	/* Encode the feedback flag. */
	if (psInst->asArg[6].uType != USEASM_REGTYPE_INTSRCSEL ||
		(psInst->asArg[6].uNumber != USEASM_INTSRCSEL_OPTDWD &&
		 psInst->asArg[6].uNumber != USEASM_INTSRCSEL_FEEDBACK &&
		 psInst->asArg[6].uNumber != USEASM_INTSRCSEL_NOFEEDBACK))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The seventh argument to atst8 must be one of optdwd, nfb or fb"));
	}
	if (psInst->asArg[6].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector value is not indexable"));
	}
	if (psInst->asArg[6].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an integer source selector"));
	}
	if (psInst->asArg[6].uNumber == USEASM_INTSRCSEL_OPTDWD)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_ATST8_OPTDWD;
	}
	else if (psInst->asArg[6].uNumber == USEASM_INTSRCSEL_NOFEEDBACK)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_ATST8_DISABLEFEEDBACK;
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDEPTHFInstruction

 PURPOSE	: Encode a DEPTHF instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDEPTHFInstruction(PCSGX_CORE_DESC		psTarget,
										PUSE_INST			psInst, 
									    IMG_PUINT32			puInst,
									    PUSEASM_CONTEXT		psContext)
{
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SYNCSTART | USEASM_OPFLAGS1_END |
				USEASM_OPFLAGS1_NOSCHED | (~USEASM_OPFLAGS1_PRED_CLRMSK), 0, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, TRUE) << EURASIA_USE1_VISTEST_DEPTHF_SPRED_SHIFT) |
				(EURASIA_USE1_VISTEST_OP2_DEPTHF << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_VISTEST << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_VISTEST_ATST8_SYNCS : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0);
	puInst[0] = 0;
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeSrc0(psContext, psInst, 0, TRUE, puInst + 0, puInst + 1, EURASIA_USE1_VISTEST_DEPTHF_S0BEXT, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 1, 0);
	EncodeSrc1(psContext, psInst, 1, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc2(psContext, psInst, 2, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);
	/* Encode the twosided flag. */
	if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL ||
		(psInst->asArg[3].uNumber != USEASM_INTSRCSEL_TWOSIDED &&
		 psInst->asArg[3].uNumber != USEASM_INTSRCSEL_NONE))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to depthf must be one of twosided or none"));
	}
	if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector value is not indexable"));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an integer source selector"));
	}
	if (psInst->asArg[3].uNumber == USEASM_INTSRCSEL_TWOSIDED)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_DEPTHF_TWOSIDED;
	}
	/* Encode the feedback flag. */
	if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL ||
		(psInst->asArg[4].uNumber != USEASM_INTSRCSEL_OPTDWD &&
		 psInst->asArg[4].uNumber != USEASM_INTSRCSEL_FEEDBACK))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to depthf must be one of optdwd or fb"));
	}
	if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An integer source selector value is not indexable"));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an integer source selector"));
	}
	if (psInst->asArg[4].uNumber == USEASM_INTSRCSEL_OPTDWD)
	{
		puInst[1] |= EURASIA_USE1_VISTEST_DEPTHF_OPTDWD;
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSETMInstruction

 PURPOSE	: Encode a SETM instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSETMInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst,
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(SGX545_USE1_OTHER_OP2_SETM << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGX545_USE1_SETM_NOSCHED : 0);
	puInst[0] = 0;

	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The argument to setm must be an immediate value"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}
	if (psInst->asArg[0].uNumber >= NumberOfMonitorsSupported(psTarget))
	{
		if (NumberOfMonitorsSupported(psTarget) == 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Monitors aren't supported on this processor"));
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum monitor supported on this processor is %d", NumberOfMonitorsSupported(psTarget) - 1));
		}
	}
	puInst[0] |= (psInst->asArg[0].uNumber << SGX545_USE0_SETM_MONITORNUM_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodePaddingInstruction

 PURPOSE	: Encode a PADDING instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodePaddingInstruction(PCSGX_CORE_DESC			psTarget,
										 PUSE_INST				psInst, 
										 IMG_PUINT32			puInst,
										 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags3;

	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER)
	if (SupportsNopTrigger(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_TRIGGER;
	}
	#else /* defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER) */
	PVR_UNREFERENCED_PARAMETER(psTarget);
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER) */

	/* Check instruction validity. */
	CheckFlags(psContext, 
			  psInst, 
			  USEASM_OPFLAGS1_SYNCSTART | USEASM_OPFLAGS1_END | USEASM_OPFLAGS1_NOSCHED, 
			  USEASM_OPFLAGS2_TOGGLEOUTFILES, 
			  uValidFlags3);
	/* Encode opcode, predicate and instruction flags */
	puInst[0] = ((psInst->uFlags2 & USEASM_OPFLAGS2_TOGGLEOUTFILES) ? EURASIA_USE0_FLOWCTRL_NOP_TOGGLEOUTFILES : 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_FLOWCTRL_NOP_SYNCS : 0) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0) |
				(EURASIA_USE1_FLOWCTRL_OP2_NOP << EURASIA_USE1_FLOWCTRL_OP2_SHIFT);

	#if defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER)
	/*
		Encode SGX545 debug bus trigger.
	*/
	if (SupportsNopTrigger(psTarget) && (psInst->uFlags3 & USEASM_OPFLAGS3_TRIGGER))
	{
		puInst[0] |= EURASIA_USE0_FLOWCTRL_NOP_TRIGGER;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_NOPTRIGGER) */
}

/*****************************************************************************
 FUNCTION	: EncodeWOPInstruction

 PURPOSE	: Encode a WOP instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeWOPInstruction(PCSGX_CORE_DESC			psTarget,
									 PUSE_INST				psInst,
									 IMG_PUINT32			puInst,
									 PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	IMG_UINT32	uValidFlags3;
	IMG_UINT32	uPartitionBiasMax;

	uValidFlags = 0;
	if (IsEnhancedNoSched(psTarget))
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	uValidFlags3 = 0;
	#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		uValidFlags3 |= USEASM_OPFLAGS3_TWOPART;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	CheckFlags(psContext, psInst, uValidFlags, 0, uValidFlags3);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER_OP2_WOP << EURASIA_USE1_OTHER_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_WOP_NOSCHED : 0);
	puInst[0] = 0;

	if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The argument to wop must be an immediate value"));
	}
	if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value is not indexable"));
	}
	if (psInst->asArg[0].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for an immediate value"));
	}

#if defined(SUPPORT_SGX_FEATURE_VCB)
	if (SupportsVCB(psTarget))
	{
		if (psInst->uFlags3 & USEASM_OPFLAGS3_TWOPART)
		{
			puInst[0] |= SGX545_USE0_WOP_TWOPART;
		}
	
		uPartitionBiasMax = SGX545_USE0_WOP_PTNB_MAX;
	}
	else
#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
	{
		uPartitionBiasMax = EURASIA_USE0_WOP_PTNB_MAX;
	}

	if (psInst->asArg[0].uNumber > uPartitionBiasMax)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum immediate value for wop is %d", uPartitionBiasMax));
	}
	puInst[0] |= (psInst->asArg[0].uNumber << EURASIA_USE0_WOP_PTNB_SHIFT);
}

#if defined(SUPPORT_SGX_FEATURE_USE_SPRVV)
static IMG_VOID EncodeSPRVVInstruction(PCSGX_CORE_DESC		psTarget,
									   PUSE_INST			psInst,
									   IMG_PUINT32			puInst,
									   PUSEASM_CONTEXT		psContext)
/*****************************************************************************
 FUNCTION	: EncodeSPRVVInstruction

 PURPOSE	: Encode an SPRVV instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!SupportsSPRVV(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The sprvv instruction isn't supported on this processor"));
	}

	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_END, 0, 0);

	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER2_OP2_PHAS << EURASIA_USE1_OTHER2_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				EURASIA_USE1_SPECIAL_OPCAT_EXTRA |
				EURASIA_USE_OTHER2_PHAS_TYPEEXT_SPRVV |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_OTHER2_PHAS_END : 0);
	puInst[0] = 0;
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_SPRVV) */

#if defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES)
/*****************************************************************************
 FUNCTION	: EncodePHASInstruction

 PURPOSE	: Encode a PHAS instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodePHASInstruction(PCSGX_CORE_DESC		psTarget,
									  PUSE_INST				psInst,
									  IMG_PUINT32			puInst,
									  PUSEASM_CONTEXT		psContext)
{
	IMG_UINT32	uValidFlags;
	if (!HasUnlimitedPhases(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The phas instruction isn't supported on this processor"));
	}
	uValidFlags = USEASM_OPFLAGS1_END;
	if (psInst->uOpcode == USEASM_OP_PHAS)
	{
		uValidFlags |= USEASM_OPFLAGS1_NOSCHED;
	}
	CheckFlags(psContext, psInst, uValidFlags, 0, 0);
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_OTHER2_OP2_PHAS << EURASIA_USE1_OTHER2_OP2_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				EURASIA_USE1_SPECIAL_OPCAT_EXTRA |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_OTHER2_PHAS_END : 0);
	puInst[0] = 0;

	if (psInst->uOpcode == USEASM_OP_PHASIMM)
	{
		IMG_UINT32	uExeAddr, uTempCount, uRate, uWaitCond, uMode;
		IMG_UINT32	uPCMax = (1 << NumberOfProgramCounterBits(psTarget)) - 1;

		puInst[1] |= EURASIA_USE1_OTHER2_PHAS_IMM;

		/*
			Encode the execution address of the next phase.
		*/
		if (psInst->asArg[0].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The first argument to phas must be an immediate value"));
		}
		if (psInst->asArg[0].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the first argument to phas"));
		}
		if (psInst->asArg[0].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value can't be indexed"));
		}
		uExeAddr = psInst->asArg[0].uNumber;
		if (uExeAddr > uPCMax)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum execution address of the next phase is 0x%.X", uPCMax));
		}
		puInst[0] |= uExeAddr;

		/*
			Encode the number of temporaries to allocate for the next phase.
		*/
		if (psInst->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to phas must be an immediate value"));
		}
		if (psInst->asArg[1].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to phas"));
		}
		if (psInst->asArg[1].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value can't be indexed"));
		}
		uTempCount = psInst->asArg[1].uNumber;
		if (uTempCount & (EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGN - 1))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The number of temporary registers must be a multiple of %d", EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGN));
		}
		if (uTempCount > EURASIA_USE_OTHER2_PHAS_TCOUNT_MAX)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum number of temporary registers is %d", EURASIA_USE_OTHER2_PHAS_TCOUNT_MAX));
		}
		puInst[1] |= 
			((uTempCount >> EURASIA_USE_OTHER2_PHAS_TCOUNT_ALIGNSHIFT) << EURASIA_USE1_OTHER2_PHAS_IMM_TCOUNT_SHIFT);

		/*
			Encode the execution rate for the next phase.
		*/
		if (psInst->asArg[2].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The third argument to phas must be a source selector"));
		}
		if (psInst->asArg[2].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the third argument to phas"));
		}
		if (psInst->asArg[2].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value can't be indexed"));
		}
		switch (psInst->asArg[2].uNumber)
		{
			case USEASM_INTSRCSEL_PIXEL: uRate = EURASIA_USE_OTHER2_PHAS_RATE_PIXEL; break;
			case USEASM_INTSRCSEL_SAMPLE: uRate = EURASIA_USE_OTHER2_PHAS_RATE_SAMPLE; break;
			case USEASM_INTSRCSEL_SELECTIVE: uRate = EURASIA_USE_OTHER2_PHAS_RATE_SELECTIVE; break;
			default: 
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for the third argument to phas are pixel, sample or selective")); 
				uRate = 0; 
				break;
			}
		}
		puInst[1] |= (uRate << EURASIA_USE1_OTHER2_PHAS_IMM_RATE_SHIFT);

		/*
			Encode the wait condition for the start of the next phase.
		*/
		if (psInst->asArg[3].uType == USEASM_REGTYPE_ADDRESSMODE &&
			psInst->asArg[3].uNumber == EURASIA_USE1_MOECTRL_ADDRESSMODE_NONE)
		{
			puInst[1] |= (EURASIA_USE_OTHER2_PHAS_WAITCOND_NONE << EURASIA_USE1_OTHER2_PHAS_IMM_WAITCOND_SHIFT);
		}
		else
		{
			if (psInst->asArg[3].uType != USEASM_REGTYPE_INTSRCSEL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to phas must be a source selector"));
			}
			if (psInst->asArg[3].uFlags != 0)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fourth argument to phas"));
			}
			if (psInst->asArg[3].uIndex != USEREG_INDEX_NONE)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value can't be indexed"));
			}
			switch (psInst->asArg[3].uNumber)
			{
				case USEASM_INTSRCSEL_PT: uWaitCond = EURASIA_USE_OTHER2_PHAS_WAITCOND_PT; break;
				case USEASM_INTSRCSEL_VCULL: uWaitCond = EURASIA_USE_OTHER2_PHAS_WAITCOND_VCULL; break;
				case USEASM_INTSRCSEL_END: uWaitCond = EURASIA_USE_OTHER2_PHAS_WAITCOND_END; break;
				default: 
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for the fourth argument to phas are none, pt, vcull and end")); 
					uWaitCond = 0; 
					break;
				}
			}
			puInst[1] |= (uWaitCond << EURASIA_USE1_OTHER2_PHAS_IMM_WAITCOND_SHIFT);
		}

		/*
			Encode the execution mode for the next phase.
		*/
		if (psInst->asArg[4].uType != USEASM_REGTYPE_INTSRCSEL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to phas must be a source selector"));
		}
		if (psInst->asArg[4].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to phas"));
		}
		if (psInst->asArg[4].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "An immediate value can't be indexed"));
		}
		switch (psInst->asArg[4].uNumber)
		{
			case USEASM_INTSRCSEL_PARALLEL: uMode = EURASIA_USE_OTHER2_PHAS_MODE_PARALLEL; break;
			case USEASM_INTSRCSEL_PERINSTANCE: uMode = EURASIA_USE_OTHER2_PHAS_MODE_PERINSTANCE; break;
			default: 
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for the fifth argument to phas are parallel or perinstance")); 
				uMode = 0; 
				break;
			}
		}
		puInst[1] |= (uMode << EURASIA_USE1_OTHER2_PHAS_IMM_MODE_SHIFT);
	}
	else
	{
		if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
		{
			puInst[1] |= EURASIA_USE1_OTHER2_PHAS_NONIMM_NOSCHED;
		}
		EncodeSrc1(psContext, psInst, 0, IMG_TRUE, EURASIA_USE1_S1BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
		EncodeSrc2(psContext, psInst, 1, IMG_TRUE, EURASIA_USE1_S2BEXT, IMG_FALSE, puInst + 0, puInst + 1, IMG_FALSE, IMG_FALSE, 0, psTarget);
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES) */

#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
/*****************************************************************************
 FUNCTION	: EncodeMOVMSKInstruction

 PURPOSE	: Encode a MOVMSK instruction.

 PARAMETERS	: psTarget			- Compile target
			  psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeMOVMSKInstruction(PCSGX_CORE_DESC			psTarget,
										PUSE_INST				psInst,
										IMG_PUINT32				puInst,
										PUSEASM_CONTEXT			psContext)
{
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32 uMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32 uSrc2Comp;
	IMG_UINT32 uCoverageType;

	if (!SupportsAlphaToCoverage(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "movmsk isn't supported on this processor"));
	}
	/* Check instruction validity. */
	CheckFlags(psContext, psInst, USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_MOVC_FMTI8 | 
				USEASM_OPFLAGS1_SYNCSTART | (~USEASM_OPFLAGS1_MASK_CLRMSK) |
				(~USEASM_OPFLAGS1_REPEAT_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK) | USEASM_OPFLAGS1_NOSCHED |
				USEASM_OPFLAGS1_MOVC_FMTMASK | USEASM_OPFLAGS1_MOVC_FMTF16 | USEASM_OPFLAGS1_END,
				USEASM_OPFLAGS2_MOVC_FMTF32,
				USEASM_OPFLAGS3_SAMPLEMASK);
	/* Encode opcode, predicate and instruction flags. */		
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_MOVC << EURASIA_USE1_OP_SHIFT) |
		 		(EncodePredicate(psContext, psInst, FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? EURASIA_USE1_NOSCHED : 0);
	/* Encode repeat count and mask */
	if (uRptCount > 0)
	{
		puInst[1] |= ((uRptCount - 1) << EURASIA_USE1_RMSKCNT_SHIFT) | EURASIA_USE1_RCNTSEL;
	}
	else
	{
		puInst[1] |= (uMask << EURASIA_USE1_RMSKCNT_SHIFT);
	}
	
	/*
		Encode destination.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->asArg[0].uType != USEASM_REGTYPE_OUTPUT)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The destination for movmsk must be an output register"));
	}
	EncodeDest(psContext, psInst, TRUE, puInst + 0, puInst + 1, IMG_FALSE, 0, psTarget);

	/*
		Encode source 0.
	*/
	if (psInst->uFlags3 & USEASM_OPFLAGS3_SAMPLEMASK)
	{
		uCoverageType = SGX545_USE1_MOVC_TSTDTYPE_COVERAGE_SMASK;

		CheckArgFlags(psContext, psInst, 1, 0);
		EncodeSrc0(psContext, 
				   psInst, 
				   1 /* uArg */, 
				   IMG_FALSE /* bAllowExtended */, 
				   puInst + 0, 
				   puInst + 1, 
				   0 /* uBankExtension */, 
				   IMG_FALSE /* bFmtSelect */, 
				   0 /* uFmtFlag */, 
				   psTarget);
	}
	else
	{
		uCoverageType = SGX545_USE1_MOVC_TSTDTYPE_COVERAGE;

		if (
				!(
					psInst->asArg[1].uType == USEASM_REGTYPE_INTSRCSEL &&
					psInst->asArg[1].uNumber == USEASM_INTSRCSEL_NONE
				 )
		   )
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Source 0 to MOVMSK is only supported with the SMSK flag"));
		}
	}

	/*
		Encode source 1.
	*/
	CheckArgFlags(psContext, psInst, 2, 0);
	EncodeSrc1(psContext, psInst, 2, TRUE, EURASIA_USE1_S1BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);

	/*
		Encode source 2.
	*/
	CheckArgFlags(psContext, psInst, 3, (~USEASM_ARGFLAGS_COMP_CLRMSK));
	EncodeSrc2(psContext, psInst, 3, TRUE, EURASIA_USE1_S2BEXT, FALSE, puInst + 0, puInst + 1, FALSE, IMG_FALSE, 0, psTarget);

	/* Encode source 2 component select */
	uSrc2Comp = (psInst->asArg[3].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	puInst[1] |= (uSrc2Comp << SGX545_USE1_MOVC_S2SCSEL_SHIFT);

	/* Encode the instruction modes. */
	puInst[1] |= (uCoverageType << EURASIA_USE1_MOVC_TSTDTYPE_SHIFT);
	if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTMASK)
	{
		puInst[1] |= (SGX545_USE1_MOVC_COVERAGETYPE_STDMASK << EURASIA_USE1_MOVC_TESTCC_SHIFT);
	}
	else if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
	{
		puInst[1] |= (SGX545_USE1_MOVC_COVERAGETYPE_STDU8 << EURASIA_USE1_MOVC_TESTCC_SHIFT);
	}
	else if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTF16)
	{
		puInst[1] |= (SGX545_USE1_MOVC_COVERAGETYPE_EXTF16 << EURASIA_USE1_MOVC_TESTCC_SHIFT) |
					 SGX545_USE1_MOVC_COVERAGETYPEEXT;
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTF32)
	{
		puInst[1] |= (SGX545_USE1_MOVC_COVERAGETYPE_EXTF32 << EURASIA_USE1_MOVC_TESTCC_SHIFT) |
					 SGX545_USE1_MOVC_COVERAGETYPEEXT;
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A movmsk instruction must specify the data type (.mask, .u8, .flt16, .flt)"));
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */

#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)	
#define USEASM_SWIZZLE3(X, Y, Z)		((X << (USEASM_SWIZZLE_FIELD_SIZE * 0)) | \
										 (Y << (USEASM_SWIZZLE_FIELD_SIZE * 1)) | \
										 (Z << (USEASM_SWIZZLE_FIELD_SIZE * 2)))

#define VEC3SWIZ_CASE(X, Y, Z)	\
	case USEASM_SWIZZLE3(USEASM_SWIZZLE_SEL_##X, USEASM_SWIZZLE_SEL_##Y, USEASM_SWIZZLE_SEL_##Z): \
	{	\
		return SGXVEC_USE_VEC3_SWIZ_STD_##X##Y##Z; \
	}	
#define VEC3SWIZ_EXTCASE(X, Y, Z)	\
	case USEASM_SWIZZLE3(USEASM_SWIZZLE_SEL_##X, USEASM_SWIZZLE_SEL_##Y, USEASM_SWIZZLE_SEL_##Z): \
	{	\
		return SGXVEC_USE_VEC3_SWIZ_EXT_##X##Y##Z; \
	}

/*****************************************************************************
 FUNCTION	: EncodeVEC3Swizzle

 PURPOSE	: Get the vec3 encoding for a swizzle in the useasm format.

 PARAMETERS	: uSwizzle				- The swizzle in the useasm format.
			  bAllowExtended		- TRUE if the extended set of swizzle is
									available.
			  pbEncodedExtFlag		- On return set to TRUE if the encoded swizzle
									was in the extended set.

 RETURNS	: The encoded swizzle or -1 if the swizzle isn't available.
*****************************************************************************/
static IMG_UINT32 EncodeVEC3Swizzle(IMG_UINT32	uSwizzle,
								    IMG_BOOL	bAllowExtended,
								    IMG_PBOOL	pbEncodedExtFlag)
{
	/*
		Mask the top channel.
	*/
	uSwizzle &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * 3));

	/*
		Check for a standard swizzle.
	*/
	if (pbEncodedExtFlag != NULL)
	{
		*pbEncodedExtFlag = IMG_FALSE;
	}
	switch (uSwizzle)
	{
		VEC3SWIZ_CASE(X, X, X)
		VEC3SWIZ_CASE(Y, Y, Y)
		VEC3SWIZ_CASE(Z, Z, Z)
		VEC3SWIZ_CASE(W, W, W)
		VEC3SWIZ_CASE(X, Y, Z)
		VEC3SWIZ_CASE(Y, Z, W)
		VEC3SWIZ_CASE(X, X, Y)
		VEC3SWIZ_CASE(X, Y, X)
		VEC3SWIZ_CASE(Y, Y, X)
		VEC3SWIZ_CASE(Y, Y, Z)
		VEC3SWIZ_CASE(Z, X, Y)
		VEC3SWIZ_CASE(X, Z, Y)
		VEC3SWIZ_CASE(Y, Z, X)
		VEC3SWIZ_CASE(Z, Y, X)
		VEC3SWIZ_CASE(Z, Z, Y)
		VEC3SWIZ_CASE(X, Y, 1)
	}


	/*
		Check for an extended swizzle.
	*/
	if (bAllowExtended)
	{
		*pbEncodedExtFlag = IMG_TRUE;
		switch (uSwizzle)
		{
			VEC3SWIZ_EXTCASE(X, Y, Y)
			VEC3SWIZ_EXTCASE(Y, X, Y)
			VEC3SWIZ_EXTCASE(X, X, Z)
			VEC3SWIZ_EXTCASE(Y, X, X)
			VEC3SWIZ_EXTCASE(X, Y, 0)
			VEC3SWIZ_EXTCASE(X, 1, 0)
			VEC3SWIZ_EXTCASE(0, 0, 0)
			VEC3SWIZ_EXTCASE(1, 1, 1)
			VEC3SWIZ_EXTCASE(2, 2, 2)
			VEC3SWIZ_EXTCASE(X, 0, 0)

			case USEASM_SWIZZLE3(USEASM_SWIZZLE_SEL_HALF, USEASM_SWIZZLE_SEL_HALF, USEASM_SWIZZLE_SEL_HALF):
			{
				return SGXVEC_USE_VEC3_SWIZ_EXT_HALF;
			}
		}
	}

	/*
		This swizzle isn't available on a VEC3 instruction.
	*/
	return USE_UNDEF;
}
#undef VEC3SWIZ_CASE
#undef VEC3SWIZ_EXTCASE

#define VEC4SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		return SGXVEC_USE_VEC4_SWIZ_STD_##X##Y##Z##W; \
	}	
#define VEC4SWIZ_EXTCASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		return SGXVEC_USE_VEC4_SWIZ_EXT_##X##Y##Z##W; \
	}

/*****************************************************************************
 FUNCTION	: EncodeVEC4Swizzle

 PURPOSE	: Get the vec4 encoding for a swizzle in the useasm format.

 PARAMETERS	: uSwizzle				- The swizzle in the useasm format.
			  bAllowExtended		- TRUE if the extended set of swizzle is
									available.
			  pbEncodedExtFlag		- On return set to TRUE if the encoded swizzle
									was in the extended set.

 RETURNS	: The encoded swizzle or -1 if the swizzle isn't available.
*****************************************************************************/
static IMG_UINT32 EncodeVEC4Swizzle(IMG_UINT32	uSwizzle,
								    IMG_BOOL	bAllowExtended,
								    IMG_PBOOL	pbEncodedExtFlag)
{
	/*
		Check for a standard swizzle.
	*/
	if (pbEncodedExtFlag != NULL)
	{
		*pbEncodedExtFlag = IMG_FALSE;
	}
	switch (uSwizzle)
	{
		VEC4SWIZ_CASE(X, X, X, X)
		VEC4SWIZ_CASE(Y, Y, Y, Y)
		VEC4SWIZ_CASE(Z, Z, Z, Z)
		VEC4SWIZ_CASE(W, W, W, W)
		VEC4SWIZ_CASE(X, Y, Z, W)
		VEC4SWIZ_CASE(Y, Z, W, W)
		VEC4SWIZ_CASE(X, Y, Z, Z)
		VEC4SWIZ_CASE(X, X, Y, Z)
		VEC4SWIZ_CASE(X, Y, X, Y)
		VEC4SWIZ_CASE(X, Y, W, Z)
		VEC4SWIZ_CASE(Z, X, Y, W)
		VEC4SWIZ_CASE(Z, W, Z, W)
		VEC4SWIZ_CASE(Y, Z, X, Z)
		VEC4SWIZ_CASE(X, X, Y, Y)
		VEC4SWIZ_CASE(X, Z, W, W)
		VEC4SWIZ_CASE(X, Y, Z, 1)
	}

	/*
		Check for an extended swizzle.
	*/
	if (bAllowExtended)
	{
		*pbEncodedExtFlag = IMG_TRUE;
		switch (uSwizzle)
		{
			VEC4SWIZ_EXTCASE(Y, Z, X, W)
			VEC4SWIZ_EXTCASE(Z, W, X, Y)
			VEC4SWIZ_EXTCASE(X, Z, W, Y)
			VEC4SWIZ_EXTCASE(Y, Y, W, W)
			VEC4SWIZ_EXTCASE(W, Y, Z, W)
			VEC4SWIZ_EXTCASE(W, Z, W, Z)
			VEC4SWIZ_EXTCASE(X, Y, Z, X)
			VEC4SWIZ_EXTCASE(Z, Z, W, W)
			VEC4SWIZ_EXTCASE(X, W, Z, X)
			VEC4SWIZ_EXTCASE(Y, Y, Y, X)
			VEC4SWIZ_EXTCASE(Y, Y, Y, Z)
			VEC4SWIZ_EXTCASE(X, Z, Y, W)
			VEC4SWIZ_EXTCASE(X, X, X, Y)
			VEC4SWIZ_EXTCASE(Z, Y, X, W)
			VEC4SWIZ_EXTCASE(Y, Y, Z, Z)
			VEC4SWIZ_EXTCASE(Z, Z, Z, Y)
		}
	}

	/*
		This swizzle isn't available on a VEC3 instruction.
	*/
	return USE_UNDEF;
}
#undef VEC4SWIZ_CASE
#undef VEC4SWIZ_EXTCASE

typedef enum
{
	DATA_FORMAT_F32,
	DATA_FORMAT_F16,
	DATA_FORMAT_C10,
} DATA_FORMAT;

static
IMG_UINT32 EncodeVecDestinationWriteMask(PUSEASM_CONTEXT	psContext,
										 PUSE_INST			psInst,
										 PUSE_REGISTER		psDest,
										 IMG_UINT32			uInputWriteMask,
										 DATA_FORMAT		eDestFormat,
										 IMG_BOOL			bC10F16MasksUnrestricted)
/*****************************************************************************
 FUNCTION	: EncodeVecDestinationWriteMask

 PURPOSE	: Convert the destination write mask to a vector instruction to
			  the hardware format.

 PARAMETERS	: psContext				- USEASM context.
			  psInst				- Input instruction.
			  psDest				- Destination register.
			  uInputWriteMask		- Write mask in the destination format.
			  eDestFormat			- Format of the destination.
			  bC10F16MasksUnrestricted	
									- TRUE if an C10/F16 destination write mask
									is unrestricted.

 RETURNS	: The encoded mask.
*****************************************************************************/
{
	IMG_UINT32	uEncodedMask;

	PVR_UNREFERENCED_PARAMETER(psInst);

	/*
		Destination write masks on C10/F16 instructions can only be unrestricted where
		the destination register bank is also valid as a source 0 register bank.
	*/
	if (!IsValidSrc0Bank(psDest, NULL, NULL))
	{
		bC10F16MasksUnrestricted = IMG_FALSE;
	}

	/*
		For a GPI destination or an F16 destination on some instructions all channels
		can be individually masked.
	*/
	if (psDest->uType == USEASM_REGTYPE_FPINTERNAL || (eDestFormat != DATA_FORMAT_F32 && bC10F16MasksUnrestricted))
	{
		uEncodedMask = 0;
		if (uInputWriteMask & USEREG_MASK_X)
		{
			uEncodedMask |= SGXVEC_USE1_VEC_DMSK_GPI_WRITEX;
		}
		if (uInputWriteMask & USEREG_MASK_Y)
		{
			uEncodedMask |= SGXVEC_USE1_VEC_DMSK_GPI_WRITEY;
		}
		if (uInputWriteMask & USEREG_MASK_Z)
		{
			uEncodedMask |= SGXVEC_USE1_VEC_DMSK_GPI_WRITEZ;
		}
		if (uInputWriteMask & USEREG_MASK_W)
		{
			uEncodedMask |= SGXVEC_USE1_VEC_DMSK_GPI_WRITEW;
		}
	}
	else 
	{
		if (eDestFormat == DATA_FORMAT_F32)
		{
			/*
				For an F32 format unified store destination only the X and Y channels
				can be written.
			*/
			if (uInputWriteMask & (USEREG_MASK_Z | USEREG_MASK_W))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction can only write either X "
					"or Y or X and Y in a unified store destination", OpcodeName(psInst->uOpcode)));
			}
	
			uEncodedMask = 0;
			if (uInputWriteMask & USEREG_MASK_X)
			{
				uEncodedMask |= SGXVEC_USE1_VEC_DMSK_F32US_WRITEX;
			}
			if (uInputWriteMask & USEREG_MASK_Y)
			{
				uEncodedMask |= SGXVEC_USE1_VEC_DMSK_F32US_WRITEY;
			}
		}
		else if (eDestFormat == DATA_FORMAT_C10)
		{
			/*
				For a C10 format destination write-masking isn't supported at all.
			*/
			if (uInputWriteMask != USEREG_MASK_XYZW)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "A destination write-mask isn't supported on "
					"the %s instruction with a C10 format destination.", OpcodeName(psInst->uOpcode)));
			}
			uEncodedMask = SGXVEC_USE1_VEC_DMSK_C10US_WRITEXYZW;
		}
		else
		{
			/*
				For an F16 format unified destination the X and Y channels can only be both written
				or both not written. Similarly for the Z and W channels.
			*/
			IMG_UINT32	uXYInputWriteMask;
			IMG_UINT32	uZWInputWriteMask;

			assert(eDestFormat == DATA_FORMAT_F16);

			uXYInputWriteMask = uInputWriteMask & (USEREG_MASK_X | USEREG_MASK_Y);
			uZWInputWriteMask = uInputWriteMask & (USEREG_MASK_Z | USEREG_MASK_W);

			uEncodedMask = 0;
			if (uXYInputWriteMask != 0)
			{
				if (uXYInputWriteMask != (USEREG_MASK_X | USEREG_MASK_Y))
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction with a unified store destination "
						"can't write-mask the X and Y channels individually", OpcodeName(psInst->uOpcode)));
				}
				uEncodedMask |= SGXVEC_USE1_VEC_DMSK_F16US_WRITEXY;
			}
			if (uZWInputWriteMask != 0)
			{
				if (uZWInputWriteMask != (USEREG_MASK_Z | USEREG_MASK_W))
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction with a unified store destination "
						"can't write-mask the Z and W channels individually", OpcodeName(psInst->uOpcode)));
				}
				uEncodedMask |= SGXVEC_USE1_VEC_DMSK_F16US_WRITEZW;
			}
		}
	}

	return uEncodedMask;
}

/*****************************************************************************
 FUNCTION	: EncodeSingleIssueVectorInstructionIncrementMode

 PURPOSE	: Encode the increment mode for a single issue vector instruction.

 PARAMETERS	: psInst			- Input instruction.
			  uArg				- Input argument which contains the increment
								mode.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSingleIssueVectorInstructionIncrementMode(PUSE_INST			psInst,
																IMG_UINT32			uArg,
																IMG_PUINT32			puInst,
																PUSEASM_CONTEXT		psContext)
{
	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_INTSRCSEL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be an increment mode",
					   uArg, OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction can't have an index",
					   uArg, OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "No flags are valid for argument %d to a %s instruction",
					   uArg, OpcodeName(psInst->uOpcode)));
	}
	switch (psInst->asArg[uArg].uNumber)
	{
		case USEASM_INTSRCSEL_INCREMENTUS:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_INCCTL_USONLY << SGXVEC_USE1_SVEC_INCCTL_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_INCREMENTGPI:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_INCCTL_GPIONLY << SGXVEC_USE1_SVEC_INCCTL_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_INCREMENTBOTH:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_INCCTL_BOTH << SGXVEC_USE1_SVEC_INCCTL_SHIFT);
			break;
		}
		case USEASM_INTSRCSEL_INCREMENTMOE:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_INCCTL_MOE << SGXVEC_USE1_SVEC_INCCTL_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "Only incuss, incgpi, incboth and moe are valid for "
						   "argument %d to a %s instruction",
						   uArg, OpcodeName(psInst->uOpcode)));
			break;
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeVEC34Swizzle

 PURPOSE	: Get the encoding for a swizzle argument to vector instruction.

 PARAMETERS	: psInst			- Input instruction.
			  uArg				- Input argument which contains the swizzle.
			  bVec4				- TRUE if the instruction is vec4.
			  pbExtendedSwizzleFlag
								- Return TRUE if the swizzle is in the
								extended set.

 RETURNS	: The encoding for the swizzle.
*****************************************************************************/
static IMG_UINT32 EncodeVEC34Swizzle(PUSE_INST			psInst,
								     IMG_UINT32			uArg,
									 IMG_BOOL			bVec4,
									 IMG_BOOL			bExtendedSwizzle,
								     IMG_PBOOL			pbExtendedSwizzleFlag,
									 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;

	if (!CheckSwizzleArgument(psInst, uArg, psContext))
	{
		return USE_UNDEF;
	}
	if (bVec4)
	{
		uEncodedSwizzle = EncodeVEC4Swizzle(psInst->asArg[uArg].uNumber, bExtendedSwizzle, pbExtendedSwizzleFlag);
	}
	else
	{
		uEncodedSwizzle = EncodeVEC3Swizzle(psInst->asArg[uArg].uNumber, bExtendedSwizzle, pbExtendedSwizzleFlag);
	}
	if (uEncodedSwizzle == USE_UNDEF)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "This swizzle isn't available on a %s instruction",
					   OpcodeName(psInst->uOpcode)));
	}
	return uEncodedSwizzle;
}

/*****************************************************************************
 FUNCTION	: EncodeSingleIssueVectorInstructionGPISource 

 PURPOSE	: Encode a GPI source to a single issue vector instruction.

 PARAMETERS	: psInst			- Input instruction.
			  uInArg			- Input argument to encode from.
			  uOutArg			- Instruction argument to encode.
			  puInst			- Written with the encoding argument.
			  bVec4				- TRUE if the instruction is vec4.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSingleIssueVectorInstructionGPISource(PUSE_INST		psInst,
														    IMG_UINT32		uInArg,
															IMG_UINT32		uOutArg,
														    IMG_PUINT32		puInst,
														    IMG_BOOL		bVec4,
															IMG_BOOL		bVMAD,
														    PUSEASM_CONTEXT	psContext,
															PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_BOOL	bAllowExtendedSwizzles;
	IMG_BOOL	bExtendedSwizzleFlag;
	IMG_UINT32	uNumber;

	/*
		Check only supported argument flags are used.
	*/
	CheckArgFlags(psContext, psInst, uInArg, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);

	/*
		Check source bank.
	*/
	if (psInst->asArg[uInArg].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a GPI register",
					   uInArg, OpcodeName(psInst->uOpcode)));
	}

	/*
		Encode source number.
	*/
	uNumber = psInst->asArg[uInArg].uNumber;
	if (uNumber >= NumberOfInternalRegisters(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "The maximum register number for argument %d is %d",
					   uInArg, NumberOfInternalRegisters(psTarget) - 1));
	}
	if (uOutArg == 0)
	{
		puInst[0] |= (uNumber << SGXVEC_USE0_SVEC_GPI0NUM_SHIFT);
	}
	else
	{
		puInst[0] |= (uNumber << SGXVEC_USE0_SVEC_VMAD34_GPI1NUM_SHIFT);
	}

	/*
		Encode absolute source modifier.
	*/
	if (uOutArg == 0)
	{
		if (psInst->asArg[uInArg].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
		{
			puInst[1] |= SGXVEC_USE1_SVEC_GPI0ABS;
		}
	}
	else
	{
		if (psInst->asArg[uInArg].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
		{
			puInst[1] |= SGXVEC_USE1_SVEC_VMAD34_GPI1ABS;
		}
	}

	/*
		Encode source swizzle.
	*/
	bAllowExtendedSwizzles = IMG_FALSE;
	if (bVMAD)
	{
		bAllowExtendedSwizzles = IMG_TRUE;
	}
	uEncodedSwizzle = EncodeVEC34Swizzle(psInst,
										 uInArg + 1, 
										 bVec4, 
										 bAllowExtendedSwizzles, 
										 &bExtendedSwizzleFlag, 
										 psContext);
	if (uOutArg == 0)
	{
		puInst[0] |= (uEncodedSwizzle << SGXVEC_USE0_SVEC_GPI0SWIZ_SHIFT);

		if (bExtendedSwizzleFlag)
		{
			puInst[1] |= SGXVEC_USE1_SVEC_VMAD34_GPI0SWIZEXT;
		}
	}
	else
	{
		puInst[0] |= (uEncodedSwizzle << SGXVEC_USE0_SVEC_VMAD34_GPI1SWIZ_SHIFT);
		if (bExtendedSwizzleFlag)
		{
			puInst[1] |= SGXVEC_USE1_SVEC_VMAD34_GPI1SWIZEXT;
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSingleIssueVectorInstructionUSSource

 PURPOSE	: Encode the unified store source to a single issue vector instruction.

 PARAMETERS	: psInst			- Input instruction.
			  uArg				- Input argument to encode from.
			  puInst			- Written with the encoding argument.
			  bVec4				- TRUE if the instruction is vec4.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSingleIssueVectorInstructionUSSource(PUSE_INST		psInst,
														   IMG_UINT32		uArg,
														   IMG_PUINT32		puInst,
														   IMG_BOOL			bVec4,
														   IMG_BOOL			bVMAD,
														   PUSEASM_CONTEXT	psContext,
														   PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32	uEncodedSwizzle;

	/*
		Encode source bank and source number.
	*/	
	EncodeWideUSSourceDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  SGXVEC_USE1_SVEC_USSBEXT,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_SVEC_USNUM_SHIFT,
									  USEASM_HWARG_SOURCE1,
									  uArg,
									  psTarget);

	/*
		Encode source modifier.
	*/
	if (psInst->asArg[uArg].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
	{
		puInst[1] |= SGXVEC_USE1_SVEC_USSABS;
	}

	/*
		Encode source swizzle.
	*/
	if (bVMAD)
	{
		IMG_BOOL	bUseExtendedSwizzle;

		uEncodedSwizzle = EncodeVEC34Swizzle(psInst, 
											 uArg + 1, 
											 bVec4, 
											 IMG_TRUE, /* bExtendedSwizzle */ 
											 &bUseExtendedSwizzle, 
											 psContext);

		puInst[0] |= (uEncodedSwizzle << SGXVEC_USE0_SVEC_VMAD34_USSWIZ_SHIFT);
		if (bUseExtendedSwizzle)
		{
			puInst[0] |= SGXVEC_USE0_SVEC_VMAD34_USSWIZEXT;
		}
	}
	else
	{
		IMG_UINT32	uChan;

		if (!CheckSwizzleArgument(psInst, uArg + 1, psContext))
		{
			return;
		}
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uInputChanSel;
			IMG_UINT32	uHwChanSel;

			uInputChanSel = 
				(psInst->asArg[uArg + 1].uNumber >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;
			switch (uInputChanSel)
			{
				case USEASM_SWIZZLE_SEL_X: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_X; break;
				case USEASM_SWIZZLE_SEL_Y: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_Y; break;
				case USEASM_SWIZZLE_SEL_Z: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_Z; break;
				case USEASM_SWIZZLE_SEL_W: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_W; break;
				case USEASM_SWIZZLE_SEL_0: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_0; break;
				case USEASM_SWIZZLE_SEL_1: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_1; break;
				case USEASM_SWIZZLE_SEL_2: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_2; break;
				case USEASM_SWIZZLE_SEL_HALF: uHwChanSel = SGXVEC_USE_SVEC_USSRC_SWIZSEL_HALF; break;
				default:
				{
					USEASM_ERRMSG((psContext->pvContext, psInst, "Unknown channel selector for channel %d", uChan));
					uHwChanSel = USE_UNDEF;
					break;
				}
			}

			switch (uChan)
			{
				case 0: puInst[0] |= (uHwChanSel << SGXVEC_USE0_SVEC_VDP34_USSWIZX_SHIFT); break;
				case 1: puInst[0] |= (uHwChanSel << SGXVEC_USE0_SVEC_VDP34_USSWIZY_SHIFT); break;
				case 2: puInst[0] |= (uHwChanSel << SGXVEC_USE0_SVEC_VDP34_USSWIZZ_SHIFT); break;
				case 3: puInst[0] |= (uHwChanSel << SGXVEC_USE0_SVEC_VDP34_USSWIZW_SHIFT); break;
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeSingleIssueVectorInstruction

 PURPOSE	: Encode a single issue vector instruction.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded instruction.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeSingleIssueVectorInstruction(PCSGX_CORE_DESC			psTarget,
												   PUSE_INST				psInst,
												   IMG_PUINT32				puInst,
												   PUSEASM_CONTEXT			psContext)
{
	IMG_UINT32	uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32	uWriteMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_BOOL	bVec4;
	IMG_BOOL	bVMAD;
	IMG_BOOL	bNegate;
	IMG_UINT32	uIncrementModeArg;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "vector instructions aren't supported on this core"));
		return;
	}

	/*
		Encode opcode and flags.
	*/
	CheckFlags(psContext, 
			   psInst, 
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_NOSCHED | 
			   USEASM_OPFLAGS1_END | 
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK), 
			   0, 
			   0);
	puInst[0] = 0;
	puInst[1] = (SGXVEC_USE1_OP_SVEC << EURASIA_USE1_OP_SHIFT) |
				(EncodeExtendedVectorPredicate(psContext, psInst) << SGXVEC_USE1_SVEC_EVPRED_SHIFT) |
		 		((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_SVEC_NOSCHED : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0);

	/*
		Encode subopcode.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_VDP3:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_OP2_DP3 << SGXVEC_USE1_SVEC_OP2_SHIFT);
			bVec4 = IMG_FALSE;
			bVMAD = IMG_FALSE;
			break;
		}
		case USEASM_OP_VDP4:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_OP2_DP4 << SGXVEC_USE1_SVEC_OP2_SHIFT);
			bVec4 = IMG_TRUE;
			bVMAD = IMG_FALSE;
			break;
		}
		case USEASM_OP_VMAD3:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_OP2_VMAD3 << SGXVEC_USE1_SVEC_OP2_SHIFT);
			bVec4 = IMG_FALSE;
			bVMAD = IMG_TRUE;
			break;
		}
		case USEASM_OP_VMAD4:
		{
			puInst[1] |= (SGXVEC_USE1_SVEC_OP2_VMAD4 << SGXVEC_USE1_SVEC_OP2_SHIFT);
			bVec4 = IMG_TRUE;
			bVMAD = IMG_TRUE;
			break;
		}
		default: IMG_ABORT();
	}

	/*
		Encode repeat count.
	*/
	if (uRptCount > 0)
	{
		if (uRptCount > SGXVEC_USE1_SVEC_RCNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "The maximum repeat count for %s is %d",
						   OpcodeName(psInst->uOpcode), SGXVEC_USE1_SVEC_RCNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_SVEC_RCNT_SHIFT);
	}

	/*
		Encode destination.
	*/
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_DBEXT,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_SVEC_DST_SHIFT,
								  USEASM_HWARG_DESTINATION,
								  0 /* uArg */,
								  psTarget);

	/*
		Encode destination write-mask
	*/
	puInst[1] |= (uWriteMask << SGXVEC_USE1_SVEC_WMSK_SHIFT);
	if (psInst->uOpcode == USEASM_OP_VDP3 && (uWriteMask & USEREG_MASK_W) != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The VDP3 instruction can't write to the W component of its "
			"destination"));
	}

	/*
		Encode first source.
	*/
	EncodeSingleIssueVectorInstructionUSSource(psInst, 1, puInst, bVec4, bVMAD, psContext, psTarget);

	/*
		Encode second source.
	*/
	EncodeSingleIssueVectorInstructionGPISource(psInst, 3, 0, puInst, bVec4, bVMAD, psContext, psTarget);

	/*
		Encode negate flag.
	*/
	bNegate = IMG_FALSE;
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		bNegate = !bNegate;
	}
	if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		bNegate = !bNegate;
	}
	if (bNegate)
	{
		puInst[1] |= SGXVEC_USE1_SVEC_USSNEG;
	}

	if (psInst->uOpcode == USEASM_OP_VDP3 || psInst->uOpcode == USEASM_OP_VDP4)
	{
		/*
			Encode clip plane mode.
		*/
		if (psInst->asArg[5].uType == USEASM_REGTYPE_CLIPPLANE)
		{
			if (psInst->asArg[5].uNumber > SGXVEC_USE1_SVEC_VDP34_CPLANE_MAXIMUM)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "The maximum clipplane destination for %s is %d",
							   OpcodeName(psInst->uOpcode),
							   SGXVEC_USE1_SVEC_VDP34_CPLANE_MAXIMUM));
			}

			puInst[1] |= SGXVEC_USE1_SVEC_VDP34_CENABLE;
			puInst[1] |= (psInst->asArg[5].uNumber << SGXVEC_USE1_SVEC_VDP34_CPLANE_SHIFT);
		}
		else
		{
			if (
					!(
						psInst->asArg[5].uType == USEASM_REGTYPE_INTSRCSEL &&
						psInst->asArg[5].uNumber == USEASM_INTSRCSEL_NONE
					 )
			   )
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "Argument 5 to %s must be a clipplane or none",
							   OpcodeName(psInst->uOpcode)));
			}
		}
		if (psInst->asArg[5].uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "An indexed register isn't valid for argument 5 to %s",
						   OpcodeName(psInst->uOpcode)));
		}
		if (psInst->asArg[5].uFlags != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "No flags are valid for argument 5 to %s",
						   OpcodeName(psInst->uOpcode)));
		}	

		uIncrementModeArg = 6;
	}
	else
	{
		/*
			Encode the third source.
		*/
		EncodeSingleIssueVectorInstructionGPISource(psInst, 5, 1, puInst, bVec4, bVMAD, psContext, psTarget);

		/*
			Encode the negate flag for the third source.
		*/
		if (psInst->asArg[5].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGXVEC_USE1_SVEC_VMAD34_GPI1NEG;
		}

		uIncrementModeArg = 7;
	}

	/*
		Encode repeat increment mode.
	*/
	EncodeSingleIssueVectorInstructionIncrementMode(psInst, uIncrementModeArg, puInst, psContext);
}

typedef enum
{
	DIVEC_OP1_VMAD3 = 0,
	DIVEC_OP1_VDP3,
	DIVEC_OP1_VMUL3,
	DIVEC_OP1_VADD3,
	DIVEC_OP1_VSSQ3,
	DIVEC_OP1_VMOV3,
	DIVEC_OP1_VMAD4,
	DIVEC_OP1_VDP4,
	DIVEC_OP1_VMUL4,
	DIVEC_OP1_VADD4,
	DIVEC_OP1_VSSQ4,
	DIVEC_OP1_VMOV4,
	DIVEC_OP1_FRSQ,
	DIVEC_OP1_FRCP,
	DIVEC_OP1_FMAD,
	DIVEC_OP1_FADD,
	DIVEC_OP1_FMUL,
	DIVEC_OP1_FSUBFLR,
	DIVEC_OP1_FEXP,
	DIVEC_OP1_FLOG,
	DIVEC_OP1_MAX,
} DIVEC_OP1;

static const struct
{
	IMG_UINT32	uOp1;
	IMG_UINT32	uExt;
	IMG_UINT32	uSrcCount;
	IMG_BOOL	bSrc01Commute;
	IMG_BOOL	bVector;
	IMG_BOOL	bVectorResult;
} g_asDiVecOp1Desc[DIVEC_OP1_MAX] = 
{
	/* DIVEC_OP1_VMAD3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMAD,	/* uOp1 */
		0,								/* uExt */
		3,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VDP3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VDP,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	
	/* DIVEC_OP1_VMUL3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMUL,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	
	/* DIVEC_OP1_VADD3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VADD,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VSSQ3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VSSQ,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VMOV3 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMOV,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VMAD4 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMAD,	/* uOp1 */
		0,								/* uExt */
		3,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VPD4 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VDP,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VMUL4 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMUL,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VADD4 */
	
	{
		SGXVEC_USE1_DVEC_OP1_STD_VADD,	/* uOp1 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VSSQ4 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VSSQ,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_VMOV4 */
	{
		SGXVEC_USE1_DVEC_OP1_STD_VMOV,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FRSQ */
	{
		SGXVEC_USE1_DVEC_OP1_STD_FRSQ,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FRCP */
	{
		SGXVEC_USE1_DVEC_OP1_STD_FRCP,	/* uOp1 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FMAD */
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FMAD,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		3,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FADD */
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FADD,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FMUL */
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FMUL,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FSUBFLR */
	
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FSUBFLR,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FEXP */
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FEXP,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP1_FLOG */
	{
		SGXVEC_USE1_DVEC_OP1_EXT_FLOG,	/* uOp1 */
		SGXVEC_USE1_DVEC3_OP1EXT,		/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bSrc01Commute */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
};

typedef enum
{
	DIVEC_OP2_VDP,
	DIVEC_OP2_VMUL,
	DIVEC_OP2_VADD,
	DIVEC_OP2_VSSQ,
	DIVEC_OP2_VMOV,
	DIVEC_OP2_FRSQ,
	DIVEC_OP2_FRCP,
	DIVEC_OP2_FMAD,
	DIVEC_OP2_FADD,
	DIVEC_OP2_FMUL,
	DIVEC_OP2_FSUBFLR,
	DIVEC_OP2_FEXP,
	DIVEC_OP2_FLOG,
	DIVEC_OP2_MAX,
} DIVEC_OP2;

static const struct
{
	IMG_UINT32		uOp2;
	IMG_UINT32		uExt;
	IMG_UINT32		uSrcCount;
	IMG_BOOL		bVector;
	IMG_BOOL		bVectorResult;
} g_asDiVecOp2Desc[DIVEC_OP2_MAX] = 
{
	/* DIVEC_OP2_VDP */
	{
		SGXVEC_USE0_DVEC_OP2_STD_VDP,	/* uOp2 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_VMUL */
	{
		SGXVEC_USE0_DVEC_OP2_STD_VMUL,	/* uOp2 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP2_VADD */
	{
		SGXVEC_USE0_DVEC_OP2_STD_VADD,	/* uOp2 */
		0,								/* uExt */
		2,								/* uSrcCount */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP2_VSSQ */
	{
		SGXVEC_USE0_DVEC_OP2_STD_VSSQ,	/* uOp2 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_TRUE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_VMOV */
	{
		SGXVEC_USE0_DVEC_OP2_STD_VMOV,	/* uOp2 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_TRUE,						/* bVector */
		IMG_TRUE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FRSQ */
	{
		SGXVEC_USE0_DVEC_OP2_STD_FRSQ,	/* uOp2 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FRCP */
	{
		SGXVEC_USE0_DVEC_OP2_STD_FRCP,	/* uOp2 */
		0,								/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FMAD */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FMAD,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		3,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FADD */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FADD,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FMUL */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FMUL,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FSUBFLR */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FSUBFLR,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		2,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FEXP */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FEXP,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
	/* DIVEC_OP2_FLOG */
	{
		SGXVEC_USE0_DVEC_OP2_EXT_FLOG,	/* uOp2 */
		SGXVEC_USE1_DVEC_OP2EXT,		/* uExt */
		1,								/* uSrcCount */
		IMG_FALSE,						/* bVector */
		IMG_FALSE,						/* bVectorResult */
	},
};


/*
Op		VDP		VMUL	VADD	VSSQ	VMOV	FRSQ	FRCP	FMAD	FADD	FMUL	FFRC	FEXP	FLOG
VMAD3	No		No		No		No		Yes		Yes		Yes		No		No		No		No		Yes		Yes
VDP3	No		No		No		No		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes
VMUL3	No		No		Yes		No		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes
VADD3	No		Yes		No		Yes		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes
VSSQ3	No		No		Yes		No		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes
VMOV3	Yes		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes
VMAD4	No		No		No		No		Yes		Yes		Yes		No		No		No		No		Yes		Yes
VDP4	No		No		No		No		Yes		Yes		Yes		No		Yes		No		Yes		Yes		Yes
VMUL4	No		No		Yes		No		Yes		Yes		Yes		No		Yes		No		Yes		Yes		Yes
VADD4	No		Yes		No		No		Yes		Yes		Yes		No		No		No		No		Yes		Yes
VSSQ4	No		No		Yes		No		Yes		Yes		Yes		No		Yes		No		Yes		Yes		Yes
VMOV4	Yes		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes
FRSQ	Yes		Yes		Yes		Yes		Yes		No		No		Yes		Yes		Yes		Yes		No		No
FRCP	Yes		Yes		Yes		Yes		Yes		No		No		Yes		Yes		Yes		Yes		No		No
FMAD	No		No		No		Yes		Yes		Yes		Yes		No		No		No		No		Yes		Yes
FADD	Yes		Yes		Yes		Yes		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes
FMUL	Yes		Yes		Yes		Yes		Yes		Yes		Yes		No		Yes		Yes		Yes		Yes		Yes
FFRC	Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes		Yes
FEXP	Yes		Yes		Yes		Yes		Yes		No		No		Yes		Yes		Yes		Yes		No		No
FLOG	Yes		Yes		Yes		Yes		Yes		No		No		Yes		Yes		Yes		Yes		No		No
*/

static const IMG_BOOL g_aabValidVectorDualIssue[DIVEC_OP1_MAX][DIVEC_OP2_MAX] = 
{
				/* VDP		VMUL	VADD	VSSQ	VMOV	FRSQ	FRCP	FMAD	FADD	FMUL	FSUBFLR	FEXP	FLOG */
/* VMAD3 */		{	0,		0,		0,		0,		1,		1,		1,		0,		0,		0,		0,		1,		1,	},
/* VDP3 */		{	0,		0,		0,		0,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* VMUL3 */		{	0,		0,		1,		0,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* VADD3 */		{	0,		1,		0,		1,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* VSSQ3 */		{	0,		0,		1,		0,		1,		1,		1,		1,		1,		1,		1,		1,		1,	},
/* VMOV3 */		{	1,		1,		1,		1,		0,		1,		1,		1,		1,		1,		1,		1,		1,	},
/* VMAD4 */		{	0,		0,		0,		0,		1,		1,		1,		0,		0,		0,		0,		1,		1,	},
/* VDP4 */		{	0,		0,		0,		0,		1,		1,		1,		0,		1,		0,		1,		1,		1,	},
/* VMUL4 */		{	0,		0,		1,		0,		1,		1,		1,		0,		1,		0,		1,		1,		1,	},
/* VADD4 */		{	0,		1,		0,		0,		1,		1,		1,		0,		0,		0,		0,		1,		1,	},
/* VSSQ4 */		{	0,		0,		1,		0,		1,		1,		1,		0,		1,		0,		1,		1,		1,	},
/* VMOV4 */		{	1,		1,		1,		1,		0,		1,		1,		1,		1,		1,		1,		1,		1,	},
/* FRSQ */		{	1,		1,		1,		1,		1,		0,		0,		1,		1,		1,		1,		0,		0,	},
/* FRCP */		{	1,		1,		1,		1,		1,		0,		0,		1,		1,		1,		1,		0,		0,	},
/* FMAD */		{	0,		0,		0,		1,		1,		1,		1,		0,		0,		0,		0,		1,		1,	},
/* FADD */		{	1,		1,		1,		1,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* FMUL */		{	1,		1,		1,		1,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* FSUBFLR */	{	1,		1,		1,		1,		1,		1,		1,		0,		1,		1,		1,		1,		1,	},
/* FEXP */		{	1,		1,		1,		1,		1,		0,		0,		1,		1,		1,		1,		0,		0,	},
/* FLOG */		{	1,		1,		1,		1,		1,		0,		0,		1,		1,		1,		1,		0,		0,	},
};

/*****************************************************************************
 FUNCTION	: GetDualIssueOp1

 PURPOSE	: Get the index into the primary opcode table corresponding to
			  a useasm opcode.

 PARAMETERS	: uOpcode				- Input opcode.
			  pbVec3				- Set to TRUE on return if the opcode is
									vec3.
			  pbVec4				- Set to TRUE on return if the opcode is
									vec4.

 RETURNS	: The corresponding index or -1 if the opcode isn't available for
			  the primary operation in a vector dual-issue instruction.
*****************************************************************************/
static DIVEC_OP1 GetDualIssueOp1(IMG_UINT32	uOpcode,
								 IMG_PBOOL	pbVec3,
								 IMG_PBOOL	pbVec4)
{
	*pbVec3 = *pbVec4 = IMG_FALSE;
	switch (uOpcode)
	{
		case USEASM_OP_VMAD3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VMAD3;
		case USEASM_OP_VDP3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VDP3;
		case USEASM_OP_VMUL3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VMUL3;
		case USEASM_OP_VADD3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VADD3;
		case USEASM_OP_VSSQ3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VSSQ3;
		case USEASM_OP_VMOV3:	*pbVec3 = IMG_TRUE; return DIVEC_OP1_VMOV3;
		case USEASM_OP_VMAD4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VMAD4;
		case USEASM_OP_VDP4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VDP4;
		case USEASM_OP_VMUL4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VMUL4;
		case USEASM_OP_VADD4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VADD4;
		case USEASM_OP_VSSQ4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VSSQ4;
		case USEASM_OP_VMOV4:	*pbVec4 = IMG_TRUE; return DIVEC_OP1_VMOV4;
		case USEASM_OP_FRSQ:	return DIVEC_OP1_FRSQ;
		case USEASM_OP_FRCP:	return DIVEC_OP1_FRCP;
		case USEASM_OP_FMAD:	return DIVEC_OP1_FMAD;
		case USEASM_OP_FADD:	return DIVEC_OP1_FADD;
		case USEASM_OP_FMUL:	return DIVEC_OP1_FMUL;
		case USEASM_OP_FSUBFLR:	return DIVEC_OP1_FSUBFLR;
		case USEASM_OP_FEXP:	return DIVEC_OP1_FEXP;
		case USEASM_OP_FLOG:	return DIVEC_OP1_FLOG;
		default:				return (DIVEC_OP1)-1;
	}
}

/*****************************************************************************
 FUNCTION	: GetDualIssueOp2

 PURPOSE	: Get the index into the secondary opcode table corresponding to
			  a useasm opcode.

 PARAMETERS	: uOpcode				- Input opcode.
			  pbVec3				- Set to TRUE on return if the opcode is
									vec3.
			  pbVec4				- Set to TRUE on return if the opcode is
									vec4.

 RETURNS	: The corresponding index or -1 if the opcode isn't available for
			  the secondary operation in a vector dual-issue instruction.
*****************************************************************************/
static DIVEC_OP2 GetDualIssueOp2(IMG_UINT32	uOpcode,
								 IMG_PBOOL	pbVec3,
								 IMG_PBOOL	pbVec4)
{
	*pbVec3 = *pbVec4 = IMG_FALSE;
	switch (uOpcode)
	{
		case USEASM_OP_VDP3:	*pbVec3 = IMG_TRUE; return DIVEC_OP2_VDP;
		case USEASM_OP_VMUL3:	*pbVec3 = IMG_TRUE; return DIVEC_OP2_VMUL;
		case USEASM_OP_VADD3:	*pbVec3 = IMG_TRUE; return DIVEC_OP2_VADD;
		case USEASM_OP_VSSQ3:	*pbVec3 = IMG_TRUE; return DIVEC_OP2_VSSQ;
		case USEASM_OP_VMOV3:	*pbVec3 = IMG_TRUE; return DIVEC_OP2_VMOV;
		case USEASM_OP_VDP4:	*pbVec4 = IMG_TRUE; return DIVEC_OP2_VDP;
		case USEASM_OP_VMUL4:	*pbVec4 = IMG_TRUE; return DIVEC_OP2_VMUL;
		case USEASM_OP_VADD4:	*pbVec4 = IMG_TRUE; return DIVEC_OP2_VADD;
		case USEASM_OP_VSSQ4:	*pbVec4 = IMG_TRUE; return DIVEC_OP2_VSSQ;
		case USEASM_OP_VMOV4:	*pbVec4 = IMG_TRUE; return DIVEC_OP2_VMOV;
		case USEASM_OP_FRSQ:	return DIVEC_OP2_FRSQ;
		case USEASM_OP_FRCP:	return DIVEC_OP2_FRCP;
		case USEASM_OP_FMAD:	return DIVEC_OP2_FMAD;
		case USEASM_OP_FADD:	return DIVEC_OP2_FADD;
		case USEASM_OP_FMUL:	return DIVEC_OP2_FMUL;
		case USEASM_OP_FSUBFLR:	return DIVEC_OP2_FSUBFLR;
		case USEASM_OP_FEXP:	return DIVEC_OP2_FEXP;
		case USEASM_OP_FLOG:	return DIVEC_OP2_FLOG;
		default:				return (DIVEC_OP2)-1;
	}
}

#define USEASM_SWIZZLE3(X, Y, Z)		((X << (USEASM_SWIZZLE_FIELD_SIZE * 0)) | \
										 (Y << (USEASM_SWIZZLE_FIELD_SIZE * 1)) | \
										 (Z << (USEASM_SWIZZLE_FIELD_SIZE * 2)))

#define STDSWIZ_CASE(X, Y, Z)	\
	case USEASM_SWIZZLE3(USEASM_SWIZZLE_SEL_##X, USEASM_SWIZZLE_SEL_##Y, USEASM_SWIZZLE_SEL_##Z): \
	{	\
		return SGXVEC_USE_DVEC_SWIZ_VEC3_STD_##X##Y##Z; \
	}

#define EXTSWIZ_CASE(X, Y, Z)	\
	case USEASM_SWIZZLE3(USEASM_SWIZZLE_SEL_##X, USEASM_SWIZZLE_SEL_##Y, USEASM_SWIZZLE_SEL_##Z): \
	{	\
		return SGXVEC_USE_DVEC_SWIZ_VEC3_EXT_##X##Y##Z; \
	}

static IMG_UINT32 EncodeDualIssueVectorVec3Swizzle(IMG_UINT32	uInputSwizzle,
												   IMG_BOOL		bExtendedSwizzleAvailable,
												   IMG_PBOOL	pbExtendedSwizzleUsed)
/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorVec3Swizzle

 PURPOSE	: Get the encoding for a swizzle argument to a dual-issue
			  vec3 instruction.

 PARAMETERS	: uInputSwizzle				- The input format swizzle.
			  bExtendedSwizzleAvailable	- TRUE if the extended swizzle set is
										available.
			  pbExtendedSwizzleUsed		- Returns TRUE if the extended swizzle
										set was used.

 RETURNS	: The encoded swizzle or USE_UNDEF if the input swizzle isn't
			  available.
*****************************************************************************/
{
	/*
		Mask the top channel.
	*/
	uInputSwizzle &= ~(USEASM_SWIZZLE_VALUE_MASK << (USEASM_SWIZZLE_FIELD_SIZE * 3));

	/*
		Check if the input swizzle matches one of the standard swizzle set.
	*/
	*pbExtendedSwizzleUsed = IMG_FALSE;
	switch (uInputSwizzle)
	{
		STDSWIZ_CASE(X, X, X)
		STDSWIZ_CASE(Y, Y, Y)
		STDSWIZ_CASE(Z, Z, Z)
		STDSWIZ_CASE(W, W, W)
		STDSWIZ_CASE(X, Y, Z)
		STDSWIZ_CASE(Y, Z, W)
		STDSWIZ_CASE(X, X, Y)
		STDSWIZ_CASE(X, Y, X)
		STDSWIZ_CASE(Y, Y, X)
		STDSWIZ_CASE(Y, Y, Z)
		STDSWIZ_CASE(Z, X, Y)
		STDSWIZ_CASE(X, Z, Y)
		STDSWIZ_CASE(0, 0, 0)
		STDSWIZ_CASE(HALF, HALF, HALF)
		STDSWIZ_CASE(1, 1, 1)
		STDSWIZ_CASE(2, 2, 2)
	}

	if (bExtendedSwizzleAvailable)
	{
		/*
			Check if the input swizzle matches one of the extended swizzles.
		*/
		*pbExtendedSwizzleUsed = IMG_TRUE;
		switch (uInputSwizzle)
		{
			EXTSWIZ_CASE(X, Y, Y)
			EXTSWIZ_CASE(Y, X, Y)
			EXTSWIZ_CASE(X, X, Z)
			EXTSWIZ_CASE(Y, X, X)
			EXTSWIZ_CASE(X, Y, 0)
			EXTSWIZ_CASE(X, 1, 0)
			EXTSWIZ_CASE(X, Z, Y)
			EXTSWIZ_CASE(Y, Z, X)
			EXTSWIZ_CASE(Z, Y, X)
			EXTSWIZ_CASE(Z, Z, Y)
			EXTSWIZ_CASE(X, Y, 1)
		}
	}

	/*
		No matching swizzle.
	*/
	return USE_UNDEF;
}

#undef USEASM_SWIZZLE3
#undef STDSWIZ_CASE
#undef EXTSWIZ_CASE

#define STDSWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		return SGXVEC_USE_DVEC_SWIZ_VEC4_STD_##X##Y##Z##W; \
	}

#define EXTSWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		return SGXVEC_USE_DVEC_SWIZ_VEC4_EXT_##X##Y##Z##W; \
	}

static IMG_UINT32 EncodeDualIssueVectorVec4Swizzle(IMG_UINT32	uInputSwizzle,
												   IMG_BOOL		bExtendedSwizzleAvailable,
												   IMG_PBOOL	pbExtendedSwizzleUsed)
/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorVec4Swizzle

 PURPOSE	: Get the encoding for a swizzle argument to a dual-issue
			  vec4 instruction.

 PARAMETERS	: uInputSwizzle				- The input format swizzle.
			  bExtendedSwizzleAvailable	- TRUE if the extended swizzle set is
										available.
			  pbExtendedSwizzleUsed		- Returns TRUE if the extended swizzle
										set was used.

 RETURNS	: The encoded swizzle or USE_UNDEF if the input swizzle isn't
			  available.
*****************************************************************************/
{
	/*
		Check if the input swizzle matches one of the standard swizzle set.
	*/
	*pbExtendedSwizzleUsed = IMG_FALSE;
	switch (uInputSwizzle)
	{
		STDSWIZ_CASE(X, X, X, X)
		STDSWIZ_CASE(Y, Y, Y, Y)
		STDSWIZ_CASE(Z, Z, Z, Z)
		STDSWIZ_CASE(W, W, W, W)
		STDSWIZ_CASE(X, Y, Z, W)
		STDSWIZ_CASE(Y, Z, W, W)
		STDSWIZ_CASE(X, Y, Z, Z)
		STDSWIZ_CASE(X, X, Y, Z)
		STDSWIZ_CASE(X, Y, X, Y)
		STDSWIZ_CASE(X, Y, W, Z)
		STDSWIZ_CASE(Z, X, Y, W)
		STDSWIZ_CASE(Z, W, Z, W)
		STDSWIZ_CASE(0, 0, 0, 0)
		STDSWIZ_CASE(HALF, HALF, HALF, HALF)
		STDSWIZ_CASE(1, 1, 1, 1)
		STDSWIZ_CASE(2, 2, 2, 2)
	}

	if (bExtendedSwizzleAvailable)
	{
		/*
			Check if the input swizzle matches one of the extended swizzles.
		*/
		*pbExtendedSwizzleUsed = IMG_TRUE;
		switch (uInputSwizzle)
		{
			EXTSWIZ_CASE(Y, Z, X, W)
			EXTSWIZ_CASE(Z, W, X, Y)
			EXTSWIZ_CASE(X, Z, W, Y)
			EXTSWIZ_CASE(Y, Y, W, W)
			EXTSWIZ_CASE(W, Y, Z, W)
			EXTSWIZ_CASE(W, Z, W, Z)
			EXTSWIZ_CASE(X, Y, Z, X)
			EXTSWIZ_CASE(Z, Z, W, W)
			EXTSWIZ_CASE(X, W, Z, X)
			EXTSWIZ_CASE(Y, Y, Y, X)
			EXTSWIZ_CASE(Y, Y, Y, Z)
			EXTSWIZ_CASE(Z, W, Z, W)
			EXTSWIZ_CASE(Y, Z, X, Z)
			EXTSWIZ_CASE(X, X, Y, Y)
			EXTSWIZ_CASE(X, Z, W, W)
			EXTSWIZ_CASE(X, Y, Z, 1)
		}
	}

	/*
		No matching swizzle.
	*/
	return USE_UNDEF;
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorSwizzle

 PURPOSE	: Encode a swizzle into a slot in a dual-issue vector instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uInArg				- Input argument which contains the swizzle.
			  eSwizzleSlot			- Slot to encode the swizzle into.
			  bVec4					- TRUE if the swizzle is for a vec4 instruction.
			  bSrc2SlotUsed			- TRUE if GPI source 2 is used.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueVectorSwizzle(PUSE_INST					psInst,
											 IMG_UINT32					uInArg,
											 DUALISSUEVECTOR_SRCSLOT	eSwizzleSlot,
											 IMG_BOOL					bScalar,
											 IMG_BOOL					bVec4,
											 IMG_BOOL					bSrc2SlotUsed,
											 IMG_PUINT32				puInst,
											 PUSEASM_CONTEXT			psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_BOOL	bExtendedSwizzleUsed;
	IMG_BOOL	bExtendedSwizzleAvailable;

	/*
		No swizzles available on GPI2.
	*/
	if (eSwizzleSlot == DUALISSUEVECTOR_SRCSLOT_GPI2)
	{
		CheckVectorOrScalarIdentitySwizzle(psContext, psInst, uInArg, !bScalar, bVec4);
		return;
	}
	
	/*
		Are extended swizzles available for this slot.
	*/
	switch (eSwizzleSlot)
	{
		case DUALISSUEVECTOR_SRCSLOT_GPI0: bExtendedSwizzleAvailable = IMG_FALSE; break;
		case DUALISSUEVECTOR_SRCSLOT_GPI1: bExtendedSwizzleAvailable = IMG_TRUE; break;
		case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE: bExtendedSwizzleAvailable = !bSrc2SlotUsed; break;
		default: IMG_ABORT();
	}

	if (bScalar)
	{
		IMG_UINT32	uSrcComponent = 
			(psInst->asArg[uInArg].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;

		bExtendedSwizzleUsed = IMG_FALSE;
		if (bVec4)
		{
			switch (uSrcComponent)
			{
				case 0: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC4_STD_XXXX; break;
				case 1: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC4_STD_YYYY; break;
				case 2: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC4_STD_ZZZZ; break;
				case 3: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC4_STD_WWWW; break;
				default: IMG_ABORT();
			}
		}
		else
		{
			switch (uSrcComponent)
			{
				case 0: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC3_STD_XXX; break;
				case 1: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC3_STD_YYY; break;
				case 2: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC3_STD_ZZZ; break;
				case 3: uEncodedSwizzle = SGXVEC_USE_DVEC_SWIZ_VEC3_STD_WWW; break;
				default: IMG_ABORT();
			}
		}
	}
	else
	{
		IMG_UINT32	uInputSwizzle;

		/*
			Check the instruction argument represents a swizzle.
		*/
		if (!CheckSwizzleArgument(psInst, uInArg + 1, psContext))
		{
			return;
		}

		uInputSwizzle = psInst->asArg[uInArg + 1].uNumber;
		if (bVec4)
		{
			uEncodedSwizzle =
				EncodeDualIssueVectorVec4Swizzle(uInputSwizzle, bExtendedSwizzleAvailable, &bExtendedSwizzleUsed);
		}
		else
		{
			uEncodedSwizzle = 
				EncodeDualIssueVectorVec3Swizzle(uInputSwizzle, bExtendedSwizzleAvailable, &bExtendedSwizzleUsed);
		}
		if (uEncodedSwizzle == USE_UNDEF)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
					   "This swizzle isn't available on a %s instruction",
					   OpcodeName(psInst->uOpcode)));
			return;
		}
	}

	/*
		Set the encoded swizzle into the slot.
	*/
	switch (eSwizzleSlot)
	{
		case DUALISSUEVECTOR_SRCSLOT_GPI0:
		{
			assert(!bExtendedSwizzleUsed);
			puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_DVEC_GPI0SWIZ_SHIFT);
			break;
		}
		case DUALISSUEVECTOR_SRCSLOT_GPI1:
		{
			puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_DVEC_GPI1SWIZ_SHIFT);
			if (bExtendedSwizzleUsed)
			{
				puInst[1] |= SGXVEC_USE1_DVEC_GPI1SWIZEXT;
			}
			break;
		}
		case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE:
		{
			puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_DVEC_USSWIZ_SHIFT);
			if (bExtendedSwizzleUsed)
			{
				assert(!bSrc2SlotUsed);
				puInst[0] |= SGXVEC_USE0_DVEC_USSWIZEXT;
			}
			break;
		}
		default: IMG_ABORT();
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorSource

 PURPOSE	: Encode a source into a slot in a dual-issue vector instruction.

 PARAMETERS	: psInst				- Input instruction.
			  bSrc2SlotUsed			- TRUE if the GPI2 source slot is used.
			  uInArg				- Input argument to encode.
			  uOutArg				- Slot to use in the instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Target core.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueVectorSource(PUSE_INST					psInst,
											IMG_BOOL					bOpIsVector,
											IMG_BOOL					bSrc2SlotUsed,
											IMG_UINT32					uInArg,
											DUALISSUEVECTOR_SRCSLOT		eOutArg,
											IMG_PUINT32					puInst,
											PUSEASM_CONTEXT				psContext,
											PCSGX_CORE_DESC				psTarget)
{
	if (eOutArg == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
	{
		if (bOpIsVector)
		{
			EncodeWideUSSourceDoubleRegisters(psContext,
											  psInst,
											  IMG_FALSE /* bAllowExtended */,
											  0 /* uBankExtension */,
											  puInst + 0,
											  puInst + 1,
											  SGXVEC_USE_DVEC_USSRC_VECTOR_REGISTER_NUMBER_BITS,
											  SGXVEC_USE0_DVEC_USSNUM_SHIFT /* uNumberFieldShift */,
											  USEASM_HWARG_SOURCE1,
											  uInArg,
											  psTarget);
		}
		else
		{
			EncodeArgument(psContext,
						   psInst,
						   USEASM_HWARG_SOURCE1,
						   uInArg,
						   IMG_FALSE /* bAllowExtended */,
						   0 /* uBankExtension */,
						   IMG_FALSE /* bSigned */,
						   puInst + 0,
						   puInst + 1,
						   IMG_FALSE /* bBitwise */,
						   IMG_FALSE /* bFmtSelect */,
						   0 /* uFmtFlag */,
						   psTarget,
						   SGXVEC_USE0_DVEC_USSNUM_SHIFT,
						   SGXVEC_USE_DVEC_USSRC_SCALAR_REGISTER_NUMBER_BITS);
		}
	}
	else
	{
		PUSE_REGISTER	psArg = &psInst->asArg[uInArg];

		if (psArg->uType != USEASM_REGTYPE_FPINTERNAL)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "Argument %d must be a GPI register", uInArg));
		}
		if (psArg->uIndex != USEREG_INDEX_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"A GPI source to a dual-issue vector instruction can't be indexed"));
		}
		switch (eOutArg)
		{
			case DUALISSUEVECTOR_SRCSLOT_GPI0: puInst[0] |= (psArg->uNumber << SGXVEC_USE0_DVEC_GPI0NUM_SHIFT); break;
			case DUALISSUEVECTOR_SRCSLOT_GPI1: puInst[0] |= (psArg->uNumber << SGXVEC_USE0_DVEC_GPI1NUM_SHIFT); break;
			case DUALISSUEVECTOR_SRCSLOT_GPI2: 
			{
				if (!bSrc2SlotUsed)
				{
					if (psArg->uNumber != SGXVEC_USE0_DVEC_GPI2NUM_DEFAULT)
					{
						USEASM_ERRMSG((psContext->pvContext, psInst,
							"For a dual-issue vector instruction where the first operation has two or more sources "
							"only i%d can be used in the GPI2 source slot", SGXVEC_USE0_DVEC_GPI2NUM_DEFAULT));
					}
				}
				else
				{
					puInst[0] |= (psArg->uNumber << SGXVEC_USE0_DVEC_GPI2NUM_SHIFT); 
				}
				break;
			}
			default: IMG_ABORT();
		}
		if (psArg->uNumber >= NumberOfInternalRegisters(psTarget))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
				"The maximum number for a GPI source to a dual-issue vector instruction is %d",
				NumberOfInternalRegisters(psTarget) - 1));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueSourceModifier

 PURPOSE	: Encode a source into a slot in a dual-issue vector instruction.

 PARAMETERS	: psContext				- USEASM context.
			  psInst				- Input instruction.
			  uInArg				- Index of the input argument to encode the
									source modifiers from.
			  eSlot					- Index of the instruction argument to encode the
									source modifiers to.
			  bScalar				- TRUE if the instruction is scalar.
			  bSrc2SlotUsed			- TRUE if the GPI2 slot is used.
			  puInst				- Points to the memory for the encoded instruction.
			  

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueSourceModifier(PUSEASM_CONTEXT			psContext,
											  PUSE_INST					psInst,
											  IMG_UINT32				uInArg,
											  DUALISSUEVECTOR_SRCSLOT	eSlot,
											  IMG_BOOL					bScalar,
											  IMG_BOOL					bSrc2SlotUsed,
											  IMG_PUINT32				puInst)
{
	IMG_UINT32	uValidArgFlags;

	/*
		Get the source modifers which are valid on this source slot.
	*/
	switch (eSlot)
	{
		case DUALISSUEVECTOR_SRCSLOT_GPI0: uValidArgFlags = 0; break;
		case DUALISSUEVECTOR_SRCSLOT_GPI1: uValidArgFlags = USEASM_ARGFLAGS_NEGATE; break;
		case DUALISSUEVECTOR_SRCSLOT_GPI2: uValidArgFlags = USEASM_ARGFLAGS_REQUIRESGPI2; break;
		case DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE: 
		{
			uValidArgFlags = USEASM_ARGFLAGS_NEGATE;
			if (!bSrc2SlotUsed)
			{
				uValidArgFlags |= USEASM_ARGFLAGS_ABSOLUTE; 
			}
			break;
		}
		default: IMG_ABORT();
	}
	if (bScalar)
	{
		uValidArgFlags |= (~USEASM_ARGFLAGS_COMP_CLRMSK);
	}
	uValidArgFlags |= USEASM_ARGFLAGS_REQUIREMOE;

	CheckArgFlags(psContext, psInst, uInArg, uValidArgFlags);

	if (eSlot == DUALISSUEVECTOR_SRCSLOT_GPI1)
	{
		if (psInst->asArg[uInArg].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGXVEC_USE1_DVEC_GPI1NEG;
		}
	}
	else if (eSlot == DUALISSUEVECTOR_SRCSLOT_UNIFIEDSTORE)
	{
		if (psInst->asArg[uInArg].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			puInst[1] |= SGXVEC_USE1_DVEC_USNEG;
		}
		if (psInst->asArg[uInArg].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
		{
			puInst[0] |= SGXVEC_USE0_DVEC_USABS;
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorSecondarySources

 PURPOSE	: Encode the sources to a secondary operation in a dual-issue vector
			  instruction.

 PARAMETERS	: psOp2Inst				- Input instruction.
			  eOp1					- Primary operation.
			  eOp2					- Secondary operation.
			  bVec3					- TRUE if the secondary operation is vec3.
			  bVec4					- TRUE if the secondary operation is vec4.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  bSrc2SlotUsed			- TRUE if the GPI2 source slot is used.
			  ePrimaryUSSource		- The source to the primary operation which
									used the unified store slot.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueVectorSecondarySources(PUSE_INST						psOp2Inst,
													  DIVEC_OP2						eOp2,
													  IMG_BOOL						bOp2IsVec4,
													  IMG_PUINT32					puInst,
													  PUSEASM_CONTEXT				psContext,
													  IMG_BOOL						bSrc2SlotUsed,
													  DUALISSUEVECTOR_SRCSLOT const	aeHwSrcMap[],
													  PCSGX_CORE_DESC				psTarget)
{
	IMG_UINT32	uSrc;
	IMG_UINT32	uSrcCount = g_asDiVecOp2Desc[eOp2].uSrcCount;
	IMG_BOOL	bVector = g_asDiVecOp2Desc[eOp2].bVector;

	for (uSrc = 0; uSrc < uSrcCount; uSrc++)
	{
		IMG_UINT32				uInArg;
		DUALISSUEVECTOR_SRCSLOT	eSlot = aeHwSrcMap[uSrc];

		if (bVector)
		{
			uInArg = 1 + uSrc * 2;
		}
		else
		{
			uInArg = 1 + uSrc;
		}

		/*
			Encode any absolute or negate modifiers on this
			source.
		*/
		EncodeDualIssueSourceModifier(psContext,
									  psOp2Inst,
									  uInArg,
									  eSlot,
									  !bVector,
									  bSrc2SlotUsed,
									  puInst);

		/*
			Encode the source number and bank.
		*/
		EncodeDualIssueVectorSource(psOp2Inst,
									bVector,
									bSrc2SlotUsed,
									uInArg,
									eSlot,
									puInst,
									psContext,
									psTarget);

		/*
			Encode the swizzle.
		*/
		EncodeDualIssueVectorSwizzle(psOp2Inst,
									 uInArg,
									 eSlot,
									 !bVector,
									 bOp2IsVec4,
									 bSrc2SlotUsed,
									 puInst,
									 psContext);
	}
}

enum DIVEC_USSOURCE
{
	DIVEC_USSOURCE_SRC0 = 0,
	DIVEC_USSOURCE_SRC1,
	DIVEC_USSOURCE_SRC2,
	DIVEC_USSOURCE_NONE,
};

static enum DIVEC_USSOURCE EncodeDualIssueVector_DoesOpHaveUnifiedStoreSource(PUSE_INST		psInst,
																			  IMG_UINT32	uSrcCount,
																			  IMG_BOOL		bVector)
/*****************************************************************************
 FUNCTION	: EncodeDualIssueVector_DoesOpHaveUnifiedStoreSource

 PURPOSE	: Check if an argument to a instruction requires the unified store
			  source slot in a dual-issue hardware instruction.

 PARAMETERS	: psInst			- Instruction to check.
			  uSrcCount			- Count of sources to the instruction.
			  bVector			- TRUE if the instruction is a vector operation
								(in this case source arguments are represented by a 
								register and swizzle pair).

 RETURNS	: The index of the unified store argument if it exists.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	for (uSrc = 0; uSrc < uSrcCount; uSrc++)
	{
		IMG_UINT32		uInArg;
		PUSE_REGISTER	psArg;

		/*
			For vector instructions skip the argument representing source swizzles.
		*/
		if (bVector)
		{
			uInArg = 1 + uSrc * 2;
		}
		else
		{
			uInArg = 1 + uSrc;
		}
		psArg = &psInst->asArg[uInArg];

		if (psArg->uType != USEASM_REGTYPE_FPINTERNAL || (psArg->uFlags & USEASM_ARGFLAGS_REQUIREMOE) != 0)
		{
			return uSrc;
		}
	}
	return DIVEC_USSOURCE_NONE;
}

static const IMG_UINT32 g_aaauOp2SrcCfg[3][3][4] = 
{
	/* uOp1SrcCount == 1 */
	{
		/* uOp2SrcCount == 1 */
		{
			/* uOp2USSource == 0 */	SGXVEC_USE0_DVEC_SRCCFG_SRC2,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */ SGXVEC_USE0_DVEC_SRCCFG_SRC2,
		},
		/* uOp2SrcCount == 2 */
		{
			/* uOp2USSource == 0 */	SGXVEC_USE0_DVEC_SRCCFG_SRC1,
			/* uOp2USSource == 1 */ SGXVEC_USE0_DVEC_SRCCFG_SRC2,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */	SGXVEC_USE0_DVEC_SRCCFG_SRC1,
		},
		/* uOp2SrcCount == 3 */
		{
			/* uOp2USSource == 0 */	SGXVEC_USE0_DVEC_SRCCFG_SRC1,
			/* uOp2USSource == 1 */ SGXVEC_USE0_DVEC_SRCCFG_SRC2,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */	SGXVEC_USE0_DVEC_SRCCFG_SRC1,
		},
	},
	/* uOp1SrcCount == 2 */
	{
		/* uOp2SrcCount == 1 */
		{
			/* uOp2USSource == 0 */	SGXVEC_USE0_DVEC_SRCCFG_SRC2,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */	SGXVEC_USE0_DVEC_SRCCFG_SRC2,
		},
		/* uOp2SrcCount == 2 */
		{
			/* uOp2USSource == 0 */	USE_UNDEF,
			/* uOp2USSource == 1 */ SGXVEC_USE0_DVEC_SRCCFG_SRC2,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */ SGXVEC_USE0_DVEC_SRCCFG_SRC2,
		},
		/* uOp2SrcCount == 3 */
		{
			/* uOp2USSource == 0 */	USE_UNDEF,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */ USE_UNDEF,
		},
	},
	/* uOp1SrcCount == 3 */
	{
		/* uOp2SrcCount == 1 */
		{
			/* uOp2USSource == 0 */	SGXVEC_USE0_DVEC_SRCCFG_NONE,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */	SGXVEC_USE0_DVEC_SRCCFG_NONE,
		},
		/* uOp2SrcCount == 2 */
		{
			/* uOp2USSource == 0 */	USE_UNDEF,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */ USE_UNDEF,
		},
		/* uOp2SrcCount == 3 */
		{
			/* uOp2USSource == 0 */	USE_UNDEF,
			/* uOp2USSource == 1 */ USE_UNDEF,
			/* uOp2USSource == 2 */ USE_UNDEF,
			/* uOp2USSource == N */ USE_UNDEF,
		},
	},
};

static IMG_BOOL EncodeDualIssueVector_SetupSourceCfg(PUSE_INST					psOp1Inst,
													 DIVEC_OP1					eOp1,
													 PUSE_INST					psOp2Inst,
													 DIVEC_OP1					eOp2,
													 PCDUALISSUEVECTOR_SRCSLOT*	ppePrimaryMap,
													 PCDUALISSUEVECTOR_SRCSLOT*	ppeSecondaryMap,
													 IMG_PUINT32				puSrcCfg,
													 PUSEASM_CONTEXT			psContext)
/*****************************************************************************
 FUNCTION	: EncodeDualIssueVector_SetupSourceCfg

 PURPOSE	: Encode the sources to a primary operation in a dual-issue vector
			  instruction.

 PARAMETERS	: psOp1Inst				- Primary input instruction.
			  eOp1					- Primary operation.
			  bOp1Vec3				- TRUE if the primary operation is vec3.
			  bOp1Vec4				- TRUE if the primary operation is vec4.
			  psOp2Inst				- Secondary input instruction.
			  eOp2					- Secondary operation.
			  bOp2Vec3				- TRUE if the secondary operation is vec3.
			  bOp2Vec4				- TRUE if the secondary operation is vec4.
			  ppePrimaryMap			- Returns the mapping of primary input
									instruction sources to encoded instruction
									sources.
			  ppeSecondaryMap		- Returns the mapping of secondary input
									instruction sources to encoded instruction
									sources.
			  puSrcCfg				- Returns the setting for the source
									configuration.
			  psContext				- Assembler context.

 RETURNS	: TRUE if the source configuration is valid; FALSE otherwise.
*****************************************************************************/
{
	enum DIVEC_USSOURCE	eOp1USSource;
	enum DIVEC_USSOURCE	eOp2USSource;
	IMG_UINT32			uOp1SrcCount = g_asDiVecOp1Desc[eOp1].uSrcCount;
	IMG_BOOL			bOp1Vector = g_asDiVecOp1Desc[eOp1].bVector;
	IMG_UINT32			uOp2SrcCount = g_asDiVecOp2Desc[eOp2].uSrcCount;
	IMG_BOOL			bOp2Vector = g_asDiVecOp2Desc[eOp2].bVector;

	eOp1USSource = 
		EncodeDualIssueVector_DoesOpHaveUnifiedStoreSource(psOp1Inst, uOp1SrcCount, bOp1Vector);
	eOp2USSource = 
		EncodeDualIssueVector_DoesOpHaveUnifiedStoreSource(psOp2Inst, uOp2SrcCount, bOp2Vector);

	if (eOp1USSource != DIVEC_USSOURCE_NONE)
	{
		if (eOp2USSource != DIVEC_USSOURCE_NONE)
		{
			USEASM_ERRMSG((psContext->pvContext, psOp1Inst, "Only one source argument to a pair of dual-issued "
				"instructions can come from the unified store"));
			return IMG_FALSE;
		}

		switch (eOp1USSource)
		{
			case DIVEC_USSOURCE_SRC0: *puSrcCfg = SGXVEC_USE0_DVEC_SRCCFG_SRC0; break;
			case DIVEC_USSOURCE_SRC1: *puSrcCfg = SGXVEC_USE0_DVEC_SRCCFG_SRC1; break;
			case DIVEC_USSOURCE_SRC2: *puSrcCfg = SGXVEC_USE0_DVEC_SRCCFG_SRC2; break;
			default: IMG_ABORT();
		}
	}
	else
	{
		*puSrcCfg = g_aaauOp2SrcCfg[uOp1SrcCount - 1][uOp2SrcCount - 1][eOp2USSource];

		if ((*puSrcCfg) == USE_UNDEF)
		{
			USEASM_ERRMSG((psContext->pvContext, psOp1Inst, "Source %d to the secondary operation must be an "
				"internal register", eOp2USSource));
			return IMG_FALSE;
		}

		/* Ambiguous case. If there is a negate modifier the difference matters. */
		if (uOp1SrcCount == 1 && (uOp2SrcCount == 2 || uOp2SrcCount == 3) && eOp2USSource == DIVEC_USSOURCE_SRC1)
		{
			if ((psOp1Inst->asArg[1].uFlags & USEASM_ARGFLAGS_REQUIRESGPI2) != 0)
			{
				*puSrcCfg = SGXVEC_USE0_DVEC_SRCCFG_NONE;
			}
			else
			{
				*puSrcCfg = SGXVEC_USE0_DVEC_SRCCFG_SRC2;
			}
		}
	}

	*ppePrimaryMap = g_aapeDualIssueVectorPrimaryMap[uOp1SrcCount - 1][*puSrcCfg];
	*ppeSecondaryMap = g_aaapeDualIssueVectorSecondaryMap[uOp1SrcCount - 1][uOp2SrcCount - 1][*puSrcCfg];

	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorPrimarySources

 PURPOSE	: Encode the sources to a primary operation in a dual-issue vector
			  instruction.

 PARAMETERS	: psOp1Inst				- Primary input instruction.
			  eOp1					- Primary operation.
			  bVector4				- TRUE if the instruction is vec4.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  bSrc2SlotUsed			- TRUE if the GPI2 slot is used.
			  pePrimaryUSSource		- Returns the source to the primary operation which
									used the unified store slot.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueVectorPrimarySources(PUSE_INST					psOp1Inst,
													DIVEC_OP1					eOp1,
													IMG_BOOL					bOp1IsVec4,
													IMG_PUINT32					puInst,
													PUSEASM_CONTEXT				psContext,
													IMG_BOOL					bSrc2SlotUsed,
													PCDUALISSUEVECTOR_SRCSLOT	pePrimaryMap,
													PCSGX_CORE_DESC				psTarget)
{
	IMG_UINT32	uSrc;
	IMG_UINT32	uSrcCount = g_asDiVecOp1Desc[eOp1].uSrcCount;
	IMG_BOOL	bVector = g_asDiVecOp1Desc[eOp1].bVector; 

	/*
		Work out which source (if any) comes from the unified store.
	*/
	for (uSrc = 0; uSrc < uSrcCount; uSrc++)
	{
		IMG_UINT32					uInArg;
		PUSE_REGISTER				psArg;
		DUALISSUEVECTOR_SRCSLOT		eSlot = pePrimaryMap[uSrc];

		if (bVector)
		{
			uInArg = 1 + uSrc * 2;
		}
		else
		{
			uInArg = 1 + uSrc;
		}
		psArg = &psOp1Inst->asArg[uInArg];

		/*
			Encode any absolute or negate modifiers on this
			source.
		*/
		EncodeDualIssueSourceModifier(psContext,
									  psOp1Inst,
									  uInArg,
									  eSlot,
									  !bVector,
									  bSrc2SlotUsed,
									  puInst);

		/*
			Encode the source number and bank.
		*/
		EncodeDualIssueVectorSource(psOp1Inst,
									bVector,
									bSrc2SlotUsed,
									uInArg,
									eSlot,
									puInst,
									psContext,
									psTarget);

		/*
			Encode the swizzle.
		*/
		EncodeDualIssueVectorSwizzle(psOp1Inst,
									 uInArg,
									 eSlot,
									 !bVector,
									 bOp1IsVec4,
									 bSrc2SlotUsed,
									 puInst,
									 psContext);
	}
}

/*****************************************************************************
 FUNCTION	: EncodeDualIssueVectorInstruction

 PURPOSE	: Encode a dual-issue vector instruction.

 PARAMETERS	: psOp1Inst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeDualIssueVectorInstruction(PUSE_INST			psOp1Inst,
												 IMG_PUINT32		puInst,
												 PUSEASM_CONTEXT	psContext,
												 PCSGX_CORE_DESC		psTarget)
{
	PUSE_INST					psOp2Inst = psOp1Inst->psNext;
	DIVEC_OP1					eOp1;
	DIVEC_OP2					eOp2;
	IMG_BOOL					bOp1IsVec3, bOp1IsVec4, bOp2IsVec3, bOp2IsVec4;
	IMG_BOOL					bOpIsVec4;
	PUSE_INST					psGPIWriterInst, psUSWriterInst;
	IMG_BOOL					bSrc2SlotUsed;
	PCDUALISSUEVECTOR_SRCSLOT	pePrimaryMap;
	PCDUALISSUEVECTOR_SRCSLOT	peSecondaryMap;
	IMG_UINT32					uSrcCfg;
	IMG_UINT32					uGPIWriterDestMask;
	IMG_UINT32					uUSWriterDestMask;
	IMG_UINT32					uOpRequiredMask;
	IMG_BOOL					bUSWriterInstHasVectorResult;
	IMG_BOOL					bF16;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psOp1Inst,
					   "%s isn't supported on this core",
					   OpcodeName(psOp1Inst->uOpcode)));
	}

	CheckFlags(psContext,
			   psOp1Inst,
			   USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_MOVC_FMTF16 | USEASM_OPFLAGS1_MAINISSUE | (~USEASM_OPFLAGS1_MASK_CLRMSK) | (~USEASM_OPFLAGS1_PRED_CLRMSK),
			   0,
			   0);
	CheckFlags(psContext,
			   psOp2Inst,
			   (~USEASM_OPFLAGS1_MASK_CLRMSK),
			   0,
			   0);

	/*
		Encode SKIPINVALID flag.
	*/
	puInst[0] = 0;
	puInst[1] = ((psOp1Inst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0);

	/*
		Encode predicate.
	*/
	puInst[1] |= (EncodeShortVectorPredicate(psContext, psOp1Inst) << SGXVEC_USE1_DVEC_SVPRED_SHIFT);

	/*
		Encode F16 flag.
	*/
	bF16 = IMG_FALSE;
	if (psOp1Inst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTF16)
	{
		bF16 = IMG_TRUE;
		puInst[1] |= SGXVEC_USE1_DVEC_F16;
	}

	/*
		Get primary opcode.
	*/
	eOp1 = GetDualIssueOp1(psOp1Inst->uOpcode, &bOp1IsVec3, &bOp1IsVec4);
	if (eOp1 == (DIVEC_OP1)-1)
	{
		USEASM_ERRMSG((psContext->pvContext, psOp1Inst,
					   "%s is not a valid primary opcode for a vector dual-issue instruction", 
					   OpcodeName(psOp1Inst->uOpcode)));
		return;
	}

	/*
		Get secondary opcode.
	*/
	eOp2 = GetDualIssueOp2(psOp2Inst->uOpcode, &bOp2IsVec3, &bOp2IsVec4);
	if (eOp2 == (DIVEC_OP2)-1)
	{
		USEASM_ERRMSG((psContext->pvContext, psOp2Inst,
					   "%s is not a valid secondary opcode for a vector dual-issue instruction", 
					   OpcodeName(psOp2Inst->uOpcode)));
		return;
	}


	/*
		Check the two opcodes can be co-issued.
	*/
	if ((bOp1IsVec3 && bOp2IsVec4) || (bOp1IsVec4 && bOp2IsVec3))
	{
		USEASM_ERRMSG((psContext->pvContext, psOp1Inst, 
			"The vector lengths of the co-issued opcodes must match for a vector dual-issue instruction"));
		return;
	}	
	if ((bOp2IsVec4 && g_asDiVecOp1Desc[eOp1].uExt != 0) || !g_aabValidVectorDualIssue[eOp1][eOp2])
	{
		USEASM_ERRMSG((psContext->pvContext, psOp1Inst,
					   "%s and %s are not valid for co-issue in a vector dual-issue instruction", 
					   OpcodeName(psOp1Inst->uOpcode),
					   OpcodeName(psOp2Inst->uOpcode)));
		return;
	}

	/*
		How many sources are used in total.
	*/
	bSrc2SlotUsed = IMG_FALSE;
	if (g_asDiVecOp1Desc[eOp1].uSrcCount >= 2)
	{
		bSrc2SlotUsed = IMG_TRUE;
	}

	/*
		Encode main opcode.
	*/
	if (bOp1IsVec4 || bOp2IsVec4)
	{
		bOpIsVec4 = IMG_TRUE;
		puInst[1] |= (SGXVEC_USE1_OP_DVEC4 << EURASIA_USE1_OP_SHIFT);
	}
	else
	{
		bOpIsVec4 = IMG_FALSE;
		puInst[1] |= (SGXVEC_USE1_OP_DVEC3 << EURASIA_USE1_OP_SHIFT);
	}

	/*
		Encode primary opcode.
	*/
	puInst[1] |= (g_asDiVecOp1Desc[eOp1].uOp1 << SGXVEC_USE1_DVEC_OP1_SHIFT);
	puInst[1] |= g_asDiVecOp1Desc[eOp1].uExt;

	/*
		Encode secondary opcode.
	*/
	puInst[0] |= (g_asDiVecOp2Desc[eOp2].uOp2 << SGXVEC_USE0_DVEC_OP2_SHIFT);
	puInst[1] |= g_asDiVecOp2Desc[eOp2].uExt;

	/*
		Encode destination configuration.
	*/
	if (
			(psOp1Inst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL) || 
			(psOp1Inst->asArg[0].uFlags & USEASM_ARGFLAGS_REQUIREMOE) != 0
	   )
	{
		if (
				(psOp2Inst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL) ||
				(psOp2Inst->asArg[0].uFlags & USEASM_ARGFLAGS_REQUIREMOE) != 0
		   )
		{
			USEASM_ERRMSG((psContext->pvContext, psOp1Inst, 
				"At least one instruction in vector dual-issue must write a GPI registers"));
		}	
		psGPIWriterInst = psOp2Inst;
		psUSWriterInst = psOp1Inst;
		bUSWriterInstHasVectorResult = g_asDiVecOp1Desc[eOp1].bVectorResult;

		puInst[1] |= SGXVEC_USE1_DVEC_DCFG_PRIWRITETOUS;
	}
	else
	{
		psGPIWriterInst = psOp1Inst;
		psUSWriterInst = psOp2Inst;
		bUSWriterInstHasVectorResult = g_asDiVecOp2Desc[eOp2].bVectorResult;
	}

	/*
		Check the destination mask for the instruction using the unified store destination.
	*/
	uUSWriterDestMask = (psUSWriterInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	if (psUSWriterInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		/*
			For a GPI destination the instruction will write the mask of channels
			implied by vector length i.e. XYZ for a vec3 instruction and
			XYZW for a vec4.
		*/
		uOpRequiredMask = bOpIsVec4 ? USEREG_MASK_XYZW : USEREG_MASK_XYZ;
	}
	else
	{
		if (bUSWriterInstHasVectorResult)
		{
			/*
				For a unified store destination with a vector instruction
				write a whole 64-bit register. Either 2 F32 channels or
				4 F16 channels.
			*/
			if (bF16)
			{
				uOpRequiredMask = USEREG_MASK_XYZW;
			}
			else
			{
				
				uOpRequiredMask = USEREG_MASK_XY;
			}
		}
		else
		{
			/*
				For a unified store destination with a scalar instruction
				write a single 32-bit register.
			*/
			uOpRequiredMask = bF16 ? USEREG_MASK_XY : USEREG_MASK_X;
		}
	}
	if (uUSWriterDestMask != uOpRequiredMask)
	{
		USEASM_ERRMSG((psContext->pvContext, psUSWriterInst,
			"Invalid destination write mask for this instruction as part of a dual-issued pair"));
	}

	/*
		Encode the GPI destination register number.
	*/
	CheckArgFlags(psContext, psGPIWriterInst, 0 /* uArgument */, 0 /* uValidFlags */);
	if (psGPIWriterInst->asArg[0].uNumber >= NumberOfInternalRegisters(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psGPIWriterInst,
			"The maximum register number for a GPI destination is %d", NumberOfInternalRegisters(psTarget) - 1));
	}
	puInst[0] |= (psGPIWriterInst->asArg[0].uNumber << SGXVEC_USE0_DVEC_GPIDNUM_SHIFT);

	/*
		Encode the unified store destination.
	*/
	CheckArgFlags(psContext, psUSWriterInst, 0 /* uArgument */, USEASM_ARGFLAGS_REQUIREMOE /* uValidFlags */);
	if (bUSWriterInstHasVectorResult)
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psUSWriterInst,
									  IMG_FALSE /* bAllowExtended */,
									  0 /* uBankExtension */,
								      puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_DVEC_USDEST_VECTOR_REGISTER_NUMBER_BITS,
									  EURASIA_USE0_DST_SHIFT /* uNumberFieldShift */,
									  USEASM_HWARG_DESTINATION,
									  0 /* uArg */,
									  psTarget);
	}
	else
	{
		EncodeDest(psContext,
				   psUSWriterInst,
				   IMG_FALSE /* bAllowExtended */,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE /* bFmtSelect */,
				   0 /* uFmtFlag */,
				   psTarget);
	}

	/*
		Get the destination mask for the instruction using the GPI destination.
	*/
	uGPIWriterDestMask = (psGPIWriterInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;

	/*
		Encode xyz component of GPI write mask.
	*/
	puInst[0] |= ((uGPIWriterDestMask & 7) << SGXVEC_USE0_DVEC_WMSK_SHIFT);

	/*
		Encode w component of GPI write mask.
	*/
	if (bOpIsVec4)
	{
		if (uGPIWriterDestMask & USEREG_MASK_W)
		{
			puInst[1] |= SGXVEC_USE1_DVEC4_WMSKW;
		}
	}
	else
	{
		if (uGPIWriterDestMask & USEREG_MASK_W)
		{
			USEASM_ERRMSG((psContext->pvContext, psGPIWriterInst,
				"Only a vec4 instruction can write to the w component of its result"));
		}
	}

	/*
		Check which input sources are using which slots in the instruction.
	*/
	if (!EncodeDualIssueVector_SetupSourceCfg(psOp1Inst,
											  eOp1,
											  psOp2Inst,
											  eOp2,
											  &pePrimaryMap,
											  &peSecondaryMap,
											  &uSrcCfg,
											  psContext))
	{
		return;
	}

	/*
		Encode the instruction source configuration.
	*/
	puInst[0] |= (uSrcCfg << SGXVEC_USE0_DVEC_SRCCFG_SHIFT);

	/*
		Encode the sources to the primary instruction.
	*/
	EncodeDualIssueVectorPrimarySources(psOp1Inst, 
										eOp1,
										bOp1IsVec4,
										puInst,
										psContext,
										bSrc2SlotUsed,
										pePrimaryMap,
										psTarget);

	/*
		Encode the sources to the secondary instruction.
	*/
	EncodeDualIssueVectorSecondarySources(psOp2Inst,
										  eOp2,
										  bOp2IsVec4,
										  puInst,
										  psContext,
										  bSrc2SlotUsed,
										  peSecondaryMap,
										  psTarget);
}

#define SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		uEncodedSwizzle = SGXVEC_USE_VECNONMAD_SRC2_SWIZ_##X##Y##Z##W; \
		break; \
	}	

/*****************************************************************************
 FUNCTION	: EncodeVEC4NONMADInstruction_Src2Swizzle

 PURPOSE	: Encode the swizzle to a F16/F32 vector MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  uHwArg				- Index of the hardware argument to encode
									into.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.


 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4NONMADInstruction_Src2Swizzle(PUSE_INST			psInst,
												 IMG_UINT32			uArg,
												 IMG_PUINT32		puInst,
												 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;

	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_SWIZZLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
		return;
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	switch (uInputSwizzle)
	{
		SWIZ_CASE(X, X, X, X)
		SWIZ_CASE(Y, Y, Y, Y)
		SWIZ_CASE(Z, Z, Z, Z)
		SWIZ_CASE(W, W, W, W)
		SWIZ_CASE(X, Y, Z, W)
		SWIZ_CASE(Y, Z, W, W)
		SWIZ_CASE(X, Y, Z, Z)
		SWIZ_CASE(X, X, Y, Z)
		SWIZ_CASE(X, Y, X, Y)
		SWIZ_CASE(X, Y, W, Z)
		SWIZ_CASE(Z, X, Y, W)
		SWIZ_CASE(Z, W, Z, W)
		SWIZ_CASE(Y, Z, X, Z)
		SWIZ_CASE(X, X, Y, Y)
		SWIZ_CASE(X, Z, W, W)
		SWIZ_CASE(X, Y, Z, 1)

		default:
		{
			uEncodedSwizzle = 0;
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "This swizzle isn't available on a %s instruction",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
	}

	puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_VECNONMAD_SRC2SWIZ_SHIFT);
}
#undef SWIZ_CASE

/*****************************************************************************
 FUNCTION	: EncodeVEC4NONMADInstruction_Swizzle

 PURPOSE	: Encode the swizzle to a F16/F32 vec4/vec2 instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  uHwArg				- Index of the hardware argument to encode
									into.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4NONMADInstruction_Src1Swizzle(PUSE_INST			psInst,
												 IMG_UINT32			uArg,
												 IMG_PUINT32		puInst,
												 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;
	IMG_UINT32	uChan;

	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_SWIZZLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
		return;
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	uEncodedSwizzle = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32	uInputSel;
		IMG_UINT32	uHwSel = USE_UNDEF;

		uInputSel = (uInputSwizzle >> (uChan * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;
		switch (uInputSel)
		{
			case USEASM_SWIZZLE_SEL_X:	
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_X;
				break;
			}
			case USEASM_SWIZZLE_SEL_Y:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_Y;
				break;
			}
			case USEASM_SWIZZLE_SEL_Z:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_Z;
				break;
			}
			case USEASM_SWIZZLE_SEL_W:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_W;
				break;
			}
			case USEASM_SWIZZLE_SEL_1:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_1;
				break;
			}
			case USEASM_SWIZZLE_SEL_0:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_0;
				break;
			}
			case USEASM_SWIZZLE_SEL_2:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_2;
				break;
			}
			case USEASM_SWIZZLE_SEL_HALF:
			{
				uHwSel = SGXVEC_USE_VECNONMAD_SRC1SWIZ_SEL_HALF;
				break;
			}
		}

		switch (uChan)
		{
			case 0: uEncodedSwizzle |= (uHwSel << SGXVEC_USE_VECNONMAD_SRC1SWIZ_X_SHIFT); break;
			case 1: uEncodedSwizzle |= (uHwSel << SGXVEC_USE_VECNONMAD_SRC1SWIZ_Y_SHIFT); break;
			case 2: uEncodedSwizzle |= (uHwSel << SGXVEC_USE_VECNONMAD_SRC1SWIZ_Z_SHIFT); break;
			case 3: uEncodedSwizzle |= (uHwSel << SGXVEC_USE_VECNONMAD_SRC1SWIZ_W_SHIFT); break;
		}
	}

	puInst[0] |= (((uEncodedSwizzle >> 0) & 0x7F) << SGXVEC_USE0_VECNONMAD_SRC1SWIZ_BITS06_SHIFT);
	puInst[1] |= (((uEncodedSwizzle >> 7) & 0x03) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT78_SHIFT);
	puInst[1] |= (((uEncodedSwizzle >> 9) & 0x01) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BIT9_SHIFT);
	puInst[1] |= (((uEncodedSwizzle >> 10) & 0x03) << SGXVEC_USE1_VECNONMAD_SRC1SWIZ_BITS1011_SHIFT);
}

#define SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		uEncodedSwizzle = SGXVEC_USE_VECMAD_SRC0SWIZZLE_##X##Y##Z##W; \
		break; \
	}	

/*****************************************************************************
 FUNCTION	: EncodeVEC4MadInstruction_Src0Swizzle

 PURPOSE	: Encode the swizzle for source 0 to a F16/F32 vector MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.


 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4MadInstruction_Src0Swizzle(PUSE_INST			psInst,
											  IMG_UINT32		uArg,
											  IMG_PUINT32		puInst,
											  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;

	if (!CheckSwizzleArgument(psInst, uArg, psContext))
	{
		return;
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	switch (uInputSwizzle)
	{
		SWIZ_CASE(X, X, X, X)
		SWIZ_CASE(Y, Y, Y, Y)
		SWIZ_CASE(Z, Z, Z, Z)
		SWIZ_CASE(W, W, W, W)
		SWIZ_CASE(X, Y, Z, W)
		SWIZ_CASE(Y, Z, X, W)
		SWIZ_CASE(X, Y, W, W)
		SWIZ_CASE(Z, W, X, Y)
		default:
		{
			uEncodedSwizzle = 0;
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "This swizzle isn't available on a %s instruction",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
	}

	puInst[0] |= (((uEncodedSwizzle >> 0) & 0x3) << SGXVEC_USE0_VECMAD_SRC0SWIZ_BITS01_SHIFT);
	puInst[1] |= (((uEncodedSwizzle >> 2) & 0x1) << SGXVEC_USE1_VECMAD_SRC0SWIZ_BIT2_SHIFT);
}
#undef SWIZ_CASE

#define SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		uEncodedSwizzle = SGXVEC_USE_VECMAD_SRC1SWIZZLE_##X##Y##Z##W; \
		break; \
	}	

/*****************************************************************************
 FUNCTION	: EncodeVEC4MadInstruction_Src1Swizzle

 PURPOSE	: Encode the swizzle for source 1 to a F16/F32 vector MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.


 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4MadInstruction_Src1Swizzle(PUSE_INST			psInst,
											  IMG_UINT32		uArg,
											  IMG_PUINT32		puInst,
											  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;

	if (!CheckSwizzleArgument(psInst, uArg, psContext))
	{
		return;
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	switch (uInputSwizzle)
	{
		SWIZ_CASE(X, X, X, X)
		SWIZ_CASE(Y, Y, Y, Y)
		SWIZ_CASE(Z, Z, Z, Z)
		SWIZ_CASE(W, W, W, W)
		SWIZ_CASE(X, Y, Z, W)
		SWIZ_CASE(X, Y, Y, Z)
		SWIZ_CASE(Y, Y, W, W)
		SWIZ_CASE(W, Y, Z, W)
		default:
		{
			uEncodedSwizzle = 0;
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "This swizzle isn't available on a %s instruction",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
	}

	puInst[0] |= (((uEncodedSwizzle >> 0) & 0x3) << SGXVEC_USE0_VECMAD_SRC1SWIZ_BITS01_SHIFT);
	puInst[1] |= (((uEncodedSwizzle >> 2) & 0x1) << SGXVEC_USE1_VECMAD_SRC1SWIZ_BIT2_SHIFT);
}
#undef SWIZ_CASE

#define SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		uEncodedSwizzle = SGXVEC_USE_VECMAD_SRC2SWIZZLE_##X##Y##Z##W; \
		break; \
	}	

/*****************************************************************************
 FUNCTION	: EncodeVEC4MadInstruction_Src2Swizzle

 PURPOSE	: Encode the swizzle for source 2 to a F16/F32 vector MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.


 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4MadInstruction_Src2Swizzle(PUSE_INST			psInst,
											  IMG_UINT32		uArg,
											  IMG_PUINT32		puInst,
											  PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;

	if (!CheckSwizzleArgument(psInst, uArg, psContext))
	{
		return;
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	switch (uInputSwizzle)
	{
		SWIZ_CASE(X, X, X, X)
		SWIZ_CASE(Y, Y, Y, Y)
		SWIZ_CASE(Z, Z, Z, Z)
		SWIZ_CASE(W, W, W, W)
		SWIZ_CASE(X, Y, Z, W)
		SWIZ_CASE(X, Z, W, W)
		SWIZ_CASE(X, X, Y, Z)
		SWIZ_CASE(X, Y, Z, Z)
		default:
		{
			uEncodedSwizzle = 0;
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "This swizzle isn't available on a %s instruction",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
	}

	puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_VECMAD_SRC2SWIZ_BITS02_SHIFT);
}
#undef SWIZ_CASE

/*****************************************************************************
 FUNCTION	: EncodeVEC4MadInstruction

 PURPOSE	: Encode a vec4 float16 or vec2 float32 MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4MadInstruction(PUSE_INST			psInst,
								  IMG_PUINT32		puInst,
								  PUSEASM_CONTEXT	psContext,
								  PCSGX_CORE_DESC	psTarget)
{
	IMG_UINT32	uInputWriteMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32	uHwWriteMask;
	IMG_UINT32	uSrcMod1Flags, uSrcMod2Flags;
	DATA_FORMAT	eDestFormat;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "%s isn't supported on this core",
					   OpcodeName(psInst->uOpcode)));
	}

	/*
		Encode instruction flags and opcode.
	*/
	CheckFlags(psContext, 
			   psInst,
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_SYNCSTART | 
			   USEASM_OPFLAGS1_NOSCHED |
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK),
			   0,
			   0);
	puInst[0] = 0;
	puInst[1] = (SGXVEC_USE1_OP_VECMAD << EURASIA_USE1_OP_SHIFT) |
				(EncodeShortVectorPredicate(psContext, psInst) << SGXVEC_USE1_VECMAD_SVPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_VECMAD_NOSCHED : 0);

	/*
		Encode instruction format.
	*/
	if (psInst->uOpcode == USEASM_OP_VF16MAD)
	{
		puInst[1] |= SGXVEC_USE1_VECMAD_F16;
		eDestFormat = DATA_FORMAT_F16;
	}
	else
	{
		assert(psInst->uOpcode == USEASM_OP_VMAD);
		eDestFormat = DATA_FORMAT_F32;
	}

	/*
		Encode destination mask.
	*/
	uHwWriteMask = 
		EncodeVecDestinationWriteMask(psContext, 
									  psInst, 
									  &psInst->asArg[0], 
									  uInputWriteMask,
									  eDestFormat,
									  IMG_FALSE /* bC10F16MasksUnrestricted */);
	puInst[1] |= (uHwWriteMask << SGXVEC_USE1_VECMAD_DMSK_SHIFT);

	/*
		Encode destination.
	*/
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_DBEXT,
							      puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_VECMAD_DST_SHIFT /* uNumberFieldShift */,
								  USEASM_HWARG_DESTINATION,
								  0 /* uArg */,
								  psTarget);

	/*
		Encode source 0.
	*/
	CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_FALSE /* bAllowExtended */,
								  0 /* uBankExtension */,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_VECMAD_SRC0_SHIFT /* uNumberFieldShift */,
								  USEASM_HWARG_SOURCE0,
								  1 /* uArg */,
								  psTarget);

	/*
		Encode source 0 swizzle.
	*/
	EncodeVEC4MadInstruction_Src0Swizzle(psInst,
										 2 /* uArg */,
										 puInst,
										 psContext);

	/*
		Encode source 1.
	*/
	CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_S1BEXT /* uBankExtension */,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_VECMAD_SRC1_SHIFT /* uNumberFieldShift */,
								  USEASM_HWARG_SOURCE1,
								  3 /* uArg */,
								  psTarget);

	/*
		Encode source 1 swizzle.
	*/
	EncodeVEC4MadInstruction_Src1Swizzle(psInst,
										 4 /* uArg */,
										 puInst,
										 psContext);

	/*
		Encode source 2.
	*/
	CheckArgFlags(psContext, psInst, 5, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_S2BEXT /* uBankExtension */,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_VECMAD_SRC2_SHIFT /* uNumberFieldShift */,
								  USEASM_HWARG_SOURCE2,
								  5  /* uArg */,
								  psTarget);

	/*
		Encode source 2 swizzle.
	*/
	EncodeVEC4MadInstruction_Src2Swizzle(psInst,
										 6 /* uArg */,
										 puInst,
										 psContext);

	/*
		Encode source modifiers.
	*/
	uSrcMod1Flags = psInst->asArg[3].uFlags;
	uSrcMod2Flags = psInst->asArg[5].uFlags;
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_NEGATE)
	{
		uSrcMod1Flags ^= USEASM_ARGFLAGS_NEGATE;
	}
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_ABSOLUTE)
	{
		puInst[1] |= SGXVEC_USE1_VECMAD_SRC0ABS;
	}
	puInst[1] |= (EncodeFloatMod(uSrcMod1Flags) << SGXVEC_USE1_VECMAD_SRC1MOD_SHIFT) |
				 (EncodeFloatMod(uSrcMod2Flags) << SGXVEC_USE1_VECMAD_SRC2MOD_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodeVEC4NONMADInstruction

 PURPOSE	: Encode a vec4 float16 or vec2 float32 non-MAD instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVEC4NONMADInstruction(PUSE_INST			psInst,
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext,
									 PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32	uInputWriteMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32	uHwWriteMask;
	IMG_UINT32	uSrcMod1Flags, uSrcMod2Flags;
	IMG_UINT32	uOp2;
	IMG_BOOL	bF16;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "%s isn't supported on this core",
					   OpcodeName(psInst->uOpcode)));
	}

	/*
		Encode instruction flags and opcode.
	*/
	CheckFlags(psContext, 
			   psInst,
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_SYNCSTART | 
			   USEASM_OPFLAGS1_NOSCHED |
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK),
			   0,
			   0);
	puInst[0] = 0;
	puInst[1] = (EncodeExtendedVectorPredicate(psContext, psInst) << SGXVEC_USE1_VECNONMAD_EVPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_VECNONMAD_NOSCHED : 0);

	/*
		Encode subopcode.
	*/
	switch (psInst->uOpcode)
	{
		/*
			F32 opcodes.
		*/
		case USEASM_OP_VMUL: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMUL; bF16 = IMG_FALSE; break;
		case USEASM_OP_VADD: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VADD; bF16 = IMG_FALSE; break;
		case USEASM_OP_VFRC: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VFRC; bF16 = IMG_FALSE; break;
		case USEASM_OP_VDSX: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDSX; bF16 = IMG_FALSE; break;
		case USEASM_OP_VDSY: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDSY; bF16 = IMG_FALSE; break;
		case USEASM_OP_VMIN: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMIN; bF16 = IMG_FALSE; break;
		case USEASM_OP_VMAX: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMAX; bF16 = IMG_FALSE; break;
		case USEASM_OP_VDP: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDP; bF16 = IMG_FALSE; break;

		/*
			F16 opcodes.
		*/
		case USEASM_OP_VF16MUL: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMUL; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16ADD: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VADD; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16FRC: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VFRC; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16DSX: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDSX; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16DSY: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDSY; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16MIN: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMIN; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16MAX: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VMAX; bF16 = IMG_TRUE; break;
		case USEASM_OP_VF16DP: uOp2 = SGXVEC_USE0_VECNONMAD_OP2_VDP; bF16 = IMG_TRUE; break;
		default: IMG_ABORT();
	}
	puInst[0] |= (uOp2 << SGXVEC_USE0_VECNONMAD_OP2_SHIFT);
	if (bF16)
	{
		puInst[1] |= (SGXVEC_USE1_OP_VECNONMADF16 << EURASIA_USE1_OP_SHIFT);
	}
	else
	{
		puInst[1] |= (SGXVEC_USE1_OP_VECNONMADF32 << EURASIA_USE1_OP_SHIFT);
	}
	
	/*
		Encode destination mask.
	*/
	uHwWriteMask = 
		EncodeVecDestinationWriteMask(psContext,
									  psInst, 
									  &psInst->asArg[0], 
									  uInputWriteMask,
									  bF16 ? DATA_FORMAT_F16 : DATA_FORMAT_F32, 
									  IMG_TRUE /* bC10F16MasksUnrestricted */);
	puInst[1] |= (uHwWriteMask << SGXVEC_USE1_VECNONMAD_DMASK_SHIFT);

	/*
		Encode destination.
	*/
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_DBEXT,
							      puInst + 0,
								  puInst + 1,
								  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
								  SGXVEC_USE0_VECNONMAD_DST_SHIFT /* uNumberFieldShift */,
								  USEASM_HWARG_DESTINATION,
								  0 /* uArg */,
								  psTarget);

	/*
		Encode source 1.
	*/
	CheckArgFlags(psContext, psInst, 1, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);
	EncodeWideUSSourceDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S1BEXT /* uBankExtension */,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_VECNONMAD_SRC1_SHIFT /* uNumberFieldShift */,
									  USEASM_HWARG_SOURCE1,
									  1 /* uArg */,
									  psTarget);

	/*
		Encode swizzle for source 1
	*/
	EncodeVEC4NONMADInstruction_Src1Swizzle(psInst,
											2 /* uArg */,
											puInst,
											psContext);

	/*
		Encode source 1 modifiers.
	*/
	uSrcMod1Flags = psInst->asArg[1].uFlags;
	if (psInst->uOpcode == USEASM_OP_VMUL || psInst->uOpcode == USEASM_OP_VF16MUL)
	{
		if (psInst->asArg[3].uFlags & USEASM_ARGFLAGS_NEGATE)
		{
			uSrcMod1Flags ^= USEASM_ARGFLAGS_NEGATE;
		}
	}
	puInst[1] |= (EncodeFloatMod(uSrcMod1Flags) << SGXVEC_USE1_VECNONMAD_SRC1MOD_SHIFT);

	/*
		Encode source 2.
	*/
	if (psInst->uOpcode == USEASM_OP_VMUL || psInst->uOpcode == USEASM_OP_VF16MUL)
	{
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_NEGATE | USEASM_ARGFLAGS_ABSOLUTE);
	}
	else
	{
		CheckArgFlags(psContext, psInst, 3, USEASM_ARGFLAGS_ABSOLUTE);
	}
	uSrcMod2Flags = psInst->asArg[3].uFlags;
	if (IsValidSrc2MappingToSrc0Bank(psContext, psInst, &psInst->asArg[3]))
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S2BEXT /* uBankExtension */,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_VECNONMAD_SRC2_SHIFT /* uNumberFieldShift */,
									  USEASM_HWARG_SOURCE2,
									  3 /* uArg */,
									  psTarget);
	}

	/*
		Encode swizzle for source 2
	*/
	EncodeVEC4NONMADInstruction_Src2Swizzle(psInst,
											4 /* uArg */,
											puInst,
											psContext);

	/*
		Encode source 2 modifier.
	*/
	if (uSrcMod2Flags & USEASM_ARGFLAGS_ABSOLUTE)
	{
		puInst[1] |= SGXVEC_USE1_VECNONMAD_SRC2ABS;
	}
}

/*****************************************************************************
 FUNCTION	: EncodeVMOVCTest

 PURPOSE	: Encode a VMOVC test.

 PARAMETERS	: psInst			- Input instruction.
			  puInst			- Written with the encoded test.
			  psContext			- Assembler context.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID EncodeVMOVCTest(PUSE_INST		psInst,
							    IMG_PUINT32		puInst,
							    PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32 uSignTest = (psInst->uTest & ~USEASM_TEST_SIGN_CLRMSK) >> USEASM_TEST_SIGN_SHIFT;
	IMG_UINT32 uZeroTest = (psInst->uTest & ~USEASM_TEST_ZERO_CLRMSK) >> USEASM_TEST_ZERO_SHIFT;
	IMG_UINT32 uCombineOp = (psInst->uTest & USEASM_TEST_CRCOMB_AND);

	if (uSignTest != USEASM_TEST_SIGN_TRUE)
	{
		puInst[1] |= SGXVEC_USE1_VMOVC_TESTCCEXT;

		if (uSignTest == USEASM_TEST_SIGN_NEGATIVE &&
			uZeroTest == USEASM_TEST_ZERO_ZERO && 
			uCombineOp == USEASM_TEST_CRCOMB_OR)
		{
			puInst[1] |= (SGXVEC_USE1_VMOVC_TESTCC_EXTLTEZERO << SGXVEC_USE1_VMOVC_TESTCC_SHIFT);
		}
		else if (uSignTest == USEASM_TEST_SIGN_NEGATIVE &&
				 uZeroTest == USEASM_TEST_ZERO_NONZERO && 
				 uCombineOp == USEASM_TEST_CRCOMB_AND)
		{
			puInst[1] |= (SGXVEC_USE1_VMOVC_TESTCC_EXTLTZERO << SGXVEC_USE1_VMOVC_TESTCC_SHIFT);
		}
		else
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only .tz, .t!z, .tn or .tn|z are valid tests for movc"));
		}
	}
	else
	{
		if (uZeroTest == USEASM_TEST_ZERO_ZERO)
		{
			puInst[1] |= (SGXVEC_USE1_VMOVC_TESTCC_STDZERO << SGXVEC_USE1_VMOVC_TESTCC_SHIFT);
		}
		else
		{
			puInst[1] |= (SGXVEC_USE1_VMOVC_TESTCC_STDNONZERO << SGXVEC_USE1_VMOVC_TESTCC_SHIFT);
		}
	}
}

#define SWIZ_CASE(X, Y, Z, W)	\
	case USEASM_SWIZZLE(X, Y, Z, W): \
	{	\
		uEncodedSwizzle = SGXVEC_USE1_VMOVC_SWIZ_##X##Y##Z##W; \
		break; \
	}	

/*****************************************************************************
 FUNCTION	: EncodeVecMovInstruction_Swizzle

 PURPOSE	: Encode the swizzle to a vector MOV/MOVC/MOVCU8 instruction.

 PARAMETERS	: psInst				- Input instruction.
			  uArg					- Index of the instruction argument that
									contains the swizzle.
			  bIdentitySwizzleOnly	- TRUE if only the identity swizzle is supported by
									the instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.


 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVecMovInstruction_Swizzle(PUSE_INST			psInst,
										 IMG_UINT32			uArg,
										 IMG_BOOL			bIdentitySwizzleOnly,
										 IMG_PUINT32		puInst,
										 PUSEASM_CONTEXT	psContext)
{
	IMG_UINT32	uEncodedSwizzle;
	IMG_UINT32	uInputSwizzle;

	if (psInst->asArg[uArg].uType != USEASM_REGTYPE_SWIZZLE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "Argument %d to a %s instruction must be a swizzle",
					   uArg, OpcodeName(psInst->uOpcode)));
		return;
	}
	if (psInst->asArg[uArg].uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "A swizzle can't be accessed in indexed mode"));
	}
	if (psInst->asArg[uArg].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for a swizzle"));
	}
	uInputSwizzle = psInst->asArg[uArg].uNumber;

	if (bIdentitySwizzleOnly && uInputSwizzle != USEASM_SWIZZLE(X, Y, Z, W))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A %s instruction with this data type doesn't support a swizzle", 
			OpcodeName(psInst->uOpcode)));
		return;
	}

	switch (uInputSwizzle)
	{
		SWIZ_CASE(X, X, X, X)
		SWIZ_CASE(Y, Y, Y, Y)
		SWIZ_CASE(Z, Z, Z, Z)
		SWIZ_CASE(W, W, W, W)
		SWIZ_CASE(X, Y, Z, W)
		SWIZ_CASE(Y, Z, W, W)
		SWIZ_CASE(X, Y, Z, Z)
		SWIZ_CASE(X, X, Y, Z)
		SWIZ_CASE(X, Y, X, Y)
		SWIZ_CASE(X, Y, W, Z)
		SWIZ_CASE(Z, X, Y, W)
		SWIZ_CASE(Z, W, Z, W)
		SWIZ_CASE(Y, Z, X, Z)
		SWIZ_CASE(X, X, Y, Y)
		SWIZ_CASE(X, Z, W, W)
		SWIZ_CASE(X, Y, Z, 1)

		default:
		{
			uEncodedSwizzle = 0;
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "This swizzle isn't available on a %s instruction",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
	}

	puInst[1] |= (uEncodedSwizzle << SGXVEC_USE1_VMOVC_SWIZ_SHIFT);
}
#undef SWIZ_CASE

/*****************************************************************************
 FUNCTION	: EncodeVecMovInstruction

 PURPOSE	: Encode a VMOV, VMOVC or VMOVCU8 instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVecMovInstruction(PUSE_INST			psInst,
								 IMG_PUINT32		puInst,
								 PUSEASM_CONTEXT	psContext,
								 PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32	uMType;
	IMG_UINT32	uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32	uInputDestMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32	uSwizArg;
	IMG_UINT32	uSrc1Arg;
	IMG_UINT32	uValidFlags1;
	IMG_BOOL	bDoubleRegistersDataType;
	IMG_BOOL	bMaskSwizzleSupported;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "%s isn't supported on this core",
					   OpcodeName(psInst->uOpcode)));
	}

	/*
		Encode instruction flags and opcode.
	*/
	uValidFlags1 = USEASM_OPFLAGS1_SKIPINVALID | 
				   USEASM_OPFLAGS1_SYNCSTART | 
				   USEASM_OPFLAGS1_NOSCHED |
				   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
				   USEASM_OPFLAGS1_END | 
				   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
				   USEASM_OPFLAGS1_MOVC_FMTI8 |
				   USEASM_OPFLAGS1_MOVC_FMTF16 |
				   (~USEASM_OPFLAGS1_PRED_CLRMSK);
	if (psInst->uOpcode == USEASM_OP_VMOVC || psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		uValidFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
	}
	CheckFlags(psContext, 
			   psInst,
			   uValidFlags1,
			   USEASM_OPFLAGS2_MOVC_FMTI10 |
			   USEASM_OPFLAGS2_MOVC_FMTI16 |
			   USEASM_OPFLAGS2_MOVC_FMTI32 |
			   USEASM_OPFLAGS2_MOVC_FMTF32,
			   0);
	puInst[0] = 0;
	puInst[1] = (SGXVEC_USE1_OP_VECMOV << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, IMG_FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_VMOVC_NOSCHED : 0);

	/*
		Encode repeat count
	*/
	if (uRptCount > 0)
	{
		if (uRptCount > SGXVEC_USE1_VMOVC_RCNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "The maximum repeat count on %s is %d",
						   OpcodeName(psInst->uOpcode), SGXVEC_USE1_VMOVC_RCNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_VMOVC_RCNT_SHIFT);
	}

	/*
		Encode mov type.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_VMOV: uMType = SGXVEC_USE1_VMOVC_MTYPE_UNCONDITIONAL; break;
		case USEASM_OP_VMOVC: uMType = SGXVEC_USE1_VMOVC_MTYPE_CONDITIONAL; break;
		case USEASM_OP_VMOVCU8: uMType = SGXVEC_USE1_VMOVC_MTYPE_U8CONDITIONAL; break;
		default: IMG_ABORT();
	}
	puInst[1] |= (uMType << SGXVEC_USE1_VMOVC_MTYPE_SHIFT);

	/*
		Encode test condition.
	*/
	if (psInst->uOpcode == USEASM_OP_VMOVC || psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		if (!(psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE))
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"A %s instruction must include a test (.tz, .t!z, .tn or .tn|z)", OpcodeName(psInst->uOpcode)));
		}
		EncodeVMOVCTest(psInst, puInst, psContext);
	}

	/*
		Encode move data type.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTI8)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_INT8 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_FALSE;
		bMaskSwizzleSupported = IMG_FALSE;
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI16)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_INT16 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_FALSE;
		bMaskSwizzleSupported = IMG_FALSE;
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI32)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_INT32 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_FALSE;
		bMaskSwizzleSupported = IMG_FALSE;
	}
	else if (psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTF16)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_F16 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_TRUE;
		bMaskSwizzleSupported = IMG_TRUE;
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTF32)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_F32 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_TRUE;
		bMaskSwizzleSupported = IMG_TRUE;
	}
	else if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTI10)
	{
		puInst[1] |= (SGXVEC_USE1_VMOVC_MDTYPE_C10 << SGXVEC_USE1_VMOVC_MDTYPE_SHIFT);
		bDoubleRegistersDataType = IMG_TRUE;
		bMaskSwizzleSupported = IMG_FALSE;
	}
	else
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A movc instruction must specify the data type (.i8, .i10, .i16, .i32, .flt or .flt16)"));
		return;
	}

	/*
		Encode destination write mask.
	*/
	if (!bMaskSwizzleSupported)
	{
		/*
			For some data types all destination channels must be written.
		*/
		if (uInputDestMask != USEREG_MASK_XYZW)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "A %s instruction with this data type doesn't support a destination mask", OpcodeName(psInst->uOpcode)));
		}
		puInst[0] |= (USEREG_MASK_XYZW << SGXVEC_USE0_VMOVC_WMASK_SHIFT);
	}
	else
	{
		DATA_FORMAT	eDestFormat;
		IMG_UINT32	uHwDestMask;

		/*
			Get the format of the destination.
		*/
		if (psInst->uFlags2 & USEASM_OPFLAGS2_MOVC_FMTF32)
		{
			eDestFormat = DATA_FORMAT_F32;
		}
		else
		{
			assert(psInst->uFlags1 & USEASM_OPFLAGS1_MOVC_FMTF16);
			eDestFormat = DATA_FORMAT_F16;
		}

		/*
			Get the encoded destination write mask.
		*/
		uHwDestMask = 
			EncodeVecDestinationWriteMask(psContext,
										  psInst,
										  &psInst->asArg[0],
										  uInputDestMask,
										  eDestFormat,
										  IMG_FALSE /* bC10F16MasksUnrestricted */);

		/*
			Set into the hardware instruction.
		*/
		puInst[0] |= (uHwDestMask << SGXVEC_USE0_VMOVC_WMASK_SHIFT);
	}
	

	/*
		Encode source swizzle.
	*/
	if (psInst->uOpcode == USEASM_OP_VMOVC || psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		uSwizArg = 4;
	}
	else
	{
		uSwizArg = 2;
	}
	EncodeVecMovInstruction_Swizzle(psInst,
									uSwizArg,
									!bMaskSwizzleSupported,
									puInst,
									psContext);

	/*
		Encode destination.
	*/
	if (bDoubleRegistersDataType)
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_DBEXT,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_VMOVC_DST_SHIFT,
									  USEASM_HWARG_DESTINATION,
									  0 /* uArg */,
									  psTarget);
	}
	else
	{
		EncodeArgument(psContext,
					   psInst,
					   USEASM_HWARG_DESTINATION,
					   0 /* uInputArg */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_DBEXT,
					   IMG_FALSE /* bSigned */,
					   puInst + 0,
					   puInst + 1,
					   IMG_FALSE /* bBitwise */,
					   IMG_FALSE /* bFmtSelect */,
					   0 /* uFmtFlag */,
					   psTarget,
					   SGXVEC_USE0_VMOVC_DST_SHIFT,
					   SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH);
	}

	/*
		Encode source 0.
	*/
	if (psInst->uOpcode == USEASM_OP_VMOVC || psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		IMG_UINT32	uComponent;

		if (psInst->uOpcode == USEASM_OP_VMOVCU8)
		{
			CheckArgFlags(psContext, psInst, 1, (~USEASM_ARGFLAGS_COMP_CLRMSK));
		}
		else
		{
			CheckArgFlags(psContext, psInst, 1, 0);
		}
		if (bDoubleRegistersDataType)
		{
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_FALSE /* bAllowExtended */,
										  0 /* uBankExtension */,
										  puInst + 0,
										  puInst + 1,
										  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
										  SGXVEC_USE0_VMOVC_SRC0_SHIFT,
										  USEASM_HWARG_SOURCE0,
										  1 /* uArg */,
										  psTarget);
		}
		else
		{
			EncodeArgument(psContext,
						   psInst,
						   USEASM_HWARG_SOURCE0,
						   1 /* uInputArg */,
						   IMG_FALSE /* bAllowExtended */,
						   0 /* uBankExtension */,
						   IMG_FALSE /* bSigned */,
						   puInst + 0,
						   puInst + 1,
						   IMG_FALSE /* bBitwise */,
						   IMG_FALSE /* bFmtSelect */,
						   0 /* uFmtFlag */,
						   psTarget,
						   SGXVEC_USE0_VMOVC_SRC0_SHIFT,
						   SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH);
		}

		/*
			Encode source 0 component select.
		*/
		uComponent = (psInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
		if (uComponent == 2)
		{
			puInst[1] |= SGXVEC_USE1_VMOVC_S0ULSEL;
		}
		else if (uComponent != 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "The component select on source to %s must be either .0 or .2",
						   OpcodeName(psInst->uOpcode)));
		}

		uSrc1Arg = 2;
	}
	else
	{
		uSrc1Arg = 1;
	}

	/*
		Encode source 1.
	*/
	CheckArgFlags(psContext, psInst, uSrc1Arg, 0);
	if (bDoubleRegistersDataType)
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S1BEXT /* uBankExtension */,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_VMOVC_SRC1_SHIFT,
									  USEASM_HWARG_SOURCE1,
									  uSrc1Arg /* uArg */,
									  psTarget);
	}
	else
	{
		EncodeArgument(psContext,
					   psInst,
					   USEASM_HWARG_SOURCE1,
					   uSrc1Arg /* uInputArg */,
					   IMG_TRUE /* bAllowExtended */,
					   EURASIA_USE1_S1BEXT,
					   IMG_FALSE /* bSigned */,
					   puInst + 0,
					   puInst + 1,
					   IMG_FALSE /* bBitwise */,
					   IMG_FALSE /* bFmtSelect */,
					   0 /* uFmtFlag */,
					   psTarget,
					   SGXVEC_USE0_VMOVC_SRC1_SHIFT,
					   SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH);
	}

	/*
		Encode source 2.
	*/
	if (psInst->uOpcode == USEASM_OP_VMOVC || psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		CheckArgFlags(psContext, psInst, 3, 0);
		if (bDoubleRegistersDataType)
		{	
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_S2BEXT /* uBankExtension */,
										  puInst + 0,
										  puInst + 1,
										  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
										  SGXVEC_USE0_VMOVC_SRC2_SHIFT,
										  USEASM_HWARG_SOURCE2,
										  3 /* uArg */,
										  psTarget);
		}
		else
		{
			EncodeArgument(psContext,
						   psInst,
						   USEASM_HWARG_SOURCE2,
						   3 /* uInputArg */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S2BEXT,
						   IMG_FALSE /* bSigned */,
						   puInst + 0,
						   puInst + 1,
						   IMG_FALSE /* bBitwise */,
						   IMG_FALSE /* bFmtSelect */,
						   0 /* uFmtFlag */,
						   psTarget,
						   SGXVEC_USE0_VMOVC_SRC2_SHIFT,
						   SGXVEC_USE0_VMOVC_NONVEC_REGISTER_NUMBER_FIELD_LENGTH);
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeVecComplexInstruction

 PURPOSE	: Encode a vector complex (RCP, RSQ, LOG, EXP) instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeVecComplexInstruction(PUSE_INST			psInst,
									 IMG_PUINT32		puInst,
									 PUSEASM_CONTEXT	psContext,
									 PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32	uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32	uInputWriteMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	DATA_FORMAT	eDestFormat;
	IMG_UINT32	uOp2;
	IMG_UINT32	uSrcComp;
	IMG_UINT32	uSrcType;
	IMG_UINT32	uDestType;
	IMG_UINT32	uHwWriteMask;

	/*
		Encode instruction flags and opcode.
	*/
	CheckFlags(psContext, 
			   psInst,
			   USEASM_OPFLAGS1_SKIPINVALID | 
			   USEASM_OPFLAGS1_SYNCSTART | 
			   USEASM_OPFLAGS1_NOSCHED |
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) |
			   USEASM_OPFLAGS1_END | 
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK),
			   0,
			   0);
	puInst[0] = 0;
	puInst[1] = (SGXVEC_USE1_OP_VECCOMPLEX << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, IMG_FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_VECCOMPLEXOP_NOSCHED : 0);

	/*
		Encode subopcode.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_VRCP: uOp2 = SGXVEC_USE1_VECCOMPLEXOP_OP2_RCP; break;
		case USEASM_OP_VRSQ: uOp2 = SGXVEC_USE1_VECCOMPLEXOP_OP2_RSQ; break;
		case USEASM_OP_VLOG: uOp2 = SGXVEC_USE1_VECCOMPLEXOP_OP2_LOG; break;
		case USEASM_OP_VEXP: uOp2 = SGXVEC_USE1_VECCOMPLEXOP_OP2_EXP; break;
		default: IMG_ABORT();
	}
	puInst[1] |= (uOp2 << SGXVEC_USE1_VECCOMPLEXOP_OP2_SHIFT);

	/*
		Encode repeat count
	*/
	if (uRptCount > 0)
	{
		if (uRptCount > SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "The maximum repeat count on %s is %d",
						   OpcodeName(psInst->uOpcode), SGXVEC_USE1_VECCOMPLEXOP_RCNT_MAXIMUM));
		}
		puInst[1] |= ((uRptCount - 1) << SGXVEC_USE1_VECCOMPLEXOP_RCNT_SHIFT);
	}

	/*
		Encode destination.
	*/
	CheckArgFlags(psContext, 
				  psInst, 
				  0 /* uArgument */, 
				  USEASM_ARGFLAGS_FMTC10 | USEASM_ARGFLAGS_FMTF16 /* uValidFlags */);
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_DBEXT,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE0_VECCOMPLEXOP_DST_FIELD_LENGTH,
								  SGXVEC_USE0_VECCOMPLEXOP_DST_SHIFT,
								  USEASM_HWARG_DESTINATION,
								  0 /* uArg */,
								  psTarget);

	/*
		Encode destination format.
	*/
	if (psInst->asArg[0].uFlags & USEASM_ARGFLAGS_FMTC10)
	{
		uDestType = SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_C10;
		eDestFormat = DATA_FORMAT_C10;
	}
	else if (psInst->asArg[0].uFlags & USEASM_ARGFLAGS_FMTF16)
	{
		uDestType = SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_F16;
		eDestFormat = DATA_FORMAT_F16;
	}
	else
	{
		uDestType = SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_F32;
		eDestFormat = DATA_FORMAT_F32;
	}
	puInst[1] |= (uDestType << SGXVEC_USE1_VECCOMPLEXOP_DDTYPE_SHIFT);

	/*
		Encode destination mask.
	*/
	uHwWriteMask = 
		EncodeVecDestinationWriteMask(psContext,
									  psInst,
									  &psInst->asArg[0],
									  uInputWriteMask,
									  eDestFormat,
									  IMG_TRUE /* bC10F16MasksUnrestricted */);
	puInst[0] |= (uHwWriteMask << SGXVEC_USE0_VECCOMPLEXOP_WMSK_SHIFT);

	/*
		Encode src1.
	*/
	CheckArgFlags(psContext, 
				  psInst, 
				  0 /* uArgument */, 
				  USEASM_ARGFLAGS_FMTC10 | 
				  USEASM_ARGFLAGS_FMTF16 |
				  USEASM_ARGFLAGS_NEGATE |
				  USEASM_ARGFLAGS_ABSOLUTE |
				  (~USEASM_ARGFLAGS_COMP_CLRMSK) /* uValidFlags */);
	EncodeArgumentDoubleRegisters(psContext,
								  psInst,
								  IMG_TRUE /* bAllowExtended */,
								  EURASIA_USE1_S1BEXT /* uBankExtension */,
								  puInst + 0,
								  puInst + 1,
								  SGXVEC_USE0_VECCOMPLEXOP_SRC1_FIELD_LENGTH,
								  SGXVEC_USE0_VECCOMPLEXOP_SRC1_SHIFT,
								  USEASM_HWARG_SOURCE1,
								  1 /* uArg */,
								  psTarget);
	
	/*
		Encode source format.
	*/
	if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTC10)
	{
		uSrcType = SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_C10;
	}
	else if (psInst->asArg[1].uFlags & USEASM_ARGFLAGS_FMTF16)
	{
		uSrcType = SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_F16;
	}
	else
	{
		uSrcType = SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_F32;
	}
	puInst[1] |= (uSrcType << SGXVEC_USE1_VECCOMPLEXOP_SDTYPE_SHIFT);

	/*
		Encode source modifier.
	*/
	puInst[1] |= (EncodeFloatMod(psInst->asArg[1].uFlags) << SGXVEC_USE1_VECCOMPLEXOP_SRC1MOD_SHIFT);

	/*
		Encode source component select.
	*/
	uSrcComp = (psInst->asArg[1].uFlags & ~USEASM_ARGFLAGS_COMP_CLRMSK) >> USEASM_ARGFLAGS_COMP_SHIFT;
	puInst[1] |= (uSrcComp << SGXVEC_USE1_VECCOMPLEXOP_SRCCHANSEL_SHIFT);
}

static
IMG_UINT32 GetPackSourceFormat(PUSE_INST	psInst)
/*****************************************************************************
 FUNCTION	: GetPackSourceFormat

 PURPOSE	: Get the source format from a VPCKxxyy opcode.

 PARAMETERS	: psInst				- Input instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psInst->uOpcode)
	{
		case USEASM_OP_VPCKU8U8:
		case USEASM_OP_VPCKS8U8:
		case USEASM_OP_VPCKO8U8:
		case USEASM_OP_VPCKU16U8:
		case USEASM_OP_VPCKS16U8:
		case USEASM_OP_VPCKF16U8:
		case USEASM_OP_VPCKF32U8:
		{
			return EURASIA_USE1_PCK_FMT_U8;
		}
		case USEASM_OP_VPCKU8S8:
		case USEASM_OP_VPCKS8S8:
		case USEASM_OP_VPCKO8S8:
		case USEASM_OP_VPCKU16S8:
		case USEASM_OP_VPCKS16S8:
		case USEASM_OP_VPCKF16S8:
		case USEASM_OP_VPCKF32S8:
		{
			return EURASIA_USE1_PCK_FMT_S8;
		}
		case USEASM_OP_VPCKU8O8:
		case USEASM_OP_VPCKS8O8:
		case USEASM_OP_VPCKO8O8:
		case USEASM_OP_VPCKU16O8:
		case USEASM_OP_VPCKS16O8:
		case USEASM_OP_VPCKF16O8:
		case USEASM_OP_VPCKF32O8:
		{
			return EURASIA_USE1_PCK_FMT_O8;
		}
		case USEASM_OP_VPCKU8U16:
		case USEASM_OP_VPCKS8U16:
		case USEASM_OP_VPCKO8U16:
		case USEASM_OP_VPCKU16U16:
		case USEASM_OP_VPCKS16U16:
		case USEASM_OP_VPCKF16U16:
		case USEASM_OP_VPCKF32U16:
		{
			return EURASIA_USE1_PCK_FMT_U16;
		}
		case USEASM_OP_VPCKU8S16:
		case USEASM_OP_VPCKS8S16:
		case USEASM_OP_VPCKO8S16:
		case USEASM_OP_VPCKU16S16:
		case USEASM_OP_VPCKS16S16:
		case USEASM_OP_VPCKF16S16:
		case USEASM_OP_VPCKF32S16:
		{
			return EURASIA_USE1_PCK_FMT_S16;
		}
		case USEASM_OP_VPCKU8F16:
		case USEASM_OP_VPCKS8F16:
		case USEASM_OP_VPCKO8F16:
		case USEASM_OP_VPCKU16F16:
		case USEASM_OP_VPCKS16F16:
		case USEASM_OP_VPCKF16F16:
		case USEASM_OP_VPCKF32F16:
		case USEASM_OP_VPCKC10F16:
		{
			return EURASIA_USE1_PCK_FMT_F16;
		}
		case USEASM_OP_VPCKU8F32:
		case USEASM_OP_VPCKS8F32:
		case USEASM_OP_VPCKO8F32:
		case USEASM_OP_VPCKU16F32:
		case USEASM_OP_VPCKS16F32:
		case USEASM_OP_VPCKF16F32:
		case USEASM_OP_VPCKF32F32:
		case USEASM_OP_VPCKC10F32:
		{
			return EURASIA_USE1_PCK_FMT_F32;
		}
		case USEASM_OP_VPCKF16C10:
		case USEASM_OP_VPCKF32C10:
		case USEASM_OP_VPCKC10C10:
		{
			return EURASIA_USE1_PCK_FMT_C10;
		}
		default: IMG_ABORT();
	}
}

static
IMG_UINT32 GetPackDestinationFormat(PUSE_INST	psInst)
/*****************************************************************************
 FUNCTION	: GetPackDestinationFormat

 PURPOSE	: Get the destination format from a VPCKxxyy opcode.

 PARAMETERS	: psInst				- Input instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psInst->uOpcode)
	{
		case USEASM_OP_VPCKU8U8:
		case USEASM_OP_VPCKU8S8:
		case USEASM_OP_VPCKU8O8:
		case USEASM_OP_VPCKU8U16:
		case USEASM_OP_VPCKU8S16:
		case USEASM_OP_VPCKU8F16:
		case USEASM_OP_VPCKU8F32:
		{
			return EURASIA_USE1_PCK_FMT_U8;
		}
		case USEASM_OP_VPCKS8U8:
		case USEASM_OP_VPCKS8S8:
		case USEASM_OP_VPCKS8O8:
		case USEASM_OP_VPCKS8U16:
		case USEASM_OP_VPCKS8S16:
		case USEASM_OP_VPCKS8F16:
		case USEASM_OP_VPCKS8F32:
		{
			return EURASIA_USE1_PCK_FMT_S8;
		}
		case USEASM_OP_VPCKO8U8:
		case USEASM_OP_VPCKO8S8:
		case USEASM_OP_VPCKO8O8:
		case USEASM_OP_VPCKO8U16:
		case USEASM_OP_VPCKO8S16:
		case USEASM_OP_VPCKO8F16:
		case USEASM_OP_VPCKO8F32:
		{
			return EURASIA_USE1_PCK_FMT_O8;
		}
		case USEASM_OP_VPCKU16U8:
		case USEASM_OP_VPCKU16S8:
		case USEASM_OP_VPCKU16O8:
		case USEASM_OP_VPCKU16U16:
		case USEASM_OP_VPCKU16S16:
		case USEASM_OP_VPCKU16F16:
		case USEASM_OP_VPCKU16F32:
		{
			return EURASIA_USE1_PCK_FMT_U16;
		}
		case USEASM_OP_VPCKS16U8:
		case USEASM_OP_VPCKS16S8:
		case USEASM_OP_VPCKS16O8:
		case USEASM_OP_VPCKS16U16:
		case USEASM_OP_VPCKS16S16:
		case USEASM_OP_VPCKS16F16:
		case USEASM_OP_VPCKS16F32:
		{
			return EURASIA_USE1_PCK_FMT_S16;
		}
		case USEASM_OP_VPCKF16U8:
		case USEASM_OP_VPCKF16S8:
		case USEASM_OP_VPCKF16O8:
		case USEASM_OP_VPCKF16U16:
		case USEASM_OP_VPCKF16S16:
		case USEASM_OP_VPCKF16F16:
		case USEASM_OP_VPCKF16F32:
		case USEASM_OP_VPCKF16C10:
		{
			return EURASIA_USE1_PCK_FMT_F16;
		}
		case USEASM_OP_VPCKF32U8:
		case USEASM_OP_VPCKF32S8:
		case USEASM_OP_VPCKF32O8:
		case USEASM_OP_VPCKF32U16:
		case USEASM_OP_VPCKF32S16:
		case USEASM_OP_VPCKF32F16:
		case USEASM_OP_VPCKF32F32:
		case USEASM_OP_VPCKF32C10:
		{
			return EURASIA_USE1_PCK_FMT_F32;
		}
		case USEASM_OP_VPCKC10F16:
		case USEASM_OP_VPCKC10F32:
		case USEASM_OP_VPCKC10C10:
		{
			return EURASIA_USE1_PCK_FMT_C10;
		}
		default: IMG_ABORT();
	}
}

static
IMG_VOID GetVectorPackConversionRestrictions(IMG_UINT32		uDestinationFormat,
							 			     IMG_UINT32		uSourceFormat,
											 IMG_BOOL		bScale,
											 IMG_PUINT32	puMaxChansConverted,
											 IMG_PUINT32	puMaxChansWritten,
											 IMG_PBOOL		pbSparseSwizzle,
											 IMG_PBOOL		pbReplicateSpecialCase)
/*****************************************************************************
 FUNCTION	: GetVectorPackConversionRestrictions

 PURPOSE	: Get the source format from a VPCKxxyy opcode.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Updated with the encoded instruction.
			  psContext				- Assembly context.
			  psTarget				- Target for assembly.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bDestinationIsFloat;
	IMG_BOOL	bSourceIsFloat;

	/*
		Is the destination format floating point?
	*/
	bDestinationIsFloat = IMG_FALSE;
	if (uDestinationFormat == EURASIA_USE1_PCK_FMT_F32 ||
		uDestinationFormat == EURASIA_USE1_PCK_FMT_F16)
	{
		bDestinationIsFloat = IMG_TRUE;
	}

	*pbReplicateSpecialCase = IMG_FALSE;

	/*
		Is the source format floating point?
	*/
	bSourceIsFloat = IMG_FALSE;
	if (uSourceFormat == EURASIA_USE1_PCK_FMT_F32 ||
		uSourceFormat == EURASIA_USE1_PCK_FMT_F16)
	{
		bSourceIsFloat = IMG_TRUE;
	}
	
	if (bDestinationIsFloat && bSourceIsFloat)
	{
		/*
			Full vector convert between F16/F32 and F16/F32.
		*/
		*puMaxChansConverted = SGXVEC_USE_PCK_F16F32_F16F32_MAX_CONVERTS;
		*puMaxChansWritten = *puMaxChansConverted;
		*pbSparseSwizzle = IMG_TRUE;
		return;
	}

	if (bSourceIsFloat || bDestinationIsFloat)
	{
		IMG_UINT32	uNonFloatFormat;

		if (bDestinationIsFloat)
		{
			uNonFloatFormat = uSourceFormat;
		}
		else
		{
			uNonFloatFormat = uDestinationFormat;
		}

		if (uNonFloatFormat == EURASIA_USE1_PCK_FMT_C10)
		{
			/*
				Full vector convert between F16/F32 and C10.
			*/
			*puMaxChansConverted = SGXVEC_USE_PCK_C10_F16F32_MAX_CONVERTS;
			*puMaxChansWritten = *puMaxChansConverted;
			*pbSparseSwizzle = IMG_TRUE;
			return;
		}
		else if (uNonFloatFormat == EURASIA_USE1_PCK_FMT_U8 && bScale)
		{
			/*
				Full vector convert between normalised U8 and F16/F32.
			*/
			*puMaxChansConverted = SGXVEC_USE_PCK_U8NORM_F16F32_MAX_CONVERTS;
			*puMaxChansWritten = *puMaxChansConverted;
			*pbSparseSwizzle = IMG_TRUE;
			return;
		}
	}


	/*
		Two channels can be converted.
	*/
	*puMaxChansConverted = SGXVEC_USE_PCK_OTHER_MAX_CONVERTS;
	*pbSparseSwizzle = IMG_FALSE;

	/*
		The maximum number of channels written depends on the width of channels
		in the destination format.
	*/
	if (uDestinationFormat == EURASIA_USE1_PCK_FMT_F32)
	{
		*puMaxChansWritten = SGXVEC_USE_PCK_NONVEC_32BIT_MAX_CHANS_WRITTEN;
	}
	else if (uDestinationFormat == EURASIA_USE1_PCK_FMT_U16 ||
			 uDestinationFormat == EURASIA_USE1_PCK_FMT_S16 ||
			 uDestinationFormat == EURASIA_USE1_PCK_FMT_F16)
	{
		*puMaxChansWritten = SGXVEC_USE_PCK_NONVEC_16BIT_MAX_CHANS_WRITTEN;
	}
	else
	{
		*pbReplicateSpecialCase = IMG_TRUE;
		*puMaxChansWritten = SGXVEC_USE_PCK_NONVEC_8BIT_MAX_CHANS_WRITTEN;
	}
}

static
IMG_VOID EncodeVecPackInstruction(PUSE_INST			psInst,
								  IMG_PUINT32		puInst,
								  PUSEASM_CONTEXT	psContext,
							  	  PCSGX_CORE_DESC	psTarget)
/*****************************************************************************
 FUNCTION	: EncodeVecPackInstruction

 PURPOSE	: Get the source format from a VPCKxxyy opcode.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Updated with the encoded instruction.
			  psContext				- Assembly context.
			  psTarget				- Target for assembly.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRepeatCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;
	IMG_UINT32	uDestMask = (psInst->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK) >> USEASM_OPFLAGS1_MASK_SHIFT;
	IMG_UINT32	uDestinationFormat;
	IMG_BOOL	bDestinationIsFloat;
	IMG_UINT32	uSourceFormat;
	IMG_BOOL	bSourceIsFloat;
	IMG_UINT32	uSwizzleArg;
	IMG_UINT32	uMaxChansConverted;
	IMG_UINT32	uMaxChanWritten;
	IMG_BOOL	bReplicateSpecialCase;
	IMG_BOOL	bSparseSwizzle;
	IMG_UINT32	uSetBitCount;
	IMG_UINT32	uHighestSetBit;
	IMG_UINT32	uChan;
	IMG_BOOL	bValidMask;

	if (!SupportsVEC34(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst,
					   "%s isn't supported on this core",
					   OpcodeName(psInst->uOpcode)));
	}

	CheckFlags(psContext,
			   psInst,
			   USEASM_OPFLAGS1_SKIPINVALID |
			   USEASM_OPFLAGS1_SYNCSTART |
			   USEASM_OPFLAGS1_END |
			   USEASM_OPFLAGS1_NOSCHED |
			   (~USEASM_OPFLAGS1_PRED_CLRMSK) |
			   (~USEASM_OPFLAGS1_REPEAT_CLRMSK) |
			   (~USEASM_OPFLAGS1_MASK_CLRMSK) /* uValidFlags1 */,
			   USEASM_OPFLAGS2_SCALE /* uValidFlags2 */,
			   0 /* uValidFlags3 */);

	/*
		Encode opcode and instruction flags.
	*/
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_PCKUNPCK << EURASIA_USE1_OP_SHIFT) |
				(EncodePredicate(psContext, psInst, IMG_FALSE) << EURASIA_USE1_EPRED_SHIFT) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID) ? EURASIA_USE1_SKIPINV : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_SYNCSTART) ? EURASIA_USE1_SYNCSTART : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_END) ? EURASIA_USE1_END : 0) |
				((psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED) ? SGXVEC_USE1_PCK_NOSCHED : 0);

	/* Encode repeat count and mask */
	if (uRepeatCount > 0)
	{
		puInst[1] |= ((uRepeatCount - 1) << SGXVEC_USE1_PCK_RCOUNT_SHIFT);
	}

	/*
		Encode destination format.
	*/
	uDestinationFormat = GetPackDestinationFormat(psInst);
	puInst[1] |= (uDestinationFormat << EURASIA_USE1_PCK_DSTF_SHIFT);

	/*
		Encode source format.
	*/
	uSourceFormat = GetPackSourceFormat(psInst);
	puInst[1] |= (uSourceFormat << EURASIA_USE1_PCK_SRCF_SHIFT);

	/*
		Encode scale flag.
	*/
	bDestinationIsFloat = IMG_FALSE;
	if (uDestinationFormat == EURASIA_USE1_PCK_FMT_F32 ||
		uDestinationFormat == EURASIA_USE1_PCK_FMT_F16)
	{
		bDestinationIsFloat = IMG_TRUE;
	}
	bSourceIsFloat = IMG_FALSE;
	if (uSourceFormat == EURASIA_USE1_PCK_FMT_F32 ||
		uSourceFormat == EURASIA_USE1_PCK_FMT_F16)
	{
		bSourceIsFloat = IMG_TRUE;
	}
	if (psInst->uFlags2 & USEASM_OPFLAGS2_SCALE)
	{
		if (bDestinationIsFloat == bSourceIsFloat)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, 
				"The scale flag isn't supported with this combination of source and destination formats"));
		}
		puInst[0] |= EURASIA_USE0_PCK_SCALE;
	}

	/* 
		Encode destination bytemask. 
	*/
	puInst[1] |= (uDestMask << EURASIA_USE1_PCK_DMASK_SHIFT);

	/*
		Get the maximum number of channels which the hardware can simultaneously
		convert between these formats.
	*/
	GetVectorPackConversionRestrictions(uDestinationFormat,
										uSourceFormat,
										(psInst->uFlags2 & USEASM_OPFLAGS2_SCALE) != 0 ? IMG_TRUE : IMG_FALSE,
										&uMaxChansConverted,
										&uMaxChanWritten,
										&bSparseSwizzle,
										&bReplicateSpecialCase);

	/*
		When writing 32-bit channels to a unified store destination register reduce the maximum possible channel 
		that can be written since unified store registers are only 64-bits wide.
	*/
	if (psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL && uDestinationFormat == EURASIA_USE1_PCK_FMT_F32)
	{
		uMaxChanWritten = min(uMaxChanWritten, 2);
	}

	/*
		Check the destination write mask is valid.
	*/
	uSetBitCount = 0;
	uHighestSetBit = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		if (uDestMask & (1 << uChan))
		{
			uSetBitCount++;
			uHighestSetBit = uChan;
		}
	}
	bValidMask = IMG_TRUE;
	if (bReplicateSpecialCase)
	{
		/*
			Only two channels can be converted in this case but four channels can be
			set in the destination mask if the first two and last two selectors in
			the swizzle are the same (checked below).
		*/
		if (uSetBitCount == 3)
		{
			bValidMask = IMG_FALSE;
		}
	}
	else
	{
		if (uSetBitCount > uMaxChansConverted || uHighestSetBit >= uMaxChanWritten)
		{
			bValidMask = IMG_FALSE;
		}
	}
	if (!bValidMask)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Invalid destination mask for conversion between these formats"));
	}

	/*
		Encode destination.
	*/
	CheckArgFlags(psContext, psInst, 0 /* uArgument */, 0 /* uValidFlags */);
	EncodeDest(psContext,
			   psInst,
			   IMG_TRUE /* bAllowExtended */,
			   puInst + 0,
			   puInst + 1,
			   IMG_FALSE /* bFmtSelect */,
			   0 /* uFmtFlag */,
			   psTarget);

	/*
		Encode sources.
	*/
	CheckArgFlags(psContext, psInst, 1 /* uArgument */, 0 /* uValidFlags */);
	if (uSourceFormat == EURASIA_USE1_PCK_FMT_F32 ||
		uSourceFormat == EURASIA_USE1_PCK_FMT_F16 ||
		uSourceFormat == EURASIA_USE1_PCK_FMT_C10)
	{
		EncodeArgumentDoubleRegisters(psContext,
									  psInst,
									  IMG_TRUE /* bAllowExtended */,
									  EURASIA_USE1_S1BEXT /* uBankExtension */,
									  puInst + 0,
									  puInst + 1,
									  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
									  SGXVEC_USE0_PCK_64BITSRC1_SHIFT,
									  USEASM_HWARG_SOURCE1,
									  1 /* uArg */,
									  psTarget);

		if (uSourceFormat == EURASIA_USE1_PCK_FMT_F32)
		{
			if (psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL && 
				psInst->asArg[2].uType == USEASM_REGTYPE_FPINTERNAL)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "If source 1 to %s is from the unified store then source 2 "
					"can't be a GPI register", OpcodeName(psInst->uOpcode)));
			}

			CheckArgFlags(psContext, psInst, 2 /* uArgument */, 0 /* uValidFlags */);
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_S2BEXT /* uBankExtension */,
										  puInst + 0,
										  puInst + 1,
										  SGXVEC_USE_VEC2_REGISTER_NUMBER_FIELD_LENGTH,
										  SGXVEC_USE0_PCK_64BITSRC2_SHIFT,
										  USEASM_HWARG_SOURCE2,
										  2 /* uArg */,
										  psTarget);
		}
	}
	else
	{
		EncodeSrc1(psContext,
				   psInst,
				   1,
				   IMG_TRUE /* bAllowExtended */,
				   EURASIA_USE1_S1BEXT /* uBankExtension */,
				   IMG_FALSE /* bSigned */,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE /* bBitwise */,
				   IMG_FALSE /* bFmtSelect */,
				   0 /* uFmtFlag */,
				   psTarget);
	}

	if (uSourceFormat != EURASIA_USE1_PCK_FMT_F32)
	{
		EncodeUnusedSource(2, puInst + 0, puInst + 1);
		uSwizzleArg = 2;
	}
	else
	{
		uSwizzleArg = 3;
	}

	/*
		Encode source selects.
	*/
	if (CheckSwizzleArgument(psInst, uSwizzleArg, psContext))
	{
		IMG_UINT32	uInputSwizzle;
		IMG_UINT32	uChan;
		IMG_UINT32	uNextUsedChan;

		uInputSwizzle = psInst->asArg[uSwizzleArg].uNumber;

		/*
			Check the swizzle has the form XYXY.
		*/
		if (bReplicateSpecialCase && uDestMask == USEREG_MASK_XYZW)
		{
			IMG_UINT32	uHalf;

			for (uHalf = 0; uHalf < 2; uHalf++)
			{
				IMG_UINT32	uSel0, uSel1;

				uSel0 = (uInputSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * (uHalf + 0))) & USEASM_SWIZZLE_VALUE_MASK;
				uSel1 = (uInputSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * (uHalf + 2))) & USEASM_SWIZZLE_VALUE_MASK;

				if (uSel0 != uSel1)
				{
					USEASM_ERRMSG((psContext->pvContext, 
								   psInst,
								   "For conversions between these formats with all channels set in the destination "
								   "mask the first two and second two channel selections in the swizzle must match."));
					break;
				}
			}
		}

		/*
			Encode the source channel selectors.
		*/
		uNextUsedChan = 0;
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uComponent;
			IMG_UINT32	uEncodedChan;

			if ((uDestMask & (1 << uChan)) != 0)
			{
				IMG_UINT32	uInputSel;

				uInputSel = (uInputSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;

				switch (uInputSel)
				{
					case USEASM_SWIZZLE_SEL_X: uComponent = 0; break;
					case USEASM_SWIZZLE_SEL_Y: uComponent = 1; break;
					case USEASM_SWIZZLE_SEL_Z: uComponent = 2; break;
					case USEASM_SWIZZLE_SEL_W: uComponent = 3; break;
					default:
					{
						USEASM_ERRMSG((psContext->pvContext, psInst, 
									   "Only non-constant channel selectors are allowed in the swizzle argument to %s",
									   OpcodeName(psInst->uOpcode)));
						uComponent = 0;
						break;
					}
				}

				if (
						(
							uSourceFormat == EURASIA_USE1_PCK_FMT_U16 || 
							uSourceFormat == EURASIA_USE1_PCK_FMT_S16
						) &&
						uComponent >= 2
				   )
				{
					USEASM_ERRMSG((psContext->pvContext, 
								   psInst, 
								   "Invalid channel selector for a 16-bit pack source format"));
				}

				/*
					There are two modes of selecting the source data depending on the source and destination formats.
					
					For destination channel N 
					
					(i) either the hardware uses the Nth component select

					(ii) or the component select corresponding to the number of bits with index less than N 
					set in the destination mask.
				*/
				if (!bSparseSwizzle)
				{
					uEncodedChan = uNextUsedChan;
					uNextUsedChan++;
				}
				else
				{
					uEncodedChan = uChan;
				}
			}
			else
			{
				/*
					Set the component select to a default value for unused components.
				*/
				uEncodedChan = uChan;
				uComponent = 0;
			}

			switch (uEncodedChan)
			{
				case 0: 
				{
					if (uSourceFormat == EURASIA_USE1_PCK_FMT_F32)
					{
						puInst[0] |= (((uComponent >> 0) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT0_SHIFT); 
						puInst[0] |= (((uComponent >> 1) & 1) << SGXVEC_USE0_PCK_F32SRC_C0SSEL_BIT1_SHIFT); 
					}
					else
					{
						puInst[0] |= (uComponent << SGXVEC_USE0_PCK_NONF32SRC_C0SSEL_SHIFT); 
					}
					break;
				}
				case 1: puInst[0] |= (uComponent << SGXVEC_USE0_PCK_C1SSEL_SHIFT); break;
				case 2: puInst[0] |= (uComponent << SGXVEC_USE0_PCK_C2SSEL_SHIFT); break;
				case 3: puInst[0] |= (uComponent << SGXVEC_USE0_PCK_C3SSEL_SHIFT); break;
			}
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

#if defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS)
/*****************************************************************************
 FUNCTION	: EncodeIntegerConditionalPredicateArgument

 PURPOSE	: Check a predicate argument to a CNDST, CNDEF, CNDSM, CNDLT or CNDEND 
			  instruction.

 PARAMETERS	: psContext				- USEASM context.
			  psInst				- Input instruction.
			  uArg					- Index of the argument to check.

 RETURNS	: The predicate register number.
*****************************************************************************/
static
IMG_UINT32 EncodeIntegerConditionalPredicateArgument(PUSEASM_CONTEXT	psContext,
													 PUSE_INST			psInst,
													 IMG_UINT32			uArg)
{
	PUSE_REGISTER	psArg = &psInst->asArg[uArg];

	if (psArg->uType != USEASM_REGTYPE_PREDICATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s must be a predicate", 
			uArg, OpcodeName(psInst->uOpcode)));
		return 0;
	}
	CheckArgFlags(psContext, psInst, uArg, 0);
	if (psArg->uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "A predicate register can't be indexed"));
	}
	if (psArg->uNumber > SGXVEC_USE_CND_MAXIMUM_PREDICATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum supported predicate is p%d", 
			SGXVEC_USE_CND_MAXIMUM_PREDICATE));
	}
	return psArg->uNumber;
}

/*****************************************************************************
 FUNCTION	: EncodeIntegerConditionalPCNDArgument

 PURPOSE	: Encode the PCND instruction to an integer conditional instruction.

 PARAMETERS	: psContext				- USEASM context.
			  psInst				- Input instruction.
			  uArg					- Index of the argument to check.
			  puInst				- Encoded instruction.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeIntegerConditionalPCNDArgument(PUSEASM_CONTEXT	psContext,
											  PUSE_INST			psInst,
											  IMG_UINT32		uArg,
											  IMG_PUINT32		puInst)
{
	PUSE_REGISTER	psArg = &psInst->asArg[uArg];
	IMG_UINT32		uPCnd;
	IMG_BOOL		bValidSource;

	bValidSource = IMG_FALSE;
	if (psArg->uType == USEASM_REGTYPE_INTSRCSEL)
	{
		if (psArg->uNumber == USEASM_INTSRCSEL_TRUE || psArg->uNumber == USEASM_INTSRCSEL_FALSE)
		{
			bValidSource = IMG_TRUE;
		}
	}
	else if (psArg->uType == USEASM_REGTYPE_PREDICATE)
	{
		bValidSource = IMG_TRUE;
	}

	if (!bValidSource)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s must be either a predicate, constant false or "
			"constant true", uArg, OpcodeName(psInst->uOpcode)));
		return;
	}
	CheckArgFlags(psContext, psInst, uArg, USEASM_ARGFLAGS_NOT);
	if (psArg->uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s can't be indexed",
			uArg, OpcodeName(psInst->uOpcode)));
		return;
	}
	if (psArg->uType == USEASM_REGTYPE_INTSRCSEL)
	{
		if ((psArg->uFlags & USEASM_ARGFLAGS_NOT) != 0)
		{
			switch (psArg->uNumber)
			{
				case USEASM_INTSRCSEL_FALSE: uPCnd = SGXVEC_USE1_CND_PCND_TRUE; break;
				case USEASM_INTSRCSEL_TRUE: uPCnd = SGXVEC_USE1_CND_PCND_FALSE; break;
				default: IMG_ABORT();
			}
		}
		else
		{
			switch (psArg->uNumber)
			{
				case USEASM_INTSRCSEL_FALSE: uPCnd = SGXVEC_USE1_CND_PCND_FALSE; break;
				case USEASM_INTSRCSEL_TRUE: uPCnd = SGXVEC_USE1_CND_PCND_TRUE; break;
				default: IMG_ABORT();
			}
		}
	}
	else
	{
		if (psArg->uNumber > SGXVEC_USE_CND_MAXIMUM_PREDICATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum supported predicate is p%d", 
				SGXVEC_USE_CND_MAXIMUM_PREDICATE));
			return;
		}
		if ((psArg->uFlags & USEASM_ARGFLAGS_NOT) != 0)
		{
			switch (psArg->uNumber)
			{
				case 0: uPCnd = SGXVEC_USE1_CND_PCND_NOTP0; break;
				case 1: uPCnd = SGXVEC_USE1_CND_PCND_NOTP1; break;
				case 2: uPCnd = SGXVEC_USE1_CND_PCND_NOTP2; break;
				case 3: uPCnd = SGXVEC_USE1_CND_PCND_NOTP3; break;
				default: IMG_ABORT();
			}
		}
		else
		{
			switch (psArg->uNumber)
			{
				case 0: uPCnd = SGXVEC_USE1_CND_PCND_P0; break;
				case 1: uPCnd = SGXVEC_USE1_CND_PCND_P1; break;
				case 2: uPCnd = SGXVEC_USE1_CND_PCND_P2; break;
				case 3: uPCnd = SGXVEC_USE1_CND_PCND_P3; break;
				default: IMG_ABORT();
			}
		}
	}
	puInst[1] |= (uPCnd << SGXVEC_USE1_CND_PCND_SHIFT);
}

/*****************************************************************************
 FUNCTION	: EncodeIntegerConditionalInstruction

 PURPOSE	: Encode a CNDST, CNDEF, CNDSM, CNDLT or CNDEND instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeIntegerConditionalInstruction(PUSE_INST			psInst,
										     IMG_PUINT32		puInst,
										     PUSEASM_CONTEXT	psContext,
										     PCSGX_CORE_DESC		psTarget)
{
	IMG_UINT32	uNextArg;

	if (!SupportsIntegerConditionalInstructions(psTarget) || FixBRN29643(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s isn't supported on this core", 
			OpcodeName(psInst->uOpcode)));
	}

	/*
		Check instruction flags.
	*/
	CheckFlags(psContext,
			   psInst,
			   USEASM_OPFLAGS1_NOSCHED | USEASM_OPFLAGS1_END,
			   0,
			   0);

	/*
		Set up the invariant parts of the instruction.
	*/
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_CND << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				EURASIA_USE1_SPECIAL_OPCAT_EXTRA;

	/*
		Set up the opcode.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_CNDST: puInst[1] |= (SGXVEC_USE1_CND_OP2_CNDST << SGXVEC_USE1_CND_OP2_SHIFT); break;
		case USEASM_OP_CNDEF: puInst[1] |= (SGXVEC_USE1_CND_OP2_CNDEF << SGXVEC_USE1_CND_OP2_SHIFT); break;
		case USEASM_OP_CNDSM: puInst[1] |= (SGXVEC_USE1_CND_OP2_CNDSM << SGXVEC_USE1_CND_OP2_SHIFT); break;
		case USEASM_OP_CNDLT: puInst[1] |= (SGXVEC_USE1_CND_OP2_CNDLT << SGXVEC_USE1_CND_OP2_SHIFT); break;
		case USEASM_OP_CNDEND: puInst[1] |= (SGXVEC_USE1_CND_OP2_CNDEND << SGXVEC_USE1_CND_OP2_SHIFT); break;
		default: IMG_ABORT();
	}

	/*
		Set up the instruction flags.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		puInst[1] |= SGXVEC_USE1_CND_NOSCHED;
	}
	if (psInst->uFlags1 & USEASM_OPFLAGS1_END)
	{
		puInst[1] |= EURASIA_USE1_END;
	}

	/*
		Encode destination.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	EncodeDest(psContext,
			   psInst,
			   IMG_FALSE /* bAllowExtended */,
			   puInst + 0,
			   puInst + 1,
			   IMG_FALSE /* bFmtSelect */,
			   0 /* uFmtFlag */,
			   psTarget);

	/*
		Encode PLOOPDST.
	*/
	uNextArg = 1;
	if (psInst->uOpcode == USEASM_OP_CNDLT)
	{
		puInst[0] |= 
			(EncodeIntegerConditionalPredicateArgument(psContext, psInst, uNextArg) << SGXVEC_USE0_CNDLT_PDSTLOOP_SHIFT);
		uNextArg++;
	}

	/*
		Encode source 1.
	*/
	CheckArgFlags(psContext, psInst, uNextArg, 0);
	EncodeSrc1(psContext,
			   psInst,
			   uNextArg,
			   IMG_FALSE /* bAllowExtended */,
			   0 /* uBankExtension */,
			   IMG_FALSE /* bSigned */,
			   puInst + 0,
			   puInst + 1,
			   IMG_FALSE /* bBitwise */,
			   IMG_FALSE /* bFmtSelect */,
			   0 /* uFmtFlag */,
			   psTarget);
	uNextArg++;

	/*
		Encode PCND
	*/
	if (psInst->uOpcode != USEASM_OP_CNDEND)
	{
		EncodeIntegerConditionalPCNDArgument(psContext, psInst, uNextArg, puInst);
		uNextArg++;
	}

	if (psInst->uOpcode == USEASM_OP_CNDSM)
	{
		/*
			Encode source 2.
		*/
		CheckArgFlags(psContext, psInst, uNextArg, 0);
		EncodeSrc2(psContext,
				   psInst,
				   uNextArg,
				   IMG_FALSE /* bAllowExtended */,
				   0 /* uBankExtension */,
				   IMG_FALSE /* bSigned */,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE /* bBitwise */,
				   IMG_FALSE /* bFmtSelect */,
				   0 /* uFmtFlag */,
				   psTarget);
		uNextArg++;
	}
	else
	{
		/*
			Encode adjustment.
		*/
		CheckArgFlags(psContext, psInst, uNextArg, 0);
		if (psInst->asArg[uNextArg].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Argument %d to %s must be an immediate value",
				uNextArg, OpcodeName(psInst->uOpcode)));
		}
		else
		{
			if (psInst->asArg[uNextArg].uNumber > SGXVEC_USE0_CND_ADJUST_MAXIMUM)
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The maximum adjustment value for %s is %d",
					OpcodeName(psInst->uOpcode), SGXVEC_USE0_CND_ADJUST_MAXIMUM));
			}
			puInst[0] |= (psInst->asArg[uNextArg].uNumber << SGXVEC_USE0_CND_ADJUST_SHIFT);
		}
		uNextArg++;
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS) */

#if defined(SUPPORT_SGX_FEATURE_USE_IDXSC)
static
IMG_VOID CheckIDXSCIndexedSource(PUSEASM_CONTEXT	psContext, 
								 PUSE_INST			psInst, 
								 PUSE_REGISTER		psRegister,
								 PCSGX_CORE_DESC		psTarget)
/*****************************************************************************
 FUNCTION	: CheckIDXSCIndexedSource

 PURPOSE	: Check the register bank to index into.

 PARAMETERS	: psContext			- USEASM context.
			  psInst			- IDXSCR or IDXSCW instruction.
			  psRegister		- Register bank to check.
			  psTarget			- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psInst);

	if (psRegister->uIndex != USEREG_INDEX_NONE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The register bank to index can't itself be indexed"));
	}
	if (!(psRegister->uType == USEASM_REGTYPE_TEMP ||
		  psRegister->uType == USEASM_REGTYPE_PRIMATTR ||
		  psRegister->uType == USEASM_REGTYPE_SECATTR ||
		  psRegister->uType == USEASM_REGTYPE_OUTPUT))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "Only the temporary, primary attribute, secondary attribute and "
			"output register bank can be indexed"));
	}
	if (FixBRN30871(psTarget))
	{
		if (psRegister->uType != USEASM_REGTYPE_TEMP)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only the temporary register bank can be indexed on this core"));
		}
		if (psRegister->uNumber > 0)
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "The static offset into the indexed register bank must "
				"be zero on this core"));
		}
	}
}

/*****************************************************************************
 FUNCTION	: EncodeIndexedReadWriteInstruction

 PURPOSE	: Encode an IDXSCR or IDXSCW instruction.

 PARAMETERS	: psInst				- Input instruction.
			  puInst				- Points to the memory for the encoded instruction.
			  psContext				- USEASM context.
			  psTarget				- Assembly target.

 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID EncodeIndexedReadWriteInstruction(PUSE_INST		psInst,
										   IMG_PUINT32		puInst,
										   PUSEASM_CONTEXT	psContext,
										   PCSGX_CORE_DESC	psTarget)
{
	if (!SupportsIDXSC(psTarget))
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "%s isn't supported on this core", 
			OpcodeName(psInst->uOpcode)));
	}

	/*
		Check instruction flags.
	*/
	CheckFlags(psContext,
			   psInst,
			   USEASM_OPFLAGS1_SKIPINVALID | USEASM_OPFLAGS1_NOSCHED,
			   0,
			   0);

	/*
		Set up the invariant parts of the instruction.
	*/
	puInst[0] = 0;
	puInst[1] = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
				(EURASIA_USE1_SPECIAL_OPCAT_FLOWCTRL << EURASIA_USE1_SPECIAL_OPCAT_SHIFT) |
				EURASIA_USE1_SPECIAL_OPCAT_EXTRA;

	/*
		Set up the opcode.
	*/
	switch (psInst->uOpcode)
	{
		case USEASM_OP_IDXSCR: puInst[1] |= (SGXVEC_USE1_OTHER2_OP2_IDXSCR << EURASIA_USE1_OTHER2_OP2_SHIFT); break;
		case USEASM_OP_IDXSCW: puInst[1] |= (SGXVEC_USE1_OTHER2_OP2_IDXSCW << EURASIA_USE1_OTHER2_OP2_SHIFT); break;
		default: IMG_ABORT();
	}

	/*
		Set up the instruction flags.
	*/
	if (psInst->uFlags1 & USEASM_OPFLAGS1_SKIPINVALID)
	{
		puInst[1] |= EURASIA_USE1_SKIPINV;
	}
	if (psInst->uFlags1 & USEASM_OPFLAGS1_NOSCHED)
	{
		puInst[1] |= SGXVEC_USE1_OTHER2_IDXSC_NOSCHED;
	}

	/*
		Encode index register source.
	*/
	if (psInst->asArg[1].uType != USEASM_REGTYPE_INDEX)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to %s must be an index register",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[1].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the second argument to %s",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[1].uIndex != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The second argument to %s can't be indexed",
			OpcodeName(psInst->uOpcode)));
	}
	switch (psInst->asArg[1].uNumber)
	{
		case USEREG_INDEX_L: 
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_LOW << SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_SHIFT);
			break;
		}
		case USEREG_INDEX_H: 
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_HIGH << SGXVEC_USE1_OTHER2_IDXSC_INDEXREG_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Only i.l or i.h are valid for the second argument to %s",
				OpcodeName(psInst->uOpcode)));
		}
	}

	/*
		Encode the format of the indexed data.
	*/
	if (psInst->asArg[3].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to %s must be immediate",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[3].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fourth argument to %s",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[3].uIndex != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fourth argument to %s can't be indexed",
			OpcodeName(psInst->uOpcode)));
	}
	switch (psInst->asArg[3].uNumber)
	{
		case 8:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_FORMAT_8888 << SGXVEC_USE1_OTHER2_IDXSC_FORMAT_SHIFT);
			break;
		}
		case 10:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_FORMAT_10101010 << SGXVEC_USE1_OTHER2_IDXSC_FORMAT_SHIFT);
			break;
		}
		case 16:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_FORMAT_16161616 << SGXVEC_USE1_OTHER2_IDXSC_FORMAT_SHIFT);
			break;
		}
		case 32:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_FORMAT_3232 << SGXVEC_USE1_OTHER2_IDXSC_FORMAT_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for the fourth argument to %s are 8, 10, 16 or 32",
				OpcodeName(psInst->uOpcode)));
			return;
		}
	}

	/*
		Encode destination.
	*/
	CheckArgFlags(psContext, psInst, 0, 0);
	if (psInst->uOpcode == USEASM_OP_IDXSCR)
	{
		EncodeDest(psContext,
				   psInst,
				   IMG_TRUE /* bAllowExtended */,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE /* bFmtSelect */,
				   0 /* uFmtFlag */,
				   psTarget);
	}
	else
	{
		CheckIDXSCIndexedSource(psContext, psInst, &psInst->asArg[0], psTarget);
		if (psInst->asArg[3].uNumber == 8)
		{
			EncodeArgument(psContext,
						   psInst,
						   USEASM_HWARG_DESTINATION,
						   0 /* uInputArg */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_DBEXT /* uBankExtension */,
						   IMG_FALSE /* bSigned */,
						   puInst + 0,
						   puInst + 1,
						   IMG_FALSE /* bBitwise */,
						   IMG_FALSE /* bFmtSelect */,
						   0 /* uFmtFlag */,
						   psTarget,
						   SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT,
						   EURASIA_USE_REGISTER_NUMBER_BITS);
		}
		else
		{
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_DBEXT,
									      puInst + 0,
										  puInst + 1,
										  EURASIA_USE_REGISTER_NUMBER_BITS + 1,
										  SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT /* uNumberFieldShift */,
										  USEASM_HWARG_DESTINATION,
										  0 /* uArg */,
										  psTarget);
		}
	}


	/*
		Encode source register.
	*/
	CheckArgFlags(psContext, psInst, 2, 0);
	if (psInst->uOpcode == USEASM_OP_IDXSCR)
	{
		CheckIDXSCIndexedSource(psContext, psInst, &psInst->asArg[2], psTarget);
		if (psInst->asArg[3].uNumber == 8)
		{
			EncodeArgument(psContext,
						   psInst,
						   USEASM_HWARG_SOURCE1,
						   2 /* uArg */,
						   IMG_TRUE /* bAllowExtended */,
						   EURASIA_USE1_S1BEXT /* uBankExtension */,
						   IMG_FALSE /* bSigned */,
						   puInst + 0,
						   puInst + 1,
						   IMG_FALSE /* bBitwise */,
						   IMG_FALSE /* bFmtSelect */,
						   0 /* uFmtFlag */,
						   psTarget,
						   SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT,
						   EURASIA_USE_REGISTER_NUMBER_BITS);
		}
		else
		{
			EncodeArgumentDoubleRegisters(psContext,
										  psInst,
										  IMG_TRUE /* bAllowExtended */,
										  EURASIA_USE1_S1BEXT,
										  puInst + 0,
										  puInst + 1,
										  EURASIA_USE_REGISTER_NUMBER_BITS + 1,
										  SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_SHIFT,
										  USEASM_HWARG_SOURCE1,
										  2 /* uArgIndex */,
										  psTarget);
		}
	}
	else
	{
		EncodeSrc1(psContext,
				   psInst,
				   2,
				   IMG_TRUE /* bAllowExtended */,
				   EURASIA_USE1_S1BEXT /* uBankExtension */,
				   IMG_FALSE /* bSigned */,
				   puInst + 0,
				   puInst + 1,
				   IMG_FALSE /* bBitwise */,
				   IMG_FALSE /* bFmtSelect */,
				   0 /* uFmtFlag */,
				   psTarget);
	}

	/*
		Encode components per register count.
	*/
	if (psInst->asArg[4].uType != USEASM_REGTYPE_IMMEDIATE)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to %s must be immediate",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[4].uFlags != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "No flags are valid for the fifth argument to %s",
			OpcodeName(psInst->uOpcode)));
	}
	if (psInst->asArg[4].uIndex != 0)
	{
		USEASM_ERRMSG((psContext->pvContext, psInst, "The fifth argument to %s can't be indexed",
			OpcodeName(psInst->uOpcode)));
	}
	switch (psInst->asArg[4].uNumber)
	{
		case 1:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_1 << SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_SHIFT);
			break;
		}
		case 2:
		{
			puInst[1] |= (SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_2 << SGXVEC_USE1_OTHER2_IDXSC_COMPCOUNT_SHIFT);
			break;
		}
		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Valid values for the fifth argument to %s are 1 or 2",
				OpcodeName(psInst->uOpcode)));
		}
	}
}
#endif /* defined(SUPPORT_SGX_FEATURE_USE_IDXSC) */

/*****************************************************************************
 FUNCTION	: UseAssembleInstruction

 PURPOSE	: Assemble a single USE instruction.

 PARAMETERS	: psInst			- The instruction to assemble.
			  pdwBaseInst		- The base to which jumps are relative.
			  puInst			- Array which will contain the encoded instruction.
			  dwCodeOffset		- Offset of the code in memory.

 RETURNS	: The number of dwords used for the instruction.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 IMG_CALLCONV UseAssembleInstruction(PCSGX_CORE_DESC psTarget,
															PUSE_INST psInst,
															IMG_PUINT32 puBaseInst,
															IMG_PUINT32 puInst,
															IMG_UINT32 uCodeOffset,
															PUSEASM_CONTEXT psContext)
{
	USE_INST sTemp;
	IMG_UINT32 uRptCount = (psInst->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK) >> USEASM_OPFLAGS1_REPEAT_SHIFT;

	if (psInst->uOpcode >= USEASM_OP_SETPGT && psInst->uOpcode <= USEASM_OP_SETPNEQ)
	{
		sTemp = *psInst;
		sTemp.uOpcode = USEASM_OP_FSUB;
		sTemp.uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
		sTemp.asArg[0].uFlags = USEASM_ARGFLAGS_DISABLEWB;
		sTemp.asArg[0].uIndex = USEREG_INDEX_NONE;
		sTemp.asArg[0].uNumber = 0;
		sTemp.asArg[0].uType = USEASM_REGTYPE_TEMP;
		sTemp.asArg[1] = psInst->asArg[0];
		sTemp.asArg[2] = psInst->asArg[1];
		sTemp.asArg[3] = psInst->asArg[2];
		sTemp.uTest = (USEASM_TEST_CHANSEL_C0 << USEASM_TEST_CHANSEL_SHIFT);
		switch (psInst->uOpcode)
		{
			case USEASM_OP_SETPGT: sTemp.uTest |= (USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
												   (USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) |
												   USEASM_TEST_CRCOMB_AND;
								   break;
			case USEASM_OP_SETPGTE: sTemp.uTest |= (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |
													(USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) |
													USEASM_TEST_CRCOMB_OR;
									break;
			case USEASM_OP_SETPEQ: sTemp.uTest |= (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |
												   (USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |
											       USEASM_TEST_CRCOMB_AND;
								   break;
			case USEASM_OP_SETPLT: sTemp.uTest |= (USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
												   (USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) |
												   USEASM_TEST_CRCOMB_AND;
								   break;
			case USEASM_OP_SETPLTE: sTemp.uTest |= (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |
													(USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) |
												    USEASM_TEST_CRCOMB_OR;
									break;
			case USEASM_OP_SETPNEQ: sTemp.uTest |= (USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
													(USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |
												    USEASM_TEST_CRCOMB_AND;
									break;
			default: break;
		}
		psInst = &sTemp;
	}
	else if (psInst->uOpcode == USEASM_OP_FTZ || psInst->uOpcode == USEASM_OP_FTNZ)
	{
		sTemp = *psInst;
		sTemp.uOpcode = USEASM_OP_FMOV;
		sTemp.uFlags1 |= USEASM_OPFLAGS1_TESTENABLE;
		sTemp.asArg[0].uFlags = USEASM_ARGFLAGS_DISABLEWB;
		sTemp.asArg[0].uIndex = USEREG_INDEX_NONE;
		sTemp.asArg[0].uNumber = 0;
		sTemp.asArg[0].uType = USEASM_REGTYPE_TEMP;
		sTemp.asArg[1] = psInst->asArg[0];
		sTemp.asArg[2] = psInst->asArg[1];
		sTemp.uTest = (USEASM_TEST_CHANSEL_C0 << USEASM_TEST_CHANSEL_SHIFT);
		if (psInst->uOpcode == USEASM_OP_FTZ)
		{
			sTemp.uTest |= (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) |
							(USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |
							USEASM_TEST_CRCOMB_AND;
		}
		else
		{
			sTemp.uTest |= (USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
							(USEASM_TEST_SIGN_TRUE << USEASM_TEST_SIGN_SHIFT) |
							USEASM_TEST_CRCOMB_AND;
		}
		psInst = &sTemp;
	}
	if (psInst->uOpcode == USEASM_OP_MOVC)
	{
		EncodeMovcInstruction(psTarget, psInst, puInst, psContext);
		return 2;
	}
	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (psInst->uOpcode == USEASM_OP_VMOVC ||
		psInst->uOpcode == USEASM_OP_VMOVCU8)
	{
		EncodeVecMovInstruction(psInst, puInst, psContext, psTarget);
		return 2;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check for a test instruction. */
	if (psInst->uFlags1 & USEASM_OPFLAGS1_TESTENABLE)
	{
		EncodeTestInstruction(psTarget, psInst, puInst, psContext);	
		return 2;
	}

	#if defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE)
	/* Check for a dual-issue instruction. */
	if ((HasDualIssue(psTarget)) &&
		(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE) &&
		(psInst->uOpcode == USEASM_OP_FMAD ||
		 psInst->uOpcode == USEASM_OP_MOV ||
		 psInst->uOpcode == USEASM_OP_FADM ||
		 psInst->uOpcode == USEASM_OP_FMSA ||
		 psInst->uOpcode == USEASM_OP_FDDP ||
		 psInst->uOpcode == USEASM_OP_FMINMAX ||
		 psInst->uOpcode == USEASM_OP_IMA32 ||
		 psInst->uOpcode == USEASM_OP_FSSQ))
	{
		EncodeDualIssueInstruction(psTarget, psInst, puInst, psContext);
		return 2;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_DUAL_ISSUE) */

	#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
	if (
			SupportsVEC34(psTarget) &&
			(psInst->uFlags1 & USEASM_OPFLAGS1_MAINISSUE) &&
			(
				psInst->uOpcode == USEASM_OP_VMAD3 ||
				psInst->uOpcode == USEASM_OP_VMAD4 ||
				psInst->uOpcode == USEASM_OP_VDP3 ||
				psInst->uOpcode == USEASM_OP_VDP4 ||
				psInst->uOpcode == USEASM_OP_VMUL3 ||
				psInst->uOpcode == USEASM_OP_VMUL4 ||
				psInst->uOpcode == USEASM_OP_VADD3 ||
				psInst->uOpcode == USEASM_OP_VADD4 ||
				psInst->uOpcode == USEASM_OP_VSSQ3 ||
				psInst->uOpcode == USEASM_OP_VSSQ4 ||
				psInst->uOpcode == USEASM_OP_VMOV3 ||
				psInst->uOpcode == USEASM_OP_VMOV4 ||
				psInst->uOpcode == USEASM_OP_FRSQ ||
				psInst->uOpcode == USEASM_OP_FRCP ||
				psInst->uOpcode == USEASM_OP_FMAD ||
				psInst->uOpcode == USEASM_OP_FADD ||
				psInst->uOpcode == USEASM_OP_FMUL ||
				psInst->uOpcode == USEASM_OP_FSUBFLR ||
				psInst->uOpcode == USEASM_OP_FEXP ||
				psInst->uOpcode == USEASM_OP_FLOG
			)
		)
	{
		EncodeDualIssueVectorInstruction(psInst, puInst, psContext, psTarget);
		return 2;
	}
	#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

	/* Check for the second part of a co-issued instruction. */
	if (psInst->uFlags2 & USEASM_OPFLAGS2_COISSUE)
	{
		return 0;
	}

	/* Expand instruction macros. */
	psInst = ExpandMacros(psTarget, psInst, &sTemp, &uRptCount, psContext);
	
	switch (psInst->uOpcode)
	{
		case USEASM_OP_FMAD:
		case USEASM_OP_FADM:
		case USEASM_OP_FMSA:
		case USEASM_OP_FSUBFLR:
		case USEASM_OP_FRCP:
		case USEASM_OP_FRSQ:
		case USEASM_OP_FLOG:
		case USEASM_OP_FEXP:
		case USEASM_OP_FDP:
		case USEASM_OP_FMIN:
		case USEASM_OP_FMAX:
		case USEASM_OP_FDSX:
		case USEASM_OP_FDSY:
		case USEASM_OP_FMAD16:
		#if defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS)
		case USEASM_OP_FSQRT:
		case USEASM_OP_FSIN:
		case USEASM_OP_FCOS:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_SQRT_SIN_COS) */	
		#if defined(SUPPORT_SGX_FEATURE_USE_FCLAMP)
		case USEASM_OP_FMINMAX:
		case USEASM_OP_FMAXMIN:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FCLAMP) */
		{
			EncodeFloatInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE)
		case USEASM_OP_FNRM32:
		case USEASM_OP_FNRM16:
		{
			EncodeNormaliseInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_FNORMALISE) */

		case USEASM_OP_FDPC:
		{
			EncodeFDPCInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_FDDP:
		{
			EncodeFDDPInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_FDDPC:
		{
			EncodeFDDPCInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_EFO:
		{
			EncodeEfoInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_PCKF16F32:
		case USEASM_OP_PCKF16F16:
		case USEASM_OP_PCKF16U16:
		case USEASM_OP_PCKF16S16:
		case USEASM_OP_PCKU16F32:
		case USEASM_OP_PCKU16F16:
		case USEASM_OP_PCKU16U16:
		case USEASM_OP_PCKU16S16:
		case USEASM_OP_PCKS16F32:
		case USEASM_OP_PCKS16F16:
		case USEASM_OP_PCKS16U16:
		case USEASM_OP_PCKS16S16:
		case USEASM_OP_PCKU8F32:
		case USEASM_OP_PCKU8F16:
		case USEASM_OP_PCKU8U16:
		case USEASM_OP_PCKU8S16:
		case USEASM_OP_PCKS8F32:
		case USEASM_OP_PCKS8F16:
		case USEASM_OP_PCKS8U16:
		case USEASM_OP_PCKS8S16:
		case USEASM_OP_PCKO8F32:
		case USEASM_OP_PCKO8F16:
		case USEASM_OP_PCKO8U16:
		case USEASM_OP_PCKO8S16:
		case USEASM_OP_PCKC10F32:
		case USEASM_OP_PCKC10F16:
		case USEASM_OP_PCKC10U16:
		case USEASM_OP_PCKC10S16:
		case USEASM_OP_UNPCKF32F32:
		case USEASM_OP_UNPCKF32F16:
		case USEASM_OP_UNPCKF32U16:
		case USEASM_OP_UNPCKF32S16:
		case USEASM_OP_UNPCKF32U8:
		case USEASM_OP_UNPCKF32S8:
		case USEASM_OP_UNPCKF32O8:
		case USEASM_OP_UNPCKF32C10:
		case USEASM_OP_UNPCKF16F16:
		case USEASM_OP_UNPCKF16U16:
		case USEASM_OP_UNPCKF16S16:
		case USEASM_OP_UNPCKF16U8:
		case USEASM_OP_UNPCKF16S8:
		case USEASM_OP_UNPCKF16O8:
		case USEASM_OP_UNPCKF16C10:
		case USEASM_OP_UNPCKU16F16:
		case USEASM_OP_UNPCKU16U16:
		case USEASM_OP_UNPCKU16S16:
		case USEASM_OP_UNPCKU16U8:
		case USEASM_OP_UNPCKU16S8:
		case USEASM_OP_UNPCKU16O8:
		case USEASM_OP_UNPCKU16C10:
		case USEASM_OP_UNPCKS16F16:
		case USEASM_OP_UNPCKS16U16:
		case USEASM_OP_UNPCKS16S16:
		case USEASM_OP_UNPCKS16U8:
		case USEASM_OP_UNPCKS16S8:
		case USEASM_OP_UNPCKS16O8:
		case USEASM_OP_UNPCKS16C10:
		case USEASM_OP_UNPCKU8U8:
		case USEASM_OP_UNPCKU8S8:
		case USEASM_OP_UNPCKS8U8:
		case USEASM_OP_UNPCKC10C10:
		{
			EncodePackInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_AND:
		case USEASM_OP_OR:
		case USEASM_OP_XOR:
		case USEASM_OP_SHL:
		case USEASM_OP_SHR:
		case USEASM_OP_ASR:
		case USEASM_OP_ROL:
		case USEASM_OP_RLP:
		{
			EncodeBitwiseInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_SOP2:
		{
			EncodeSOP2Instruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_SOP2WM:
		{
			EncodeSOPWMInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_LRP2:
		{		
			EncodeLRP2Instruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_LRP1:
		{
			EncodeLRP1Instruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_SOP3:
		{
			EncodeSOP3Instruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_U8DOT3:
		case USEASM_OP_U8DOT4:
		case USEASM_OP_U8DOT3OFF:
		case USEASM_OP_U8DOT4OFF:
		case USEASM_OP_U16DOT3:
		case USEASM_OP_U16DOT4:
		case USEASM_OP_U16DOT3OFF:
		case USEASM_OP_U16DOT4OFF:
		{	
			AssembleDot34(psContext, psInst, puInst, psTarget);
			break;
		}

		case USEASM_OP_IMA8:
		case USEASM_OP_FPMA:
		{
			EncodeFPMAInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_IMA16:
		{
			EncodeIMA16Instruction(psInst, puInst, psContext, psTarget);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_INT_DIV)
		case USEASM_OP_IDIV:
		{
			EncodeIDIVInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_INT_DIV) */

		#if defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD)
		case USEASM_OP_IMA32:
		{
			EncodeIMA32Instruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_32BIT_INT_MAD) */

		case USEASM_OP_IMAE:
		{
			EncodeIMAEInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_ADIF:
		case USEASM_OP_ADIFSUM:
		{
			EncodeADIFInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_SSUM16:
		{
			EncodeSSUM16Instruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_FIRV:
		{
			EncodeFIRVInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_FIRVH:
		{
			EncodeFIRVHInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_BILIN:
		{
			EncodeBILINInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_FIRH:
		{
			EncodeFIRHInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_FIRHH:
		{
			EncodeFIRHHInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_SMP1D:
		case USEASM_OP_SMP2D:
		case USEASM_OP_SMP3D:
		case USEASM_OP_SMP1DBIAS:
		case USEASM_OP_SMP2DBIAS:
		case USEASM_OP_SMP3DBIAS:
		case USEASM_OP_SMP1DREPLACE:
		case USEASM_OP_SMP2DREPLACE:
		case USEASM_OP_SMP3DREPLACE:
		case USEASM_OP_SMP1DGRAD:
		case USEASM_OP_SMP2DGRAD:
		case USEASM_OP_SMP3DGRAD:
		{
			EncodeSMPInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_LDAB:
		case USEASM_OP_LDAW:
		case USEASM_OP_LDAD:
		case USEASM_OP_LDAQ:
		case USEASM_OP_STAB:
		case USEASM_OP_STAW:
		case USEASM_OP_STAD:
		case USEASM_OP_STAQ:
		case USEASM_OP_LDLB:
		case USEASM_OP_LDLW:
		case USEASM_OP_LDLD:
		case USEASM_OP_LDLQ:
		case USEASM_OP_STLB:
		case USEASM_OP_STLW:
		case USEASM_OP_STLD:
		case USEASM_OP_STLQ:
		case USEASM_OP_LDTB:
		case USEASM_OP_LDTW:
		case USEASM_OP_LDTD:
		case USEASM_OP_LDTQ:
		case USEASM_OP_STTB:
		case USEASM_OP_STTW:
		case USEASM_OP_STTD:
		case USEASM_OP_STTQ:
		{
			EncodeLDSTInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD)
		case USEASM_OP_ELDD:
		case USEASM_OP_ELDQ:
		{
			EncodeELDInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_EXTENDED_LOAD) */

		#if defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS)
		case USEASM_OP_LDATOMIC:
		{
			if (!SupportsLDSTAtomicOps(psTarget))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst, "The %s instruction isn't supported on this core",
					OpcodeName(psInst->uOpcode)));
			}
			EncodeLDSTInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_LD_ST_ATOMIC_OPS) */

		case USEASM_OP_BA:
		case USEASM_OP_BR:
		{
			if (!EncodeBranchInstruction(psTarget, psInst, puInst, psContext, uCodeOffset, puBaseInst))
			{
				return USE_UNDEF;
			}
			break;
		}

		case USEASM_OP_LAPC:
		{
			EncodeLAPCInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION)
		case USEASM_OP_BEXCEPTION:
		{
			EncodeBEXCEPTIONInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_BRANCH_EXCEPTION) */

		case USEASM_OP_IDF:
		{
			EncodeIDFInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_WDF:
		{
			EncodeWDFInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_LIMM:
		{
			EncodeLIMMInstruction(psTarget, psInst, puInst, psContext, uCodeOffset, puBaseInst);
			break;
		}

		case USEASM_OP_EMITPIXEL1:
		case USEASM_OP_EMITPIXEL2:
		case USEASM_OP_EMITSTATE:
		case USEASM_OP_EMITVERTEX:
		case USEASM_OP_EMITPRIMITIVE:
		case USEASM_OP_EMITPDS:
		#if defined(SUPPORT_SGX_FEATURE_VCB)
		case USEASM_OP_EMITVCBVERTEX:
		case USEASM_OP_EMITVCBSTATE:
		case USEASM_OP_EMITMTEVERTEX:
		case USEASM_OP_EMITMTESTATE:
		#endif /* defined(SUPPORT_SGX_FEATURE_VCB) */
		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		case USEASM_OP_EMITPIXEL:
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */
		{
			EncodeEmitInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_SMOA:
		{
			EncodeSMOAInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		case USEASM_OP_SMLSI:
		{
			EncodeSMLSIInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		case USEASM_OP_SMR:
		case USEASM_OP_SMBO:
		case USEASM_OP_IMO:
		{
			EncodeMOEInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		case USEASM_OP_SETFC:
		{
			EncodeSETFCInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#if defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS)
		case USEASM_OP_MOEST:
		{
			EncodeMOESTInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_STORE_MOE_TO_REGISTERS) */

		#if defined(SUPPORT_SGX_FEATURE_USE_CFI)
		case USEASM_OP_CF:
		case USEASM_OP_CI:
		case USEASM_OP_CFI:
		{
			EncodeCFIInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_CFI) */

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		case USEASM_OP_VDP3:
		case USEASM_OP_VDP4:
		case USEASM_OP_VMAD3:
		case USEASM_OP_VMAD4:
		{
			EncodeSingleIssueVectorInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_VADD3:
		case USEASM_OP_VADD4:
		case USEASM_OP_VMUL3:
		case USEASM_OP_VMUL4:
		case USEASM_OP_VSSQ3:
		case USEASM_OP_VSSQ4:
		case USEASM_OP_VMOV3:
		case USEASM_OP_VMOV4:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst,
						   "%s should have been coissued",
						   OpcodeName(psInst->uOpcode)));
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

		case USEASM_OP_MOV:
		{
			EncodeMOVInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_LABEL:
		{
			IMG_UINT32 uLabelAddress = (IMG_UINT32)((puInst - puBaseInst) / 2UL);
			/* Store the label address for backward branches. */
			psContext->pfnSetLabelAddress(psContext->pvContext,
										  psInst->asArg[0].uNumber,
										  uLabelAddress);
			/* Set up the address for any branches to this label we have already encountered. */
			if (!FixupLabelAddresses(psTarget,
									 uLabelAddress,
									 psInst->asArg[0].uNumber,
									 puBaseInst,
									 uCodeOffset,
									 psContext))
			{
				return USE_UNDEF;
			}
			return 0;
		}

		case USEASM_OP_LOCK:
		case USEASM_OP_RELEASE:
		{
			EncodeMutexInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_LDR:
		case USEASM_OP_STR:
		{
			EncodeLDRSTRInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_PCOEFF:
		case USEASM_OP_PTOFF:
		{
			EncodePTOFFPCOEFFInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_ATST8:
		{
			EncodeATST8Instruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_DEPTHF:
		{
			EncodeDEPTHFInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_SETM:
		{
			EncodeSETMInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES)
		case USEASM_OP_PHAS:
		case USEASM_OP_PHASIMM:
		{
			EncodePHASInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_UNLIMITED_PHASES) */

		#if defined(SUPPORT_SGX_FEATURE_USE_SPRVV)
		case USEASM_OP_SPRVV:
		{
			EncodeSPRVVInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_SPRVV) */

		case USEASM_OP_WOP:
		{
			EncodeWOPInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		case USEASM_OP_NOP:
		{
			return 0;
		}

		case USEASM_OP_PADDING:
		{
			EncodePaddingInstruction(psTarget, psInst, puInst, psContext);
			break;
		}

		#if defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE)
		case USEASM_OP_MOVMSK:
		{
			EncodeMOVMSKInstruction(psTarget, psInst, puInst, psContext);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_ALPHATOCOVERAGE) */

		#if defined(SUPPORT_SGX_FEATURE_USE_VEC34)
		case USEASM_OP_VMAD:
		case USEASM_OP_VF16MAD:
		{
			EncodeVEC4MadInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_VMUL:
		case USEASM_OP_VADD:
		case USEASM_OP_VFRC:
		case USEASM_OP_VMIN:
		case USEASM_OP_VMAX:
		case USEASM_OP_VDSX:
		case USEASM_OP_VDSY:
		case USEASM_OP_VDP:
		case USEASM_OP_VF16MUL:
		case USEASM_OP_VF16ADD:
		case USEASM_OP_VF16FRC:
		case USEASM_OP_VF16DSX:
		case USEASM_OP_VF16DSY:
		case USEASM_OP_VF16MIN:
		case USEASM_OP_VF16MAX:
		case USEASM_OP_VF16DP:
		{
			EncodeVEC4NONMADInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_VRCP:
		case USEASM_OP_VRSQ:
		case USEASM_OP_VLOG:
		case USEASM_OP_VEXP:
		{
			EncodeVecComplexInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_VPCKU8U8:
		case USEASM_OP_VPCKU8S8:
		case USEASM_OP_VPCKU8O8:
		case USEASM_OP_VPCKU8U16:
		case USEASM_OP_VPCKU8S16:
		case USEASM_OP_VPCKU8F16:
		case USEASM_OP_VPCKU8F32:
		case USEASM_OP_VPCKS8U8:
		case USEASM_OP_VPCKS8S8:
		case USEASM_OP_VPCKS8O8:
		case USEASM_OP_VPCKS8U16:
		case USEASM_OP_VPCKS8S16:
		case USEASM_OP_VPCKS8F16:
		case USEASM_OP_VPCKS8F32:
		case USEASM_OP_VPCKO8U8:
		case USEASM_OP_VPCKO8S8:
		case USEASM_OP_VPCKO8O8:
		case USEASM_OP_VPCKO8U16:
		case USEASM_OP_VPCKO8S16:
		case USEASM_OP_VPCKO8F16:
		case USEASM_OP_VPCKO8F32:
		case USEASM_OP_VPCKU16U8:
		case USEASM_OP_VPCKU16S8:
		case USEASM_OP_VPCKU16O8:
		case USEASM_OP_VPCKU16U16:
		case USEASM_OP_VPCKU16S16:
		case USEASM_OP_VPCKU16F16:
		case USEASM_OP_VPCKU16F32:
		case USEASM_OP_VPCKS16U8:
		case USEASM_OP_VPCKS16S8:
		case USEASM_OP_VPCKS16O8:
		case USEASM_OP_VPCKS16U16:
		case USEASM_OP_VPCKS16S16:
		case USEASM_OP_VPCKS16F16:
		case USEASM_OP_VPCKS16F32:
		case USEASM_OP_VPCKF16U8:
		case USEASM_OP_VPCKF16S8:
		case USEASM_OP_VPCKF16O8:
		case USEASM_OP_VPCKF16U16:
		case USEASM_OP_VPCKF16S16:
		case USEASM_OP_VPCKF16F16:
		case USEASM_OP_VPCKF16F32:
		case USEASM_OP_VPCKF16C10:
		case USEASM_OP_VPCKF32U8:
		case USEASM_OP_VPCKF32S8:
		case USEASM_OP_VPCKF32O8:
		case USEASM_OP_VPCKF32U16:
		case USEASM_OP_VPCKF32S16:
		case USEASM_OP_VPCKF32F16:
		case USEASM_OP_VPCKF32F32:
		case USEASM_OP_VPCKF32C10:
		case USEASM_OP_VPCKC10F16:
		case USEASM_OP_VPCKC10F32:
		case USEASM_OP_VPCKC10C10:
		{
			EncodeVecPackInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_VMOV:
		{
			EncodeVecMovInstruction(psInst, puInst, psContext, psTarget);
			break;
		}

		case USEASM_OP_IDXSCR:
		case USEASM_OP_IDXSCW:
		{
			EncodeIndexedReadWriteInstruction(psInst, puInst, psContext, psTarget);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_VEC34) */

		#if defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS)
		case USEASM_OP_CNDST:
		case USEASM_OP_CNDEF:
		case USEASM_OP_CNDSM:
		case USEASM_OP_CNDLT:
		case USEASM_OP_CNDEND:
		{
			EncodeIntegerConditionalInstruction(psInst, puInst, psContext, psTarget);
			break;
		}
		#endif /* defined(SUPPORT_SGX_FEATURE_USE_INTEGER_CONDITIONALS) */

		case USEASM_OP_AINTRP1:
		case USEASM_OP_AINTRP2:
		case USEASM_OP_AADD:
		case USEASM_OP_ASUB:
		case USEASM_OP_ASOP:
		case USEASM_OP_ARSOP:
		case USEASM_OP_ALRP:
		case USEASM_OP_ASOP2:
		{
			if (psInst->psPrev == NULL || !(psInst->psPrev->uFlags1 & USEASM_OPFLAGS1_MAINISSUE))
			{
				USEASM_ERRMSG((psContext->pvContext, psInst,
							   "%s should have been coissued",
							   OpcodeName(psInst->uOpcode)));
			}
			return 0;
		}

		default:
		{
			USEASM_ERRMSG((psContext->pvContext, psInst, "Unknown instruction"));
			break;
		}
	}
	return 2;
}

IMG_INTERNAL IMG_UINT32 IMG_CALLCONV UseGetOpcodeArgumentCount(IMG_UINT32	uOpcode)
/*****************************************************************************
 FUNCTION	: UseGetOpcodeArgumentCount

 PURPOSE	: Returns the number of arguments to a USEASM opcode.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: The count of arguments.
*****************************************************************************/
{
	return OpcodeArgumentCount(uOpcode);
}

IMG_INTERNAL IMG_BOOL IMG_CALLCONV UseIsTextureSampleOpcode(IMG_UINT32 uOpcode)
/*****************************************************************************
 FUNCTION	: UseIsTextureSampleOpcode

 PURPOSE	: Checks for a USEASM opcode which is a texture sample.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: TRUE if the opcode is a texture sample.
*****************************************************************************/
{
	return (OpcodeDescFlags(uOpcode)& USC_DESCFLAG_TEXTURESAMPLE) != 0 ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL IMG_BOOL IMG_CALLCONV UseAcceptsSkipinvOpcode(IMG_UINT32 uOpcode)
/*****************************************************************************
 FUNCTION	: UseAcceptsSkipinvOpcode

 PURPOSE	: Checks for a USEASM opcode which supports the SKIPINV flag.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: TRUE or FALSE
*****************************************************************************/
{
	return OpcodeAcceptsSkipInv(uOpcode);
}

IMG_INTERNAL IMG_BOOL IMG_CALLCONV UseAcceptsNoSchedOpcode(IMG_UINT32 uOpcode)
/*****************************************************************************
 FUNCTION	: UseAcceptsNoSchedOpcode

 PURPOSE	: Checks for a USEASM opcode which supports the NOSCHED flag.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: TRUE or FALSE
*****************************************************************************/
{
	return OpcodeAcceptsNoSched(uOpcode);
}

IMG_INTERNAL IMG_BOOL IMG_CALLCONV UseAcceptsNoSchedEnhancedOpcode(IMG_UINT32 uOpcode)
/*****************************************************************************
 FUNCTION	: UseAcceptsNoSchedEnhancedOpcode

 PURPOSE	: Checks for a USEASM opcode which supports the NOSCHED flag on
			  cores with the "no instruction pairing" feature.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: TRUE or FALSE
*****************************************************************************/
{
	return OpcodeAcceptsNoSchedEnhanced(uOpcode);
}

#if defined(USER)
IMG_INTERNAL IMG_CHAR const * IMG_CALLCONV UseGetNameOpcode(IMG_UINT32 uOpcode)
/*****************************************************************************
 FUNCTION	: UseGetNameOpcode

 PURPOSE	: Gets the name of a USEASM opcode.

 PARAMETERS	: uOpcode		- Opcode to check.

 RETURNS	: A pointer to the name.
*****************************************************************************/
{
	return OpcodeName(uOpcode);
}
#endif /* defined(USER) */

IMG_INTERNAL IMG_VOID CheckUndefinedLabels(PUSEASM_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: CheckUndefinedLabels

 PURPOSE	: Check for undefined labels in the program.

 PARAMETERS	:

 RETURNS	: Nothing.
*****************************************************************************/
{
	LABEL_CONTEXT* psLabelContext;
	IMG_UINT32 i;

	psLabelContext = (LABEL_CONTEXT*)psContext->pvLabelState;
	if (psLabelContext != NULL)
	{
		for (i = 0; i < psLabelContext->uLabelReferenceCount; i++)
		{
			USEASM_ERRMSG((psContext->pvContext, psLabelContext->psLabelReferences[i].psInst, "Label %s is not defined", psContext->pfnGetLabelName(psContext->pvContext, psLabelContext->psLabelReferences[i].uLabel)));
		}
	}
}

IMG_INTERNAL IMG_UINT32 IMG_CALLCONV UseAssembler(PCSGX_CORE_DESC psTarget,
													PUSE_INST psInst,
													IMG_PUINT32 puInst,
													IMG_UINT32 uCodeOffset,
													PUSEASM_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: UseAssembler

 PURPOSE	: Assemble a USE program.

 PARAMETERS	: psInst			- The list of instructions to assemble.
			  pdwOutput			- Array which will contain the encoding instructions.
			  dwCodeOffset		- Offset of the code in memory.

 RETURNS	: The number of instructions on success or -1 on failure.
*****************************************************************************/
{
	IMG_PUINT32 puBaseInst = puInst;

	for ( ; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uInstSize;

		uInstSize = UseAssembleInstruction(psTarget, psInst, puBaseInst, puInst, uCodeOffset, psContext);
		if (uInstSize == USE_UNDEF)
		{
			return USE_UNDEF;
		}
		puInst += uInstSize;
	}
	CheckUndefinedLabels(psContext);
	return (IMG_UINT32)((puInst - puBaseInst) / 2UL);
}

/******************************************************************************
 End of file (useasm.c)
******************************************************************************/
