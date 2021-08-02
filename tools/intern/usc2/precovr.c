/******************************************************************************
 * Name         : precovr.c
 * Title        : Optimizations on input (UNIFLEX) code
 * Created      : July 2006
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: precovr.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include <limits.h>

#define UFREG_DMASK_FULL	((1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT) | (1U << UFREG_DMASK_Z_SHIFT) | (1U << UFREG_DMASK_W_SHIFT))

/*
	Structure representing a register in the input program which might
	be replaced by a register containing the same data in U8 format.
*/
typedef struct
{
	/*
		Type of the register to be replaced.
	*/
	UF_REGTYPE		eType;
	/*
		Number of the register to be replaced.
	*/
	IMG_UINT32		uUnpackedNum;
	/*
		Mask of channels in the register to be replaced.
	*/
	IMG_UINT32		uMask;
	/*
		Number of the replacement temporary register .
	*/
	IMG_UINT32		uPackedNum;
	IMG_UINT32		uPrecision;
	/*
		Format of the register to be replaced.
	*/
	UF_REGFORMAT	eFormat;

	/*
		Instruction for which this packing is generated. 
	*/
	#ifdef SRC_DEBUG
	PUNIFLEX_INST	psInst;
	#endif /* SRC_DEBUG */

} PACKED_OUTPUT, *PPACKED_OUTPUT;

#define USC_MAX_PACKEDOUTPUTS				(16)

typedef struct
{
	/*
		Array of the registers which should be converted
		to U8 format.
	*/
	PPACKED_OUTPUT				apsPackedRegisters[USC_MAX_PACKEDOUTPUTS];
	IMG_UINT32					uNumPackedRegisters;
} PRECOVR_STATE, *PPRECOVR_STATE;

#define USC_NUM_PRECOVR_STATES			2

typedef struct
{
	/*
		State used in precovr.c.
	*/
	PPRECOVR_STATE				psPrecovrState;
	IMG_UINT32					uNumPrecovrStates;
} PRECOVR_CONTEXT, *PPRECOVR_CONTEXT;

IMG_INTERNAL
IMG_UINT32 GetSourceLiveChans(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psInst, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: GetSourceLiveChans

 PURPOSE	: Get channels live in a source register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction
			  uSrc				- Source

 RETURNS	: A mask of the live channels.
*****************************************************************************/
{
	IMG_UINT32	uDestLive = 0; /* channels live in [any] destination */
	IMG_UINT32	uLiveChans = 0; /* channels live in (swizzled source) */

	switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
	{
	default: ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests == 0); break;
	case 2: uDestLive = psInst->sDest2.u.byMask;
		//fallthrough to include first dest also
	case 1: uDestLive |= psInst->sDest.u.byMask;
		if (uDestLive == 0)
		{
			return 0x0;
		}
	}

	switch (psInst->eOpCode)
	{
		case UFOP_ADD:
		case UFOP_SUB:
		case UFOP_MUL:
		case UFOP_MAD:
		case UFOP_MOV:
		case UFOP_MOVA_TRC:
		case UFOP_MOVA_RNE:
		case UFOP_MOVA_INT:
		case UFOP_CMP:
		case UFOP_CMPLT:
		case UFOP_CND:
		case UFOP_FRC:
		case UFOP_FLR:
		case UFOP_CEIL:
		case UFOP_TRC:
		case UFOP_RNE:
		case UFOP_RND:
		case UFOP_MIN:
		case UFOP_MAX:
		case UFOP_DSX:
		case UFOP_DSY:
		case UFOP_SLT:
		case UFOP_SGE:
		case UFOP_SGN:
		case UFOP_SETP:
		case UFOP_LRP:
		case UFOP_SETBEQ:
		case UFOP_SETBNE:
		case UFOP_SETBLT:
		case UFOP_SETBGE:
		case UFOP_MOVCBIT:
		case UFOP_AND:
		case UFOP_SHL:
		case UFOP_ASR:
		case UFOP_NOT:
		case UFOP_OR:
		case UFOP_SHR:
		case UFOP_XOR:
		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_EXP:
		case UFOP_LOG:
		case UFOP_POW:
		case UFOP_EXPP:
		case UFOP_LOGP:
		case UFOP_DIV:
		case UFOP_SQRT:
		case UFOP_MUL2:
		case UFOP_DIV2:
		case UFOP_MOV_CP:
		case UFOP_SINCOS2:
		case UFOP_FTOI:
		{
			uLiveChans = uDestLive;
			break;
		}

		case UFOP_DOT2ADD:
		{
			uLiveChans = (uSrc == 2) ? 0x4 : 0x3;
			break;
		}

		case UFOP_DOT3:
		{
			uLiveChans = 0x7;
			break;
		}
		case UFOP_DOT4:
		case UFOP_DOT4_CP:
		{
			uLiveChans = 0xF;
			break;
		}

		case UFOP_SINCOS:
		{
			uLiveChans = 0x8;
			break;
		}

		case UFOP_DST:
		{
			uLiveChans = (uSrc == 0) ? 0x6 : 0xa;
			break;
		}

		case UFOP_LIT:
		case UFOP_OGLLIT:
		{
			uLiveChans = 0xb;
			break;
		}
		
		case UFOP_LD:
		case UFOP_LDB:
		case UFOP_LDL:
		case UFOP_LDP:
		case UFOP_LDPIFTC:
		case UFOP_LDD:
		case UFOP_LDC:
		case UFOP_LDCLZ:
		case UFOP_LDLOD:
		case UFOP_LDGATHER4:
		{
			IMG_UINT32 uSampler = psInst->asSrc[1].uNum;
			IMG_BOOL bProjected = (psInst->eOpCode == UFOP_LDP) ? IMG_TRUE : IMG_FALSE;

			if (
					(
						psInst->eOpCode == UFOP_LDB || 
						psInst->eOpCode == UFOP_LDL || 
						psInst->eOpCode == UFOP_LDC ||
						psInst->eOpCode == UFOP_LDCLZ
					) && 
					uSrc == 2)
			{
				/*
					Just the alpha channel used from the LOD bias/LOD replace/PCF comparison source.
				*/
				uLiveChans = 0x8;
			}
			else
			{
				IMG_UINT32	uDimensionality;

				uDimensionality = GetTextureCoordinateUsedChanCount(psState, uSampler);
				if (psState->psSAOffsets->asTextureDimensionality[uSampler].bIsArray)
				{
					uDimensionality++;
				}
				uLiveChans = (1U << uDimensionality) - 1;

				if (!(psInst->eOpCode == UFOP_LDD && uSrc == 2))
				{
					if (psInst->eOpCode == UFOP_LDPIFTC)
					{
						if (psInst->asSrc[0].eType == UFREG_TYPE_TEXCOORD &&
							GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psInst->asSrc[0].uNum))
						{
							bProjected = IMG_TRUE;
						}
					}
					if (bProjected)
					{
						uLiveChans |= 0x8;
					}
				}
			}
			break;
		}

		case UFOP_KPLT:
		case UFOP_KPLE:
		{
			uLiveChans = 0xF;
			break;
		}

		case UFOP_KILLNZBIT:
		{
			uLiveChans = 1 << UFREG_SWIZ_Z;
			break;
		}

		case UFOP_NRM:
		{
			uLiveChans = 0x7;
			break;
		}

		case UFOP_SPLIT:
		{
			uLiveChans = 0x1;
			break;
		}

		default: imgabort();
	}

	return SwizzleMask(psInst->asSrc[uSrc].u.uSwiz, uLiveChans);
}

/*****************************************************************************
 FUNCTION	: IsConditionalStart

 PURPOSE	: Test whether an instruction starts a conditional block.

 PARAMETERS	: psInst			- The input instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL IsConditionalStart(PUNIFLEX_INST	psInst)
{	
	if (psInst->eOpCode == UFOP_IFC ||
		psInst->eOpCode == UFOP_IFP ||
		psInst->eOpCode == UFOP_IF ||
		psInst->eOpCode == UFOP_IFNZBIT)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static IMG_BOOL IsResultUsed(PINTERMEDIATE_STATE psState, 
							 PUNIFLEX_INST psInst, UF_REGTYPE eType, IMG_UINT32 uNum, IMG_UINT32 uLive, PUNIFLEX_INST psIgnore)
/*****************************************************************************
 FUNCTION	: IsResultUsed

 PURPOSE	: Checks if the result of an instruction is used.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to start the check at.
			  eType, uNum		- Result type and register number.
			  uLive				- Channels live in the result.

 RETURNS	: The register number.
*****************************************************************************/
{
	for (; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32 uSrc;

		/*
			Stop when we reach the end of the main program 
			(we can assume we aren't called for instructions within a function)
		*/
		if (psInst->eOpCode == UFOP_RET)
		{
			return IMG_FALSE;
		}
		else if (psInst->eOpCode == UFOP_ELSE)
		{
			IMG_UINT32	uIfCount;

			/*
				Skip over the ELSE clause.
			*/
			uIfCount = 1;
			for (; ; psInst = psInst->psILink)
			{
				if (IsConditionalStart(psInst))
				{
					uIfCount++;
				}
				if (psInst->eOpCode == UFOP_ENDIF)
				{
					uIfCount--;
					if (uIfCount == 0)
					{
						break;
					}
				}
			}
		}
		else if (psInst->eOpCode == UFOP_ENDIF)
		{
			continue;
		}
		/*
			Otherwise stop when we reach flow control to simplify tracking.
		*/
		else if (IsInputInstFlowControl(psInst))
		{
			return IMG_TRUE;
		}

		/*
			Check for using the register.
		*/
		if (psInst != psIgnore)
		{
			for (uSrc = 0; uSrc < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uSrc++)
			{
				IMG_UINT32 uLiveChans = GetSourceLiveChans(psState, psInst, uSrc);

				if (psInst->asSrc[uSrc].eType == eType &&
					psInst->asSrc[uSrc].uNum == uNum &&
					(uLiveChans & uLive) != 0)
				{
					return IMG_TRUE;
				}
			}
		}

		/*
			Check for unconditionally overwriting the register.
		*/
		if (psInst->uPredicate == UF_PRED_NONE)
		{
			switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
			{
			default: ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests==0); break;
			case 2:
				if (psInst->sDest2.eType == eType && psInst->sDest2.uNum == uNum)
				{
					uLive &= ~(*((IMG_UINT32*)(&psInst->sDest2.u.byMask)));
				}
				//fallthrough
			case 1:
				if (psInst->sDest.eType == eType && psInst->sDest.uNum == uNum)
				{
					uLive &= ~(*((IMG_UINT32*)(&psInst->sDest.u.byMask)));
				}
				if (uLive == 0)
				{
					return IMG_FALSE;
				}
			}
		}
	}
	return IMG_FALSE;

}

static IMG_BOOL IsSaturatedSource(PINTERMEDIATE_STATE psState, 
								  PUF_REGISTER psSrc, 
								  IMG_UINT32 uChanMask, 
								  PUNIFLEX_INST psStartInst,
								  IMG_PBOOL pbLimitedPrecision);

IMG_INTERNAL
IMG_UINT32 SwizzleMask(IMG_UINT32 uSwiz, IMG_UINT32 uMask)
/*****************************************************************************
 FUNCTION	: SwizzleMask

 PURPOSE	: Swizzle a mask of channel.

 PARAMETERS	: uSwiz			- Swizzle to apply.
			  uMask			- The mask to apply it to.

 RETURNS	: The swizzled mask.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uSwizMask;

	uSwizMask = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (uMask & (1U << uChan))
		{
			IMG_UINT32 uSrcChan = (uSwiz >> (uChan * 3)) & 0x7;
			if (uSrcChan <= UFREG_SWIZ_W)
			{
				uSwizMask |= (1U << uSrcChan);
			}
		}
	}

	return uSwizMask;
}

static IMG_UINT32 AddPackedRegister(PINTERMEDIATE_STATE psState, PPRECOVR_STATE psPState, PUF_REGISTER psSrc, IMG_UINT32 uMask, IMG_UINT32 uPrecision, PUNIFLEX_INST psPackInst)
/*****************************************************************************
 FUNCTION	: AddPackedRegister

 PURPOSE	: Add a register to the list of those required in packed format.

 PARAMETERS	: psState			- Compiler state.
			  psSrc				- Register to add.
			  uMask				- Channels live in the register.
			  psPackInst        - Instruction for which packing is done.

 RETURNS	: The register number.
*****************************************************************************/
{
	IMG_UINT32	uReg;
	PPACKED_OUTPUT psPackedReg;

	#if!defined(SRC_DEBUG)
	PVR_UNREFERENCED_PARAMETER(psPackInst);
	#endif /* SRC_DEBUG */

	for (uReg = 0; uReg < psPState->uNumPackedRegisters; uReg++)
	{
		#ifdef SRC_DEBUG
		psPState->apsPackedRegisters[uReg]->psInst = IMG_NULL;
		#endif /* SRC_DEBUG */

		if (psPState->apsPackedRegisters[uReg]->eType == psSrc->eType &&
			psPState->apsPackedRegisters[uReg]->uUnpackedNum == psSrc->uNum &&
			psPState->apsPackedRegisters[uReg]->eFormat == psSrc->eFormat)
		{
			#ifdef SRC_DEBUG
			psPState->apsPackedRegisters[uReg]->psInst = psPackInst;
			#endif /* SRC_DEBUG */

			psPState->apsPackedRegisters[uReg]->uMask |= uMask;
			psPState->apsPackedRegisters[uReg]->uPrecision = max(psPState->apsPackedRegisters[uReg]->uPrecision, uPrecision);
			return psPState->apsPackedRegisters[uReg]->uPackedNum;
		}
	}
	if (psPState->apsPackedRegisters[uReg] == NULL)
		psPState->apsPackedRegisters[uReg] = UscAlloc(psState, sizeof(PACKED_OUTPUT));
	psPackedReg = psPState->apsPackedRegisters[uReg];
	psPackedReg->eType = psSrc->eType;
	psPackedReg->uUnpackedNum = psSrc->uNum;
	psPackedReg->uMask = uMask;
	psPackedReg->uPackedNum = psState->uInputTempRegisterCount++;
	psPackedReg->uPrecision = uPrecision;
	psPackedReg->eFormat = psSrc->eFormat;
	
	#ifdef SRC_DEBUG
	psPackedReg->psInst = psPackInst;
	#endif /* SRC_DEBUG */

	psPState->uNumPackedRegisters++;
	return psPackedReg->uPackedNum;
}

