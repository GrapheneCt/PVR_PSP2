 /*****************************************************************************
 * Name         : dce.c
 * Title        : Dead Code Elimination, Liveness calculation
 * Created      : April 2005
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
 * $Log: dce.c $
 * ./
 *  --- Revision Logs Removed --- 
 *  
 * Increase the cases where a vec4 instruction can be split into two vec2
 * parts.
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "uscshrd.h"
#include "usedef.h"
#include "bitops.h"

FORCE_INLINE 
PUSC_LIVE_CALLBACK SetInstCallData(PUSC_LIVE_CALLBACK psCallback,
								   PINST psInst,
								   USC_LCB_POS ePos,
								   IMG_BOOL bDest,
								   USC_LCB_OPERAND_TYPE eRegType,
								   IMG_PVOID pvRegData)
/*********************************************************************************
 Function			: SetInstCallData

 Description		: Set instruction level values in a callback data structure.

 Parameters			: psCallBack   - The structure to fill
                      psInst       - Instruction being processed
                      ePos         - Point of call (function entry, exit or processing register)
                      bDest        - Register is written (IMG_TRUE) or read (IMG_FALSE)
                      eRegType     - Register being written/read
                      pvRegData     - Register specific data

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	if (psCallback == NULL)
		return NULL;

	psCallback->psInst = psInst;
	psCallback->bIsBlock = IMG_FALSE;
	psCallback->ePos = ePos;

	psCallback->bDest = bDest;
	psCallback->eRegType = eRegType;
	psCallback->pvRegData = pvRegData;

	return psCallback;
}

FORCE_INLINE 
PUSC_LIVE_CALLBACK SetBlockCallData(PUSC_LIVE_CALLBACK psCallback,
									PCODEBLOCK psBlock,
									USC_LCB_POS ePos,
									IMG_UINT32 uBranch)
/*********************************************************************************
 Function			: SetBlockCallData

 Description		: Set codeblock level values in a callback data structure.

 Parameters			: psCallBack   - The structure to fill
                      psBlock      - Codeblock being processed
                      ePos         - Point of call (entry, exit)
                      uBranch      - 0: Sequential block, n: n'th branch of a conditional

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	if (psCallback == NULL)
		return NULL;

	psCallback->psBlock = psBlock;
	psCallback->bIsBlock = IMG_TRUE;
	psCallback->ePos = ePos;
	psCallback->uBranch = uBranch;
	
	return psCallback;
}

IMG_INTERNAL
IMG_VOID ClearRegLiveSet(PINTERMEDIATE_STATE psState,
						 PREGISTER_LIVESET psLiveSet)
/*********************************************************************************
 Function			: ClearRegLiveSet

 Description		: Clear a register liveness set.

 Parameters			: psState		- Compiler state
					  psLiveSet		- Liveness set to clear

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	if(psLiveSet == NULL)
		return;

	memset(psLiveSet->puIndexReg, 0, sizeof(psLiveSet->puIndexReg));

	ClearVector(psState, &psLiveSet->sFpInternal);
	ClearVector(psState, &psLiveSet->sPredicate);
	ClearVector(psState, &psLiveSet->sPrimAttr);
	ClearVector(psState, &psLiveSet->sTemp);
	ClearVector(psState, &psLiveSet->sOutput);
}

IMG_INTERNAL
IMG_VOID InitRegLiveSet(PREGISTER_LIVESET psLiveSet)
/*********************************************************************************
 Function			: InitRegLiveSet

 Description		: Initialise a register liveness set.
 
 Parameters			: psLiveSet		- Liveness set to initialise

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	if(psLiveSet == NULL)
		return;

	memset(psLiveSet->puIndexReg, 0, sizeof(psLiveSet->puIndexReg));
	psLiveSet->bLinkReg = IMG_FALSE;

	InitVector(&psLiveSet->sFpInternal, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	InitVector(&psLiveSet->sPredicate, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	InitVector(&psLiveSet->sPrimAttr, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	InitVector(&psLiveSet->sTemp, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	InitVector(&psLiveSet->sOutput, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
}

IMG_INTERNAL
IMG_VOID FreeRegLiveSet(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET psLiveSet)
/*********************************************************************************
 Function			: FreeRegLiveSet

 Description		: Free a register liveness set.

 Parameters			: psState		- Compiler state
					  psLiveSet		- Liveness set to initialise

 Return				: Nothing
*********************************************************************************/
{
	if(psLiveSet == NULL)
		return;

	ClearVector(psState, &psLiveSet->sFpInternal);
	ClearVector(psState, &psLiveSet->sPredicate);
	ClearVector(psState, &psLiveSet->sPrimAttr);
	ClearVector(psState, &psLiveSet->sTemp);
	ClearVector(psState, &psLiveSet->sOutput);
	UscFree(psState, psLiveSet);
}

IMG_INTERNAL
PREGISTER_LIVESET AllocRegLiveSet(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: AllocRegLiveSet

 Description		: Allocate storage for a register live set.

 Parameters			: psState			- Compiler state.

 Globals Effected	: None

 Return				: The allocated live set.
*********************************************************************************/
{
	PREGISTER_LIVESET	psLiveset;

	psLiveset = UscAlloc(psState, sizeof(REGISTER_LIVESET));
	InitRegLiveSet(psLiveset);
	return psLiveset;
}

IMG_INTERNAL
IMG_VOID CopyRegLiveSet(PINTERMEDIATE_STATE psState,
						PREGISTER_LIVESET psSrc,
						PREGISTER_LIVESET psDst)
/*********************************************************************************
 Function			: CopyRegLiveSet

 Description		: Initialise a register liveness set.

 Parameters			: psState	- Compiler state
					  psSrc		- Source
					  psDst	- Destination

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if(psSrc == NULL || psDst == NULL)
		return;

	memcpy(psDst->puIndexReg, psSrc->puIndexReg, sizeof(psDst->puIndexReg));

	VectorCopy(psState, &psSrc->sFpInternal, &psDst->sFpInternal);
	VectorCopy(psState, &psSrc->sPredicate, &psDst->sPredicate);
	VectorCopy(psState, &psSrc->sPrimAttr, &psDst->sPrimAttr);
	VectorCopy(psState, &psSrc->sTemp, &psDst->sTemp);
	VectorCopy(psState, &psSrc->sOutput, &psDst->sOutput);

	psDst->bLinkReg = psSrc->bLinkReg;
}

IMG_INTERNAL
IMG_UINT32 ChanSelToMask(PINTERMEDIATE_STATE psState, PTEST_DETAILS psTest)
/*********************************************************************************
 Function			: ChanSelToMask

 Description		: Convert a test instruction channel select to the mask of
					  channels used from the ALU result.
 
 Parameters			: psState	- Internal compiler state
					  psTest	- Test details.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	switch (psTest->eChanSel)
	{
		case USEASM_TEST_CHANSEL_C0:
		{ 
			return USC_X_CHAN_MASK; 
		}
		case USEASM_TEST_CHANSEL_C1:
		{
			return USC_Y_CHAN_MASK;
		}
		case USEASM_TEST_CHANSEL_C2:
		{
			return USC_Z_CHAN_MASK;
		}
		case USEASM_TEST_CHANSEL_C3:	
		{
			return USC_W_CHAN_MASK;
		}
		case USEASM_TEST_CHANSEL_ANDALL:
		case USEASM_TEST_CHANSEL_ORALL:
		{
			return USC_XYZW_CHAN_MASK;
		}
		case USEASM_TEST_CHANSEL_AND02:
		case USEASM_TEST_CHANSEL_OR02:	
		{
			return USC_X_CHAN_MASK | USC_Z_CHAN_MASK;
		}
		default: imgabort();
	}
}


static IMG_UINT32 GetSourcesUsedFromPack(PINTERMEDIATE_STATE psState,
										 const INST *psInst, 
										 IMG_UINT32 uDestMask, 
										 IMG_UINT32 uLiveChansInDest)
/*********************************************************************************
 Function			: GetSourcesUsedFromPack

 Description		: Get the sources used by a pack instruction.

 Parameters			: psState			- Internal compiler state
					  psInst			- Pack instruction.
					  uDestMask			- Destination mask for the pack
					  uLiveChansInDest	- Channels live in the destination.

 Globals Effected	: None

 Return				: Mask of the sources used.
*********************************************************************************/
{
	IMG_UINT32	uArgMask = 0;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	switch (psInst->eOpcode)
	{
		/*
			Pack instructions with a 16-bit destination format.
		*/
		case IUNPCKS16S8:
	    case IUNPCKS16U8:
		case IUNPCKU16U8:
		case IUNPCKU16S8:
		case IUNPCKU16U16:
		case IUNPCKU16S16:
		case IUNPCKU16F16:
		case IPCKU16F32:
		case IUNPCKS16U16:
		case IPCKS16F32:
		case IUNPCKF16C10:
		case IUNPCKF16O8:
		case IUNPCKF16U8:
		case IUNPCKF16S8:
		case IUNPCKF16U16:
		case IUNPCKF16S16:
		case IPCKF16F16:
		case IPCKF16F32:
		{
			if (uDestMask & 3)
			{
				if (uLiveChansInDest & 3)
				{
					uArgMask |= 1;
				}
				if (uDestMask & uLiveChansInDest & 12)
				{
					uArgMask |= 2;
				}
			}
			else
			{
				if (uDestMask & uLiveChansInDest & 12)
				{
					uArgMask |= 1;
				}
			}
			break;
		}

		/*
			Pack instructions with a 8-bit/10-bit destination format.
		*/
		case IPCKU8U8:
		case IPCKU8S8:
		case IPCKU8U16:
		case IPCKU8S16:
		case IPCKS8U8:
		case IPCKS8U16:
		case IPCKS8S16:
		case IPCKU8F16:
		case IPCKU8F32:
		case IPCKS8F16:
		case IPCKS8F32:
		case IPCKC10C10:
		case IPCKC10F32:
		case IPCKC10F16:
		{
			IMG_UINT32	uChan, uArg;

			/*
				The channel corresponding to the Nth set bit in the
				mask receives source 0 if N is even or source 1 if
				N is odd.
			*/
			for (uChan = 0, uArg = 0; uChan < 4; uChan++)
			{
				if (uDestMask & (1U << uChan))
				{
					if (uLiveChansInDest & (1U << uChan))
					{
						uArgMask |= (1U << uArg);
					}
					uArg = (uArg + 1) % 2;
				}
			}
			break;
		}

		/*
			Pack instructions with a 32-bit destination format.
		*/
		case IUNPCKF32C10:
		case IUNPCKF32O8:
		case IUNPCKF32U8:
		case IUNPCKF32S8:
		case IUNPCKF32U16:
		case IUNPCKF32S16:
		case IUNPCKF32F16:
		{
			uArgMask = 1;
			break;
		}
		default: imgabort();
	}
	return uArgMask;
}

static
IMG_UINT32 GetChansUsedFromPackSource(PINTERMEDIATE_STATE	psState,
									  const INST			*psInst, 
									  IMG_UINT32			uArg)
/*********************************************************************************
 Function			: GetChansUsedFromPackSource

 Description		: Get channels used by a pack instruction source.

 Parameters			: psState	- Internal compiler state
					  psInst	- Pack instruction.
					  uArg		- Argument number.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uChansUsed;
	IMG_UINT32	uComponent = psInst->u.psPck->auComponent[uArg];
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if(TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_CHANS_USED_PCK_U8C10SRC))
	{
		/*
			Pack instructions with 8-bit/10-bit source format.
			
			IUNPCKS16S8, IUNPCKS16S8, IUNPCKU16U8, IUNPCKF16C10, IUNPCKF16O8, IUNPCKF16U8
			IUNPCKF16S8, IPCKU8U8, IPCKC10C10, IUNPCKF32C10, IUNPCKF32O8, IUNPCKF32U8, IUNPCKF32S8
		*/
		uChansUsed = 1 << uComponent;
	}
	else
	if(TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_CHANS_USED_PCK_F16SRC))
	{
		/*
			Pack instructions with a 16-bit source format.
			
			IUNPCKF16U16, IUNPCKF16S16, IPCKF16F16, IPCKU8F16, IUNPCKU16U16, IUNPCKU16F16, 
			IPCKC10F16, IUNPCKF32U16, IUNPCKF32S16, IUNPCKF32F16
		*/
		uChansUsed = 3 << uComponent;
	}
	else
	{
		/*
			Pack instructions with a 32-bit source format.
			
			IPCKU16F32, IPCKS16F32, IPCKF16F32, IPCKU8F32, IPCKC10F32
		*/
		uChansUsed = USC_DESTMASK_FULL;
	}

	return uChansUsed;
}

/*********************************************************************************
 Function			: RemapC10Channels

 Description		: Remap the live channels to a C10 argument to an integer 
					  instruction for the hardware behaviour whereby the alpha
					  of a unified store register actually comes from the blue
					  channel.
 
 Parameters			: psArg			- Argument to remap for.
					  uLiveChans	- Live channels in the argument.

 Globals Effected	: None

 Return				: Remapped mask of the channels used.
*********************************************************************************/
static IMG_UINT32 RemapC10Channels(PINTERMEDIATE_STATE psState, const ARG *psArg, IMG_UINT32 uLiveChans)
{
	if (psArg->eFmt == UF_REGFORMAT_C10)
	{
		IMG_BOOL	bUnifiedStore = IMG_FALSE;

		/*
			Non-temporaries are always in the unified store.
		*/
		if (psArg->uType != USEASM_REGTYPE_TEMP && 
			psArg->uType != USEASM_REGTYPE_FPINTERNAL &&
			psArg->uType != USC_REGTYPE_REGARRAY)
		{
			bUnifiedStore = IMG_TRUE;
		}
		/*
			Before C10 register allocation we are assuming that all
			temporary registers are 40 bits wide.  Afterwards those
			registers that need 40 bits have been mapped to the
			internal registers.
		*/
		if ((psState->uFlags & USC_FLAGS_POSTC10REGALLOC) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0 &&
			psArg->uType != USEASM_REGTYPE_FPINTERNAL)
		{
			bUnifiedStore = IMG_TRUE;
		}

		/*
			Remap alpha to red.
		*/
		if (bUnifiedStore && (uLiveChans & 8) != 0)
		{
			uLiveChans = (uLiveChans & ~8U) | 1;
		}
	}
	return uLiveChans;
}

/*********************************************************************************
 Function			: GetLiveChansInSOP2Argument

 Description		: Get channels used by an instruction source to SOP2.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
static IMG_UINT32 GetLiveChansInSOP2Argument(const INST *psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest)
{
	IMG_UINT32 uLiveChans;

	uLiveChans = 0;
	if (uArg == 0)
	{
		if (!(psInst->u.psSop2->uCSel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop2->bComplementCSel1))
		{
			uLiveChans |= (uLiveChansInDest & 7);
		}
		if (!(psInst->u.psSop2->uASel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop2->bComplementASel1))
		{
			uLiveChans |= (uLiveChansInDest & 8);
		}
	}
	if (uArg == 1)
	{
		if (!(psInst->u.psSop2->uCSel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop2->bComplementCSel2))
		{
			uLiveChans |= (uLiveChansInDest & 7);
		}
		if (!(psInst->u.psSop2->uASel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop2->bComplementASel2))
		{
			uLiveChans |= (uLiveChansInDest & 8);
		}
	}
	if ((uLiveChansInDest & 7) != 0 &&
		(psInst->u.psSop2->uCSel1 == (USEASM_INTSRCSEL_SRC1 + uArg) || 
		 psInst->u.psSop2->uCSel2 == (USEASM_INTSRCSEL_SRC1 + uArg)))
	{
		uLiveChans |= (uLiveChansInDest & 7);
	}
	if ((uLiveChansInDest & 8) != 0 &&
		(psInst->u.psSop2->uASel1 == (USEASM_INTSRCSEL_SRC1 + uArg) || 
		 psInst->u.psSop2->uASel2 == (USEASM_INTSRCSEL_SRC1 + uArg)))
	{
		uLiveChans |= (uLiveChansInDest & 8);
	}
	if ((uLiveChansInDest & 7) != 0 &&
		(psInst->u.psSop2->uCSel1 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg) || 
		 psInst->u.psSop2->uCSel2 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg)))
	{
		uLiveChans |= 8;
	}
	if ((uLiveChansInDest & 8) != 0 &&
		(psInst->u.psSop2->uASel1 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg) || 
		 psInst->u.psSop2->uASel2 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg)))
	{
		uLiveChans |= 8;
	}

	return uLiveChans;
}

/*********************************************************************************
 Function			: GetLiveChansInLRP1Argument

 Description		: Get channels used by an instruction source to LRP1.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
static IMG_UINT32 GetLiveChansInLRP1Argument(PINTERMEDIATE_STATE psState, const INST *psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest)
{
	IMG_UINT32 uLiveChans;

	PVR_UNREFERENCED_PARAMETER(psState);
	
	uLiveChans = 0;
	/*
		RGB = (1 - CS) * MOD(CSEL10) + CS * MOD(CSEL11)

		where CS = SRC0RGB or SRC0ALPHA
	*/
	if (uLiveChansInDest & 7)
	{
		/*
			Check if either CSEL10/CSEL11 multiplies source 0 by something non-zero.
		*/
		if (uArg == 0)
		{
			if (!(psInst->u.psLrp1->uCSel10 == USEASM_INTSRCSEL_ZERO && !psInst->u.psLrp1->bComplementCSel10) ||
				!(psInst->u.psLrp1->uCSel11 == USEASM_INTSRCSEL_ZERO && !psInst->u.psLrp1->bComplementCSel11))
			{
				if (psInst->u.psLrp1->uCS == USEASM_INTSRCSEL_SRC0)
				{
					uLiveChans |= (uLiveChansInDest & 7);
				}
				else
				{
					ASSERT(psInst->u.psLrp1->uCS == USEASM_INTSRCSEL_SRC0ALPHA);
					uLiveChans |= 8;
				}
			}
		}
		/* Check for this source being multipled by CS/1-CS. */
		if (psInst->u.psLrp1->uCSel10 == (USEASM_INTSRCSEL_SRC0 + uArg) ||
			psInst->u.psLrp1->uCSel11 == (USEASM_INTSRCSEL_SRC0 + uArg))
		{
			uLiveChans |= (uLiveChansInDest & 7);
		}
	}
	/*
		ALPHA = MOD(ASEL1) * SRC1 + MOD(ASEL2) * SRC2
	*/
	if (uLiveChansInDest & 8)
	{
		/* Check for this source being multipled by either of the two arguments. */
		if (psInst->u.psLrp1->uASel1 == (USEASM_INTSRCSEL_SRC0ALPHA + uArg) || 
			psInst->u.psLrp1->uASel2 == (USEASM_INTSRCSEL_SRC2ALPHA + uArg))
		{
			uLiveChans |= 8;
		}
		/* Check for multiplying src1 by something non-zero. */
		if (uArg == 1 && !(psInst->u.psLrp1->uASel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psLrp1->bComplementASel1))
		{
			uLiveChans |= 8;
		}
		/* Check for multiplying src2 by something non-zero. */
		if (uArg == 2 && !(psInst->u.psLrp1->uASel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psLrp1->bComplementASel2))
		{
			uLiveChans |= 8;
		}
	}

	return uLiveChans;
}

/*********************************************************************************
 Function			: GetLiveChansInSOP3Argument

 Description		: Get channels used by an instruction source to SOP3.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 GetLiveChansInSOP3Argument(PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest)
{
	IMG_UINT32 uLiveChans;

	uLiveChans = 0;
	if (uArg == 0 && psInst->u.psSop3->uCoissueOp == USEASM_OP_ALRP)
	{
		if (!(psInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementASel1) ||
			!(psInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementASel2))
		{
			uLiveChans |= (uLiveChansInDest & 8);
		}
	}
	if (uArg == 1)
	{
		if (!(psInst->u.psSop3->uCSel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementCSel1))
		{
			uLiveChans |= (uLiveChansInDest & 7);
		}
		if (psInst->u.psSop3->uCoissueOp == USEASM_OP_ASOP)
		{
			if (!(psInst->u.psSop3->uASel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementASel1))
			{
				uLiveChans |= (uLiveChansInDest & 8);
			}
		}
	}
	if (uArg == 2)
	{
		if (!(psInst->u.psSop3->uCSel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementCSel2))
		{
			uLiveChans |= (uLiveChansInDest & 7);
		}
		if (psInst->u.psSop3->uCoissueOp == USEASM_OP_ASOP)
		{
			if (!(psInst->u.psSop3->uASel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSop3->bComplementASel2))
			{
				uLiveChans |= (uLiveChansInDest & 8);
			}
		}
	}
	if ((uLiveChansInDest & 7) != 0 &&
		(psInst->u.psSop3->uCSel1 == (USEASM_INTSRCSEL_SRC0 + uArg) || 
		 psInst->u.psSop3->uCSel2 == (USEASM_INTSRCSEL_SRC0 + uArg)))
	{
		uLiveChans |= (uLiveChansInDest & 7);
	}
	if ((uLiveChansInDest & 8) != 0 &&
		(psInst->u.psSop3->uASel1 == (USEASM_INTSRCSEL_SRC0 + uArg) || 
		 psInst->u.psSop3->uASel2 == (USEASM_INTSRCSEL_SRC0 + uArg)))
	{
		uLiveChans |= (uLiveChansInDest & 8);
	}
	if ((uLiveChansInDest & 7) != 0 &&
		(psInst->u.psSop3->uCSel1 == (USEASM_INTSRCSEL_SRC0ALPHA + uArg) || 
		 psInst->u.psSop3->uCSel2 == (USEASM_INTSRCSEL_SRC0ALPHA + uArg)))
	{
		uLiveChans |= 8;
	}
	if ((uLiveChansInDest & 8) != 0 &&
		(psInst->u.psSop3->uASel1 == (USEASM_INTSRCSEL_SRC0ALPHA + uArg) || 
		 psInst->u.psSop3->uASel2 == (USEASM_INTSRCSEL_SRC0ALPHA + uArg)))
	{
		uLiveChans |= 8;
	} 

	return uLiveChans;
}

/*********************************************************************************
 Function			: GetLiveChansInFPMAArgument

 Description		: Get channels used by an instruction source to FPMA.
 
 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
static IMG_UINT32 GetLiveChansInFPMAArgument(const INST *psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest)
{
	IMG_UINT32 uLiveChans;

	uLiveChans = 0;
	if (uLiveChansInDest & 7)
	{
		if (uArg == 0 && psInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0)
		{
			uLiveChans |= uLiveChansInDest;
		}
		if (uArg == 1 && (psInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1 || psInst->u.psFpma->uCSel1 == USEASM_INTSRCSEL_SRC1))
		{
			uLiveChans |= uLiveChansInDest;
		}
		if (uArg == 2 && psInst->u.psFpma->uCSel2 == USEASM_INTSRCSEL_SRC2)
		{
			uLiveChans |= uLiveChansInDest;
		}
		if (uArg == 0 && psInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC0ALPHA)
		{
			uLiveChans |= 8;
		}
		if (uArg == 1 && (psInst->u.psFpma->uCSel0 == USEASM_INTSRCSEL_SRC1ALPHA || psInst->u.psFpma->uCSel1 == USEASM_INTSRCSEL_SRC1ALPHA))
		{
			uLiveChans |= 8;
		}
		if (uArg == 2 && psInst->u.psFpma->uCSel2 == USEASM_INTSRCSEL_SRC2ALPHA)
		{
			uLiveChans |= 8;
		}
	}
	if (uLiveChansInDest & 8)
	{
		if (uArg == 0 && psInst->u.psFpma->uASel0 == USEASM_INTSRCSEL_SRC0ALPHA)
		{
			uLiveChans |= 8;
		}
		if (uArg == 1 && psInst->u.psFpma->uASel0 == USEASM_INTSRCSEL_SRC1ALPHA)
		{
			uLiveChans |= 8;
		}
		if (uArg == 1 || uArg == 2)
		{
			uLiveChans |= 8;
		}
	}

	return uLiveChans;
}

/*********************************************************************************
 Function			: GetLiveChansInSOPWMArgument

 Description		: Get channels used by an instruction source to SOPWM.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
static IMG_UINT32 GetLiveChansInSOPWMArgument(PINTERMEDIATE_STATE psState, const INST *psInst, IMG_UINT32 uArg, IMG_UINT32 uLiveChansInDest)
{
	IMG_UINT32	uLiveChans;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if (psInst->u.psSopWm->bRedChanFromAlpha)
	{
		/*
			In this case the destination mask has only the
			RED channel bit set but from the point of view
			of the internal calculation the data comes
			from the ALPHA channel.
		*/
		ASSERT(psInst->auDestMask[0] == USC_ALL_CHAN_MASK);
		if (uLiveChansInDest & USC_RED_CHAN_MASK)
		{
			uLiveChansInDest = USC_ALPHA_CHAN_MASK;
		}
		else
		{
			uLiveChansInDest = 0;
		}
	}

	uLiveChans = 0;

	/*
		Check for multiplying this argument by something non-zero.
	*/
	if (uArg == 0 && !(psInst->u.psSopWm->uSel1 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSopWm->bComplementSel1))
	{
		uLiveChans |= uLiveChansInDest;
	}
	if (uArg == 1 && !(psInst->u.psSopWm->uSel2 == USEASM_INTSRCSEL_ZERO && !psInst->u.psSopWm->bComplementSel2))
	{
		uLiveChans |= uLiveChansInDest;
	}
	/*
		Check for referencing this argument through one of the
		source selectors.
	*/
	if (psInst->u.psSopWm->uSel1 == (USEASM_INTSRCSEL_SRC1 + uArg) || 
		psInst->u.psSopWm->uSel2 == (USEASM_INTSRCSEL_SRC1 + uArg))
	{
		uLiveChans |= uLiveChansInDest;
	}
	if (psInst->u.psSopWm->uSel1 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg) || 
		psInst->u.psSopWm->uSel2 == (USEASM_INTSRCSEL_SRC1ALPHA + uArg))
	{
		uLiveChans |= 8;
	}

	return uLiveChans;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
