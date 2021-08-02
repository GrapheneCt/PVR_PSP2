/*************************************************************************
 * Name         : intcvt.c
 * Title        : Integer Optimizations
 * Created      : Feb 2006
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
 * $Log: intcvt.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"
#include "reggroup.h"
#include <limits.h>

/* Local declarations. */
typedef struct
{
	PARG			psMovDest;
	PARG			psMovSrc;
	IMG_UINT32		uMovDestComp;
	IMG_UINT32		uMovSrcComp;
	UF_REGFORMAT	eSrcFormat;
} INTEGER_COMPONENT_MOVE_CONTEXT;

static IMG_BOOL EliminateIntegerMoves(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbMovesGenerated);

/*********************************************************************************
 Function			: AddAlphaSwizzleC10Constant

 Description		: Add an instruction to the secondary update program to
					  swizzle a C10 constant.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- Instruction argument to be swizzled.
					  uArg
					  uChansToSwizzle	- Mask of channels used from the swizzle
										result.
					  bCheckOnly		- If FALSE add the instruction to the
										secondary update program and replace the
										instruction argument by the swizzled
										result.
										  If TRUE just check if swizzling the
									    argument is possible.

 Return				: TRUE if the secondary attribute can be added.
*********************************************************************************/
static
IMG_BOOL AddAlphaSwizzleC10Constant(PINTERMEDIATE_STATE		psState,
								    PINST					psInst,
									IMG_UINT32				uArg,
									IMG_UINT32				uChansToSwizzle,
									IMG_BOOL				bCheckOnly)
{
	PARG			psArg = &psInst->asArg[uArg];
	UF_REGFORMAT	eArgFmt = psArg->eFmt;

	ASSERT(eArgFmt == UF_REGFORMAT_C10 || eArgFmt == UF_REGFORMAT_U8);

	/*
		Check the argument is a constant.
	*/
	if (!IsDriverLoadedSecAttr(psState, psArg))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Check that there are secondary attributes available.
	*/
	if (!CheckAndUpdateInstSARegisterCount(psState, 2, 2, IMG_TRUE))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Check we are allowed to move calculations to the secondary update
		program.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) == 0)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
			
	/*
		If we aren't just checking then add an instruction to do the swizzle.
	*/
	if (!bCheckOnly)
	{
		PINST		psPackInst;
		IMG_UINT32	uNumResultRegisters;
		IMG_UINT32	uNumHwRegistersPerResult;
		IMG_UINT32	uIdx;

		/*
			Calculate the number of secondary attributes needed for the result of the
			swizzle.
		*/
		if (eArgFmt == UF_REGFORMAT_C10)
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
			{
				if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0)
				{
					/*
						Two 32-bit registers (to hold 4 x 10-bit channels). 
					*/
					uNumResultRegisters = SCALAR_REGISTERS_PER_C10_VECTOR;
					uNumHwRegistersPerResult = 1;
				}
				else
				{
					/*
						One 40-bit register (4 x 10-bit channels).
					*/
					uNumResultRegisters = 1;
					uNumHwRegistersPerResult = SCALAR_REGISTERS_PER_C10_VECTOR;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				if ((uChansToSwizzle & USC_ALPHA_CHAN_MASK) != 0)
				{
					/*
						One 40-bit register (4 x 10-bit channels).
					*/
					uNumResultRegisters = 1;
					uNumHwRegistersPerResult = SCALAR_REGISTERS_PER_C10_VECTOR;
				}
				else
				{
					/*
						One 30-bit register (3 x 10-bit channels).
					*/
					uNumResultRegisters = 1;
					uNumHwRegistersPerResult = SCALAR_REGISTERS_PER_C10_VEC3;
				}
			}
		}
		else
		{
			/*
				One 32-bit register (4 x 8-bit channels).
			*/
			uNumResultRegisters = uNumHwRegistersPerResult =  SCALAR_REGISTERS_PER_U8_VECTOR;
		}

		/*
			Add a pack instruction to the secondary program to do
			the swizzling.
		*/
		psPackInst = AllocateInst(psState, NULL);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0)
		{
			if (eArgFmt == UF_REGFORMAT_C10)
			{
				SetOpcodeAndDestCount(psState, psPackInst, IVPCKC10C10, uNumResultRegisters);
			}
			else
			{
				SetOpcode(psState, psPackInst, IVPCKU8U8);
			}
			SetSrcFromArg(psState, psPackInst, 0 /* uSrcIdx */, psArg + 0);
			SetSrcFromArg(psState, psPackInst, 1 /* uSrcIdx */, psArg + 1);
			psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W);
			psPackInst->u.psVec->aeSrcFmt[0] = eArgFmt;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			IMG_UINT32	uComponent;

			uComponent = USC_ALPHA_CHAN;
			if (eArgFmt == UF_REGFORMAT_C10)
			{
				SetOpcode(psState, psPackInst, IPCKC10C10);
				if ((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0)
				{
					uComponent = USC_RED_CHAN;
				}
			}
			else
			{
				SetOpcode(psState, psPackInst, IPCKU8U8);
			}
			SetSrcFromArg(psState, psPackInst, 0 /* uSrcIdx */, psArg);
			SetPCKComponent(psState, psPackInst, 0 /* uSrcIdx */, uComponent);
			SetSrcFromArg(psState, psPackInst, 1 /* uSrcIdx */, psArg);
			SetPCKComponent(psState, psPackInst, 1 /* uSrcIdx */, uComponent);
		}
		AppendInst(psState, psState->psSecAttrProg->sCfg.psExit, psPackInst);

		/*
			Allocate secondary attributes live out of the secondary program and into the main program to hold
			the swizzled result.
		*/
		for (uIdx = 0; uIdx < uNumResultRegisters; uIdx++)
		{
			IMG_UINT32	uTemp;

			/*
				Allocate a secondary attribute.
			*/
			uTemp = GetNextRegister(psState);
			BaseAddNewSAProgResult(psState,
								   eArgFmt,
								   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								   IMG_FALSE /* bVector */,
								   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								   0 /* uHwRegAlignment */,
								   uNumHwRegistersPerResult,
								   uTemp,
								   uChansToSwizzle,
								   SAPROG_RESULT_TYPE_CALC,
								   IMG_FALSE /* bPartOfRange */);

			/*
				Set the secondary attribute as a destination of the PCK instruction...
			*/
			SetDest(psState, psPackInst, uIdx, USEASM_REGTYPE_TEMP, uTemp, eArgFmt);
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0 && eArgFmt == UF_REGFORMAT_C10)
			{
				if (uIdx == 0)
				{
					psPackInst->auLiveChansInDest[uIdx] = uChansToSwizzle;
				}
				else
				{
					ASSERT(uIdx == 1);
					if ((uChansToSwizzle & USC_ALPHA_CHAN_MASK) != 0)
					{
						psPackInst->auLiveChansInDest[uIdx] = USC_RED_CHAN_MASK;
					}
					else
					{
						psPackInst->auLiveChansInDest[uIdx] = 0;
					}
				}
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				psPackInst->auLiveChansInDest[uIdx] = uChansToSwizzle;
			}

			/*
				...and a source of the instruction in the main program.
			*/
			SetSrc(psState, psInst, uArg + uIdx, USEASM_REGTYPE_TEMP, uTemp, eArgFmt);

			/*
				Set the channels live out of the secondary program.
			*/
			SetRegisterLiveMask(psState,
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut, 
								USEASM_REGTYPE_TEMP,
								uTemp,
								0 /* uArrayOffset */,
								psPackInst->auLiveChansInDest[uIdx]);
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0)
		{
			ASSERT(psPackInst->uDestCount == 2);
			MakeGroup(psState, psPackInst->asDest, 2 /* uArgCount */, HWREG_ALIGNMENT_EVEN);

			MakeGroup(psState, psPackInst->asArg, 2 /* uArgCount */, HWREG_ALIGNMENT_EVEN);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	/*
		Update maximum number of regs used by any instruction for cores 
		where temporary registers cannot be allocated for the secondary 
		update program.
	*/
	CheckAndUpdateInstSARegisterCount(psState, 2, 2, IMG_FALSE);

	return IMG_TRUE;
}

static
IMG_BOOL IsSOP3MultiplyAdd(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbSrc0AlphaSwizzle)
/*****************************************************************************
 FUNCTION	: IsSOP3MultiplyAdd

 PURPOSE	: Checks for a SOP3 instruction which calculates (per-channel):
				DEST = SRC0 * SRC1 + ONE * SRC2

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  pbSrc0AlphaSwizzle
								- Returns TRUE if source 0 has an alpha replicate
								swizzle.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PSOP3_PARAMS psSop3;

	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3);
	psSop3 = psInst->u.psSop3;

	/*
		Check the colour calculation is
			SRC0/SRC0ALPHA * SRC1 + (1 - 0) * SRC2
	*/
	if (!(psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0 || psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA))
	{
		return IMG_FALSE;
	}
	if (psSop3->bComplementCSel1)
	{
		return IMG_FALSE;
	}
	if (psSop3->uCSel2 != USEASM_INTSRCSEL_ZERO)
	{
		return IMG_FALSE;
	}
	if (!psSop3->bComplementCSel2)
	{
		return IMG_FALSE;
	}

	/*
		Check there is no destination modifier.
	*/
	if (psSop3->bNegateCResult)
	{
		return IMG_FALSE;
	}

	/*
		Check the alpha calculation is
			SRC0/SRC0ALPHA * SRC1 + (1 - 0) * SRC2
	*/
	if (psSop3->uCoissueOp != USEASM_OP_ASOP)
	{
		return IMG_FALSE;
	}
	if (!(psSop3->uASel1 == USEASM_INTSRCSEL_SRC0 || psSop3->uASel1 == USEASM_INTSRCSEL_SRC0ALPHA))
	{
		return IMG_FALSE;
	}
	if (psSop3->bComplementASel1)
	{
		return IMG_FALSE;
	}
	if (psSop3->uASel2 != USEASM_INTSRCSEL_ZERO)
	{
		return IMG_FALSE;
	}
	if (!psSop3->bComplementASel2)
	{
		return IMG_FALSE;
	}

	/*
		Check there is no destination modifier.
	*/
	if (psSop3->bNegateAResult)
	{
		return IMG_FALSE;
	}

	*pbSrc0AlphaSwizzle = IMG_FALSE;
	if (psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		*pbSrc0AlphaSwizzle = IMG_TRUE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsSOP3Interpolate(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbSrc0AlphaSwizzle)
/*****************************************************************************
 FUNCTION	: IsSOP3Interpolate

 PURPOSE	: Checks for a SOP3 instruction which calculates (per-channel):
				DEST = (1 - SRC0) * SRC1 + SRC0 * SRC2

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  pbSrc0AlphaSwizzle
								- Returns TRUE if source 0 has an alpha replicate
								swizzle.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PSOP3_PARAMS psSop3;

	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3);
	psSop3 = psInst->u.psSop3;

	/*
		Check the colour calculation is
			(1 - SRC0) * SRC1 + SRC0 * SRC2
	*/
	if (!(psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0 || psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA))
	{
		return IMG_FALSE;
	}
	if (!psSop3->bComplementCSel1)
	{
		return IMG_FALSE;
	}
	if (!(psSop3->uCSel2 == USEASM_INTSRCSEL_SRC0 || psSop3->uCSel2 == USEASM_INTSRCSEL_SRC0ALPHA))
	{
		return IMG_FALSE;
	}
	if (psSop3->bComplementCSel2)
	{
		return IMG_FALSE;
	}
	/*
		Check the alpha replicate swizzle is the same for both.
	*/
	if (psSop3->uCSel1 != psSop3->uCSel2)
	{
		return IMG_FALSE;
	}
	/*
		Check there is no destination modifier.
	*/
	if (psSop3->bNegateCResult)
	{
		return IMG_FALSE;
	}

	/*
		Check the alpha calculation is
			(1 - SRC0) * SRC1 + SRC0 * SRC2
	*/
	if (psSop3->uCoissueOp != USEASM_OP_ALRP)
	{
		return IMG_FALSE;
	}
	if (psSop3->uAOp != USEASM_INTSRCSEL_ADD)
	{
		return IMG_FALSE;
	}
	if (!(psSop3->uASel1 == USEASM_INTSRCSEL_SRC1ALPHA && !psSop3->bComplementASel1))
	{
		return IMG_FALSE;
	}
	if (!(psSop3->uASel2 == USEASM_INTSRCSEL_SRC2ALPHA && !psSop3->bComplementASel2))
	{
		return IMG_FALSE;
	}
	/*
		Check there is no destination modifier.
	*/
	if (psSop3->bNegateAResult)
	{
		return IMG_FALSE;
	}

	*pbSrc0AlphaSwizzle = IMG_FALSE;
	if (psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		*pbSrc0AlphaSwizzle = IMG_TRUE;
	}

	return IMG_TRUE;
}

static
IMG_UINT32 GetLiveChansInSwizzledConstant(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: GetLiveChansInSwizzledConstant

 PURPOSE	: Get mask of channels used from the result of applying an alpha
			  replicate swizzle to a secondary attribute source to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: The mask of live channels.
*****************************************************************************/
{
	IMG_UINT32	uLiveChanMask;

	uLiveChanMask = psInst->auLiveChansInDest[0];
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0)
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		if ((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0)
		{
			if ((uLiveChanMask & USC_W_CHAN_MASK) != 0)
			{
				uLiveChanMask &= ~USC_W_CHAN_MASK;
				uLiveChanMask |= USC_X_CHAN_MASK;
			}
		}
	}
	return uLiveChanMask;
}

IMG_INTERNAL
PCSOURCE_ARGUMENT_PAIR GetCommutableSOP3Sources(PINTERMEDIATE_STATE psState, PINST psInst, PSOP3_COMMUTE_MODE peCommuteMode)
/*****************************************************************************
 FUNCTION	: GetCommutableSOP3Sources

 PURPOSE	: Check for a SOP3 instruction where source 0 and another source can be
			  swapped.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  peCommuteMode		- How to update the instruction when swapping.

 RETURNS	: The pairs of sources which are commutable or NULL if no pairs are.
*****************************************************************************/
{
	static const SOURCE_ARGUMENT_PAIR	sSrc01 = {0, 1};
	static const SOURCE_ARGUMENT_PAIR	sSrc02 = {0, 2};
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	static const SOURCE_ARGUMENT_PAIR	sSrc04 = {0, 4};
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	IMG_BOOL							bSrc0Alpha;
	IMG_UINT32							uSrcToSwap;

	/*
		Check the instruction does
			SRC0/SRC0ALPHA * SRC1 + SRC2
	*/
	if (IsSOP3MultiplyAdd(psState, psInst, &bSrc0Alpha))
	{
		uSrcToSwap = 1;
		*peCommuteMode = SOP3_COMMUTE_MODE_MAD;
	}
	/*
		Check the instruction does 
			(1 - SRC0) * SRC1 + SRC0 * SRC2
		or
			(1 - SRC0ALPHA) * SRC1 + SRC0ALPHA * SRC2
	*/
	else if (IsSOP3Interpolate(psState, psInst, &bSrc0Alpha))
	{
		uSrcToSwap = 2;
		*peCommuteMode = SOP3_COMMUTE_MODE_LRP;
	}
	else
	{
		return NULL;
	}

	/*
		In the modified instruction we can't apply an alpha replicate swizzle. So check if it's
		possible to apply the alpha replicate in a separate instruction in the secondary update
		program.
	*/
	if (bSrc0Alpha)
	{
		if (!AddAlphaSwizzleC10Constant(psState, 
										psInst, 
										0 /* uArg */, 
										GetLiveChansInSwizzledConstant(psState, psInst), 
										IMG_TRUE /* bCheckOnly */))
		{
			return NULL;
		}
	}

	/*
		Return the pair of sources to swap.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == ISOP3_VEC)
	{
		switch (uSrcToSwap)
		{
			case 1: return &sSrc02; 
			case 2: return &sSrc04;
			default: imgabort();
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		switch (uSrcToSwap)
		{
			case 1: return &sSrc01; 
			case 2: return &sSrc02;
			default: imgabort();
		}
	}
}

static
IMG_VOID CommuteSOP3Sources_MAD(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CommuteSOP3Sources_MAD

 PURPOSE	: Update a SOP3 instruction doing a multiply-add operation
			  to swap source 0 and another source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSOP3_PARAMS psSop3;

	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3);
	psSop3 = psInst->u.psSop3;

	if (psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		/*
			Add an instruction to the secondary update program to generate an alpha replicated
			version of source 0 and replace source 0 by the alpha replicated version.
		*/
		AddAlphaSwizzleC10Constant(psState, 
								   psInst, 
								   0 /* uArg */, 
								   GetLiveChansInSwizzledConstant(psState, psInst), 
								   IMG_FALSE /* bCheckOnly */);

		/*
			We no longer need to swizzle source 0 in the instruction.
		*/
		psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
	}

	/*
		Swap source 0 and source 1.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == ISOP3_VEC)
	{
		SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 2 /* uSrc2Idx */);
		SwapInstSources(psState, psInst, 1 /* uSrc1Idx */, 3 /* uSrc2Idx */);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
	}
}

static
IMG_VOID CommuteSOP3Sources_LRP(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CommuteSOP3Sources_LRP

 PURPOSE	: Update a SOP3 instruction doing a linear interpolate operation
			  to swap source 0 and another source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSOP3_PARAMS psSop3;

	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_SOP3);
	psSop3 = psInst->u.psSop3;

	if (psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		/*
			Add an instruction to the secondary update program to generate an alpha replicated
			version of source 0 and replace the existing source 0 by it.
		*/
		AddAlphaSwizzleC10Constant(psState, 
								   psInst, 
								   0 /* uArg */, 
								   GetLiveChansInSwizzledConstant(psState, psInst), 
								   IMG_FALSE /* bCheckOnly */);
	}

	/*
		Update the instruction's colour calculation to
			(1 - SRC2) * SRC1 + SRC0 * SRC2
	*/
	psSop3->uCSel1 = USEASM_INTSRCSEL_SRC2;
	ASSERT(psSop3->bComplementCSel1);
	psSop3->uCSel2 = USEASM_INTSRCSEL_SRC0;
	ASSERT(!psSop3->bComplementCSel2);

	/*
		Update the instruction's alpha calculation to match.
	*/
	psSop3->uCoissueOp = USEASM_OP_ASOP;
	psSop3->uASel1 = USEASM_INTSRCSEL_SRC2ALPHA;
	psSop3->bComplementASel1 = psSop3->bComplementCSel1;
	psSop3->uASel2 = USEASM_INTSRCSEL_SRC0ALPHA;
	psSop3->bComplementASel2 = psSop3->bComplementCSel2;

	/*
		Swap source 0 and source 2.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == ISOP3_VEC)
	{
		SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 4 /* uSrc2Idx */);
		SwapInstSources(psState, psInst, 1 /* uSrc1Idx */, 5 /* uSrc2Idx */);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 2 /* uSrc2Idx */);
	}
}

IMG_INTERNAL
IMG_VOID CommuteSOP3Sources(PINTERMEDIATE_STATE psState, 
							PINST				psInst, 
							SOP3_COMMUTE_MODE	eCommuteMode)