static IMG_BOOL IsIntegerTexture(PINTERMEDIATE_STATE psState, IMG_UINT32 uSampler)
/*****************************************************************************
 FUNCTION	: IsIntegerTexture

 PURPOSE	: Is a texture format 8 bits per channel or less.

 PARAMETERS	: psState			- Compiler state.
			  uSampler			- Sampler to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUNIFLEX_TEXFORM			psTexForm;
	IMG_UINT32					uChan;
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParam = &psState->psSAOffsets->asTextureParameters[uSampler];

	psTexForm = &psTexParam->sFormat;
	if (psTexParam->bGamma)
	{
		return IMG_FALSE;
	}
	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32 uDestChan = (psTexForm->uSwizzle >> (uChan * 3)) & 0x7;
		if (uDestChan == UFREG_SWIZ_0 || uDestChan == UFREG_SWIZ_1)
		{
			continue;
		}
		if (psTexForm->peChannelForm[uDestChan] != USC_CHANNELFORM_U8 &&
			psTexForm->peChannelForm[uDestChan] != USC_CHANNELFORM_ZERO &&
			psTexForm->peChannelForm[uDestChan] != USC_CHANNELFORM_ONE)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL IsSaturatedTemp(PINTERMEDIATE_STATE psState,
								PUF_REGISTER psSrc,
								IMG_UINT32 uChanMask,
								PUNIFLEX_INST psStartInst,
								IMG_PBOOL pbLimitedPrecision)
/*****************************************************************************
 FUNCTION	: IsSaturatedTemp

 PURPOSE	: Checks if a temporary register is within the range 0..1.

 PARAMETERS	: psState			- Compiler state.
			  psSrc				- Source to check.
			  uChanMask			- Mask of channels to check.
			  apsProg			- Start of the program.
			  uStart			- Number of the instruction which has
								the source.
			  pbLimitedPrecision
								- TRUE if the temporary register contains
								an arithmetic result.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uDest;

	for (psInst = psStartInst->psBLink; psInst != NULL; psInst = psInst->psBLink)
	{

		/*
			Stop if we reach a flow control construct.
		*/
		if (psInst->eOpCode == UFOP_ENDIF ||
			psInst->eOpCode == UFOP_CALL ||
			psInst->eOpCode == UFOP_CALLNZ ||
			psInst->eOpCode == UFOP_CALLP ||
			psInst->eOpCode == UFOP_CALLNZBIT ||
			psInst->eOpCode == UFOP_ENDLOOP ||
			psInst->eOpCode == UFOP_ENDREP ||
			psInst->eOpCode == UFOP_GLSLENDLOOP)
		{
			return IMG_FALSE;
		}

		for (uDest = g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest > 0; uDest--)
		{
			PUF_REGISTER psDest = (uDest == 2) ? &psInst->sDest2 : &psInst->sDest;
			if (psDest->u.byMask &&
				psDest->eType == UFREG_TYPE_TEMP &&
				psDest->uNum == psSrc->uNum &&
				(psDest->u.byMask & uChanMask) != 0)
			{
				IMG_UINT32	uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
				IMG_UINT32	uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
				IMG_BOOL	bSat = IMG_FALSE;
				if (psInst->uPredicate != UF_PRED_NONE)
				{
					return IMG_FALSE;
				}
				if (psDest->eFormat == UF_REGFORMAT_I32 || psDest->eFormat == UF_REGFORMAT_U32)
				{
					//DX10 seems to suggest the bit-pattern resulting from these should be reinterpreted as a 32-bit float.
					//In which case all bets are off.
					return IMG_FALSE;
				}
				if (uSat == UFREG_DMOD_SATZEROONE)
				{
					bSat = IMG_TRUE;
				}
				else
				{
					if (uScale != UFREG_DMOD_SCALEMUL1)
					{
						return IMG_FALSE;
					}
					switch (psInst->eOpCode)
					{
						case UFOP_LD:
						case UFOP_LDB:
						case UFOP_LDL:
						case UFOP_LDD:
						case UFOP_LDP:
						case UFOP_LDPIFTC:
						{
							if (IsIntegerTexture(psState, psInst->asSrc[1].uNum))
							{
								bSat = IMG_TRUE;
							}
							break;
						}
						case UFOP_MOV:
						{
							IMG_UINT32	uMovMask = psDest->u.byMask & uChanMask;
							if (IsSaturatedSource(psState, &psInst->asSrc[0], uMovMask, psInst, NULL))
							{
								bSat = IMG_TRUE;
							}
							if (pbLimitedPrecision != NULL)
							{
								*pbLimitedPrecision = IMG_FALSE;
							}
							break;
						}
						case UFOP_MUL:
						{
							IMG_UINT32 uMulMask = psDest->u.byMask & uChanMask;
							ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests==1);
							//2-dest multiplies (a la DX10) are 32-bit int only and should have been caught above...
							if (IsSaturatedSource(psState, &psInst->asSrc[0], uMulMask, psInst, NULL) &&
								IsSaturatedSource(psState, &psInst->asSrc[1], uMulMask, psInst, NULL))
							{
								bSat = IMG_TRUE;
							}
							if (pbLimitedPrecision != NULL)
							{
								*pbLimitedPrecision = IMG_FALSE;
							}
							break;
						}
						default: 
						{
							break;
						}
					}
				}
				if (!bSat)
				{
					return IMG_FALSE;
				}
				uChanMask &= ~(*((IMG_UINT32*)(&psInst->sDest.u.byMask)));
				if (uChanMask == 0)
				{
					/* all channels dealt with (successfully) */
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL IsSaturatedSource(PINTERMEDIATE_STATE	psState,
								  PUF_REGISTER			psSrc,
								  IMG_UINT32			uChanMask,
								  PUNIFLEX_INST			psStartInst,
								  IMG_PBOOL				pbLimitedPrecision)
/*****************************************************************************
 FUNCTION	: IsSaturatedSource

 PURPOSE	: Checks if a source is within the range 0..1.

 PARAMETERS	: psState			- Compiler state.
			  psSrc				- Source to check.
			  uChanMask			- Mask of channels to check.
			  psProg			- Start of the program.
			  uStart			- Number of the instruction which has
								the source.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uMod = (psSrc->byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32	uSwizzledChanMask;
	
	if (pbLimitedPrecision != NULL)
	{
		*pbLimitedPrecision = IMG_TRUE;
	}

	/*
		Check for source modifiers which take the data outside the range 0..1.
	*/
	if (psSrc->byMod & UFREG_SOURCE_NEGATE)
	{
		return IMG_FALSE;
	}

	if (uMod != UFREG_SMOD_NONE && uMod != UFREG_SMOD_COMPLEMENT)
	{
		return IMG_FALSE;
	}

	/*
		Check for any channels which have a constant value swizzled into them.
	*/
	uSwizzledChanMask = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (uChanMask & (1U << uChan))
		{
			IMG_UINT32	uSrcChan = (psSrc->u.uSwiz >> (uChan * 3)) & 0x7;
			if (uSrcChan == UFREG_SWIZ_2)
			{
				return IMG_FALSE;
			}
			else if (uSrcChan == UFREG_SWIZ_0 || uSrcChan == UFREG_SWIZ_1 || uSrcChan == UFREG_SWIZ_HALF)
			{
				uChanMask &= ~(1U << uChan);
			}
			else
			{
				uSwizzledChanMask |= (1U << uSrcChan);
			}
		}
	}

	/*
		Check based on the type of the register.
	*/
	switch (psSrc->eType)
	{
		case UFREG_TYPE_COL: 
		{
			return IMG_TRUE;
		}
		case UFREG_TYPE_HW_CONST:
		{
			return (psSrc->uNum != HW_CONST_2) ? IMG_TRUE : IMG_FALSE;
		}
		case UFREG_TYPE_CONST:
		{
			for (uChan = 0; uChan < 4; uChan++)
			{
				IMG_UINT32	uSrcChan = (psSrc->u.uSwiz >> (uChan * 3)) & 7;
				IMG_FLOAT	fChanValue;
				IMG_UINT32	uChanValue;

				if ((uChanMask & (1U << uChan)) == 0)
				{
					continue;
				}

				if (!CheckForStaticConst(psState, psSrc->uNum, uSrcChan, psSrc->uArrayTag, &uChanValue))
				{
					return IMG_FALSE;
				}
				fChanValue = *(IMG_PFLOAT)&uChanValue;
				if (fChanValue < 0.0f || fChanValue > 1.0f)
				{
					return IMG_FALSE;
				}
			}
			return IMG_TRUE;
		}
		case UFREG_TYPE_TEMP:
		{
			return IsSaturatedTemp(psState, psSrc, uSwizzledChanMask, psStartInst, pbLimitedPrecision);
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL ConvertMoveInstruction(PINTERMEDIATE_STATE psState,
									   PPRECOVR_STATE psPState,
									   PUNIFLEX_INST psInst,
									   IMG_UINT32 uLiveMask,
									   IMG_UINT32 uTag,
									   IMG_UINT32 uDestPrecision)
/*****************************************************************************
 FUNCTION	: ConvertMoveInstruction

 PURPOSE	: Convert a mov instruction to operate on packed register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  psBlock			- Block to insert the instructions into.
			  uLiveMask			- Channels that are live in the destination.
			  uTag				- Packed register to write into.

 RETURNS	: TRUE if the instruction could be converted; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	PUNIFLEX_INST	psPackInst = psInst;

	/*
		Don't convert a U32/I32->F32 conversion instruction. 
	*/
	if (psInst->asSrc[0].eFormat == UF_REGFORMAT_I32 ||
		psInst->asSrc[0].eFormat == UF_REGFORMAT_U32 ||
		psInst->asSrc[0].eFormat == UF_REGFORMAT_I16 ||
		psInst->asSrc[0].eFormat == UF_REGFORMAT_U16)
	{
		return IMG_FALSE;
	}

	if (uSat == UFREG_DMOD_SATNEGONEONE || uSat == UFREG_DMOD_SATZEROMAX)
	{
		return IMG_FALSE;
	}
	if (uScale != UFREG_DMOD_SCALEMUL1 && uScale != UFREG_DMOD_SCALEMUL2)
	{
		return IMG_FALSE;
	}
	if (psInst->asSrc[0].byMod != UFREG_SMOD_NONE)
	{
		return IMG_FALSE;
	}
	if (!(psInst->asSrc[0].eType == UFREG_TYPE_TEMP ||
		  psInst->asSrc[0].eType == UFREG_TYPE_COL ||
		  psInst->asSrc[0].eType == UFREG_TYPE_HW_CONST ||
		  psInst->asSrc[0].eType == UFREG_TYPE_CONST))
	{
		return IMG_FALSE;
	}

	psInst->sDest.eType = UFREG_TYPE_TEMP;
	psInst->sDest.uNum = uTag;
	psInst->sDest.eFormat = UF_REGFORMAT_U8;
	if (psInst->asSrc[0].eType == UFREG_TYPE_TEMP)
	{
		IMG_UINT32	uSrcNum, uSrcMask;

		uSrcMask = SwizzleMask(psInst->asSrc[0].u.uSwiz, psInst->sDest.u.byMask & uLiveMask);
		uSrcNum = AddPackedRegister(psState, psPState, &psInst->asSrc[0], uSrcMask, uDestPrecision, psPackInst);

		psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[0].uNum = uSrcNum;
	}
	
	/* Keep the original given format for constants. */
	if(psInst->asSrc[0].eType != UFREG_TYPE_CONST)
	{
		psInst->asSrc[0].eFormat = UF_REGFORMAT_U8;
	}
	return IMG_TRUE;
}

static IMG_BOOL ConvertMulInstruction(PINTERMEDIATE_STATE	psState,
									  PPRECOVR_STATE		psPState,
									  PUNIFLEX_INST			psInst,
									  IMG_UINT32			uLiveMask,
									  IMG_UINT32			uDestTag,
									  IMG_UINT32			uDestPrecision)
/*****************************************************************************
 FUNCTION	: ConvertMulInstruction

 PURPOSE	: Convert a mul instruction to operate on packed register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  uLiveMask			- Channels that are live in the destination.
			  uDestTag			- Packed register to write into.

 RETURNS	: TRUE if the instruction could be converted; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uMask = psInst->sDest.u.byMask & uLiveMask;
	IMG_UINT32	uSwizMask = MaskToSwiz(uMask);
	IMG_UINT32	uSrc1Mod = (psInst->asSrc[0].byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32	uSrc2Mod = (psInst->asSrc[1].byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32	uArg;
	IMG_BOOL	abSrcAlphaSwiz[2];
	IMG_BOOL	abLimitedPrecision[2];
	IMG_UINT32	uSourcePrecision;
	PUNIFLEX_INST	psPackInst = psInst;

	/*
		Check for an incompatible destination modifier.
	*/
	if (uSat == UFREG_DMOD_SATNEGONEONE || 
		uSat == UFREG_DMOD_SATZEROMAX || 
		(uScale != UFREG_DMOD_SCALEMUL1 && uScale != UFREG_DMOD_SCALEMUL2))
	{
		return IMG_FALSE;
	}

	for (uArg = 0; uArg < 2; uArg++)
	{
		if (!IsSaturatedSource(psState, &psInst->asSrc[uArg], uMask, psInst, &abLimitedPrecision[uArg]))
		{
			return IMG_FALSE;
		}
		if (psInst->asSrc[uArg].eType != UFREG_TYPE_TEMP &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_COL &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_CONST &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_HW_CONST)
		{
			return IMG_FALSE;
		}
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_CONST && 
			GetRegisterFormat(psState, &psInst->asSrc[uArg]) != UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP ||
			psInst->asSrc[uArg].eType == UFREG_TYPE_COL)
		{
			if (uMask != 0x8 && (psInst->asSrc[uArg].u.uSwiz & uSwizMask) == (UFREG_SWIZ_AAAA & uSwizMask))
			{
				abSrcAlphaSwiz[uArg] = IMG_TRUE;
			}
			else
			{
				abSrcAlphaSwiz[uArg] = IMG_FALSE;
			}
		}
		else
		{
			abSrcAlphaSwiz[uArg] = IMG_FALSE;
		}
	}	

	if (uSrc1Mod == UFREG_SMOD_COMPLEMENT && uSrc2Mod == UFREG_SMOD_COMPLEMENT)
	{
		return IMG_FALSE;
	}
	if (uScale == UFREG_DMOD_SCALEMUL2 && (uSrc1Mod == UFREG_SMOD_COMPLEMENT || uSrc2Mod == UFREG_SMOD_COMPLEMENT))
	{
		return IMG_FALSE;
	}
	if (uScale == UFREG_DMOD_SCALEMUL2 && (abSrcAlphaSwiz[0] || abSrcAlphaSwiz[1]))
	{
		return IMG_FALSE;
	}

	uSourcePrecision = uDestPrecision;
	if (!abLimitedPrecision[0] || !abLimitedPrecision[1])
	{
		uSourcePrecision++;
		if (uSourcePrecision > psState->psSAOffsets->uPackPrecision)
		{
			return IMG_FALSE;
		}
	}

	psInst->sDest.eType = UFREG_TYPE_TEMP;
	psInst->sDest.uNum = uDestTag;
	psInst->sDest.eFormat = UF_REGFORMAT_U8;

	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
		{
			IMG_UINT32	uSrcNum;
			IMG_UINT32	uSrcMask = SwizzleMask(psInst->asSrc[uArg].u.uSwiz, psInst->sDest.u.byMask & uLiveMask);

			uSrcNum = AddPackedRegister(psState, psPState, &psInst->asSrc[uArg], uSrcMask, uSourcePrecision, psPackInst);

			psInst->asSrc[uArg].eType = UFREG_TYPE_TEMP;
			psInst->asSrc[uArg].uNum = uSrcNum;
		}
		else if (psInst->asSrc[uArg].eType == UFREG_TYPE_CONST)
		{
			{
				PUNIFLEX_INST	psMove;
				psMove = (PUNIFLEX_INST)UscAlloc(psState, sizeof(UNIFLEX_INST));
				psMove->eOpCode = UFOP_MOV;
#ifdef SRC_DEBUG
				psMove->uSrcLine = psInst->uSrcLine;
#endif
				psMove->uPredicate = UF_PRED_NONE;
				psMove->sDest.eType = UFREG_TYPE_TEMP;
				psMove->sDest.uNum = psState->uInputTempRegisterCount++;
				psMove->sDest.eFormat = UF_REGFORMAT_U8;
				psMove->sDest.u.byMask = (IMG_BYTE)GetSourceLiveChans(psState, psInst, uArg);
				psMove->sDest.byMod = UFREG_DMOD_SATNONE;
				psMove->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
				psMove->sDest.uArrayTag = 0;
				psMove->asSrc[0] = psInst->asSrc[uArg];
				psMove->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
				psMove->asSrc[0].byMod = UFREG_SMOD_NONE;
				psMove->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
				psMove->asSrc[0].uArrayTag = 0;
				psMove->asSrc[0].u1.uRelativeStrideInComponents = 4;
				/*
					Insert the instruction into the linked list.
				*/
				if (psInst->psBLink != NULL)
				{
					psInst->psBLink->psILink = psMove;
				}
				
				psMove->psBLink = psInst->psBLink;
				psMove->psILink = psInst;

				psInst->psBLink = psMove;
			}
			psInst->asSrc[uArg].eType = UFREG_TYPE_TEMP;
			psInst->asSrc[uArg].uNum = (psState->uInputTempRegisterCount) - 1;
		}
		psInst->asSrc[uArg].eFormat = UF_REGFORMAT_U8;
	}

	return IMG_TRUE;
}

