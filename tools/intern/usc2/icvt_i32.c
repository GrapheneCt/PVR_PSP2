/*************************************************************************
 * Name         : icvt_i32.c
 * Title        : UNIFLEX-to-intermediate code conversion for 32 bit integers.
 * Created      : Jan 2008
 *
 * Copyright    : 2007-2010 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: icvt_i32.c $
 **************************************************************************/


/* Supported on: 545 */

#include "uscshrd.h"
#include "bitops.h"
#include <limits.h>

IMG_INTERNAL
IMG_VOID IntegerAbsolute(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psBlock,
						 PARG					psSrc,
						 PARG					psResult);

static
IMG_VOID IntegerAbsoluteFmt(PINTERMEDIATE_STATE	psState,
                            PCODEBLOCK			psBlock,
                            PARG				psSrc,
                            PARG				psResult,
                            UF_REGFORMAT        eFmt);

IMG_INTERNAL
IMG_VOID GetSourceTypeless(PINTERMEDIATE_STATE psState, 
						   PCODEBLOCK psCodeBlock, 
						   PUF_REGISTER psSource, 
						   IMG_UINT32 uChan, 
						   PARG psHwSource, 
						   IMG_BOOL bAllowSourceMod,
						   PFLOAT_SOURCE_MODIFIER psSourceMod)
/*****************************************************************************
 FUNCTION	: GetSourceTypeless
    
 PURPOSE	: Converts an input source to the intermediate format for
			  instructions which are scalar even on cores with the
			  vector instruction set.

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert any extra instructions needed
							to update the input destination.
			  psSource		- Input source instruction.
			  uChan			- Channel in the input source.
			  psHwSource	- Returns the intermediate register.
			  bAllowSourceMod
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		PINST		psUnpackVec;
		IMG_UINT32	uUnpackRegNum;

		/*
			Generate an instruction to move from the channel of the vector register corresponding
			to the input source to a temporary register.
		*/
		uUnpackRegNum = GetNextRegister(psState);

		psUnpackVec = AllocateInst(psState, NULL);

		SetOpcode(psState, psUnpackVec, IUNPCKVEC);

		psUnpackVec->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psUnpackVec->asDest[0].uNumber = uUnpackRegNum;

		GetSourceF32_Vec(psState, 
						 psCodeBlock, 
						 psSource, 
						 1 << uChan, 
						 psUnpackVec,
						 0 /* uSrcIdx */);

		psUnpackVec->u.psVec->auSwizzle[0] = 
			CombineSwizzles(ConvertSwizzle(psState, psSource->u.uSwiz), g_auReplicateSwizzles[uChan]);

		AppendInst(psState, psCodeBlock, psUnpackVec);

		/*
			Return the temporary register.
		*/
		InitInstArg(psHwSource);
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = uUnpackRegNum;
		if (bAllowSourceMod)
		{
			psSourceMod->uComponent = 0;
			psSourceMod->bAbsolute = IMG_FALSE;
			if (psSource->byMod & UFREG_SOURCE_ABS) 
			{
				psSourceMod->bAbsolute = IMG_TRUE;
			}
			psSourceMod->bNegate = IMG_FALSE;
			if (psSource->byMod & UFREG_SOURCE_NEGATE) 
			{
				psSourceMod->bNegate = IMG_TRUE;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		GetSourceF32(psState, psCodeBlock, psSource, uChan, psHwSource, bAllowSourceMod, psSourceMod);
	}
}

IMG_INTERNAL
IMG_VOID GetDestinationTypeless(PINTERMEDIATE_STATE psState, 
								PCODEBLOCK psCodeBlock, 
								PUF_REGISTER psDest, 
								IMG_UINT32 uChan, 
								PARG psHwSource)
/*****************************************************************************
 FUNCTION	: GetDestinationTypeless
    
 PURPOSE	: Converts an input destination to the intermediate format for
			  instructions which are scalar even on cores with the
			  vector instruction set.

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to insert any extra instructions needed
							to update the input destination.
			  psDest		- Input destination.
			  uChan			- Channel to write.
			  psHwSource	- Returns the intermediate register to write to.
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			Write to a scalar temporary register. Later we'll add an instruction
			to move from the scalar temporaries to the real (vector) destination.
		*/
		InitInstArg(psHwSource);
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = USC_TEMPREG_VECTEMPDEST + uChan;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		GetDestinationF32(psState, psCodeBlock, psDest, uChan, psHwSource);
	}
}

#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ImmediateConstant(IMG_UINT32 uValue, PARG psArg)
/*****************************************************************************
 FUNCTION	: ImmediateConstant
    
 PURPOSE	: Fills an intermediate instruction argument to be an immediate value

 PARAMETERS	: uValue				- The desired immediate value
			  psArg					- Pointer to the structure to fill out
			  
 RETURNS	: None.
*****************************************************************************/
{
	InitInstArg(psArg);
	psArg->uType	= USEASM_REGTYPE_IMMEDIATE;
	psArg->uNumber	= uValue;
}
#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX545)
static
IMG_VOID GenerateIMA32Instruction(PINTERMEDIATE_STATE		psState,
								  PCODEBLOCK				psBlock,
								  UF_OPCODE					eOpcode,
								  PARG						psDestLow,
								  PARG						psDestHigh,
								  IMG_UINT32				uPredSrc,
								  IMG_BOOL					bPredNegate,
								  PARG						psArgA,
								  IMG_BOOL					bNegateA,
								  PARG						psArgB,
								  IMG_BOOL					bNegateB,
								  PARG						psArgC,
								  IMG_BOOL					bNegateC,
								  IMG_BOOL					bSigned,
								  PINST						psInsertBeforeInst)
/*****************************************************************************
 FUNCTION	: GenerateIMA32Instruction
    
 PURPOSE	: Generates an IMA32 instruction.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the instruction to.
			  eOpcode			- Operation to do in the instruction
								(ADD, MUL/MUL2 or MAD).
			  psDestLow			- Destination for the lower 32-bits of the result.
			  psDestHigh		- Destination for the upper 32-bits of the result.
			  uPredSrc			- Predicate for the instruction.
			  bPredNegate	
			  psArgA			- Source arguments.
			  psArgB
			  psArgC
			  bNegateA			- Source negate modifiers.
			  bNegateB
			  bNegateC
			  bSigned			- If TRUE do signed arithmetic.
			  psInsertBeforeInst
								- Instruction to insert the IMA32 before.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psIma32Inst;
	IMG_INT32	iDest;

	psIma32Inst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psIma32Inst, IIMA32);

	/*
		Set instruction specific parameters.
	*/
	psIma32Inst->u.psIma32->bSigned = bSigned;

	/*
		Copy the destinations to the IMA32 instruction.
	*/
	for (iDest = 1; iDest >= 0; iDest--)
	{
		PARG	psDest = (iDest == 0) ? psDestLow : psDestHigh;

		if (psDest == NULL)
		{
			if (iDest == 0)
			{
				/*
					Set the first destination to an unused register.
				*/
				SetDest(psState, 
						psIma32Inst, 
						iDest, 
						USEASM_REGTYPE_TEMP, 
						GetNextRegister(psState), 
						UF_REGFORMAT_F32);
			}
			else
			{
				/*
					Disable writing of the upper 32-bits of the result.
				*/
				SetDestUnused(psState, psIma32Inst, iDest);
			}
			continue;
		}

		/*
			On cores affected by BRN33442 the IMA32 can't be predicated.
		*/
		if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33442) != 0 && uPredSrc != USC_PREDREG_NONE)
		{
			ARG		sUnconditionalDest;
			PINST	psMoveInst;

			/*
				Allocate a new intermediate register.
			*/
			MakeNewTempArg(psState, psDest->eFmt, &sUnconditionalDest);

			/*
				Set the new register as the destination of the (unconditionally) executed
				IMA32 instruction.
			*/
			SetDestFromArg(psState, psIma32Inst, iDest, &sUnconditionalDest);

			/*
				Add a move instruction controlled by the predicate 
				to copy from the new register to the real destination.
			*/
			psMoveInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psMoveInst, IMOV);
			SetPredicate(psState, psMoveInst, uPredSrc, bPredNegate);
			SetDestFromArg(psState, psMoveInst, 0 /* uDestIdx */, psDest);
			SetSrcFromArg(psState, psMoveInst, 0 /* uSrcIdx */, &sUnconditionalDest);
			InsertInstBefore(psState, psBlock, psMoveInst, psInsertBeforeInst);
			psInsertBeforeInst = psMoveInst;
		}
		else
		{
			SetDestFromArg(psState, psIma32Inst, iDest, psDest);
		}
	}

	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_33442) == 0)
	{
		/*
			Copy the predicate to the instruction.
		*/
		SetPredicate(psState, psIma32Inst, uPredSrc, bPredNegate);
	}

	/*
		Swizzle the arguments and add constant arguments depending
		on the specific operation.
	*/
	switch (eOpcode)
	{
		case UFOP_MOV:
		{
			/* A * 1 + 0 */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			ImmediateConstant(1, &psIma32Inst->asArg[1]);
			ImmediateConstant(0, &psIma32Inst->asArg[2]);
			break;
		}
		case UFOP_ADD:
		case UFOP_ADD2:
		{
			/* A * 1 + B */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			ImmediateConstant(1, &psIma32Inst->asArg[1]);
			SetSrcFromArg(psState, psIma32Inst, 2 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[2] = bNegateB;
			break;
		}
		case UFOP_MUL:
		case UFOP_MUL2:
		{
			/* A * B + 0 */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32Inst, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			ImmediateConstant(0, &psIma32Inst->asArg[2]);
			break;
		}
		case UFOP_MAD:
		case UFOP_MAD2:
		{
			/* A * B + C */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32Inst, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			SetSrcFromArg(psState, psIma32Inst, 2 /* uSrcIdx */, psArgC);
			psIma32Inst->u.psIma32->abNegate[2] = bNegateC;
			break;
		}
		default:
		{
		    break;
		}
	}

	InsertInstBefore(psState, psBlock, psIma32Inst, psInsertBeforeInst);
}

IMG_INTERNAL
IMG_VOID GenerateIntegerArithmetic_545(PINTERMEDIATE_STATE		psState,
									   PCODEBLOCK				psBlock,
									   PINST					psInsertBeforeInst,
									   UF_OPCODE				eOpcode,
									   PARG						psDestLow,
									   PARG						psDestHigh,
									   IMG_UINT32				uPredSrc,
									   IMG_BOOL					bPredNegate,
									   PARG						psArgA,
									   IMG_BOOL					bNegateA,
									   PARG						psArgB,
									   IMG_BOOL					bNegateB,
									   PARG						psArgC,
									   IMG_BOOL					bNegateC,
									   IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: GenerateIntegerArithmetic_545
    
 PURPOSE	: Generates an instructions to implement 32-bit integer arithmetic
			  on SGX545.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the instructions to.
			  psInsertBeforeInst
								- Point to insert the instructions at.
			  eOpcode			- Operation to do in the instructions
								(ADD, MUL/MUL2 or MAD).
			  psDestLow			- Destination for the lower 32-bits of the result.
			  psDestHigh		- Destination for the upper 32-bits of the result.
			  uPredSrc			- Predicate for the instruction.
			  bPredNegate	
			  psArgA			- Source arguments.
			  psArgB
			  psArgC
			  bNegateA			- Source negate modifiers.
			  bNegateB
			  bNegateC
			  bSigned			- If TRUE do signed arithmetic.
			  
 RETURNS	: None.
*****************************************************************************/
{
	/*
		Generate the two instruction results into DESTLOW and i2
	*/
	GenerateIMA32Instruction(psState,
							 psBlock,
							 eOpcode,
							 psDestLow,
							 psDestHigh,
							 uPredSrc,
							 bPredNegate,
							 psArgA,
							 bNegateA,
							 psArgB,
							 bNegateB,
							 psArgC,
							 bNegateC,
							 bSigned,
							 psInsertBeforeInst);
}
#endif /* defined(SUPPORT_SGX545) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID GenerateIMA32Instruction_543(PINTERMEDIATE_STATE		psState, 
									  PCODEBLOCK				psBlock, 
									  UF_OPCODE					eOpcode, 
									  PARG						psDestLow, 
									  IMG_UINT32				uPredSrc, 
									  IMG_BOOL					bPredNegate, 
									  PARG						psArgA, 
									  IMG_BOOL					bNegateA,
									  PARG						psArgB, 
									  IMG_BOOL					bNegateB,
									  PARG						psArgC, 
									  IMG_BOOL					bNegateC,
									  IMG_BOOL					bSigned, 
									  PINST						psInsertBeforeInst)
/*****************************************************************************
 FUNCTION	: GenerateIMA32Instruction_543
    
 PURPOSE	: Generates an IMA32 instruction on SGX543.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the instruction to.
			  eOpcode			- Operation to do in the instruction
								(ADD, MUL/MUL2 or MAD).
			  psDestLow			- Destination for the lower 32-bits of the result.
			  uPredSrc			- Predicate for the instruction.
			  bPredNegate
			  psArgA			- Source arguments.
			  psArgB
			  psArgC
			  bNegateA			- Source negate modifiers.
			  bNegateB
			  bNegateC
			  bSigned			- If TRUE do signed arithmetic.
			  psInsertBeforeInst
								- Instruction to insert the IMA32 before.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psIma32Inst;
	PINST		psIma32InstStep2 = IMG_NULL;

	psIma32Inst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psIma32Inst, IIMA32);

	psIma32Inst->u.psIma32->bSigned = bSigned;

	if (psDestLow != NULL)
	{
		/*
			Copy the destination to the high.
		*/
		SetDestFromArg(psState, psIma32Inst, 0 /* uDestIdx */, psDestLow);
	}
	else
	{
		/*
			Set the first destination to an unused register.
		*/
		SetDest(psState, psIma32Inst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);
	}

	/*
		Copy the predicate to the instruction.
	*/
	SetPredicate(psState, psIma32Inst, uPredSrc, bPredNegate);

	/*
		Swizzle the arguments and add constant arguments depending
		on the specific operation.
	*/
	switch (eOpcode)
	{
		case UFOP_MOV:
		{
			/* A * 1 + 0 */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			ImmediateConstant(1, &psIma32Inst->asArg[1]);
			ImmediateConstant(0, &psIma32Inst->asArg[2]);
			psIma32Inst->u.psIma32->uStep = 1;
			break;
		}
		case UFOP_ADD:
		case UFOP_ADD2:
		{
			/* A * 1 + B */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			ImmediateConstant(1, &psIma32Inst->asArg[1]);
			SetSrcFromArg(psState, psIma32Inst, 2 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[2] = bNegateB;
			psIma32Inst->u.psIma32->uStep = 1;
			break;
		}
		case UFOP_MUL:
		case UFOP_MUL2:
		{
			/* Step1 = A * B(Low 16) + 0 */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32Inst, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			ImmediateConstant(0, &psIma32Inst->asArg[2]);
			psIma32Inst->u.psIma32->uStep = 1;

			/* Step2 = A * B(High 16) + result of Step1*/
			psIma32InstStep2 = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psIma32InstStep2, IIMA32);
			psIma32InstStep2->u.psIma32->uStep = 2;
			psIma32InstStep2->u.psIma32->bSigned = bSigned;

			/* 
				Use the given destination for final result 
			*/
			MoveDest(psState, psIma32InstStep2, 0 /* uMoveToDestIdx */, psIma32Inst, 0 /* uMoveFromDestIdx */);

			/*
				Get an unused register for the intermediate results
			*/
			SetDest(psState, psIma32Inst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);

			SetSrcFromArg(psState, psIma32InstStep2, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32InstStep2, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			SetSrcFromArg(psState, psIma32InstStep2, 2 /* uSrcIdx */, &psIma32Inst->asDest[0]);
			psIma32InstStep2->asArg[2] = psIma32Inst->asDest[0];
			break;
		}
		case UFOP_MAD:
		case UFOP_MAD2:
		{
			/* Step1 = A * B(Low 16) + C */
			SetSrcFromArg(psState, psIma32Inst, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32Inst, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			SetSrcFromArg(psState, psIma32Inst, 2 /* uSrcIdx */, psArgC);
			psIma32Inst->u.psIma32->abNegate[2] = bNegateC;
			psIma32Inst->u.psIma32->uStep = 1;

			/* Step2 = A * B(High 16) + result of Step1*/
			psIma32InstStep2 = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psIma32InstStep2, IIMA32);
			psIma32InstStep2->u.psIma32->uStep = 2;
			psIma32InstStep2->u.psIma32->bSigned = bSigned;

			/* 
				Use the given destination for final result 
			*/
			MoveDest(psState, psIma32InstStep2, 0 /* uMoveToDestIdx */, psIma32Inst, 0 /* uMoveFromDestIdx */);

			/*
				Get an unused register for the intermediate results
			*/
			SetDest(psState, psIma32Inst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);

			SetSrcFromArg(psState, psIma32InstStep2, 0 /* uSrcIdx */, psArgA);
			psIma32Inst->u.psIma32->abNegate[0] = bNegateA;
			SetSrcFromArg(psState, psIma32InstStep2, 1 /* uSrcIdx */, psArgB);
			psIma32Inst->u.psIma32->abNegate[1] = bNegateB;
			SetSrcFromArg(psState, psIma32InstStep2, 2 /* uSrcIdx */, &psIma32Inst->asDest[0]);

			break;
		}
		default:
		{
		    break;
		}
	}

	InsertInstBefore(psState, psBlock, psIma32Inst, psInsertBeforeInst);
	
	/*
		Is step2 used for calculation
	*/
	if(psIma32InstStep2 != IMG_NULL)
	{
		InsertInstBefore(psState, psBlock, psIma32InstStep2, psInsertBeforeInst);
	}
}

static
IMG_VOID GenerateIntegerArithmetic_543(PINTERMEDIATE_STATE		psState,
									   PCODEBLOCK				psBlock,
									   PINST					psInsertBeforeInst,
									   UF_OPCODE				eOpcode,
									   PARG						psDestLow,
									   IMG_UINT32				uPredSrc,
									   IMG_BOOL					bPredNegate,
									   PARG						psArgA,
									   IMG_BOOL					bNegateA,
									   PARG						psArgB,
									   IMG_BOOL					bNegateB,
									   PARG						psArgC,
									   IMG_BOOL					bNegateC,
									   IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: GenerateIntegerArithmetic_545
    
 PURPOSE	: Generates an instructions to implement 32-bit integer arithmetic
			  on SGX543. On SGX543 the hardware IMA32 instruction only gives
			  32 least significant bits of the result.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the instructions to.
			  psInsertBeforeInst
								- Point to insert the new instructions at.
			  eOpcode			- Operation to do in the instructions
								(ADD, MUL/MUL2 or MAD).
			  psDestLow			- Destination for the lower 32-bits of the result.
			  uPredSrc			- Predicate for the instruction.
			  bPredNegate	
			  psArgA			- Source arguments.
			  psArgB
			  psArgC
			  bSigned			- If TRUE do signed arithmetic.
			  
 RETURNS	: None.
*****************************************************************************/
{
	/*
		Generate the two instruction results into DESTLOW and i2
	*/
	GenerateIMA32Instruction_543(psState, 
								 psBlock, 
								 eOpcode, 
								 psDestLow, 
								 uPredSrc, 
								 bPredNegate, 
								 psArgA, 
								 bNegateA,
								 psArgB, 
								 bNegateB,
								 psArgC, 
								 bNegateC,
								 bSigned, 
								 psInsertBeforeInst);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static 
IMG_VOID Generate32bitIntegerMulNonNeg_Non545(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psBlock,
											  PINST					psInsertBeforeInst,
											  PARG					psDestLow,
											  PARG					psDestHigh,
											  IMG_UINT32			uPredSrc,
											  IMG_BOOL				bPredNegate,
											  PARG					psArgA,
											  PARG					psArgB,
											  IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: Generate32bitIntegerMulNonNeg_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit integer multiply. (A*B)

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Point to insert the new instructions at.
			  psDestLow			- Register that will be written with the low 32-bits
								of the result. Can be NULL.
			  psDestHigh		- Register that will be written with the high 32-bits
								of the result. Can be NULL.
			  psArgA			- Source arguments.
			  psArgB
			  bSigned			- TRUE to do a signed multiply.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uHighLow = GetNextRegister(psState);
	IMG_UINT32	uLowLow = GetNextRegister(psState);
	IMG_UINT32	uMiddleDword = GetNextRegister(psState);
	IMG_UINT32	uFirstCarry = USC_UNDEF;
	IMG_UINT32	uSecondCarry = USC_UNDEF;
	IMG_UINT32	uHighHigh = USC_UNDEF;
	PINST		psInst;

	/*
		Allocate registers only needed for calculating a 64-bit result.
	*/
	if (psDestHigh != NULL)
	{
		uHighHigh = GetNextRegister(psState);
		uFirstCarry = GetNextRegister(psState);
		uSecondCarry = GetNextRegister(psState);
	}

	/*
		LOW_LOW = A.LOW * B.LOW
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IIMAE);

	psInst->u.psImae->bSigned = IMG_FALSE;
	psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uLowLow, UF_REGFORMAT_F32);
	SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
	SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psArgB);
	psInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[2].uNumber = 0;

	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	/*
		C1, HIGH_LOW_TEMP = A.HIGH * B.LOW + LOW_LOW.HIGH
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IIMAE);

	psInst->u.psImae->bSigned = IMG_FALSE;
	psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_Z16;

	/* Main destination. */
	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uHighLow, UF_REGFORMAT_F32);

	if (psDestHigh != NULL)
	{
		/* Carry destination. */
		SetDest(psState, psInst, IMAE_CARRYOUT_DEST, USEASM_REGTYPE_TEMP, uFirstCarry, UF_REGFORMAT_F32);
	}

	SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
	SetComponentSelect(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);
	SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psArgB);
	SetSrc(psState, psInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uLowLow, UF_REGFORMAT_F32);
	SetComponentSelect(psState, psInst, 2 /* uArgIdx */, 2 /* uComponent */);

	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	/*
		C2, MIDDLE_DWORD = A.LOW * B.HIGH + HIGH_LOW
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IIMAE);

	psInst->u.psImae->bSigned = IMG_FALSE;
	psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

	/* Main destination. */
	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uMiddleDword, UF_REGFORMAT_F32);

	if (psDestHigh != NULL)
	{
		/* Carry destination. */
		SetDest(psState, psInst, IMAE_CARRYOUT_DEST, USEASM_REGTYPE_TEMP, uSecondCarry, UF_REGFORMAT_F32);
	}

	SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
	SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psArgB);
	SetComponentSelect(psState, psInst, 1 /* uArgIdx */, 2 /* uComponent */);
	SetSrc(psState, psInst, 2 /* uArgIdx */, USEASM_REGTYPE_TEMP, uHighLow, UF_REGFORMAT_F32);

	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	if (psDestHigh != NULL)
	{
		IMG_UINT32	uCombinedCarry = GetNextRegister(psState);
		IMG_UINT32	uCombinedCarryShifted = GetNextRegister(psState);

		/*
			COMBINED_CARRY = FIRST_CARRY | SECOND_CARRY
		*/
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IOR);

		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uCombinedCarry, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFirstCarry, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSecondCarry, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

		/*
			COMBINED_CARRY_SHIFTED = TEMP4 << 16
		*/
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, ISHL);

		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uCombinedCarryShifted, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, BITWISE_SHIFTEND_SOURCE, USEASM_REGTYPE_TEMP, uCombinedCarry, UF_REGFORMAT_F32);
		psInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 16;

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

		/*
			HIGH_HIGH = A.HIGH * A.HIGH + COMBINED_CARRY_SHIFTED
		*/
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IIMAE);

		psInst->u.psImae->bSigned = bSigned;
		psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uHighHigh, UF_REGFORMAT_F32);
		SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
		SetComponentSelect(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);
		SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psArgB);
		SetComponentSelect(psState, psInst, 1 /* uArgIdx */, 2 /* uComponent */);
		SetSrc(psState, psInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uCombinedCarryShifted, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

		if (bSigned)
		{
			IMG_UINT32	uArg;

			/*
				Since the LOW/HIGH multiplies above are unsigned adjust the result by
				doing RESULT.HIGH = RESULT.HIGH - (sign_bit(HIGH) * LOW)
			*/
			for (uArg = 0; uArg < 2; uArg++)
			{
				PARG		psSignBit;
				PARG		psMultiplicand;
				IMG_UINT32	uSignBit = GetNextRegister(psState);
				IMG_UINT32	uTemp8 = GetNextRegister(psState);
				IMG_UINT32	uTemp9 = GetNextRegister(psState);
				IMG_UINT32	uTemp10 = GetNextRegister(psState);

				if (uArg == 0)
				{
					psSignBit = psArgA;
					psMultiplicand = psArgB;
				}
				else
				{
					psSignBit = psArgB;
					psMultiplicand = psArgA;
				}

				/*
					SIGN_BIT = A < 0 ? 0xFFFFFFFF : 0
							   B < 0 ? 0xFFFFFFFF : 0
				*/
				psInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psInst, IMOVC_I32);
				psInst->u.psMovc->eTest = TEST_TYPE_LT_ZERO;
				SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSignBit, UF_REGFORMAT_F32);
				SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psSignBit);
				psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_INT32ONE;
				psInst->asArg[2].uType = USEASM_REGTYPE_FPCONSTANT;
				psInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

				/*
					TEMP8 = SIGN_BIT * B.LOW + HIGH_HIGH
				*/
				psInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psInst, IIMAE);
	
				psInst->u.psImae->bSigned = IMG_FALSE;
				psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

				SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp8, UF_REGFORMAT_F32);
				SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSignBit, UF_REGFORMAT_F32);
				SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psMultiplicand);
				SetSrc(psState, psInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uHighHigh, UF_REGFORMAT_F32);
	
				InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

				/*
					TEMP9 = SIGN_BIT * B.LOW + TEMP8.HIGH
				*/
				psInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psInst, IIMAE);

				psInst->u.psImae->bSigned = IMG_FALSE;
				psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_Z16;

				SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp9, UF_REGFORMAT_F32);
				SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSignBit, UF_REGFORMAT_F32);
				SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psMultiplicand);
				SetSrc(psState, psInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp8, UF_REGFORMAT_F32);
				SetComponentSelect(psState, psInst, 2 /* uArgIdx */, 2 /* uComponent */);

				InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

				/*
					TEMP10 = (TEMP8.LOW, TEMP9.LOW)
				*/
				psInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psInst, IUNPCKU16U16);
				SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp10, UF_REGFORMAT_F32);
				SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp8, UF_REGFORMAT_F32);
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp9, UF_REGFORMAT_F32);
				InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

				/*
					Replace the original high-word result by the sign adjusted value.
				*/
				uHighHigh = uTemp10;
			}
		}
	}

	if (psDestLow != NULL)
	{
		/*
			DESTLOW = (LOWLOW.LOW, MIDDLE_DWORD.LOW)
		*/
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IUNPCKU16U16);

		SetPredicate(psState, psInst, uPredSrc, bPredNegate);

		SetDestFromArg(psState, psInst, 0 /* uDestIdx */, psDestLow);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uLowLow, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uMiddleDword, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);
	}

	if (psDestHigh != NULL)
	{
		/*
			DESTHIGH = TEMP2.HIGH + TEMP3
		*/	
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IIMAE);

		psInst->u.psImae->bSigned = IMG_FALSE;
		psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

		SetPredicate(psState, psInst, uPredSrc, bPredNegate);

		SetDestFromArg(psState, psInst, 0 /* uDestIdx */, psDestHigh);

		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uMiddleDword, UF_REGFORMAT_F32);
		SetComponentSelect(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);

		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 1;

		SetSrc(psState, psInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uHighHigh, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);
	}
}

