/*************************************************************************
 * Name         : layout.c
 * Title        : Layout of output code - common to NOSCHED and HW-Compilation
 * Created      : July 2008
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
 * $Log: layout.c $
 **************************************************************************/
 
#include "uscshrd.h"
#include "bitops.h"
#include "layout.h"
#include <limits.h>

IMG_INTERNAL
IMG_VOID AppendInstruction(PLAYOUT_INFO psLayout, IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION	: AppendInstruction
    
 PURPOSE	: Update the layout state when adding an instruction at the current
			  end of the program.

 PARAMETERS	: psLayout		- Layout state.
			  eOpcode		- Opcode of the appended instruction.
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	psLayout->eLastOpcode = eOpcode;
	psLayout->bLastLabelWasSyncEndDest = psLayout->bThisLabelIsSyncEndDest;
	psLayout->bThisLabelIsSyncEndDest = IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsIllegalInstructionPair
    
 PURPOSE	: Check for an illegal combination of instructions.

 PARAMETERS	: psState			- The compiler state
			  psInst1/psInst2	- The pair to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL IsIllegalInstructionPair(PINTERMEDIATE_STATE	psState, 
								  IOPCODE				eOpcode1, 
								  IOPCODE				eOpcode2)
{
	if	(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING)
	{
		return IMG_FALSE;	
	}

	if ((eOpcode1 == ILAPC || eOpcode1 == IBR || eOpcode1 == ICALL) &&
		(eOpcode2 == ILAPC || eOpcode2 == IBR || eOpcode2 == ICALL))
	{
		return IMG_TRUE;
	}
	if (eOpcode1 == ISETL && (eOpcode2 == ISAVL || eOpcode2 == ILAPC))
	{
		return IMG_TRUE;
	}
	if (eOpcode1 == ISETL && eOpcode2 == ICALL)
	{
		return IMG_TRUE;
	}
	if (eOpcode1 == ICALL && eOpcode2 == ISAVL)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static 
IMG_BOOL IsSyncEndDest(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: IsSyncEndDest

 DESCRIPTION	: Checks whether a block is the destination of any branches
				  with the sync_end flag set

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block to inspect

 RETURNS		: TRUE if any of the block's predecessors jump to it with
				  sync_end; FALSE if none do.
******************************************************************************/
{
	IMG_UINT32 uPred;
	for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
	{
		PCODEBLOCK psPred = psBlock->asPreds[uPred].psDest;
		IMG_UINT32 uSucc = psBlock->asPreds[uPred].uDestIdx;

		//examine sync_end flag (stored according to block type)
		switch (psPred->eType)
		{
		case CBTYPE_UNCOND:
			if (psPred->u.sUncond.bSyncEnd)
			{
				ASSERT (psBlock->uNumPreds > 1);
				return IMG_TRUE;
			}
			break;
		case CBTYPE_COND:
			if (psPred->u.sCond.uSyncEndBitMask & (1U << uSucc)) return IMG_TRUE;
			break;
		case CBTYPE_SWITCH:
			/*
				By the time we're doing hardware layout, SWITCHs should already
				have been translated into nested conditionals; however, they're
				easy enough to handle (so as to make IsSyncEndDest more general)
			*/
			if (psPred->u.sSwitch.pbSyncEnd[uSucc]) return IMG_TRUE;
			break;
		case CBTYPE_EXIT: //shouldn't be a predecessor of psBlock!
		case CBTYPE_CONTINUE:
		case CBTYPE_UNDEFINED: //not post-construction
			imgabort();
		}
		//That predecessor does not jump to psBlock with sync_end. Try next one
	}
	//no predecessor jumps here with sync_end.
	return IMG_FALSE;
}


IMG_INTERNAL
IMG_VOID NoSchedLabelCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout,
					 IMG_UINT32 uDestLabel, IMG_BOOL bSyncEndDest)
/******************************************************************************
 FUNCTION		: NoSchedLabelCB

 PURPOSE		: LAY_LABEL callback for modelling generation of a label during
				  NOSCHED setup.

 PARAMETERS		: psState		- Current compilation context
				  psLayout		- stores/models instructions being generated
							      (inc. other callbacks - like an OOP object)
				  uDestLabel	- number of label to make, i.e. for jumps to it
				  bSyncEndDest	- whether any jumps to the label use sync-end.

 RETURNS		: Nothing, but updates psLayout via callbacks if padding reqd

 NOTE			: IMG_INTERNAL as also used by H/W-code generation stage as
				  _part_ of its own handling of labels
******************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(uDestLabel);

	/*
		for NOSCHED, all we need to do is model (the effect on the instruction
		address/offset of) any padding that is required for the label.
	*/

	/*
		Ensure:-
		(i) Any branch with sync-end is to a double instruction aligned destination.
		(ii) If the first instruction in a pair is the target of a branch with sync-end that
		the second instruction in the pair isn't also the target of a branch.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING) == 0)
	{
		if (bSyncEndDest || psLayout->bLastLabelWasSyncEndDest) 
		{
			if(psLayout->pfnAlignToEven)
			{
				psLayout->pfnAlignToEven(psState, psLayout);
			}
		}
	}

	/*
		Record whether the label before the next instruction to be emitted is the
		destination of a branch with sync-end.
	*/
	if (bSyncEndDest)
	{
		psLayout->bThisLabelIsSyncEndDest = bSyncEndDest;
	}
}

IMG_INTERNAL
IMG_VOID CommonBranchCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout,
					  IOPCODE eOpcode, IMG_PUINT32 puDestLabel,
					  IMG_UINT32 uPredSrc, IMG_BOOL bPredNegate,
					  IMG_BOOL bSyncEnd)