static IMG_BOOL IsStaticConstInRange(PINTERMEDIATE_STATE psState,
									 PUF_REGISTER psSrc,
									 IMG_UINT32 uMask,
									 IMG_BOOL bLessEqualZero)
/*****************************************************************************
 FUNCTION	: IsStaticConstInRange

 PURPOSE	: Checks if a constant is static and inside a certain range.

 PARAMETERS	: psState			- Compiler state.
			  psSrc				- Constant source to check.
			  uMask				- Mask of channels to check.
			  bLessEqualZero	- TRUE to check for less than or equal to zero;
								  FALSE to check for greater than or equal to one.

 RETURNS	: TRUE if the constant is static and inside the specified range.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32	uSwizChan = EXTRACT_CHAN(psSrc->u.uSwiz, uChan);

		if ((uMask & (1U << uChan)) == 0)
		{
			continue;
		}
		
		if (uSwizChan > UFREG_SWIZ_A)
		{
			if (bLessEqualZero)
			{
				if (uSwizChan != UFREG_SWIZ_0)
				{
					return IMG_FALSE;
				}
			}
			else
			{
				if (!(uSwizChan == UFREG_SWIZ_1 || uSwizChan == UFREG_SWIZ_2))
				{
					return IMG_FALSE;
				}
			}
		}
		else
		{
			IMG_UINT32	uValue;
			IMG_FLOAT	fValue;

			if (!CheckForStaticConst(psState, psSrc->uNum, uSwizChan, psSrc->uArrayTag, &uValue))
			{
				return IMG_FALSE;
			}

			fValue = *(IMG_PFLOAT)&uValue;
			if ((bLessEqualZero && fValue > 0.0f) || (!bLessEqualZero && fValue < 1.0f))
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL ConvertMinMaxInstruction(PINTERMEDIATE_STATE psState,
										 PPRECOVR_STATE psPState,
										 PUNIFLEX_INST psInst, 
										 IMG_UINT32 uLiveMask, 
										 IMG_UINT32 uDestTag,
										 IMG_UINT32 uDestPrecision)
/*****************************************************************************
 FUNCTION	: ConvertMinMaxInstruction

 PURPOSE	: Convert a min or max instruction to operate on packed register.

 PARAMETERS	: psState			- Compiler state.
			  psPState			- Preconversion state
			  psInst			- Instruction to convert.
			  uLiveMask			- Channels that are live in the destination.
			  uTag				- Packed register to write into.

 RETURNS	: TRUE if the instruction could be converted; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uMask = psInst->sDest.u.byMask & uLiveMask;
	IMG_BOOL	bMin = (psInst->eOpCode == UFOP_MIN) ? IMG_TRUE : IMG_FALSE;
	IMG_UINT32	uArg;
	PUNIFLEX_INST	psPackInst = psInst;

	/*
		Check for an incompatible destination modifier.
	*/
	if (uSat == UFREG_DMOD_SATNEGONEONE || uSat == UFREG_DMOD_SATZEROMAX || uScale != UFREG_DMOD_SCALEMUL1)
	{
		return IMG_FALSE;
	}

	/*
		Check if one of the arguments is a constant less than or equal to zero (for a max) or
		a constant greater than or equal to one (for a min).
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_CONST &&
			psInst->asSrc[uArg].byMod == 0 &&
			IsStaticConstInRange(psState, &psInst->asSrc[uArg], uMask, !bMin))
		{
			break;
		}
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_CONST && 
			GetRegisterFormat(psState, &psInst->asSrc[uArg]) != UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
	}
	if (uArg == 2)
	{
		return IMG_FALSE;
	}

	/*
		Check for a source which can't be 8-bit.
	*/
	if (psInst->asSrc[1 - uArg].eType != UFREG_TYPE_TEMP)
	{
		return IMG_FALSE;
	}

	/*
		If the min/max doesn't have any effect because the result will be saturated later then convert to a move.
	*/
	psInst->eOpCode = UFOP_MOV;
	psInst->sDest.eType = UFREG_TYPE_TEMP;
	psInst->sDest.uNum = uDestTag;
	psInst->sDest.eFormat = UF_REGFORMAT_U8;
	if (uArg != 1)
	{
		psInst->asSrc[0] = psInst->asSrc[1];
	}
	if (psInst->asSrc[0].eType == UFREG_TYPE_TEMP)
	{
		IMG_UINT32	uSrcNum;
		IMG_UINT32	uSrcMask = SwizzleMask(psInst->asSrc[0].u.uSwiz, psInst->sDest.u.byMask & uLiveMask);

		uSrcNum = AddPackedRegister(psState, psPState, &psInst->asSrc[0], uSrcMask, uDestPrecision, psPackInst);

		psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[0].uNum = uSrcNum;
	}
	psInst->asSrc[0].eFormat = UF_REGFORMAT_U8;
	return IMG_TRUE;
}

static IMG_BOOL ConvertAddInstruction(PINTERMEDIATE_STATE psState,
									  PPRECOVR_STATE psPState,
									  PUNIFLEX_INST psInst,
									  IMG_UINT32 uLiveMask,
									  IMG_UINT32 uDestTag,
									  IMG_UINT32 uDestPrecision)