static 
IMG_VOID Generate32bitIntegerMul_Non545(PINTERMEDIATE_STATE		psState,
										PCODEBLOCK				psBlock,
										PINST					psInsertBeforeInst,
										PARG					psDestLow,
										PARG					psDestHigh,
										IMG_UINT32				uPredSrc,
										IMG_BOOL				bPredNegate,
										PARG					psArgA,
										IMG_BOOL				bArgANegate,
										PARG					psArgB,
										IMG_BOOL				bArgBNegate,
										IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: Generate32bitIntegerMul_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit integer multiply. (A*B)

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInsertBeforeInst	
								- Point to add the intermediate instructions at.
			  psDestLow			- Register that will be written with the low 32-bits
								of the result. Can be NULL.
			  psDestHigh		- Register that will be written with the high 32-bits
								of the result. Can be NULL.
			  psArgA			- Source arguments.
			  psArgB
			  bArgANegate		- Source argument negate modifiers.
			  bArgBNegate
			  bSigned			- TRUE to do a signed multiply.
			  
 RETURNS	: None.
*****************************************************************************/
{
	if ((bArgANegate || bArgBNegate) && !(bArgANegate && bArgBNegate))
	{	
		ARG	sTempDestLow;
		ARG sTempDestHigh;
		PARG psTempDestHigh;
		ARG sCarryTemp;

		/*
			Generate the multiply result without negating the sources
			into temporary registers.
		*/
		MakeNewTempArg(psState, UF_REGFORMAT_F32, &sTempDestLow);

		if (psDestHigh != NULL)
		{
			MakeNewTempArg(psState, UF_REGFORMAT_F32, &sTempDestHigh);
			psTempDestHigh = &sTempDestHigh;

			MakeNewTempArg(psState, UF_REGFORMAT_F32, &sCarryTemp);
		}
		else
		{
			InitInstArg(&sTempDestHigh);
			psTempDestHigh = NULL;

			InitInstArg(&sCarryTemp);
		}

		Generate32bitIntegerMulNonNeg_Non545(psState,
											 psBlock,
											 psInsertBeforeInst,
											 &sTempDestLow,
											 psTempDestHigh,
											 USC_PREDREG_NONE,
											 IMG_FALSE,
											 psArgA,
											 psArgB,
											 bSigned);

		/*
			Apply negation by negating the result.
		*/

		/*
			NOT	TEMPDESTLOW, TEMPDESTLOW
		*/
		{
			PINST	psXorInst;

			psXorInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psXorInst, INOT);
			psXorInst->asDest[0] = sTempDestLow;
			psXorInst->asArg[0] = sTempDestLow;
			InsertInstBefore(psState, psBlock, psXorInst, psInsertBeforeInst);
		}

		/*
			DESTLOW, CARRY = 1 * 1 + TEMPDESTLOW
		*/
		{
			PINST	psImaeInst;
			PARG	psNonCarryDest;

			psImaeInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psImaeInst, IIMAE);

			psImaeInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;

			if (psDestHigh != NULL)
			{
				SetDestFromArg(psState, psImaeInst, IMAE_CARRYOUT_DEST, &sCarryTemp);
			}

			psNonCarryDest = psDestLow != NULL ? psDestLow : &sTempDestLow;
			SetDestFromArg(psState, psImaeInst, 0 /* uDestIdx */, psNonCarryDest);

			SetPredicate(psState, psImaeInst, uPredSrc, bPredNegate);

			psImaeInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[0].uNumber = 1;
			psImaeInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[1].uNumber = 1;
			psImaeInst->asArg[2] = sTempDestLow;
			InsertInstBefore(psState, psBlock, psImaeInst, psInsertBeforeInst);
		}

		if (psDestHigh != NULL)
		{
			/*
				TEMPDESTHIGH = ~TEMPDESTHIGH
			*/
			{
				PINST	psXorInst;

				psXorInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psXorInst, INOT);
				psXorInst->asDest[0] = sTempDestHigh;
				psXorInst->asArg[0] = sTempDestHigh;
				InsertInstBefore(psState, psBlock, psXorInst, psInsertBeforeInst);
			}
			/*
				DESTHIGH = TEMPDESTHIGH + CARRY
			*/
			{
				PINST	psImaeInst;

				psImaeInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psImaeInst, IIMAE);

				psImaeInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;
				psImaeInst->asDest[0] = *psDestHigh;

				SetPredicate(psState, psImaeInst, uPredSrc, bPredNegate);

				psImaeInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
				psImaeInst->asArg[0].uNumber = 0;
				psImaeInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psImaeInst->asArg[1].uNumber = 1;
				psImaeInst->asArg[2] = sTempDestHigh;
				/* Carry source. */
				SetSrcFromArg(psState, psImaeInst, IMAE_CARRYIN_SOURCE, &sCarryTemp);
				InsertInstBefore(psState, psBlock, psImaeInst, psInsertBeforeInst);
			}
		}
	}
	else
	{
		Generate32bitIntegerMulNonNeg_Non545(psState,
											 psBlock,
											 psInsertBeforeInst,
											 psDestLow,
											 psDestHigh,
											 uPredSrc,
											 bPredNegate,
											 psArgA,
											 psArgB,
											 bSigned);
	}
}

static
IMG_VOID Generate32bitIntegerAdd_Non545(PINTERMEDIATE_STATE		psState,
										PCODEBLOCK				psBlock,
										PINST					psInsertBeforeInst,
										PARG					psDestLow,
										PARG					psDestHigh,
										IMG_UINT32				uPredSrc,
										IMG_BOOL				bPredNegate,
										PARG					psArgA,
										IMG_BOOL				bNegateA,
										PARG					psArgB,
										IMG_BOOL				bNegateB,
										IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: Generate32bitIntegerAdd_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit integer add (A+B)

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Point to add the intermediate instructions at.
			  psDestLow			- Register that will be written with the low 32-bits
								of the result. Can be NULL.
			  psDestHigh		- Register that will be written with the high 32-bits
								of the result. Can be NULL.
			  uPredSrc			- Predicate to control writing of the result.
			  bPredNegate
			  psArgA			- Source arguments.
			  psArgB
			  bNegateA			- Source negate modifiers.
			  bNegateB
			  bSigned			- TRUE to do a signed multiply.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;
	IMG_UINT32	uTemp1 = GetNextRegister(psState);
	IMG_UINT32	uTemp2 = GetNextRegister(psState);
	IMG_UINT32	uCarryTemp = GetNextRegister(psState);
	ARG			sNegatedArgA;
	ARG			sNegatedArgB;

	/*
		Apply negation modifiers to the sources.
	*/
	if (bNegateA)
	{
		IntegerNegate(psState, psBlock, psInsertBeforeInst, psArgA, &sNegatedArgA);

		psArgA = &sNegatedArgA;
	}
	if (bNegateB)
	{
		IntegerNegate(psState, psBlock, psInsertBeforeInst, psArgB, &sNegatedArgB);

		psArgB = &sNegatedArgB;
	}

	/*
		Add the low halves of both register together generating a 17-bit result.

		TEMP1 = A.LOW + B.LOW
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IIMAE);

	psInst->u.psImae->bSigned = IMG_FALSE;
	psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_Z16;

	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp1, UF_REGFORMAT_F32);
	SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 1;
	SetSrcFromArg(psState, psInst, 2 /* uSrcIdx */, psArgB);

	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	/*
		Make the carry-in for the next add the top bit of the previous result.

		I2 = TEMP1.HIGH
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IUNPCKU16U16);
	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uCarryTemp, UF_REGFORMAT_F32);
	SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp1, UF_REGFORMAT_F32);
	SetPCKComponent(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 0;
	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	/*
		Add the high halves of both registers together plus the carry-in from the add of the
		low halves.

		TEMP2 = A.HIGH + B.HIGH + I2
	*/
	psInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psInst, IIMAE);

	psInst->u.psImae->bSigned = bSigned;
	psInst->u.psImae->uSrc2Type = bSigned ? USEASM_INTSRCSEL_S16 : USEASM_INTSRCSEL_Z16;

	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp2, UF_REGFORMAT_F32);

	SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psArgA);
	SetComponentSelect(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);

	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 1;

	SetSrcFromArg(psState, psInst, 2 /* uSrcIdx */, psArgB);
	SetComponentSelect(psState, psInst, 2 /* uArgIdx */, 2 /* uComponent */);

	/* Carry source. */
	SetSrc(psState, psInst, IMAE_CARRYIN_SOURCE, USEASM_REGTYPE_TEMP, uCarryTemp, UF_REGFORMAT_F32);

	InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

	if (psDestHigh != NULL)
	{
		IMG_UINT32	uTemp3 = GetNextRegister(psState);

		/*
			Move bit 32 into the high destination.

			DESTHIGH/TEMP3 = TEMP2.HIGH
		*/
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IUNPCKU16U16);
		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp3, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp2, UF_REGFORMAT_F32);
		SetPCKComponent(psState, psInst, 0 /* uArgIdx */, 2 /* uComponent */);
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

		{
			IMG_UINT32	uIdx;

			for (uIdx = 0; uIdx < 2; uIdx++)
			{
				IMG_BOOL	bNegate = (uIdx == 0) ? bNegateA : bNegateB;
				PARG		psArg = (uIdx == 0) ? psArgA : psArgB;

				if (bNegate)
				{
					PINST		psXorInst;
					IMG_UINT32	uPredTemp = psState->uNumPredicates++;
					IMG_UINT32	uOldTemp3;

					/*
						(UNSIGNED) pTEMP = (ARG != 0) ? TRUE : FALSE
						(SIGNED) pTEMP = (ARG == 0x80000000) ? TRUE : FALSE
					*/
					psXorInst = AllocateInst(psState, psInsertBeforeInst);
					SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
					psXorInst->u.psTest->eAluOpcode = IXOR;
					MakePredicateArgument(&psXorInst->asDest[TEST_PREDICATE_DESTIDX], uPredTemp);
					SetSrcFromArg(psState, psXorInst, 0 /* uSrcIdx */, psArg);
					if (bSigned)
					{
						psXorInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
						psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
						psXorInst->asArg[1].uNumber = 0x80000000;
					}
					else
					{
						psXorInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
						psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
						psXorInst->asArg[1].uNumber = 0;
					}
					InsertInstBefore(psState, psBlock, psXorInst, psInsertBeforeInst);

					/*
						(IF pTEMP) XOR TEMP3, TEMP3, 0x1
					*/
					uOldTemp3 = uTemp3;
					uTemp3 = GetNextRegister(psState);

					psXorInst = AllocateInst(psState, psInsertBeforeInst);
					SetOpcode(psState, psXorInst, IXOR);
					SetDest(psState, psXorInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp3, UF_REGFORMAT_F32);
					SetPartialDest(psState, psXorInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uOldTemp3, UF_REGFORMAT_F32);
					SetPredicate(psState, psXorInst, uPredTemp, IMG_FALSE /* bPredNegate */);
					SetSrc(psState, psXorInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uOldTemp3, UF_REGFORMAT_F32);
					psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psXorInst->asArg[1].uNumber = 1;
					InsertInstBefore(psState, psBlock, psXorInst, psInsertBeforeInst);
				}
			}
		}

		if (bSigned || bNegateA || bNegateB)
		{
			IMG_UINT32	uTemp4 = GetNextRegister(psState);

			/*
				TEMP4 = TEMP3 & 1
			*/
			psInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psInst, IAND);
			SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp4, UF_REGFORMAT_F32);
			SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp3, UF_REGFORMAT_F32);
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 1;
			InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);

			/*
				Sign extend from 33-bits to 64-bits.

				DESTHIGH = (TEMP4 == 0) ? 0 : 0xFFFFFFFF
			*/
			psInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psInst, IMOVC_I32);
			psInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;

			SetDestFromArg(psState, psInst, 0 /* uDestIdx */, psDestHigh);

			SetPredicate(psState, psInst, uPredSrc, bPredNegate);

			SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp4, UF_REGFORMAT_F32);
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
			psInst->asArg[2].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_INT32ONE;
			InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);
		}
		else
		{
			/*
				Just move from TEMP3 into the high destination.
			*/
			psInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psInst, IMOV);

			SetDestFromArg(psState, psInst, 0 /* uDestIdx */, psDestHigh);

			SetPredicate(psState, psInst, uPredSrc, bPredNegate);

			SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp3, UF_REGFORMAT_F32);

			InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);
		}
	}

	if (psDestLow != NULL)
	{
		/*
			Combine the top half and bottom half of the result.  
		
			DESTLOW = (TEMP1.LOW, TEMP2.LOW)
		*/
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IUNPCKU16U16);

		SetPredicate(psState, psInst, uPredSrc, bPredNegate);

		SetDestFromArg(psState, psInst, 0 /* uDestIdx */, psDestLow);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp1, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTemp2, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psBlock, psInst, psInsertBeforeInst);
	}
}