/*****************************************************************************
 FUNCTION	: CommuteSOP3Sources

 PURPOSE	: Update a SOP3 instruction to swap source 0 and another source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  eCommuteMode		- How to update the instruction to swap the two sources.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (eCommuteMode)
	{
		case SOP3_COMMUTE_MODE_MAD: CommuteSOP3Sources_MAD(psState, psInst); break;
		case SOP3_COMMUTE_MODE_LRP: CommuteSOP3Sources_LRP(psState, psInst); break;
		default: imgabort();
	}
}

static IMG_BOOL IsRGBClear(PINST psInst, PINST psSecondInst)
/*****************************************************************************
 FUNCTION	: IsRGBClear
    
 PURPOSE	: Check for an instruction clearing the RGB part of a register to zero.

 PARAMETERS	: psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is clearing RGB.
*****************************************************************************/
{
	IMG_UINT32	uEffectiveDestMask;

	uEffectiveDestMask = psInst->auDestMask[0];
	if (psSecondInst != NULL)
	{
		uEffectiveDestMask &= ~psSecondInst->auDestMask[0];
	}

	if ((psInst->eOpcode == IPCKU8F16 || psInst->eOpcode == IPCKU8F32) &&
		uEffectiveDestMask == 0x7 &&
		psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
		psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
		psInst->asArg[1].uType == USEASM_REGTYPE_FPCONSTANT &&
		psInst->asArg[1].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL IsAlphaMultiply(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsAlphaMultiply
    
 PURPOSE	: Check for an instruction which multiplies two registers into alpha.

 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Flow control block containing the instruction to
							check.
			  psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is an alpha multiply.
*****************************************************************************/
{
	if (
			psInst->eOpcode == ISOPWM &&
			psInst->auDestMask[0] == 0x8 &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 &&
			!psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSopWm->bComplementSel2 &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			(
				CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[0]) || 
				CanUseSource0(psState, psCodeBlock->psOwner->psFunc, &psInst->asArg[1])
			)
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL IsSOPWMAdd(PINST	psInst)
/*****************************************************************************
 FUNCTION	: IsSOPWMAdd
    
 PURPOSE	: Check for a SOPWM instruction which does an add.

 PARAMETERS	: psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is a SOPWM add.
*****************************************************************************/
{
	if (
			psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel2
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL IsSOPWMSub(PINST	psInst)
/*****************************************************************************
 FUNCTION	: IsSOPWMAdd
    
 PURPOSE	: Check for a SOPWM instruction which does a subtract.

 PARAMETERS	: psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is a SOPWM add.
*****************************************************************************/
{
	if (
			psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_SUB &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_SUB &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel2
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsSOPWMMultiply(PINST		psInst,
						 IMG_PBOOL	pbAlphaSwizzle)	
/*****************************************************************************
 FUNCTION	: IsSOPWMMultiply
    
 PURPOSE	: Check for a SOPWM instruction which does a multiply.

 PARAMETERS	: psInst		- Instruction to check.
			  pbAlphaSwizzle
							- If non-NULL returns TRUE if one source to the
							multiply has an alpha swizzle.
							If NULL the instruction returns FALSE if one
							source has an alpha swizzle.

 RETURNS	: TRUE if the instruction is a SOPWM multiply.
*****************************************************************************/
{
	if (
			psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			!psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSopWm->bComplementSel2
		)
	{
		IMG_UINT32	uSel1;

		uSel1 = psInst->u.psSopWm->uSel1;
		if (uSel1 == USEASM_INTSRCSEL_SRC2)
		{
			if (pbAlphaSwizzle != NULL)
			{
				*pbAlphaSwizzle = IMG_FALSE;
			}	
			return IMG_TRUE;
		}
		if (uSel1 == USEASM_INTSRCSEL_SRC2ALPHA &&
			pbAlphaSwizzle != NULL)
		{
			*pbAlphaSwizzle = IMG_TRUE;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsSOPWMMove(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsSOPWMMove
    
 PURPOSE	: Check for a SOPWM instruction which does a move.

 PARAMETERS	: psInst		- Instruction to check.

 RETURNS	: TRUE if the instruction is a SOPWM move.
*****************************************************************************/
{
	if (
			(
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				psInst->eOpcode == ISOPWM_VEC ||
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				psInst->eOpcode == ISOPWM 
			) &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSopWm->bComplementSel2
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsResultSaturated
    
 PURPOSE	: Check for an integer instruction with an implicit saturation
			  on the result.

 PARAMETERS	: psInst	- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL IsResultSaturated(PINST	psInst)
{
	if (
			psInst->asDest[0].eFmt == UF_REGFORMAT_U8 &&
			(
				psInst->asArg[0].eFmt == UF_REGFORMAT_C10 ||
				psInst->asArg[1].eFmt == UF_REGFORMAT_C10
			)
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL AllChannelsTheSameInSource(PARG psArg)
/*****************************************************************************
 FUNCTION	: AllChannelsTheSameInSource
    
 PURPOSE	: Check for a source argument where the component select doesn't
			  matter.

 PARAMETERS	: psArg			- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
	{
		if (
				((psArg->uNumber >> 0) & 0xFF) == ((psArg->uNumber >> 8) & 0xFF) &&
				((psArg->uNumber >> 0) & 0xFF) == ((psArg->uNumber >> 16) & 0xFF) &&
				((psArg->uNumber >> 0) & 0xFF) == ((psArg->uNumber >> 24) & 0xFF)
		   )
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID SetConstantOne(PINTERMEDIATE_STATE	psState,
							   PINST				psInst,
							   PARG					psArg)
/*****************************************************************************
 FUNCTION	: SetOneConstant
    
 PURPOSE	: Sets up a source argument to a PCK argument that represents ONE
			  in the PCK's source format.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- PCK instruction.
			  psArg			- Argument to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IPCKU8F32: 
		case IPCKC10F32:
		{
			psArg->uType = USEASM_REGTYPE_FPCONSTANT;
			psArg->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
			break;
		}
		case IPCKU8F16:
		case IPCKC10F16:
		{
			psArg->uType = USEASM_REGTYPE_FPCONSTANT;
			psArg->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
			break;
		}
		case IPCKU8U8:
		case IMOV:
		{
			psArg->uType = USEASM_REGTYPE_IMMEDIATE;
			psArg->uNumber = USC_U8_VEC4_ONE;
			break;
		}
		default: imgabort();
	}
}

static IMG_UINT32 IsConstantOne(PINTERMEDIATE_STATE	psState,
								PINST				psInst,
								PARG				psArg)
/*****************************************************************************
 FUNCTION	: IsConstantOne
    
 PURPOSE	: Get if a source argument to a PCK instruction represents ONE
			  in the PCK's source format.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- PCK instruction.
			  psArg			- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IPCKU8F32: 
		{
			if (psArg->uType == USEASM_REGTYPE_FPCONSTANT &&
				psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case IPCKU8F16: 
		{
			if (psArg->uType == USEASM_REGTYPE_FPCONSTANT &&
				psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		case IPCKU8U8:
		case IMOV:
		{
			if (psArg->uType == USEASM_REGTYPE_IMMEDIATE &&
				psArg->uNumber == USC_U8_VEC4_ONE)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		default: imgabort();
	}
}

static IMG_BOOL CombineFormatConvertSetConst(PINTERMEDIATE_STATE	psState,
											 PINST					psFirstInst,
											 PINST					psSecondInst,
											 IMG_UINT32				uSrcFromDestMask)
/*****************************************************************************
 FUNCTION	: CombineFormatConvertSetConst
    
 PURPOSE	: Merge a format conversion together with an instruction combining
			  the result of the conversion with a constant.

 PARAMETERS	: psState				- Compiler state
			  psFirstInst			- The two instructions to try and combine.
			  psSecondInst	

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uSrcFromDest;
	IMG_UINT32	uConstSrc;
	PARG		psOtherArg;
	IMG_UINT32	uDestMaskFromFirstInst;
	IMG_UINT32	uDestMaskFromConst;

	/*
			IPCKU8F32		TEMP.X,  SRC
			IPCKU8U8		DEST.XY, TEMP.X, #0
		->
			IPCKU8F32		DEST.XY, SRC, #0
	*/

	if (!(psFirstInst->eOpcode == IPCKU8F32 ||
		  psFirstInst->eOpcode == IPCKC10F32 ||
		  psFirstInst->eOpcode == IPCKU8F16 ||
		  psFirstInst->eOpcode == IPCKC10F16))
	{
		return IMG_FALSE;
	}
	if (!(psSecondInst->eOpcode == IPCKC10C10 || psSecondInst->eOpcode == IPCKU8U8))
	{
		return IMG_FALSE;
	}

	if (g_auSetBitCount[psSecondInst->auDestMask[0]] != 2)
	{
		return IMG_FALSE;
	}

	/*
		Get the source to the second instruction which is the same as the result of the
		first instruction.
	*/
	if (g_auSetBitCount[uSrcFromDestMask] != 1)
	{
		return IMG_FALSE;
	}
	uSrcFromDest = g_aiSingleComponent[uSrcFromDestMask];

	/*
		Get the other source to the second instruction.
	*/
	psOtherArg = &psSecondInst->asArg[1 - uSrcFromDest];

	/*
		Check the other source to the second instruction is a constant.
	*/
	if (psOtherArg->uType != USEASM_REGTYPE_IMMEDIATE)
	{
		return IMG_FALSE;
	}
	if (!(psOtherArg->uNumber == 0 || psOtherArg->uNumber == USC_U8_VEC4_ONE))
	{
		return IMG_FALSE;
	}

	/*
		Decompose the mask of the second instruction into the channels copied from each
		source.
	*/
	uDestMaskFromFirstInst = g_auSetBitMask[psSecondInst->auDestMask[0]][uSrcFromDest];
	uDestMaskFromConst = g_auSetBitMask[psSecondInst->auDestMask[0]][1 - uSrcFromDest];

	/*
		Move the destination of the first instruction to the second instruction.
	*/
	MoveDest(psState, psFirstInst, 0 /* uMoveToDestIdx */, psSecondInst, 0 /* uMoveFromDestIdx */);
	psFirstInst->auDestMask[0] = psSecondInst->auDestMask[0];
	psFirstInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];

	/*
		If necessary move the existing source to the first instruction so it is still copied to the
		right place in the destination.
	*/
	if (uDestMaskFromFirstInst > uDestMaskFromConst)
	{
		MovePackSource(psState, psFirstInst, 1 /* uMoveToSrcIdx */, psFirstInst, 0 /* uMoveFromSrcIdx */);
		uConstSrc = 0;
	}
	else		
	{
		uConstSrc = 1;
	}

	/*
		Set the other argument to the same constant in the source format for the conversion instruction.
	*/
	if (psOtherArg->uNumber == USC_U8_VEC4_ONE)
	{
		SetConstantOne(psState, psFirstInst, &psFirstInst->asArg[uConstSrc]);
	}
	else
	{
		SetSrc(psState, psFirstInst, uConstSrc, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, psFirstInst->asDest[0].eFmt);
	}
	SetPCKComponent(psState, psFirstInst, uConstSrc, 0 /* uComponent */);

	return IMG_TRUE;
}

static IMG_BOOL CombineDifferentSrcFmtPacks(PINTERMEDIATE_STATE psState,
											PINST psFirstInst, 
											PINST psSecondInst)
/*****************************************************************************
 FUNCTION	: CombineDifferentSrcFmtPacks
    
 PURPOSE	: Try to combine together two pack instructions if one has only
			  constant arguments by converting the constants to the source
			  format of the other PCK.

 PARAMETERS	: psState				- Compiler state
			  psFirstInst			- The two instructions to try and combine.
			  psSecondInst	

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	PINST		apsInst[2];
	IMG_UINT32	uInstIdx;

	apsInst[0] = psFirstInst;
	apsInst[1] = psSecondInst;

	for (uInstIdx = 0; uInstIdx < 2; uInstIdx++)
	{
		PINST		psInst = apsInst[uInstIdx];
		PARG		psArg0 = &psInst->asArg[0];

		/*
			Check for packing constant 0 into the result.
		*/
		if (
				psArg0->uType == USEASM_REGTYPE_FPCONSTANT &&
				psArg0->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
			)
		{
			break;
		}
		if (
				psArg0->uType == USEASM_REGTYPE_IMMEDIATE &&
				psArg0->uNumber == 0
		   )
		{
			break;
		}
		/*
			Check for packing constant 1 into the result.
		*/
		if (IsConstantOne(psState, psInst, psArg0))
		{
			/*
				Change to the appropriate constant format for the
				other PCK instruction.
			*/
			SetConstantOne(psState, apsInst[1 - uInstIdx], psArg0);
			break;
		}
	}

	/*
		Fail if neither instruction has constant arguments.
	*/
	if (uInstIdx == 2)
	{
		return IMG_FALSE;
	}

	if (psFirstInst->eOpcode == IMOV)
	{
		/*
			Convert a MOV instruction to PCKU8U8.
		*/
		psFirstInst->auDestMask[0] = 
			psSecondInst->auLiveChansInDest[0] & ~psSecondInst->auDestMask[0];
		SetOpcode(psState, psFirstInst, IPCKU8U8);
		SetPCKComponent(psState, psFirstInst, 0 /* uComponent */, g_aiSingleComponent[psFirstInst->auDestMask[0]]);
	}
	ASSERT(psSecondInst->eOpcode != IMOV);

	ASSERT(g_abSingleBitSet[psFirstInst->auDestMask[0]]);
	ASSERT(g_abSingleBitSet[psSecondInst->auDestMask[0]]);

	/*
		Copy the opcode and SCALE flag from the PCK with non-constant
		arguments.
	*/
	if (uInstIdx == 0)
	{
		SetOpcode(psState, psFirstInst, psSecondInst->eOpcode);
		psFirstInst->u.psPck->bScale = psSecondInst->u.psPck->bScale;
	}
	/*
		Clear the component select on the constant argument.
	*/
	if (uInstIdx == 0)
	{
		SetPCKComponent(psState, psFirstInst, 0 /* uArgIdx */, 0 /* uComponent */);
	}
	else
	{
		SetPCKComponent(psState, psSecondInst, 0 /* uArgIdx */, 0 /* uComponent */);
	}
	/*
		Combine the arguments from the two instructions.
	*/
	if (psFirstInst->auDestMask[0] < psSecondInst->auDestMask[0])
	{
		MovePackSource(psState, psFirstInst, 1 /* uDestArgIdx */, psSecondInst, 0 /* uSrcArgIdx */);
	}
	else
	{
		MovePackSource(psState, psFirstInst, 1 /* uDestArgIdx */, psFirstInst, 0 /* uSrcArgIdx */);
		MovePackSource(psState, psFirstInst, 0 /* uDestArgIdx */, psSecondInst, 0 /* uSrcArgIdx */);
	}

	/*
		Combine the destination masks for the two instructions.
	*/
	MoveDest(psState, psFirstInst, 0 /* uMoveToDestIdx */, psSecondInst, 0 /* uMoveFromDestIdx */);
	psFirstInst->auDestMask[0] |= psSecondInst->auDestMask[0];
	psFirstInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];

	return IMG_TRUE;
}

static IMG_UINT32 SwapArgsInSrcSel(PINTERMEDIATE_STATE psState, IMG_UINT32 uSel)
/*****************************************************************************
 FUNCTION	: SwapArgsInSrcSel
    
 PURPOSE	: Swap the order of SRC1 and SRC2 in an integer source selector.

 PARAMETERS	: psState		- Compiler state.
			  uSel			- The integer source selectors to update.

 RETURNS	: The updated integer source selectors.
*****************************************************************************/
{
	switch (uSel)
	{
		case USEASM_INTSRCSEL_SRC1: return USEASM_INTSRCSEL_SRC2;
		case USEASM_INTSRCSEL_SRC1ALPHA: return USEASM_INTSRCSEL_SRC2ALPHA;
		case USEASM_INTSRCSEL_SRC2: return USEASM_INTSRCSEL_SRC1;
		case USEASM_INTSRCSEL_SRC2ALPHA: return USEASM_INTSRCSEL_SRC1ALPHA;
		case USEASM_INTSRCSEL_SRC2SCALE: imgabort();
		case USEASM_INTSRCSEL_ZERO: return USEASM_INTSRCSEL_ZERO;
		default: imgabort();
	}
}

static IMG_UINT32 SelectAlphaInSrcSel(PINTERMEDIATE_STATE psState, IMG_UINT32 uSel)
/*****************************************************************************
 FUNCTION	: SelectAlphaInSrcSel
    
 PURPOSE	: Change references to source register to reference the alpha channel
			  of the same register.

 PARAMETERS	: psState		- Compiler state.
			  uSel			- The integer source selectors to update.

 RETURNS	: The updated integer source selectors.
*****************************************************************************/
{
	switch (uSel)
	{
		case USEASM_INTSRCSEL_SRC1:		
		case USEASM_INTSRCSEL_SRC1ALPHA:
		{
			return USEASM_INTSRCSEL_SRC1ALPHA;
		}
		case USEASM_INTSRCSEL_SRC2:		
		case USEASM_INTSRCSEL_SRC2ALPHA:
		{
			return USEASM_INTSRCSEL_SRC2ALPHA;
		}
		case USEASM_INTSRCSEL_ZERO: 
		{
			return USEASM_INTSRCSEL_ZERO;
		}
		default: imgabort();
	}
}

static IMG_VOID CombineSopwmsMatchingArgsIntoSop2(PINTERMEDIATE_STATE	psState,
												  PINST					psDestInst, 
												  PINST					psVectorInst, 
												  PINST					psScalarInst, 
												  PINST					psLastInst,
												  IMG_BOOL				bSwapVectorArgs,
												  IMG_BOOL				bSwapScalarArgs)
/*****************************************************************************
 FUNCTION	: CombineSopwmsMatchingArgsIntoSop2
    
 PURPOSE	: Combine two SOPWM instructions together.

 PARAMETERS	: psVectorInst
			  psScalarInst		- The instructions to combine.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SOP2_PARAMS	sSop2;

	/*
		Copy the source selectors from the vector SOPWM to the SOP2.
	*/
	sSop2.uCOp = psVectorInst->u.psSopWm->uCop;
	if (!bSwapVectorArgs)
	{
		sSop2.uCSel1 = psVectorInst->u.psSopWm->uSel1;
		sSop2.uCSel2 = psVectorInst->u.psSopWm->uSel2;
		sSop2.bComplementCSel1 = psVectorInst->u.psSopWm->bComplementSel1;
		sSop2.bComplementCSel2 = psVectorInst->u.psSopWm->bComplementSel2;
	}
	else
	{
		sSop2.uCSel1 = SwapArgsInSrcSel(psState, psVectorInst->u.psSopWm->uSel2);
		sSop2.uCSel2 = SwapArgsInSrcSel(psState, psVectorInst->u.psSopWm->uSel1);
		sSop2.bComplementCSel1 = psVectorInst->u.psSopWm->bComplementSel2;
		sSop2.bComplementCSel2 = psVectorInst->u.psSopWm->bComplementSel1;
	}
	sSop2.bComplementCSrc1 = IMG_FALSE;

	/*
		Copy the source selectors from the scalar SOPWM to the SOP2.
	*/
	sSop2.uAOp = psScalarInst->u.psSopWm->uAop;
	if (!bSwapScalarArgs)
	{
		sSop2.uASel1 = psScalarInst->u.psSopWm->uSel1;
		sSop2.uASel2 = psScalarInst->u.psSopWm->uSel2;
		sSop2.bComplementASel1 = psScalarInst->u.psSopWm->bComplementSel1;
		sSop2.bComplementASel2 = psScalarInst->u.psSopWm->bComplementSel2;
	}
	else
	{
		sSop2.uASel1 = SwapArgsInSrcSel(psState, psScalarInst->u.psSopWm->uSel2);
		sSop2.uASel2 = SwapArgsInSrcSel(psState, psScalarInst->u.psSopWm->uSel1);
		sSop2.bComplementASel1 = psScalarInst->u.psSopWm->bComplementSel2;
		sSop2.bComplementASel2 = psScalarInst->u.psSopWm->bComplementSel1;
	}
	sSop2.bComplementASrc1 = IMG_FALSE;

	/*
		Change references to source registers to reference the alpha channel.
	*/
	sSop2.uASel1 = SelectAlphaInSrcSel(psState, sSop2.uASel1);
	sSop2.uASel2 = SelectAlphaInSrcSel(psState, sSop2.uASel2);

	/*
		Create the new instruction.
	*/
	SetOpcode(psState, psDestInst, ISOP2);
	psDestInst->auDestMask[0] = USC_DESTMASK_FULL;
	*psDestInst->u.psSop2 = sSop2;
	if (psDestInst != psLastInst)
	{
		MoveDest(psState, psDestInst, 0 /* uMoveToDestIdx */, psLastInst, 0 /* uMoveFromDestIdx */);
		psDestInst->auLiveChansInDest[0] = psLastInst->auLiveChansInDest[0];
	}
}

static IMG_VOID CombineSopwmsWithSpareArgIntoSop2(PINTERMEDIATE_STATE	psState,
												  PINST					psDestInst, 
												  PINST					psVectorInst, 
												  PINST					psScalarInst, 
												  PINST					psLastInst)
/*****************************************************************************
 FUNCTION	: CombineSopwmsWithSpareArgIntoSop2
    
 PURPOSE	: Combine two SOPWM instructions together

 PARAMETERS	: psVectorInst
			  psScalarInst		- The instructions to combine.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SOP2_PARAMS	sParams;

	/*
		Copy the vector source selectors.
	*/
	sParams.uCOp = psVectorInst->u.psSopWm->uCop;
	sParams.uCSel1 = psVectorInst->u.psSopWm->uSel1;
	sParams.uCSel2 = psVectorInst->u.psSopWm->uSel2;
	sParams.bComplementCSel1 = psVectorInst->u.psSopWm->bComplementSel1;
	sParams.bComplementCSel2 = psVectorInst->u.psSopWm->bComplementSel2;
	sParams.bComplementCSrc1 = IMG_FALSE;
	
	/*
		Copy the scalar source selectors.
	*/
	sParams.uAOp = psScalarInst->u.psSopWm->uAop;
	sParams.bComplementASrc1 = IMG_FALSE;
	if (psScalarInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC1)
	{
		sParams.uASel2 = USEASM_INTSRCSEL_SRC2;
	}
	else if (psScalarInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC1ALPHA)
	{
		sParams.uASel2 = USEASM_INTSRCSEL_SRC2ALPHA;
	}
	else
	{
		ASSERT(psScalarInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO);
		sParams.uASel2 = psScalarInst->u.psSopWm->uSel1;
	}
	sParams.uASel1 = USEASM_INTSRCSEL_ZERO;
	sParams.bComplementASel1 = IMG_FALSE;
	sParams.bComplementASel2 = psScalarInst->u.psSopWm->bComplementSel1;

	SetOpcode(psState, psDestInst, ISOP2);
	psDestInst->auDestMask[0] = USC_DESTMASK_FULL;

	MoveSrc(psState, psDestInst, 1 /* uCopyToSrcIdx */, psScalarInst,  0 /* uCopyFromSrcIdx */);
	if (psDestInst != psVectorInst)
	{
		MoveSrc(psState, psDestInst, 0 /* uCopyToSrcIdx */, psVectorInst,  0 /* uCopyFromSrcIdx */);
	}
	*psDestInst->u.psSop2 = sParams;
	if (psDestInst != psLastInst)
	{
		MoveDest(psState, psDestInst, 0 /* uMoveToDestIdx */, psLastInst, 0 /* uMoveFromDestIdx */);
		psDestInst->auLiveChansInDest[0] = psLastInst->auLiveChansInDest[0];
	}
}

static 
IMG_BOOL CombineSopwms(PINTERMEDIATE_STATE psState,
					   PINST psInst, 
					   PINST psNextInst)
/*****************************************************************************
 FUNCTION	: CombineSopwms
    
 PURPOSE	: Combine two SOPWM instructions together

 PARAMETERS	: psState			- Internal compiler state
			  psInst			- 
			  psNextInst		- The instructions to combine.

 RETURNS	: TRUE if the SOPWMs were combined.
*****************************************************************************/
{
	/*
		If the two instructions are exactly the same then just combine
		their masks.
	*/
	if (psNextInst->asArg[0].uType == psInst->asArg[0].uType &&
		psNextInst->asArg[0].uNumber == psInst->asArg[0].uNumber &&
		psNextInst->asArg[1].uType == psInst->asArg[1].uType &&
		psNextInst->asArg[1].uNumber == psInst->asArg[1].uNumber &&
		psNextInst->eOpcode == ISOPWM &&
		psNextInst->u.psSopWm->uCop == psInst->u.psSopWm->uCop &&
		psNextInst->u.psSopWm->uAop == psInst->u.psSopWm->uAop &&
		psNextInst->u.psSopWm->uSel1 == psInst->u.psSopWm->uSel1 &&
		psNextInst->u.psSopWm->uSel2 == psInst->u.psSopWm->uSel2 &&
		psNextInst->u.psSopWm->bComplementSel1 == psInst->u.psSopWm->bComplementSel1 &&
		psNextInst->u.psSopWm->bComplementSel2 == psInst->u.psSopWm->bComplementSel2)
	{
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] |= psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
		return IMG_TRUE;
	}
	/*
		If the source registers are equal but in a different order then
		merge.
	*/
	else if (psNextInst->asArg[1].uType == psInst->asArg[0].uType &&
			 psNextInst->asArg[1].uNumber == psInst->asArg[0].uNumber &&
			 psNextInst->asArg[0].uType == psInst->asArg[1].uType &&
			 psNextInst->asArg[0].uNumber == psInst->asArg[1].uNumber &&
			 psNextInst->eOpcode == ISOPWM &&
			 psNextInst->u.psSopWm->uCop == psInst->u.psSopWm->uCop &&
			 psNextInst->u.psSopWm->uAop == psInst->u.psSopWm->uAop &&
			 psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 &&
			 psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 &&
			 psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			 psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			 !psInst->u.psSopWm->bComplementSel1 &&
			 !psInst->u.psSopWm->bComplementSel2 &&
			 !psNextInst->u.psSopWm->bComplementSel1 &&
			 !psNextInst->u.psSopWm->bComplementSel2)
	{
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] |= psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
		return IMG_TRUE;
	}
	/*
		Otherwise if we don't need to preserve any channels in the destination and one
		instruction writes vector channels only and the other alpha channels only then
		combine the two instructions into a SOP2.
	*/
	else if (
				(psInst->auDestMask[0] | psNextInst->auDestMask[0]) == psNextInst->auLiveChansInDest[0]  &&
				(
					((psInst->auDestMask[0] & 0x8) == 0 && psNextInst->auDestMask[0] == 0x8) ||
					(psInst->auDestMask[0] == 0x8 && (psNextInst->auDestMask[0] & 0x8) == 0)
				)
			)
	{
		IMG_BOOL	bCombine;
		IMG_BOOL	bSwapped;

		/*
			If both instructions have unused arguments then combine them by merging the arguments.
		*/
		if (	
				GetLiveChansInArg(psState, psInst, 1) == 0 &&
				GetLiveChansInArg(psState, psNextInst, 1) == 0
			)
		{
			if (psNextInst->auDestMask[0] & 0x7)
			{
				CombineSopwmsWithSpareArgIntoSop2(psState, psInst, psNextInst, psInst, psNextInst);
			}
			else
			{
				CombineSopwmsWithSpareArgIntoSop2(psState, psInst, psInst, psNextInst, psNextInst);
			}
			return IMG_TRUE;
		}

		/*
			If both instructions have the same arguments then combine them.
		*/
		bCombine = IMG_FALSE;
		bSwapped = IMG_FALSE;
		if (
					psNextInst->asArg[0].uType == psInst->asArg[0].uType &&
					psNextInst->asArg[0].uNumber == psInst->asArg[0].uNumber &&
					psNextInst->asArg[1].uType == psInst->asArg[1].uType &&
					psNextInst->asArg[1].uNumber == psInst->asArg[1].uNumber
		   )
		{
			bCombine = IMG_TRUE;
		}
		else if (
					psNextInst->asArg[1].uType == psInst->asArg[0].uType &&
					psNextInst->asArg[1].uNumber == psInst->asArg[0].uNumber &&
					psNextInst->asArg[0].uType == psInst->asArg[1].uType &&
					psNextInst->asArg[0].uNumber == psInst->asArg[1].uNumber
				)
		{
			bCombine = IMG_TRUE;
			bSwapped = IMG_TRUE;
		}
		if (bCombine)
		{
			if (psNextInst->auDestMask[0] & 0x7)
			{
				CombineSopwmsMatchingArgsIntoSop2(psState, psInst, psNextInst, psInst, psNextInst, bSwapped, IMG_FALSE);
			}
			else
			{
				CombineSopwmsMatchingArgsIntoSop2(psState, psInst, psInst, psNextInst, psNextInst, IMG_FALSE, bSwapped);
			}
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;	 
}

static IMG_BOOL RemoveRedundantConversion(PINTERMEDIATE_STATE psState,
										  PINST psInst,
										  PINST psNextInst, 
										  IMG_UINT32 uSrcFromDest,
										  IMG_UINT32 uPartialDestFromDest,
										  IMG_PBOOL pbDropNextInst,
										  IMG_PBOOL pbMovesGenerated,
										  IMG_BOOL bC10)
/*****************************************************************************
 FUNCTION	: RemoveRedundantConversion
    
 PURPOSE	: Remove a redundant format conversions.

 PARAMETERS	: psInst, psNextInst		- The two format conversion instructions.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are the result of the first instruction.
			  uPartialDestFromDest		- Mask of destinations to the second instruction
										which are the result of the first instruction.

 RETURNS	: TRUE if the format conversion was removed.
*****************************************************************************/
{
	IMG_UINT32	uReplaceMask;
	IMG_UINT32	uChan;
	IMG_UINT32	uSrcChan;
	IMG_UINT32	uSrc = 0;
	IMG_UINT32	uUnusedSrc = UINT_MAX;

	if (uPartialDestFromDest != 0)
	{
		return IMG_FALSE;
	}

	uSrcChan = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);

	/*
		Check which sources to the conversion back to U8 are from
		the first instruction.
	*/
	uReplaceMask = 0;
	for (uChan = 0; uChan < 4; uChan++)
	{
		if (psNextInst->auDestMask[0] & (1U << uChan))
		{
			if ((uSrcFromDest & (1U << uSrc)) != 0)
			{
				uReplaceMask |= (1U << uChan);
			}
			else
			{
				uUnusedSrc = uSrc;
			}

			uSrc = (uSrc + 1) % 2;
		}
	}

	if (uReplaceMask != 0)
	{
		/*
			If we are partially writing a register or swizzling then use PACK otherwise switch to MOV.
		*/
		if (psNextInst->auDestMask[0] == psNextInst->auLiveChansInDest[0] && uReplaceMask == (IMG_UINT32)(1U << uSrcChan))
		{
			SetOpcode(psState, psInst, IMOV);
			*pbMovesGenerated = IMG_TRUE;
		}
		else
		{
			ModifyOpcode(psState, psInst, (bC10 ? IPCKC10C10 : IPCKU8U8));
			psInst->u.psPck->bScale = IMG_FALSE;
			psInst->auDestMask[0] = uReplaceMask;

			if (g_auSetBitCount[uReplaceMask] > 1)
			{
				CopySrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
				SetPCKComponent(psState, psInst, 1 /* uArgIdx */, GetPCKComponent(psState, psInst, 0 /* uArgIdx */));
			}
			else
			{
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
			}
		}

		psNextInst->auDestMask[0] &= ~uReplaceMask;

		CopyPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, psNextInst, 0 /* uDestIdx */);
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0] & ~psNextInst->auDestMask[0];
			
		if (psNextInst->auDestMask[0] == 0)
		{
			*pbDropNextInst = IMG_TRUE;

			/*
				Move the destination of the second instruction to the first instruction.
			*/
			MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		}
		else
		{
			ARG		sNewPartialDest;

			ASSERT(uUnusedSrc != UINT_MAX);

			/*
				Create a new destination for the copied channels.
			*/
			MakeNewTempArg(psState, bC10 ? UF_REGFORMAT_C10 : UF_REGFORMAT_U8, &sNewPartialDest);
			SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sNewPartialDest);

			/*
				Use the new destination as the source for the channels we have removed from the
				second instruction's destination mask.
			*/
			SetPartiallyWrittenDest(psState, psNextInst, 0 /* uDestIdx */, &sNewPartialDest);

			if (g_auSetBitCount[psNextInst->auDestMask[0]] == 1)
			{
				if (uUnusedSrc != 0)
				{
					MoveSrc(psState, psNextInst, 0 /* uMoveToSrcIdx */, psNextInst, uUnusedSrc);
					SetPCKComponent(psState, psNextInst, 0 /* uSrcIdx */, GetPCKComponent(psState, psNextInst, uUnusedSrc));
				}
				SetSrc(psState, psNextInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
			}
			else
			{
				CopySrc(psState, psNextInst, 1 - uUnusedSrc, psNextInst, uUnusedSrc);
				SetPCKComponent(psState, psNextInst, 1 - uUnusedSrc, GetPCKComponent(psState, psNextInst, uUnusedSrc));
			}

			*pbDropNextInst = IMG_FALSE;
		}

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CombineSOPWMMulSOPWMDouble(PINTERMEDIATE_STATE psState, 
										   PINST psInst, 
										   PINST psNextInst,
										   IMG_UINT32 uSrcFromDest,
										   IMG_UINT32 uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: CombineSOPWMMulSOPWMDouble
    
 PURPOSE	: Try to combine a SOPWM doing a multiply with another SOPWM
			  doubling the result.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- The instructions.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are the result of the first instruction.
			  uPartialDestFromDest		- Mask of destinations to the second instruction
										which are the result of the first instruction.

 RETURNS	: TRUE if two instructions were combined.
*****************************************************************************/
{
	/*
		Check the first instruction does DEST1=SRC1*SRC2
	*/
	if (!IsSOPWMMultiply(psInst, NULL))
	{
		return IMG_FALSE;
	}
	/*
		If the first instruction includes an implicit saturation of
		the result then we can't combine the calculations.
	*/
	if (IsResultSaturated(psInst))
	{
		return IMG_FALSE;
	}

	/*
		Check the first instruction does DEST2=SRC3+SRC4
	*/
	if (!IsSOPWMAdd(psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check SRC3=SRC4=DEST1
	*/
	if (uSrcFromDest == 3 && psNextInst->auDestMask[0] == psInst->auDestMask[0])
	{
		/*
			Copy the destination from the second instruction to
			the first.
		*/
		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		psInst->auDestMask[0] = psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		/*
			If the second instruction is partially updating a different register to the destination of the
			first instruction then move the partial destination.
		*/
		if (uPartialDestFromDest == 0)
		{
			CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		}
	
		/*
			Convert the first instruction to do
				DEST2=SRC1*SRC2+SRC2*SRC1
		*/
		psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_SRC1;
	
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SOPWMToMove
    
 PURPOSE	: Try to convert a SOPWM to a move type instruction.

 PARAMETERS	: psState		- Compiler state
			  psInst		- Instruction to convert.
			  uMoveArg		- Argument that is being moved.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SOPWMToMove(PINTERMEDIATE_STATE psState, 
							PINST psInst, 
							IMG_UINT32 uMoveArg, 
							IMG_PBOOL pbMovesGenerated)
{
	/*
		For C10 data the SOPWM effectively copied the red channel to the alpha of the
		destination so check for that case.
	*/
	if (psInst->asArg[uMoveArg].uType == USEASM_REGTYPE_TEMP || 
		(psInst->auDestMask[0] & 8) == 0 || 
		psInst->asDest[0].eFmt != UF_REGFORMAT_C10)
	{
		/* 
			SetOpcode will reduce the argument array from 2 to 1 args, so we have to
			copy the arg that we want to use for the MOV before updating the opcode.
		*/
		if (uMoveArg != 0)
		{
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, uMoveArg);
		}
		SetOpcode(psState, psInst, IMOV);

		psInst->auDestMask[0] = USC_DESTMASK_FULL;
		*pbMovesGenerated = IMG_TRUE;
	}
	else if (psInst->auDestMask[0] == 8)
	{
		SetOpcode(psState, psInst, IPCKC10C10);
		if (uMoveArg != 0)
		{
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, uMoveArg);
		}
	}
}

/*****************************************************************************
 FUNCTION	: DropRedundantMinMax
    
 PURPOSE	: Try to convert an integer MIN/MAX instruction with a fixed result
			  to a move.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to try and convert.

 RETURNS	: TRUE if the instruction was simplified.
*****************************************************************************/
static IMG_BOOL DropRedundantMinMax(PINTERMEDIATE_STATE psState, PINST	psInst)
{
	IMG_BOOL	bMax, bMin;
	IMG_UINT32	uArg;
	IMG_BOOL	abOne[2];
	IMG_BOOL	abZero[2];
	IMG_UINT32	uPreservedArg;

	/*
		Check for	SRC1 * ONE op SRC2 * ONE
	*/
	if (
			!(psInst->eOpcode == ISOPWM &&
			  psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			  psInst->u.psSopWm->bComplementSel1 &&
			  psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			  psInst->u.psSopWm->bComplementSel2 
			 )
	   ) 
	{
		return IMG_FALSE;
	}

	/*
		Check that the operation is either a MIN or MAX
	*/
	bMax = bMin = IMG_FALSE;
	if (
			((psInst->auDestMask[0] & 7) == 0 || psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_MAX) &&
			((psInst->auDestMask[0] & 8) == 0 || psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_MAX)
	   )
	{
		bMax = IMG_TRUE;
	}
	if (
			((psInst->auDestMask[0] & 7) == 0 || psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_MIN) &&
			((psInst->auDestMask[0] & 8) == 0 || psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_MIN)
	   )
	{
		bMin = IMG_TRUE;
	}
	if (!bMax && !bMin)
	{
		return IMG_FALSE;
	}
		
	/*
		Check both of the source arguments for a fixed value.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		PARG	psArg = &psInst->asArg[uArg];

		abOne[uArg] = abZero[uArg] = IMG_FALSE;
		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE &&
			psArg->uNumber == USC_U8_VEC4_ONE &&
			psArg->eFmt == UF_REGFORMAT_U8)
		{
			abOne[uArg] = IMG_TRUE;
		}
		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE &&
			psArg->uNumber == 0)
		{
			abZero[uArg] = IMG_TRUE;
		}
	}	

	uPreservedArg = UINT_MAX;
	/* MIN(1, 1) = 1 / MAX(1, 1) = 1 */
	if (abOne[0] && abOne[1])
	{
		uPreservedArg = 0;
	}
	/* MAX(1, 0) = 1 / MIN(1, 0) = 0 */
	else if (abOne[0] && abZero[1])
	{
		uPreservedArg = bMax ? 0 : 1;
	}
	/* MAX(0, 1) = 1 / MIN(0, 1) = 0 */
	else if (abZero[0] && abOne[1])
	{
		uPreservedArg = bMax ? 1 : 0;
	}
	/* MAX(0, 0) = 0 / MIN(0, 0) = 0 */
	else if (abZero[0] && abZero[1])
	{
		uPreservedArg = 0;
	}

	if (uPreservedArg != UINT_MAX)
	{
		/*
			Convert to a move.
		*/
		if (uPreservedArg != 0)
		{
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, uPreservedArg);
		}
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

		psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL ReducePCKToMove(PINTERMEDIATE_STATE psState, PINST psPCKInst)
/*****************************************************************************
 FUNCTION	: ReducePCKToMove
    
 PURPOSE	: Check for a PCKC10C10/PCKU8U8 instruction which is equivalent
			  to a simple move.

 PARAMETERS	: psState		- Compiler state.
			  psPCKInst		- Instruction to check.

 RETURNS	: TRUE if the instruction was simplified to a move.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uDestMask = psPCKInst->auDestMask[0];
	IMG_UINT32	uSrcIdx;

	if (g_auSetBitCount[uDestMask] > 2)
	{
		return IMG_FALSE;
	}

	/*
		Check for each channel written in the result that it is copied
		from the same channel in the source register.
	*/
	uSrcIdx = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((uDestMask & (1U << uChan)) != 0)
		{
			if (
					!AllChannelsTheSameInSource(&psPCKInst->asArg[uSrcIdx]) && 
					GetPCKComponent(psState, psPCKInst, uSrcIdx) != uChan
			   )
			{
				return IMG_FALSE;
			}
			uSrcIdx++;
		}
	}

	/*
		Check both source arguments are equal.
	*/
	if (g_auSetBitCount[uDestMask] > 1 && !EqualArgs(&psPCKInst->asArg[0], &psPCKInst->asArg[1]))
	{
		return IMG_FALSE;
	}

	/*
		If there are any channel not written in the destination check their source is the same as
		the source arguments.
	*/
	if ((psPCKInst->auLiveChansInDest[0] & ~psPCKInst->auDestMask[0]) != 0)
	{
		if (psPCKInst->apsOldDest[0] == NULL || !EqualArgs(&psPCKInst->asArg[0], psPCKInst->apsOldDest[0]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Replace the PCK instruction by a move.
	*/
	SetOpcode(psState, psPCKInst, IMOV);
	psPCKInst->auDestMask[0] = USC_ALL_CHAN_MASK;
	if (NoPredicate(psState, psPCKInst))
	{
		SetPartiallyWrittenDest(psState, psPCKInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
	}

	return IMG_TRUE;
}

static IMG_BOOL ConvertPartialDestToSrc(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ConvertPartialDestToSrc
    
 PURPOSE	: Check for a PCKC10C10/PCKU8U8 instruction where the source for
			  unwritten channels is the same as one of the sources.

 PARAMETERS	: psState		- Compiler state.
			  psPCKInst		- Instruction to check.

 RETURNS	: TRUE if the instruction was simplified to remove the partial
			  destination.
*****************************************************************************/
{
	ARG			asDestChanSrc[VECTOR_LENGTH];
	IMG_UINT32	auDestChanSrcComp[VECTOR_LENGTH];
	IMG_UINT32	auNewSrc[PCK_SOURCE_COUNT];
	IMG_UINT32	uDestChan;
	IMG_UINT32	uNewDestMask;
	IMG_UINT32	uSrc;

	ASSERT(psInst->eOpcode == IPCKU8U8 || psInst->eOpcode == IPCKC10C10);
	ASSERT(psInst->uDestCount == 1);

	if (psInst->apsOldDest[0] == NULL)
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}

	/*
		Get the source argument or overwritten destination copied to each channel in the 
		new destination.
	*/
	uSrc = 0;
	for (uDestChan = 0; uDestChan < VECTOR_LENGTH; uDestChan++)
	{
		if ((psInst->auDestMask[0] & (1U << uDestChan)) != 0)
		{
			asDestChanSrc[uDestChan] = psInst->asArg[uSrc];
			auDestChanSrcComp[uDestChan] = GetPCKComponent(psState, psInst, uSrc);
			uSrc = (uSrc + 1) % PCK_SOURCE_COUNT;
		}
		else
		{
			asDestChanSrc[uDestChan] = *psInst->apsOldDest[0];
			auDestChanSrcComp[uDestChan] = uDestChan;
		}
	}

	/*
		We removed the partial destination what would the new
		destination mask be?
	*/
	uNewDestMask = psInst->auLiveChansInDest[0];
	if (g_auSetBitCount[uNewDestMask] == 3)
	{
		uNewDestMask = USC_ALL_CHAN_MASK;
	}

	for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
	{
		auNewSrc[uSrc] = USC_UNDEF;
	}

	/*
		Check that where a single source argument in the new instruction
		would be copied to multiple channels in the destination those channels had the
		same value in the old instruction.
	*/
	uSrc = 0;
	for (uDestChan = 0; uDestChan < VECTOR_LENGTH; uDestChan++)
	{
		if ((uNewDestMask & (1U << uDestChan)) != 0)
		{
			if ((psInst->auLiveChansInDest[0] & (1U << uDestChan)) != 0)
			{
				if (auNewSrc[uSrc] == USC_UNDEF)
				{
					auNewSrc[uSrc] = uDestChan;
				}
				else
				{
					IMG_UINT32	uMatchChan = auNewSrc[uSrc];

					if (
							!EqualArgs(&asDestChanSrc[uMatchChan], &asDestChanSrc[uDestChan]) ||
							auDestChanSrcComp[uMatchChan] != auDestChanSrcComp[uDestChan]
					   )
					{
						return IMG_FALSE;
					}
				}
			}
			uSrc = (uSrc + 1) % PCK_SOURCE_COUNT;
		}
	}

	/*
		Clear the new instruction.
	*/
	psInst->auDestMask[0] = uNewDestMask;

	for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
	{
		if (auNewSrc[uSrc] == USC_UNDEF)
		{
			SetSrc(psState, psInst, uSrc, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_U8);
			SetPCKComponent(psState, psInst, uSrc, 0 /* uComponent */);
		}
		else
		{
			SetSrcFromArg(psState, psInst, uSrc, &asDestChanSrc[auNewSrc[uSrc]]);
			SetPCKComponent(psState, psInst, uSrc, auDestChanSrcComp[auNewSrc[uSrc]]);
		}
	}

	/*
		Clearly the reference to a partially overwritten source.
	*/
	SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartiallyWrittenDest */);

	return IMG_TRUE;
}

static IMG_BOOL NormaliseIntegerInstructions(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: NormaliseIntegerInstructions
    
 PURPOSE	: Convert integer instructions in a block to the most simple form.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- The block to optimize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst, psNextInst;
	IMG_BOOL	bMovesGenerated = IMG_FALSE;
	IMG_BOOL	bChanges = IMG_FALSE;
	IMG_BOOL	bIntegerMovesGenerated = IMG_FALSE;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;

		/*
			Remove a format conversion where the converted register is the
			same regardless of format.
		*/
		if ((psInst->eOpcode == IUNPCKF32C10 || psInst->eOpcode == IUNPCKF32U8) &&
			psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[0].uNumber == 0)
		{
			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0].eFmt = UF_REGFORMAT_F32;
			bMovesGenerated = IMG_TRUE;
		}
		/*
			Put a multiply in a standard form. SRC1*ZERO+SRC2*SRC1 -> SRC1*SRC2 + SRC2*ZERO
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_SRC1 &&
			!psInst->u.psSopWm->bComplementSel2)
		{
			psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2;
			psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		}
		/*
			Check for an add with no effect.
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->bComplementSel2 &&
			psInst->auDestMask[0] == psInst->auLiveChansInDest[0])
		{
			/*
				Check for one of the arguments being zero and no format conversion.
			*/
			if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
				psInst->asArg[1].uNumber == 0 &&
				psInst->asDest[0].eFmt == psInst->asArg[0].eFmt)
			{
				SOPWMToMove(psState, psInst, 0, &bMovesGenerated);
				bChanges = IMG_TRUE;
			}
			if (psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
				psInst->asArg[0].uNumber == 0 &&
				psInst->asDest[0].eFmt == psInst->asArg[1].eFmt)
			{
				SOPWMToMove(psState, psInst, 1, &bMovesGenerated);
				bChanges = IMG_TRUE;
			}
		}
		/*
			Remove unnecessary swizzles on a SOPWM.
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->auDestMask[0] == 0x8)
		{
			if (psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC1ALPHA)
			{
				psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC1;
				bChanges = IMG_TRUE;
			}
			if (psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2ALPHA)
			{
				psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2;
				bChanges = IMG_TRUE;
			}
			if (psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_SRC1ALPHA)
			{
				psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_SRC1;
				bChanges = IMG_TRUE;
			}
			if (psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_SRC2ALPHA)
			{
				psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_SRC2;
				bChanges = IMG_TRUE;
			}
		}
		/*
			Check for a multiply with no effect.
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			(
				psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 ||
				psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2ALPHA
			) &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSopWm->bComplementSel1 &&
			!psInst->u.psSopWm->bComplementSel2 &&
			psInst->auDestMask[0] == psInst->auLiveChansInDest[0])
		{
			/*
				Check for one of the arguments being one and no format conversion.
			*/
			if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
				psInst->asArg[1].uNumber == USC_U8_VEC4_ONE)
			{
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_U8);

				psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
				psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;

				bChanges = IMG_TRUE;
			}
			else
			{
				if (psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
					psInst->asArg[0].uNumber == USC_U8_VEC4_ONE)
				{
					MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
					SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_U8);

					psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
					psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
	
					bChanges = IMG_TRUE;
				}
			}
		}
		/*
			Check for a SOPWM which is equivalent to a move.
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->asDest[0].eFmt == psInst->asArg[0].eFmt &&
			psInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			!psInst->u.psSopWm->bComplementSel2)
		{
			if (psInst->asDest[0].uType == psInst->asArg[0].uType &&
				psInst->asDest[0].uNumber == psInst->asArg[0].uNumber &&
				psInst->asArg[0].uIndexType == USC_REGTYPE_NOINDEX)
			{
				RemoveInst(psState, psBlock, psInst);
				FreeInst(psState, psInst);
				continue;
			}
			else if (psInst->auDestMask[0] == psInst->auLiveChansInDest[0])
			{
				SetOpcode(psState, psInst, IMOV);
				psInst->auDestMask[0] = USC_DESTMASK_FULL;
				bMovesGenerated = IMG_TRUE;
				bChanges = IMG_TRUE;
			}
			else if (g_abSingleBitSet[psInst->auDestMask[0]])
			{
				if (psInst->asDest[0].eFmt == UF_REGFORMAT_U8)
				{
					SetOpcode(psState, psInst, IPCKU8U8);
				}
				else
				{
					ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_C10);
					SetOpcode(psState, psInst, IPCKC10C10);
				}
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, g_aiSingleComponent[psInst->auDestMask[0]]);
				bMovesGenerated = IMG_TRUE;
				bIntegerMovesGenerated = IMG_TRUE;
				bChanges = IMG_TRUE;
			}
		}

		if (psInst->eOpcode == IPCKU8U8 || psInst->eOpcode == IPCKC10C10)
		{
			/*
				Check for a pack which is equivalent to a move.
			*/
			if (ReducePCKToMove(psState, psInst))
			{
				bChanges = IMG_TRUE;
				bMovesGenerated = IMG_TRUE;
			}
			else if (ConvertPartialDestToSrc(psState, psInst))
			{
				bChanges = IMG_TRUE;
			}
		}
		if ((psInst->eOpcode == IPCKU8F32 || psInst->eOpcode == IPCKU8U8) &&
			psInst->auDestMask[0] == USC_DESTMASK_FULL &&
			psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[0].uNumber == 0 &&
			psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[1].uNumber == 0)
		{
			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0].eFmt = UF_REGFORMAT_U8;
			bMovesGenerated = IMG_TRUE;
		}

		/*
			Check for a pack with no effect.
		*/
		if ((psInst->eOpcode == IPCKU8U8 || psInst->eOpcode == IPCKC10C10) &&
			psInst->asDest[0].uType == psInst->asArg[0].uType &&
			psInst->asDest[0].uNumber == psInst->asArg[0].uNumber &&
			psInst->asArg[0].uIndexType == USC_REGTYPE_NOINDEX &&
			g_auSetBitCount[psInst->auDestMask[0]] <= 2)
		{
			IMG_UINT32	uChan;

			for (uChan = 0; (psInst->auDestMask[0] & (1U << uChan)) == 0; uChan++);

			if (GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == uChan)
			{
				psInst->auDestMask[0] &= ~(1U << uChan);

				if (psInst->auDestMask[0] == 0)
				{
					RemoveInst(psState, psBlock, psInst);
					FreeInst(psState, psInst);
					continue;
				}
				else
				{
					MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
				}
				bChanges = IMG_TRUE;
			}
		}

		/*
			Check for a combine of two 10-bit registers where channels
			are only required from one.
		*/
		if (psInst->eOpcode == ICOMBC10)
		{
			if ((psInst->auLiveChansInDest[0] & USC_W_CHAN_MASK) == 0)
			{
				SetOpcode(psState, psInst, IMOV);
				bMovesGenerated = IMG_TRUE;
			}
			else if ((psInst->auLiveChansInDest[0] & USC_XYZ_CHAN_MASK) == 0)
			{
				SetOpcode(psState, psInst, IPCKC10C10);
				MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, COMBC10_ALPHA_SOURCE);
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, 0 /* uComponent */);
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_C10);
				bChanges = IMG_TRUE;
			}
		}
		/*
			Check for a MIN/MAX with no effect.
		*/
		if (DropRedundantMinMax(psState, psInst))
		{
			bChanges = IMG_TRUE;
		}
	}

	if (bIntegerMovesGenerated)
	{
		EliminateIntegerMoves(psState, psBlock, &bMovesGenerated);
	}
	if (bMovesGenerated)
	{
		EliminateMovesBP(psState, psBlock);
	}

	return bChanges;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveBanks
    
 PURPOSE	: Check if substituting the source of an integer component move can't
			  be done because of restrictions on source banks.

 PARAMETERS	: psState			- Compiler state.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovSrc			- Move source.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveBanks(PINTERMEDIATE_STATE	psState,
											   PINST				psNextInst,
											   PARG					psMovSrc,
											   PSOURCE_VECTOR		psSourceVector,
											   IMG_BOOL				bCheckOnly)
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < psNextInst->uArgumentCount; uArg++)
	{
		if (!CheckSourceVector(psSourceVector, uArg))
		{
			continue;
		}
		/*
			Nothing to do if the source bank is supported.
		*/
		if (CanUseSrc(psState, psNextInst, uArg, psMovSrc->uType, psMovSrc->uIndexType))
		{
			continue;
		}
		/*
			For SOP3 with ALPHA=SRC1*SRC0+SRC2 swap arguments 0 and 1.
		*/
		if (
				uArg == 0 &&
				psNextInst->eOpcode == ISOP3 &&
				psNextInst->auLiveChansInDest[0] == USC_ALPHA_CHAN_MASK &&
				psNextInst->u.psSop3->uCoissueOp == USEASM_OP_ASOP &&
				(
					psNextInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_SRC0 ||
					psNextInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_SRC0ALPHA
				) &&
				!(
					psNextInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_SRC0 ||
					psNextInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_SRC0ALPHA
				) &&
				!psNextInst->u.psSop3->bComplementASel1 &&
				!CheckSourceVector(psSourceVector, 1 /* uSrc */)
			)
		{
			if (!bCheckOnly)
			{
				SwapInstSources(psState, psNextInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
			}

			continue;
		}
		/*
			For SOP3 with ALPHA=SRC1+SRC2*SRC0 swap arguments 0 and 2.
		*/
		if (
				uArg == 0 &&
				psNextInst->eOpcode == ISOP3 &&
				psNextInst->auLiveChansInDest[0] == (1U << 3) &&
				psNextInst->u.psSop3->uCoissueOp == USEASM_OP_ASOP &&
				!(
					psNextInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_SRC0 ||
					psNextInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_SRC0ALPHA
				) &&
				(
					psNextInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_SRC0 ||
					psNextInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_SRC0ALPHA
				) &&
				!psNextInst->u.psSop3->bComplementASel2 &&
				!CheckSourceVector(psSourceVector, 2 /* uSrc */)
			)
		{
			if (!bCheckOnly)
			{
				SwapInstSources(psState, psNextInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
			}

			continue;
		}

		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_ComplexOp
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_ComplexOp(PINTERMEDIATE_STATE	psState,
														  PCODEBLOCK			psBlock,
														  PINST					psNextInst,
														  PARG					psMovDest,
														  IMG_UINT32			uMovDestComp,
														  UF_REGFORMAT			eSrcFormat,
														  IMG_UINT32			uMovSrcComp,
														  PSOURCE_VECTOR		psSourceVector,
														  IMG_BOOL				bCheckOnly)
{
	IMG_UINT32	uComponentSelect = GetComponentSelect(psState, psNextInst, 0 /* uArgIdx */);

	ASSERT(CheckSourceVector(psSourceVector, COMPLEXOP_SOURCE));
	ASSERT(uComponentSelect == uMovDestComp);

	/*
		The scalar instructions don't support conversion from F16/F32->C10			
	*/
	if (eSrcFormat == UF_REGFORMAT_F32 || eSrcFormat == UF_REGFORMAT_F16)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	ASSERT(psMovDest->eFmt == UF_REGFORMAT_C10);
	ASSERT(psNextInst->asArg[0].eFmt == UF_REGFORMAT_C10);

	if (!bCheckOnly)
	{
		/*
			For a scalar instruction the source swizzle affects
			which channels of the destination is written so insert
			an instruction to swizzle the result to the correct
			channel.
		*/
		if (uComponentSelect != uMovSrcComp)
		{
			PINST		psSwizzleInst;
			IMG_UINT32	uSwizzledTempNum = GetNextRegister(psState);

			psSwizzleInst = AllocateInst(psState, psNextInst);

			SetOpcode(psState, psSwizzleInst, IPCKC10C10);
			MoveDest(psState, psSwizzleInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
			psSwizzleInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
			psSwizzleInst->auDestMask[0] = 1 << uComponentSelect;
			SetSrc(psState, psSwizzleInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSwizzledTempNum, UF_REGFORMAT_C10);
			SetPCKComponent(psState, psSwizzleInst, 0 /* uArgIdx */, uMovSrcComp);
			psSwizzleInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psSwizzleInst->asArg[1].uNumber = 0;
			InsertInstBefore(psState, psBlock, psSwizzleInst, psNextInst->psNext);
	
			SetDest(psState, psNextInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSwizzledTempNum, UF_REGFORMAT_C10);
			psNextInst->auLiveChansInDest[0] = 1 << uMovSrcComp;
		}
		SetComponentSelect(psState, psNextInst, 0 /* uArgIdx */, uMovSrcComp);
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_PCK
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_PCK(PINTERMEDIATE_STATE	psState,
												    PCODEBLOCK			psBlock,
												    PINST				psNextInst,
												    UF_REGFORMAT		eSrcFormat,
												    IMG_UINT32			uMovSrcComp,
												    PSOURCE_VECTOR		psSourceVector,
												    IMG_BOOL			bCheckOnly)
{
	if	(eSrcFormat == UF_REGFORMAT_F16 || eSrcFormat == UF_REGFORMAT_F32)
	{
		IMG_UINT32	uArg;

		if (psNextInst->eOpcode == IUNPCKF32U8)
		{
			/*
				Don't replace F32->U8->F32 by F32->F32 since that removes
				the clamp to [0..1].
			*/
			return IMG_FALSE;
		}

		/*
			These instructions can pack two registers simultaneously so
			check that we can change the format of both of the sources.
		*/
		for (uArg = 0; uArg < psNextInst->uArgumentCount; uArg++)
		{
			if (
					!CheckSourceVector(psSourceVector, uArg) &&
					GetLiveChansInArg(psState, psNextInst, uArg) != 0
			   )
			{
				return IMG_FALSE;
			}
		}
		if (!bCheckOnly)
		{
			SetPCKComponent(psState, psNextInst, 0 /* uArgIdx */, uMovSrcComp);
			SetPCKComponent(psState, psNextInst, 1 /* uArgIdx */, uMovSrcComp);
			if (eSrcFormat == UF_REGFORMAT_F32)
			{
				switch (psNextInst->eOpcode)
				{
					case IUNPCKF32C10: 
					{
						SetOpcode(psState, psNextInst, IMOV);
						psBlock->uFlags |= NEED_GLOBAL_MOVE_ELIM;
						break;
					}
					case IUNPCKF16C10:
					case IUNPCKF16U8:
					{
						ModifyOpcode(psState, psNextInst, IPCKF16F32); 
						psNextInst->u.psPck->bScale = IMG_FALSE;
						break;
					}
					case IPCKU8U8: 
					{
						ModifyOpcode(psState, psNextInst, IPCKU8F32); 
						psNextInst->u.psPck->bScale = IMG_TRUE;
						break;
					}
					case IPCKC10C10: 
					{
						ModifyOpcode(psState, psNextInst, IPCKC10F32); 
						psNextInst->u.psPck->bScale = IMG_TRUE;
						break;
					}
					default: imgabort();
				}
			}
			else
			{
				switch (psNextInst->eOpcode)
				{
					case IUNPCKF32C10: 
					{
						ModifyOpcode(psState, psNextInst, IUNPCKF32F16);
						psNextInst->u.psPck->bScale = IMG_FALSE;
						break;
					}
					case IUNPCKF16C10:
					case IUNPCKF16U8:
					{
						ModifyOpcode(psState, psNextInst, IPCKF16F16); 
						psNextInst->u.psPck->bScale = IMG_FALSE;
						break;
					}
					case IPCKU8U8: 
					{
						ModifyOpcode(psState, psNextInst, IPCKU8F16); 
						psNextInst->u.psPck->bScale = IMG_TRUE;
						break;
					}
					case IPCKC10C10: 
					{
						ModifyOpcode(psState, psNextInst, IPCKC10F16); 
						psNextInst->u.psPck->bScale = IMG_TRUE;
						break;
					}
					default: imgabort();
				}
			}
		}
	}
	else
	{
		if (!bCheckOnly)
		{
			if (CheckSourceVector(psSourceVector, PCK_SOURCE0))
			{
				SetPCKComponent(psState, psNextInst, 0 /* uArgIdx */, uMovSrcComp);
			}
			if (CheckSourceVector(psSourceVector, PCK_SOURCE1))
			{
				SetPCKComponent(psState, psNextInst, 1 /* uArgIdx */, uMovSrcComp);
			}
		}
	}
	return IMG_TRUE;
}


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_PCKVec
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change - for vector packs.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_PCKVec(PINTERMEDIATE_STATE	psState,
													   PINST				psNextInst,
													   UF_REGFORMAT			eSrcFormat,
													   IMG_UINT32			uMovSrcComp,
													   PSOURCE_VECTOR		psSourceVector,
													   PARG					psMovSrc,
													   IMG_BOOL				bCheckOnly)
{
	IMG_UINT32 uChan;

	ASSERT(eSrcFormat == UF_REGFORMAT_U8);
	ASSERT(psNextInst->eOpcode == IVPCKFLTU8);
	ASSERT(CheckSourceVector(psSourceVector, PCK_SOURCE0));
	
	if (bCheckOnly)
	{
		return IMG_TRUE;
	}

	SetSrcFromArg(psState, psNextInst, 0, psMovSrc);
	
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		psNextInst->u.psVec->auSwizzle[0] = SetChan(psNextInst->u.psVec->auSwizzle[0], uChan, uMovSrcComp);
	}

	return IMG_TRUE;
}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)


/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_COMBC10
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  psMovSrc			- Move source.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_COMBC10(PINTERMEDIATE_STATE	psState,
														PARG				psMovDest,
													    UF_REGFORMAT		eSrcFormat,
													    PSOURCE_VECTOR		psSourceVector,
													    IMG_BOOL			bCheckOnly)
{
	/*
		This instruction reads 3 C10 components from the first
		arg, and a single component (always component 0)
		from the second arg.

		If it is reading the mov destination component through the
		first source, then we cannot eliminate the move.

		If it's reading the mov destination component through
		the second source, and we have previously encountered a
		write dependency, then we cannot eliminate the move.
	*/
	ASSERT(psMovDest->eFmt == UF_REGFORMAT_C10);
	if	(CheckSourceVector(psSourceVector, COMBC10_RGB_SOURCE))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if	(CheckSourceVector(psSourceVector, COMBC10_ALPHA_SOURCE))
	{
		if	(eSrcFormat == UF_REGFORMAT_F16 ||
			 eSrcFormat == UF_REGFORMAT_F32)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_TEST
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_TEST(PINTERMEDIATE_STATE	psState,
													 PINST					psNextInst,
													 IMG_UINT32				uMovDestComp,
													 UF_REGFORMAT			eSrcFormat,
													 IMG_UINT32				uMovSrcComp,
													 PSOURCE_VECTOR			psSourceVector,
													 IMG_BOOL				bCheckOnly)
{
	IMG_UINT32	uArg;
	IMG_UINT32	uOtherArg;

	if (CheckSourceVector(psSourceVector, FPTEST_SOURCE0))
	{
		uArg = 0;
		uOtherArg = 1;
	}
	else
	{
		uArg = 1;
		uOtherArg = 0;
	}

	if (eSrcFormat == UF_REGFORMAT_F16 || eSrcFormat == UF_REGFORMAT_F32)
	{
		/*
			Check for a test instruction referencing only arguments which can be
			converted to float.
		*/
		if (psNextInst->eOpcode != ITESTPRED)
		{
			return IMG_FALSE;
		}
		if (psNextInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT)
		{
			return IMG_FALSE;
		}
		if (
				!(
					(
						CheckSourceVector(psSourceVector, FPTEST_SOURCE0) &&
						CheckSourceVector(psSourceVector, FPTEST_SOURCE1)
					) ||
					(
						psNextInst->asArg[uOtherArg].uType == USEASM_REGTYPE_IMMEDIATE &&
						psNextInst->asArg[uOtherArg].uNumber == 0
					)
				 )
		    )
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		if (!bCheckOnly)
		{
			IMG_UINT32	uArgIdx;

			/*
				Change the test opcode to the equivalent floating point operation.
			*/
			switch (psNextInst->u.psTest->eAluOpcode)
			{
				case IFPADD8: psNextInst->u.psTest->eAluOpcode = IFADD; break;
				case IFPSUB8: psNextInst->u.psTest->eAluOpcode = IFSUB; break;
				default: imgabort();
			}
			psNextInst->u.psTest->sTest.eChanSel = USEASM_TEST_CHANSEL_C0;

			/*
				Change formats on the arguments we didn't replace.
			*/
			for (uArgIdx = 0; uArgIdx < psNextInst->uArgumentCount; uArgIdx++)
			{
				if (!CheckSourceVector(psSourceVector, uArgIdx))
				{
					ASSERT(psNextInst->asArg[uArgIdx].uType == USEASM_REGTYPE_IMMEDIATE);
					ASSERT(psNextInst->asArg[uArgIdx].uNumber == 0);
					psNextInst->asArg[uArgIdx].eFmt = UF_REGFORMAT_F32;
				}
			}

			/*
				Check if the test might be simplified.
			*/
			SimplifyTESTPRED(psState, psNextInst);
		}
	}
	else
	{
		static const USEASM_TEST_CHANSEL	aeChanToSel[] =
		{
			USEASM_TEST_CHANSEL_C0,		/* USC_X_CHAN */
			USEASM_TEST_CHANSEL_C1,		/* USC_Y_CHAN */
			USEASM_TEST_CHANSEL_C2,		/* USC_Z_CHAN */
			USEASM_TEST_CHANSEL_C3,		/* USC_W_CHAN */
		};

		ASSERT(psNextInst->u.psTest->sTest.eChanSel == aeChanToSel[uMovDestComp]);

		/*
			If we are changing the source component for a test which does a writeback we
			would need to swizzle the destination.
		*/
		if (psNextInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT && 
			uMovDestComp != uMovSrcComp)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		if (uMovDestComp != uMovSrcComp)
		{
			/*
				If we are changing component check it is okay to change the other source too.
			*/
			if (
					!(
						(
							CheckSourceVector(psSourceVector, FPTEST_SOURCE0) &&
							CheckSourceVector(psSourceVector, FPTEST_SOURCE1)
						) || 
						AllChannelsTheSameInSource(&psNextInst->asArg[uOtherArg])
					  )
			   )
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}
		if (!bCheckOnly)
		{
			/* Update the channel that is used for the test. */
			psNextInst->u.psTest->sTest.eChanSel = aeChanToSel[uMovSrcComp];
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat_INTARITH
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  psMovSrc			- Move source.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Bit mask of the source being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat_INTARITH(PINTERMEDIATE_STATE	psState,
														 PINST					psNextInst,
														 PARG					psMovDest,
														 IMG_UINT32				uMovDestComp,
														 UF_REGFORMAT			eSrcFormat,
														 PARG					psMovSrc,
														 IMG_UINT32				uMovSrcComp,
														 PSOURCE_VECTOR			psSourceVector,
														 IMG_BOOL				bCheckOnly)
{
	IMG_BOOL	bSwizzle;

	if (eSrcFormat == UF_REGFORMAT_F16 || eSrcFormat == UF_REGFORMAT_F32)
	{
		/*
			Check for a hardware constant - we can replace the F32/F16 form of the constant
			by the U8 form.
		*/
		if (
				psMovSrc->uType == USEASM_REGTYPE_FPCONSTANT &&
				(
					(
						eSrcFormat == UF_REGFORMAT_F32 &&
						psMovSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1 
					) ||
					(
						eSrcFormat == UF_REGFORMAT_F16 &&
						psMovSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE
					) ||
					psMovSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
				)
			)
		{
			if (!bCheckOnly)
			{
				IMG_UINT32	uArgIdx;

				for (uArgIdx = 0; uArgIdx < psNextInst->uArgumentCount; uArgIdx++)
				{
					PARG	psArg = &psNextInst->asArg[uArgIdx];

					if (CheckSourceVector(psSourceVector, uArgIdx))
					{
						ASSERT(psArg->uType == USEASM_REGTYPE_FPCONSTANT);
						psArg->uType = USEASM_REGTYPE_IMMEDIATE;

						if (psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
						{
							ASSERT(psArg->eFmt == UF_REGFORMAT_F32);
							psArg->uNumber = USC_U8_VEC4_ONE;
						}
						else if (psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
						{
							ASSERT(psArg->eFmt == UF_REGFORMAT_F16);
							psArg->uNumber = USC_U8_VEC4_ONE;
						}
						else
						{
							ASSERT(psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO);
							psArg->uNumber = 0;
						}
						psArg->eFmt = UF_REGFORMAT_U8;
					}
				}
			}
			return IMG_TRUE;
		}

		/*
			Otherwise integer ALU instructions can't accept floating point data.
		*/
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Check if a swizzle would be needed to replace this component.
	*/
	bSwizzle = IMG_TRUE;
	if (uMovDestComp == uMovSrcComp)
	{
		bSwizzle = IMG_FALSE;
	}

	/*
		For unified store registers the hardware takes the alpha values
		from the red channel. Check if we can take advantage of this
		to avoid a swizzle.
	*/
	if (psMovDest->eFmt == UF_REGFORMAT_C10 &&
		psMovDest->uType == USEASM_REGTYPE_TEMP &&
		psMovSrc->uType != USEASM_REGTYPE_TEMP &&
		uMovDestComp == 3 &&
		uMovSrcComp == 0)
	{
		bSwizzle = IMG_FALSE;
	}
	if (bSwizzle)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: CheckIntegerComponentMoveFormat
    
 PURPOSE	: Check if the source to an integer component move can be substituted
			  for the destination in an instruction given a channel or format
			  change.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block containing the instruction.
			  psNextInst		- Instruction whose source are being replaced.
			  psMovDest			- Move destination.
			  uMovDestComp		- Component which is written by the move.
			  bFromFloat		- Is the move also a conversion from F32.
			  psMovSrc			- Move source.
			  uMovSrcComp		- Component which is read by the move.
			  psSourceVector	- Mask of the arguments being replaced.
			  bCheckOnly		- If TRUE just check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL CheckIntegerComponentMoveFormat(PINTERMEDIATE_STATE	psState,
												PCODEBLOCK			psBlock,
											    PINST				psNextInst,
												PARG				psMovDest,
												IMG_UINT32			uMovDestComp,
												UF_REGFORMAT		eSrcFormat,
											    PARG				psMovSrc,
												IMG_UINT32			uMovSrcComp,
											    PSOURCE_VECTOR		psSourceVector,
											    IMG_BOOL			bCheckOnly)
{
	/*
		We cannot replace the move destination component with the source if
		it is read along with any other components in the move destination
		register. In other words, all reads of the move destination component
		must be of that component only.
	*/
	switch (psNextInst->eOpcode)
	{
		case IFRCP:
		case IFRSQ:
		case IFEXP:
		case IFLOG:
		#if defined(SUPPORT_SGX545)
		case IFSIN:
		case IFCOS:
		case IFSQRT:
		#endif /* defined(SUPPORT_SGX545) */
		{
			if (!CheckIntegerComponentMoveFormat_ComplexOp(psState,
														   psBlock,
														   psNextInst,
														   psMovDest,
														   uMovDestComp,
														   eSrcFormat,
														   uMovSrcComp,
														   psSourceVector,
														   bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}
		case IUNPCKF16C10:
		case IUNPCKF16U8:
		case IUNPCKF32C10:
		case IUNPCKF32U8:
		case IPCKU8U8:
		case IPCKC10C10:
		{
			if (!CheckIntegerComponentMoveFormat_PCK(psState,
												     psBlock,
													 psNextInst,
													 eSrcFormat,
													 uMovSrcComp,
													 psSourceVector,
													 bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVPCKFLTU8:
		{
			if (!CheckIntegerComponentMoveFormat_PCKVec(psState,
														psNextInst,
														eSrcFormat,
														uMovSrcComp,
														psSourceVector,
														psMovSrc,
														bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case ICOMBC10:
		{
			if (!CheckIntegerComponentMoveFormat_COMBC10(psState,
														 psMovDest,
														 eSrcFormat,
														 psSourceVector,
														 bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}

		case ITESTPRED:
		{
			if (!CheckIntegerComponentMoveFormat_TEST(psState,
													  psNextInst,
													  uMovDestComp,
													  eSrcFormat,
													  uMovSrcComp,
													  psSourceVector,
													  bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}

		case IFPMA:
		case ISOP3:
		case ISOP2:
		case ISOPWM:
		case ILRP1:
		{
			if (!CheckIntegerComponentMoveFormat_INTARITH(psState,
														  psNextInst,
														  psMovDest,
														  uMovDestComp,
														  eSrcFormat,
														  psMovSrc,
														  uMovSrcComp,
														  psSourceVector,
														  bCheckOnly))
			{
				return IMG_FALSE;
			}
			break;
		}
		default:
		{
			/*
				Assume all other instructions will reference multiple components
				of the move destination register.

				NB: This isn't strictly true (SOPxx can access the alpha
				channel on it's own for example), but it keeps the code
				here simpler for now.
			*/
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: GlobalSubstComplement
    
 PURPOSE	: Replace occurences of a U8/C10 temporary in an instruction by
			  by a use of the complement of another register.

 PARAMETERS	: psState			- Internal compiler state
			  psNextInst		- Instruction to change.
			  psSourceVector	- Mask of source arguments to change.
			  pvContext			- Points to the replacement argument.
			  bCheckOnly		- If TRUE only check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static
IMG_BOOL GlobalSubstComplement(PINTERMEDIATE_STATE	psState,
							   PINST				psInst,
							   PSOURCE_VECTOR		psSourceVector,
							   IMG_PVOID			pvContext,
							   IMG_BOOL				bCheckOnly)
{
	PARG psArg = (PARG)pvContext;	
	IMG_UINT32 uSrcIdx;

	/*
		Replace the source arguments in the mask.
	*/
	if (psInst->eOpcode == IFPMA)
	{
		if (!bCheckOnly)
		{
			for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
			{
				if (CheckSourceVector(psSourceVector, uSrcIdx))
				{
					SetSrcFromArg(psState, psInst, uSrcIdx, psArg);
					switch (uSrcIdx)
					{
						case 0:
						{
							psInst->u.psFpma->bComplementCSel0 = IMG_TRUE;
							psInst->u.psFpma->bComplementASel0 = IMG_TRUE;
							break;
						}
						case 1:
						{
							psInst->u.psFpma->bComplementCSel1 = IMG_TRUE;
							psInst->u.psFpma->bComplementASel1 = IMG_TRUE;
							break;
						}
						case 2:
						{
							psInst->u.psFpma->bComplementCSel2 = IMG_TRUE;
							psInst->u.psFpma->bComplementASel2 = IMG_TRUE;
							break;
						}
						default: imgabort();
					}
				}
			}
		}
		return IMG_TRUE;
	}
	else if (psInst->eOpcode == ISOPWM)
	{
		IMG_BOOL	bAlphaSwizzle;

		if (!IsSOPWMMultiply(psInst, &bAlphaSwizzle))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		if (CheckSourceVector(psSourceVector, 0 /* uSrcIdx */) && CheckSourceVector(psSourceVector, 1 /* uSrcIdx */))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		if (bAlphaSwizzle && CheckSourceVector(psSourceVector, 0 /* uSrcIdx */))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}

		if (!bCheckOnly)
		{
			if (CheckSourceVector(psSourceVector, 0 /* uSrcIdx */))
			{
				SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
			}
			else
			{
				ASSERT(CheckSourceVector(psSourceVector, 1 /* uSrcIdx */));
			}
			SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, psArg);
		
			psInst->u.psSopWm->bComplementSel1 = !psInst->u.psSopWm->bComplementSel1;
		}
		return IMG_TRUE;
	}
	else
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
}

/*****************************************************************************
 FUNCTION	: GlobalReplaceIntegerComponentMoveArgument
    
 PURPOSE	: Replace a component of a U8/C10 temporary.

 PARAMETERS	: psState			- Internal compiler state
			  psNextInst		- Instruction to change.
			  psSourceVector	- Mask of source arguments to change.
			  pvContext			- Information about the component to replace.
			  bCheckOnly		- If TRUE only check if replacement is possible.
			  
 RETURNS	: TRUE if replacement is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL GlobalReplaceIntegerComponentMoveArguments(PINTERMEDIATE_STATE	psState,
														   PINST				psInst,
														   PSOURCE_VECTOR		psSourceVector,
														   IMG_PVOID			pvContext,
														   IMG_BOOL				bCheckOnly)
{
	INTEGER_COMPONENT_MOVE_CONTEXT*	psCtx = (INTEGER_COMPONENT_MOVE_CONTEXT*)pvContext;	
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
				SetSrcFromArg(psState, psInst, uSrcIdx, psCtx->psMovSrc);
			}
		}
	}

	if (!CheckIntegerComponentMoveBanks(psState, psInst, psCtx->psMovSrc, psSourceVector, bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if (!CheckIntegerComponentMoveFormat(psState, 
										 psInst->psBlock, 
										 psInst, 
										 psCtx->psMovDest, 
										 psCtx->uMovDestComp, 
										 psCtx->eSrcFormat, 
										 psCtx->psMovSrc,
										 psCtx->uMovSrcComp,
										 psSourceVector, 
										 bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: EliminateAlphaSwizzleReplace
    
 PURPOSE	: Rewrite an instruction so an alpha replicate is done on a mask of
			  arguments.

 PARAMETERS	: psState			- Internal compiler state
			  psNextInst		- Instruction to change.
			  psSourceVector	- Mask of source arguments to change.
			  pvNewSrc			- If bCheckOnly is FALSE then replace the source
								arguments by this argument.
			  bCheckOnly		- If TRUE only check if rewriting is possible.
			  
 RETURNS	: TRUE if rewriting is possible; FALSE otherwise.
*****************************************************************************/
static IMG_BOOL EliminateAlphaSwizzleReplace(PINTERMEDIATE_STATE	psState,
											 PINST					psNextInst,
											 PSOURCE_VECTOR			psSourceVector,
											 IMG_PVOID				pvNewSrc,
											 IMG_BOOL				bCheckOnly)
{
	PARG		psNewSrc = (PARG)pvNewSrc;
	IMG_UINT32	uSrcIdx;

	if (!bCheckOnly)
	{
		/*
			Replace all of the source arguments in the mask by the move instruction's source.
		*/
		for (uSrcIdx = 0; uSrcIdx < psNextInst->uArgumentCount; uSrcIdx++)
		{
			if (CheckSourceVector(psSourceVector, uSrcIdx))
			{
				SetSrcFromArg(psState, psNextInst, uSrcIdx, psNewSrc);
			}
		}
	}

	switch (psNextInst->eOpcode)
	{
		case IFPMA:
		{
			if (bCheckOnly)
			{
				/*
					Every source can use an alpha swizzle.
				*/
				return IMG_TRUE;
			}
			/*
				Replace the argument's source select by the corresponding alpha replicate
				select.
			*/
			if (CheckSourceVector(psSourceVector, 0 /* uSrc */))
			{
				ASSERT(psNextInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0 ||
					   psNextInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0ALPHA);
				psNextInst->u.psFpma->uCSel0 = USEASM_INTSRCSEL_SRC0ALPHA;
			}
			if (CheckSourceVector(psSourceVector, 1 /* uSrc */))
			{
				if (psNextInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1 ||
					psNextInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1ALPHA)
				{
					psNextInst->u.psFpma->uCSel0 = USEASM_INTSRCSEL_SRC1ALPHA;
				}
				psNextInst->u.psFpma->uCSel1 = USEASM_INTSRCSEL_SRC1ALPHA;
			}
			if (CheckSourceVector(psSourceVector, 2 /* uSrc */))
			{
				psNextInst->u.psFpma->uCSel2 = USEASM_INTSRCSEL_SRC2ALPHA;
			}
			return IMG_TRUE;
		}
		case ISOP3:
		{
			/*
				Source 0 can only be referrred to through a source selector.
			*/
			if (CheckSourceVector(psSourceVector, 1 /* uSrc */) || CheckSourceVector(psSourceVector, 2 /* uSrc */))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			ASSERT(CheckSourceVector(psSourceVector, 0 /* uSrc */));
			
			if (!bCheckOnly)
			{
				/*
					Replace references to source 0.
				*/
				if (psNextInst->u.psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0)
				{
					psNextInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
				}
				if (psNextInst->u.psSop3->uCSel2 == USEASM_INTSRCSEL_SRC0)
				{
					psNextInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_SRC0ALPHA;
				}
			}
			return IMG_TRUE;
		}
		case ISOPWM:
		{
			if (CheckSourceVector(psSourceVector, 0 /* uSrc */) && !CheckSourceVector(psSourceVector, 1 /* uSrc */))
			{
				/*
					Check for calculating: SRC1 * SRC2 or SRC1 * (1 - SRC2)
				*/
				if (
						psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
						psNextInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
						psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 &&
						psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
						!psNextInst->u.psSopWm->bComplementSel2
					)
				{
					if (!psNextInst->u.psSopWm->bComplementSel1)
					{
						/*
							Swap the order of the two sources so an alpha swizzle can be applied
							to (what was) the first source.
						*/
						if (!bCheckOnly)
						{
							SwapInstSources(psState, psNextInst, 0 /* uSrcIdx */, 1 /* uSrcIdx */);
							psNextInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2ALPHA;
						}
						return IMG_TRUE;
					}
					/*
						Check if the SOPWM instruction needs a destination write mask.
					*/
					if ((psNextInst->auLiveChansInDest[0] & ~psNextInst->auDestMask[0]) == 0)
					{
						/*
							Convert the SOPWM instruction to a SOP2 instruction so we can
							apply a complement modifier to one source and an alpha replicate
							modifier to the other.
						*/
						if (!bCheckOnly)
						{
							SwapInstSources(psState, psNextInst, 0 /* uSrcIdx */, 1 /* uSrcIdx */);

							SetOpcode(psState, psNextInst, ISOP2);
							psNextInst->auDestMask[0] = USC_ALL_CHAN_MASK;

							/*
								DEST = (1 - SRC1) * SRC2ALPHA
							*/
							psNextInst->u.psSop2->uCSel1 = USEASM_INTSRCSEL_SRC2ALPHA;
							psNextInst->u.psSop2->bComplementCSel1 = IMG_FALSE;
							psNextInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_SRC2ALPHA;
							psNextInst->u.psSop2->bComplementASel1 = IMG_FALSE;

							psNextInst->u.psSop2->bComplementCSrc1 = IMG_TRUE;
							psNextInst->u.psSop2->bComplementASrc1 = IMG_TRUE;

							psNextInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
							psNextInst->u.psSop2->bComplementCSel2 = IMG_FALSE;
							psNextInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
							psNextInst->u.psSop2->bComplementASel2 = IMG_FALSE;

							psNextInst->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
							psNextInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
						}
						return IMG_TRUE;
					}
				}
			}

			/*
				Check that the argument is referenced to only through a source selector.
			*/
			if (CheckSourceVector(psSourceVector, 0 /* uSrc */))
			{
				if (!(psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO && !psNextInst->u.psSopWm->bComplementSel1))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
				if (!bCheckOnly && psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_SRC1)
				{
					psNextInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_SRC1ALPHA;
				}
			}
			if (CheckSourceVector(psSourceVector, 1 /* uSrc */))
			{
				if (!(psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO && !psNextInst->u.psSopWm->bComplementSel2))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
				if (!bCheckOnly && psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2)
				{
					psNextInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2ALPHA;
				}
			}
			return IMG_TRUE;
		}
		default:
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
}

static IMG_VOID GenerateMoveForMaskedDest(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: GenerateMoveForMaskedDest
    
 PURPOSE	: Generate MOV instructions to replace an instruction which, as part of
			  its operation, copies data from an old destination to a new destination.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to generate moves from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psOldDest != NULL)
		{
			PINST	psCopyInst;

			psCopyInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psCopyInst, IMOV);
			MoveDest(psState, psCopyInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
			MovePartialDestToSrc(psState, psCopyInst, 0 /* uMoveToSrcIdx */, psInst, uDestIdx);
			InsertInstBefore(psState, psInst->psBlock, psCopyInst, psInst);
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef struct REPLACE_FLOAT_PACK_CONTEXT
{
	PARG				psMovSrc;
	IMG_UINT32			uSrcSwizzle;
	UF_REGFORMAT		eSrcFmt;
	VEC_SOURCE_MODIFIER	sSrcMod;
	IMG_BOOL			bMovesGenerated;
}  REPLACE_FLOAT_PACK_CONTEXT, *PREPLACE_FLOAT_PACK_CONTEXT;

static IMG_BOOL GlobalReplaceFloatPackArguments(PINTERMEDIATE_STATE	psState,
											    PINST				psInst,
												PSOURCE_VECTOR		psSourceVector,
												IMG_PVOID			pvContext,
												IMG_BOOL			bCheckOnly)
/*****************************************************************************
 FUNCTION	: GlobalReplaceFloatPackArguments
    
 PURPOSE	: Helper function to check if a float to U8/C10 conversion can
			  be merged with another instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to merge with.
			  psSourceVector
							- Vector of source arguments to the instruction which
							are the result of the conversion instruction.
			  pvContext		- Information about the conversion instruction.
			  bCheckOnly	- If TRUE just check if the merge is possible.
							  If FALSE perform the merge.

 RETURNS	: TRUE if the conversion can be merged with this instruction.8
*****************************************************************************/
{
	PREPLACE_FLOAT_PACK_CONTEXT	psContext = (PREPLACE_FLOAT_PACK_CONTEXT)pvContext;

	switch (psInst->eOpcode)
	{
		case IVPCKFLTC10:
		{
			ASSERT(CheckSourceVector(psSourceVector, 0 /* uSrc */));

			if (psInst->u.psVec->asSrcMod[0].bNegate || psInst->u.psVec->asSrcMod[0].bAbs)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}

			if (!bCheckOnly)
			{
				ModifyOpcode(psState, psInst, IVMOV);
				SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psContext->psMovSrc);
				psInst->u.psVec->auSwizzle[0] = CombineSwizzles(psContext->uSrcSwizzle, psInst->u.psVec->auSwizzle[0]);
				psInst->u.psVec->aeSrcFmt[0] = psContext->eSrcFmt;
				psInst->u.psVec->bPackScale = IMG_FALSE;
				CombineSourceModifiers(&psInst->u.psVec->asSrcMod[0], 
									   &psContext->sSrcMod, 
									   &psInst->u.psVec->asSrcMod[0]);

				psContext->bMovesGenerated = IMG_TRUE;
			}
			return IMG_TRUE;
		}
		case IFRCP:
		case IFRSQ:
		case IFLOG:
		case IFEXP:
		{
			IMG_UINT32				uComponent;
			FLOAT_SOURCE_MODIFIER	sSourceMod;

			ASSERT(CheckSourceVector(psSourceVector, 0 /* uSrc */));

			uComponent = GetComponentSelect(psState, psInst, 0 /* uSrcIdx */);
			sSourceMod = *GetFloatMod(psState, psInst, 0 /* uSrcIdx */);

			switch (psInst->eOpcode)
			{
				case IFRCP: SetOpcode(psState, psInst, IVRCP); break;
				case IFRSQ: SetOpcode(psState, psInst, IVRSQ); break;
				case IFLOG: SetOpcode(psState, psInst, IVLOG); break;
				case IFEXP: SetOpcode(psState, psInst, IVEXP); break;
				default: imgabort();
			}

			SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 0);

			psInst->u.psVec->auSwizzle[0] = CombineSwizzles(psContext->uSrcSwizzle, g_auReplicateSwizzles[uComponent]);

			psInst->u.psVec->aeSrcFmt[0] = psContext->eSrcFmt;

			psInst->u.psVec->asSrcMod[0].bNegate = sSourceMod.bNegate;
			psInst->u.psVec->asSrcMod[0].bAbs = sSourceMod.bAbsolute;
			
			CombineSourceModifiers(&psInst->u.psVec->asSrcMod[0], 
								   &psContext->sSrcMod, 
								   &psInst->u.psVec->asSrcMod[0]);

			SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psContext->psMovSrc);
	
			return IMG_TRUE;
		}
		case IPCKC10C10:
		case IPCKU8U8:
		{
			IMG_UINT32	uSrc;

			/*
				Check all used sources to the PCK instruction are the results
				of the float->U8/C10 conversion instruction.
			*/
			for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
			{
				if (GetLiveChansInArg(psState, psInst, uSrc) != 0)
				{
					if (!CheckSourceVector(psSourceVector, uSrc))
					{
						ASSERT(bCheckOnly);
						return IMG_FALSE;
					}
				}
			}

			if (!bCheckOnly)
			{
				IMG_UINT32	auComponent[2];
				IMG_UINT32	uSrc;
				IMG_UINT32	uSwizzle;
				IMG_UINT32	uChan;

				for (uSrc = 0; uSrc < PCK_SOURCE_COUNT; uSrc++)
				{
					auComponent[uSrc] = GetPCKComponent(psState, psInst, uSrc);
				}

				/*
					Combine the swizzles on the float conversion instruction with the
					component selects on this instruction.
				*/
				uSrc = 0;
				uSwizzle = 0;
				for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
				{
					if ((psInst->auDestMask[0] & (1U << uChan)) != 0)
					{	
						USEASM_INTSRCSEL	eChanSel;

						eChanSel = GetChan(psContext->uSrcSwizzle, auComponent[uSrc]);
						uSwizzle = SetChan(uSwizzle, uChan, eChanSel);

						uSrc = (uSrc + 1) % PCK_SOURCE_COUNT;
					}
				}

				/*
					Convert this instruction to a float->U8/C10 conversion instruction.
				*/
				switch (psInst->eOpcode)
				{
					case IPCKC10C10: SetOpcode(psState, psInst, IVPCKC10FLT); break;
					case IPCKU8U8: SetOpcode(psState, psInst, IVPCKU8FLT); break;
					default: imgabort();
				}

				psInst->u.psVec->aeSrcFmt[0] = psContext->eSrcFmt;
				psInst->u.psVec->auSwizzle[0] = uSwizzle;
				CombineSourceModifiers(&psInst->u.psVec->asSrcMod[0], 
									   &psContext->sSrcMod, 
									   &psInst->u.psVec->asSrcMod[0]);
				psInst->u.psVec->bPackScale = IMG_TRUE;

				SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psContext->psMovSrc);
				for (uSrc = 1; uSrc < psInst->uArgumentCount; uSrc++)
				{
					SetSrcUnused(psState, psInst, uSrc);
				}
			}
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_BOOL SubstFixedPointComponentMove(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SubstFixedPointComponentMove
    
 PURPOSE	: Try to substitute the source to a move of a component of a fixed
			  point vector for its destination.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Component move instruction.

 RETURNS	: TRUE if the substitution was done and the instruction was removed.
*****************************************************************************/
{
	IMG_UINT32						uDestComp;
	IMG_UINT32						uSrcComp;
	UF_REGFORMAT					eSrcFormat;
	INTEGER_COMPONENT_MOVE_CONTEXT	sCtx;

	if (!g_abSingleBitSet[psInst->auDestMask[0]])
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}
	if (psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_FALSE;
	}
	if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_FALSE;
	}

	switch (psInst->eOpcode)
	{
		case IPCKC10F32:
		case IPCKU8F32:
		{
			eSrcFormat = UF_REGFORMAT_F32;
			break;
		}
		case IPCKC10F16:
		case IPCKU8F16:
		{
			eSrcFormat = UF_REGFORMAT_F16;
			break;
		}
		case IPCKC10C10: eSrcFormat = UF_REGFORMAT_C10; break;
		case IPCKU8U8: eSrcFormat = UF_REGFORMAT_U8; break;
		default: imgabort();
	}

	for	(uDestComp = 0; (psInst->auDestMask[0] & (1 << uDestComp)) == 0; uDestComp++);

	uSrcComp = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);

	sCtx.psMovDest = &psInst->asDest[0];
	sCtx.uMovDestComp = uDestComp;
	sCtx.eSrcFormat = eSrcFormat;
	sCtx.uMovSrcComp = uSrcComp;
	sCtx.psMovSrc = &psInst->asArg[0];

	if (ReplaceAllUsesMasked(psState, 
							 &psInst->asDest[0], 
							 psInst->auDestMask[0], 
							 psInst->apsPredSrc, 
							 psInst->uPredCount,
							 GetPredicateNegate(psState, psInst),
							 GetPredicatePerChan(psState, psInst),
							 GlobalReplaceIntegerComponentMoveArguments, 
							 (IMG_PVOID)&sCtx,
							 IMG_FALSE /* bCheckOnly */))
	{
		CopyPartiallyOverwrittenData(psState, psInst);
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL EliminateIntegerMoves(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: EliminateIntegerMoves
    
 PURPOSE	: Try to remove moves of integer channels.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- The block to optimize.
			  pbMovesGenerated
							- Set to true if new move instructions were
							generated.

 RETURNS	: TRUE if changes were made to the block.
*****************************************************************************/
{
	PINST		psInst, psNextInst;
	IMG_BOOL	bChanges = IMG_FALSE;

	PVR_UNREFERENCED_PARAMETER(pbMovesGenerated);

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (	
				(
					psInst->eOpcode == IVPCKC10FLT  ||
					psInst->eOpcode == IVPCKU8FLT
				) && 
				psInst->u.psVec->bPackScale &&
				psInst->asArg[0].uType == USEASM_REGTYPE_TEMP &&
				NoPredicate(psState, psInst)
			)
		{
			REPLACE_FLOAT_PACK_CONTEXT	sContext;

			sContext.psMovSrc = &psInst->asArg[0];
			sContext.uSrcSwizzle = psInst->u.psVec->auSwizzle[0];
			sContext.eSrcFmt = psInst->u.psVec->aeSrcFmt[0];
			sContext.sSrcMod = psInst->u.psVec->asSrcMod[0];
			sContext.bMovesGenerated = IMG_FALSE;

			if (ReplaceAllUsesMasked(psState, 
									 &psInst->asDest[0], 
									 psInst->auDestMask[0], 
									 psInst->apsPredSrc, 
									 psInst->uPredCount,
									 GetPredicateNegate(psState, psInst),
									 GetPredicatePerChan(psState, psInst),
									 GlobalReplaceFloatPackArguments, 
									 (IMG_PVOID)&sContext,
									 IMG_FALSE))
			{
				CopyPartiallyOverwrittenData(psState, psInst);
				RemoveInst(psState, psBlock, psInst);
				FreeInst(psState, psInst);
				bChanges = IMG_TRUE;
				if (sContext.bMovesGenerated)
				{
					*pbMovesGenerated = IMG_TRUE;
				}
				continue;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		if (
				psInst->eOpcode == ISOPWM &&
				NoPredicate(psState, psInst) &&
				IsSOPWMSub(psInst) &&
				psInst->asArg[0].uType == USEASM_REGTYPE_IMMEDIATE &&
				psInst->asArg[0].uNumber == 0xFFFFFFFF &&
				psInst->asArg[0].eFmt == UF_REGFORMAT_U8 &&
				!(
					psInst->asArg[1].eFmt == UF_REGFORMAT_C10 &&
					psInst->asDest[0].eFmt != UF_REGFORMAT_C10
				)
			)
		{
			if (ReplaceAllUsesMasked(psState, 
									 &psInst->asDest[0], 
									 psInst->auDestMask[0], 
									 psInst->apsPredSrc,
									 psInst->uPredCount,
									 GetPredicateNegate(psState, psInst),
									 GetPredicatePerChan(psState, psInst),
									 GlobalSubstComplement, 
									 &psInst->asArg[1] /* pvContext */,
									 IMG_FALSE))
			{
				CopyPartiallyOverwrittenData(psState, psInst);
				RemoveInst(psState, psBlock, psInst);
				FreeInst(psState, psInst);
				bChanges = IMG_TRUE;
				continue;
			}
		}
		if (
				psInst->eOpcode == IPCKC10C10 || 
				psInst->eOpcode == IPCKU8U8 || 
				psInst->eOpcode == IPCKC10F32 ||
				psInst->eOpcode == IPCKU8F32 ||
				psInst->eOpcode == IPCKC10F16 ||
				psInst->eOpcode == IPCKU8F16
		   )
		{
			if (SubstFixedPointComponentMove(psState, psInst))
			{
				bChanges = IMG_TRUE;
				continue;
			}
		}
		/*
			Try and make use of the builtin alpha replicate on integer ALU instructions to
			replace an alpha replicate done in a seperate instruction.
		*/
		{
			IMG_BOOL	bAlphaReplicate;

			bAlphaReplicate = IMG_FALSE;
			/*
				For C10 data the alpha replicate will replicate from the blue channel
				for unified store registers. Later on temporary registers might be
				mapped to internal registers so they have to be treated as none
				unified store.
			*/
			if (
					psInst->eOpcode == IPCKC10C10 &&
					(
						(
							psInst->asArg[0].uType == USEASM_REGTYPE_TEMP &&
							GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 3
						) ||
						(
							psInst->asArg[0].uType != USEASM_REGTYPE_TEMP &&
							GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 0
						)
					)
			   )
			{
				bAlphaReplicate = IMG_TRUE;
			}
			if (
					psInst->eOpcode == IPCKU8U8 &&
					GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 3
			   )
			{
				bAlphaReplicate = IMG_TRUE;
			}

			if (bAlphaReplicate &&
				EqualPCKArgs(psState, psInst) &&
				NoPredicate(psState, psInst))
			{
				if (DoReplaceAllUsesMasked(psState, 
										   &psInst->asDest[0], 
										   psInst->auDestMask[0], 
										   psInst->apsPredSrc, 
										   psInst->uPredCount,
										   GetPredicateNegate(psState, psInst),
										   GetPredicatePerChan(psState, psInst),
										   EliminateAlphaSwizzleReplace,
										   NULL /* pvCtx */,
										   IMG_TRUE /* bCheckOnly */))
				{
					bChanges = IMG_TRUE;

					DoReplaceAllUsesMasked(psState, 
										   &psInst->asDest[0], 
										   psInst->auDestMask[0], 
										   psInst->apsPredSrc, 
										   psInst->uPredCount,
										   GetPredicateNegate(psState, psInst),
										   GetPredicatePerChan(psState, psInst),
										   EliminateAlphaSwizzleReplace,
										   &psInst->asArg[0] /* pvCtx */,
										   IMG_FALSE /* bCheckOnly */);
	
					/*
						Drop the move instruction.
					*/
					GenerateMoveForMaskedDest(psState, psInst);
					RemoveInst(psState, psBlock, psInst);
					FreeInst(psState, psInst);
					continue;
				}
			}
		}
		/*
			Try and make use of the builtin alpha replicate on integer ALU instructions to
			replace a replicate to all channels pack instruction by a move to alpha. This
			doesn't save any instructions here but means we don't need an internal register
			for the destination.
		*/
		if (psInst->eOpcode == IPCKC10C10 &&
			(psInst->auDestMask[0] & psInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) != 0 &&
			(psInst->auDestMask[0] & psInst->auLiveChansInDest[0] & USC_RGB_CHAN_MASK) != 0 &&
			psInst->asArg[1].uType == psInst->asArg[0].uType &&
			psInst->asArg[1].uNumber == psInst->asArg[0].uNumber &&
			psInst->asArg[1].uIndexType == psInst->asArg[0].uIndexType &&
			psInst->asArg[1].uIndexNumber == psInst->asArg[0].uIndexNumber &&
			psInst->asArg[1].uIndexStrideInBytes == psInst->asArg[0].uIndexStrideInBytes &&
			GetPCKComponent(psState, psInst, 1 /* uArgIdx */) == GetPCKComponent(psState, psInst, 0 /* uArgIdx */) &&
			NoPredicate(psState, psInst))
		{
			if (DoReplaceAllUsesMasked(psState, 
									   &psInst->asDest[0], 
									   psInst->auDestMask[0], 
									   psInst->apsPredSrc,
									   psInst->uPredCount,
									   GetPredicateNegate(psState, psInst),
									   GetPredicatePerChan(psState, psInst),
									   EliminateAlphaSwizzleReplace,
									   NULL /* pvCtx */,
									   IMG_TRUE /* bCheckOnly */))
			{
				ARG		sRepArg;

				/*
					Create a new argument for the source swapped to the alpha channel.
				*/
				MakeNewTempArg(psState, UF_REGFORMAT_C10, &sRepArg);

				/*
					Replace references to the destination by the new argument.
				*/
				DoReplaceAllUsesMasked(psState, 
									   &psInst->asDest[0], 
									   psInst->auDestMask[0], 
									   psInst->apsPredSrc,
									   psInst->uPredCount,
									   GetPredicateNegate(psState, psInst),
									   GetPredicatePerChan(psState, psInst),
									   EliminateAlphaSwizzleReplace,
									   &sRepArg /* pvCtx */,
									   IMG_FALSE /* bCheckOnly */);

				/*
					Change the pack instruction to write to alpha only.
				*/
				GenerateMoveForMaskedDest(psState, psInst);
				SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sRepArg);
				psInst->auDestMask[0] = USC_ALPHA_CHAN_MASK;
				psInst->auLiveChansInDest[0] = USC_ALPHA_CHAN_MASK;
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_C10);

				bChanges = IMG_TRUE;

				continue;
			}
		}
	}
	return bChanges;
}

static IMG_BOOL CombineMulMulToSOP3(PINTERMEDIATE_STATE		psState,
									PCODEBLOCK				psBlock,
									PINST					psInst,
									PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineMulMulToSOP3
    
 PURPOSE	: Try to combine two integer multiply which share a source.

 PARAMETERS	: psBlock					- Flow control block containing the
										two instructions.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uInstSharedArg, uNextInstSharedArg;
	IMG_UINT32	uArg;
	IMG_BOOL	bAlphaReplicate;
	IMG_BOOL	bNextInstAlphaReplace;
	IMG_BOOL	bSrc0FromInst;

	/*
		Check for a multiply into the RGB channels.
	*/
	if (
			!(
				IsSOPWMMultiply(psInst, &bAlphaReplicate) &&
				psInst->auDestMask[0] == 7
			 )
	  )
	{
		return IMG_FALSE;
	}
	/*
		Check for a multiply into the ALPHA channels.
	*/
	if (
			!(
				IsSOPWMMultiply(psNextInst, &bNextInstAlphaReplace) &&
				psNextInst->auDestMask[0] == 8
			 )
	  )
	{
		return IMG_FALSE;
	}
	/*
		Check for a shared source.
	*/
	uInstSharedArg = uNextInstSharedArg = UINT_MAX;
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (EqualArgs(&psInst->asArg[uArg], &psNextInst->asArg[0]))
		{
			uInstSharedArg = uArg;
			uNextInstSharedArg = 0;
			break;
		}
		else if (EqualArgs(&psInst->asArg[uArg], &psNextInst->asArg[1]))
		{
			uInstSharedArg = uArg;
			uNextInstSharedArg = 1;
			break;
		}
	}
	if (uInstSharedArg == UINT_MAX)
	{
		return IMG_FALSE;
	}

	/*
		The shared source can't have an alpha replicate in the SOP3.
	*/
	if (bAlphaReplicate && uInstSharedArg == 1)
	{
		return IMG_FALSE;
	}

	/*
		Check one of the non-shared sources can use source 0.
	*/
	if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[1 - uInstSharedArg]))
	{
		bSrc0FromInst = IMG_TRUE;
	}
	else if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[1 - uNextInstSharedArg]))
	{
		bSrc0FromInst = IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}

	SetOpcode(psState, psInst, ISOP3);
	if (bSrc0FromInst)
	{
		/*
			Swap the shared source into SRC1 if required.
		*/
		if (uInstSharedArg == 0)
		{	
			SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
		}
		MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psNextInst, 1 - uNextInstSharedArg /* uCopyFromSrcIdx */);

		psInst->u.psSop3->uCSel1 = bAlphaReplicate ? USEASM_INTSRCSEL_SRC0ALPHA : USEASM_INTSRCSEL_SRC0;
		psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_SRC2ALPHA;
	}
	else
	{
		/*
			Swap the shared source into SRC1 and the
			other source into SRC2.
		*/
		if (uInstSharedArg == 0)
		{
			MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
			MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		}
		else
		{
			MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		}
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 1 - uNextInstSharedArg /* uCopyFromSrcIdx */);

		psInst->u.psSop3->uCSel1 = bAlphaReplicate ? USEASM_INTSRCSEL_SRC2ALPHA : USEASM_INTSRCSEL_SRC2;
		psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_SRC0ALPHA;
	}

	psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
	psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementCSel1 = IMG_FALSE;

	psInst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop3->bNegateCResult = IMG_FALSE;

	psInst->u.psSop3->uCoissueOp = USEASM_OP_ASOP;
	psInst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop3->bNegateAResult = IMG_FALSE;

	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	
	return IMG_TRUE;
}

static IMG_BOOL CombineAddMovToSOP3(PINTERMEDIATE_STATE		psState,
									PCODEBLOCK				psBlock,
									PINST					psAddInst,
									PINST					psMoveInst,
									IMG_BOOL				bAddIsFirst)
/*****************************************************************************
 FUNCTION	: CombineAddMovToSOP3
    
 PURPOSE	: Try to combine an integer addd and a move into a SOP3.

 PARAMETERS	: psState					- Compiler state.
			  psBlock					- Flow control block containing the
										two instructions.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	PINST		psDestInst;
	IMG_BOOL	bMoveInst;
	IMG_UINT32	uNewLiveChansInDest;
	SOP3_PARAMS	sSop3;
	PINST		psNewDestCopyFromInst;

	if (
			!(
				IsSOPWMAdd(psAddInst) &&
				psAddInst->auDestMask[0] == 7
			 )
		)
	{
		return IMG_FALSE;
	}

	if (
			(
				bAddIsFirst && 
				psMoveInst->auDestMask[0] != 8
			) || 
			!CanUseSource0(psState, psBlock->psOwner->psFunc, &psMoveInst->asArg[0])
	   )
	{
		return IMG_FALSE;
	}

	bMoveInst = IMG_FALSE;
	if ((psMoveInst->eOpcode == IPCKU8U8 && psAddInst->asDest[0].eFmt == UF_REGFORMAT_U8) ||
		(psMoveInst->eOpcode == IPCKC10C10 && psAddInst->asDest[0].eFmt == UF_REGFORMAT_C10))
	{
		if (GetPCKComponent(psState, psMoveInst, 0 /* uArgIdx */) == 3 || AllChannelsTheSameInSource(&psMoveInst->asArg[0]))
		{
			bMoveInst = IMG_TRUE;
		}
	}
	else if (IsSOPWMMove(psMoveInst))
	{
		bMoveInst = IMG_TRUE;
	}
	else if (psMoveInst->eOpcode == IMOV)
	{
		bMoveInst = IMG_TRUE;
	}
	if (!bMoveInst)
	{
		return IMG_FALSE;
	}

	/* RGB = 1 * SRC1 + 1 * SRC2 */
	sSop3.uCSel1 = USEASM_INTSRCSEL_ZERO;
	sSop3.bComplementCSel1 = IMG_TRUE;
	sSop3.uCSel2 = USEASM_INTSRCSEL_ZERO;
	sSop3.bComplementCSel2 = IMG_TRUE;
	sSop3.uCOp = psAddInst->u.psSopWm->uCop;
	sSop3.bNegateCResult = IMG_FALSE;
	
	/* ALPHA = (1 - SRC0A) * ZERO + SRC0A * ONE */
	sSop3.uCoissueOp = USEASM_OP_ALRP;
	sSop3.uAOp = USEASM_INTSRCSEL_ADD;
	sSop3.bNegateAResult = IMG_FALSE;
	sSop3.uASel1 = USEASM_INTSRCSEL_ZERO;
	sSop3.bComplementASel1 = IMG_FALSE;
	sSop3.uASel2 = USEASM_INTSRCSEL_ZERO;
	sSop3.bComplementASel2 = IMG_TRUE;

	if (bAddIsFirst)
	{
		psDestInst = psAddInst;
		psNewDestCopyFromInst = psMoveInst;
		uNewLiveChansInDest = psMoveInst->auLiveChansInDest[0];
	}
	else
	{
		psDestInst = psMoveInst;
		psNewDestCopyFromInst = psAddInst;
		uNewLiveChansInDest = psAddInst->auLiveChansInDest[0];
	}

	SetOpcode(psState, psDestInst, ISOP3);
	MoveDest(psState, psDestInst, 0 /* uCopyToDestIdx */, psNewDestCopyFromInst, 0 /* uCopyFromDestIdx */);
	psDestInst->auDestMask[0] = USC_DESTMASK_FULL;
	psDestInst->auLiveChansInDest[0] = uNewLiveChansInDest;

	MoveSrc(psState, psDestInst, 2 /* uCopyToSrcIdx */, psAddInst, 1 /* uCopyFromSrcIdx */);
	MoveSrc(psState, psDestInst, 1 /* uCopyToSrcIdx */, psAddInst, 0 /* uCopyFromSrcIdx */);

	if (psDestInst != psMoveInst)
	{
		MoveSrc(psState, psDestInst, 0 /* uCopyToSrcIdx */, psMoveInst, 0 /* uCopyFromSrcIdx */);
	}

	*psDestInst->u.psSop3 = sSop3;

	return IMG_TRUE;
}

static IMG_BOOL IsLRP1Mul(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINST psMulInst, IMG_PBOOL pbMulInstAlphaReplicate)
/*****************************************************************************
 FUNCTION	: IsLRP1Mul
    
 PURPOSE	: Checks if an instruction is a fixed point multiply suitable to be
			  part of a LRP1 instruction.

 PARAMETERS	: psState					- Compiler state.
			  psMulInst					- Instruction to check.
			  pbMulInstAlphaReplicate	- TRUE if the instruction uses
										alpha replicate.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check for a multiply into the RGB channels.
	*/
	if (
			!(
				IsSOPWMMultiply(psMulInst, pbMulInstAlphaReplicate) &&
				psMulInst->auDestMask[0] == USC_RGB_CHAN_MASK
			 )
	  )
	{
		return IMG_FALSE;
	}
	/*
		Check that at least one of the SOPWM sources can go in source 0 of the SOP3. Only source 0
		can use alpha replicate so if that option is used then the alpha replicate source to SOPWM
		must be able to use source 0.
	*/
	if	(
			!(
				(
					!(*pbMulInstAlphaReplicate) && 
					CanUseSource0(psState, psBlock->psOwner->psFunc, &psMulInst->asArg[0])
				) || 
				CanUseSource0(psState, psBlock->psOwner->psFunc, &psMulInst->asArg[1])
			 )
		)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static IMG_VOID ConvertMulMovToLRP1(PINTERMEDIATE_STATE	psState,
									PCODEBLOCK			psBlock,
									PINST				psMulInst,
									PINST				psMoveInst,
									IMG_BOOL			bMulIsFirst,
									IMG_BOOL			bMulInstAlphaReplicate,
									PARG				psMoveArg)
/*****************************************************************************
 FUNCTION	: ConvertMulMovToLRP1
    
 PURPOSE	: Combine an integer multiply and a move into a LRP1

 PARAMETERS	: psState					- Compiler state.
			  psBlock					- Flow control block containing the
										two instructions.
			  psMulInst, psMoveInst		- The two instructions to combine.
			  bMulIsFirst				- If TRUE then the multiply occurs
										first in the block.
			  psMoveArg					- Source for the moved data.

 RETURNS	: Nothing.
*****************************************************************************/
{
	LRP1_PARAMS	sLrp1;
	IMG_BOOL	bSwapMulArgs;
	PINST		psDestInst;
	PINST		psNewDestSrcInst;
	IMG_UINT32	uNewLiveChansInDest;

	/*
		Get the instruction modes for the new instruction.
	*/
	/* RGB = (1 - SRC0) * ZERO + SRC0 * SRC2 */
	if (bMulInstAlphaReplicate)
	{
		sLrp1.uCS = USEASM_INTSRCSEL_SRC0ALPHA;
	}
	else
	{
		sLrp1.uCS = USEASM_INTSRCSEL_SRC0;
	}
	sLrp1.uCSel10 = USEASM_INTSRCSEL_ZERO;
	sLrp1.bComplementCSel10 = IMG_FALSE;
	sLrp1.uCSel11 = USEASM_INTSRCSEL_SRC2;
	sLrp1.bComplementCSel11 = IMG_FALSE;

	/* ALPHA = ONE * SRC1 + ZERO * SRC2 */
	sLrp1.uASel1 = USEASM_INTSRCSEL_ZERO;
	sLrp1.bComplementASel1 = IMG_TRUE;
	sLrp1.uASel2 = USEASM_INTSRCSEL_ZERO;
	sLrp1.bComplementASel2 = IMG_FALSE;
	sLrp1.uAOp = USEASM_INTSRCSEL_ADD;

	bSwapMulArgs = IMG_FALSE;
	if (bMulInstAlphaReplicate || !CanUseSource0(psState, psBlock->psOwner->psFunc, &psMulInst->asArg[0]))
	{
		bSwapMulArgs = IMG_TRUE;
	}

	if (bMulIsFirst)
	{
		psDestInst = psMulInst;
		psNewDestSrcInst = psMoveInst;
		uNewLiveChansInDest = psMoveInst->auLiveChansInDest[0];
	}
	else
	{
		psDestInst = psMoveInst;
		psNewDestSrcInst = psMulInst;
		uNewLiveChansInDest = psMulInst->auLiveChansInDest[0];
	}

	/*
		Replace the first instruction by a LRP1.
	*/
	SetOpcode(psState, psDestInst, ILRP1);
	MoveDest(psState, psDestInst, 0 /* uCopyToDestIdx */, psNewDestSrcInst, 0 /* uCopyFromDestIdx */);
	psDestInst->auDestMask[0] = USC_DESTMASK_FULL;
	psDestInst->auLiveChansInDest[0] = uNewLiveChansInDest;
	*psDestInst->u.psLrp1 = sLrp1;

	if (bSwapMulArgs)
	{
		MoveSrc(psState, psDestInst, 2 /* uCopyToSrcIdx */, psMulInst, 0 /* uCopyFromSrcIdx */);
		MoveSrc(psState, psDestInst, 0 /* uCopyToSrcIdx */, psMulInst, 1 /* uCopyFromSrcIdx */);
	}
	else
	{	
		MoveSrc(psState, psDestInst, 2 /* uCopyToSrcIdx */, psMulInst, 1 /* uCopyFromSrcIdx */);
		if (psDestInst != psMulInst)
		{
			MoveSrc(psState, psDestInst, 0 /* uCopyToSrcIdx */, psMulInst, 0 /* uCopyFromSrcIdx */);
		}
	}
	SetSrcFromArg(psState, psDestInst, 1 /* uSrcIdx */, psMoveArg);
}

static IMG_BOOL CombineMulMovSrcToLRP1(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psBlock,
									   PINST				psMulInst,
									   PINST				psMoveInst)
/*****************************************************************************
 FUNCTION	: CombineMulMovToLRP1
    
 PURPOSE	: Try to combine an integer multiply and a move into a SOP3.

 PARAMETERS	: psBlock					- Flow control block containing the
										two instructions.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_BOOL	bMulInstAlphaReplicate;

	/*
		Check the multiply instruction is suitable for converting to (part of) a LRP1
		instruction.
	*/
	if (!IsLRP1Mul(psState, psBlock, psMulInst, &bMulInstAlphaReplicate))
	{
		return IMG_FALSE;
	}

	/*
		Check the second instruction is copying the RGB channels of the result of the multiply.
	*/
	if (!IsSOPWMMove(psMoveInst))
	{
		return IMG_FALSE;
	}
	if (psMoveInst->auDestMask[0] != USC_RGB_CHAN_MASK)
	{
		return IMG_FALSE;
	}
	if (psMoveInst->apsOldDest[0] == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Replace the first instruction by a LRP1.
	*/
	ConvertMulMovToLRP1(psState, 
					    psBlock, 
						psMulInst, 
						psMoveInst, 
						IMG_TRUE /* bMulIsFirst */, 
						bMulInstAlphaReplicate,
						psMoveInst->apsOldDest[0]); 
	return IMG_TRUE;
}

static IMG_BOOL CombineMulMovToLRP1(PINTERMEDIATE_STATE		psState,
									PCODEBLOCK				psBlock,
									PINST					psMulInst,
									PINST					psMoveInst,
									IMG_BOOL				bMulIsFirst)
/*****************************************************************************
 FUNCTION	: CombineMulMovToLRP1
    
 PURPOSE	: Try to combine an integer multiply and a move into a SOP3.

 PARAMETERS	: psBlock					- Flow control block containing the
										two instructions.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_BOOL	bPack;
	ARG			sMoveArg;
	IMG_BOOL	bMulInstAlphaReplicate;

	/*
		Check the multiply instruction is suitable for converting to (part of) a LRP1
		instruction.
	*/
	if (!IsLRP1Mul(psState, psBlock, psMulInst, &bMulInstAlphaReplicate))
	{
		return IMG_FALSE;
	}

	if (bMulIsFirst && psMoveInst->auDestMask[0] != USC_W_CHAN_MASK)
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is a move of some sort.
	*/
	if (
			(
				psMoveInst->eOpcode == IPCKU8U8 && 
				psMulInst->asDest[0].eFmt == UF_REGFORMAT_U8
			) ||
			(
				psMoveInst->eOpcode == IPCKC10C10 && 
				psMulInst->asDest[0].eFmt == UF_REGFORMAT_C10
			)
		)
	{
		bPack = IMG_TRUE;
	}
	else if (IsSOPWMMove(psMoveInst) || psMoveInst->eOpcode == IMOV)
	{
		bPack = IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}
	/*
		The SOP3 can only select the alpha channel so if the second instruction is a pack
		check we aren't trying to do a swizzle from another channel.
	*/
	if (bPack)
	{
		if (
				!(
					GetPCKComponent(psState, psMoveInst, 0 /* uArgIdx */) == 3 ||
					(
						psMoveInst->eOpcode == IPCKC10C10 &&
						psMoveInst->asArg[0].uType != USEASM_REGTYPE_TEMP &&
						GetPCKComponent(psState, psMoveInst, 0 /* uArgIdx */) == 0
					) ||
					AllChannelsTheSameInSource(&psMoveInst->asArg[0])
				 )
		   )
		{
			return IMG_FALSE;
		}
	}

	/*
		Replace the first instruction by a LRP1.
	*/
	sMoveArg = psMoveInst->asArg[0];
	ConvertMulMovToLRP1(psState, psBlock, psMulInst, psMoveInst, bMulIsFirst, bMulInstAlphaReplicate, &sMoveArg);

	return IMG_TRUE;
}

static IMG_VOID CombineMOVSOPWM(PINTERMEDIATE_STATE		psState,
								PINST					psInst,
								PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineMOVSOPWM
    
 PURPOSE	: Combine a MOV together with a SOPWM with an unused source.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Convert MOV to SOP2.
	*/
	SetOpcode(psState, psInst, ISOP2);
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	if (psNextInst->auDestMask[0] == 0x8)
	{
		/*
			SRC1 = RGB data
			SRC2 = ALPHA data
		*/
		MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);

		/*
			Copy the data from the SOPWM swapping references from SRC1 to SRC2.
		*/
		psInst->u.psSop2->uAOp = psNextInst->u.psSopWm->uAop;
		psInst->u.psSop2->uASel1 = SwapArgsInSrcSel(psState, psNextInst->u.psSopWm->uSel1);
		psInst->u.psSop2->uASel2 = SwapArgsInSrcSel(psState, psNextInst->u.psSopWm->uSel2);
		psInst->u.psSop2->bComplementASel1 = psNextInst->u.psSopWm->bComplementSel2;
		psInst->u.psSop2->bComplementASel2 = psNextInst->u.psSopWm->bComplementSel1;
		psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;

		/*
			DEST.RGB = SRC1 * ONE + SRC2 * ZERO
		*/
		psInst->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
		psInst->u.psSop2->uCSel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementCSel1 = IMG_TRUE;
		psInst->u.psSop2->bComplementCSel2 = IMG_FALSE;
		psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;
	}
	else
	{
		/*
			SRC1 = RGB data
			SRC2 = ALPHA data
		*/
		MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);

		/*
			Copy the data from the SOPWM.
		*/
		psInst->u.psSop2->uCOp = psNextInst->u.psSopWm->uCop;
		psInst->u.psSop2->uCSel1 = psNextInst->u.psSopWm->uSel1;
		psInst->u.psSop2->uCSel2 = psNextInst->u.psSopWm->uSel2;
		psInst->u.psSop2->bComplementCSel1 = psNextInst->u.psSopWm->bComplementSel1;
		psInst->u.psSop2->bComplementCSel2 = psNextInst->u.psSopWm->bComplementSel2;
		psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;

		/*
			DEST.ALPHA = SRC1 * ZERO + SRC2 * ONE
		*/
		psInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
		psInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementASel1 = IMG_FALSE;
		psInst->u.psSop2->bComplementASel2 = IMG_TRUE;
		psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;
	}
}

static IMG_BOOL ConvertPackToMove(PINTERMEDIATE_STATE psState,
								  PINST psInst,
								  PINST psNextInst)
/*****************************************************************************
 FUNCTION	: ConvertPackToMove
    
 PURPOSE	: Check if a pair of PCKC10C10 or PCKU8U8 instructions can be
			  simplified to a single MOV instruction.

 PARAMETERS	: psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were simplified.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	PARG		psSrcArg = NULL;

	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		PINST		psWriterInst;
		PARG		psArg;
		IMG_UINT32	uPrevDestMask;
		IMG_UINT32	uArgComponent;

		/*
			Find which of the pair of instructions writes this channel
			of the destination.
		*/
		if (psNextInst->auDestMask[0] & (1U << uChan))
		{
			psWriterInst = psNextInst;
		}
		else if (psInst->auDestMask[0] & (1U << uChan))
		{
			psWriterInst = psInst;
		} 
		else
		{
			continue;
		}

		/*
			Which source is this channel of the PCK result coming from.
		*/
		uPrevDestMask = psWriterInst->auDestMask[0] & ((1U << uChan) - 1);
		if ((g_auSetBitCount[uPrevDestMask] % 2) == 1)
		{
			psArg = &psWriterInst->asArg[1];
			uArgComponent = GetPCKComponent(psState, psWriterInst, 1 /* uArgIdx */);
		}
		else
		{
			psArg = &psWriterInst->asArg[0];
			uArgComponent = GetPCKComponent(psState, psWriterInst, 0 /* uArgIdx */);
		}
		/*
			Check we aren't swizzling this channel.
		*/
		if (uArgComponent != uChan)
		{
			return IMG_FALSE;
		}
		/*
			Check that all of the pack results are from the same register.
		*/
		if (psSrcArg != NULL && !SameRegister(psArg, psSrcArg))
		{
			return IMG_FALSE;
		}
		psSrcArg = psArg;
	}

	/*
		Convert the first PCK instruction to a MOV.
	*/
	if (psSrcArg != &psInst->asArg[0])
	{
		SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, psSrcArg);
	}
	SetOpcode(psState, psInst, IMOV);
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	return IMG_TRUE;
}

static IMG_BOOL CombineSOPWMMOVToSOP2(PINTERMEDIATE_STATE	psState,
									  PINST					psInst,
									  PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineSOPWMMOVToSOP
    
 PURPOSE	: Try to combine a SOPWM and a move into a SOP2.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_BOOL	bNextInstIsMove;
	IMG_UINT32	uMatchArg;
	SOP2_PARAMS	sSop2;

	/*
		Check the first instruction is a SOPWM.
	*/
	if (psInst->eOpcode != ISOPWM)
	{
		return IMG_FALSE;
	}
	if (psInst->auDestMask[0] != USC_RGB_CHAN_MASK)
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is a move of a single channel
		into the alpha channel of the result without a swizzle.
	*/
	bNextInstIsMove = IMG_FALSE;
	if (
			psNextInst->eOpcode == IPCKU8U8 &&
			psNextInst->auDestMask[0] == USC_ALPHA_CHAN_MASK &&
			(
				GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */) == 3 &&
				AllChannelsTheSameInSource(&psNextInst->asArg[0])
			)
	   )
	{
		bNextInstIsMove = IMG_TRUE;
	}
	if (
			psNextInst->eOpcode == IPCKC10C10 &&
			psNextInst->auDestMask[0] == 0x8 &&
			(
				(psNextInst->asArg[0].uType == USEASM_REGTYPE_TEMP && GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */) == 3) ||
				(psNextInst->asArg[0].uType != USEASM_REGTYPE_TEMP && GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */) == 0) ||
				AllChannelsTheSameInSource(&psNextInst->asArg[0])
		    )
	   )
	{
		bNextInstIsMove = IMG_TRUE;
	}
	if (!bNextInstIsMove)
	{
		return IMG_FALSE;
	}

	if (SameRegister(&psNextInst->asArg[0], &psInst->asArg[0]))
	{
		uMatchArg = 0;
	}
	else if (SameRegister(&psNextInst->asArg[0], &psInst->asArg[1]))
	{
		uMatchArg = 1;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Copy the SOPWM parameters to the SOP2's RGB calculation.
	*/
	sSop2.uCOp = psInst->u.psSopWm->uCop;
	sSop2.uCSel1 = psInst->u.psSopWm->uSel1;
	sSop2.uCSel2 = psInst->u.psSopWm->uSel2;
	sSop2.bComplementCSel1 = psInst->u.psSopWm->bComplementSel1;
	sSop2.bComplementCSel2 = psInst->u.psSopWm->bComplementSel2;
	sSop2.bComplementCSrc1 = IMG_FALSE;

	/*
		RESULT.A = SRC1 * ONE + SRC2 * ZERO /
				 = SRC1 * ZERO + SRC2 * ONE
	*/
	sSop2.uAOp = USEASM_INTSRCSEL_ADD;
	sSop2.uASel1 = USEASM_INTSRCSEL_ZERO;
	sSop2.uASel2 = USEASM_INTSRCSEL_ZERO;
	if (uMatchArg == 0)
	{
		sSop2.bComplementASel1 = IMG_TRUE;
		sSop2.bComplementASel2 = IMG_FALSE;
	}
	else
	{
		sSop2.bComplementASel1 = IMG_FALSE;
		sSop2.bComplementASel2 = IMG_TRUE;
	}
	sSop2.bComplementASrc1 = IMG_FALSE;

	/*
		Convert the first instruction to a SOP2
	*/
	SetOpcode(psState, psInst, ISOP2);
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	*psInst->u.psSop2 = sSop2;

	return IMG_TRUE;
}

static IMG_BOOL IsFPMove(PINTERMEDIATE_STATE psState, PINST psInst)
{
	if (psInst->eOpcode == IMOV)
	{
		return IMG_TRUE;
	}
	if (IsSOPWMMove(psInst))
	{
		return IMG_TRUE;
	}
	if (psInst->eOpcode == IPCKU8U8 || psInst->eOpcode == IPCKC10C10)
	{
		if (g_abSingleBitSet[psInst->auDestMask[0]])
		{
			IMG_UINT32	uWrittenChan = g_aiSingleComponent[psInst->auDestMask[0]];

			if (
					AllChannelsTheSameInSource(&psInst->asArg[0]) ||
					GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == uWrittenChan
				)
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL CombineMOVMOVToSOP2(PINTERMEDIATE_STATE		psState,
									PINST					psInst,
									PINST					psNextInst)
/*****************************************************************************
 FUNCTION	: CombineMOVMOVToSOP
    
 PURPOSE	: Try to combine a pair of moves into a SOP2.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uInstEffectiveMask;
	IMG_BOOL	bInstWritesAlpha;
	IMG_UINT32	uCombinedMask;

	/*
		Check both instructions are MOVs.
	*/
	if (!IsFPMove(psState, psInst))
	{
		return IMG_FALSE;
	}
	if (!IsFPMove(psState, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Check one instruction is writing the ALPHA channel and one instruction is writing the RGB channels.
	*/
	uInstEffectiveMask = psInst->auDestMask[0] & ~psNextInst->auDestMask[0];
	if ((uInstEffectiveMask & USC_ALPHA_CHAN_MASK) == 0 && (psNextInst->auDestMask[0] & USC_RGB_CHAN_MASK) == 0)
	{
		bInstWritesAlpha = IMG_FALSE;
	}
	else if ((uInstEffectiveMask & USC_RGB_CHAN_MASK) == 0 && (psNextInst->auDestMask[0] & USC_ALPHA_CHAN_MASK) == 0)
	{
		bInstWritesAlpha = IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check we don't need to preserve any channels in the destination.
	*/
	uCombinedMask = psInst->auDestMask[0] | psNextInst->auDestMask[0];
	if ((psNextInst->auLiveChansInDest[0] & ~uCombinedMask) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Convert the first instruction to a SOP2
	*/
	SetOpcode(psState, psInst, ISOP2);
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	/*
		Copy the second source from the second instruction.
	*/
	if (bInstWritesAlpha)
	{
		MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
		MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psNextInst, 0 /* uMoveFromSrcIdx */);
	}
	else
	{
		MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psNextInst, 0 /* uMoveFromSrcIdx */);
	}

	/*
		RESULT.RGB = SRC1 * ONE + SRC2 * ZERO
	*/
	psInst->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop2->uCSel1 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop2->bComplementCSel1 = IMG_TRUE;
	psInst->u.psSop2->bComplementCSel2 = IMG_FALSE;
	psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;

	/*
		RESULT.A = SRC1 * ZERO + SRC2 * ZERO
	*/
	psInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop2->bComplementASel1 = IMG_FALSE;
	psInst->u.psSop2->bComplementASel2 = IMG_TRUE;
	psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;
		
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_BOOL CombineVectorPacksToU8C10(PINTERMEDIATE_STATE	psState,
										  PINST					psFirstInst,
										  PINST					psSecondInst)
/*****************************************************************************
 FUNCTION	: CombineVectorPacksToU8C10
    
 PURPOSE	: Try to combine two PCK instructions with U8 or C10 destination
			  format and sharing the same destination register.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uSwizzle;

	/*
		Check for a PCK to U8 or C10 format.
	*/
	if (!(psFirstInst->eOpcode == IVPCKU8FLT || psFirstInst->eOpcode == IVPCKC10FLT))
	{
		return IMG_FALSE;
	}
	/*
		Check that both PCK instructions have the same source and destination format.
	*/
	if (psSecondInst->eOpcode != psFirstInst->eOpcode)
	{
		return IMG_FALSE;
	}
	/*
		Check both instructions have the same source argument.
	*/
	if (
			!(
				psFirstInst->asArg[0].uType != USC_REGTYPE_UNUSEDSOURCE && 
				EqualArgs(&psFirstInst->asArg[0], &psSecondInst->asArg[0])
			 )
		)
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions have the same source modifier.
	*/
	if (psFirstInst->u.psVec->asSrcMod[0].bNegate != psSecondInst->u.psVec->asSrcMod[0].bNegate ||
		psFirstInst->u.psVec->asSrcMod[0].bAbs != psSecondInst->u.psVec->asSrcMod[0].bAbs)
	{
		return IMG_FALSE;
	}

	/*
		Combine the swizzles for both instructions.
	*/
	uSwizzle = 0;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		USEASM_INTSRCSEL	eChanSel;
		if ((psSecondInst->auDestMask[0] & (1U << uChan)) != 0)
		{
			eChanSel = GetChan(psSecondInst->u.psVec->auSwizzle[0], uChan);
		}
		else
		{		
			eChanSel = GetChan(psFirstInst->u.psVec->auSwizzle[0], uChan);
		}
		uSwizzle = SetChan(uSwizzle, uChan, eChanSel);
	}

	/*
		Set up the new PCK instruction.
	*/
	MoveDest(psState, psFirstInst, 0 /* uCopyToDestIdx */, psSecondInst, 0 /* uCopyFromDestIdx */);
	psFirstInst->auDestMask[0] |= psSecondInst->auDestMask[0];
	psFirstInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];
	psFirstInst->u.psVec->auSwizzle[0] = uSwizzle;

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_BOOL CombinePacksToU8C10(PINTERMEDIATE_STATE	psState,
									PINST				psFirstInst,
									PINST				psSecondInst)
/*****************************************************************************
 FUNCTION	: CombinePacksToU8C10
    
 PURPOSE	: Try to combine two PCK instructions with U8 or C10 destination
			  format and sharing the same destination register.

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to combine.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	PARG		apsSwiz[CHANS_PER_REGISTER] = {NULL, NULL, NULL, NULL};
	IMG_UINT32	auSwizComponent[CHANS_PER_REGISTER];
	ARG			asNewSrc[2];
	IMG_UINT32	auNewComponent[2];
	IMG_UINT32	uArg;
	IMG_UINT32	uChan;
	IMG_UINT32	uCombinedDestMask;
	IMG_UINT32	uFirstInstDestMask;
	IMG_UINT32	uPreservedInFirstInstDestMask;
											   

	/*
		Check for a PCK to U8 or C10 format.
	*/
	if (
			!(
				psFirstInst->eOpcode == IPCKU8F32 ||
				psFirstInst->eOpcode == IPCKC10F32 ||
				psFirstInst->eOpcode == IPCKU8U8 ||
				psFirstInst->eOpcode == IPCKC10C10 ||
				psFirstInst->eOpcode == IPCKU8F16 ||
				psFirstInst->eOpcode == IPCKC10F16
			 )
		)
	{
		return IMG_FALSE;
	}
	/*
		Check that both PCK instructions have the same source and destination format.
	*/
	if (psSecondInst->eOpcode != psFirstInst->eOpcode)
	{
		return IMG_FALSE;
	}

	/*
		Get the effective destination mask for the first instruction (excluding channels
		overwritten by the second instruction).
	*/
	uFirstInstDestMask = psFirstInst->auDestMask[0] & ~psSecondInst->auDestMask[0];
	ASSERT(uFirstInstDestMask != 0);

	/*
		Get the destination mask for the combined instruction.
	*/
	uCombinedDestMask = uFirstInstDestMask | psSecondInst->auDestMask[0];

	/*
		Simple case for only two bits set.
	*/
	if (g_auSetBitCount[uCombinedDestMask] == 2)
	{
		IMG_UINT32	uOrigFirstInstDestMask;

		/*
			The data for the channel corresponding to the first bit
			set in the destination mask comes from SRC1; the data for
			the other channel from SRC2.
		*/
		uOrigFirstInstDestMask = uFirstInstDestMask;
		psFirstInst->auDestMask[0] = uCombinedDestMask;
		psFirstInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];
		if (uOrigFirstInstDestMask < psSecondInst->auDestMask[0])
		{
			MovePackSource(psState, psFirstInst, 1 /* uDestArgIdx */, psSecondInst, 0 /* uSrcArgIdx */);
		}
		else
		{
			MovePackSource(psState, psFirstInst, 1 /* uDestArgIdx */, psFirstInst, 0 /* uSrcArgIdx */);
			MovePackSource(psState, psFirstInst, 0 /* uDestArgIdx */, psSecondInst, 0 /* uSrcArgIdx */);
		}
		MoveDest(psState, psFirstInst, 0 /* uCopyToDestIdx */, psSecondInst, 0 /* uCopyFromDestIdx */);
		return IMG_TRUE;
	}
	
	ASSERT(g_auSetBitCount[uCombinedDestMask] >= 3);

	/*
		Check if we can convert one of the instruction updating an immediate partial destination by
		a conversion of an appropriate constant.
	*/
	if (psFirstInst->eOpcode == IPCKU8F32 &&
		g_auSetBitCount[uCombinedDestMask] == 3 && 
		psSecondInst->auLiveChansInDest[0] == USC_ALL_CHAN_MASK)
	{
		if (psFirstInst->apsOldDest[0] != NULL &&
			psFirstInst->apsOldDest[0]->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			IMG_UINT32	uCopiedFromPartialDestMask;
			IMG_UINT32	uCopiedFromPartialDestChan;
			IMG_UINT32	uCopiedValue;

			uCopiedFromPartialDestMask = (~uCombinedDestMask) & USC_ALL_CHAN_MASK;
			uCopiedFromPartialDestChan = g_aiSingleComponent[uCopiedFromPartialDestMask];

			uCopiedValue = (psFirstInst->apsOldDest[0]->uNumber >> (uCopiedFromPartialDestChan * BITS_PER_BYTE)) & 0xFF;

			if (uCopiedValue == 0 || uCopiedValue == 0xFF)
			{
				PINST	psAddConstInst;
 
				/*
					Check one of the instruction has space to insert an extra conversion of a constant.
				*/
				psAddConstInst = NULL;
				if (g_abSingleBitSet[psFirstInst->auDestMask[0]])
				{
					psAddConstInst = psFirstInst;
				}
				else if (g_abSingleBitSet[psSecondInst->auDestMask[0]])
				{
					psAddConstInst = psSecondInst;
				}

				if (psAddConstInst != NULL)
				{
					IMG_UINT32	uConstSrc;

					if (psAddConstInst->auDestMask[0] < uCopiedFromPartialDestMask)
					{
						uConstSrc = 1;
					}
					else
					{
						MoveSrc(psState, psAddConstInst, 1 /* uMoveToSrcIdx */, psAddConstInst, 0 /* uMoveFromSrcIdx */);
						uConstSrc = 0;
					}
					SetSrc(psState,
						   psAddConstInst,
						   uConstSrc,
						   USEASM_REGTYPE_FPCONSTANT,
						   (uCopiedValue == 0) ? EURASIA_USE_SPECIAL_CONSTANT_ZERO : EURASIA_USE_SPECIAL_CONSTANT_FLOAT1,
						   UF_REGFORMAT_F32);

					psAddConstInst->auDestMask[0] |= uCopiedFromPartialDestMask;

					uCombinedDestMask |= uCopiedFromPartialDestMask;
					if (psAddConstInst == psFirstInst)
					{
						uFirstInstDestMask |= uCopiedFromPartialDestMask;
					}
				}
			}
		}
	}

	/*
		Don't generate an instruction with 3 bits set in its destination mask.
	*/
	uPreservedInFirstInstDestMask = 0;
	if (g_auSetBitCount[uCombinedDestMask] == 3)
	{
		uPreservedInFirstInstDestMask = psSecondInst->auLiveChansInDest[0] & ~uCombinedDestMask;
		uCombinedDestMask = USC_DESTMASK_FULL;
	}

	/*
		Get the source data for each channel written by the second instruction.
	*/
	uArg = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if (psSecondInst->auDestMask[0] & (1U << uChan))
		{
			apsSwiz[uChan] = &psSecondInst->asArg[uArg];
			auSwizComponent[uChan] = GetPCKComponent(psState, psSecondInst, uArg);

			uArg = (uArg + 1) % 2;
		}
	}

	/*
		Get the source data for each channel written by the first instruction.
	*/
	uArg = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if (uFirstInstDestMask & (1U << uChan))
		{
			apsSwiz[uChan] = &psFirstInst->asArg[uArg];
			auSwizComponent[uChan] = GetPCKComponent(psState, psFirstInst, uArg);

			uArg = (uArg + 1) % 2;
		}
	}

	/*
		Get the source data combined from the overwritten destination of the first instruction.
	*/
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if ((uPreservedInFirstInstDestMask & (1U << uChan)) != 0)
		{
			apsSwiz[uChan] = psFirstInst->apsOldDest[0];
			auSwizComponent[uChan] = uChan;
		}
	}

	/*
		Each of channels {0,2} and {1,3} gets data from the same
		source so check the sources are equal.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (apsSwiz[uArg] == NULL)
		{
			ASSERT(apsSwiz[uArg + 2] != NULL);
			apsSwiz[uArg] = apsSwiz[uArg + 2];
			auSwizComponent[uArg] = auSwizComponent[uArg + 2];
		}
		else if (apsSwiz[uArg + 2] == NULL)
		{
			ASSERT(apsSwiz[uArg] != NULL);
			apsSwiz[uArg + 2] = apsSwiz[uArg];
			auSwizComponent[uArg + 2] = auSwizComponent[uArg];
		}
		else if (!(EqualArgs(apsSwiz[uArg], apsSwiz[uArg + 2]) && auSwizComponent[uArg] == auSwizComponent[uArg + 2]))
		{
			return IMG_FALSE;
		}
	}

	/*
		Get the new sources for the combined pack instruction.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psSecondInst->auDestMask[0] & (1U << uArg))
		{
			asNewSrc[uArg] = *apsSwiz[uArg];
			auNewComponent[uArg] = auSwizComponent[uArg];
		}
		else if (psSecondInst->auDestMask[0] & (1U << (uArg + 2)))
		{
			asNewSrc[uArg] = *apsSwiz[uArg + 2];
			auNewComponent[uArg] = auSwizComponent[uArg + 2];
		}
		else
		{
			asNewSrc[uArg] = *apsSwiz[uArg];
			auNewComponent[uArg] = auSwizComponent[uArg];
		}
	}

	/*
		Set up the new PCK instruction.
	*/
	MoveDest(psState, psFirstInst, 0 /* uCopyToDestIdx */, psSecondInst, 0 /* uCopyFromDestIdx */);
	psFirstInst->auDestMask[0] = uCombinedDestMask;
	psFirstInst->auLiveChansInDest[0] = psSecondInst->auLiveChansInDest[0];
	if ((psFirstInst->auLiveChansInDest[0] & ~psFirstInst->auDestMask[0]) == 0)
	{
		SetPartiallyWrittenDest(psState, psFirstInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
	}
	SetSrcFromArg(psState, psFirstInst, 0 /* uSrcIdx */, &asNewSrc[0]);
	SetPCKComponent(psState, psFirstInst, 0 /* uArgIdx */, auNewComponent[0]);
	SetSrcFromArg(psState, psFirstInst, 1 /* uSrcIdx */, &asNewSrc[1]);
	SetPCKComponent(psState, psFirstInst, 1 /* uArgIdx */, auNewComponent[1]);

	return IMG_TRUE;
}

static IMG_BOOL CombineAlphaRGBMoveMultiply(PINTERMEDIATE_STATE		psState,
											PCODEBLOCK				psBlock,
											PINST					psInst,
											PINST					psNextInst,
											IMG_UINT32				uNextInstSrcFromDest)
/*****************************************************************************
 FUNCTION	: CombineAlphaRGBMoveMultiply
    
 PURPOSE	: Try to combine an integer instruction moving different data into the
			  RGB and ALPHA channels with a multiply on the result.

 PARAMETERS	: psBlock					- Flow control block containing the two
										instructions.
			  psInst, psNextInst		- The two instructions to combine.
			  pbDropNextInst			- Set to TRUE on return if psNextInst
										can be dropped.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uMatchArg;
	IMG_BOOL	bMulAlphaSwizzle;

	/*
		Check the first instruction is moving different data into the RGB and
		ALPHA channels of the destination.
	*/
	if (
			psInst->eOpcode == IPCKU8U8 && 
			psInst->auDestMask[0] == USC_W_CHAN_MASK &&
			(
				GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == USC_W_CHAN ||
				AllChannelsTheSameInSource(&psInst->asArg[0])
			) &&
			psInst->apsOldDest[0] != NULL
	   )
	{
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check the second instruction is a fixed point multiply.
	*/
	if (!IsSOPWMMultiply(psNextInst, &bMulAlphaSwizzle))
	{
		return IMG_FALSE;
	}

	/*	
		Check the second instruction doesn't need a destination write mask.
	*/
	if ((psNextInst->auLiveChansInDest[0] & ~psNextInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check only one argument to the second instruction is the result of the
		first instruction.
	*/
	if (!g_abSingleBitSet[uNextInstSrcFromDest])
	{
		return IMG_FALSE;
	}
	uMatchArg = g_aiSingleComponent[uNextInstSrcFromDest];

	/*
		Check the result of the first instruction doesn't have a alpha replicate
		modifier when used in the second instruction.
	*/
	if (uMatchArg == 1 && bMulAlphaSwizzle)
	{
		return IMG_FALSE;
	}

	/*	
		Check the source to the second instruction which isn't the result of the first
		instruction is compatible with the register bank restrictions on source 0.
	*/
	if (!CanUseSource0(psState, psBlock->psOwner->psFunc, &psNextInst->asArg[1 - uMatchArg]))
	{
		return IMG_FALSE;
	}

	/*
		Convert the first instruction to a SOP3.
	*/
	SetOpcode(psState, psInst, ISOP3);

	/*
		Move the destination of the second instruction to the first.
	*/
	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoevFromDestIdx */);
	psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	/*
		Copy the predicate from the second instruction to the first.
	*/
	CopyPredicate(psState, psInst, psNextInst);

	/*
		Convert

			MOVE	T.RGB = A; T.ALPHA = B
			MUL		D, T, S
		->
			SOP3	D, S, A, B
	*/
	MoveSrc(psState, psInst, 2 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
	MovePartialDestToSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromDestIdx */);
	MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psNextInst, 1 - uMatchArg /* uMoveFromSrcIdx */);

	/*
		DESTRGB = SRC0 * SRC1 + ZERO * SRC2
	*/
	psInst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
	if (bMulAlphaSwizzle)
	{
		psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
	}
	else
	{
		psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
	}
	psInst->u.psSop3->bComplementCSel1 = IMG_FALSE;
	psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
	psInst->u.psSop3->bNegateCResult = IMG_FALSE;

	/*
		DESTA = (1 - SRC0) * ZERO + SRC0 * SRC2;
	*/
	psInst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop3->uCoissueOp = USEASM_OP_ALRP;
	psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementASel1 = IMG_FALSE;
	psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_SRC2ALPHA;
	psInst->u.psSop3->bComplementASel2 = IMG_FALSE;
	psInst->u.psSop3->bNegateAResult = IMG_FALSE;

	return IMG_TRUE;
}

static IMG_BOOL CombineSharedDestInstructions(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psBlock,
											  PINST					psInst,
											  PINST					psNextInst,
											  IMG_PBOOL				pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: CombineSharedDestInstructions
    
 PURPOSE	: Try to combine two integer instructions which write the same
			  destination.

 PARAMETERS	: psBlock					- Flow control block containing the two
										instructions.
			  psInst, psNextInst		- The two instructions to combine.
			  pbDropNextInst			- Set to TRUE on return if psNextInst
										can be dropped.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	/*
		Ensure the predicates match.
	*/
	if (!EqualPredicates(psInst, psNextInst))
	{
		return IMG_FALSE;
	}

	/*
		Try to combine packs of the same type by merging their arguments.
	*/
	if (CombinePacksToU8C10(psState, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (CombineVectorPacksToU8C10(psState, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if ((psInst->eOpcode == IUNPCKF16U8 ||
		 psInst->eOpcode == IUNPCKF16O8 ||
		 psInst->eOpcode == IUNPCKF16S8 ||
		 psInst->eOpcode == IUNPCKF16C10) &&
		(psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 12) &&
		psNextInst->eOpcode == psInst->eOpcode &&
		(psNextInst->auDestMask[0] == 3 || psNextInst->auDestMask[0] == 12) &&
		psNextInst->auDestMask[0] != psInst->auDestMask[0])
	{
		IMG_UINT32	uOrigInstDestMask;

		uOrigInstDestMask = psInst->auDestMask[0];
		psInst->auDestMask[0] |= psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
		if (uOrigInstDestMask < psNextInst->auDestMask[0])
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}
		else
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
			MovePackSource(psState, psInst, 0 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}
		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		return IMG_TRUE;
	}

	/*
		Check for a MOV and non-converting PACK with the same
		arguments.
	*/
	if (
			psInst->eOpcode == IMOV &&
			(
				psNextInst->eOpcode == IPCKU8U8 ||
				psNextInst->eOpcode == IPCKC10C10
			) && 
			psNextInst->asArg[0].uType == psInst->asArg[0].uType &&
			psNextInst->asArg[0].uNumber == psInst->asArg[0].uNumber &&
			(
				g_abSingleBitSet[psNextInst->auDestMask[0]] ||
				(
					g_auSetBitCount[psNextInst->auDestMask[0]] == 2 &&
					psNextInst->asArg[1].uType == psInst->asArg[0].uType &&
					psNextInst->asArg[1].uNumber == psInst->asArg[0].uNumber
				)
			)
		)
	{
		IMG_UINT32	uComp1, uComp2 = 0;

		/*
			Get the component set in the second instruction's destination mask.
		*/
		for (uComp1 = 0; (psNextInst->auDestMask[0] & (1U << uComp1)) == 0; uComp1++);
		if (!g_abSingleBitSet[psNextInst->auDestMask[0]])
		{
			for (uComp2 = uComp1 + 1; (psNextInst->auDestMask[0] & (1U << uComp2)) == 0; uComp2++);
		}

		/*
			Check that the second instruction isn't doing a swizzle.
		*/
		if (uComp1 == GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */) &&
			(g_abSingleBitSet[psNextInst->auDestMask[0]] || uComp2 == GetPCKComponent(psState, psNextInst, 1 /* uArgIdx */)))
		{
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			psInst->auLiveChansInDest[0] |= psNextInst->auLiveChansInDest[0];

			*pbMovesGenerated = IMG_TRUE;
			return IMG_TRUE;
		}
	}
	/*
		Check for combing a MOV and a PACK by merging their arguments.
	*/
	if (psInst->eOpcode == IMOV && 
		g_abSingleBitSet[psInst->auLiveChansInDest[0]] &&
		(psNextInst->eOpcode == IPCKU8U8 || psNextInst->eOpcode == IPCKC10C10) &&
		g_abSingleBitSet[psNextInst->auDestMask[0]])
	{
		IMG_UINT32	uChan;

		for (uChan = 0; (psInst->auLiveChansInDest[0] & (1U << uChan)) == 0; uChan++);

		SetOpcode(psState, psInst, psNextInst->eOpcode);
		
		SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uChan);
		if (psInst->auLiveChansInDest[0] < psNextInst->auDestMask[0])
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}
		else
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
			MovePackSource(psState, psInst, 0 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}

		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		psInst->auDestMask[0] = psInst->auLiveChansInDest[0] | psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		return IMG_TRUE;
	}
	/*
		Check for combining packs with different source formats by converting
		some of their arguments.
	*/
	if (
			(
				(psInst->eOpcode == IPCKU8F32 && psNextInst->eOpcode == IPCKU8U8) ||
				(psInst->eOpcode == IPCKU8F32 && psNextInst->eOpcode == IPCKU8F16) ||
				(psInst->eOpcode == IPCKU8U8 && psNextInst->eOpcode == IPCKU8F32) ||
				(psInst->eOpcode == IPCKU8U8 && psNextInst->eOpcode == IPCKU8F16) ||
				(psInst->eOpcode == IPCKU8F16 && psNextInst->eOpcode == IPCKU8F32) ||
				(psInst->eOpcode == IPCKU8F16 && psNextInst->eOpcode == IPCKU8U8)
			) &&
			g_abSingleBitSet[psInst->auDestMask[0]] &&
			g_abSingleBitSet[psNextInst->auDestMask[0]]
	   )
	{
		if (CombineDifferentSrcFmtPacks(psState, psInst, psNextInst))
		{
			return IMG_TRUE;
		}
	}
	if (
			psInst->eOpcode == IMOV && 
			psNextInst->eOpcode == IPCKU8F32 &&
			g_auSetBitCount[psNextInst->auLiveChansInDest[0]] == 2
	   )
	{
		if (CombineDifferentSrcFmtPacks(psState, psInst, psNextInst))
		{
			return IMG_TRUE;
		}
	}

	/*
		Try to convert a pair of pack instructions which are equivalent to
		a move.
	*/
	if (
			(
				psInst->eOpcode == IPCKC10C10 ||
				psInst->eOpcode == IPCKU8U8
			) &&
			psNextInst->eOpcode == psInst->eOpcode &&
			psNextInst->auLiveChansInDest[0] == (psNextInst->auDestMask[0] | psInst->auDestMask[0])
	   )
	{
		if (ConvertPackToMove(psState, psInst, psNextInst))
		{
			return IMG_TRUE;
		}
	}

	/*
		Try to combine SOPWM instructions.
	*/
	if (psInst->eOpcode == ISOPWM && psNextInst->eOpcode == ISOPWM)
	{
		if (CombineSopwms(psState, psInst, psNextInst))
		{
			return IMG_TRUE;
		}
	}
	/*
		Try to combine a SOPWM instruction writing
		the RGB components of a register and a move into
		the ALPHA component.
	*/
	if (CombineSOPWMMOVToSOP2(psState, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	/*
		Try to optimize a pair of instructions that set the
		RGB and ALPHA components of a register.
	*/
	if (CombineMOVMOVToSOP2(psState, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	
	/*
		Try to combine a MOV into either rgb or alpha and a SOPWM
		where the SOPWM has a spare argument.
	*/
	if (psInst->eOpcode == IMOV && 
		psNextInst->eOpcode == ISOPWM &&
		(psNextInst->auDestMask[0] == 0x7 || psNextInst->auDestMask[0] == 0x8) &&
		GetLiveChansInArg(psState, psNextInst, 1) == 0)
	{
		CombineMOVSOPWM(psState, psInst, psNextInst);
		return IMG_TRUE;
	}
	/*
		Try to combine a PACK and a SOPWM where the SOPWM is
		effectively a move.
	*/
	if (psInst->eOpcode == IPCKU8U8 && 
		g_abSingleBitSet[psInst->auDestMask[0]] &&
		psNextInst->eOpcode == ISOPWM &&
		GetLiveChansInArg(psState, psNextInst, 1) == 0 &&
		psNextInst->asDest[0].eFmt == psNextInst->asArg[0].eFmt &&
		psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD &&
		psNextInst->u.psSopWm->uAop == USEASM_INTSRCSEL_ADD &&
		psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
		psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
		psNextInst->u.psSopWm->bComplementSel1 &&
		!psNextInst->u.psSopWm->bComplementSel2)
	{
		IMG_UINT32	uComponent;

		ASSERT((psInst->auDestMask[0] & psNextInst->auDestMask[0]) == 0);
		
		if (g_abSingleBitSet[psNextInst->auDestMask[0]])
		{
			/*
				Get the component set in the mask.
			*/
			for (uComponent = 0; uComponent < 4; uComponent++)
			{
				if (psNextInst->auDestMask[0] == (IMG_UINT32)(1U << uComponent))
				{
					break;
				}
			}

			if (psInst->auDestMask[0] < psNextInst->auDestMask[0])
			{
				MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);
				SetPCKComponent(psState, psInst, 1 /* uArgIdx */, uComponent);
			}
			else
			{
				MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);

				MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uComponent);
			}

			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			psInst->auDestMask[0] |= psNextInst->auDestMask[0];
			psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

			return IMG_TRUE;
		}
		else if (psInst->auDestMask[0] == 0x8 &&
				 GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 3 &&
				 (psInst->auDestMask[0] | psNextInst->auDestMask[0]) == psNextInst->auLiveChansInDest[0])
		{
			SetOpcode(psState, psInst, ISOP2);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
			MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrc */);

			psInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
			psInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop2->bComplementASel1 = IMG_FALSE;
			psInst->u.psSop2->bComplementASel2 = IMG_TRUE;
			psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;

			psInst->u.psSop2->uCOp = psNextInst->u.psSopWm->uCop;
			psInst->u.psSop2->uCSel1 = psNextInst->u.psSopWm->uSel1;
			psInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop2->bComplementCSel1 = psNextInst->u.psSopWm->bComplementSel1;
			psInst->u.psSop2->bComplementCSel2 = IMG_FALSE;
			psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;

			return IMG_TRUE;
		}
	}
	/*
		Try to combine a SOPWM doing a multiply and a MOV by using SOP3.
	*/
	if (CombineMulMovToLRP1(psState, psBlock, psInst, psNextInst, IMG_TRUE) ||
		CombineMulMovToLRP1(psState, psBlock, psNextInst, psInst, IMG_FALSE))
	{
		return IMG_TRUE;
	}
	/*
		Try to combine a SOPWM doing an RGB multiply and another doing an
		ALPHA multiply by using SOP3.
	*/
	if (CombineMulMulToSOP3(psState, psBlock, psInst, psNextInst))
	{
		return IMG_TRUE;
	}
	/*
		Try to combine a SOPWM doing a add and a MOV by using SOP3.
	*/
	if (CombineAddMovToSOP3(psState, psBlock, psInst, psNextInst, IMG_TRUE) ||
		CombineAddMovToSOP3(psState, psBlock, psNextInst, psInst, IMG_FALSE))
	{
		return IMG_TRUE;
	}
	/*
		Try to combine setting rgb to zero together with a multiply in alpha.
	*/
	{
		IMG_BOOL	bCombine = IMG_FALSE, bAlphaFirst = IMG_FALSE;

		if (IsAlphaMultiply(psState, psBlock, psInst) && IsRGBClear(psNextInst, NULL))
		{
			bCombine = IMG_TRUE;
			bAlphaFirst = IMG_TRUE;
		}
		if (IsRGBClear(psInst, psNextInst) && IsAlphaMultiply(psState, psBlock, psNextInst))
		{
			bCombine = IMG_TRUE;
		}
		if (bCombine)
		{
			SetOpcode(psState, psInst, ISOP3);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	
			if (bAlphaFirst)
			{
				if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[0]))
				{
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
				}
				else
				{
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
				}
			}
			else
			{
				if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psNextInst->asArg[0]))
				{
					MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psNextInst, 1 /* uCopyFromSrcIdx */);
				}
				else
				{	
					MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 1 /* uCopyFromSrcIdx */);
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psNextInst, 0 /* uCopyFromSrcIdx */);
				}
			}
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
	
			/* RGB = 1 * SRC1 + 0 * SRC2 */
			psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop3->bComplementCSel1 = IMG_TRUE;
			psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
			psInst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
			psInst->u.psSop3->bNegateCResult = IMG_FALSE;

			/* ALPHA = (1 - SRC0A) * ZERO + SRC0A * SRC2A */
			psInst->u.psSop3->uCoissueOp = USEASM_OP_ALRP;
			psInst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
			psInst->u.psSop3->bNegateAResult = IMG_FALSE;
			psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop3->bComplementASel1 = IMG_FALSE;
			psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_SRC2ALPHA;
			psInst->u.psSop3->bComplementASel2 = IMG_FALSE;

			return IMG_TRUE;
		}
	}

	/*
		Combine a masked move together with a PCK instruction.

			PCKUF32		D1, A, B
			SOPWM		D2.X[=D1], C, #0
		->
			PCKU8F32	D2.YZW[=C], A, B
	*/
	if (
			(
				psInst->eOpcode == IPCKU8F32 || 
				psInst->eOpcode == IPCKU8F16
			) && 
			psInst->apsOldDest[0] == NULL &&
			IsSOPWMMove(psNextInst) &&
			psNextInst->apsOldDest[0] != NULL
		)
	{
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] = (~psNextInst->auDestMask[0]) & USC_ALL_CHAN_MASK;
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, &psNextInst->asArg[0]);

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: CombineIntMulAdd
    
 PURPOSE	: Try to combine integer MUL and ADD instructions.

 PARAMETERS	: psState				- Internal compiler state.
			  psBlock				- Flow control block containing the
									two instructions.
			  psInst, psNextInst	- MUL and ADD instructions.
			  uSrcFromDest			- Mask of sources to the second instruction
									which are the result of the first instruction.
			  uPartialDestFromDest	- Mask of destinations to the second instruction
									which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
