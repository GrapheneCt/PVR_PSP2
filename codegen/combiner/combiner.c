/******************************************************************************
 * Name         : combiner.c
 * Title        : Pixel Shader Compiler Text Interface
 * Author       : John Russell
 * Created      : Jan 2002
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
 *
 * Modifications:-
 * $Log: combiner.c $
 *****************************************************************************/


#include <string.h>

#include "sgxdefs.h"
#include "pvr_debug.h"

#include "combiner.h"

typedef enum
{
	REGISTER_TYPE_TEMP,
	REGISTER_TYPE_PRIMATTR,
	REGISTER_TYPE_SECATTR,
	REGISTER_TYPE_UNKNOWN,
} REGISTER_TYPE, *PREGISTER_TYPE;

/*********************************************************************************
 Function		: ReplaceArgument

 Description	: Replaces a source argument to a hardware instruction.

 Parameters		: puInst0, puInst1			- Pointer to the instruction words.
				  uSrcIdx					- Source to replace.
				  eNewRegType				- New argument register type.
				  uNewRegNum				- New argument register number.

 Return			: none
*********************************************************************************/
static IMG_VOID ReplaceArgument(IMG_PUINT32					puInst0,
								IMG_PUINT32					puInst1,
								IMG_UINT32					uSrcIdx,
								REGISTER_TYPE				eNewRegType,
								IMG_UINT32					uNewRegNum)
{
	switch (uSrcIdx)
	{
		case 0:
		{
			/*
				Clear existing state.
			*/
			(*puInst1) &= EURASIA_USE1_S0BANK_CLRMSK;
			(*puInst0) &= EURASIA_USE0_SRC0_CLRMSK;

			/*
				Set new state.
			*/
			if (eNewRegType == REGISTER_TYPE_TEMP)
			{
				(*puInst1) |= (EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT);
			}
			else
			{
				(*puInst1) |= (EURASIA_USE1_S0STDBANK_PRIMATTR << EURASIA_USE1_S0BANK_SHIFT);
			}
			(*puInst0) |= (uNewRegNum << EURASIA_USE0_SRC0_SHIFT);
			break;
		}
		case 1:
		{
			/*
				Clear existing state.
			*/
			(*puInst0) &= EURASIA_USE0_S1BANK_CLRMSK;
			(*puInst0) &= EURASIA_USE0_SRC1_CLRMSK;
			(*puInst1) &= ~EURASIA_USE1_S1BEXT;

			/*
				Set new state.
			*/
			switch (eNewRegType)
			{
				case REGISTER_TYPE_TEMP: 
				{
					(*puInst0) |= (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_PRIMATTR: 
				{
					(*puInst0) |= (EURASIA_USE0_S1STDBANK_PRIMATTR << EURASIA_USE0_S1BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_SECATTR:
				{
					(*puInst0) |= (EURASIA_USE0_S1STDBANK_SECATTR << EURASIA_USE0_S1BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_UNKNOWN:
				{
					PVR_DBG_BREAK;
					break;
				}
			}
			(*puInst0) |= (uNewRegNum << EURASIA_USE0_SRC1_SHIFT);
			break;
		}
		case 2:
		{
			/*
				Clear existing state.
			*/
			(*puInst0) &= EURASIA_USE0_S2BANK_CLRMSK;
			(*puInst0) &= EURASIA_USE0_SRC2_CLRMSK;
			(*puInst1) &= ~EURASIA_USE1_S2BEXT;

			/*
				Set new state.
			*/
			switch (eNewRegType)
			{
				case REGISTER_TYPE_TEMP: 
				{
					(*puInst0) |= (EURASIA_USE0_S2STDBANK_TEMP << EURASIA_USE0_S2BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_PRIMATTR: 
				{
					(*puInst0) |= (EURASIA_USE0_S2STDBANK_PRIMATTR << EURASIA_USE0_S2BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_SECATTR:
				{
					(*puInst0) |= (EURASIA_USE0_S2STDBANK_SECATTR << EURASIA_USE0_S2BANK_SHIFT);
					break;
				}
				case REGISTER_TYPE_UNKNOWN:
				{
					PVR_DBG_BREAK;
					break;
				}
			}
			(*puInst0) |= (uNewRegNum << EURASIA_USE0_SRC2_SHIFT);
			break;
		}
	}
}

/*********************************************************************************
 Function		: GetDestination

 Description	: Get the details of a destination in a standard format.
 
 Parameters		: uInst0, uInst1		- Instruction.
				  peDestType			- Returns the destination type.
				  puDestNum				- Returns the destination number.

 Return			: none
*********************************************************************************/
static IMG_VOID GetDestination(IMG_UINT32				uInst0,
							   IMG_UINT32				uInst1,
							   PREGISTER_TYPE			peDestType,
							   IMG_PUINT32				puDestNum)
{
	IMG_UINT32	uDestBank;
	IMG_UINT32	uDestBankExt;

	*puDestNum = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;

	uDestBank = (uInst1 & ~EURASIA_USE1_D1BANK_CLRMSK) >> EURASIA_USE1_D1BANK_SHIFT;
	uDestBankExt = (uInst1 & EURASIA_USE1_DBEXT);

	if (uDestBankExt)
	{
		*peDestType = REGISTER_TYPE_UNKNOWN;
	}
	else
	{
		switch (uDestBank)
		{
			case EURASIA_USE1_D1STDBANK_TEMP: *peDestType = REGISTER_TYPE_TEMP; break;
			case EURASIA_USE1_D1STDBANK_PRIMATTR: *peDestType = REGISTER_TYPE_PRIMATTR; break;
			default: *peDestType = REGISTER_TYPE_UNKNOWN; break;
		}
	}
}

/*********************************************************************************
 Function		: GetSource

 Description	: Get the details of a source in a standard format.

 Parameters		: uInst0, uInst1		- Instruction.
				  uSrc					- Index of the source to get.
				  puSrcBankExt			- Returns the source.
				  puSrcBank
				  puSrcNum

 Return			: none
*********************************************************************************/
static IMG_VOID GetSource(IMG_UINT32					uInst0,
						  IMG_UINT32					uInst1,
						  IMG_UINT32					uSrc,
						  IMG_PUINT32					puSrcBankExt,
						  IMG_PUINT32					puSrcBank,
						  IMG_PUINT32					puSrcNum)
{
	switch (uSrc)
	{
		case 0:
		{
			IMG_UINT32	uSrcBank;
			uSrcBank = (uInst1 & ~EURASIA_USE1_S0BANK_CLRMSK) >> EURASIA_USE1_S0BANK_SHIFT;
			if (uSrcBank == EURASIA_USE1_S0STDBANK_TEMP)
			{
				*puSrcBank = EURASIA_USE0_S1STDBANK_TEMP;
			}
			else
			{
				*puSrcBank = EURASIA_USE0_S1STDBANK_PRIMATTR;
			}
			*puSrcBankExt = 0;

			*puSrcNum = (uInst0 & ~EURASIA_USE0_SRC0_CLRMSK) >> EURASIA_USE0_SRC0_SHIFT;
			break;
		}
		case 1:
		{
			*puSrcBank = (uInst0 & ~EURASIA_USE0_S1BANK_CLRMSK) >> EURASIA_USE0_S1BANK_SHIFT;
			*puSrcNum = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
			*puSrcBankExt = uInst1 & EURASIA_USE1_S1BEXT;
			break;
		}
		case 2:
		{
			*puSrcBank = (uInst0 & ~EURASIA_USE0_S2BANK_CLRMSK) >> EURASIA_USE0_S2BANK_SHIFT;
			*puSrcNum = (uInst0 & ~EURASIA_USE0_SRC2_CLRMSK) >> EURASIA_USE0_SRC2_SHIFT;
			*puSrcBankExt = uInst1 & EURASIA_USE1_S2BEXT;
			break;
		}
	}
}

/*********************************************************************************
 Function		: ReplaceArguments

 Description	: Try to replace uses of a register.

 Parameters		: uReplaceCodeCount				- Count of instructions to replace
												the register within.
				  puReplaceCode					- Instruction to replace the register
												in.
				  bOldRegIsTemp					- If TRUE the register to be replaced
												is a temporary otherwise it is a
												primary attribute.
				  uOldRegNum					- Number of the register to be replaced.
				  eNewRegType					- Type of the replacement register.
				  uNewRegNum					- Number of the replacement register.
				  bDoReplace					- If TRUE replace the register otherwise
												just check if replacement is possible.

 Return			: TRUE if replacing the register is possible.
*********************************************************************************/
static IMG_BOOL ReplaceArguments(IMG_UINT32				uReplaceCodeCount,
								 IMG_PUINT32			puReplaceCode,
								 IMG_BOOL				bOldRegIsTemp,
								 IMG_UINT32				uOldRegNum,
								 REGISTER_TYPE			eNewRegType,
								 IMG_UINT32				uNewRegNum,
								 IMG_BOOL				bDoReplace)
{
	IMG_UINT32	uInstIdx;
	IMG_BOOL	bNewRegOverwritten = IMG_FALSE;

	for (uInstIdx = 0; uInstIdx < uReplaceCodeCount; uInstIdx++)
	{
		IMG_UINT32	uInst0, uInst1;
		IMG_UINT32	uOp;
		IMG_UINT32	uSrcsUsedMask;
		IMG_UINT32	uDestMask;
		IMG_BOOL	bShortPredicate;
		IMG_UINT32	uSrc;

		uInst0 = puReplaceCode[uInstIdx * 2 + 0];
		uInst1 = puReplaceCode[uInstIdx * 2 + 1];

		uOp = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;

		/*
			Check which source arguments are used by the instruction.
		*/
		uDestMask = 0xF;
		switch (uOp)
		{
			case EURASIA_USE1_OP_PCKUNPCK:
			{
				uSrcsUsedMask = 6;
				bShortPredicate = IMG_FALSE;

				/*
					Get the destination mask.
				*/
				uDestMask = (uInst1 & ~EURASIA_USE1_PCK_DMASK_CLRMSK) >> EURASIA_USE1_PCK_DMASK_SHIFT;
				break;
			}
			case EURASIA_USE1_OP_SOP2:
			{
				uSrcsUsedMask = 6;
				bShortPredicate = IMG_TRUE;
				break;
			}
			case EURASIA_USE1_OP_SOPWM:
			{
				IMG_UINT32	uWriteMask;

				uSrcsUsedMask = 6;

				/*
					Convert the destination mask to BGRA order.
				*/
				uWriteMask = (uInst1 & ~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK) >> EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT;
				uDestMask = 0;
				if (uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_B) uDestMask |= 1;
				if (uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_G) uDestMask |= 2;
				if (uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_R) uDestMask |= 4;
				if (uWriteMask & EURASIA_USE1_SOP2WM_WRITEMASK_A) uDestMask |= 8;

				bShortPredicate = IMG_TRUE;
				break;
			}
			case EURASIA_USE1_OP_FPMA:
			case EURASIA_USE1_OP_SOP3:
			{
				uSrcsUsedMask = 7;
				bShortPredicate = IMG_TRUE;
				break;
			}
			case EURASIA_USE1_OP_MOVC:
			{
				IMG_UINT32	uTestType;

				uTestType = (uInst1 & ~EURASIA_USE1_MOVC_TSTDTYPE_CLRMSK) >> EURASIA_USE1_MOVC_TSTDTYPE_SHIFT;
				if (uTestType == EURASIA_USE1_MOVC_TSTDTYPE_UNCOND)
				{
					uSrcsUsedMask = 2;
				}
				else
				{
					uSrcsUsedMask = 7;
				}
				bShortPredicate = IMG_FALSE;
				break;
			}
			default:
			{
				/*
					Give up if the opcode is unknown since we don't know what sources it uses.
				*/
				return IMG_FALSE;
			}
		}

		for (uSrc = 0; uSrc < 3; uSrc++)
		{
			IMG_UINT32	uSrcBank;
			IMG_UINT32	uSrcBankExt;
			IMG_UINT32	uSrcNum;

			if ((uSrcsUsedMask & (1UL << uSrc)) == 0)
			{
				continue;
			}

			/*
				Extract the bank and register number for this argument.
			*/
			GetSource(uInst0, uInst1, uSrc, &uSrcBankExt, &uSrcBank, &uSrcNum);

			/*
				Compare with the register to be replaced.
			*/
			if (
					(
						(uSrcBank == EURASIA_USE0_S1STDBANK_TEMP && uSrcBankExt == 0 && bOldRegIsTemp) ||
						(uSrcBank == EURASIA_USE0_S1STDBANK_PRIMATTR && uSrcBankExt == 0 && !bOldRegIsTemp)
					) &&
					uSrcNum == uOldRegNum
			   )
			{
				if (bDoReplace)
				{
					/*
						Substitute the replacement register.
					*/
					ReplaceArgument(&puReplaceCode[uInstIdx * 2 + 0],
									&puReplaceCode[uInstIdx * 2 + 1],
									uSrc,
									eNewRegType,
									uNewRegNum);
				}
				else
				{
					/*
						We can't use a secondary attribute in hardware source 0.
					*/
					if (
							eNewRegType == REGISTER_TYPE_SECATTR &&
							uSrc == 0
					   )
					{
						return IMG_FALSE;
					}

					/*
						If the replacement register has been overwritten then we can't do the replacement.
					*/
					if (bNewRegOverwritten)
					{
						return IMG_FALSE;
					}
				}
			}
		}

		{
			REGISTER_TYPE	eDestType;
			IMG_UINT32		uDestNum;
			IMG_UINT32		uEPred;

			/*
				Get the instruction predicate.
			*/
			if (bShortPredicate)
			{
				IMG_UINT32	uSPred;

				uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;
				switch (uSPred)
				{
					case EURASIA_USE1_SPRED_ALWAYS: uEPred = EURASIA_USE1_EPRED_ALWAYS; break;
					case EURASIA_USE1_SPRED_P0: uEPred = EURASIA_USE1_EPRED_P0; break;
					case EURASIA_USE1_SPRED_P1: uEPred = EURASIA_USE1_EPRED_P1; break;
					case EURASIA_USE1_SPRED_NOTP0: uEPred = EURASIA_USE1_EPRED_NOTP0; break;
					default: PVR_DBG_BREAK; uEPred = 0; break;
				}
			}
			else
			{
				uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
			}

			/*
				Get the destination register.
			*/
			GetDestination(uInst0, uInst1, &eDestType, &uDestNum);

			/*
				If we are completely overwriting the register to be replaced then we have checked all the
				uses of it. If we are conditionally or partially writing it then we can't do the replacement.
			*/
			if (
					(
						(eDestType == REGISTER_TYPE_TEMP && bOldRegIsTemp) ||
						(eDestType == REGISTER_TYPE_PRIMATTR && !bOldRegIsTemp)
					) &&
					uDestNum == uOldRegNum
			   )
			{
				if (uDestMask != 0xF || uEPred != EURASIA_USE1_EPRED_ALWAYS)
				{
					return IMG_FALSE;
				}
				else
				{
					return IMG_TRUE;
				}
			}

			/*
				Check for overwriting the replacement register.
			*/
			if (
					eDestType == eNewRegType &&
					uDestNum == uNewRegNum
			   )
			{
				bNewRegOverwritten = IMG_TRUE;
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL CheckForIntegerMultiply(IMG_UINT32	uInst0,
										IMG_UINT32	uInst1)
/*********************************************************************************
 Function		: CheckForIntegerMultiply

 Description	: Check for an instruction which does an integer multiply of
				  source 1 and source2.
 
 Parameters		: uInst0, uInst1		- Instruction to check.

 Return			: TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32	uOp;

	/*
		Get the opcode.
	*/
	uOp = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;

	if (uOp == EURASIA_USE1_OP_SOPWM)
	{
		IMG_UINT32	uSel1, uSel2;
		IMG_UINT32	uMod1, uMod2;
		IMG_UINT32	uWriteMask;
		IMG_UINT32	uSPred;
		IMG_UINT32	uCOp;
		IMG_UINT32	uAOp;

		uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;

		uWriteMask = (uInst1 & ~EURASIA_USE1_SOP2WM_WRITEMASK_CLRMSK) >> EURASIA_USE1_SOP2WM_WRITEMASK_SHIFT;

		uSel1 = (uInst1 & ~EURASIA_USE1_SOP2WM_SEL1_CLRMSK) >> EURASIA_USE1_SOP2WM_SEL1_SHIFT;
		uSel2 = (uInst1 & ~EURASIA_USE1_SOP2WM_SEL2_CLRMSK) >> EURASIA_USE1_SOP2WM_SEL2_SHIFT;
		uMod1 = (uInst1 & ~EURASIA_USE1_SOP2WM_MOD1_CLRMSK) >> EURASIA_USE1_SOP2WM_MOD1_SHIFT;
		uMod2 = (uInst1 & ~EURASIA_USE1_SOP2WM_MOD2_CLRMSK) >> EURASIA_USE1_SOP2WM_MOD2_SHIFT;
		uCOp = (uInst1 & ~EURASIA_USE1_SOP2WM_COP_CLRMSK) >> EURASIA_USE1_SOP2WM_COP_SHIFT;
		uAOp = (uInst1 & ~EURASIA_USE1_SOP2WM_AOP_CLRMSK) >> EURASIA_USE1_SOP2WM_AOP_SHIFT;

		if (
				uSPred == EURASIA_USE1_SPRED_ALWAYS &&
				uWriteMask == EURASIA_USE1_SOP2WM_WRITEMASK_RGBA &&
				uSel1 == EURASIA_USE1_SOP2WM_SEL1_SRC2 &&
				uSel2 == EURASIA_USE1_SOP2WM_SEL2_ZERO &&
				uMod1 == EURASIA_USE1_SOP2WM_MOD1_NONE &&
				uMod2 == EURASIA_USE1_SOP2WM_MOD2_NONE &&
				uCOp == EURASIA_USE1_SOP2WM_COP_ADD &&
				uAOp == EURASIA_USE1_SOP2WM_AOP_ADD
		   )
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	else if (uOp == EURASIA_USE1_OP_SOP2)
	{
		IMG_UINT32	uCMod1 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD1_CLRMSK) >> EURASIA_USE1_SOP2_CMOD1_SHIFT;
		IMG_UINT32	uCMod2 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD2_CLRMSK) >> EURASIA_USE1_SOP2_CMOD2_SHIFT;
		IMG_UINT32	uCSel1 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL1_CLRMSK) >> EURASIA_USE1_SOP2_CSEL1_SHIFT;
		IMG_UINT32	uCSel2 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL2_CLRMSK) >> EURASIA_USE1_SOP2_CSEL2_SHIFT;
		IMG_UINT32	uASel1 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL1_CLRMSK) >> EURASIA_USE1_SOP2_ASEL1_SHIFT;
		IMG_UINT32	uASel2 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL2_CLRMSK) >> EURASIA_USE1_SOP2_ASEL2_SHIFT;
		IMG_UINT32	uAMod1 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD1_CLRMSK) >> EURASIA_USE1_SOP2_AMOD1_SHIFT;
		IMG_UINT32	uAMod2 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD2_CLRMSK) >> EURASIA_USE1_SOP2_AMOD2_SHIFT;
		IMG_UINT32	uSrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_SRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_SRC1MOD_SHIFT;
		IMG_UINT32	uASrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_ASRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_ASRC1MOD_SHIFT;
		IMG_UINT32	uCOp = (uInst0 & ~EURASIA_USE0_SOP2_COP_CLRMSK) >> EURASIA_USE0_SOP2_COP_SHIFT;
		IMG_UINT32	uAOp = (uInst0 & ~EURASIA_USE0_SOP2_AOP_CLRMSK) >> EURASIA_USE0_SOP2_AOP_SHIFT;
		IMG_UINT32	uADstMod = (uInst0 & ~EURASIA_USE0_SOP2_ADSTMOD_CLRMSK) >> EURASIA_USE0_SOP2_ADSTMOD_SHIFT;
		IMG_UINT32	uRCntSel = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
		IMG_UINT32	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;

		/*
			Ignore repeated instructions.
		*/
		if (uRCntSel > 0)
		{
			return IMG_FALSE;
		}

		/*
			Ignore predicated instructions.
		*/
		if (uSPred != EURASIA_USE1_SPRED_ALWAYS)
		{
			return IMG_FALSE;
		}

		if (
				/* RGB = SRC2 * SRC1 + ZERO * SRC2 */
				uCSel1 == EURASIA_USE1_SOP2_CSEL1_SRC2 &&
				uCMod1 == EURASIA_USE1_SOP2_CMOD1_NONE &&
				uCSel2 == EURASIA_USE1_SOP2_CSEL2_ZERO &&
				uCMod2 == EURASIA_USE1_SOP2_CMOD2_NONE &&
				uSrc1Mod == EURASIA_USE0_SOP2_SRC1MOD_NONE &&
				uCOp == EURASIA_USE0_SOP2_COP_ADD &&
				/* A = SRC2 * SRC1 + ZERO * SRC2 */
				uASel1 == EURASIA_USE1_SOP2_ASEL1_SRC2ALPHA &&
				uAMod1 == EURASIA_USE1_SOP2_AMOD1_NONE &&
				uASel2 == EURASIA_USE1_SOP2_ASEL2_ZERO &&
				uAMod2 == EURASIA_USE1_SOP2_AMOD2_NONE &&
				uASrc1Mod == EURASIA_USE0_SOP2_ASRC1MOD_NONE &&
				uADstMod == EURASIA_USE0_SOP2_ADSTMOD_NONE &&
				uAOp == EURASIA_USE0_SOP2_AOP_ADD
		   )
		{
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CheckForIntegerAdd(IMG_UINT32	uInst0,
								   IMG_UINT32	uInst1)
/*********************************************************************************
 Function		: CheckForIntegerAdd

 Description	: Check for an instruction which does an integer add of
				  source 1 and source2.
 
 Parameters		: uInst0, uInst1		- Instruction to check.

 Return			: TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32	uOp;

	uOp = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;

	if (uOp == EURASIA_USE1_OP_SOP2)
	{
		IMG_UINT32	uCOp, uAOp, uCSrcMod1, uASrcMod1, uADstMod, uCSel1, uCSel2;
		IMG_UINT32	uCMod1, uCMod2, uASel1, uASel2, uAMod1, uAMod2;
		IMG_UINT32	uRCntSel = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
		IMG_UINT32	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;

		/*
			Ignore repeated instructions.
		*/
		if (uRCntSel > 0)
		{
			return IMG_FALSE;
		}

		/*
			Ignore predicated instructions.
		*/
		if (uSPred != EURASIA_USE1_SPRED_ALWAYS)
		{
			return IMG_FALSE;
		}

		uCOp = (uInst0 & ~EURASIA_USE0_SOP2_COP_CLRMSK) >> EURASIA_USE0_SOP2_COP_SHIFT;
		uAOp = (uInst0 & ~EURASIA_USE0_SOP2_AOP_CLRMSK) >> EURASIA_USE0_SOP2_AOP_SHIFT;

		uCSrcMod1 = (uInst0 & ~EURASIA_USE0_SOP2_SRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_SRC1MOD_SHIFT;
		uASrcMod1 = (uInst0 & ~EURASIA_USE0_SOP2_ASRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_ASRC1MOD_SHIFT;

		uADstMod = (uInst0 & ~EURASIA_USE0_SOP2_ADSTMOD_CLRMSK) >> EURASIA_USE0_SOP2_ADSTMOD_SHIFT;

		uCSel1 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL1_CLRMSK) >> EURASIA_USE1_SOP2_CSEL1_SHIFT;
		uCSel2 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL2_CLRMSK) >> EURASIA_USE1_SOP2_CSEL2_SHIFT;

		uCMod1 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD1_CLRMSK) >> EURASIA_USE1_SOP2_CMOD1_SHIFT;
		uCMod2 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD2_CLRMSK) >> EURASIA_USE1_SOP2_CMOD2_SHIFT;

		uASel1 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL1_CLRMSK) >> EURASIA_USE1_SOP2_ASEL1_SHIFT;
		uASel2 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL2_CLRMSK) >> EURASIA_USE1_SOP2_ASEL2_SHIFT;

		uAMod1 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD1_CLRMSK) >> EURASIA_USE1_SOP2_AMOD1_SHIFT;
		uAMod2 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD2_CLRMSK) >> EURASIA_USE1_SOP2_AMOD2_SHIFT;

		/*
			Check:
				RGB = (1 - ZERO) * SRC1 + (1 - ZERO) * SRC2
				A = (1 - ZERO) * SRC1 + (1 - ZERO) * SRC2
		*/
		if (
				uCOp == EURASIA_USE0_SOP2_COP_ADD &&
				uAOp == EURASIA_USE0_SOP2_AOP_ADD &&
				uCSrcMod1 == EURASIA_USE0_SOP2_SRC1MOD_NONE &&
				uASrcMod1 == EURASIA_USE0_SOP2_ASRC1MOD_NONE &&
				uADstMod == EURASIA_USE0_SOP2_ADSTMOD_NONE &&
				uCSel1 == EURASIA_USE1_SOP2_CSEL1_ZERO &&
				uCMod1 == EURASIA_USE1_SOP2_CMOD1_COMPLEMENT &&
				uCSel2 == EURASIA_USE1_SOP2_CSEL2_ZERO &&
				uCMod2 == EURASIA_USE1_SOP2_CMOD2_COMPLEMENT &&
				uASel1 == EURASIA_USE1_SOP2_ASEL1_ZERO &&
				uAMod1 == EURASIA_USE1_SOP2_AMOD1_COMPLEMENT &&
				uASel2 == EURASIA_USE1_SOP2_ASEL2_ZERO &&
				uAMod2 == EURASIA_USE1_SOP2_AMOD2_COMPLEMENT
		   )
		{
			return IMG_TRUE;
		}
		return IMG_FALSE;
	}
	return IMG_FALSE;
}

/*********************************************************************************
 Function		: CombineSOPs

 Description	: Try to combine an integer multiply and an integer add into a
				  single instruction.

 Parameters		: uShaderInst0, uShaderInst1	- Possible integer multiply.
				  ui32BlendCodeCount			- Blend code possibly doing an
				  pui32BlendCode				  an integer add.
				  pui32OutputCode				- On success written with a
												single instruction doing a
												multiply-add.

 Return			: TRUE or FALSE.
*********************************************************************************/
static IMG_BOOL CombineSOPs(IMG_UINT32		uShaderInst0,
							IMG_UINT32		uShaderInst1,
							IMG_UINT32		ui32BlendCodeCount,
							IMG_PUINT32		pui32BlendCode,
							IMG_PUINT32		pui32OutputCode)
{
	if (CheckForIntegerMultiply(uShaderInst0, uShaderInst1))
	{
		IMG_UINT32	auMulSrcBankExt[2], auMulSrcBank[2], auMulSrcNum[2];
		IMG_UINT32	uDestBank, uDestNum, uDestBankExt;
		IMG_UINT32	uSrc;
		IMG_UINT32	uMulSrc0;
		IMG_UINT32	uAddInst0, uAddInst1;
		IMG_UINT32	uMatchingSrcs;
		IMG_UINT32	auAddSrcBankExt[2], auAddSrcBank[2], auAddSrcNum[2];
		IMG_BOOL	bDestIsTemp;
		IMG_UINT32	uAddOtherSrc;

		/*
			Get the multiply destination.
		*/
		uDestBank = (uShaderInst1 & ~EURASIA_USE1_D1BANK_CLRMSK) >> EURASIA_USE1_D1BANK_SHIFT;
		uDestNum = (uShaderInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
		uDestBankExt = uShaderInst1 & EURASIA_USE1_DBEXT;

		uMulSrc0 = 0xFFFFFFFF;
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			GetSource(uShaderInst0, uShaderInst1, uSrc + 1, &auMulSrcBankExt[uSrc], &auMulSrcBank[uSrc], &auMulSrcNum[uSrc]);

			/*
				Check that at least one of the sources to the multiply instruction can
				use source 0.
			*/
			if (
					uMulSrc0 == 0xFFFFFFFF &&
					auMulSrcBankExt[uSrc] == 0 &&
					(
						auMulSrcBank[uSrc] == EURASIA_USE0_S1STDBANK_TEMP ||
						auMulSrcBank[uSrc] == EURASIA_USE0_S1STDBANK_PRIMATTR
					)
			   )
			{
				uMulSrc0 = uSrc;
			}
		}
		if (uMulSrc0 == 0xFFFFFFFF)
		{
			return IMG_FALSE;
		}

		/*
			Check the multiply is writing to a temporary register or a
			primary attribute.
		*/
		if (
				!(
					uDestBankExt == 0 &&
					(
						uDestBank == EURASIA_USE1_D1STDBANK_TEMP || 
						uDestBank == EURASIA_USE1_D1STDBANK_PRIMATTR
					)
				 )
		   )
		{
			return IMG_FALSE;
		}
		if (uDestBank == EURASIA_USE1_D1STDBANK_TEMP)
		{
			bDestIsTemp = IMG_TRUE;
		}
		else
		{
			bDestIsTemp = IMG_FALSE;
		}

		/*
			This isn't strictly necessary but it saves us from having to check that uses of the mul
			destination in the add are the last uses.
		*/
		if (ui32BlendCodeCount != 1)
		{
			return IMG_FALSE;
		}

		uAddInst0 = pui32BlendCode[0];
		uAddInst1 = pui32BlendCode[1];

		if ((uAddInst1 & EURASIA_USE1_SKIPINV) != (uShaderInst1 & EURASIA_USE1_SKIPINV))
		{
			return IMG_FALSE;
		}

		/*
			Check the blending instruction is an integer add.
		*/
		if (!CheckForIntegerAdd(uAddInst0, uAddInst1))
		{
			return IMG_FALSE;
		}

		/*
			Check which sources to the add instruction come from the 
			multiply instruction.
		*/
		uMatchingSrcs = 0;
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			GetSource(uAddInst0, uAddInst1, uSrc + 1, &auAddSrcBankExt[uSrc], &auAddSrcBank[uSrc], &auAddSrcNum[uSrc]);

			if (
					auAddSrcBankExt[uSrc] == 0 &&
					(
						(bDestIsTemp && auAddSrcBank[uSrc] == EURASIA_USE0_S1STDBANK_TEMP) ||
						(!bDestIsTemp && auAddSrcBank[uSrc] == EURASIA_USE0_S1STDBANK_PRIMATTR)
					) &&
					auAddSrcNum[uSrc] == uDestNum
			   )
			{
				uMatchingSrcs |= (1UL << uSrc);
			}
		}
		if (!(uMatchingSrcs == 1 || uMatchingSrcs == 2))
		{
			return IMG_FALSE;
		}
		if (uMatchingSrcs == 1)
		{
			uAddOtherSrc = 1;
		}
		else
		{
			uAddOtherSrc = 0;
		}

		/*
			Set up a SOP3 instruction.
		*/
		pui32OutputCode[1]	=	(EURASIA_USE1_OP_SOP3		<<	EURASIA_USE1_OP_SHIFT) |
								(EURASIA_USE1_SPRED_ALWAYS	<<	EURASIA_USE1_SPRED_SHIFT) |
								(uShaderInst1 & EURASIA_USE1_SKIPINV) |
								(uAddInst1 & EURASIA_USE1_DBEXT) |
								(uAddInst1 & EURASIA_USE1_END) |
								(uAddInst1 & ~EURASIA_USE1_D1BANK_CLRMSK);
		pui32OutputCode[0]	=	(uAddInst0 & ~EURASIA_USE0_DST_CLRMSK);

		/*
			RGB = SRC0 * SRC1 + (1 - ZERO) * SRC2
			A = SRC0 * SRC1 + (1 - ZERO) * SRC2
		*/
		pui32OutputCode[1]	|=	(EURASIA_USE1_SOP3_CMOD1_NONE		<<	EURASIA_USE1_SOP3_CMOD1_SHIFT) |
								(EURASIA_USE1_SOP3_COP_ADD			<<	EURASIA_USE1_SOP3_COP_SHIFT) |
								(EURASIA_USE1_SOP3_CMOD2_COMPLEMENT	<<	EURASIA_USE1_SOP3_CMOD2_SHIFT) |
								(EURASIA_USE1_SOP3_AMOD1_NONE		<<	EURASIA_USE1_SOP3_AMOD1_SHIFT) |
								(EURASIA_USE1_SOP3_ASEL1_SRC0ALPHA	<<	EURASIA_USE1_SOP3_ASEL1_SHIFT) |
								(EURASIA_USE1_SOP3_DESTMOD_NONE		<<	EURASIA_USE1_SOP3_DESTMOD_SHIFT) |
								(EURASIA_USE1_SOP3_AOP_ADD			<<	EURASIA_USE1_SOP3_AOP_SHIFT) |
								(EURASIA_USE1_SOP3_CSEL1_SRC0		<<	EURASIA_USE1_SOP3_CSEL1_SHIFT) |
								(EURASIA_USE1_SOP3_CSEL2_ZERO		<<	EURASIA_USE1_SOP3_CSEL2_SHIFT);
		
		/*
			Source 2 comes from the source to the ADD that wasn't the same as the MUL destination.
		*/
		pui32OutputCode[1]	|=	(auAddSrcBankExt[uAddOtherSrc] ? EURASIA_USE1_S2BEXT : 0);
		pui32OutputCode[0]	|=	(auAddSrcBank[uAddOtherSrc]		<<	EURASIA_USE0_S2BANK_SHIFT) |
								(auAddSrcNum[uAddOtherSrc]		<<	EURASIA_USE0_SRC2_SHIFT);

		/*
			Source 1 comes from one of the sources to MUL
		*/
		pui32OutputCode[1]	|=	(auMulSrcBankExt[1 - uMulSrc0] ? EURASIA_USE1_S1BEXT : 0);
		pui32OutputCode[0]	|=	(auMulSrcBank[1 - uMulSrc0]			<<	EURASIA_USE0_S1BANK_SHIFT) |
								(auMulSrcNum[1 - uMulSrc0]			<<	EURASIA_USE0_SRC1_SHIFT);

		/*
			...and source 0 from the other one.
		*/
		if (auMulSrcBank[uMulSrc0] == EURASIA_USE0_S1STDBANK_TEMP)
		{
			pui32OutputCode[1]	|=	(EURASIA_USE1_S0STDBANK_TEMP		<<	EURASIA_USE1_S0BANK_SHIFT);
		}
		else
		{
			pui32OutputCode[1]	|=	(EURASIA_USE1_S0STDBANK_PRIMATTR	<<	EURASIA_USE1_S0BANK_SHIFT);
		}
		pui32OutputCode[0]		|=	(auMulSrcNum[uMulSrc0]				<<	EURASIA_USE0_SRC1_SHIFT);

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*********************************************************************************
 Function		: CheckForMove

 Description	: Check for an instruction which is effectively a move from source 1.

 Parameters		: uInst0, uInst1		- Instruction to check.

 Return			: TRUE or FALSE.
*********************************************************************************/
static IMG_UINT32 CheckForMove(IMG_UINT32	uInst0,
							   IMG_UINT32	uInst1)
{
	IMG_UINT32	uOp;

	/*
		Get the opcode of the instruction which writes the pixel shader result.
	*/
	uOp = (uInst1 & ~EURASIA_USE1_OP_CLRMSK) >> EURASIA_USE1_OP_SHIFT;

	if (uOp == EURASIA_USE1_OP_SOP2)
	{
		IMG_UINT32	uCMod1 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD1_CLRMSK) >> EURASIA_USE1_SOP2_CMOD1_SHIFT;
		IMG_UINT32	uCMod2 = (uInst1 & ~EURASIA_USE1_SOP2_CMOD2_CLRMSK) >> EURASIA_USE1_SOP2_CMOD2_SHIFT;
		IMG_UINT32	uCSel1 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL1_CLRMSK) >> EURASIA_USE1_SOP2_CSEL1_SHIFT;
		IMG_UINT32	uCSel2 = (uInst1 & ~EURASIA_USE1_SOP2_CSEL2_CLRMSK) >> EURASIA_USE1_SOP2_CSEL2_SHIFT;
		IMG_UINT32	uASel1 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL1_CLRMSK) >> EURASIA_USE1_SOP2_ASEL1_SHIFT;
		IMG_UINT32	uASel2 = (uInst1 & ~EURASIA_USE1_SOP2_ASEL2_CLRMSK) >> EURASIA_USE1_SOP2_ASEL2_SHIFT;
		IMG_UINT32	uAMod1 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD1_CLRMSK) >> EURASIA_USE1_SOP2_AMOD1_SHIFT;
		IMG_UINT32	uAMod2 = (uInst1 & ~EURASIA_USE1_SOP2_AMOD2_CLRMSK) >> EURASIA_USE1_SOP2_AMOD2_SHIFT;
		IMG_UINT32	uSrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_SRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_SRC1MOD_SHIFT;
		IMG_UINT32	uASrc1Mod = (uInst0 & ~EURASIA_USE0_SOP2_ASRC1MOD_CLRMSK) >> EURASIA_USE0_SOP2_ASRC1MOD_SHIFT;
		IMG_UINT32	uCOp = (uInst0 & ~EURASIA_USE0_SOP2_COP_CLRMSK) >> EURASIA_USE0_SOP2_COP_SHIFT;
		IMG_UINT32	uAOp = (uInst0 & ~EURASIA_USE0_SOP2_AOP_CLRMSK) >> EURASIA_USE0_SOP2_AOP_SHIFT;
		IMG_UINT32	uADstMod = (uInst0 & ~EURASIA_USE0_SOP2_ADSTMOD_CLRMSK) >> EURASIA_USE0_SOP2_ADSTMOD_SHIFT;
		IMG_UINT32	uRCntSel = (uInst1 & ~EURASIA_USE1_INT_RCOUNT_CLRMSK) >> EURASIA_USE1_INT_RCOUNT_SHIFT;
		IMG_UINT32	uSPred = (uInst1 & ~EURASIA_USE1_SPRED_CLRMSK) >> EURASIA_USE1_SPRED_SHIFT;

		/*
			Ignore repeated instructions.
		*/
		if (uRCntSel > 0)
		{
			return IMG_FALSE;
		}

		/*
			Ignore predicated instructions.
		*/
		if (uSPred != EURASIA_USE1_SPRED_ALWAYS)
		{
			return IMG_FALSE;
		}

		if (
				/* RGB = (1 - ZERO) * SRC1 + ZERO * SRC2 */
				uCSel1 == EURASIA_USE1_SOP2_CSEL1_ZERO &&
				uCMod1 == EURASIA_USE1_SOP2_CMOD1_COMPLEMENT &&
				uCSel2 == EURASIA_USE1_SOP2_CSEL2_ZERO &&
				uCMod2 == EURASIA_USE1_SOP2_CMOD2_NONE &&
				uSrc1Mod == EURASIA_USE0_SOP2_SRC1MOD_NONE &&
				uCOp == EURASIA_USE0_SOP2_COP_ADD &&
				/* A = (1 - ZERO) * SRC1 + ZERO * SRC2 */
				uASel1 == EURASIA_USE1_SOP2_ASEL1_ZERO &&
				uAMod1 == EURASIA_USE1_SOP2_AMOD1_COMPLEMENT &&
				uASel2 == EURASIA_USE1_SOP2_ASEL2_ZERO &&
				uAMod2 == EURASIA_USE1_SOP2_AMOD2_NONE &&
				uASrc1Mod == EURASIA_USE0_SOP2_ASRC1MOD_NONE &&
				uADstMod == EURASIA_USE0_SOP2_ADSTMOD_NONE &&
				uAOp == EURASIA_USE0_SOP2_AOP_ADD
		   )
		{
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}
	if (uOp == EURASIA_USE1_OP_MOVC)
	{
		IMG_UINT32	uTestType;
		IMG_UINT32	uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;

		/*
			Check for an unconditional move.
		*/
		uTestType = (uInst1 & ~EURASIA_USE1_MOVC_TSTDTYPE_CLRMSK) >> EURASIA_USE1_MOVC_TSTDTYPE_SHIFT;

		if (
				uTestType == EURASIA_USE1_MOVC_TSTDTYPE_UNCOND &&
				uEPred == EURASIA_USE1_EPRED_ALWAYS
		   )
		{
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}

#if defined(SGX_FEATURE_USE_VEC34)
	if (uOp == EURASIA_USE1_OP_ANDOR)
	{
		IMG_UINT32 uEPred = (uInst1 & ~EURASIA_USE1_EPRED_CLRMSK) >> EURASIA_USE1_EPRED_SHIFT;
		IMG_UINT32 uRCnt = (uInst1 & ~SGXVEC_USE1_BITWISE_RCOUNT_CLRMSK) >> SGXVEC_USE1_BITWISE_RCOUNT_SHIFT;

		/*
			Ignore repeated instructions.
		*/
		if (uRCnt > 0)
		{
			return IMG_FALSE;
		}

		/*
			Ignore predicated instructions.
		*/
		if (uEPred != EURASIA_USE1_SPRED_ALWAYS)
		{
			return IMG_FALSE;
		}

		/*
			Second source must be #0.
		*/
		if( ((uInst1 & EURASIA_USE1_S2BEXT) == 0) || 
			((uInst0 & ~EURASIA_USE0_S2BANK_CLRMSK) != (EURASIA_USE0_S2EXTBANK_IMMEDIATE << EURASIA_USE0_S2BANK_SHIFT)) ||
			((uInst1 & ~EURASIA_USE1_BITWISE_SRC2IEXTH_CLRMSK) != 0) ||
			((uInst0 & ~(EURASIA_USE0_BITWISE_SRC2IEXTLPSEL_CLRMSK & EURASIA_USE0_SRC2_CLRMSK)) != 0)	)
		{
			return IMG_FALSE;
		}

		/* Either AND with #~0 or OR with #0 */
		if((uInst1 & ~EURASIA_USE1_BITWISE_OP2_CLRMSK) == EURASIA_USE1_BITWISE_OP2_AND)
		{
			if((uInst1 & EURASIA_USE1_BITWISE_SRC2INV) == 0)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			if(uInst1 & EURASIA_USE1_BITWISE_SRC2INV)
			{
				return IMG_FALSE;
			}
		}

		return IMG_TRUE;
	}
#endif

	return IMG_FALSE;
}