static 
IMG_VOID Generate32bitIntegerMad_Non545(PINTERMEDIATE_STATE		psState,
										PCODEBLOCK				psBlock,
										PINST					psInsertBeforeInst,
										PARG					psDestLow,
										PARG					psDestHigh,
										IMG_UINT32				uPredSrc,
										IMG_BOOL				bPredNegate,
										PARG					psArgA,
										IMG_BOOL				bNegateA,
										PARG					psArgB,
										IMG_BOOL				bNegateB,
										PARG					psArgC,
										IMG_BOOL				bNegateC,
										IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: Generate32bitIntegerMad_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit integer multiply-add (A*B + C)

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Point to insert the intermediate instructions at.
			  psDestLow			- Register that will be written with the low 32-bits
								of the result. Can be NULL.
			  psDestHigh		- Register that will be written with the high 32-bits
								of the result. Can be NULL.
			  psArgA			- Source arguments.
			  psArgB
			  psArgC
			  bNegateA			- Source negation modifiers.
			  bNegateB
			  bNegateC
			  bSigned			- TRUE to do a signed multiply.
			  
 RETURNS	: None.
*****************************************************************************/
{
	ARG			sT0, sT1, sT2;
	PARG		psT0, psT1, psT2;

	/*
		Get temporary registers for use in the calcuation.
	*/
	psT0 = &sT0;
	MakeNewTempArg(psState, UF_REGFORMAT_F32, psT0);
	if (psDestHigh != NULL)
	{
		psT1 = &sT1;
		MakeNewTempArg(psState, UF_REGFORMAT_F32, psT1);

		psT2 = &sT2;
		MakeNewTempArg(psState, UF_REGFORMAT_F32, psT2);
	}
	else
	{
		psT1 = psT2 = NULL;
	}

	/*
		T1, T0 = A * B
	*/
	Generate32bitIntegerMul_Non545(psState,
								   psBlock,
								   psInsertBeforeInst,
								   psT0,
								   psT1,
								   uPredSrc,
								   bPredNegate,
								   psArgA,
								   bNegateA,
								   psArgB,
								   bNegateB,
								   bSigned);

	/*
		T2, DEST.LOW = T0 + C
	*/
	Generate32bitIntegerAdd_Non545(psState,
								   psBlock,
								   psInsertBeforeInst,
								   psDestLow,
								   psT2,
								   uPredSrc,
								   bPredNegate,
								   psT0,
								   IMG_FALSE /* bNegateA */,
								   psArgC,
								   bNegateC,
								   bSigned);

	if (psDestHigh != NULL)
	{
		/*
			DEST.HIGH = T1 + T2
		*/
		Generate32bitIntegerAdd_Non545(psState,
									   psBlock,
									   psInsertBeforeInst,
									   psDestHigh,
									   NULL,
									   uPredSrc,
									   bPredNegate,	
									   psT1,
									   IMG_FALSE /* bNegateA */,
									   psT2,
									   IMG_FALSE /* bNegateB */,
									   bSigned);
	}
}

static
PCODEBLOCK GenerateIntegerDividePostCheck_Non545(PINTERMEDIATE_STATE	psState, 
												 PCODEBLOCK		        psCodeBlock,
												 PARG					psQuotientDest, 
												 PARG					psRemainderDest, 
												 PARG					psDividend, 
												 PARG					psNegDivisor)
/*****************************************************************************
 FUNCTION	: GenerateIntegerDividePostCheck_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit unsigned integer divide.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock	    - Where to insert the instruction.
			  psQuotientDest	- Registers to be updated with the divide results.
			  psRemainderDest
			  psDividend		- Arguments to the divide.
			  psNegDivisor
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uNumBitsTemp = GetNextRegister(psState);
	IMG_UINT32	uDividendTemp = GetNextRegister(psState);
	PCODEBLOCK	psPreLoop1Block;
	PCODEBLOCK	psPreLoop2Block;
	PCODEBLOCK	psLoop1;
	PCODEBLOCK	psLoop1BodyRem;
	PCODEBLOCK	psLoop1Break;
	PCODEBLOCK	psLoop2;
	PCODEBLOCK	psLoop2Break;

	psPreLoop1Block = AllocateBlock(psState, psCodeBlock->psOwner);

	/* link control flow */
	SetBlockUnconditional(psState, psCodeBlock, psPreLoop1Block);

	/*
		MOV	numbits, 32
	*/
	{
		PINST	psMovInst;

		psMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovInst, IMOV);
		psMovInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMovInst->asDest[0].uNumber = uNumBitsTemp;
		psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psMovInst->asArg[0].uNumber = 32;
		AppendInst(psState, psPreLoop1Block, psMovInst);
	}

	/*
		MOV	remainder, 0
	*/
	{
		PINST	psMovInst;

		psMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovInst, IMOV);
		psMovInst->asDest[0] = *psRemainderDest;
		psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psMovInst->asArg[0].uNumber = 0;
		AppendInst(psState, psPreLoop1Block, psMovInst);
	}

	/*
		MOV dividend_temp, dividend
	*/
	{
		PINST	psMovInst;

		psMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovInst, IMOV);
		psMovInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMovInst->asDest[0].uNumber = uDividendTemp;
		psMovInst->asArg[0] = *psDividend;
		AppendInst(psState, psPreLoop1Block, psMovInst);
	}

	/*
		for (;;)
		{
	*/
	psLoop1 = AllocateBlock(psState, psCodeBlock->psOwner);
	/* link control flow */
	SetBlockUnconditional(psState, psPreLoop1Block, psLoop1);
	{
		PCODEBLOCK	psLoop1Body;
		IMG_UINT32	uBitTemp = GetNextRegister(psState);
		IMG_UINT32	uRemainder1Temp = GetNextRegister(psState);

		psLoop1Body = AllocateBlock(psState, psCodeBlock->psOwner);
		/* link control flow */
		SetBlockUnconditional(psState, psLoop1, psLoop1Body);

		/*
			bit = (dividend1 & 0x80000000) >> 31;
		*/
		{
			PINST	psShrInst;
	
			psShrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShrInst, ISHR);
			psShrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asDest[0].uNumber = uBitTemp;
			psShrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asArg[0].uNumber = uDividendTemp;
			psShrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShrInst->asArg[1].uNumber = 31;
			AppendInst(psState, psLoop1Body, psShrInst);
		}

		/*
			remainder1 = (remainder << 1) | bit;
		*/
		{
			PINST	psShlInst;
	
			psShlInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShlInst, ISHL);
			psShlInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShlInst->asDest[0].uNumber = uRemainder1Temp;
			psShlInst->asArg[0] = *psRemainderDest;
			psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShlInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop1Body, psShlInst);
		}
		{
			PINST	psOrInst;
	
			psOrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psOrInst, IOR);
			psOrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asDest[0].uNumber = uRemainder1Temp;
			psOrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asArg[0].uNumber = uRemainder1Temp;
			psOrInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asArg[1].uNumber = uBitTemp;
			AppendInst(psState, psLoop1Body, psOrInst);
		}

		/*
			if (remainder1 < divisor) {
				break;
			}
		*/
		{
			ARG			sSignResult;
			ARG			sRemainder1Temp;
			PINST		psOrInst;

			InitInstArg(&sSignResult);
			sSignResult.uType = USEASM_REGTYPE_TEMP;
			sSignResult.uNumber = GetNextRegister(psState);

			InitInstArg(&sRemainder1Temp);
			sRemainder1Temp.uType = USEASM_REGTYPE_TEMP;
			sRemainder1Temp.uNumber = uRemainder1Temp;

			Generate32bitIntegerAdd_Non545(psState, 
										   psLoop1Body, 
										   NULL /* psInsertBeforeInst */, 
										   NULL /* psDestLow */, 
										   &sSignResult, 
										   USC_PREDREG_NONE, 
										   IMG_FALSE, 
										   &sRemainder1Temp, 
										   IMG_FALSE /* bNegateA */,
										   psNegDivisor, 
										   IMG_FALSE /* bNegateB */,
										   IMG_FALSE);

			/*
				OR.testnz	p0, sign_result, #0
			*/
			psOrInst = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psOrInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
			psOrInst->u.psTest->eAluOpcode = IOR;
			MakePredicateArgument(&psOrInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			psOrInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
			psOrInst->asArg[0] = sSignResult;
			psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psOrInst->asArg[1].uNumber = 0;
			AppendInst(psState, psLoop1Body, psOrInst);

			/*
				Add a block to represent the break from the loop.
			*/
			psLoop1Break = AllocateBlock(psState, psCodeBlock->psOwner);

			/*
				Add a new basic block for the instructions after the break.
			*/
			psLoop1BodyRem = AllocateBlock(psState, psCodeBlock->psOwner);

			/* make conditional flow */
			SetBlockConditional(psState, psLoop1Body, USC_PREDREG_TEMP, psLoop1Break, psLoop1BodyRem, IMG_FALSE /* bStatic */);
		}

		/*
			remainder = remainder1
		*/
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMovInst, IMOV);
			psMovInst->asDest[0] = *psRemainderDest;
			psMovInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psMovInst->asArg[0].uNumber = uRemainder1Temp;
			AppendInst(psState, psLoop1BodyRem, psMovInst);
		}

		/*
			dividend1 = dividend1 << 1;
		*/
		{
			PINST	psShlInst;
	
			psShlInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShlInst, ISHL);
			psShlInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShlInst->asDest[0].uNumber = uDividendTemp;
			psShlInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psShlInst->asArg[0].uNumber = uDividendTemp;
			psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShlInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop1BodyRem, psShlInst);
		}

		/*
			num_bits--;
		*/
		{
			PINST	psAddInst;

			psAddInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psAddInst, IISUB16);
			psAddInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psAddInst->asDest[0].uNumber = uNumBitsTemp;
			psAddInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psAddInst->asArg[0].uNumber = uNumBitsTemp;
			psAddInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psAddInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop1BodyRem, psAddInst);
		}

		/* link control flow */
		SetBlockUnconditional(psState, psLoop1BodyRem, psLoop1Body);
	}

	psPreLoop2Block = AllocateBlock(psState, psCodeBlock->psOwner);

	/* link control flow */
	SetBlockUnconditional(psState, psLoop1Break, psPreLoop2Block);

	/*
		quotient = 0;
	*/
	{
		PINST	psMovInst;

		psMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovInst, IMOV);
		psMovInst->asDest[0] = *psQuotientDest;
		psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psMovInst->asArg[0].uNumber = 0;
		AppendInst(psState, psPreLoop2Block, psMovInst);
	}

	/*
		for (i = 0; i < num_bits; i++) {
	*/
	{
		PINST	psIMovInst;

		psIMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcodeAndDestCount(psState, psIMovInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psIMovInst->u.psTest->eAluOpcode = IMOV;
		MakePredicateArgument(&psIMovInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
		psIMovInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
		psIMovInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psIMovInst->asArg[0].uNumber = uNumBitsTemp;
		AppendInst(psState, psPreLoop2Block, psIMovInst);
	}

	psLoop2 = AllocateBlock(psState, psCodeBlock->psOwner);

	/* link control flow */
	SetBlockUnconditional(psState, psPreLoop2Block, psLoop2);

	{
		PCODEBLOCK	psLoop2Body;
		IMG_UINT32	uBitTemp = GetNextRegister(psState);
		IMG_UINT32	uTTemp = GetNextRegister(psState);
		IMG_UINT32	uQTemp = GetNextRegister(psState);

		psLoop2Body = AllocateBlock(psState, psCodeBlock->psOwner);

		/* link control flow */
		SetBlockUnconditional(psState, psLoop2, psLoop2Body);

		/*
			bit = (dividend1 & 0x80000000) >> 31;
		*/
		{
			PINST	psShrInst;
	
			psShrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShrInst, ISHR);
			psShrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asDest[0].uNumber = uBitTemp;
			psShrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asArg[0].uNumber = uDividendTemp;
			psShrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShrInst->asArg[1].uNumber = 31;
			AppendInst(psState, psLoop2Body, psShrInst);
		}

		/*
			remainder = (remainder << 1) | bit;
		*/
		{
			PINST	psShlInst;
	
			psShlInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShlInst, ISHL);
			psShlInst->asDest[0] = *psRemainderDest;
			psShlInst->asArg[0] = *psRemainderDest;
			psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShlInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop2Body, psShlInst);
		}
		{
			PINST	psOrInst;
	
			psOrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psOrInst, IOR);
			psOrInst->asDest[0] = *psRemainderDest;
			psOrInst->asArg[0] = *psRemainderDest;
			psOrInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asArg[1].uNumber = uBitTemp;
			AppendInst(psState, psLoop2Body, psOrInst);
		}

		/*
			t = remainder - divisor;
		*/
		{
			ARG	sT;

			InitInstArg(&sT);
			sT.uType = USEASM_REGTYPE_TEMP;
			sT.uNumber = uTTemp;

			Generate32bitIntegerAdd_Non545(psState,
										   psLoop2Body,
										   NULL /* psInsertBeforeInst */, 
										   &sT,
										   NULL,
										   USC_PREDREG_NONE,
										   IMG_FALSE,
										   psRemainderDest,
										   IMG_FALSE /* bNegateA */,
										   psNegDivisor,
										   IMG_FALSE /* bNegateB */,
										   IMG_FALSE);
		}

		/*
			q = !((t & 0x80000000) >> 31);
		*/
		{
			PINST	psShrInst;
	
			psShrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShrInst, ISHR);
			psShrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asDest[0].uNumber = uQTemp;
			psShrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psShrInst->asArg[0].uNumber = uTTemp;
			psShrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShrInst->asArg[1].uNumber = 31;
			AppendInst(psState, psLoop2Body, psShrInst);
		}
		{
			PINST	psXorInst;
	
			psXorInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psXorInst, IXOR);
			psXorInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psXorInst->asDest[0].uNumber = uQTemp;
			psXorInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psXorInst->asArg[0].uNumber = uQTemp;
			psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psXorInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop2Body, psXorInst);
		}

		/*
			dividend1 = dividend1 << 1;
		*/	
		{
			PINST	psShlInst;
	
			psShlInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShlInst, ISHL);
			psShlInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psShlInst->asDest[0].uNumber = uDividendTemp;
			psShlInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psShlInst->asArg[0].uNumber = uDividendTemp;
			psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShlInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop2Body, psShlInst);
		}

		/*
			 quotient = (quotient << 1) | q;
		*/
		{
			PINST	psShlInst;
	
			psShlInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psShlInst, ISHL);
			psShlInst->asDest[0] = *psQuotientDest;
			psShlInst->asArg[0] = *psQuotientDest;
			psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psShlInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop2Body, psShlInst);
		}
		{
			PINST	psOrInst;
	
			psOrInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psOrInst, IOR);
			psOrInst->asDest[0] = *psQuotientDest;
			psOrInst->asArg[0] = *psQuotientDest;
			psOrInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asArg[1].uNumber = uQTemp;
			AppendInst(psState, psLoop2Body, psOrInst);
		}

		/*
			if (q) {
				remainder = t;
			}
		*/
		{
			PINST	psOrInst;

			psOrInst = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psOrInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
			psOrInst->u.psTest->eAluOpcode = IOR;
			MakePredicateArgument(&psOrInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			psOrInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
			psOrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psOrInst->asArg[0].uNumber = uQTemp;
			psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psOrInst->asArg[1].uNumber = 0;
			AppendInst(psState, psLoop2Body, psOrInst);
		}
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMovInst, IMOV);
			
			SetPredicate(psState, psMovInst, USC_PREDREG_TEMP, IMG_FALSE /* bPredNegate */);

			psMovInst->asDest[0] = *psRemainderDest;
			psMovInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psMovInst->asArg[0].uNumber = uTTemp;
			AppendInst(psState, psLoop2Body, psMovInst);
		}

		/*
			}
		*/
		{
			PINST	psSubInst;

			/*
				SUB.testnz		num_bits, num_bit, 1
			*/
			psSubInst = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psSubInst, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
			psSubInst->u.psTest->eAluOpcode = IISUB16;
			MakePredicateArgument(&psSubInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			psSubInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
			psSubInst->asDest[TEST_UNIFIEDSTORE_DESTIDX].uType = USEASM_REGTYPE_TEMP;
			psSubInst->asDest[TEST_UNIFIEDSTORE_DESTIDX].uNumber = uNumBitsTemp;
			psSubInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psSubInst->asArg[0].uNumber = uNumBitsTemp;
			psSubInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psSubInst->asArg[1].uNumber = 1;
			AppendInst(psState, psLoop2Body, psSubInst);

			psLoop2Break = AllocateBlock(psState, psCodeBlock->psOwner);
			/* make conditional flow */
			SetBlockConditional(psState, psLoop2Body, USC_PREDREG_TEMP, psLoop2Break, psLoop2Body, IMG_FALSE);
		}
	}
	return psLoop2Break;
}

static PCODEBLOCK GenerateIntegerDivide_Non545(PINTERMEDIATE_STATE	psState,
												PCODEBLOCK            psCodeBlock, 
												PARG					psQuotientDest, 
												PARG					psRemainderDest, 
												PARG					psDividend, 
												PARG					psDivisor)
/*****************************************************************************
 FUNCTION	: GenerateIntegerDivide_Non545
    
 PURPOSE	: Generate instructions to do a 32-bit unsigned integer divide.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock	    - Where to insert the instruction.
			  psQuotientDest	- Registers to be updated with the divide results.
			  psRemainderDest
			  psDividend		- Arguments to the divide.
			  psDivisor
			  
 RETURNS	: None.
*****************************************************************************/
{
	PCODEBLOCK	psDivisorZeroTrue;
	PCODEBLOCK	psDivisorEqDividendTrue;
	PCODEBLOCK	psDividendLtDivisorTrue;
	PCODEBLOCK	psExitConversion;

	/* allocate a block to exit from this conversion */
	psExitConversion = AllocateBlock(psState, psCodeBlock->psOwner);

	/*
		Check for a divisor of zero

			OR.testz	pTEMP, DIVISOR, #0
	*/
	{
		PINST	psOrInst;

		psOrInst = AllocateInst(psState, IMG_NULL);
		SetOpcodeAndDestCount(psState, psOrInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psOrInst->u.psTest->eAluOpcode = IOR;
		MakePredicateArgument(&psOrInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
		psOrInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
		psOrInst->asArg[0] = *psDivisor;
		psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psOrInst->asArg[1].uNumber = 0;
		AppendInst(psState, psCodeBlock, psOrInst);
	}
	
	/*
		IF divisor == 0
	*/
	/*
		{
	*/
	{
		psDivisorZeroTrue = AllocateBlock(psState, psCodeBlock->psOwner);

		/* unconditional jump to exit block */
		SetBlockUnconditional(psState, psDivisorZeroTrue, psExitConversion);

		/*
			MOV	quotient, #0xFFFFFFFF
		*/
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMovInst, IMOV);
			psMovInst->asDest[0] = *psQuotientDest;
			psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psMovInst->asArg[0].uNumber = 0xFFFFFFFF;
			AppendInst(psState, psDivisorZeroTrue, psMovInst);
		}

		/*
			MOV	remainder, #0xFFFFFFFF
		*/
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMovInst, IMOV);
			psMovInst->asDest[0] = *psRemainderDest;
			psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psMovInst->asArg[0].uNumber = 0xFFFFFFFF;
			AppendInst(psState, psDivisorZeroTrue, psMovInst);
		}
	}
	/*
		}
		else
		{
	*/
	{
        PCODEBLOCK	psDivisorZeroFalse;

		psDivisorZeroFalse = AllocateBlock(psState, psCodeBlock->psOwner);

		/* make conditional flow */
		SetBlockConditional(psState, psCodeBlock, USC_PREDREG_TEMP, psDivisorZeroTrue, psDivisorZeroFalse, IMG_FALSE);

		/*
			Check for the divisor and dividend equal

			XOR.testnz	pTEMP, divisor, dividend
		*/
		{
			PINST	psXorInst;
	
			psXorInst = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
			psXorInst->u.psTest->eAluOpcode = IXOR;
			MakePredicateArgument(&psXorInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			psXorInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
			psXorInst->asArg[0] = *psDivisor;
			psXorInst->asArg[1] = *psDividend;
			AppendInst(psState, psDivisorZeroFalse, psXorInst);
		}

		/*
			IF divisor == dividend
		*/
		/*
			{
		*/
		{
			psDivisorEqDividendTrue = AllocateBlock(psState, psCodeBlock->psOwner);

			/* unconditional jump to exit block */
			SetBlockUnconditional(psState, psDivisorEqDividendTrue, psExitConversion);

			/*
				MOV	quotient, #0x1
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOV);
				psMovInst->asDest[0] = *psQuotientDest;
				psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[0].uNumber = 0x1;
				AppendInst(psState, psDivisorEqDividendTrue, psMovInst);
			}

			/*
				MOV	remainder, #0x0
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOV);
				psMovInst->asDest[0] = *psRemainderDest;
				psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[0].uNumber = 0x0;
				AppendInst(psState, psDivisorEqDividendTrue, psMovInst);
			}
		}
		/*
			}
			else
			{
		*/
		{
			PCODEBLOCK	psDivisorEqDividendFalse;
			ARG			sNegDivisor;
			ARG			sDividendDivisorCmpResult;

			psDivisorEqDividendFalse = AllocateBlock(psState, psCodeBlock->psOwner);

			/* make conditional flow */
			SetBlockConditional(psState, psDivisorZeroFalse, USC_PREDREG_TEMP, psDivisorEqDividendTrue, psDivisorEqDividendFalse, IMG_FALSE);

			/*
				Calculate -DIVISOR
			*/
			IntegerNegate(psState, psDivisorEqDividendFalse, NULL /* psInsertBeforeInst */, psDivisor, &sNegDivisor);

			/*
				Calculate DIVIDEND + (-DIVISOR)
			*/
			InitInstArg(&sDividendDivisorCmpResult);
			sDividendDivisorCmpResult.uType = USEASM_REGTYPE_TEMP;
			sDividendDivisorCmpResult.uNumber = GetNextRegister(psState);
			Generate32bitIntegerAdd_Non545(psState, 
										   psDivisorEqDividendFalse, 
										   NULL /* psInsertBeforeInst */, 
										   NULL /* psDestLow */, 
										   &sDividendDivisorCmpResult, 
										   USC_PREDREG_NONE, 
										   IMG_FALSE, 
										   psDividend, 
										   IMG_FALSE /* bNegateA */,
										   &sNegDivisor, 
										   IMG_FALSE /* bNegateB */,
										   IMG_FALSE);

			/*
				Check for (DIVIDEND + (-DIVISOR)) < 0

				
			*/
			{
				PINST	psOrInst;

				psOrInst = AllocateInst(psState, IMG_NULL);
				SetOpcodeAndDestCount(psState, psOrInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
				psOrInst->u.psTest->eAluOpcode = IOR;
				MakePredicateArgument(&psOrInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
				psOrInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
				psOrInst->asArg[0] = sDividendDivisorCmpResult;
				psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psOrInst->asArg[1].uNumber = 0;
				AppendInst(psState, psDivisorEqDividendFalse, psOrInst);
			}

			/*
				IF (DIVIDEND + (-DIVISOR)) < 0
				{
			*/
			{
				psDividendLtDivisorTrue = AllocateBlock(psState, psCodeBlock->psOwner);

				/* unconditional jump to exit block */
				SetBlockUnconditional(psState, psDividendLtDivisorTrue, psExitConversion);

				/*
					MOV	quotient, #0x0
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = *psQuotientDest;
					psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[0].uNumber = 0x0;
					AppendInst(psState, psDividendLtDivisorTrue, psMovInst);
				}

				/*
					MOV	remainder, dividend
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = *psRemainderDest;
					psMovInst->asArg[0] = *psDividend;
					AppendInst(psState, psDividendLtDivisorTrue, psMovInst);
				}
			}
			/*
				}
				else
				{
			*/
			{
				PCODEBLOCK	psDividendLtDivisorFalse;
				PCODEBLOCK  psExitConversionInner;

				psDividendLtDivisorFalse = AllocateBlock(psState, psCodeBlock->psOwner);
				/* make conditional flow */
				SetBlockConditional(psState, psDivisorEqDividendFalse, USC_PREDREG_TEMP, psDividendLtDivisorTrue, psDividendLtDivisorFalse, IMG_FALSE);

				psExitConversionInner = 
					GenerateIntegerDividePostCheck_Non545(psState,
					                                      psDividendLtDivisorFalse,
													      psQuotientDest,
													      psRemainderDest,
													      psDividend,
													      &sNegDivisor);

				/* unconditional jump to exit block for inner conversion */
				SetBlockUnconditional(psState, psExitConversionInner, psExitConversion);
			}
		}
	}
	return psExitConversion;
}

IMG_INTERNAL
IMG_VOID GenerateIntegerArithmetic(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psBlock,
								   PINST					psInsertBeforeInst,
								   UF_OPCODE				eOpcode,
								   PARG						psDestLow,
								   PARG						psDestHigh,
								   IMG_UINT32				uPredSrc,
								   IMG_BOOL					bPredNegate,
								   PARG						psArgA,
								   IMG_BOOL					bNegateA,
								   PARG						psArgB,
								   IMG_BOOL					bNegateB,
								   PARG						psArgC,
								   IMG_BOOL					bNegateC,
								   IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: GenerateIntegerArithmetic

 PURPOSE	: Generates instructions to implement an integer arithmetic operation.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Codeblock into which to insert instructions
			  psInsertBeforeInst	- Point to insert the instructions at.
			  eOpcode				- Operation to perform. Can be UFOP_ADD, UFOP_MUL,
									UFOP_MUL2 or UFOP_MAD.
			  psDestLow,			- Registers for the results of the operation.
			  psDestHigh
			  uPredSrc				- Predicate to guard update of the destination
			  bPredNegate			registers.
			  psArgA,				- Arguments to the operation/
			  psArgB,
			  psArgC
			  bNegateA				- Argument negate modifiers.
			  bNegateB
			  bNegateC
			  bSigned				- If TRUE do signed arithmetic.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/* Argument required by atomic functions, as their return value is loaded
	 from memory, rather than the result of the associated computation. */
	ARG sAtomicNew, sAddress;
	IMG_BOOL bDoStore = IMG_FALSE;
	#if defined(SUPPORT_SGX545)
	if (
		 (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) && 
		 !(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32)
	   )

	{
		GenerateIntegerArithmetic_545(psState,
									  psBlock,
									  psInsertBeforeInst,
									  eOpcode,
									  psDestLow,
									  psDestHigh,
									  uPredSrc,
									  bPredNegate,
									  psArgA,
									  bNegateA,
									  psArgB,
									  bNegateB,
									  psArgC,
									  bNegateC,
									  bSigned);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
		 (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) != 0 && 
		 (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) != 0 && 
		 psDestHigh == IMG_NULL
	   )
	{
		GenerateIntegerArithmetic_543(psState,
									  psBlock,
									  psInsertBeforeInst,
									  eOpcode,
									  psDestLow,
									  uPredSrc,
									  bPredNegate,
									  psArgA,
									  bNegateA,
									  psArgB,
									  bNegateB,
									  psArgC,
									  bNegateC,
									  bSigned);
	}
	else 
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		switch (eOpcode)
		{
			case UFOP_ATOM_INC:
			case UFOP_ATOM_DEC:
			{
				/* No modifiers for second operand */
				bNegateB = IMG_FALSE;
				/* Second argument for atomic inc/dec is an immediate */
				InitInstArg(psArgB);
				psArgB->uType = USEASM_REGTYPE_IMMEDIATE;
				psArgB->uNumber = (IMG_UINT)((eOpcode == UFOP_ATOM_INC) ? 1 : -1);
			}
			case UFOP_ATOM_ADD:
			{
				/* Generate a temporary register */
				InitInstArg(&sAtomicNew);
				sAtomicNew.uType = USEASM_REGTYPE_TEMP;
				sAtomicNew.uNumber = GetNextRegister(psState);
				sAtomicNew.eFmt = psDestLow->eFmt;
				/* Remember to store the old result later */
				sAddress = *psArgA;
				bDoStore = IMG_TRUE;
				/* All atomics have to memload the first argument */
				GenerateAtomicLoad(psState, psBlock, &sAddress, psDestLow);
				/* Flip ArgA with value loaded from memory so that
				 common functions work as they should */
				*psArgA = *psDestLow;
				/* Replace the return register which we will store later */
				*psDestLow = sAtomicNew;
			}
			case UFOP_ADD:
			case UFOP_ADD2:
			{
				Generate32bitIntegerAdd_Non545(psState,
											   psBlock,
											   psInsertBeforeInst,
											   psDestLow,
											   psDestHigh,
											   uPredSrc,
											   bPredNegate,
											   psArgA,
											   bNegateA,
											   psArgB,
											   bNegateB,
											   bSigned);
				break;
			}
			case UFOP_MUL:
			case UFOP_MUL2:
			{
				Generate32bitIntegerMul_Non545(psState,
											   psBlock,
											   psInsertBeforeInst,
											   psDestLow,
											   psDestHigh,
											   uPredSrc,
											   bPredNegate,
											   psArgA,
											   bNegateA,
											   psArgB,
											   bNegateB,
											   bSigned);
				break;
			}
			case UFOP_MAD:
			case UFOP_MAD2:
			{
				Generate32bitIntegerMad_Non545(psState,
											   psBlock,
											   psInsertBeforeInst,
											   psDestLow,
											   psDestHigh,
											   uPredSrc,
											   bPredNegate,
											   psArgA,
											   bNegateA,
											   psArgB,
											   bNegateB,
											   psArgC,
											   bNegateC,
											   bSigned);
				break;
			}
			default:
			{
			    break;
			}
		}

		if (bDoStore)
		{
			/* End of an atomic operation, store the new value */
			GenerateAtomicStore(psState, psBlock, &sAddress, psDestLow);
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID Generate32BitComparison_543(PINTERMEDIATE_STATE	psState, 
									 PCODEBLOCK				psCodeBlock, 
									 PARG					psArgA, 
									 IMG_BOOL				bNegateA,
									 PARG					psArgB, 
									 IMG_BOOL				bNegateB,
									 IMG_BOOL				bSigned, 
									 PARG					psSignBit)
/*****************************************************************************
 FUNCTION	: Generate32BitComparison_543

 PURPOSE	: Generates the sign bit of B - A.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psArgA				- Register containing A.
			  bNegateA				- Negation modifier for A.
			  psArgB				- Register containing B.
			  bNegateB				- Negation modifier for B.
			  bSigned				- Whether to do a signed or an unsigned
									subtract.
			  psSignBit				- Returns a register containing the sign bit.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psTestMaskInst;
	ARG			sNegatedArgA;
	ARG			sNegatedArgB;

	/*
		Apply negation modifiers to the sources.
	*/
	if (bNegateA)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgA, &sNegatedArgA);

		psArgA = &sNegatedArgA;
	}
	if (bNegateB)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgB, &sNegatedArgB);

		psArgB = &sNegatedArgB;
	}

	psTestMaskInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psTestMaskInst, ITESTMASK, 1 /* uDestCount */);
	
	psTestMaskInst->asArg[0] = *psArgB;
	psTestMaskInst->asArg[1] = *psArgA;

	InitInstArg(psSignBit);
	psSignBit->uType = USEASM_REGTYPE_TEMP;
	psSignBit->uNumber = GetNextRegister(psState);

	psTestMaskInst->asDest[0] = *psSignBit;

	if(bSigned)
	{
		psTestMaskInst->u.psTest->eAluOpcode = IISUB32;
	}
	else
	{
		psTestMaskInst->u.psTest->eAluOpcode = IISUBU32;
	}

	psTestMaskInst->u.psTest->sTest.eType = TEST_TYPE_LT_ZERO;

	/*
		Set the test-mask flag to DWORD.
	*/
	psTestMaskInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;

	AppendInst(psState, psCodeBlock, psTestMaskInst);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID GenerateInt16Arithmetic(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psBlock,
								 UF_OPCODE				eOpcode,
								 PARG					psDestLow,
								 IMG_UINT32				uPredSrc,
								 IMG_BOOL				bPredNegate,
								 PARG					asArgs,
								 PFLOAT_SOURCE_MODIFIER	asArgMods,
								 IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: GenerateInt16Arithmetic

 PURPOSE	: Generates instructions to implement a 16-bit integer arithmetic operation.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Codeblock into which to insert instructions
			  eOpcode				- Operation to perform. Can be UFOP_ADD, UFOP_MUL,
									UFOP_MUL2 or UFOP_MAD.
			  psDestLow,			- Registers for the results of the operation.
			  psDestHigh
			  uPredSrc				- Predicate to guard update of the destination
			  bPredNegate			registers.
			  asArgs				- Arguments to the operation
			  asArgMods				- Argument modifiers for the operation.
			  bSigned				- If TRUE do signed arithmetic.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psImaeInst;
	PARG		apsArgs[3];
	ARG			asNegArgs[3];
	IMG_UINT32	uArg;

	PVR_UNREFERENCED_PARAMETER(bSigned);

	/*
		Apply negate modifiers in a seperate instruction.
	*/
	for (uArg = 0; uArg < g_asInputInstDesc[eOpcode].uNumSrcArgs; uArg++)
	{
		if (asArgMods[uArg].bNegate)
		{
			IntegerNegate(psState, psBlock, NULL /* psInsertInstBefore */, &asArgs[uArg], &asNegArgs[uArg]);
			apsArgs[uArg] = &asNegArgs[uArg];
		}
		else
		{
			apsArgs[uArg] = &asArgs[uArg];
		}
	}

	psImaeInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psImaeInst, IIMAE);

	/*
		Copy the destination to the instruction.
	*/
	psImaeInst->asDest[0] = *psDestLow;

	/*
		Copy the predicate to the instruction.
	*/
	SetPredicate(psState, psImaeInst, uPredSrc, bPredNegate);

	/*
		Use only the low 16 bits of source 2.
	*/
	psImaeInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_Z16;

	switch (eOpcode)
	{
		case UFOP_ADD:
		case UFOP_ADD2:
		{
			/*
				DEST = SRCA * 1 + SRCB
			*/
			psImaeInst->asArg[0] = *apsArgs[0];
			psImaeInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[1].uNumber = 1;
			psImaeInst->asArg[2] = *apsArgs[1];
			break;
		}
		case UFOP_MUL:
		case UFOP_MUL2:
		{
			/*
				DEST = SRCA * SRCB + 0
			*/
			psImaeInst->asArg[0] = *apsArgs[0];
			psImaeInst->asArg[1] = *apsArgs[1];
			psImaeInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[2].uNumber = 0;
			break;
		}
		case UFOP_MAD:
		case UFOP_MAD2:
		{
			/*
				DEST = SRCA * SRCB + SRC2
			*/
			psImaeInst->asArg[0] = *apsArgs[0];
			psImaeInst->asArg[1] = *apsArgs[1];
			psImaeInst->asArg[2] = *apsArgs[2];
			break;
		}
		default: imgabort();
	}

	AppendInst(psState, psBlock, psImaeInst);
}