/*********************************************************************************
 Function			: ApplySwizzleToMask

 Description		: Given the channels referenced in a source argument with a
					  swizzle get the channels referenced in the source register
					  after the swizzle is applied.

 Parameters			: uSwizzle			- Swizzle.
					  uMask				- Mask.

 Globals Effected	: None

 Return				: The channels referenced by the swizzle.
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 ApplySwizzleToMask(IMG_UINT32			uSwizzle,
							  IMG_UINT32			uInMask)
{
	IMG_UINT32	uChan;
	IMG_UINT32	uOutMask;

	uOutMask = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		IMG_UINT32	uSelChan = 
			(uSwizzle >> (uChan * USEASM_SWIZZLE_FIELD_SIZE)) & USEASM_SWIZZLE_VALUE_MASK;

		if ((uInMask & (1 << uChan)) == 0)
		{
			continue;
		}
		switch (uSelChan)
		{
			case USEASM_SWIZZLE_SEL_X: uOutMask |= USC_X_CHAN_MASK; break;
			case USEASM_SWIZZLE_SEL_Y: uOutMask |= USC_Y_CHAN_MASK; break;
			case USEASM_SWIZZLE_SEL_Z: uOutMask |= USC_Z_CHAN_MASK; break;
			case USEASM_SWIZZLE_SEL_W: uOutMask |= USC_W_CHAN_MASK; break;
		}
	}
	return uOutMask;
}

static
IMG_UINT32 GetLiveChansInVDUALArgument(PINTERMEDIATE_STATE	psState, 
									   const INST			*psInst, 
									   IMG_UINT32			uSrcSlot,
									   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInVDUALArgument

 Description		: Get the channels referenced from a VDUAL instruction source
					  before applying the source swizzle.

 Parameters			: psState			- Compiler state.
					  psInst			- Vector instruction.
					  uLiveChansInDest	- Channels referenced from the instruction's
										destination.

 Globals Effected	: None

 Return				: The channels referenced from the source.
*********************************************************************************/
{
	VDUAL_OP_TYPE	eSrcUsage;
	VDUAL_OP		eOp;
	IMG_UINT32		uLiveChansInDest;
	IMG_BOOL		bUsesGPIDestination;

	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(uSrcSlot < VDUAL_SRCSLOT_COUNT);
	eSrcUsage = psInst->u.psVec->sDual.aeSrcUsage[uSrcSlot];

	/*
		Get the operation (primary or secondary) which uses this
		source argument.
	*/
	if (eSrcUsage == VDUAL_OP_TYPE_PRIMARY)
	{
		if (psInst->u.psVec->sDual.bPriOpIsDummy)
		{
			return 0;
		}
		eOp = psInst->u.psVec->sDual.ePriOp;
		bUsesGPIDestination = psInst->u.psVec->sDual.bPriUsesGPIDest;
	}
	else if (eSrcUsage == VDUAL_OP_TYPE_SECONDARY)
	{
		if (psInst->u.psVec->sDual.bSecOpIsDummy)
		{
			return 0;
		}
		eOp = psInst->u.psVec->sDual.eSecOp;
		bUsesGPIDestination = !psInst->u.psVec->sDual.bPriUsesGPIDest;
	}
	else
	{
		ASSERT(eSrcUsage == VDUAL_OP_TYPE_NONE);
		return 0;
	}

	/*
		Get the channels used from the result of the operation.
	*/
	if (bUsesGPIDestination)
	{
		uLiveChansInDest = auLiveChansInDest[VDUAL_DESTSLOT_GPI];
	}
	else
	{	
		uLiveChansInDest = auLiveChansInDest[VDUAL_DESTSLOT_UNIFIEDSTORE];
	}

	if (uLiveChansInDest == 0)
	{
		return 0;
	}

	/*
		Translate the channels used from the result of the calculation to
		the channels used from the source argument.
	*/
	switch (eOp)
	{
		case VDUAL_OP_DP3:
		case VDUAL_OP_SSQ3:
		{
			return USC_XYZ_CHAN_MASK;
		}

		case VDUAL_OP_DP4:
		case VDUAL_OP_SSQ4:
		{
			return USC_XYZW_CHAN_MASK;
		}

		case VDUAL_OP_RCP:
		case VDUAL_OP_RSQ:
		case VDUAL_OP_LOG:
		case VDUAL_OP_EXP:
		{
			return USC_X_CHAN_MASK;
		}

		default:
		{
			return uLiveChansInDest;
		}
	}
}

static
IMG_UINT32 MapChannelsToExpandedSources(PINTERMEDIATE_STATE psState, 
										PCINST				psInst, 
										IMG_UINT32			uSrcSlot, 
										IMG_UINT32			uLiveChansInResult)
/*********************************************************************************
 Function			: MapChannelsToExpandedSources

 Description		: Remaps the channels used from an instruction result to the
					  channels used from different sources for VPCK instructions
					  where the first two F32 channels are accessed from the
					  first source and the second two from the second source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction to remap for.
					  uSrcSlot			- Source to remap for.
					  uLiveChansInResult
										- Channels used from the instruction result.

 Globals Effected	: None

 Return				: The mask of channels used from the instruction source.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	uLiveChansInResult = ApplySwizzleToMask(psInst->u.psVec->uPackSwizzle, uLiveChansInResult);
	switch (uSrcSlot)
	{
		case 0: return (uLiveChansInResult >> USC_X_CHAN) & USC_XY_CHAN_MASK;
		case 1: return (uLiveChansInResult >> USC_Z_CHAN) & USC_XY_CHAN_MASK;
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_UINT32 GetVTestChanSelMask(PINTERMEDIATE_STATE psState, PCINST psInst)
/*********************************************************************************
 Function			: GetVTestChanSelMask

 Description		: Get the mask of channels used from the ALU result to generate
					  the predicate results.

 Parameters			: psState			- Compiler state.
					  psInst			- VTEST instruction.

 Globals Effected	: None

 Return				: The channels referenced from the ALU result.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	ASSERT(psInst->eOpcode == IVTEST);

	switch (psInst->u.psVec->sTest.eChanSel)
	{
		case VTEST_CHANSEL_XCHAN:
		{
			return USC_X_CHAN_MASK; 
		}
		case VTEST_CHANSEL_PERCHAN: 
		{
			IMG_UINT32 uDest;
			IMG_UINT32 uMask = 0;
			for (uDest = VTEST_PREDICATE_DESTIDX; uDest < VTEST_PREDICATE_DESTIDX+VTEST_PREDICATE_ONLY_DEST_COUNT; ++uDest)
			{
				if (psInst->auLiveChansInDest[uDest] != 0)
				{
					uMask |= (1 << uDest);
				}
			}
			return uMask;
		}
		case VTEST_CHANSEL_ANDALL:
		case VTEST_CHANSEL_ORALL:
		{
			return USC_XYZW_CHAN_MASK; 
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInVectorArgumentPreSwizzle(PINTERMEDIATE_STATE	psState, 
												  PCINST				psInst, 
												  IMG_UINT32			uSrcSlot,
												  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInVectorArgumentPreSwizzle

 Description		: Get the channels referenced from a vector instruction source
					  before applying the source swizzle.

 Parameters			: psState			- Compiler state.
					  psInst			- Vector instruction.
					  uLiveChansInDest	- Channels referenced from the instruction's
										destination.

 Globals Effected	: None

 Return				: The channels referenced from the source.
*********************************************************************************/
{
	IOPCODE		eOpcode;
	IMG_UINT32	uLiveChansInALUResult;

	if (psInst->u.psVec->uDestSelectShift != USC_X_CHAN)
	{
		ASSERT(psInst->eOpcode != IVTEST);
		auLiveChansInDest[0] >>= psInst->u.psVec->uDestSelectShift;
	}

	if (psInst->eOpcode == IVDUAL)
	{
		return GetLiveChansInVDUALArgument(psState, psInst, uSrcSlot, auLiveChansInDest);
	}

	if (psInst->eOpcode == IVTEST)
	{
		/*
			Combine both the channels used from the ALU result to set the predicate destination and the
			channels written directly to the non-predicate destination (if any).
		*/
		uLiveChansInALUResult = 0;
		if (auLiveChansInDest[VTEST_PREDICATE_DESTIDX] != 0)
		{
			uLiveChansInALUResult |= GetVTestChanSelMask(psState, psInst); 
		}
		if (psInst->uDestCount > VTEST_PREDICATE_ONLY_DEST_COUNT)
		{
			uLiveChansInALUResult |= auLiveChansInDest[1];
		}
	}
	else
	{
		uLiveChansInALUResult = auLiveChansInDest[0];
	}

	if (psInst->eOpcode == IVTEST || psInst->eOpcode == IVTESTMASK || psInst->eOpcode == IVTESTBYTEMASK)
	{
		eOpcode = psInst->u.psVec->sTest.eTestOpcode;
	}
	else
	{
		eOpcode = psInst->eOpcode;
	}

	switch (eOpcode)
	{
		case IVMOV:
		case IVMOV_EXP:
		case IVMOVC:
		case IVMOVCBIT:
		case IVMUL:
		case IVMUL3:
		case IVADD:
		case IVADD3:
		case IVMAD:
		case IVMAD4:
		case IVMIN:
		case IVMAX:
		case IVDSX:
		case IVDSY:
		case IVDSX_EXP:
		case IVDSY_EXP:
		case IVFRC:
		case IVTRC:
		case IVPCKU8U8:
		case IVPCKU8FLT:
		case IVPCKS8FLT:
		case IVPCKC10FLT:
		case IVPCKFLTFLT:
		case IVPCKFLTU8:
		case IVPCKFLTS8:
		case IVPCKFLTC10:
		case IVPCKFLTC10_VEC:
		case IUNPCKVEC:
		case IVPCKFLTU16:
		case IVPCKFLTS16:
		case ICVTFLT2ADDR:
		case IVLOAD:
		{
			return uLiveChansInALUResult;
		}
		case IVPCKU8FLT_EXP:
		case IVPCKC10FLT_EXP:
		case IVPCKFLTFLT_EXP:
		{
			return MapChannelsToExpandedSources(psState, psInst, uSrcSlot, uLiveChansInALUResult);
		}
		case IVPCKS16FLT:
		case IVPCKS16FLT_EXP:
		case IVPCKU16FLT:
		case IVPCKU16FLT_EXP:
		{
			IMG_UINT32	uLiveChansInSrc;
			IMG_UINT32	uHalf;

			uLiveChansInSrc = 0;
			for (uHalf = 0; uHalf < 2; uHalf++)
			{
				if (uLiveChansInALUResult & (USC_DESTMASK_LOW << (uHalf * 2)))
				{
					uLiveChansInSrc |= (1 << uHalf);
				}
			}
			if (eOpcode == IVPCKS16FLT_EXP || eOpcode == IVPCKU16FLT_EXP)
			{
				return MapChannelsToExpandedSources(psState, psInst, uSrcSlot, uLiveChansInSrc);
			}
			else
			{
				return uLiveChansInSrc;
			}
		}
		case IVRSQ:
		case IVRCP:
		case IVLOG:
		case IVEXP:
		{
			if (uLiveChansInALUResult != 0)
			{
				return USC_W_CHAN_MASK;
			}
			else
			{
				return 0;
			}
		}
		case IVDP:
		case IVDP_GPI:
		case IVDP_CP:
		{
			if (uLiveChansInALUResult != 0)
			{
				return USC_XYZW_CHAN_MASK;
			}
			else
			{
				return 0;
			}
		}
		case IVDP3:
		case IVDP3_GPI:
		{
			if (uLiveChansInALUResult != 0)
			{
				return USC_XYZ_CHAN_MASK;
			}
			else
			{
				return 0;
			}
		}
		case IVDP2:
		{
			if (uLiveChansInALUResult != 0)
			{
				return USC_XY_CHAN_MASK;
			}
			else
			{
				return 0;
			}
		}
		case IVMOVCU8_FLT:
		{
			if (uSrcSlot == 0)
			{
				return USC_X_CHAN_MASK;
			}
			else
			{
				return uLiveChansInALUResult;
			}
		}
		default: imgabort();
	}
}

static IMG_UINT32 ConcatenateVectorMask(PINTERMEDIATE_STATE	psState,
										const INST			*psInst,
										IMG_UINT32			uVecDestBase,
										IMG_UINT32			uVecDestIdx,
										IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: ConcatenateVectorMask

 Description		: For a vector instruction which writes to a set of scalar
					  registers concatenate together the write masks for all the
					  registers.

 Parameters			: psState			- Compiler state.
					  psInst			- Vector instruction.
					  uVecDestBase		- Start of the vector destinations within the overall
										instruction destinations.
					  uVecDestIdx		- Index of the vector to get the mask for.
					  auLiveChansInDest	- Live channels in each instruction destination.

 Globals Effected	: None

 Return				: The vector mask.
*********************************************************************************/
{
	IMG_UINT32	uLiveChansInDest;

	if (psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS)
	{
		PARG		psBaseDest;
		IMG_UINT32	uBaseDestIdx;
		IMG_UINT32	uDestCount;

		/*
			In this case the information about channels used in the destination(s) is in
			units of bytes or C10 channels and a single vector sized destination is split
			across multiple scalar destination registers.
		*/

		uBaseDestIdx = uVecDestBase + uVecDestIdx * VECTOR_LENGTH;
		psBaseDest = &psInst->asDest[uBaseDestIdx];
		
		ASSERT(psInst->uDestCount > uBaseDestIdx);
		
		uDestCount = psInst->uDestCount - uBaseDestIdx;

		uLiveChansInDest = 0;
		if (psBaseDest->eFmt == UF_REGFORMAT_F32)
		{
			IMG_UINT32	uChanIdx;

			/*
				Concatenate up to four registers. Each represents one
				channel of the vector destination.
			*/
			for (uChanIdx = 0; uChanIdx < min(uDestCount, SCALAR_REGISTERS_PER_F32_VECTOR); uChanIdx++)
			{
				if (auLiveChansInDest[uBaseDestIdx + uChanIdx])
				{
					uLiveChansInDest |= (1 << uChanIdx);
				}
			}
		}
		else if (psBaseDest->eFmt == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uRegIdx;

			/*
				Concatenate up to two registers. Each represents two
				channels of the vector destination.
			*/
			for (uRegIdx = 0; uRegIdx < min(uDestCount, SCALAR_REGISTERS_PER_F16_VECTOR); uRegIdx++)
			{
				if ((auLiveChansInDest[uBaseDestIdx + uRegIdx] & USC_XY_CHAN_MASK) != 0)
				{
					uLiveChansInDest |= (1 << ((uRegIdx * F16_CHANNELS_PER_SCALAR_REGISTER) + 0));
				}
				if ((auLiveChansInDest[uBaseDestIdx + uRegIdx] & USC_ZW_CHAN_MASK) != 0)
				{
					uLiveChansInDest |= (1 << ((uRegIdx * F16_CHANNELS_PER_SCALAR_REGISTER) + 1));
				}
			}
		}
		else
		{
			ASSERT(psBaseDest->eFmt == UF_REGFORMAT_C10);

			/*
				The destination can be represents either by one scalar register (holding 30 bits of C10 data)
				or two scalar registers (holding 40 bits of C10 data). In the latter case the ALPHA channel of
				the first destination and the RED channel of the second destination correspond to different
				parts of the last channel of C10 data so they must be either both used or both unused.
			*/

			uLiveChansInDest = auLiveChansInDest[uBaseDestIdx + 0];

			if (uDestCount >= 2)
			{
				IMG_UINT32	uLiveChansInLowDest = auLiveChansInDest[uBaseDestIdx + 0];
				IMG_UINT32	uLiveChansInHighDest = auLiveChansInDest[uBaseDestIdx + 1];

				ASSERT(( (uLiveChansInLowDest & USC_ALPHA_CHAN_MASK) &&  (uLiveChansInHighDest & USC_RED_CHAN_MASK)) || 
					   (!(uLiveChansInLowDest & USC_ALPHA_CHAN_MASK) && !(uLiveChansInHighDest & USC_RED_CHAN_MASK)));
			}
		}
	}
	else
	{
		/*
			Simple case: the destination uses channel sized units.
		*/

		ASSERT((uVecDestBase + uVecDestIdx) < psInst->uDestCount);
		uLiveChansInDest = auLiveChansInDest[uVecDestBase + uVecDestIdx];
	}

	return uLiveChansInDest;
}

/*********************************************************************************
 Function			: GetLiveChansInVectorResult

 Description		: Get channels used from the result of a vector instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  auLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID GetLiveChansInVectorResult(PINTERMEDIATE_STATE	psState, 
									const INST			*psInst, 
									IMG_UINT32			auLiveChansInDestNonVec[],
									IMG_UINT32			auLiveChansInDestVec[])
{
	if (psInst->eOpcode == IUNPCKVEC || psInst->eOpcode == IVTESTBYTEMASK)
	{
		/*
			This instruction has a scalar destination and a vector source so the input
			live channels mask is in units of bytes. If any bytes are used then the X
			channel is used from the vector source.
		*/

		if (psInst->eOpcode == IVTESTBYTEMASK)
		{
			ASSERT(psInst->uDestCount == 2);
			if ((psState->uFlags2 & USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS) == 0)
			{
				ASSERT(auLiveChansInDestNonVec[1] == 0);
			}
			auLiveChansInDestVec[0] = auLiveChansInDestNonVec[0];
		}
		else 
		{
			ASSERT(psInst->uDestCount == 1);
			ASSERT(psInst->eOpcode == IUNPCKVEC);

			if (psInst->asDest[0].eFmt == UF_REGFORMAT_F32)
			{
				if (auLiveChansInDestNonVec[0] != 0)
				{
					auLiveChansInDestVec[0] = USC_X_CHAN_MASK;
				}
				else
				{
					auLiveChansInDestVec[0] = 0;
				}
			}
			else
			{
				ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_F16);
	
				auLiveChansInDestVec[0] = 0;
				if ((auLiveChansInDestNonVec[0] & USC_XY_CHAN_MASK) != 0)
				{
					auLiveChansInDestVec[0] |= USC_X_CHAN_MASK;
				}
				if ((auLiveChansInDestNonVec[0] & USC_ZW_CHAN_MASK) != 0)
				{
					auLiveChansInDestVec[0] |= USC_Y_CHAN_MASK;
				}
			}
		}
	}
	else if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
	{
		if (psInst->eOpcode == IVTEST)
		{
			/*
				Copy the channels used from the predicate destination.
			*/
			auLiveChansInDestVec[0] = auLiveChansInDestNonVec[0];
			auLiveChansInDestVec[1] = auLiveChansInDestNonVec[1];
			auLiveChansInDestVec[2] = auLiveChansInDestNonVec[2];
			auLiveChansInDestVec[3] = auLiveChansInDestNonVec[3];
			/*
			Add in any channels written to a non-predicate destination.
			*/
			if (psInst->uDestCount > VTEST_PREDICATE_ONLY_DEST_COUNT)
			{
				auLiveChansInDestVec[4] = 
					ConcatenateVectorMask(psState, 
					psInst, 
					VTEST_UNIFIEDSTORE_DESTIDX, 
					0 /* uVecDestIdx */, 
					auLiveChansInDestNonVec);
			}
		}
		else
		{
			IMG_UINT32	uVecDestIdx;
			IMG_UINT32	uNumVectorDests;

			if (psInst->eOpcode == IVDUAL)
			{
				uNumVectorDests = VDUAL_DESTSLOT_COUNT;
			}
			else
			{
				uNumVectorDests = 1;
			}

			for (uVecDestIdx = 0; uVecDestIdx < uNumVectorDests; uVecDestIdx++)
			{
				auLiveChansInDestVec[uVecDestIdx] = 
					ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, uVecDestIdx, auLiveChansInDestNonVec);
			}
		}
	}
	else if (
				(psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0 &&
				(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORDEST) != 0
			)
	{
		auLiveChansInDestVec[0] = auLiveChansInDestNonVec[0];
		if (psInst->uDestCount > 1)
		{
			/*
				The ALPHA channel in the first destination and the RED channel in the second
				destination in fact refer to different parts of the same channel so they
				should always be live together.
			*/
			ASSERT((!(auLiveChansInDestNonVec[0] & USC_ALPHA_CHAN_MASK) && !(auLiveChansInDestNonVec[1] & USC_RED_CHAN_MASK)) || 
				   ( (auLiveChansInDestNonVec[0] & USC_ALPHA_CHAN_MASK) &&  (auLiveChansInDestNonVec[1] & USC_RED_CHAN_MASK)));
		}
	}			
	else
	{
		ASSERT(psInst->uDestCount == 1);
		auLiveChansInDestVec[0] = auLiveChansInDestNonVec[0];
	}
}

static IMG_UINT32 GetPostSwizzleVectorLiveChans(PINTERMEDIATE_STATE	psState,
												const INST			*psInst,
												IMG_UINT32			uArg,
												IMG_UINT32			uLiveChansInArg,
												IMG_UINT32			auLiveChansInDestVec[])
/*********************************************************************************
 Function			: GetPostSwizzleVectorLiveChans

 Description		: Get the mask of channels used from a vector instruction source:
				      applying the source swizzle and considering scalarized arguments.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Index of the instruction argument.
					  uLiveChanInArg	- Mask of vector channels used from the input
										to the ALU function computed by the instruction.
					  auLiveChansInDestVec
										- Mask of channels from the instruction's
										destinations.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uPostSwizzleLiveChans;

	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_C10VECTORSRCS)
	{
		uPostSwizzleLiveChans = ApplySwizzleToMask(psInst->u.psVec->auSwizzle[0], uLiveChansInArg);

		if (uArg == 0)
		{
			return uPostSwizzleLiveChans;
		}
		else
		{
			ASSERT(uArg == 1);
			if (psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS)
			{
				return (uPostSwizzleLiveChans & USC_ALPHA_CHAN_MASK) ? USC_RED_CHAN_MASK : 0;
			}
			else
			{
				return 0;
			}
		}
	}
	else
	{
		IMG_UINT32		uVecSrcSlot;
		UF_REGFORMAT	eSrcSlotFmt;

		uVecSrcSlot = uArg / SOURCE_ARGUMENTS_PER_VECTOR;
		eSrcSlotFmt = psInst->u.psVec->aeSrcFmt[uVecSrcSlot];
			
		/*
			For vector arguments get the channels referenced after the swizzle.
		*/	
		uPostSwizzleLiveChans = ApplySwizzleToMask(psInst->u.psVec->auSwizzle[uVecSrcSlot], uLiveChansInArg);

		/*
			Map X -> Z and Y -> W.
		*/
		if (GetBit(psInst->u.psVec->auSelectUpperHalf, uVecSrcSlot))
		{
			ASSERT((uPostSwizzleLiveChans & USC_ZW_CHAN_MASK) == 0);
			uPostSwizzleLiveChans <<= USC_Z_CHAN;
		}

		if ((uArg % SOURCE_ARGUMENTS_PER_VECTOR) == 0)
		{
			/*
				Return a mask of vector channels.
			*/
			return uPostSwizzleLiveChans;
		}
		else
		{
			IMG_UINT32	uRegIdx;

			uRegIdx = (uArg % SOURCE_ARGUMENTS_PER_VECTOR) - 1;

			/*
				Special case for VMOVCU8_FLT which uses a mask of bytes (not F16/F32 channels)
				from its first source.
			*/
			if (psInst->eOpcode == IVMOVCU8_FLT && uVecSrcSlot == 0)
			{
				IMG_UINT32	uSingleChan;

				ASSERT(g_abSingleBitSet[uPostSwizzleLiveChans]);
				uSingleChan = (IMG_UINT32)g_aiSingleComponent[uPostSwizzleLiveChans];
				if (uRegIdx == uSingleChan)
				{
					return auLiveChansInDestVec[0];
				}
				else
				{
					return 0;
				}
			}
	
			/*
				Return a mask of byte channels.
			*/
			return ChanMaskToByteMask(psState, uPostSwizzleLiveChans, uRegIdx, eSrcSlotFmt);
		}
	}
}

/*********************************************************************************
 Function			: BaseGetLiveChansInVectorArgument

 Description		: Get channels used by a source to a vector instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.
					  puPreSwizzleLiveChanMask
										- Returns the mask of channels used before
										applying the swizzle.
					  puPostSwizzleLiveChanMask
										- Returns the mask of channels used after
										applying the swizzle.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID BaseGetLiveChansInVectorArgument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDestNonVec[],
										  IMG_PUINT32			puPreSwizzleLiveChanMask,
										  IMG_PUINT32			puPostSwizzleLiveChanMask)
{
	IMG_UINT32		uPreSwizzleLiveChanMask;
	IMG_UINT32		auLiveChansInDestVec[VECTOR_MAX_DESTINATION_COUNT];

	/*
		Get the channels live in the destination considered as a vector.
	*/
	GetLiveChansInVectorResult(psState, psInst, auLiveChansInDestNonVec, auLiveChansInDestVec);
	
	/*
		Get the live channels in each source argument based on the channels live
		in the destination.
	*/
	uPreSwizzleLiveChanMask = 
		GetLiveChansInVectorArgumentPreSwizzle(psState, psInst, uArg / SOURCE_ARGUMENTS_PER_VECTOR, auLiveChansInDestVec);
	if (puPreSwizzleLiveChanMask != NULL)
	{
		*puPreSwizzleLiveChanMask = uPreSwizzleLiveChanMask;
	}

	/*
		Apply the source swizzle and, if required, convert from a mask of vector channels to a mask of bytes.
	*/
	if (puPostSwizzleLiveChanMask != NULL)
	{
		*puPostSwizzleLiveChanMask = 
			GetPostSwizzleVectorLiveChans(psState, psInst, uArg, uPreSwizzleLiveChanMask, auLiveChansInDestVec);
	}
}