static IMG_BOOL CombineIntMulAdd(PINTERMEDIATE_STATE psState,
								 PCODEBLOCK psBlock,
								 PINST	psInst, 
								 PINST	psNextInst,
								 IMG_UINT32 uSrcFromDest,
								 IMG_UINT32 uPartialDestFromDest)
{
	IMG_UINT32	uMatchArg;

	/*
		Check for the first instruction doing SRC1*SRC2/SRC1*SRC2ALPHA
	*/
	if (
			!(
				psInst->eOpcode == ISOPWM &&
				NoPredicate(psState, psInst) &&
				psInst->auDestMask[0] == psInst->auLiveChansInDest[0] &&
				(
					(psInst->u.psSopWm->uCop != USEASM_INTSRCSEL_MAX) && 
					(psInst->u.psSopWm->uCop != USEASM_INTSRCSEL_MIN)
				) &&
				(
					(psInst->auDestMask[0] & 0x8) == 0 || 
					(psInst->u.psSopWm->uAop == psInst->u.psSopWm->uCop)
				) &&
				(psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2 || psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2ALPHA) &&
				psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
				!psInst->u.psSopWm->bComplementSel2 
			 )
	   )
	{
		return IMG_FALSE;
	}

	/*
		If the destination is U8 format and either of the sources are C10 format
		then the SOPWM includes an implicit saturate to 0..1 on the result. So we
		can't combine the two instructions.
	*/
	if (IsResultSaturated(psInst))
	{
		return IMG_FALSE;
	}

	/*
		Check for the second instruction doing SRC1+SRC2
	*/
	if (
			!(	
				psNextInst->eOpcode == ISOPWM &&
				NoPredicate(psState, psNextInst) &&
				psNextInst->auDestMask[0] == psNextInst->auLiveChansInDest[0] &&
				psNextInst->auDestMask[0] == psInst->auDestMask[0] &&
				(
					psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_ADD || 
					psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_SUB
				) &&
				(
					(psNextInst->auDestMask[0] & 0x8) == 0 || 
					psNextInst->u.psSopWm->uAop == psNextInst->u.psSopWm->uCop
				) &&
				psNextInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
				psNextInst->u.psSopWm->bComplementSel1 &&
				psNextInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
				psNextInst->u.psSopWm->bComplementSel2
			 )
	   )
	{
		return IMG_FALSE;
	}

	/*
		The second instruction completely updated its destination so doesn't have a partial
		destination.
	*/
	ASSERT(uPartialDestFromDest == 0);

	/*
		Check for an argument to the second instruction which matches the destination
		of the first instruction.
	*/
	if (!g_abSingleBitSet[uSrcFromDest])
	{
		return IMG_FALSE;
	}
	uMatchArg = g_aiSingleComponent[uSrcFromDest];

	/* 
		FPMA can only perform [-]A + B*C, so we cannot combine if the second 
		SOPWM is performing a SUB, and the result of the first SOPWM forms it's
		second argument (i.e. the two SOPWMs do: 
		
		X = B * C	=>	Y = A - B*C
		Y = A - X
	*/
	if (
			CanUseSource0(psState, psBlock->psOwner->psFunc, &psNextInst->asArg[1 - uMatchArg]) &&
			!(
				psNextInst->eOpcode == ISOPWM &&
				psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_SUB && 
				uMatchArg == 1
			 )
	   )
	{
		IMG_UINT32	uSOPWMSel1 = psInst->u.psSopWm->uSel1;
		IMG_BOOL	bSOPWMComplementSel1 = psInst->u.psSopWm->bComplementSel1;

		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		psInst->auDestMask[0] = USC_DESTMASK_FULL;
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		SetOpcode(psState, psInst, IFPMA);

		psInst->u.psFpma->uCSel0 = USEASM_INTSRCSEL_SRC0;
		psInst->u.psFpma->uASel0 = USEASM_INTSRCSEL_SRC0ALPHA;
		psInst->u.psFpma->uCSel1 = USEASM_INTSRCSEL_SRC1;
		psInst->u.psFpma->uCSel2 = uSOPWMSel1;
		psInst->u.psFpma->bComplementCSel0 = psInst->u.psFpma->bComplementASel0 = IMG_FALSE;
		psInst->u.psFpma->bComplementCSel1 = psInst->u.psFpma->bComplementASel1 = IMG_FALSE;
		psInst->u.psFpma->bComplementCSel2 = psInst->u.psFpma->bComplementASel2 = bSOPWMComplementSel1;

		MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
		MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 1 - uMatchArg /* uCopyFromSrcIdx */);

		if	(psNextInst->u.psSopWm->uCop == USEASM_INTSRCSEL_SUB)
		{
			psInst->u.psFpma->bNegateSource0 = IMG_TRUE;
		}

		return IMG_TRUE;
	}
	else 
	{
		IMG_BOOL bCanUseSrc0ForSrc1;

		if (psInst->u.psSopWm->bComplementSel1)
		{
			return IMG_FALSE;
		}

		/*
			Move an alpha swizzle of a secondary attribute into the secondary update program so
			we can put the other SOPWM source in source 0 of the SOP3.
		*/
		if (
			CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[0]) &&
				psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2ALPHA &&
				AddAlphaSwizzleC10Constant(psState, 
										   psInst, 
										   1 /* uArg */, 
										   psInst->auLiveChansInDest[0], 
										   IMG_TRUE /* bCheckOnly */)	
		   )
		{
			AddAlphaSwizzleC10Constant(psState, 
									   psInst, 
									   1 /* uArg */, 
									   psInst->auLiveChansInDest[0], 
									   IMG_FALSE /* bCheckOnly */);
			psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2;
		}

		/*
			Check if we can use SOP3 in the mode SRC1 * SRC0 + SRC2 * ONE or 
			SRC1 * ONE + SRC1 * SRC0. We need one of the sources to the
			multiply to fit in source 0 and we can't be using a source
			selector to alpha replicate the other source.
		*/
		bCanUseSrc0ForSrc1 = IMG_FALSE;
		if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[0]) && psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2)
		{
			bCanUseSrc0ForSrc1 = IMG_TRUE; 
		}

		if (bCanUseSrc0ForSrc1 || CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[1]))
		{
			IMG_UINT32	uSOPWMSel1 = psInst->u.psSopWm->uSel1;

			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

			SetOpcode(psState, psInst, ISOP3);
			psInst->u.psSop3->uCOp = psNextInst->u.psSopWm->uCop;
			psInst->u.psSop3->uAOp = psNextInst->u.psSopWm->uCop;
			psInst->u.psSop3->bNegateCResult = IMG_FALSE;
			psInst->u.psSop3->bNegateAResult = IMG_FALSE;
			psInst->u.psSop3->uCoissueOp = USEASM_OP_ASOP;
			if (uMatchArg == 0)
			{
				/*
					RGB	= SRC0 * SRC1 + (1 - ZERO) * SRC2
				*/
				if (uSOPWMSel1 == USEASM_INTSRCSEL_SRC2)
				{
					psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
				}
				else
				{
					ASSERT(uSOPWMSel1 == USEASM_INTSRCSEL_SRC2ALPHA);
					psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
				}
				psInst->u.psSop3->bComplementCSel1 = IMG_FALSE;
				psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
				psInst->u.psSop3->bComplementCSel2 = IMG_TRUE;
	
				/*
					ALPHA = SRC0 * SRC1 + (1 - ZERO) * SRC2
				*/
				psInst->u.psSop3->uAOp = psNextInst->u.psSopWm->uCop;
				psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_SRC0ALPHA;
				psInst->u.psSop3->bComplementASel1 = IMG_FALSE;
				psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_ZERO;
				psInst->u.psSop3->bComplementASel2 = IMG_TRUE;
			
				if (!bCanUseSrc0ForSrc1)
				{
					SwapInstSources(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
				}
				MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psNextInst, 1 - uMatchArg /* uCopyFromSrcIdx */);
			}
			else
			{	
				/*
					RGB = (1 - ZERO) * SRC1 + SRC0 * SRC2
				*/
				psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_ZERO;
				psInst->u.psSop3->bComplementCSel1 = IMG_TRUE;
				if (uSOPWMSel1 == USEASM_INTSRCSEL_SRC2)
				{
					psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_SRC0;
				}
				else
				{
					ASSERT(uSOPWMSel1 == USEASM_INTSRCSEL_SRC2ALPHA);
					psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_SRC0ALPHA;
				}
				psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
	
				/*
					ALPHA = (1 - ZERO) * SRC1 + SRC0 * SRC2
				*/
				psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
				psInst->u.psSop3->bComplementASel1 = IMG_TRUE;
				psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_SRC0ALPHA;
				psInst->u.psSop3->bComplementASel2 = IMG_FALSE;

				if (!bCanUseSrc0ForSrc1)
				{
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
				}
				else
				{
					MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
				}
			
				MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psNextInst, 1 - uMatchArg /* uCopyFromSrcIdx */);
			}

			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: FoldC10Combine
    
 PURPOSE	: Try to fold a COMBC10 instruction into a SOPWM instruction which
			  uses the combine result.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Flow control block containing the two
									instructions.
			  psInst, psNextInst	- The two instructions to optimize.
			  uSrcFromDest			- Mask of sources to the second instruction
									which are the result of the first instruction.
			  uPartialDestFromDest	- Mask of destinations to the second instruction
									which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