/*********************************************************************************
 Function		: PVRCombineBlendCodeWithShader

 Description	: Combine pixel post-processing code together with the pixel shader code.

 Parameters		: pui32Output				- Array which will contain the combined code on return.
				  ui32ShaderCodeCount		- Count of instructions in the pixel shader code.
				  pui32ShaderCode			- Points to the pixel shader instructions.
				  ui32BlendCodeCount		- Count of instructions in the blend code.
				  pui32BlendCode			- Points to the blending instructions.
				  bMultipleShaderOutputs	- TRUE if the pixel shader writes its result in
											multiple places or not in the last instruction.
				  bC10FormatControlOn		- TRUE if the C10 format control is on at the
											last instruction of the shader.

 Return			: The number of instructions in the combined code.
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 PVRCombineBlendCodeWithShader(IMG_PUINT32			pui32Output,
										 IMG_UINT32				ui32ShaderCodeCount,
										 IMG_PUINT32			pui32ShaderCode,
										 IMG_UINT32				ui32BlendCodeCount,
										 IMG_PUINT32			pui32BlendCode,
										 IMG_BOOL				bMultipleShaderOutputs,
										 IMG_BOOL				bC10FormatControlOn)
{
	IMG_UINT32	uInst0, uInst1;
	IMG_PUINT32	puBlendStart;

	PVR_UNREFERENCED_PARAMETER(bC10FormatControlOn);

	if (!bMultipleShaderOutputs && ui32BlendCodeCount > 0)
	{
		uInst0 = pui32ShaderCode[(ui32ShaderCodeCount - 1) * 2 + 0];
		uInst1 = pui32ShaderCode[(ui32ShaderCodeCount - 1) * 2 + 1];

		/*
			Check if its possible to combine an integer multiple at the end of the shader code with an integer
			add in the blend code.
		*/
		if (CombineSOPs(uInst0, uInst1, ui32BlendCodeCount, pui32BlendCode, pui32Output + (ui32ShaderCodeCount - 1) * 2))
		{
			/*
				CombineSOPs has already set up an integer multiply-add in the last instruction of the output so copy
				the rest of the shader code in behind it.
			*/
			memcpy(pui32Output, pui32ShaderCode, (ui32ShaderCodeCount - 1) * EURASIA_USE_INSTRUCTION_SIZE);
			return ui32ShaderCodeCount;
		}

		if (CheckForMove(uInst0, uInst1))
		{
			IMG_UINT32	uDestBank;
			IMG_UINT32	uDestNum;
			IMG_UINT32	uDestBankExt;

			/*
				Get the destination register.
			*/
			uDestBank = (uInst1 & ~EURASIA_USE1_D1BANK_CLRMSK) >> EURASIA_USE1_D1BANK_SHIFT;
			uDestNum = (uInst0 & ~EURASIA_USE0_DST_CLRMSK) >> EURASIA_USE0_DST_SHIFT;
			uDestBankExt = uInst1 & EURASIA_USE1_DBEXT;

			if (
					uDestBankExt == 0 &&
					(
						uDestBank == EURASIA_USE1_D1STDBANK_TEMP || 
						uDestBank == EURASIA_USE1_D1STDBANK_PRIMATTR
					)
				)
			{
				IMG_UINT32		uSrcBank;
				IMG_UINT32		uSrcNum;
				IMG_UINT32		uSrcBankExt;
				REGISTER_TYPE	eSrcType;

				uSrcBank = (uInst0 & ~EURASIA_USE0_S1BANK_CLRMSK) >> EURASIA_USE0_S1BANK_SHIFT;
				uSrcNum = (uInst0 & ~EURASIA_USE0_SRC1_CLRMSK) >> EURASIA_USE0_SRC1_SHIFT;
				uSrcBankExt = uInst1 & EURASIA_USE1_S1BEXT;

				eSrcType = REGISTER_TYPE_UNKNOWN;
				if (uSrcBankExt == 0)
				{
					switch (uSrcBank)
					{
						case EURASIA_USE0_S1STDBANK_TEMP: eSrcType = REGISTER_TYPE_TEMP; break;
						case EURASIA_USE0_S1STDBANK_PRIMATTR: eSrcType = REGISTER_TYPE_PRIMATTR; break;
						case EURASIA_USE0_S1STDBANK_SECATTR: eSrcType = REGISTER_TYPE_SECATTR; break;
					}
				}

				if (eSrcType != REGISTER_TYPE_UNKNOWN)
				{
					/*
						Can we get rid of the move by replacing uses of the result in
						the blending code.
					*/
					if (ReplaceArguments(ui32BlendCodeCount,
										 pui32BlendCode,
										 (IMG_BOOL)(uDestBank == EURASIA_USE1_D1STDBANK_TEMP),
										 uDestNum,
										 eSrcType,
										 uSrcNum,
										 IMG_FALSE))
					{
						/*
							Combine the two code blocks overwriting the last instruction of the pixel shader.
						*/
						memcpy(pui32Output, pui32ShaderCode, (ui32ShaderCodeCount - 1) * EURASIA_USE_INSTRUCTION_SIZE);
						puBlendStart = pui32Output + (((ui32ShaderCodeCount - 1) * EURASIA_USE_INSTRUCTION_SIZE) >> 2);
						memcpy(puBlendStart, pui32BlendCode, ui32BlendCodeCount * EURASIA_USE_INSTRUCTION_SIZE);

						/*
							Replace uses of the move result in the blending code.
						*/
						ReplaceArguments(ui32BlendCodeCount,
										 puBlendStart,
										 (IMG_BOOL)(uDestBank == EURASIA_USE1_D1STDBANK_TEMP),
										 uDestNum,
										 eSrcType,
										 uSrcNum,
										 IMG_TRUE);

						return ui32ShaderCodeCount + ui32BlendCodeCount - 1;
					}
				}
			}
		}
		
	}

	/*
		Combine two code blocks without changing the instructions.
	*/
	memcpy(pui32Output, pui32ShaderCode, ui32ShaderCodeCount * EURASIA_USE_INSTRUCTION_SIZE);
	puBlendStart = pui32Output + ((ui32ShaderCodeCount * EURASIA_USE_INSTRUCTION_SIZE) >> 2);
	memcpy(puBlendStart, pui32BlendCode, ui32BlendCodeCount * EURASIA_USE_INSTRUCTION_SIZE);

	return ui32ShaderCodeCount + ui32BlendCodeCount;
}

/******************************************************************************
 End of file (combiner.c)
******************************************************************************/