/*********************************************************************************
 Function			: GetLiveChansInVectorArgument

 Description		: Get channels used by a source to a vector instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 GetLiveChansInVectorArgument(PINTERMEDIATE_STATE	psState, 
									    PCINST				psInst, 
										IMG_UINT32			uArg, 
										IMG_UINT32			auLiveChansInDestNonVec[])
{
	IMG_UINT32 uPostSwizzleLiveChanMask;
	BaseGetLiveChansInVectorArgument(psState, 
									 psInst, 
									 uArg, 
									 auLiveChansInDestNonVec, 
									 NULL /* puPreSwizzleLiveChanMask */,
									 &uPostSwizzleLiveChanMask);
	return uPostSwizzleLiveChanMask;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID GetLiveChansInDest(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
							IMG_UINT32			auLiveChansInDest[])
{
	IMG_UINT32	uDestIdx;

	ASSERT(psInst->uDestCount <= USC_MAX_NONCALL_DEST_COUNT);
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		auLiveChansInDest[uDestIdx] = 
			psInst->auLiveChansInDest[uDestIdx] & GetDestMaskIdx(psInst, uDestIdx);
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_VOID GetLiveChansInSourceSlot(PINTERMEDIATE_STATE	psState, 
								  PINST					psInst, 
								  IMG_UINT32			uSlotIdx,
								  IMG_PUINT32			puChansUsedPreSwizzle,
								  IMG_PUINT32			puChansUsedPostSwizzle)
/*********************************************************************************
 Function			: GetLiveChansInSourceSlot

 Description		: Get the channels used from a source slot to a vector instruction
					  (which might be either a single vector register or a vector of
					  scalar registers).

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uSlotIdx			- Index of the source slot.
					  puChansUsedPreSwizzle
										- Returns the mask of channels used from the
										source slot before the swizzle is applied.
					  puChansUsedPostSwizzle
										- Returns the mask of channels used from the
										source slot after the swizzle is applied.
								
	
 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		auLiveChansInDestNonVec[USC_MAX_NONCALL_DEST_COUNT];

	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(psInst->uDestCount <= USC_MAX_NONCALL_DEST_COUNT);
	GetLiveChansInDest(psState, psInst, auLiveChansInDestNonVec);
	BaseGetLiveChansInVectorArgument(psState, 
									 psInst, 
									 uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR, 
									 auLiveChansInDestNonVec, 
									 puChansUsedPreSwizzle,
									 puChansUsedPostSwizzle);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_UINT32 GetLiveChansInIMA8Arg(PINTERMEDIATE_STATE psState, 
								 PCINST psInst, 
								 IMG_UINT32 uArg, 
								 IMG_UINT32 auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInC10Arg

 Description		: Get channels used from a source to an IMA8 instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	ASSERT(psInst->eOpcode == IIMA8);
	ASSERT(psInst->uDestCount == 1);
	return GetLiveChansInFPMAArgument(psInst, uArg, auLiveChansInDest[0]);
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInC10Arg(PINTERMEDIATE_STATE psState, 
								const INST *psInst, 
								IMG_UINT32 uArg, 
								IMG_UINT32 auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInC10Arg

 Description		: Get channels used from a source to a C10 instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uLiveChansInArg;
	PARG		psArg = &psInst->asArg[uArg];

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && 
			(psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0
	   )
	{
		IMG_UINT32	uLiveChansInDest;

		if (psInst->eOpcode == IFPTESTPRED_VEC)
		{
			ASSERT(psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT);

			if (auLiveChansInDest[0] != 0)
			{
				uLiveChansInDest = ChanSelToMask(psState, &psInst->u.psTest->sTest);
			}
			else
			{
				uLiveChansInDest = 0;
			}
		}
		else if (psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
		{
			ASSERT(psInst->uDestCount >= 1);
			uLiveChansInDest = auLiveChansInDest[0];

			if (psInst->uDestCount > 1)
			{
				ASSERT(psInst->asDest[1].eFmt == UF_REGFORMAT_C10);
				if ((psState->uFlags2 & USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS) == 0)
				{
					ASSERT((auLiveChansInDest[1] & ~USC_RED_CHAN_MASK) == 0);
					ASSERT((!(auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) && !(auLiveChansInDest[1] & USC_RED_CHAN_MASK)) || 
						   ( (auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) &&  (auLiveChansInDest[1] & USC_RED_CHAN_MASK)));
				}

				ASSERT(psInst->uDestCount == 2);

				if (auLiveChansInDest[1] & USC_RED_CHAN_MASK)
				{
					uLiveChansInDest |= USC_ALPHA_CHAN_MASK;
				}
			}
		}
		else
		{
			ASSERT(psInst->uDestCount == 1);

			uLiveChansInDest = auLiveChansInDest[0];
		}

		switch (psInst->eOpcode)
		{
			case ISOPWM_VEC:
			{
				uLiveChansInArg = GetLiveChansInSOPWMArgument(psState, psInst, uArg / 2, uLiveChansInDest);
				break;
			}
			case ISOP2:
			case ISOP2_VEC:
			{
				uLiveChansInArg = GetLiveChansInSOP2Argument(psInst, uArg / 2, uLiveChansInDest);
				break;
			}
			case ISOP3_VEC:
			{
				uLiveChansInArg = GetLiveChansInSOP3Argument(psInst, uArg / 2, uLiveChansInDest);
				break;
			}
			case ILRP1_VEC:
			{
				uLiveChansInArg = GetLiveChansInLRP1Argument(psState, psInst, uArg / 2, uLiveChansInDest);
				break;
			}
			case IFPMA_VEC:
			{
				uLiveChansInArg = GetLiveChansInFPMAArgument(psInst, uArg / 2, uLiveChansInDest);
				break;
			}
			case IFPDOT_VEC:
			{
				uLiveChansInArg = (1 << psInst->u.psDot34->uVecLength) - 1;
				break;
			}
			case IVPCKC10C10:
			case IVMOVC10:
			case IVMOVCC10:
			case IVMOVCU8:
			{
				uLiveChansInArg = ApplySwizzleToMask(psInst->u.psVec->auSwizzle[0], uLiveChansInDest);
				break;
			}
			case IFPSUB8_VEC:
			case IFPADD8_VEC:
			case IFPTESTPRED_VEC:
			case IFPTESTMASK_VEC:
			{
				uLiveChansInArg = uLiveChansInDest;
				break;
			}
			default: imgabort();
		}

		if ((uArg % 2) == 1)
		{
			if (psArg->eFmt == UF_REGFORMAT_U8 || psArg->uType == USC_REGTYPE_UNUSEDSOURCE)
			{
				return 0;
			}
			else
			{
				ASSERT(psArg->eFmt == UF_REGFORMAT_C10);
				if (uLiveChansInArg & USC_ALPHA_CHAN_MASK)
				{
					return USC_RED_CHAN_MASK;
				}
				else
				{
					return 0;
				}
			}
		}
		else
		{
			return uLiveChansInArg;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{	
		switch (psInst->eOpcode)
		{
			case ISOPWM:
			{
				uLiveChansInArg = GetLiveChansInSOPWMArgument(psState, psInst, uArg, auLiveChansInDest[0]);
				break;
			}
			case ISOP2:
			{
				uLiveChansInArg = GetLiveChansInSOP2Argument(psInst, uArg, auLiveChansInDest[0]);
				break;
			}
			case ISOP3:
			{
				uLiveChansInArg = GetLiveChansInSOP3Argument(psInst, uArg, auLiveChansInDest[0]);
				break;
			}
			case ILRP1:
			{
				uLiveChansInArg = GetLiveChansInLRP1Argument(psState, psInst, uArg, auLiveChansInDest[0]);
				break;
			}
			case IFPMA:
			{
				uLiveChansInArg = GetLiveChansInFPMAArgument(psInst, uArg, auLiveChansInDest[0]);
				break;
			}
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			case IVPCKC10C10:
			{	
				uLiveChansInArg = ApplySwizzleToMask(psInst->u.psVec->auSwizzle[0], auLiveChansInDest[0]);	
				break;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			default: imgabort();
		}

		uLiveChansInArg = RemapC10Channels(psState, psArg, uLiveChansInArg);

		return uLiveChansInArg;
	}
}

static
IMG_UINT32 FormatToChannelMask(PINTERMEDIATE_STATE psState, const ARG *psArg, IMG_UINT32 uComponent)
/*********************************************************************************
 Function			: FormatToChannelMask

 Description		: Get the mask of bytes/C10 components referenced by a channel
					  selected from a source argument.

 Parameters			: psState			- Compiler state.
					  psArg				- Source argument.
					  uComponent		- Channel selected.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psArg->eFmt == UF_REGFORMAT_F32)
	{
		return USC_XYZW_CHAN_MASK;
	}
	else if (psArg->eFmt == UF_REGFORMAT_C10)
	{
		return USC_X_CHAN_MASK << uComponent;
	}
	else
	{
		ASSERT(psArg->eFmt == UF_REGFORMAT_F16);
		return USC_XY_CHAN_MASK << uComponent;
	}
}

#if defined(SUPPORT_SGX545)
static
IMG_UINT32 GetChansUsedByDUALPrimary(PINTERMEDIATE_STATE	psState,
									 const INST				*psInst,
									 IMG_UINT32				uLiveChansInPriDest,
									 IMG_UINT32				uArg,
									 const ARG				*psArg)
/*********************************************************************************
 Function			: GetChansUsedByDUALPrimary

 Description		: Get channels used from a source by a dual-issue instruction's
					  primary operation.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uLiveChansInSecDest
										- Live channels in the secondary destination.
					  psArg				- Argument.
					  uArg				- Argument number.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	if (psDual->ePrimaryOp == IMOV)
	{
		if (uArg == 1)
		{
			return uLiveChansInPriDest;
		}
		else
		{
			return 0;
		}
	}
	else if (psDual->ePrimaryOp == IFSSQ)
	{
		if (uArg == 1 || uArg == 2)
		{
			return uLiveChansInPriDest;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if (uArg < DUAL_MAX_PRI_SOURCES)
		{
			return FormatToChannelMask(psState, psArg, psDual->auSourceComponent[uArg]);
		}
		else
		{
			return 0;
		}
	}
}

static
IMG_UINT32 GetChansUsedByDUALSecondary(PINTERMEDIATE_STATE	psState,
									   const INST			*psInst,
									   IMG_UINT32			uLiveChansInSecDest,
									   IMG_UINT32			uArg,
									   const ARG			*psArg)
/*********************************************************************************
 Function			: GetChansUsedByDUALSecondary

 Description		: Get channels used from a source by a dual-issue instruction's
					  secondary operation.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uLiveChansInSecDest
										- Live channels in the secondary destination.
					  psArg				- Argument.
					  uArg				- Argument number.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32		uChansUsed;
	PDUAL_PARAMS	psDual = psInst->u.psDual;

	if ((psDual->uSecSourcesUsed & (1 << uArg)) == 0)
	{
		return 0;
	}

	switch (psDual->eSecondaryOp)
	{
		case IMOV: 
		{
			return uLiveChansInSecDest; 
		}
		case IFMAD16:
		{
			uChansUsed = 0;
			if ((uLiveChansInSecDest & USC_XY_CHAN_MASK) != 0)
			{
				uChansUsed |= USC_XY_CHAN_MASK;
			}
			if ((uLiveChansInSecDest & USC_ZW_CHAN_MASK) != 0)
			{
				uChansUsed |= USC_ZW_CHAN_MASK;
			}
			return uChansUsed;
		}
		default:
		{
			if (uArg < DUAL_MAX_PRI_SOURCES)
			{
				return FormatToChannelMask(psState, psArg, psDual->auSourceComponent[uArg]);
			}
			else
			{
				ASSERT(psArg->eFmt == UF_REGFORMAT_F32);
				return USC_XYZW_CHAN_MASK;
			}
		}
	}
}


IMG_INTERNAL
IMG_UINT32 GetLiveChansInDUALArgument(PINTERMEDIATE_STATE	psState,
									  PCINST				psInst,
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInDUALArgument

 Description		: Get channels used by a dual-issue instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uLiveChans;
	PARG		psArg = &psInst->asArg[uArg];

	uLiveChans = GetChansUsedByDUALPrimary(psState, psInst, auLiveChansInDest[DUAL_PRIMARY_DESTINDEX], uArg, psArg);
	uLiveChans |= GetChansUsedByDUALSecondary(psState, psInst, auLiveChansInDest[DUAL_SECONDARY_DESTINDEX], uArg, psArg);

	return uLiveChans;
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_UINT32 GetTestALUChansUsed(PINTERMEDIATE_STATE psState, const INST *psInst)
/*********************************************************************************
 Function			: GetTestALUChansUsed

 Description		: Get the mask of bytes/C10 channels used from the ALU result
				      by a TEST instruction.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_TEST);

	switch (psInst->u.psTest->eAluOpcode)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IFPADD8_VEC:
		case IFPSUB8_VEC:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		case IFPADD8:
		case IFPSUB8:
		{
			return ChanSelToMask(psState, &psInst->u.psTest->sTest);	
		}
		case ISHL:
		{
			if (psInst->u.psTest->sTest.eType == TEST_TYPE_SIGN_BIT_CLEAR || 
				psInst->u.psTest->sTest.eType == TEST_TYPE_SIGN_BIT_SET)
			{
				/*
					For a SIGN only test only the bit 31 is used.
				*/
				return USC_W_CHAN_MASK;
			}
			else
			{
				return USC_ALL_CHAN_MASK;
			}
		}
		default:
		{
			return USC_DESTMASK_FULL;
		}
	}
}

