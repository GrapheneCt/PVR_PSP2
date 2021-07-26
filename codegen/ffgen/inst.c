/******************************************************************************
 * Name         : inst.c
 * Author       : James McCarthy
 * Created      : 16/11/2005
 *
 * Copyright    : 2005-2008 by Imagination Technologies Limited.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: inst.c $
 *****************************************************************************/
#include <stdio.h>
#include <stdarg.h>

#include "apidefs.h"
#include "codegen.h"
#include "macros.h"
#include "inst.h"
#include "reg.h"
#include "source.h"
#include "sgxdefs.h"
#include "usetab.h"

/******************************************************************************
 * Function Name: AddInstToList
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static UseInstructionList *AddInstToList(FFGenCode			*psFFGenCode,
										UseInstructionList	*psInstructionList,
										USE_INST			*psInstructionToAdd,
										IMG_UINT32			uLineNumber,
										const IMG_CHAR		*pszFileName)
{
	UseInstructionList *psNewEntry;
	UseInstructionList *psList = psInstructionList;

	PVR_UNREFERENCED_PARAMETER(psFFGenCode);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);
	PVR_UNREFERENCED_PARAMETER(pszFileName);

	if (psList)
	{
		/* Go to end of list */
		while (psList->psNext)
		{
			psList = psList->psNext;
		}
	}

	/* Create new list entry */
	psNewEntry = FFGENMalloc2(psFFGenCode->psFFGenContext, sizeof(UseInstructionList), uLineNumber, pszFileName);

	/* Add register to list entry */
	psNewEntry->psInstruction  = psInstructionToAdd;
	psNewEntry->psNext = IMG_NULL;

	if (psList)
	{
		psList->psNext = psNewEntry;
	}
	else
	{
		psInstructionList = psNewEntry;
	}

	return psInstructionList;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyInstructionList(FFGenContext *psFFGenContext, UseInstructionList *psList)
{
	while (psList)
	{
		UseInstructionList *psNext = psList->psNext;
		
		/* Free the entry */
		FFGENFree(psFFGenContext, psList);

		psList = psNext;
	}
}


/******************************************************************************
 * Function Name: EncodeReg
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID EncodeReg(  FFGenCode		*psFFGenCode,
							FFGenReg		*psReg,
							IMG_BOOL		bUseRegOffset,
							IMG_UINT32		uRegFlags,
							IMG_INT32		iRegOffset)
{
	USE_INST     *psUseInst = psFFGenCode->psCurrentUseInst;
	USE_REGISTER *psUseReg  = &(psUseInst->asArg[psFFGenCode->uNumUseArgs]);

	/* If we're only setting up a predicate then we need to set up fake destination register (that won't be written to anyway) */
	if (psReg->eType == USEASM_REGTYPE_PREDICATE && psFFGenCode->uNumUseArgs == 0)
	{
		psUseReg->uType   = USEASM_REGTYPE_TEMP;
		psUseReg->uNumber = 0;
		psUseReg->uFlags  = USEASM_ARGFLAGS_DISABLEWB;
		psUseReg->uIndex  = 0;

		/* Increase number of use args */
		psFFGenCode->uNumUseArgs++;

		psUseReg  = &(psUseInst->asArg[psFFGenCode->uNumUseArgs]);
	}

	/* Setup use arg */
	psUseReg->uType   = psReg->eType;
	psUseReg->uNumber = psReg->uOffset;
	psUseReg->uFlags  = uRegFlags;
	psUseReg->uIndex  = psReg->uIndex;

	if (bUseRegOffset)
	{
		psUseReg->uNumber += iRegOffset;
	}


	/* Increase number of use args */
	psFFGenCode->uNumUseArgs++;
}

/******************************************************************************
 * Function Name: GetUseInst
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static USE_INST *GetUseInst(FFGenCode *psFFGenCode)
{
	/* Alloc mem for instruction and zero */
	USE_INST *psUseInst = FFGENCalloc(psFFGenCode->psFFGenContext, sizeof(USE_INST));

	if (!psFFGenCode->psUseInsts)
	{
		psFFGenCode->psUseInsts       = psUseInst;
		psFFGenCode->psCurrentUseInst = IMG_NULL;
	}
	else
	{
		psFFGenCode->psCurrentUseInst->psNext = psUseInst;
	}

	/* Set default instruction values */
	psUseInst->psPrev        = psFFGenCode->psCurrentUseInst;

	/* set pointer to new instruction */
	psFFGenCode->psCurrentUseInst = psUseInst;

	/* increase number of instructions */
	psFFGenCode->uNumInstructions++;

	/* init number of use args */
	psFFGenCode->uNumUseArgs = 0;

	return psUseInst;
	
}

/******************************************************************************
 * Function Name: CheckRegUsage
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  : Tries to prevent invalid regs being used for the first source 
                  of some instructions 
 *****************************************************************************/
static IMG_VOID CheckRegUsage(  FFGenCode			*psFFGenCode,
								FFGenInstruction	*psOrigInst,
								FFGenReg			**ppsRegToRelease)
{
	*ppsRegToRelease = IMG_NULL;
		
	if (psOrigInst->eOpcode == USEASM_OP_FMAD ||
		psOrigInst->eOpcode == USEASM_OP_FMUL ||
		psOrigInst->eOpcode == USEASM_OP_FADD)
	{
		FFGenReg         *psSrcReg0 = psOrigInst->ppsRegs[1];
		FFGenReg         *psSrcReg1 = psOrigInst->ppsRegs[2];

		/* Check for invalid register combinations */
		if (psSrcReg0->eType != USEASM_REGTYPE_TEMP &&
			psSrcReg0->eType != USEASM_REGTYPE_PRIMATTR)
		{
			/* Can't use instruction inside psFFGenCode as it's currently being used */
			FFGenInstruction sInst = {0};
			FFGenInstruction *psInst = &sInst;
			IMG_UINT32 uDestBaseOffsetEnabled = psFFGenCode->uEnabledSMBODestBaseOffset;

			FFGenReg *psTemp;

			IMG_UINT32 uRegSize = 1;

			if (psSrcReg1->eType == USEASM_REGTYPE_TEMP ||
				psSrcReg1->eType == USEASM_REGTYPE_PRIMATTR)
			{
				COMMENT(psFFGenCode, "Possible optimisation? - Switch src regs 0 and 1 to avoid copy"); 
			}

			/* Does the temp need to be bigger? */
			if (GET_REPEAT_COUNT(psOrigInst))
			{
				uRegSize = GET_REPEAT_COUNT(psOrigInst);
			}

			/* Get a temporary register to hold the info */
			psTemp = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0 , 0, uRegSize, IMG_NULL);

			/* 
			   If the original instruction had an offset applied to it, then use it when copying the
			   temp but strip it from the instruction so it doesn't get applied to the copy 
			*/
			if (psOrigInst->aiRegOffsets[1])
			{
				SETSRCOFF(0, psOrigInst->aiRegOffsets[1]);
				psOrigInst->aiRegOffsets[1]  = 0;
				psOrigInst->uUseRegOffset   &= ~(1 << 1);
			}

			if(uDestBaseOffsetEnabled)
			{
				DISABLE_DST_BASE_OFFSET(psFFGenCode);
			}

			/* Move the reg into a temp */
			SET_REPEAT_COUNT(uRegSize);
			INST1(MOV, psTemp, psSrcReg0, "Instruction below can't have existing reg type for 1st source so move into temp");

			if(uDestBaseOffsetEnabled)
			{
				ENABLE_DST_BASE_OFFSET(psFFGenCode, uDestBaseOffsetEnabled);
			}
			/* Replace use of reg with copy */
			psOrigInst->ppsRegs[1] = psTemp;

			*ppsRegToRelease = psTemp;
		}
	}
}

#ifdef FFGEN_UNIFLEX

/******************************************************************************
 * Function Name: AddUniFlexInstructionToList
 *
 * Inputs       : psFFGenCode, psInstToAdd
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  : Adds a new UniFlex instruction to the linked list in psFFGenCode
 *****************************************************************************/