static
IMG_VOID GenerateSignResult(PINTERMEDIATE_STATE	psState, 
							PCODEBLOCK			psCodeBlock,
							PARG				psArgA,
							IMG_BOOL			bNegateA,
							PARG				psArgB,
							IMG_BOOL			bNegateB,
							IMG_BOOL			bSigned,
							PARG				psSignBit)
/*****************************************************************************
 FUNCTION	: GenerateSignResult

 PURPOSE	: Generates the sign bit of B - A.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psArgA				- Register containing A.
			  bNegateA				- If TRUE negate A.
			  psArgB				- Register containing B.
			  bNegateB				- If TRUE negate B.
			  bSigned				- Whether to do a signed or an unsigned
									subtract.
			  psSignBit				- Returns a register containing the sign bit.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX545)
	if ( 
		 (
		   (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) && 
		   !(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32)
		 )
	   )
	{
		ARG		sConstArg;

		/*
			1. compute B-A. We do this as (-1)A + B, i.e. by multiplying by -1,
			rather than using the negate modifier - consider 0x8000 0000 = -2^31. Negation in 32 bits leaves
			unchanged, which is then interpreted as -2^31 by the subsequent signed multiply/add; instead
			multiplying by -1 makes a signed _64-bit_ representation of 2^31).
		*/
		ImmediateConstant(1, &sConstArg);

		MakeNewTempArg(psState, UF_REGFORMAT_F32, psSignBit);

		GenerateIMA32Instruction(psState,
								 psCodeBlock,
								 UFOP_MAD,
								 NULL /* psDestLow */,
								 psSignBit,
								 USC_PREDREG_NONE,
								 IMG_FALSE /* bPredNegate */,
								 psArgA,
								 bNegateA,
								 &sConstArg,
								 IMG_TRUE /* bNegateB */,
								 psArgB,
								 bNegateB,
								 bSigned,
								 IMG_NULL);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		Generate32BitComparison_543(psState, 
									psCodeBlock, 
									psArgA, 
									bNegateA,
									psArgB, 
									bNegateB,
									bSigned, 
									psSignBit);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		InitInstArg(psSignBit);
		psSignBit->uType = USEASM_REGTYPE_TEMP;
		psSignBit->uNumber = GetNextRegister(psState);

		Generate32bitIntegerAdd_Non545(psState,
									   psCodeBlock,
									   NULL /* psInsertInstBefore */, 
									   NULL /* psDestLow */,
									   psSignBit,
									   USC_PREDREG_NONE,
									   IMG_FALSE,
									   psArgA,
									   !bNegateA,
									   psArgB,
									   bNegateB,
									   bSigned);
	}
}

static
IMG_VOID GenerateInt8Comparison(PINTERMEDIATE_STATE		psState,
								 PCODEBLOCK					psCodeBlock,
								 UFREG_COMPOP				eCompType,
								 PARG						psArgA,
								 PARG						psArgB,
								 IMG_BOOL					bSigned,
								 PARG						psResult,
								 IMG_UINT32					uPredSrc,
								 IMG_BOOL					bPredNegate)
/*****************************************************************************
 FUNCTION	: GenerateInt8Comparison

 PURPOSE	: Generate intermediate instructions to write a register with
			  0xFF if the comparison between two values succeeds or 0x0
			  if it fails.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  eCompType				- Comparison type.
			  psArgA				- Source arguments to compare.
			  psArgB
			  bSigned				- If TRUE do a signed comparison.
									  If FALSE do an unsigned comparison.
			  psResult				- Destination to write with the comparison
									result.
			  uPredSrc				- Predicate to guard update of the destination.
			  bPredNegate

 RETURNS	: Nothing
*****************************************************************************/
{
	PINST		psTMInst;

	psTMInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psTMInst, ITESTMASK);

	psTMInst->u.psTest->eAluOpcode = bSigned ? IISUB8 : IISUBU8;
	CompOpToTest(psState, eCompType, &psTMInst->u.psTest->sTest);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psTMInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
	}
	else
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psTMInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_BYTE;
	}

	SetPredicate(psState, psTMInst, uPredSrc, bPredNegate);

	psTMInst->asDest[0] = *psResult;

	psTMInst->asArg[0] = *psArgA;
	psTMInst->asArg[1] = *psArgB;

	AppendInst(psState, psCodeBlock, psTMInst);
}

static
IMG_VOID ConvertMinMaxInstructionInt8(PINTERMEDIATE_STATE	psState, 
									   PCODEBLOCK			psCodeBlock,
								       PARG					psDest,
									   IMG_UINT32			uPredSrc,
									   IMG_BOOL				bPredNegate,
									   PARG					psArgA,
									   IMG_BOOL				bNegateA,
									   PARG					psArgB,
									   IMG_BOOL				bNegateB,
									   IMG_BOOL				bSigned,
									   IMG_BOOL				bMax)
/*****************************************************************************
 FUNCTION	: ConvertMinMaxInstructionInt8

 PURPOSE	: Converts one channel of an I8_UN or U8_UN MIN or MAX instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  psDest				- Destination for the min/max result.
			  uPredSrc				- Predicate to control update of the
			  bPredNegate			destination.
			  psArgA				- First argument to the min/max.
			  bNegateA
			  psArgB				- Second argument to the min/max.
			  bNegateB
			  bSigned				- If TRUE do a signed comparison.
			  bMax					- If TRUE do a max operation.
									  If FALSE do a min operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMovcInst;
	ARG		sCompResult;
	ARG		sNegArgA;
	ARG		sNegArgB;

	/*
		If either of the arguments have the negate flag then
		take the negation in a separate instruction.
	*/
	if (bNegateA)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgA, &sNegArgA);

		psArgA = &sNegArgA;
	}
	if (bNegateB)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgB, &sNegArgB);

		psArgB = &sNegArgB;
	}

	/*
		Create a new result 
	*/
	InitInstArg(&sCompResult);
	sCompResult.uType = USEASM_REGTYPE_TEMP;
	sCompResult.uNumber = GetNextRegister(psState);

	/*
		Generate the comparison between the two arguments.

		COMP_RESULT = ArgA > ArgB ? 0xFF : 0x0
	*/
	GenerateInt8Comparison(psState, 
							psCodeBlock, 
							UFREG_COMPOP_GT, 
							psArgA, 
							psArgB, 
							bSigned, 
							&sCompResult,
							USC_PREDREG_NONE,
							IMG_FALSE /* bPredNegate */);

	/* 2. "main" instruction conditionally moves either A or B */
	psMovcInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMovcInst, IMOVC_U8);
	psMovcInst->asDest[0] = *psDest;

	SetPredicate(psState, psMovcInst, uPredSrc, bPredNegate);

	psMovcInst->asArg[0] = sCompResult;
	psMovcInst->u.psMovc->eTest = bMax ? TEST_TYPE_NEQ_ZERO : TEST_TYPE_EQ_ZERO;
	psMovcInst->asArg[1] = *psArgA;
	psMovcInst->asArg[2] = *psArgB;
	
	/* has already been predicated and had destination set. */
	AppendInst(psState, psCodeBlock, psMovcInst);

}

static
IMG_VOID GenerateInt16Comparison(PINTERMEDIATE_STATE		psState,
								 PCODEBLOCK					psCodeBlock,
								 UFREG_COMPOP				eCompType,
								 PARG						psArgA,
								 PARG						psArgB,
								 IMG_BOOL					bSigned,
								 PARG						psResult,
								 IMG_UINT32					uPredSrc,
								 IMG_BOOL					bPredNegate)
/*****************************************************************************
 FUNCTION	: GenerateInt16Comparison

 PURPOSE	: Generate intermediate instructions to write a register with
			  0xFFFF if the comparison between two values succeeds or 0x0
			  if it fails.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  eCompType				- Comparison type.
			  psArgA				- Source arguments to compare.
			  psArgB
			  bSigned				- If TRUE do a signed comparison.
									  If FALSE do an unsigned comparison.
			  psResult				- Destination to write with the comparison
									result.
			  uPredSrc				- Predicate to guard update of the destination.
			  bPredNegate

 RETURNS	: Nothing
*****************************************************************************/
{
	PINST		psTMInst;

	psTMInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psTMInst, ITESTMASK);

	psTMInst->u.psTest->eAluOpcode = bSigned ? IISUB16 : IISUBU16;
	CompOpToTest(psState, eCompType, &psTMInst->u.psTest->sTest);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psTMInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psTMInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_WORD;
	}

	SetPredicate(psState, psTMInst, uPredSrc, bPredNegate);

	psTMInst->asDest[0] = *psResult;

	psTMInst->asArg[0] = *psArgA;
	psTMInst->asArg[1] = *psArgB;

	AppendInst(psState, psCodeBlock, psTMInst);
}

static
IMG_VOID ConvertMinMaxInstructionInt16(PINTERMEDIATE_STATE	psState, 
									   PCODEBLOCK			psCodeBlock,
								       PARG					psDest,
									   IMG_UINT32			uPredSrc,
									   IMG_BOOL				bPredNegate,
									   PARG					psArgA,
									   IMG_BOOL				bNegateA,
									   PARG					psArgB,
									   IMG_BOOL				bNegateB,
									   IMG_BOOL				bSigned,
									   IMG_BOOL				bMax)
/*****************************************************************************
 FUNCTION	: ConvertMinMaxInstructionInt16

 PURPOSE	: Converts one channel of an I16 or U16 MIN or MAX instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  psDest				- Destination for the min/max result.
			  uPredSrc				- Predicate to control update of the
			  bPredNegate			destination.
			  psArgA				- First argument to the min/max.
			  bNegateA
			  psArgB				- Second argument to the min/max.
			  bNegateB
			  bSigned				- If TRUE do a signed comparison.
			  bMax					- If TRUE do a max operation.
									  If FALSE do a min operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMovcInst;
	ARG		sCompResult;
	ARG		sNegArgA;
	ARG		sNegArgB;

	/*
		If either of the arguments have the negate flag then
		take the negation in a separate instruction.
	*/
	if (bNegateA)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgA, &sNegArgA);

		psArgA = &sNegArgA;
	}
	if (bNegateB)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgB, &sNegArgB);

		psArgB = &sNegArgB;
	}

	/*
		Create a new result 
	*/
	InitInstArg(&sCompResult);
	sCompResult.uType = USEASM_REGTYPE_TEMP;
	sCompResult.uNumber = GetNextRegister(psState);

	/*
		Generate the comparison between the two arguments.

		COMP_RESULT = ArgA > ArgB ? 0xFFFF : 0x0
	*/
	GenerateInt16Comparison(psState, 
							psCodeBlock, 
							UFREG_COMPOP_GT, 
							psArgA, 
							psArgB, 
							bSigned, 
							&sCompResult,
							USC_PREDREG_NONE,
							IMG_FALSE /* bPredNegate */);

	/* 2. "main" instruction conditionally moves either A or B */
	psMovcInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMovcInst, IMOVC_I16);
	psMovcInst->asDest[0] = *psDest;

	SetPredicate(psState, psMovcInst, uPredSrc, bPredNegate);

	psMovcInst->asArg[0] = sCompResult;
	psMovcInst->u.psMovc->eTest = bMax ? TEST_TYPE_NEQ_ZERO : TEST_TYPE_EQ_ZERO;
	psMovcInst->asArg[1] = *psArgA;
	psMovcInst->asArg[2] = *psArgB;
	
	/* has already been predicated and had destination set. */
	AppendInst(psState, psCodeBlock, psMovcInst);

}

static
IMG_VOID ConvertMinMaxInstructionInt32(PINTERMEDIATE_STATE	psState, 
									   PCODEBLOCK			psCodeBlock,
								       PARG					psDest,
									   IMG_UINT32			uPredSrc,
									   IMG_BOOL				bPredNegate,
									   PARG					psArgA,
									   IMG_BOOL				bNegateA,
									   PARG					psArgB,
									   IMG_BOOL				bNegateB,
									   IMG_BOOL				bSigned,
									   IMG_BOOL				bMax)
/*****************************************************************************
 FUNCTION	: ConvertMinMaxInstructionInt32

 PURPOSE	: Converts one channel of an I32 or U32 MIN or MAX instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  psDest				- Destination for the min/max result.
			  uPredSrc				- Predicate to control update of the
			  bPredNegate			destination.
			  psArgA				- First argument to the min/max.
			  bNegateA
			  psArgB				- Second argument to the min/max.
			  bNegateB
			  bSigned				- If TRUE do a signed comparison.
			  bMax					- If TRUE do a max operation.
									  If FALSE do a min operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMovcInst;
	ARG		sSignBit;
	ARG		sNegArgA;
	ARG		sNegArgB;

	/*
		Generate the comparison between the two arguments.
	*/
	GenerateSignResult(psState,
					   psCodeBlock,
					   psArgA,
					   bNegateA,
					   psArgB,
					   bNegateB,
					   bSigned,
					   &sSignBit);

	/*
		If either of the arguments have the negate flag then
		take the negation in a separate instruction.
	*/
	if (bNegateA)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgA, &sNegArgA);

		psArgA = &sNegArgA;
	}
	if (bNegateB)
	{
		IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, psArgB, &sNegArgB);

		psArgB = &sNegArgB;
	}

	/* 2. "main" instruction conditionally moves either A or B */
	psMovcInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMovcInst, IMOVC_I32);
	psMovcInst->asDest[0] = *psDest;

	SetPredicate(psState, psMovcInst, uPredSrc, bPredNegate);

	psMovcInst->asArg[0] = sSignBit;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		psMovcInst->u.psMovc->eTest = TEST_TYPE_LT_ZERO;
		if(bMax)
		{
			psMovcInst->asArg[1] = *psArgA;
			psMovcInst->asArg[2] = *psArgB;
		}
		else
		{
			psMovcInst->asArg[1] = *psArgB;
			psMovcInst->asArg[2] = *psArgA;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psMovcInst->u.psMovc->eTest = bMax ? TEST_TYPE_LT_ZERO : TEST_TYPE_GTE_ZERO;
		psMovcInst->asArg[1] = *psArgA;
		psMovcInst->asArg[2] = *psArgB;
	}
	
	/* has already been predicated and had destination set. */
	AppendInst(psState, psCodeBlock, psMovcInst);

}

IMG_INTERNAL
IMG_VOID ExpandFTRCInstruction(PINTERMEDIATE_STATE		psState,
							   PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandFTRCInstruction

 PURPOSE	: Expand a FTRC instruction (which isn't supported directly by the
			  hardware on all cores) to a sequence of supported instructions.

 PARAMETERS	: psState				- Compiler state
			  psInst				- instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32				uNegFlrTempNum = GetNextRegister(psState);
	IMG_UINT32				uFlrTempNum = GetNextRegister(psState);
	PINST					psFrcInst;
	PINST					psFmovInst;
	PINST					psMovcInst;
	PARG					psTrcSrc = &psInst->asArg[0];
	PFLOAT_SOURCE_MODIFIER	psArgMod = &psInst->u.psFloat->asSrcMod[0];

	#if defined(SUPPORT_SGX545)
	/*
		Check for a core which supported this instruction directly.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PACK_MULTIPLE_ROUNDING_MODES) != 0)
	{
		/*
			The hardware doesn't support a floating point source modifier on the TRC instruction
			so apply any source modifiers in a separate instruction.
		*/
		ApplyFloatSourceModifier(psState, 
								 psInst->psBlock,
								 psInst,
								 psTrcSrc,
								 psTrcSrc,
								 psTrcSrc->eFmt,
								 &psInst->u.psFloat->asSrcMod[0]);

		/*
			Switch to the hardware instruction variant.
		*/
		SetOpcode(psState, psInst, ITRUNC_HW);

		return;
	}
	#endif /* defined(SUPPORT_SGX545) */

	/*
		NEGFLRTEMP = 0 - FLOOR(ABS(ARG))
	*/
	psFrcInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psFrcInst, IFSUBFLR);

	SetDest(psState, psFrcInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNegFlrTempNum, UF_REGFORMAT_F32);

	psFrcInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
	psFrcInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;

	CopySrc(psState, psFrcInst, 1 /* uCopyToArgIdx */, psInst, 0 /* uCopyFromArgIdx */);
	psFrcInst->u.psFloat->asSrcMod[0] = *psArgMod;
	psFrcInst->u.psFloat->asSrcMod[1].bNegate = IMG_FALSE;
	psFrcInst->u.psFloat->asSrcMod[1].bAbsolute = IMG_TRUE;

	InsertInstBefore(psState, psInst->psBlock, psFrcInst, psInst);

	/*
		FLRTEMP = -NEGFLRTEMP
	*/
	psFmovInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psFmovInst, IFMOV);
	SetDest(psState, psFmovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uFlrTempNum, UF_REGFORMAT_F32);
	SetSrc(psState, psFmovInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uNegFlrTempNum, UF_REGFORMAT_F32);
	psFmovInst->u.psFloat->asSrcMod[0].bNegate = IMG_TRUE;
	InsertInstBefore(psState, psInst->psBlock, psFmovInst, psInst);

	/*
		TEMP1 = ARG >= 0 ? TEMP2 : TEMP1
	*/
	psMovcInst = AllocateInst(psState, psInst);

	SetOpcode(psState, psMovcInst, IMOVC);
	psMovcInst->u.psMovc->eTest = TEST_TYPE_GTE_ZERO;

	MoveDest(psState, psMovcInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);

	CopyPredicate(psState, psMovcInst, psInst);

	/*
		Insert an extra move if the conversion source isn't compatible
		with MOVC source 0.
	*/
	if (psArgMod->bNegate || psArgMod->bAbsolute || psInst->asArg[0].eFmt != UF_REGFORMAT_F32)
	{
		PINST		psMovInst;
		IMG_UINT32	uSrcModTemp = GetNextRegister(psState);

		psMovInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMovInst, IFMOV);
		SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSrcModTemp, UF_REGFORMAT_F32);
		MoveSrc(psState, psMovInst, 0 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
		psMovInst->u.psFloat->asSrcMod[0] = *psArgMod;
		InsertInstBefore(psState, psInst->psBlock, psMovInst, psInst);

		SetSrc(psState, psMovcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrcModTemp, UF_REGFORMAT_F32);
	}
	else
	{
		ASSERT(psTrcSrc->eFmt == UF_REGFORMAT_F32);
		MoveSrc(psState, psMovcInst, 0 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
	}
	SetSrc(psState, psMovcInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFlrTempNum, UF_REGFORMAT_F32);
	SetSrc(psState, psMovcInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uNegFlrTempNum, UF_REGFORMAT_F32);
	InsertInstBefore(psState, psInst->psBlock, psMovcInst, psInst);

	/*
		Free the expanded instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

static
IMG_VOID ConvertF32ConversionInstructionInt16(PINTERMEDIATE_STATE		psState,
											  PCODEBLOCK				psCodeBlock,
											  PARG						psDest,
											  IMG_UINT32				uPredSrc,
											  IMG_BOOL					bPredNegate,
											  PARG						psArg,
											  IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: ConvertF32ConversionInstructionInt16

 PURPOSE	: Converts one channel of an F32 to I16/U16 conversion instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArg					- Argument to the instruction.
			  bSigned				- If set the comparison should be signed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psPckInst;

	/*
		Convert from floating point to integer.
	*/
	psPckInst = AllocateInst(psState, NULL);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		IMG_UINT32	uChan;

		SetOpcode(psState, psPckInst, bSigned ? IVPCKS16FLT : IVPCKU16FLT);

		SetSrcUnused(psState, psPckInst, 0 /* uSrcIdx */);
		SetSrcFromArg(psState, psPckInst, 1 /* uSrcIdx */, psArg);
		for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
		{
			SetSrcUnused(psState, psPckInst, 1 + uChan);
		}
		psPckInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
		psPckInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psPckInst, bSigned ? IPCKS16F32 : IPCKU16F32);

		psPckInst->asArg[0] = *psArg;
		psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psPckInst->asArg[1].uNumber = 0;
	}
	psPckInst->asDest[0] = *psDest;
	SetPredicate(psState, psPckInst, uPredSrc, bPredNegate);
	AppendInst(psState, psCodeBlock, psPckInst);
}

static
IMG_VOID ConvertF32ConversionInstructionInt8(PINTERMEDIATE_STATE		psState,
											  PCODEBLOCK				psCodeBlock,
											  PARG						psDest,
											  IMG_UINT32				uPredSrc,
											  IMG_BOOL					bPredNegate,
											  PARG						psArg,
											  IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: ConvertF32ConversionInstructionInt8

 PURPOSE	: Converts one channel of an F32 to I16/U16 conversion instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArg					- Argument to the instruction.
			  bSigned				- If set the comparison should be signed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psPckInst;

	/*
		Convert from floating point to integer.
	*/
	psPckInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psPckInst, bSigned ? IPCKS8F32 : IPCKU8F32);
	psPckInst->asDest[0] = *psDest;
	SetPredicate(psState, psPckInst, uPredSrc, bPredNegate);
	psPckInst->asArg[0] = *psArg;
	psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psPckInst->asArg[1].uNumber = 0;
	AppendInst(psState, psCodeBlock, psPckInst);
}

IMG_INTERNAL
IMG_VOID ExpandCVTINT2ADDRInstruction(PINTERMEDIATE_STATE	psState,
									  PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandCVTINT2ADDRInstruction

 PURPOSE	: Expands a CVTINT2ADDRinstruction (which isn't supported by the
			  hardware directly) to a sequence of instructions which are
			  supported.

 PARAMETERS	: psState				- Compiler state
			  psInst				- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psInst->eOpcode == ICVTINT2ADDR);

	SetOpcode(psState, psInst, IUNPCKU16U16);
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 0;
}

IMG_INTERNAL
IMG_VOID ExpandCVTFLT2INTInstruction(PINTERMEDIATE_STATE	psState,
									 PINST					psInst)