static
IMG_UINT32 GetLiveChansInCALLArg(PINTERMEDIATE_STATE psState, const INST *psInst, IMG_UINT32 uArg)
/*********************************************************************************
 Function			: GetLiveChansInCALLArg

 Description		: Get channels used by a CALL instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PFUNC	psTarget = psInst->u.psCall->psTarget;

	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(uArg < psTarget->sIn.uCount);
	return psTarget->sIn.asArray[uArg].uChanMask;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInTESTArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInTESTArgument

 Description		: Get channels used by a TESTMASK/TESTPRED instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32		uALUChansUsed;
	PTEST_PARAMS	psTest = psInst->u.psTest;

	if (psInst->eOpcode == ITESTPRED)
	{
		uALUChansUsed = 0;

		/*	
			Get the mask of channels used from the ALU result to set the
			destination predicate.
		*/
		ASSERT(psInst->uDestCount > TEST_PREDICATE_DESTIDX);
		if (auLiveChansInDest[TEST_PREDICATE_DESTIDX] != 0)
		{
			uALUChansUsed |= GetTestALUChansUsed(psState, psInst);
		}

		if (GetBit(psInst->auFlag, INST_ALPHA))
		{
			ASSERT(psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT);
			ASSERT(uALUChansUsed == USC_X_CHAN_MASK);
			uALUChansUsed = USC_W_CHAN_MASK;
		}

		/*
			Add in any channels from the ALU result written to a non-predicate
			destination.
		*/
		if (psInst->uDestCount > TEST_UNIFIEDSTORE_DESTIDX)
		{
			uALUChansUsed |= auLiveChansInDest[TEST_UNIFIEDSTORE_DESTIDX];
		}
	}
	else
	{
		ASSERT(psInst->uDestCount == 1);
		if (psInst->u.psTest->sTest.eMaskType == USEASM_TEST_MASK_BYTE)
		{
			uALUChansUsed = auLiveChansInDest[0];
		}
		else
		{
			uALUChansUsed = 0;
			if (auLiveChansInDest[0] != 0)
			{
				uALUChansUsed = GetTestALUChansUsed(psState, psInst);
			}
		}
	}

	if (psTest->eAluOpcode == IFPADD8 || psTest->eAluOpcode == IFPSUB8)
	{
		/*
			These ALU opcodes calculate a result for each 8-bit/10-bit channel in the destination
			from the corresponding channel in each of the sources.
		*/
		return uALUChansUsed;
	}
	else if (psTest->eAluOpcode == ISHL)
	{
		if (
				uArg == BITWISE_SHIFTEND_SOURCE && 
				psInst->asArg[BITWISE_SHIFT_SOURCE].uType == USEASM_REGTYPE_IMMEDIATE
			)
		{
			IMG_UINT32	uShiftInBytes;

			uShiftInBytes = psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber / 8;
			return uALUChansUsed >> uShiftInBytes;
		}
		else
		{
			return USC_ALL_CHAN_MASK;
		}
	}
	else if ((g_psInstDesc[psInst->u.psTest->eAluOpcode].uFlags & DESC_FLAGS_F16F32SELECT) != 0)
	{
		ASSERT(uArg < TEST_ALUOPCODE_MAXIMUM_ARGUMENT_COUNT);
		return FormatToChannelMask(psState, &psInst->asArg[uArg], psTest->auSrcComponent[uArg]);
	}
	else
	{
		return USC_ALL_CHAN_MASK;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInCOMBC10Argument(PINTERMEDIATE_STATE	psState, 
										 PCINST					psInst, 
										 IMG_UINT32				uArg, 
										 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInCOMBC10Argument

 Description		: Get channels used by a COMBC10 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	ASSERT(psInst->eOpcode == ICOMBC10 || psInst->eOpcode == ICOMBC10_ALPHA);

	/* This packs 3 channels of C10 data from Arg0, and a 4th from either channel 0 or channel 3 of Arg1 */
	if (uArg == COMBC10_RGB_SOURCE)
	{
		return auLiveChansInDest[0] & USC_RGB_CHAN_MASK;
	}
	else
	{
		ASSERT(uArg == COMBC10_ALPHA_SOURCE);
		if ((auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) == 0)
		{
			return 0;
		}
		if (psInst->eOpcode == ICOMBC10)
		{
			return USC_RED_CHAN_MASK;
		}
		else
		{
			ASSERT(psInst->eOpcode == ICOMBC10_ALPHA);
			return USC_ALPHA_CHAN_MASK;
		}
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInMOVArgument(PINTERMEDIATE_STATE		psState,
									 PCINST						psInst,
									 IMG_UINT32					uArg,
									 IMG_UINT32					auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInMOVArgument

 Description		: Get channels used by a MOV/DELTA instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	return RemapC10Channels(psState, &psInst->asArg[uArg], auLiveChansInDest[0]);
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInMOVC_I32Argument(PINTERMEDIATE_STATE	psState,
										  PCINST				psInst,
										  IMG_UINT32			uArg,
										  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInMOVC_I32Argument

 Description		: Get channels used by a MOVC_I32 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	ASSERT(psInst->eOpcode == IMOVC_I32);
	if (uArg == 0)
	{
		return USC_ALL_CHAN_MASK;
	}
	else
	{
		return auLiveChansInDest[0];
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInFPDOTArgument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFPDOTArgument

 Description		: Get channels used by a FPDOT/FP16DOT instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uArg);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	ASSERT(psInst->eOpcode == IFPDOT || psInst->eOpcode == IFP16DOT);

	return (1 << psInst->u.psDot34->uVecLength) - 1;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInFARITH16Argument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFARITH16Argument

 Description		: Get channels used by a IFMOV16/IFMUL16/IFADD16/IFMAD16 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PFARITH16_PARAMS	psParams = psInst->u.psArith16;
	PVR_UNREFERENCED_PARAMETER(psState);

	ASSERT(uArg < (sizeof(psParams->aeSwizzle) / sizeof(psParams->aeSwizzle[0])));
	switch (psParams->aeSwizzle[uArg])
	{
		case FMAD16_SWIZZLE_LOWHIGH:
		{
			return auLiveChansInDest[0];
		}
		case FMAD16_SWIZZLE_LOWLOW:
		{
			return USC_XY_CHAN_MASK;
		}
		case FMAD16_SWIZZLE_HIGHHIGH:
		{
			return USC_ZW_CHAN_MASK;
		}
		case FMAD16_SWIZZLE_CVTFROMF32:
		{
			return USC_DESTMASK_FULL;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInEFOArgument(PINTERMEDIATE_STATE psState, PCINST psInst, IMG_UINT32 uArg, IMG_UINT32 auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInEFOArgument

 Description		: Get channels used by a EFO instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	if (uArg < EFO_HARDWARE_SOURCE_COUNT)
	{
		IMG_UINT32	uComponent;

		uComponent = psInst->u.psEfo->sFloat.asSrcMod[uArg].uComponent;
		return FormatToChannelMask(psState, &psInst->asArg[uArg], uComponent);
	}
	else
	{
		ASSERT(uArg == EFO_I0_SRC || uArg == EFO_I1_SRC);
		return USC_ALL_CHAN_MASK;
	}
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
IMG_INTERNAL
IMG_UINT32 GetLiveChansInFNRMArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFNRMArgument

 Description		: Get channels used by a FNRM instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uComponent;

	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	uComponent = psInst->u.psNrm->auComponent[uArg];
	return FormatToChannelMask(psState, &psInst->asArg[uArg], uComponent);
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInFNRM16Argument(PINTERMEDIATE_STATE psState, 
										PCINST				psInst, 
										IMG_UINT32			uArg, 
										IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFNRM16Argument

 Description		: Get channels used by a FNRM16 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uSwizzle;
	IMG_UINT32	uChan;
	IMG_UINT32	uLiveChansInArg;

	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	ASSERT(psInst->asArg[2].uType == USEASM_REGTYPE_SWIZZLE);
	uSwizzle = psInst->asArg[2].uNumber;

	uLiveChansInArg = 0;
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		IMG_UINT32	uSel;

		/*
			Extract the swizzle selector for this channel.
		*/
		uSel = 
			(uSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * uChan)) & USEASM_SWIZZLE_VALUE_MASK;
		/*
			If the selector refers to this argument then increase the mask of
			live channels.
		*/
		if (uSel >= USEASM_SWIZZLE_SEL_X && uSel <= USEASM_SWIZZLE_SEL_W)
		{
			IMG_UINT32	uArgSel = uSel / 2;
					
			if (uArgSel == uArg)
			{
				uLiveChansInArg |= (USC_DESTMASK_LOW << ((uSel % 2) * 2));
			}
		}
	}

	return uLiveChansInArg;
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

IMG_INTERNAL
IMG_UINT32 GetLiveChansInIMAEArgument(PINTERMEDIATE_STATE	psState, 
									  PCINST				psInst, 
									  IMG_UINT32			uArg, 
									  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInIMAEArgument

 Description		: Get channels used by a IMAE instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	if (auLiveChansInDest[0] != 0 || auLiveChansInDest[1] != 0)
	{
		/*
			IMAE does 16x16+32+Carry or 16x16+16+Carry.
		*/
		if (uArg == IMAE_CARRYIN_SOURCE)
		{
			/* One byte (actually only one bit) used from the carry-in source. */
			return USC_XYZW_CHAN_MASK;
		}
		else if (uArg == 2 && psInst->u.psImae->uSrc2Type == USEASM_INTSRCSEL_U32)
		{
			/*
				If source 2 is interpreted as 32-bit then all channels are used.
			*/
			return USC_XYZW_CHAN_MASK;
		}
		else
		{
			/*
				Either the first or the second 16-bits is used from this source.
			*/
			ASSERT(uArg <= IMAE_UNIFIED_STORE_SOURCE_COUNT);
			return USC_XY_CHAN_MASK << psInst->u.psImae->auSrcComponent[uArg];
		}
	}
	return 0;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInIMA16Argument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInIMA16Argument

 Description		: Get channels used by a IMA16 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uLiveChansInArg;

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);

	uLiveChansInArg = 0;
	if ((auLiveChansInDest[0] & USC_XY_CHAN_MASK) != 0)
	{
		uLiveChansInArg |= USC_XY_CHAN_MASK;
	}
	if ((auLiveChansInDest[0] & USC_ZW_CHAN_MASK) != 0)
	{
		uLiveChansInArg |= USC_ZW_CHAN_MASK;
	}
	return uLiveChansInArg;
}

#if defined(OUTPUT_USPBIN)
IMG_INTERNAL
IMG_UINT32 GetLiveChansInSMPUSPNDRArgument(PINTERMEDIATE_STATE	psState, 
										   PCINST				psInst, 
										   IMG_UINT32			uArg, 
										   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSMPUSPNDRArgument

 Description		: Get channels used by a SMP_USP_NDR instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	/*
		Non-dependent samples have a single source, referencing the
		first chunk of pre-sampled data in the PA registers.
	*/
	return ((uArg >= NON_DEP_SMP_TFDATA_ARG_START && uArg <= (NON_DEP_SMP_TFDATA_ARG_START + NON_DEP_SMP_TFDATA_ARG_MAX_COUNT - 1)) || 
		   (uArg >= NON_DEP_SMP_COORD_ARG_START && uArg <= (NON_DEP_SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize)))? USC_DESTMASK_FULL : 0;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSMPUNPACKArgument(PINTERMEDIATE_STATE	psState, 
										   PCINST				psInst, 
										   IMG_UINT32			uArg, 
										   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSMPUNPACKArgument

 Description		: Get channels used by a SMP_UNPACK instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	if ((uArg >= UF_MAX_CHUNKS_PER_TEXTURE) && (uArg < SMPUNPACK_TFDATA_ARG_START))
	{
		if((uArg - UF_MAX_CHUNKS_PER_TEXTURE) < psInst->uDestCount)
		{
			return USC_DESTMASK_FULL;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return USC_DESTMASK_FULL;
	}
}
#endif /* #if defined(OUTPUT_USPBIN) */

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSHLArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSHLArgument

 Description		: Get channels used by a SHL instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	ASSERT(psInst->eOpcode == ISHL);

	if (	
			uArg == BITWISE_SHIFTEND_SOURCE && 
			psInst->asArg[BITWISE_SHIFT_SOURCE].uType == USEASM_REGTYPE_IMMEDIATE
	   )
	{
		IMG_UINT32	uShiftInBytes;

		uShiftInBytes = psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber / BITS_PER_BYTE;
		return auLiveChansInDest[0] >> uShiftInBytes;
	}
	else
	{
		return USC_DESTMASK_FULL;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSHRArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSHRArgument

 Description		: Get channels used by a SHR instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	ASSERT(psInst->eOpcode == ISHR);

	if (psInst->asArg[uArg].eFmt == UF_REGFORMAT_C10)
	{
		/*
			Assume that whole components are being shifted so at most
			30 bits can be accessed.
		*/
		ASSERT(uArg == BITWISE_SHIFTEND_SOURCE);
		return 7;
	}
	else
	{
		return USC_DESTMASK_FULL;
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_UINT32 GetLiveChansInVPCKFixedPointToFloatArgument(PINTERMEDIATE_STATE	psState, 
													   PCINST				psInst, 
													   IMG_UINT32			uArg, 
													   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInVPCKFixedPointToFloatArgument

 Description		: Get channels used by a VPCKFLTU8/VPCKFLTS8/VPCKFLTU16/VPCKFLTS16 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uLiveChanMask;
	IMG_UINT32	uDestChansUsed;

	PVR_UNREFERENCED_PARAMETER(uArg);

	uDestChansUsed = 
		ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, 0 /* uVecDestIdx */, auLiveChansInDest);
	uDestChansUsed >>= psInst->u.psVec->uDestSelectShift;

	uLiveChanMask = 
		ApplySwizzleToMask(psInst->u.psVec->auSwizzle[0], uDestChansUsed);
	if (psInst->eOpcode == IVPCKFLTU16 || psInst->eOpcode == IVPCKFLTS16)
	{
		IMG_UINT32	uLiveByteMask;

		uLiveByteMask = 0;
		if (uLiveChanMask & USC_X_CHAN_MASK)
		{
			uLiveByteMask |= USC_XY_CHAN_MASK;
		}
		if (uLiveChanMask & USC_Y_CHAN_MASK)
		{
			uLiveByteMask |= USC_ZW_CHAN_MASK;
		}
		return uLiveByteMask;
	}
	else
	{
		ASSERT(psInst->eOpcode == IVPCKFLTU8 || psInst->eOpcode == IVPCKFLTS8);
		return uLiveChanMask;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInVPCKU8U8Argument(PINTERMEDIATE_STATE	psState, 
										  PCINST				psInst, 
										  IMG_UINT32			uArg, 
										  IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInVPCKU8U8Argument

 Description		: Get channels used by a VPCKU8U8 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	ASSERT(psInst->eOpcode == IVPCKU8U8);
	ASSERT(uArg == 0);

	return ApplySwizzleToMask(psInst->u.psVec->auSwizzle[0], auLiveChansInDest[0]);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_UINT32 GetLiveChansInStoreArgument(PINTERMEDIATE_STATE		psState,
									   PCINST					psInst,
									   IMG_UINT32				uArg,
									   IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInStoreArgument

 Description		: Get channels used by a STAB/STAW/STAD/STLB/STLW/STLD instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	if (uArg == MEMSTORE_DATA_ARGINDEX)
	{
		IMG_UINT32	uDataSize;

		switch (psInst->eOpcode)
		{
			case ISTAB:
			case ISTLB:	
			{
				uDataSize = BYTE_SIZE;
				break;
			}
			case ISTAW:
			case ISTLW:	
			{
				if (psInst->asArg[uArg].eFmt == UF_REGFORMAT_C10)
				{
					/*
						For storing a word from a C10 format register
						we can assume only 10 bits (one C10 channel)
						are actually used.
					*/
					uDataSize = 1;
				}
				else
				{
					uDataSize = WORD_SIZE; 
				}
				break;
			}
			case ISTAD:
			case ISTLD:
			{
				if (psInst->asArg[uArg].eFmt == UF_REGFORMAT_C10)
				{
					/*
						For storing a dword from a C10 format register
						we can assume only 30 bits (three C10 channels)
						are actually used.
					*/
					return USC_XYZ_CHAN_MASK;
				}
				else
				{
					uDataSize = LONG_SIZE;
				}
				break;
			}
			default: imgabort();
		}

		return (1 << uDataSize) - 1;
	}
	else
	{
		return USC_ALL_CHAN_MASK;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSPILLWRITEArgument(PINTERMEDIATE_STATE	psState, 
										    PCINST				psInst, 
										    IMG_UINT32			uArg, 
										    IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSPILLWRITEArgument

 Description		: Get channels used by a SPILLWRITE instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);
	PVR_UNREFERENCED_PARAMETER(uArg);

	ASSERT(psInst->eOpcode == ISPILLWRITE);

	return psInst->u.psMemSpill->uLiveChans;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInMOVC_U8_C10_Argument(PINTERMEDIATE_STATE	psState, 
											  PCINST				psInst, 
										      IMG_UINT32			uArg, 
										      IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInMOVC_U8_C10_Argument

 Description		: Get channels used by a MOVC_U8/MOVC_C10 instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uArg);

	ASSERT(psInst->eOpcode == IMOVC_U8 || psInst->eOpcode == IMOVC_C10);

	return auLiveChansInDest[0];
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSMPArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSMPArgument

 Description		: Get channels used by a texture sample instruction source.

 Parameters			: psState			- Compiler state.
					  psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Mask of channels used from each instruction
										destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	#if defined(OUTPUT_USPBIN)
	if (
			(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0
			&&
			((uArg >= SMP_STATE_ARG_START) && (uArg < SMP_DRC_ARG_START))
		)
	{
		return 0;
	}
	else
	#endif /* defined(OUTPUT_USPBIN) */
	if (uArg == (SMP_COORD_ARG_START + psInst->u.psSmp->uCoordSize - 1))
	{
		/*
			The instruction may only partially use the last register of the 
			coordinate data.
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInst->u.psSmp->bCoordSelectUpper)
		{
			ASSERT(psInst->u.psSmp->uCoordSize == 1);
			ASSERT((psInst->u.psSmp->uUsedCoordChanMask & ~USC_XY_CHAN_MASK) == 0);
			return psInst->u.psSmp->uUsedCoordChanMask << USC_Z_CHAN;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			return psInst->u.psSmp->uUsedCoordChanMask;
		}
	}
	else if (uArg == SMP_PROJ_ARG)
	{
		return psInst->u.psSmp->uProjMask << psInst->u.psSmp->uProjChannel;
	}
	else if (uArg == SMP_ARRAYINDEX_ARG)
	{
		if (psInst->u.psSmp->bTextureArray)
		{
			return USC_ALL_CHAN_MASK;
		}
		else
		{
			return 0;
		}
	}
	else if (uArg == SMP_LOD_ARG_START)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (
				(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
				(psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) == 0
			)
		{
			/*
				For SGX543 the LOD source is the bottom channel of a vector register.
			*/
			return USC_X_CHAN_MASK;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			return USC_ALL_CHAN_MASK;
		}
	}
	else if (uArg >= SMP_GRAD_ARG_START && uArg < (SMP_GRAD_ARG_START + psInst->u.psSmp->uGradSize))
	{
		IMG_UINT32	uGradIdx;

		uGradIdx = uArg - SMP_GRAD_ARG_START;
		ASSERT(uGradIdx < SMP_MAXIMUM_GRAD_ARG_COUNT);
		return psInst->u.psSmp->auUsedGradChanMask[uGradIdx];
	}
	else
	{
		return USC_ALL_CHAN_MASK;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInPCKArgument(PINTERMEDIATE_STATE	psState, 
									 PCINST					psInst, 
									 IMG_UINT32				uArg, 
									 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInPCKLArgument

 Description		: Get channels used by a PCK instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	IMG_UINT32	uSrcsUsed;

	uSrcsUsed = GetSourcesUsedFromPack(psState, 
										psInst, 
										psInst->auDestMask[0], 
										auLiveChansInDest[0]);

	if (uSrcsUsed & (1U << uArg))
	{
		return GetChansUsedFromPackSource(psState, psInst, uArg);
	}
	else
	{
		return 0;
	}
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInFloatArgument(PINTERMEDIATE_STATE	psState, 
									   PCINST				psInst, 
									   IMG_UINT32			uArg, 
									   IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFloatArgument

 Description		: Get channels used by a scalar floating point instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FLOAT);
	ASSERT(uArg < psInst->uArgumentCount);

	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	return FormatToChannelMask(psState, &psInst->asArg[uArg], psInst->u.psFloat->asSrcMod[uArg].uComponent);
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInFPTESTArgument(PINTERMEDIATE_STATE	psState, 
									    PCINST				psInst, 
									    IMG_UINT32			uArg, 
									    IMG_UINT32			auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInFPTESTArgument

 Description		: Get channels used by a FPADD8/FPSUB8 instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uArg);

	ASSERT(psInst->eOpcode == IFPADD8 || psInst->eOpcode == IFPSUB8);

	return auLiveChansInDest[0];
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInSaveRestoreIRegArgument(PINTERMEDIATE_STATE	psState, 
												 PCINST					psInst, 
												 IMG_UINT32				uArg, 
												 IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInSaveRestoreIRegArgument

 Description		: Get channels used by a SAVEIREG/RESTOREIREG instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  auLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uArg);

	ASSERT(psInst->eOpcode == ISAVEIREG || psInst->eOpcode == IRESTOREIREG);

	return auLiveChansInDest[0];
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInArgumentDefault(PINTERMEDIATE_STATE	psState, 
									     PCINST					psInst, 
									     IMG_UINT32				uArg, 
									     IMG_UINT32				auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInArgumentDefault

 Description		: Get channels used by an instruction source to a 

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(uArg);
	PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);

	return USC_ALL_CHAN_MASK;
}

IMG_INTERNAL
IMG_UINT32 GetLiveChansInArgument(PINTERMEDIATE_STATE psState, 
								  PCINST psInst, 
								  IMG_UINT32 uArg, 
								  IMG_UINT32 auLiveChansInDest[])
/*********************************************************************************
 Function			: GetLiveChansInArgument

 Description		: Get channels used by an instruction source.

 Parameters			: psInst			- Instruction.
					  uArg				- Argument number.
					  uLiveChansInDest	- Live channels in the destination.

 Globals Effected	: None

 Return				: Mask of the channels used.
*********************************************************************************/
{
	PFN_GET_LIVE_CHANS_IN_ARGUMENT	pfGetLiveChansInArgument;

	pfGetLiveChansInArgument = g_psInstDesc[psInst->eOpcode].pfGetLiveChansInArgument;
	ASSERT(pfGetLiveChansInArgument != NULL);
	return pfGetLiveChansInArgument(psState, psInst, uArg, auLiveChansInDest);
}

/*****************************************************************************
 FUNCTION	: GetLiveChansInArg
    
 PURPOSE	: Get the channels that are used from an instruction argument.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the channels for.
			  uArg			- Argument number to get the channels for.
			  
 RETURNS	: The mask of channels used.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetLiveChansInArg(PINTERMEDIATE_STATE psState, 
										  PINST psInst, 
										  IMG_UINT32 uArg)
{
	if (psInst->eOpcode == ICALL)
	{
		return GetLiveChansInCALLArg(psState, psInst, uArg);
	}
	else
	{
		IMG_UINT32	auLiveChansInDest[USC_MAX_NONCALL_DEST_COUNT];
		
		GetLiveChansInDest(psState, psInst, auLiveChansInDest);
		return GetLiveChansInArgument(psState, 
									  psInst, 
									  uArg, 
									  auLiveChansInDest);
	}
}

static IMG_VOID MergeLivesets(PINTERMEDIATE_STATE psState,
							  PREGISTER_LIVESET psDest, 
							  PREGISTER_LIVESET psSrc)
/*********************************************************************************
 Function			: MergeLiveness

 Description		: Merge together two sets of register liveness information.
 
 Parameters			: psState				- Compiler stat
					  psDest				- Destination for the merge.
					  psSrc					- Source for the merge.
					  uNumTempRegisters		- The number of temporary registers.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 i;
	VectorOp(psState, USC_VEC_OR, 
			 &psDest->sPredicate, 
			 &psDest->sPredicate, &psSrc->sPredicate);
	VectorOp(psState, USC_VEC_OR, 
			 &psDest->sFpInternal, 
			 &psDest->sFpInternal, &psSrc->sFpInternal);
	VectorOp(psState, USC_VEC_OR, 
			 &psDest->sPrimAttr, 
			 &psDest->sPrimAttr, &psSrc->sPrimAttr);

	VectorOp(psState, USC_VEC_OR, 
			 &psDest->sTemp, 
			 &psDest->sTemp, &psSrc->sTemp);

	VectorOp(psState, USC_VEC_OR, 
			 &psDest->sOutput, 
			 &psDest->sOutput, &psSrc->sOutput);
	for (i = 0; i < (sizeof(psDest->puIndexReg) / sizeof(psDest->puIndexReg[0])); i++)
	{
		psDest->puIndexReg[i] |= psSrc->puIndexReg[i];
	}
	if (psSrc->bLinkReg)
	{
		psDest->bLinkReg = IMG_TRUE;
	}
}

static IMG_BOOL CompareLivesets(PINTERMEDIATE_STATE psState, 
								PREGISTER_LIVESET psDest, 
								PREGISTER_LIVESET psSrc)
/*********************************************************************************
 Function			: CompareLiveness

 Description		: Compare two sets of register liveness information.
 
 Parameters			: psDest				- Sets to compare.
					  psSrc					

 Globals Effected	: None

 Return				: TRUE if the two live sets are equal.
*********************************************************************************/
{
	IMG_UINT32 i;
	USC_VECTOR sTemp;

	InitVector(&sTemp, USC_MIN_VECTOR_CHUNK, IMG_FALSE);

	if (VectorOp(psState, USC_VEC_EQ, &sTemp, &psDest->sPredicate, &psSrc->sPredicate) == NULL)
	{	
		return IMG_FALSE;
	}
	if (VectorOp(psState, USC_VEC_EQ, &sTemp, &psDest->sFpInternal, &psSrc->sFpInternal) == NULL)
	{	
		return IMG_FALSE;
	}
	if (VectorOp(psState, USC_VEC_EQ, &sTemp, &psDest->sPrimAttr, &psSrc->sPrimAttr) == NULL)
	{	
		return IMG_FALSE;
	}
	if (VectorOp(psState, USC_VEC_EQ, &sTemp, &psDest->sTemp, &psSrc->sTemp) == NULL)
	{	
		return IMG_FALSE;
	}
	if (VectorOp(psState, USC_VEC_EQ, &sTemp, &psDest->sOutput, &psSrc->sOutput) == NULL)
	{	
		return IMG_FALSE;
	}
	for (i = 0; i < (sizeof(psDest->puIndexReg) / sizeof(psDest->puIndexReg[0])); i++)
	{
		if (psDest->puIndexReg[i] != psSrc->puIndexReg[i])
		{
			return IMG_FALSE;
		}
	}
	if (psDest->bLinkReg != psSrc->bLinkReg)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

//#define PRINT_LIVESETS
#ifdef PRINT_LIVESETS
static IMG_VOID SPrintLivesetRegisterType(PINTERMEDIATE_STATE	psState, 
										  PREGISTER_LIVESET		psLiveset, 
										  IMG_PBOOL				pbFirst,
										  IMG_UINT32			uRegisterType,
										  IMG_PCHAR				pszTypeName,
										  IMG_UINT32			uRegisterCount,
										  IMG_UINT32			uMaxMask)
{
	IMG_UINT32	uRegIdx;

	for (uRegIdx = 0; uRegIdx < uRegisterCount; uRegIdx++)
	{
		IMG_UINT32	uLiveMask;

		uLiveMask = 
			GetRegisterLiveMask(psState, 
							    psLiveset, 
								uRegisterType, 
								uRegIdx, 
								0 /* uArrayOffset */);

		if (uLiveMask != 0)
		{
			if (!(*pbFirst))
			{
				printf(" ");
			}
			else
			{
				(*pbFirst) = IMG_FALSE;
			}
			printf("%s%d", pszTypeName, uRegIdx);
			if (uLiveMask != uMaxMask)
			{
				printf(".%s%s%s%s",
					(uLiveMask & USC_X_CHAN_MASK) ? "x" : "",
					(uLiveMask & USC_Y_CHAN_MASK) ? "y" : "",
					(uLiveMask & USC_Z_CHAN_MASK) ? "z" : "",
					(uLiveMask & USC_W_CHAN_MASK) ? "w" : "");
			}
		}
	}
}

static IMG_VOID SPrintLiveset(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET psLiveset)
{
	IMG_BOOL	bFirst = IMG_FALSE;

	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_TEMP, 
							  "r", 
							  psState->uNumRegisters,
							  USC_ALL_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_FPINTERNAL,
							  "i", 
							  psState->uGPISizeInScalarRegs,
							  USC_ALL_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_PRIMATTR, 
							  "pa", 
							  EURASIA_USE_PRIMATTR_BANK_SIZE,
							  USC_ALL_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_OUTPUT, 
							  "o", 
							  EURASIA_USE_OUTPUT_BANK_SIZE,
							  USC_ALL_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_INDEX, 
							  "idx", 
							  EURASIA_USE_INDEX_BANK_SIZE,
							  USC_ALL_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_PREDICATE, 
							  "p", 
							  psState->uNumPredicates,
							  USC_X_CHAN_MASK);
	SPrintLivesetRegisterType(psState, 
							  psLiveset, 
							  &bFirst, 
							  USEASM_REGTYPE_LINK, 
							  "link", 
							  1 /* uRegisterCount */,
							  USC_X_CHAN_MASK);
}
#endif

static
IMG_VOID TranslateArrayElement(PINTERMEDIATE_STATE	psState,
							   PREGISTER_LIVESET	psLiveset,
							   IMG_UINT32			uArrayNumber,
							   IMG_UINT32			uArrayOffset,
							   USC_PVECTOR*			ppsVector,
							   IMG_PUINT32			puStart)
/*********************************************************************************
 Function			: TranslateArrayElement

 Description		: Gets the mask of live channels for a register.
 
 Parameters			: psState		- Internal compiler state
					  psLiveset		- Register liveset.
					  uArrayNumber	- Number of the array.
					  uArrayOffset	- Offset within the array.
					  ppsVector		- Returns the vector holding the mask
									of channels live in this element.
					  puStart		- Returns the start within the vector of the
									mask of channels live in this element.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_VEC_ARRAY_REG	psArray;

	ASSERT(uArrayNumber < psState->uNumVecArrayRegs);
	/* Vector register array must be specified. */
	ASSERT(psState->apsVecArrayReg != NULL);
	ASSERT(psState->apsVecArrayReg[uArrayNumber] != NULL);

	psArray = psState->apsVecArrayReg[uArrayNumber];

	/*
		Translate to a register number.
	*/
	*puStart = psArray->uBaseReg * CHANS_PER_REGISTER + uArrayOffset * psArray->uChannelsPerDword;

	/*
		Get the live channels for the translated register.
	*/
	switch (psArray->uRegType)
	{
		case USEASM_REGTYPE_TEMP: *ppsVector = &psLiveset->sTemp; break;
		case USEASM_REGTYPE_PRIMATTR: *ppsVector = &psLiveset->sPrimAttr; break;
		case USEASM_REGTYPE_OUTPUT: *ppsVector = &psLiveset->sOutput; break;
		default: imgabort();
	}
}

IMG_INTERNAL IMG_UINT32 GetRegisterLiveMask(PINTERMEDIATE_STATE psState, 
											PREGISTER_LIVESET psLiveset, 
											IMG_UINT32 uType, 
											IMG_UINT32 uNumber, 
											IMG_UINT32 uArrayOffset)
/*********************************************************************************
 Function			: GetRegisterLiveMask

 Description		: Gets the mask of live channels for a register.
 
 Parameters			: psState		- Internal compiler state
					  psLiveness	- Register liveness information.
					  uType			- Register to check.
					  uNumber
					  uArrayOffset

 Globals Effected	: None

 Return				: TRUE if the register is live.
*********************************************************************************/
{
	IMG_UINT32 uStart = uNumber * CHANS_PER_REGISTER;
	switch (uType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			return VectorGetRange(psState, 
								  &psLiveset->sTemp, 
								  uStart + CHANS_PER_REGISTER - 1, 
								  uStart);
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			return VectorGetRange(psState, 
								  &psLiveset->sFpInternal, 
								  uStart + CHANS_PER_REGISTER - 1, 
								  uStart);
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			return VectorGetRange(psState, 
								  &psLiveset->sPrimAttr, 
								  uStart + CHANS_PER_REGISTER - 1, 
								  uStart);
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			return VectorGetRange(psState, 
								  &psLiveset->sOutput, 
								  uStart + CHANS_PER_REGISTER - 1, 
								  uStart);
		}
		case USEASM_REGTYPE_INDEX: 
		{
			return GetRange(psLiveset->puIndexReg, 
							uStart + CHANS_PER_REGISTER - 1, 
							uStart);
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			if (VectorGetRange(psState, 
							   &psLiveset->sPredicate, 
							   uNumber, 
							   uNumber) != 0)
			{
				return USC_DESTMASK_FULL;
			}
			else
			{
				return 0;
			}
		}
		case USC_REGTYPE_REGARRAY: 
		{
			USC_PVECTOR	psVector;
			IMG_UINT32	uStart_RegArray;
			TranslateArrayElement(psState, psLiveset, uNumber, uArrayOffset, &psVector, &uStart_RegArray);
			return VectorGetRange(psState,
								  psVector,
								  uStart_RegArray + CHANS_PER_REGISTER - 1,
								  uStart_RegArray);
		}
		case USC_REGTYPE_UNUSEDDEST:
		{
			return 0;
		}
		case USEASM_REGTYPE_LINK:
		{
			if(psLiveset->bLinkReg)
			{
				return USC_DESTMASK_FULL;
			}
			else
			{
				return 0;
			}
		}
		default: 
		{	
			return USC_DESTMASK_FULL;
		}
	}
}

IMG_INTERNAL IMG_VOID SetRegisterLiveMask(PINTERMEDIATE_STATE psState, 
										  PREGISTER_LIVESET psLiveset, 
										  IMG_UINT32 uType, 
										  IMG_UINT32 uNumber, 
										  IMG_UINT32 uArrayOffset, 
										  IMG_UINT32 uMask)
/*********************************************************************************
 Function			: SetRegisterLiveMask

 Description		: Sets the live mask for a register.

 Parameters			: psState		- Internal compiler state
					  psLiveness	- Register liveness information.
					  uType			- Register type.
					  uNumber		- Register number.
					  uMask			- Mask to set.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uStart = uNumber * CHANS_PER_REGISTER;
	switch (uType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			VectorSetRange(psState, &psLiveset->sTemp, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			VectorSetRange(psState, &psLiveset->sFpInternal, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			VectorSetRange(psState, &psLiveset->sPrimAttr,
						   uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
			break;
		}
		case USEASM_REGTYPE_OUTPUT:
		{
			VectorSetRange(psState, &psLiveset->sOutput,
						   uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			if (uMask == 0)
			{
				VectorSetRange(psState, &psLiveset->sPredicate, 
							   uNumber, uNumber, 0);
			}
			else
			{
				VectorSetRange(psState, &psLiveset->sPredicate,
							   uNumber, uNumber, 1);
			}
			break;
		}
		case USC_REGTYPE_REGARRAY: 
		{
			USC_PVECTOR		psVector;
			IMG_UINT32		uStart_RegArray;

			TranslateArrayElement(psState, psLiveset, uNumber, uArrayOffset, &psVector, &uStart_RegArray);
			VectorSetRange(psState, psVector, uStart_RegArray + CHANS_PER_REGISTER - 1, uStart_RegArray, uMask);
			break;
		}
		case USEASM_REGTYPE_INDEX: 
		{
			SetRange(psLiveset->puIndexReg,
					 uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
			break;
		}
		case USEASM_REGTYPE_LINK:
		{
			if(uMask == USC_DESTMASK_FULL)
			{
				psLiveset->bLinkReg = IMG_TRUE;
			}
			else
			{
				psLiveset->bLinkReg = IMG_FALSE;
			}
			break;
		}
	}
}

static IMG_VOID ChangeRegisterLiveMask(PINTERMEDIATE_STATE psState, 
									   PREGISTER_LIVESET psLiveset, 
									   IMG_BOOL bSet,
									   IMG_UINT32 uType, 
									   IMG_UINT32 uNumber, 
									   IMG_UINT32 uArrayOffset, 
									   IMG_UINT32 uWrittenMask)
/*********************************************************************************
 Function			: ChangeRegisterLiveMask

 Description		: Change the live flag for a register.

 Parameters			: psState		- Internal compiler state
					  psLiveness	- Register liveness information.
					  bSet			- Set (IMG_TRUE) or clear (IMG_FALSE) the mask
					  uType			- Register to change.
					  uNumber
					  uArrayOffset
					  uWrittenMask	- Mask to set or clear.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uStart = uNumber * CHANS_PER_REGISTER;
	IMG_UINT32 uOldMask, uNewMask;
	IMG_PUINT32 puArray = NULL;
	USC_PVECTOR psVector = NULL;

	switch (uType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			psVector = &psLiveset->sTemp;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			psVector = &psLiveset->sFpInternal;
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			psVector = &psLiveset->sPrimAttr;
			break;
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			psVector = &psLiveset->sOutput;
			break;
		}
		case USEASM_REGTYPE_PREDICATE: 
		{
			ASSERT(uWrittenMask == 0 || uWrittenMask == USC_ALL_CHAN_MASK);
			if (bSet)
			{
				if (uWrittenMask == USC_ALL_CHAN_MASK)
				{
					VectorSetRange(psState, &psLiveset->sPredicate, uNumber, uNumber, 1);
				}
			}
			else
			{
				if (uWrittenMask == USC_ALL_CHAN_MASK)
				{
					VectorSetRange(psState, &psLiveset->sPredicate, uNumber, uNumber, 0);
				}
			}
			return;
		}
		case USEASM_REGTYPE_INDEX: 
		{
			puArray = psLiveset->puIndexReg;
			break;
		}
		case USC_REGTYPE_REGARRAY: 
		{
			TranslateArrayElement(psState, psLiveset, uNumber, uArrayOffset, &psVector, &uStart);
			break;
		}
		default:
		{
			puArray = NULL;
			break;
		}
	}

	if (puArray != NULL)
	{
		uOldMask = GetRange( puArray, uStart + CHANS_PER_REGISTER - 1, uStart); 
		uNewMask = bSet? (uOldMask | uWrittenMask): (uOldMask & ~uWrittenMask);
		SetRange(puArray, uStart + CHANS_PER_REGISTER - 1, uStart, uNewMask); 
	}

	if (psVector != NULL)
	{
		if (bSet)
		{
			VectorOrRange(psState, psVector, uStart + CHANS_PER_REGISTER - 1, uStart, uWrittenMask);
		}
		else
		{
			VectorAndRange(psState, psVector, uStart + CHANS_PER_REGISTER - 1, uStart, ~uWrittenMask);
		}
	}
}

#if 0
static IMG_VOID ReduceRegisterLiveMask(PINTERMEDIATE_STATE psState,
									   PREGISTER_LIVESET psLiveset,
									   IMG_UINT32 uType,
									   IMG_UINT32 uNumber,
									   IMG_UINT32 uArrayOffset,
									   IMG_UINT32 uWrittenMask)
/*********************************************************************************
 Function			: IncreaseRegisterLiveMask

 Description		: Reduce the mask of live channels for a register.

 Parameters			: psState		- Compiler state.
					  psLiveset		- Register liveset to update.
					  uType			- Register to update.
					  uNumber
					  uArrayOffset
					  uReadMask		- Mask of channels to clear.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ChangeRegisterLiveMask(psState, psLiveset, IMG_FALSE,
						   uType, uNumber, uArrayOffset, uWrittenMask);
}
#endif

IMG_INTERNAL
IMG_VOID IncreaseRegisterLiveMask(PINTERMEDIATE_STATE psState, 
		  						  PREGISTER_LIVESET psLiveset, 
								  IMG_UINT32 uType, 
								  IMG_UINT32 uNumber, 
								  IMG_UINT32 uArrayOffset, 
								  IMG_UINT32 uReadMask)
/*********************************************************************************
 Function			: IncreaseRegisterLiveMask

 Description		: Increase the mask of live channels for a register.

 Parameters			: psState		- Compiler state.
					  psLiveset		- Register liveset to update.
					  uType			- Register to update.
					  uNumber
					  uArrayOffset
					  uReadMask		- Mask of channels to set.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ChangeRegisterLiveMask(psState, psLiveset, IMG_TRUE, 
						   uType, uNumber, uArrayOffset, uReadMask);
}

static
IMG_VOID GetIndexRangeForDest(PINTERMEDIATE_STATE	psState,
							  PARG					psDest,
							  IMG_PUINT32			puRealType,
							  IMG_PUINT32			puBaseReg,
							  IMG_PUINT32			puNumRegs)
/*********************************************************************************
 Function			: GetIndexRangeForDest

 Description		: For an indexed destination argument gets the range of registers
					  potentially accessed.

 Parameters			: psState		- Compiler state.
					  psDest		- Destination argument to get the range for.
					  puRealType	- Returns the underlying type for the argument.
					  puBaseReg		- Returns the first register in the range.
					  puNumRegs		- Returns the number of registers in the range.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	if (psDest->uType == USC_REGTYPE_REGARRAY)
	{
		IMG_UINT32	uArrayNum = psDest->uNumber;
		PUSC_VEC_ARRAY_REG	psArray;

		ASSERT(uArrayNum < psState->uNumVecArrayRegs);
		psArray = psState->apsVecArrayReg[uArrayNum];
		
		/*
			Represent register arrays by ranges of other
			registers.
		*/
		*puRealType = psArray->uRegType;

		/*
			Assume we can write any registers in the 
			register array.
		*/
		*puBaseReg = psArray->uBaseReg;
		*puNumRegs = psArray->uRegs;
	}
	else
	{
		ASSERT(psDest->uType == USEASM_REGTYPE_OUTPUT);

		/*
			No change of type.
		*/
		*puRealType = USEASM_REGTYPE_OUTPUT;

		/*
			Assume we can write any registers in the output bank.
		*/
		*puBaseReg = 0;
		*puNumRegs = EURASIA_USE_OUTPUT_BANK_SIZE;
	}
}

/*********************************************************************************
 Function			: GetIndexedRegisterLiveMask

 Description		: Gets the mask of live channels for a register with an index.
 
 Parameters			: psState		- Internal compiler state
					  psLiveness	- Register liveness information.
					  psDest		- Register to check.

 Globals Effected	: None

 Return				: The mask of live channels.
*********************************************************************************/
static
IMG_UINT32 GetIndexedRegisterLiveMask(PINTERMEDIATE_STATE	psState,
									  PREGISTER_LIVESET		psLiveset,
								      PARG					psDest)
{
	USC_PVECTOR	psVector;
	IMG_UINT32	uBaseReg;
	IMG_UINT32	uNumRegs;
	IMG_UINT32	uIdx;
	IMG_UINT32	uLiveChansInDest;
	IMG_UINT32	uRealType;

	/*
		Get the range of registers potentially accessed by the index.
	*/
	GetIndexRangeForDest(psState, psDest, &uRealType, &uBaseReg, &uNumRegs);

	switch (uRealType)
	{
		case USEASM_REGTYPE_TEMP:	psVector = &psLiveset->sTemp; break;
		case USEASM_REGTYPE_OUTPUT:	psVector = &psLiveset->sOutput; break;
		default: imgabort();
	}

	/*
		Take the union of the live channels in all the registers
		in the range potentially written by the instruction.
	*/
	uLiveChansInDest = 0;
	for (uIdx = 0; uIdx < uNumRegs; uIdx++)
	{
		IMG_UINT32		uChanIdx;

		uChanIdx = (uBaseReg + uIdx) * CHANS_PER_REGISTER;
		uLiveChansInDest |= VectorGetRange(psState, 
										   psVector, 
										   uChanIdx + CHANS_PER_REGISTER - 1, 
										   uChanIdx);
		/*
			Stop once we can't increase the mask anymore.
		*/
		if (uLiveChansInDest == USC_DESTMASK_FULL)
		{
			break;
		}
	}
	return uLiveChansInDest;
}

/*********************************************************************************
 Function			: IncreaseIndexedRegisterLiveMask

 Description		: Reduce the mask of live channels for a register which includes
					  an index.

 Parameters			: psState		- Compiler state.
					  psLiveset		- Register liveset to update.
					  psArg			- Register to update.
					  uMask			- Mask of channels to set.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static
IMG_VOID IncreaseIndexedRegisterLiveMask(PINTERMEDIATE_STATE	psState,
										 PREGISTER_LIVESET		psLiveset,
										 PARG					psArg,
										 IMG_UINT32				uMask)
{
	switch (psArg->uType)
	{
		case USEASM_REGTYPE_SECATTR:
		case USC_REGTYPE_GSINPUT:
		{
			/*
				No need to track liveness for secondary attributes or
				geometry shader inputs.
			*/
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		case USEASM_REGTYPE_OUTPUT:
		{
			IMG_UINT32	uRegNum;
			IMG_UINT32	uRange = 0;

			/*
				Get the possible range read in the source.
			*/
			if (psArg->uType == USEASM_REGTYPE_PRIMATTR)
			{
				if(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
				{
					uRange = USC_MAXIMUM_TEXTURE_COORDINATES * CHANNELS_PER_INPUT_REGISTER;
				}
				else if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
				{
					uRange = EURASIA_USE_PRIMATTR_BANK_SIZE;
				}
			}
			else
			{
				ASSERT(psArg->uType == USEASM_REGTYPE_OUTPUT);
				uRange = EURASIA_USE_OUTPUT_BANK_SIZE;
			}

			/*
				Increase the live channels for each register in the range.
			*/
			for (uRegNum = 0; uRegNum < uRange; uRegNum++)
			{
				IncreaseRegisterLiveMask(psState, 
										 psLiveset, 
										 psArg->uType, 
										 uRegNum, 
										 0, 
										 uMask);
			}
			break;
		}
		case USC_REGTYPE_REGARRAY:
		{
			IMG_UINT32	uArrayOffset;
			IMG_UINT32	uArrayLength;

			ASSERT(psState->apsVecArrayReg != NULL);
			ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
			ASSERT(psState->apsVecArrayReg[psArg->uNumber] != NULL);
			uArrayLength = psState->apsVecArrayReg[psArg->uNumber]->uRegs;

			/*
				Increase the live channels for each temporary register in the
				array.
			*/
			for (uArrayOffset = 0; uArrayOffset < uArrayLength; uArrayOffset++)
			{
				IncreaseRegisterLiveMask(psState, 
										 psLiveset, 
										 psArg->uType, 
										 psArg->uNumber, 
										 uArrayOffset, 
										 uMask);
			}
			break;
		}
		default:
		{
			/*
				No other register types are valid for an index.
			*/
			imgabort();
		}
	}
}

static IMG_VOID ClearSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx)
/*********************************************************************************
 Function			: ClearSource

 Description		: Set an unused source to a safe value.
 
 Parameters			: psState		- Compiler state.	
					  psInst		- Instruction to modify.
					  uSrcIdx		- Source to modify.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	SetSrc(psState, psInst, uSrcIdx, USEASM_REGTYPE_IMMEDIATE, 0, psInst->asArg[uSrcIdx].eFmt);
}

static IMG_VOID SwapPackSources(PINTERMEDIATE_STATE psState,
								PINST psInst,
								IMG_UINT32 uOldMask,
								IMG_UINT32 uNewMask)
/*********************************************************************************
 Function			: SwapPackSources

 Description		: Swap pack instruction sources for a new mask.
 
 Parameters			: psState	- Internal compiler state
					  psInst	- Pack instruction.
					  uOldMask	- Old mask
					  uNewMask	- New mask.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case ICOMBC10:	
		{
			if ((uNewMask & USC_RGB_CHAN_MASK) == 0)
			{
				SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
			}
			if ((uNewMask & USC_ALPHA_CHAN_MASK) == 0)
			{
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);
			}
			break;
		}

		case IUNPCKS16S8:
	    case IUNPCKS16U8:
		case IUNPCKU16U8:
		case IUNPCKU16U16:
		case IUNPCKU16S16:
		case IUNPCKU16F16:
		case IPCKU16F32:
		case IUNPCKS16U16:
		case IPCKS16F32:
		case IUNPCKF16C10:
		case IUNPCKF16O8:
		case IUNPCKF16U8:
		case IUNPCKF16S8:
		case IUNPCKF16U16:
		case IUNPCKF16S16:
		case IPCKF16F16:
		case IPCKF16F32:
		{
			if ((uOldMask & 3) != 0 && (uNewMask & 3) == 0)
			{
				MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
				psInst->u.psPck->auComponent[0] = psInst->u.psPck->auComponent[1];

				ClearSource(psState, psInst, 1 /* uSrcIdx */);
				psInst->u.psPck->auComponent[1] = 0;
			}
			else if ((uNewMask & 12) == 0)
			{
				ClearSource(psState, psInst, 1 /* uSrcIdx */);
				psInst->u.psPck->auComponent[1] = 0;
			}
			break;
		}

		case IPCKU8U8:
		case IPCKU8S8:
		case IPCKU8U16:
		case IPCKU8S16:
		case IPCKS8U8:
		case IPCKS8U16:
		case IPCKS8S16:
		case IPCKU8F16:
		case IPCKU8F32:
		case IPCKS8F16:
		case IPCKS8F32:
		case IPCKC10C10:
		case IPCKC10F32:
		case IPCKC10F16:
		{
			if (g_auSetBitCount[uNewMask] >= 3)
			{
				return;
			}
			if (g_auSetBitCount[uNewMask] == 1)
			{
				IMG_UINT32	uSrcPos, uChan;

				uSrcPos = 0;
				for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
				{
					if (uNewMask == (IMG_UINT32)(1 << uChan))
					{
						break;
					}
					if (uOldMask & (1U << uChan))
					{
						uSrcPos++;
					}
				}

				if (uSrcPos == 1)
				{
					MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, 1 /* uMoveFromSrcIdx */);
					psInst->u.psPck->auComponent[0] = psInst->u.psPck->auComponent[1];
				}
				ClearSource(psState, psInst, 1 /* uSrcIdx */);
				psInst->u.psPck->auComponent[1] = 0;
			}
			else
			{
				IMG_UINT32	auSrcPos[2];
				IMG_UINT32	uSrcPos, uNextSrc, uChan;

				uSrcPos = 0;
				uNextSrc = 0;
				for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
				{
					if (uNewMask & (1U << uChan))
					{
						auSrcPos[uNextSrc++] = uSrcPos;
					}
					if (uOldMask & (1U << uChan))
					{
						uSrcPos = (uSrcPos + 1) % 2;
					}
				}

				if (auSrcPos[0] == 1 && auSrcPos[1] == 0)
				{
					SwapPCKSources(psState, psInst);
				}
				else if (auSrcPos[0] == 0 && auSrcPos[1] == 0)
				{
					CopySrc(psState, psInst, 1 /* uCopyToSrcIdx */, psInst, 0 /* uCopyFromSrcIdx */);
					psInst->u.psPck->auComponent[1] = psInst->u.psPck->auComponent[0];
				}
				else if (auSrcPos[0] == 1 && auSrcPos[1] == 1)
				{
					CopySrc(psState, psInst, 0 /* uCopyToSrcIdx */, psInst, 1 /* uCopyFromSrcIdx */);
					psInst->u.psPck->auComponent[0] = psInst->u.psPck->auComponent[1];
				}
			}
			break;
		}

		case IUNPCKF32C10:
		case IUNPCKF32O8:
		case IUNPCKF32U8:
		case IUNPCKF32S8:
		case IUNPCKF32U16:
		case IUNPCKF32S16:
		case IUNPCKF32F16:
		{
			/*
				All these can only set 1 channel in the dest, so they only use
				1 source
			*/
			break;
		}

		default:
		{
			imgabort();
			break;
		}
	}
}

/*********************************************************************************
 Function			: IncreaseIndexLiveMask

 Description		: Increase the channels live in an index used in an instruction
					  argument.
 
 Parameters			: psState		- Current compiler state.
					  psArg			- Indexed argument.
					  psLiveness	- Live register set.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID IncreaseIndexLiveMask(PINTERMEDIATE_STATE	psState,
							   PARG					psArg,
							   PREGISTER_LIVESET	psLiveset)
{
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		IncreaseRegisterLiveMask(psState, psLiveset, psArg->uIndexType, psArg->uIndexNumber, 0, USC_XY_CHAN_MASK);
	}
}

/*********************************************************************************
 Function			: GetDestLiveMask

 Description		: Get the channels used in an instruction destination argument.
 
 Parameters			: psState		- Current compiler state.
					  psLiveset		- Live register set.
					  psDest		- Destination to get the live channels in.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static
IMG_UINT32 GetDestLiveMask(PINTERMEDIATE_STATE	psState,
						   PREGISTER_LIVESET	psLiveset,
						   PARG					psDest)
{
	IMG_UINT32	uLiveChansInDest;

	if (psDest->uIndexType != USC_REGTYPE_NOINDEX)
	{
		/*
			Get the live channels for the register range written.
		*/
		uLiveChansInDest = GetIndexedRegisterLiveMask(psState,
													  psLiveset,
													  psDest);
		return uLiveChansInDest;
	}
	else
	{
		/*
			Get the live channels for the exact register written.
		*/
		uLiveChansInDest = GetRegisterLiveMask(psState, 
											   psLiveset, 
											   psDest->uType, 
											   psDest->uNumber,
											   psDest->uArrayOffset);

		return uLiveChansInDest;
	}
}

static
IMG_VOID HandleEFOIRegDest(PINTERMEDIATE_STATE	psState, 
						   PINST				psInst, 
						   PARG					psDest, 
						   IMG_UINT32			uUsedChans, 
						   IMG_PBOOL			pbWriteIReg)
/*********************************************************************************
 Function			: HandleEFOIRegDest

 Description		: Update an EFO instruction if an internal register destination
					  is unused.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- EFO instruction.
					  psDest			- Internal register destinations.
					  uUsedChans		- Channels used from the internal register.
					  pbWriteIReg		- Points to the internal register write
										enable flag.

 Return				: Nothing.
*********************************************************************************/
{
	if (uUsedChans == 0 && (*pbWriteIReg))
	{
		/*
			If the corresponding internal register is
			unused then disable the write to it.
		*/
		ASSERT(psDest->uType == USC_REGTYPE_UNUSEDDEST);
		*pbWriteIReg = IMG_FALSE;

		/*
			Propogate liveness information to the sources.
		*/
		SetupEfoUsage(psState, psInst);
	}
}

static
IMG_VOID HandleEFODest(PINTERMEDIATE_STATE	psState,
					   PINST				psInst,
					   IMG_UINT32			uDestIdx,
					   IMG_UINT32			uUsedChans)
/*********************************************************************************
 Function			: HandleEFODest

 Description		: Handle special cases for EFO destination arguments when
					  doing dead code elimination.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- EFO instruction.
					  uDestIdx			- Index of the destination to handle.
					  uUsedChans		- Channels used from the destination argument.

 Return				: Nothing.
*********************************************************************************/
{
	PARG	psDest = &psInst->asDest[uDestIdx];

	switch (uDestIdx)
	{
		case EFO_US_DEST:
		{
			if (uUsedChans == 0 && !psInst->u.psEfo->bIgnoreDest)
			{
				/*
					If the main destination is unused then replace it
					with a dummy register which is never read.
				*/
				ASSERT(psInst->u.psEfo != NULL);
				psInst->u.psEfo->bIgnoreDest = IMG_TRUE;
				SetDest(psState, psInst, EFO_US_DEST, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_F32);
				SetupEfoUsage(psState, psInst);
			}
			break;
		}
		case EFO_USFROMI0_DEST:
		case EFO_USFROMI1_DEST:
		{
			/*
				These destinations are used only in the EFO generation stage so we should
				never see them.
			*/
			ASSERT(psDest->uType == USC_REGTYPE_UNUSEDDEST);
			break;
		}
		case EFO_I0_DEST:
		{
			HandleEFOIRegDest(psState, psInst, psDest, uUsedChans, &psInst->u.psEfo->bWriteI0);
			break;
		}
		case EFO_I1_DEST:
		{
			HandleEFOIRegDest(psState, psInst, psDest, uUsedChans, &psInst->u.psEfo->bWriteI1);
			break;
		}
		default: imgabort();
	}
}

/*********************************************************************************
 Function			: ReduceInstDestMask

 Description		: Try to reduce the channels written by an instruction to those
					  actually read by later instructions.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- Instruction to update.
					  auLiveChansInDest	- Channels used from the destinations.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static
IMG_VOID ReduceInstDestMask(PINTERMEDIATE_STATE		psState,
							PINST					psInst,
							IMG_UINT32				auLiveChansInDest[])
{
	if	((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DESTCOMPMASK) != 0 &&
		 (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) == 0)
	{
		ASSERT(psInst->uDestCount == 1);
		psInst->auDestMask[0] = psInst->auDestMask[0] & auLiveChansInDest[0];
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	else if (psInst->eOpcode == IVDUAL && (psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0)		
	{
		IMG_UINT32	uDestSlot;

		for (uDestSlot = 0; uDestSlot < VDUAL_DESTSLOT_COUNT; uDestSlot++)
		{
			IMG_UINT32	uUsedVecChanMask;
			IMG_UINT32	uWrittenVecChanMask;
			IMG_UINT32	uRealMask;
			IMG_UINT32	uActualMask;
			PARG		psBaseDest;
			IMG_UINT32	uBaseDest;
			IMG_UINT32	uDestCount;
			IMG_UINT32	uDest;

			uBaseDest = uDestSlot * MAX_SCALAR_REGISTERS_PER_VECTOR;
			psBaseDest = &psInst->asDest[uBaseDest];

			/*
				Convert the mask of bytes used by later instructions from the scalar registers making up this 
				destination into a mask of F32 or F16 channels.
			*/
			uUsedVecChanMask = 
				ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, uDestSlot, auLiveChansInDest);

			/*
				Convert the mask of bytes written by this instruction into a mask of F32 or F16 channels. 
			*/
			uWrittenVecChanMask = 
				ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, uDestSlot, psInst->auDestMask);

			/*
				Get the mask of channels written and actually used.
			*/
			uRealMask = uUsedVecChanMask & uWrittenVecChanMask;

			/*
				Apply the hardware restrictions to get the mask of channels actually written.
			*/
			uActualMask = GetMinimumVDUALDestinationMask(psState, psInst, uDestSlot, psBaseDest, uRealMask);
			if (uActualMask == USC_ANY_NONEMPTY_CHAN_MASK)
			{
				uActualMask = uWrittenVecChanMask;
			}

			/*
				Convert the mask of F16/F32 channels back to a mask of bytes and update the instruction's
				destination write masks.
			*/
			uDestCount = min(MAX_SCALAR_REGISTERS_PER_VECTOR, psInst->uDestCount - uBaseDest);
			for (uDest = 0; uDest < uDestCount; uDest++)
			{
				psInst->auDestMask[uBaseDest + uDest] = ChanMaskToByteMask(psState, uActualMask, uDest, psBaseDest->eFmt);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	else
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			if (psInst->auDestMask[uDestIdx] & ~auLiveChansInDest[uDestIdx])
			{
				IMG_UINT32	uOldMask = psInst->auDestMask[uDestIdx];
				IMG_UINT32	uRealMask = psInst->auDestMask[uDestIdx] & auLiveChansInDest[uDestIdx];
				IMG_UINT32	uMinimalMask;

				/*	
					Get the smallest mask of channels which can be updated by the instruction e.g. only
					.X might be used by later instructions but the hardware only supports updating .X and .Y.
				*/
				uMinimalMask = GetMinimalDestinationMask(psState, psInst, uDestIdx, uRealMask);

				/*
					Store the updated destination mask.
				*/
				if (uMinimalMask != USC_ANY_NONEMPTY_CHAN_MASK)
				{
					psInst->auDestMask[uDestIdx] = uMinimalMask;
				}
				else
				{
					ASSERT(psInst->auDestMask[uDestIdx] != 0);
					uMinimalMask = psInst->auDestMask[uDestIdx];
				}

				if (
						uMinimalMask == 0 && 
						(g_psInstDesc[psInst->eOpcode].uFlags & (DESC_FLAGS_TEXTURESAMPLE | DESC_FLAGS_MULTIPLEDEST)) == 0
				   )
				{
					SetDestUnused(psState, psInst, uDestIdx);
					psInst->auDestMask[uDestIdx] = 0;
				}

				if (psInst->eOpcode == IEFO)
				{
					/*
						Special cases for EFO destinations.
					*/
					HandleEFODest(psState, psInst, uDestIdx, uRealMask);
				}
				else if ((psInst->eOpcode >= IUNPCKS16S8 && psInst->eOpcode <= IPCKC10F16) || psInst->eOpcode == ICOMBC10)
				{
					ASSERT(uDestIdx == 0);
					SwapPackSources(psState, psInst, uOldMask, uMinimalMask);
				}
			}
		}
	}

	#if defined(OUTPUT_USPBIN)
	/*
		Reduce the channels that this pseudo-sample unpack instruction needs
		to fetch to reflect the liveness of the destination registers
	*/
	if	(psInst->eOpcode == ISMPUNPACK_USP)
	{
		IMG_UINT32	uChanMask;
		IMG_UINT32	uDestCount;
		IMG_UINT32	uDestOffset;

		uDestCount	= psInst->uDestCount;
		uChanMask	= psInst->u.psSmpUnpack->uChanMask;

		for (uDestOffset = 0; uDestOffset < uDestCount; uDestOffset++)
		{
			IMG_UINT32	uDestMask;
			IMG_UINT32	uLiveChansInDest;

			uDestMask			= psInst->auDestMask[uDestOffset];
			uLiveChansInDest	= psInst->auLiveChansInDest[uDestOffset];

			switch (psInst->asDest[0].eFmt)
			{
				case UF_REGFORMAT_F32:
				{
					if	(!uLiveChansInDest)
					{
						uDestMask = 0;
					}
					if	(!uDestMask)
					{
						uChanMask &= ~(1U << uDestOffset);
					}
					if	((uChanMask & (0xFU << uDestOffset)) == 0)
					{
						 uDestCount = uDestOffset;	
					}	
					break;
				}
				case UF_REGFORMAT_F16:
				{
					if	((uLiveChansInDest & 0x3U) == 0)
					{
						uDestMask &= ~0x3U;
					}
					if	((uLiveChansInDest & 0xCU) == 0)
					{
						uDestMask &= ~0xCU;
					}

					if	((uDestMask & 0x3U) == 0)
					{
						uChanMask &= ~(1U << (uDestOffset*2));
					}
					if	((uDestMask & 0xCU) == 0)
					{
						uChanMask &= ~(2U << (uDestOffset*2));
					}

					if	((uChanMask & (0xFU << (uDestOffset*2))) == 0)
					{
						 uDestCount = uDestOffset;
					}
					break;
				}
				case UF_REGFORMAT_C10:
				case UF_REGFORMAT_U8:
				{
					uDestMask &= uLiveChansInDest;

					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					if (
							psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
							(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && 
							(psState->uFlags & USC_FLAGS_REPLACEDVECTORREGS) != 0 &&
							uDestOffset == 1
					   )
					{
						/*
							This register represents the second 32 bit destination spanned by the 40 bit
							C10 result. Because the ALPHA channel (bits 30-40) is split across the two
							destinations the bottom bits of this destination will be live iff the
							top bits of the first destination are live. So we can ignore this destination.
						*/
					}
					else
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					{
						ASSERT(uDestOffset == 0);
						uChanMask = uDestMask;
					}
					break;
				}
				default:
				{
					break;
				}
			}

			psInst->auDestMask[uDestOffset] = uDestMask;
		}

		psInst->u.psSmpUnpack->uChanMask = uChanMask;
		SetDestCount(psState, psInst, uDestCount);
	}
	#endif /* #if defined(OUTPUT_USPBIN) */
}

