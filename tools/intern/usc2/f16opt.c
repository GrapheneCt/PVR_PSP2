/*************************************************************************
 * Name         : f16opt.c
 * Created      : Jan 2002
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
 * $Log: f16opt.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"

/* SrcRef: Reference to the source operand of an instruction */
typedef struct _SRCREF_
{
	PINST psInst;
	IMG_UINT32 uNum;
} SRCREF, *PSRCREF;
#define SRCREF_DEFAULT { NULL, ((IMG_UINT32)-1) }

static IMG_BOOL AllF16ChannelsTheSame(PARG	psArg)
/*****************************************************************************
 FUNCTION	: AllF16ChannelsTheSame
    
 PURPOSE	: Check for a source argument where the F16 component select doesn't
			  matter.

 PARAMETERS	: psArg			- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			psArg->uType == USEASM_REGTYPE_FPCONSTANT &&
			(
				psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO ||
				psArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE
			)
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CanSubstituteF16RegisterByC10(PINST			psNextInst,
											  IMG_UINT32	uAComponent)
/*****************************************************************************
 FUNCTION	: CanSubstituteF16RegisterByC10
    
 PURPOSE	: Check if a reference to an F16 format register can be replaced by a
			  C10 format register.

 PARAMETERS	: psNextInst		- Instruction to subtitute in.
			  uAComponent		- Component in the C10 register.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{	
	/*
		Convert PCKU8F16 to SOPWM converting C10->U8 providing the
		PCKU8F16 doesn't also do a swizzle.
	*/
	if (
			psNextInst->eOpcode == IPCKU8F16 &&
			psNextInst->u.psPck->bScale &&
			g_aiSingleComponent[psNextInst->auDestMask[0]] == (IMG_INT32)uAComponent
		)
	{
		return IMG_TRUE;
	}
	/*
		Convert PCKC10F16 -> PCKC10C10
	*/
	if (
			psNextInst->eOpcode == IPCKC10F16 &&
			g_abSingleBitSet[psNextInst->auDestMask[0]]
	   )
	{
		return IMG_TRUE;
	}
	/*
		Convert UNPCKF32F16 to UNPCKF32C10
	*/
	if (psNextInst->eOpcode == IUNPCKF32F16)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CanSubstituteF16RegisterSwizzle(PINTERMEDIATE_STATE	psState,
												PINST				psNextInst,
												PARG				psRegA,
												IMG_UINT32			uAComponent,
												IMG_BOOL			bAIsF32,
												IMG_UINT32			uBComponent,
												IMG_BOOL			bBIsF32,
												IMG_BOOL			bReplacingAllArgs)
/*****************************************************************************
 FUNCTION	: CanSubstituteF16RegisterSwizzle
    
 PURPOSE	: Check if a register can be replaced by a register with a different
			  format or swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psNextInst		- Instruction to substitute in.
			  psRegA			- Replacement register.
			  uAComponent		- Component in the replacement register.
			  bAIsF32			- TRUE if the replacement register is F32 format.
			  uBComponent		- Component in the replaced register.
			  bBIsF32			- TRUE if the replaced register is F32 format.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	ASSERT(!(bAIsF32 && bBIsF32));

	if (!(bAIsF32 || bBIsF32 || uAComponent != uBComponent))
	{
		/* No swizzle or format conversion required. */
		return IMG_TRUE;
	}

	/*
		F16 arithmetic instructions.
	*/
	if (TestInstructionGroup(psNextInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST))
	{
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES)
		{
			/*
				Any swizzle or format conversion is valid.
			*/
			return IMG_TRUE;
		}
		else
		{
			/* Shouldn't read data in F32 format. */
			ASSERT(!bBIsF32);

			if (bAIsF32)
			{
				/*
					For an instruction where only a single channel of the
					result is used convert the calculation to F32.
				*/
				if (
						NoPredicate(psState, psNextInst) &&
						(
							psNextInst->auLiveChansInDest[0] == 3 ||
							psNextInst->auLiveChansInDest[0] == 12
						) 
				   )
				{
					return IMG_TRUE;
				}
				return IMG_FALSE;
			}
			else
			{
				/*
					Check for a register where component selects don't
					matter.
				*/
				if (AllF16ChannelsTheSame(psRegA))
				{
					return IMG_TRUE;
				}
				return IMG_FALSE;
			}
		}
	}
	/*
		Instructions whose sources can be either F16 or F32.
	*/
	if (HasF16F32SelectInst(psNextInst))
	{
		/*
			If changing the source format of a complex op with the TYPE_PRESERVE
			flag then the format of the destination changes to match.  
		*/
		if (bAIsF32 &&
			(g_psInstDesc[psNextInst->eOpcode].uFlags & DESC_FLAGS_COMPLEXOP) != 0 &&
			GetBit(psNextInst->auFlag, INST_TYPE_PRESERVE) &&
			!NoPredicate(psState, psNextInst))
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}
	/*
		Format conversion instructions.
	*/
	if (psNextInst->eOpcode == IPCKF16F16 ||
		psNextInst->eOpcode == IPCKU8F16 ||
		psNextInst->eOpcode == IPCKC10F16 ||
		psNextInst->eOpcode == IPCKU8F32 ||
		psNextInst->eOpcode == IPCKC10F32 ||
		psNextInst->eOpcode == IUNPCKF32F16 ||
		psNextInst->eOpcode == IPCKF16F32)
	{
		/*
			Only change the source format if all the arguments
			to the instruction are replaced.
		*/
		if ((bAIsF32 || bBIsF32) && !bReplacingAllArgs)
		{
			return IMG_FALSE;
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_BOOL CanSubstituteF16FormatConvert(PINTERMEDIATE_STATE		psState,
											  PINST						psNextInst,
											  PARG						psRegA,
											  IMG_UINT32				uAComponent,
											  IMG_BOOL					bAIsF32,
											  IMG_BOOL					bAIsC10,
											  IMG_UINT32				uBComponent,
											  IMG_BOOL					bBIsF32,
											  PFLOAT_SOURCE_MODIFIER	psSourceModifier,
											  PSOURCE_VECTOR			psReplaceArgMask,
											  IMG_BOOL					bReplacingAllArgs)
/*****************************************************************************
 FUNCTION	: CanSubstituteF16FormatConvert
    
 PURPOSE	: Check if a register can be replaced by a register with a different
			  format or swizzle.

 PARAMETERS	: psState			- Compiler state.
			  psNextInst		- Instruction to substitute in.
			  psRegA			- Replacement register.
			  uAComponent		- Component in the replacement register.
			  bAIsF32			- TRUE if the replacement register is F32 format.
			  bAIsC10			- TRUE if the replacement register is C10 format.
			  uBComponent		- Component in the replaced register.
			  bBIsF32			- TRUE if the replaced register is F32 format.
			  bSourceModifier	- TRUE if the replacement register has a source
								modifier (NEGATE or ABSOLUTE).
			  psReplaceArgMask	- Mask of arguments to the instruction which are to
								be replaced.
			  bReplacingAllArgs	- TRUE if all arguments to the instruction are to be
								replaced or are unreferenced.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (bAIsC10)
	{
		/*
			Can an F16 format register be replaced by a C10 format register.
		*/
		ASSERT(!bBIsF32);
		ASSERT(psSourceModifier == NULL);
		if (!CanSubstituteF16RegisterByC10(psNextInst, uAComponent))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Check that this instruction can support any swizzle or format conversion.
		*/
		if (!CanSubstituteF16RegisterSwizzle(psState,
											 psNextInst,
											 psRegA,
											 uAComponent,
											 bAIsF32,
											 uBComponent,
											 bBIsF32,
											 bReplacingAllArgs))
		{
			return IMG_FALSE;
		}
		
		/*
			Check for trying to substitute into an instruction without source modifiers on the
			arguments.
		*/
		if (psSourceModifier != NULL && (psSourceModifier->bNegate || psSourceModifier->bAbsolute))
		{
			IMG_BOOL	bSModValid;
			IMG_UINT32	uArg;
	
			/*
				Does the instruction support source modifiers directly?
			*/
			bSModValid = IMG_TRUE;
			for (uArg = 0; uArg < psNextInst->uArgumentCount; uArg++)
			{
				if (CheckSourceVector(psReplaceArgMask, uArg))
				{
					if (!CanHaveSourceModifier(psState, psNextInst, uArg, psSourceModifier->bNegate, psSourceModifier->bAbsolute))
					{
						bSModValid = IMG_FALSE;
						break;
					}
				}
			}
			if (!bSModValid)
			{
				/*
					Could we insert an instruction to do the source modifier after the pack.
				*/
				if (
						!(
							psNextInst->eOpcode == IPCKF16F16 &&
							(psNextInst->auLiveChansInDest[0] & ~psNextInst->auDestMask[0]) == 0 &&
							NoPredicate(psState, psNextInst) &&
							bReplacingAllArgs
						 )
				    )
				{
					return IMG_FALSE;
				}
			}
		}
	}
	return IMG_TRUE;
}


static IMG_VOID SubstituteF16Argument(PINTERMEDIATE_STATE psState,
									  PCODEBLOCK psBlock,
									  PINST psNextInst,
									  IMG_UINT32 uArg,
									  PARG psRegA,
									  IMG_BOOL bAIsF32,
									  IMG_BOOL bAIsC10,
									  IMG_BOOL bBIsF32,
									  PFLOAT_SOURCE_MODIFIER psSourceModifier,
									  IMG_UINT32 uAComponent,
									  IMG_UINT32 uBComponent,
									  IMG_PBOOL pbNewMoves)