IMG_INTERNAL IMG_VOID AddUniFlexInstructionToList(FFGenCode *psFFGenCode, UNIFLEX_INST *psInstToAdd)
{
	/*Create a new Uniflex instruction*/
	UNIFLEX_INST *psNewInst;
	
	psNewInst = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(UNIFLEX_INST));

	*psNewInst = *psInstToAdd;

	/*Add the instruction to end of the list */
	if(psFFGenCode->psUniFlexInstructions)
	{
		UNIFLEX_INST* psInst = psFFGenCode->psUniFlexInstructions;

		while(psInst->psILink)
		{
			psInst = psInst->psILink;
		}

		psInst->psILink = psNewInst;
	}
	else
	{
		psFFGenCode->psUniFlexInstructions = psNewInst;
	}

	psNewInst->psILink = NULL;
}

/******************************************************************************
 * Function Name: SplitUniflexInst
 *
 * Inputs       :	psDest, psSource,
 					uChan:	Standard USC concept of an RGBA channel.
 							From 0-3; 0 is X (aka R), 3 is A.
 					uNumSrcRegs: the number of src registers the instruct. uses.
 * Outputs      : psDest
 * Returns      : void
 * Globals Used :
 *
 * Description  : Given a source instruction (psSource) and a channel
 				  description (uChan) construct a new instruction (psDest) that
 				  has the same function as the old instruction, but only uses
 				  the specified channel. Essentially allows a single uniflex
 				  instr to be split into 4 sub-instrs, one for each chan.
 *****************************************************************************/
static IMG_VOID SplitUniflexInst(PUNIFLEX_INST psDest,
								 PUNIFLEX_INST psSource,
								 IMG_UINT16 uChan,
								 IMG_UINT32 uNumSrcRegs)
{
	IMG_UINT i;

	PVR_ASSERT(uNumSrcRegs - 1 < 5);
	PVR_ASSERT(uChan < 4);
	
	*psDest = *psSource;

	if (psDest->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		psDest->sDest.uNum += uChan;
		psDest->sDest.u.byMask = 0x1;
	}
	else
	{
		psDest->sDest.u.byMask = (IMG_BYTE) (0x1 << uChan);
	}

	for (i = 0; i < uNumSrcRegs - 1; i++)
	{
		IMG_UINT16 uSwizChan = (psDest->asSrc[i].u.uSwiz >> (uChan * 3)) & 0x7;
		
		if (uSwizChan <= UFREG_SWIZ_W)
		{
			if (psDest->asSrc[i].eType == UFREG_TYPE_VSOUTPUT)
			{
				psDest->asSrc[i].uNum += uSwizChan;
				psDest->asSrc[i].u.uSwiz = UFREG_SWIZ_RRRR;
			}
			else
			{
				psDest->asSrc[i].u.uSwiz = uSwizChan;
			}
		}
	}
}


/******************************************************************************
 * Function Name: StoreUniFlexInstruction
 *
 * Inputs       : psFFGenCode, psInstruction
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  : Converts useasm instructions supplied for FFTNL programs (not 
				  geometry shaders) into UniFlex instructions, add adds them
				  to psFFGenCode's linked list.
 *****************************************************************************/