static IMG_VOID IncreaseLiveMaskForRead(PINTERMEDIATE_STATE psState,
										PREGISTER_LIVESET psLiveset,
										PARG psSource,
										IMG_UINT32 uLiveChansInArg)
{
	if (psSource->uIndexType == USC_REGTYPE_NOINDEX)
	{
		/* Simple register */
		IncreaseRegisterLiveMask(psState, 
								 psLiveset, 
								 psSource->uType, 
								 psSource->uNumber, 
								 psSource->uArrayOffset, 
								 uLiveChansInArg);
	}
	else
	{
		/* Indexed register */
		IncreaseIndexedRegisterLiveMask(psState, psLiveset, psSource, uLiveChansInArg);

		/*
			Increase the live channels for the index used in the source.
		*/
		IncreaseIndexLiveMask(psState, psSource, psLiveset);
	}	
}

IMG_INTERNAL
IMG_UINT32 GetPreservedChansInPartialDest(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_UINT32			uDestIdx)
{
	ASSERT(uDestIdx < psInst->uDestCount);
	if (NoPredicate(psState, psInst))
	{
		return psInst->auLiveChansInDest[uDestIdx] & ~psInst->auDestMask[uDestIdx];
	}
	else
	{
		return psInst->auLiveChansInDest[uDestIdx];
	}
}

static
IMG_BOOL GetLiveChansInPredicateSrc(PINTERMEDIATE_STATE	psState,
									PINST				psInst,
									IMG_UINT32			uPredIdx,
									IMG_PUINT32			auLiveChansInDest)
/*********************************************************************************
 Function			: GetLiveChansInPredicateSrc

 Description		: Check if a predicate source to an instruction is controlling
					  the write of any data which is used later.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- Instruction to update.
					  uPredIdx			- Predicate source.
					  auLiveChansInDest	- Channels used from the destinations.

 Globals Effected	: None

 Return				: TRUE if the predicate source is used.
*********************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (GetBit(psInst->auFlag, INST_PRED_PERCHAN) != 0)
	{
		if (psInst->eOpcode == IVTEST)
		{
			PARG	psDestPred = &psInst->asDest[uPredIdx];

			/*
				Check if a predicate destination controlled by this predicate source is
				used.
			*/
			if (psDestPred->uType != USC_REGTYPE_UNUSEDDEST)
			{
				if (auLiveChansInDest[uPredIdx] != 0)
				{
					return IMG_TRUE;
				}
			}

			/*
				If the VTEST instruction is also writing a unified store destination then
				check if the channel controlled by this predicate is written and used
				later.
			*/
			if (psInst->uDestCount > VTEST_PREDICATE_ONLY_DEST_COUNT)
			{
				IMG_UINT32	uUSDestMask;

				uUSDestMask = ConcatenateVectorMask(psState,
													psInst, 
													VTEST_UNIFIEDSTORE_DESTIDX, 
													0 /* uVecDestIdx */, 
													auLiveChansInDest);
				if ((uUSDestMask & (1 << uPredIdx)) != 0)
				{
					return IMG_TRUE;
				}
			}
		}	
		else
		{
			IMG_UINT32	uDestMask;

			ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0);

			/*
				Check if the channel in the destination(s) controlled by this predicate is written
				and used later.
			*/
			uDestMask = ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, 0 /* uVecDestIdx */, auLiveChansInDest);
			if (psInst->eOpcode == IVDUAL)
			{
				uDestMask |= 
					ConcatenateVectorMask(psState, psInst, 0 /* uVecDestBase */, 1 /* uVecDestIdx */, auLiveChansInDest);
			}
			if ((uDestMask & (1 << uPredIdx)) != 0)
			{
				return IMG_TRUE;
			}
		}
		return IMG_FALSE;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PVR_UNREFERENCED_PARAMETER(psInst);
		PVR_UNREFERENCED_PARAMETER(auLiveChansInDest);
		ASSERT(uPredIdx == 0);
		return IMG_TRUE;
	}
}

IMG_INTERNAL IMG_BOOL IsPredicateSourceLive(PINTERMEDIATE_STATE psState, 
										    PINST psInst, 
										    IMG_UINT32 uArg)
/*********************************************************************************
 Function			: IsPredicateSourceLive

 Description		: Check if a predicate source to an instruction is controlling
					  the write of any data which is used later.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- Instruction to update.
					  uPredIdx			- Predicate source.

 Globals Effected	: None

 Return				: TRUE if the predicate source is used.
*********************************************************************************/
{
	IMG_UINT32	auLiveChansInDest[USC_MAX_NONCALL_DEST_COUNT];

	ASSERT(psInst->eOpcode != ICALL);
	GetLiveChansInDest(psState, psInst, auLiveChansInDest);
	return GetLiveChansInPredicateSrc(psState, psInst, uArg, auLiveChansInDest);
}

static IMG_BOOL ComputeLivenessForInst(PINTERMEDIATE_STATE psState, 
								PCODEBLOCK psCodeBlock, 
								PINST psInst, 
								PREGISTER_LIVESET psLiveset, 
								IMG_BOOL bRemoveNops)
/*********************************************************************************
 Function			: ComputeLivenessForInst

 Description		: Work out the change in the liveness of registers for an instruction.
 
 Parameters			: psState - Current compiler state.
					  psCodeBlock - Code block containing the instruction.
					  psInst - Instruction.
					  psLiveness - Liveness after the instruction.
					  bRemoveNop - Should instructions which don't update live registers be removed

 Globals Effected	: None

 Return				: IMG_TRUE if the instruction was dead.
*********************************************************************************/
{
	IMG_UINT32 uArgCount = psInst->uArgumentCount;
	IMG_BOOL bLive = IMG_FALSE;
	IMG_UINT32 uArg;
	IMG_UINT32 uDestIdx;
	IMG_UINT32 auLiveChansInDest[USC_MAX_NONCALL_DEST_COUNT];
	IMG_UINT32 uPred;
	
	ASSERT(psInst->eOpcode != ICALL);

	/* Mutex instructions update no registers but are always live */
	if ((psInst->eOpcode == ILOCK) || (psInst->eOpcode == IRELEASE))
		return IMG_FALSE;

	/* Remove empty moves (mov x x) */
	if (bRemoveNops)
	{
		if (psInst->eOpcode == IMOV &&
			EqualArgs(&(psInst->asDest[0]), &(psInst->asArg[0])))
		{
			RemoveInst(psState, psCodeBlock, psInst);
			FreeInst(psState, psInst);
			return IMG_TRUE;
		}
	}


	/*
	 * Test instruction for liveness
	 */

	/* Is the main destination live. */
	if (psInst->uDestCount > 0)
	{
		if (GetBit(psInst->auFlag, INST_DUMMY_FETCH))
		{
			ASSERT(psInst->uDestCount == 1);
			auLiveChansInDest[0] = USC_DESTMASK_FULL;
			bLive = IMG_TRUE;
		}
		else
		{
			ASSERT(psInst->uDestCount <= USC_MAX_NONCALL_DEST_COUNT);
			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				PARG	psDest = &psInst->asDest[uDestIdx];

				auLiveChansInDest[uDestIdx] = GetDestLiveMask(psState, psLiveset, psDest);

				/*
					Record whether any registers written by this instruction are live
					after it
				*/
				if	(auLiveChansInDest[uDestIdx] & psInst->auDestMask[uDestIdx])
				{
					bLive = IMG_TRUE;
				}
				psInst->auLiveChansInDest[uDestIdx] = auLiveChansInDest[uDestIdx];

				auLiveChansInDest[uDestIdx] &= GetDestMaskIdx(psInst, uDestIdx);
				if (psInst->eOpcode == ISOPWM && psInst->u.psSopWm->bRedChanFromAlpha)
				{
					/* if bRedChanFromAlpha, then only red channel should be live */
					psInst->auLiveChansInDest[uDestIdx] &= USC_RED_CHAN_MASK;
				}
			}
		}
	}

	/*
	 * Destination operand
	 */

	/* Does a write by the destination mean a register is no longer live. */
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psDest = &psInst->asDest[uDestIdx];

		/*
			For a destination where the register number is known we can't
			clear live channels for any register.
		*/
		if (psDest->uIndexType != USC_REGTYPE_NOINDEX)
		{
			continue;
		}

		SetRegisterLiveMask(psState, 
							psLiveset, 
							psDest->uType, 
							psDest->uNumber,
							psDest->uArrayOffset, 
							0 /* uNewDestMask */);	
	}

	/* If the instruction doesn't update a live register then remove it. */
	if (!bLive)
	{
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG		psOldDest = psInst->apsOldDest[uDestIdx];

			if (psOldDest != NULL)
			{
				IMG_UINT32	uLiveChansInOldDest;

				uLiveChansInOldDest = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);

				IncreaseLiveMaskForRead(psState, 
										psLiveset, 
										psOldDest, 
										uLiveChansInOldDest);
			}
		}	

		if (bRemoveNops)
		{
			CopyPartiallyOverwrittenData(psState, psInst);
			RemoveInst(psState, psCodeBlock, psInst);
			FreeInst(psState, psInst);
		}
		return IMG_TRUE;
	}

	/* Remove empty moves (mov x x) */
	if (bRemoveNops)
	{
		/* 
			If the instruction supports masked writes then reduce the mask as far as possible

			NB: USP pseudo-sample instructions have a write mask for each destination
				register, although to avoid confusion with normal maskable instructions,
				they are not flagged as supporting a write mask.
		*/
		ReduceInstDestMask(psState, psInst, auLiveChansInDest);
	}

	/*
	 * Source operands
	 */

	/*
		Increase the live channels for any index used in a destination.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if (psInst->asDest[uDestIdx].uIndexType != USC_REGTYPE_NOINDEX)
		{
			IncreaseIndexLiveMask(psState, &psInst->asDest[uDestIdx], psLiveset);
		}
	}

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psOldDest != NULL)
		{
			IMG_UINT32	uPassthroughMask;

			uPassthroughMask = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);
			if (uPassthroughMask == 0)
			{
				if (bRemoveNops)
				{
					SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psPartialDest */);
				}
			}
			else
			{
				IncreaseLiveMaskForRead(psState, psLiveset, psOldDest, uPassthroughMask);
			}
		}
	}

	/*
		Increase the live channels for any predicate used to control update of the
		destination(s).
	*/
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		if (psInst->apsPredSrc[uPred] != NULL)
		{
			if (GetLiveChansInPredicateSrc(psState, psInst, uPred, auLiveChansInDest) != 0)
			{
				IncreaseLiveMaskForRead(psState, psLiveset, psInst->apsPredSrc[uPred], USC_ALL_CHAN_MASK);
			}
		}
	}

	/* Which source registers does the instruction require. */
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		IMG_UINT32	uLiveChansInArg;
		PARG		psArg = &psInst->asArg[uArg];

		/*
			Translate channels used from the result to channels
			used from this source.
		*/
		uLiveChansInArg = 
			GetLiveChansInArgument(psState, 
								   psInst, 
								   uArg, 
								   auLiveChansInDest);

		if (uLiveChansInArg != 0)
		{
			/*
				Add the channels used from the result to the liveset.
			*/
			IncreaseLiveMaskForRead(psState, psLiveset, psArg, uLiveChansInArg);
		}
	}

	return IMG_FALSE;
}