/*****************************************************************************
 FUNCTION	: ConvertAddInstruction

 PURPOSE	: Convert an add instruction to operate on packed register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to convert.
			  psBlock			- Block to insert the instructions into.
			  uLiveMask			- Channels that are live in the destination.
			  apsProg			- Start of the input program.
			  uInstNum			- Number of the instruction to convert.
			  uTag				- Packed register to write into.

 RETURNS	: TRUE if the instruction could be converted; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uMask = psInst->sDest.u.byMask & uLiveMask;
	IMG_UINT32	uSrc1Mod = (psInst->asSrc[0].byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32	uSrc2Mod = (psInst->asSrc[1].byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32	uArg;
	IMG_BOOL	abLimitedPrecision[2];
	IMG_UINT32	uSourcePrecision;
	PUNIFLEX_INST	psPackInst = psInst;

	/*
		Check for an incompatible destination modifier.
	*/
	if (uSat == UFREG_DMOD_SATNEGONEONE || uSat == UFREG_DMOD_SATZEROMAX || uScale != UFREG_DMOD_SCALEMUL1)
	{
		return IMG_FALSE;
	}

	for (uArg = 0; uArg < 2; uArg++)
	{
		if (!IsSaturatedSource(psState, &psInst->asSrc[uArg], uMask, psInst, &abLimitedPrecision[uArg]))
		{
			return IMG_FALSE;
		}
		if (psInst->asSrc[uArg].eType != UFREG_TYPE_TEMP &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_COL &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_CONST &&
			psInst->asSrc[uArg].eType != UFREG_TYPE_HW_CONST)
		{
			return IMG_FALSE;
		}
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_CONST && 
			GetRegisterFormat(psState, &psInst->asSrc[uArg]) != UF_REGFORMAT_F32)
		{
			return IMG_FALSE;
		}
	}

	if (uSrc1Mod == UFREG_SMOD_COMPLEMENT || uSrc2Mod == UFREG_SMOD_COMPLEMENT)
	{
		return IMG_FALSE;
	}

	uSourcePrecision = uDestPrecision;
	if (!abLimitedPrecision[0] || !abLimitedPrecision[1])
	{
		uSourcePrecision++;
		if (uSourcePrecision > psState->psSAOffsets->uPackPrecision)
		{
			return IMG_FALSE;
		}
	}

	psInst->sDest.eType = UFREG_TYPE_TEMP;
	psInst->sDest.uNum = uDestTag;
	psInst->sDest.eFormat = UF_REGFORMAT_U8;
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
		{
			IMG_UINT32	uSrcNum;
			IMG_UINT32	uSrcMask = SwizzleMask(psInst->asSrc[uArg].u.uSwiz, psInst->sDest.u.byMask & uLiveMask);

			uSrcNum = AddPackedRegister(psState, psPState, &psInst->asSrc[uArg], uSrcMask, uSourcePrecision, psPackInst);

			psInst->asSrc[uArg].eType = UFREG_TYPE_TEMP;
			psInst->asSrc[uArg].uNum = uSrcNum;
		}
		psInst->asSrc[uArg].eFormat = UF_REGFORMAT_U8;
	}

	return IMG_TRUE;
}


static IMG_BOOL ConvertInstructionToInteger(PINTERMEDIATE_STATE psState,
											PPRECOVR_STATE		psPState,
											PUNIFLEX_INST		psInst,
											PPACKED_OUTPUT		apsOutput[])
/*****************************************************************************
 FUNCTION	: ConvertInstructionToInteger

 PURPOSE	: Convert an instruction to operate on packed register.

 PARAMETERS	: psState			- Compiler state.
			  psPState			- Preconversion state
			  psInst			- Instruction to convert.
			  apsOutput			- Array of PPACKED_OUTPUTs, one per instruction destination
								(NULL if that destination is not wanted packed; otherwise,
								 contains mask, precision, and packed-register number)

 RETURNS	: TRUE if the instruction could be converted; FALSE otherwise.
*****************************************************************************/
{
	/*
		Don't convert predicated instructions.
	*/
	if (psInst->uPredicate != UF_PRED_NONE)
	{
		return IMG_FALSE;
	}

	/*
		Don't convert integer calculations.
	*/
	if (psInst->sDest.eFormat == UF_REGFORMAT_U32 ||
		psInst->sDest.eFormat == UF_REGFORMAT_I32 ||
		psInst->sDest.eFormat == UF_REGFORMAT_U16 ||
		psInst->sDest.eFormat == UF_REGFORMAT_I16)
	{
		return IMG_FALSE;
	}

	/*
		Check if the result is used in an instruction which we haven't converted
		to integer.
	*/
	if (IsResultUsed(psState,
					 psInst->psILink,
					 psInst->sDest.eType,
					 psInst->sDest.uNum,
					 psInst->sDest.u.byMask,
					 NULL))
	{
		return IMG_FALSE;
	}
	
	/*
		Do the conversion depending on the opcode.
	*/
	switch (psInst->eOpCode)
	{
		case UFOP_MOV:
		{
			return ConvertMoveInstruction(psState, psPState, psInst, apsOutput[0]->uMask, apsOutput[0]->uPackedNum, apsOutput[0]->uPrecision);
		}
		case UFOP_MUL:
		{
			if(!(psState->bInvariantShader))
			{
				return ConvertMulInstruction(psState, psPState, psInst, apsOutput[0]->uMask, apsOutput[0]->uPackedNum, apsOutput[0]->uPrecision);
			}
			else
			{
				return IMG_FALSE;
			}
		}
		case UFOP_ADD:
		{
			if(!(psState->bInvariantShader))
			{
				return ConvertAddInstruction(psState, psPState, psInst, apsOutput[0]->uMask, apsOutput[0]->uPackedNum, apsOutput[0]->uPrecision);
			}	
			else
			{
				return IMG_FALSE;
			}
		}
		case UFOP_LD:
		case UFOP_LDB:
		case UFOP_LDL:
		case UFOP_LDD:
		case UFOP_LDP:
		case UFOP_LDPIFTC:
		{
			if (IsIntegerTexture(psState, psInst->asSrc[1].uNum))
			{
				psInst->sDest.eType = UFREG_TYPE_TEMP;
				psInst->sDest.uNum = apsOutput[0]->uPackedNum;
				psInst->sDest.eFormat = UF_REGFORMAT_U8;
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		case UFOP_MIN:
		case UFOP_MAX:
		{
			return ConvertMinMaxInstruction(psState, psPState, psInst, apsOutput[0]->uMask, apsOutput[0]->uPackedNum, apsOutput[0]->uPrecision);
		}
		default:
		{
			/*
				This includes I32/U32 MUL2/DIV2 operations - although signature above means we have the info req'd
				for both dests, it seems the spec wants us to reinterpret the bit pattern of any 32-bit int as a float
				(and then type-converting that). Since there's no way to compute the result of that any more directly,
				we don't support converting such two-dest multiply instructions.
			*/
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL IsIntegerDot3Arg(PUF_REGISTER psArg,
								 IMG_UINT32 uMask,
								 PINTERMEDIATE_STATE psState,
								 PUNIFLEX_INST psInst,
								 PUNIFLEX_INST* ppsTexldInst)
/*****************************************************************************
 FUNCTION	: IsIntegerDot3Arg

  PURPOSE	: Searches backwards from psInst (a dot3 instruction) to find the LD
	instruction producing the specified argument to psInst. If this instruction
	loads from an integer texture, and it's result is not othewise used, then
	returns TRUE and sets ppsTexLdInst to that instruction; otherwise, returns
	FALSE (including case where the argument is not produced by an LD instruction).

 PARAMETERS	:

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUNIFLEX_INST	psDotInst = psInst;
	if (psArg->u.uSwiz != UFREG_SWIZ_NONE)
	{
		return IMG_FALSE;
	}
	if (psArg->byMod != UFREG_SMOD_NONE && psArg->byMod != UFREG_SMOD_SIGNED)
	{
		return IMG_FALSE;
	}
	if (psArg->eType == UFREG_TYPE_COL)
	{
		return IMG_TRUE;
	}
	if (psArg->eType == UFREG_TYPE_TEMP) 
	{
		IMG_UINT32		uNum = psArg->uNum;
		
		for (psInst = psInst->psBLink; psInst != NULL; psInst = psInst->psBLink)
		{
			switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
			{
			default: ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests == 0); break;
			case 2:
				/* At present there are no two-dest texture loads... */
				if ((psInst->sDest2.eType == UFREG_TYPE_TEMP && psInst->sDest2.uNum == uNum && psInst->sDest2.u.byMask) ||
					(psInst->sDest.eType == UFREG_TYPE_TEMP && psInst->sDest.uNum == uNum && psInst->sDest.u.byMask))
					return IMG_FALSE;
				break;
			case 1:
				if (psInst->sDest.eType == UFREG_TYPE_TEMP &&
					psInst->sDest.uNum == uNum)
				{
					if ((uMask & ~(*((IMG_UINT32*)(&psInst->sDest.u.byMask)))) != 0)
					{
						return IMG_FALSE;
					}
					if (psInst->eOpCode != UFOP_LD &&
						psInst->eOpCode != UFOP_LDP &&
						psInst->eOpCode != UFOP_LDPIFTC &&
						psInst->eOpCode != UFOP_LDB &&
						psInst->eOpCode != UFOP_LDL &&
						psInst->eOpCode != UFOP_LDD)
					{
						return IMG_FALSE;
					}
					if (!IsIntegerTexture(psState, psInst->asSrc[1].uNum))
					{
						return IMG_FALSE;
					}

					if (IsResultUsed(psState, psInst->psILink, UFREG_TYPE_TEMP, uNum, 
									 psInst->sDest.u.byMask, psDotInst))
					{
						return IMG_FALSE;
					}

					*ppsTexldInst = psInst;

					return IMG_TRUE;
				}
			}
		}
		ASSERT (psInst == NULL);
	}
	return IMG_FALSE;
}

static IMG_VOID ConvertTextureSource(PINTERMEDIATE_STATE	psState, 
									 IMG_UINT32				uLiveMask, 
									 PUNIFLEX_INST			psLdInst,
									 IMG_PUINT32			puSrcNum)
/*****************************************************************************
 FUNCTION	: ConvertTextureSource

 PURPOSE	:

 PARAMETERS	: 

 RETURNS	: The swizzle for the texture source.
*****************************************************************************/
{
	#ifdef UF_TESTBENCH
	/* uLiveMask is only used in the ASSERT macro on non-UF_TESTBENCH builds */
	PVR_UNREFERENCED_PARAMETER(uLiveMask);
	#endif	/* #ifdef UF_TESTBENCH */

	ASSERT((uLiveMask & ~psLdInst->sDest.u.byMask) == 0);
	ASSERT(psLdInst->eOpCode >= UFOP_LD && psLdInst->eOpCode <= UFOP_LDD);

	psLdInst->sDest.eType = UFREG_TYPE_TEMP;
	psLdInst->sDest.uNum = psState->uInputTempRegisterCount++;
	psLdInst->sDest.eFormat = UF_REGFORMAT_U8;

	*puSrcNum = psLdInst->sDest.uNum;

	return;
}

static IMG_VOID ConvertDotProduct(PINTERMEDIATE_STATE	psState,
								  PUNIFLEX_INST			psInst)
/*****************************************************************************
 FUNCTION	: ConvertDotProduct

 PURPOSE	: 

 PARAMETERS	: 

 RETURNS	: 
*****************************************************************************/
{
	IMG_UINT32		uMask = (psInst->eOpCode == UFOP_DOT3) ? 7 : 15;
	IMG_UINT32		uScale = (psInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32		uSat = (psInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32		uArg;
	PUNIFLEX_INST	apsTexldInst[2];
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (!IsIntegerDot3Arg(&psInst->asSrc[uArg],
							  uMask,
							  psState,
							  psInst,
							  &apsTexldInst[uArg]))
		{
			return;
		}
	}
	if (psInst->asSrc[0].byMod != psInst->asSrc[1].byMod)
	{
		return;
	}
	if (psInst->asSrc[0].byMod == UFREG_SMOD_NONE)
	{
		if (uScale > UFREG_DMOD_SCALEMUL8 || uSat != UFREG_DMOD_SATZEROONE)
		{
			return;
		}
	}
	else
	{
		if (uScale > UFREG_DMOD_SCALEMUL2 || !(uSat == UFREG_DMOD_SATZEROONE || uSat == UFREG_DMOD_SATNEGONEONE))
		{
			return;
		}
	}
	for (uArg = 0; uArg < 2; uArg++)
	{
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
		{
			psInst->asSrc[uArg].eType = UFREG_TYPE_TEMP;

			ConvertTextureSource(psState,
								 uMask, 
								 apsTexldInst[uArg], 
								 &psInst->asSrc[uArg].uNum);
		}
		psInst->asSrc[uArg].eFormat = UF_REGFORMAT_U8;
	}
}

static IMG_VOID ConvertPrologOperations(PINTERMEDIATE_STATE		psState,
										PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ConvertPrologOperations

 PURPOSE	: 

 PARAMETERS	: 

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	/*
		Try to convert some operations at the start of the program to integer.
	*/
	for (psInst = psProg->psHead; psInst != NULL; psInst = psInst->psILink)
	{
		/*
			Stop when we reach flow control to simplify tracking data dependencies.
		*/
		if (IsInputInstFlowControl(psInst))
		{
			break;
		}
		if (g_asInputInstDesc[psInst->eOpCode].uNumDests == 0)
		{
			continue;
		}
		if ((psInst->eOpCode == UFOP_DOT3 || psInst->eOpCode == UFOP_DOT4) && 
			GetRegisterFormat(psState, &psInst->sDest) == UF_REGFORMAT_F32)
		{
			ConvertDotProduct(psState, psInst);
		}
	}
}

static IMG_VOID AddPackToInteger(PINTERMEDIATE_STATE	psState,
								 PPACKED_OUTPUT			psOutput,
								 IMG_UINT32				uDestMask,
								 PUNIFLEX_INST	        psPackInst,
								 PUNIFLEX_INST			psInsertAfter)
/*****************************************************************************
 FUNCTION	: AddPackToInteger

 PURPOSE	: Add an instruction to pack an instruction result to integer

 PARAMETERS	: psState			- Compiler state..
			  uDestNum			- Register number for the packed result.
			  uDestMask			- Channels to pack.
			  eSrcType			- Register to pack.
			  eSrcFormat
			  uSrcNum
			  ppsILink			- Where to insert the pack instruction.
			  puNumInsts		- Where to record number of instructions added
			  psPackInst        - Instruction for which packing was done.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psMove;

	#if!defined(SRC_DEBUG)
	PVR_UNREFERENCED_PARAMETER(psPackInst);
	#endif /* SRC_DEBUG */

	psMove = (PUNIFLEX_INST)UscAlloc(psState, sizeof(UNIFLEX_INST));

	psMove->eOpCode = UFOP_MOV;

	#ifdef SRC_DEBUG
	if(psPackInst)
	{
		psMove->uSrcLine = psPackInst->uSrcLine;
	}
	else
	{
		psMove->uSrcLine = psInsertAfter->uSrcLine;
	}
	#endif /* SRC_DEBUG */

	psMove->uPredicate = UF_PRED_NONE;
	psMove->sDest.eType = UFREG_TYPE_TEMP;
	psMove->sDest.uNum = psOutput->uPackedNum;
	psMove->sDest.eFormat = UF_REGFORMAT_U8;
	psMove->sDest.u.byMask = (IMG_UINT8)uDestMask;
	psMove->sDest.byMod = UFREG_DMOD_SATNONE;
	psMove->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psMove->sDest.uArrayTag = 0;
	psMove->asSrc[0].eType = psOutput->eType;
	psMove->asSrc[0].uNum = psOutput->uUnpackedNum;
	psMove->asSrc[0].eFormat = psOutput->eFormat;
	psMove->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
	psMove->asSrc[0].byMod = UFREG_SMOD_NONE;
	psMove->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psMove->asSrc[0].uArrayTag = 0;
	psMove->asSrc[0].u1.uRelativeStrideInComponents = 4;

	psMove->psBLink = psInsertAfter;
	psMove->psILink = psInsertAfter->psILink;

	if (psInsertAfter->psILink != NULL)
	{
		ASSERT(psInsertAfter->psILink->psBLink == psInsertAfter);
		psInsertAfter->psILink->psBLink = psMove;
	}

	psInsertAfter->psILink = psMove;
}

static PUNIFLEX_INST FindElse(PINTERMEDIATE_STATE	psState,
							  PUNIFLEX_INST			psStartInst)
/*****************************************************************************
 FUNCTION	: FindElse

 PURPOSE	: Find the start of the other part of an if statement.

 PARAMETERS	: psState		- Internal compiler state
			  psStartInst	- Instruction before the ENDIF

 RETURNS	: The instruction before the ELSE or NULL if it doesn't exist.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uIfCount = 0;

	for (psInst = psStartInst; psInst != NULL; psInst = psInst->psBLink)
	{
		if (psInst->eOpCode == UFOP_ENDIF)
		{
			uIfCount++;
		}
		if (IsConditionalStart(psInst))
		{
			if (uIfCount == 0)
			{
				return NULL;
			}
			else
			{
				uIfCount--;
			}
		}
		if (psInst->eOpCode == UFOP_ELSE && uIfCount == 0)
		{
			return psInst->psBLink;
		}
	}

	imgabort();
	// PRQA S 0744 6
}

static IMG_VOID FindPackedRegister(PINTERMEDIATE_STATE psState,
								   PPRECOVR_STATE psPState,
								   PUF_REGISTER psDest,
								   PPACKED_OUTPUT *psOutput)
/*****************************************************************************
 FUNCTION	: FindPackedRegister

 PURPOSE	: Extracts an entry from the list of registers to be packed

 PARAMETERS	: psState			- Compiler state.
			  psPState			- Precover state, including said list
			  psDest			- Register to be removed from list
			  ppsOutput			- Packed register info is copied from list to here.
			  					  Note that it's ->eFormat will be set also.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uReg;
	for (uReg = 0; uReg < psPState->uNumPackedRegisters; uReg++)
	{
		if (psPState->apsPackedRegisters[uReg]->eType == psDest->eType &&
			psPState->apsPackedRegisters[uReg]->uUnpackedNum == psDest->uNum &&
			(psPState->apsPackedRegisters[uReg]->uMask & psDest->u.byMask) != 0)
		{
			if (psPState->apsPackedRegisters[uReg]->eFormat == UF_REGFORMAT_INVALID)
			{
				psPState->apsPackedRegisters[uReg]->eFormat = psDest->eFormat;
			}
			else
			{
				ASSERT(psPState->apsPackedRegisters[uReg]->eFormat == psDest->eFormat);
			}
			*psOutput=UscAlloc(psState,sizeof(PACKED_OUTPUT));
			memcpy(*psOutput,psPState->apsPackedRegisters[uReg],sizeof(PACKED_OUTPUT));
			/*
				Remove the written value from the list of packed registers
			*/
			psPState->apsPackedRegisters[uReg]->uMask &= ~(*((IMG_UINT32*)(&psDest->u.byMask)));
			if (psPState->apsPackedRegisters[uReg]->uMask == 0)
			{
				*psPState->apsPackedRegisters[uReg] = 
					*psPState->apsPackedRegisters[psPState->uNumPackedRegisters - 1];
				psPState->uNumPackedRegisters--;
			}
			break;
		}
	}
}