static IMG_VOID StoreUniFlexInstruction(FFGenCode *psFFGenCode, FFGenInstruction *psInstruction)
{
	UNIFLEX_INST sInst = {0};
	IMG_BOOL bSingleValueInputs = IMG_TRUE;
	IMG_BOOL bNonVec = IMG_FALSE;
	IMG_UINT i;

	switch(psInstruction->eOpcode)
	{
#if defined(FFGEN_UNIFLEX)
		case USEASM_OP_FLOOR:
		{
			sInst.eOpCode = UFOP_FLR;

			break;
		}
#endif /* defined(FFGEN_UNIFLEX) */
		case USEASM_OP_FMOV:
		case USEASM_OP_MOV:
		{
			if(psInstruction->ppsRegs[0]->eType == UFREG_TYPE_ADDRESS)
			{
				sInst.eOpCode = UFOP_MOVA_TRC;
			}
			else
			{
				sInst.eOpCode = UFOP_MOV;
			}

			break;
		}
		case USEASM_OP_FADD:
		{
			sInst.eOpCode = UFOP_ADD;
			break;
		}
		case USEASM_OP_FDP4:
		{
			sInst.eOpCode = UFOP_DOT4;
			bSingleValueInputs = IMG_FALSE; /* These take vector inputs but give scalar outputs */
			psInstruction->aiRegOffsets[0] += 3; /* Cancel out the DP4_ADJUST(a) macro */
			break;
		}
		case USEASM_OP_FDP3:
		{
			sInst.eOpCode = UFOP_DOT3;
			bSingleValueInputs = IMG_FALSE;
			psInstruction->aiRegOffsets[0] += 2;
			break;
		}
		case USEASM_OP_FDPC4:
		{
			sInst.eOpCode = UFOP_DOT4_CP;
			bSingleValueInputs = IMG_FALSE;	
			psInstruction->aiRegOffsets[0] += 3;
			{
				/* The uniflex dpc4 takes the clipplane as the third source, whereas the USSE instruction takes it as the first, 
				   so swap around the first and third source arguments */
				FFGenReg* psReg;
				IMG_INT32 iRegOffset;
				psReg = psInstruction->ppsRegs[1];
				psInstruction->ppsRegs[1] = psInstruction->ppsRegs[3];
				psInstruction->ppsRegs[3] = psReg;
				iRegOffset = psInstruction->aiRegOffsets[1];
				psInstruction->aiRegOffsets[1] = psInstruction->aiRegOffsets[3];
				psInstruction->aiRegOffsets[3] = iRegOffset;
			}
			break;
		}
		case USEASM_OP_FMUL:
		{
			sInst.eOpCode = UFOP_MUL;
			break;
		}
		case USEASM_OP_FRSQ:
		{
			sInst.eOpCode = UFOP_RSQRT;
			break;
		}
		case USEASM_OP_FRCP:
		{
			sInst.eOpCode = UFOP_RECIP;
			break;
		}
		case USEASM_OP_FMAX:
		{
			sInst.eOpCode = UFOP_MAX;
			break;
		}
		case USEASM_OP_FMAD:
		{
			sInst.eOpCode = UFOP_MAD;
			break;		
		}
		case USEASM_OP_FLOG:
		{
			sInst.eOpCode = UFOP_LOG;
			break;
		}
		case USEASM_OP_FEXP:
		{
			sInst.eOpCode = UFOP_EXP;
			break;
		}
		case USEASM_OP_AND:		
		{
			sInst.eOpCode = UFOP_AND;		
			break;
		}
		case USEASM_OP_FSUB:		
		{
			sInst.eOpCode = UFOP_SUB;		
			break;
		}
		case USEASM_OP_PADDING:
		case USEASM_OP_SMLSI:
		case USEASM_OP_COMMENT:
		case USEASM_OP_LABEL:
		case USEASM_OP_COMMENTBLOCK:
		{
			return;
		}
		default:
		{
			PVR_DBG_BREAK;
		}
	} /* switch(psInstruction->eOpcode) */
	

	sInst.sDest.uNum = psInstruction->ppsRegs[0]->uOffset;

	/* Find out what the destination flag should be */
	if(psInstruction->uUseRegOffset & (1 << 0))
	{
		if(psInstruction->ppsRegs[0]->eType == USEASM_REGTYPE_OUTPUT)
		{
			sInst.sDest.u.byMask = 1;
			sInst.sDest.uNum += psInstruction->aiRegOffsets[0];
		}	
		else
		{
			sInst.sDest.u.byMask = (IMG_BYTE)(1 << psInstruction->aiRegOffsets[0]);
		}
	}
	else if(psInstruction->ppsRegs[0]->uSizeInDWords == 1)
	{
		sInst.sDest.u.byMask = 0x1;
	}
	else if(GET_REPEAT_COUNT(psInstruction))
	{
		sInst.sDest.u.byMask = (IMG_BYTE)((1 << GET_REPEAT_COUNT(psInstruction)) - 1);
		bSingleValueInputs = IMG_FALSE; /* MOV, FADD, FMAD etc is often done for vector outputs, and if a 
										repeat count is used then the inputs will always be vector as well.
										FFgen never mixes vector & scalar inputs in a single instruction.*/
	}
	else
	{
		sInst.sDest.u.byMask = 0x1;				
	}


	switch(psInstruction->ppsRegs[0]->eType)
	{
		case USEASM_REGTYPE_OUTPUT:
		{
			sInst.sDest.eType = UFREG_TYPE_VSOUTPUT;
			bNonVec = IMG_TRUE;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		{
			sInst.sDest.eType = UFREG_TYPE_TEMP;
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			PVR_ASSERT(psInstruction->uFlags1 & USEASM_OPFLAGS1_TESTENABLE);

			sInst.eOpCode = UFOP_SETP; /* Overwrite the opcode */

			/* Select the predicate to set */
			sInst.sDest.uNum = 0;
			sInst.sDest.u.byMask = (IMG_UINT8)(1 << psInstruction->ppsRegs[0]->uOffset);
			sInst.sDest.eType = UFREG_TYPE_PREDICATE;

			/* Set the second source to comparison operator */
			sInst.asSrc[1].eType = UFREG_TYPE_COMPOP;
			sInst.asSrc[1].u.uSwiz = UFREG_SWIZ_NONE;

			switch(psInstruction->uTest)
			{
					/* SET_TESTPRED_POSITIVE */
				case ((USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) |
					(USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
					(USEASM_TEST_CRCOMB_AND)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_GT;
					break;
					/* SET_TESTPRED_NEGATIVE */
				}
				case ((USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) |
					(USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) |
					(USEASM_TEST_CRCOMB_AND)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_LT;
					break;
					/* SET_TESTPRED_NEGATIVE_OR_ZERO */
				}
				case ((USEASM_TEST_SIGN_NEGATIVE << USEASM_TEST_SIGN_SHIFT) | (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_LE;
					break;
					/* SET_TESTPRED_POSITIVE_OR_ZERO */
				}
				case ((USEASM_TEST_SIGN_POSITIVE << USEASM_TEST_SIGN_SHIFT) | (USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_GE;
					break;
					/* SET_TESTPRED_NONZERO */
				}
				case ((USEASM_TEST_ZERO_NONZERO << USEASM_TEST_ZERO_SHIFT) | (USEASM_TEST_CRCOMB_AND)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_NE;
					break;
					/* SET_TESTPRED_ZERO */
				}
				case ((USEASM_TEST_ZERO_ZERO << USEASM_TEST_ZERO_SHIFT) | (USEASM_TEST_CRCOMB_AND)):
				{
					sInst.asSrc[1].uNum = UFREG_COMPOP_EQ;
					break;
				}
				default:
				{
					PVR_DBG_BREAK;
				}
			}

			if(psInstruction->uNumRegs < 3)
			{
				/*	Set the 3rd source to the hardware constant zero. This is done with the MOV 
				useasm instruction for example, when we are just testing if a number is negative. */
				sInst.asSrc[2].eType = UFREG_TYPE_HW_CONST;
				sInst.asSrc[2].uNum = HW_CONST_0;
				sInst.asSrc[2].u.uSwiz = UFREG_SWIZ_NONE;
			}

			break;
		}
		case UFREG_TYPE_ADDRESS:
		{
			sInst.sDest.eType = UFREG_TYPE_ADDRESS;
			break;
		}
		default:
		{
			PVR_DBG_BREAK;
		}
	} /* switch(psInstruction->ppsRegs[0]->eType) */

	for(i = 1; i < psInstruction->uNumRegs; i++)
	{
		IMG_UINT32 uSrcNum = i-1;
		IMG_UINT16 asSwizzles[] = {UFREG_SWIZ_RRRR, UFREG_SWIZ_GGGG, UFREG_SWIZ_BBBB, UFREG_SWIZ_AAAA};

		if(uSrcNum > 0 && psInstruction->ppsRegs[0]->eType == USEASM_REGTYPE_PREDICATE)
		{
			uSrcNum++; /* We are setting a predicate register, so make room for the comparison operator */
		}

		if(psInstruction->auRegFlags[i] & USEASM_ARGFLAGS_NEGATE)
		{
			sInst.asSrc[uSrcNum].byMod |= UFREG_SOURCE_NEGATE;
		}
		else if(psInstruction->auRegFlags[i] & USEASM_ARGFLAGS_ABSOLUTE)
		{
			sInst.asSrc[uSrcNum].byMod |= UFREG_SOURCE_ABS;
		}
		else
		{
			PVR_ASSERT(!psInstruction->auRegFlags[i]);
		}
	
		sInst.asSrc[uSrcNum].uNum  = psInstruction->ppsRegs[i]->uOffset;

		if(psInstruction->uUseRegOffset & (1 << i))
		{
			/* In a matrix multiply for example, each row of the matrix is offset by a multiple of 4, 
			so we add this onto	the uNum of the register. The remainder of the division by 4 is used
			to select a single component from a vector by swizzling */
			sInst.asSrc[uSrcNum].uNum += psInstruction->aiRegOffsets[i] & ~3;

			if(psInstruction->ppsRegs[i]->eType == USEASM_REGTYPE_OUTPUT)
			{
				sInst.asSrc[uSrcNum].uNum += psInstruction->aiRegOffsets[i];
				sInst.asSrc[uSrcNum].u.uSwiz = UFREG_SWIZ_RRRR;
			}
			else
			{
				if(bSingleValueInputs)
				{
					sInst.asSrc[uSrcNum].u.uSwiz = asSwizzles[psInstruction->aiRegOffsets[i] % 4];
				}
				else
				{
					sInst.asSrc[uSrcNum].u.uSwiz = UFREG_SWIZ_NONE;
				}
			}
		}
		else
		{
			if(bSingleValueInputs)
			{
				sInst.asSrc[uSrcNum].u.uSwiz = UFREG_SWIZ_RRRR;
			}
			else
			{
				sInst.asSrc[uSrcNum].u.uSwiz = UFREG_SWIZ_NONE;
			}
		}

		switch(psInstruction->ppsRegs[i]->eType)
		{
			case USEASM_REGTYPE_SECATTR:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_CONST;
				
				PVR_ASSERT((sInst.asSrc[uSrcNum].uNum & 0x3) == 0);
				
				sInst.asSrc[uSrcNum].uNum /= 4;
				break;
			}
			case USEASM_REGTYPE_FPCONSTANT:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_HW_CONST;
				sInst.asSrc[uSrcNum].u.uSwiz = UFREG_SWIZ_NONE;

				switch(psInstruction->ppsRegs[i]->uOffset)
				{
					case EURASIA_USE_SPECIAL_CONSTANT_ZERO:
					{
						sInst.asSrc[uSrcNum].uNum = HW_CONST_0;
						break;
					}
					case EURASIA_USE_SPECIAL_CONSTANT_FLOAT1:
					{
						sInst.asSrc[uSrcNum].uNum = HW_CONST_1;
						break;
					}
					case EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2:
					{
						sInst.asSrc[uSrcNum].uNum = HW_CONST_HALF;
						break;
					}
					case EURASIA_USE_SPECIAL_CONSTANT_FLOAT2:
					{
						sInst.asSrc[uSrcNum].uNum = HW_CONST_2;
						break;
					}
				}		
				break;
			}
			case USEASM_REGTYPE_TEMP:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_TEMP;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_VSINPUT;
				sInst.asSrc[uSrcNum].uNum /= 4;
				break;
			}
			case USEASM_REGTYPE_OUTPUT:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_VSOUTPUT;
				bNonVec = IMG_TRUE;
				break;
			}
			case USEASM_REGTYPE_CLIPPLANE:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_CLIPPLANE;
				sInst.asSrc[uSrcNum].uNum = psInstruction->aiRegOffsets[i];
				break;
			}
			case USEASM_REGTYPE_IMMEDIATE:
			{
				sInst.asSrc[uSrcNum].eType = UFREG_TYPE_IMMEDIATE;
				break;
			}
			default:
			{
				PVR_DBG_BREAK;
			}
		} /* switch(psInstruction->ppsRegs[i]->eType) */

		if(psInstruction->ppsRegs[i]->eBindingRegDesc == FFGEN_STATE_MODELVIEWMATRIXPALETTE ||
			psInstruction->ppsRegs[i]->eBindingRegDesc == FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE)
		{
			/* This source is a palette of matrices, so we index the palette by using the first channel of the address register. */
			sInst.asSrc[uSrcNum].eRelativeIndex = UFREG_RELATIVEINDEX_A0X;

			/* This is only used from matrix palette, so hard code the stride to be the size of a matrix */
			sInst.asSrc[uSrcNum].u1.uRelativeStrideInComponents = FFTNL_SIZE_MATRIX_4X4;
		}

	} /* for(i = 1; i < psInstruction->uNumRegs; i++) */						
	
	/* Check if this instruction is conditioned on a predicate */
	if(GET_PRED(psInstruction))
	{
		IMG_UINT32 auPredicates[] = {UF_PRED_X, UF_PRED_Y, UF_PRED_Z, UF_PRED_W};

		sInst.uPredicate = auPredicates[GET_PRED(psInstruction) - USEASM_PRED_P0];
	}

	/* Even though vo4.g and vo5.r are the same thing, 
	   USC2 has the restriction that a single register should be refered to in
	   one way only. This block forces any "vo4.gb" into a "vo5.r vo6.r"*/
	if (GET_REPEAT_COUNT(psInstruction) && bNonVec)
	{
		IMG_UINT16 uChan;
		
		for (uChan = 0; uChan < 4; uChan++)
		{
			UNIFLEX_INST sNewInst;

			/* Skip channels that the destination doesn't use*/
			if ((0x1 << uChan) & ~sInst.sDest.u.byMask)
			{
				continue;
			}
			
			PVRUniFlexInitInst(IMG_NULL, &sNewInst);
			SplitUniflexInst(&sNewInst, &sInst, uChan, psInstruction->uNumRegs);
			AddUniFlexInstructionToList(psFFGenCode, &sNewInst);
		}
	}
	else
	{

#if defined(DEBUG)
		/* make sure only "r" component is used if type is vsoutoput */
		if (bNonVec)
		{
			if (sInst.sDest.eType == UFREG_TYPE_VSOUTPUT)
			{
				PVR_ASSERT(sInst.sDest.u.byMask == 0x1);
			}
			
			for (i = 0; i < psInstruction->uNumRegs - 1; i++)
			{
				if (sInst.asSrc[i].eType == UFREG_TYPE_VSOUTPUT)
				{
					PVR_ASSERT(sInst.asSrc[i].u.uSwiz == UFREG_SWIZ_RRRR);
				}
			}
		}
#endif
	
		/*Add the new UniFlex instruction to the linked list*/
		AddUniFlexInstructionToList(psFFGenCode, &sInst);
	}
}