/******************************************************************************
 FUNCTION	: CommonBranchCB

 PURPOSE	: LAY_BRANCH callback, does work for generating/modelling branches
			  common to HW/NOSCHED/USP phases (Notably, handles deferred branches with sync_end).

 PARAMETERS		: psState		- Current compilation context
				  psLayout		- stores/models instructions being generated
							      (inc. other callbacks - like an OOP object)
				  eOpcode		- opcode of branch instruction (IBR or ICALL)
				  puDestLabel	- pointer to number of label to which to jump
								  (label number -1 => none assigned yet)
				  uPredSrc		- register upon which to predicate branch
				  bPredNegate	- whether to negate sense of previous
				  bSyncEnd		- whether to jump with the sync_end flag set

 RETURNS		: Nothing

 OUTPUT			: Assigns number to *puDestLabel if none assigned previously
******************************************************************************/
{
	/*
		Predicated branches do not work with sync_end - except after another branch with the same
		predicate, as this "forces" the predicate to be ready/available.
		(Note - alternative mechanism would be to jump _round_ an
		 unconditional branch with sync_end, using the inverted predicate;
		 we do not consider this yet tho.)
	*/
	if	(bSyncEnd && (uPredSrc != USC_PREDREG_NONE))
	{
		IMG_UINT32	uTempDestLabel = UINT_MAX;
		IMG_BOOL	bPadLabel;
		/*
			Extra recursive call _through psLayout callback_ in case we're
			being executed as part of CompileToHW's handling of branches
			(as if psLayout were an OOP object - NoSchedBranchCB might be being
			called only as part of a "super." invocation)
		*/
		psLayout->pfnBranch(psState, psLayout, IBR, &uTempDestLabel,
				     uPredSrc, bPredNegate, IMG_FALSE, EXECPRED_BRANCHTYPE_NORMAL);
		if (psState->puInstructions && psLayout->puInst == NULL) return;
		
		/*
			Check if the two branches (the dummy branch to make the predicate ready and
			the real branch with syncend) can be consecutive in the program.
		*/
		ASSERT (psLayout->eLastOpcode == IBR);
		bPadLabel = IsIllegalInstructionPair(psState, IBR, eOpcode);

		/*
			If the two branches can't be consecutive then insert a NOP inbetween.
		*/
		if (bPadLabel)
		{
			psLayout->pfnAlignToEven(psState, psLayout);
		}

		/*
			Create label as destination of first branch. If required insert a padding
			instruction before the label so the two branches aren't consecutive.
		*/
		psLayout->pfnLabel(psState, psLayout, uTempDestLabel, IMG_FALSE /* bSyncEndDest */);
		if (psState->puInstructions && psLayout->puInst == NULL) return;
		
		//no further padding should be required...?
		ASSERT (IsIllegalInstructionPair(psState, psLayout->eLastOpcode, eOpcode) == 0
				|| (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING)
				|| ((psLayout->puInst - psState->puInstructions) % 4) == 0);
	}
	else if (IsIllegalInstructionPair(psState, psLayout->eLastOpcode, eOpcode))
	{
		psLayout->pfnAlignToEven(psState, psLayout);		
	}

	//assign label if reqd - TODO not when compiling, only for NOSCHED?
	if (puDestLabel && *puDestLabel == UINT_MAX) *puDestLabel = psState->uNextLabel++;
	AppendInstruction(psLayout, eOpcode);
}