static
IMG_VOID DoIntegerConversion(PINTERMEDIATE_STATE	psState,
							 PPRECOVR_CONTEXT		psContext,
							 PINPUT_PROGRAM			psProg,
							 PUNIFLEX_INST			psStartInst,
							 PPRECOVR_STATE			psPState)
/*****************************************************************************
 FUNCTION	: DoIntegerConversion

 PURPOSE	: Try converting the tail of the input program to integer instructions.

 PARAMETERS	: psState			- Compiler state..
			  psProg			- Input program.
			  uStartInst		- Instruction to start conversion from.
									(i.e. last instruction in (sub-)program)
			  psPState			- Registers that are wanted in packed format.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uReg;
	IMG_BOOL bStopScanning=IMG_FALSE;

	for (psInst = psStartInst;
		 psInst != NULL && psPState->uNumPackedRegisters > 0 && !bStopScanning;
		 psInst = psInst->psBLink)
	{
		PPACKED_OUTPUT	apsOutput[2];
		PUNIFLEX_INST	psElseStart;
		IMG_BOOL bWantToConvert=IMG_FALSE;
		IMG_UINT32 uDest;

		/*
			Try converting instructions in both branches of an if.
		*/
		if (
				psInst->eOpCode == UFOP_ENDIF && 
				psContext->uNumPrecovrStates < USC_NUM_PRECOVR_STATES &&
				(psElseStart = FindElse(psState, psInst->psBLink)) != NULL
		   )
		{
			IMG_UINT32 uIdx;
			PPRECOVR_STATE psNextPState = &psContext->psPrecovrState[psContext->uNumPrecovrStates++];

			psState->uFlags &= ~USC_FLAGS_INTEGERINLASTBLOCKONLY;
			
			/* Copy the state */
#ifdef DEBUG
			{
				IMG_UINT32 i;
				for (i = 0; i < psPState->uNumPackedRegisters; i ++)
				{
					ASSERT(psNextPState->apsPackedRegisters[i] == NULL);
				}
			}
#endif /* def DEBUG */
			psNextPState->uNumPackedRegisters = psPState->uNumPackedRegisters;
			for (uIdx = 0; uIdx < psNextPState->uNumPackedRegisters; uIdx ++)
			{
				if (psPState->apsPackedRegisters[uIdx] != NULL)
				{
					psNextPState->apsPackedRegisters[uIdx] = UscAlloc(psState,
																	  sizeof(PACKED_OUTPUT));
					*psNextPState->apsPackedRegisters[uIdx] =
						*psPState->apsPackedRegisters[uIdx];
				}
			}


			DoIntegerConversion(psState, 
								psContext,
								psProg, 
								psElseStart, 
								psNextPState);
			continue;
		}
		/*
			Stop if we reach flow control (since data dependencies are more complicated).
		*/
		if (IsInputInstFlowControl(psInst))
		{
			break;
		}
		for (uDest = g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest > 0; uDest--)
		{
			PUF_REGISTER psDest = (uDest == 2) ? &psInst->sDest2 : &psInst->sDest;
			apsOutput[uDest - 1] = NULL;
			if (psDest->u.byMask == 0) continue; //next dest
			if (psDest->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
			{
				bStopScanning = IMG_TRUE;
				bWantToConvert = IMG_FALSE;
				break;
			}
			/*
				Check if the result of this instruction is wanted in packed format.
			*/
			FindPackedRegister(psState, psPState, psDest, &apsOutput[uDest - 1]);
			if (apsOutput[uDest - 1]) bWantToConvert = IMG_TRUE;
		}
		if (bWantToConvert)
		{
			/*
				Try to convert the instruction to an integer
				calculation. If we can't then insert instructions to pack
				immediately afterwards.
			*/
			if (!ConvertInstructionToInteger(psState,
											 psPState,
											 psInst,
											 apsOutput))
			{
				//add pack for every desired destination operand.
				for (uDest = g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest > 0; uDest--)
				{
					PPACKED_OUTPUT psOutput = apsOutput[uDest-1];
					if (psOutput)
					{
						IMG_UINT32 uMask = psOutput->uMask &
							((uDest == 2) ? psInst->sDest2.u.byMask : psInst->sDest.u.byMask);
						AddPackToInteger(psState, psOutput, uMask, NULL, psInst); 
					}
				}
			}
		}
		//free up any PACKED_OUTPUT structures we pulled out of the wanted list because
		//they were written by this instruction
		for (uDest = g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest > 0; uDest--)
		{
			if (apsOutput[uDest - 1])
			{
				UscFree(psState, apsOutput[uDest - 1]);
			}
		}
	}

	/*
		Pack any remaining registers to integer.
	*/
	if (psInst == NULL) //loop ended because we hit end of program
	{
		if (psState->sShader.psPS->uEmitsPresent == 0)
		{
			ASSERT (psPState->uNumPackedRegisters == 1);
		}
	}
	else //broke out of loop because we hit flow control or UFREG_RELATIVEINDEX_<something>
	{
		for (uReg = 0; uReg < psPState->uNumPackedRegisters; uReg++)
		{
			PPACKED_OUTPUT	psOutput = psPState->apsPackedRegisters[uReg];
			
			AddPackToInteger(psState, 
							 psOutput,
							 psOutput->uMask,
							 #ifdef SRC_DEBUG
							 psOutput->psInst,
							 #else /* SRC_DEBUG */
							 NULL,
							 #endif /* SRC_DEBUG */
							 psInst);
		}
	}
}

static
IMG_VOID FreePrecovrState(PINTERMEDIATE_STATE psState,
						  PPRECOVR_STATE psData)
/*****************************************************************************
 FUNCTION	: FreePrecovrState

 PURPOSE	: Release memory used for precover state. 

 PARAMETERS	: psState			- Compiler state..
			  psData			- Structure to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uCtr, uIdx;
	IMG_UINT32 uNumOutputs = USC_MAX_PACKEDOUTPUTS;

	if (psData == NULL)
		return;

	/* Release array elements */
	for (uIdx = 0; uIdx < USC_NUM_PRECOVR_STATES; uIdx++)
	{
		for (uCtr = 0; uCtr < uNumOutputs; uCtr++)
		{
			UscFree(psState, psData[uIdx].apsPackedRegisters[uCtr]);
		}
	}

	/* Release structure */
	UscFree(psState, psData);

}