/*****************************************************************************
 FUNCTION	: SubstituteF16Argument
    
 PURPOSE	: Replace an argument to an F16 instruction with another register.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block contains the instruction.
			  psNextInst		- Instruction which is to have one of the	
								arguments substituted.
			  uArg				- Argument to be substituted.
			  bAIsF32			- Is an F32 value to be substituted.
			  uAComponent		- Argument component.
			  pbNewMoves		- Set to TRUE on return if new moves have
								been added.
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	if (bAIsC10)
	{
		if (psNextInst->eOpcode == IPCKU8F16)
		{
			ASSERT(uArg == 0);
		
			/*
				Replace the conversion F16->U8 by a conversion C10->U8
			*/
			SetOpcode(psState, psNextInst, ISOPWM);

			SetSrcFromArg(psState, psNextInst, 0 /* uSrcIdx */, psRegA);

			psNextInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psNextInst->asArg[1].uNumber = 0;

			psNextInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
			psNextInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
			psNextInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
			psNextInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
			psNextInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
			psNextInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
		}
		else if (psNextInst->eOpcode == IPCKC10F16)
		{
			ASSERT(g_abSingleBitSet[psNextInst->auDestMask[0]]);
			SetOpcode(psState, psNextInst, IPCKC10C10);
			SetSrcFromArg(psState, psNextInst, 0 /* uSrcIdx */, psRegA);
			SetComponentSelect(psState, psNextInst, 0 /* uSrcIdx */, uAComponent);
		}
		else
		{
			ASSERT(psNextInst->eOpcode == IUNPCKF32F16);
			SetOpcode(psState, psNextInst, IUNPCKF32C10);
			psNextInst->u.psPck->bScale = IMG_TRUE;
			SetSrcFromArg(psState, psNextInst, 0 /* uSrcIdx */, psRegA);
			SetComponentSelect(psState, psNextInst, 0 /* uArgIdx */, uAComponent);
		}

		return;
	}

	/*
		Check for changing format F32 -> F16
	*/
	if (bBIsF32 && !bAIsF32 && psNextInst->eOpcode == IPCKF16F32)
	{
		SetOpcode(psState, psNextInst, IPCKF16F16);
	}

	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) == 0)
	{
		/*
			Convert an F16 mad to an F32 mad.
		*/
		if ((TestInstructionGroup(psNextInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST)) &&
			bAIsF32)
		{
			PINST psPackInst;
			IMG_UINT32	uCArg;
			FLOAT_PARAMS sFloatParams;
			IMG_UINT32 uConvertedTemp;

			/*
				Convert the opcode.
			*/
			sFloatParams = psNextInst->u.psArith16->sFloat;
			switch (psNextInst->eOpcode)
			{
				case IFMOV16: SetOpcode(psState, psNextInst, IFMOV); break;
				case IFADD16: SetOpcode(psState, psNextInst, IFADD); break;
				case IFMUL16: SetOpcode(psState, psNextInst, IFMUL); break;
				case IFMAD16: SetOpcode(psState, psNextInst, IFMAD); break;
				default: imgabort();
			}
			*psNextInst->u.psFloat = sFloatParams;
		
			for (uCArg = 0; uCArg < psNextInst->uArgumentCount; uCArg++)
			{
				PARG	psCArg = &psNextInst->asArg[uCArg];

				/*
					If the MAD16 is of the upper halve of the arguments then adjust the
					component select.
				*/
				if (psNextInst->auLiveChansInDest[0] == 12)
				{
					psNextInst->u.psFloat->asSrcMod[uCArg].uComponent = 2;
				}

				/*
					Format selection isn't available on hardware constants so convert to their
					f32 equivalents.
				*/
				if (psCArg->uType == USEASM_REGTYPE_FPCONSTANT)
				{
					if (psCArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
					{
						psCArg->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					}
					else
					{
						ASSERT(psCArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO);
					}
					psCArg->eFmt = UF_REGFORMAT_F32;
					psNextInst->u.psFloat->asSrcMod[uCArg].uComponent = 0;
				}
			}	

			/*
				Pack the result back down to F16.
			*/
			uConvertedTemp = GetNextRegister(psState);

			psPackInst = AllocateInst(psState, psNextInst->psNext);

			SetOpcode(psState, psPackInst, IPCKF16F32);

			MoveDest(psState, psPackInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
			psPackInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
			psPackInst->auDestMask[0] = psNextInst->auLiveChansInDest[0];
			SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uConvertedTemp, UF_REGFORMAT_F32);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			InsertInstBefore(psState, psBlock, psPackInst, psNextInst->psNext);
	
			SetDest(psState, psNextInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uConvertedTemp, UF_REGFORMAT_F32);
			psNextInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;
		}	
	}

	/*
		Check for changing the source format of a
		pack instruction.
	*/
	if (bAIsF32)
	{
		switch (psNextInst->eOpcode)
		{
			case IPCKF16F16: 
				SetOpcode(psState, psNextInst, IPCKF16F32); 
				break;
			case IUNPCKF32F16: 	
				SetOpcode(psState, psNextInst, IMOV); 
				*pbNewMoves = IMG_TRUE; 
				break;
			case IPCKU8F16: 
				SetOpcode(psState, psNextInst, IPCKU8F32); 
				psNextInst->u.psPck->bScale = IMG_TRUE;
				break;
			case IPCKC10F16: 
				SetOpcode(psState, psNextInst, IPCKC10F32); 
				psNextInst->u.psPck->bScale = IMG_TRUE;
				break;
			default: break;
		}
	}
	if (bBIsF32)
	{
		switch (psNextInst->eOpcode)
		{
			case IPCKU8F32: 
				SetOpcode(psState, psNextInst, IPCKU8F16); 
				psNextInst->u.psPck->bScale = IMG_TRUE;
				break;
			case IPCKC10F32: 
				SetOpcode(psState, psNextInst, IPCKC10F16); 
				psNextInst->u.psPck->bScale = IMG_TRUE;
				break;
			default: break;
		}
	}

	/*
		Format conversion isn't available for hardware constants so instead convert
		1.0 in f16 format to the equivalent constant in f32 format.
	*/
	if (psRegA->uType == USEASM_REGTYPE_FPCONSTANT &&
		psRegA->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
	{
		if (HasF16F32SelectInst(psNextInst))
		{
			SetSrc(psState, 
				   psNextInst, 
				   uArg, 
				   USEASM_REGTYPE_FPCONSTANT, 
				   EURASIA_USE_SPECIAL_CONSTANT_FLOAT1, 
				   UF_REGFORMAT_F32); 
			SetComponentSelect(psState, psNextInst, uArg, 0 /* uComponent */);
			return;
		}
	}
	if (psRegA->uType == USEASM_REGTYPE_FPCONSTANT && HasF16F32SelectInst(psNextInst) && bBIsF32 && !bAIsF32)
	{
		ASSERT(psRegA->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO);

		SetSrc(psState, 
			   psNextInst, 
			   uArg, 
			   USEASM_REGTYPE_FPCONSTANT, 
			   EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
			   UF_REGFORMAT_F32); 
		SetComponentSelect(psState, psNextInst, uArg, 0 /* uComponent */);
		return;
	}

	/*
		Check if we can simplify an instruction because we are substituting a
		value whose format is irrelevant.
	*/
	if (psRegA->uType == USEASM_REGTYPE_FPCONSTANT &&
		psRegA->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
		psNextInst->eOpcode == IUNPCKF32F16)
	{
		ASSERT(uArg == 0);

		SetOpcode(psState, psNextInst, IMOV);
		SetSrc(psState, 
			   psNextInst, 
			   uArg, 
			   USEASM_REGTYPE_FPCONSTANT, 
			   EURASIA_USE_SPECIAL_CONSTANT_ZERO, 
			   UF_REGFORMAT_F32); 
		return;
	}

	/*
		Check for needing an extra format conversion after
		a scalar instruction.
	*/
	if (
			(g_psInstDesc[psNextInst->eOpcode].uFlags & DESC_FLAGS_COMPLEXOP) != 0 &&
			GetBit(psNextInst->auFlag, INST_TYPE_PRESERVE)
	   )
	{
		PINST	psPackInst;

		ASSERT(uArg == 0);

		if (bAIsF32)
		{
			IMG_UINT32	uConversionTemp = GetNextRegister(psState);

			psPackInst = AllocateInst(psState, psNextInst->psNext);

			SetOpcode(psState, psPackInst, IPCKF16F32);
			MoveDest(psState, psPackInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
			psPackInst->auDestMask[0] = psNextInst->auLiveChansInDest[0];
			SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uConversionTemp, UF_REGFORMAT_F32);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			InsertInstBefore(psState, psBlock, psPackInst, psNextInst->psNext);

			SetDest(psState, psNextInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uConversionTemp, UF_REGFORMAT_F32);
			SetBit(psNextInst->auFlag, INST_TYPE_PRESERVE, 0);
			psNextInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;
		}
		else if (uAComponent != GetComponentSelect(psState, psNextInst, uArg))
		{
			IMG_UINT32	uConversionTemp = GetNextRegister(psState);

			psPackInst = AllocateInst(psState, psNextInst->psNext);

			SetOpcode(psState, psPackInst, IPCKF16F16);
			MoveDest(psState, psPackInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
			psPackInst->auDestMask[0] = psNextInst->auLiveChansInDest[0];
			SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uConversionTemp, UF_REGFORMAT_F16);
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uAComponent);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			InsertInstBefore(psState, psBlock, psPackInst, psNextInst->psNext);

			SetDest(psState, psNextInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uConversionTemp, UF_REGFORMAT_F16);
			psNextInst->auLiveChansInDest[0] = 0x3 << uAComponent;
		}
	}

	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) == 0)
	{
		/*
			No component select on F16 ALU instructions. We must be accessing the right
			component by virtue of the destination mask.
		*/
		if ((TestInstructionGroup(psNextInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST)) && !bAIsF32)
		{
			ASSERT((psNextInst->auLiveChansInDest[0] == 3 && uAComponent == 0) || (psNextInst->auLiveChansInDest[0] == 12 && uAComponent == 2) || AllF16ChannelsTheSame(psRegA));
			uAComponent = 0;
		}
	}
	else
	{
		if (TestInstructionGroup(psNextInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST))
		{
			PFARITH16_PARAMS	psNextInstParams = psNextInst->u.psArith16;

			if (bAIsF32)
			{
				psNextInstParams->aeSwizzle[uArg] = FMAD16_SWIZZLE_CVTFROMF32;

				if (psNextInst->asArg[uArg].uType == USEASM_REGTYPE_FPCONSTANT &&
					psNextInst->asArg[uArg].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
				{
					psNextInst->asArg[uArg].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
					psNextInstParams->aeSwizzle[uArg] = FMAD16_SWIZZLE_LOWHIGH;
				}
			}
			else
			{
				if (uAComponent == 0)
				{
					psNextInstParams->aeSwizzle[uArg] = FMAD16_SWIZZLE_LOWLOW; 
				}
				else
				{
					ASSERT(uAComponent == 2);
					psNextInstParams->aeSwizzle[uArg] = FMAD16_SWIZZLE_HIGHHIGH;
				}
			}
			uAComponent = 0;
		}
	}

	/*
		Replace the register.
	*/
	SetSrcFromArg(psState, psNextInst, uArg, psRegA);
	ASSERT((bAIsF32 && psRegA->eFmt == UF_REGFORMAT_F32) || (!bAIsF32 && psRegA->eFmt == UF_REGFORMAT_F16));
	if (uAComponent != uBComponent)
	{
		SetComponentSelect(psState, psNextInst, uArg, uAComponent);
	}

	/*
		Transfer source modifiers onto the destination instruction.
	*/
	if (psSourceModifier != NULL && (psSourceModifier->bNegate || psSourceModifier->bAbsolute))
	{
		if (psNextInst->eOpcode == IPCKF16F16 ||
			psNextInst->eOpcode == IPCKF16F32)
		{
			/*
				Only add the source modifier instruction after the pack
				when replacing the last argument.
			*/
			if (uArg == 1 || psNextInst->auDestMask[0] != USC_DESTMASK_FULL)
			{
				PINST		psSModInst;
				IMG_UINT32	uSModTemp = GetNextRegister(psState);

				ASSERT((psNextInst->auLiveChansInDest[0] & ~psNextInst->auDestMask[0]) == 0);

				psSModInst = AllocateInst(psState, psNextInst);

				SetOpcode(psState, psSModInst, IFMOV16);
				MoveDest(psState, psSModInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
				psSModInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
				SetSrc(psState, psSModInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSModTemp, UF_REGFORMAT_F16);
				psSModInst->u.psArith16->sFloat.asSrcMod[0] = *psSourceModifier;
				InsertInstBefore(psState, psBlock, psSModInst, psNextInst->psNext);
	
				SetDest(psState, psNextInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSModTemp, UF_REGFORMAT_F16);
			}
		}
		else
		{
			CombineInstSourceModifiers(psState, psNextInst, uArg, psSourceModifier);
		}
	}
}

typedef struct _F16_SUBST_CONTEXT
{
	PARG					psRegA;
	IMG_UINT32				uAComponent;
	IMG_BOOL				bAIsF32;
	IMG_BOOL				bAIsC10;
	IMG_BOOL				bBIsF32;
	PFLOAT_SOURCE_MODIFIER	psSourceModifier;
	PARG					psRegB;
	IMG_UINT32				uBComponent;
	IMG_UINT32				uBMask;
	IMG_PBOOL				pbNewMoves;
	PWEAK_INST_LIST			psEvalList;
} F16_SUBST_CONTEXT, *PF16_SUBST_CONTEXT;

static IMG_BOOL SubstInF16Inst(PINTERMEDIATE_STATE	psState, 
							   PINST				psNextInst, 
							   PSOURCE_VECTOR		psReplaceArgMask, 
							   IMG_PVOID			pvContext, 
							   IMG_BOOL				bCheckOnly)
{
	IMG_UINT32			uArg;
	PF16_SUBST_CONTEXT	psContext = (PF16_SUBST_CONTEXT)pvContext;

	ASSERT(!bCheckOnly);

	if (psContext->psEvalList != NULL)
	{
		WeakInstListAppend(psState, psContext->psEvalList, psNextInst);
	}

	for (uArg = 0; uArg < psNextInst->uArgumentCount; uArg++)
	{
		if (CheckSourceVector(psReplaceArgMask, uArg))
		{
			SubstituteF16Argument(psState,
								  psNextInst->psBlock,
								  psNextInst,
								  uArg,
								  psContext->psRegA,
								  psContext->bAIsF32,
								  psContext->bAIsC10,
								  psContext->bBIsF32,
								  psContext->psSourceModifier,
								  psContext->uAComponent,
								  psContext->uBComponent,
								  psContext->pbNewMoves);
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL CanSubstInF16Inst(PINTERMEDIATE_STATE	psState, 
								  PINST					psNextInst, 
								  PSOURCE_VECTOR		psReplaceArgMask,
								  IMG_PVOID				pvContext, 
								  IMG_BOOL				bCheckOnly)
{
	IMG_UINT32			uArg;
	PF16_SUBST_CONTEXT	psContext = (PF16_SUBST_CONTEXT)pvContext;
	IMG_BOOL			bReplacingAllArgs;

	ASSERT(bCheckOnly);

	bReplacingAllArgs = IMG_TRUE;
	for (uArg = 0; uArg < psNextInst->uArgumentCount; uArg++)
	{
		IMG_UINT32 uLiveChansInArg;
		
		uLiveChansInArg = GetLiveChansInArg(psState, psNextInst, uArg);

		/*
			Check for an argument unused by the instruction.
		*/
		if (!CheckSourceVector(psReplaceArgMask, uArg) && uLiveChansInArg != 0)
		{
			bReplacingAllArgs = IMG_FALSE;
		}
	}

	/*
		Check restrictions on substituting a format conversion or
		swizzle.
	*/
	return CanSubstituteF16FormatConvert(psState,
										 psNextInst,
										 psContext->psRegA,
										 psContext->uAComponent,
										 psContext->bAIsF32,
										 psContext->bAIsC10,
										 psContext->uBComponent,
										 psContext->bBIsF32,
										 psContext->psSourceModifier,
										 psReplaceArgMask,
										 bReplacingAllArgs);
}

static IMG_BOOL CanCombineF16Registers(PINTERMEDIATE_STATE		psState,
									   PARG						psRegA,
									   IMG_UINT32				uAComponent,
									   IMG_BOOL					bAIsF32,
									   IMG_BOOL					bAIsC10,
									   IMG_BOOL					bBIsF32,
									   PFLOAT_SOURCE_MODIFIER	psSourceModifier,
									   PARG						psRegB,
									   IMG_UINT32				uBComponent)
/*****************************************************************************
 FUNCTION	: CanCombineF16Registers
    
 PURPOSE	: Checks if it is possible to replace a half of a register with
			  another register.

 PARAMETERS	: psState			- Compiler state.
			  psRegA			- Register to replace with.
			  uAComponent		- Component to replace with.
			  psRegB			- Register to be replaced.
			  uBComponent		- Component to be replaced.
			  
 RETURNS	: TRUE if combining is possible.
*****************************************************************************/
{
	F16_SUBST_CONTEXT	sContext;

	sContext.psRegA = psRegA;
	sContext.uAComponent = uAComponent;
	sContext.bAIsF32 = bAIsF32;
	sContext.bAIsC10 = bAIsC10;
	sContext.bBIsF32 = bBIsF32;
	sContext.psSourceModifier = psSourceModifier;
	sContext.psRegB = psRegB;
	sContext.uBComponent = uBComponent;
	sContext.uBMask = bBIsF32 ? 0xf : (0x3 << uBComponent);
	sContext.psEvalList = NULL;;

	return DoReplaceAllUsesMasked(psState,
								  psRegB,
								  sContext.uBMask,
								  IMG_NULL, 
								  0,
								  IMG_FALSE,
								  IMG_FALSE,
								  CanSubstInF16Inst,
								  &sContext,
								  IMG_TRUE /* bCheckOnly */);
}

static IMG_VOID CombineF16Registers(PINTERMEDIATE_STATE		psState,
									PARG					psRegA,
									IMG_UINT32				uAComponent,
									IMG_BOOL				bAIsF32,
									IMG_BOOL				bAIsC10,
									IMG_BOOL				bBIsF32,
									PFLOAT_SOURCE_MODIFIER	psSourceModifier,
									PARG					psRegB,
									IMG_UINT32				uBComponent,
									IMG_PBOOL				pbNewMoves,
									PWEAK_INST_LIST			psEvalList)
/*****************************************************************************
 FUNCTION	: CombineF16Registers
    
 PURPOSE	: Replace uses of one register half with another.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to do replacement after.
			  psRegA			- Register to replace with.
			  uRegAComponent	- Component to be replaced.
			  uAComponent		- Component to replace with.
			  psRegB			- Register to be replaced.
			  uBComponent		- Component to be replaced.
			  psLastUse			- Reference to the argument where the register
								half is last used.
			  psEvalList		- 
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	F16_SUBST_CONTEXT	sContext;
	IMG_BOOL			bRet;

	sContext.psRegA = psRegA;
	sContext.uAComponent = uAComponent;
	sContext.bAIsF32 = bAIsF32;
	sContext.bAIsC10 = bAIsC10;
	sContext.bBIsF32 = bBIsF32;
	sContext.psSourceModifier = psSourceModifier;
	sContext.psRegB = psRegB;
	sContext.uBComponent = uBComponent;
	sContext.pbNewMoves = pbNewMoves;
	sContext.uBMask = bBIsF32 ? 0xf : (0x3 << uBComponent);
	sContext.psEvalList = psEvalList;

	bRet = DoReplaceAllUsesMasked(psState,
								  psRegB,
								  sContext.uBMask,
								  IMG_NULL, 
								  0,
								  IMG_FALSE,
								  IMG_FALSE,
								  SubstInF16Inst,
								  &sContext,
								  IMG_FALSE /* bCheckOnly */);
	ASSERT(bRet);
}

static IMG_UINT32 F16SwizzleToComponent(PINTERMEDIATE_STATE psState,
										FMAD16_SWIZZLE eSwizzle, 
										IMG_UINT32 uHalf)
/*****************************************************************************
 FUNCTION	: F16SwizzleToComponent
    
 PURPOSE	: Get the component accessed by an FMAD16 swizzle on a register half.

 PARAMETERS	: psState	- Internal compiler state
			  eSwizzle	- The FMAD16 swizzle.
			  uHalf		- The register half.

 RETURNS	: The component..
*****************************************************************************/
{
	IMG_UINT32 uCompIdx = 0;

	switch (eSwizzle)
	{
		case FMAD16_SWIZZLE_LOWHIGH:
		{
			uCompIdx = (uHalf == 0) ? 0 : 2;
			break;
		}
		case FMAD16_SWIZZLE_LOWLOW:
		{
			uCompIdx = 0;
			break;
		}
		case FMAD16_SWIZZLE_HIGHHIGH:
		{
			uCompIdx = 2;
			break;
		}
		default: imgabort();
	}

	return uCompIdx;
}


static IMG_BOOL OptimizeDependentF16Instructions(PINTERMEDIATE_STATE	psState,
												 PINST					psInst, 
												 PINST*					ppsNextInst,
												 IMG_PBOOL				pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: OptimizeDependentF16Instructions
    
 PURPOSE	: Try to optimize a pair of dependent F16 instructions.

 PARAMETERS	: psInst					- The first instruction in the pair.
			  ppsNextInst				- Returns the second instruction in the pair.
			  pbMovesGenerated			- Set to TRUE if new moves were generated.

 RETURNS	: TRUE if the second instruction can be dropped.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	PINST		psNextInst;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseLocation;

	ASSERT(psInst->uDestCount == 1);

	/*
		Check if the destination of this instruction is used only as the source for unwritten
		channels in a masked write.
	*/
	if (!UseDefGetSingleUse(psState, &psInst->asDest[0], &psNextInst, &eUseType, &uUseLocation))
	{
		return IMG_FALSE;
	}
	if (eUseType != USE_TYPE_OLDDEST)
	{
		return IMG_FALSE;
	}
	if (psNextInst->psBlock != psInst->psBlock)
	{
		return IMG_FALSE;
	}
	if (!EqualPredicates(psNextInst, psInst))
	{
		return IMG_FALSE;
	}
	if (psNextInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}
	ASSERT(uUseLocation == 0);
	*ppsNextInst = psNextInst;
	
	/*
		Merge a pair of compatible F16 arithmetic instruction writing the same register.
	*/
	if ((psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 12) &&
		(psNextInst->auDestMask[0] == 3 || psNextInst->auDestMask[0] == 12) &&
		psInst->auDestMask[0] != psNextInst->auDestMask[0] &&
		(psInst->eOpcode == IFADD16 || psInst->eOpcode == IFMUL16 || psInst->eOpcode == IFMAD16) &&
		psInst->eOpcode == psNextInst->eOpcode)
	{
		IMG_UINT32	uMatchingArgs;
		IMG_UINT32	uPackCount;
		IMG_BOOL	bInstIsLow = (psInst->auDestMask[0] == 3) ? IMG_TRUE : IMG_FALSE;
		IMG_BOOL	bFormatMismatch = IMG_FALSE;

		/*
			Check how many arguments are shared between the two instructions.
		*/
		uMatchingArgs = 0;
		uPackCount = 0;
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			FMAD16_SWIZZLE	eInstSwz = psInst->u.psArith16->aeSwizzle[uArg];
			FMAD16_SWIZZLE	eNextInstSwz = psNextInst->u.psArith16->aeSwizzle[uArg];
			PARG			psInstSrc = &psInst->asArg[uArg];
			PARG			psNextInstSrc = &psNextInst->asArg[uArg];

			if (
					psNextInstSrc->uType == psInstSrc->uType &&
					psNextInstSrc->uNumber == psInstSrc->uNumber &&
					psNextInstSrc->uArrayOffset == psInstSrc->uArrayOffset &&
					psNextInstSrc->uIndexType == psInstSrc->uIndexType &&
					psNextInstSrc->uIndexNumber == psInstSrc->uIndexNumber &&
					psNextInstSrc->uIndexStrideInBytes == psInstSrc->uIndexStrideInBytes &&
					/*
						Check for needing a HIGHLOW swizzle which we don't support.
					*/
					!(
						bInstIsLow &&
						eInstSwz == FMAD16_SWIZZLE_HIGHHIGH &&
						eNextInstSwz == FMAD16_SWIZZLE_LOWLOW
					 ) &&
					!(
						!bInstIsLow &&
						eInstSwz == FMAD16_SWIZZLE_LOWLOW &&
						eNextInstSwz == FMAD16_SWIZZLE_HIGHHIGH
					 ) &&
					/*
						Make sure the modifiers match too
					*/
					IsNegated(psState, psNextInst, uArg) == IsNegated(psState, psInst, uArg) &&
					IsAbsolute(psState, psNextInst, uArg) == IsAbsolute(psState, psInst, uArg)
			   )
			{
				uMatchingArgs |= (1U << uArg);
			}
			/*
				Check for different source formats, or use of modifiers, which means we 
				couldn't pack the arguments in a single instruction.
			*/
			else if (
						(eInstSwz == FMAD16_SWIZZLE_CVTFROMF32 &&
						 eNextInstSwz != FMAD16_SWIZZLE_CVTFROMF32) ||
						(eInstSwz != FMAD16_SWIZZLE_CVTFROMF32 &&
						 eNextInstSwz == FMAD16_SWIZZLE_CVTFROMF32) ||
						/*
							Check for modifiers (which aren't supported by pack/unpack inst)		
						*/
						HasNegateOrAbsoluteModifier(psState, psInst, uArg) ||
						HasNegateOrAbsoluteModifier(psState, psNextInst, uArg)
					)
			{
				bFormatMismatch = IMG_TRUE;
			}
			else
			{
				uPackCount++;
			}
		}

		if (uPackCount <= 1 && !bFormatMismatch)
		{
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				FMAD16_SWIZZLE	eInstSwz = psInst->u.psArith16->aeSwizzle[uArg];
				FMAD16_SWIZZLE	eNextInstSwz = psNextInst->u.psArith16->aeSwizzle[uArg];

				if ((uMatchingArgs & (1U << uArg)) == 0)
				{
					PINST			psPackInst;
					FMAD16_SWIZZLE	ePackSwiz0;
					FMAD16_SWIZZLE	ePackSwiz1;
					ARG				sPackResult;

					/*
						Generate a new PCK instruction to generate a source for the new
						arithmetic instruction from the register halves used by each of the
						old instructions.
					*/
					psPackInst = AllocateInst(psState, psInst);

					MakeNewTempArg(psState, UF_REGFORMAT_F16, &sPackResult);

					SetOpcode(psState, psPackInst, INVALID_MAXARG);
					SetDestFromArg(psState, psPackInst, 0 /* uDestIdx */, &sPackResult);
					if (psInst->auDestMask[0] == 3)
					{
						CopySrc(psState, psPackInst, 0, psInst, uArg);
						CopySrc(psState, psPackInst, 1, psNextInst, uArg);

						ePackSwiz0 = eInstSwz;
						ePackSwiz1 = eNextInstSwz;
					}
					else
					{
						CopySrc(psState, psPackInst, 1, psInst, uArg);
						CopySrc(psState, psPackInst, 0, psNextInst, uArg);

						ePackSwiz1 = eInstSwz;
						ePackSwiz0 = eNextInstSwz;
					}
					if (psInst->u.psArith16->aeSwizzle[uArg] == FMAD16_SWIZZLE_CVTFROMF32)
					{
						SetOpcode(psState, psPackInst, IPCKF16F32);
					}
					else
					{
						SetOpcode(psState, psPackInst, IPCKF16F16);
						SetComponentSelect(psState, 
										    psPackInst, 
											0 /* uArgIdx */, 
											F16SwizzleToComponent(psState, ePackSwiz0, 0));
						SetComponentSelect(psState, 
										   psPackInst, 
										   1 /* uArgIdx */, 
										   F16SwizzleToComponent(psState, ePackSwiz1, 1));
					}

					InsertInstBefore(psState, psNextInst->psBlock, psPackInst, psNextInst);

					/*
						Replace the old source by the result of the PCK.
					*/
					SetSrcFromArg(psState, psInst, uArg, &sPackResult);
					psInst->u.psArith16->aeSwizzle[uArg] = FMAD16_SWIZZLE_LOWHIGH;
				}
				else
				{
					if (eInstSwz != eNextInstSwz)
					{
						FMAD16_SWIZZLE	eLowSwiz, eHighSwiz;
						FMAD16_SWIZZLE	eCombinedSwiz;
							
						if (bInstIsLow)
						{
							eLowSwiz = eInstSwz;
							eHighSwiz = eNextInstSwz;
						}
						else
						{
							eLowSwiz = eNextInstSwz;
							eHighSwiz = eInstSwz;
						}

						if (eLowSwiz == FMAD16_SWIZZLE_LOWHIGH)
						{
							eLowSwiz = FMAD16_SWIZZLE_LOWLOW;
						}
						if (eHighSwiz == FMAD16_SWIZZLE_LOWHIGH)
						{
							eHighSwiz = FMAD16_SWIZZLE_HIGHHIGH;
						}
						ASSERT(eLowSwiz != FMAD16_SWIZZLE_CVTFROMF32);
						ASSERT(eHighSwiz != FMAD16_SWIZZLE_CVTFROMF32);
						ASSERT(!(eLowSwiz == FMAD16_SWIZZLE_HIGHHIGH && eHighSwiz == FMAD16_SWIZZLE_LOWLOW));

						if (eLowSwiz == FMAD16_SWIZZLE_LOWLOW && eHighSwiz == FMAD16_SWIZZLE_HIGHHIGH)
						{
							eCombinedSwiz = FMAD16_SWIZZLE_LOWHIGH;
						}
						else if (eLowSwiz == FMAD16_SWIZZLE_LOWLOW && eHighSwiz == FMAD16_SWIZZLE_LOWLOW)
						{
							eCombinedSwiz = FMAD16_SWIZZLE_LOWLOW;
						}
						else if (eLowSwiz == FMAD16_SWIZZLE_HIGHHIGH && eHighSwiz == FMAD16_SWIZZLE_HIGHHIGH)
						{
							eCombinedSwiz = FMAD16_SWIZZLE_HIGHHIGH;
						}
						else
						{
							imgabort();
						}
						psInst->u.psArith16->aeSwizzle[uArg] = eCombinedSwiz;
					}
				}
			}

			/*
				Update the new instruction to write the whole register.
			*/
			MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

			return IMG_TRUE;
		}
	}

	/*
		Check for compatible packs of F16 values.
	*/
	if ((psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 12) &&
		(psNextInst->auDestMask[0] == 3 || psNextInst->auDestMask[0] == 12) &&
		psInst->auDestMask[0] != psNextInst->auDestMask[0] &&
		psNextInst->eOpcode == psInst->eOpcode &&
		(psNextInst->eOpcode == IPCKF16F16 || psNextInst->eOpcode == IPCKF16F32))
	{
		/*
			Combine the arguments.
		*/
		if (psInst->auDestMask[0] == 3)
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}
		else
		{
			MovePackSource(psState, psInst, 1 /* uDestArgIdx */, psInst, 0 /* uSrcArgIdx */);
			MovePackSource(psState, psInst, 0 /* uDestArgIdx */, psNextInst, 0 /* uSrcArgIdx */);
		}
	
		/*
			Combine the destinations.
		*/
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] |= psNextInst->auDestMask[0];
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];
		
		/*
			If the arguments to the pack are the halves of the same register then
			convert to a MOV.
		*/
		if (
				psInst->eOpcode == IPCKF16F16 &&
				psInst->asArg[0].uType == psInst->asArg[1].uType &&
				psInst->asArg[0].uNumber == psInst->asArg[1].uNumber &&
				psInst->asArg[0].uArrayOffset == psInst->asArg[1].uArrayOffset &&
				psInst->asArg[0].uIndexType == psInst->asArg[1].uIndexType &&
				psInst->asArg[0].uIndexNumber == psInst->asArg[1].uIndexNumber &&
				psInst->asArg[0].uIndexStrideInBytes == psInst->asArg[1].uIndexStrideInBytes &&
				(
					(
						psInst->u.psPck->auComponent[0] == 0 &&
						psInst->u.psPck->auComponent[1] == 2
					) ||
					AllF16ChannelsTheSame(&psInst->asArg[0])
				)
			)
		{
			SetOpcode(psState, psInst, IMOV);
	
			*pbMovesGenerated = IMG_TRUE;
		}
		if (
				psInst->eOpcode == IPCKF16F32 &&
				psInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT &&
				psInst->asArg[0].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
				psInst->asArg[1].uType == USEASM_REGTYPE_FPCONSTANT &&
				psInst->asArg[1].uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO
		   )
		{
			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0].eFmt = UF_REGFORMAT_F16;
	
			*pbMovesGenerated = IMG_TRUE;
		}
		return IMG_TRUE;
	}
	if (psInst->eOpcode == IMOV &&
		psNextInst->eOpcode == IPCKF16F16 &&
		(psNextInst->auDestMask[0] == 3 || psNextInst->auDestMask[0] == 12))
	{
		SetOpcode(psState, psInst, IPCKF16F16);
		if (psNextInst->auDestMask[0] == 3)
		{
			MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromSrcIdx */);
			MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psNextInst, 0 /* uMoveFromSrcIdx */);
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */));
		}
		else
		{
			MoveSrc(psState, psInst, 1 /* uMoveToSrcIdx */, psNextInst, 0 /* uMoveFromSrcIdx */);
			SetPCKComponent(psState, psInst, 1 /* uArgIdx */, GetPCKComponent(psState, psNextInst, 0 /* uArgIdx */));
		}

		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psNextInst, 0/* uMoveFromDestIdx */);
		psInst->auLiveChansInDest[0] = psNextInst->auLiveChansInDest[0];

		/*
			If the arguments to the pack are the halves of the same register then
			convert to a MOV.
		*/
		if (
				psInst->asArg[0].uType == psInst->asArg[1].uType &&
				psInst->asArg[0].uNumber == psInst->asArg[1].uNumber &&
				psInst->asArg[0].uArrayOffset == psInst->asArg[1].uArrayOffset &&
				psInst->asArg[0].uIndexType == psInst->asArg[1].uIndexType &&
				psInst->asArg[0].uIndexNumber == psInst->asArg[1].uIndexNumber &&
				psInst->asArg[0].uIndexStrideInBytes == psInst->asArg[1].uIndexStrideInBytes &&
				(
					GetPCKComponent(psState, psInst, 1 /* uArgIdx */) == 2 ||
					AllF16ChannelsTheSame(&psInst->asArg[1])
				)
		   )	
		{
			SetOpcode(psState, psInst, IMOV);

			*pbMovesGenerated = IMG_TRUE;
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID CompressF16Masks(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: CompressF16Masks
    
 PURPOSE	: Modify the program as through F16 arithmetic instruction supported masks.

 PARAMETERS	: psState		- Compiler state.
              psBlock		- The block to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst, psNextInst;

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;

		if (
				psInst->eOpcode == IPCKF16F16 &&
				NoPredicate(psState, psInst) &&
				(
					psInst->auDestMask[0] == 3 || 
					psInst->auDestMask[0] == 12
				) &&
				/* Check that we aren't swizzling the result as well. */
				!(
					psInst->auDestMask[0] == 3 && 
					GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 2
				) &&
				!(
					psInst->auDestMask[0] == 12 && 
					GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 0
				) &&
				psInst->psPrev != NULL &&
				TestInstructionGroup(psInst->psPrev->eOpcode, USC_OPT_GROUP_ARITH_F16_INST) &&
				NoPredicate(psState, psInst->psPrev) &&
				UseDefIsSingleSourceUse(psState, psInst, 0 /* uSrcIdx */, &psInst->psPrev->asDest[0])
			)
		{
			MoveDest(psState, psInst->psPrev, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			CopyPartiallyWrittenDest(psState, psInst->psPrev, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			psInst->psPrev->auDestMask[0] = psInst->auDestMask[0];
			psInst->psPrev->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

			RemoveInst(psState, psBlock, psInst);
			FreeInst(psState, psInst);
		}
	}
}

static IMG_VOID ExpandF16Masks(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: ExpandF16Masks
    
 PURPOSE	: Undo the changes done by CompressF16Masks.

 PARAMETERS	: psState		- Compiler state.
              psBlock		- The block to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		if ((TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST)) &&
			psInst->auDestMask[0] != USC_DESTMASK_FULL)
		{
			PINST		psPackInst;
			IMG_UINT32	uMaskedDestTempNum;

			uMaskedDestTempNum = GetNextRegister(psState);

			psPackInst = AllocateInst(psState, psInst);

			SetOpcode(psState, psPackInst, IPCKF16F16);

			MoveDest(psState, psPackInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			CopyPartiallyWrittenDest(psState, psPackInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			psPackInst->auDestMask[0] = psInst->auDestMask[0];
			psPackInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

			SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uMaskedDestTempNum, UF_REGFORMAT_F16);
			if (psInst->auDestMask[0] == 12)
			{
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, 2);
			}
			else
			{
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, 0);
			}

			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			psPackInst->asArg[1].eFmt = UF_REGFORMAT_F16;

			SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uMaskedDestTempNum, UF_REGFORMAT_F16);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = GetLiveChansInArg(psState, psPackInst, 0 /* uArg */);
			SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartialDest */);

			InsertInstBefore(psState, psBlock, psPackInst, psInst->psNext);
		}
	}
}