static IMG_VOID LayoutCall(PINTERMEDIATE_STATE psState,
						   PLAYOUT_INFO psLayout,
						   PINST psCallInst)
/******************************************************************************
 FUNCTION		: LayoutCall

 DESCRIPTION	: Lays out the body of a call block (i.e. a single ICALL inst)

 PARAMETERS		: psState		- Current compilation context
				  psLayout		- callbacks and data for the instructions being
								  generated/modelled
				  psCallInst	- the ICALL being the body of the block

 RETURNS		: Nothing, but updates psLayout (by invoking callbacks therein)							  
******************************************************************************/
{
	IMG_UINT32 uFuncLabel = psCallInst->u.psCall->psTarget->uLabel;
	IMG_BOOL bNeedSync = psCallInst->u.psCall->psTarget->sCfg.psEntry->bDomSync;
	IMG_UINT32 uPredSrc;
	IMG_BOOL bPredNegate, bPredPerChan;

	ASSERT (NoPredicate(psState, psCallInst));
	GetPredicate(psCallInst, &uPredSrc, &bPredNegate, &bPredPerChan, 0);

	ASSERT(!bPredPerChan);
	ASSERT(psCallInst->uPredCount < 2);

//#if 0
	/*
		Handling predicated calls (with sync_end) is complicated...
		At present we shouldn't actually need to, tho, as a predicated call is
		always represented as a conditional branching around an IsCall block
	*/
	if (uPredSrc != USC_PREDREG_NONE
		&& bNeedSync
#ifdef SUPPORT_SGX545
		&& ((psState->uCompilerFlags & USE_SYNCEND_ON_NOT_BRANCH) == 0)
		
#endif
		)
	{
		IMG_UINT32 uSkippedCallDest = UINT_MAX;
		
		//conditionally (to _not_ do the CALL), skip (with sync_end)...
		if (uPredSrc <= 1 || bPredNegate)
		{
			//IBR to do the skipping
			psLayout->pfnBranch(psState, psLayout, IBR,
				&uSkippedCallDest, uPredSrc,
				bPredNegate ? IMG_FALSE : IMG_TRUE/*if not call*/,
				IMG_TRUE/*syncend*/, EXECPRED_BRANCHTYPE_NORMAL);
		}	
		else
		{
			//can't skip according to inverted predicate!
			IMG_UINT32 uDoCallDest = UINT_MAX;
			//so to do CALL, skip...
			psLayout->pfnBranch(psState, psLayout, IBR,
				&uDoCallDest, uPredSrc,
				bPredNegate /*same sense as call*/,
				IMG_FALSE/*not syncend*/,
				EXECPRED_BRANCHTYPE_NORMAL);
			//over an unconditional branch around the call:
			psLayout->pfnBranch(psState, psLayout, IBR,
								&uSkippedCallDest, USC_PREDREG_NONE,
								IMG_FALSE, IMG_TRUE/*syncend*/,
								EXECPRED_BRANCHTYPE_NORMAL);
			//...to a label that'll execute the CALL itself:
			psLayout->pfnLabel(psState, psLayout, uDoCallDest,
							   IMG_FALSE/*not syncend dest*/);
		}
		//over unconditional CALL:
		psLayout->pfnBranch(psState, psLayout, ICALL,
			&uFuncLabel, USC_PREDREG_NONE, IMG_FALSE, IMG_FALSE/*no syncend*/,
			EXECPRED_BRANCHTYPE_NORMAL);
		//...and resume here
		psLayout->pfnLabel(psState, psLayout, uSkippedCallDest, IMG_TRUE/*syncend dest*/);
	}
	else
//#endif
	{
		//simple case! - but TODO, how do we get pfnBranch to do syncend-on-NOT-branch?
		psLayout->pfnBranch(psState, psLayout, ICALL,
			&uFuncLabel, uPredSrc,
			bPredNegate, IMG_FALSE,
			EXECPRED_BRANCHTYPE_NORMAL);
	}
}