/*****************************************************************************
 FUNCTION	: ExpandCVTFLT2INTInstruction

 PURPOSE	: Expands a CVTFLT2INT instruction (which isn't supported by the
			  hardware directly) to a sequence of instructions which are
			  supported.

 PARAMETERS	: psState				- Compiler state
			  psInst				- Instruction to expand.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uClampMinTemp = GetNextRegister(psState);
	IMG_UINT32	uClampMaxPredTemp = GetNextRegister(psState);
	IMG_UINT32	uScaledTemp = GetNextRegister(psState);
	IMG_UINT32	uFracTemp = GetNextRegister(psState);
	IMG_UINT32	uFloorTemp = GetNextRegister(psState);
	IMG_UINT32	uLowWordTemp = GetNextRegister(psState);
	IMG_UINT32	uPreClampTemp = GetNextRegister(psState);
	IMG_BOOL	bSigned = psInst->u.psCvtFlt2Int->bSigned;

	ASSERT(psInst->eOpcode == ICVTFLT2INT);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		ExpandCVTFLT2INTInstruction_Vec(psState, psInst);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Clamp to the minimum representable integer.
	*/
	{
		PINST	psMaxInst;

		/*
			TEMP1 = MAX(TEMP1, 0) / 
			TEMP1 = MAX(TEMP1, -0x80000000)
		*/
		psMaxInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMaxInst, IFMAX);
		SetDest(psState, psMaxInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uClampMinTemp, UF_REGFORMAT_F32);
		MoveSrc(psState, psMaxInst, 0 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);

		if (!bSigned)
		{
			psMaxInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psMaxInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
		}
		else
		{
			IMG_FLOAT	fMinInt = -(IMG_FLOAT)0x80000000;

			psMaxInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psMaxInst->asArg[1].uNumber = *(IMG_PUINT32)&fMinInt;
		}
		InsertInstBefore(psState, psInst->psBlock, psMaxInst, psInst);
	}

	/*
		TEMP3 = TEMP1 - 0xFFFFFFFF/0x7FFFFFFF

		We will use TEMP3 as a predicate to clamp to the maximum integer - we can't just clamp
		here because 0xFFFFFFFF isn't exactly representable in F32.
	*/
	{
		PINST		psAddInst;
		IMG_FLOAT	fMaxInt;
		
		if (!bSigned)
		{
			fMaxInt = (IMG_FLOAT)0xFFFFFFFF;
		}
		else
		{
			fMaxInt = (IMG_FLOAT)0x7FFFFFFF;
		}

		psAddInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psAddInst, IFADD);
		SetDest(psState, psAddInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uClampMaxPredTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psAddInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uClampMinTemp, UF_REGFORMAT_F32);
		psAddInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psAddInst->asArg[1].uNumber = *(IMG_PUINT32)&fMaxInt;
		psAddInst->u.psFloat->asSrcMod[1].bNegate = IMG_TRUE;
		InsertInstBefore(psState, psInst->psBlock, psAddInst, psInst);
	}
	
	/*
		TEMP1 = TEMP1 * (1/65536)
	*/
	{
		PINST	psMulInst;

		psMulInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMulInst, IFMUL);
		SetDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uScaledTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psMulInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uClampMinTemp, UF_REGFORMAT_F32);
		psMulInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		psMulInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER65536;
		InsertInstBefore(psState, psInst->psBlock, psMulInst, psInst);
	}
	
	/*
		TEMP2 = FRC(TEMP1)
	*/
	{
		PINST	psFrcInst;
	
		psFrcInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psFrcInst, IFSUBFLR);
		SetDest(psState, psFrcInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uFracTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psFrcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uScaledTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psFrcInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uScaledTemp, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psFrcInst, psInst);
	}
	
	/*
		TEMP1 = TEMP1 - TEMP2
	*/
	{
		PINST	psAddInst;

		psAddInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psAddInst, IFADD);
		SetDest(psState, psAddInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uFloorTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psAddInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uScaledTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psAddInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFracTemp, UF_REGFORMAT_F32);
		psAddInst->u.psFloat->asSrcMod[1].bNegate = IMG_TRUE;
		InsertInstBefore(psState, psInst->psBlock, psAddInst, psInst);
	}
	
	/*
		TEMP2 = TEMP2 * 65536
	*/
	{
		PINST		psMulInst;
		IMG_FLOAT	f65536;

		f65536 = (IMG_FLOAT)65536;

		psMulInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMulInst, IFMUL);
		SetDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uLowWordTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psMulInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFracTemp, UF_REGFORMAT_F32);
		psMulInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psMulInst->asArg[1].uNumber = *(IMG_PUINT32)&f65536;
		InsertInstBefore(psState, psInst->psBlock, psMulInst, psInst);
	}
	
	if (!bSigned)
	{
		PINST	psPackInst;

		/*
			TEMP1 = TO_WORD(TEMP2) | (TO_WORD(TEMP1) << 16)
		*/
		psPackInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psPackInst, IPCKU16F32);
		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uLowWordTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psPackInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFloorTemp, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);
	}
	else
	{
		PINST		psPackInst;
		ARG			sPartialPreClampTemp;

		MakeNewTempArg(psState, UF_REGFORMAT_F32, &sPartialPreClampTemp);

		/*
			TEMP1 = TO_WORD(TEMP2) | (TO_SIGNED_WORD(TEMP1) << 16)

			TEMP2 is effectively already in 2-complement format since if
			ARG < 0 then TEMP2 = 65536 - (ABS(ARG) % 65536) so we
			can use an unsigned conversion.
		*/

		psPackInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psPackInst, IPCKS16F32);
		SetDestFromArg(psState, psPackInst, 0 /* uDestIdx */, &sPartialPreClampTemp);
		SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
		SetSrc(psState, psPackInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uFloorTemp, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);

		psPackInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psPackInst, IPCKU16F32);
		psPackInst->auDestMask[0] = USC_DESTMASK_LOW;
		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);
		SetPartiallyWrittenDest(psState, psPackInst, 0 /* uDestIdx */, &sPartialPreClampTemp);
		SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uLowWordTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psPackInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psPackInst, psInst);
	}
	
	/*
		RESULT = (TEMP3 >= 0) ? 0xFFFFFFFF : TEMP1
	*/
	{
		PINST	psMovcInst;

		psMovcInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMovcInst, IMOVC);

		CopyPartiallyWrittenDest(psState, psMovcInst, 0 /* uCopyToDestIdx */, psInst, 0 /* uCopyFromDestIdx */);
		MoveDest(psState, psMovcInst, 0 /* uCopyToDestIdx */, psInst, 0 /* uCopyFromDestIdx */);
		CopyPredicate(psState, psMovcInst, psInst);

		psMovcInst->u.psMovc->eTest = TEST_TYPE_GTE_ZERO;
		SetSrc(psState, psMovcInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uClampMaxPredTemp, UF_REGFORMAT_F32);
		if (!bSigned)
		{
			psMovcInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psMovcInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_INT32ONE;
		}
		else
		{
			psMovcInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psMovcInst->asArg[1].uNumber = 0x7FFFFFFF;
		}
		SetSrc(psState, psMovcInst, 2 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPreClampTemp, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInst->psBlock, psMovcInst, psInst);
	}
	
	/*
		Free the expanded instruction.
	*/
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);
}

static
IMG_VOID ConvertF32ConversionInstructionInt32(PINTERMEDIATE_STATE		psState, 
											  PCODEBLOCK				psCodeBlock,
											  PARG						psDest,
											  IMG_UINT32				uPredSrc,
											  IMG_BOOL					bPredNegate,
											  PARG						psArg,
											  IMG_BOOL					bSigned)
/*****************************************************************************
 FUNCTION	: ConvertF32ConversionInstructionInt32

 PURPOSE	: Converts one channel of an F32 to I32/U32 conversion instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArg					- Argument to the instruction.
			  bSigned				- If set the comparison should be signed.
			  bRoundToNearestEven	- If set the float should be rounded using
			  						round to nearest even instead of round to zero

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psCvtInst;

	/*
		Convert the rounded value from float to integer.
	*/
	psCvtInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psCvtInst, ICVTFLT2INT);
	psCvtInst->u.psCvtFlt2Int->bSigned = bSigned;
	psCvtInst->asDest[0] = *psDest;
	SetPredicate(psState, psCvtInst, uPredSrc, bPredNegate);
	psCvtInst->asArg[0] = *psArg;
	AppendInst(psState, psCodeBlock, psCvtInst);
}

static
IMG_VOID GenerateOrTest(PINTERMEDIATE_STATE	psState,
						PCODEBLOCK			psCodeBlock,
						IMG_UINT32			uPredDest,
						IMG_UINT32			uPredSrc,
						IMG_BOOL			bPredNegate,
						PARG				psData,
						IMG_BOOL			bNonZero)
/*****************************************************************************
 FUNCTION	: GenerateOrTest

 PURPOSE	: Generates a bitwise test instruction.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Codeblock into which to insert instructions
			  uPredDest				- Predicate to write with the result.
			  uPredSrc				- Predicate to guard update of the destination.
			  psData				- Data to test.
			  bNonZero				- If TRUE test for non-zero.
									  If FALSE test for zero.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psOrInst;

	psOrInst = AllocateInst(psState, NULL);
	SetOpcodeAndDestCount(psState, psOrInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	psOrInst->u.psTest->eAluOpcode = IOR;
	MakePredicateArgument(&psOrInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);
	psOrInst->asArg[0] = *psData;
	psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psOrInst->asArg[1].uNumber = 0;
	if (bNonZero)
	{
		psOrInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
	}
	else
	{
		psOrInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
	}

	SetPredicate(psState, psOrInst, uPredSrc, bPredNegate);
	
	AppendInst(psState, psCodeBlock, psOrInst);
}

static
IMG_VOID GenerateIntMovc(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 IMG_UINT32				uDestRegNum,
						 PARG					psSrc0,
						 IMG_UINT32				uSrc1RegType,
						 IMG_UINT32				uSrc1RegNum,
						 IMG_UINT32				uSrc2RegType,
						 IMG_UINT32				uSrc2RegNum)
/*****************************************************************************
 FUNCTION	: GenerateIntMovc

 PURPOSE	: Generates an integer conditional move instruction.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Codeblock into which to insert instructions
			  uDestRegNum			- Number of the temporary register to write.
			  psSrc0				- Source for the test.
			  uSrc1RegType			- Value to move if the test passes.
			  uSrc1RegNum
			  uSrc2RegType			- Value to move if the test fails.
			  uSrc2RegNum

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMovcInst;

	psMovcInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMovcInst, IMOVC_I32);
	psMovcInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMovcInst->asDest[0].uNumber = uDestRegNum;
	psMovcInst->asArg[0] = *psSrc0;
	psMovcInst->u.psMovc->eTest = TEST_TYPE_NEQ_ZERO;
	psMovcInst->asArg[1].uType = uSrc1RegType;
	psMovcInst->asArg[1].uNumber = uSrc1RegNum;
	psMovcInst->asArg[2].uType = uSrc2RegType;
	psMovcInst->asArg[2].uNumber = uSrc2RegNum;

	AppendInst(psState, psCodeBlock, psMovcInst);
}

IMG_INTERNAL
IMG_VOID CreateComparisonInt16(PINTERMEDIATE_STATE	psState,
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
 FUNCTION	: CreateComparisonInt16

 PURPOSE	: Generates a comparison between two U16 or I16 values.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uCompOp			- Comparison type.
			  psSrc1			- First source to be compared.
			  psSrc2			- Second source to be compared.
			  uChan				- Channel within the sources to be compared.
			  uCompPredSrc		- Source predicate for the comparison.
			  bCompPredNegate
			  bInvert			- If TRUE invert the logic of the test.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psTestInst;
	IMG_BOOL	bSigned;
	ARG			sHwSrc1, sHwSrc2;

	ASSERT(eChanOp == UFREG_COMPCHANOP_NONE);

	/*
		Is this a signed or unsigned comparison.
	*/
	bSigned = IMG_FALSE;
	if (psSrc1->eFormat == UF_REGFORMAT_I32)
	{
		bSigned = IMG_TRUE;
	}

	if (bInvert)
	{
		uCompOp = g_puInvertCompOp[uCompOp];
	}

	/*
		Get the sources to compare.
	*/
	GetSourceTypeless(psState,
					  psCodeBlock,
					  psSrc1,
					  uChan,
					  &sHwSrc1,
					  IMG_FALSE /* bAllowSourceMod */,
					  NULL /* psSourceMod */);
	GetSourceTypeless(psState,
					  psCodeBlock,
					  psSrc2,
					  uChan,
					  &sHwSrc2,
					  IMG_FALSE /* bAllowSourceMod */,
					  NULL /* psSourceMod */);

	/*
		PDEST = SRC1 compare SRC2
	*/
	psTestInst = AllocateInst(psState, NULL);

	SetOpcodeAndDestCount(psState, psTestInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);

	psTestInst->u.psTest->eAluOpcode = bSigned ? IISUB16 : IISUBU16;
	CompOpToTest(psState, uCompOp, &psTestInst->u.psTest->sTest);

	SetPredicate(psState, psTestInst, uCompPredSrc, bCompPredNegate);

	MakePredicateArgument(&psTestInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);

	psTestInst->asArg[0] = sHwSrc1;
	psTestInst->asArg[1] = sHwSrc2;

	AppendInst(psState, psCodeBlock, psTestInst);
}

IMG_INTERNAL
IMG_VOID CreateComparisonInt32(PINTERMEDIATE_STATE	psState,
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
 FUNCTION	: CreateComparisonInt32

 PURPOSE	: Generates a comparison between two U32 or I32 values.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uCompOp			- Comparison type.
			  psSrc1			- First source to be compared.
			  psSrc2			- Second source to be compared.
			  uChan				- Channel within the sources to be compared.
			  eCompOp			- 
			  uCompPredSrc		- Source predicate for the comparison.
			  bCompPredNegate
			  bInvert			- If TRUE invert the logic of the test.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG	sHwSrc1, sHwSrc2;
	FLOAT_SOURCE_MODIFIER sMod1, sMod2;
	IMG_BOOL bSigned;

	ASSERT(eChanOp == UFREG_COMPCHANOP_NONE);

	/*
		Is this a signed or unsigned comparison.
	*/
	bSigned = IMG_FALSE;
	if (psSrc1->eFormat == UF_REGFORMAT_I32)
	{
		bSigned = IMG_TRUE;
	}

	if (bInvert)
	{
		uCompOp = g_puInvertCompOp[uCompOp];
	}

	/*
		Get the sources to compare.
	*/
	GetSourceTypeless(psState,
					  psCodeBlock,
					  psSrc1,
					  uChan,
					  &sHwSrc1,
					  IMG_TRUE /* bAllowSourceMod */,
					  &sMod1);
	GetSourceTypeless(psState,
					  psCodeBlock,
					  psSrc2,
					  uChan,
					  &sHwSrc2,
					  IMG_TRUE /* bAllowSourceMod */,
					  &sMod2);

	/*
		Apply absolute source modifiers in a seperate instruction.
	*/
	if (sMod1.bAbsolute)
	{
		IntegerAbsolute(psState, psCodeBlock, &sHwSrc1, &sHwSrc1);
	}
	if (sMod2.bAbsolute)
	{
		IntegerAbsolute(psState, psCodeBlock, &sHwSrc2, &sHwSrc2);
	}

	if (uCompOp == UFREG_COMPOP_EQ ||
		uCompOp == UFREG_COMPOP_NE)
	{
		PINST	psXorInst;
		/*
			Apply negation modifiers in a seperate instruction.
		*/
		if (sMod1.bNegate)
		{
			IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, &sHwSrc1, &sHwSrc1);
		}
		if (sMod2.bNegate)
		{
			IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, &sHwSrc2, &sHwSrc2);
		}

		/*
			XOR.TEST	pN, SRC1, SRC2
		*/
		psXorInst = AllocateInst(psState, NULL);
		SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psXorInst->u.psTest->eAluOpcode = IXOR;
		MakePredicateArgument(&psXorInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);
		psXorInst->asArg[0] = sHwSrc1;
		psXorInst->asArg[1] = sHwSrc2;

		SetPredicate(psState, psXorInst, uCompPredSrc, bCompPredNegate);

		if (uCompOp == UFREG_COMPOP_EQ)
		{
			psXorInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
		}
		else
		{
			psXorInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
		}

		AppendInst(psState, psCodeBlock, psXorInst);
	}
	else if (uCompOp == UFREG_COMPOP_GE || uCompOp == UFREG_COMPOP_LT)
	{
		ARG			sComparisonResult;
		IMG_BOOL	bNonZero;

		/*
			Generate: TEMP = sign extended carry bit of SRC1 - SRC2
		*/
		GenerateSignResult(psState,
						   psCodeBlock,
						   &sHwSrc2,
						   sMod1.bNegate,
						   &sHwSrc1,
						   sMod2.bNegate,
						   bSigned,
						   &sComparisonResult);

		if (uCompOp == UFREG_COMPOP_GE)
		{
			/*
				Check: SIGN_BIT(SRC1 - SRC2) == 0
			*/
			bNonZero = IMG_FALSE;
		}
		else
		{
			/*
				Check: SIGN_BIT(SRC1 - SRC2) != 0	
			*/
			bNonZero = IMG_TRUE;
		}

		/*
			pDEST = SIGN_BIT(SRC1 - SRC2) ?= 0
		*/
		GenerateOrTest(psState, psCodeBlock, uPredDest, uCompPredSrc, bCompPredNegate, &sComparisonResult, bNonZero);
	}
	else
	{
		ARG			sComparisonResultLow;
		ARG			sComparisonResultHigh;
		IMG_UINT32	uTemp = GetNextRegister(psState);
		ARG			sTemp;
		IMG_BOOL	bNonZero;

		ASSERT(uCompOp == UFREG_COMPOP_GT || uCompOp == UFREG_COMPOP_LE);

		/*
			Get new registers to hold the result of SRC1 - SRC2
		*/
		InitInstArg(&sComparisonResultLow);
		sComparisonResultLow.uType = USEASM_REGTYPE_TEMP;
		sComparisonResultLow.uNumber = GetNextRegister(psState);

		InitInstArg(&sComparisonResultHigh);
		sComparisonResultHigh.uType = USEASM_REGTYPE_TEMP;
		sComparisonResultHigh.uNumber = GetNextRegister(psState);

		/*
			Generate: SRC1 - SRC2
		*/
		GenerateIntegerArithmetic(psState,
								  psCodeBlock,
								  NULL /* psInsertInstBefore */, 
								  UFOP_ADD,
								  &sComparisonResultLow,
								  &sComparisonResultHigh,
								  USC_PREDREG_NONE,
								  IMG_FALSE,
								  &sHwSrc1,
								  sMod1.bNegate,
								  &sHwSrc2,
								  !sMod2.bNegate,
								  NULL /* psArgC */,
								  IMG_FALSE /* bNegateC */,
								  bSigned);

		/*
			Set TEMP = (RESULT_LOW != 0) ? 0xFFFFFFFF : 0
		*/
		GenerateIntMovc(psState, 
						psCodeBlock, 
						uTemp, 
						&sComparisonResultLow, 
						USEASM_REGTYPE_FPCONSTANT, 
						EURASIA_USE_SPECIAL_CONSTANT_INT32ONE, 
						USEASM_REGTYPE_FPCONSTANT, 
						EURASIA_USE_SPECIAL_CONSTANT_ZERO);

		/*
			Set TEMP = (RESULT_HIGH != 0) ? 0 : TEMP
		*/
		GenerateIntMovc(psState, 
						psCodeBlock, 
						uTemp, 
						&sComparisonResultHigh, 
						USEASM_REGTYPE_FPCONSTANT, 
						EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
						USEASM_REGTYPE_TEMP, 
						uTemp);

		/*
			Set pDEST = TEMP != 0
		*/
		InitInstArg(&sTemp);
		sTemp.uType = USEASM_REGTYPE_TEMP;
		sTemp.uNumber = uTemp;
		if (uCompOp == UFREG_COMPOP_GT)
		{
			bNonZero = IMG_TRUE;
		}
		else
		{
			bNonZero = IMG_FALSE;
		}
		GenerateOrTest(psState, psCodeBlock, uPredDest, uCompPredSrc, bCompPredNegate, &sTemp, bNonZero);
	}
}

static
IMG_VOID ConvertComparisonInstructionInt8(PINTERMEDIATE_STATE	psState, 
										   PCODEBLOCK			psCodeBlock,
									       PARG					psDest,
										   IMG_UINT32			uPredSrc,
										   IMG_BOOL				bPredNegate,
										   PARG					psArgA,
										   PARG					psArgB,
										   IMG_BOOL				bSigned,
										   UF_OPCODE			eOpCode)
/*****************************************************************************
 FUNCTION	: ConvertComparisonInstructionInt8

 PURPOSE	: Converts one channel of an I8 or U8 SETBEQ/SETBNE/SETBLT/SETBGE instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArgA				- Arguments to the instruction.
			  psArgB
			  bSigned				- If set the comparison should be signed.
			  eOpCode				- Comparison type.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UFREG_COMPOP	eComparisonType;

	switch (eOpCode)
	{
		case UFOP_SETBEQ: eComparisonType = UFREG_COMPOP_EQ; break;
		case UFOP_SETBNE: eComparisonType = UFREG_COMPOP_NE; break;
		case UFOP_SETBGE: eComparisonType = UFREG_COMPOP_GE; break;
		case UFOP_SETBLT: eComparisonType = UFREG_COMPOP_LT; break;
		default: imgabort();
	}

	/*
		Generate 

			DEST = A compare B ? 0xFFFF : 0
	*/
	GenerateInt8Comparison(psState,
							psCodeBlock,
							eComparisonType,
							psArgA,
							psArgB,
							bSigned,
							psDest,
							uPredSrc,
							bPredNegate);
}

static
IMG_VOID ConvertComparisonInstructionInt16(PINTERMEDIATE_STATE	psState, 
										   PCODEBLOCK			psCodeBlock,
									       PARG					psDest,
										   IMG_UINT32			uPredSrc,
										   IMG_BOOL				bPredNegate,
										   PARG					psArgA,
										   PARG					psArgB,
										   IMG_BOOL				bSigned,
										   UF_OPCODE			eOpCode)
/*****************************************************************************
 FUNCTION	: ConvertComparisonInstructionInt16

 PURPOSE	: Converts one channel of an I16 or U16 SETBEQ/SETBNE/SETBLT/SETBGE instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Basic block into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArgA				- Arguments to the instruction.
			  psArgB
			  bSigned				- If set the comparison should be signed.
			  eOpCode				- Comparison type.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UFREG_COMPOP	eComparisonType;

	switch (eOpCode)
	{
		case UFOP_SETBEQ: eComparisonType = UFREG_COMPOP_EQ; break;
		case UFOP_SETBNE: eComparisonType = UFREG_COMPOP_NE; break;
		case UFOP_SETBGE: eComparisonType = UFREG_COMPOP_GE; break;
		case UFOP_SETBLT: eComparisonType = UFREG_COMPOP_LT; break;
		default: imgabort();
	}

	/*
		Generate 

			DEST = A compare B ? 0xFFFF : 0
	*/
	GenerateInt16Comparison(psState,
							psCodeBlock,
							eComparisonType,
							psArgA,
							psArgB,
							bSigned,
							psDest,
							uPredSrc,
							bPredNegate);
}

static
IMG_VOID ConvertComparisonInstructionInt32(PINTERMEDIATE_STATE	psState, 
										   PCODEBLOCK			psCodeBlock,
									       PARG					psDest,
										   IMG_UINT32			uPredSrc,
										   IMG_BOOL				bPredNegate,
										   PARG					psArgA,
										   PARG					psArgB,
										   IMG_BOOL				bSigned,
										   UF_OPCODE			eOpCode)