static IMG_VOID NormaliseF32Source(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: NormaliseF32Source
    
 PURPOSE	: After replacing a source to an instruction with per-source F16/F32
			  select normalise it to remove conversions not supported by the
			  hardware.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  uSrc				- Source which was modified.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psSrc;

	psSrc = &psInst->asArg[uSrc];
	if (psSrc->uType == USEASM_REGTYPE_FPCONSTANT)
	{
		if (psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
		{
			SetSrc(psState, 
				   psInst,
				   uSrc,
				   USEASM_REGTYPE_FPCONSTANT,
				   EURASIA_USE_SPECIAL_CONSTANT_FLOAT1,
				   UF_REGFORMAT_F32);
		}
		else
		{
			ASSERT(psSrc->uNumber == EURASIA_USE_SPECIAL_CONSTANT_ZERO);
			SetSrc(psState, 
				   psInst,
				   uSrc,
				   USEASM_REGTYPE_FPCONSTANT,
				   EURASIA_USE_SPECIAL_CONSTANT_ZERO,
				   UF_REGFORMAT_F32);
		}
		SetComponentSelect(psState, psInst, uSrc /* uArgIdx */, 0 /* uComponent */);
	}
}

static IMG_BOOL OptimizeMultipleUseDependentInstruction(PINTERMEDIATE_STATE psState, 
														PINST psInst, 
														PINST psNextInst, 
														IMG_UINT32 uLocation)
/*****************************************************************************
 FUNCTION	: OptimizeMultipleUseDependentInstruction
    
 PURPOSE	: Apply optimizations to the F16 inst which is used many times

 PARAMETERS	: psInst, psNextInst		- The inst to optimize.
			  uLocation					- Argument index of the use

 RETURNS	: If any optimisations were performed
*****************************************************************************/
{
	if (psInst->eOpcode == IPCKF16F16)
	{
		if (psNextInst->eOpcode == IPCKF16F16)
		{
			if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
				psInst->asArg[1].uNumber == 0)
			{
				if (psNextInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
					psNextInst->asArg[1].uNumber == 0 &&
					(psNextInst->auDestMask[0] == 3 || psNextInst->auDestMask[0] == 12))
				{
					if (GetComponentSelect(psState, psNextInst, 0) == 2 &&
						psInst->auDestMask[0] == 12)
					{
						CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						SetPCKComponent(psState, psNextInst, 0, GetPCKComponent(psState, psInst, 0));
						return IMG_TRUE;
					}
					else if (GetComponentSelect(psState, psNextInst, 0) == 0 &&
						psInst->auDestMask[0] == 3)
					{
						CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						SetPCKComponent(psState, psNextInst, 0, GetPCKComponent(psState, psInst, 0));
						return IMG_TRUE;
					}
				}
				else
				{
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					SetPCKComponent(psState, psNextInst, uLocation, GetPCKComponent(psState, psInst, 0));
					return IMG_TRUE;
				}
			}
		}
		else if (psNextInst->eOpcode == IFMUL || psNextInst->eOpcode == IFADD || psNextInst->eOpcode == IFMAD)
		{
			if (GetComponentSelect(psState, psNextInst, uLocation) == 0)
			{
				if (psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 15)
				{
					/* reading from channel 0 so use the first pack argument */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, GetComponentSelect(psState, psInst, 0));
					NormaliseF32Source(psState, psNextInst, uLocation);
					return IMG_TRUE;
				}
			}
			else
			{
				ASSERT(GetComponentSelect(psState, psNextInst, uLocation) == 2);
				if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
					psInst->asArg[1].uNumber == 0)
				{
					if (psInst->auDestMask[0] == 12)
					{
						CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, GetComponentSelect(psState, psInst, 0));
						NormaliseF32Source(psState, psNextInst, uLocation);
						return IMG_TRUE;
					}
				}
				else
				{
					/* pack must have 2 full arguments so take the second one */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
					SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, GetComponentSelect(psState, psInst, 1));
					NormaliseF32Source(psState, psNextInst, uLocation);
					return IMG_TRUE;
				}
			}
		}
	}
	else if (psInst->eOpcode == IPCKF16F32)
	{
		if (psNextInst->eOpcode == IFMUL || psNextInst->eOpcode == IFADD || psNextInst->eOpcode == IFMAD)
		{
			if (GetComponentSelect(psState, psNextInst, uLocation) == 0)
			{
				if (psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 15)
				{
					/* reading from channel 0 so use the first pack argument */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					return IMG_TRUE;
				}
			}
			else
			{
				ASSERT(GetComponentSelect(psState, psNextInst, uLocation) == 2);
				if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
					psInst->asArg[1].uNumber == 0)
				{
					if (psInst->auDestMask[0] == 12)
					{
						CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, 0 /* uComponent */);
						return IMG_TRUE;
					}
				}
				else
				{
					/* pack must have 2 full arguments so take the second one */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
					SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, 0 /* uComponent */);
					return IMG_TRUE;
				}
			}
		}
	}
	else if (psInst->eOpcode == IUNPCKF16U8)
	{
		if (psNextInst->eOpcode == IPCKU8F16)
		{
			if (GetComponentSelect(psState, psNextInst, uLocation) == 0)
			{
				if (psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 15)
				{
					/* reading from channel 0 so use the first pack argument */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					ModifyOpcode(psState, psNextInst, IPCKU8U8);
					psNextInst->u.psPck->bScale = IMG_FALSE;
					return IMG_TRUE;
				}
			}
			else
			{
				ASSERT(GetComponentSelect(psState, psNextInst, uLocation) == 2);
				if (psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
					psInst->asArg[1].uNumber == 0)
				{
					if (psInst->auDestMask[0] == 12)
					{
						CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						SetComponentSelect(psState, psNextInst, uLocation /* uArgIdx */, 0 /* uComponent */);
						ModifyOpcode(psState, psNextInst, IPCKU8U8);
						psNextInst->u.psPck->bScale = IMG_FALSE;
						return IMG_TRUE;
					}
				}
				else
				{
					/* pack must have 2 full arguments so take the second one */
					CopySrc(psState, psNextInst, uLocation /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
					SetPCKComponent(psState, psNextInst, uLocation, GetPCKComponent(psState, psInst, 1));
					ModifyOpcode(psState, psNextInst, IPCKU8U8);
					psNextInst->u.psPck->bScale = IMG_FALSE;
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL OptimizeMultipleUseDependentInstructions(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: OptimizeMultipleUseDependentInstructions
    
 PURPOSE	: Apply optimizations to the F16 inst which is used many times

 PARAMETERS	: psInst		- The inst to optimize.

 RETURNS	: If any optimisations were performed
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;
	IMG_BOOL bChanges = IMG_FALSE;

	if (psInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}

	psUseDef = UseDefGet(psState, psInst->asDest[0].uType, psInst->asDest[0].uNumber);
	if (psUseDef == NULL)
	{
		return IMG_FALSE;
	}

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF		psUse;
		PINST		psUseInst;

		psNextListEntry = psListEntry->psNext;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psUseDef->psDef)
		{
			continue;
		}
		if (psUse->eType != USE_TYPE_SRC)
		{
			continue;
		}
		psUseInst = psUse->u.psInst;
		bChanges |= OptimizeMultipleUseDependentInstruction(psState, psInst, psUseInst, psUse->uLocation);
	}

	return bChanges;
}

static IMG_BOOL Float16Optimize(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbMovesGenerated)
/*****************************************************************************
 FUNCTION	: Float16Optimize
    
 PURPOSE	: Apply optimizations to the F16 instructions in a block.

 PARAMETERS	: psBlock		- The block to optimize.

 RETURNS	: If new move instructions were introduced in the optimization step.
*****************************************************************************/
{
	IMG_BOOL	bChanges = IMG_FALSE;
	PINST		psInst;
	PINST		psNextInst;

	if (!TestBlockForInstGroup(psBlock, USC_OPT_GROUP_DEP_F16_OPTIMIZATION))
	{
		return IMG_FALSE;
	}

	if(TestBlockForInstGroup(psBlock, USC_OPT_GROUP_ARITH_F16_INST))
	{
		CompressF16Masks(psState, psBlock);
	}

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
	{
		PINST	psCombinedInst;

		psNextInst = psInst->psNext;

		if (!NoPredicate(psState, psInst))
		{
			continue;
		}
		if (!TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_DEP_F16_OPTIMIZATION))
		{
			continue;
		}
		if (IsInstReferencingNonSSAData(psState, psInst))
		{
			continue;
		}

		if (OptimizeDependentF16Instructions(psState, psInst, &psCombinedInst, pbMovesGenerated))
		{
			if (psNextInst == psCombinedInst)
			{
				psNextInst = psNextInst->psNext;
			}

			RemoveInst(psState, psBlock, psInst);
			InsertInstAfter(psState, psBlock, psInst, psCombinedInst);

			RemoveInst(psState, psBlock, psCombinedInst);
			FreeInst(psState, psCombinedInst);

			bChanges = IMG_TRUE;
			continue;
		}
		if (OptimizeMultipleUseDependentInstructions(psState, psInst))
		{
			bChanges = IMG_TRUE;
			continue;
		}
	}

	if(TestBlockForInstGroup(psBlock, USC_OPT_GROUP_ARITH_F16_INST))
	{
		ExpandF16Masks(psState, psBlock);
	}

	return bChanges;
}

typedef struct
{
	IMG_BOOL	bSrcIsF32;
	IMG_UINT32	uSrcComponent;
} F16_FULL_PACK_CONTEXT;

static IMG_BOOL GlobalF16FullPackReplaceArguments(PINTERMEDIATE_STATE			psState,
												  PCODEBLOCK					psCodeBlock,
												  PINST							psInst,
												  PARGUMENT_REFS				psArgRefs,
												  PARG							psRegA,
												  PARG							psRegB,
												  IMG_PVOID						pvContext,
												  IMG_BOOL						bCheckOnly)
/*****************************************************************************
 FUNCTION	: GlobalF16FullPackReplaceArguments
    
 PURPOSE	: Check if replacing the destination of a PCKF16F16/PCKF16F32 with identical
			  sources and writing a whole register by the source is possible in an instruction.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psCodeBlock		- The block containing the instruction.
			  psInst			- The instruction whose arguments are to be replaced.
			  psArgRefs			- References in the instruction to the argument to be
								replaced.
			  psRegA			- Source to the move.
			  pvContext			- Move specific context.
			  bCheckOnly		- If TRUE just check if replacement is possible.

 RETURNS	: Nothing.
*****************************************************************************/
{
	F16_FULL_PACK_CONTEXT*	psCtx = (F16_FULL_PACK_CONTEXT*)pvContext;
	IMG_UINT32				uArg;

	PVR_UNREFERENCED_PARAMETER(psRegA);
	PVR_UNREFERENCED_PARAMETER(psRegB);
	PVR_UNREFERENCED_PARAMETER(psCodeBlock);

	ASSERT(!psArgRefs->bUsedAsIndex);

	if (psArgRefs->bUsedAsPartialDest)
	{
		return IMG_FALSE;
	}

	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (CheckSourceVector(&psArgRefs->sSourceVector, uArg))
		{
			if (!CanUseSrc(psState, psInst, uArg, psRegA->uType, psRegA->uIndexType))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
		}
	}


	if (psCtx->bSrcIsF32)
	{
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_FARITH16)
		{
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) == 0)
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			if (!bCheckOnly)
			{
				for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
				{
					if (CheckSourceVector(&psArgRefs->sSourceVector, uArg))
					{
						psInst->u.psArith16->aeSwizzle[uArg] = FMAD16_SWIZZLE_CVTFROMF32;
						if (psInst->asArg[uArg].uType == USEASM_REGTYPE_FPCONSTANT &&
							psInst->asArg[uArg].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
						{
							psInst->asArg[uArg].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
							psInst->u.psArith16->aeSwizzle[uArg] = FMAD16_SWIZZLE_LOWHIGH;
						}
					}
				}
			}
			return IMG_TRUE;
		}
		else if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_COMPLEXOP)
		{
			ASSERT(CheckSourceVector(&psArgRefs->sSourceVector, COMPLEXOP_SOURCE));
			if (GetBit(psInst->auFlag, INST_TYPE_PRESERVE))
			{
				ASSERT(bCheckOnly);
				return IMG_FALSE;
			}
			if (!bCheckOnly)
			{
				SetComponentSelect(psState, psInst, 0 /* uArgIdx */, 0 /* uComponent */);
			}
			return IMG_TRUE;
		}
		else if (HasF16F32SelectInst(psInst))
		{
			/*
				Change to use the same component of the source as the move.
			*/
			if (!bCheckOnly)
			{
				for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
				{
					if (CheckSourceVector(&psArgRefs->sSourceVector, uArg))
					{
						SetComponentSelect(psState, psInst, uArg, 0 /* uComponent */);
					}
				}
			}
			return IMG_TRUE;
		}
		else
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	else
	{
		switch (psInst->eOpcode)
		{
			case IFMOV16:
			case IFMUL16:
			case IFADD16:
			case IFMAD16:
			{		
				if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) == 0)
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
				if (!bCheckOnly)
				{
					FMAD16_SWIZZLE	eF16Swizzle;

					if (psCtx->uSrcComponent == 0)
					{
						eF16Swizzle = FMAD16_SWIZZLE_LOWLOW;
					}
					else
					{
						eF16Swizzle = FMAD16_SWIZZLE_HIGHHIGH;
					}

					for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
					{
						if (CheckSourceVector(&psArgRefs->sSourceVector, uArg))
						{	
							psInst->u.psArith16->aeSwizzle[uArg] = eF16Swizzle;
						}
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
}

static IMG_BOOL FindUnpacksOfResult(PINTERMEDIATE_STATE psState, 
									PINST psFMad16Inst, 
									IMG_PUINT32 puUnpackMask, 
									PINST apsUnpacks[])
/*****************************************************************************
 FUNCTION	: FindUnpacksOfResult
    
 PURPOSE	: Looks for unpacks F16->F32 on the result of an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psFMad16Inst		- Instruction whose result is to be checked.
			  puUnpackMask		- Returns the mask of register halves which are unpacked.
			  apsUnpacks		- Returns pointers to the unpack instructions for each half.
			  
 RETURNS	: TRUE if 1 or 2 unpack instructions were found with no other uses
			  of the result.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psFMad16Result;
	PUSC_LIST_ENTRY	psListEntry;

	*puUnpackMask = 0;
	apsUnpacks[0] = apsUnpacks[1] = NULL;

	psFMad16Result = UseDefGet(psState, psFMad16Inst->asDest[0].uType, psFMad16Inst->asDest[0].uNumber);
	if (psFMad16Result == NULL)
	{
		return IMG_FALSE;
	}

	for (psListEntry = psFMad16Result->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF		psUse;
		PINST		psPackInst;
		IMG_UINT32	uArg;
		IMG_UINT32	uHalf;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psFMad16Result->psDef)
		{
			continue;
		}
		if (psUse->eType != USE_TYPE_SRC)
		{
			return IMG_FALSE;
		}
		psPackInst = psUse->u.psInst;
		uArg = psUse->uLocation;
		if (psPackInst->psBlock != psFMad16Inst->psBlock)
		{
			return IMG_FALSE;
		}
		if (psPackInst->eOpcode != IUNPCKF32F16)
		{
			return IMG_FALSE;
		}
		uHalf = (GetPCKComponent(psState, psPackInst, uArg) == 2) ? 1 : 0;
		if ((*puUnpackMask) & (1 << uHalf))
		{
			return IMG_FALSE;
		}
		(*puUnpackMask) |= (1 << uHalf);
		apsUnpacks[uHalf] = psPackInst;
	}
	return IMG_TRUE;
}

static PINST PromoteF16InstToF32(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uHalf)
/*****************************************************************************
 FUNCTION	: PromoteF16InstToF32
    
 PURPOSE	: Returns whether the opcode is FMAD16, FMUL16, FADD16 or not

 PARAMETERS	: psState			- Compiler state
			  psInst			- F16 inst
			  uHalf				- 0 means read F16 channel 0, non-zero is chan2
			  
 RETURNS	: The new F32 inst
*****************************************************************************/
{
	PINST		psNewInst;
	IMG_UINT32	uUnpackedTemp;
	IMG_UINT32	uArg;

	psNewInst = AllocateInst(psState, psInst);

	/* Get the corresponding F32 opcode. */
	switch (psInst->eOpcode)
	{
		case IFMOV16: 
			SetOpcode(psState, psNewInst, IFMOV); 
			break;
		case IFMUL16: 
			SetOpcode(psState, psNewInst, IFMUL); 
			break;
		case IFMAD16: 
			SetOpcode(psState, psNewInst, IFMAD); 
			break;
		case IFADD16: 
			SetOpcode(psState, psNewInst, IFADD); 
			break;
		default: imgabort();
	}

	uUnpackedTemp = GetNextRegister(psState);

	SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uUnpackedTemp, UF_REGFORMAT_F32);

	CopyPredicate(psState, psNewInst, psInst);

	/*
		Copy the arguments.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		PARG			psNewArg = &psNewInst->asArg[uArg];
		FMAD16_SWIZZLE	eOldSwiz = psInst->u.psArith16->aeSwizzle[uArg];

		CopySrc(psState, psNewInst, uArg, psInst, uArg);
		psNewInst->u.psFloat->asSrcMod[uArg] = psInst->u.psArith16->sFloat.asSrcMod[uArg];
		if (eOldSwiz != FMAD16_SWIZZLE_CVTFROMF32)
		{
			IMG_UINT32	uNewComponent;

			/*
				Map the FMAD16 swizzle to a component select.
			*/
			uNewComponent = F16SwizzleToComponent(psState, eOldSwiz, uHalf);
			SetComponentSelect(psState, psNewInst, uArg, uNewComponent);

			/*
				Replace F16 hardware constants by the equivalent F32 constant.
			*/
			if (psNewArg->uType == USEASM_REGTYPE_FPCONSTANT)
			{
				if (psNewArg->uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
				{
					psNewArg->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				}
				psNewArg->eFmt = UF_REGFORMAT_F32;
				SetComponentSelect(psState, psNewInst, uArg, 0 /* uComponent */);
			}
		}
	}

	/*
		Promote a FMOV which doesn't modify its argument to MOV.
	*/
	if (psNewInst->eOpcode == IFMOV &&
		psNewInst->asArg[0].eFmt == UF_REGFORMAT_F32 &&
		!HasNegateOrAbsoluteModifier(psState, psNewInst, 0))
	{
		SetOpcode(psState, psNewInst, IMOV);
	}

	return psNewInst;
}