static
IMG_VOID DoPhaseStart(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout, LAYOUT_POINT ePhaseStart)
/******************************************************************************
 FUNCTION		: DoPhaseStart

 DESCRIPTION	: Do actions when starting a block of code which is 'called'
				  directly by the hardware.

 PARAMETERS		: psState		- Current compilation context
				  psLayout		- Layout context.
				  ePhaseStart	- Type of phase we are starting.
 
 RETURNS		: Nothing
******************************************************************************/
{
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNLIMITED_PHASES) == 0)
	{
		/*
			On this core the start of a block of code must be double instruction aligned.
		*/
		psLayout->pfnAlignToEven(psState, psLayout);
	}
	else
	{
		ASSERT((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING) != 0);
	}

	/*
		Invoke the caller of LayoutProgram's callback.
	*/
	psLayout->pfnPointActions(psState, psLayout, ePhaseStart);
}

static IMG_VOID LayoutBlockBP(PINTERMEDIATE_STATE psState,
							  PCODEBLOCK psBlock,
							  IMG_PVOID pvLayout)
/******************************************************************************
 FUNCTION		: LayoutBlockBP

 DESCRIPTION	: BLOCK_PROC to layout final HW code for a block (call on each
				  block, in order in which to output them)

 PARAMETERS		: psState	- Current compilation context
				  psBlock	- block to layout
				  pvLayout	- PLAYOUT_INFO to layout context: block labels,
							  callbacks for generating/modelling instructions.
 
 RETURNS		: Nothing
******************************************************************************/
{
	PLAYOUT_INFO psLayout = (PLAYOUT_INFO)pvLayout;
	IMG_PUINT32 puLabel = &psLayout->auLabels[psBlock->uIdx];
	IMG_BOOL bSpecialTransition;
	
	//abort if something has already gone wrong (on earlier block)
	if (psState->puInstructions && psLayout->puInst == NULL) return;
	
	//too many situations in which we may have been assigned a label to assert them all!
	
	//1. label at beginning, if any branches (or CALLs!) here. (includes any padding req'd for sync_end)
	if (psBlock->uNumPreds > 1 ||
		 (psBlock->uNumPreds == 1 && psBlock->asPreds[0].psDest->uIdx >= psBlock->uIdx) ||
		 *puLabel != UINT_MAX /*assigned by earlier predecessor*/ ||
		 IsSyncEndDest(psState, psBlock))
	{
		/*
			Output a label, as there are jumps here.
			(This includes case of jumps back to first node of program)
		*/
		if (*puLabel == UINT_MAX) *puLabel = psState->uNextLabel++;
		psLayout->pfnLabel(psState, psLayout, *puLabel, IsSyncEndDest(psState, psBlock));
	}

	//2.5 quick check for split feedback
	if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && 
		psState->psPreFeedbackDriverEpilogBlock)
	{
		ASSERT (psState->psPreFeedbackDriverEpilogBlock->eType == CBTYPE_UNCOND);
		if (psBlock == psState->psPreFeedbackDriverEpilogBlock->asSuccs[0].psDest)
		{
			DoPhaseStart(psState, psLayout, LAYOUT_POST_FEEDBACK_START);
		}
	}
	if ((psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0 && 
		psState->psPreSplitBlock != NULL)
	{
		ASSERT (psState->psPreSplitBlock->eType == CBTYPE_UNCOND);
		if (psBlock == psState->psPreSplitBlock->asSuccs[0].psDest)
		{
			DoPhaseStart(psState, psLayout, LAYOUT_POST_SPLIT_START);
		}
	}

#ifdef UF_TESTBENCH
	if (psState->puVerifLoopHdrs)
	{
		/*
			record start positions of any blocks which are loop headers (for verifier)
			-- has to be after any padding added for labels in previous step!
		*/
		IMG_UINT32 uPred;
		for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
		{
			if (Dominates(psState, psBlock, psBlock->asPreds[uPred].psDest))
			{
				//yes, is a header.
				psState->puVerifLoopHdrs[psState->uNumVerifLoopHdrs] = 
					(IMG_UINT32)((psLayout->puInst - psState->puInstructions)/2);
				psState->uNumVerifLoopHdrs++;
				break;
			}
		}
	}
#endif

	//2. body.
	if (IsCall(psState, psBlock))
	{
		LayoutCall(psState, psLayout, psBlock->psBody);
	}
	else
	{
		psLayout->puInst = psLayout->pfnInstrs(psState, psBlock, psLayout->puInst, psLayout);
	}

	if (psState->puInstructions && psLayout->puInst == NULL) return;	

	/*
		Check for a special transition between different program phases. Control flow transfer is handled by
		code inserted by the driver and we never need to insert a branch.
	*/
	bSpecialTransition = IMG_FALSE;
	if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && psState->psPreFeedbackDriverEpilogBlock == psBlock)
	{
		bSpecialTransition = IMG_TRUE;
	}
	if ((psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0 && psState->psPreSplitBlock == psBlock)
	{
		bSpecialTransition = IMG_TRUE;
	}
	if (bSpecialTransition)
	{
		ASSERT(psBlock->eType == CBTYPE_UNCOND);
		ASSERT(!psBlock->u.sUncond.bSyncEnd);
	}

	//3. jumps.
	switch (psBlock->eType)
	{
	case CBTYPE_UNCOND:
		if (
				!bSpecialTransition &&
				(
					psBlock->asSuccs[0].psDest->uIdx != psBlock->uIdx + 1 ||
					psBlock->u.sUncond.bSyncEnd
				)
			)
		{
			psLayout->pfnBranch(psState, psLayout, IBR,
								/* dest label */
								&psLayout->auLabels[psBlock->asSuccs[0].psDest->uIdx],
								USC_PREDREG_NONE, IMG_FALSE/*bPredNegate*/,
								psBlock->u.sUncond.bSyncEnd, EXECPRED_BRANCHTYPE_NORMAL);
		}
		//else, block can fall-through to its successor - no branch req'd.
		break;
	case CBTYPE_COND:
		{ //TODO on 545, consider using sync-end-if-don't-jump!
			IMG_UINT32 uFallthrough, uJumpSucc;
			PCODEBLOCK psJumpDest;
			IMG_BOOL bNegate;
			#if defined(EXECPRED)
			if ((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643))
			{
				if (psBlock->u.sCond.bDontGenBranch)
				{
					break;
				}
			}
			#endif /* #if defined(EXECPRED) */
			//2a. is EITHER successor a fall-through?
			for (uFallthrough = 0; uFallthrough < 2; uFallthrough++)
			{
				if (psBlock->asSuccs[uFallthrough].psDest->uIdx == psBlock->uIdx + 1 &&
					(psBlock->u.sCond.uSyncEndBitMask & (1U << uFallthrough)) == 0)
				{
					break;
				}
			}
			if (uFallthrough == 0 && !(psBlock->u.sCond.sPredSrc.uNumber < EURASIA_USE_EPRED_NUM_NEGATED_PREDICATES))
			{
				/*
					TODO ugly temporary workaround: can only branch according
					to NEGATED predicate for predicate registers !p0 and !p1;
					for later pred regs !p1,... we'll branch round branch :-(
				*/
				uFallthrough = 2;
			}
			uJumpSucc = (uFallthrough < 2) ? 1 - uFallthrough : 0;
			psJumpDest = psBlock->asSuccs[uJumpSucc].psDest;
			//2b. conditional branch to uJumpSucc

			//negate if uJumpSucc==1 (i.e. conditional jump to false successor)
			bNegate = (uJumpSucc) ? IMG_TRUE : IMG_FALSE;

			#if defined(EXECPRED)
			if (((psState->uCompilerFlags2 & UF_FLAGS2_EXECPRED) && (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_INTEGER_CONDITIONALS) && !(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_29643)) && (!psBlock->bStatic))
			{
				psLayout->pfnBranch(psState, psLayout, IBR,
									&psLayout->auLabels[psJumpDest->uIdx] /*dest*/,
									USC_PREDREG_NONE,
									IMG_FALSE /* bNegate */,
									(psBlock->u.sCond.uSyncEndBitMask
										& (1U << uJumpSucc)) ? IMG_TRUE : IMG_FALSE,
									(!bNegate) ? EXECPRED_BRANCHTYPE_ANYINSTTRUE : EXECPRED_BRANCHTYPE_ALLINSTSFALSE);
			}			
			else
			#endif /* defined(EXECPRED) */
			{
				psLayout->pfnBranch(psState, psLayout, IBR,
									&psLayout->auLabels[psJumpDest->uIdx] /*dest*/,
									psBlock->u.sCond.sPredSrc.uNumber,
									bNegate,
									(psBlock->u.sCond.uSyncEndBitMask
										& (1U << uJumpSucc)) ? IMG_TRUE : IMG_FALSE,
									EXECPRED_BRANCHTYPE_NORMAL);
			}
			
			if (uFallthrough==2)
			{
				//2c. can't fallthrough to other successor; unconditional jump
				IMG_UINT32 uDestIdx = psBlock->asSuccs[1].psDest->uIdx;
				psLayout->pfnBranch(psState, psLayout, IBR,
									/* dest */&psLayout->auLabels[uDestIdx],
									USC_PREDREG_NONE, IMG_FALSE,
									(psBlock->u.sCond.uSyncEndBitMask & (1<<1))
										? IMG_TRUE : IMG_FALSE,
									EXECPRED_BRANCHTYPE_NORMAL);
			}
		}
		break;
	case CBTYPE_SWITCH:
	case CBTYPE_UNDEFINED:
	case CBTYPE_CONTINUE:
		//SWITCHes should have been translated away by now...
		imgabort();
	case CBTYPE_EXIT:
		;
	}
}