/*****************************************************************************
 FUNCTION	: ConvertComparisonInstructionInt32

 PURPOSE	: Converts one channel of an I32 or U32 SETBEQ/SETBNE/SETBLT/SETBGE instruction
			  to intermediate code.

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- codeblock into which to insert instructions
			  psDest				- Destination register to write the result of
									the instruction to.
			  uPredSrc				- Predicate to control update of the desination.
			  bPredNegate
			  psArgA				- Arguments to the instruction.
			  psArgB
			  bSigned				- If set the comparison should be signed.
			  eOpCode				- Comparison type.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (eOpCode == UFOP_SETBEQ || eOpCode == UFOP_SETBNE)
	{
		PINST	psTestMaskInst;

		/*
			Create a TESTMASK instruction calculating 

				DEST = (A == B) ? 0xFFFFFFFF : 0x00000000
			or	DEST = (A != B) ? 0xFFFFFFFF : 0x00000000
		*/

		psTestMaskInst = AllocateInst(psState, NULL);

		SetOpcode(psState, psTestMaskInst, ITESTMASK);

		psTestMaskInst->asDest[0] = *psDest;

		SetPredicate(psState, psTestMaskInst, uPredSrc, bPredNegate);

		psTestMaskInst->asArg[0] = *psArgA;
		psTestMaskInst->asArg[1] = *psArgB;

		psTestMaskInst->u.psTest->eAluOpcode = IXOR;
		if (eOpCode == UFOP_SETBEQ)
		{
			psTestMaskInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
		}
		else
		{
			psTestMaskInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			psTestMaskInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			psTestMaskInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
		}

		AppendInst(psState, psCodeBlock, psTestMaskInst);
	}
	else
	{
		PINST		psMovcInst;
		ARG			sComparisonResult;
		TEST_TYPE	eMovcTest;

		/*
			Generate: TEMP = sign extended carry bit of A - B
		*/
		GenerateSignResult(psState,
						   psCodeBlock,
						   psArgB,
						   IMG_FALSE /* bNegateB */,
						   psArgA,
						   IMG_FALSE /* bNegateA */,
						   bSigned,
						   &sComparisonResult);

		if (eOpCode == UFOP_SETBGE)
		{
			eMovcTest = TEST_TYPE_EQ_ZERO;
		}
		else
		{
			eMovcTest = TEST_TYPE_NEQ_ZERO;
		}

		/* 2. "main" instruction conditionally moves either 0xffffffff or 0 */
		psMovcInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovcInst, IMOVC_I32);
		psMovcInst->asDest[0] = *psDest;

		SetPredicate(psState, psMovcInst, uPredSrc, bPredNegate);

		psMovcInst->asArg[0] = sComparisonResult;
		psMovcInst->u.psMovc->eTest = eMovcTest;
		psMovcInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psMovcInst->asArg[1].uNumber = 0xFFFFFFFF;
		psMovcInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psMovcInst->asArg[2].uNumber = 0;
	
		/* has already been predicated and had destination set. */
		AppendInst(psState, psCodeBlock, psMovcInst);
	}
}

static
IMG_VOID MoveTempToDestination(PINTERMEDIATE_STATE		psState,
							   PCODEBLOCK				psBlock,
							   PUF_REGISTER				psDest,
							   IMG_UINT32				uPredicate,
							   IMG_UINT32				uChan,
							   IMG_UINT32				uRepChan,
							   IMG_UINT32				uTempDestNum)
/*****************************************************************************
 FUNCTION	: MoveTempToDestination

 PURPOSE	: .

 PARAMETERS	: psState				- Compiler state
			  psCodeBlock			- Codeblock into which to insert instructions
			  psDest				- Input format destination to move to.
			  uPredicate			- Input format predicate for the move.
			  uChan					- Channel to move.
			  uRepChan				- The previous channel of which this channel
									is a duplicate or -1 if no duplicate exits.
			  uTempDestNum			- Number of temporary register that the
									main instruction wrote its result to for
									this channel.

 RETURNS	: None
*****************************************************************************/
{
	PINST	psMoveInst;
	
	psMoveInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMoveInst, IMOV);
	GetDestinationTypeless(psState, psBlock, psDest, uChan, &psMoveInst->asDest[0]);
	if (uRepChan != UINT_MAX)
	{
		/*
			Copying FROM the destination of an earlier instruction
			(GetDestination used to fill out the SOURCE of this move...)
		*/
		GetDestinationTypeless(psState, psBlock, psDest, uRepChan, &psMoveInst->asArg[0]);
	}
	else
	{
		psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asArg[0].uNumber = uTempDestNum;
		psMoveInst->asArg[0].eFmt = UF_REGFORMAT_F32;
	}

	GetInputPredicateInst(psState, psMoveInst, uPredicate, uChan);
	AppendInst(psState, psBlock, psMoveInst);
}

IMG_INTERNAL
IMG_VOID IntegerAbsolute(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psBlock,
						 PARG					psSrc,
						 PARG					psResult)
/*****************************************************************************
 FUNCTION	: IntegerAbsolute

 PURPOSE	: Generates instruction to get the absolute value of an integer
			  source.

 PARAMETERS	: psState				- Compiler state
			  psBlock				- Block onto which to append instructions
			  psSrc					- Register containing the source data.
			  psResult				- On output containing the register with the
									absolute value of the source data.

 *****************************************************************************/
{
	ARG		sNegatedSrc;
	PINST	psMovcInst;

	/*
		Generate:
			sNegatedSrc = -psSrc
	*/
	IntegerNegate(psState, psBlock, NULL /* psInsertInstBefore */, psSrc, &sNegatedSrc);

	/*
		MOVC	RESULT, psSrc, sNegatedSrc, psSrc

		(RESULT = (psSrc < 0) ? sNegatedSrc : psSrc)
	*/
	psMovcInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMovcInst, IMOVC_I32);
	psMovcInst->u.psMovc->eTest = TEST_TYPE_LT_ZERO;
	psMovcInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMovcInst->asDest[0].uNumber = GetNextRegister(psState);
	psMovcInst->asArg[0] = *psSrc;
	psMovcInst->asArg[1] = sNegatedSrc;
	psMovcInst->asArg[2] = *psSrc;
	AppendInst(psState, psBlock, psMovcInst);

	/*
		Return the register containing the absolute value of psSrc.
	*/
	*psResult = psMovcInst->asDest[0];
}

static
IMG_VOID IntegerAbsoluteFmt(PINTERMEDIATE_STATE psState,
						 PCODEBLOCK             psBlock,
						 PARG                   psSrc,
                         PARG                   psResult,
                         UF_REGFORMAT           eFmt)
/*****************************************************************************
 FUNCTION	: IntegerAbsoluteFmt

 PURPOSE	: Generates instruction to get the absolute value of an integer
			  source.

 PARAMETERS	: psState				- Compiler state
			  psBlock				- Block onto which to append instructions
			  psSrc					- Register containing the source data.
			  psResult				- On output containing the register with the
									absolute value of the source data.
			  eFmt                  - Integer format

 *****************************************************************************/
{
	ARG		sNegatedSrc;
	PINST	psMovcInst;
	IOPCODE eOpCode;

	/*
		Generate:
			sNegatedSrc = -psSrc
	*/
	IntegerNegate(psState, psBlock, NULL /* psInsertInstBefore */, psSrc, &sNegatedSrc);

	/*
		MOVC	RESULT, psSrc, sNegatedSrc, psSrc

		(RESULT = (psSrc < 0) ? sNegatedSrc : psSrc)
	*/
	psMovcInst = AllocateInst(psState, NULL);

	switch(eFmt)
	{
	    case UF_REGFORMAT_I32:
		    eOpCode = IMOVC_I32;
		    break;
		    
	    case UF_REGFORMAT_I16:
		    eOpCode = IMOVC_I16;
		    break;
		    
	    case UF_REGFORMAT_I8_UN:
		    eOpCode = IMOVC_U8;
		    break;
		    
	    default:
		    eOpCode = IMOVC_I32;
	}
	
	SetOpcode(psState, psMovcInst, eOpCode);
	psMovcInst->u.psMovc->eTest = TEST_TYPE_LT_ZERO;
	psMovcInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMovcInst->asDest[0].uNumber = GetNextRegister(psState);
	psMovcInst->asArg[0] = *psSrc;
	psMovcInst->asArg[1] = sNegatedSrc;
	psMovcInst->asArg[2] = *psSrc;
	AppendInst(psState, psBlock, psMovcInst);

	/*
		Return the register containing the absolute value of psSrc.
	*/
	*psResult = psMovcInst->asDest[0];
}

IMG_INTERNAL
IMG_VOID ZeroExtendInteger(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psCodeBlock,
						   IMG_UINT32			uSrcBitWidth,
						   PARG					psSrc,
						   IMG_UINT32			uByteOffset,
						   PARG					psDest)
/*****************************************************************************
 FUNCTION	: ZeroExtendInteger
    
 PURPOSE	: Generates code to zero extend an 8 or 16-bit integer to 32 bits.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  eFormat			- Format of the channel.
			  psSrc				- Register containing the source data.
			  uByteOffset		- Offset of the source data within the register.
			  psDest			- Returns the register containing the zero
								extended result.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psZeroMoveInst;
	PINST		psPckInst;
	IMG_UINT32	uTempDest = GetNextRegister(psState);

	/*
		Set the whole destination register to 0.
	*/
	psZeroMoveInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psZeroMoveInst, IMOV);
	psZeroMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psZeroMoveInst->asDest[0].uNumber = uTempDest;
	psZeroMoveInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psZeroMoveInst->asArg[0].uNumber = 0;
	AppendInst(psState, psCodeBlock, psZeroMoveInst);

	/*
		Move the result of the texture sample into the low
		8-bits or low 16-bits.
	*/
	psPckInst = AllocateInst(psState, NULL);
	if (uSrcBitWidth == 8)
	{
		SetOpcode(psState, psPckInst, IPCKU8U8);
		psPckInst->auDestMask[0] = USC_RED_CHAN_MASK;
	}
	else
	{
		ASSERT(uSrcBitWidth == 16);

		SetOpcode(psState, psPckInst, IUNPCKU16U16);
		psPckInst->auDestMask[0] = USC_DESTMASK_LOW;
	}
	psPckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asDest[0].uNumber = uTempDest;
	psPckInst->asArg[0] = *psSrc;
	SetPCKComponent(psState, psPckInst, 0 /* uArgIdx */, uByteOffset);
	psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psPckInst->asArg[1].uNumber = 0;
	AppendInst(psState, psCodeBlock, psPckInst);

	/*
		Return the register containing the unpacked result.
	*/
	psDest->uType = USEASM_REGTYPE_TEMP;
	psDest->uNumber = uTempDest;
}

IMG_INTERNAL
IMG_VOID SignExtendInteger(PINTERMEDIATE_STATE	psState,
					       PCODEBLOCK			psCodeBlock,
						   IMG_UINT32			uSrcBitWidth,
						   PARG					psSrc,
						   IMG_UINT32			uByteOffset,
						   PARG					psDest)
/*****************************************************************************
 FUNCTION	: SignExtendInteger
    
 PURPOSE	: Generates code to sign extend an 8 or 16-bit integer to 32 bits.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uSrcBitWidth		- Bit width of the integer to sign extend.
			  psSrc				- Register containing the source data.
			  uByteOffset		- Offset of the source data within the register.
			  psDest			- Returns the register containing the sign
								extended result.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psPckInst, psShlInst, psAsrInst;
	IMG_UINT32	uTempDest = GetNextRegister(psState);

	/*
		Move the source data into the low 8-bits or low 16-bits.
	*/
	psPckInst = AllocateInst(psState, NULL);
	if (uSrcBitWidth == 8)
	{
		SetOpcode(psState, psPckInst, IPCKU8U8);
		psPckInst->auDestMask[0] = USC_RED_CHAN_MASK;
	}
	else
	{
		ASSERT(uSrcBitWidth == 16);

		SetOpcode(psState, psPckInst, IUNPCKU16U16);
		psPckInst->auDestMask[0] = USC_DESTMASK_LOW;
	}
	psPckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asDest[0].uNumber = uTempDest;
	psPckInst->asArg[0] = *psSrc;
	SetPCKComponent(psState, psPckInst, 0 /* uArgIdx */, uByteOffset);
	psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psPckInst->asArg[1].uNumber = 0;
	AppendInst(psState, psCodeBlock, psPckInst);

	/*
		Shift the data left so it's sign bit is in bit 31.
	*/
	psShlInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psShlInst, ISHL);
	psShlInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psShlInst->asDest[0].uNumber = uTempDest;
	psShlInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psShlInst->asArg[0].uNumber = uTempDest;
	psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psShlInst->asArg[1].uNumber = 32 - uSrcBitWidth;
	AppendInst(psState, psCodeBlock, psShlInst);

	/*
		Do an arithmetic right shift back to the original
		position to get sign extension.
	*/
	psAsrInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psAsrInst, IASR);
	psAsrInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psAsrInst->asDest[0].uNumber = uTempDest;
	psAsrInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psAsrInst->asArg[0].uNumber = uTempDest;
	psAsrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psAsrInst->asArg[1].uNumber = 32 - uSrcBitWidth;
	AppendInst(psState, psCodeBlock, psAsrInst);

	/*
		Return the register containing the unpacked result.
	*/
	psDest->uType = USEASM_REGTYPE_TEMP;
	psDest->uNumber = uTempDest;
}

static
IMG_VOID ConvertFloatToIntegerConversion(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psBlock,
										 UF_REGFORMAT			eDestFormat,
										 PUNIFLEX_INST			psInputInst,
										 PARG					psDest,
										 IMG_UINT32				uPredSrc,
										 IMG_BOOL				bPredNegate,
										 PARG					psSrc,
										 PFLOAT_SOURCE_MODIFIER	psSrcMod,
										 IMG_BOOL				bSigned)
/*****************************************************************************
 FUNCTION	: ConvertFloatToIntegerConversion
    
 PURPOSE	: Generates code to expand a single channel of an input instruction
			  converting an F32 source to integer.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Block to add the intermediate instructions to.
			  psInputInst			- Input instruction.
			  psDest				- Intermediate format destination for the instruction.
			  uPredSrc				- Predicate to control update of the destination.
			  bPredNegate
			  psSrc					- Intermediate format source for the instruction.
			  psSrcMod				- Modifier on the intermediate format source.
			  bSigned				- TRUE if the instruction's destination is a signed integer.
			  bRoundToNearestEven	- TRUE if the float should be rounded to the nearest even
			  					  	integer (as a float) before being converted to an integer.
			  
 RETURNS	: None.
*****************************************************************************/
{
	ASSERT(GetRegisterFormat(psState, &psInputInst->asSrc[0]) == UF_REGFORMAT_F32);

	/*
		The FTOI instruction is only generated internally so we can assume it never has
		source modifiers.
	*/
	ASSERT(!psSrcMod->bNegate);
	ASSERT(!psSrcMod->bAbsolute);

	if (eDestFormat == UF_REGFORMAT_I8_UN ||
		eDestFormat == UF_REGFORMAT_U8_UN)
	{
		/*
			Convert from F32 to I8/U8
		*/
		ConvertF32ConversionInstructionInt8(psState,
											psBlock,
											psDest,
											uPredSrc,
											bPredNegate,
											psSrc,
											bSigned);
	}
	else if (eDestFormat == UF_REGFORMAT_I16 ||
			 eDestFormat == UF_REGFORMAT_U16)
	{
		/*
			Convert from F32 to I16/U16
		*/
		ConvertF32ConversionInstructionInt16(psState,
											 psBlock,
											 psDest,
											 uPredSrc,
											 bPredNegate,
											 psSrc,
											 bSigned);
	}
	else
	{
		/*
			Convert from F32 to I32/U32
		*/
		ConvertF32ConversionInstructionInt32(psState,
											 psBlock,
											 psDest,
											 uPredSrc,
											 bPredNegate,
											 psSrc,
											 bSigned);
	}
}

static
PCODEBLOCK ConvertIntegerMOV(PINTERMEDIATE_STATE		psState,
							 PCODEBLOCK					psBlock,
							 UF_REGFORMAT				eDestFormat,
							 PUNIFLEX_INST				psInputInst,
							 PARG						psDest,
							 IMG_UINT32					uPredSrc,
							 IMG_BOOL					bPredNegate,
							 PARG						psSrc,
							 PFLOAT_SOURCE_MODIFIER		psSrcMod)