//#define DO_ADDITIONAL_F16TOF32_CONVERSION

#ifdef DO_ADDITIONAL_F16TOF32_CONVERSION
static IMG_BOOL IsPromotableF16ArithInst(IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: IsPromotableF16ArithInst
    
 PURPOSE	: Returns whether the opcode is FMAD16, FMUL16, FADD16 or not

 PARAMETERS	: eOpcode			- Opcode to check
			  
 RETURNS	: TRUE if opcode is FMAD16, FMUL16, FADD16
*****************************************************************************/
{
	switch (eOpcode)
	{
		case IFADD16:
		case IFMUL16:
		case IFMAD16:
		case IFMOV16:
			return IMG_TRUE;
		default:
			return IMG_FALSE;
	}
}
#endif /* DO_ADDITIONAL_F16TOF32_CONVERSION */

#ifdef DO_ADDITIONAL_F16TOF32_CONVERSION
/* this should be moved to usedef.c once committed and tested properly */
IMG_INTERNAL
IMG_BOOL UseDefGetInstUseCount(PINTERMEDIATE_STATE psState,
									PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefGetInstUseCount
    
 PURPOSE	: Counts uses of this usedef when read by instructions.

 PARAMETERS	: psState		- Compiler state.
			  psDest		- Argument to check.

 RETURNS	: TRUE if the register is used only once.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psUseListEntry;
	PVREGISTER psRegister = psDest->psRegister;
	IMG_UINT32 uCount = 0;

	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(psDest != NULL);

	if (psRegister == NULL)
	{
		return IMG_UNDEF;
	}

	psUseDef = psRegister->psUseDefChain;
	if (!(psUseDef->uType == USEASM_REGTYPE_TEMP || psUseDef->uType == USEASM_REGTYPE_PREDICATE))
	{
		return IMG_UNDEF;
	}

	for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
	{	
		PUSEDEF	psUseOrDef;

		psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		if (psUseOrDef == psUseDef->psDef)
		{
			continue;
		}

		if (!(psUseOrDef->eType >= USE_TYPE_FIRSTINSTUSE && psUseOrDef->eType <= USE_TYPE_LASTINSTUSE))
		{
			return IMG_UNDEF;
		}

		uCount++;
	}

	return uCount;
}
#endif /* DO_ADDITIONAL_F16TOF32_CONVERSION */

IMG_INTERNAL
IMG_BOOL PromoteFARITH16ToF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: PromoteFARITH16ToF32
    
 PURPOSE	: Find FMAD16 instructions which could be changed to FMADs to remove
			  unpacks F16->F32 on the result.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to change.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;
	IMG_BOOL	bChanges = IMG_FALSE;
	PINST		psNextInst;

	if (psBlock->psOwner->psFunc != psState->psMainProg)
	{
			return IMG_FALSE;
	}

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		if (TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST) && NoPredicate(psState, psInst))
		{
			PINST apsUnpacks[2];
			IMG_UINT32 uUnpackMask;

			if (FindUnpacksOfResult(psState, psInst, &uUnpackMask, apsUnpacks))
			{
				PINST		psNewInst;
				IMG_UINT32	uHalf;
				PINST		psInsertPoint = psInst->psNext;

				for (uHalf = 0; uHalf < 2; uHalf++)
				{
					if ((uUnpackMask & (1 << uHalf)) == 0)
					{
						continue;
					}

					psNewInst = PromoteF16InstToF32(psState, psInst, uHalf);

					/*
						Insert the new F32 instruction.
					*/
					InsertInstBefore(psState, psBlock, psNewInst, psInsertPoint);
					psInsertPoint = psNewInst->psNext;

					/*
						Replace the unpack by a move.
					*/
					SetOpcode(psState, apsUnpacks[uHalf], IMOV);
					SetSrcFromArg(psState, apsUnpacks[uHalf], 0 /* uSrc */, &psNewInst->asDest[0]);

					bChanges = IMG_TRUE;
				}

				/*
					Remove the FMAD16 instruction.
				*/
				RemoveInst(psState, psBlock, psInst);
				FreeInst(psState, psInst);
			}
#ifdef DO_ADDITIONAL_F16TOF32_CONVERSION
			else if (psInst->auLiveChansInDest[0] == 3 || psInst->auLiveChansInDest[0] == 12)
			{
				/* check for the instructions which read from these instructions.
				if all can take the result as 32-bit instead of 16 then promote */
				PARG psDest = &psInst->asDest[0];
				IMG_UINT32 uNumUses = UseDefGetInstUseCount(psState, psDest);
				if (uNumUses >= 1)
				{
					PUSEDEF_CHAIN	psUseDef;
					PUSC_LIST_ENTRY	psUseListEntry;
					PUSC_LIST_ENTRY	psUseNextListEntry;
					PVREGISTER		psRegister = psDest->psRegister;
					IMG_BOOL		bAllOk = IMG_TRUE;
					PINST			psNewInst;
					IMG_UINT32		uHalf = psInst->auLiveChansInDest[0] == 3 ? 0 : 1;

					psUseDef = psRegister->psUseDefChain;

					for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
					{
						PUSEDEF	psUseOrDef;

						psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

						if (psUseOrDef == psUseDef->psDef)
						{
							continue;
						}
						else
						{
							PINST psUseInst = UseDefGetInst(psUseOrDef);
							ASSERT(psUseInst != NULL);

							switch (psUseInst->eOpcode)
							{
								case IFADD:
								case IFMUL:
								case IFMAD:
									break;
								case IUNPCKF32F16:
									break;
								case IPCKF16F16:
								{
									if (psUseOrDef->uLocation == 0)
									{
										if (psUseInst->asArg[1].uType != USEASM_REGTYPE_FPCONSTANT)
										{
											bAllOk = IMG_FALSE;
										}
									}
									else
									{
										ASSERT(psUseOrDef->uLocation == 1);
										if (psUseInst->asArg[0].uType != USEASM_REGTYPE_FPCONSTANT)
										{
											bAllOk = IMG_FALSE;
										}
									}
									break;
								}
								case IPCKU8F16:
								{
									if (psUseOrDef->uLocation == 0)
									{
										if (psUseInst->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE || 
											psUseInst->asArg[1].uNumber != 0)
										{
											bAllOk = IMG_FALSE;
										}
									}
									else
									{
										bAllOk = IMG_FALSE;
									}
									break;
								}
								case IFSUBFLR:
									break;
								default:
									bAllOk = IMG_FALSE;
									break;
							}
							if (!bAllOk)
							{
								break;
							}
						}
					}
					if (!bAllOk)
					{
						continue;
					}

					psNewInst = PromoteF16InstToF32(psState, psInst, uHalf);
					/*
						Insert the new F32 instruction.
					*/
					InsertInstAfter(psState, psInst->psBlock, psNewInst, psInst);
					/*
						Remove the FMAD16 instruction.
					*/
					RemoveInst(psState, psInst->psBlock, psInst);
					FreeInst(psState, psInst);

					for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseNextListEntry)
					{
						PUSEDEF	psUseOrDef;

						psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

						psUseNextListEntry = psUseListEntry->psNext;

						if (psUseOrDef == psUseDef->psDef)
						{
							continue;
						}
						else
						{
							PINST psUseInst = UseDefGetInst(psUseOrDef);
							ASSERT(psUseInst != NULL);

							switch (psUseInst->eOpcode)
							{
								case IFADD:
								case IFMUL:
								case IFMAD:
								{
									SetSrcFromArg(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, &psNewInst->asDest[0]);
									SetComponentSelect(psState, psUseInst, psUseOrDef->uLocation /* uSrcIdx */, 0);
									break;
								}
								case IPCKF16F16:
								{
									SetOpcode(psState, psUseInst, IPCKF16F32);

									SetSrcFromArg(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, &psNewInst->asDest[0]);
									SetComponentSelect(psState, psUseInst, psUseOrDef->uLocation /* uSrcIdx */, 0);
									if (psUseOrDef->uLocation == 0)
									{
										ASSERT(psUseInst->asArg[1].uType == USEASM_REGTYPE_FPCONSTANT);
										psUseInst->asArg[1].eFmt = UF_REGFORMAT_F32;
									}
									else
									{
										ASSERT(psUseInst->asArg[0].uType == USEASM_REGTYPE_FPCONSTANT);
										psUseInst->asArg[0].eFmt = UF_REGFORMAT_F32;
									}
									break;
								}
								case IUNPCKF32F16:
								{
									ASSERT(psUseOrDef->uLocation == 0);
									SetOpcode(psState, psUseInst, IMOV);
									SetSrcFromArg(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, &psNewInst->asDest[0]);
									break;
								}
								case IPCKU8F16:
								{
									ASSERT(psUseOrDef->uLocation == 0);
									ASSERT(psUseInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE);
									ASSERT(psUseInst->asArg[1].uNumber == 0);

									ModifyOpcode(psState, psUseInst, IPCKU8F32);
									SetSrcFromArg(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, &psNewInst->asDest[0]);
									SetComponentSelect(psState, psUseInst, psUseOrDef->uLocation /* uSrcIdx */, 0);
									break;
								}
								case IFSUBFLR:
								{
									SetSrcFromArg(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, &psNewInst->asDest[0]);
									SetComponentSelect(psState, psUseInst, psUseOrDef->uLocation /* uSrcIdx */, 0);
									break;
								}
								default:
									imgabort();
							}
						}
					}
				}
			}