static
IMG_PUINT32 AllocArrayOfNegOnes(PINTERMEDIATE_STATE psState, IMG_UINT32 uSize)
/******************************************************************************
 FUNCTION		: AllocArrayOfNegOnes

 DESCRIPTION	: Allocates an array of IMG_UINT32s and fills it with -1's

 PARAMETERS		: psState	- Compiler intermediate state
				  uSize		- number of IMG_UINT32 elements for array

 RETURNS		: Pointer to new array.
******************************************************************************/
{
	IMG_PUINT32 auReturn;
	usc_alloc_num(psState, auReturn, uSize);
	memset(auReturn, UINT_MAX, uSize * sizeof(IMG_UINT32));
	return auReturn;
}

static INLINE 
IMG_VOID CheckAndSet(PINTERMEDIATE_STATE psState, IMG_PUINT32 puVar,
							IMG_UINT32 uInit, IMG_UINT32 uNew)
/******************************************************************************
 FUNCTION	: CheckAndSet

 PURPOSE	: Macro-ish function to set a variable; in DEBUG builds, first
			  checks the variable hasn't already been set to a different value.

 PARAMETERS	: psState	- Compiler intermediate state
			  puVar		- Variable to set
			  uInit		- for DEBUG builds, initial value meaning 'unset'
			  uNew		- value to which to set variable
******************************************************************************/
{
#if defined(DEBUG)
	if (*puVar != uInit)
	{
		ASSERT (*puVar == uNew);
	}
	else //{
#else
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(uInit);
#endif /*defined(DEBUG)*/
	*puVar = uNew;
}