#endif /* #ifdef FFGEN_UNIFLEX */

/******************************************************************************
 * Function Name: StoreInstruction
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID StoreInstruction(  FFGenCode		*psFFGenCode,
								   FFGenInstruction	*psInstruction,
								   IMG_UINT32		uLineNumber)
{
	FFGenInstructionList *psNewInstructionEntry;
	FFGenInstruction     *psNewInstruction;
	IMG_UINT32            i;
	IMG_BOOL              bCountInstruction = IMG_TRUE;

	if (psInstruction->eOpcode == USEASM_OP_COMMENT ||
		psInstruction->eOpcode == USEASM_OP_COMMENTBLOCK)
	{
		/* For purposes of instruction counts, comments do not count */
		bCountInstruction = IMG_FALSE;

		/* Only store comments for debug and standalone builds */
#if !defined(STANDALONE) && !defined(DEBUG)
		return;
#endif
	}

	/* alloc memory for entry */
	psNewInstructionEntry = FFGENMalloc(psFFGenCode->psFFGenContext, sizeof(FFGenInstructionList));

	if (!psNewInstructionEntry)
	{
		psFFGenCode->psFFGenContext->pfnPrint("StoreInstruction: Failed to alloc mem for instruction entry\n");
		return;
	}

	psNewInstruction      = &(psNewInstructionEntry->sInstruction); 

	/* Copy the instruction */
	*psNewInstruction = *psInstruction;

	/* Copy the src regs */
	for (i = 0; i < psInstruction->uNumRegs; i++)
	{
		if (!psInstruction->ppsRegs[i])
		{
			psFFGenCode->psFFGenContext->pfnPrint("StoreInstruction: Reg %d was null (%d)\n", i, uLineNumber);
			return;
		}

		/* Store the register */
		psNewInstructionEntry->asRegs[i] = *(psInstruction->ppsRegs[i]);

		/* Make instruction point at stored register */
		psNewInstruction->ppsRegs[i] = &(psNewInstructionEntry->asRegs[i]);
	}

	psNewInstructionEntry->uLineNumber        = uLineNumber;
	psNewInstructionEntry->uInstructionNumber = psFFGenCode->uNumInstructionsStored;

	/* Increase instructions stored count */
	if (bCountInstruction)
	{
		psFFGenCode->uNumInstructionsStored++;
	}

	/* Alloc memory for comment? */
	if (psInstruction->pszComment)
	{
		psNewInstruction->pszComment = FFGENMalloc(psFFGenCode->psFFGenContext,(IMG_UINT32)(strlen(psInstruction->pszComment) + 1));

		if (!psNewInstruction->pszComment)
		{
			psFFGenCode->psFFGenContext->pfnPrint("StoreInstructionfn: Failed to alloc mem for instruction entry\n");
			return;
		}

		/* copy the comment */
		strcpy(psNewInstruction->pszComment, psInstruction->pszComment);
	}

	/* Setup the previous and next pointers */
	psNewInstructionEntry->psNext = IMG_NULL;
	psNewInstructionEntry->psPrev = psFFGenCode->psCurrentInstructionEntry;

	/* Add to end of list */
	if (psFFGenCode->psCurrentInstructionEntry)
	{
		psFFGenCode->psCurrentInstructionEntry->psNext = psNewInstructionEntry;
	}

	/* make this the current entry */
	psFFGenCode->psCurrentInstructionEntry = psNewInstructionEntry;

	/* If this is the first entry then make it head of the list */
	if (!psFFGenCode->psStoredInstructionList)
	{
		psFFGenCode->psStoredInstructionList = psNewInstructionEntry;
	}
}

/******************************************************************************
 * Function Name: EncodeInstructionfn
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID EncodeInstructionfn(FFGenCode			*psFFGenCode,
										  FFGenInstruction	*psInstruction,
										  IMG_UINT32		uLineNumber)
{
	IMG_UINT32 i;

	USE_INST *psUseInst;

	FFGenReg *psRegToRelease = IMG_NULL;

	IMG_BOOL bComment = (psInstruction->eOpcode == USEASM_OP_COMMENT ||	psInstruction->eOpcode == USEASM_OP_COMMENTBLOCK);

	/* Increase aligned instruction count */
	if (!bComment && psInstruction->eOpcode != USEASM_OP_LABEL)
	{
		psFFGenCode->uAlignedInstructionCount++;
	}

	if(!psFFGenCode->bSeondPass)
	{
		/* Check to see if register usage is valid with this instruction - dont do it on second pass */
		CheckRegUsage(psFFGenCode, psInstruction, &psRegToRelease);
	}

#ifdef FFGEN_UNIFLEX
	if( IF_FFGENCODE_UNIFLEX )
	{
		StoreUniFlexInstruction(psFFGenCode, psInstruction);
	}
	else
