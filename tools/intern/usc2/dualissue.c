/*************************************************************************
 * Name         : dualissue.c
 * Title        : SGX545-specific dual-issue instructions
 * Created      : March 2007
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
 *
 * Modifications:-
 * $Log: dualissue.c $
 **************************************************************************/

#if defined(SUPPORT_SGX545)
/* Supported on: 545 */

#include "uscshrd.h"
#include "bitops.h"
#include "reggroup.h"
#include "usedef.h"

/* 
   USC_NUM_EFO_IREGISTERS: The number of internal registers that an efo can use/interfere with. 
*/
#define USC_NUM_EFO_IREGISTERS (2)

/*
	USC_NUM_DUALISSUE_IREGISTERS: The number of internal registers usable by dual-issue instructions.
*/
#define USC_NUM_DUALISSUE_IREGISTERS		(3)
#define USC_NUM_DUALISSUE_IREGISTERS_MASK	((1 << USC_NUM_DUALISSUE_IREGISTERS) - 1)

typedef struct _NONTEMP
{
	/*
		Details of the non-temporary register.
	*/
	IMG_UINT32		uRegType;
	IMG_UINT32		uRegNumber;
	/*
		List of possible sites where copy of the non-temporary data into an internal
		register could be dual-issued.
	*/
	USC_LIST		sMoveSiteList;
} NONTEMP, *PNONTEMP;

typedef enum _NONTEMP_MOVESITE_TYPE
{
	/*
		When expanding the instruction to a dual-issue primary operation a hardware constant
		would be added as a source.
	*/
	NONTEMP_MOVESITE_TYPE_IMPLICITCONST,
	/*
		One of the instruction's sources contains non-temporary data.
	*/
	NONTEMP_MOVESITE_TYPE_SHAREDSRC,
} NONTEMP_MOVESITE_TYPE, *PNONTEMP_MOVESITE_TYPE;

/*
	Information about a possible site where a move from some specific non-temporary data
	into an internal register could be dual-issued.
*/
typedef struct _NONTEMP_MOVESITE
{
	/*
		Entry in the list of sites associated with the specific non-temporary data.
	*/
	USC_LIST_ENTRY			sNonTempListEntry;
	/*
		Entry in the list of sites associated with the instruction.
	*/
	USC_LIST_ENTRY			sInstListEntry;
	/*
		Points back to the associated instruction.
	*/
	PINST					psInst;
	/*
		Points back to the associated non-temporary data.
	*/
	PNONTEMP				psNonTemp;
	/*
		Type of the move site.
	*/
	NONTEMP_MOVESITE_TYPE	eType;
	union
	{
		/*
			Number of the implicit constant which would be generated when converting the instruction to a 
			dual-issue primary. 
		*/
		IMG_UINT32				uConstNum;
		/*
			Number of the source which contains the non-temporary data.
		*/
		IMG_UINT32				uSrcIdx;
		IMG_UINT32				uSiteData;
	} u;
} NONTEMP_MOVESITE, *PNONTEMP_MOVESITE;

typedef enum _DUALISSUE_STAGEDATA_FLAGS
{
	/* Instruction could be converted to a dual-issue sum-of-squares primary operation. */
	DUALISSUE_STAGEDATA_FLAGS_SSQ,
	/* Instruction deschedules (before or after execution). */
	DUALISSUE_STAGEDATA_FLAGS_DESCHED,
	/* Instruction is an EFO with an unused unified store destination writeback. */
	DUALISSUE_STAGEDATA_FLAGS_EFO,
	/* Instruction could be converted to a MOV dual-issue primary operation. */
	DUALISSUE_STAGEDATA_FLAGS_PRIMARYMOVE,
	/* Instruction could be converted to a MOV dual-issue secondary operation. */
	DUALISSUE_STAGEDATA_FLAGS_SECONDARYMOVE,
	/* Instruction could be a dual-issue primary operation. */
	DUALISSUE_STAGEDATA_FLAGS_PRIMARYINST,
	/* Count of possible flags. */
	DUALISSUE_STAGEDATA_FLAGS_COUNT,
} DUALISSUE_STAGEDATA_FLAGS, *PDUALISSE_STAGEDATA_FLAGS;

typedef struct _DUALISSUE_STAGEDATA
{
	/* Entry in the list of instructions with allocated stage data. */
	USC_LIST_ENTRY	sStageDataListEntry;
	/* Points back to the associated instruction. */
	PINST			psInst;
	/* Properties of the instruction. */
	IMG_UINT32		auFlags[UINTS_TO_SPAN_BITS(DUALISSUE_STAGEDATA_FLAGS_COUNT)];
	/* Entry in the list of instructions which could be a primary SSQ instruction. */
	USC_LIST_ENTRY	sSSQListEntry;
	/* Entry in the list of descheduling instructions. */
	USC_LIST_ENTRY	sDeschedListEntry;
	/* Entry in the list of EFO instructions with an unused unified store destination writeback. */
	USC_LIST_ENTRY	sEFOWriteBackListEntry;
	/* Entry in the list of instructions which could be converted to a MOV dual-issue primary operation. */
	USC_LIST_ENTRY	sPrimaryMoveListEntry;
	/* Entry in the list of instructions which could be converted to a MOV dual-issue secondary operation. */
	USC_LIST_ENTRY	sSecondaryMoveListEntry;
	/* List of dual-issued move sites at the instruction. */
	USC_LIST		sInstMoveSiteList;
} DUALISSUE_STAGEDATA, *PDUALISSUE_STAGEDATA;

typedef struct _DISSUE_STATE
{
	/*
		Tree mapping from non-temporary data to a list of possible sites where a move from
		the non-temporary data into an internal register could be dual-issued. 
	*/
	PUSC_TREE	psNonTempTree;

	/*
		List of descheduling instructions.
	*/
	USC_LIST	sDeschedList;

	/*
		List of instructions with per-instruction dual issue stage data allocated.
	*/
	USC_LIST	sStageDataList;

	/*
		List of instructions which could be converted to a primary SSQ operation.
	*/
	USC_LIST	sSSQList;

	/*
		List of EFO instructions with an unused unified store writeback.
	*/
	USC_LIST	sEFOWriteBackList;

	/*
		List of possible instructions where a move from an internal register to a unified store
		register could be inserted.
	*/
	USC_LIST	sSecondaryMoveList;

	/*
		List of instructions which could be converted to a MOV dual-issue primary operation.
	*/
	USC_LIST	sPrimaryMoveList;

	/* Whether any block has been changed by a call to DualIssueBP*/
	IMG_BOOL bChanged;
} DISSUE_STATE, *PDISSUE_STATE;

/* USC_NUM_DUALISSUE_OPERANDS: Maximum number of operands in a dual issue instruction */
#define USC_NUM_DUALISSUE_OPERANDS (4)

/* USC_MAX_AVAIL_MOVE_LOCS: Number of entries in the array of possible ireg move locations. */
#define USC_MAX_AVAIL_MOVE_LOCS	   (4)

typedef enum _DI_MOVE_TYPE
{
	/* MOV which can be a primary operation. */
	DI_MOVE_TYPE_PRIMOV			= 0,
	/* MOV which can be a secondary operation. */
	DI_MOVE_TYPE_SECMOV			= 1,
	/* Primary operation which shares a source with the new MOV. */
	DI_MOVE_TYPE_SHAREDPRISRC	= 2,
	/* Use the write-back of an EFO. */
	DI_MOVE_TYPE_EFOWB			= 3,
	/* 
		Dual-issue the instruction with itself to write both its original destination
		and an internal register.
	*/
	DI_MOVE_TYPE_DUALWITHSELF	= 4,
} DI_MOVE_TYPE;

typedef struct _DI_MOVE_LOC
{
	/*
		Points to the instruction where the move can be inserted.
	*/
	PINST				psInst;
	/*
		Type of the move to do.
	*/
	DI_MOVE_TYPE		eType;
} DI_MOVE_LOC, *PDI_MOVE_LOC;

typedef struct _CONVERT_OPERAND
{
	/*
		Points to the argument in the secondary instruction which is to be
		replaced.
	*/
	ARG				sOperand;
	/* Internal register to replace with. */
	IMG_UINT32		uIReg;
	/* Mask of sources in the secondary to be replaced. */
	IMG_UINT32		uSrcMask;
	/* Possible move location for this argument. */
	DI_MOVE_LOC		sMoveLoc;
	/*
		TRUE if the last use of this internal register will be at
		the dual-issue site.
	*/
	IMG_BOOL		bLastUseAtSite;
} CONVERT_OPERAND, *PCONVERT_OPERAND;

typedef struct _DI_SOURCE
{
	/* Source register. */
	ARG			sArg;
	/* Component select. */
	IMG_UINT32	uComponent;
} DI_SOURCE, *PDI_SOURCE;

typedef struct _DUALISSUE_SECONDARY
{
	/*
		List of secondary operands to convert to
		internal registers.
	*/
	CONVERT_OPERAND	asConvert[USC_NUM_DUALISSUE_OPERANDS];
	IMG_UINT32		uConvertCount;

	/*
		Mask of operands which can be replaced everywhere
		by an internal register.
	*/
	IMG_UINT32		uReplace;

	/*
		Index into asConvert of the secondary source 0.
	*/
	IMG_UINT32		uSrc0Idx;

	/* 
		TRUE if the destination needs to be replaced by
		an internal register.
	*/
	IMG_BOOL		bConvertDest;

	/*
		The secondary instruction uses source 0.
	*/
	IMG_BOOL		bUsesSrc0;

	/*
		Index of the internal register to use for the
		destination.
	*/
	IMG_UINT32		uDestIReg;

	/*
		Second operation source arguments.
	*/
	DUAL_SEC_SRC	aeSecondarySrcs[USC_NUM_DUALISSUE_OPERANDS];

	/*
		TRUE if a secondary operation source argument is copied to
		the primary.
	*/
	IMG_BOOL		abCopiedToPrimary[USC_NUM_DUALISSUE_OPERANDS];

	/*
		If TRUE copy one of the secondary operation's sources into
		the primary's source 2.
	*/
	IMG_BOOL		bCopySourceFromSecondary;

	/*
		Secondary source to put into the primary source 2.
	*/
	DI_SOURCE		sPriSource2;

	/*
		Commute the first two sources to the primary operation
		when generating the dual-isuse instruction.
	*/
	IMG_BOOL		bCommutePriSrc01;
} DUALISSUE_SECONDARY, *PDUALISSUE_SECONDARY;

static
IMG_VOID FreeDualIssueData(PINTERMEDIATE_STATE psState, PDISSUE_STATE psIssueState, PINST psInst);

static
IMG_BOOL IsValidDualSource0(PINTERMEDIATE_STATE psState, PINST psInst, PARG psArg)
/*****************************************************************************
 FUNCTION	: IsValidDualSource0
    
 PURPOSE	: Check if a source argument is compatible with the restrictions
			  on register banks used in a dual-issue instruction's source 0.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to which the argument is a source.
			  psArg		- Argument to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uHwType;

	uHwType = GetPrecolouredRegisterType(psState, psInst, psArg->uType, psArg->uNumber);
	return IsSrc0StandardBanks(uHwType, psArg->uIndexType);
}

static
IMG_BOOL IsFDDPPrimary(PINTERMEDIATE_STATE	psState,
					   PINST				psInst,
					   IMG_PBOOL			pbSwapSrc12)
/*****************************************************************************
 FUNCTION	: IsFDDPPrimary
    
 PURPOSE	: Check if an instruction could be a FDDP primary operation.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  pbSwapSrc12	- Returns the order for sources 1/2 when
							converting.
			  
 RETURNS	: IMG_TRUE if the instruction could be a FDDP primary operation.
*****************************************************************************/
{
	/*
		Check for an EFO not using its write-back.
	*/
	if (psInst->eOpcode != IEFO)
	{
		return IMG_FALSE;
	}
	if (!psInst->u.psEfo->bIgnoreDest)
	{
		return IMG_FALSE;
	}
	/*
		Check for:
			M0 = SRC0 * SRC2
			M1 = SRC1 * SRC2
	*/
	if (!(psInst->u.psEfo->eM0Src0 == SRC0 &&
		  psInst->u.psEfo->eM0Src1 == SRC2 &&
		  psInst->u.psEfo->eM1Src0 == SRC1 &&
		  psInst->u.psEfo->eM1Src1 == SRC2))
	{
		return IMG_FALSE;
	}
	if (!(psInst->u.psEfo->bWriteI0 && psInst->u.psEfo->bWriteI1))
	{
		return IMG_FALSE;
	}
	/*
		Dual-issue FDDP always writes DEST=SRC0*SRC1 / I0=SRC0*SRC2 so
		check if we need to swap the sources so the right result is
		written to I0.
	*/
	if (psInst->u.psEfo->eI0Src == M0 && psInst->u.psEfo->eI1Src == M1)
	{
		if (pbSwapSrc12 != NULL)
		{
			*pbSwapSrc12 = IMG_TRUE;
		}
	}
	else if (psInst->u.psEfo->eI0Src == M1 && psInst->u.psEfo->eI1Src == M0)
	{
		if (pbSwapSrc12 != NULL)
		{
			*pbSwapSrc12 = IMG_FALSE;
		}
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check if the source which will go into the FDDP source 0 has a 
		compatible bank.
	*/
	if (!IsValidDualSource0(psState, psInst, &psInst->asArg[2]))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

#define PRIMARY_OP_REMAP_ONE			((IMG_UINT32)-1)
#define PRIMARY_OP_REMAP_ZERO			((IMG_UINT32)-2)
#define PRIMARY_OP_REMAP_UNUSED			((IMG_UINT32)-3)

static
IMG_VOID GetPrimaryOperand(PINTERMEDIATE_STATE psState, 
						   PINST psPrimary, 
						   IMG_UINT32 uOpNum, 
						   PARG psOperand, 
						   IMG_PUINT32 puOperandComp)
/*****************************************************************************
 FUNCTION	: GetPrimaryOperand
    
 PURPOSE	: Get the operand an instruction will have when it is converted to a primary.

 PARAMETERS	: psState		- Compiler state.
			  psPrimary		- Instruction to be primary.
			  uOpNum		- Operand number (0:dst, 1..3:src0..src2).

 OUTPUT     : psOperand		- The operand that will be in position uOpNum after 
							  the instruction is converted to a primary.
                              psOperand->uType is set to USC_REGTYPE_DUMMY
                              if operand doesn't exist in the primary instruction.
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	switch (psPrimary->eOpcode)
	{
		case IFADD: /* IFADD d s0 s1 --> IFADM d s0 s1 1.0 */ 
		{
			if (uOpNum == 3)
			{
				InitInstArg(psOperand);
				psOperand->uType = USEASM_REGTYPE_FPCONSTANT;
				psOperand->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				*puOperandComp = 0;

				return;
			}
			break;
		}
		case IFMUL: /* IFMUL d s0 s1 --> IFMAD d s0 s1 0.0 */ 
		{
			if (uOpNum == 3)
			{
				InitInstArg(psOperand);

				psOperand->uType = USEASM_REGTYPE_FPCONSTANT;
				psOperand->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				*puOperandComp = 0;

				return;
			}
			break;
		}
		case IFSSQ: /* fssq d s0 s1 s1 --> fssq d, _, s0, s1 */
		case IFMSA: /* fmsa d s0 s1 s1 --> fmsa d, _, s0, s1 */
		{
			if (uOpNum == 0)
			{
				*psOperand = psPrimary->asDest[0];
				*puOperandComp = 0;
			}
			else if (uOpNum == 1)
			{
				psOperand->uType = USC_REGTYPE_DUMMY;
				*puOperandComp = 0;
			}
			else
			{
				*psOperand = psPrimary->asArg[uOpNum - 2];
				*puOperandComp = GetComponentSelect(psState, psPrimary, uOpNum - 2);
			}
			return;
		}
		case IMOV: /* mov d, s1 */
		{
			if (uOpNum == 1 || uOpNum == 3)
			{
				psOperand->uType = USC_REGTYPE_DUMMY;
				*puOperandComp = 0;
			}
			else
			{
				*psOperand = psPrimary->asArg[0];
				*puOperandComp = 0;
			}
			return;
		}
		case IFMIN: /* fmin d s0 s1 -> fminmax d s0 s0 s1 */
		{
			if (uOpNum == 3)
			{
				*psOperand = psPrimary->asArg[1];
				*puOperandComp = GetComponentSelect(psState, psPrimary, 1 /* uArgIdx */);
				return;
			}
			else if (uOpNum == 2)
			{
				*psOperand = psPrimary->asArg[0];
				*puOperandComp = GetComponentSelect(psState, psPrimary, 0 /* uArgIdx */);
				return;
			}
			break;
		}
		case IEFO:
		{
			IMG_BOOL	bSwapSrc12 = IMG_FALSE;
			
			IsFDDPPrimary(psState, psPrimary, &bSwapSrc12);

			/*
				EFO		s0, s1, s2 -> FDDP	s2, s0, s1
								   or FDDP	s2, s1, s0
			*/
			if (uOpNum == 1)
			{
				*psOperand = psPrimary->asArg[2];
				*puOperandComp = GetComponentSelect(psState, psPrimary, 2 /* uArgIdx */);
			}
			else if (uOpNum == 2 || uOpNum == 3)
			{
				if (bSwapSrc12)
				{
					*psOperand = psPrimary->asArg[1 - (uOpNum - 2)];
					*puOperandComp = GetComponentSelect(psState, psPrimary, 1 - (uOpNum - 2) /* uArgIdx */);
				}
				else
				{
					*psOperand = psPrimary->asArg[uOpNum - 2];
					*puOperandComp = GetComponentSelect(psState, psPrimary, uOpNum - 2 /* uArgIdx */);
				}
				return;
			}
			break;
		}
		default:
		{
			break;
		}
	}
	/* No conversion needed */
	if (uOpNum == 0)
	{
		(*psOperand) = psPrimary->asDest[0];
		*puOperandComp = 0;
	}
	else
	{
		if (uOpNum > psPrimary->uArgumentCount)
		{
			InitInstArg(psOperand);

			psOperand->uType = USEASM_REGTYPE_FPCONSTANT;
			psOperand->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
			*puOperandComp = 0;
		}
		else
		{
			(*psOperand) = psPrimary->asArg[uOpNum - 1];
			*puOperandComp = GetComponentSelect(psState, psPrimary, uOpNum - 1);
		}
	}

	return;
}

static
IMG_BOOL IsFragileInst(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsFragileInst
    
 PURPOSE	: Whether an instruction should be considered fragile.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.

 NOTES		: A fragile instruction is one which can't have an operand blindly replaced.
*****************************************************************************/
{
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) ||
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
	   )
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL SubstEqualArgs(PARG psArgA, PARG psArgB)
/*****************************************************************************
 FUNCTION	: SubstEqualArgs
    
 PURPOSE	: Check if two registers are equal for substitution.

 PARAMETERS	: psArgA, psArgB   - Registers to compare.
			  
 RETURNS	: IMG_TRUE if the registers are equal.
*****************************************************************************/
{
	return psArgA->uType == psArgB->uType &&
		psArgA->uNumber == psArgB->uNumber &&
		psArgA->uIndexType == psArgB->uIndexType &&
		psArgA->uIndexNumber == psArgB->uIndexNumber &&
		psArgA->uIndexStrideInBytes == psArgB->uIndexStrideInBytes;
}

static 
IMG_BOOL GetFreeIReg(PINTERMEDIATE_STATE psState, 
					 IMG_UINT32 uFree,
					 IMG_PUINT32 puIReg)