static IMG_BOOL FoldC10Combine(PINTERMEDIATE_STATE psState, 
							   PCODEBLOCK psBlock,
							   PINST psInst, 
							   PINST psNextInst,
							   IMG_UINT32 uSrcFromDest,
							   IMG_UINT32 uPartialDestFromDest)
{
	IMG_UINT32	uMatchArg;
	IMG_BOOL	bMulAlphaSwizzle;

	/*
		Check the first instruction is COMBC10.
	*/
	if (
			!(
				psInst->eOpcode == ICOMBC10 &&
				psInst->auDestMask[0] == USC_DESTMASK_FULL &&
				psInst->asArg[1].uType != USEASM_REGTYPE_TEMP
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is SOPWM.
	*/
	if (psNextInst->eOpcode != ISOPWM)
	{
		return IMG_FALSE;
	}
	/*
		Find the argument from the SOPWM which is the result of the COMBC10.
	*/
	if (!g_abSingleBitSet[uSrcFromDest])
	{
		return IMG_FALSE;
	}
	uMatchArg = g_aiSingleComponent[uSrcFromDest];
	if (psNextInst->auLiveChansInDest[0] != psNextInst->auDestMask[0])
	{
		return IMG_FALSE;
	}
	ASSERT(uPartialDestFromDest == 0);
	/*
		Check for a SOPWM doing a convert from C10 to U8.
	*/
	if (IsSOPWMMove(psNextInst) &&
		psNextInst->asDest[0].eFmt == UF_REGFORMAT_U8)
	{
		/*
			ICOMBC10	TEMP, RGB, ALPHA
			ISOPWM		DEST, TEMP, #0

			->

			SOP2		DEST, RGB, ALPHA
		*/
		SetOpcode(psState, psInst, ISOP2);
		MoveSrc(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);

		CopyPredicate(psState, psInst, psNextInst);

		/*
			DESTRGB = SRC1 * ONE + SRC2 * ZERO
		*/
		psInst->u.psSop2->uCSel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementCSel1 = IMG_TRUE;
		psInst->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementCSel2 = IMG_FALSE;

		/*
			DESTA = SRC1 * ZERO + SRC2 * ONE
		*/
		psInst->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementASel1 = IMG_FALSE;
		psInst->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSop2->bComplementASel2 = IMG_TRUE;

		psInst->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
		psInst->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;

		psInst->u.psSop2->bComplementCSrc1 = IMG_FALSE;
		psInst->u.psSop2->bComplementASrc1 = IMG_FALSE;

		return IMG_TRUE;
	}
	/*
		Check for a SOPWM doing a multiply.
	*/
	if (IsSOPWMMultiply(psNextInst, &bMulAlphaSwizzle))
	{
		if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psNextInst->asArg[1 - uMatchArg]))
		{
			/*
				ICOMBC10	TEMP, RGB, ALPHA
				ISOPWM		DEST, TEMP, OTHER

				->

				ISOP3		DEST, OTHER, RGB, ALPHA
			*/
			SetOpcode(psState, psInst, ISOP3);
			MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
			CopyPredicate(psState, psInst, psNextInst);
			MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
			MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
			MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psNextInst, 1 - uMatchArg /* uCopyFromSrcIdx */);

			/*
				DESTRGB = SRC0 * SRC1 + ZERO * SRC2
			*/
			psInst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
			if (bMulAlphaSwizzle)
			{
				psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
			}
			else
			{
				psInst->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
			}
			psInst->u.psSop3->bComplementCSel1 = IMG_FALSE;
			psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
			psInst->u.psSop3->bNegateCResult = IMG_FALSE;

			/*
				DESTA = (1 - SRC0) * ZERO + SRC0 * SRC2;
			*/
			psInst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
			psInst->u.psSop3->uCoissueOp = USEASM_OP_ALRP;
			psInst->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSop3->bComplementASel1 = IMG_FALSE;
			psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_SRC2ALPHA;
			psInst->u.psSop3->bComplementASel2 = IMG_FALSE;
			psInst->u.psSop3->bNegateAResult = IMG_FALSE;

			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL ConvertUnpackInstToO8(PINTERMEDIATE_STATE	psState,
									  PINST					psDepInst,
									  IMG_UINT32			uMatch,
									  PARG					psNewSrc,
									  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: ConvertUnpackInstToO8
    
 PURPOSE	: Check if its possible to convert an instruction from a pack C10->F32 
			  to pack a O8->F32.

 PARAMETERS	: psState		- Compiler state.
			  psDepInst		- Instruction using the packed data.
			  uMatch		- Mask of sources to the instruction using the packed
							data.
			  psNewSrc		- Replacement for source for the new packs.
			  bCheckOnly	- If TRUE just check if conversion is possible.

 RETURNS	: TRUE if conversion is possible.
*****************************************************************************/
{
	if (!(psDepInst->eOpcode == IUNPCKF32C10 || psDepInst->eOpcode == IUNPCKF16C10))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if (
			psDepInst->eOpcode == IUNPCKF16C10 && 
			psDepInst->auDestMask[0] == USC_DESTMASK_FULL && 
			uMatch != PCK_ALLSOURCES_ARGMASK
		)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	if (!bCheckOnly)
	{
		psDepInst->asArg[0].uType = psNewSrc->uType;
		psDepInst->asArg[0].uNumber = psNewSrc->uNumber;
		psDepInst->asArg[0].eFmt = psNewSrc->eFmt;
		if (psDepInst->eOpcode == IUNPCKF32C10)
		{
			ModifyOpcode(psState, psDepInst, IUNPCKF32O8);
		}
		else
		{
			ModifyOpcode(psState, psDepInst, IUNPCKF16O8);
			psDepInst->asArg[1].uType = psNewSrc->uType;
			psDepInst->asArg[1].uNumber = psNewSrc->uNumber;
			psDepInst->asArg[1].eFmt = psNewSrc->eFmt;
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL ConvertUnpacksToO8(PINTERMEDIATE_STATE	psState,
								   PINST				psNextInst,
								   PARG					psNewSrc,
								   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: ConvertUnpacksToO8
    
 PURPOSE	: Check if its possible to convert all the places where a
			  result is used from packs C10->F32 to packs O8->F32.

 PARAMETERS	: psState		- Compiler state.
			  psNextInst	- Instruction whose result is to be checked.
			  psNewSrc		- Replacement for source for the new packs.
			  bCheckOnly	- If TRUE just check if conversion is possible.

 RETURNS	: TRUE if conversion is possible.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PINST			psLastInst;
	IMG_UINT32		uSrcMask;
	PUSC_LIST_ENTRY	psListEntry;

	psUseDef = UseDefGet(psState, psNextInst->asDest[0].uType, psNextInst->asDest[0].uNumber);

	psLastInst = NULL;
	uSrcMask = 0;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		PINST	psUseInst;

		if (psUse == psUseDef->psDef)
		{
			continue;
		}
		if (psUse->eType != USE_TYPE_SRC)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
		psUseInst = psUse->u.psInst;
		if (psLastInst != psUseInst)
		{
			if (psLastInst != NULL)
			{
				if (!ConvertUnpackInstToO8(psState, psLastInst, uSrcMask, psNewSrc, bCheckOnly))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
			}
			uSrcMask = 0;
			psLastInst = psUseInst;
		}
		ASSERT(psUse->uLocation <= BITS_PER_UINT);
		uSrcMask |= (1U << psUse->uLocation);
	}

	if (uSrcMask != 0)
	{
		if (!ConvertUnpackInstToO8(psState, psLastInst, uSrcMask, psNewSrc, bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsSOP2DoubleMove(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsSOP2DoubleMove
    
 PURPOSE	: Check for a SOP2 which moves source 1 into the RGB channels of the
			  result and source 2 into the ALPHA channel of the result.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			(
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				psInst->eOpcode == ISOP2_VEC ||
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				psInst->eOpcode == ISOP2 
			) &&
			NoPredicate(psState, psInst) &&
			/*
				RGB = (1 - ZERO) * SRC1 + ZERO * SRC2
			*/
			psInst->u.psSop2->uCOp == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSop2->uCSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSop2->bComplementCSel1 &&
			psInst->u.psSop2->uCSel2 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSop2->bComplementCSel2 &&
			!psInst->u.psSop2->bComplementCSrc1 &&
			/*
				ALPHA = ZERO * SRC1 + (1 - ZERO) * SRC2
			*/
			psInst->u.psSop2->uAOp == USEASM_INTSRCSEL_ADD &&
			psInst->u.psSop2->uASel1 == USEASM_INTSRCSEL_ZERO &&
			!psInst->u.psSop2->bComplementASel1 &&
			psInst->u.psSop2->uASel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSop2->bComplementASel2 &&
			!psInst->u.psSop2->bComplementASrc1)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CombineSOPWMSOP2Move(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 PINST					psNextInst,
									 IMG_UINT32				uSrcFromDest,
									 IMG_UINT32				uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: CombineSOPWMSOP2Move
    
 PURPOSE	: Try to combine a SOPWM followed by a SOP2 which selects
			  the RGB channel of the SOPWM result and the ALPHA channel of
			  one of the multiply sources.

 PARAMETERS	: psState				- Compiler state.
			  psInst, psNextInst	- Two instructions to try and combine.
			  uSrcFromDest			- Mask of sources to the second instruction
									which are the result of the first instruction.
			  uPartialDestFromDest	- Mask of destinations to the second instruction
									which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uMatchDestArg, uMatchSrcArg;
	SOP2_PARAMS	sSop2;
	IMG_UINT32	uArg;

	if (	
			!(
				psInst->eOpcode == ISOPWM &&
				NoPredicate(psState, psInst)
			 )
	   )
	{
		return IMG_FALSE;
	}
	/*
		Check for a SOP2 moving source 1 into the RGB channels of the destination
		and source 2 into the ALPHA channel of the destination.
	*/
	if (!IsSOP2DoubleMove(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check if the SOP2 includes a saturation on the result.
	*/
	if (IsResultSaturated(psInst))
	{
		return IMG_FALSE;
	}

	/*	
		Check that exactly one of the SOP2 sources comes from the
		result of the SOPWM.
	*/
	if (!g_abSingleBitSet[uSrcFromDest])
	{
		return IMG_FALSE;
	}
	uMatchDestArg = g_aiSingleComponent[uSrcFromDest];
	/*
		Check the SOPWM is writing all the channels used from its destination.
	*/
	if (
			(uMatchDestArg == 0 && psInst->auDestMask[0] != 7) ||
			(uMatchDestArg == 1 && psInst->auDestMask[0] != 8)
	   )
	{
		return IMG_FALSE;
	}
	ASSERT(uPartialDestFromDest == 0);
	/*
		Check that the other SOP2 source is the same as one of the SOPWM sources.
	*/
	uMatchSrcArg = UINT_MAX;
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (EqualArgs(&psInst->asArg[uArg], &psNextInst->asArg[1 - uMatchDestArg]))
		{
			uMatchSrcArg = uArg;
			break;
		}
	}
	if (uMatchSrcArg == UINT_MAX)
	{
		return IMG_FALSE;
	}

	if (psInst->auDestMask[0] == 7)
	{
		/*
			Copy the SOPWM operation to SOP2 RGB operation.
		*/
		sSop2.uCOp = psInst->u.psSopWm->uCop;
		sSop2.uCSel1 = psInst->u.psSopWm->uSel1;
		sSop2.bComplementCSel1 = psInst->u.psSopWm->bComplementSel1;
		sSop2.uCSel2 = psInst->u.psSopWm->uSel2;
		sSop2.bComplementCSel2 = psInst->u.psSopWm->bComplementSel2;
		sSop2.bComplementCSrc1 = IMG_FALSE;

		/*
			Select one of the SOPWM argument into the alpha channels of the SOP2 destination.
		*/
		sSop2.uAOp = USEASM_INTSRCSEL_ADD;
		sSop2.uASel1 = USEASM_INTSRCSEL_ZERO;
		sSop2.uASel2 = USEASM_INTSRCSEL_ZERO;
		if (uMatchSrcArg == 0)
		{
			sSop2.bComplementASel1 = IMG_TRUE;
			sSop2.bComplementASel2 = IMG_FALSE;
		}
		else
		{
			sSop2.bComplementASel1 = IMG_FALSE;
			sSop2.bComplementASel2 = IMG_TRUE;
		}
		sSop2.bComplementASrc1 = IMG_FALSE;
	}
	else
	{
		/*
			Select one of the SOPWM argument into the rgb channels of the SOP2 destination.
		*/
		sSop2.uCOp = USEASM_INTSRCSEL_ADD;
		sSop2.uCSel1 = USEASM_INTSRCSEL_ZERO;
		sSop2.uCSel2 = USEASM_INTSRCSEL_ZERO;
		if (uMatchSrcArg == 0)
		{
			sSop2.bComplementCSel1 = IMG_TRUE;
			sSop2.bComplementCSel2 = IMG_FALSE;
		}
		else
		{
			sSop2.bComplementCSel1 = IMG_FALSE;
			sSop2.bComplementCSel2 = IMG_TRUE;
		}
		sSop2.bComplementCSrc1 = IMG_FALSE;

		/*
			Copy the SOPWM operation to the SOP2 ALPHA operation.
		*/
		sSop2.uAOp = psInst->u.psSopWm->uAop;
		sSop2.uASel1 = SelectAlphaInSrcSel(psState,psInst->u.psSopWm->uSel1);
		sSop2.bComplementASel1 = psInst->u.psSopWm->bComplementSel1;
		sSop2.uASel2 = SelectAlphaInSrcSel(psState, psInst->u.psSopWm->uSel2);
		sSop2.bComplementASel2 = psInst->u.psSopWm->bComplementSel2;
		sSop2.bComplementASrc1 = IMG_FALSE;
	}

	/*
		Combine the two instructions into a single SOP2.
	*/
	SetOpcode(psState, psInst, ISOP2);

	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	CopyPredicate(psState, psInst, psNextInst);
	psInst->auDestMask[0] = USC_DESTMASK_FULL;

	*psInst->u.psSop2 = sSop2;

	return IMG_TRUE;
}

static IMG_BOOL CombineSOP2MoveSOPWMMultiply(PINTERMEDIATE_STATE	psState,
											 PCODEBLOCK				psBlock,
											 PINST					psInst,
											 PINST					psNextInst,
											 IMG_UINT32				uSrcFromDest,
											 IMG_UINT32				uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: CombineSOP2MoveSOPWMMultiply
    
 PURPOSE	: Try to combine a SOP2 which moves one source into the RGB channels
			  of the result and the other into the ALPHA channel of the result
			  together with a multiply on the result of the SOP2.

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Flow control block containing the two
									instructions.
			  psInst, psNextInst	- Two instructions to try and combine.
			  uSrcFromDest			- Mask of sources to the second instruction
									which are the result of the first instruction.
			  uPartialDestFromDest	- Mask of destinations to the second instruction
									which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_UINT32	uMatchDestArg;
	IMG_UINT32	uRGBSrcSel, uAlphaSrcSel;

	/*
		Check for a SOP2 moving source 1 into the RGB channels of the destination
		and source 2 into the ALPHA channel of the destination.
	*/
	if (!IsSOP2DoubleMove(psState, psInst))
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is a multiply.
	*/
	if (!IsSOPWMMultiply(psNextInst, NULL))
	{
		return IMG_FALSE;
	}

	/*	
		Check that exactly one of the multiply sources comes from the
		result of the SOP2.
	*/
	if (!g_abSingleBitSet[uSrcFromDest])
	{
		return IMG_FALSE;
	}
	uMatchDestArg = g_aiSingleComponent[uSrcFromDest];

	/*
		Check the SOPWM doesn't do a partial update.
	*/
	if (psNextInst->auLiveChansInDest[0] != psNextInst->auDestMask[0])
	{
		return IMG_FALSE;
	}
	ASSERT(uPartialDestFromDest == 0);

	/*
		Check one of the sources to the SOP2 can use source 0.
	*/
	if (
			!CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[0]) &&
			!CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[1])
	   )
	{
		return IMG_FALSE;
	}

	/*
		Combine the two instructions into a single SOP3 writing the same
		destination.
	*/
	SetOpcode(psState, psInst, ISOP3);

	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
	CopyPredicate(psState, psInst, psNextInst);

	if (CanUseSource0(psState, psBlock->psOwner->psFunc, &psInst->asArg[0]))
	{
		MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);

		uRGBSrcSel = USEASM_INTSRCSEL_SRC0;
		uAlphaSrcSel = USEASM_INTSRCSEL_SRC2ALPHA;
	}
	else
	{
		MoveSrc(psState, psInst, 2 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);

		uRGBSrcSel = USEASM_INTSRCSEL_SRC2;
		uAlphaSrcSel = USEASM_INTSRCSEL_SRC0ALPHA;
	}
	MoveSrc(psState, psInst, 1 /* uCopyToSrcIdx */, psNextInst, 1 - uMatchDestArg /* uCopyFromSrcIdx */);

	/*
		RGB = (SOP2 RGB source) * SRC1 + ZERO * SRC2
	*/
	psInst->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop3->uCSel1 = uRGBSrcSel;
	psInst->u.psSop3->bComplementCSel1 = IMG_FALSE;
	psInst->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementCSel2 = IMG_FALSE;
	psInst->u.psSop3->bNegateCResult = IMG_FALSE;

	psInst->u.psSop3->uCoissueOp = USEASM_OP_ASOP;

	/*
		ALPHA = (SOP2 Alpha source) * SRC1 + ZERO * SRC2
	*/
	psInst->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
	psInst->u.psSop3->uASel1 = uAlphaSrcSel;
	psInst->u.psSop3->bComplementASel1 = IMG_FALSE;
	psInst->u.psSop3->uASel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSop3->bComplementASel2 = IMG_FALSE;
	psInst->u.psSop3->bNegateAResult = IMG_FALSE;

	return IMG_TRUE;
}

static IMG_BOOL CheckForScale(PINTERMEDIATE_STATE		psState,
							  PINST						psInst,
							  PINST						psNextInst,
							  IMG_PBOOL					pbMovesGenerated,
							  IMG_PBOOL					pbDropNextInst)
/*****************************************************************************
 FUNCTION	: CheckForScale
    
 PURPOSE	: 

 PARAMETERS	: psState					- Compiler state.
			  psInst, psNextInst		- The two instructions to optimize.
			  pbDropNextInst			- Set to TRUE on return if the next
										instruction can be dropped.
			  pbMovesGenerated			- Set to TRUE on return if new moves
										were generated.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	IMG_BOOL	bAlphaReplicate;

	/*
		Check a SOPWM multiplying a U8 value by a uniform with the destination in C10 format.
	*/
	if (IsSOPWMMultiply(psInst, &bAlphaReplicate) &&
		psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
		psInst->asArg[0].eFmt == UF_REGFORMAT_U8)
	{
		IMG_UINT32	uUsedChans;

		if (!bAlphaReplicate)
		{
			uUsedChans = psInst->auDestMask[0];
		}
		else
		{
			ASSERT(psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_SRC2ALPHA);
			uUsedChans = 8;
		}

		/*
			Check the uniform source has the fixed value 2
		*/
		if (IsStaticC10Value(psState, 2.0f, &psInst->asArg[1], uUsedChans))
		{
			/*
				Check for a SOPWM using the result of the first instruction and subtracting
				one from its result.
			*/
			if (IsSOPWMSub(psNextInst) &&
				psNextInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
				psNextInst->auDestMask[0] == psInst->auDestMask[0] &&
				NoPredicate(psState, psNextInst) &&
				UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]) &&
				psNextInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
				psNextInst->asArg[1].uNumber == USC_U8_VEC4_ONE)
			{
				if (ConvertUnpacksToO8(psState, psNextInst, NULL, IMG_TRUE))
				{
					SetOpcode(psState, psInst, IMOV);
					psInst->asDest[0].eFmt = UF_REGFORMAT_U8;

					*pbMovesGenerated = IMG_TRUE;
	
					ConvertUnpacksToO8(psState, psNextInst, &psInst->asDest[0], IMG_FALSE);	
					*pbDropNextInst = IMG_TRUE;
					return IMG_TRUE;
				}
			}
		}
	}
	/*
		Check a SOP3 multiplying a U8 value by a secondary attribute with the destination in C10 format.
	*/
	if (psInst->eOpcode == ISOP3 &&
		psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
		/*
			Check RGB=SRC0*SRC1+ZERO*SRC2
		*/
		psInst->u.psSop3->uCSel1 == USEASM_INTSRCSEL_SRC0 &&
		!psInst->u.psSop3->bComplementCSel1 &&
		psInst->u.psSop3->uCSel2 == USEASM_INTSRCSEL_ZERO &&
		!psInst->u.psSop3->bComplementCSel2 &&
		psInst->u.psSop3->uCOp == USEASM_INTSRCSEL_ADD &&
		!psInst->u.psSop3->bNegateCResult &&
		/* 
			Check ALPHA=(1-SRC0)*ZERO+SRC0*SRC2 
		*/
		psInst->u.psSop3->uCoissueOp == USEASM_OP_ALRP &&
		psInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_ZERO &&
		!psInst->u.psSop3->bComplementASel1 &&
		psInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_SRC2ALPHA &&
		!psInst->u.psSop3->bComplementASel2 &&
		!psInst->u.psSop3->bNegateAResult &&
		psInst->u.psSop3->uAOp == USEASM_INTSRCSEL_ADD &&
		/* Check the non-constant source is in U8 format format */
		psInst->asArg[0].eFmt == UF_REGFORMAT_U8)
	{
		/*
			Check the constant argument has the fixed value 2
		*/
		if (IsStaticC10Value(psState, 2.0f, &psInst->asArg[1], 0x7) &&
			IsStaticC10Value(psState, 2.0f, &psInst->asArg[2], 0x8))
		{
			/*
				Check for a SOPWM using the result of the first instruction and subtracting
				one from its result.
			*/
			if (IsSOPWMSub(psNextInst) &&
				psNextInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
				psNextInst->auDestMask[0] == psInst->auDestMask[0] &&
				NoPredicate(psState, psNextInst) &&
				UseDefIsSingleSourceUse(psState, psNextInst, 0 /* uSrcIdx */, &psInst->asDest[0]) && 
				psNextInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
				psNextInst->asArg[1].uNumber == USC_U8_VEC4_ONE)
			{
				if (ConvertUnpacksToO8(psState, psNextInst, NULL, IMG_TRUE))
				{
					SetOpcode(psState, psInst, IMOV);
					psInst->asDest[0].eFmt = UF_REGFORMAT_U8;

					*pbMovesGenerated = IMG_TRUE;
	
					ConvertUnpacksToO8(psState, psNextInst, &psInst->asDest[0], IMG_FALSE);	
					*pbDropNextInst = IMG_TRUE;
					return IMG_TRUE;
				}
			}
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL CheckIntegerMaxMin(PINTERMEDIATE_STATE		psState,
								   PINST					psInst,
								   IMG_UINT32				uOp,
								   IMG_BOOL					bOne,
								   IMG_PUINT32				puNonConstArg)
/*****************************************************************************
 FUNCTION	: CheckIntegerMaxMin
    
 PURPOSE	: Check for an integer instruction doing a MIN or a MAX against
			  a constant value.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- The instruction to check.
			  uOp					- Operation to check for (either MIN or MAX)
			  bOne					- If TRUE check for 1 else check for 0.
			  puNonConstArg			- Returns the number of the argument that
									doesn't contain the constant value.

 RETURNS	: TRUE if the instruction is a MIN/MAX.
*****************************************************************************/
{
	if (
			psInst->eOpcode == ISOPWM &&
			NoPredicate(psState, psInst) &&
			psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
			psInst->u.psSopWm->uCop == uOp &&
			psInst->u.psSopWm->uAop == uOp &&
			psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel1 &&
			psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO &&
			psInst->u.psSopWm->bComplementSel2
	   )
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < 2; uArg++)
		{
			PARG	psArg = &psInst->asArg[uArg];
			
			if (bOne)
			{
				if (psArg->uType == USEASM_REGTYPE_IMMEDIATE &&
					psArg->uNumber == USC_U8_VEC4_ONE)
				{
					*puNonConstArg = 1 - uArg;
					return IMG_TRUE;
				}
			}
			else
			{
				if (
						psArg->uType == USEASM_REGTYPE_IMMEDIATE &&
						psArg->uNumber == 0
				   )
				{
					*puNonConstArg = 1 - uArg;
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL ConvertMinMaxToFormatConvert(PINTERMEDIATE_STATE	psState,
											 PINST					psInst,
											 PINST					psNextInst,
											 IMG_UINT32				uSrcFromDest,
											 IMG_UINT32				uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: ConvertMinMaxToFormatConvert
    
 PURPOSE	: Convert a sequence of instructions doing MIN(MAX(X, 0), 1) to
			  a format conversion from C10 to U8 and then back again.

 PARAMETERS	: psInst, psNextInst		- The two instructions to optimize.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are the result of the first instruction.
			  uPartialDestFromDest		- Mask of destinations to the second instruction
										which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were converted.
*****************************************************************************/
{
	IMG_UINT32	uArg1, uArg2;
	IMG_UINT32	uU8TempNum;

	/*
		Check the first instruction is doing DEST = MAX(SRC, 0)
	*/
	if (!CheckIntegerMaxMin(psState,
							psInst, 
						    USEASM_INTSRCSEL_MAX, 
						    IMG_FALSE, 
						    &uArg1))
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is doing DEST' = MIN(DEST, 1)
	*/
	if (!CheckIntegerMaxMin(psState,
							psNextInst, 
							USEASM_INTSRCSEL_MIN, 
							IMG_TRUE,
							&uArg2))
	{
		return IMG_FALSE;
	}
	/*
		Check the result of the first instruction is used only in the second
		instruction.
	*/
	if (uSrcFromDest != (IMG_UINT32)(1U << uArg2))
	{
		return IMG_FALSE;
	}
	if (psInst->auDestMask[0] != psNextInst->auDestMask[0])
	{
		return IMG_FALSE;
	}

	/*
		Convert the second instruction to a MOVE with the new destination as a source.
	*/
	uU8TempNum = GetNextRegister(psState);
	SetSrc(psState, psNextInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);
	if (uPartialDestFromDest != 0)
	{
		CopyPartiallyWrittenDest(psState, psNextInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
	}

	psNextInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
	psNextInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
	psNextInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
	psNextInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
	psNextInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psNextInst->u.psSopWm->bComplementSel2 = IMG_FALSE;

	/*
		Convert the first instruction to a MOVE but with a U8 destination.
	*/
	SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);
	SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */,  NULL /* psPartialDest */);
	psInst->auLiveChansInDest[0] = psInst->auDestMask[0];

	if (uArg1 != 0)
	{
		MoveSrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, uArg1);
	}

	psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
	psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
	psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
	psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;

	return IMG_TRUE;
}

static IMG_BOOL FoldDestFormatConversion(PINTERMEDIATE_STATE	psState,
										 PINST					psInst,
										 PINST					psNextInst,
										 IMG_UINT32				uSrcFromDest,
										 IMG_UINT32				uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: FoldDestFormatConversion
    
 PURPOSE	: Optimize an instruction followed by a format conversion on the
			  result to one instruction.

 PARAMETERS	: psInst, psNextInst		- The two instructions to optimize.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are the result of the first instruction.
			  uPartialDestFromDest		- Mask of destinations to the second instruction
										which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were converted.
*****************************************************************************/
{
	/*
		Check the first instruction can convert it's result to U8 format.
	*/
	if (!HasC10FmtControl(psInst))
	{
		return IMG_FALSE;
	}
	/*
		Check the format of the first instruction's result is C10.
	*/
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_C10)
	{
		return IMG_FALSE;
	}

	/* second instruction is converting the first to F16
	can just make the arithmetic be f16 then avoid the conversion */
	if (psInst->eOpcode == IFPDOT &&
		psInst->u.psDot34->uVecLength == 3 &&
		psInst->u.psDot34->uDot34Scale == 0 &&
		!psInst->u.psDot34->bOffset &&
		psNextInst->eOpcode == IUNPCKF16C10 &&
		EqualArgs(&psInst->asArg[0], &psInst->asArg[1]))
	{
		IMG_UINT32 uDestIdx;
		PINST psSourceInst = UseDefGetDefInst(psState, psInst->asArg[0].uType, psInst->asArg[0].uNumber, &uDestIdx);
		if (psSourceInst &&
			uDestIdx == 0 &&
			psSourceInst->eOpcode == IPCKC10F16 &&
			psSourceInst->auLiveChansInDest[0] == 7 &&
			psSourceInst->auDestMask[0] == 1 &&
			psSourceInst->u.psPck->auComponent[0] == 0 &&
			psSourceInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
			psSourceInst->asArg[1].uNumber == 0 &&
			psSourceInst->apsOldDest[0])
		{
			PINST psOldDestInst = UseDefGetDefInst(psState, psSourceInst->apsOldDest[0]->uType, psSourceInst->apsOldDest[0]->uNumber, &uDestIdx);
			if (psOldDestInst &&
				uDestIdx == 0 &&
				psOldDestInst->eOpcode == IPCKC10F16 &&
				psOldDestInst->auLiveChansInDest[0] == 6 &&
				psOldDestInst->auDestMask[0] == 6 &&
				EqualArgs(&psOldDestInst->asArg[0], &psOldDestInst->asArg[1]))
			{
				/* got a C10 DP instruction reading from packs of F16->C10.  so just make an F16 DP
				instruction instead and convert the dest from F32 to F16 */
				IMG_UINT32 uNewFMSADestReg = GetNextRegister(psState);
				PINST psNewFMSAInst = psInst;//AllocateInst(psState, psInst);

				IMG_UINT32 uNewFMADDestReg = GetNextRegister(psState);
				PINST psNewFMADInst = AllocateInst(psState, psInst);

				PINST psNewPackInst = AllocateInst(psState, psInst);

				/* setup the FMSA */
				SetOpcode(psState, psNewFMSAInst, IFMSA);

				SetDest(psState, psNewFMSAInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewFMSADestReg, UF_REGFORMAT_F32);
				SetSrcFromArg(psState, psNewFMSAInst, 0 /* uSrcIdx */, &psOldDestInst->asArg[0]);
				SetSrcFromArg(psState, psNewFMSAInst, 1 /* uSrcIdx */, &psSourceInst->asArg[0]);
				SetSrcFromArg(psState, psNewFMSAInst, 2 /* uSrcIdx */, &psSourceInst->asArg[0]);

				SetComponentSelect(psState, psNewFMSAInst, 0 /* uSrcIdx */, GetComponentSelect(psState, psOldDestInst, 0));
				SetComponentSelect(psState, psNewFMSAInst, 1 /* uSrcIdx */, GetComponentSelect(psState, psSourceInst, 0));
				SetComponentSelect(psState, psNewFMSAInst, 2 /* uSrcIdx */, GetComponentSelect(psState, psSourceInst, 0));

				psNewFMSAInst->auLiveChansInDest[0] = 15;

				/* setup the FMAD */
				SetOpcode(psState, psNewFMADInst, IFMAD);
				InsertInstAfter(psState, psInst->psBlock, psNewFMADInst, psNextInst);

				SetDest(psState, psNewFMADInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewFMADDestReg, UF_REGFORMAT_F32);
				SetSrcFromArg(psState, psNewFMADInst, 0 /* uSrcIdx */, &psOldDestInst->asArg[1]);
				SetSrcFromArg(psState, psNewFMADInst, 1 /* uSrcIdx */, &psOldDestInst->asArg[1]);
				SetSrcFromArg(psState, psNewFMADInst, 2 /* uSrcIdx */, &psNewFMSAInst->asDest[0]);

				SetComponentSelect(psState, psNewFMADInst, 0 /* uSrcIdx */, GetComponentSelect(psState, psOldDestInst, 1));
				SetComponentSelect(psState, psNewFMADInst, 1 /* uSrcIdx */, GetComponentSelect(psState, psOldDestInst, 1));

				SetOpcode(psState, psNewPackInst, IPCKF16F32);
				MoveDest(psState, psNewPackInst, 0, psNextInst, 0);
				SetSrcFromArg(psState, psNewPackInst, 0 /* uSrcIdx */, &psNewFMADInst->asDest[0]);
				MoveSrc(psState, psNewPackInst, 1, psNextInst, 1);
				psNewPackInst->u.psPck->auComponent[0] = 0;
				psNewPackInst->u.psPck->bScale = IMG_FALSE;
				psNewPackInst->auDestMask[0] = psNextInst->auDestMask[0];
				psNewPackInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

				InsertInstAfter(psState, psInst->psBlock, psNewPackInst, psNewFMADInst);

				return IMG_TRUE;
			}
		}
	}

	/*
		Check the second instruction is just moving data.
	*/
	if (!IsSOPWMMove(psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check the second instruction is converting the first instruction's
		result to U8 format.
	*/
	if (
			!(
				psNextInst->asDest[0].eFmt == UF_REGFORMAT_U8 &&
				NoPredicate(psState, psNextInst) &&
				uSrcFromDest == 1 &&
				uPartialDestFromDest == 0
			  )
		)
	{
		return IMG_FALSE;
	}
	/*
		Check all the channels read by the second instruction come from the
		result of the first instruction.
	*/
	if ((psNextInst->auDestMask[0] & ~psInst->auDestMask[0]) != 0)
	{
		return IMG_FALSE;
	}
	/*
		Check either
		(a) The second instruction is writing all the live channels in
		its destination
		or
		(b) If the second instruction is partially updating its destination
		then the first instruction can also do partial updates.
	*/
	if (
			!(
				psInst->eOpcode == ISOPWM ||
				psNextInst->auDestMask[0] == psNextInst->auLiveChansInDest[0]
			 )
	   )
	{
		IMG_UINT32	uU8TempNum = GetNextRegister(psState);

		/*
			Don't remove the SOPWM but change the destination of the
			first instruction to U8.
		*/
		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);

		SetSrc(psState, psNextInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);

		return IMG_FALSE;
	}

	/*
		Copy the destination of the second instruction to the destination
		of the first.
	*/
	MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
	if (psInst->eOpcode == ISOPWM)
	{
		CopyPartiallyWrittenDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		psInst->auDestMask[0] = psNextInst->auDestMask[0];
	}
	psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

	return IMG_TRUE;
}

static IMG_BOOL CombineFormatConvertWithConstantSet(PINTERMEDIATE_STATE	psState,
													PINST				psInst,
													PINST				psNextInst,
													IMG_UINT32			uSrcFromDest,
													IMG_UINT32			uPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: CombineFormatConvertWithConstantSet
    
 PURPOSE	: Combine a format conversion together with an instruction copying constant
			  data into some channels of the result.

 PARAMETERS	: psInst, psNextInst		- The two instructions to optimize.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are the result of the first instruction.
			  uPartialDestFromDest		- Mask of destinations to the second instruction
										which are the result of the first instruction.

 RETURNS	: TRUE if the two instructions were converted.
*****************************************************************************/
{
	IMG_UINT32	uConstantChan;
	PARG		psOldDest;
	IMG_UINT32	uConstantData;

	/*
		Check for a format conversion.
	*/
	if (!(psInst->eOpcode == IPCKU8F32 || 
		  psInst->eOpcode == IPCKU8F16 ||
		  psInst->eOpcode == IPCKC10F32 ||
		  psInst->eOpcode == IPCKC10F16))
	{
		return IMG_FALSE;
	}
	if (!psInst->u.psPck->bScale)
	{
		return IMG_FALSE;
	}

	/*
		Check for a spare source argument to the format conversion.
	*/
	if (g_auSetBitCount[psInst->auDestMask[0]] != 1)
	{
		return IMG_FALSE;
	}

	/*
		Check the second instruction is combining the format conversion
		together with constant data.
	*/
	if (!IsSOPWMMove(psNextInst))
	{
		return IMG_FALSE;
	}
	if (psNextInst->asDest[0].eFmt != psNextInst->asArg[0].eFmt)
	{
		return IMG_FALSE;
	}
	if (!(uSrcFromDest == 1 && uPartialDestFromDest == 0))
	{
		return IMG_FALSE;
	}
	if (g_auSetBitCount[psNextInst->auDestMask[0]] != 3)
	{
		return IMG_FALSE;
	}
	uConstantChan = g_aiSingleComponent[(~psNextInst->auDestMask[0]) & USC_ALL_CHAN_MASK];

	if (psNextInst->apsOldDest[0] == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Check the data copied into the unwritten channels in the destination is
		constant.
	*/
	psOldDest = psNextInst->apsOldDest[0];
	if (psOldDest->uType != USEASM_REGTYPE_IMMEDIATE)
	{
		return IMG_FALSE;
	}
	
	uConstantData = psOldDest->uNumber >> (uConstantChan * BITS_PER_BYTE);
	if (!(uConstantData == 0 || uConstantData == 255))
	{
		return IMG_FALSE;
	}

	MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
	psInst->auDestMask[0] |= (1U << uConstantChan);
	psInst->auLiveChansInDest[0] |= (1U << uConstantChan);

	InitInstArg(&psInst->asArg[1]);
	if (uConstantData == 0)
	{
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
	}
	else
	{
		SetConstantOne(psState, psInst, &psInst->asArg[1]);
	}

	return IMG_TRUE;
}

static IMG_BOOL IsMulMov(PINST psInst, IMG_PBOOL pbSrc0AlphaSwizzle)
/*****************************************************************************
 FUNCTION	: IsMulMov
    
 PURPOSE	: Checks for an instruction moving SRC1 into the ALPHA channel
			  of the result and multiply SRC0 and SRC1 into the RGB channels
			  of the result.

 PARAMETERS	: psInst				- Instruction to check.
			  pbSrc0AlphaSwizzle	- Returns TRUE if the instruction has an
									alpha swizzle modifier on source 0.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PLRP1_PARAMS	psLrp1;

	if (psInst->eOpcode != ILRP1)
	{
		return IMG_FALSE;
	}
	psLrp1 = psInst->u.psLrp1;

	/* RGB = (1 - SRC0) * ZERO + SRC0 * SRC2 */
	if (psLrp1->uCS == USEASM_INTSRCSEL_SRC0)
	{
		*pbSrc0AlphaSwizzle = IMG_FALSE;
	}
	else if (psLrp1->uCS == USEASM_INTSRCSEL_SRC0ALPHA)
	{
		*pbSrc0AlphaSwizzle = IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
	if (!(psLrp1->uCSel10 == USEASM_INTSRCSEL_ZERO && !psLrp1->bComplementCSel10))
	{
		return IMG_FALSE;
	}
	if (!(psLrp1->uCSel11 == USEASM_INTSRCSEL_SRC2 && !psLrp1->bComplementCSel11))
	{
		return IMG_FALSE;
	}

	/* ALPHA = ONE * SRC1 + ZERO * SRC2 */
	if (!(psLrp1->uAOp == USEASM_INTSRCSEL_ADD))
	{
		return IMG_FALSE;
	}
	if (!(psLrp1->uASel1 == USEASM_INTSRCSEL_ZERO && psLrp1->bComplementASel1))
	{
		return IMG_FALSE;
	}
	if (!(psLrp1->uASel2 == USEASM_INTSRCSEL_ZERO && !psLrp1->bComplementASel2))
	{
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

static IMG_BOOL CombineMul_MulMov(PINTERMEDIATE_STATE psState, PINST psInst, PINST psNextInst, IMG_UINT32 uSrcFromDest)
/*****************************************************************************
 FUNCTION	: CombineMul_MulMov
    
 PURPOSE	: Combine two multiplies with the same sourec arguments.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instructions to try and combine.
			  psNextInst
			  uSrcFromDest		- Mask of sources to the second instruction
								which are the as the destination to the first
								instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_BOOL	bInstAlphaSwizzle;
	IMG_BOOL	bNextInstSrc0AlphaSwizzle;
	IMG_BOOL	bCombine;

	/*
		Check the first instruction is a fixed-point multiply.
	*/
	if (!IsSOPWMMultiply(psInst, &bInstAlphaSwizzle))
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}
	if ((psInst->auDestMask[0] & USC_W_CHAN_MASK) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Check the first instruction does
			DEST.RGB = SRC0 * SRC2 or SRC0.ALPHA * SRC2
			DEST.ALPHA = SRC1
	*/
	if (!IsMulMov(psNextInst, &bNextInstSrc0AlphaSwizzle))
	{
		return IMG_FALSE;
	}
	if (!NoPredicate(psState, psNextInst))
	{
		return IMG_FALSE;
	}
	/*
		Check the result of the SOPWM is source 1.
	*/
	if (uSrcFromDest != (1U << 1))
	{
		return IMG_FALSE;
	}

	/*
		Check the two sources to the SOPWM are the same as the LRP1's source 0 and source 2.
	*/
	bCombine = IMG_FALSE;
	if (EqualArgs(&psInst->asArg[0], &psNextInst->asArg[0]) &&
		EqualArgs(&psInst->asArg[1], &psNextInst->asArg[2]) &&
		bInstAlphaSwizzle == bNextInstSrc0AlphaSwizzle)
	{
		bCombine = IMG_TRUE;
	}
	/*
		Check if the sources are the same but in the opposite order.
	*/
	else if (EqualArgs(&psInst->asArg[0], &psNextInst->asArg[2]) &&
			 EqualArgs(&psInst->asArg[1], &psNextInst->asArg[0]) &&
			 !bInstAlphaSwizzle &&
			 !bNextInstSrc0AlphaSwizzle)
	{
		bCombine = IMG_TRUE;
	}
	
	if (bCombine)
	{
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		return IMG_TRUE;
	}

	return IMG_FALSE;
}


static IMG_BOOL OptimizeDependentIntegerInstructions(PINTERMEDIATE_STATE psState,
													 PCODEBLOCK psBlock,
													 PINST psInst, 
													 PINST psNextInst, 
													 IMG_UINT32 uSrcFromDest,
													 IMG_UINT32 uPartialDestFromDest,
													 IMG_PBOOL pbDropNextInst, 
													 IMG_PBOOL pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: OptimizeDependentIntegerInstructions
    
 PURPOSE	: Try to optimize a pair of instructions with the second dependent on
			  the first.

 PARAMETERS	: psBlock					- The flow control block containing the	
										two instructions.
			  psInst, psNextInst		- The two instructions to optimize.
			  uSrcFromDest				- Mask of sources to the second instruction
										which are from the destination of the first.
			  uPartialDestFromDest		- Mask of partial destinations to the
										second instruction which are from the destination
										of the first.
			  pbDropNextInst			- Set to TRUE on return if the next
										instruction can be dropped.
			  pbMovesGenerated			- Set to TRUE on return if new moves
										were generated.

 RETURNS	: TRUE if the two instructions were combined.
*****************************************************************************/
{
	/*
		Check for a redundant conversion to float and then back again.
	*/
	if (psInst->eOpcode == IUNPCKF32U8 && 
		psInst->u.psPck->bScale &&
		psNextInst->eOpcode == IPCKU8F32 &&
		NoPredicate(psState, psNextInst) &&
		psNextInst->u.psPck->bScale)
	{
		if (RemoveRedundantConversion(psState, 
									  psInst, 
									  psNextInst, 
									  uSrcFromDest,
									  uPartialDestFromDest,
									  pbDropNextInst, 
									  pbMovesGenerated, 
									  IMG_FALSE))
		{
			return IMG_TRUE;
		}
	}
	if (psInst->eOpcode == IUNPCKF32C10 &&
		psInst->u.psPck->bScale &&
		psNextInst->eOpcode == IPCKC10F32 &&
		NoPredicate(psState, psNextInst) &&
		psNextInst->u.psPck->bScale)
	{
		if (RemoveRedundantConversion(psState,
									  psInst, 
									  psNextInst, 
									  uSrcFromDest,
									  uPartialDestFromDest,
									  pbDropNextInst, 
									  pbMovesGenerated, 
									  IMG_TRUE))
		{
			return IMG_TRUE;
		}
	}

	if (psInst->eOpcode == IPCKC10F16 &&
		psInst->u.psPck->bScale &&
		psNextInst->eOpcode == IUNPCKF16C10 &&
		NoPredicate(psState, psNextInst) &&
		psNextInst->u.psPck->bScale &&
		EqualArgs(&psInst->asArg[0], &psInst->asArg[1]) &&
		psInst->auDestMask[0] == 6 &&
		psNextInst->auDestMask[0] == USC_DESTMASK_FULL &&
		psNextInst->auLiveChansInDest[0] == USC_DESTMASK_FULL &&
		GetComponentSelect(psState, psInst, 0 /* uArgIdx */) == 2 &&
		GetComponentSelect(psState, psInst, 1 /* uArgIdx */) == 0 &&
		GetComponentSelect(psState, psNextInst, 0 /* uArgIdx */) == 2 &&
		GetComponentSelect(psState, psNextInst, 1 /* uArgIdx */) == 1)
	{
		/* converting f16 value into c10 and reversing fields, then undoing this so just remove
		both instructions */
		SetOpcode(psState, psInst, IMOV);
		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psNextInst, 0 /* uCopyFromDestIdx */);
		psInst->auDestMask[0] = USC_DESTMASK_FULL;
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		*pbDropNextInst = IMG_TRUE;
		*pbMovesGenerated = IMG_TRUE;

		return IMG_TRUE;
	}


	/*
		Check for a C10/U8 arithmetic instruction followed by a format conversion of the result.
	*/
	if (FoldDestFormatConversion(psState, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		return IMG_TRUE;
	}

	/*
		Check if a case where a C10/U8 mul/add sequence can be combined into a FPMA.
	*/
	if (CombineIntMulAdd(psState, psBlock, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		return IMG_TRUE;
	}

	/*
		Try to combine a SOPWM which writes a result together with a SOPWM which
		uses it.
	*/
	if (CombineSOPWMMulSOPWMDouble(psState, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		return IMG_TRUE;
	}

	/*
		Check for combining C10 rgb and alpha followed by a SOPWM using the result.
		Replace the SOPWM by a SOP3 that uses the sources to the combine directly.
	*/
	if (FoldC10Combine(psState, psBlock, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		*pbDropNextInst = IMG_TRUE;
		return IMG_TRUE;
	}

	/*
		Check for instructions doing a 2X-1 operation in C10 format on a U8 texture result. If the result is 
		subsequently used in a UNPCKF32U8 or UNPCKF16U8 we can remove the the 2X-1 operation
		and change the unpacks to UNPCKF32O8 or UNPCKF16O8.
	*/
	if (CheckForScale(psState, psInst, psNextInst, pbMovesGenerated, pbDropNextInst))
	{
		return IMG_TRUE;
	}
		
	/*	
		C10 to U8 conversion should not be optimized.
	*/
	if(!((psInst->asDest[0].eFmt == UF_REGFORMAT_U8) && (psInst->asArg[0].eFmt == UF_REGFORMAT_C10)))
	{
		/*
			Check for combining a SOP2 moving two sources in the RGB and ALPHA channels of the 
			result following by a multiply on the result.
		*/
		if (CombineSOP2MoveSOPWMMultiply(psState, psBlock, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
		{
			return IMG_TRUE;
		}
	}

	/*
		Check for combining a SOPWM followed by a SOP2 which selects the result of SOPWM into
		the RGB channel and one of SOPWM sources into the ALPHA channel.
	*/
	if (CombineSOPWMSOP2Move(psState, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		return IMG_TRUE;
	}

	/*
		Convert a MIN(MAX(X, 0), 1) operation to a conversion to U8 and then back to C10.
	*/
	if (ConvertMinMaxToFormatConvert(psState, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		*pbDropNextInst = IMG_FALSE;
		return IMG_TRUE;
	}
	
	/*
		Check for combining a format conversion together with an instruction moving writing constant
		data into some channels of the result.
	*/
	if (CombineFormatConvertWithConstantSet(psState, psInst, psNextInst, uSrcFromDest, uPartialDestFromDest))
	{
		return IMG_TRUE;
	}

	if (uPartialDestFromDest == 0)
	{
		/*
			Check for combining a fixed point multiply into RGB with a move into ALPHA.
		*/
		if (CombineMulMovSrcToLRP1(psState, psBlock, psInst, psNextInst))
		{
			return IMG_TRUE;
		}

		/*
			Combine a move copying different data into the ALPHA and RGB channels with a multiply on the
			result.
		*/
		if (CombineAlphaRGBMoveMultiply(psState, psBlock, psInst, psNextInst, uSrcFromDest))
		{
			return IMG_TRUE;
		}

		if (CombineMul_MulMov(psState, psInst, psNextInst, uSrcFromDest))
		{
			return IMG_TRUE;
		}

		/*
			Merge a format conversion together with another instruction copying the result of the conversion
			and also a constant into its destination.
		*/
		if (CombineFormatConvertSetConst(psState, psInst, psNextInst, uSrcFromDest))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static IMG_BOOL FindOtherInstruction(PINTERMEDIATE_STATE	psState,
									 PINST					psFirstInst,
									 PINST*					ppsSecondInst,
									 IMG_PUINT32			puSrcFromDest,
									 IMG_PUINT32			puPartialDestFromDest)
/*****************************************************************************
 FUNCTION	: FindOtherInstruction
    
 PURPOSE	: Look for another instruction connected to the first and where
			  there are no dependencies preventing the first from moving
			  forward to the location of the second.

 PARAMETERS	: psState				- Compiler state.
			  psFirstInst			- First instruction.
			  ppsSecondInst			- Returns the second instruction.
			  puSrcFromDest			- If non-zero then a mask of sources to the
									second instruction which are the same as the
									destination of the first instruction.
									If zero then the first and second instructions
									share a destination.
			  puPartialDestFromDest	- Mask of partial destinations to the second
									instructions which the same as the destination
									of the first instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	IMG_UINT32		uSrcUseMask;
	IMG_UINT32		uPartialDestUseMask;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psSecondInst;

	if (!NoPredicate(psState, psFirstInst) || psFirstInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}

	psUseDef = UseDefGet(psState, psFirstInst->asDest[0].uType, psFirstInst->asDest[0].uNumber);

	if (psUseDef == NULL)
	{
		return IMG_FALSE;
	}

	psSecondInst = NULL;
	uSrcUseMask = 0;
	uPartialDestUseMask = 0;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{	
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psUseDef->psDef)
		{
			continue;
		}

		if (!(psUse->eType == USE_TYPE_SRC || psUse->eType == USE_TYPE_OLDDEST))
		{
			return IMG_FALSE;
		}
		if (psSecondInst != NULL && psSecondInst != psUse->u.psInst)
		{
			return IMG_FALSE;
		}
		if (psUse->u.psInst->psBlock != psFirstInst->psBlock)
		{
			return IMG_FALSE;
		}
		psSecondInst = psUse->u.psInst;

		if (psUse->eType == USE_TYPE_SRC)
		{
			uSrcUseMask |= (1U << psUse->uLocation);
		}
		else 
		{
			ASSERT(psUse->eType == USE_TYPE_OLDDEST);
			uPartialDestUseMask |= (1U << psUse->uLocation);
		}
	}

	/*
		No uses of the first instruction's destination.
	*/
	if (psSecondInst == NULL)
	{
		return IMG_FALSE;
	}

	*ppsSecondInst = psSecondInst;
	*puSrcFromDest = uSrcUseMask;
	*puPartialDestFromDest = uPartialDestUseMask;
	return IMG_TRUE;
}

static IMG_BOOL IntegerPeepholeOptimize(PINTERMEDIATE_STATE		psState,
										PCODEBLOCK				psBlock,
										PINST					psFirstInst,
										PINST*					ppsSecondInst,
										IMG_PBOOL				pbDropNextInst,
										IMG_PBOOL				pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: IntegerPeepholeOptimize
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: 
*****************************************************************************/
{
	/*
		Check for one of the instructions we can optimize.
	*/
	if (g_psInstDesc[psFirstInst->eOpcode].uOptimizationGroup & USC_OPT_GROUP_INTEGER_PEEPHOLE)
	{
		IMG_UINT32	uSrcFromDest;
		IMG_UINT32	uPartialDestFromDest;

		/*
			Look for a connected instruction.
		*/

		if (FindOtherInstruction(psState, psFirstInst, ppsSecondInst, &uSrcFromDest, &uPartialDestFromDest))
		{
			if (uSrcFromDest == 0)
			{
				/*
					Try to combine instructions writing the same destination.
				*/
				if (CombineSharedDestInstructions(psState, psBlock, psFirstInst, *ppsSecondInst, pbMovesGenerated))
				{
					return IMG_TRUE;
				}
			}
			else
			{
				/*
					Try to combine instructions where the first writes some of the
					sources to the second.
				*/
				if (OptimizeDependentIntegerInstructions(psState,
														 psBlock,
														 psFirstInst, 
														 *ppsSecondInst, 
														 uSrcFromDest,
														 uPartialDestFromDest,
														 pbDropNextInst, 
														 pbMovesGenerated))
				{
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL 
IMG_BOOL IntegerOptimize(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: IntegerOptimize
    
 PURPOSE	: Apply optimizations to the integer instructions in a block.

 PARAMETERS	: psBlock		- The block to optimize.

 RETURNS	: If new move instructions were introduced in the optimization step.
*****************************************************************************/
{
	IMG_BOOL		bChanges = IMG_FALSE;
	IMG_BOOL		bMerges;
	PINST			psFirstInst;
	PINST			psNextInst;

	*pbMovesGenerated = IMG_FALSE;

	/*
		Check for opportunities to replace temporaries containing C10 format
		data by temporaries containing U8 format data.
	*/
	FormatConvert(psState);

	if (EliminateIntegerMoves(psState, psBlock, pbMovesGenerated))
	{
		bChanges = IMG_TRUE;
	}

	if (NormaliseIntegerInstructions(psState, psBlock))
	{
		bChanges = IMG_TRUE;
	}

	bMerges = IMG_FALSE;
	for (psFirstInst = psBlock->psBody; psFirstInst != NULL; psFirstInst = psNextInst)
	{
		PINST		psSecondInst;
		IMG_BOOL	bDropNextInst;

		psNextInst = psFirstInst->psNext;
		bDropNextInst = IMG_TRUE;
		if (IntegerPeepholeOptimize(psState, psBlock, psFirstInst, &psSecondInst, &bDropNextInst, pbMovesGenerated))
		{
			if (bDropNextInst)
			{
				RemoveInst(psState, psBlock, psFirstInst);
				InsertInstBefore(psState, psBlock, psFirstInst, psSecondInst);
	
				RemoveInst(psState, psBlock, psSecondInst);
				if (psNextInst == psSecondInst)
				{
					psNextInst = psFirstInst;
				}
				FreeInst(psState, psSecondInst);
			}

			bMerges = bChanges = IMG_TRUE;
		}
	}

	return bChanges;
}

typedef struct _FORMATCONVERT_SET
{
	USC_LIST		sTempList;
	IMG_BOOL		bUnconvertable;
} FORMATCONVERT_SET, *PFORMATCONVERT_SET;

typedef struct _FORMATCONVERT_TEMP
{
	IOPCODE				eNewDefOpcode;
	USC_LIST_ENTRY		sTempListEntry;
	USC_LIST_ENTRY		sPendingTempListEntry;
	USC_LIST_ENTRY		sSetListEntry;
	IMG_UINT32			uTempNum;
	IMG_UINT32			uProcessedChanMask;
	IMG_UINT32			uPendingChanMask;
	PFORMATCONVERT_SET	psSet;
} FORMATCONVERT_TEMP, *PFORMATCONVERT_TEMP;

typedef struct _FORMATCONVERT_CONTEXT
{
	USC_LIST		sTempList;
	USC_LIST		sPendingTempList;
} FORMATCONVERT_CONTEXT, *PFORMATCONVERT_CONTEXT;

static IMG_VOID FormatConvert_CombineSets(PINTERMEDIATE_STATE	psState,
										  PFORMATCONVERT_SET	psSet1,
										  PFORMATCONVERT_SET	psSet2)
{
	PUSC_LIST_ENTRY		psListEntry;

	if (psSet1 == psSet2)
	{
		return;
	}

	for (psListEntry = psSet2->sTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFORMATCONVERT_TEMP	psSet2Temp;

		psSet2Temp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sSetListEntry);

		ASSERT(psSet2Temp->psSet == psSet2);
		psSet2Temp->psSet = psSet1;
	}

	AppendListToList(&psSet1->sTempList, &psSet2->sTempList);

	UscFree(psState, psSet2);
}

static PFORMATCONVERT_TEMP FormatConvert_FindTemp(PFORMATCONVERT_CONTEXT	psContext,
												  IMG_UINT32				uTempNum)
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psContext->sTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFORMATCONVERT_TEMP	psTemp;

		psTemp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sTempListEntry);

		if (psTemp->uTempNum == uTempNum)
		{
			return psTemp;
		}
	}
	return NULL;
}

static IMG_VOID FormatConvert_AddTemp(PINTERMEDIATE_STATE		psState, 
									  PFORMATCONVERT_CONTEXT	psContext,
									  IMG_UINT32				uTempNum, 
									  IMG_UINT32				uChanMask,
									  PFORMATCONVERT_TEMP		psLinkedTemp,
									  IOPCODE					eNewDefOpcode)
{
	PFORMATCONVERT_TEMP		psTemp;
	PFORMATCONVERT_SET		psSet;

	psTemp = FormatConvert_FindTemp(psContext, uTempNum);

	if (psTemp != NULL)
	{
		ASSERT((psTemp->uProcessedChanMask & uChanMask) == 0);
		ASSERT((psTemp->uPendingChanMask & uChanMask) == 0);

		if (psTemp->uPendingChanMask == 0)
		{
			AppendToList(&psContext->sPendingTempList, &psTemp->sPendingTempListEntry);
		}
		psTemp->uPendingChanMask |= uChanMask;

		if (psLinkedTemp != NULL)
		{
			FormatConvert_CombineSets(psState, psTemp->psSet, psLinkedTemp->psSet);
		}
		ASSERT(eNewDefOpcode == IINVALID);
		ASSERT(psTemp->eNewDefOpcode == IINVALID);

		return;
	}

	if (psLinkedTemp == NULL)
	{
		psSet = UscAlloc(psState, sizeof(*psSet));
		InitializeList(&psSet->sTempList);
		psSet->bUnconvertable = IMG_FALSE;
	}
	else
	{
		psSet = psLinkedTemp->psSet;
	}

	psTemp = UscAlloc(psState, sizeof(*psTemp));
	psTemp->uTempNum = uTempNum;
	psTemp->uProcessedChanMask = 0;
	psTemp->uPendingChanMask = uChanMask;
	psTemp->psSet = psSet;
	psTemp->eNewDefOpcode = eNewDefOpcode;
	AppendToList(&psContext->sTempList, &psTemp->sTempListEntry);
	AppendToList(&psContext->sPendingTempList, &psTemp->sPendingTempListEntry);
	AppendToList(&psSet->sTempList, &psTemp->sSetListEntry);
}

static IMG_BOOL FormatConvert_CheckReplace(PINTERMEDIATE_STATE		psState,
										   PFORMATCONVERT_CONTEXT	psContext,
										   PFORMATCONVERT_TEMP		psTemp)
{
	PUSEDEF_CHAIN	psConversionUseDef;
	PUSC_LIST_ENTRY	psListEntry;
		
	psConversionUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psTemp->uTempNum);

	for (psListEntry = psConversionUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psConversionUseDef->psDef)
		{
			continue;
		}
		if (!(psUse->eType == USE_TYPE_OLDDEST || psUse->eType == USE_TYPE_SRC))
		{
			return IMG_FALSE;
		}
		if (psUse->eType == USE_TYPE_SRC)
		{
			IMG_UINT32	uChanMask;
			PINST		psUseInst = psUse->u.psInst;

			uChanMask = GetLiveChansInArg(psState, psUseInst, psUse->uLocation);
			if ((uChanMask & ~psTemp->uProcessedChanMask) != 0)
			{
				return IMG_FALSE;
			}
			if (HasC10FmtControl(psUseInst))
			{
				continue;
			}
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psUseInst->eOpcode == IVPCKFLTC10)
			{
				continue;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			if (psUseInst->eOpcode == IUNPCKF32C10)
			{
				continue;
			}
			if (psUseInst->eOpcode == IUNPCKF16C10 || psUseInst->eOpcode == IPCKC10C10)
			{
				IMG_UINT32	uOtherSrc = 1 - psUse->uLocation;
				PARG		psOtherSrc;
				IMG_UINT32	uLiveChansInOtherSrc;

				uLiveChansInOtherSrc = GetLiveChansInArg(psState, psUseInst, uOtherSrc);
				if (uLiveChansInOtherSrc == 0)
				{
					continue;
				}
				psOtherSrc = &psUseInst->asArg[uOtherSrc];
				if (psOtherSrc->uType == USEASM_REGTYPE_IMMEDIATE && psOtherSrc->uNumber == 0)
				{
					continue;
				}
				if (psOtherSrc->uType == USEASM_REGTYPE_TEMP)
				{
					PFORMATCONVERT_TEMP	psOtherTemp;

					psOtherTemp = FormatConvert_FindTemp(psContext, psOtherSrc->uNumber);
					if (psOtherTemp == NULL)
					{
						return IMG_FALSE;
					}
					if ((uLiveChansInOtherSrc & ~psOtherTemp->uProcessedChanMask) != 0)
					{
						return IMG_FALSE;
					}
					if (psOtherTemp->psSet->bUnconvertable)
					{
						return IMG_FALSE;
					}
					if (psTemp->psSet != psOtherTemp->psSet)
					{
						FormatConvert_CombineSets(psState, psTemp->psSet, psOtherTemp->psSet);
					}
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		else
		{
			ASSERT(psUse->eType == USE_TYPE_OLDDEST);

			if (!NoPredicate(psState, psUse->u.psInst))
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

static IMG_VOID FormatConvert_DoReplace(PINTERMEDIATE_STATE		psState,
										PFORMATCONVERT_CONTEXT	psContext,
									    PFORMATCONVERT_TEMP		psTemp)
{
	PUSEDEF_CHAIN	psConversionUseDef;
	PUSEDEF			psConversionDef;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;
	IMG_UINT32		uU8TempNum;
	PINST			psDefInst;
	IMG_UINT32		uDefDestIdx;
	PARG			psOldDest;
		
	psConversionUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psTemp->uTempNum);

	uU8TempNum = GetNextRegister(psState);

	ASSERT(psConversionUseDef->psDef != NULL);
	psConversionDef = psConversionUseDef->psDef;

	ASSERT(psConversionDef->eType == DEF_TYPE_INST);
	psDefInst = psConversionDef->u.psInst;
	uDefDestIdx = psConversionDef->uLocation;

	if (psTemp->eNewDefOpcode != IINVALID)
	{
		ModifyOpcode(psState, psDefInst, psTemp->eNewDefOpcode);
	}

	psOldDest = psDefInst->apsOldDest[uDefDestIdx];
	if (psOldDest != NULL)
	{
		IMG_UINT32	uPreservedChanMask;

		uPreservedChanMask = GetPreservedChansInPartialDest(psState, psDefInst, uDefDestIdx);
		if ((uPreservedChanMask & psTemp->uProcessedChanMask) != 0)
		{
			ASSERT((uPreservedChanMask & ~psTemp->uProcessedChanMask) == 0);

			if (psOldDest->eFmt == UF_REGFORMAT_C10)
			{
				PFORMATCONVERT_TEMP	psOldDestTemp;

				ASSERT(psOldDest->uType == USEASM_REGTYPE_TEMP);
				psOldDestTemp = FormatConvert_FindTemp(psContext, psOldDest->uNumber);
				ASSERT(psOldDestTemp != NULL);
				ASSERT(!psOldDestTemp->psSet->bUnconvertable);
				ASSERT((uPreservedChanMask & ~psOldDestTemp->uProcessedChanMask) == 0);
			}
			else
			{
				ASSERT(psOldDest->eFmt == UF_REGFORMAT_U8);
			}
		}
		else
		{
			PINST	psCopyInst;

			psCopyInst = AllocateInst(psState, psDefInst);
			SetOpcode(psState, psCopyInst, IMOV);
			MoveDest(psState, psCopyInst, 0 /* uMoveToDestIdx */, psDefInst, uDefDestIdx);
			psCopyInst->auLiveChansInDest[0] = psDefInst->auLiveChansInDest[uDefDestIdx] & ~psTemp->uProcessedChanMask;
			MovePartialDestToSrc(psState, psCopyInst, 0 /* uMoveToSrcIdx */, psDefInst, uDefDestIdx);
			InsertInstBefore(psState, psDefInst->psBlock, psCopyInst, psDefInst);
		}
	}

	SetDest(psState, psDefInst, uDefDestIdx, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);
	psDefInst->auDestMask[uDefDestIdx] &= psTemp->uProcessedChanMask;
	psDefInst->auLiveChansInDest[uDefDestIdx] &= psTemp->uProcessedChanMask;

	for (psListEntry = psConversionUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psConversionUseDef->psDef)
		{
			continue;
		}
		
		if (psUse->eType == USE_TYPE_SRC)
		{
			PINST		psUseInst = psUse->u.psInst;
			IMG_UINT32	uSrcIdx = psUse->uLocation;
			IMG_UINT32	uLiveChans;

			uLiveChans = GetLiveChansInArg(psState, psUseInst, uSrcIdx);
			if ((uLiveChans & psTemp->uProcessedChanMask) != 0)
			{
				ASSERT((uLiveChans & ~psTemp->uProcessedChanMask) == 0);

				SetSrc(psState, psUseInst, uSrcIdx, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);

				switch (psUseInst->eOpcode)
				{
					case IUNPCKF32C10: ModifyOpcode(psState, psUseInst, IUNPCKF32U8); break;
					case IUNPCKF16C10: ModifyOpcode(psState, psUseInst, IUNPCKF16U8); break;
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					case IVPCKFLTC10: ModifyOpcode(psState, psUseInst, IVPCKFLTU8); break;
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					case IPCKC10C10: ModifyOpcode(psState, psUseInst, IPCKU8U8); break;
					default: break;
				}
			}
		}
		else
		{
			PINST		psUseInst = psUse->u.psInst;
			IMG_UINT32	uDestIdx = psUse->uLocation;
			IMG_UINT32	uPreservedChans;

			ASSERT(psUse->eType == USE_TYPE_OLDDEST);

			uPreservedChans = GetPreservedChansInPartialDest(psState, psUseInst, uDestIdx);
			if ((uPreservedChans & psTemp->uProcessedChanMask) != 0)
			{
				ASSERT((uPreservedChans & ~psTemp->uProcessedChanMask) == 0);

				SetPartialDest(psState, psUseInst, uDestIdx, USEASM_REGTYPE_TEMP, uU8TempNum, UF_REGFORMAT_U8);
			}
		}
	}
}


static IMG_VOID FormatConvert_FlagConvertedData(PINTERMEDIATE_STATE		psState, 
												PFORMATCONVERT_CONTEXT	psContext,
												PFORMATCONVERT_TEMP		psTemp,
												IMG_UINT32				uConvertChanMask)
{
	PUSEDEF_CHAIN	psConversionUseDef;
	PUSC_LIST_ENTRY	psListEntry;
		
	psConversionUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psTemp->uTempNum);

	for (psListEntry = psConversionUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;
		PINST	psUseInst;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psConversionUseDef->psDef)
		{
			continue;
		}

		psUseInst = psUse->u.psInst;
		if (psUse->eType == USE_TYPE_OLDDEST && NoPredicate(psState, psUseInst))
		{
			PARG		psDest = &psUseInst->asDest[psUse->uLocation];
			
			if (psDest->uType == USEASM_REGTYPE_TEMP)
			{
				IMG_UINT32	uChanMask;

				uChanMask = uConvertChanMask & ~psUseInst->auDestMask[psUse->uLocation];
				if (uChanMask != 0)
				{
					FormatConvert_AddTemp(psState, psContext, psDest->uNumber, uChanMask, psTemp, IINVALID /* eNewDefOpcode */);
				}
			}
		}
		else if (psUse->eType == USE_TYPE_SRC && psUseInst->eOpcode == IPCKC10C10)
		{
			IMG_UINT32	uComponent = GetPCKComponent(psState, psUseInst, psUse->uLocation);

			if ((uConvertChanMask & (1U << uComponent)) != 0)
			{
				IMG_UINT32	uDestMask = psUseInst->auDestMask[0];
				IMG_UINT32	uChanMask;

				if (psUse->uLocation == 0)
				{
					IMG_UINT32	uFirstSetBitMask, uThirdSetBitMask;
	
					uFirstSetBitMask = g_auSetBitMask[uDestMask][0];
					uThirdSetBitMask = g_auSetBitMask[uDestMask][2];
					uChanMask = uFirstSetBitMask | uThirdSetBitMask;
				}
				else
				{
					IMG_UINT32	uSecondSetBitMask, uFourthSetBitMask;

					ASSERT(psUse->uLocation == 1);
					
					uSecondSetBitMask = g_auSetBitMask[uDestMask][1];
					uFourthSetBitMask = g_auSetBitMask[uDestMask][3];
					uChanMask = uSecondSetBitMask | uFourthSetBitMask;
				}

				FormatConvert_AddTemp(psState, 
									  psContext, 
									  psUseInst->asDest[0].uNumber, 
									  uChanMask, 
									  psTemp, 
									  IINVALID /* eNewDefOpcode */);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID FormatConvert(PINTERMEDIATE_STATE psState)
{
	PUSC_LIST_ENTRY			psListEntry;
	FORMATCONVERT_CONTEXT	sContext;
	SAFE_LIST_ITERATOR		sIter;

	InitializeList(&sContext.sTempList);
	InitializeList(&sContext.sPendingTempList);

	/*
		Flag all the temporary registers containing data converted from U8 to C10.
	*/
	InstListIteratorInitialize(psState, ISOPWM, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);

		/*
			Look for an instruction which does U8->C10 conversion.
		*/
		if (
				IsSOPWMMove(psInst) &&
				NoPredicate(psState, psInst) &&
				psInst->asDest[0].uType == USEASM_REGTYPE_TEMP &&
				psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
				psInst->asArg[0].eFmt == UF_REGFORMAT_U8
		   )
		{
			FormatConvert_AddTemp(psState, 
								  &sContext, 
								  psInst->asDest[0].uNumber, 
								  psInst->auDestMask[0], 
								  NULL /* psLinkedTemp */, 
								  IINVALID /* eNewDefOpcode */);
		}
		/*
			Look for a constant C10 value which also fits in a U8 format register.
		*/
		if (
				psInst->eOpcode == IPCKC10F32 &&
				NoPredicate(psState, psInst) &&
				g_abSingleBitSet[psInst->auDestMask[0]] &&
				psInst->asDest[0].uType == USEASM_REGTYPE_TEMP &&
				psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
				(
					psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO ||
					psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1
				)
			)
		{
			FormatConvert_AddTemp(psState, 
								  &sContext, 
								  psInst->asDest[0].uNumber, 
								  psInst->auDestMask[0], 
								  NULL /* psLinkedTemp */, 
								  IPCKU8F32 /* eNewDefOpcode */);
		}
	}
	InstListIteratorFinalise(&sIter);

	/*
		Flag all temporary registers containing either swizzled copies of converted data 
		or partially overwritten converted data.
	*/
	while ((psListEntry = RemoveListHead(&sContext.sPendingTempList)) != NULL)
	{	
		PFORMATCONVERT_TEMP	psTemp;
		IMG_UINT32			uConvertChanMask;

		psTemp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sPendingTempListEntry);
		uConvertChanMask = psTemp->uPendingChanMask;
		ASSERT((psTemp->uProcessedChanMask & uConvertChanMask) == 0);
		psTemp->uProcessedChanMask |= uConvertChanMask;
		psTemp->uPendingChanMask = 0;
		FormatConvert_FlagConvertedData(psState, &sContext, psTemp, uConvertChanMask);
	}

	/*
		Check which temporary registers containing converted data can be replaced by straight copies
		of the U8 data.
	*/
	for (psListEntry = sContext.sTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFORMATCONVERT_TEMP	psTemp;

		psTemp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sTempListEntry);
		if (!psTemp->psSet->bUnconvertable)
		{
			if (!FormatConvert_CheckReplace(psState, &sContext, psTemp))
			{
				psTemp->psSet->bUnconvertable = IMG_TRUE;
			}
		}
	}

	/*
		Replace C10 format registers by U8 format registers.
	*/
	for (psListEntry = sContext.sTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFORMATCONVERT_TEMP	psTemp;

		psTemp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sTempListEntry);
		if (!psTemp->psSet->bUnconvertable)
		{
			FormatConvert_DoReplace(psState, &sContext, psTemp);
		}
	}

	/*
		Free used memory.
	*/
	while ((psListEntry = RemoveListHead(&sContext.sTempList)) != NULL)
	{
		PFORMATCONVERT_TEMP	psTemp;
		PFORMATCONVERT_SET	psSet;

		psTemp = IMG_CONTAINING_RECORD(psListEntry, PFORMATCONVERT_TEMP, sTempListEntry);

		psSet = psTemp->psSet;
		RemoveFromList(&psSet->sTempList, &psTemp->sSetListEntry);
		if (IsListEmpty(&psSet->sTempList))
		{
			UscFree(psState, psSet);
		}

		UscFree(psState, psTemp);
	}
}