static
IMG_VOID ComputeLivenessForBlockEnd(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PREGISTER_LIVESET psLiveSet)
/*********************************************************************************
 Function			: ComputeLivenessForBlockEnd

 Description		: Add any registers used by a block to choose its successor
					  to a set of live registers.
 
 Parameters			: psState			- Current compiler state.
					  psBlock			- Block to update for.
					  psLiveSet			- Set of live registers to update.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	switch (psBlock->eType)
	{
	case CBTYPE_UNCOND:
	case CBTYPE_EXIT:
		break;
	case CBTYPE_COND:
		//this copied/specialised from dce.c::UpdatePredicate
		if(psBlock->u.sCond.sPredSrc.uType != USC_REGTYPE_EXECPRED)
		{
			IncreaseLiveMaskForRead(psState, psLiveSet, &psBlock->u.sCond.sPredSrc, USC_DESTMASK_FULL);
		}
		break;
	case CBTYPE_SWITCH:
		/*
			Liveness gets done in lots of places/phases in the compiler;
			thus, we cannot rely on switches having been translated out yet.
		*/
		IncreaseLiveMaskForRead(psState, psLiveSet, psBlock->u.sSwitch.psArg, USC_DESTMASK_FULL);
		break;
	case CBTYPE_CONTINUE:
	case CBTYPE_UNDEFINED: //shouldn't occur post-construction!
		imgabort();
	}
}

IMG_INTERNAL IMG_VOID DeadCodeEliminationBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvRemoveNops)
/*********************************************************************************
 Function			: DeadCodeEliminationBlock

 Description		: Recompute liveness info within a single block,
		optionally removing instructions which arent used.
 
 Parameters			: psState - Current compiler state.
						psCodeBlock - Code block
						pvRemoveNops - an IMG_BOOL (cast, not pointer) as to whether
							dead instructions should be removed.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_BOOL bRemoveNops = (pvRemoveNops) ? IMG_TRUE : IMG_FALSE;
	PINST psInst, psPrevInst;
	PREGISTER_LIVESET psTempLiveness;

	psTempLiveness = AllocRegLiveSet(psState);

	CopyRegLiveSet(psState, &psBlock->sRegistersLiveOut, psTempLiveness);

	//successor test is before the end of the block
	ComputeLivenessForBlockEnd(psState, psBlock, psTempLiveness);
	
	if (IsCall(psState, psBlock))
	{
		/*
			probably being called by DeadCodeElimination, rather than anywhere else;
			other places generally call DoOnAllBasicBlocks with bHandlesCalls = IMG_FALSE
		*/
		if (bRemoveNops && psBlock->psBody->u.psCall->bDead)
		{
			PINST psRemove = psBlock->psBody;
			RemoveInst(psState, psBlock, psRemove);
			FreeInst(psState, psRemove);
		}
	}
	else
	{
		for (psInst = psBlock->psBodyTail; psInst; psInst = psPrevInst)
		{
			psPrevInst = psInst->psPrev;
			ComputeLivenessForInst(psState, psBlock, psInst, psTempLiveness, bRemoveNops);
		}
	}

	FreeRegLiveSet(psState, psTempLiveness);
}

//fwd decl
static IMG_BOOL DoLiveness(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET asFuncEndRegs, PFUNC psFunc, PREGISTER_LIVESET psLiveset);

typedef struct _LIVENESS_STATE_ {
	/*
		IMG_TRUE if at least some instruction in the function is thought to be live
		with respect to the current call site. (Note this unfortunately includes the
		case where said instruction merely sets a predicate used in the function body,
		even if the predicate only decides between, say, executing two different-but-
		-both-dead sequences of instructions. This is because it's too hard to decide
		whether such predicates need to be preserved (because they affect termination)
		or not, especially when we can't actually modify the function (removing conditionals
		- normally done only in DeadCodeElimination by multiple iterations including MergeBBs).
		This is, unfortunately, a slight step backward from USC 1, but we're hoping to
		compensate with better inlining (/heuristics) - TODO
	*/
	IMG_BOOL bFuncEntirelyDead;
	/* Array of register livesets to accumulate liveness at end of function */
	PREGISTER_LIVESET asFuncEndRegs;
} LIVENESS_STATE, *PLIVENESS_STATE;

IMG_INTERNAL
IMG_VOID ComputeRegistersLiveInSet(PINTERMEDIATE_STATE	psState, 
								   PCODEBLOCK			psBlock, 
								   PCODEBLOCK			psLiveInBlock)
/*********************************************************************************
 Function			: ComputeRegistersLiveInSet

 Description		: Get the set of registers live in to a block.
 
 Parameters			: psState			- Current compiler state.
					  psBlock			- Block to get the set of registers live in for.
					  psLiveInBlock		- The set of registers are copied to this block's
										set of registers live out.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINST				psInst;
	PREGISTER_LIVESET	psLiveInSet;

	ASSERT(!IsCall(psState, psBlock));
	
	psLiveInSet = &psLiveInBlock->sRegistersLiveOut;
	CopyRegLiveSet(psState, &psBlock->sRegistersLiveOut, psLiveInSet); 
	for (psInst = psBlock->psBodyTail; psInst; psInst = psInst->psPrev)
	{
		ComputeLivenessForInst(psState,	psBlock, psInst, psLiveInSet, IMG_FALSE /* bRemoveNops */);
	}

}

static IMG_BOOL LivenessDF(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvLiveset, IMG_PVOID* pArgs, IMG_PVOID pvLiveState)
/******************************************************************************
 Function			: LivenessDF

 Description		: DATAFLOW_FN for Liveness Analysis (does not make changes/do DCE).
						Pass to DoDataflow in backwards mode (any starting order - analysis
						iterates until convergence), using REGISTER_LIVESETs as dataflow values.

 Parameters			: psBlock - block to analyse (whose successor's values changed)
					  pvLiveset - old REGISTER_LIVESET value for this node
					  pArgs - pointer to array of PREGISTER_LIVESETS, being df values
						for the block's successors.
					  pvLiveState - PLIVENESS_STATE to function entry sets / state

 Globals Effected	: writes merge of successor values (i.e. liveness at CB exit) to
					  psBlock->sRegistersLiveOut; writes result (i.e. liveness at CB
					  entry) to pvLiveset; updates pvLiveState.

Return	: whether the dataflow value (in pvLiveset) has changed.
******************************************************************************/
{
	PREGISTER_LIVESET* apSuccLiveIn = (PREGISTER_LIVESET*)pArgs;
	PREGISTER_LIVESET psLiveset = (PREGISTER_LIVESET)pvLiveset;
	PREGISTER_LIVESET psBackup = AllocRegLiveSet(psState);
	IMG_UINT32 i;
	PLIVENESS_STATE psLiveState = (PLIVENESS_STATE)pvLiveState;
	IMG_BOOL bResult;
	
	CopyRegLiveSet(psState, psLiveset, psBackup);
#ifdef PRINT_LIVESETS
{
	printf("Computing liveset for block %i...currently ", psBlock->uIdx);
	SPrintLiveset(psState, psLiveset);
	printf("\n");
	for (i = 0; i < psBlock->uNumSuccs; i++)
	{
		printf("Got liveset from successor block %i as ", psBlock->asSuccs[i].psDest->uIdx);
		SPrintLiveset(psState, apSuccLiveIn[i]);
		printf("\n");
	}
}
#endif
	//1. merge successor live-on-entry's into this block's live-on-exit
	if (psBlock->uNumSuccs)
	{
		CopyRegLiveSet(psState, apSuccLiveIn[0], &psBlock->sRegistersLiveOut);
		for (i=1; i < psBlock->uNumSuccs; i++)
		{
			MergeLivesets(psState, &psBlock->sRegistersLiveOut, apSuccLiveIn[i]);
		}
	}
	else
	{
		ASSERT (psBlock == psBlock->psOwner->psExit);
		ASSERT (psBlock->eType == CBTYPE_EXIT);
		/*
			Initial value starts off in sRegistersLiveOut (already),
			and is never changed.
		*/
	}
	
	//2. Copy the array of registers so we can work on them.
	/*
		We'll use our result space as temporary storage,
		into which the result is gradually computed
	*/
	CopyRegLiveSet(psState, &psBlock->sRegistersLiveOut, psLiveset);
#ifdef PRINT_LIVESETS
{
	printf("Computed merge at bottom of block %i as ", psBlock->uIdx);
	SPrintLiveset(psState, psLiveset);
	printf("\n");
}
#endif
	//3a. First, incorporate any predicate/reg. required for choosing successor.
	ComputeLivenessForBlockEnd(psState, psBlock, psLiveset);
	
	//3b. Calculate liveness information for each instruction in the basic block.
	if (IsCall(psState, psBlock))
	{
		PINST				psCallInst = psBlock->psBody;
		PFUNC				psTarget = psCallInst->u.psCall->psTarget;
		IMG_UINT32			uParamIdx;
		PREGISTER_LIVESET	psBackupCall;
		IMG_BOOL			bUsedOutputs;
		IMG_BOOL			bNoUsedInsts;
		
		//note no state for SA prog, as should contain no calls...
		ASSERT (psLiveState->asFuncEndRegs);
		ASSERT (NoPredicate(psState, psBlock->psBody));

		/*
			Translate call result registers to the register numbers used in the
			function.
		*/
		bUsedOutputs = IMG_FALSE;
		for (uParamIdx = 0; uParamIdx < psCallInst->uDestCount; uParamIdx++)
		{
			IMG_UINT32	uLiveChansInResult;
			PARG		psDest = &psCallInst->asDest[uParamIdx];
			PFUNC_INOUT	psOut = &psTarget->sOut.asArray[uParamIdx];

			uLiveChansInResult = GetRegisterLiveMask(psState,
													 psLiveset,
													 psDest->uType,
													 psDest->uNumber,
													 psDest->uArrayOffset);
			if (uLiveChansInResult != 0)
			{
				bUsedOutputs = IMG_TRUE;
			}

			SetRegisterLiveMask(psState,
								psLiveset,
								psDest->uType,
								psDest->uNumber,
								psDest->uArrayOffset,
								0 /* uMask */);
			IncreaseRegisterLiveMask(psState,
									 psLiveset,
									 psOut->uType,
									 psOut->uNumber,
									 0 /* uArrayOffset */,
									 uLiveChansInResult);
		}

		//backup the state at the end of the call...
		psBackupCall = AllocRegLiveSet(psState);
		CopyRegLiveSet(psState, psLiveset, psBackupCall);

		bNoUsedInsts = 
			/*
				store whether the call is dead (writes no live registers) or not.
				After all calls to DoLiveness (on outermost fn first, but including nested calls),
				the value remaining in each psCall->bDead will be that from the final call to
				DoLiveness on the containing function, i.e. that done directly by DeadCodeElimination
				(in inverse nesting order) taking the merge of all liveness info into account.
			*/
			DoLiveness(psState, psLiveState->asFuncEndRegs, psTarget, psLiveset);

		if (!bUsedOutputs && bNoUsedInsts)
		{
			psBlock->psBody->u.psCall->bDead = IMG_TRUE;
		}
		else
		{
			psBlock->psBody->u.psCall->bDead = IMG_FALSE;
		}
			
		if (!psBlock->psBody->u.psCall->bDead)
		{
			/*
				Call to be retained - thus,
				Accumulate the channels that were live at the end of the function. 
			*/
			MergeLivesets(psState,
					  &psLiveState->asFuncEndRegs[psTarget->uLabel], 
					  psBackupCall);

			/*
				Function containing a call is live.
			*/
			psLiveState->bFuncEntirelyDead = IMG_FALSE;
		}

		/*	
			Translate call input registers to the register numbers used in the calle.
		*/
		for (uParamIdx = 0; uParamIdx < psCallInst->uArgumentCount; uParamIdx++)
		{
			IMG_UINT32	uLiveChansInInput;
			PARG		psSrc = &psCallInst->asArg[uParamIdx];
			PFUNC_INOUT	psIn = &psTarget->sIn.asArray[uParamIdx];

			uLiveChansInInput = GetRegisterLiveMask(psState,
													psLiveset,
													psIn->uType,
													psIn->uNumber,
													0 /* uArrayOffset */);
			SetRegisterLiveMask(psState,
								psLiveset,
								psIn->uType,
								psIn->uNumber,
								0 /* uArrayOffset */,
								0 /* uMask */);
			IncreaseLiveMaskForRead(psState, psLiveset, psSrc, uLiveChansInInput);
		}
		
		//~Note commented out code for dealing with predicated calls:
		//~MergeLivesets(psState, psLiveset, psBackupCall);
		//~/* Add in any channels made live by the predicate used to guard the call. */
		//~UpdatePredicate(psState, psLiveset, psBlock->u.sCall.uPredSrc, NULL);

		FreeRegLiveSet(psState, psBackupCall);
	}
	else
	{
		PINST psInst, psPrevInst;
		for (psInst = psBlock->psBodyTail; psInst; psInst = psPrevInst)
		{
			psPrevInst = psInst->psPrev;

			if (!ComputeLivenessForInst(psState,
										psBlock,
										psInst,
										psLiveset,
										IMG_FALSE/*don't remove NOPs yet*/))
			{
				//instr live
				psLiveState->bFuncEntirelyDead = IMG_FALSE;
			}

			#ifdef PRINT_LIVESETS
			{
				printf("Computing liveset for block %i after %s...currently ", psBlock->uIdx, g_psInstDesc[psInst->eOpcode].pszName);
				SPrintLiveset(psState, psLiveset);
				printf("\n");
			}
			#endif /* PRINT_LIVESETS */
		}
	}

#ifdef DEBUG
	//4. There shouldn't be any IReg's live at the boundary of the basic block.
	for (i = 0; i < psState->uGPISizeInScalarRegs; i++)
	{
		ASSERT(GetRegisterLiveMask(psState, 
								   psLiveset, 
								   USEASM_REGTYPE_FPINTERNAL, 
								   i, 
								   0) == 0);
	}
#endif

	//that leaves liveness at entry to the block in psResult, as required.
	bResult = CompareLivesets(psState, psLiveset, psBackup) ? IMG_FALSE : IMG_TRUE;
	FreeRegLiveSet(psState, psBackup);
#ifdef PRINT_LIVESETS
{
	printf("Liveset for block %i computed as ", psBlock->uIdx);
	SPrintLiveset(psState, psLiveset);
	printf(" - %s\n\n", bResult ? "different" : "same");
}
#endif
	return bResult;
}

/* Return TRUE if the entire function body is dead (i.e. does not write to any registers in the initial psLiveset) */
static IMG_BOOL DoLiveness(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET asFuncEndRegs, PFUNC psFunc , PREGISTER_LIVESET psLiveset)
{
	LIVENESS_STATE sLiveState;
	PREGISTER_LIVESET asLivesets = UscAlloc(psState, sizeof(asLivesets[0]) * psFunc->sCfg.uNumBlocks);
	IMG_UINT32 i;

	sLiveState.bFuncEntirelyDead = IMG_TRUE; //unless we find a live instr somewhere
	sLiveState.asFuncEndRegs = asFuncEndRegs;

	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		InitRegLiveSet(&asLivesets[i]);

	//setup "initial" value for live out of exit (never overwritten)
	CopyRegLiveSet(psState, psLiveset, &psFunc->sCfg.psExit->sRegistersLiveOut);

	DoDataflow(psState, psFunc, IMG_FALSE, sizeof(REGISTER_LIVESET),
			   asLivesets, LivenessDF, &sLiveState);

	//update psLiveset with liveness at entry to CFG
	CopyRegLiveSet(psState, &asLivesets[psFunc->sCfg.psEntry->uIdx], psLiveset);

	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		ClearRegLiveSet(psState, &asLivesets[i]);
	UscFree(psState, asLivesets);
	
	return sLiveState.bFuncEntirelyDead;
}

IMG_INTERNAL
IMG_VOID DoSALiveness(PINTERMEDIATE_STATE psState)
{
	PREGISTER_LIVESET asLivesets;
	IMG_UINT32 i;
	if (psState->psSecAttrProg == NULL) return;

	asLivesets = UscAlloc(psState, sizeof(asLivesets[0]) * psState->psSecAttrProg->sCfg.uNumBlocks);
	for (i = 0; i < psState->psSecAttrProg->sCfg.uNumBlocks; i++)
		InitRegLiveSet(&asLivesets[i]);

	CopyRegLiveSet(psState, &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut, &asLivesets[psState->psSecAttrProg->sCfg.psExit->uIdx]);

	DoDataflow(psState, psState->psSecAttrProg, IMG_FALSE, sizeof(REGISTER_LIVESET),
			   asLivesets, LivenessDF, NULL/*No DCE State - only needed for CALLs and there aren't any*/);

	//no need to keep result.

	for (i = 0; i < psState->psSecAttrProg->sCfg.uNumBlocks; i++)
		ClearRegLiveSet(psState, &asLivesets[i]);
	UscFree(psState, asLivesets);
}

static IMG_VOID GetRegistersLiveAtEnd(PINTERMEDIATE_STATE psState, PREGISTER_LIVESET psLiveset)
/*********************************************************************************
 Function			: GetRegistersLiveAtEnd

 Description		: Initialize the set of registers live at the end of the program.
 
 Parameters			: psState			- Current compiler state.
					  psLiveset		- Updated with the set of registers.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psState->sFixedRegList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFIXED_REG_DATA psOut = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);
		IMG_UINT32		uConsecutiveRegIdx;
		if (psOut->bLiveAtShaderEnd == IMG_TRUE)
		{
			for (uConsecutiveRegIdx = 0; uConsecutiveRegIdx < psOut->uConsecutiveRegsCount; uConsecutiveRegIdx++)
			{
				if (!(psOut->aeUsedForFeedback != NULL && psOut->aeUsedForFeedback[uConsecutiveRegIdx] == FEEDBACK_USE_TYPE_ONLY))
				{
					IMG_UINT32	uMask;

					if (psOut->auMask != NULL)
					{
						uMask = psOut->auMask[uConsecutiveRegIdx];
					}
					else
					{
						uMask = USC_ALL_CHAN_MASK;
					}
	
					IncreaseRegisterLiveMask(psState,
								 			 psLiveset,
											 psOut->uVRegType,
											 psOut->auVRegNum[uConsecutiveRegIdx],
											 0 /* uArrayOffset */,
											 uMask);
				}
			}
		}
	}
}

IMG_INTERNAL IMG_VOID DeadCodeElimination(PINTERMEDIATE_STATE psState, IMG_BOOL bFreeBlocks)
/*********************************************************************************
 Function			: DeadCodeElimination

 Description		: Remove unused instructions from the program.
 
 Parameters			: psState			- Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 i;
	PUSC_LIST_ENTRY psListEntry;
	PFUNC psFunc, psNextFunc;
	IMG_BOOL bRepeat;
	PREGISTER_LIVESET asFuncEndRegistersLive = UscAlloc(psState, 
												  (psState->uMaxLabel * sizeof(REGISTER_LIVESET)));
	PREGISTER_LIVESET psMainEndRegistersLive;

	/*
		Allocate storage needed for this stage.
	*/
	for (i = 0; i < psState->uMaxLabel; i++)
	{
		InitRegLiveSet(&asFuncEndRegistersLive[i]);
	}

	for (;;)
	{
		bRepeat = IMG_FALSE;	
		GetRegistersLiveAtEnd(psState, &asFuncEndRegistersLive[psState->psMainProg->uLabel]);

		/* Scan everything, outermost first (so functions have info from union-of-callers ready). */
		for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
		{
			if (psFunc == psState->psSecAttrProg) continue;
			
			/* Set link register as live for non entry functions */
			if(psState->psMainProg != psFunc)
			{
				SetRegisterLiveMask(psState, &asFuncEndRegistersLive[psFunc->uLabel], 
					USEASM_REGTYPE_LINK, 0, 0, USC_DESTMASK_FULL);
			}

			DoLiveness(psState, asFuncEndRegistersLive, psFunc, &asFuncEndRegistersLive[psFunc->uLabel]/*overwritten...*/);
			CopyRegLiveSet(psState, &asFuncEndRegistersLive[psFunc->uLabel]/*...then read here...*/, &psFunc->sCallStartRegistersLive);
		}
		/*
			That just calculated sRegistersLiveOut everywhere. Use that to delete things,
			innermost first - so any empty functions can remove their CALLs (in outer fns!).
		*/
		for (psFunc = psState->psFnInnermost; psFunc; psFunc = psNextFunc)
		{
			psNextFunc = psFunc->psFnNestOuter; //in case function becomes empty and is deleted
			DoOnCfgBasicBlocks(psState, &(psFunc->sCfg), ANY_ORDER, DeadCodeEliminationBP, IMG_TRUE/*CALLs w/ bDead*/, (IMG_PVOID)(IMG_UINTPTR_T)IMG_TRUE);
			if (bFreeBlocks) bRepeat |= MergeBasicBlocks(psState, psFunc);
			//hmmm. Should we ever NOT call MergeBBs? Could still end up with empty blocks either way???
		}
		/* See if that's caused us to reduce the number of calls to any function to 1 - if so, inline it! */
		if (bFreeBlocks) //???
		{
			for (psFunc = psState->psFnInnermost; psFunc; psFunc = psNextFunc)
			{
				psNextFunc = psFunc->psFnNestOuter;

				//skip externally-callable functions (includes main)
				if (psFunc->pchEntryPointDesc) continue;

				ASSERT (psFunc->psCallSiteHead);
				if (psFunc->psCallSiteHead->u.psCall->psCallSiteNext == NULL)
				{
					//function has only a single CALL
					PFUNC psInlineInto = psFunc->psCallSiteHead->u.psCall->psBlock->psOwner->psFunc;
					//Inline! (This also deletes the (now block-less) FUNC structure.)
					InlineFunction(psState, psFunc->psCallSiteHead);
					/*
						Note that with only one active CALL, the function body
						has already been specialised to its new context (i.e.
						the "merge" across said unique remaining CALL site).
						Thus, the inlining should not cause us to need another
						pass of DCE.
					*/
					MergeBasicBlocks(psState, psInlineInto);
				}
			}
		}
		if (bRepeat)
		{
			/*
				Repeating because of block merging. Specifically, this might have removed
				a test of a predicate (at the end of what was once a conditional block,
				now removed), making the predicate - and the instruction computing it - dead.
				Thus, we want to recompute liveness (the iterative analysis), in this function
				and (potentially) anything it calls...hence, redo the lot.
			*/
			for (i = 0; i < psState->uMaxLabel; i++)
			{
				ClearRegLiveSet(psState, &asFuncEndRegistersLive[i]/*...and cleared here*/);
			}
		}
		else break;
	}

	psMainEndRegistersLive = &asFuncEndRegistersLive[psState->psMainProg->uLabel];
	for (psListEntry = psState->sFixedRegList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFIXED_REG_DATA psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

		if (psFixedReg->puUsedChans != NULL)
		{
			IMG_UINT32	uRegIdx;

			for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
			{
				IMG_UINT32	uLiveMask;
				IMG_UINT32	uVRegNum = psFixedReg->auVRegNum[uRegIdx];

				uLiveMask = 
					GetRegisterLiveMask(psState, psMainEndRegistersLive, psFixedReg->uVRegType, uVRegNum, 0);
				SetRegMask(psFixedReg->puUsedChans, uRegIdx, uLiveMask);
			}
		}
	}

	for (i = 0; i < psState->uMaxLabel; i ++)
	{
		ClearRegLiveSet(psState, &asFuncEndRegistersLive[i]);
	}
	UscFree(psState, asFuncEndRegistersLive);
}

#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
/*********************************************************************************
 Function			: ComputeInstRegsInitialisd

 Description		: Record the channels currently thought to be initialised in
					  each destination before the instruction writes to it; update
					  the set of initialised registers to reflect the write.
 
 Parameters			: psState			- Current compiler state.
					  psInst			- The instruciton to examine
					  psInitialisedRegs	- The set of data (currently thought to be)
						initialised prior to the instruction. (Note that if this is
						incorrect, the wrong flags will be set in the instruction,
						but the same changes are applied to the set regardless, so
						a later call with correct psInitialisedRegs will set the
						flags right, just as if the earlier call did not happen).

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID	ComputeInstRegsInitialisd(PINTERMEDIATE_STATE	psState, 
										  PINST					psInst,
										  PREGISTER_LIVESET		psInitialisedRegs)
{
	IMG_UINT32	uDestOffset;
	IMG_UINT32	uDestMask;

	#if defined(OUTPUT_USPBIN)
	if	(psInst->eOpcode == ISMPUNPACK_USP)
	{
		for (uDestOffset = 0; uDestOffset < psInst->uDestCount; uDestOffset++)
		{
			IMG_UINT32	uInitialisedChans;

			/*
				Record whether this destination has been updated prior to 
				the instruction
			*/
			if (psInst->asDest[uDestOffset].uIndexType != USC_REGTYPE_NOINDEX)
			{
				uInitialisedChans = GetIndexedRegisterLiveMask(	psState, 
																psInitialisedRegs, 
																&psInst->asDest[uDestOffset]);
			}
			else
			{
				uInitialisedChans = GetRegisterLiveMask(psState, 
														psInitialisedRegs, 
														psInst->asDest[uDestOffset].uType, 
														psInst->asDest[uDestOffset].uNumber, 
														0);
			}
			psInst->u.psSmpUnpack->auInitialisedChansInDest[uDestOffset] = uInitialisedChans;
		}
	}
	else
	#endif /* defined(OUTPUT_USPBIN) */
	if (psInst->uDestCount > 0)
	{
		/*
			Record whether the destination has been updated prior to this instruction
		*/
		if (psInst->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
		{
			psInst->uInitialisedChansInDest = GetIndexedRegisterLiveMask(psState,
																		 psInitialisedRegs,
																		 &psInst->asDest[0]);
		}
		else
		{
			psInst->uInitialisedChansInDest = GetRegisterLiveMask(psState, 
																  psInitialisedRegs,
																  psInst->asDest[0].uType,
																  psInst->asDest[0].uNumber,
																  psInst->asDest[0].uArrayOffset);
		}
	}
	else
	{
		psInst->uInitialisedChansInDest = USC_XYZW_CHAN_MASK;
	}

	/*
		Record the register components written
	*/
	for (uDestOffset = 0; uDestOffset < psInst->uDestCount; uDestOffset++)
	{
		uDestMask = psInst->auDestMask[uDestOffset];

		if	(psInst->asDest[uDestOffset].uIndexType != USC_REGTYPE_NOINDEX)
		{
			IncreaseIndexedRegisterLiveMask(psState,
										    psInitialisedRegs,
											&psInst->asDest[uDestOffset],
											uDestMask);
		}
		else
		{
			IncreaseRegisterLiveMask(psState, 
									 psInitialisedRegs,
									 psInst->asDest[uDestOffset].uType,
									 psInst->asDest[uDestOffset].uNumber,
									 psInst->asDest[uDestOffset].uArrayOffset,
									 uDestMask);
		}
	}
}