static IMG_BOOL IsBlockStart(UF_OPCODE eInstType)
/*****************************************************************************
 FUNCTION	: IsBlockStart

 PURPOSE	: Test whether an instruction starts a logical block.

 PARAMETERS	: eInstType			- The type of the (uniflex) instruction.

 RETURNS	: IMG_TRUE if the instruction starts a logical block, IMG_FALSE otherwise.
*****************************************************************************/
{
	if (eInstType == UFOP_IF || eInstType == UFOP_IFC || eInstType == UFOP_IFNZBIT ||
		eInstType == UFOP_LOOP || eInstType == UFOP_REP ||
		eInstType == UFOP_IFP || eInstType == UFOP_GLSLLOOP ||
		eInstType == UFOP_GLSLSTATICLOOP ||
		eInstType == UFOP_LABEL || eInstType == UFOP_SWITCH)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static IMG_BOOL IsBlockEnd(UF_OPCODE eInstType)
/*****************************************************************************
 FUNCTION	: IsBlockEnd

 PURPOSE	: Test whether an instruction ends a logical block.

 PARAMETERS	: eInstType			- The type of the (uniflex) instruction.

 RETURNS	: IMG_TRUE if the instruction ends a logical block, IMG_FALSE otherwise.
*****************************************************************************/
{
	if (eInstType == UFOP_ENDIF || eInstType == UFOP_ENDLOOP || eInstType == UFOP_ENDREP ||
	    eInstType == UFOP_RET || eInstType == UFOP_GLSLENDLOOP || eInstType == UFOP_ENDSWITCH)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

IMG_INTERNAL //also used by verif_input...
PUNIFLEX_INST SkipInstBlock(PUNIFLEX_INST psFirstInst,
										UF_OPCODE eTerminate0,
										UF_OPCODE eTerminate1,
										UF_OPCODE eTerminate2)
/*****************************************************************************
 FUNCTION	: SkipInstBlock

 PURPOSE	: Skips a block of instructions

 PARAMETERS	: psFirstInst		- Pointer to the first instruction in the block
			  eTerminate0, eTerminate1, eTerminate2 - Instruction types marking the end of the block

 RETURNS	: The first instruction of type eTerminate0 or eTerminate1
			  at the same nesting level as the first instruction NULL
			  if no such instructions were found.

 NOTES		: Assumes that blocks are well formed.
*****************************************************************************/
{
	IMG_UINT32 uNesting = 0;
	PUNIFLEX_INST psCurr = psFirstInst;

	while(psCurr != NULL)
	{
		/* Test for end of block at top level */
		if (psCurr->eOpCode == eTerminate0 ||
			psCurr->eOpCode == eTerminate1 ||
			psCurr->eOpCode == eTerminate2)
		{
			if (uNesting == 0)
			{
				return psCurr;
			}
		}
		else
		{ 
			/* Test for start of block */
			if (IsBlockStart(psCurr->eOpCode))
			{
				uNesting ++;
			}
			/* Test for end of block */
			if (IsBlockEnd(psCurr->eOpCode))
			{
				uNesting --;
			}
		}
		psCurr = psCurr->psILink;
	}

	return psCurr;
}

IMG_INTERNAL 
IMG_VOID ConvertOperationsToInteger(PINTERMEDIATE_STATE		psState, 
									PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ConvertOperationsToInteger

 PURPOSE	: Try converting the tail of the input program to integer instructions.

 PARAMETERS	: psState			- Compiler state..
			  psProg			- Input program.
			  uInstCount		- Count of instructions.
			  psLastBlock		- Block to insert integer instructions into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PPRECOVR_STATE		psPState;
	IMG_UINT32			uOutputWrittenMask;
	IMG_UINT32			uIdx;
	PUNIFLEX_INST		psInst;
	PUNIFLEX_INST		psRetInst;
	PUNIFLEX_INST		psMainStart;
	IMG_UINT32			uNesting;
	PRECOVR_CONTEXT		sContext;
	PPIXELSHADER_STATE	psPS;
	IMG_UINT32			uPixelShaderOutputTempNum;

	/*
		Get the pixel shader specific state.
	*/
	ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);
	psPS = psState->sShader.psPS;

	/*
		Ordinarily we will insert integer instructions only at the end of the
		program.
	*/
	psState->uFlags |= USC_FLAGS_INTEGERINLASTBLOCKONLY;

	/*
		Allocate a temporary to hold the pixel shader output converted to U8 format.
	*/
	psPS->uPackedPSOutputTempRegNum = psState->uInputTempRegisterCount++;

	/*
		If the program does dynamic indexing into the pixel shader output then don't try to optimize.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING) != 0)
	{
		return;
	}

	/*
		Allocate a temporary to replace the pixel shader output.
	*/
	psPS->ePSOutputRegType = UFREG_TYPE_TEMP;
	psPS->uPSOutputRegNum = uPixelShaderOutputTempNum = psState->uInputTempRegisterCount++;
	psPS->uPSOutputRegArrayTag = USC_UNDEF;

	/*
		Get the format of the pixel shader output.
	*/
	uOutputWrittenMask = 0;
	for (psInst = psProg->psHead; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32 i;
		for (i = g_asInputInstDesc[psInst->eOpCode].uNumDests; i > 0; i--)
		{
			PUF_REGISTER psDest = (i == 2) ? &(psInst->sDest2) : &(psInst->sDest);
			if (psDest->u.byMask && psDest->eType == UFREG_TYPE_PSOUTPUT && psDest->uNum == UFREG_OUTPUT_MC0)
			{
				psDest->eType = UFREG_TYPE_TEMP;
				psDest->uNum = uPixelShaderOutputTempNum;
			}
		}
		for (i = 0; i < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; i++)
		{
			PUF_REGISTER	psSrc = &psInst->asSrc[i];

			if (psSrc->eType == UFREG_TYPE_PSOUTPUT && psSrc->uNum == UFREG_OUTPUT_MC0)
			{
				psSrc->eType = UFREG_TYPE_TEMP;
				psSrc->uNum = uPixelShaderOutputTempNum;
			}
		}
	}
	
	sContext.uNumPrecovrStates = 1;

	/* Allocate precover array */
	sContext.psPrecovrState = UscAlloc(psState, 
									   sizeof(PRECOVR_STATE) * USC_NUM_PRECOVR_STATES);
	for (uIdx = 0 ; uIdx < USC_NUM_PRECOVR_STATES; uIdx ++)
	{
		memset(sContext.psPrecovrState[uIdx].apsPackedRegisters, 0, 
			   sizeof(sContext.psPrecovrState[uIdx].apsPackedRegisters));
	}

	/* Initialise the array */
	psPState = &sContext.psPrecovrState[0];

	psPState->uNumPackedRegisters = 1;
	if (psPState->apsPackedRegisters[0] == NULL)
		psPState->apsPackedRegisters[0] = UscAlloc(psState, sizeof(PACKED_OUTPUT));

	psPState->apsPackedRegisters[0]->eType = UFREG_TYPE_TEMP;
	psPState->apsPackedRegisters[0]->uUnpackedNum = uPixelShaderOutputTempNum;
	psPState->apsPackedRegisters[0]->uPackedNum = psPS->uPackedPSOutputTempRegNum;
	psPState->apsPackedRegisters[0]->uMask = psPS->uInputOrderPackedPSOutputMask;
	psPState->apsPackedRegisters[0]->uPrecision = 0;
	psPState->apsPackedRegisters[0]->eFormat = psPS->aeColourResultFormat[0];
	#ifdef SRC_DEBUG
	psPState->apsPackedRegisters[0]->psInst = IMG_NULL;
	#endif /* SRC_DEBUG */

	/*
		Find the label which starts the main function.
	*/
	psMainStart = LabelToInst(psProg->psHead, USC_MAIN_LABEL_NUM);

	/*
		Find the last instruction in the main function.
	*/
	psInst = psMainStart->psILink;
	uNesting = 0;
	for (;;)
	{
		/*
			Found either the end of the instruction list or the start of another
			function before the end of main. 
		*/
		if (psInst == NULL || psInst->eOpCode == UFOP_LABEL)
		{
			USC_ERROR(UF_ERR_INVALID_PROG_STRUCT, "Invalid program structure");
		}
		if (psInst->eOpCode == UFOP_RET)
		{
			/*
				If we found a RET nested inside flow control then the main function has
				multiple exit points. For simplicity don't try and optimize - we'll just
				add the conversion from F32 -> U8 at the end of main.
			*/
			if (uNesting > 0)
			{
				psState->uFlags |= USC_FLAGS_MULTIPLERETSFROMMAIN;
				FreePrecovrState(psState, sContext.psPrecovrState);
				return;
			}
			/*
				An unconditional RET is the end of the main function.
			*/
			break;
		}
		/* Test for start of block */
		if (IsBlockStart(psInst->eOpCode))
		{
			uNesting++;
		}
		/* Test for end of block */
		if (IsBlockEnd(psInst->eOpCode))
		{
			uNesting--;
		}
		psInst = psInst->psILink;
	}

	/*
		Save the last instruction in the main function.
	*/
	psRetInst = psInst;
	
	/*
		Look for opportunities to use 8-bit precision.
	*/
	DoIntegerConversion(psState, &sContext, psProg, psRetInst, psPState);

	/*
		Try to convert some instructions at the start of the program.
	*/
	ConvertPrologOperations(psState, psProg);

	/* Release the allocated memory */
	FreePrecovrState(psState, sContext.psPrecovrState);
}

typedef struct
{
	IMG_UINT32			uWrittenMask;
	IMG_UINT32			uF16RegNum;

	/*
		Instruction for which conversion will occur. 
	*/
	#ifdef SRC_DEBUG
	PUNIFLEX_INST		psInst;
	#endif /* SRC_DEBUG */

} F16OVERRIDE_REGISTER, *PF16OVERRIDE_REGISTER;

typedef struct
{
	PUNIFLEX_INST			psMainStart;
	IMG_BOOL				bChangedPSOutput;
	PF16OVERRIDE_REGISTER	psRegisterMap;
	IMG_UINT32				uInitialInputTempRegisterCount;
} F16OVERRIDE_STATE, *PF16OVERRIDE_STATE;

static IMG_VOID ConvertInputInstF16ToF32(PUNIFLEX_INST			psInst, 
										 IMG_PBOOL				pbChangedPSOutput, 
										 IMG_BOOL				bConvertTemps)
/*****************************************************************************
 FUNCTION	: ConvertInputInstF16ToF32

 PURPOSE	: 

 PARAMETERS	: psInst		- The instruction to convert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
	{
	case 2:
		if (psInst->sDest2.u.byMask)
		{
			if (psInst->sDest2.eFormat == UF_REGFORMAT_F16) //a DX10-style vector SINCOS...which isn't yet implemented!
			{
				if (psInst->sDest2.eType == UFREG_TYPE_PSOUTPUT)
				{
					*pbChangedPSOutput = IMG_TRUE;
				}
				psInst->sDest2.eFormat = UF_REGFORMAT_F32;
				// ASSERT (psInst->sDest.u.byMask == 0 || psInst->sDest.eFormat == UF_REGFORMAT_F16);
				/* change first dest too - if sDest.u.byMask==0 it'll never be read. */
				psInst->sDest.eFormat = UF_REGFORMAT_F32;
			}
			break;
		}
		//else ASSERT (psInst->sDest.u.byMask);
	case 1:
		if (psInst->sDest.eFormat == UF_REGFORMAT_F16)
		{
			if (psInst->sDest.eType == UFREG_TYPE_PSOUTPUT)
			{
				*pbChangedPSOutput = IMG_TRUE;
			}
			psInst->sDest.eFormat = UF_REGFORMAT_F32;
		}
	default: ;
	}//end switch
	for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
	{
		if (psInst->asSrc[uArg].eFormat != UF_REGFORMAT_F16)
		{
			continue;
		}
		if (
				psInst->asSrc[uArg].eType == UFREG_TYPE_VSINPUT ||
				psInst->asSrc[uArg].eType == UFREG_TYPE_VSOUTPUT ||
				psInst->asSrc[uArg].eType == UFREG_TYPE_PSOUTPUT ||
				(
					bConvertTemps && 
					psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP
				)
		   )
		{
			if (psInst->asSrc[uArg].eType == UFREG_TYPE_PSOUTPUT)
			{
				*pbChangedPSOutput = IMG_TRUE;
			}
			psInst->asSrc[uArg].eFormat = UF_REGFORMAT_F32;
		}
	}
}