#endif
	/* If we're in two pass mode don't encode the instruction just store it for now */
	if (psFFGenCode->eCodeGenMethod == FFCGM_TWO_PASS && !psFFGenCode->bSeondPass)
	{
		StoreInstruction(psFFGenCode, psInstruction, uLineNumber);
		goto StripInstruction;
	}

	if (bComment)
	{
		goto HandleComment;
	}

	/* Get next use instruction */
	psUseInst = GetUseInst(psFFGenCode);

	/* Store this for help in debug */
	psUseInst->uSourceLine = uLineNumber;

	/* Set use op code */
	psUseInst->uOpcode = psInstruction->eOpcode;

	/* Add any extra info if required */
	switch (psInstruction->eOpcode)
	{
		case USEASM_OP_SDM:
			psUseInst->uOpcode = USEASM_OP_SDM;
			psInstruction->uExtraInfo |= (EURASIA_USE1_EFO_WI0EN |
										  EURASIA_USE1_EFO_WI1EN |
										  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
										  (EURASIA_USE1_EFO_ASRC_SRC0SRC1_SRC2SRC0 << EURASIA_USE1_EFO_ASRC_SHIFT) |
										  (EURASIA_USE1_EFO_ISRC_I0M0_I1M1 << EURASIA_USE1_EFO_ISRC_SHIFT));
			break;
		case USEASM_OP_DMA:
			psUseInst->uOpcode = USEASM_OP_DMA;
			psInstruction->uExtraInfo |= (EURASIA_USE1_EFO_WI0EN |
										  EURASIA_USE1_EFO_WI1EN |
										  (EURASIA_USE1_EFO_MSRC_SRC0SRC1_SRC0SRC2 << EURASIA_USE1_EFO_MSRC_SHIFT) |
										  (EURASIA_USE1_EFO_ASRC_M0I0_I1M1 << EURASIA_USE1_EFO_ASRC_SHIFT) |
										  (EURASIA_USE1_EFO_ISRC_I0A0_I1A1 << EURASIA_USE1_EFO_ISRC_SHIFT));
			break;
		default:
			break;
	}

	/* Set skip invalid by default */
	if (OpcodeAcceptsSkipInv(psUseInst->uOpcode))
	{
		psInstruction->uFlags1 |= USEASM_OPFLAGS1_SKIPINVALID;
	}

#if 0
	if (psInstruction->eOpcode == USEASM_OP_LDAD)
	{
		FFGenReg *psDRCReg = psInstruction->ppsSrcRegs[3];
		
		if (psDRCReg->eType != USEASM_REGTYPE_DRC)
		{
			psFFGenCode->psFFGenContext->pfnPrint("DRC Reg not found\n");
		}

		/* Update WDF Status with the DRC channel used for this fetch */
		psInstruction->psDestReg->eWDFStatus = psDRCReg->uOffset;
	}
#endif

	psUseInst->uFlags1 = psInstruction->uFlags1;
	psUseInst->uFlags2 = psInstruction->uFlags2;
	psUseInst->uFlags3 = psInstruction->uFlags3;
	psUseInst->uTest   = psInstruction->uTest;

	/* Set value if repeat count not set */
	if (!(psInstruction->uFlags1 & ~USEASM_OPFLAGS1_REPEAT_CLRMSK))
	{
		if(!(psInstruction->uFlags1 & ~USEASM_OPFLAGS1_MASK_CLRMSK))
		{
			psUseInst->uFlags1 |= 1 << USEASM_OPFLAGS1_MASK_SHIFT;
		}
	}

	/* Loop through all regs */
	for (i = 0; i < psInstruction->uNumRegs; i++)
	{
		if (!psInstruction->ppsRegs[i])
		{
			psFFGenCode->psFFGenContext->pfnPrint("EncodeInstruction: Reg %d was null (%d)\n", i, uLineNumber);
			return;
		}

		/* Convert to use register */
		EncodeReg(psFFGenCode,
				  psInstruction->ppsRegs[i],
				  (psInstruction->uUseRegOffset & (1 << i)),
				  psInstruction->auRegFlags[i],
				  psInstruction->aiRegOffsets[i]);
	}


	if (psInstruction->uExtraInfo)
	{
		/* get next register */
		USE_REGISTER *psUseReg  = &(psUseInst->asArg[psFFGenCode->uNumUseArgs]);

		/* add internal register as next source */
		psUseReg->uType   = 0;
		psUseReg->uNumber = psInstruction->uExtraInfo;
		psUseReg->uFlags  = 0;
		psUseReg->uIndex  = 0;

		/* Increase number of use args */
		psFFGenCode->uNumUseArgs++;
	}

	if(psInstruction->bIndexPatch)
	{
		psFFGenCode->psIndexPatchUseInsts = AddInstToList(psFFGenCode,
														  psFFGenCode->psIndexPatchUseInsts,
														  psUseInst,
														  uLineNumber,
														  IMG_NULL);
	}

 HandleComment:

	PrintInstruction(psFFGenCode,
					 psInstruction,
					 uLineNumber,
					 psFFGenCode->uNumInstructions - 1);

 StripInstruction:	

	/* Strip flags */
	psInstruction->uFlags1       = 0;
	psInstruction->uFlags2		 = 0;
	psInstruction->uFlags3		 = 0;
	psInstruction->uTest         = 0;
	psInstruction->uExtraInfo    = 0;
	psInstruction->uUseRegOffset = 0;

	/* Loop through all src regs */
	for (i = 0; i < psInstruction->uNumRegs; i++)
	{
		/* strip source modifiers */
		psInstruction->auRegFlags[i] = 0;

		/* Strip source offsets */
		psInstruction->aiRegOffsets[i] = 0;

		/* Remove src reg */
		psInstruction->ppsRegs[i] = IMG_NULL;
	}

	/* Reset number of src regs */
	psInstruction->uNumRegs = 0;

	if (psRegToRelease)
	{
		ReleaseReg(psFFGenCode, psRegToRelease);
	}
}

/******************************************************************************
 * Function Name: EncodeInstructionList
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID EncodeInstructionList(FFGenCode *psFFGenCode)
{
	FFGenInstructionList *psInstructionEntry = psFFGenCode->psStoredInstructionList;

	psFFGenCode->bSeondPass = IMG_TRUE;

	/* Loop through the instruction list */
	while (psInstructionEntry)
	{
		FFGenInstructionList *psCurrentEntry = psInstructionEntry;
		
		/* Encode the instruction */
		EncodeInstructionfn(psFFGenCode, &(psCurrentEntry->sInstruction), psCurrentEntry->uLineNumber);

		/* Get the next entry */
		psInstructionEntry = psInstructionEntry->psNext;

		/* Free the comment */
		if (psCurrentEntry->sInstruction.pszComment)
		{
			FFGENFree(psFFGenCode->psFFGenContext, psCurrentEntry->sInstruction.pszComment);
		}

		/* Free the entry */
		FFGENFree(psFFGenCode->psFFGenContext, psCurrentEntry);
	}

	
	psFFGenCode->psStoredInstructionList = IMG_NULL;
}