typedef struct
{
	/*
		Register set for RegsInitialisedBP to use on instructions; used as a
		temporary working area by RegsInitialisedDF.
	*/
	REGISTER_LIVESET sRegInitSet;

	/*Sequence of register sets into which to accumulate sets at start of each fn*/
	PREGISTER_LIVESET asFuncStartRegs;
} REG_INIT_STATE, *PREG_INIT_STATE;

static IMG_VOID RegsInitialisedBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock,
								  IMG_PVOID pvRegState)
/******************************************************************************
 FUNCTION:		RegsInitialisedBP

 DESCRIPTION:	Sets initialised flags on, and computes registers initialised by,
				each instruction in a basic block

 PARAMETERS:	pvRegState - a PREG_INIT_STATE containing the initialised set to
					apply to (and mutate according to) the block, as well as to
					accumulate initialised sets for each CALL.
******************************************************************************/
{
	PREG_INIT_STATE psRegState = (PREG_INIT_STATE)pvRegState;
	PINST psInst;
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		if (psInst->eOpcode == ICALL)
		{
			/* Accumulate the initialised registers at the start of a function... */
			MergeLivesets(psState, &psRegState->asFuncStartRegs[psInst->u.psCall->psTarget->uLabel], &psRegState->sRegInitSet);
			/*
				In the meantime, a single pass will correctly compute the registers
				initialised *after* the CALL (even tho it'll also set the wrong flags
				inside the function - that'll be corrected by a later iterative pass
				using the above set merged from all CALLs), into psRegState->sRegInitSet
				(also used as the initial value).
			*/
			DoOnCfgBasicBlocks(psState, &(psInst->u.psCall->psTarget->sCfg), ANY_ORDER, RegsInitialisedBP, IMG_TRUE, psRegState);
		}
		else
		{
			ComputeInstRegsInitialisd(psState, psInst, &psRegState->sRegInitSet);
		}
	}
}

/*********************************************************************************
 Function			: RegsInitialisedDF

 Description		: Determines whether destination registers may have been
					  written by the shader prior to each instruction, for a
					  sequence of blocks.
 
 Parameters			: pvRegState - PREG_INIT_STATE to traversal state.
*********************************************************************************/
static IMG_BOOL RegsInitialisedDF(PINTERMEDIATE_STATE	psState,
										PCODEBLOCK psBlock,
										IMG_PVOID pvDest,
										IMG_PVOID* ppvPreds,
										IMG_PVOID pvRegState)
{
	PREG_INIT_STATE psRegState = (PREG_INIT_STATE)pvRegState;
	PREGISTER_LIVESET psDest = (PREGISTER_LIVESET)pvDest;
	PREGISTER_LIVESET *ppsPreds = (PREGISTER_LIVESET*)ppvPreds;
	IMG_UINT32 uPred;
	
	//1. merge values from predecessors to get block-start state in psRegState->sRegInitSet
	if (psBlock == psBlock->psOwner->psEntry)
	{
		CopyRegLiveSet(psState, psDest, &psRegState->sRegInitSet);
	}
	for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
	{
		if (uPred==0 && psBlock != psBlock->psOwner->psEntry)
		{
			CopyRegLiveSet(psState, ppsPreds[uPred], &psRegState->sRegInitSet);
		}
		else
		{
			MergeLivesets(psState, &psRegState->sRegInitSet, ppsPreds[uPred]);
		}
	}
	
	//2. process body
	RegsInitialisedBP(psState, psBlock,  psRegState);
	
	//that leaves the dataflow value at end of block in psRegState->sRegInitState. Check if it's changed...
	if (CompareLivesets(psState, &psRegState->sRegInitSet, psDest)) return IMG_FALSE;
	
	//value changed. Update dataflow-value array and continue analysis
	CopyRegLiveSet(psState, &psRegState->sRegInitSet, psDest);
	return IMG_TRUE;
}

static IMG_VOID DoRegsInitialised(PINTERMEDIATE_STATE psState, PREG_INIT_STATE psRegState,
								  PFUNC psFunc, PREGISTER_LIVESET psInitial)
{
	PREGISTER_LIVESET asLivesets = UscAlloc(psState, sizeof(asLivesets[0]) * psFunc->sCfg.uNumBlocks);
	IMG_UINT32 i;
	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		InitRegLiveSet(&asLivesets[i]);

	CopyRegLiveSet(psState, psInitial, &asLivesets[psFunc->sCfg.psEntry->uIdx]);

	DoDataflow(psState, psFunc, IMG_TRUE /*forwards*/,
			   sizeof(REGISTER_LIVESET), asLivesets, RegsInitialisedDF, psRegState);

	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++)
		ClearRegLiveSet(psState, &asLivesets[i]);
	UscFree(psState, asLivesets);
}

/*********************************************************************************
 Function			: ComputeShaderRegsInitialised

 Description		: Determines whether destination registers may have been
					  written by the shader prior to each instruction, for the
					  entire shader.
 
 Parameters			: psState		- Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL IMG_VOID ComputeShaderRegsInitialised(PINTERMEDIATE_STATE psState)
{
	REG_INIT_STATE sRegState;
	PFUNC psFunc;
	IMG_UINT32 i;
	
	InitRegLiveSet(&sRegState.sRegInitSet);
	sRegState.asFuncStartRegs = UscAlloc(psState, 
													 psState->uMaxLabel * sizeof(REGISTER_LIVESET));
	
	for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
	{
		InitRegLiveSet(&sRegState.asFuncStartRegs[psFunc->uLabel]);
	}

	/*
		Initialise the set of data that is defined prior to the start
		of the program.

		NB: For simplicity, we currently assume all components of every
			PA register used for input-data has been initialised.
	*/
	
	for	(i = 0; i < psState->sHWRegs.uNumPrimaryAttributes; i++)
	{
		IncreaseRegisterLiveMask(psState, 
								 &sRegState.asFuncStartRegs[psState->psMainProg->uLabel],
								 USEASM_REGTYPE_PRIMATTR,
								 i,
								 0,
								 USC_DESTMASK_FULL);
	}

	/*
		Assume all registers that contain the value of constants are initialized before
		the secondary loading program.
	*/
	for	(i = 0; i < psState->sSAProg.uInRegisterConstantCount; i++)
	{
		IncreaseRegisterLiveMask(psState, 
								 &sRegState.asFuncStartRegs[psState->psSecAttrProg->uLabel], 
								 USEASM_REGTYPE_PRIMATTR,
								 psState->psSAOffsets->uInRegisterConstantOffset + i,
								 0,
								 USC_DESTMASK_FULL);
	}

	/*
		Determine the data potentially initialised prior to each instruction
		in each function, including the main and secondary update programs,
		and record those channels for each destination register.
	*/
	for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
	{
		DoRegsInitialised(psState, &sRegState, psFunc,
			&sRegState.asFuncStartRegs[psFunc->uLabel]);
	}

	/* Free all resources */
	ClearRegLiveSet(psState, &sRegState.sRegInitSet);
	for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
	{
		ClearRegLiveSet(psState, &sRegState.asFuncStartRegs[psFunc->uLabel]);
	}
	
	UscFree(psState, sRegState.asFuncStartRegs);
}
#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

static
IMG_VOID InitRegisterTypeUseDef(PREGISTER_TYPE_USEDEF psRegTypeUseDef)
/*********************************************************************************
 Function		: InitRegisterTypeUseDef

 Description	: Initialise a register use-def record for a single register type.
 
 Parameters		: psRegTypeUseDef	- The use/def record to initialise

 Return			: Nothing
*********************************************************************************/
{
	InitVector(&psRegTypeUseDef->sChans, USC_MIN_VECTOR_CHUNK, IMG_FALSE /* bDefault */);
	InitVector(&psRegTypeUseDef->sC10, USC_MIN_VECTOR_CHUNK, IMG_FALSE /* bDefault */);
	InitVector(&psRegTypeUseDef->sNonC10, USC_MIN_VECTOR_CHUNK, IMG_FALSE /* bDefault */);
}

IMG_INTERNAL
IMG_VOID InitRegUseDef(PREGISTER_USEDEF psUseDef)
/*********************************************************************************
 Function		: InitRegUseDef

 Description	: Initialise a register use-def record.
 
 Parameters		: psUseDef	- The use/def record to initialise

 Return			: Nothing
*********************************************************************************/
{
	InitRegisterTypeUseDef(&psUseDef->sTemp);
	InitRegisterTypeUseDef(&psUseDef->sPrimAttr);
	InitRegisterTypeUseDef(&psUseDef->sOutput);
	InitRegisterTypeUseDef(&psUseDef->sIndex);
	InitRegisterTypeUseDef(&psUseDef->sInternal);
	InitVector(&psUseDef->sPredicate, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	psUseDef->bLocalMemory = IMG_FALSE;
	psUseDef->bInternalRegsClobbered = IMG_FALSE;
}

static
IMG_VOID ClearRegisterTypeUseDef(PINTERMEDIATE_STATE psState, PREGISTER_TYPE_USEDEF psRegTypeUseDef)
/*********************************************************************************
 Function		: ClearRegisterTypeUseDef

 Description	: Clear a register use-def record for a single register type.
 
 Parameters		: psState			- Compiler state.
				  psRegTypeUseDef	- The use/def record to initialise

 Return			: Nothing
*********************************************************************************/
{
	ClearVector(psState, &psRegTypeUseDef->sChans);
	ClearVector(psState, &psRegTypeUseDef->sC10);
	ClearVector(psState, &psRegTypeUseDef->sNonC10);
}

IMG_INTERNAL
IMG_VOID ClearRegUseDef(PINTERMEDIATE_STATE psState,
						PREGISTER_USEDEF psUseDef)
/*********************************************************************************
 Function		: ClearRegUseDef

 Description	: Clear a register use-def record.
 
 Parameters		: psState	- Compiler state
				  psUseDef	- The use/def record to clear

 Return			: Nothing
*********************************************************************************/
{
	ClearRegisterTypeUseDef(psState, &psUseDef->sTemp);
	ClearRegisterTypeUseDef(psState, &psUseDef->sOutput);
	ClearRegisterTypeUseDef(psState, &psUseDef->sPrimAttr);
	ClearRegisterTypeUseDef(psState, &psUseDef->sIndex);
	ClearRegisterTypeUseDef(psState, &psUseDef->sInternal);
	ClearVector(psState, &psUseDef->sPredicate);
	psUseDef->bLocalMemory = IMG_FALSE;
	psUseDef->bInternalRegsClobbered = IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID InitUseDef(PUSEDEF_RECORD psUseDef)
/*********************************************************************************
 Function		: InitUseDef

 Description	: Initialise a use-def record.
 
 Parameters		: psUseDef	- The use/def record to initialise

 Return			: Nothing
*********************************************************************************/
{
	InitRegUseDef(&psUseDef->sUse);
	InitRegUseDef(&psUseDef->sDef);
}

IMG_INTERNAL
IMG_VOID ClearUseDef(PINTERMEDIATE_STATE psState,
					 PUSEDEF_RECORD psUseDef)
/*********************************************************************************
 Function		: ClearUseDef

 Description	: Clear a use-def record.
 
 Parameters		: psState	- Compiler state
				  psUseDef	- The use/def record to Clear

 Return			: Nothing
*********************************************************************************/
{
	ClearRegUseDef(psState, &psUseDef->sUse);
	ClearRegUseDef(psState, &psUseDef->sDef);
}


IMG_INTERNAL
IMG_UINT32 GetRegUseDef(PINTERMEDIATE_STATE psState, 
						PREGISTER_USEDEF psUseDef,
						IMG_UINT32 uRegType,
						IMG_UINT32 uRegNum)
/*********************************************************************************
 Function			: GetRegUseDef

 Description		: Test whether a register is in a use/def record.
 
 Parameters			: psState	- Internal compiler state
					  psUseDef	- The use/def record to look in.
					  uRegType	- Type of the register to look for.
					  uRegNum	- Number of the register to look for
					  
 Return				: IMG_TRUE if present, IMG_FALSE if not.
*********************************************************************************/
{
	IMG_UINT32 uStart = uRegNum * CHANS_PER_REGISTER;

	/* Set the value in the appropriate field */
	switch (uRegType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			return VectorGetRange(psState, &psUseDef->sTemp.sChans, 
								  uStart + CHANS_PER_REGISTER - 1, uStart); 
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			return VectorGetRange(psState, &psUseDef->sOutput.sChans, 
								  uStart + CHANS_PER_REGISTER - 1, uStart); 
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			return VectorGetRange(psState, &psUseDef->sPrimAttr.sChans,
								  uStart + CHANS_PER_REGISTER - 1, uStart); 
		}
		case USEASM_REGTYPE_INDEX:
		{
			return VectorGetRange(psState, &psUseDef->sIndex.sChans,
								  uStart + CHANS_PER_REGISTER - 1, uStart); 
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			return VectorGetRange(psState, &psUseDef->sInternal.sChans,
								  uStart + CHANS_PER_REGISTER - 1, uStart); 
		}
		case USEASM_REGTYPE_PREDICATE: 
		{
			return VectorGet(psState, &psUseDef->sPredicate, uRegNum);
		}
		case USC_REGTYPE_REGARRAY: 
		{
			IMG_UINT32 uBaseReg, uNumRegs, uIdx, uRet;
			
			/* Vector register array must be specified. */
			ASSERT(uRegNum < psState->uNumVecArrayRegs);
			ASSERT(psState->apsVecArrayReg != NULL);
			ASSERT(psState->apsVecArrayReg[uRegNum] != NULL);

			/* Take the union of all registers in the array */
			uRegType = psState->apsVecArrayReg[uRegNum]->uRegType;
			uBaseReg = psState->apsVecArrayReg[uRegNum]->uBaseReg;
			uNumRegs = psState->apsVecArrayReg[uRegNum]->uRegs;
			uRet = 0;
			for (uIdx = 0; uIdx < uNumRegs; uIdx ++)
			{
				uRet |= GetRegUseDef(psState, psUseDef, uRegType, uBaseReg + uIdx);

				/* Stop when there's no point in going on */
				if (uRet == USC_DESTMASK_FULL)
					return USC_DESTMASK_FULL;
			}

			return uRet;
		}
		default: 
		{	
			return 0;
		}
	}
}

static 
IMG_VOID IncreaseRegTypeUseDef(PINTERMEDIATE_STATE		psState, 
							   PREGISTER_TYPE_USEDEF	psUseDef, 
							   IMG_UINT32				uRegNum,
							   IMG_UINT32				uMask,
							   UF_REGFORMAT				eFmt)
/*********************************************************************************
 Function			: IncreaseRegTypeUseDef

 Description		: Add register channels in a use/def record.
 
 Parameters			: psState	- Internal compiler state
					  psUseDef	- The storage for registers of the same type.
					  uRegNum	- The register number.
					  uMask		- Mask of the register channels to add.
					  eFmt		- Format of the register.

 Output				: psUseDef	- The use/def record.

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uStart = uRegNum * CHANS_PER_REGISTER;
	VectorOrRange(psState, &psUseDef->sChans, uStart + CHANS_PER_REGISTER - 1, uStart, uMask);
	if (eFmt == UF_REGFORMAT_C10)
	{
		VectorSet(psState, &psUseDef->sC10, uRegNum, 1 /* uData */);
	}
	else
	{
		VectorSet(psState, &psUseDef->sNonC10, uRegNum, 1 /* uData */);
	}
}

static
IMG_VOID IncreaseRegUseDef(PINTERMEDIATE_STATE psState, 
						   PREGISTER_USEDEF psUseDef, 
						   IMG_UINT32 uRegType,
						   IMG_UINT32 uRegNum,
						   IMG_UINT32 uMask,
						   UF_REGFORMAT eFmt)
/*********************************************************************************
 Function			: IncreaseRegUseDef

 Description		: Add register channels in a use/def record.
 
 Parameters			: psState	- Internal compiler state
					  psReg		- The register.
					  uMask		- Mask of the register channels to add/remove.
					  eFmt		- Format of the register.

 Output				: psUseDef	- The use/def record.

 Return				: Nothing.
*********************************************************************************/
{
	/* Set the mask in the appropriate field */
	switch (uRegType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			IncreaseRegTypeUseDef(psState, &psUseDef->sTemp, uRegNum, uMask, eFmt);
			break;
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			IncreaseRegTypeUseDef(psState, &psUseDef->sOutput, uRegNum, uMask, eFmt);
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			IncreaseRegTypeUseDef(psState, &psUseDef->sPrimAttr, uRegNum, uMask, eFmt);
			break;
		}
		case USEASM_REGTYPE_INDEX:
		{
			IncreaseRegTypeUseDef(psState, &psUseDef->sIndex, uRegNum, uMask, eFmt);
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			IncreaseRegTypeUseDef(psState, &psUseDef->sInternal, uRegNum, uMask, eFmt);
			break;
		}
		case USEASM_REGTYPE_PREDICATE: 
		{
			VectorSet(psState, &psUseDef->sPredicate, uRegNum, uMask);
			break;
		}
		case USC_REGTYPE_REGARRAY: 
		{
			IMG_UINT32 uBaseReg, uNumRegs, uIdx, uRegType_RegArray;
			
			/* Vector register array must be specified. */
			ASSERT(uRegNum < psState->uNumVecArrayRegs);	
			ASSERT(psState->apsVecArrayReg != NULL);
			ASSERT(psState->apsVecArrayReg[uRegNum] != NULL);

			/* Take the union of all registers in the array */
			uRegType_RegArray = psState->apsVecArrayReg[uRegNum]->uRegType;
			uBaseReg = psState->apsVecArrayReg[uRegNum]->uBaseReg;
			uNumRegs = psState->apsVecArrayReg[uRegNum]->uRegs;
			for (uIdx = 0; uIdx < uNumRegs; uIdx ++)
			{
				IncreaseRegUseDef(psState, psUseDef, uRegType_RegArray, uBaseReg + uIdx, uMask, eFmt);
			}
			break;
		}
	}
}

IMG_INTERNAL 
IMG_VOID ReduceRegUseDef(PINTERMEDIATE_STATE psState, 
						 PREGISTER_USEDEF psUseDef, 
						 IMG_UINT32 uRegType,
						 IMG_UINT32 uRegNum,
						 IMG_UINT32 uMask)
/*********************************************************************************
 Function			: ReduceRegUseDef

 Description		: Remove register channels from a use/def record.
 
 Parameters			: psState	- Internal compiler state
					  psReg		- The register.
					  uMask		- Mask of the register channels to add/remove.

 Output				: psUseDef	- The use/def record.

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uStart = uRegNum * CHANS_PER_REGISTER;

	/* Set the mask in the appropriate field */
	switch (uRegType)
	{
		case USEASM_REGTYPE_TEMP: 
		{
			VectorAndRange(psState, 
						   &psUseDef->sTemp.sChans, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, 
						   ~uMask);
			break;
		}
		case USEASM_REGTYPE_OUTPUT: 
		{
			VectorAndRange(psState, 
						   &psUseDef->sOutput.sChans, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, 
						   ~uMask);
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			VectorAndRange(psState, 
						   &psUseDef->sPrimAttr.sChans, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, 
						   ~uMask);
			break;
		}
		case USEASM_REGTYPE_INDEX:
		{
			VectorAndRange(psState, 
						   &psUseDef->sIndex.sChans, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, 
						   ~uMask);
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			VectorAndRange(psState, 
						   &psUseDef->sInternal.sChans, 
						   uStart + CHANS_PER_REGISTER - 1, uStart, 
						   ~uMask);
			break;
		}
		case USEASM_REGTYPE_PREDICATE: 
		{
			if (uMask & 1)
			{
				VectorSet(psState, &psUseDef->sPredicate, uRegNum, 0);
			}
			break;
		}
		case USC_REGTYPE_REGARRAY: 
		{
			IMG_UINT32 uBaseReg, uNumRegs, uIdx, uRegType_RegArray;
			
			/* Vector register array must be specified. */
			ASSERT(uRegNum < psState->uNumVecArrayRegs);	
			ASSERT(psState->apsVecArrayReg != NULL);
			ASSERT(psState->apsVecArrayReg[uRegNum] != NULL);

			/* Take the union of all registers in the array */
			uRegType_RegArray = psState->apsVecArrayReg[uRegNum]->uRegType;
			uBaseReg = psState->apsVecArrayReg[uRegNum]->uBaseReg;
			uNumRegs = psState->apsVecArrayReg[uRegNum]->uRegs;
			for (uIdx = 0; uIdx < uNumRegs; uIdx ++)
			{
				ReduceRegUseDef(psState, psUseDef, uRegType_RegArray, uBaseReg + uIdx, uMask);
			}
			break;
		}
	}
}

static
IMG_VOID InstUseIndex(PINTERMEDIATE_STATE psState, PARG psArg, PREGISTER_USEDEF psUse)
/*********************************************************************************
 Function			: InstUseIndex

 Description		: Add any dynamic index used by an instruction argument (source or
					  destination) to a USE record.
 
 Parameters			: psState	- Compiler state
					  psArg		- Argument.

 Output				: psUse		- Updated with the register use by the dynamic
								index.

 Return				: Nothing
*********************************************************************************/
{
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		/* Mark index register as used. */
		IncreaseRegUseDef(psState, 
						  psUse, 
						  psArg->uIndexType,
						  psArg->uIndexNumber, 
						  USC_DESTMASK_FULL,
						  UF_REGFORMAT_F32);
	}
}