#endif /* DO_ADDITIONAL_F16TOF32_CONVERSION */
		}
#ifdef DO_ADDITIONAL_F16TOF32_CONVERSION
		else if (psInst->eOpcode == IPCKU8F16 || 
				psInst->eOpcode == IPCKF16F16 || 
				(
					psInst->eOpcode == IFSUBFLR &&
					psInst->asArg[0].eFmt == UF_REGFORMAT_F16 &&
					psInst->asArg[1].eFmt == UF_REGFORMAT_F16
				))
		{
			IMG_UINT32 uSource0DestIdx;
			PINST psSource0Inst = UseDefGetDefInst(psState, psInst->asArg[0].uType, psInst->asArg[0].uNumber, &uSource0DestIdx);
			IMG_UINT32 uSource1DestIdx;
			PINST psSource1Inst = UseDefGetDefInst(psState, psInst->asArg[1].uType, psInst->asArg[1].uNumber, &uSource1DestIdx);

			if (psSource0Inst && 
				uSource0DestIdx == 0 &&
				psSource1Inst &&
				uSource1DestIdx == 0 &&
				IsPromotableF16ArithInst(psSource0Inst->eOpcode) &&
				(
					(
						psSource0Inst == psSource1Inst && 
						UseDefGetInstUseCount(psState, &psSource0Inst->asDest[0]) == 2 &&
						GetComponentSelect(psState, psInst, 0) == GetComponentSelect(psState, psInst, 1)
					)
					||
					(
						UseDefGetSingleSourceUse(psState, psInst, &psSource0Inst->asDest[0]) != USC_UNDEF &&
						UseDefGetSingleSourceUse(psState, psInst, &psSource1Inst->asDest[0]) != USC_UNDEF &&
						IsPromotableF16ArithInst(psSource1Inst->eOpcode)
					)
				)
				)

			{
				/* packing the only uses of F16 arith instructions so make them F32 instead */
				PINST			psNewInst0;
				IMG_UINT32		uHalf0 = psSource0Inst->auLiveChansInDest[0] == 3 ? 0 : 1;

				psNewInst0 = PromoteF16InstToF32(psState, psSource0Inst, uHalf0);
				/*
					Insert the new F32 instruction.
				*/
				InsertInstAfter(psState, psSource0Inst->psBlock, psNewInst0, psSource0Inst);
				/*
					Remove the FMAD16 instruction.
				*/
				RemoveInst(psState, psSource0Inst->psBlock, psSource0Inst);
				FreeInst(psState, psSource0Inst);

				if (psSource0Inst != psSource1Inst)
				{
					PINST			psNewInst1;
					IMG_UINT32		uHalf1 = psSource1Inst->auLiveChansInDest[0] == 3 ? 0 : 1;

					psNewInst1 = PromoteF16InstToF32(psState, psSource1Inst, uHalf1);
					/*
						Insert the new F32 instruction.
					*/
					InsertInstAfter(psState, psSource1Inst->psBlock, psNewInst1, psSource1Inst);
					/*
						Remove the FMAD16 instruction.
					*/
					RemoveInst(psState, psSource1Inst->psBlock, psSource1Inst);
					FreeInst(psState, psSource1Inst);

					SetSrcFromArg(psState, psInst, /* uSrcIdx */ 1, &psNewInst1->asDest[0]);
				}
				else
				{
					SetSrcFromArg(psState, psInst, /* uSrcIdx */ 1, &psNewInst0->asDest[0]);
				}

				SetSrcFromArg(psState, psInst, /* uSrcIdx */ 0, &psNewInst0->asDest[0]);

				switch (psInst->eOpcode)
				{
					case IPCKU8F16:
						ModifyOpcode(psState, psInst, IPCKU8F32);
						break;
					case IPCKF16F16:
						ModifyOpcode(psState, psInst, IPCKF16F32);
						break;
					case IFSUBFLR:
						break;
					default:
						imgabort();
				}

				SetComponentSelect(psState, psInst, 0 /* uSrcIdx */, 0);
				SetComponentSelect(psState, psInst, 1 /* uSrcIdx */, 0);

				bChanges = IMG_TRUE;
			}
		}
		else if (psInst->eOpcode == IFADD || 
				psInst->eOpcode == IFMUL || 
				psInst->eOpcode == IFMAD || 
				psInst->eOpcode == IFMIN || 
				psInst->eOpcode == IFMAX)
		{
			IMG_UINT32 uArgIdx;
			for (uArgIdx = 0; uArgIdx < psInst->uArgumentCount; uArgIdx++)
			{
				IMG_UINT32 uSourceDestIdx;
				PINST psSourceInst = UseDefGetDefInst(psState, psInst->asArg[uArgIdx].uType, psInst->asArg[uArgIdx].uNumber, &uSourceDestIdx);

				if (psSourceInst && 
					uSourceDestIdx == 0 &&
					UseDefGetSingleSourceUse(psState, psInst, &psSourceInst->asDest[0]) != USC_UNDEF &&
					IsPromotableF16ArithInst(psSourceInst->eOpcode))
				{
					/* F32 inst is the only use of an F16 inst.  so promote the F16 inst */
					PINST			psNewInst;
					IMG_UINT32		uHalf = psSourceInst->auLiveChansInDest[0] == 3 ? 0 : 1;

					psNewInst = PromoteF16InstToF32(psState, psSourceInst, uHalf);
					/*
						Insert the new F32 instruction.
					*/
					InsertInstAfter(psState, psSourceInst->psBlock, psNewInst, psSourceInst);
					/*
						Remove the FMAD16 instruction.
					*/
					RemoveInst(psState, psSourceInst->psBlock, psSourceInst);
					FreeInst(psState, psSourceInst);

					SetSrcFromArg(psState, psInst, /* uSrcIdx */ uArgIdx, &psNewInst->asDest[0]);
					SetComponentSelect(psState, psInst, uArgIdx /* uSrcIdx */, 0);

					bChanges = IMG_TRUE;
				}
				else if (psSourceInst && 
					uSourceDestIdx == 0 &&
					psSourceInst->eOpcode == IMOV &&
					psSourceInst->asDest[0].eFmt == UF_REGFORMAT_F16 &&
					UseDefGetSingleSourceUse(psState, psInst, &psSourceInst->asDest[0]) != USC_UNDEF &&
					!NoPredicate(psState, psSourceInst) &&
					psSourceInst->apsOldDest[0] &&
					(
						psSourceInst->auLiveChansInDest[0] == 3 ||
						psSourceInst->auLiveChansInDest[0] == 12
					))
				{
					IMG_UINT32 uOldSourceDestIdx;
					PINST psOldSourceInst = UseDefGetDefInst(psState, psSourceInst->apsOldDest[0]->uType, psSourceInst->apsOldDest[0]->uNumber, &uOldSourceDestIdx);
					
					PINST psUseInst;
					USEDEF_TYPE eUseType;
					IMG_UINT32 uUseSrcIdx;

					if (psOldSourceInst && 
						uOldSourceDestIdx == 0 &&
						psOldSourceInst->eOpcode == IMOV &&
						psOldSourceInst->asDest[0].eFmt == UF_REGFORMAT_F16 &&
						UseDefGetSingleUse(psState, &psOldSourceInst->asDest[0], &psUseInst, &eUseType, &uUseSrcIdx) &&
						NoPredicate(psState, psOldSourceInst) &&
						psSourceInst->auLiveChansInDest[0] == psOldSourceInst->auLiveChansInDest[0])
					{
						if (psUseInst == psSourceInst &&
							eUseType == USE_TYPE_OLDDEST &&
							uUseSrcIdx == 0)
						{
							/* reading a predicated mov which itself reads from a mov
							both are 16 bit moves but can really be 32 bit */
							IMG_UINT32 uNewSourceDest = GetNextRegister(psState);
							IMG_UINT32 uNewOldSourceDest = GetNextRegister(psState);

							SetDest(psState, psSourceInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);
							SetDest(psState, psOldSourceInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewOldSourceDest, UF_REGFORMAT_F32);

							SetPartiallyWrittenDest(psState, psSourceInst, 0, &psOldSourceInst->asDest[0]);

							SetOpcode(psState, psSourceInst, IFMOV);
							SetOpcode(psState, psOldSourceInst, IFMOV);

							if (psSourceInst->auLiveChansInDest[0] == 12)
							{
								/* need to read from channel 2 */
								SetComponentSelect(psState, psSourceInst, 0 /* uSrcIdx */, 2);
							}

							if (psOldSourceInst->auLiveChansInDest[0] == 12)
							{
								/* need to read from channel 2 */
								SetComponentSelect(psState, psOldSourceInst, 0 /* uSrcIdx */, 2);
							}

							SetSrc(psState, psInst, /* uSrcIdx */ uArgIdx, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);
							SetComponentSelect(psState, psInst, uArgIdx /* uSrcIdx */, 0);

							bChanges = IMG_TRUE;
						}
					}
				}
				else if (psSourceInst && 
					psSourceInst->eOpcode == IDELTA &&
					(
						psSourceInst->auLiveChansInDest[0] == 3 ||
						psSourceInst->auLiveChansInDest[0] == 12
					) &&
					psSourceInst->uArgumentCount == 2 &&
					psSourceInst->asDest[0].eFmt == UF_REGFORMAT_F16 &&
					UseDefGetSingleSourceUse(psState, psInst, &psSourceInst->asDest[0]) != USC_UNDEF)
				{
					PINST auSourceInsts[2];
					IMG_UINT32 uDeltaSrcIdx;
					IMG_UINT32 uNewSourceDest;

					for (uDeltaSrcIdx = 0; uDeltaSrcIdx < psSourceInst->uArgumentCount; uDeltaSrcIdx++)
					{
						IMG_UINT32 uDeltaSourceDestIdx;
						PINST psDeltaSrcInst;
						
						if (psSourceInst->asArg[uDeltaSrcIdx].uType == USEASM_REGTYPE_FPCONSTANT &&
							psSourceInst->asArg[uDeltaSrcIdx].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE)
						{
							auSourceInsts[uDeltaSrcIdx] = NULL;
							continue;
						}

						psDeltaSrcInst = UseDefGetDefInst(psState, 
														  psSourceInst->asArg[uDeltaSrcIdx].uType, 
														  psSourceInst->asArg[uDeltaSrcIdx].uNumber, 
														  &uDeltaSourceDestIdx);

						if (psDeltaSrcInst &&
							uDeltaSourceDestIdx == 0 &&
							psSourceInst->auLiveChansInDest[0] == psDeltaSrcInst->auLiveChansInDest[0] &&
							(
								(
									IsPromotableF16ArithInst(psDeltaSrcInst->eOpcode) &&
									UseDefGetSingleSourceUse(psState, psSourceInst, &psDeltaSrcInst->asDest[0]) != USC_UNDEF
								)
								||
								(
									psDeltaSrcInst->eOpcode == IPCKF16F32 &&
									psDeltaSrcInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
									psDeltaSrcInst->asArg[1].uNumber == 0
								)
							))
						{
							auSourceInsts[uDeltaSrcIdx] = psDeltaSrcInst;
						}
						else
						{
							psSourceInst = NULL;
							break;
						}
					}
					if (psSourceInst == NULL)
					{
						/* couldn't process this delta inst so continue to the next source */
						continue;
					}

					uNewSourceDest = GetNextRegister(psState);

					/* all sources are either constants or instructions convertable to F32 and only used in this
					delta instructions */
					for (uDeltaSrcIdx = 0; uDeltaSrcIdx < psSourceInst->uArgumentCount; uDeltaSrcIdx++)
					{
						PINST psDeltaSrcInst = auSourceInsts[uDeltaSrcIdx];

						if (psDeltaSrcInst == NULL)
						{
							/* constant.  just remove the format specifier */
							ASSERT(psSourceInst->asArg[uDeltaSrcIdx].uNumber == EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE);
							psSourceInst->asArg[uDeltaSrcIdx].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
							psSourceInst->asArg[uDeltaSrcIdx].eFmt = UF_REGFORMAT_F32;
						}
						else if (psDeltaSrcInst->eOpcode == IPCKF16F32)
						{
							/* reading from a pack which is already converting an F32 so can just
							read directly from the F32 source */
							SetSrcFromArg(psState, psSourceInst, uDeltaSrcIdx /* uSrcIdx */, &psDeltaSrcInst->asArg[0]);
						}
						else
						{
							PINST			psNewInst;
							IMG_UINT32		uHalf = psDeltaSrcInst->auLiveChansInDest[0] == 3 ? 0 : 1;

							psNewInst = PromoteF16InstToF32(psState, psDeltaSrcInst, uHalf);
							/*
								Insert the new F32 instruction.
							*/
							InsertInstAfter(psState, psDeltaSrcInst->psBlock, psNewInst, psDeltaSrcInst);
							/*
								Remove the FMAD16 instruction.
							*/
							RemoveInst(psState, psDeltaSrcInst->psBlock, psDeltaSrcInst);
							FreeInst(psState, psDeltaSrcInst);

							SetSrcFromArg(psState, psSourceInst, /* uSrcIdx */ uDeltaSrcIdx, &psNewInst->asDest[0]);
							SetComponentSelect(psState, psSourceInst, uDeltaSrcIdx /* uSrcIdx */, 0);
						}
					}

					SetDest(psState, psSourceInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);

					psSourceInst->auDestMask[0] = USC_DESTMASK_FULL;
					psSourceInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;

					SetSrc(psState, psInst, /* uSrcIdx */ uArgIdx, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);
					SetComponentSelect(psState, psInst, uArgIdx /* uSrcIdx */, 0);

					bChanges = IMG_TRUE;
				}
				else if (psSourceInst && 
					uSourceDestIdx == 0 &&
					(
						psSourceInst->eOpcode == IFEXP ||
						psSourceInst->eOpcode == IFLOG
					) &&
					psSourceInst->asDest[0].eFmt == UF_REGFORMAT_F16 &&
					UseDefGetSingleSourceUse(psState, psInst, &psSourceInst->asDest[0]) != USC_UNDEF)
				{
					IMG_UINT32 uNewSourceDest;
					IMG_UINT32 uSourceSourceDestIndex;
					PINST psSourceSourceInst = UseDefGetDefInst(psState, 
																psSourceInst->asArg[0].uType, 
																psSourceInst->asArg[0].uNumber, 
																&uSourceSourceDestIndex);

					if (psSourceSourceInst && 
						uSourceSourceDestIndex == 0 &&
						UseDefGetSingleSourceUse(psState, psSourceInst, &psSourceSourceInst->asDest[0]) != USC_UNDEF &&
						IsPromotableF16ArithInst(psSourceSourceInst->eOpcode))
					{
						PINST			psNewInst;
						IMG_UINT32		uHalf = psSourceSourceInst->auLiveChansInDest[0] == 3 ? 0 : 1;

						psNewInst = PromoteF16InstToF32(psState, psSourceSourceInst, uHalf);
						/*
							Insert the new F32 instruction.
						*/
						InsertInstAfter(psState, psSourceSourceInst->psBlock, psNewInst, psSourceSourceInst);
						/*
							Remove the FMAD16 instruction.
						*/
						RemoveInst(psState, psSourceSourceInst->psBlock, psSourceSourceInst);
						FreeInst(psState, psSourceSourceInst);

						SetSrcFromArg(psState, psSourceInst, /* uSrcIdx */ 0, &psNewInst->asDest[0]);
						SetComponentSelect(psState, psSourceInst, 0 /* uSrcIdx */, 0);

						uNewSourceDest = GetNextRegister(psState);

						SetDest(psState, psSourceInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);

						psSourceInst->auDestMask[0] = USC_DESTMASK_FULL;
						psSourceInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;

						SetSrc(psState, psInst, /* uSrcIdx */ uArgIdx, USEASM_REGTYPE_TEMP, uNewSourceDest, UF_REGFORMAT_F32);
						SetComponentSelect(psState, psInst, uArgIdx /* uSrcIdx */, 0);

						bChanges = IMG_TRUE;
					}
				}
			}
		}
		else if (psInst->eOpcode == IFNRMF16)
		{
			/* check for only packs of the result meaning we should really do a 32-bit NRM instead */
			IMG_BOOL bAllOk = IMG_TRUE;
			IMG_UINT32 uDest;
			IMG_UINT32 auNewDests[3];
			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				PARG psDest = &psInst->asDest[uDest];
				IMG_UINT32 uNumUses = UseDefGetInstUseCount(psState, psDest);
				if (uNumUses >= 1)
				{
					PUSEDEF_CHAIN	psUseDef;
					PUSC_LIST_ENTRY	psUseListEntry;
					PVREGISTER psRegister = psDest->psRegister;

					psUseDef = psRegister->psUseDefChain;

					for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
					{
						PUSEDEF	psUseOrDef;

						psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

						if (psUseOrDef == psUseDef->psDef)
						{
							continue;
						}
						else
						{
							PINST psUseInst = UseDefGetInst(psUseOrDef);
							ASSERT(psUseInst != NULL);

							if (psUseInst->eOpcode != IUNPCKF32F16)
							{
								bAllOk = IMG_FALSE;
								break;
							}
						}
					}
					if (!bAllOk)
					{
						break;
					}
				}
			}
			if (!bAllOk)
			{
				continue;
			}

			/* all uses of the dests of this instruction are F16->F32 packs so instead
			put the packs on the sources then the optimizer will continue to optimise those */
			auNewDests[0] = GetNextRegister(psState);
			auNewDests[1] = GetNextRegister(psState);
			auNewDests[2] = GetNextRegister(psState);
			
			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				PUSEDEF_CHAIN	psUseDef;
				PARG psDest = &psInst->asDest[uDest];
				PUSC_LIST_ENTRY	psUseListEntry;
				PUSC_LIST_ENTRY	psUseNextListEntry;
				PVREGISTER psRegister = psDest->psRegister;

				psUseDef = psRegister->psUseDefChain;

				for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseNextListEntry)
				{
					PUSEDEF	psUseOrDef;

					psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

					psUseNextListEntry = psUseListEntry->psNext;

					if (psUseOrDef == psUseDef->psDef)
					{
						continue;
					}
					else
					{
						PINST psUseInst = UseDefGetInst(psUseOrDef);
						IMG_UINT32 uNewDest;
						ASSERT(psUseInst != NULL);

						ASSERT(psUseOrDef->uLocation == 0);
						if (uDest == 0)
						{
							if (GetComponentSelect(psState, psUseInst, psUseOrDef->uLocation) == 0)
							{
								uNewDest = auNewDests[0];
							}
							else
							{
								ASSERT(GetComponentSelect(psState, psUseInst, psUseOrDef->uLocation) == 2);
								uNewDest = auNewDests[1];
							}
						}
						else
						{
							ASSERT(uDest == 1);
							uNewDest = auNewDests[2];
						}
						SetOpcode(psState, psUseInst, IMOV);
						SetSrc(psState, psUseInst, /* uSrcIdx */ psUseOrDef->uLocation, USEASM_REGTYPE_TEMP, uNewDest, UF_REGFORMAT_F32);
					}
				}
			}

			SetOpcodeAndDestCount(psState, psInst, IFNRM, 3); 

			SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, auNewDests[0], UF_REGFORMAT_F32);
			SetDest(psState, psInst, 1 /* uDestIdx */, USEASM_REGTYPE_TEMP, auNewDests[1], UF_REGFORMAT_F32);
			SetDest(psState, psInst, 2 /* uDestIdx */, USEASM_REGTYPE_TEMP, auNewDests[2], UF_REGFORMAT_F32);
			psInst->auDestMask[0] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;
			psInst->auDestMask[1] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[1] = USC_DESTMASK_FULL;
			psInst->auDestMask[2] = USC_DESTMASK_FULL;
			psInst->auLiveChansInDest[2] = USC_DESTMASK_FULL;

			{
				PINST apsNewPacks[3];
				IMG_UINT32 uSrcIdx;
				for (uSrcIdx = 0; uSrcIdx < 3; uSrcIdx++)
				{
					PINST psNewInst = AllocateInst(psState, psInst);
					IMG_UINT32 uNewDest = GetNextRegister(psState);

					SetOpcode(psState, psNewInst, IUNPCKF32F16);
					SetDest(psState, psNewInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewDest, UF_REGFORMAT_F32);
					psNewInst->auLiveChansInDest[0] = USC_DESTMASK_FULL;
					psNewInst->auDestMask[0] = USC_DESTMASK_FULL;
					if (uSrcIdx == 0 || uSrcIdx == 1)
					{
						CopySrc(psState, psNewInst, 0 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
						if (uSrcIdx == 0)
						{
							SetPCKComponent(psState, psNewInst, 0, 0);
						}
						else
						{
							SetPCKComponent(psState, psNewInst, 0, 2);
						}
					}
					else
					{
						CopySrc(psState, psNewInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
						SetPCKComponent(psState, psNewInst, 0, 0);
					}
					InsertInstBefore(psState, psBlock, psNewInst, psInst);

					apsNewPacks[uSrcIdx] = psNewInst;
				}

				for (uSrcIdx = 0; uSrcIdx < 3; uSrcIdx++)
				{
					SetSrcFromArg(psState, psInst, uSrcIdx, &apsNewPacks[uSrcIdx]->asDest[0]);
				}
				return IMG_TRUE;
			}
		}
#endif /* DO_ADDITIONAL_F16TOF32_CONVERSION */
	}
	return bChanges;
}