/******************************************************************************
 * Function Name: GetDRC
 *
 * Inputs       : psFFGenCode
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetDRC(FFGenCode *psFFGenCode)
{
	IMG_UINT32 i;
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	for(i = 0; i < EURASIA_USE_DRC_BANK_SIZE; i++)
	{
		if(!psFFGenCode->abOutstandingDRC[i])
		{
			psFFGenCode->abOutstandingDRC[i] = IMG_TRUE;

			return i;
		}
	}

	psFFGenCode->sDRCReg.uOffset = 0;
	INST0(WDF, &psFFGenCode->sDRCReg, IMG_NULL);

	return 0;
}


/******************************************************************************
 * Function Name: IssueOutstandingWDFs
 *
 * Inputs       : psFFGenCode
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IssueOutstandingWDFs(FFGenCode *psFFGenCode)
{
	IMG_UINT32 i;
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	IMG_BOOL bOutstandingWDFs = IMG_FALSE;

	for(i = 0; i < EURASIA_USE_DRC_BANK_SIZE; i++)
	{
		if(psFFGenCode->abOutstandingDRC[i])
		{
			bOutstandingWDFs = IMG_TRUE;
			break;
		}
	}

	if(bOutstandingWDFs)
	{
		NEW_BLOCK(psFFGenCode, "Issue outstanding WDFs");

		for(i = 0; i < EURASIA_USE_DRC_BANK_SIZE; i++)
		{
			if(psFFGenCode->abOutstandingDRC[i])
			{
				psFFGenCode->sDRCReg.uOffset = i;
				INST0(WDF, &psFFGenCode->sDRCReg, IMG_NULL);
			}
		}
	}
}
/******************************************************************************
 * Function Name: GetLabel
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetLabel(FFGenCode *psFFGenCode, IMG_CHAR *pszLabelName)
{
	/* Alloc mem for new label name */
	IMG_CHAR *pszNewLabelName = FFGENMalloc(psFFGenCode->psFFGenContext, (IMG_UINT32)(strlen(pszLabelName) + 10));

	IMG_UINT32 uLabelID = psFFGenCode->uLabelCount;

	if (!pszNewLabelName)
	{
		psFFGenCode->psFFGenContext->pfnPrint("GetLabel: Error, couldn't alloc label name\n");
		return 0;
	}

	/* Construct label name */
	sprintf(pszNewLabelName, "%s_LABEL%u", pszLabelName, psFFGenCode->uLabelCount);
	
	/* Check if we need to increase list size */
	if (psFFGenCode->uLabelCount >= psFFGenCode->uLabelListSize)
	{
		psFFGenCode->uLabelListSize += 20;
		
		psFFGenCode->ppsLabelNames = FFGENRealloc(psFFGenCode->psFFGenContext, psFFGenCode->ppsLabelNames, sizeof(IMG_CHAR *) * psFFGenCode->uLabelListSize);
		
		if (!psFFGenCode->ppsLabelNames)
		{
			psFFGenCode->psFFGenContext->pfnPrint("GetLabel: Error, couldn't alloc label list\n");
			return 0;
		}
	}

	/* Add label name to list */
	psFFGenCode->ppsLabelNames[psFFGenCode->uLabelCount] = pszNewLabelName;

	/* Increase number of labels */
	psFFGenCode->uLabelCount++;

	return uLabelID;
}

/******************************************************************************
 * Function Name: DestroyLabelList
 *
 * Inputs       : psFFGenCode
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyLabelList(FFGenCode *psFFGenCode)
{
	IMG_UINT32 i;

	for (i = 0; i < psFFGenCode->uLabelCount; i++)
	{
		FFGENFree(psFFGenCode->psFFGenContext, psFFGenCode->ppsLabelNames[i]);
	}

	if (psFFGenCode->ppsLabelNames)
	{
		FFGENFree(psFFGenCode->psFFGenContext, psFFGenCode->ppsLabelNames);
	}
}

#if !defined(SGX545)

/******************************************************************************
 * Function Name: SetupNoSchedForInternalRegs
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID SetupNoSchedForInternalRegs(FFGenCode *psFFGenCode)
{
	FFGenInstructionList *psInstructionEntry = psFFGenCode->psCurrentInstructionEntry;
	FFGenInstruction	 *psInst             = IMG_NULL;

#if !defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
	IMG_UINT32 uBackCount = 0;
	IMG_UINT32 uMinBackCount, uMaxBackCount;
	IMG_UINT32 uNumPaddingInstructions;

	/* Scheduling must be disabled on the instruction pair 
	   preceeding the execution of the instructions which reference internal registers

	There are two pairing cases:

	---------		Instruction pair boundary
	Inst-2			<- apply NoSched here
	Inst-1			<- or here
	---------
	EFO
	EFO
	---------

	Different pairing:

	Inst-3			<- apply NoSched here
	Inst-2			<- or here
	---------		Instruction pair boundary		
	Inst-1			
	EFO
	---------
	EFO
	...
	--------- 
	
	If the instructions do not support the NoSched flag then 
	Padding instructions are inserted (No-ops) with NoSched.

	Branch and WDF instructions force descheduling. If either 
	of these instructions is encountered, or a Label (branch
	destination) BEFORE the NoSched has been applied then 
	Padding must be inserted */
	
	/* Is this instruction the first or second in a pair? */
	if (psFFGenCode->uAlignedInstructionCount & 0x1)
	{
		uMinBackCount = 2;
		uMaxBackCount = 3;
		uNumPaddingInstructions = 1;
	}
	else
	{
		uMinBackCount = 1;
		uMaxBackCount = 2;
		uNumPaddingInstructions = 2;
	}

	/* Search the previous instructions for suitable placement of NoSched */
	while (psInstructionEntry && (uBackCount < uMaxBackCount))
	{
		if(psInstructionEntry)
		{
			psInst = &(psInstructionEntry->sInstruction);
			
			/* Branch and WDF deschedule, so need No-op. 
			   Branch destination also implies a deschedule 
			   (because of the branch!) */
			if(psInst->eOpcode == USEASM_OP_LABEL ||
			   psInst->eOpcode == USEASM_OP_WDF ||
			   psInst->eOpcode == USEASM_OP_BR)
			{
				break;
			}

			if (psInst->eOpcode != USEASM_OP_COMMENT &&
				psInst->eOpcode != USEASM_OP_COMMENTBLOCK)
			{
				uBackCount++;

				if((uBackCount >= uMinBackCount) &&
				   (uBackCount <= uMaxBackCount))
				{
					if (OpcodeAcceptsNoSched(psInst->eOpcode))
					{
						/* NoSched applied, so no padding is required */
						SET_NOSCHED();
						uNumPaddingInstructions = 0;

						break;
					}
				}
			}
		}

		psInstructionEntry = psInstructionEntry->psPrev;
	}
	
	/* Insert padding instructions with NoSched */
	if(uNumPaddingInstructions)
	{
		IMG_UINT32 i;

		psInst = &(psFFGenCode->sInstruction);

		for(i=0; i<uNumPaddingInstructions; i++)
		{
			SET_NOSCHED();
			INST(PADDING, "Following code need to be aligned to instruction pairs");
		}
	}
#else
	FFGenInstruction *psPreInst = IMG_NULL;

	/* The same rules apply as above, but the lack of instruction
	   pairing makes things easier */

	while(psInstructionEntry)
	{
		psInst = &psInstructionEntry->sInstruction;

		if(psInst->eOpcode == USEASM_OP_LABEL ||
		   psInst->eOpcode == USEASM_OP_WDF ||
		   psInst->eOpcode == USEASM_OP_BR)
		{
			break;
		}

		if( psInst->eOpcode != USEASM_OP_COMMENT		&&
			psInst->eOpcode != USEASM_OP_COMMENTBLOCK)
		{
			psPreInst = psInst;
			break;
		}

		psInstructionEntry = psInstructionEntry->psPrev;
	}
	
	if(psPreInst && OpcodeAcceptsNoSchedEnhanced(psPreInst->eOpcode))
	{
		/* Insert a couple of  noscheds to make sure the 1st 2 pairs of efos are safe */
		psInst = &psInstructionEntry->sInstruction;
		SET_NOSCHED();
	}
	else
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		SET_NOSCHED();
		INST(PADDING, "Following code need to be aligned to instruction pairs");
	}
#endif
}

#endif

#if !defined(SGX545) && !defined(SGX_FEATURE_USE_VEC34)
/******************************************************************************
 * Function Name: M4x4_efos
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID M4x4_efos(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_UINT32 uDestBaseoffset)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

	FFGenReg *psTemp   = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0, 0, 1, IMG_NULL);

	/*	
		Instructions	SGX535		SGX520

		prevInst		nosched
		prevInst		nosched		nosched

		sdm				nosched		nosched
		dma				nosched		nosched
		dma				nosched		nosched
		dma				nosched		nosched
		sdm				nosched		nosched
		dma				nosched		nosched
		dma							nosched
		dma									
		mov							
	*/
	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	SETSRCOFF(0, 0); SETSRCOFF(1, 0); SETSRCOFF(2, 4);
	INST3(SDM, psTemp, psVector, psMatrix, psMatrix, IMG_NULL);

	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	SETSRCOFF(0, 1); SETSRCOFF(1, 1); SETSRCOFF(2, 5);
	INST3(DMA, psTemp, psVector, psMatrix, psMatrix, IMG_NULL);

	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	SETSRCOFF(0, 2); SETSRCOFF(1, 2); SETSRCOFF(2, 6);
	INST3(DMA, psTemp, psVector, psMatrix, psMatrix, IMG_NULL);

	/* Enable dest base offset */
	ENABLE_DST_BASE_OFFSET(psFFGenCode, uDestBaseoffset);

	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	SETDESTOFF(0);  SETSRCOFF(0, 3); SETSRCOFF(1, 3); SETSRCOFF(2, 7);
	INST3(DMA, psDest, psVector, psMatrix, psMatrix, IMG_NULL);

	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_I1);
	SETDESTOFF(1);  SETSRCOFF(0, 0); SETSRCOFF(1, 8); SETSRCOFF(2, 12);
	INST3(SDM, psDest, psVector, psMatrix, psMatrix, IMG_NULL);

	/* Disable dest base offset */
	DISABLE_DST_BASE_OFFSET(psFFGenCode);

	SET_NOSCHED();
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	SETSRCOFF(0, 1); SETSRCOFF(1, 9); SETSRCOFF(2, 13);
	INST3(DMA, psTemp, psVector, psMatrix, psMatrix, IMG_NULL);