/*****************************************************************************
 FUNCTION	: ConvertIntegerMOV
    
 PURPOSE	: Generates code to expand a single channel of an input MOV instruction
			  with an integer format destination.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction.
			  psDest			- Intermediate format destination for the instruction.
			  uPredSrc			- Predicate to control update of the destination.
			  bPredNegate
			  psSrc				- Intermediate format source for the instruction.
			  psSrcMod			- Modifier on the intermediate format source.
 
 RETURNS	: A pointer to the last codeblock
*****************************************************************************/
{
	UF_REGFORMAT	eSrcFormat;
	IMG_UINT32		uSat;

	uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
				
	eSrcFormat = GetRegisterFormat(psState, &psInputInst->asSrc[0]);

	ASSERT(eSrcFormat == UF_REGFORMAT_I8_UN ||
		   eSrcFormat == UF_REGFORMAT_U8_UN ||
		   eSrcFormat == UF_REGFORMAT_I16 ||
		   eSrcFormat == UF_REGFORMAT_U16 ||
		   eSrcFormat == UF_REGFORMAT_I32 ||
		   eSrcFormat == UF_REGFORMAT_U32);

	/*
		Apply a negate modifier in a separate instruction.
	*/
	if (psSrcMod->bNegate)
	{
		IntegerNegate(psState, psBlock, NULL /* psInsertInstBefore */, psSrc, psSrc);
	}

	if (
			(
				eDestFormat == UF_REGFORMAT_U32 ||
				eDestFormat == UF_REGFORMAT_I32
			) &&
			(
				eSrcFormat == UF_REGFORMAT_I16 || 
				eSrcFormat == UF_REGFORMAT_U16
			)
			&&
			(
				/* Check we don't need to saturate */
				(psState->uCompilerFlags & UF_OPENCL) == 0 ||
				uSat == 0
			)
	   )
	{
		PINST		psMovInst;
		IMG_BOOL	bSignExtend;

		/*
			Convert from a 16-bit integer to a 32-bit integer.
		*/
		psMovInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psMovInst, IMOV);

		SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);

		psMovInst->asDest[0] = *psDest;

		bSignExtend = IMG_FALSE;
		/*
		   When using OpenCL we want do to a sign extension
		   when the source is signed, regardless of the
		   destination format.
		*/ 
		if ((psState->uCompilerFlags & UF_OPENCL) != 0)
		{
			if (eSrcFormat == UF_REGFORMAT_I16)
			{
				bSignExtend = IMG_TRUE;
			}
		}
		else
		{
			if (eDestFormat == UF_REGFORMAT_I32 && eSrcFormat == UF_REGFORMAT_I16)
			{
				bSignExtend = IMG_TRUE;
			}
		}

		if (bSignExtend)
		{
			/*
				Sign extend from 16-bits from 32-bits.
			*/
			SignExtendInteger(psState, 
							  psBlock, 
							  16 /* uSrcBitWidth */, 
							  psSrc, 
							  0 /* uByteOffset */, 
							  &psMovInst->asArg[0]);
		}
		else
		{
			/*
				Zero extend from 16-bits from 32-bits.
			*/
			ZeroExtendInteger(psState, 
							  psBlock, 
							  16 /* uSrcBitWidth */, 
							  psSrc, 
							  0 /* uByteOffset */, 
							  &psMovInst->asArg[0]);
		}

		AppendInst(psState, psBlock, psMovInst);

		return psBlock;
	}
	else if(psState->uCompilerFlags & UF_OPENCL)
	{
		/* Conversions from 8-bit formats */
		if
		(
			(eDestFormat == UF_REGFORMAT_U8_UN && eSrcFormat == UF_REGFORMAT_I8_UN)
			&&
			(
				/*
				   N.B. If any type of dest saturation is requested 
				   we saturate to the most positive/most negative value
				   which can fit in the dest format
				*/
				uSat != 0
			)
		)
		{
			/*
				Convert from a signed 8-bit integer to a (saturated) unsigned 8-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psPckInst, IPCKU8S8);
			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if 
			(
				(eDestFormat == UF_REGFORMAT_I8_UN && eSrcFormat == UF_REGFORMAT_U8_UN)
				&&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from an unsigned 8-bit integer to a saturated signed 8-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psPckInst, IPCKS8U8);
			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if (
				(
					eDestFormat == UF_REGFORMAT_U16 ||
					eDestFormat == UF_REGFORMAT_I16
				) &&
				(
					eSrcFormat == UF_REGFORMAT_I8_UN || 
					eSrcFormat == UF_REGFORMAT_U8_UN
				) &&
				(
					/* Check we don't need to saturate */
					uSat == 0
				)
		   )
		{
			PINST	psMovInst;

			/*
				Convert from a 8-bit integer to 16-bit integer.
			*/
			psMovInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMovInst, IMOV);
			SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
			psMovInst->asDest[0] = *psDest;
				
			if (eSrcFormat == UF_REGFORMAT_I8_UN)
			{
				/*
					Sign extend from 8-bits from 16-bits.
				*/
				SignExtendInteger(psState, 
								  psBlock, 
								  8 /* uSrcBitWidth */, 
								  psSrc, 
								  0 /* uByteOffset */, 
								  &psMovInst->asArg[0]);
			}
			else
			{
				/*
					Zero extend from 8-bits from 16-bits.
				*/
				ZeroExtendInteger(psState, 
								  psBlock, 
								  8 /* uSrcBitWidth */, 
								  psSrc, 
								  0 /* uByteOffset */, 
								  &psMovInst->asArg[0]);
			}

			AppendInst(psState, psBlock, psMovInst);

			return psBlock;
		}
		else if (
				(
					eDestFormat == UF_REGFORMAT_U16 ||
					eDestFormat == UF_REGFORMAT_I16
				) &&
				(
					eSrcFormat == UF_REGFORMAT_I8_UN || 
					eSrcFormat == UF_REGFORMAT_U8_UN
				) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from a 8-bit integer to a (saturated) 16-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			
			if (eSrcFormat == UF_REGFORMAT_U8_UN)
			{
				if(eDestFormat == UF_REGFORMAT_U16)
				{
					SetOpcode(psState, psPckInst, IUNPCKU16U8);
				}
				else
				{
					SetOpcode(psState, psPckInst, IUNPCKS16U8);
				}
			}
			else
			{
				if(eDestFormat == UF_REGFORMAT_U16)
				{
					SetOpcode(psState, psPckInst, IUNPCKU16S8);
				}
				else
				{
					SetOpcode(psState, psPckInst, IUNPCKS16S8);
				}
			}

			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if (
				(
					eDestFormat == UF_REGFORMAT_U32 ||
					eDestFormat == UF_REGFORMAT_I32
				) &&
				(
					eSrcFormat == UF_REGFORMAT_I8_UN || 
					eSrcFormat == UF_REGFORMAT_U8_UN
				) &&
				(
					/* Check we don't need to saturate */
					uSat == 0
				)
		   )
		{
			PINST	psMovInst;

			/*
				Convert from a 8-bit integer to a 32-bit integer.
			*/
			psMovInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMovInst, IMOV);
			SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
			psMovInst->asDest[0] = *psDest;
				
			if (eSrcFormat == UF_REGFORMAT_I8_UN)
			{
				/*
					Sign extend from 8-bits from 32-bits.
				*/
				SignExtendInteger(psState, 
								  psBlock, 
								  8 /* uSrcBitWidth */, 
								  psSrc, 
								  0 /* uByteOffset */, 
								  &psMovInst->asArg[0]);
			}
			else
			{
				/*
					Zero extend from 8-bits from 32-bits.
				*/
				ZeroExtendInteger(psState, 
								  psBlock, 
								  8 /* uSrcBitWidth */, 
								  psSrc, 
								  0 /* uByteOffset */, 
								  &psMovInst->asArg[0]);
			}

			AppendInst(psState, psBlock, psMovInst);

			return psBlock;
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_U32 && eSrcFormat == UF_REGFORMAT_I8_UN ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			/*
				Convert from a signed 8-bit integer to a (saturated) unsigned 32-bit integer.
			*/

			/* psDest = psSrc && 0x80 */
			{
				PINST	psInst;

				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IAND);
				psInst->asDest[0] = psDest[0];
				psInst->asArg[0] = psSrc[0];
				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0x80;
				AppendInst(psState, psBlock, psInst);
			}

			/*
				psDest = (psDest : psSrc ? 0x0)
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOVC_I32);
				psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
				psMovInst->asDest[0] = *psDest;
				psMovInst->asArg[0] = *psDest;
				psMovInst->asArg[1] = *psSrc;
				psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[2].uNumber = 0x0;
				AppendInst(psState, psBlock, psMovInst);
			}

			return psBlock;
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_I32 && eSrcFormat == UF_REGFORMAT_I8_UN ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMovInst, IMOV);
			SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
			psMovInst->asDest[0] = *psDest;
				
			/*
				Sign extend from 8-bits from 32-bits.
			*/
			SignExtendInteger(psState, 
							  psBlock, 
							  8 /* uSrcBitWidth */, 
							  psSrc, 
							  0 /* uByteOffset */, 
							  &psMovInst->asArg[0]);

			AppendInst(psState, psBlock, psMovInst);

			return psBlock;
		}

		/* Conversions from 16-bit formats */
		else if 
			(
				(
					eDestFormat == UF_REGFORMAT_I8_UN || 
					eDestFormat == UF_REGFORMAT_U8_UN
				) &&
				(
					eSrcFormat == UF_REGFORMAT_U16 ||
					eSrcFormat == UF_REGFORMAT_I16
				) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from a 16-bit integer to a saturated 8-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			
			if (eSrcFormat == UF_REGFORMAT_U16)
			{
				if(eDestFormat == UF_REGFORMAT_U8_UN)
				{
					SetOpcode(psState, psPckInst, IPCKU8U16);
				}
				else
				{
					SetOpcode(psState, psPckInst, IPCKS8U16);
				}
			}
			else
			{
				if(eDestFormat == UF_REGFORMAT_U8_UN)
				{
					SetOpcode(psState, psPckInst, IPCKU8S16);
				}
				else
				{
					SetOpcode(psState, psPckInst, IPCKS8S16);
				}
			}

			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if 
			(
				( eDestFormat == UF_REGFORMAT_U16 && eSrcFormat == UF_REGFORMAT_I16 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from a signed 16-bit integer to a (saturated) unsigned 16-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psPckInst, IUNPCKU16S16);
			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if 
			(
				( eDestFormat == UF_REGFORMAT_I16 && eSrcFormat == UF_REGFORMAT_U16 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from an unsigned 16-bit integer to a (saturated) signed 16-bit integer.
			*/
			PINST	psPckInst;

			psPckInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psPckInst, IUNPCKS16U16);
			psPckInst->asDest[0] = *psDest;
			psPckInst->asArg[0] = *psSrc;
			psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPckInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psPckInst);

			return psBlock;
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_U32 && eSrcFormat == UF_REGFORMAT_I16 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			/*
				Convert from a signed 16-bit integer to a (saturated) unsigned 32-bit integer.
			*/

			/* psDest = psSrc && 0x8000 */
			{
				PINST	psInst;

				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IAND);
				psInst->asDest[0] = psDest[0];
				psInst->asArg[0] = psSrc[0];
				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0x8000;
				AppendInst(psState, psBlock, psInst);
			}

			/*
				psDest = (psDest : psSrc ? 0x0)
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOVC_I32);
				psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
				psMovInst->asDest[0] = *psDest;
				psMovInst->asArg[0] = *psDest;
				psMovInst->asArg[1] = *psSrc;
				psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[2].uNumber = 0x0;
				AppendInst(psState, psBlock, psMovInst);
			}

			return psBlock;
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_I32 && eSrcFormat == UF_REGFORMAT_I16 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			PINST	psMovInst;

			psMovInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMovInst, IMOV);
			SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
			psMovInst->asDest[0] = *psDest;
				
			/*
				Sign extend from 16-bits from 32-bits.
			*/
			SignExtendInteger(psState, 
							  psBlock, 
							  16 /* uSrcBitWidth */, 
							  psSrc, 
							  0 /* uByteOffset */, 
							  &psMovInst->asArg[0]);

			AppendInst(psState, psBlock, psMovInst);

			return psBlock;
		}


		/* Conversions from 32-bit formats */
		else if 
			(
				(
					eDestFormat == UF_REGFORMAT_I8_UN || 
					eDestFormat == UF_REGFORMAT_U8_UN
				) &&
				(
					eSrcFormat == UF_REGFORMAT_U32 ||
					eSrcFormat == UF_REGFORMAT_I32
				) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from a 32-bit integer to a saturated 8-bit integer.
			*/
			if (eSrcFormat == UF_REGFORMAT_U32 && eDestFormat == UF_REGFORMAT_U8_UN)
			{
				/* psDest = psSrc && 0xffffff00 */
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0xffffff00;
					AppendInst(psState, psBlock, psInst);
				}

				/*
					psDest = (psDest : psSrc ? 0xff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0xff;
					AppendInst(psState, psBlock, psMovInst);
				}

				return psBlock;
			}
			else if (eSrcFormat == UF_REGFORMAT_I32 && eDestFormat == UF_REGFORMAT_U8_UN)
			{
				PCODEBLOCK psPosBlock;
				PCODEBLOCK psNegBlock;
				PCODEBLOCK psEndBlock;

				/* Set a predicate if the number is positive */
				/* P = psSrc && 0x80000000 */
				{
					PINST	psTestInst;

					psTestInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psTestInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
					psTestInst->u.psTest->eAluOpcode = IAND;
					MakePredicateArgument(&psTestInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psTestInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
					psTestInst->asArg[0] = psSrc[0];
					psTestInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psTestInst->asArg[1].uNumber = 0x80000000;
					AppendInst(psState, psBlock, psTestInst);
				}

				psPosBlock = AllocateBlock(psState, psBlock->psOwner);
				psNegBlock = AllocateBlock(psState, psBlock->psOwner);
				psEndBlock = AllocateBlock(psState, psBlock->psOwner);

				SetBlockUnconditional(psState, psPosBlock, psEndBlock);
				SetBlockUnconditional(psState, psNegBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState, 
									psBlock,
									USC_PREDREG_TEMP,
									psPosBlock,
									psNegBlock,
									IMG_FALSE);

				/*
					Do the negative block first
					psDst = 0
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[0].uNumber = 0;
					AppendInst(psState, psNegBlock, psMovInst);
				}

				/*
					And now the positive block
					Check if the number is too big for the destination
					psDest = psSrc && 0xffffff00
				*/
				{
					PINST	psAndInst;

					psAndInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psAndInst, IAND);
					psAndInst->asDest[0] = *psDest; 
					psAndInst->asArg[0] = psSrc[0];
					psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psAndInst->asArg[1].uNumber = 0xffffff00;
					AppendInst(psState, psPosBlock, psAndInst);
				}

				/*
					psDest = (psDest : psSrc ? 0xff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0xff;
					AppendInst(psState, psPosBlock, psMovInst);
				}

				return psEndBlock;
			}
			else if (eSrcFormat == UF_REGFORMAT_U32 && eDestFormat == UF_REGFORMAT_I8_UN)
			{
				/*
					Check if the number is too big for the destination
					psDest = psSrc && 0xffffff80
				*/
				{
					PINST	psAndInst;

					psAndInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psAndInst, IAND);
					psAndInst->asDest[0] = *psDest; 
					psAndInst->asArg[0] = psSrc[0];
					psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psAndInst->asArg[1].uNumber = 0xffffff80;
					AppendInst(psState, psBlock, psAndInst);
				}

				/*
					psDest = (psDest : psSrc ? 0x7f)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0x7f;
					AppendInst(psState, psBlock, psMovInst);
				}

				return psBlock;
			}
			else
			{
				PCODEBLOCK psPosBlock;
				PCODEBLOCK psNegBlock;
				PCODEBLOCK psEndBlock;

				PCODEBLOCK psTooNegBlock;
				PCODEBLOCK psNotTooNegBlock;

				/* Set a predicate if the number is positive */
				/* P = psSrc && 0x80000000 */
				{
					PINST	psTestInst;

					psTestInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psTestInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
					psTestInst->u.psTest->eAluOpcode = IAND;
					MakePredicateArgument(&psTestInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psTestInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
					psTestInst->asArg[0] = psSrc[0];
					psTestInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psTestInst->asArg[1].uNumber = 0x80000000;
					AppendInst(psState, psBlock, psTestInst);
				}

				psPosBlock = AllocateBlock(psState, psBlock->psOwner);
				psNegBlock = AllocateBlock(psState, psBlock->psOwner);
				psEndBlock = AllocateBlock(psState, psBlock->psOwner);

				SetBlockUnconditional(psState, psPosBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState,
									psBlock,
									USC_PREDREG_TEMP,
									psPosBlock,
									psNegBlock,
									IMG_FALSE);

				/*
					Do the negative block first
					Check if the number is too negative for the destination
					(ignoring the sign-bit)
					P = psDest = (psSrc && 0xffffff80 == 0)
				*/
				{
					PINST	psAndInst;

					psAndInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psAndInst, IAND);
					psAndInst->asDest[0] = psDest[0];
					psAndInst->asArg[0] = psSrc[0];
					psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psAndInst->asArg[1].uNumber = 0xffffff80;
					AppendInst(psState, psNegBlock, psAndInst);
				}

				{
					PINST	psXorInst;
		
					psXorInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
					psXorInst->u.psTest->eAluOpcode = IXOR;
					MakePredicateArgument(&psXorInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psXorInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
					psXorInst->asDest[TEST_UNIFIEDSTORE_DESTIDX] = psDest[0];
					psXorInst->asArg[0] = psDest[0];
					psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psXorInst->asArg[1].uNumber = 0xffffff80;
					AppendInst(psState, psNegBlock, psXorInst);
				}

				psTooNegBlock 	 = AllocateBlock(psState, psNegBlock->psOwner);
				psNotTooNegBlock = AllocateBlock(psState, psNegBlock->psOwner);

				SetBlockUnconditional(psState, psTooNegBlock, psEndBlock);
				SetBlockUnconditional(psState, psNotTooNegBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState, 
									psNegBlock,
									USC_PREDREG_TEMP,
									psTooNegBlock,
									psNotTooNegBlock,
									IMG_FALSE);

				/* If the number is too negative */

				/*
					psDest = 0x80
				*/
				{
					PINST	psMovInst;
		
					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = psDest[0];
					psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[0].uNumber = 0x80;
					AppendInst(psState, psTooNegBlock, psMovInst);
				}

				/* If the number is not too negative */

				/*
					psDest = psSrc && 0x7f
				*/
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0x7f;
					AppendInst(psState, psNotTooNegBlock, psInst);
				}
		
				/*
					psDest = psDest || 0x80
				*/
				{
					PINST	psInst;
		
					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IOR);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psDest[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0x80;
					AppendInst(psState, psNotTooNegBlock, psInst);
				}

				/*
					And now the positive block
					Check if the number is too big for the destination
					psDest = psSrc && 0xffffff80
				*/
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0xffffff80;
					AppendInst(psState, psPosBlock, psInst);
				}

				/*
					psDest = (psDest : psSrc ? 0x7f)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0x7f;
					AppendInst(psState, psPosBlock, psMovInst);
				}

				return psEndBlock;
			}
		}
		else if 
			(
				(
					eDestFormat == UF_REGFORMAT_I16 || 
					eDestFormat == UF_REGFORMAT_U16
				) &&
				(
					eSrcFormat == UF_REGFORMAT_U32 ||
					eSrcFormat == UF_REGFORMAT_I32
				) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
		   )
		{
			/*
				Convert from a 32-bit integer to a saturated 16-bit integer.
			*/
			if (eSrcFormat == UF_REGFORMAT_U32 && eDestFormat == UF_REGFORMAT_U16)
			{
				/* psDest = psSrc && 0xffff0000 */
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0xffff0000;
					AppendInst(psState, psBlock, psInst);
				}

				/*
					psDest = (psDest : psSrc ? 0xffff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0xffff;
					AppendInst(psState, psBlock, psMovInst);
				}

				return psBlock;
			}
			else
			if (eSrcFormat == UF_REGFORMAT_I32 && eDestFormat == UF_REGFORMAT_U16)
			{
				PCODEBLOCK psPosBlock;
				PCODEBLOCK psNegBlock;
				PCODEBLOCK psEndBlock;

				/* Set a predicate if the number is positive */
				/* P = psSrc && 0x80000000 */
				{
					PINST	psTestInst;

					psTestInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psTestInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
					psTestInst->u.psTest->eAluOpcode = IAND;
					MakePredicateArgument(&psTestInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psTestInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
					psTestInst->asArg[0] = psSrc[0];
					psTestInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psTestInst->asArg[1].uNumber = 0x80000000;
					AppendInst(psState, psBlock, psTestInst);
				}

				psPosBlock = AllocateBlock(psState, psBlock->psOwner);
				psNegBlock = AllocateBlock(psState, psBlock->psOwner);
				psEndBlock = AllocateBlock(psState, psBlock->psOwner);

				SetBlockUnconditional(psState, psPosBlock, psEndBlock);
				SetBlockUnconditional(psState, psNegBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState,
									psBlock,
									USC_PREDREG_TEMP,
									psPosBlock,
									psNegBlock,
									IMG_FALSE);
				/*
					Do the positive positive block
					Check if the number is too big for the destination
					psDest = psSrc && 0x7fff0000
				*/
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0x7fff0000;
					AppendInst(psState, psPosBlock, psInst);
				}

				/*
					psDest = (psDest : psSrc ? 0xffff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0xffff;
					AppendInst(psState, psPosBlock, psMovInst);
				}

				/*
					Now the negative block
					psDest = 0
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = psDest[0];
					psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[0].uNumber = 0x0;
					AppendInst(psState, psNegBlock, psMovInst);
				}

				return psEndBlock;
			}
			else if (eSrcFormat == UF_REGFORMAT_U32 && eDestFormat == UF_REGFORMAT_I16)
			{
				/*
					Check if the number is too big for the destination
					psDest = psSrc && 0xffff8000
				*/
				{
					PINST	psAndInst;

					psAndInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psAndInst, IAND);
					psAndInst->asDest[0] = *psDest; 
					psAndInst->asArg[0] = psSrc[0];
					psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psAndInst->asArg[1].uNumber = 0xffff8000;
					AppendInst(psState, psBlock, psAndInst);
				}

				/*
					psDest = (psDest : psSrc ? 0x7fff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0x7fff;
					AppendInst(psState, psBlock, psMovInst);
				}

				return psBlock;
			}
			else
			{
				PCODEBLOCK psPosBlock;
				PCODEBLOCK psNegBlock;
				PCODEBLOCK psEndBlock;

				PCODEBLOCK psTooNegBlock;
				PCODEBLOCK psNotTooNegBlock;

				/* Set a predicate if the number is positive */
				/* P = psSrc && 0x80000000 */
				{
					PINST	psTestInst;

					psTestInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psTestInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
					psTestInst->u.psTest->eAluOpcode = IAND;
					MakePredicateArgument(&psTestInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psTestInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
					psTestInst->asArg[0] = psSrc[0];
					psTestInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psTestInst->asArg[1].uNumber = 0x80000000;
					AppendInst(psState, psBlock, psTestInst);
				}

				psPosBlock = AllocateBlock(psState, psBlock->psOwner);
				psNegBlock = AllocateBlock(psState, psBlock->psOwner);
				psEndBlock = AllocateBlock(psState, psBlock->psOwner);

				SetBlockUnconditional(psState, psPosBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState,
									psBlock,
									USC_PREDREG_TEMP,
									psPosBlock,
									psNegBlock,
									IMG_FALSE);

				/*
					Do the negative block first
					Check if the number is too negative for the destination
					(ignoring the sign-bit)
					P = psDest = (psSrc && 0xffff8000 == 0)
				*/
				{
					PINST	psAndInst;

					psAndInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psAndInst, IAND);
					psAndInst->asDest[0] = psDest[0];
					psAndInst->asArg[0] = psSrc[0];
					psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psAndInst->asArg[1].uNumber = 0xffff8000;
					AppendInst(psState, psNegBlock, psAndInst);
				}

				{
					PINST	psXorInst;
		
					psXorInst = AllocateInst(psState, IMG_NULL);
					SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
					psXorInst->u.psTest->eAluOpcode = IXOR;
					MakePredicateArgument(&psXorInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
					psXorInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
					psXorInst->asDest[TEST_UNIFIEDSTORE_DESTIDX] = psDest[0];
					psXorInst->asArg[0] = psDest[0];
					psXorInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psXorInst->asArg[1].uNumber = 0xffff8000;
					AppendInst(psState, psNegBlock, psXorInst);
				}

				psTooNegBlock 	 = AllocateBlock(psState, psNegBlock->psOwner);
				psNotTooNegBlock = AllocateBlock(psState, psNegBlock->psOwner);

				SetBlockUnconditional(psState, psTooNegBlock, psEndBlock);
				SetBlockUnconditional(psState, psNotTooNegBlock, psEndBlock);

				/* Set the condition on the block */
				SetBlockConditional(psState, 
									psNegBlock,
									USC_PREDREG_TEMP,
									psTooNegBlock,
									psNotTooNegBlock,
									IMG_FALSE);

				/* If the number is too negative */

				/*
					psDest = 0x8000
				*/
				{
					PINST	psMovInst;
		
					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOV);
					psMovInst->asDest[0] = psDest[0];
					psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[0].uNumber = 0x8000;
					AppendInst(psState, psTooNegBlock, psMovInst);
				}

				/* If the number is not too negative */

				/*
					psDest = psSrc && 0x7fff
				*/
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0x7fff;
					AppendInst(psState, psNotTooNegBlock, psInst);
				}
		
				/*
					psDest = psDest || 0x8000
				*/
				{
					PINST	psInst;
		
					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IOR);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psDest[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0x8000;
					AppendInst(psState, psNotTooNegBlock, psInst);
				}

				/*
					And now the positive block
					Check if the number is too big for the destination
					psDest = psSrc && 0xffff8000
				*/
				{
					PINST	psInst;

					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, IAND);
					psInst->asDest[0] = psDest[0];
					psInst->asArg[0] = psSrc[0];
					psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psInst->asArg[1].uNumber = 0xffff8000;
					AppendInst(psState, psPosBlock, psInst);
				}

				/*
					psDest = (psDest : psSrc ? 0x7fff)
				*/
				{
					PINST	psMovInst;

					psMovInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psMovInst, IMOVC_I32);
					psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
					psMovInst->asDest[0] = *psDest;
					psMovInst->asArg[0] = *psDest;
					psMovInst->asArg[1] = *psSrc;
					psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
					psMovInst->asArg[2].uNumber = 0x7fff;
					AppendInst(psState, psPosBlock, psMovInst);
				}

				return psEndBlock;
			}
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_U32 && eSrcFormat == UF_REGFORMAT_I32 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			/*
				Convert from a signed 32-bit integer to a (saturated) unsigned 32-bit integer.
			*/

			/* psDest = psSrc && 0x80000000 */
			{
				PINST	psInst;

				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IAND);
				psInst->asDest[0] = psDest[0];
				psInst->asArg[0] = psSrc[0];
				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0x80000000;
				AppendInst(psState, psBlock, psInst);
			}

			/*
				psDest = (psDest : psSrc ? 0x0)
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOVC_I32);
				psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
				psMovInst->asDest[0] = *psDest;
				psMovInst->asArg[0] = *psDest;
				psMovInst->asArg[1] = *psSrc;
				psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[2].uNumber = 0x0;
				AppendInst(psState, psBlock, psMovInst);
			}

			return psBlock;
		}
		else if	(
				( eDestFormat == UF_REGFORMAT_I32 && eSrcFormat == UF_REGFORMAT_U32 ) &&
				(
					/*
					   N.B. If any type of dest saturation is requested 
					   we saturate to the most positive/most negative value
					   which can fit in the dest format
					*/
					uSat != 0
				)
			)
		{
			/*
				Convert from a unsigned 32-bit integer to a (saturated) signed 32-bit integer.
			*/

			/* psDest = psSrc && 0x80000000 */
			{
				PINST	psInst;

				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IAND);
				psInst->asDest[0] = psDest[0];
				psInst->asArg[0] = psSrc[0];
				psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psInst->asArg[1].uNumber = 0x80000000;
				AppendInst(psState, psBlock, psInst);
			}

			/*
				psDest = (psDest : psSrc ? 0x7fffffff)
			*/
			{
				PINST	psMovInst;

				psMovInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMovInst, IMOVC_I32);
				psMovInst->u.psMovc->eTest = TEST_TYPE_EQ_ZERO;
				psMovInst->asDest[0] = *psDest;
				psMovInst->asArg[0] = *psDest;
				psMovInst->asArg[1] = *psSrc;
				psMovInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
				psMovInst->asArg[2].uNumber = 0x7fffffff;
				AppendInst(psState, psBlock, psMovInst);
			}

			return psBlock;
		}
		else
		{
			PINST	psMovInst;

			/*
				Straight forward copy of I32/U32/I16/U16/I8/U8 data without
				conversion.
			*/
			psMovInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMovInst, IMOV);
			SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
			psMovInst->asDest[0] = *psDest;
			psMovInst->asArg[0] = *psSrc;
			AppendInst(psState, psBlock, psMovInst);

			return psBlock;
		}
	}
	else
	{
		PINST	psMovInst;

		/*
			Straight forward copy of I32/U32/I16/U16/I8/U8 data without
			conversion.
		*/
		psMovInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psMovInst, IMOV);
		SetPredicate(psState, psMovInst, uPredSrc, bPredNegate);
		psMovInst->asDest[0] = *psDest;
		psMovInst->asArg[0] = *psSrc;
		AppendInst(psState, psBlock, psMovInst);

		return psBlock;
	}
}