IMG_INTERNAL
IMG_BOOL ReplaceFMOV16(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbNewMoves, PWEAK_INST_LIST psEvalList)
{
	ASSERT(psInst->eOpcode == IFMOV16);

	if (psInst->asDest[0].uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL ||
		psInst->asArg[0].uType == USC_REGTYPE_REGARRAY)
	{
		return IMG_FALSE;
	}

	if (psInst->auLiveChansInDest[0] == 0x3 || psInst->auLiveChansInDest[0] == 0xc)
	{
		IMG_UINT32	uAComponent = 0;
		IMG_UINT32	uBComponent;
		IMG_BOOL	bAIsF32 = IMG_FALSE;

		switch (psInst->u.psArith16->aeSwizzle[0])
		{
			case FMAD16_SWIZZLE_LOWHIGH:
			{
				uAComponent = (psInst->auLiveChansInDest[0] == 0x3) ? 0 : 2;
				break;
			}
			case FMAD16_SWIZZLE_LOWLOW:	 
			{
				uAComponent = 0;
				break;
			}
			case FMAD16_SWIZZLE_HIGHHIGH:
			{
				uAComponent = 2;
				break;
			}
			case FMAD16_SWIZZLE_CVTFROMF32:
			{
				uAComponent = 0;
				bAIsF32 = IMG_TRUE;
				break;
			}
			default: imgabort();
		}
		uBComponent = (psInst->auLiveChansInDest[0] == 0x3) ? 0 : 2;

		if (CanCombineF16Registers(psState, 
								   &psInst->asArg[0], 
								   uAComponent, 
								   bAIsF32,
								   IMG_FALSE /* bAIsC10 */,
								   IMG_FALSE /* bBIsF32 */,
								   &psInst->u.psArith16->sFloat.asSrcMod[0],
								   &psInst->asDest[0], 
								   uBComponent)) 
		{
			CombineF16Registers(psState, 
								&psInst->asArg[0], 
								uAComponent,
								bAIsF32,
								IMG_FALSE /* bAIsC10 */,
								IMG_FALSE /* bBIsF32 */,
								&psInst->u.psArith16->sFloat.asSrcMod[0],
								&psInst->asDest[0], 
								uBComponent, 
								pbNewMoves,
								psEvalList);
			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);
			return IMG_TRUE;
		}
	}
	else
	{
		if (EliminateGlobalMove(psState, 
								psInst->psBlock, 
								psInst, 
								&psInst->asArg[0], 
								&psInst->asDest[0], 
								psEvalList))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL EliminateF16Move(PINTERMEDIATE_STATE psState, PINST psInst, IMG_PBOOL pbNewMoves)
/*****************************************************************************
 FUNCTION	: EliminateF16Move
    
 PURPOSE	: Try to replace the destination of an instruction swizzling/packing
			  F16 data by the source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if(!(NoPredicate(psState, psInst) && TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_ELIMINATE_F16_MOVES)))
	{
		return IMG_FALSE;
	}
	
	/*
		Check for a move with source modifier we could transfer onto the
		instructions that use the result of the move.
	*/
	if (psInst->eOpcode == IFMOV16)
	{
		return ReplaceFMOV16(psState, psInst, pbNewMoves, NULL /* psEvalList */);
	}
	else
	/*
		Check for a move or format conversion of an f16 value.
	*/
	if ((psInst->eOpcode == IPCKF16F16 || psInst->eOpcode == IPCKF16F32 || psInst->eOpcode == IUNPCKF32F16) && 
		(psInst->auDestMask[0] == 0x3 || psInst->auDestMask[0] == 0xc || psInst->eOpcode == IUNPCKF32F16))
	{
		IMG_BOOL	bAIsF32 = (psInst->eOpcode == IPCKF16F32);
		IMG_BOOL	bBIsF32 = (psInst->eOpcode == IUNPCKF32F16);
		IMG_UINT32	uBComponent;

		/*
			Get the component to replace in the destination register.
		*/
		if (bBIsF32)
		{
			uBComponent = 0;
		}
		else
		{
			uBComponent = (psInst->auDestMask[0] == 0x3) ? 0 : 2;
		}

		if (psInst->asDest[0].uType == USEASM_REGTYPE_TEMP && 
			psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL &&
			psInst->asArg[0].uType != USC_REGTYPE_REGARRAY)
		{
			if (CanCombineF16Registers(psState, 
									   &psInst->asArg[0], 
									   GetPCKComponent(psState, psInst, 0 /* uArgIdx */), 
									   bAIsF32,
									   IMG_FALSE /* bAIsC10 */,
									   bBIsF32,
									   NULL /* psSourceModifier */,
									   &psInst->asDest[0], 
									   uBComponent)) 
			{
				CombineF16Registers(psState, 
									&psInst->asArg[0], 
									GetPCKComponent(psState, psInst, 0 /* uArgIdx */), 
									bAIsF32,
									IMG_FALSE,
									bBIsF32,
									NULL /* psSourceModifier */,
									&psInst->asDest[0], 
									uBComponent, 
									pbNewMoves,
									NULL /* psEvalList */);
				CopyPartiallyOverwrittenData(psState, psInst);
				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);
				return IMG_TRUE;
			}
		}
	}
	else
	if (psInst->eOpcode == IUNPCKF16C10 &&
		psInst->u.psPck->bScale &&

		(psInst->auDestMask[0] == 0x3 || psInst->auDestMask[0] == 0xc))
	{
		IMG_UINT32	uBComponent;

		uBComponent = (psInst->auDestMask[0] == 0x3) ? 0 : 2;

		if (psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL && 
			psInst->asArg[0].uType != USEASM_REGTYPE_FPINTERNAL && 
			psInst->asDest[0].uType != USEASM_REGTYPE_INDEX && 
			psInst->asDest[0].uType != USC_REGTYPE_REGARRAY && 
			psInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX)
		{
			if (CanCombineF16Registers(psState, 
									   &psInst->asArg[0], 
									   GetPCKComponent(psState, psInst, 0 /* uArgIdx */), 
									   IMG_FALSE /* bAIsF32 */,
									   IMG_TRUE /* bAIsC10 */,
									   IMG_FALSE /* bBIsF32 */,
									   NULL /* psSourceModifier */,
									   &psInst->asDest[0], 
									   uBComponent)) 
			{
				CombineF16Registers(psState, 
									&psInst->asArg[0], 
									GetPCKComponent(psState, psInst, 0 /* uArgIdx */), 
									IMG_FALSE,
									IMG_TRUE,
									IMG_FALSE,
									NULL /* psSourceModifier */,
									&psInst->asDest[0], 
									uBComponent, 
									pbNewMoves,
									NULL /* psEvalList */);
				CopyPartiallyOverwrittenData(psState, psInst);
				RemoveInst(psState, psInst->psBlock, psInst);
				FreeInst(psState, psInst);
				return IMG_TRUE;
			}
		}
	}
	else
	if ((psInst->eOpcode == IPCKF16F16 || psInst->eOpcode == IPCKF16F32) &&

		psInst->auDestMask[0] == USC_DESTMASK_FULL &&
		EqualPCKArgs(psState, psInst))
	{
		F16_FULL_PACK_CONTEXT	sCtx;

		if (psInst->eOpcode == IPCKF16F32)
		{
			sCtx.bSrcIsF32 = IMG_TRUE;
			sCtx.uSrcComponent = 0;
		}
		else
		{
			sCtx.bSrcIsF32 = IMG_FALSE;
			sCtx.uSrcComponent = GetPCKComponent(psState, psInst, 0 /* uArgIdx */);
		}

		if (DoGlobalMoveReplace(psState, 
								psInst->psBlock, 
								psInst, 
								&psInst->asArg[0], 
								&psInst->asDest[0], 
								GlobalF16FullPackReplaceArguments, 
								&sCtx))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL 
IMG_VOID EliminateF16MovesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: EliminateF16MovesBP
    
 PURPOSE	: Eliminate moves of F16 registers from a basic block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to eliminate moves from.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst, psNextInst;
	IMG_BOOL	bChanges;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	if (psState->bInvariantShader == IMG_TRUE)
	{
		/* 
			This optimization is not invariance safe as it is can modify F32 to F16 
			in one shader but not in an other shader.
		*/
		return;
	}

	do
	{
		IMG_BOOL	bNewMoves = IMG_FALSE;

		bChanges = IMG_FALSE;

		if(TestBlockForInstGroup(psBlock, USC_OPT_GROUP_ELIMINATE_F16_MOVES))
		{
			for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
			{
				psNextInst = psInst->psNext;
	
				if( TestInstructionGroup( psInst->eOpcode, USC_OPT_GROUP_ELIMINATE_F16_MOVES ) && 
					NoPredicate(psState, psInst))
				{
					if (psInst->eOpcode == IPCKF16F16 &&
	
						ReferencedOutsideBlock(psState, psBlock, &psInst->asDest[0]))
					{
						if (psInst->auDestMask[0] == 3 &&
							GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 0 &&
							psInst->auLiveChansInDest[0] == 3)
						{
							SetOpcode(psState, psInst, IMOV);
							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							bNewMoves = IMG_TRUE;
						}
						if (
								psInst->auDestMask[0] == 12 &&
								(
									GetPCKComponent(psState, psInst, 0 /* uArgIdx */) == 2 ||
									AllF16ChannelsTheSame(&psInst->asArg[0])
								) &&
								psInst->auLiveChansInDest[0] == 12)
						{
							SetOpcode(psState, psInst, IMOV);
							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							bNewMoves = IMG_TRUE;
						}
					}
		
					if (EliminateF16Move(psState, psInst, &bNewMoves))
					{
						bChanges = IMG_TRUE;
					}
				}
			}
		}
		
		if(TestBlockForInstGroup(psBlock, USC_OPT_GROUP_ARITH_F16_INST))
		{
			if (PromoteFARITH16ToF32(psState, psBlock))
			{
				bChanges = IMG_TRUE;
				bNewMoves = IMG_TRUE;
			}
		}
		
		if (Float16Optimize(psState, psBlock, &bNewMoves))
		{
			bChanges = IMG_TRUE;
		}
		
		EliminateMovesBP(psState, psBlock);
	} while (bChanges);
}