#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
	SET_NOSCHED();
#endif
	SETSRCOFF(0, 2); SETSRCOFF(1, 10); SETSRCOFF(2, 14);
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	INST3(DMA, psTemp, psVector, psMatrix, psMatrix, IMG_NULL);

	/* Enable dest base offset */
	ENABLE_DST_BASE_OFFSET(psFFGenCode, uDestBaseoffset);

	SETDESTOFF(2);  SETSRCOFF(0, 3); SETSRCOFF(1, 11); SETSRCOFF(2, 15);
	USEINTERNALREG(EURASIA_USE1_EFO_DSRC_A0); // Only so it matches the text input version - doesn't do anything!
	INST3(DMA, psDest, psVector, psMatrix, psMatrix, IMG_NULL);

	psFFGenCode->sInternalReg.uOffset = 1; 
	SETDESTOFF(3);
	INST1(MOV, psDest, &(psFFGenCode->sInternalReg), IMG_NULL);

	/* Disable dest base offset */
	DISABLE_DST_BASE_OFFSET(psFFGenCode);

	ReleaseReg(psFFGenCode, psTemp);
}
#endif /* #if !defined(SGX545) */

/******************************************************************************
 * Function Name: M4x4_dp4
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID M4x4_dp4(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_UINT32 uDestBaseOffset)
{
	IMG_UINT32 i;

	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		for (i = 0; i < 4; i++)
		{
			SETDESTOFF(DP4_ADJUST(i)); SETSRCOFF(1, i * 4);
			INST2(FDP4, psDest, psVector, psMatrix, IMG_NULL);
		}
	}
	else
#endif
	{
		/* Enable dest base offset */
		ENABLE_DST_BASE_OFFSET(psFFGenCode, uDestBaseOffset);

		if (psDest->uOffset < 3)
		{
			SMLSI(0, 1, 1, 1, "Disable dest increments");

			for (i = 0; i < 4; i++)
			{
				SETDESTOFF(i); SETSRCOFF(1, i * 4);
				INST2(FDP4, psDest, psVector, psMatrix, IMG_NULL);
			}

			SMLSI(1, 1, 1, 1, "Enable dest increments");
		}
		else
		{
			for (i = 0; i < 4; i++)
			{
				SETDESTOFF(DP4_ADJUST(i)); SETSRCOFF(1, i * 4);
				INST2(FDP4, psDest, psVector, psMatrix, IMG_NULL);
			}
		}

		/* Disable dest base offset */
		DISABLE_DST_BASE_OFFSET(psFFGenCode);
	}
}

#if defined(SGX_FEATURE_USE_VEC34)

/******************************************************************************
 * Function Name: M4x4_vdp4
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
static IMG_VOID M4x4_vdp4(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_UINT32 uDestBaseOffset)
{
	FFGenInstruction *psInst = &(psFFGenCode->sInstruction);
	FFGenReg *psSwizzleReg  = &(psFFGenCode->sSwizzleXYZW);
	FFGenReg *psInternalReg = &(psFFGenCode->sInternalReg);
	FFGenReg *psIntSrcSelReg = &(psFFGenCode->sIntSrcSelReg);
	FFGenReg sIntSrcSelReg2 = psFFGenCode->sIntSrcSelReg;
	FFGenReg sVector2 = *psVector;

	/* Enable dest base offset */
	ENABLE_DST_BASE_OFFSET(psFFGenCode, uDestBaseOffset);

#if !defined(FFGEN_UNIFLEX)
	/* Vector must be aligned to 2 */
	PVR_ASSERT((psVector->uOffset & 1) == 0);
	PVR_ASSERT((psDest->uOffset & 1) == 0);
#endif

	SET_NOSCHED();
	SETDESTMASK(0xF);

	sVector2.uOffset += 2;

	INST3(VPCKF32F32, psInternalReg, psVector, &sVector2, psSwizzleReg, IMG_NULL);

	SET_REPEAT_COUNT(4);
	SET_INTSRCSEL(psIntSrcSelReg, USEASM_INTSRCSEL_NONE);
	SET_INTSRCSEL(&sIntSrcSelReg2, USEASM_INTSRCSEL_INCREMENTUS);
	SETDESTMASK(0xF);

	INST6(VDP4, psDest, psMatrix, psSwizzleReg, psInternalReg, psSwizzleReg, psIntSrcSelReg, &sIntSrcSelReg2, IMG_NULL);

	/* Disable dest base offset */
	DISABLE_DST_BASE_OFFSET(psFFGenCode);
}

#endif /* #if defined(SGX_FEATURE_USE_VEC34) */

 /******************************************************************************
 * Function Name: M4x4
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID M4x4(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_UINT32 uDestBaseOffset, IMG_CHAR *pszComment)
{	
	FFGenInstruction	 *psInst             = IMG_NULL;
	FFGenReg			 *psTempVector		 = IMG_NULL;
	/* Print comment */
	if (pszComment)
	{
		COMMENT(psFFGenCode, "%s", pszComment);
	}

	psInst = &(psFFGenCode->sInstruction);

	/* If destination is same as source vector, copy
	   source vector to temporaries */
	if((psDest->eType == psVector->eType) &&
	   (psDest->uOffset == psVector->uOffset))
	{
		psTempVector = GetReg(psFFGenCode, USEASM_REGTYPE_TEMP, 0, 0, 4, IMG_NULL);
		
		SET_REPEAT_COUNT(4);
		INST1(MOV, psTempVector, psVector, "Move source vector into temporary");
		
		/* Source data from temporaries */
		psVector = psTempVector;
	}

	if ((psVector->eType == USEASM_REGTYPE_PRIMATTR || psVector->eType == USEASM_REGTYPE_TEMP) &&
		(psMatrix->uIndex == USEREG_INDEX_NONE)
#ifdef FFGEN_UNIFLEX
		&& !IF_FFGENCODE_UNIFLEX
#endif
		)
	{
#if defined(SGX545)
		M4x4_dp4(psFFGenCode, psDest, psVector, psMatrix, uDestBaseOffset);
#else
#if defined(SGX_FEATURE_USE_VEC34)
		SetupNoSchedForInternalRegs(psFFGenCode);

		M4x4_vdp4(psFFGenCode, psDest, psVector, psMatrix, uDestBaseOffset);

#else

		SetupNoSchedForInternalRegs(psFFGenCode);

		M4x4_efos(psFFGenCode, psDest, psVector, psMatrix, uDestBaseOffset);
#endif
#endif
	}
	else
	{
		M4x4_dp4(psFFGenCode, psDest, psVector, psMatrix, uDestBaseOffset);
	}

	if(psTempVector != IMG_NULL)
	{
		ReleaseReg(psFFGenCode, psTempVector);
	}
}