/*****************************************************************************
 FUNCTION	:  GetFreeIReg
    
 PURPOSE	:  Get a free internal register.

 PARAMETERS	:  psState			- Internal compiler state
			   uFree			- Mask of free registers.

 OUTPUT:	   puIReg			- Register to use.
			  
 RETURNS	:  True iff an internal register is available at the instruction.
*****************************************************************************/
{
	IMG_UINT32 uRegNum;
	IMG_UINT32 uIdx;

	ASSERT(puIReg != NULL);

	for (uIdx = 0; uIdx < USC_NUM_DUALISSUE_IREGISTERS; uIdx++)
	{
		uRegNum = (USC_NUM_EFO_IREGISTERS + uIdx) % USC_NUM_DUALISSUE_IREGISTERS;

		if ((uFree & (1 << uRegNum)) != 0)
		{
			*puIReg = uRegNum;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsSSQPrimary(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsSSQPrimary
    
 PURPOSE	: Check if an instruction could be a SSQ primary operation.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: IMG_TRUE if the instruction could be a SSQ primary operation.
*****************************************************************************/
{
	if (psInst->eOpcode == IFMSA && EqualFloatSrcs(psState,psInst, 1 /* uArg1Idx */, psInst, 2 /* uArg2Idx */))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsPrimaryInst(PINST				psInst)
/*****************************************************************************
 FUNCTION	: IsPrimaryInst
    
 PURPOSE	: Determine whether an instruction can be a primary for dual-issue.

 PARAMETERS	: State				- Compiler state.
			  psBlock			- Block containing the instruction to check.
			  psInst			- Instruction to check.
			  
 RETURNS	: True iff instruction can be primary.
*****************************************************************************/
{
	if (psInst->sStageData.psDIData != NULL)
	{
		if (GetBit(psInst->sStageData.psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_PRIMARYINST))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsValidPrimarySourceModifier(PINTERMEDIATE_STATE	psState, 
									  PINST					psInst, 
									  IMG_UINT32			uDualSrcIdx, 
									  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: IsValidPrimarySourceModifier
    
 PURPOSE	: Checks whether a source to the primary instruction has a valid
			  source modifier to be a source to a dual-issue instruction in
			  a particular position.

 PARAMETERS	: State				- Compiler state.
			  psInst			- Instruction to check.
			  uDualSrcIdx		- Location of the source in the dual-issue
								instruction.
			  uSrcIdx			- Location of the source in the pre-existing
								primary instruction.
			  
 RETURNS	: True if the source modifier is valid.
*****************************************************************************/
{
	/*
		No absolute source modifiers on any source.
	*/
	if (IsAbsolute(psState, psInst, uSrcIdx))
	{
		return IMG_FALSE;
	}
	/*
		No negate source modifier on source 0.
	*/
	if (uDualSrcIdx == 0)
	{
		IMG_BOOL	bAllowSource0Negate;

		/* No negate modifier unless we can swap it to the other source. */
		bAllowSource0Negate = IMG_FALSE;
		if (psInst->eOpcode == IFMUL || psInst->eOpcode == IFMAD || psInst->eOpcode == IFADM || psInst->eOpcode == IFADD)
		{
			bAllowSource0Negate = IMG_TRUE;
		}
		if (!bAllowSource0Negate && IsNegated(psState, psInst, uSrcIdx))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL CheckIsPrimaryInst(PINTERMEDIATE_STATE	psState,
							PINST				psInst)
/*****************************************************************************
 FUNCTION	: CheckIsPrimaryInst
    
 PURPOSE	: Determine whether an instruction can be a primary for dual-issue.

 PARAMETERS	: State				- Compiler state.
			  psInst			- Instruction to check.
			  
 RETURNS	: True iff instruction can be primary.
*****************************************************************************/
{
	IMG_BOOL bPass;
	IMG_UINT32 uArgCount;
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(psState);

	/* Instruction checks */
	if (psInst->uDestCount == 0)
	{
		return IMG_FALSE;
	}
	
	/* check predication */
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}

	/* Check by opcode */
	bPass = IMG_FALSE;
	switch (psInst->eOpcode)
	{
		case IMOV:
		{
			bPass = IMG_TRUE;
			break;
		}
		case IFMAD:
		case IFADM:
		case IFMINMAX:
		{
			bPass = IMG_TRUE;
			break;
		}
		case IEFO:
		{
			/*
				Check if an EFO could be converted to a FDDP

				EFO I0 = m0, I1 = m1, m0 = SRC0 * SRC2, m1 = SRC1 * SRC2

				to

				FDDP i0, i1, SRC2, SRC1, SRC0
			*/
			if (IsFDDPPrimary(psState, psInst, NULL))
			{
				bPass = IMG_TRUE;
			}
			break;
		}
		case IIMA32:
		{
			/* IIMA32: no high bits of result */
			if (psInst->asDest[IMA32_HIGHDWORD_DEST].uType == USC_REGTYPE_UNUSEDDEST)
			{
				bPass = IMG_TRUE;
			}
			break;
		}

		/* Things that can be converted. */
		case IFMSA: /* fmsa d s0 s1 s1 --> fssq d s0 s1 */
		{
			/* IFMSA: primary if it can be converted to sum-of-squares */
			if (EqualFloatSrcs(psState,psInst, 1 /* uArg1Idx */, psInst, 2 /* uArg2Idx */))
				bPass = IMG_TRUE;
			break;
		}
		case IFADD: /* IFADD d s0 s1 --> IFADM d s0 s1 1.0 */ 
		case IFMUL: /* IFMUL d s0 s1 --> IFMAD d s0 s1 0.0 */ 
		{
			bPass = IMG_TRUE;
			break;
		}
		case IFMIN: /* IFMIN d s0 s1 --> IFMINMAX d s0 s0 s1 */
		{
			bPass = IMG_TRUE;
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
	if (!bPass)
	{
		return IMG_FALSE;
	}
		
	/* 
	   Check operands.
	   Operands must not have absolute modifier set.
	   Format must be f32.
	 */

	/* Destination */
	if (psInst->asDest[0].uType != USEASM_REGTYPE_TEMP &&
		psInst->asDest[0].uType != USEASM_REGTYPE_PRIMATTR &&
		psInst->asDest[0].uType != USEASM_REGTYPE_OUTPUT &&
		psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_FALSE;
	}

	
	/*
		Check for unsupported source modifiers.
	*/
	uArgCount = psInst->uArgumentCount;
	for (i = 0; i < uArgCount; i++)
	{
		if (!IsValidPrimarySourceModifier(psState, psInst, i, i))
		{
			return IMG_FALSE;
		}
		if (!(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_F16F32SELECT))
		{
			if (psInst->asArg[i].eFmt == UF_REGFORMAT_F16)
			{
				return IMG_FALSE;
			}
		}
		/*
			A dual issued IMA32 never sign extends immediate arguments so check if
			we are using negative immediates.
		*/
		if (psInst->eOpcode == IIMA32 &&
			psInst->asArg[i].uType == USEASM_REGTYPE_IMMEDIATE &&
			psInst->asArg[i].uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check for an instruction which uses source 0.
	*/
	if (
			!(
				psInst->eOpcode == IFMSA ||
				psInst->eOpcode == IFMOV ||
				psInst->eOpcode == IMOV ||
				psInst->eOpcode == IEFO
			)
	   )
	{
		/*
			Check for extended register banks used on the first two sources.
		*/
		if (!IsValidDualSource0(psState, psInst, &psInst->asArg[0]))
		{
			/*
				Check if its possible to swap the two sources.
			*/
			if (
					psInst->eOpcode == IFMINMAX ||
					psInst->eOpcode == IFMIN || 
					uArgCount == 1 || 
					!IsValidDualSource0(psState, psInst, &psInst->asArg[1]) ||
					!IsValidPrimarySourceModifier(psState, psInst, 0 /* uDualSrcIdx */, 1 /* uSrcIdx */)
			   )
			{
				return IMG_FALSE;
			}

			/*
				Swap the two instruction sources.
			*/
			CommuteSrc01(psState, psInst);
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsSecondaryInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsSecondaryInst
    
 PURPOSE	: Determine whether an instruction can be a secondary for dual-issue.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  
 RETURNS	: True iff instruction can be secondary.
*****************************************************************************/
{
	IMG_UINT32 i;
	IMG_UINT32 uArgCount;
	IMG_BOOL bAllowNegate;

	/* Instruction checks */
	if (psInst->uDestCount == 0)
	{
		return IMG_FALSE;
	}

	/* check predication */
	if (!NoPredicate(psState, psInst))
	{
		return IMG_FALSE;
	}

	/* Check by opcode */
	switch (psInst->eOpcode)
	{
		case IFMAD:
		case IFMAD16:
		case IFMSA:
		case IFSUBFLR:
		case IFMAXMIN:
		case IFADD:
		case IFMUL:
		{
			break;
		}
		case IFRCP:
		case IFRSQ:
		case IFEXP:
		case IFLOG:
		case IFSIN:
		case IFCOS:
		{
			/* No format conversion from C10 on dual-issue. */
			if (psInst->asArg[0].eFmt == UF_REGFORMAT_C10)
			{
				return IMG_FALSE;
			}
			break;
		}
		case IIMA32:
		{
			/*
				IMA32 isn't valid as a secondary operation on cores affected by this bug.
			*/
			if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_30898) != 0)
			{
				return IMG_FALSE;
			}

			/* IIMA32: no high bits of result */
			if (psInst->asDest[IMA32_HIGHDWORD_DEST].uType != USC_REGTYPE_UNUSEDDEST)
			{
				return IMG_FALSE;
			}
			break;
		}
		case IMOV:
		{
			if (psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
			{
				return IMG_FALSE;
			}
			break;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	/* 
	   Operands must not have absolute or negate modifier or indexing. 
	*/

	/* Destination */
	if (psInst->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}

	/*
		Allow negate for ADD providing only one source is negated.
	*/
	bAllowNegate = IMG_FALSE;
	if (psInst->eOpcode == IFADD)
	{
		bAllowNegate = IMG_TRUE;
		if (IsNegated(psState, psInst, 0) && IsNegated(psState, psInst, 1))
		{
			return IMG_FALSE;
		}
	}

	/* Sources */
	uArgCount = psInst->uArgumentCount;
	for (i = 0; i < uArgCount; i++)
	{
		if (IsAbsolute(psState, psInst, i) ||
			(!bAllowNegate && IsNegated(psState, psInst, i)) ||
			psInst->asArg[i].uIndexType != USC_REGTYPE_NOINDEX)
		{
			return IMG_FALSE;
		}
		if (!(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_F16F32SELECT))
		{
			if (psInst->asArg[i].eFmt == UF_REGFORMAT_F16)
			{
				return IMG_FALSE;
			}
		}
		if (psInst->eOpcode == IFMAD16 && psInst->u.psArith16->aeSwizzle[i] != FMAD16_SWIZZLE_LOWHIGH)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_INT GetPrimaryArgCount(PINST psPrimary)
/*****************************************************************************
 FUNCTION	: GetPrimaryArgCount
    
 PURPOSE	: Get the number of arguments that an instruction will have when it 
			  is converted to a primary.

 PARAMETERS	: psPrimary		- Instruction to be primary.

 RETURNS	: The argument count of the primary.
*****************************************************************************/
{
	IMG_UINT32 uArgCount = psPrimary->uArgumentCount;

	switch (psPrimary->eOpcode)
	{
		case IFADD: /* IFADD d s0 s1 --> IFADM d s0 s1 1.0 */ 
		case IFMUL: /* IFMUL d s0 s1 --> IFMAD d s0 s1 0.0 */ 
			return 3;
		default:
			break;
	}
	return uArgCount;
}

static
IMG_UINT32 CoInstArgMap(PINTERMEDIATE_STATE psState, 
						PINST psInstS, 
						IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: CoInstArgMap
    
 PURPOSE	: Map arguments to a co-instruction to primary instruction's sources

 PARAMETERS	: psState	- Internal compiler state
			  psInstS	- Secondary instruction.
			  uArg		- Argument to map to a source operand in the primary.

 RETURNS	: Operand number in the secondary for given argument.
*****************************************************************************/
{
	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	ASSERT(uArg >= 0 && uArg <= 2);

	switch (psInstS->eOpcode)
	{
		case IFSUBFLR:
		case IFRCP:
		case IFRSQ:
		case IFEXP:
		case IFLOG:
		case IFSIN:
		case IFCOS:
		case IFADD:
		case IFSUB:
		case IFMUL:
		case IMOV:
		{
			ASSERT(uArg <= 1);
			return (uArg + 1);
		}
		default:
			break;
	}

	return uArg;
}


static
IMG_BOOL EqualArgsAndComponents(PINTERMEDIATE_STATE psState,
								PINST psInst1, 
								IMG_UINT32 uArgIdx1, 
								PINST psInst2, 
								IMG_UINT32 uArgIdx2)
/*****************************************************************************
 FUNCTION	: EqualArgsAndComponents
    
 PURPOSE	: Check that two instruction arguments refer to the same register
			  and have the same component select.

 PARAMETERS	: psState	- Internal compiler state
			  psInst1	- First argument to check.
			  uArgIdx1
			  psInst2	- Second argument to check.
			  uArgIdx2

 RETURNS	: TRUE if the operand was added to the conversion list.
*****************************************************************************/
{
	if (!EqualArgs(&psInst1->asArg[uArgIdx1], &psInst2->asArg[uArgIdx2]))
	{
		return IMG_FALSE;
	}
	if (GetComponentSelect(psState, psInst1, uArgIdx1) != GetComponentSelect(psState, psInst2, uArgIdx2))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL AddToConvertList(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  PDUALISSUE_SECONDARY	psSec,
						  PARG					psOperand,
						  IMG_UINT32			uSrcIdx,
						  IMG_BOOL				bCombineOnly,
						  IMG_PUINT32			puIdx)
/*****************************************************************************
 FUNCTION	: AddToConvertList
    
 PURPOSE	: Map arguments to a co-instruction to primary instruction's sources

 PARAMETERS	: psState	- Internal compiler state
			  psBlock	- Basic block that contains the dual-issue instructions.
			  psSec		- Conversion state.
			  psOperand	- Secondary operand to be converted.
			  uSrcIdx	- Index of the source to the secondary corresponding
						  to this operand.
			  bCombineOnly
						- If TRUE only check if this operand is already in
						  the conversion list.
			  puIdx		- Returns the index of the entry for this operand.

 RETURNS	: TRUE if the operand was added to the conversion list.
*****************************************************************************/
{
	IMG_UINT32			uConvertIdx;
	PCONVERT_OPERAND	psConvert;

	/* Don't replace one internal register by another. */
	if (psOperand->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return IMG_FALSE;
	}
	/* No F16/F32 format select on internal register sources. */
	if (psOperand->eFmt == UF_REGFORMAT_F16)
	{
		return IMG_FALSE;
	}

	/*
		Check if we are already converting this source.
	*/
	for (uConvertIdx = 0; uConvertIdx < psSec->uConvertCount; uConvertIdx++)
	{
		psConvert = &psSec->asConvert[uConvertIdx];

		/* Don't combine with the destination. */
		if (uConvertIdx == 0 && psSec->bConvertDest)
		{
			continue;
		}
		if (EqualArgs(&psConvert->sOperand, psOperand))
		{
			/*
				Update the mask of secondary source corresponding to
				this conversion.
			*/
			psConvert->uSrcMask |= (1 << uSrcIdx);

			/*
				Return the index of the combined entry.
			*/
			if (puIdx != NULL)
			{
				*puIdx = uConvertIdx;
			}

			return IMG_TRUE;
		}
	}

	if (bCombineOnly)
	{
		return IMG_FALSE;
	}

	/*
		Add the operand to this conversion list.
	*/
	ASSERT(psSec->uConvertCount < (sizeof(psSec->asConvert) / sizeof(psSec->asConvert[0])));
	psConvert = &psSec->asConvert[psSec->uConvertCount];
	psConvert->sOperand = *psOperand;
	psConvert->uIReg = USC_UNDEF;
	if (uSrcIdx != USC_UNDEF)
	{
		psConvert->uSrcMask = 1 << uSrcIdx;
	}
	else
	{
		psConvert->uSrcMask = 0;
	}
	psConvert->sMoveLoc.psInst = NULL;
	psConvert->bLastUseAtSite = IMG_FALSE;

	/*
		Return the index of the newly added entry.
	*/
	if (puIdx != NULL)
	{
		*puIdx = psSec->uConvertCount;
	}

	/*
		Prelimary check for operand that could be completely replaced by internal
		registers.
	*/
	if (
			psOperand->uType == USEASM_REGTYPE_TEMP ||
			psOperand->uType == USEASM_REGTYPE_PRIMATTR ||
			psOperand->uType == USEASM_REGTYPE_OUTPUT
		)
	{
		if (!ReferencedOutsideBlock(psState, psBlock, psOperand))
		{
			psSec->uReplace |= (1 << psSec->uConvertCount);
		}
	}

	psSec->uConvertCount++;

	return IMG_TRUE;
}

static
IMG_BOOL PrimarySources01Commute(PINTERMEDIATE_STATE psState, PINST psPrimary)
/*****************************************************************************
 FUNCTION	: PrimarySources01Commute
    
 PURPOSE	: Checks whether the first two sources to the primary operation of
			  a dual-issue instruction commute.

 PARAMETERS	: psState	- Internal compiler state
			  psPrimary	- Primary operation.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check that the first two sources to the ALU operation commute.
	*/
	if (!InstSource01Commute(psState, psPrimary))
	{
		return IMG_FALSE;
	}
	/*
		Check the source modifier on the current second source would be valid in the
		dual-issue instruction's source 0.
	*/
	if (!IsValidPrimarySourceModifier(psState, psPrimary, 0 /* uDualSrcIdx */, 1 /* uSrcIdx */))
	{
		return IMG_FALSE;
	}
	/*
		Check the source modifier on the current first source would be valid in the
		dual-issue instruction's source 1.
	*/
	if (!IsValidPrimarySourceModifier(psState, psPrimary, 1 /* uDualSrcIdx */, 0 /* uSrcIdx */))
	{
		return IMG_FALSE;
	}
	/*
		Check the register bank for the current second source would be valid in the
		dual-issue instruction's source 0.
	*/
	if (!IsValidDualSource0(psState, psPrimary, &psPrimary->asArg[1]))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL SecondarySources01Commute(PINTERMEDIATE_STATE psState, PINST psSecondary)
/*****************************************************************************
 FUNCTION	: SecondarySources01Commute
    
 PURPOSE	: Checks whether the first two sources to the secondary operation of
			  a dual-issue instruction commute.

 PARAMETERS	: psState	- Internal compiler state
			  psPrimary	- Secondary operation.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psSecondary->eOpcode == IFADD && (IsNegated(psState, psSecondary, 0) || IsNegated(psState, psSecondary, 1)))
	{
		return IMG_FALSE;
	}
	return InstSource01Commute(psState, psSecondary);
}

static
IMG_VOID SwapDISources(PDI_SOURCE asSrcs, IMG_UINT32 uIdx1, IMG_UINT32 uIdx2)
/*****************************************************************************
 FUNCTION	: SwapDISources
    
 PURPOSE	: Swaps two elements in an array of dual-issue sources.

 PARAMETERS	: asSrcs			- Array to modify.
			  uIdx1				- Entries to swap.
			  uIdx2

 RETURNS	: Nothing.
*****************************************************************************/
{
	DI_SOURCE	sTemp;

	sTemp = asSrcs[uIdx1];
	asSrcs[uIdx1] = asSrcs[uIdx2];
	asSrcs[uIdx2] = sTemp;
}

static
IMG_BOOL EqualDISources(PDI_SOURCE psSource1, PDI_SOURCE psSource2)
/*****************************************************************************
 FUNCTION	: EqualDISources
    
 PURPOSE	: Compares two sources to a dual-issue instruction.

 PARAMETERS	: psSource1			- Sources to compare.
			  psSource2

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (!EqualArgs(&psSource1->sArg, &psSource2->sArg))
	{
		return IMG_FALSE;
	}
	if (psSource1->uComponent != psSource2->uComponent)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_VOID ReorderSources(PINTERMEDIATE_STATE		psState,
						PINST					psPrimary,
						PINST					psSecondary,
						PDI_SOURCE				asPriSrcs,
						PDI_SOURCE				asSecSrcs,
						IMG_PBOOL				pbCommutePriSrc01)
/*****************************************************************************
 FUNCTION	: ReorderSources
    
 PURPOSE	: Compares two sources to a dual-issue instruction.

 PARAMETERS	: psState			- Compiler state.
			  psPrimary			- Dual-issue primary operation.
			  psSecondary		- Dual-issue secondary operation.
			  asPriSrcs			- Returns the arguments to the primary operation.
			  asSecSrcs			- Returns the argunents to the secondary operation.
			  pbCommutePriSrc01	- If TRUE commute the first two primary sources when
								creating the dual-issue instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uPriSrc;
	IMG_UINT32	uSecSrc;
	IMG_UINT32	uIdx;
	IMG_BOOL	bPrimarySourcesCommute;
	IMG_BOOL	bSecondarySourcesCommute;
	IMG_UINT32	uBestMatchIdx;
	IMG_UINT32	uBestMatchCount;
	IMG_UINT32	uBestIRegInPriSlotCount;
	static const struct
	{
		IMG_BOOL	bCommutePriSrc01;
		IMG_BOOL	bCommuteSecSrc01;
	} abSwapCombinations[] = 
	{
		{IMG_FALSE, IMG_FALSE},
		{IMG_FALSE, IMG_TRUE},
		{IMG_TRUE, IMG_FALSE},
		{IMG_TRUE, IMG_TRUE},
	};

	*pbCommutePriSrc01 = IMG_FALSE;

	/*	
		Get the primary sources as they would be if it was part of a dual-issue
		instruction.
	*/
	for (uPriSrc = 0; uPriSrc < DUAL_MAX_PRI_SOURCES; uPriSrc++)
	{
		GetPrimaryOperand(psState, 
						  psPrimary, 
						  1 + uPriSrc, 
						  &asPriSrcs[uPriSrc].sArg, 
						  &asPriSrcs[uPriSrc].uComponent);
	}

	/*
		Get the secondary sources.
	*/
	for (uSecSrc = 0; uSecSrc < psSecondary->uArgumentCount; uSecSrc++)
	{
		asSecSrcs[uSecSrc].sArg = psSecondary->asArg[uSecSrc];
		asSecSrcs[uSecSrc].uComponent = GetComponentSelect(psState, psSecondary, uSecSrc);
	}

	if (psSecondary->eOpcode == IFADD && IsNegated(psState, psSecondary, 0))
	{
		/*
			Swap the sources to allow the negate modifier.
		*/
		SwapDISources(asSecSrcs, 0 /* uIdx1 */, 1 /* uIdx2 */);
	}

	/*
		Check if the primary sources could be swapped.
	*/
	bPrimarySourcesCommute = IMG_FALSE;
	if (PrimarySources01Commute(psState, psPrimary))
	{
		bPrimarySourcesCommute = IMG_TRUE;
	}

	/*
		Check if the secondary sources could be swapped.
	*/
	bSecondarySourcesCommute = IMG_FALSE;
	if (SecondarySources01Commute(psState, psSecondary))
	{
		bSecondarySourcesCommute = IMG_TRUE;
	}

	/*
		If order of sources to neither operation can be modified
		then there is nothing more to do.
	*/
	if (!bPrimarySourcesCommute && !bSecondarySourcesCommute)
	{
		return;
	}

	/*
		A secondary MOV operation can use any source from the primary
		so there is no point in swapped sources.
	*/
	if (psSecondary->eOpcode == IMOV)
	{
		return;
	}

	/*
		Check for each possible combination of swaps how many sources to the
		secondary match the corresponding source to the primary.
	*/
	uBestMatchIdx = USC_UNDEF;
	uBestMatchCount = 0;
	uBestIRegInPriSlotCount = 0;
	for (uIdx = 0; uIdx < 4; uIdx++)
	{
		IMG_UINT32 const*	auPriRemap;
		IMG_UINT32 const*	auSecRemap;
		const IMG_UINT32	auNoSwap[] = {0, 1, 2};
		const IMG_UINT32	auSwap01[] = {1, 0, 2}; 
		IMG_UINT32			uMatchCount;
		IMG_UINT32			uIRegInPriSlotCount;
		IMG_BOOL			bBetter;

		if (!abSwapCombinations[uIdx].bCommutePriSrc01)
		{
			auPriRemap = auNoSwap;
		}
		else
		{
			if (!bPrimarySourcesCommute)
			{
				continue;
			}
			auPriRemap = auSwap01;
		}

		if (!abSwapCombinations[uIdx].bCommuteSecSrc01)
		{
			auSecRemap = auNoSwap;
		}
		else
		{
			if (!bSecondarySourcesCommute)
			{
				continue;
			}
			auSecRemap = auSwap01;
		}

		uMatchCount = 0;
		uIRegInPriSlotCount = 0;
		for (uSecSrc = 0; uSecSrc < psSecondary->uArgumentCount; uSecSrc++)
		{
			IMG_UINT32	uSArg = CoInstArgMap(psState, psSecondary, auSecRemap[uSecSrc]);
			PDI_SOURCE	psPriSrc = &asPriSrcs[auPriRemap[uSArg]];
			PDI_SOURCE	psSecSrc = &asSecSrcs[uSecSrc];

			/*
				Check for a primary source which matches the secondary source.
			*/
			if (EqualDISources(psPriSrc, psSecSrc))
			{
				uMatchCount++;
			}

			/*
				Check for a source which is unused by the primary (and therefore available
				for encoding a secondary argument) with the secondary argument an internal
				register. We'd prefer to put a secondary argument which is a unified store
				register here. The unified store argument can then use the unused source
				in the primary and doesn't need to be replaced by an internal register.
			*/
			if (psPriSrc->sArg.uType == USC_REGTYPE_DUMMY && psSecSrc->sArg.uType == USEASM_REGTYPE_FPINTERNAL)
			{
				uIRegInPriSlotCount++;
			}
		}

		bBetter = IMG_FALSE;
		if (uMatchCount > uBestMatchCount)
		{
			bBetter = IMG_TRUE;
		}
		if (uMatchCount == uBestMatchCount && uIRegInPriSlotCount < uBestIRegInPriSlotCount)
		{	
			bBetter = IMG_TRUE;
		}
		if (bBetter || uBestMatchIdx == USC_UNDEF)
		{
			uBestMatchCount = uMatchCount;
			uBestIRegInPriSlotCount = uIRegInPriSlotCount;
			uBestMatchIdx = uIdx;
		}
	}

	if (abSwapCombinations[uBestMatchIdx].bCommutePriSrc01)
	{
		SwapDISources(asPriSrcs, 0 /* uIdx1 */, 1 /* uIdx2 */);
		*pbCommutePriSrc01 = IMG_TRUE;
	}
	else
	{
		*pbCommutePriSrc01 = IMG_FALSE;
	}
	
	if (abSwapCombinations[uBestMatchIdx].bCommuteSecSrc01)
	{
		SwapDISources(asSecSrcs, 0 /* uIdx1 */, 1 /* uIdx2 */);
	}
}

static
IMG_BOOL GetConversionSettings(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   PINST				psPrimary,
							   PINST				psSecondary,
							   PDUALISSUE_SECONDARY	psSec)
/*****************************************************************************
 FUNCTION	: GetConversionSettings
    
 PURPOSE	: Get the settings needed to convert an instruction to the
			  secondary operation of a dual-issue instruction.

 PARAMETERS	: psState	- Internal compiler state
			  psBlock	- Basic block that contains the dual-issue instructions.
			  psPrimary	- Primary operation.
			  psSecondary
						- Secondary operation.
			  psSec		- Conversion state.

 RETURNS	: TRUE if conversion is possible.
*****************************************************************************/
{
	PARG		psSecDest = &psSecondary->asDest[0];
	DI_SOURCE	asPriSrcs[DUAL_MAX_PRI_SOURCES];
	DI_SOURCE	asSecSrcs[DUAL_MAX_SEC_SOURCES];

	/*
		Initialise conversion settings.
	*/
	psSec->uConvertCount = 0;
	psSec->uDestIReg = USC_UNDEF;
	psSec->bConvertDest = IMG_FALSE;
	psSec->uSrc0Idx = USC_UNDEF;
	psSec->uReplace = 0;
	psSec->bUsesSrc0 = IMG_FALSE;
	psSec->bCopySourceFromSecondary = IMG_FALSE;
	InitInstArg(&psSec->sPriSource2.sArg);
	psSec->sPriSource2.uComponent = USC_UNDEF;
	memset(psSec->abCopiedToPrimary, 0, sizeof(psSec->abCopiedToPrimary));

	/*
		Check if swapping either the primary or secondary sources would allow more sources
		from the primary to be reused in the secondary.
	*/
	ReorderSources(psState, 
				   psPrimary, 
				   psSecondary, 
				   asPriSrcs,
				   asSecSrcs,
				   &psSec->bCommutePriSrc01);

	if (psSecDest->uType != USEASM_REGTYPE_FPINTERNAL)
	{
		psSec->bConvertDest = IMG_TRUE;
		/*
			Add the destination to the list of operands to be
			converted.
		*/
		if (!AddToConvertList(psState, psBlock, psSec, psSecDest, USC_UNDEF, IMG_FALSE, NULL))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Record the internal register destination for
			the secondary operation.
		*/
		ASSERT(psSecDest->uNumber < USC_NUM_DUALISSUE_IREGISTERS);
		psSec->uDestIReg = psSecDest->uNumber;
	}

	/* Secondary mov is a special case. */
	if (psSecondary->eOpcode == IMOV)
	{
		IMG_UINT32 uArgCountP = min(GetPrimaryArgCount(psPrimary), DUAL_MAX_PRI_SOURCES);
		PARG psSecArg = &psSecondary->asArg[0];
		DUAL_SEC_SRC eSrc;

		/* 
		   Source operand must be an internal register or a primary source. 
		   (Don't bother converting destination or source).
		*/
		eSrc = DUAL_SEC_SRC_INVALID;
		if (psSecArg->uType == USEASM_REGTYPE_FPINTERNAL)
		{
			ASSERT(psSecArg->uNumber < USC_NUM_DUALISSUE_IREGISTERS);
			eSrc = DUAL_SEC_SRC_I0 + psSecArg->uNumber;
		}
		else
		{
			IMG_UINT32	uPriArg;
			IMG_UINT32	uPriArgStart;

			if (psPrimary->eOpcode == IMOV || psPrimary->eOpcode == IFSSQ)
			{
				/* Primary arguments start from the dual-issue instruction's source 1. */
				uPriArgStart = 1;
			}
			else
			{
				/* Primary arguments start from the dual-issue instruction's source 0. */
				uPriArgStart = 0;
			}

			for (uPriArg = 0; uPriArg < uArgCountP; uPriArg++)
			{
				PDI_SOURCE	psPriSrc = &asPriSrcs[uPriArg + uPriArgStart];

				if (psPriSrc->sArg.uType == USC_REGTYPE_DUMMY)
				{
					continue;
				}

				if (EqualArgs(psSecArg, &psPriSrc->sArg) && psPriSrc->uComponent == 0)
				{
					eSrc = DUAL_SEC_SRC_PRI0 + uPriArg + uPriArgStart;
					break;
				}
			}
		}
		if (eSrc == DUAL_SEC_SRC_INVALID)
		{
			return IMG_FALSE;
		}
		psSec->aeSecondarySrcs[0] = eSrc;
	}
	else
	{
		IMG_UINT32	uArgCountS = psSecondary->uArgumentCount;
		IMG_UINT32	uArg;

		/*
			If the secondary uses source 0 then either

				both the destination and source 0 have to
				be the same internal register
			or
				the destination has to be i0 and source 0
				the same as the primary source 0.
		*/
		if (uArgCountS > 2)
		{
			DUAL_SEC_SRC	eSrc0;
			PDI_SOURCE		psPriSrc0 = &asPriSrcs[0];
			PDI_SOURCE		psSecSrc0 = &asSecSrcs[0];

			psSec->bUsesSrc0 = IMG_TRUE;

			eSrc0 = DUAL_SEC_SRC_INVALID;
			if (psSecDest->uType == USEASM_REGTYPE_FPINTERNAL)
			{
				/*	
					The destination is already an internal register
				*/

				/* Check if source 0 is already the same internal register. */
				if (psSecSrc0->sArg.uType == USEASM_REGTYPE_FPINTERNAL &&
					psSecSrc0->sArg.uNumber == psSecDest->uNumber)
				{
					eSrc0 = DUAL_SEC_SRC_I0 + psSecSrc0->sArg.uNumber;
				}
				/* Check for writing i0 and source 0 the same as the primary source 0. */
				else if (psSecDest->uNumber == 0 && EqualDISources(psPriSrc0, psSecSrc0))
				{
					eSrc0 = DUAL_SEC_SRC_PRI0;
				}
			}
			else
			{
				/*
					Check for source 0 already an internal register. We'll need to
					allocate the same internal register to the destination later.
				*/
				if (psSecSrc0->sArg.uType == USEASM_REGTYPE_FPINTERNAL)
				{
					ASSERT(psSecSrc0->sArg.uNumber < USC_NUM_DUALISSUE_IREGISTERS);
					eSrc0 = DUAL_SEC_SRC_I0 + psSecSrc0->sArg.uNumber;
				}
				/*
					Check for source 0 the same as the primary source 0. We'll need to
					allocate i0 to the secondary destination later.
				*/
				else if (EqualDISources(psPriSrc0, &asSecSrcs[0]))
				{
					eSrc0 = DUAL_SEC_SRC_PRI0;
				}
			}
			if (eSrc0 == DUAL_SEC_SRC_INVALID)
			{
				if (!AddToConvertList(psState, psBlock, psSec, &asSecSrcs[0].sArg, 0, IMG_FALSE, &psSec->uSrc0Idx))
				{
					return IMG_FALSE;
				}
				eSrc0 = DUAL_SEC_SRC_UNDEFINED_IREG;
			}
			psSec->aeSecondarySrcs[0] = eSrc0;
		}

		/* Source operand i must be an internal register or primary source i. */
		for (uArg = (uArgCountS > 2) ? 1 : 0;
			 uArg < uArgCountS;
			 uArg++)
		{
			PDI_SOURCE	psSecSrc = &asSecSrcs[uArg];

			if (psSecSrc->sArg.uType == USEASM_REGTYPE_FPINTERNAL)
			{
				ASSERT(psSecSrc->sArg.uNumber < USC_NUM_DUALISSUE_IREGISTERS);
				psSec->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_I0 + psSecSrc->sArg.uNumber;
			}
			else
			{
				IMG_UINT32	uSArg;
				PDI_SOURCE	psPriSrc;

				uSArg = CoInstArgMap(psState, psSecondary, uArg);
				psPriSrc = &asPriSrcs[uSArg];

				if (
						/*
							Check for a source not unused by the primary.
						*/
						psPriSrc->sArg.uType == USC_REGTYPE_DUMMY &&
						/*
							Don't put an F16 source in the primary unless the
							primary opcode supports F16/F32 format selects.
						*/
					    (
							(g_psInstDesc[psPrimary->eOpcode].uFlags & DESC_FLAGS_F16F32SELECT) ||
							psSecSrc->sArg.eFmt == UF_REGFORMAT_F32
						)
				   )
				{
					psSec->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_PRI0 + uSArg;
					psSec->sPriSource2 = *psSecSrc;
					psSec->bCopySourceFromSecondary = IMG_TRUE;
					psSec->abCopiedToPrimary[uArg] = IMG_TRUE;
				}
				else if (EqualDISources(psPriSrc, psSecSrc))
				{
					psSec->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_PRI0 + uSArg;
				}
				else
				{
					if (!AddToConvertList(psState, psBlock, psSec, &psSecSrc->sArg, uArg, IMG_FALSE, NULL))
					{
						return IMG_FALSE;
					}
					psSec->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_UNDEFINED_IREG;
				}
			}
		}

		/*
			For any secondary sources which are the same as the corresponding primary source but which
			are also converted (because they occur several times as arguments to the secondary) convert
			them everywhere.
		*/
		for (uArg = 0;
			 uArg < uArgCountS;
			 uArg++)
		{
			PARG	psSecOperand = &asSecSrcs[uArg].sArg;

			if (psSec->aeSecondarySrcs[uArg] == DUAL_SEC_SRC_PRI0 ||
				psSec->aeSecondarySrcs[uArg] == DUAL_SEC_SRC_PRI1 ||
				psSec->aeSecondarySrcs[uArg] == DUAL_SEC_SRC_PRI2)
			{
				IMG_PUINT32	puIdx;

				puIdx = NULL;
				if (uArg == 0 && uArgCountS > 2)
				{
					puIdx = &psSec->uSrc0Idx;
				}

				if (AddToConvertList(psState, psBlock, psSec, psSecOperand, uArg, IMG_TRUE, puIdx))
				{
					psSec->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_UNDEFINED_IREG;

					if (psSec->abCopiedToPrimary[uArg])
					{
						psSec->abCopiedToPrimary[uArg] = IMG_FALSE;
						psSec->bCopySourceFromSecondary = IMG_FALSE;
					}
				}
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL AreCoInsts(PINTERMEDIATE_STATE psState,
					PINST psPrimary, 
					PINST psSecondary)
/*****************************************************************************
 FUNCTION	: AreCoInsts
    
 PURPOSE	: Determine whether instructions can be dual-issued together.

 PARAMETERS	: psState		- Internal compiler state
			  psPrimary		- Instruction to be primary.
			  psSecondary	- Instruction to be Secondary.

 RETURNS	: True iff instruction can be co-instructions, possibly subject to 
			  converting the secondary, by commuting sources, or replacing
			  operands with iregisters.
*****************************************************************************/
{
	IMG_BOOL bPass;

	ASSERT (psPrimary != NULL && psSecondary != NULL);

	/* Check predication */
	if (!EqualPredicates(psPrimary, psSecondary))
	{
		return IMG_FALSE;
	}

	/* Check by opcode of secondary */
	bPass = IMG_FALSE;
	switch (psSecondary->eOpcode)
	{
		case IFMAD:
		case IFMAD16:
		case IFMSA:
		{
			switch (psPrimary->eOpcode)
			{
				case IFMUL:
				case IFMAD:
				{
					if (psSecondary->eOpcode == IFMAD)
						bPass = IMG_TRUE;
					break;
				}
				case IIMA32:
				case IMOV:
				{
					bPass = IMG_TRUE;
					break;
				}
				default:
				{
					break;
				}
			}	
			break;
		}
		case IFSUBFLR:
		case IFADD:
		case IFSUB:
		{
			if (!(psPrimary->eOpcode == IEFO || psPrimary->eOpcode == IFMINMAX || psPrimary->eOpcode == IFMIN))
				bPass = IMG_TRUE;
			break;
		}
		case IFMAXMIN:
		{
			if (psPrimary->eOpcode == IMOV)
				bPass = IMG_TRUE;
			break;
		}
		case IFRCP:
		case IFRSQ:
		case IFEXP:
		case IFLOG:
		case IFSIN:
		case IFCOS:
		{
			bPass = IMG_TRUE;
			break;
		}
		case IFMUL:
		{
			switch(psPrimary->eOpcode)
			{
				case IFMAD:
				case IMOV:
				case IFMINMAX:
				case IFMIN:
				case IIMA32:
				case IFMUL:
				{
					bPass = IMG_TRUE;
					break;
				}
				default:
				{
					bPass = IMG_FALSE;
					break;
				}
			}
			break;
		}

		case IIMA32:
		{
			if (!(psPrimary->eOpcode == IFMINMAX || psPrimary->eOpcode == IIMA32 || psPrimary->eOpcode == IFMIN))
			{
				bPass = IMG_TRUE;
			}
			break;
		}
		case IMOV:
		{
			bPass = IMG_TRUE;
			break;
		}
		default:
		{
			break;
		}
	}
	if (!bPass)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_VOID MoveDualSrc(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uToSrcIdx, IMG_UINT32 uFromSrcIdx)
/*****************************************************************************
 FUNCTION	: MoveDualSrc
    
 PURPOSE	: Move a source argument within a dual-issue instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  uToSrcIdx		- New position for the source argument.
			  uFromSrcIdx	- Old position for the source argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	MoveSrc(psState, psInst, uToSrcIdx, psInst, uFromSrcIdx);
	psDual->abPrimarySourceNegate[uToSrcIdx] = psDual->abPrimarySourceNegate[uFromSrcIdx];
	psDual->auSourceComponent[uToSrcIdx] = psDual->auSourceComponent[uFromSrcIdx];
}

static
IMG_VOID SwapDualSrcs(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrc1Idx, IMG_UINT32 uSrc2Idx)
/*****************************************************************************
 FUNCTION	: SwapDualSrcs
    
 PURPOSE	: Swap the position of two source arguments within a dual-issue instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  uSrc1Idx		- Positions to swap.
			  uSrc2Idx

 RETURNS	: Nothing.
*****************************************************************************/
{
	SwapInstSources(psState, psInst, uSrc1Idx, uSrc2Idx);
}

static
IMG_VOID CopyDualSrc(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uToSrcIdx, IMG_UINT32 uFromSrcIdx)
/*****************************************************************************
 FUNCTION	: CopyDualSrc
    
 PURPOSE	: Duplicate a source argument within a dual-issue instruction.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  uToSrcIdx		- Position to duplicate to.
			  uFromSrcIdx	- Position to duplicate from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	SetSrcFromArg(psState, psInst, uToSrcIdx, &psInst->asArg[uFromSrcIdx]);
	psDual->abPrimarySourceNegate[uToSrcIdx] = psDual->abPrimarySourceNegate[uFromSrcIdx];
	psDual->auSourceComponent[uToSrcIdx] = psDual->auSourceComponent[uFromSrcIdx];
}

static
IMG_VOID ClearDualSrc(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx)
/*****************************************************************************
 FUNCTION	: ClearDualSrc
    
 PURPOSE	: Mark a source argument to a dual-issue instruction as unused.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  uToSrcIdx		- Position of the argument to clear.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	ASSERT(uSrcIdx < DUAL_MAX_PRI_SOURCES);

	InitInstArg(&psInst->asArg[uSrcIdx]);
	psInst->asArg[uSrcIdx].uType = USC_REGTYPE_UNUSEDSOURCE;
	psInst->asArg[uSrcIdx].uNumber = 0;
	psDual->abPrimarySourceNegate[uSrcIdx] = IMG_FALSE;
	psDual->auSourceComponent[uSrcIdx] = 0;
}

static
IMG_VOID SetDualSrcConstant(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, IMG_UINT32 uConstNum)
/*****************************************************************************
 FUNCTION	: ClearDualSrc
    
 PURPOSE	: Set a source argument to a dual-issue instruction to a hardware
			  constant.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  uToSrcIdx		- Position of the argument to set.
			  uConstNum		- Hardware constant number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	ASSERT(uSrcIdx < DUAL_MAX_PRI_SOURCES);

	InitInstArg(&psInst->asArg[uSrcIdx]);
	psInst->asArg[uSrcIdx].uType = USEASM_REGTYPE_FPCONSTANT;
	psInst->asArg[uSrcIdx].uNumber = uConstNum;
	psDual->abPrimarySourceNegate[uSrcIdx] = IMG_FALSE;
	psDual->auSourceComponent[uSrcIdx] = 0;
}

static
IMG_VOID NegatePrimarySource0(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: NegatePrimarySource0
    
 PURPOSE	: Update the primary operation of a dual-issue instruction to
			  apply a negate modifier to the first source by using the
			  commutatively of the operation.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	switch (psDual->ePrimaryOp)
	{
		case IFDDP:
		case IEFO:
		case IFADM:
		case IFADD:
		{
			psDual->abPrimarySourceNegate[1] = !psDual->abPrimarySourceNegate[1];
			psDual->abPrimarySourceNegate[2] = !psDual->abPrimarySourceNegate[2];
			break;
		}
		case IFMUL:
		case IFMAD:
		{
			psDual->abPrimarySourceNegate[1] = !psDual->abPrimarySourceNegate[1];
			break;
		}
		default: imgabort();
	}
}

static
IMG_VOID ConvertPrimaryInst(PINTERMEDIATE_STATE		psState,
							PINST					psInst, 
							PDUALISSUE_SECONDARY	psSec)
/*****************************************************************************
 FUNCTION	: ConvertPrimaryInst
    
 PURPOSE	: Modify an instruction so that it can be co-issued as a primary.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- Instruction to modify.
			  psSec			- Secondary conversion settings.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uArg;
	IOPCODE			ePriOpcode = psInst->eOpcode;
	PDUAL_PARAMS	psDual;
	IMG_BOOL		bSwapSrc12;
	IMG_UINT32		uNewDestCount;
	IMG_BOOL		abNegate[DUAL_MAX_PRI_SOURCES];
	IMG_UINT32		auComponent[DUAL_MAX_PRI_SOURCES];
	
	/* 
		The dual-issue instruction (except for primary FDDP) has two destinations: a unified store and an 
		internal register. 
	*/
	uNewDestCount = 2;

	/*
		Save negate source modifiers and component selects from the old instruction.
	*/
	for (uArg = 0; uArg < min(psInst->uArgumentCount, DUAL_MAX_PRI_SOURCES); uArg++)
	{
		abNegate[uArg] = IsNegated(psState, psInst, uArg);
		auComponent[uArg] = GetComponentSelect(psState, psInst, uArg);
		ASSERT(!IsAbsolute(psState, psInst, uArg));
	}

	/*
		When converting EFO -> FDDP remember whether
		to swap the sources.
	*/
	bSwapSrc12 = IMG_FALSE;
	if (psInst->eOpcode == IEFO)
	{
		IMG_BOOL	bRet;

		bRet = IsFDDPPrimary(psState, psInst, &bSwapSrc12);
		ASSERT(bRet);

		/*
			Now writing i1 through the instruction destination not
			as an implict EFO destination.
		*/
		MoveDest(psState, psInst, 0 /* uCopyToDestIdx */, psInst, EFO_I1_DEST);
		psInst->auDestMask[0] = psInst->auDestMask[EFO_I1_DEST];
		psInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[EFO_I1_DEST];

		/*
			Add the extra DDP internal register destination as the
			third destination.
		*/
		#if SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM != 0
		#pragma message("Unexpected implicit internal register destination for FDDP as dual-issued primary")
		#endif /* SGX545_USE_FDUAL_FDDP_IREG_DEST_NUM != 0 */
		MoveDest(psState, psInst, 2 /* uCopyToDestIdx */, psInst, EFO_I0_DEST);
		psInst->auDestMask[2] = psInst->auDestMask[EFO_I0_DEST];
		psInst->auLiveChansInDest[2] = psInst->auLiveChansInDest[EFO_I0_DEST];

		uNewDestCount = 3;
	}

	/*
		Convert to a dual-issue instruction. 
	*/
	SetOpcodeAndDestCount(psState, psInst, IDUAL, uNewDestCount);
	psDual = psInst->u.psDual;
	
	/* Clear DI types */
	SetBit(psInst->auFlag, INST_DI_PRIMARY, 0);
	SetBit(psInst->auFlag, INST_DI_SECONDARY, 0);

	/*
		Set the primary operation.
	*/
	psDual->ePrimaryOp = ePriOpcode;

	/*
		Copy negate modifiers to the new instruction.
	*/
	memcpy(psDual->abPrimarySourceNegate, abNegate, sizeof(psDual->abPrimarySourceNegate));

	/*
		Copy component selects to the new instruction.
	*/
	memcpy(psDual->auSourceComponent, auComponent, sizeof(psDual->auSourceComponent));

	switch (ePriOpcode)
	{
		case IFMSA:	/* fmsa d s0 s1 s1 --> fssq d s0 s1 */
		{
			if (EqualArgsAndComponents(psState, psInst, 1 /* uArgIdx1 */, psInst, 2 /* uArgIdx2 */))
			{
				psDual->ePrimaryOp = IFSSQ;

				MoveDualSrc(psState, psInst, 2 /* uTo */, 1 /* uFrom */);
				MoveDualSrc(psState, psInst, 1 /* uTo */, 0 /* uFrom */);
				ClearDualSrc(psState, psInst, 0 /* uSrcIdx */);
			}
			break;
		}
		case IFADD: /* IFADD d s0 s1 --> IFADM d s0 s1 1.0 */ 
		{
			psDual->ePrimaryOp = IFADM;

			SetDualSrcConstant(psState, psInst, 2 /* uSrcIdx */, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1);
			break;
		}
		case IFMUL: /* IFMUL d s0 s1 --> IFMAD d s0 s1 0.0 */ 
		{
			psDual->ePrimaryOp = IFMAD;

			SetDualSrcConstant(psState, psInst, 2 /* uSrcIdx */, EURASIA_USE_SPECIAL_CONSTANT_ZERO);
			break;
		}
		case IFMIN: /* IFMIN d s0 s1 --> IFMINMAX d s0 s0 s1 */
		{
			psDual->ePrimaryOp = IFMINMAX;

			MoveDualSrc(psState, psInst, 2 /* uToSrcIdx */, 1 /* uFromSrcIdx */);
			CopyDualSrc(psState, psInst, 1 /* uToSrcIdx */, 0 /* uFromSrcIdx */);
			break;
		}
		case IMOV:
		{
			/* IMOV d s --> DUAL d __ s __ */
			MoveDualSrc(psState, psInst, 1 /* uToSrcIdx */, 0 /* uFromSrcIdx */);
			ClearDualSrc(psState, psInst, 0 /* uSrcIdx */);
			ClearDualSrc(psState, psInst, 2 /* uSrcIdx */);
			break;
		}
		case IEFO:
		{
			psDual->ePrimaryOp = IFDDP;

			if (bSwapSrc12)
			{
				/*
					SRC0: A			C  
					SRC1: B		->	B
					SRC2: C			A
				*/
				SwapDualSrcs(psState, psInst, 2 /* uSrc1Idx */, 0 /* uSrc2Idx */);
			}
			else
			{
				/*
					SRC0: A			B		C       
					SRC1: B		->	A	->	A
					SRC2: C			C		B
				*/
				SwapDualSrcs(psState, psInst, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
				SwapDualSrcs(psState, psInst, 0 /* uSrc1Idx */, 2 /* uSrc2Idx */);
			}
			break;
		}
		default:
		{
			break;
		}
	}

	/*
		Handle a negate modifier on source 0 by swapping it to the
		source it is multiplied by.
	*/
	if (psDual->abPrimarySourceNegate[0])
	{
		NegatePrimarySource0(psState, psInst);
		psDual->abPrimarySourceNegate[0] = IMG_FALSE;
	}

	if (psSec == NULL)
	{
		return;
	}

	/* 
	   Substitute iregisters for source operands using assignments from the
	   dual issue-state.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg ++)
	{
		IMG_UINT32	uCtr;

		for (uCtr = 0; uCtr < psSec->uConvertCount; uCtr ++)
		{
			PCONVERT_OPERAND	psCvtOp = &psSec->asConvert[uCtr];

			if (psSec->bConvertDest && uCtr == 0)
			{
				continue;
			}
			if (psSec->uReplace & (1 << uCtr))
			{
				if (SubstEqualArgs(&psInst->asArg[uArg], &psCvtOp->sOperand))
				{
					SetSrc(psState, psInst, uArg, USEASM_REGTYPE_FPINTERNAL, psCvtOp->uIReg, psInst->asArg[uArg].eFmt);
				}
			}
		}
	}
}

static
IMG_VOID DualMove(PINTERMEDIATE_STATE	psState,
				  PDISSUE_STATE			psIssueState,
				  PDI_MOVE_LOC			psLoc,
				  PINST					psSecInst,
				  IMG_UINT32			uIReg,
				  PARG					psUSReg)
/*****************************************************************************
 FUNCTION	: DualMove
    
 PURPOSE	: Dual-issue a move between an internal register and a unified store
			  register.

 PARAMETERS	: psState		- Internal compiler state.
			  psIssueState	- Module state.
			  psLoc			- Where to insert the move.
			  psSecInst		- Dual-issue secondary operation.
			  uIReg			- Internal register number.
			  psUSReg		- Unified store register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psSite = psLoc->psInst;

	ASSERT(psUSReg->eFmt == UF_REGFORMAT_F32);

	/*
		Remove the instruction from the lists of possible secondary move locations.
	*/
	FreeDualIssueData(psState, psIssueState, psSite);

	switch (psLoc->eType)
	{
		case DI_MOVE_TYPE_PRIMOV:
		{
			PDUAL_PARAMS	psDual;

			ASSERT(psSite->eOpcode == IMOV);

			/*
				Convert the existing instruction to a dual-issue.
			*/
			ConvertPrimaryInst(psState, psSite, NULL);

			psDual = psSite->u.psDual;

			/*
				Make the secondary operation a MOV writing the
				internal register.
			*/
			psDual->eSecondaryOp = IMOV;
			SetDest(psState, psSite, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
			psSite->auDestMask[1] = USC_ALL_CHAN_MASK;
			psSite->auLiveChansInDest[1] = USC_ALL_CHAN_MASK;

			/*
				Put the unified store source into the dual-issue's
				source 2.
			*/
			psDual->aeSecondarySrcs[0] = DUAL_SEC_SRC_PRI2;
			psDual->uSecSourcesUsed = DUAL_SEC_SRC_PRI2_MASK;
			SetSrcFromArg(psState, psSite, 2 /* uSrcIdx */, psUSReg);
			break;
		}
		case DI_MOVE_TYPE_EFOWB:
		{
			ASSERT(psSite->eOpcode == IEFO);
			ASSERT(psSite->u.psEfo->bIgnoreDest);
			ASSERT(CanUseDest(psState, psSite, EFO_US_DEST, psUSReg->uType, psUSReg->uIndexType));

			/*
				Set up the EFO write back to write from the internal register
				to the unified store register.
			*/
			psSite->u.psEfo->bIgnoreDest = IMG_FALSE;
			ASSERT(uIReg < USC_NUM_EFO_IREGISTERS);
			psSite->u.psEfo->eDSrc = I0 + uIReg;
			ASSERT(EqualArgs(psUSReg, &psSecInst->asDest[0]));
			MoveDest(psState, psSite, EFO_US_DEST, psSecInst, 0 /* uMoveFromDestIdx */);
			SetupEfoUsage(psState, psSite);
			psSite->auLiveChansInDest[0] = USC_ALL_CHAN_MASK;

			break;
		}
		case DI_MOVE_TYPE_SECMOV:
		{
			PDUAL_PARAMS	psDual;

			ASSERT(psSite->eOpcode == IMOV);
			ASSERT(psSite->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL);
			
			/*
				Convert the instruction to a dual-issue primary.
			*/
			ConvertPrimaryInst(psState, psSite, NULL);

			psDual = psSite->u.psDual;

			/*
				Switch the existing MOV to the secondary operation.
			*/
			psDual->eSecondaryOp = IMOV;
			SetDest(psState, psSite, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, psSite->asDest[0].uNumber, UF_REGFORMAT_F32);
			psSite->auDestMask[1] = USC_ALL_CHAN_MASK;
			psSite->auLiveChansInDest[1] = USC_ALL_CHAN_MASK;

			/*
				Move the source for the existing move to source 2.
			*/
			psDual->aeSecondarySrcs[0] = DUAL_SEC_SRC_PRI2;
			psDual->uSecSourcesUsed = DUAL_SEC_SRC_PRI2_MASK;
			MoveSrc(psState, psSite, 2 /* uMoveToSrcIdx */, psSite, 1 /* uMoveFromSrcIdx */);

			/*
				Make the internal register the source for the
				primary operation.
			*/
			SetSrc(psState, psSite, 1 /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);

			/*
				Add the unified store register as the primary
				destination.
			*/
			ASSERT(EqualArgs(psUSReg, &psSecInst->asDest[0]));
			MoveDest(psState, psSite, 0 /* uMoveToDestIdx */, psSecInst, 0 /* uMoveFromDestIdx */);
			break;
		}
		case DI_MOVE_TYPE_SHAREDPRISRC:
		{
			PDUAL_PARAMS	psDual;
			IMG_UINT32		uArg;

			/*
				Convert the existing instruction to a dual-issue
				primary.
			*/
			ConvertPrimaryInst(psState, psSite, NULL);
			
			psDual = psSite->u.psDual;

			/*
				Make the secondary operation a MOV to the internal
				register.
			*/
			psDual->eSecondaryOp = IMOV;
			SetDest(psState, psSite, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
			psSite->auDestMask[1] = USC_ALL_CHAN_MASK;
			psSite->auLiveChansInDest[1] = USC_ALL_CHAN_MASK;

			/*
				Try to use a spare source to SSQ.
			*/
			if (psDual->ePrimaryOp == IFSSQ && IsValidDualSource0(psState, psSite, psUSReg))
			{
				ASSERT(psSite->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE);
				SetSrcFromArg(psState, psSite, 0 /* uSrcIdx */, psUSReg);
				psDual->aeSecondarySrcs[0] = DUAL_SEC_SRC_PRI0;
				psDual->uSecSourcesUsed = DUAL_SEC_SRC_PRI0_MASK;
				break;
			}

			/*
				Find the primary source that matches the unified store
				source.
			*/
			for (uArg = 0; uArg < DUAL_MAX_PRI_SOURCES; uArg++)
			{
				if (SubstEqualArgs(psUSReg, &psSite->asArg[uArg]))
				{
					break;
				}
			}
			ASSERT(uArg < DUAL_MAX_PRI_SOURCES);
			psDual->aeSecondarySrcs[0] = DUAL_SEC_SRC_PRI0 + uArg;
			psDual->uSecSourcesUsed = 1 << psDual->aeSecondarySrcs[0];
			break;
		}
		case DI_MOVE_TYPE_DUALWITHSELF:
		{
			PDUAL_PARAMS	psDual;
			IMG_UINT32		uArg;
			IOPCODE			ePriOp;
			IMG_UINT32		auComponent[DUAL_MAX_PRI_SOURCES];

			/*
				Save the existing source modifiers.
			*/
			for (uArg = 0; uArg < psSite->uArgumentCount; uArg++)
			{
				ASSERT(!HasNegateOrAbsoluteModifier(psState, psSite, uArg));
				auComponent[uArg] = GetComponentSelect(psState, psSite, uArg);
			}

			/*
				Convert the existing instruction to a dual-issue
				primary.
			*/
			ePriOp = psSite->eOpcode;
			SetOpcode(psState, psSite, IDUAL);
			psDual = psSite->u.psDual;
			psDual->ePrimaryOp = IFMAD;
			switch (ePriOp)
			{
				case IFMAD: 
				{
					break;
				}
				case IFMUL:
				{
					psSite->asArg[2].uType = USEASM_REGTYPE_FPCONSTANT;
					psSite->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
					break;
				}
				case IFADD:
				{
					MoveSrc(psState, psSite, 2 /* uCopyToIdx */, psSite, 1 /* uCopyFromIdx */);

					InitInstArg(&psSite->asArg[1]);
					psSite->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
					psSite->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					break;
				}
				default: imgabort();
			}

			/*
				Make the secondary operation the same as the
				primary.
			*/
			psDual->eSecondaryOp = psDual->ePrimaryOp;
			SetDestCount(psState, psSite, 2 /* uNewDestCount */);
			SetDest(psState, psSite, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
			psSite->auDestMask[1] = USC_ALL_CHAN_MASK;
			psSite->auLiveChansInDest[1] = USC_ALL_CHAN_MASK;

			/*	
				Restore the source modifiers from the original instruction.
			*/
			memcpy(psDual->auSourceComponent, auComponent, sizeof(psDual->auSourceComponent));

			/*
				Use the same argument as the primary.
			*/
			psDual->uSecSourcesUsed = 0;
			for (uArg = 0; uArg < g_psInstDesc[psDual->eSecondaryOp].uDefaultArgumentCount; uArg++)
			{
				psDual->aeSecondarySrcs[uArg] = DUAL_SEC_SRC_PRI0 + uArg;
				psDual->uSecSourcesUsed |= (1 << psDual->aeSecondarySrcs[uArg]);
			}
			break;
		}
		default: imgabort();
	}
	if(IsSecondaryInst(psState, psSite))
	{
		SetBit(psSite->auFlag, INST_DI_SECONDARY, 1);
	}
}

static
IMG_BOOL LastUseAtSecondary(PINTERMEDIATE_STATE	psState,
							PINST				psInstP,
							PINST				psInstS,
							PARG				psOperand)
/*****************************************************************************
 FUNCTION	: LastUseAtSecondary
    
 PURPOSE	: Check whether the last use of an operand to the secondary operation
			  is at the point of the dual-issue.

 PARAMETERS	: psState			- Compiler state.
			  psInstP		- Location of the dual-issue instruction.
			  psInstS		- Instruction moving to the location of the
								dual-issue instruction.
			  psOperand			- Secondary operand to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;

	ASSERT(psOperand->uType == USEASM_REGTYPE_TEMP);
	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psOperand->uNumber);

	/*
		Look at all the uses for one in the same block and after the location of the new
		dual-issue instruction. If the operand is used in another block then we won't be able
		to replace it completely by an internal register so this is safe (if inefficent).
	*/
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == USE_TYPE_SRC &&
			psUse->u.psInst->psBlock == psInstP->psBlock &&
			psUse->u.psInst->uBlockIndex != psInstS->uBlockIndex &&
			psUse->u.psInst->uBlockIndex > psInstP->uBlockIndex)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL AssignIRegForDest(PINTERMEDIATE_STATE		psState,
						   PINST					psInstP,
						   PINST					psInstS,
						   PDUALISSUE_SECONDARY		psSec,
						   IMG_UINT32				uIRegsForSrcs)
/*****************************************************************************
 FUNCTION	: AssignIRegForDest
    
 PURPOSE	: Assign an internal register to replace the destination of an
			  instruction which is going to be the secondary operation of a
			  dual-issue instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInstP		- Point where the dual-issue instruction
								will be generated.
			  psInstS		- Instruction moving to where the dual-issue
								instruction will be generated.
			  psSec				- Secondary conversion settings.
			  uIRegsForSrcs		- Mask of internal registers which are available
								for sources.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32			uIRegsForDest;
	PCONVERT_OPERAND	psDestOp = &psSec->asConvert[0];
	PINST				psInstAfterSite;

	uIRegsForDest = (1 << USC_NUM_DUALISSUE_IREGISTERS) - 1;

	/*
		Start with the internal register live after the
		dual-issue instruction (treating the instruction 
		moving to the dual-issue site as though it was already there).
	*/
	psInstAfterSite = psInstP->psNext;
	if (psInstAfterSite == psInstS)
	{
		psInstAfterSite = psInstAfterSite->psNext;
	}
	if (psInstAfterSite != NULL)
	{
		IMG_UINT32	uIRegsLiveAfterSite;

		uIRegsLiveAfterSite = UseDefGetIRegsLiveBeforeInst(psState, psInstAfterSite);
		uIRegsForDest &= ~uIRegsLiveAfterSite;
	}

	if (psSec->bUsesSrc0)
	{
		DUAL_SEC_SRC	eSrc0 = psSec->aeSecondarySrcs[0];

		if (eSrc0 == DUAL_SEC_SRC_PRI0)
		{
			/*
				Must use i0 for destination.
			*/
			uIRegsForDest &= (1 << 0);
		}
		else if (eSrc0 == DUAL_SEC_SRC_I0 ||
				 eSrc0 == DUAL_SEC_SRC_I1 ||
				 eSrc0 == DUAL_SEC_SRC_I2)
		{
			/*
				Must use the same internal register as source 0.
			*/
			uIRegsForDest &= (1 << (eSrc0 - DUAL_SEC_SRC_I0));
		}
		else
		{
			ASSERT(eSrc0 == DUAL_SEC_SRC_UNDEFINED_IREG);

			/*
				Must use the same internal register for the
				destination and source 0 so only consider
				internal registers available for the sources.
			*/
			uIRegsForDest &= uIRegsForSrcs;
		}
	}

	/*
		Get an internal register from the mask of those
		available.
	*/
	if (!GetFreeIReg(psState, uIRegsForDest, &psDestOp->uIReg))
	{
		return IMG_FALSE;
	}

	/*
		Record the internal register for the destination.
	*/
	ASSERT(psSec->uDestIReg == USC_UNDEF);
	psSec->uDestIReg = psDestOp->uIReg;

	return IMG_TRUE;
}

static
IMG_BOOL GetDualIssueSettings(PINTERMEDIATE_STATE	psState,
							  PINST					psInstP,
							  PINST					psInstS,
							  PDUALISSUE_SECONDARY	psSec)
/*****************************************************************************
 FUNCTION	: GetDualIssueSettings
    
 PURPOSE	: Assign internal registers for secondary sources that need to be
			  converted.

 PARAMETERS	: psState		- Compiler state.
			  psInstP		- Primary operation.
			  psInstS		- Instruction moving to where the dual-issue
								instruction will be generated.
			  psSec			- Secondary conversion settings.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uIRegsForSrcs;
	IMG_UINT32	uCvtIdx;
	IMG_UINT32	uCvtStart;

	uIRegsForSrcs = (1 << USC_NUM_DUALISSUE_IREGISTERS) - 1;
	/*
		Start with the internal registers live before the dual-issue
		instruction (treating the instruction moving to the dual-issue
		site as though it was already there).
	*/
	uIRegsForSrcs &= ~UseDefGetIRegsLiveBeforeInst(psState, psInstP);

	/*
		Assign an internal register for the destination.
	*/
	if (psSec->bConvertDest)
	{
		if (!AssignIRegForDest(psState, psInstP, psInstS, psSec, uIRegsForSrcs))
		{
			return IMG_FALSE;
		}
		uCvtStart = 1;
	}
	else
	{
		uCvtStart = 0;
	}

	/*
		Check for secondary operands whose last use is at the dual-issue instruction. We
		can allocate them the same internal register as one of the dual-issue destinations.
	*/
	for (uCvtIdx = uCvtStart; uCvtIdx < psSec->uConvertCount; uCvtIdx++)
	{
		PCONVERT_OPERAND	psSrcOp = &psSec->asConvert[uCvtIdx];

		psSrcOp->bLastUseAtSite = IMG_TRUE;
		if (
				(psSec->uReplace & (1 << uCvtIdx)) &&
				!LastUseAtSecondary(psState, psInstP, psInstS, &psSrcOp->sOperand)
		   )
		{
			psSrcOp->bLastUseAtSite = IMG_FALSE;
		}
	}

	/*
		Assign internal registers for the source arguments.
	*/
	for (uCvtIdx = uCvtStart; uCvtIdx < psSec->uConvertCount; uCvtIdx++)
	{
		PCONVERT_OPERAND	psSrcOp = &psSec->asConvert[uCvtIdx];
		IMG_UINT32			uIRegsForThisSrc;
		IMG_UINT32			uIRegsForThisSrcNonReplace;
		IMG_UINT32			uSrc;
		
		uIRegsForThisSrc = uIRegsForSrcs;
		if (uCvtIdx == psSec->uSrc0Idx)
		{
			/*
				For source 0 we need to assign the same internal register
				as the destination.
			*/
			uIRegsForThisSrc &= (1 << psSec->uDestIReg);
		}

		uIRegsForThisSrcNonReplace = uIRegsForThisSrc;

		/*
			If the operand needs to be replaced after the dual-issue instruction
			as well then we can't use an internal register written in the dual-issue
			instruction.
		*/
		if (!psSrcOp->bLastUseAtSite)
		{
			uIRegsForThisSrc &= ~(1 << psSec->uDestIReg);
			uIRegsForThisSrc &= ~GetIRegsWrittenMask(psState, psInstP, IMG_FALSE);
		}

		/*
			Try to get an internal register from the mask of the those available.
		*/
		if (!GetFreeIReg(psState, uIRegsForThisSrc, &psSrcOp->uIReg))
		{
			/*
				If that failed then try to get an internal register which is the same
				as one of the destinations.
			*/
			if (
					uIRegsForThisSrcNonReplace != uIRegsForThisSrc &&
					GetFreeIReg(psState, uIRegsForThisSrcNonReplace, &psSrcOp->uIReg)
			   )
			{
				/*
					Can't completely replace this operand by an internal register.
				*/
				psSec->uReplace &= ~(1 << uCvtIdx);
			}
			else
			{
				return IMG_FALSE;
			}
		}

		/*
			Subtract the allocated internal register from those available
			for sources.
		*/
		uIRegsForSrcs &= ~(1 << psSrcOp->uIReg);

		/*
			Set up the secondary sources now we know the internal register
			to use.
		*/
		for (uSrc = 0; uSrc < USC_NUM_DUALISSUE_OPERANDS; uSrc++)
		{
			if (psSrcOp->uSrcMask & (1 << uSrc))
			{
				ASSERT(psSec->aeSecondarySrcs[uSrc] == DUAL_SEC_SRC_UNDEFINED_IREG);
				psSec->aeSecondarySrcs[uSrc] = DUAL_SEC_SRC_I0 + psSrcOp->uIReg;
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsDualIssueWithSelf(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 IMG_UINT32				uIReg)
/*****************************************************************************
 FUNCTION	: IsDualIssueWithSelf
    
 PURPOSE	: Check if an instruction can be converted to a dual-issue instruction
			  writing the same result both to its existing destination and an
			  internal register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uIReg			- Internal register to write from the second
							operation.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	/*
		We need to use source 0 from the primary in the secondary. That's only valid
		if the secondary destination is i0.
	*/
	if (uIReg != 0)
	{
		return IMG_FALSE;
	}
	/*
		Check for an opcode which can be dual-issued with itself.
	*/
	if (!(psInst->eOpcode == IFMAD || psInst->eOpcode == IFMUL || psInst->eOpcode == IFADD))
	{
		return IMG_FALSE;
	}
	/*
		Check for source modifiers. These aren't supported in the secondary operation.
	*/
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		if (HasNegateOrAbsoluteModifier(psState, psInst, uArg))
		{
			return IMG_FALSE;
		}
	}
	if (IsPrimaryInst(psInst))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL DoesIntervalContainDeschedule(PINTERMEDIATE_STATE	psState,
									   PDISSUE_STATE		psIssueState,
									   PINST				psIntervalStart,
									   PINST				psIntervalEnd)
/*****************************************************************************
 FUNCTION	: DoesIntervalContainDeschedule
    
 PURPOSE	: Checks if there is a descheduling instruction inside an
			  interval.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psIntervalStart		- Interval to check.
			  psIntervalEnd

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psIssueState->sDeschedList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PDUALISSUE_STAGEDATA	psDeschedData;
		PINST					psDeschedInst;

		psDeschedData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sDeschedListEntry);
		psDeschedInst = psDeschedData->psInst;

		/*
			Skip descheduling instructions before the start of the interval.
		*/
		if (psDeschedInst->uBlockIndex < psIntervalStart->uBlockIndex)
		{
			continue;
		}
			
		if (IsDeschedBeforeInst(psState, psDeschedInst))
		{
			/*
				If the contents of the internal registers are lost before the instruction starts then
				check if the interval ends either at the instruction or after it.
			*/
			if (psDeschedInst->uBlockIndex <= psIntervalEnd->uBlockIndex)
			{
				return IMG_TRUE;
			}
		}
		else
		{
			ASSERT(IsDeschedAfterInst(psDeschedInst));
			
			/*
				If the contents of the internal registers are lost after the instruction reads its sources
				then check if interval ends after the instruction.
			*/
			if (psDeschedInst->uBlockIndex < psIntervalEnd->uBlockIndex)
			{
				return IMG_TRUE;
			}
		}

		/*
			The list of descheduling instructions is sorted in the same order as the block so we
			can stop looking now.
		*/
		return IMG_FALSE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL CouldReplaceIntervalByInternalRegister(PINTERMEDIATE_STATE	psState,
												PDISSUE_STATE		psIssueState,
												PINST				psIntervalStart,
												PINST				psIntervalEnd,
												IMG_UINT32			uIReg)
/*****************************************************************************
 FUNCTION	: CouldReplaceIntervalByInternalRegister
    
 PURPOSE	: Check if an interval contains either a descheduling instruction
			  or another use of a specific internal register.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psIntervalStart		- Interval to check.
			  psIntervalEnd
			  uIReg					- Internal register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check if the interval contains a deschedule.
	*/
	if (DoesIntervalContainDeschedule(psState, psIssueState, psIntervalStart, psIntervalEnd))
	{
		return IMG_FALSE;
	}
	/*
		Check if the interval contains another use of the same internal register.
	*/
	if (UseDefIsIRegLiveInInternal(psState, uIReg, psIntervalStart, psIntervalEnd))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
PDUALISSUE_STAGEDATA AllocateDualIssueData(PINTERMEDIATE_STATE psState, PDISSUE_STATE psIssueState, PINST psInst)
/*****************************************************************************
 FUNCTION	: AllocateDualIssueData
    
 PURPOSE	: Allocates per-instruction data used when generating dual-issue.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psInst				- Instruction to allocate for.

 RETURNS	: A pointer to the per-instruction data.
*****************************************************************************/
{
	PDUALISSUE_STAGEDATA	psDIData;

	if (psInst->sStageData.psDIData != NULL)
	{
		return psInst->sStageData.psDIData;
	}

	psInst->sStageData.psDIData = psDIData = UscAlloc(psState, sizeof(*psInst->sStageData.psDIData));
	psDIData->psInst = psInst;
	InitializeList(&psDIData->sInstMoveSiteList);
	memset(psDIData->auFlags, 0, sizeof(psDIData->auFlags));
	AppendToList(&psIssueState->sStageDataList, &psDIData->sStageDataListEntry);

	return psDIData;
}

static
IMG_VOID FreeDualIssueData(PINTERMEDIATE_STATE psState, PDISSUE_STATE psIssueState, PINST psInst)
/*****************************************************************************
 FUNCTION	: FreeDualIssueData
    
 PURPOSE	: Frees per-instruction data used when generating dual-issue.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psInst				- Instruction to free for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDUALISSUE_STAGEDATA	psDIData;
	PUSC_LIST_ENTRY			psListEntry;

	psDIData = psInst->sStageData.psDIData;
	if (psDIData == NULL)
	{
		return;
	}

	SetBit(psInst->auFlag, INST_DI_PRIMARY, 0);
	SetBit(psInst->auFlag, INST_DI_SECONDARY, 0);
	
	RemoveFromList(&psIssueState->sStageDataList, &psDIData->sStageDataListEntry);

	if (GetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_SSQ))
	{
		RemoveFromList(&psIssueState->sSSQList, &psDIData->sSSQListEntry);
	}
	if (GetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_EFO))
	{
		RemoveFromList(&psIssueState->sEFOWriteBackList, &psDIData->sEFOWriteBackListEntry);
	}
	if (GetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_DESCHED))
	{
		RemoveFromList(&psIssueState->sDeschedList, &psDIData->sDeschedListEntry);
	}
	if (GetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_PRIMARYMOVE))
	{
		RemoveFromList(&psIssueState->sPrimaryMoveList, &psDIData->sPrimaryMoveListEntry);
	}
	if (GetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_SECONDARYMOVE))
	{
		RemoveFromList(&psIssueState->sSecondaryMoveList, &psDIData->sSecondaryMoveListEntry);
	}

	while ((psListEntry = RemoveListHead(&psDIData->sInstMoveSiteList)) != NULL)
	{
		PNONTEMP_MOVESITE	psMoveSite;

		psMoveSite = IMG_CONTAINING_RECORD(psListEntry, PNONTEMP_MOVESITE, sInstListEntry);
		RemoveFromList(&psMoveSite->psNonTemp->sMoveSiteList, &psMoveSite->sNonTempListEntry);
		UscFree(psState, psMoveSite);
	}

	UscFree(psState, psDIData);
	psInst->sStageData.psDIData = NULL;
}

static
IMG_VOID AddNonTempMoveSite(PINTERMEDIATE_STATE		psState,
							PDISSUE_STATE			psIssueState,
							PINST					psInst,
							NONTEMP_MOVESITE_TYPE	eSiteType,
							IMG_UINT32				uSiteData,
							IMG_UINT32				uRegType,
							IMG_UINT32				uRegNumber)
/*****************************************************************************
 FUNCTION	: AddNonTempMoveSite
    
 PURPOSE	: Adds information about use of non-temporary data in an instruction
			  which could be converted to the primary operation of a dual-issue
			  instruction.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psInst				- Instruction to contains the use.
			  eSiteType				- Type of use.
			  uSiteData				- Information about the use.
			  uRegType				- Non-temporary data register type.
			  uRegNumber			- Non-temporary data register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PNONTEMP_MOVESITE	psMoveSite;
	NONTEMP				sKey;
	PNONTEMP			psNonTemp;

	psMoveSite = UscAlloc(psState, sizeof(*psMoveSite));
	psMoveSite->eType = eSiteType;
	psMoveSite->psInst = psInst;
	psMoveSite->u.uSiteData = uSiteData;
	AppendToList(&psInst->sStageData.psDIData->sInstMoveSiteList, &psMoveSite->sInstListEntry);

	sKey.uRegType = uRegType;
	sKey.uRegNumber = uRegNumber;
	psNonTemp = UscTreeGetPtr(psIssueState->psNonTempTree, &sKey);

	if (psNonTemp == NULL)
	{
		NONTEMP	sNewElem;

		sNewElem.uRegType = uRegType;
		sNewElem.uRegNumber = uRegNumber;
		InitializeList(&sNewElem.sMoveSiteList);

		psNonTemp = UscTreeAdd(psState, psIssueState->psNonTempTree, &sNewElem);
	}

	psMoveSite->psNonTemp = psNonTemp;
	AppendToList(&psNonTemp->sMoveSiteList, &psMoveSite->sNonTempListEntry);
}

static 
IMG_VOID AddAsImplicitConstSite(PINTERMEDIATE_STATE	psState,
								PDISSUE_STATE		psIssueState,
								PINST				psInst,
								IMG_UINT32			uConstNum)
/*****************************************************************************
 FUNCTION	: AddAsImplicitConstSite
    
 PURPOSE	: Adds information about an instruction which could be converted to 
			  the primary operation of a dual-issue instruction.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psInst				- Instruction.
			  uConstNum				- Hardware constant which would be a source
									to the instruction when it was converted.

 RETURNS	: Nothing.
*****************************************************************************/
{
	AddNonTempMoveSite(psState, 
					   psIssueState, 
					   psInst, 
					   NONTEMP_MOVESITE_TYPE_IMPLICITCONST, 
					   uConstNum, 
					   USEASM_REGTYPE_FPCONSTANT, 
					   uConstNum);
}

static
IMG_VOID AddAsSharedSrcSite(PINTERMEDIATE_STATE	psState,
							PDISSUE_STATE		psIssueState,
							PINST				psInst,
							IMG_UINT32			uSharedSrc)
/*****************************************************************************
 FUNCTION	: AddAsImplicitConstSite
    
 PURPOSE	: Adds information about an instruction which could be converted to 
			  the primary operation of a dual-issue instruction and has a
			  non-temporary source.

 PARAMETERS	: psState				- Compiler state.
			  psIssueState			- Module state.
			  psInst				- Instruction.
			  uSharedSrc			- Index of the non-temporary source.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psSrc = &psInst->asArg[uSharedSrc];

	AddNonTempMoveSite(psState, 
					   psIssueState, 
					   psInst, 
					   NONTEMP_MOVESITE_TYPE_SHAREDSRC, 
					   uSharedSrc, 
					   psSrc->uType, 
					   psSrc->uNumber);
}

static
IMG_INT32 CmpNonTempCB(IMG_PVOID pvElem1, IMG_PVOID pvElem2)
/*****************************************************************************
 FUNCTION	: CmpNonTempCB
    
 PURPOSE	: Helper to sort elements of ISSUE_STATE.psNonTempTree.

 PARAMETERS	: pvElem1, pvElem2	- The elements to compare

 RETURNS	: -ve	if ELEM1 < ELEM2
			  0		if ELEM1 == ELEM2
			  +ve	if ELEM1 > ELEM2
*****************************************************************************/
{
	PNONTEMP	psElem1 = (PNONTEMP)pvElem1;
	PNONTEMP	psElem2 = (PNONTEMP)pvElem2;

	if (psElem1->uRegType == psElem2->uRegType)
	{
		return psElem1->uRegNumber - psElem2->uRegNumber;
	}
	return psElem1->uRegType - psElem2->uRegType;
}

static
IMG_VOID FreeDualIssueInstData(PINTERMEDIATE_STATE		psState,
							   PDISSUE_STATE			psIssueState)
/*****************************************************************************
 FUNCTION	: FreeDualIssueInstData
    
 PURPOSE	: Free per-instruction dual-issue information.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;

	for (psListEntry = psIssueState->sStageDataList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PDUALISSUE_STAGEDATA	psDIData;

		psNextListEntry = psListEntry->psNext;
		psDIData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sStageDataListEntry);
		FreeDualIssueData(psState, psIssueState, psDIData->psInst);
	}

	UscTreeDelete(psState, psIssueState->psNonTempTree);
	psIssueState->psNonTempTree = NULL;
}

static
IMG_VOID SetupDualIssueInstData(PINTERMEDIATE_STATE	psState,
							    PDISSUE_STATE		psIssueState,
								PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: SetupDualIssueInstData
    
 PURPOSE	: Sets up per-instruction dual-issue information.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psBlock			- Block to set up for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;

	ASSERT(psIssueState->psNonTempTree == NULL);
	psIssueState->psNonTempTree = UscTreeMake(psState, sizeof(NONTEMP), CmpNonTempCB);

	InitializeList(&psIssueState->sStageDataList);
	InitializeList(&psIssueState->sPrimaryMoveList);
	InitializeList(&psIssueState->sSecondaryMoveList);
	InitializeList(&psIssueState->sEFOWriteBackList);
	InitializeList(&psIssueState->sSSQList);
	InitializeList(&psIssueState->sDeschedList);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32				uSrcIdx;
		PDUALISSUE_STAGEDATA	psDIData;

		psInst->sStageData.psDIData = psDIData = NULL;

		/*	
			Build up a list of descheduling instructions in the block.
		*/
		if (IsDeschedulingPoint(psState, psInst))
		{
			psDIData = AllocateDualIssueData(psState, psIssueState, psInst);
			SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_DESCHED, 1);
			AppendToList(&psIssueState->sDeschedList, &psInst->sStageData.psDIData->sDeschedListEntry);
		}

		/*
			Check for an EFO instruction with an unused unified store destination writeback. We can use these
			instructions to insert a copy from an internal register to a temporary register.
		*/
		if (psInst->eOpcode == IEFO && psInst->u.psEfo->bIgnoreDest)
		{
			psDIData = AllocateDualIssueData(psState, psIssueState, psInst);
			SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_EFO, 1);
			AppendToList(&psIssueState->sEFOWriteBackList, &psInst->sStageData.psDIData->sEFOWriteBackListEntry);
		}

		/*
			Check for a MOV which could be the secondary operation in a dual-issue instruction. We can use these
			instructions to insert a copy from an internal register to a temporary register.
		*/
		if (
				psInst->eOpcode == IMOV && 
				psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL && 
				IsSecondaryInst(psState, psInst)
			)
		{
			psDIData = AllocateDualIssueData(psState, psIssueState, psInst);
			SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_SECONDARYMOVE, 1);
			AppendToList(&psIssueState->sSecondaryMoveList, &psInst->sStageData.psDIData->sSecondaryMoveListEntry);
		}

		if(IsSecondaryInst(psState, psInst))
		{
			SetBit(psInst->auFlag, INST_DI_SECONDARY, 1);
		}
		
		/*
			Skip instructions which can't be a primary operation in a dual-issued instruction.
		*/
		if (!CheckIsPrimaryInst(psState, psInst))
		{
			continue;
		}

		psDIData = AllocateDualIssueData(psState, psIssueState, psInst);
		SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_PRIMARYINST, 1);
		
		SetBit(psInst->auFlag, INST_DI_PRIMARY, 1);

		/*
			Check for a instruction which could be a sum-of-squares primary operation in a dual-issued instruction. We can
			insert a copy to an internal register from a unified store register here.
		*/
		if (IsSSQPrimary(psState, psInst))
		{
			SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_SSQ, 1);
			AppendToList(&psIssueState->sSSQList, &psInst->sStageData.psDIData->sSSQListEntry);
		}

		/*
			Check for a MOV instruction which could be the primary operation in a dual-issued instruction. We can
			insert a copy to an internal register from a unified store register here.
		*/
		if (psInst->eOpcode == IMOV)
		{
			SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_PRIMARYMOVE, 1);
			AppendToList(&psIssueState->sPrimaryMoveList, &psInst->sStageData.psDIData->sPrimaryMoveListEntry);
		}

		/*
			Check for an instruction which will have a constant source if converted to the primary operation
			of a dual-issue instruction. We can insert a secondary operation which copies the constant source
			to an internal register.
		*/
		switch (psInst->eOpcode)
		{
			/* ADD A, B, C -> ADM A, B, C, 1 */
			case IFADD:	
			{
				AddAsImplicitConstSite(psState, psIssueState, psInst, EURASIA_USE_SPECIAL_CONSTANT_FLOAT1); 
				break;
			}
			/* MUL A, B, C -> MAD A, B, C, 0 */
			case IFMUL:		
			{
				AddAsImplicitConstSite(psState, psIssueState, psInst, EURASIA_USE_SPECIAL_CONSTANT_ZERO); 
				break;
			}
			default: break;
		}

		for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
		{
			PARG	psSrc = &psInst->asArg[uSrcIdx];

			if (
					(
						psSrc->uType == USEASM_REGTYPE_FPCONSTANT ||
						psSrc->uType == USEASM_REGTYPE_IMMEDIATE ||
						psSrc->uType == USEASM_REGTYPE_SECATTR ||
						psSrc->uType == USC_REGTYPE_CALCSECATTR
					 ) &&
					 psSrc->uIndexType == USC_REGTYPE_NOINDEX
				)
			{
				AddAsSharedSrcSite(psState, psIssueState, psInst, uSrcIdx);
			}
		}
	}
}

static
IMG_VOID ReplaceByInternalRegister(PINTERMEDIATE_STATE		psState,
								   PDISSUE_STATE			psIssueState,
								   IMG_UINT32				uTempNum,
								   IMG_UINT32				uIReg)
/*****************************************************************************
 FUNCTION	: ReplaceByInternalRegister
    
 PURPOSE	: Replaces a temporary everywhere by an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  uTempNum			- Temporary to replace.
			  uIReg				- Internal register to replace by.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL		bIsSecondaryInst;
	PUSEDEF_CHAIN	psUseDef;
	PUSEDEF			psDef;
	PINST			psDefInst;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempNum);

	/*
		Replace the definition.
	*/
	ASSERT(psUseDef->psDef != NULL);
	psDef = psUseDef->psDef;
	ASSERT(psDef->eType == DEF_TYPE_INST);
	psDefInst = psDef->u.psInst;
	SetDest(psState, psDefInst, psDef->uLocation, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);

	bIsSecondaryInst = IsSecondaryInst(psState, psDefInst);
	if(bIsSecondaryInst)
	{
		SetBit(psDefInst->auFlag, INST_DI_SECONDARY, 1);
	}
	/*
		If we've replaced the destination of an MOV by an internal register then we might have
		created a suitable secondary operation for a dual-issue.
	*/
	if (psDefInst->eOpcode == IMOV && bIsSecondaryInst)
	{
		PDUALISSUE_STAGEDATA	psDIData;

		psDIData = AllocateDualIssueData(psState, psIssueState, psDefInst);
		SetBit(psDIData->auFlags, DUALISSUE_STAGEDATA_FLAGS_SECONDARYMOVE, 1);
		AppendToList(&psIssueState->sSecondaryMoveList, &psDefInst->sStageData.psDIData->sSecondaryMoveListEntry);
	}

	/*
		Replace all of the uses.
	*/
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF		psUse;
		PINST		psUseInst;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse != psUseDef->psDef)
		{
			ASSERT(psUse->eType == USE_TYPE_SRC);
			psUseInst = psUse->u.psInst;

			SetSrc(psState, psUseInst, psUse->uLocation, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
		}
	}
}

static
IMG_VOID ReplaceSecondaryArguments(PINTERMEDIATE_STATE	psState,
								   PDISSUE_STATE		psIssueState,
								   PDUALISSUE_SECONDARY	psSec)
/*****************************************************************************
 FUNCTION	: ReplaceSecondaryArguments
    
 PURPOSE	: For any arguments to a dual-issued secondary operation which
			  can be replaced everywhere by an internal register: replace them

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psSec				- Information about the secondary operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOpIdx;

	for (uOpIdx = 0; uOpIdx < psSec->uConvertCount; uOpIdx++)
	{
		PCONVERT_OPERAND	psOp = &psSec->asConvert[uOpIdx];

		if (GetBit(&psSec->uReplace, uOpIdx))
		{
			ReplaceByInternalRegister(psState, 
									  psIssueState, 
									  psOp->sOperand.uNumber, 
									  psOp->uIReg);
		}
	}
}

static
IMG_BOOL CanReplaceByInternalRegister(PINTERMEDIATE_STATE	psState,
									  PDISSUE_STATE			psIssueState,
									  PCODEBLOCK			psBlock,
									  IMG_UINT32			uTempNum,
									  IMG_UINT32			uIReg)
/*****************************************************************************
 FUNCTION	: CanReplaceByInternalRegister
    
 PURPOSE	: Checks if its possible to replaces a temporary everywhere by an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psBlock			- Current block.
			  uTempNum			- Temporary to replace.
			  uIReg				- Internal register to replace by.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSEDEF			psDef;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psLastUseInst;
	PINST			psDefInst;

	/*
		Check if this temporary is part of a group of registers which require consecutive
		hardware register numbers then it can't be replaced by an internal register.
	*/
	if (IsNodeInGroup(FindRegisterGroup(psState, uTempNum)))
	{
		return IMG_FALSE;
	}

	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempNum);

	/*
		If this temporary is defined in the driver prolog then it can't be replaced by an
		internal register.
	*/
	ASSERT(psUseDef->psDef != NULL);
	psDef = psUseDef->psDef;
	if (psDef->eType != DEF_TYPE_INST)
	{
		return IMG_FALSE;
	}
	psDefInst = psDef->u.psInst;

	/*
		If this temporary is defined in another block then it can't be replaced by an internal
		register (we never allow internal registers to be live between blocks).
	*/
	if (psDefInst->psBlock != psBlock)
	{
		return IMG_FALSE;
	}

	/*
		If the temporary is partially/conditionally written then it can't be replaced by an 
		internal register. 
	*/
	if (GetPreservedChansInPartialDest(psState, psDefInst, psDef->uLocation) != 0)
	{
		return IMG_FALSE;
	}
	
	/*
		Check an internal register is supported as a destination by this instruction.
	*/
	if (!CanUseDest(psState, psDefInst, psDef->uLocation, USEASM_REGTYPE_FPINTERNAL, USC_REGTYPE_NOINDEX))
	{
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
		if (psReader->psBlock != psBlock)
		{
			return IMG_FALSE;
		}
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

	/*
		Check for no uses of the internal register in the block.
	*/
	if (psLastUseInst == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Check if the interval where the temporary is live contains either a deschedule or another
		use of the same internal register.
	*/
	if (!CouldReplaceIntervalByInternalRegister(psState, psIssueState, psDefInst, psLastUseInst, uIReg))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsSecondaryMove(PINTERMEDIATE_STATE	psState,
						 PDISSUE_STATE			psIssueState,
						 PINST					psInst,
						 IMG_UINT32				uIReg,
						 PINST					psInstP,
						 PINST					psInstS,
						 IMG_BOOL				bForSrc)
/*****************************************************************************
 FUNCTION	: IsSecondaryMove
    
 PURPOSE	: Checks if an instruction is a suitable point to insert a copy
			  of data to/from an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psBlock			- Current block.
			  uIReg				- Internal register to copy to/from.
			  psInstP		- Location of the dual-issue instruction.
			  psInstS		- Location of the other instruction in the
								dual-issued pair.
			  bForSrc			- TRUE for a copy to an internal register.
								  FALSE for a copy from an internal register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psNewIntervalStart;
	PINST		psNewIntervalEnd;

	/*
		Skip instructions which are part of the dual-issue operation we are trying
		to create.
	*/
	if (psInst == psInstS)
	{
		return IMG_FALSE;
	}

	if (bForSrc)
	{
		/*
			Skip instructions after the site of the dual-issued instructions.
		*/
		if (psInst->uBlockIndex >= psInstP->uBlockIndex)
		{
			return IMG_FALSE;
		}
		psNewIntervalStart = psInst;
		psNewIntervalEnd = psInstP;
	}
	else
	{
		/*
			Skip instructions before the site of the dual-issued instruction.
		*/
		if (psInst->uBlockIndex <= psInstP->uBlockIndex)
		{
			return IMG_FALSE;
		}
		psNewIntervalStart = psInstP;
		psNewIntervalEnd = psInst;
	}
	/*
		Check if the interval between the move site and the dual-issued instructions
		contains either a deschedule or another use of the same internal register.
	*/
	if (!CouldReplaceIntervalByInternalRegister(psState, psIssueState, psNewIntervalStart, psNewIntervalEnd, uIReg))
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_VOID StoreDualIssueMoveLocation(PINTERMEDIATE_STATE		psState,
									PDISSUE_STATE			psIssueState,
									PINST					psInstP,
									PINST					psInstS,
									PDUALISSUE_SECONDARY	psSec,
									IMG_UINT32				uOp,
									PINST					psMoveInst,
									DI_MOVE_TYPE			eMoveType,
									IMG_BOOL				bForSrc)
/*****************************************************************************
 FUNCTION	: StoreDualIssueMoveLocation
    
 PURPOSE	: Sets a possible location for a copy from/to an internal register.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  uIReg				- Internal register to copy to/from.
			  psInstP		- Location of the dual-issue instruction.
			  psInstS		- Location of the other instruction in the
								dual-issued pair.
			  psSec				- Information about the secondary operation.
			  uOp				- Secondary operand to copy.
			  psMoveInst		- Location for the copy.
			  eMoveType			- Type of copy.
			  bForSrc			- TRUE for a copy to an internal register.
								  FALSE for a copy from an internal register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uPrevOp;
	PCONVERT_OPERAND	psOp = &psSec->asConvert[uOp];
	IMG_BOOL			bReplace;

	/*
		Check if this instruction has already been chose as the site for a dual-issued move
		for an earlier operand.
	*/
	for (uPrevOp = 0; uPrevOp < uOp; uPrevOp++)
	{
		if (psSec->asConvert[uPrevOp].sMoveLoc.psInst == psMoveInst)
		{
			return;
		}
	}

	/*
		Check for an instruction which could have a move from the temporary to an
		internal register dual-issued.
	*/
	if (!IsSecondaryMove(psState, psIssueState, psMoveInst, psOp->uIReg, psInstP, psInstS, bForSrc))
	{
		return;
	}

	/*
		Look for the move site closest to the dual-issued instruction to minimize the
		lifetime of the internal register.
	*/
	bReplace = IMG_FALSE;
	if (psOp->sMoveLoc.psInst == NULL)
	{
		bReplace = IMG_TRUE;
	}
	else
	{
		if (bForSrc)
		{
			if (psOp->sMoveLoc.psInst->uBlockIndex < psMoveInst->uBlockIndex)
			{
				bReplace = IMG_TRUE;
			}
		}
		else
		{
			if (psOp->sMoveLoc.psInst->uBlockIndex > psMoveInst->uBlockIndex)
			{
				bReplace = IMG_TRUE;
			}
		}
	}
	if (bReplace)
	{
		psOp->sMoveLoc.eType = eMoveType;
		psOp->sMoveLoc.psInst = psMoveInst;
	}
}

static
IMG_VOID FindSecondaryMoveForSourceTemp(PINTERMEDIATE_STATE		psState,
										PDISSUE_STATE			psIssueState,
										PCODEBLOCK				psBlock,
										PDUALISSUE_SECONDARY	psSec,
										IMG_UINT32				uOpIdx,
										PINST					psInstP,
										PINST					psInstS)
/*****************************************************************************
 FUNCTION	: FindSecondaryMoveForSourceTemp
    
 PURPOSE	: Looks for possible locations where a copy of data from a temporary
			  to an internal register could be inserted.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psBlock			- Current block.
			  psSec				- Information about the secondary operation.
			  uOpIdx			- Secondary operand to copy.
			  psInstP		- Location of the dual-issue instruction.
			  psInstS		- Location of the other instruction in the
								dual-issued pair.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN		psUseDef;
	PUSEDEF				psDef;
	PUSC_LIST_ENTRY		psListEntry;
	IMG_UINT32			uTempNum;
	PCONVERT_OPERAND	psOp = &psSec->asConvert[uOpIdx];

	ASSERT(psOp->sOperand.uType == USEASM_REGTYPE_TEMP);
	uTempNum = psOp->sOperand.uNumber;

	psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, uTempNum);

	psDef = psUseDef->psDef;

	/*
		Check if we could change the instruction where the temporary is written so that it writes
		the same data both into the temporary and into an internal register.
	*/
	if (psDef != NULL && psDef->eType == DEF_TYPE_INST)
	{
		PINST	psDefInst = psDef->u.psInst;

		if (
				psDefInst->psBlock == psBlock && 
				IsDualIssueWithSelf(psState, psDefInst, psOp->uIReg)
		   )
		{
			ASSERT(psDef->uLocation == 0);
			StoreDualIssueMoveLocation(psState, 
									   psIssueState, 
									   psInstP, 
									   psInstS, 
									   psSec, 
									   uOpIdx, 
									   psDefInst, 
									   DI_MOVE_TYPE_DUALWITHSELF,
									   IMG_TRUE /* bForSrc */);
		}
	}

	/*
		Look for locations where the temporary is used as a source and we could convert the
		instruction to a dual-issued primary and add a secondary move from the source to the
		internal register.
	*/
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;
		PINST	psUseInst;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		/*
			Skip uses of the temporary not in an instruction source.
		*/
		if (psUse->eType != USE_TYPE_SRC)
		{
			continue;
		}
		psUseInst = psUse->u.psInst;
		
		/*
			Skip instructions not in the same block as the dual-issued instruction.
		*/
		if (psUseInst->psBlock != psBlock)
		{
			continue;
		}

		/*
			Check for an instruction which can be dual-issued with a move.
		*/
		if (!IsPrimaryInst(psUseInst))
		{
			continue;
		}

		StoreDualIssueMoveLocation(psState, 
								   psIssueState, 
								   psInstP, 
								   psInstS,
								   psSec, 
								   uOpIdx, 
								   psUseInst, 
								   DI_MOVE_TYPE_SHAREDPRISRC,
								   IMG_TRUE /* bForSrc */);
	}
}

static
IMG_VOID FindSecondaryMoveForNonTemp(PINTERMEDIATE_STATE	psState,
									 PDISSUE_STATE			psIssueState,
									 PDUALISSUE_SECONDARY	psSec,
									 IMG_UINT32				uOpIdx,
									 PINST					psInstP,
									 PINST					psInstS)
/*****************************************************************************
 FUNCTION	: FindSecondaryMoveForSourceTemp
    
 PURPOSE	: Looks for possible locations where a copy of data from a non-temporary
			  to an internal register could be inserted.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psSec				- Information about the secondary operation.
			  uOpIdx			- Secondary operand to copy.
			  psInstP		- Location of the dual-issue instruction.
			  psInstS		- Location of the other instruction in the
								dual-issued pair.

 RETURNS	: Nothing.
*****************************************************************************/
{		
	NONTEMP				sKey;
	PNONTEMP			psNonTemp;
	PCONVERT_OPERAND	psOp = &psSec->asConvert[uOpIdx];

	sKey.uRegType = psOp->sOperand.uType;
	sKey.uRegNumber = psOp->sOperand.uNumber;
	psNonTemp = UscTreeGetPtr(psIssueState->psNonTempTree, &sKey);
	if (psNonTemp != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;
		PINST			psBestMoveInst;

		psBestMoveInst = NULL;
		for (psListEntry = psNonTemp->sMoveSiteList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{	
			PNONTEMP_MOVESITE	psMoveSite;
			PINST				psMoveSiteInst;

			psMoveSite = IMG_CONTAINING_RECORD(psListEntry, PNONTEMP_MOVESITE, sNonTempListEntry);
			psMoveSiteInst = psMoveSite->psInst;

			/*
				Look for the move site closest to the dual-issued instruction to minimize the
				lifetime of the internal register.
			*/
			StoreDualIssueMoveLocation(psState, 
									   psIssueState, 
									   psInstP, 
									   psInstS,
									   psSec, 
									   uOpIdx, 
									   psMoveSiteInst, 
									   DI_MOVE_TYPE_SHAREDPRISRC,
									   IMG_TRUE /* bForSrc */);
		}
	}
}

static
IMG_VOID FindSecondaryMoveForSource(PINTERMEDIATE_STATE		psState,
								    PDISSUE_STATE			psIssueState,
									PCODEBLOCK				psBlock,
									PINST					psSecInst,
									PDUALISSUE_SECONDARY	psSec,
									IMG_UINT32				uOpIdx,
									PINST					psPriInst)
/*****************************************************************************
 FUNCTION	: FindSecondaryMoveForSource
    
 PURPOSE	: Looks for possible locations where a copy of data to an internal 
			  register could be inserted.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psBlock			- Current block.
			  psSecInst			- Secondary operation.
			  psSec				- Information about the secondary operation.
			  uOpIdx			- Secondary operand to copy.
			  psPriInst			- Primary operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCONVERT_OPERAND	psOp = &psSec->asConvert[uOpIdx];
	PARG				psOperand = &psOp->sOperand;
	PUSC_LIST_ENTRY		psListEntry;
	PINST				psDefInst;

	/*
		If the operand is defined in the current block then get the ID of the
		instruction defining it.
	*/
	psDefInst = NULL;
	if (psOperand->uType == USEASM_REGTYPE_TEMP)
	{
		PUSEDEF_CHAIN	psOpUseDef;
		PUSEDEF			psOpDef;

		psOpUseDef = UseDefGet(psState, psOperand->uType, psOperand->uNumber);
		
		psOpDef = psOpUseDef->psDef;
		if (psOpDef != NULL)
		{
			ASSERT(UseDefIsDef(psOpDef));

			if (psOpDef->eType == DEF_TYPE_INST && psOpDef->u.psInst->psBlock == psBlock)
			{
				psDefInst = psOpDef->u.psInst;
			}
		}
	}

	if (IsValidDualSource0(psState, psSecInst, &psOp->sOperand))
	{
		for (psListEntry = psIssueState->sSSQList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PDUALISSUE_STAGEDATA	psSSQData;
			PINST					psSSQInst;

			psSSQData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sSSQListEntry);
			psSSQInst = psSSQData->psInst;

			/*
				Skip instructions before the instruction where a temporary operand is defined.
			*/
			if (psDefInst != NULL && psDefInst->uBlockIndex >= psSSQInst->uBlockIndex)
			{
				continue;
			}

			StoreDualIssueMoveLocation(psState,	
									   psIssueState,
									   psPriInst, 
									   psSecInst,
									   psSec, 
									   uOpIdx, 
									   psSSQInst, 
									   DI_MOVE_TYPE_SHAREDPRISRC,
									   IMG_TRUE /* bForSrc */);
		}
	}

	for (psListEntry = psIssueState->sPrimaryMoveList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PDUALISSUE_STAGEDATA	psMovData;
		PINST					psMovInst;

		psMovData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sPrimaryMoveListEntry);
		psMovInst = psMovData->psInst;

		/*
			Skip instructions before the instruction where a temporary operand is defined.
		*/
		if (psDefInst != NULL && psDefInst->uBlockIndex >= psMovInst->uBlockIndex)
		{
			continue;
		}

		StoreDualIssueMoveLocation(psState, 
								   psIssueState,
								   psPriInst, 
								   psSecInst, 
								   psSec, 
								   uOpIdx, 
								   psMovInst, 
								   DI_MOVE_TYPE_PRIMOV,
								   IMG_TRUE /* bForSrc */);
	}
}

static
PINST GetFirstUseInst(PINTERMEDIATE_STATE	psState, 
					  PCODEBLOCK			psBlock,
					  PARG					psArg)
/*****************************************************************************
 FUNCTION	: GetFirstUseInst
    
 PURPOSE	: Gets the earliest instruction in a particular block which
			  uses a register.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to check.
			  psArg				- Register to check for.

 RETURNS	: The the earliest instruction or NULL if the register
			  isn't used in the block.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psFirstUseInst;

	psUseDef = UseDefGet(psState, psArg->uType, psArg->uNumber);
	ASSERT(psUseDef != NULL);

	psFirstUseInst = NULL;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse->eType >= USE_TYPE_FIRSTINSTUSE && psUse->eType <= USE_TYPE_LASTINSTUSE)
		{
			PINST	psUseInst = psUse->u.psInst;

			if (psUseInst->psBlock == psBlock)
			{
				if (psFirstUseInst == NULL || psFirstUseInst->uBlockIndex > psUseInst->uBlockIndex)
				{
					psFirstUseInst = psUseInst;
				}
			}
		}
	}

	return psFirstUseInst;
}

static 
IMG_VOID CheckReplaceSecondaryArguments(PINTERMEDIATE_STATE		psState,
										PINST					psSecInst,
									    PDUALISSUE_SECONDARY	psSec,
									    PDISSUE_STATE			psIssueState,
									    PCODEBLOCK				psBlock,
									    PINST					psPriInst)
/*****************************************************************************
 FUNCTION	: CheckReplaceSecondaryArguments
    
 PURPOSE	: Checks which arguments to the secondary operation could be completely
			  replaced by internal registers; and for those that can't are there
			  any locations where a copy to/from an internal register could be
			  dual-issued.

 PARAMETERS	: psState			- Compiler state.
			  psSecInst			- Secondary operation.
			  psSec				- Information about the secondary operation.
			  psIssueState		- Module state.
			  psBlock			- Current block.
			  psPriInst				- Primary operation.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOpIdx;

	for (uOpIdx = 0; uOpIdx < psSec->uConvertCount; uOpIdx++)
	{
		PCONVERT_OPERAND	psOp = &psSec->asConvert[uOpIdx];
		PARG				psArg = &psOp->sOperand;

		/*
			Check for a secondary source/destination which could be completely replaced
			by an internal register.
		*/
		if (psArg->uType == USEASM_REGTYPE_TEMP)
		{
			if (GetBit(&psSec->uReplace, uOpIdx))
			{
				if (CanReplaceByInternalRegister(psState, 
												 psIssueState, 
												 psBlock, 
												 psArg->uNumber, 
												 psOp->uIReg))
				{	
					continue;
				}
				SetBit(&psSec->uReplace, uOpIdx, 0);
			}
	
			/*
				Look for locations where a move from/to the secondary argument could be
				dual-issued.
			*/
			if (psSec->bConvertDest && uOpIdx == 0)
			{
				PUSC_LIST_ENTRY	psListEntry;
				PINST			psFirstUseInst;

				/*
					Get the first instruction in the block using the secondary argument.
				*/
				psFirstUseInst = GetFirstUseInst(psState, psBlock, &psOp->sOperand);

				/*
					Look for instructions which could be the secondary operation in a 
					dual-issued instruction with a move as the primary.
				*/
				for (psListEntry = psIssueState->sSecondaryMoveList.psHead; 
					 psListEntry != NULL; 
					 psListEntry = psListEntry->psNext)
				{
					PDUALISSUE_STAGEDATA	psMovData;

					psMovData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sSecondaryMoveListEntry);

					/*
						Skip instructions which are after the first use of the secondary argument.
					*/
					if (psFirstUseInst != NULL && psMovData->psInst->uBlockIndex >= psFirstUseInst->uBlockIndex)
					{
						continue;
					}

					StoreDualIssueMoveLocation(psState,
											   psIssueState,
											   psPriInst,
											   psSecInst,
											   psSec,
											   uOpIdx,
											   psMovData->psInst,
											   DI_MOVE_TYPE_SECMOV,
											   IMG_FALSE /* bForSrc */);
				}

				/*
					Look for EFO instructions with unused unified store destination writebacks.
				*/
				if (psOp->uIReg < USC_NUM_EFO_IREGISTERS)
				{
					for (psListEntry = psIssueState->sEFOWriteBackList.psHead; 
						 psListEntry != NULL; 
						 psListEntry = psListEntry->psNext)
					{
						PDUALISSUE_STAGEDATA	psEfoData;
	
						psEfoData = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_STAGEDATA, sEFOWriteBackListEntry);

						/*
							Skip instructions which are after the first use of the secondary argument.
						*/
						if (psFirstUseInst != NULL && psEfoData->psInst->uBlockIndex >= psFirstUseInst->uBlockIndex)
						{
							continue;
						}

						StoreDualIssueMoveLocation(psState,
												   psIssueState,
												   psPriInst,
												   psSecInst,
												   psSec,
												   uOpIdx,
												   psEfoData->psInst,
												   DI_MOVE_TYPE_EFOWB,
												   IMG_FALSE /* bForSrc */);
					}
				}
			}
			else
			{
				FindSecondaryMoveForSourceTemp(psState, 
											   psIssueState,
											   psBlock, 
											   psSec,
											   uOpIdx,
											   psPriInst, 
											   psSecInst);
			}
		}
		else
		{
			/*
				Look for locations where a copy of the non-temporary data to an internal
				register could be inserted.
			*/
			FindSecondaryMoveForNonTemp(psState, 
										psIssueState,
										psSec,
										uOpIdx,
										psPriInst, 
										psSecInst);
		}

		/*
			Look for locations where a copy of any kind of data to an internal register
			could be inserted.
		*/
		if (!(psSec->bConvertDest && uOpIdx == 0))
		{
			FindSecondaryMoveForSource(psState,
									   psIssueState,
									   psBlock,
									   psSecInst,
									   psSec,
									   uOpIdx,
									   psPriInst);
		}
	}
}

static
IMG_BOOL ShouldDualIssue(PINTERMEDIATE_STATE psState, 
						 PDUALISSUE_SECONDARY psSec,
						 PINST psInstS)
/*****************************************************************************
 FUNCTION	: ShouldDualIssue
    
 PURPOSE	: Decide whether to dual-issue a group of instructions.

 PARAMETERS	: psState		- Compiler state.
			  psInstS		- The secondary instruction (with all the data).

 RETURNS	: True if instructions should be dual-issued.
*****************************************************************************/
{
	IMG_UINT32 uCtr, uNumMoves;

	ASSERT(psInstS != NULL);

	/*
		Check the number of extra MOV instructions.
	*/
	uNumMoves = 0;
	for (uCtr = 0; uCtr < psSec->uConvertCount; uCtr++)
	{
		PCONVERT_OPERAND	psCvtOp = &psSec->asConvert[uCtr];

		/*
			A move is required if we can't completely replace
			the operand by an internal register and there's no
			other instruction where we could insert a move for
			free.
		*/
		if (
				!(psSec->uReplace & (1 << uCtr)) &&
				psCvtOp->sMoveLoc.psInst == NULL
		   )
		{
			uNumMoves++;
		}
	}

	if (uNumMoves > 0)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_VOID FinalisePair(PINTERMEDIATE_STATE	psState, 
					  PDUALISSUE_SECONDARY	psSec,
					  PINST					psInst,
					  PINST					psInstS)
/*****************************************************************************
 FUNCTION	: FinalisePair
    
 PURPOSE	: Apply final conversions to a dual-instruction pair.

 PARAMETERS	: psState	- Compiler state
			  psFirst	- First instruction in the pair.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uCtr;
	PDUAL_PARAMS psDual = psInst->u.psDual;
	IMG_UINT32 uUsedIRegMask;
	IMG_UINT32 uIReg;
	IMG_BOOL bUsesSrc2;

	/*
		Copy the secondary opcode.
	*/
	psInst->u.psDual->eSecondaryOp = psInstS->eOpcode;

	/*
		Need to swap the primary sources?
	*/
	if (psSec->bCommutePriSrc01)
	{
		ARG			sTemp;
		IMG_UINT32	uTempComponent;

		/*
			Save the details of the first source.
		*/
		sTemp = psInst->asArg[0];
		uTempComponent = psDual->auSourceComponent[0];

		/*
			Copy the second source over the first source.
		*/
		SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, &psInst->asArg[1]);
		psDual->auSourceComponent[0] = psDual->auSourceComponent[1];

		/*
			Copy the saved source over the second source.
		*/
		SetSrcFromArg(psState, psInst, 1 /* uSrcIdx */, &sTemp);
		psDual->auSourceComponent[1] = uTempComponent;

		/*
			Swap a negation modifier on the second source to the first source.
		*/
		ASSERT(!psDual->abPrimarySourceNegate[0]);
		if (psDual->abPrimarySourceNegate[1])
		{
			psDual->abPrimarySourceNegate[1] = IMG_FALSE;
			NegatePrimarySource0(psState, psInst);
		}
	}

	/*
		Convert ADD -> SUB to apply a negation modifier.
	*/
	if (psInstS->eOpcode == IFADD && (IsNegated(psState, psInstS, 0) || IsNegated(psState, psInstS, 1)))
	{
		psInst->u.psDual->eSecondaryOp = IFSUB;
	}

	/*
		Is source 2 used by the primary?
	*/
	if (psDual->ePrimaryOp == IMOV)
	{
		bUsesSrc2 = IMG_FALSE;
	}
	else
	{
		bUsesSrc2 = IMG_TRUE;
	}

	/*
		Copy the secondary destination.
	*/
	ASSERT(psSec->uDestIReg < USC_NUM_DUALISSUE_IREGISTERS);
	SetDest(psState, psInst, 1 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, psSec->uDestIReg, UF_REGFORMAT_F32);
	psInst->auDestMask[1] = psInstS->auDestMask[0];
	psInst->auLiveChansInDest[1] = psInstS->auLiveChansInDest[0];

	/*
		If required encode a secondary source into a
		source not used by the primary.
	*/
	if (psSec->bCopySourceFromSecondary)
	{
		ASSERT(psInst->asArg[2].uType == USC_REGTYPE_UNUSEDSOURCE);
		SetSrcFromArg(psState, psInst, 2 /* uSrcIdx */, &psSec->sPriSource2.sArg);
		SetComponentSelect(psState, psInst, 2 /* uSrcIdx */, psSec->sPriSource2.uComponent);
	}

	/*
		Copy the secondary operands.
	*/
	uUsedIRegMask = 0;
	psDual->uSecSourcesUsed = 0;
	for (uCtr = 0; uCtr < psInstS->uArgumentCount; uCtr++)
	{
		DUAL_SEC_SRC	eSrc = psSec->aeSecondarySrcs[uCtr];

		psDual->aeSecondarySrcs[uCtr] = eSrc;
		psDual->uSecSourcesUsed |= 1 << eSrc;

		/*
			Record which internal registers are used by the secondary operation.
		*/
		if (eSrc == DUAL_SEC_SRC_I0 ||
			eSrc == DUAL_SEC_SRC_I1 ||
			eSrc == DUAL_SEC_SRC_I2)
		{
			uUsedIRegMask |= (1 << (eSrc - DUAL_SEC_SRC_I0));
		}
		else
		{
			ASSERT(eSrc == DUAL_SEC_SRC_PRI0 || eSrc == DUAL_SEC_SRC_PRI1 || eSrc == DUAL_SEC_SRC_PRI2);
		}
	}
	if (psSec->bUsesSrc0)
	{
		IMG_UINT32	uSecondaryGPIDest = psInst->asDest[1].uNumber;

		ASSERT(psDual->aeSecondarySrcs[0] == (DUAL_SEC_SRC)(DUAL_SEC_SRC_I0 + uSecondaryGPIDest) ||
			   (psDual->aeSecondarySrcs[0] == DUAL_SEC_SRC_PRI0 && uSecondaryGPIDest == 0));
	}

	/*
		Add extra sources for implicitly referenced
		internal registers.
	*/
	for (uIReg = 0; uIReg < USC_NUM_DUALISSUE_IREGISTERS; uIReg++)
	{
		if (uUsedIRegMask & (1 << uIReg))
		{
			SetSrc(psState, psInst, 3 + uIReg /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);
		}
		else
		{
			SetSrcUnused(psState, psInst, 3 + uIReg /* uSrcIdx */);
		}
	}
}

static
PINST CreateIRegMove(PINTERMEDIATE_STATE	psState,
					 PINST					psSecInst,
					 PINST					psInstP,
					 IMG_BOOL				bIRegToDest,
					 IMG_UINT32				uIReg,
					 PARG					psUSReg)
/*****************************************************************************
 FUNCTION	: CreateIRegMove
    
 PURPOSE	: Create an instruction which moves data between an internal
			  register and a unified store source.

 PARAMETERS	: psState			- Compiler state.
			  psSecInst			- Source line information.
			  psInstP		- Point to insert the move.
			  bIRegToDest		- Direction for move.
			  uIReg				- Number of the internal register.
			  psUSReg			- Unified store source.
			  
 RETURNS	: A pointer to the created instruction.
*****************************************************************************/
{
	PINST	psMoveInst;

	psMoveInst = AllocateInst(psState, psSecInst);
	SetOpcode(psState, psMoveInst, IMOV);

	if (bIRegToDest)
	{
		/* Move from ireg to destination */
		SetDestFromArg(psState, psMoveInst, 0 /* uDestIdx */, psUSReg);

		/* Move from ireg to destination */
		SetSrc(psState, psMoveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);

		/* Add to the code block */
		InsertInstBefore(psState, psInstP->psBlock, psMoveInst, psInstP->psNext);
	}
	else
	{
		/* Move from source to ireg. */
		SetSrcFromArg(psState, psMoveInst, 0 /* uSrcIdx */, psUSReg);

		/* Move from source to ireg. */
		SetDest(psState, psMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_FPINTERNAL, uIReg, UF_REGFORMAT_F32);

		/* Add to the code block */
		InsertInstBefore(psState, psInstP->psBlock, psMoveInst, psInstP);
	}

	return psMoveInst;
}

static
IMG_VOID CreateSecondaryMoves(PINTERMEDIATE_STATE	psState,
							  PDISSUE_STATE			psIssueState,
							  PDUALISSUE_SECONDARY	psSec,
							  PINST					psInstP,
							  PINST					psSecInst)
/*****************************************************************************
 FUNCTION	: CreateSecondaryMoves
    
 PURPOSE	: Create an instruction which moves data between an internal
			  register and a unified store source.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Module state.
			  psInstP		- Location of the combined instruction.
			  psSecInst			- Secondary instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uCtr;

	/* 
	   Emit the move for each operand (if any).  A move is only needed
	   if the secondary instruction has an operand replaced with a
	   previously unused iregister and the iregister can't replace the
	   operand throughout the code-block.

	   Update the iregister usage from the secondary's operands. Note
	   that iregister useage for other instruction pairs is not
	   updated. This means that iregisters that have an assignment in
	   the current issue state are unlikely to be available for later
	   pairs. Any dual-issue pairs that are missed in the current
	   group because of unavailable iregisters should be picked up
	   when subsequent groups are formed.
	 */
	for (uCtr = 0; uCtr < psSec->uConvertCount; uCtr++)
	{
		PCONVERT_OPERAND	psCvtOp = &psSec->asConvert[uCtr];
		PDI_MOVE_LOC		psLoc = &psCvtOp->sMoveLoc;
		IMG_UINT32			uIReg = psCvtOp->uIReg;
		PARG				psOperand = &psCvtOp->sOperand;
		IMG_BOOL			bDualMoved = IMG_FALSE;
		IMG_BOOL			bForDest;
		PINST				psMoveLoc;

		/*
			Skip operand we are completely replacing by
			internal registers.
		*/
		if (GetBit(&psSec->uReplace, uCtr))
		{
			continue;
		}

		bForDest = IMG_FALSE;
		if (psSec->bConvertDest && uCtr == 0)
		{
			bForDest = IMG_TRUE;
		}

		psMoveLoc = psCvtOp->sMoveLoc.psInst;

		if (psMoveLoc != NULL)
		{
			DualMove(psState, psIssueState, psLoc, psSecInst, uIReg, psOperand);
			
			/* Managed to dual-issue the move. */
			bDualMoved = IMG_TRUE;
		}
		if (!bDualMoved)
		{
			/* Create a seperate MOV instruction. */
			psMoveLoc = CreateIRegMove(psState, psSecInst, psInstP, bForDest, uIReg, psOperand);		
		}
	}
}

static
IMG_VOID DualIssuePair(PINTERMEDIATE_STATE	psState,
					   PDISSUE_STATE		psIssueState,
					   PDUALISSUE_SECONDARY	psSec,
					   PCODEBLOCK			psBlock,
					   PINST				psPriInst,
					   PINST				psSecInst)
/*****************************************************************************
 FUNCTION	: DualIssuePair
    
 PURPOSE	: Make a dual-issue instruction from a pair of instructions

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Dual-issue state.
			  psBlock			- Block that contains both of the instructions.
			  psPriInst			- Primary instruction.
			  psSecInst			- Secondary instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/* Got a dual-issue pair that is compatible with current issue state. */
	ASSERT(psPriInst != NULL);
	ASSERT(psSecInst != NULL);
			
	/* 
	   Convert the primary and secondary instructions, generating the
	   instructions to move operands to/from iregisters. The move
	   instructions are generated by ConvertSecondaryInst and stored in
	   apsMoveInst.
	*/
	ConvertPrimaryInst(psState, psPriInst, psSec);

	RemoveInst(psState, psBlock, psSecInst);

	/* 
	   Emit moves to and from iregs. 
	*/
	CreateSecondaryMoves(psState, psIssueState, psSec, psPriInst, psSecInst);
		
	/* 	
	   A dual-issue pair was formed.
	*/

	/* 
	   Replace operands between the earliest writer and the last reader. 
	*/
	ReplaceSecondaryArguments(psState, psIssueState, psSec);

	/* Clean-up the pair, setting the issue type in the pair's instructions. */
	FinalisePair(psState, psSec, psPriInst, psSecInst);

	/*
		Free information about the two instructions as possible locations of copies to/from
		internal registers.
	*/
	FreeDualIssueData(psState, psIssueState, psPriInst);
	FreeDualIssueData(psState, psIssueState, psSecInst);

	/* Free the instruction that was moved to the secondary operation. */
	FreeInst(psState, psSecInst);
}

static
IMG_BOOL AssignDualIssueInternalRegisters(PINTERMEDIATE_STATE	psState,
										  PDISSUE_STATE			psIssueState,
										  PDUALISSUE_SECONDARY	psSec,
										  PCODEBLOCK			psBlock,
										  PINST					psInstP,
										  PINST					psInstS)
/*****************************************************************************
 FUNCTION	: AssignDualIssueInternalRegisters
    
 PURPOSE	: Try to assign internal registers for the sources/destinations of
			  the secondary operation of a dual-issue instruction.

 PARAMETERS	: psState			- Compiler state.
			  psIssueState		- Dual-issue state.
			  psSec				- Returns information about the internal registers
								assigned.
			  psBlock			- Block that contains both of the instructions.
			  psInstP			- Primary instruction.
			  psInstS			- Secondary instruction.
			  
 RETURNS	: TRUE if a dual-issue instruction should be created.
*****************************************************************************/
{
	/* 
	   Get the settings need to convert the secondary instruction. This
	   includes the internal register to use and which operands must be
	   replaced by iregisters.
	*/
	if (!GetDualIssueSettings(psState, 
							  psInstP,
							  psInstS,
							  psSec))
	{
		/* Failed to get settings. */
		return IMG_FALSE;
	}

	/*
		Check for sources/destinations to the secondary operation could be replaced
		by internal registers. Or if replacement isn't possible where we could insert
		copies to/from internal registers.
	*/
	CheckReplaceSecondaryArguments(psState,
								   psInstS,
								   psSec,
								   psIssueState,
								   psBlock,
								   psInstP);

	/* 
	   Calculate cost of dual-issuing with this pair and make the
	   decision about whether to dual-issue the pair.
	*/
	if (!ShouldDualIssue(psState, psSec, psInstS))
	{
		/* Won't dual-issue with current uPriInst and uSecInst, go on looking. */
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL CouldDualIssueInAnyOrder(PDGRAPH_STATE		psDepState,
								  PDISSUE_STATE		psIssueState,
								  PCODEBLOCK		psBlock,
								  PINST				psInstA,
								  PINST				psInstB,
								  IMG_BOOL			bACanPrimary,
								  IMG_BOOL			bACanSecondary)
/*****************************************************************************
 FUNCTION	: CouldDualIssueInAnyOrder
    
 PURPOSE	: Check whether two instructions could be dual issued.

 PARAMETERS	: psDepState		- Dependency state.
			  psIssueState		- Dual issue state.
			  psBlock			- Block that contains both instructions.
			  psStart			- Instruction to start looking from.
			  psInstA			- The first instruction for the dual-issue.
			  psInstA			- The second instruction for the dual-issue.
			  bACanPrimary		- Whether uInstA can be primary instruction.
			  bACanSecondary	- Whether uInstA can be secondary instruction

 RETURNS	: True iff instructions could be dual issued.
*****************************************************************************/
{
	IMG_BOOL			bGotPairAB, bGotPairBA, bBCanPrimary, bBCanSecondary;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	if (psInstB == psInstA)
	{
		return IMG_FALSE;
	}

	/* Look for a match for instruction uInstA */
	bGotPairAB = IMG_FALSE;
	bGotPairBA = IMG_FALSE;
	bBCanPrimary = IMG_FALSE;
	bBCanSecondary = IMG_FALSE;

	if(bACanPrimary)
	{
		bBCanSecondary = GetBit(psInstB->auFlag, INST_DI_SECONDARY);
	}

	if(bACanSecondary)
	{
		bBCanPrimary = GetBit(psInstB->auFlag, INST_DI_PRIMARY);
	}

	/* Test for primary A, secondary B. */
	if ((bACanPrimary && bBCanSecondary) && AreCoInsts(psState, psInstA, psInstB))
	{
		bGotPairAB = IMG_TRUE;
	}

	if (bGotPairAB)
	{
		IMG_BOOL			bSuccess;
		DUALISSUE_SECONDARY	sSec;
		
		bSuccess = GetConversionSettings(psState, psBlock, psInstA, psInstB, &sSec);

		if (bSuccess)
		{
			bSuccess = AssignDualIssueInternalRegisters(psState,
														psIssueState,
														&sSec,
														psBlock,
														psInstA,
														psInstB);

			if (bSuccess)
			{
				DualIssuePair(psState,
							  psIssueState,
							  &sSec,
							  psBlock,
							  psInstA,
							  psInstB);

				return IMG_TRUE;
			}
		}
	}

	/* Test for secondary A, primary B. */
	if ((bACanSecondary && bBCanPrimary) && AreCoInsts(psState, psInstB, psInstA))
	{
		bGotPairBA = IMG_TRUE; 
	}

	if (bGotPairBA)
	{
		IMG_BOOL			bSuccess;
		DUALISSUE_SECONDARY	sSec;
		
		bSuccess = GetConversionSettings(psState, psBlock, psInstB, psInstA, &sSec);

		if (bSuccess)
		{
			bSuccess = AssignDualIssueInternalRegisters(psState,
														psIssueState,
														&sSec,
														psBlock,
														psInstB,
														psInstA);

			if (bSuccess)
			{
				DualIssuePair(psState,
							  psIssueState,
							  &sSec,
							  psBlock,
							  psInstB,
							  psInstA);

				return IMG_TRUE;
			}
		}
	}

	/* Can't dual-issue the current pair. */
	return IMG_FALSE;
}


static
IMG_BOOL FindSecondReadyInst(PDGRAPH_STATE	psDepState,
							 PDISSUE_STATE	psIssueState,
							 PCODEBLOCK		psBlock,
							 PINST			psInstA,
							 IMG_BOOL		bACanPrimary,
							 IMG_BOOL		bACanSecondary)
/*****************************************************************************
 FUNCTION	: FindSecondReadyInst
    
 PURPOSE	: Find second of a pair of dual-issue instructions that is compatible with
			  current settings, in the set of instructions that are ready to be scheduled.

 PARAMETERS	: psDepState		- Dependency state.
			  psIssueState		- Dual issue state.
			  psBlock			- Block that contains both instructions.
			  psInstA			- The first instruction for the dual-issue.
			  bACanPrimary		- Whether uInstA can be primary instruction.
			  bACanSecondary	- Whether uInstA can be secondary instruction

 RETURNS	: True iff instruction could be found.
*****************************************************************************/
{
	PINST				psInstB;
	PINTERMEDIATE_STATE	psState = psDepState->psState;

	/* 
	   Go through the code block looking for an instruction, psInstB, that can
	   be dual-issued with psInstA.
	*/
	InsertInstBefore(psState, psBlock, psInstA, NULL);
	for (psInstB = GetNextAvailable(psDepState, NULL); psInstB != NULL; psInstB = GetNextAvailable(psDepState, psInstB))
	{
		IMG_UINT32	uInst;
		IMG_BOOL	bSuccess;

		if (psInstA == psInstB)
		{
			continue;
		}
		
		/*
			Don't look at instructions more than 'uMaxInstMovement' instructions away from instruction A.
		*/
		if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
		{
			if (psInstB->uId < psInstA->uId && (psInstA->uId - psInstB->uId) > psState->uMaxInstMovement)
			{
				continue;
			}
			if (psInstB->uId > psInstA->uId && (psInstB->uId - psInstA->uId) > psState->uMaxInstMovement)
			{
				/*
					The available list is sorted by instruction ID so we've now tried all the possible
					pairs for instruction A.
				*/
				break;
			}
		}

		InsertInstBefore(psState, psBlock, psInstB, NULL);

		/* Schedule all other instructions in the block. */
		for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
		{
			PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);

			if (psInst != NULL && psInst != psInstA && psInst != psInstB)
			{
				InsertInstBefore(psState, psBlock, psInst, NULL);
			}
		}

		bSuccess = CouldDualIssueInAnyOrder(psDepState,
											psIssueState,
											psBlock,
											psInstA,
											psInstB,
											bACanPrimary,
											bACanSecondary);
		
		if (bSuccess)
		{
			return IMG_TRUE;
		}
		
		/* Remove the instructions that were just output */
		for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
		{
			PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);

			if (psInst != NULL && psInst != psInstA)
			{
				RemoveInst(psState, psBlock, psInst);
			}
		}
	}
	
	RemoveInst(psState, psBlock, psInstA);

	return IMG_FALSE;
}

static
IMG_BOOL OutputDependenciesNoDeschedule(PDGRAPH_STATE		psDepState,
										PCODEBLOCK			psBlock,
										PINST				psInst)
/*****************************************************************************
 FUNCTION	: OutputDependenciesNoDeschedule
    
 PURPOSE	: Output the instructions that an instruction depends on, unless they are
			  deschedule instructions.
			  
 PARAMETERS	: psDepState		- Dependency state.
			  psBlock			- Block into which to insert the instructions.
			  psInst			- The instruction whose dependencies we want to fulfill.

 RETURNS	: Return TRUE if all instructions were output, FALSE if some could not be
			  output because they were deschedule points.
*****************************************************************************/
{
	IMG_UINT32			uInst;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		PINST psSecondInst = ArrayGet(psState, psDepState->psInstructions, uInst);

		if (psSecondInst != NULL && 
			GraphGet(psState, psDepState->psClosedDepGraph, psInst->uId, uInst))
		{
			if (IsDeschedulingPoint(psState, psSecondInst))
			{
				return IMG_FALSE;
			}
			
			if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
			{
				if ((uInst >= psBlock->uInstCount && (uInst - psBlock->uInstCount > psState->uMaxInstMovement)) ||
					(uInst < psBlock->uInstCount && (psBlock->uInstCount - uInst > psState->uMaxInstMovement)))
				{
					return IMG_FALSE;
				}
			}

			InsertInstBefore(psState, psBlock, psSecondInst, NULL);
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL FindSecondNotReadyInst(PDGRAPH_STATE	psDepState,
								PDISSUE_STATE	psIssueState,
								PCODEBLOCK		psBlock,
								PINST			psInstA,
								IMG_BOOL		bACanPrimary,
								IMG_BOOL		bACanSecondary)
/*****************************************************************************
 FUNCTION	: FindSecondNotReadyInst
    
 PURPOSE	: Find second of a pair of dual-issue instructions that is compatible with
			  current settings, in the set of instructions that are not yet ready to be scheduled.

 PARAMETERS	: psDepState		- Dependency state.
			  psIssueState		- Dual issue state.
			  psBlock			- Block that contains both instructions.
			  psInstA			- The first instruction for the dual-issue.
			  bACanPrimary		- Whether uInstA can be primary instruction.
			  bACanSecondary	- Whether uInstA can be secondary instruction

 RETURNS	: True iff instruction could be found.
*****************************************************************************/
{
	IMG_UINT32			uInst;
	PINTERMEDIATE_STATE psState = psDepState->psState;
	IMG_UINT32			uBStart, uBLimit;

	/*
		Search for possible instructions to make a dual-issued pair in a 'uMaxInstMovement'
		sized window around instruction A.
	*/
	if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
	{
		if (psInstA->uId > psState->uMaxInstMovement)
		{
			uBStart = psInstA->uId - psState->uMaxInstMovement;
		}
		else
		{
			uBStart = 0;
		}
		if ((psDepState->uBlockInstructionCount - psInstA->uId) > psState->uMaxInstMovement)
		{
			uBLimit = psInstA->uId + psState->uMaxInstMovement;
		}
		else
		{
			uBLimit = psDepState->uBlockInstructionCount;
		}
	}
	else
	{
		uBStart = 0;
		uBLimit = min(psState->uMaxInstMovement, psDepState->uBlockInstructionCount);
	}

	/* 
	   Go through the instructions looking for an instruction, psInstB, that can
	   be dual-issued with psInstA. 
	*/
	for (uInst = uBStart; uInst < uBLimit; uInst++)
	{
		IMG_UINT32	uInst2;
		IMG_BOOL	bSuccess;
		PINST		psInstB = ArrayGet(psState, psDepState->psInstructions, uInst);
				
		if (psInstB == NULL ||
			IsEntryInList(&psDepState->sAvailableList, &psInstB->sAvailableListEntry) ||
			GraphGet(psState, psDepState->psClosedDepGraph, uInst, psInstA->uId))
		{
			continue;
		}
		
		/* Output all instructions needed to make psInstB ready, then output the pair. */
		bSuccess = OutputDependenciesNoDeschedule(psDepState, psBlock, psInstB);
		
		if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
		{
			if ((psInstA->uId >= psBlock->uInstCount && (psInstA->uId - psBlock->uInstCount > psState->uMaxInstMovement)) ||
				(psInstA->uId < psBlock->uInstCount && (psBlock->uInstCount - psInstA->uId > psState->uMaxInstMovement)))
			{
				bSuccess = IMG_FALSE;
			}
		}

		if (bSuccess)
		{
			InsertInstBefore(psState, psBlock, psInstA, NULL);
			InsertInstBefore(psState, psBlock, psInstB, NULL);

			/* Schedule all other instructions in the block. */
			for (uInst2 = 0; uInst2 < psDepState->uBlockInstructionCount; uInst2++)
			{
				PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst2);

				if (psInst != NULL && psInst != psInstA && psInst != psInstB && psInst->psBlock == NULL)
				{
					InsertInstBefore(psState, psBlock, psInst, NULL);
				}
			}
		
			bSuccess = CouldDualIssueInAnyOrder(psDepState,
										 psIssueState,
										 psBlock,
										 psInstA,
										 psInstB,
										 bACanPrimary,
										 bACanSecondary);

			if (bSuccess)
			{
				return IMG_TRUE;
			}
		}

		/* Remove the instructions that were just output */
		for (uInst2 = 0; uInst2 < psDepState->uBlockInstructionCount; uInst2++)
		{
			PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst2);

			if (psInst != NULL && psInst->psBlock != NULL)
			{
				RemoveInst(psState, psBlock, psInst);
			}
		}
	}

	return IMG_FALSE;
}

IMG_VOID static DualIssueBP(PINTERMEDIATE_STATE psState, 
						PCODEBLOCK psBlock,
						IMG_PVOID pvIssueState)
/*****************************************************************************
 FUNCTION	: DualIssueBP
    
 PURPOSE	: Try to dual issue instructions in a basic-block. Includes
	removing dead code and recalculating kill flags, if dead code was detected.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to optimize.
			  pvIssueState		- PDISSUE_STATE Structure to use for dual-issue data.

  RETURNS	: None, tho pvIssueState->bChanged set if anything done.
*****************************************************************************/
{
/*
  Run through the instructions in a code-block looking for a group of
  instruction pairs which can be dual-issued. An instruction-pair is added to
  the group if it can be assigned internal registers to replace operands in
  the secondary which are compatible with the ireg usage of existing members
  of the group.
*/
	PDISSUE_STATE	psIssueState = (PDISSUE_STATE)pvIssueState;
	PINST			psInstA, psNext;
	PDGRAPH_STATE	psDepState;
	IMG_UINT32		uFavourGPIReads = 0;

	ASSERT(psBlock);//->uType == CBTYPE_BASIC);
	ASSERT(psIssueState != NULL);

	/* Precompute data for instruction in the block. */
	SetupDualIssueInstData(psState, psIssueState, psBlock);

	/* 
	   Run through each instruction in the code-block, trying to find a group
	   beginning at each instruction. A group is found as a set of instruction
	   pairs that is compatible with the existing dual-issue state (assignment
	   to iregisters). 

	   For each pair, the operands that must be converted to iregisters are
	   tested to see if the iregisters can replace the original register
	   throughout the code-block. The cost of dual-issuing the instruction
	   pair is calculated, taking account of how many instructions will be
	   needed to move data to and from original operands and the
	   iregisters. This is used to decide whether to add the dual-issue the
	   instruction pair. If the pair is dual-issued, the instructions are
	   moved together, any conversions are applied, any moves to/from
	   iregisters needed for the pair are emitted and the dual-issue state is
	   updated with the assignments for the new instruction pair.

	   Finally, when all pairs in the group have been found, the registers
	   used by the instructions pairs are are replaced by iregisters in the
	   code-block. This is done in two passes, one to replace registers used
	   as sources in dual-issue instructions, the other to replace registers
	   used as destinations.
	*/
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE);
	ClearBlockInsts(psState, psBlock);
	
	psNext = GetNextAvailable(psDepState, NULL);

	for (psInstA = GetNextAvailable(psDepState, NULL); psInstA != NULL; psInstA = psNext)
	{
		IMG_BOOL bACanPrimary, bACanSecondary, bFoundNextInst;

		if (IsFragileInst(psInstA) || 
			IsDeschedulingPoint(psState, psInstA) ||
			psInstA->eOpcode == IDUAL)
		{		
			InsertInstBefore(psState, psBlock, psInstA, NULL);
			RemoveInstruction(psDepState, psInstA);
			psNext = GetNextAvailable(psDepState, NULL);
			continue;
		}
		
		/* Test whether psInstA can be primary or a secondary. */
		bACanPrimary = GetBit(psInstA->auFlag, INST_DI_PRIMARY);
		bACanSecondary = GetBit(psInstA->auFlag, INST_DI_SECONDARY);
		
		if (bACanPrimary || bACanSecondary)
		{
			/*
				Look for another instruction to dual-issue with this one, that is ready.
				If there isn't one, try to find an instruction that is not ready.
			*/
			if (FindSecondReadyInst(psDepState,
									psIssueState,
									psBlock,
									psInstA,
									bACanPrimary,
									bACanSecondary) ||
				FindSecondNotReadyInst(psDepState,
									   psIssueState,
									   psBlock,
									   psInstA,
									   bACanPrimary,
									   bACanSecondary))
			{
				/* Recalculate dependency graph (because GPIs have been introduced) and start again. */
				FreeBlockDGraphState(psState, psBlock);
				psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
				ComputeClosedDependencyGraph(psDepState, IMG_FALSE);
				ClearBlockInsts(psState, psBlock);
				psNext = GetNextAvailable(psDepState, NULL);
				psIssueState->bChanged = IMG_TRUE;
				continue;
			}
		}
		
		InsertInstBefore(psState, psBlock, psInstA, NULL);

		RemoveInstruction(psDepState, psInstA);
		
		uFavourGPIReads &= ~GetIRegsReadMask(psState, psInstA, IMG_FALSE);
		uFavourGPIReads |= GetIRegsWrittenMask(psState, psInstA, IMG_FALSE);

		bFoundNextInst = IMG_FALSE;
		if (uFavourGPIReads != 0)
		{
			for (psNext = GetNextAvailable(psDepState, NULL); psNext != NULL; psNext = GetNextAvailable(psDepState, psNext))
			{
				if ((GetIRegsReadMask(psState, psNext, IMG_FALSE) & uFavourGPIReads) != 0)
				{
					bFoundNextInst = IMG_TRUE;
					break;
				}
			}
		}

		if (!bFoundNextInst)
		{
			psNext = GetNextAvailable(psDepState, NULL);
		}
	}

	/* Free per-instruction information. */
	FreeDualIssueInstData(psState, psIssueState);
	FreeBlockDGraphState(psState, psBlock);
}

IMG_INTERNAL 
IMG_BOOL GenerateDualIssue(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: GenerateDualIssue
    
 PURPOSE	: Try to dual issue instructions in a program.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: True if any instructions are dual-issued, false otherwise.

 NOTES		: Try to dual-issue pairs of instructions in the code-block. If a
			  dual-issue pair is found, the primary instruction is marked
			  PRIMARY_INST and represents the dual-issue instruction. The
			  secondary instruction is marked SECONDARY_INST. A dual-issued
			  instruction is always linked to its pair through field
			  psCoInst. The secondary instruction is kept in the code-block but
			  moved to immediately follow the primary. This allows most
			  code-analysis on dual-issued instructions to proceed as if the
			  dual-issue instructions were single issued in sequence. When a
			  secondary is explicitly treated as part of a dual-issued
			  instruction, it should be accessed through the  primary
			  instruction. Secondary instructions are removed from the
			  code block when the nosched flag is set (by function
			  SetupNoSchedFlag) at the end of the optimization pass.

			  This pass should be run before allocation of temporary registers
			  but after all code-motion has been completed. If this is to be
			  run before a code-motion pass, the dependency graph functions
			  must be modified to handle dual-issue instructions. 
*****************************************************************************/
{
	PDISSUE_STATE psIssueState;
	IMG_BOOL bChanged;

	/*
	   Don't dual-issue if there aren't enough temporaries to hold all internal registers
	   otherwise the register allocator will have problems.
	*/
	if (psState->psSAOffsets->uNumAvailableTemporaries < USC_NUM_DUALISSUE_IREGISTERS)
	{
		return IMG_FALSE;
	}

	/*
		Check if we are allowed to move instructions at all to form dual-issue pairs.
	*/
	if (psState->uMaxInstMovement == 0)
	{
		return IMG_FALSE;
	}

	/* Allocate data structures */
	psIssueState = (PDISSUE_STATE)UscAlloc(psState, sizeof(DISSUE_STATE));
	ASSERT(psIssueState != NULL);
	/* Initialise data */
	psIssueState->bChanged = IMG_FALSE;
	psIssueState->psNonTempTree = NULL;

	DoOnAllBasicBlocks(psState, ANY_ORDER, DualIssueBP, IMG_FALSE, psIssueState);
	
	/* Free data structures */
	bChanged = psIssueState->bChanged;
	UscFree(psState, psIssueState);

	return bChanged;
}

#endif /* defined(SUPPORT_SGX545) */

/* EOF */