static
PCODEBLOCK ConvertIntegerInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertIntegerInstruction

 PURPOSE	: Converts to intermediate code (all channels of) an I32 or U32
			  input instruction.

 PARAMETERS	: psState				- Compiler state
			  psBlock			-  block onto which to append instructions
			  psSrc					- input instruction to converted

 *****************************************************************************/
{
	/*
		All I32 instructions are vector instructions, so many of issues are the same between all:
		Specifically, (a) the possibility of computing the same value into multiple channels, and
		(b) the possibility of overwriting source operands before they are needed, requiring
		temporary destinations
	*/
	IMG_UINT32	uTempDest = 0; /* channels needing to be written into temps */
	
	/* for each channel, number of any earlier channel containing the same result */
	IMG_UINT32	puRepDest[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
	IMG_UINT32	aauTempDestNum[2][4];
		
	IMG_UINT32	uChan;
	IMG_UINT32	uDest2Mask;
	IMG_UINT32	uDest1Mask;
	IMG_UINT32	uDestMask;

	/* If this instruction requires mutex locks */
	IMG_BOOL	bAtomic = IsInstructionAtomic(psSrc->eOpCode);
	/* FIXME: some cores have atomic instructions and thus do not need locks */
	IMG_BOOL    bNeedsRelease = bAtomic;
	/* FIXME: some cores/atomic instructions do not require the final store */
	IMG_BOOL	bDoStore = bAtomic;
	ARG			sAtomicNew;
	ARG			sAddress;

	/* Lock the mutex if this is an atomic instruction */
	if (bNeedsRelease)
		GenerateAtomicLock(psState, psBlock);
	
	uDest1Mask = psSrc->sDest.u.byMask;
	if (g_asInputInstDesc[psSrc->eOpCode].uNumDests == 2)
	{
		uDest2Mask = psSrc->sDest2.u.byMask;
	}
	else
	{
		uDest2Mask = 0;
	}
	uDestMask = uDest1Mask | uDest2Mask;
	
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32				j;
		ARG						asDest[2];
		PARG					apsDest[2];
		IMG_UINT32				uPredSrc;
		IMG_BOOL				bPredNegate;
		IMG_UINT32				uSrc;
		ARG						asArg[3];
		FLOAT_SOURCE_MODIFIER	asArgMod[3];
		IMG_UINT32				uDestIdx;
		IMG_BOOL				bSigned;
		UF_REGFORMAT			eDestFormat;

		/* Ignore components that aren't in the instruction mask. */
		if ((uDestMask & (1U << uChan)) == 0)
		{
			continue;
		}
		/* Ignore this component if it is the same as an earlier one. */
		if (puRepDest[uChan] != UINT_MAX)
		{
			continue;
		}
	
		/* Check for later duplicate of this op, or later use of it's dest. register */
		for (j = (uChan + 1); j < 4; j++)
		{
			if ((uDestMask & (1U << j)) != 0 && puRepDest[j] == UINT_MAX)
			{
				/* 
				   Check if the result for this component should be replicated to another component.
				*/
				if (
						(psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_XYZW &&
						!((uDest1Mask & (1U << j)) != 0 && (uDest1Mask & (1U << uChan)) == 0) &&
						!((uDest2Mask & (1U << j)) != 0 && (uDest2Mask & (1U << uChan)) == 0)
				   )
				{
					IMG_UINT32 k;
					puRepDest[j] = uChan;
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, uChan) != EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							puRepDest[j] = UINT_MAX;
						}
					}
				}
				/* Check if we are writing a component which is used later in the instruction. */
				if (puRepDest[j] == UINT_MAX)
				{
					IMG_UINT32 k;
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (uChan == EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							if ((uDest1Mask & (1U << j)) != 0 &&
								psSrc->sDest.eType == psSrc->asSrc[k].eType &&
								psSrc->sDest.uNum == psSrc->asSrc[k].uNum)
							{
								uTempDest |= (1U << uChan);
							}
							if ((uDest2Mask & (1U << j)) != 0 &&
								psSrc->sDest2.eType == psSrc->asSrc[k].eType &&
								psSrc->sDest2.uNum == psSrc->asSrc[k].uNum)
							{
								uTempDest |= (1U << uChan);
							}
						}
					}
				}
			}
		}

		/*
			Always write a temporary destination because the instruction has
			restrictions on the destination banks.
		*/
		if (psSrc->eOpCode == UFOP_DIV || psSrc->eOpCode == UFOP_DIV2)
		{
			uTempDest |= (1U << uChan);
		}
		
		/*
			Get the sources for the instruction.
		*/
		for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
		{
			PUF_REGISTER	psInputDest = (uDestIdx == 0) ? &psSrc->sDest : &psSrc->sDest2;
			IMG_UINT32		uInputDestMask = (uDestIdx == 0) ? uDest1Mask : uDest2Mask;

			if (uInputDestMask & (1U << uChan))
			{
				PARG	psHwDest = &asDest[uDestIdx];

				if (uTempDest & (1U << uChan))
				{
					/*
						Get a spare register to use a destination.
					*/
					aauTempDestNum[uDestIdx][uChan] = (uDestIdx == 0) ? USC_TEMPREG_TEMPDEST : USC_TEMPREG_TEMPDEST2;
					aauTempDestNum[uDestIdx][uChan] += uChan;

					/*
						Set an argument structure referencing this register.
					*/
					InitInstArg(psHwDest);
					psHwDest->uType = USEASM_REGTYPE_TEMP;
					psHwDest->uNumber = aauTempDestNum[uDestIdx][uChan];
				}
				else
				{
					/*
						Convert the destination for this channel to the intermediate format.
					*/
					GetDestinationTypeless(psState, psBlock, psInputDest, uChan, psHwDest);
				}	

				apsDest[uDestIdx] = psHwDest;
			}
			else
			{
				apsDest[uDestIdx] = NULL;
			}
		}

		if (uTempDest & (1U << uChan))
		{
			/*
				If we are writing to a temporary then make the main instruction unpredicated and
				apply the predicate on the MOV we'll add later.
			*/
			uPredSrc = USC_PREDREG_NONE;
			bPredNegate = IMG_FALSE;
		}
		else
		{
			/*
				Get the predicate for the instruction in the intermediate format.
			*/
			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uChan);
		}

		/*
			Get the sources for the instruction in the intermediate format.
		*/
		for (uSrc = 0; uSrc < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; uSrc++)
		{
			GetSourceTypeless(psState, psBlock, &psSrc->asSrc[uSrc], uChan, &asArg[uSrc], IMG_TRUE, &asArgMod[uSrc]);

			/*
				Handle an absolute modifier on a source.
			*/
			if (asArgMod[uSrc].bAbsolute)
			{
				switch (psSrc->asSrc[uSrc].eFormat)
				{
					case UF_REGFORMAT_U32:
					case UF_REGFORMAT_U16:
					{
						/* Ignore the absolute modifier for an unsigned type. */
						asArgMod[uSrc].bAbsolute = IMG_FALSE;
						break;
					}
				    case UF_REGFORMAT_I32:
				    case UF_REGFORMAT_I8_UN:
				    case UF_REGFORMAT_I16:
				    {
					    asArgMod[uSrc].bAbsolute = IMG_FALSE;

					    if( psState->uCompilerFlags & UF_OPENCL )
						    IntegerAbsoluteFmt(psState,psBlock, &asArg[uSrc], &asArg[uSrc],psSrc->asSrc[uSrc].eFormat);
					    else
						    IntegerAbsolute(psState, psBlock, &asArg[uSrc], &asArg[uSrc]);
						    
					    break;
				    }
					case UF_REGFORMAT_F32:
					{
						/* Apply the absolute modifier directly in the main instruction. */
						break;
					}
					default: imgabort();
				}
			}
		}

		/*
			Get the destination format.
		*/
		if (uDest1Mask)
		{
			eDestFormat = psSrc->sDest.eFormat;
		}
		else
		{
			eDestFormat = psSrc->sDest2.eFormat;
		}

		/*
			Signed or unsigned arithmetic.
		*/
		if (eDestFormat == UF_REGFORMAT_I8_UN || eDestFormat == UF_REGFORMAT_I16 ||
			  eDestFormat == UF_REGFORMAT_I32)
		{
			bSigned = IMG_TRUE;
		}
		else
		{
			ASSERT(eDestFormat == UF_REGFORMAT_U8_UN || eDestFormat == UF_REGFORMAT_U16 ||
				eDestFormat == UF_REGFORMAT_U32);
			bSigned = IMG_FALSE;
		}

		switch (psSrc->eOpCode)
		{
			case UFOP_MOV:
			{
				psBlock = ConvertIntegerMOV(psState, 
											psBlock, 
											eDestFormat, 
											psSrc, 
											apsDest[0], 
											uPredSrc, 
											bPredNegate, 
											&asArg[0], 
											&asArgMod[0]);
				break;
			}
			case UFOP_ATOM_XCHG:
			{
				/* We handle the store here, so disable the final store */
				bDoStore = IMG_FALSE;
				/* Load old value to destination, store new value */
				GenerateAtomicLoad(psState, psBlock, &asArg[0], apsDest[0]);
				GenerateAtomicStore(psState, psBlock, &asArg[0], &asArg[1]);
				break;
			}
			case UFOP_FTOI:
			{
				ConvertFloatToIntegerConversion(psState, 
												psBlock, 
												eDestFormat, 
												psSrc, 
												apsDest[0], 
												uPredSrc, 
												bPredNegate, 
												&asArg[0], 
												&asArgMod[0],
												bSigned);
				break;
			}
			case UFOP_SETBEQ:
			case UFOP_SETBGE:
			case UFOP_SETBLT:
			case UFOP_SETBNE:
			{
				/*
					No negate modifiers on SETBEQ/SETBNE/SETBGE/SETBLT.
				*/
				ASSERT(!asArgMod[0].bNegate);
				ASSERT(!asArgMod[1].bNegate);

				if (eDestFormat == UF_REGFORMAT_I8_UN || eDestFormat == UF_REGFORMAT_U8_UN)
				{
					ConvertComparisonInstructionInt8(psState,
													 psBlock,
													 apsDest[0],
													 uPredSrc,
													 bPredNegate,
													 &asArg[0],
													 &asArg[1],
													 bSigned,
													 psSrc->eOpCode);
				}
				else if (eDestFormat == UF_REGFORMAT_I16 || eDestFormat == UF_REGFORMAT_U16)
				{
					ConvertComparisonInstructionInt16(psState,
													  psBlock,
													  apsDest[0],
													  uPredSrc,
													  bPredNegate,
													  &asArg[0],
													  &asArg[1],
													  bSigned,
													  psSrc->eOpCode);
				}
				else
				{
					ConvertComparisonInstructionInt32(psState,
													  psBlock,
													  apsDest[0],
													  uPredSrc,
													  bPredNegate,
													  &asArg[0],
													  &asArg[1],
													  bSigned,
													  psSrc->eOpCode);
				}
				break;
			}
			case UFOP_ATOM_MIN:
			case UFOP_ATOM_MAX:
			{
				/* Generate a temporary register */
				InitInstArg(&sAtomicNew);
				sAtomicNew.uType = USEASM_REGTYPE_TEMP;
				sAtomicNew.uNumber = GetNextRegister(psState);
				sAtomicNew.eFmt = apsDest[0]->eFmt;
				/* Remember to store the old result later */
				sAddress = asArg[0];
				GenerateAtomicLoad(psState, psBlock, &sAddress, apsDest[0]);
				/* Switch the registers so that the operation uses the loaded value
				 and make sure we return the loaded value, rather than the one calculated */
				asArg[0] = *(apsDest[0]);
				*(apsDest[0]) = sAtomicNew;
			}
			case UFOP_MIN:
			case UFOP_MAX:
			{
				IMG_BOOL	bMax;

				if ((psSrc->eOpCode == UFOP_MAX) || (psSrc->eOpCode == UFOP_ATOM_MAX))
					bMax = IMG_TRUE;
				else
					bMax = IMG_FALSE;

				/* signed or unsigned */
				if (eDestFormat == UF_REGFORMAT_I16 || eDestFormat == UF_REGFORMAT_U16)
				{
					ConvertMinMaxInstructionInt16(psState, 
												  psBlock, 
												  apsDest[0], 
												  uPredSrc, 
												  bPredNegate, 
												  &asArg[0], 
												  asArgMod[0].bNegate,
												  &asArg[1], 
												  asArgMod[1].bNegate,
												  bSigned, 
												  bMax);
				}
				else if (eDestFormat == UF_REGFORMAT_I8_UN || eDestFormat == UF_REGFORMAT_U8_UN)
				{
					ConvertMinMaxInstructionInt8(psState, 
						psBlock, 
						apsDest[0], 
						uPredSrc, 
						bPredNegate, 
						&asArg[0], 
						asArgMod[0].bNegate,
						&asArg[1], 
						asArgMod[1].bNegate,
						bSigned, 
						bMax);
				}
				else
				{
					ConvertMinMaxInstructionInt32(psState, 
												  psBlock, 
												  apsDest[0], 
												  uPredSrc, 
												  bPredNegate, 
												  &asArg[0], 
												  asArgMod[0].bNegate,
												  &asArg[1], 
												  asArgMod[1].bNegate,
												  bSigned, 
												  bMax);
				}

				if (bDoStore)
					GenerateAtomicStore(psState, psBlock, &sAddress, apsDest[0]);

				break;
			}
			case UFOP_ADD: /* signedness makes no difference */
			case UFOP_ATOM_INC:
			case UFOP_ATOM_DEC:
			case UFOP_ATOM_ADD:
			case UFOP_ADD2:
			case UFOP_MAD2:
			case UFOP_MUL2: /* signed or unsigned, but with first dest only */
			case UFOP_MAD: /* signed or unsigned */
			{
				if (eDestFormat == UF_REGFORMAT_I16 || eDestFormat == UF_REGFORMAT_U16)
				{
					GenerateInt16Arithmetic(psState,
											psBlock,
											psSrc->eOpCode,
											apsDest[0],
											uPredSrc,
											bPredNegate,
											asArg,
											asArgMod,
											bSigned);
				}
				else
				{
					GenerateIntegerArithmetic(psState,
											  psBlock,
											  NULL /* psInsertBeforeInst */, 
											  psSrc->eOpCode,
											  apsDest[0],
											  apsDest[1],
											  uPredSrc,
											  bPredNegate,
											  &asArg[0],
											  asArgMod[0].bNegate,
											  &asArg[1],
											  asArgMod[1].bNegate,
											  &asArg[2],
											  asArgMod[2].bNegate,
											  bSigned);
				}
				break;
			}
			case UFOP_DIV:
			case UFOP_DIV2: /* unsigned only, but with first dest only */
			{
				ASSERT (GetRegisterFormat(psState, &psSrc->asSrc[0]) == UF_REGFORMAT_U32);

				#if defined(SUPPORT_SGX545)
				if (
						(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) &&
						!(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) &&
						!(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27005)
				   )
				{
					PINST	psDivInst;

					psDivInst = AllocateInst(psState, IMG_NULL);

					SetOpcodeAndDestCount(psState, psDivInst, IIDIV32, 2 /* uNewDestCount */);

					for (uDestIdx = 0; uDestIdx < 2; uDestIdx++)
					{
						if (apsDest[uDestIdx] != NULL)
						{
							psDivInst->asDest[uDestIdx] = *apsDest[uDestIdx];
						}
						else
						{
							/* Must have a destination even if its unused. */
							psDivInst->asDest[uDestIdx].uType = USEASM_REGTYPE_TEMP;
							psDivInst->asDest[uDestIdx].uNumber = USC_TEMPREG_DUMMY;
						}
					}

					psDivInst->asArg[IDIV32_DIVIDEND_ARGINDEX] = asArg[0];
					psDivInst->asArg[IDIV32_DIVISOR_ARGINDEX] = asArg[1];

					psDivInst->asArg[IDIV32_DRC_ARGINDEX].uType = USEASM_REGTYPE_DRC;
					psDivInst->asArg[IDIV32_DRC_ARGINDEX].uNumber = 0;
	
					AppendInst(psState, psBlock, psDivInst);
				}
				else
				#endif /* defined(SUPPORT_SGX545) */
				{
					ARG		sQuotientDest;
					ARG		sRemainderDest;

					if (apsDest[0] != NULL)
					{
						sQuotientDest = *apsDest[0];
					}
					else
					{
						InitInstArg(&sQuotientDest);
						sQuotientDest.uType = USEASM_REGTYPE_TEMP;
						sQuotientDest.uNumber = USC_TEMPREG_DUMMY;
					}

					if (apsDest[1] != NULL)
					{
						sRemainderDest = *apsDest[1];
					}
					else
					{
						InitInstArg(&sRemainderDest);
						sRemainderDest.uType = USEASM_REGTYPE_TEMP;
						sRemainderDest.uNumber = USC_TEMPREG_DUMMY;
					}

					psBlock = GenerateIntegerDivide_Non545(psState, 
						                         psBlock,
												 &sQuotientDest,
												 &sRemainderDest,
												 &asArg[0],
												 &asArg[1]);
				}
				break;
			}
			default:
			{
				imgabort();
			}
		}

	}

	/* Insert MOV's to get temporary/repeated values into the right place(s) */
	for (uChan = 0; uChan < 4; uChan++)
	{
		if ((uTempDest & (1U << uChan)) != 0 || puRepDest[uChan] != UINT_MAX)
		{
			if (psSrc->sDest.u.byMask & (1U << uChan))
			{
				MoveTempToDestination(psState,
									  psBlock,
									  &psSrc->sDest,
									  psSrc->uPredicate,
									  uChan,
									  puRepDest[uChan],
									  aauTempDestNum[0][uChan]);
			}
			if (psSrc->sDest2.u.byMask & (1U << uChan))
			{
				MoveTempToDestination(psState,
									  psBlock,
									  &psSrc->sDest2,
									  psSrc->uPredicate,
									  uChan,
									  puRepDest[uChan],
									  aauTempDestNum[1][uChan]);
			}
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For cores with the vector instruction set integer instructions are
		still represented as writing scalar registers so generate an extra
		move from the scalar destinations to the vector register corresponding
		to the destination of the input instruction.
	*/
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		if (psSrc->sDest.u.byMask != 0)
		{
			StoreIntoVectorDest(psState, psBlock, psSrc, &psSrc->sDest);
		}
		if (psSrc->sDest2.u.byMask != 0)
		{
			StoreIntoVectorDest(psState, psBlock, psSrc, &psSrc->sDest2);
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* Release the mutex if this is an atomic instruction */
	if (bNeedsRelease)
		GenerateAtomicRelease(psState, psBlock);

	return psBlock;
}

static
IMG_VOID ConvertMovaIntInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psInputInst)
/*****************************************************************************
 FUNCTION	: ConvertMovaIntInstruction
    
 PURPOSE	: Convert an input MOVA_INT to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bFloat32			- Type of the MOVA source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psInputInst->sDest.u.byMask & (1U << uChan))
		{
			ARG			sSrc;
			IMG_UINT32	uPredSrc;
			IMG_BOOL	bPredSrcNegate;
			PINST		psInst;

			/*
				Get any predicate for this channel.
			*/
			GetInputPredicate(psState, &uPredSrc, &bPredSrcNegate, psInputInst->uPredicate, uChan);

			/* Get the source for the address offset. */
			GetSourceTypeless(psState, 
							  psCodeBlock, 
							  &psInputInst->asSrc[0], 
							  uChan, 
							  &sSrc, 
							  IMG_FALSE /* bSourceModifier */,
							  NULL /* psSourceMod */);

			/*
				Insert an instruction to convert the integer value to a suitable form for indexing.
			*/
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, ICVTINT2ADDR);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_ADDRESS + uChan;
			SetPredicate(psState, psInst, uPredSrc, bPredSrcNegate);
			psInst->asArg[0] = sSrc;
			AppendInst(psState, psCodeBlock, psInst);
		}
	}
}

IMG_INTERNAL
PCODEBLOCK ConvertInstToIntermediateInt32(PINTERMEDIATE_STATE psState, 
										  PCODEBLOCK psBlock, 
										  PUNIFLEX_INST psSrc,
										  IMG_BOOL bConditional,
										  IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediate
    
 PURPOSE	: Convert a 32-bit integer input instruction to the intermediate
			  format (either signed or unsigned)

 PARAMETERS	: psState			- Compiler state.
			  psBlock	- Block to which intermediate instruction(s), possibly with
				flow control, will be appended.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: Block to which intermediate code for subsequent input instrs
	should be appended (different from psBlock iff flow control was used).
*****************************************************************************/
{
	/*
		Use the common code for SETP (which writes a predicate register with
		a comparison result).
	*/
	if (psSrc->eOpCode == UFOP_SETP)
	{
		ConvertSetpInstructionNonC10(psState, psBlock, psSrc);
		return psBlock;
	}

	/*
		Instructions where the destination register format is irrelevant.
	*/
	if (psSrc->eOpCode == UFOP_MOVA_INT)
	{
		ConvertMovaIntInstruction(psState, psBlock, psSrc);
		return psBlock;
	}
	if ((psSrc->eOpCode == UFOP_MOVCBIT) || (psSrc->eOpCode == UFOP_ATOM_CMPXCHG))
	{
		ConvertComparisonInstructionF32(psState, psBlock, psSrc);
		return psBlock;
	}
	if (psSrc->eOpCode == UFOP_OR ||
		psSrc->eOpCode == UFOP_AND ||
		psSrc->eOpCode == UFOP_XOR ||
		psSrc->eOpCode == UFOP_SHL ||
		psSrc->eOpCode == UFOP_SHR ||
		psSrc->eOpCode == UFOP_ASR ||
		psSrc->eOpCode == UFOP_ATOM_OR ||
		psSrc->eOpCode == UFOP_ATOM_AND ||
		psSrc->eOpCode == UFOP_ATOM_XOR)
	{
		ConvertBitwiseInstructionF32(psState, psBlock, psSrc);
		return psBlock;
	}
	if (psSrc->eOpCode == UFOP_MOVVI)
	{
		ConvertVertexInputInstructionTypeless(psState, psBlock, psSrc);
		return psBlock;
	}
	if (psSrc->eOpCode == UFOP_LD ||
		psSrc->eOpCode == UFOP_LDB ||
		psSrc->eOpCode == UFOP_LDD ||
		psSrc->eOpCode == UFOP_LDL ||
		psSrc->eOpCode == UFOP_LDP ||
		psSrc->eOpCode == UFOP_LDC ||
		psSrc->eOpCode == UFOP_LDCLZ ||
		psSrc->eOpCode == UFOP_LDPIFTC ||
		psSrc->eOpCode == UFOP_LD2DMS ||
		psSrc->eOpCode == UFOP_LDGATHER4)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			return ConvertSamplerInstructionFloatVec(psState, psBlock, psSrc, bConditional && !bStaticCond, IMG_FALSE /* bF16 */);
		}
		else
		#endif
		return ConvertSamplerInstructionF32(psState, psBlock, psSrc, bConditional && !bStaticCond);
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For a pure copy (no format conversion) use a vector move.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		if (psSrc->eOpCode == UFOP_MOV)
		{
			UF_REGFORMAT	eDestFormat = psSrc->sDest.eFormat;
			UF_REGFORMAT	eSrcFormat = psSrc->asSrc[0].eFormat;

			if (
					(
						eDestFormat == UF_REGFORMAT_U32 || 
						eDestFormat == UF_REGFORMAT_I32
					) &&
					eSrcFormat == eDestFormat &&
					psSrc->sDest.byMod == 0 &&
					psSrc->asSrc[0].byMod == UFREG_SMOD_NONE
				)
			{
				return ConvertInstToIntermediateF32_Vec(psState, psBlock, psSrc, bConditional, bStaticCond);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/* Do the instruction itself. */
	psBlock = ConvertIntegerInstruction(psState, psBlock, psSrc);

	/* Store first dest (of any case above) into an indexable temporary register. */
	if (psSrc->sDest.u.byMask && psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psBlock, &psSrc->sDest, psSrc->sDest.eFormat, USC_TEMPREG_F32INDEXTEMPDEST);
	}
	/* Store second dest into an indexable temporary register */
	if (g_asInputInstDesc[psSrc->eOpCode].uNumDests == 2 && 
		psSrc->sDest2.u.byMask &&
		psSrc->sDest2.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psBlock, &psSrc->sDest2, psSrc->sDest2.eFormat, USC_TEMPREG_F32INDEXTEMPDEST);
	}
	return psBlock;
}

IMG_INTERNAL
IMG_VOID IntegerNegate(PINTERMEDIATE_STATE		psState,
					   PCODEBLOCK				psCodeBlock,
					   PINST					psInsertBeforeInst,
					   PARG						psSrc,
					   PARG						psResult)
/*****************************************************************************
 FUNCTION	: IntegerNegate
    
 PURPOSE	: Get the twos complement negation of a dword.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Location in the block to add the instructions at.
			  psSrc				- Register containing the dword to be negated.
			  psResult			- Returns a temporary register which contains
								the negated value.
			  
 RETURNS	: None.
*****************************************************************************/
{
	ARG	sSrc = *psSrc;

	/*
		Get a new register for the negated result.
	*/
	InitInstArg(psResult);
	psResult->uType = USEASM_REGTYPE_TEMP;
	psResult->uNumber = GetNextRegister(psState);

	#if defined(SUPPORT_SGX545)
	if (
		 (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) && 
		 !(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32)
	   )
	{
		/*
			IMA32	TEMP, SRC.NEGATE, 1, 0
		*/
		GenerateIntegerArithmetic_545(psState,
									  psCodeBlock,
									  psInsertBeforeInst,
									  UFOP_MOV,
									  psResult,
									  NULL,
									  USC_PREDREG_NONE,
									  IMG_FALSE,
									  &sSrc,
									  IMG_TRUE /* bNegateA */,
									  NULL,
									  IMG_FALSE /* bNegateB */,
									  NULL,
									  IMG_FALSE /* bNegateC */,
									  IMG_TRUE);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
		 (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_32BIT_INT_MAD) != 0 && 
		 (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IMA32_32X16_PLUS_32) != 0
	   )
	{
		/*
			IMA32	TEMP, SRC.NEGATE, 1, 0
		*/
		GenerateIntegerArithmetic_543(psState,
									  psCodeBlock,
									  psInsertBeforeInst,
									  UFOP_MOV,
									  psResult,
									  USC_PREDREG_NONE,
									  IMG_FALSE,
									  &sSrc,
									  IMG_TRUE /* bNegateA */,
									  NULL,
									  IMG_FALSE /* bNegateB */,
									  NULL,
									  IMG_FALSE /* bNegateC */,
									  IMG_TRUE);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		IMG_UINT32	uTemp1 = GetNextRegister(psState);

		/*
			TEMP1 = NOT(SRC)
		*/
		{
			PINST	psXorInst;

			psXorInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psXorInst, INOT);
			psXorInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psXorInst->asDest[0].uNumber = uTemp1;
			psXorInst->asArg[0] = sSrc;
			InsertInstBefore(psState, psCodeBlock, psXorInst, psInsertBeforeInst);
		}

		/*
			RESULT = 1 * 1 + TEMP1
		*/
		{
			PINST	psImaeInst;

			psImaeInst = AllocateInst(psState, psInsertBeforeInst);
			SetOpcode(psState, psImaeInst, IIMAE);
			psImaeInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_U32;
			psImaeInst->asDest[0] = *psResult;
			psImaeInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[0].uNumber = 1;
			psImaeInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psImaeInst->asArg[1].uNumber = 1;
			psImaeInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
			psImaeInst->asArg[2].uNumber = uTemp1;
			InsertInstBefore(psState, psCodeBlock, psImaeInst, psInsertBeforeInst);
		}
	}
}

/* End of file */