static IMG_UINT32 GetHwInstCountForF16Instruction(PUNIFLEX_INST psInst)
/*****************************************************************************
 FUNCTION	: GetHwInstCountForF16Instruction

 PURPOSE	: Get an estimated instruction count for a MOV/MUL/ADD/MAD instruction
			  if it were done at F16 precision.

 PARAMETERS	: psInst		- The instruction to check.

 RETURNS	: The instruction count.
*****************************************************************************/
{
	IMG_UINT32	uHalf, uHwInstCount;
	IMG_BOOL	bMad;

	bMad = IMG_FALSE;
	if (psInst->eOpCode == UFOP_MUL ||
		psInst->eOpCode == UFOP_ADD ||
		psInst->eOpCode == UFOP_MAD)
	{
		bMad = IMG_TRUE;
	}

	uHwInstCount = 0;
	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		IMG_UINT32	uHalfMask = (psInst->sDest.u.byMask >> (uHalf * 2)) & 3;
		IMG_UINT32	uArg;

		if (uHalfMask == 0)
		{
			continue;
		}

		/*
			One instruction to do the calculation.
		*/
		if (bMad)
		{
			uHwInstCount++;
		}

		for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
		{
			IMG_UINT32	uSwizChan0, uSwizChan1;

			uSwizChan0 = EXTRACT_CHAN(psInst->asSrc[uArg].u.uSwiz, (uHalf * 2) + 0);
			uSwizChan1 = EXTRACT_CHAN(psInst->asSrc[uArg].u.uSwiz, (uHalf * 2) + 1);

			if (uHalfMask == 1 || uHalfMask == 2)
			{
				IMG_UINT32	uSwizChan;

				uSwizChan = (uHalfMask == 1) ? uSwizChan0 : uSwizChan1;

				/*
					One instruction for a PCK if the instruction uses a constant not available in
					F16 format or uses the opposite half of the source register.
				*/
				if (uSwizChan == UFREG_SWIZ_2 ||
					uSwizChan == UFREG_SWIZ_HALF ||
					(uHalfMask == 1 && (uSwizChan % 2) != 0) ||
					(uHalfMask == 2 && (uSwizChan % 2) != 1))
				{
					uHwInstCount++;
				}
			}
			else
			{
				if (uSwizChan0 > UFREG_SWIZ_A || uSwizChan1 > UFREG_SWIZ_A)
				{
					/*
						One instruction for a PCK if each channel selects a different constant or a 
						constant is used not available in F16 format.
					*/
					if (
							!(
								(uSwizChan0 == UFREG_SWIZ_0 && uSwizChan1 == UFREG_SWIZ_0) ||
								(uSwizChan0 == UFREG_SWIZ_1 && uSwizChan1 == UFREG_SWIZ_1)
							 )
					   )
					{
						uHwInstCount++;
					}
				}
				else if ((uSwizChan0 % 2) != 0 || (uSwizChan1 % 2) != 1)
				{
					/*
						One instruction for a PCK to do a swizzle.
					*/
					uHwInstCount++;
				}
			}
		}
	}

	return uHwInstCount;
}

static IMG_VOID ChangeTemporaryRegisterFormats(PF16OVERRIDE_STATE	psOState,
											   PUNIFLEX_INST		psInst)
/*****************************************************************************
 FUNCTION	: ChangeTemporaryRegisterFormats

 PURPOSE	: Update the register formats for temporary register we have
			  converted to F32.

 PARAMETERS	: psOState		- Per-stage state.
			  psInst		- The instruction to convert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
	{
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP && 
			psInst->asSrc[uArg].eFormat == UF_REGFORMAT_F16)
		{
			if (psOState->psRegisterMap[psInst->asSrc[uArg].uNum].uF16RegNum == UINT_MAX)
			{
				psInst->asSrc[uArg].eFormat = UF_REGFORMAT_F32;
			}
			else
			{
				psInst->asSrc[uArg].uNum = psOState->psRegisterMap[psInst->asSrc[uArg].uNum].uF16RegNum;
			}
		}
	}
}

static IMG_BOOL CheckInstConversion(PINTERMEDIATE_STATE	psState,
									PF16OVERRIDE_STATE	psOState,
									PUNIFLEX_INST		psInst)
/*****************************************************************************
 FUNCTION	: CheckInstConversion

 PURPOSE	: Check if we should convert an F16 precision instruction to F32.

 PARAMETERS	: psState		- Compiler state.
			  psOState		- Per-stage state.
			  psInst		- The instruction to convert.

 RETURNS	: TRUE if the instruction should be converted to F32.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	switch (psInst->sDest.eType)
	{
		case UFREG_TYPE_TEMP:
		{
			PF16OVERRIDE_REGISTER	psReg = &psOState->psRegisterMap[psInst->sDest.uNum];

			/*
				If this register is partially or conditionally written and we already converted a
				previous write then convert this instruction too.
			*/
			if (psInst->uPredicate != UF_PRED_NONE || (psReg->uWrittenMask & ~(*((IMG_UINT32*)(&psInst->sDest.u.byMask)))) != 0)
			{
				if (psReg->uF16RegNum == UINT_MAX)
				{
					return IMG_TRUE;
				}
			}
			break;
		}
		case UFREG_TYPE_VSOUTPUT:
		{
			/*
				Vertex shader outputs always have to be F32 format so always convert instructions
				writting them.
			*/
			return IMG_TRUE;
		}
		case UFREG_TYPE_PSOUTPUT:
		{
			if ((psOState->bChangedPSOutput) || psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
			{
				return IMG_TRUE;
			}
			break;
		}
		case UFREG_TYPE_PREDICATE:
		{
			break;
		}
		default:
		{
			break;
		}
	}

	/*
		For the texture load instructions the format of the texture coordinates 
		and the format of the destination are independent.
	*/
	if (psInst->eOpCode == UFOP_LD ||
		psInst->eOpCode == UFOP_LDP ||
		psInst->eOpCode == UFOP_LDPIFTC ||
		psInst->eOpCode == UFOP_LDL ||
		psInst->eOpCode == UFOP_LDB ||
		psInst->eOpCode == UFOP_LDD)
	{
		return IMG_FALSE;
	}

	/*
		...otherwise convert the instruction if it has any F32 sources.
	*/
	for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
	{
		/*
			Check if this instruction uses any source registers we
			have already converted to F32.
		*/
		if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
		{
			if (psInst->asSrc[uArg].eFormat == UF_REGFORMAT_F32)
			{
				return IMG_TRUE;
			}
		}
		/*
			Check if this instruction uses any source registers which
			are always in F32 format.
		*/
		else if (psInst->asSrc[uArg].eType == UFREG_TYPE_VSINPUT ||
				 psInst->asSrc[uArg].eType == UFREG_TYPE_VSOUTPUT)
		{
			return IMG_TRUE;
		}
	}

	switch (psInst->eOpCode)
	{
		case UFOP_MOV:
		{
			IMG_UINT32	uHwInstCount;

			if (psInst->asSrc[0].eFormat == UF_REGFORMAT_F32)
			{
				/*
					Always convert a conversion instruction.
				*/
				return IMG_TRUE;
			}
			else
			{
				/*
					Otherwise check for a move with a swizzle. We won't be able to move the
					swizzle onto instructions which use the move result so convert to
					F32.
				*/
				uHwInstCount = GetHwInstCountForF16Instruction(psInst);
				if (uHwInstCount > 0)
				{
					return IMG_TRUE;
				}
				return IMG_FALSE;
			}
		}
		case UFOP_ADD:
		case UFOP_MUL:
		case UFOP_MAD:
		{
			IMG_UINT32	uHwInstCount;

			/*
				Count the number of hardware instructions required to implement
				this input instruction (including for swizzling).
			*/
			uHwInstCount = GetHwInstCountForF16Instruction(psInst);

			/*
				If more instructions are required than the equivalent F32 instructions
				then convert.
			*/
			if (uHwInstCount > g_auSetBitCount[psInst->sDest.u.byMask])
			{
				return IMG_TRUE;
			}
			else if (uHwInstCount == g_auSetBitCount[psInst->sDest.u.byMask])
			{
				if (psInst->sDest.eType == UFREG_TYPE_TEMP && psOState->psRegisterMap[psInst->sDest.uNum].uWrittenMask == 0)
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_LOG:
		case UFOP_EXP:
		{
			/*
				For complex instructions we effectively have swizzles on the sources,
			*/
			return IMG_FALSE;
		}
		case UFOP_NRM:
		{
			/*
				If a NRM instruction hasn't been macro expanded then it is going to be
				implemented using the hardware's normalise units which has unlimited
				swizzles.
			*/
			return IMG_FALSE;
		}
		default:
		{
			/*
				For any other instruction we would have to implement it using F32 instructions
				anyway so convert it.
			*/
			return IMG_TRUE;
		}
	}
}

static IMG_VOID InsertF16ToF32Convert(PINTERMEDIATE_STATE		psState,
									  IMG_UINT32				uDestNum,
									  IMG_UINT32				uSrcNum,
									  PUNIFLEX_INST				psInsertBefore,
									  PUNIFLEX_INST	            psConvInst)
/*****************************************************************************
 FUNCTION	: InsertF16ToF32Convert

 PURPOSE	: Add a new input instruction to convert from F16 to F32.

 PARAMETERS	: psState		- Compiler state.
			  uDestNum		- Temporary register number which is the destination
							for the converted value.
			  uSrcNum		- Temporary register number which is the source for
							the converted value.
			  psInsertBefore
							- The new instruction will be inserted immediately
							before this instruction.
			  psConvInst    - Instruction for which conversion will occur.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psMove;

    #if!defined(SRC_DEBUG)
	PVR_UNREFERENCED_PARAMETER(psConvInst);
	#endif /* SRC_DEBUG */

	psMove = (PUNIFLEX_INST)UscAlloc(psState, sizeof(UNIFLEX_INST));

	#ifdef SRC_DEBUG
	psMove->uSrcLine = psConvInst->uSrcLine;
	#endif /* SRC_DEBUG */

	psMove->eOpCode = UFOP_MOV;
	psMove->uPredicate = UF_PRED_NONE;
	/*
		Set up the destination.
	*/
	psMove->sDest.eType = UFREG_TYPE_TEMP;
	psMove->sDest.uNum = uDestNum;
	psMove->sDest.eFormat = UF_REGFORMAT_F32;
	psMove->sDest.u.byMask = UFREG_DMASK_FULL;
	psMove->sDest.byMod = UFREG_DMOD_SATNONE;
	psMove->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psMove->sDest.uArrayTag = 0;
	/*
		Set up the source.
	*/
	psMove->asSrc[0].eType = UFREG_TYPE_TEMP;
	psMove->asSrc[0].uNum = uSrcNum;
	psMove->asSrc[0].eFormat = UF_REGFORMAT_F16;
	psMove->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
	psMove->asSrc[0].byMod = UFREG_SMOD_NONE;
	psMove->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psMove->asSrc[0].uArrayTag = 0;
	psMove->asSrc[0].u1.uRelativeStrideInComponents = 4;

	/*
		Insert the instruction into the linked list.
	*/
	if (psInsertBefore->psBLink != NULL)
	{
		psInsertBefore->psBLink->psILink = psMove;
	}
	
	psMove->psBLink = psInsertBefore->psBLink;
	psMove->psILink = psInsertBefore;

	psInsertBefore->psBLink = psMove;
}

static IMG_VOID ConvertRegisterPrecision(PINTERMEDIATE_STATE	psState,
										 PF16OVERRIDE_STATE		psOState,
										 PUNIFLEX_INST			psLast,
										 IMG_UINT32				uFrom,
										 IMG_UINT32				uTo)