IMG_INTERNAL
IMG_VOID LayoutProgram(PINTERMEDIATE_STATE psState, LAY_INSTRS pfnInstrs,
					   LAY_BRANCH pfnBranch, LAY_LABEL pfnLabel, 
					   LAY_POINT_ACTIONS pfnPointActions, 
					   LAY_ALIGN_TO_EVEN pfnAlignToEven, 
					   LAYOUT_PROGRAM_PARTS eLayoutProgramParts)
/******************************************************************************
 FUNCTION	: LayoutProgram

 PURPOSE	: Invokes callbacks according to the layout of the HW code to be
			  generated - i.e. allows passes to perform modifications that are
			  layout-sensitive. (Specifially, NOSCHED and actual output!)

 PARAMETERS	: psState	- Current compilation context
			  pfnInstrs	- Callback to invoke to output/model the instructions
						  forming the body of each block
			  pfnBranch	- Callback to output/model an IBR/ICALL instruction
						  jumping to a label in the layed-out instructions
			  pfnLabel	- Callback to output/model a label
			  pfnNop	- Optional callback to invoke when a NOP is needed

 RETURNS	: Nothing
******************************************************************************/
{
	LAYOUT_INFO sLayout;
	PFUNC psFunc;
	
	pfnPointActions(psState, &sLayout, LAYOUT_INIT); 
	sLayout.eLastOpcode = INOP;
	sLayout.bLastLabelWasSyncEndDest = IMG_FALSE;
	sLayout.bThisLabelIsSyncEndDest = IMG_FALSE;
	sLayout.pfnInstrs = pfnInstrs;
	sLayout.pfnBranch = pfnBranch;
	sLayout.pfnLabel = pfnLabel;
	sLayout.pfnPointActions = pfnPointActions;
	sLayout.pfnAlignToEven = pfnAlignToEven;
	
	psState->uNextLabel = psState->uMaxLabel + 1;
	if(eLayoutProgramParts == LAYOUT_MAIN_PROGRAM || eLayoutProgramParts == LAYOUT_WHOLE_PROGRAM)
	{
		/* Encode the sub-routines and main program (not SA Prog) */
		for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
		{
			if (psFunc == psState->psSecAttrProg) continue;

			/*
				Sort the blocks into the order in which we will output them. We use
				DebugPrintSF - i.e. the same preorder as for debug printing.
				This isn't terribly clever, really we should pick an ordering that
				tries to maximise fallthroughs, avoids jumps if !p2, etc. - TODO
			*/
			DoOnCfgBasicBlocks(psState, &(psFunc->sCfg), DebugPrintSF, NULL, IMG_TRUE, NULL);
			ASSERT (psFunc->sCfg.psEntry->uIdx == 0 || psFunc->sCfg.psExit->uNumPreds == 0);

			//setup labels...
			sLayout.auLabels = AllocArrayOfNegOnes(psState, psFunc->sCfg.uNumBlocks);
			if (psFunc == psState->psMainProg)
			{
				//record start of main program
				DoPhaseStart(psState, &sLayout, LAYOUT_MAIN_PROG_START);
			}
			else
			{
				sLayout.auLabels[psFunc->sCfg.psEntry->uIdx] = psFunc->uLabel;
				
				/*
					If this core has restrictions on which instructions can be paired then, for simplicity,
					insert padding between each function. That way instructions from two different functions
					are never part of the same pair.
				*/
				if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING) == 0)
				{
					pfnAlignToEven(psState, &sLayout);		
				}
			}
	
			//generate the code, filling in label array!
			DoOnCfgBasicBlocks(psState, &(psFunc->sCfg), DebugPrintSF, LayoutBlockBP,
							   IMG_TRUE /*ICALLs*/, &sLayout);
		
			UscFree(psState, sLayout.auLabels);
			sLayout.auLabels = NULL;
			
			if (psFunc != psState->psMainProg) pfnBranch(psState, &sLayout, ILAPC, NULL, USC_PREDREG_NONE, IMG_FALSE, IMG_FALSE, EXECPRED_BRANCHTYPE_NORMAL);

			pfnPointActions(psState, &sLayout, LAYOUT_SUB_ROUT_END);

		}

		pfnPointActions(psState, &sLayout, LAYOUT_MAIN_PROG_END);
		CheckAndSet(psState, &psState->uMainProgLabelCount, 0, psState->uNextLabel);
		
		if(psState->psUseasmContext)
		{
			/*
				For unconditional blocks pointing to itself (halt state)
				pvLabelState needs to be freed
			*/
			if(psState->psUseasmContext->pvLabelState)
			{
				UscFree(psState, psState->psUseasmContext->pvLabelState);
				psState->psUseasmContext->pvLabelState = IMG_NULL;
			}
		}

		/*
			NOSCHED code did the following first, but no idea why...
			comparing with USPBIN SA-prog-only variation (LayoutSAProg, below),
			perhaps it should be an assertion?
		*/
		//sNSState.uLabelCount	= psState->uNumLabels + 1;
	}
	
	if(eLayoutProgramParts == LAYOUT_SA_PROGRAM || eLayoutProgramParts == LAYOUT_WHOLE_PROGRAM)
	{
		/*
			Encode the secondary update program.
		*/
		if (psState->psSecAttrProg)
		{
			/*
				Currently, the layout code expects puInst to be based from psState->puInstructions,
				but for the secondary update program, this isn't the case (the SAUp prog goes into its
				own block of memory!). Thus, we temporarily fiddle psState->puInstructions to point
				to the base of the block of memory into which the current CFG is being assembled...
			*/
			pfnPointActions(psState, &sLayout, LAYOUT_SA_PROG_START);
			/*
				note that old hw.c would have skipped doing this if
				psState->uSAProgInstCount == 0 (this is set by the NOSCHED pass, as
				below). However, it seems harmless if the SA program is empty -
				it just won't do anything...
			*/
			sLayout.auLabels = AllocArrayOfNegOnes(psState, psState->psSecAttrProg->sCfg.uNumBlocks);
			DoOnCfgBasicBlocks(psState, &(psState->psSecAttrProg->sCfg), DebugPrintSF,
							   LayoutBlockBP, IMG_TRUE /*ICALLs*/, &sLayout);
			UscFree(psState, sLayout.auLabels);
			sLayout.auLabels = NULL;

		}
		pfnPointActions(psState, &sLayout, LAYOUT_SA_PROG_END);
	}
}