FMAD16_SWIZZLE IMG_INTERNAL CombineFMad16Swizzle(PINTERMEDIATE_STATE psState,
											 FMAD16_SWIZZLE eFirstSwiz, 
											 FMAD16_SWIZZLE eSecondSwiz)
/*****************************************************************************
 FUNCTION	: CombineFMad16Swizzle
    
 PURPOSE	: Combine two fmad16 swizzles applied in series into a single swizzle.

 PARAMETERS	: uFirstSwiz, uSecondSwiz		- The swizzles to combine.
			  
 RETURNS	: The resulting swizzle.
*****************************************************************************/
{
	ASSERT(eSecondSwiz != FMAD16_SWIZZLE_CVTFROMF32);
	if (eFirstSwiz == FMAD16_SWIZZLE_LOWHIGH)
	{
		return eSecondSwiz;
	}
	else
	{
		ASSERT(eFirstSwiz == FMAD16_SWIZZLE_LOWLOW || eFirstSwiz == FMAD16_SWIZZLE_HIGHHIGH || eFirstSwiz == FMAD16_SWIZZLE_CVTFROMF32);
		return eFirstSwiz;
	}
}

IMG_BOOL IMG_INTERNAL SubstituteFMad16Swizzle(PINTERMEDIATE_STATE	psState,
											  PINST					psInst,
											  FMAD16_SWIZZLE		eSwizzle,
											  IMG_UINT32			uArg,
											  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: SubstituteFMad16Swizzle
    
 PURPOSE	: Check if its possible to replace a use of the destination of 
			  an FMOV16 with a swizzle on the source by the source.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  eSwizzle			- Swizzle to check.
			  uArg				- Argument number to check.
			  bCheckOnly		- If FALSE replace the swizzle.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_ARITH_F16_INST))
	{
		if (!bCheckOnly)
		{
			psInst->u.psArith16->aeSwizzle[uArg] = 
				CombineFMad16Swizzle(psState,
								     eSwizzle,
									 psInst->u.psArith16->aeSwizzle[uArg]);
		}
		return IMG_TRUE;
	}
	else
	if ((psInst->eOpcode == IPCKU8F16 ||
		 psInst->eOpcode == IPCKC10F16) &&
		eSwizzle != FMAD16_SWIZZLE_CVTFROMF32)
	{
		if (!bCheckOnly)
		{
			IMG_UINT32	uComponent;

			uComponent = GetPCKComponent(psState, psInst, uArg);
			uComponent = F16SwizzleToComponent(psState, eSwizzle, uComponent);
			SetPCKComponent(psState, psInst, uArg, uComponent);
		}
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/* EOF */