IMG_INTERNAL
IMG_VOID InstUseSubset(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_USEDEF psUse, IMG_UINT32 const* puSet)
/*********************************************************************************
 Function			: InstUseSubset

 Description		: Work out the register use for an instruction.
 
 Parameters			: psState	- Compiler state
					  psInst	- Instruction.
					  puSet		- Bit vector of the subset of arguments to include.

 Output				: psUse		- Updated with the register use of the instruction.

 Return				: Nothing
*********************************************************************************/
{
	/* Mostly taken from dce.c:ComputeLivenessForInst */
	IMG_UINT32 uArg, uArgCount, uDestIdx, uPred;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		/*
			Mark a dynamically indexed destination as using the index
			register.
		*/
		InstUseIndex(psState, psDest, psUse);
	}

	/*
		If the program is in SSA form then the destination is always completely written, channels
		not written with the result of the instruction are copied from the old destination argument.

		So if the instruction is either predicated or has a partial write mask then always add the
		old destination to the USE record.

		If the program isn't in the SSA form then the 'new' destination and the old are always the
		same register and the old destination is only used (in a way which conflicts with other
		defines) if the instruction is predicated.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_SSA_FORM) != 0 || !NoPredicate(psState, psInst))
	{
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psOldDest = psInst->apsOldDest[uDestIdx];

			if (psOldDest != NULL)
			{
				IMG_UINT32	uRegNum;
				IMG_UINT32	uChansUsed;

				if (psState->uFlags & USC_FLAGS_MOESTAGE)
				{
					uRegNum = psOldDest->uNumberPreMoe;
				}
				else
				{
					uRegNum = psOldDest->uNumber;
				}

				if ((psState->uFlags2 & USC_FLAGS2_SSA_FORM) != 0)
				{
					uChansUsed = GetPreservedChansInPartialDest(psState, psInst, uDestIdx);
				}
				else
				{
					uChansUsed = psInst->auDestMask[uDestIdx];
				}

				IncreaseRegUseDef(psState, 
								  psUse,
								  psOldDest->uType, 
								  uRegNum,
								  uChansUsed,
								  psOldDest->eFmt);
			}
		}
	}

	/* Predicate source. */
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		if (psInst->apsPredSrc[uPred] != NULL)
		{
			IncreaseRegUseDef(psState, 
							  psUse,
							  psInst->apsPredSrc[uPred]->uType,
							  psInst->apsPredSrc[uPred]->uNumber, 
							  USC_ALL_CHAN_MASK,
							  psInst->apsPredSrc[uPred]->eFmt);
		}
	}

	/* Source registers */
	uArgCount = psInst->uArgumentCount;
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		PARG const psSource = &psInst->asArg[uArg];
		USC_REGTYPE uSrcType = psSource->uType;
		IMG_UINT32 uNumRegsUsed, i;
		IMG_UINT32	uNumber;		// register number
		IMG_UINT32	uSrcChans;

		/*
			Skip arguments we aren't asked to consider.
		*/
		if (puSet != NULL && !GetBit(puSet, uArg))
		{
			continue;
		}

		/*
			Add any dynamic index used.
		*/
		InstUseIndex(psState, psSource, psUse);

		if (psState->uFlags & USC_FLAGS_MOESTAGE)
		{
			uNumber = psSource->uNumberPreMoe;
		}
		else
		{
			uNumber = psSource->uNumber;
		}

		/* Number of registers that are used, starting with current source operand. */
		uNumRegsUsed = 1;

		if (psSource->uIndexType != USC_REGTYPE_NOINDEX)
		{
			/* 
			   Dynamically indexed source may use any of the registers 
			   so mark all used. 
			*/
			switch(psSource->uType)
			{
				case USEASM_REGTYPE_TEMP: 
				{
					uNumRegsUsed = EURASIA_USE_TEMPORARY_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_OUTPUT: 
				{
					uNumRegsUsed = EURASIA_USE_OUTPUT_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_PRIMATTR:
				{
					uNumRegsUsed = EURASIA_USE_PRIMATTR_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_SECATTR:
				{
					uNumRegsUsed = EURASIA_USE_SECATTR_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_INDEX:
				{
					uNumRegsUsed = EURASIA_USE_INDEX_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_FPINTERNAL:
				{
					uNumRegsUsed = EURASIA_USE_FPINTERNAL_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_PREDICATE:
				{
					uNumRegsUsed = EURASIA_USE_PREDICATE_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_DRC:
				{
					uNumRegsUsed = EURASIA_USE_DRC_BANK_SIZE;
					break;
				}
				case USEASM_REGTYPE_GLOBAL:
				{
					uNumRegsUsed = EURASIA_USE_GLOBAL_BANK_SIZE;
					break;
				}
				case USC_REGTYPE_REGARRAY:
				{
					ASSERT(psState->apsVecArrayReg != NULL);
					ASSERT(psState->apsVecArrayReg[psSource->uNumber] != NULL);

					uSrcType = psState->apsVecArrayReg[psSource->uNumber]->uRegType;
					uNumber = psState->apsVecArrayReg[psSource->uNumber]->uBaseReg;
					uNumRegsUsed = psState->apsVecArrayReg[psSource->uNumber]->uRegs;
					break;
				}
				default:
				{
					uNumRegsUsed = 0;
					break;
				}
			}
		}

		/* 
		   Stores with fetch can use a number of registers. 
		   Assumption: fetches only use increment mode.
		*/
		switch (psInst->eOpcode)
		{
			case ISTAB:
			case ISTLB:
			case ISTAW:
			case ISTLW:
			case ISTAD:
			case ISTLD:
			{
				if (GetBit(psInst->auFlag, INST_FETCH) && uArg == 0)
				{
					/* Fetch count gives the number of registers used. */
					uNumRegsUsed = max(uNumRegsUsed, psInst->uRepeat);
				}
				break;
			}

			default:
			{
				break;
			}
		}

		/* 
		   Calculate which channels in the source registers are used. 
		   (Use liveness functions to calculate the channels.)
		*/
		uSrcChans = GetLiveChansInArg(psState, psInst, uArg);

		/* Mark the registers used in the sources. */
		for (i = 0; i < uNumRegsUsed; i ++)
		{
			IncreaseRegUseDef(psState, 
							  psUse,
							  uSrcType,
							  uNumber + i, 
							  uSrcChans,
							  psSource->eFmt);
		}
	}

	#if defined(OUTPUT_USPBIN)
	/*
		Add all the USP temporaries to the USE record.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
	{
		IMG_UINT32	uRegIdx;
		IMG_UINT32	uTempCount;

		uTempCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);
		for (uRegIdx = 0; uRegIdx < uTempCount; uRegIdx++)
		{
			IncreaseRegUseDef(psState,
							  psUse,
							  psInst->u.psSmp->sUSPSample.sTempReg.uType,
							  psInst->u.psSmp->sUSPSample.sTempReg.uNumber + uRegIdx,
							  USC_DESTMASK_FULL,
							  psInst->u.psSmp->sUSPSample.sTempReg.eFmt);
		}
	}
	if(psInst->eOpcode == ISMPUNPACK_USP)
	{
		IMG_UINT32	uRegIdx;
		IMG_UINT32	uTempCount;

		uTempCount = GetUSPPerSampleUnpackTemporaryCount();
		for (uRegIdx = 0; uRegIdx < uTempCount; uRegIdx++)
		{
			IncreaseRegUseDef(psState,
							  psUse,
							  psInst->u.psSmpUnpack->sTempReg.uType,
							  psInst->u.psSmpUnpack->sTempReg.uNumber + uRegIdx,
							  USC_DESTMASK_FULL,
							  psInst->u.psSmpUnpack->sTempReg.eFmt);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Flag loads or stores with local addressing as using local memory. The
		reason for including stores is because we don't track precisely which
		locations are updated by the index.
	*/
	if (
			(
				(
					(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) != 0 ||
					(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) != 0
				) &&
				(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE) != 0
			) ||
			psInst->eOpcode == IIDF
	   )
	{
		psUse->bLocalMemory = IMG_TRUE;
	}
}	

IMG_INTERNAL
IMG_VOID InstUse(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_USEDEF psUse)
/*********************************************************************************
 Function			: InstUse

 Description		: Work out the register use for an instruction.
 
 Parameters			: psState	- Compiler state
					  psInst - Instruction.

 Output				: psUse - Updated with the register use of the instruction.

 Return				: Nothing
*********************************************************************************/
{
	InstUseSubset(psState, psInst, psUse, NULL);
}

IMG_INTERNAL
IMG_VOID InstDef(PINTERMEDIATE_STATE psState, 
				 PINST psInst, 
				 PREGISTER_USEDEF psDef)
/*********************************************************************************
 Function		: InstDef

 Description	: Work out the register definitions for an instruction.
 
 Parameters		: psState		- Compiler state
			      psInst 		- Instruction.

 Output			: psDef - Updated with the register use of the instruction.

 Return			: Nothing
*********************************************************************************/
{
	/* Mostly taken from dce.c:ComputeInstRegsInitialised */
	IMG_UINT32 uDestOffset;
	IMG_UINT32 uDestMask;
	IMG_UINT32 uDestIdx;

	/* Main destination. */
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG		psDest = &psInst->asDest[uDestIdx];
		IMG_UINT32	uBaseReg;
		IMG_UINT32	uNumRegs;
		IMG_UINT32	uDestType;

		uDestType = psDest->uType;
		if (psDest->uIndexType != USC_REGTYPE_NOINDEX)
		{
			/*
				Get the range of registers potentially referenced by the index.
			*/
			GetIndexRangeForDest(psState, psDest, &uDestType, &uBaseReg, &uNumRegs);
		}
		else
		{
			/*
				Simple case: only one register is defined.
			*/
			if (psState->uFlags & USC_FLAGS_MOESTAGE)
			{
				uBaseReg = psDest->uNumberPreMoe;
			}
			else
			{
				uBaseReg = psDest->uNumber;
			}
			uNumRegs = 1;
		}

		/* Get the mask of channels written in this destination. */
		if (
				(psState->uFlags2 & USC_FLAGS2_SSA_FORM) != 0 && 
				(
					psDest->uType == USEASM_REGTYPE_TEMP || 
					psDest->uType == USEASM_REGTYPE_PREDICATE
				)
			)
		{
			uDestMask = USC_ALL_CHAN_MASK;
		}
		else
		{
			uDestMask = GetDestMaskIdx(psInst, uDestIdx);
		}

		for (uDestOffset = 0; uDestOffset < uNumRegs; uDestOffset++)
		{
			/* Mark register as defined */
			IncreaseRegUseDef(psState, 
							  psDef, 
							  uDestType,
							  uBaseReg + uDestOffset, 
							  uDestMask,
							  psDest->eFmt);
		}
	}

	/* 
	   Special Cases.
	*/

	#if defined(OUTPUT_USPBIN)
	/*
		Add all the USP temporaries to the DEF record.
	*/
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE)
	{
		IMG_UINT32	uRegIdx;
		IMG_UINT32	uTempCount;

		uTempCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);
		for (uRegIdx = 0; uRegIdx < uTempCount; uRegIdx++)
		{
			IncreaseRegUseDef(psState,
							  psDef,
							  psInst->u.psSmp->sUSPSample.sTempReg.uType,
							  psInst->u.psSmp->sUSPSample.sTempReg.uNumber + uRegIdx,
							  USC_DESTMASK_FULL,
							  psInst->u.psSmp->sUSPSample.sTempReg.eFmt);
		}
	}
	if (psInst->eOpcode == ISMPUNPACK_USP)
	{
		IMG_UINT32	uRegIdx;
		IMG_UINT32	uTempCount;

		uTempCount = GetUSPPerSampleUnpackTemporaryCount();
		for (uRegIdx = 0; uRegIdx < uTempCount; uRegIdx++)
		{
			IncreaseRegUseDef(psState,
							  psDef,
							  psInst->u.psSmpUnpack->sTempReg.uType,
							  psInst->u.psSmpUnpack->sTempReg.uNumber + uRegIdx,
							  USC_DESTMASK_FULL,
							  psInst->u.psSmpUnpack->sTempReg.eFmt);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/* Descheduling points clobber internal registers */
	if (IsDeschedulingPoint(psState, psInst))
	{
		psDef->bInternalRegsClobbered = IMG_TRUE;
	}

	/*
		Flag stores with local addressing as defining local memory.
	*/
	if (
			(
				(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) != 0 &&
				(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE) != 0
			) ||
			psInst->eOpcode == IIDF
	   )
	{
		psDef->bLocalMemory = IMG_TRUE;
	}
}

IMG_INTERNAL
IMG_VOID ComputeInstUseDef(PINTERMEDIATE_STATE psState,
						   PUSEDEF_RECORD psUseDef, 
						   PINST psFromInst,
						   PINST psToInst)
/*********************************************************************************
 Function	 : ComputeInstUseDef

 Description : Work out the register use-def for instructions from psFromInst to
			   psToInst (inclusive). If psToInst is NULL, sequence includes
			   all instructions following psFromInst. 

 Parameters  : psState		- Compiler state
			   psUseDef	   	- Use-Def record to fill
			   psFromInst  	- First instruction in sequence
			   psToInst  	- Instruction.

 Output		 : psDef - Updated with the register use of the instruction.

 Return		 : Nothing
*********************************************************************************/
{
	PINST psInst, psLastInst;
	
	if (psToInst == psFromInst)
	{
		psLastInst = psFromInst->psNext;
	}
	else
	{
		psLastInst = psToInst;
	}

	for(psInst = psFromInst; 
		psInst != NULL && psInst != psLastInst; 
		psInst = psInst->psNext)
	{
		/* Calculate register use */
		InstUse(psState, psInst, &psUseDef->sUse);
		/* Calculate register def */
		InstDef(psState, psInst, &psUseDef->sDef);
	}
}

static
IMG_BOOL InternalRegsReferenced(PINTERMEDIATE_STATE psState,
								PREGISTER_USEDEF	psUseOrDef,
								IMG_UINT32			uInternalRegScalarIndex)
/*********************************************************************************
 Function	 : InternalRegsReferenced

 Description : Check whether a USE record or a DEF record references the any of
			   internal registers.

 Parameters  : psState		- Compiler state.
			   psUseOrDef	- Record to check.

 Return		 : TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32	uStart = uInternalRegScalarIndex * CHANS_PER_REGISTER;
	if (VectorGetRange(psState, &psUseOrDef->sInternal.sChans, uStart + CHANS_PER_REGISTER - 1, uStart) != 0)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static 
IMG_BOOL AnyInternalRegsReferenced(PINTERMEDIATE_STATE	psState,
								PREGISTER_USEDEF	psUseOrDef)
/*********************************************************************************
 Function	 : AnyInternalRegsReferenced

 Description : Check whether a USE record or a DEF record references the any of
			   internal registers.

 Parameters  : psState		- Compiler state.
			   psUseOrDef	- Record to check.

 Return		 : TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32	uRegIdx;

	for (uRegIdx = 0; uRegIdx < psState->uGPISizeInScalarRegs; uRegIdx++)
	{
		if(InternalRegsReferenced(psState, psUseOrDef, uRegIdx))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_UINT32	GetAvailableInternalRegsBetweenInterval(PINTERMEDIATE_STATE psState, PINST psFromInst, PINST psToInst)
/*********************************************************************************
 Function	 : GetAvailableInternalRegsBetweenInterval

 Description : Returns 32 bit array indicating use or no-use of respective indexed
			   Internal register.

 Parameters  : psState		- Compiler state.
			   psFromInst	- Interval start
			   psToInst		- Interval End

 Return		 : TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32 uRegIdx, uAvailableIRegs = 0;

	for (uRegIdx = 0; uRegIdx < psState->uGPISizeInScalarRegs; uRegIdx++)
	{
		if(!UseDefIsIRegLiveInInternal(psState, 
										uRegIdx, 
										psFromInst, 
										psToInst))
		{
			uAvailableIRegs |= (1U << uRegIdx);
		}
	}
	
	if(psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS)
	{
		uAvailableIRegs = uAvailableIRegs & (~(0xFFFFFFFF << psState->uGPISizeInScalarRegs));
	}
	else
	{
		uAvailableIRegs = uAvailableIRegs & (~(0xFFFFFFFF << (psState->uGPISizeInScalarRegs >> 2)));
	}
	return uAvailableIRegs;
}

IMG_INTERNAL
IMG_BOOL SelectInternalReg(IMG_UINT32 uIRegAvailable, IMG_UINT32 uVectorMask, IMG_PUINT32 puRegIregIndex)
/*********************************************************************************
 Function	 : SelectInternalReg

 Description : Selects single internal register from given bit array

 Parameters  : psState			- Compiler state.
			   puRegIregIndex	- Retuned selected Internal register.

 Return		 : TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32 uIregIndex;
	IMG_UINT32 uVectorLength = 0;
	
	while(uVectorMask >> uVectorLength)
		uVectorLength++;
	
	for(uIregIndex = 0; uIregIndex < (32/uVectorLength); uIregIndex++)
	{
		if((uIRegAvailable & (uVectorMask << (uIregIndex * uVectorLength))) 
				  == (uVectorMask << (uIregIndex * uVectorLength)))
		{
			*puRegIregIndex = uIregIndex * uVectorLength;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL InstUseDefined(PINTERMEDIATE_STATE psState,
						PREGISTER_USEDEF psDef, 
						PINST psInst)
/*********************************************************************************
 Function	 : InstUseDefined

 Description : Test whether any used register is marked as defined.

 Parameters  : psDef	  	- Def record to check.
			   psInst	  	- Instruction.

 Return		 : Whether a used register is marked as defined.
*********************************************************************************/
{
	REGISTER_USEDEF sInstUse;
	IMG_BOOL		bDisjoint;
	
	/* Calculate use-def for instruction */
	InitRegUseDef(&sInstUse);
	InstUse(psState, psInst, &sInstUse);

	/* Check whether registers banks are disjoint */
	bDisjoint = DisjointUseDef(psState, psDef, &sInstUse);

	/*
		Check if the record clobbers the internal registers and this instruction
		reads them.
	*/
	if (psDef->bInternalRegsClobbered && AnyInternalRegsReferenced(psState, &sInstUse))
	{
		bDisjoint = IMG_FALSE;
	}

	/* Free use-def information. */
	ClearRegUseDef(psState, &sInstUse);

	return !bDisjoint;
}

IMG_INTERNAL
IMG_BOOL InstDefReferenced(PINTERMEDIATE_STATE psState, 
						   PREGISTER_USEDEF psUseOrDef, 
						   PINST psInst)
/*********************************************************************************
 Function	 : InstDefUsed

 Description : Test whether any defined register is part of a USE record or a DEF
			   record.

 Parameters  : psState		- Compiler State
			   psUseOrDef	 - USE record or DEF record to check.
			   psInst	  	- Instruction.

 Return		 : Whether a written register is marked as used.
*********************************************************************************/
{
	REGISTER_USEDEF sInstDef;
	IMG_BOOL		bDisjoint;

	/* Calculate use-def for instruction */
	InitRegUseDef(&sInstDef);
	InstDef(psState, psInst, &sInstDef);

	/* Check whether registers banks are disjoint */
	bDisjoint = DisjointUseDef(psState, psUseOrDef, &sInstDef);

	/*
		Check if the record clobbers the internal registers and this instruction
		writes them.
	*/
	if (psUseOrDef->bInternalRegsClobbered && AnyInternalRegsReferenced(psState, &sInstDef))
	{
		bDisjoint = IMG_FALSE;
	}
	/*
		Check if this instruction clobbers the internal registers and the record
		reads/writes them.
	*/
	if (sInstDef.bInternalRegsClobbered && AnyInternalRegsReferenced(psState, psUseOrDef))
	{
		bDisjoint = IMG_FALSE;
	}

	/* Free use-def information. */
	ClearRegUseDef(psState, &sInstDef);

	return !bDisjoint;
}

static
IMG_BOOL DisjointRegisterTypeUseDef(PINTERMEDIATE_STATE		psState, 
									PREGISTER_TYPE_USEDEF	psUseDefA, 
									PREGISTER_TYPE_USEDEF	psUseDefB)
/*********************************************************************************
 Function	 : DisjointRegisterTypeUseDef

 Description : Test whether two register use-def records for a particular register
			   type are disjoint.

 Parameters  : psState				- Compiler state
			   psUseDeffA, psUseDefB	- Use-Def records to test

 Return		 : Whether the records are disjoint.
*********************************************************************************/
{
	VECTOR_ITERATOR	sIterA, sIterB;

	/*
		Look for a register which has channels marked as used/defined in both records.
	*/
	VectorIteratorInitialize(psState, &psUseDefA->sChans, CHANS_PER_REGISTER, &sIterA);
	VectorIteratorInitialize(psState, &psUseDefB->sChans, CHANS_PER_REGISTER, &sIterB);
	while (VectorIteratorContinue(&sIterA) && VectorIteratorContinue(&sIterB))
	{
		IMG_UINT32	uRegNumA, uRegNumB;

		uRegNumA = VectorIteratorCurrentPosition(&sIterA) / CHANS_PER_REGISTER;
		uRegNumB = VectorIteratorCurrentPosition(&sIterB) / CHANS_PER_REGISTER;
		if (uRegNumA == uRegNumB)
		{
			IMG_UINT32	uMaskA, uMaskB;
			IMG_BOOL	bC10, bNonC10;

			uMaskA = VectorIteratorCurrentMask(&sIterA);
			uMaskB = VectorIteratorCurrentMask(&sIterB);

			/*
				Check if the register is read/written as C10 in either record.
			*/
			bC10 = IMG_FALSE;
			if (VectorGet(psState, &psUseDefA->sC10, uRegNumA) || VectorGet(psState, &psUseDefB->sC10, uRegNumA))
			{
				bC10 = IMG_TRUE;
			}

			/*
				Check if the register is read/written as non-C10 in either record.
			*/
			bNonC10 = IMG_FALSE;
			if (VectorGet(psState, &psUseDefA->sNonC10, uRegNumA) || VectorGet(psState, &psUseDefB->sNonC10, uRegNumA))
			{
				bNonC10 = IMG_TRUE;
			}

			/*
				The records are non disjoint if 
				(i) Overlapping masks of channels have been used/defined 
				(ii) The register has been defined/used as C10 in one and defined/used as non-C10 in another. In
				this case the masks relate to channels of different bit widths and are incomparable.
			*/
			if ((uMaskA & uMaskB) != 0)
			{
				return IMG_TRUE;
			}
			if (bC10 && bNonC10)
			{
				return IMG_TRUE;
			}
		}

		if (uRegNumA <= uRegNumB)
		{
			VectorIteratorNext(&sIterA);
		}
		if (uRegNumB <= uRegNumA)
		{
			VectorIteratorNext(&sIterB);
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL DisjointUseDef(PINTERMEDIATE_STATE psState,
						PREGISTER_USEDEF psUseDefA, 
						PREGISTER_USEDEF psUseDefB)
/*********************************************************************************
 Function	 : DisjointUseDef

 Description : Test whether two register use-def records are disjoint.

 Parameters  : psState				- Compiler state
			  psUseDfefA, psUseDefB	- Use-Def records to test

 Return		 : Whether the records are disjoint.
*********************************************************************************/
{
	USC_VECTOR sDest;

	InitVector(&sDest, USC_MIN_VECTOR_CHUNK, IMG_FALSE);

	/* Temporary registers */
	if (DisjointRegisterTypeUseDef(psState, &psUseDefA->sTemp, &psUseDefB->sTemp))
	{
		return IMG_FALSE;
	}

	/* PrimAttr registers */
	if (DisjointRegisterTypeUseDef(psState, &psUseDefA->sPrimAttr, &psUseDefB->sPrimAttr))
	{
		return IMG_FALSE;
	}

	/* Internal registers */
	if (DisjointRegisterTypeUseDef(psState, &psUseDefA->sInternal, &psUseDefB->sInternal))
	{
		return IMG_FALSE;
	}

	/* Predicate registers */
	if (VectorOp(psState, USC_VEC_DISJ, &sDest, 
				 &psUseDefA->sPredicate, &psUseDefB->sPredicate) == NULL)
	{
		return IMG_FALSE;
	}

	/* Index registers */
	if (DisjointRegisterTypeUseDef(psState, &psUseDefA->sIndex, &psUseDefB->sIndex))
	{
		return IMG_FALSE;
	}

	/* Output registers */
	if (DisjointRegisterTypeUseDef(psState, &psUseDefA->sOutput, &psUseDefB->sOutput))
	{
		return IMG_FALSE;
	}

	/* Local memory. */
	if (psUseDefA->bLocalMemory && psUseDefB->bLocalMemory)
	{
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

#if defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN)
static
IMG_VOID InstRegUse(PUSC_LIVE_CALLBACK psCallBack,
					PINST psInst)
/*********************************************************************************
 Function			: InstRegUse

 Description		: Work out the register use for an instruction.
 
 Parameters			: psState	- Compiler state
                      psCallBack - Callback to run on each register used.
					  psInst - Instruction.

 Return				: Nothing
*********************************************************************************/
{
	/* Mostly taken from dce.c:ComputeLivenessForInst */
	IMG_UINT32 uArg, uArgCount;
	IMG_UINT32 uDestIdx;
	
	if (psCallBack == NULL)
	{
		return;
	}

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest;

		psOldDest = psInst->apsOldDest[uDestIdx];
		if (psOldDest != NULL)
		{
			psCallBack->pfFn(SetInstCallData(psCallBack,
											 psInst,
											 USC_LCB_PROC,
											 IMG_FALSE,
											 USC_LCB_OLDDEST,
											 (IMG_PVOID)(IMG_UINTPTR_T)uDestIdx));
		}
	}

	/* Predicate source. */
	if (psInst->uDestCount > 0)
	{
		IMG_UINT32 uPred;
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			psCallBack->pfFn(SetInstCallData(psCallBack,
											 psInst,
											 USC_LCB_PROC,
											 IMG_FALSE,
											 USC_LCB_PRED,
											 psInst->apsPredSrc[uPred]));
		}
	}

	/* Source registers */
	uArgCount = psInst->uArgumentCount;
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		PARG psSource = &psInst->asArg[uArg];

		if (psSource->uIndexType != USC_REGTYPE_NOINDEX)
		{
			/* Mark index register as used. */
			psCallBack->pfFn(SetInstCallData(psCallBack,
											 psInst,
											 USC_LCB_PROC,
											 IMG_FALSE,
											 USC_LCB_INDEX,
											 (IMG_PVOID)(IMG_UINTPTR_T)uArg));
		}

		/* Mark the register */
		psCallBack->pfFn(SetInstCallData(psCallBack,
										 psInst,
										 USC_LCB_PROC,
										 IMG_FALSE,
										 USC_LCB_SRC,
										 (IMG_PVOID)(IMG_UINTPTR_T)uArg));

	}
}	

static
IMG_VOID InstRegDef(PUSC_LIVE_CALLBACK psCallBack,
					PINST psInst)
/*********************************************************************************
 Function		: InstRegDef

 Description	: Work out the register definitions for an instruction.
 
 Parameters		: psCallBack	- Callback to run on each register defined
			      psInst 		- Instruction.

 Return			: Nothing
*********************************************************************************/
{
	/* Mostly taken from dce.c:ComputeInstRegsInitialised */
	IMG_UINT32 uDestOffset;
	IMG_UINT32 uDestCount;

	if (psCallBack == NULL)
	{
		return;
	}

	/* Main destination. */
	if	(psInst->uDestCount > 0)
	{
		uDestCount = psInst->uDestCount;

		/* Run through each destination register, marking it as defined */
		for (uDestOffset = 0; 
			 uDestOffset < uDestCount;
			 uDestOffset++)
		{
			/* Mark index register as used. */
			if	(psInst->asDest[0].uIndexType != USC_REGTYPE_NOINDEX)
			{
				psCallBack->pfFn(SetInstCallData(psCallBack,
												 psInst,
												 USC_LCB_PROC,
												 IMG_FALSE,
												 USC_LCB_INDEX,
												 (IMG_PVOID)(IMG_UINTPTR_T)uDestOffset));
			}

			/* Mark register as defined */
			psCallBack->pfFn(SetInstCallData(psCallBack,
											 psInst,
											 USC_LCB_PROC,
											 IMG_TRUE,
											 USC_LCB_DEST,
											 (IMG_PVOID)(IMG_UINTPTR_T)uDestOffset));
		}
	}
}

static 
IMG_VOID RegUseInst(PUSC_LIVE_CALLBACK psCallBack,
					PINST psInst)
/*****************************************************************************
 FUNCTION	: RegUseInst
    
 PURPOSE	: Analyse register use in an instruction

 PARAMETERS	: psState          - Compiler state
              psCallBack       - Callback
              psInst           - Instruction in sequence


 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psCallBack == NULL)
	{
		return;
	}

	/* Callback on entry */
	psCallBack->pfFn(SetInstCallData(psCallBack,
									 psInst,
									 USC_LCB_ENTRY,
									 IMG_TRUE,
									 USC_LCB_TYPE_UNDEF,
									 (IMG_PVOID)0U));

	/* Register definitions */
	InstRegDef(psCallBack, psInst);

	/* Register use */
	InstRegUse(psCallBack, psInst);

	/* Callback on exit */
	psCallBack->pfFn(SetInstCallData(psCallBack,
									 psInst,
									 USC_LCB_EXIT,
									 IMG_TRUE,
									 USC_LCB_TYPE_UNDEF,
									 (IMG_PVOID)0U));
}


IMG_INTERNAL
IMG_VOID RegUseInstSeq(PUSC_LIVE_CALLBACK psCallBack,
					   PINST psLastInst)
/*****************************************************************************
 FUNCTION	: RegUseInstSeq

 PURPOSE	: Analyse register use in a sequence of instructions

 PARAMETERS	: psState          - Compiler state
              psCallBack       - Callback
              psLastInst       - Last instruction in sequence

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;

	for (psInst = psLastInst;
		 psInst != NULL;
		 psInst = psInst->psPrev)
	{
		RegUseInst(psCallBack, psInst);
	}
}

IMG_INTERNAL IMG_VOID RegUseBlock(PINTERMEDIATE_STATE psState,
								  PUSC_LIVE_CALLBACK psCallBack,
								  PFUNC psFunc)
/******************************************************************************
Function		: RegUseBlock

Description		: Use to iterate a USC_LIVE_CALLBACK procedure over a CFG.
	Note that this uses the DebugPrintSF sort order (but backwards!), which
	isn't optimal (e.g. it doesn't always put loop bodies with their headers right
	when loops are nested), but the *correctness* of register allocation shouldn't
	depend on the ordering. (Hence, TODO - work out a really good ordering for
	register allocation!!)
******************************************************************************/
{
	IMG_UINT32 uBlock;
	//sort blocks - into forwards order :-(
	DoOnCfgBasicBlocks(psState, &(psFunc->sCfg), DebugPrintSF, NULL/*no callback*/, IMG_TRUE, NULL);
	
	//hence, manually iterate, in reverse
	for (uBlock = psFunc->sCfg.uNumBlocks; uBlock-- > 0; ) //NOTE DECREMENT post-test
	{
		PCODEBLOCK psBlock = psFunc->sCfg.apsAllBlocks[uBlock];
		PINST psInst;

		//1. Entry callback
		psCallBack->pfFn(SetBlockCallData(psCallBack, 
										  psBlock,
										  USC_LCB_ENTRY,
										  0));
		SetBlockCallData(psCallBack, 
						 psBlock,
						 USC_LCB_PROC,
						 0);

		//2. Callback for successor test
		switch (psBlock->eType)
		{
		case CBTYPE_EXIT:
		case CBTYPE_UNCOND:
			break;
		case CBTYPE_COND:
			if(psBlock->u.sCond.sPredSrc.uType != USC_REGTYPE_EXECPRED)
			{
				psCallBack->pfFn(SetInstCallData(psCallBack,
												 NULL,
												 USC_LCB_PROC,
												 IMG_FALSE,
												 USC_LCB_PRED,
												 (IMG_PVOID)(IMG_UINTPTR_T)psBlock->u.sCond.sPredSrc.uNumber));
			}
			break;
		case CBTYPE_SWITCH:
			psCallBack->pfFn(SetInstCallData(psCallBack,
											 NULL,
											 USC_LCB_PROC,
											 IMG_FALSE,
											 USC_LCB_SWITCHARG,
											 (IMG_PVOID)(IMG_UINTPTR_T)psBlock->u.sSwitch.psArg));
			break;
		case CBTYPE_CONTINUE:
		case CBTYPE_UNDEFINED:
			imgabort();
		}

		//3. Instruction Sequence (backwards).
		for (psInst = psBlock->psBodyTail; psInst; psInst = psInst->psPrev)
		{
			if (psInst->eOpcode == ICALL)
			{
				ASSERT(NoPredicate(psState, psInst));
			}
			else
			{
				RegUseInst(psCallBack, psInst);
			}
		}
		
		//4. Exit callback
		psCallBack->pfFn(SetBlockCallData(psCallBack, 
										  psBlock,
										  USC_LCB_EXIT,
										  0));

		SetBlockCallData(psCallBack, 
						 psBlock,
						 USC_LCB_PROC,
						 0);
	}
}

#endif /* defined(REGALLOC_LSCAN) || defined(IREGALLOC_LSCAN) */

/******************************************************************************
 End of file (dce.c)
******************************************************************************/