IMG_INTERNAL 
IMG_BOOL CanSetEndFlag(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/******************************************************************************
 FUNCTION		: CanSetEndFlag

 DESCRIPTION	: Determines whether we can set the END flag on the last
				  instruction of a function (if not, a NOP must be appended)

 PARAMETERS		: psState	- Compiler Intermediate State
				  psFunc	- Function (whose end) to inspect

 RETURNS		: IMG_TRUE iff the last instr of the function's exit block
				  supports the END flag; IMG_FALSE otherwise.
******************************************************************************/
{
	/*
		last block has no body, just a label at start  - unless it has only
		one predecessor, which falls through to it (so no label required)
		- but in that case, said predecessor must end with conditional
		branch(es) to some other successor(s), and we can't set the end
		flag on IBR's either!
	*/
	if (psFunc->sCfg.psExit->psBody == NULL)
	{
		return IMG_FALSE;
	}
	
	/*
		Otherwise check the last instruction in the last block.
	*/
	return SupportsEndFlag(psState, psFunc->sCfg.psExit->psBodyTail);
}


IMG_INTERNAL
IMG_BOOL PointActionsHwCB(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout, LAYOUT_POINT eLayoutPoint)
{
	switch(eLayoutPoint)
	{
	case LAYOUT_INIT:
		psLayout->puInst = psState->puInstructions;
		break;
	case LAYOUT_MAIN_PROG_START:
		CheckAndSet(psState, &psState->uMainProgStart, UINT_MAX, 
			(IMG_UINT32)(((psLayout->puInst) - psState->puInstructions) / 2));
		break;
	case LAYOUT_SUB_ROUT_END:	
		if (psState->puInstructions != NULL && (psLayout->puInst) == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Error generating subroutine USSE code");
		}
		break;
	case LAYOUT_POST_FEEDBACK_START:
		if (psState->uMainProgFeedbackPhase1Start == UINT_MAX)
		{
			psState->uMainProgFeedbackPhase1Start = (IMG_UINT32)((psLayout->puInst - psState->puInstructions) / 2);
		}
		else
		{
			ASSERT ((IMG_INT32)psState->uMainProgFeedbackPhase1Start
						== (psLayout->puInst - psState->puInstructions) / 2);
		}
		break;
	case LAYOUT_POST_SPLIT_START:
		if (psState->uMainProgSplitPhase1Start == UINT_MAX)
		{
			psState->uMainProgSplitPhase1Start = (IMG_UINT32)((psLayout->puInst - psState->puInstructions) / 2);
		}
		else
		{
			ASSERT ((IMG_INT32)psState->uMainProgSplitPhase1Start
						== (psLayout->puInst - psState->puInstructions) / 2);
		}
		break;
	case LAYOUT_MAIN_PROG_END:
		if (psState->puInstructions != NULL && (psLayout->puInst) == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Error generating main USSE code");
		}	
		/*
			Record the overall number of instructions and labels for the main program
		*/
		CheckAndSet(psState, &psState->uMainProgInstCount, 0, (IMG_UINT32)(((psLayout->puInst) - psState->puInstructions) / 2));		
		break;
	case LAYOUT_SA_PROG_START:
		(psLayout->puMainProgInstrs) = psState->puInstructions; //back up...
		psState->puInstructions = psState->puSAInstructions;
		(psLayout->puInst) = psState->puInstructions;
		break;
	case LAYOUT_SA_PROG_END:
		if(psState->psSecAttrProg)
		{
			if (psState->puInstructions && (psLayout->puInst) == NULL)
			{
				USC_ERROR(UF_ERR_NO_MEMORY, "Error generating SA-update USSE code");
			}
			CheckAndSet(psState, &psState->uSAProgInstCount, 0, (IMG_UINT32)(((psLayout->puInst) - psState->puInstructions) / 2));
			
			psState->puInstructions = (psLayout->puMainProgInstrs); //...and restore
		}
		else
		{
			psState->uSAProgInstCount = 0;
		}
		break;
	default:
		return IMG_FALSE;
	}
	return IMG_TRUE;
}