/*****************************************************************************
 FUNCTION	: ConvertRegisterPrecision

 PURPOSE	: Convert previous references to a register in F16 format to F32
			  format.

 PARAMETERS	: psState		- Compiler state.
			  psOState		- Module state.
			  psLast		- The starting point for the conversion.
			  uFrom			- F16 register number.
			  uTo			- F32 register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	for (psInst = psLast; psInst != NULL; psInst = psInst->psBLink)
	{
		IMG_UINT32	uArg;
		IMG_BOOL	bConvertedSource;

		bConvertedSource = IMG_FALSE;
		for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
		{
			if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP &&
				psInst->asSrc[uArg].uNum == uFrom)
			{
				ASSERT(psInst->asSrc[uArg].eFormat == UF_REGFORMAT_F16);
				psInst->asSrc[uArg].uNum = uTo;
				psInst->asSrc[uArg].eFormat = UF_REGFORMAT_F32;
				bConvertedSource = IMG_TRUE;
			}
		}
		switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
		{
		default:
			ASSERT (g_asInputInstDesc[psInst->eOpCode].uNumDests == 0);
			break;
		case 2:
			if (psInst->sDest2.u.byMask)
			{
				if (psInst->sDest2.eType == UFREG_TYPE_TEMP && psInst->sDest2.uNum == uFrom)
				{
					ASSERT(psInst->sDest2.eFormat == UF_REGFORMAT_F16);
					psInst->sDest2.uNum = uTo;
					psInst->sDest2.eFormat = UF_REGFORMAT_F32;
				}
				if (psInst->sDest.u.byMask == 0) break;
			}
			else ASSERT (psInst->sDest.u.byMask);
			//and fall-through
		case 1:
			if (psInst->sDest.eType == UFREG_TYPE_TEMP &&
				psInst->sDest.uNum == uFrom)
			{
				ASSERT(psInst->sDest.eFormat == UF_REGFORMAT_F16);
				psInst->sDest.uNum = uTo;
				psInst->sDest.eFormat = UF_REGFORMAT_F32;
			}
		}

		/*
			If this was an instruction we left at F16 precision and it now has
			an F32 source then convert the destination register to F32 too.
		*/
		if (bConvertedSource && 
			g_asInputInstDesc[psInst->eOpCode].uNumDests > 0 &&
			psInst->sDest.eFormat == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uOldRegNum;
			IMG_UINT32	uNewRegNum;
			IMG_UINT32	uRegIdx;

			ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests == 1);

			/*
				Look for the old register number for the destination.
			*/
			uOldRegNum = psInst->sDest.uNum;
			for (uRegIdx = 0; uRegIdx < psOState->uInitialInputTempRegisterCount; uRegIdx++)
			{
				if (psOState->psRegisterMap[uRegIdx].uF16RegNum == uOldRegNum)
				{
					break;
				}
			}

			if (uRegIdx < psOState->uInitialInputTempRegisterCount)
			{
				uNewRegNum = uRegIdx;

				/*
					Convert uses of the register to F32 format.
				*/
				ConvertRegisterPrecision(psState, psOState, psLast, uOldRegNum, uNewRegNum);

				/*
					Clear the corresponding entry in the register map.
				*/
				psOState->psRegisterMap[uNewRegNum].uF16RegNum = UINT_MAX;
			}
			else
			{
				/*
					Convert uses of the register to F32 format.
				*/
				ConvertRegisterPrecision(psState, psOState, psLast, uOldRegNum, uOldRegNum);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID OverrideFloat16Precisions(PINTERMEDIATE_STATE	psState,
								   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: OverrideFloat16Precisions

 PURPOSE	: Change calculations using F16 precision to F32 precision.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- Input program.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST		psInst;
	PUNIFLEX_INST		psMainStart;
	PUNIFLEX_INST		psMainEnd;
	F16OVERRIDE_STATE	sOState;
	IMG_UINT32			uInputTempRegisterCount;
	IMG_UINT32			uArg, uDest;
	IMG_UINT32			uReg;

	/*
		Allocate an array to hold information about each register.
	*/
	uInputTempRegisterCount = psState->uInputTempRegisterCount;
	sOState.psRegisterMap = UscAlloc(psState, uInputTempRegisterCount * sizeof(sOState.psRegisterMap[0]));
	for (uReg = 0; uReg < uInputTempRegisterCount; uReg++)
	{
		sOState.psRegisterMap[uReg].uWrittenMask = 0;
		sOState.psRegisterMap[uReg].uF16RegNum = UINT_MAX;

		#ifdef SRC_DEBUG
		sOState.psRegisterMap[uReg].psInst = IMG_NULL;
		#endif /* SRC_DEBUG */
	}
	sOState.uInitialInputTempRegisterCount = uInputTempRegisterCount;

	/*
		Convert all F16 indexable temporary arrays to F32.
	*/
	for (psInst = psProg->psHead; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32 i;
		for (i = g_asInputInstDesc[psInst->eOpCode].uNumDests; i > 0; i--)
		{
			PUF_REGISTER psDest = (i == 2) ? &psInst->sDest2 : &psInst->sDest;
			if (psDest->u.byMask &&
				psDest->eType == UFREG_TYPE_INDEXABLETEMP &&
				psDest->eFormat == UF_REGFORMAT_F16)
			{
				psDest->eFormat = UF_REGFORMAT_F32;
			}
		}
		for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
		{
			if (psInst->asSrc[uArg].eType == UFREG_TYPE_INDEXABLETEMP &&
				psInst->asSrc[uArg].eFormat == UF_REGFORMAT_F16)
			{
				psInst->asSrc[uArg].eFormat = UF_REGFORMAT_F32;
			}
		}
	}
	/*
		If the program uses dynamic indexing into the pixel shader output then update the format
		of the output too (since it is treated as an indexable temporary array).
	*/
	if ((psState->uFlags2 & USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING) != 0)
	{
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
		IMG_UINT32			uSurf;

		for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
		{
			if (psPS->aeColourResultFormat[uSurf] == UF_REGFORMAT_F16)
			{
				psPS->aeColourResultFormat[uSurf] = UF_REGFORMAT_F32;
			}
		}
	}

	/*
		Find the first instruction in the main function.
	*/
	sOState.psMainStart = psMainStart = LabelToInst(psProg->psHead, USC_MAIN_LABEL_NUM);

	/*
		Look for a run of instructions at the start of the main function which
		would be better done as F16.
	*/
	sOState.bChangedPSOutput = IMG_FALSE;
	psMainEnd = NULL;
	for (psInst = psMainStart; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_BOOL bConvert;
		/*
			Stop as soon as we reach flow control.
		*/
		if (IsInputInstFlowControl(psInst))
		{
			psMainEnd = psInst;
			break;
		}

		/*
			Update any F16 arguments to the instruction by either changing them to F32 or renumbering them
		*/
		ChangeTemporaryRegisterFormats(&sOState, psInst);

		//clear any remappings/state for F16 registers overwritten by this instruction in a different format.
		switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
		{
		default:
			ASSERT(g_asInputInstDesc[psInst->eOpCode].uNumDests == 0);
			/* no dests -> nothing to do, continue round loop */
			continue; 
		case 2:
			if (psInst->sDest2.u.byMask)
			{
				if (psInst->sDest2.eFormat != UF_REGFORMAT_F16)
				{
					if (psInst->sDest2.eType == UFREG_TYPE_TEMP)
					{
						/*
							Instruction for which conversion will occur. 
						*/
						#ifdef SRC_DEBUG
						sOState.psRegisterMap[psInst->sDest2.uNum].psInst = psInst;
						#endif /* SRC_DEBUG */
						sOState.psRegisterMap[psInst->sDest2.uNum].uF16RegNum = UINT_MAX;
						sOState.psRegisterMap[psInst->sDest2.uNum].uWrittenMask |= psInst->sDest2.u.byMask;
					}
					if (psInst->sDest.u.byMask)
					{
						ASSERT (psInst->sDest.eFormat != UF_REGFORMAT_F16);
						/* (will continue below after possibly clearly remappings for 1st dest) */
					}
					else continue; /* (round loop - instr is not F16, but should not fallthrough & examine invalid sDest.eFormat) */
				}
				else if (psInst->sDest.u.byMask==0) break; /* (out of switch - instr F16, so process below, but sDest.eFormat invalid) */
			}
			else ASSERT (psInst->sDest.u.byMask);
			//fall-through
		case 1:
			if (psInst->sDest.eFormat != UF_REGFORMAT_F16)
			{
				if (psInst->sDest.eType == UFREG_TYPE_TEMP && psInst->sDest.u.byMask != 0)
				{
					/*
						Instruction for which conversion will occur. 
					*/
					#ifdef SRC_DEBUG
					sOState.psRegisterMap[psInst->sDest.uNum].psInst = psInst;
					#endif /* SRC_DEBUG */

					sOState.psRegisterMap[psInst->sDest.uNum].uF16RegNum = UINT_MAX;
					sOState.psRegisterMap[psInst->sDest.uNum].uWrittenMask |= psInst->sDest.u.byMask;
				}
				continue; /* (instr not F16, so go round loop) */
			}
		}//end switch
		
		/* At this point, the instruction should definitely be F16: */
		ASSERT ((psInst->sDest.u.byMask && psInst->sDest.eFormat==UF_REGFORMAT_F16) ||
				(g_asInputInstDesc[psInst->eOpCode].uNumDests==2 && psInst->sDest2.u.byMask && psInst->sDest2.eFormat==UF_REGFORMAT_F16));

		bConvert = CheckInstConversion(psState, &sOState, psInst);
		if (bConvert)
		{
			ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_FALSE);
		}
		for (uDest = g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest > 0; uDest--)
		{
			PUF_REGISTER psDest = (uDest == 2) ? &psInst->sDest2 : &psInst->sDest;
			if (psDest->u.byMask && psDest->eType == UFREG_TYPE_TEMP)
			{
				if (bConvert)
				{
					/*
						Instruction for which conversion will occur. 
					*/
					#ifdef SRC_DEBUG
					sOState.psRegisterMap[psDest->uNum].psInst = psInst;
					#endif /* SRC_DEBUG */

					/*
						If previous instrs writing to this F16 dest, have been changed...
						(to write to uF16RegNum instead)
					*/
					if (sOState.psRegisterMap[psDest->uNum].uF16RegNum != UINT_MAX)
					{
						if (sOState.psRegisterMap[psDest->uNum].uWrittenMask != 0)
						{
							/*
								...then convert previous references to F32, and reinstate the original register number
								(strictly, need only make F32 if psInst is predicated or only partially overwrites, but never mind)
							*/
							ConvertRegisterPrecision(psState,
													 &sOState,
													 psInst, 
													 sOState.psRegisterMap[psDest->uNum].uF16RegNum,
													 psDest->uNum);
						}
					}

					sOState.psRegisterMap[psDest->uNum].uF16RegNum = UINT_MAX;
					sOState.psRegisterMap[psDest->uNum].uWrittenMask |= psDest->u.byMask;
				}
				else 
				{
					/*
						Instr is F16 but not converted/ible.
						Instead, change its destination to a fresh(/reused) temporary
					*/
					#ifdef SRC_DEBUG
					/*
						Instruction for which conversion will occur. 
					*/
					sOState.psRegisterMap[psDest->uNum].psInst = psInst;
					#endif /* SRC_DEBUG */

					sOState.psRegisterMap[psDest->uNum].uWrittenMask |= psDest->u.byMask;
					if (sOState.psRegisterMap[psDest->uNum].uF16RegNum == UINT_MAX)
					{
						sOState.psRegisterMap[psDest->uNum].uF16RegNum = psState->uInputTempRegisterCount++;
					}
					psDest->uNum = sOState.psRegisterMap[psDest->uNum].uF16RegNum;
				}
			}
		}
	}

	/*
		For all other instructions convert them from F16 to F32.
	*/
	for (psInst = psProg->psHead; psInst != psMainStart; psInst = psInst->psILink)
	{
		ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_TRUE);
	}
	for (psInst = psMainEnd; psInst != NULL; psInst = psInst->psILink)
	{
		ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_TRUE);
	}

	if (sOState.bChangedPSOutput)
	{
		IMG_UINT32			uSurf;
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		for (psInst = psMainStart; psInst != psMainEnd; psInst = psInst->psILink)
		{
			switch (g_asInputInstDesc[psInst->eOpCode].uNumDests)
			{
			default: ASSERT (g_asInputInstDesc[psInst->eOpCode].uNumDests == 0); break;
			case 2:
				if (psInst->sDest2.u.byMask)
				{
					if (psInst->sDest2.eType == UFREG_TYPE_PSOUTPUT)
					{
						ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_FALSE);
						break;
					}
					else if (psInst->sDest.u.byMask == 0) break;
				}
				else ASSERT (psInst->sDest.u.byMask);
				//...and fall-through
			case 1:
				if (psInst->sDest.eType == UFREG_TYPE_PSOUTPUT)
				{
					ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_FALSE);
				}
			}
			for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
			{
				if (psInst->asSrc[uArg].eType == UFREG_TYPE_PSOUTPUT)
				{
					ConvertInputInstF16ToF32(psInst, &sOState.bChangedPSOutput, IMG_FALSE);
				}
			}
		}

		/*
			Update the recorded formats of the registers holding the pixel shader outputs.
		*/
		for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
		{
			if (psPS->aeColourResultFormat[uSurf] == UF_REGFORMAT_F16)
			{
				psPS->aeColourResultFormat[uSurf] = UF_REGFORMAT_F32;
			}
		}
		if (psPS->eZFormat == UF_REGFORMAT_F16)
		{
			psPS->eZFormat = UF_REGFORMAT_F32;
		}
		if (psPS->eOMaskFormat == UF_REGFORMAT_F16)
		{
			psPS->eZFormat = UF_REGFORMAT_F32;
		}
	}
	
	/*
		If we stopped checking because we reached a flow control instruction then insert instructions
		to convert any F16 registers to F32 at that point since references to them afterwards will
		always be F32 format. 
	*/
	if (psMainEnd != NULL)
	{
		IMG_UINT32	uRegL;

		for (uRegL = 0; uRegL < sOState.uInitialInputTempRegisterCount; uRegL++)
		{
			if (sOState.psRegisterMap[uRegL].uF16RegNum != UINT_MAX)
			{	
				// PRQA S 3197 6
				PUNIFLEX_INST psSrcInst = IMG_NULL;

				#ifdef SRC_DEBUG
				psSrcInst = (sOState.psRegisterMap[uRegL].psInst);
				#endif /* SRC_DEBUG */

				/*
					Idea of remapping earlier was to avoid converting from F16 virtual to SAME-numbered F32 virtual.
					This is because the conversion to intermediate code uses a fixed mapping of input code registers to
					intermediate code registers, and hence a F16-to-F32 conversion from an input reg to itself results
					in the intermediate code using unnecessary temporaries and register moves which can't be eliminated by move elimination.
				*/
				InsertF16ToF32Convert(psState, uRegL, sOState.psRegisterMap[uRegL].uF16RegNum, psMainEnd, psSrcInst);
			}
		}
	}

	UscFree(psState, sOState.psRegisterMap);
}

/******************************************************************************
 End of file (precovr.c)
******************************************************************************/