/******************************************************************************
 * Function Name: M4x3
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID M4x3(FFGenCode *psFFGenCode, FFGenReg *psDest, FFGenReg *psVector, FFGenReg *psMatrix, IMG_CHAR *pszComment)
{
	IMG_UINT32 i;
	FFGenInstruction	 *psInst;
	psInst = &(psFFGenCode->sInstruction);

	/* Print comment */
	if (pszComment)
	{
		COMMENT(psFFGenCode, "%s", pszComment);
	}

	if (psDest->uOffset < 2
#ifdef FFGEN_UNIFLEX
	&& !IF_FFGENCODE_UNIFLEX
#endif
	)
	{
		SMLSI(0, 1, 1, 1, "Disable dest increments");

		for (i = 0; i < 3; i++)
		{
			SETDESTOFF(i); SETSRCOFF(1, i * 4);
			INST2(FDP3, psDest, psVector, psMatrix, IMG_NULL);
		}

		SMLSI(1, 1, 1, 1, "Enable dest increments");
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			SETDESTOFF(DP3_ADJUST(i)); SETSRCOFF(1, i * 4);
			INST2(FDP3, psDest, psVector, psMatrix, IMG_NULL);
		}
	}
}

/******************************************************************************
 * Function Name: IF_PRED
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IF_PRED(FFGenCode *psFFGenCode, FFGenReg *psPredReg, IMG_CHAR *pszDesc, IMG_CHAR *pszComment)
{
#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		UNIFLEX_INST sInst = {0};
		sInst.eOpCode = UFOP_IFP;

		sInst.asSrc[0].eType = UFREG_TYPE_PREDICATE;
		sInst.asSrc[0].uNum  = psPredReg->uOffset;
		sInst.asSrc[0].u.uSwiz = UFREG_SWIZ_RRRR;

		AddUniFlexInstructionToList(psFFGenCode, &sInst);
		
		INCREASE_INDENT();
	}
	else
#endif
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psLabel = &(psFFGenCode->sLabelReg);

		/* get a label for this predicate */
		IMG_UINT32 uLabelID = GetLabel(psFFGenCode, pszDesc);

		if (psFFGenCode->uConditionalDepth >= FFTNLGEN_MAX_NESTED_DEPTH)
		{
			psFFGenCode->psFFGenContext->pfnPrint("IF_PRED: Error, maximum nested depth is %d\n", FFTNLGEN_MAX_NESTED_DEPTH);
			return;
		}

		/* Store this label name */
		psFFGenCode->uConditionalLabelStack[psFFGenCode->uConditionalDepth] = uLabelID;

		/* Print comment */
		if (pszComment)
		{
			COMMENT(psFFGenCode, "%s", pszComment);
		}

		psLabel->uOffset = uLabelID;
		USE_NEGPRED(psPredReg);
		INST0(BR, psLabel, IMG_NULL);

		/* Increase stack depth count */
		psFFGenCode->uConditionalDepth++;

		INCREASE_INDENT();
	}
}

/******************************************************************************
 * Function Name: ELSE_PRED
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ELSE_PRED(FFGenCode *psFFGenCode, IMG_CHAR *pszDesc, IMG_CHAR *pszComment)
{
#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		UNIFLEX_INST sInst = {0};
		sInst.eOpCode = UFOP_ELSE;
		AddUniFlexInstructionToList(psFFGenCode, &sInst);
	}
	else
#endif
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psLabel = &(psFFGenCode->sLabelReg);

		IMG_UINT32 uElseBlockLabelID, uEndBlockLabelID;

		if (psFFGenCode->uConditionalDepth <= 0)
		{
			psFFGenCode->psFFGenContext->pfnPrint("END_PRED: Error, condition stack is zero\n");
			return;
		}

		/* Fetch label name stored*/
		uElseBlockLabelID = psFFGenCode->uConditionalLabelStack[psFFGenCode->uConditionalDepth - 1];

		/* Get a new label for endif */
		uEndBlockLabelID = GetLabel(psFFGenCode, pszDesc);

		/* Replace label with new target */
		psFFGenCode->uConditionalLabelStack[psFFGenCode->uConditionalDepth - 1] = uEndBlockLabelID;

		psLabel->uOffset = uEndBlockLabelID;
		INST0(BR, psLabel, IMG_NULL);

		if(pszComment)
		{
			COMMENT(psFFGenCode, "%s", pszComment);
		}

		/* Generate IF target lable */
		psLabel->uOffset = uElseBlockLabelID;
		INST0(LABEL, psLabel, IMG_NULL);
	}
}

/******************************************************************************
 * Function Name: END_PRED
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID END_PRED(FFGenCode *psFFGenCode, IMG_CHAR *pszComment)
{
#ifdef FFGEN_UNIFLEX
	if(IF_FFGENCODE_UNIFLEX)
	{
		UNIFLEX_INST sInst = {0};
		sInst.eOpCode = UFOP_ENDIF;
		AddUniFlexInstructionToList(psFFGenCode, &sInst);
		
		DECREASE_INDENT();
	}
	else
#endif
	{
		FFGenInstruction *psInst = &(psFFGenCode->sInstruction);

		FFGenReg *psLabel = &(psFFGenCode->sLabelReg);

		IMG_UINT32 uLabelID;

		if (psFFGenCode->uConditionalDepth <= 0)
		{
			psFFGenCode->psFFGenContext->pfnPrint("END_PRED: Error, condition stack is zero\n");
			return;
		}

		/* Fetch label name */
		uLabelID = psFFGenCode->uConditionalLabelStack[psFFGenCode->uConditionalDepth - 1];

		/* Print comment */
		if (pszComment)
		{
			COMMENT(psFFGenCode, "%s", pszComment);
		}

		/* Generate IF target lable */
		psLabel->uOffset = uLabelID;
		INST0(LABEL, psLabel, IMG_NULL);

		/* Decrease stack depth count */
		psFFGenCode->uConditionalDepth--;

		DECREASE_INDENT();
	}
}

#if defined(STANDALONE) || defined(DEBUG)
#define COMMENT_BUFFER_SIZE 255
#endif

/******************************************************************************
 * Function Name: COMMENT
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV COMMENT(FFGenCode *psFFGenCode,
										   const IMG_CHAR *pszFormat, ...)
{
#if !defined(STANDALONE) && !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psFFGenCode);
	PVR_UNREFERENCED_PARAMETER(pszFormat);
	return;
#else

	va_list vaArgs;
	IMG_UINT32 uStringPos = 0;
	FFGenInstruction sInst = {0};
	FFGenInstruction *psInst = &sInst;
	IMG_CHAR acCommentBuffer[COMMENT_BUFFER_SIZE];

	va_start (vaArgs, pszFormat);

	uStringPos += vsprintf(&(acCommentBuffer[uStringPos]), pszFormat, vaArgs);

	va_end (vaArgs);

	if (uStringPos >= COMMENT_BUFFER_SIZE)
	{
		psFFGenCode->psFFGenContext->pfnPrint("COMMENT: Warning comment buffer exceeded, mem has been overwritten\n");
		return;
	}

	/* Print the comment */
	INST(COMMENT, acCommentBuffer);

#endif
}

/******************************************************************************
 * Function Name: NEW_BLOCK
 *
 * Inputs       :
 * Outputs      :
 * Returns      :
 * Globals Used :
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV NEW_BLOCK(FFGenCode *psFFGenCode,
											 const IMG_CHAR *pszFormat, ...)
{
#if !defined(STANDALONE) && !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psFFGenCode);
	PVR_UNREFERENCED_PARAMETER(pszFormat);
	return;
#else
	
	va_list vaArgs;
	IMG_UINT32 uStringPos = 0;
	FFGenInstruction sInst = {0};
	FFGenInstruction *psInst = &sInst;
	IMG_CHAR acCommentBuffer[COMMENT_BUFFER_SIZE];

	va_start (vaArgs, pszFormat);

	uStringPos += vsprintf(&(acCommentBuffer[uStringPos]), pszFormat, vaArgs);

	va_end (vaArgs);

	if (uStringPos >= COMMENT_BUFFER_SIZE)
	{
		psFFGenCode->psFFGenContext->pfnPrint("NEW_BLOCK: Warning comment buffer exceeded, mem has been overwritten\n");
		return;
	}

	/* Print the comment */
	INST(COMMENTBLOCK, acCommentBuffer);

#endif
}

/******************************************************************************
 End of file (inst.c)
******************************************************************************/

