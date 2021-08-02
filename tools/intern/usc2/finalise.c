/*************************************************************************
 * Name         : finalise.c
 * Title        : Post-regalloc finalisation steps
 * Created      : Nov 2006
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
 * $Log: finalise.c $
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "layout.h"
#include <limits.h>

static IMG_BOOL SupportsSkipInv(PINST psInst)
/*****************************************************************************
 FUNCTION	: SupportsSkipInv
    
 PURPOSE	: Checks if an instruction can accept the skipinv flag.

 PARAMETERS	: psInst		- The instruction to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_SUPPORTSKIPINV)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL 
IMG_BOOL SupportNoSched(PINST psInst)
/*****************************************************************************
 FUNCTION	: SupportNoSched
    
 PURPOSE	: Check if a given instruction can set the NOSCHED flag.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_SUPPORTSNOSCHED) == 0)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static IMG_BOOL IsDeschedule(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsDeschedule
    
 PURPOSE	: Is this an instruction which might cause a deschedule.

 PARAMETERS	: psInst	- The instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psInst->eOpcode == IWDF || GetBit(psInst->auFlag, INST_SYNCSTART))
		? IMG_TRUE : IMG_FALSE;
}

static IMG_BOOL SupportsNoSched_NoPairs(PINTERMEDIATE_STATE	psState,
										PINST				psInst)
/*****************************************************************************
 FUNCTION	: SupportsNoSched_NoPairs
    
 PURPOSE	: Check if an instruction supports the NOSCHED flag on cores
			  without instruction pairing.

 PARAMETERS	: psState		- Internal compiler state
			  psInst		- The instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX545)
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD)
	   )
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX545) */
	/*
		We don't know what instruction the driver will insert here so we can't 
		set the NOSCHED flag.
	*/
	if (psInst->eOpcode == IDRVPADDING)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 FUNCTION	: SetNoSchedFlagForBasicBlockNoPairs
    
 PURPOSE	: LAY_INSTRS callback to set the NOSCHED flag in the body of a
			  block for cores which do *not* do instruction pairing.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block within which to set the nosched flag.
			  puInst		- Where to "output" instructions (see note)
 IN/OUTPUT	: psLayout		- Updated with information about the opcode of the
							  instruction most recently output (includes NOPs and branches)			  
 
 NOTE		: During NOSCHED, puInst should not be dereferenced, and only
			  represents the current offset from the program start in
			  psState->puInstructions.
			  
 RETURNS	: New value of puInst, i.e. to "output" what comes after this block
******************************************************************************/
static IMG_PUINT32 SetNoSchedFlagForBasicBlockNoPairs(PINTERMEDIATE_STATE	psState,
													  PCODEBLOCK			psBlock,
													  IMG_PUINT32			puInst,
													  PLAYOUT_INFO			psLayout)
{
	PINST					psInst;
	PINST					psNextInst;
	IREGLIVENESS_ITERATOR	sIRegIterator;
	IMG_UINT32				uPrevIRegsLive;

	/*
		Initialize state for tracking when the internal registers are live.
	*/
	UseDefIterateIRegLiveness_Initialize(psState, psBlock, &sIRegIterator);

	/*
		Setup the no-sched flag for each instruction within this basic-block, tracking
		the IRegs live as we go, and adding padding where necessary.

		NB: The no-sched flag (set on either instruction of a pair) stops the
			execution-thread being switched after the FOLLOWING pair. Since
			a thread-switch causes the IReg contents to be lost, we must set
			no-sched to avoid the loss of IReg contents while they are live
			within the block. Furthermore, a WDF instruction, or one with the
			sync-start flag set, will always force an execution-thread switch
			after the pair it is in.
	*/
	uPrevIRegsLive = 0;
	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		IMG_UINT32	uIRegsLive;

		ASSERT (psInst->eOpcode != ICALL);

		/*
			Get the mask of internal registers live before this instruction.
		*/
		uIRegsLive = UseDefIterateIRegLiveness_Current(&sIRegIterator);

		/*
			Save the offset to the start of the padding inserted for the driver
			following the alpha-calculation code
		*/
		if	(psInst == psState->psMainProgFeedbackPhase0EndInst)
		{
			psState->uMainProgFeedbackPhase0End = (IMG_UINT32)((puInst - psState->puInstructions) / 2);
		}

		/*
			If the internal registers are live between this instruction and the
			previous one then set the NOSCHED flag two instructions back.
		*/
		if	(uIRegsLive != 0 || GetBit(psInst->auFlag, INST_DISABLE_SCHED))
		{
			ASSERT(!IsDeschedule(psInst->psPrev));

			if (psInst->psPrev->psPrev == NULL ||
				!SupportsNoSched_NoPairs(psState, psInst->psPrev->psPrev))
			{
				PINST	psNop;

				/*
					At the moment we have

						A
						B
						C

					with the internal registers live between B and C.

					We are going to change to

						A
						NOP
						B
						C

					but there is no way to disable scheduling between A and B so the
					internal registers can't have been live at that point.
				*/
				ASSERT(uPrevIRegsLive == 0);

				psNop = AllocateInst(psState, psInst->psPrev);

				SetOpcode(psState, psNop, INOP);
				SetBit(psNop->auFlag, INST_NOSCHED, 1);

				puInst += 2;

				InsertInstBefore(psState, psBlock, psNop, psInst->psPrev);
			}
			else
			{
				SetBit(psInst->psPrev->psPrev->auFlag, INST_NOSCHED, 1);
			}
		}

		/*
			Update the mask of which internal registers are live.
		*/
		uPrevIRegsLive = uIRegsLive;
		UseDefIterateIRegLiveness_Next(psState, &sIRegIterator, psInst);

		/*
			Update the information about the last instruction we have processed
		*/
		AppendInstruction(psLayout, psInst->eOpcode);
		puInst += 2;
		psNextInst = psInst->psNext;
	}

	return puInst;
}

static IMG_PUINT32 SetNoSchedFlagForBasicBlock(PINTERMEDIATE_STATE	psState,
											   PCODEBLOCK			psBlock,
											   IMG_PUINT32			puInst,
											   PLAYOUT_INFO			psLayout)
/*****************************************************************************
 FUNCTION	: SetNoSchedFlagForBasicBlock
    
 PURPOSE	: LAY_INSTRS callback to set the NOSCHED flag in the body of a
			  block for cores on which instruction pairing *does* occur.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block within which to set the nosched flag.
			  puInst		- Where to "output" instructions (see note)
 IN/OUTPUT	: psLayout		- Updated with information about the opcode of the
							  instruction most recently output (includes NOPs and branches)		  
 
 RETURNS	: New value of puInst, i.e. to "output" what comes after this block

 NOTE 1		: During NOSCHED, puInst should not be dereferenced, and only
			  represents the current offset from the program start in
			  psState->puInstructions.
*******************************************************************************/
{
	IMG_BOOL bRestart;
	IMG_UINT32 uOffset;
	IREGLIVENESS_ITERATOR sIRegIterator;
	ASSERT (psState -> puInstructions == NULL);
	do
	{
		PINST		psInst, psNext;
		PINST		psLastInPair;
		IMG_BOOL	bIRegsLiveInPair;
		IMG_UINT32	uIRegsLiveHistory;
		
		bRestart = IMG_FALSE;
		
		uOffset = (IMG_UINT32)((puInst - psState->puInstructions) / 2);

		/*
			Add padding to avoid an illegal opcode pairing at the start of the block.
		*/
		if	(psBlock->psBody)
		{
			PINST	psFirstInst;

			psFirstInst = psBlock->psBody;

			if	(
					((uOffset % 2) == 1) && 
					IsIllegalInstructionPair(psState, psLayout->eLastOpcode, psFirstInst->eOpcode)
				)
			{
				PINST	psNop;

				psNop = AllocateInst(psState, psFirstInst);

				SetOpcode(psState, psNop, INOP);
				psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

				InsertInstBefore(psState, psBlock, psNop, psFirstInst);
			}
		}

		/*
			IRegs cannot be live at the start of a block
		*/
		UseDefIterateIRegLiveness_Initialize(psState, psBlock, &sIRegIterator);
		bIRegsLiveInPair	= IMG_FALSE;
		uIRegsLiveHistory	= 0;

		psLastInPair = NULL;

		/*
			Setup the no-sched flag for each instruction within this basic-block, tracking
			the IRegs live as we go, and adding padding where necessary.

			NB: The no-sched flag (set on either instruction of a pair) stops the
				execution-thread being switched after the FOLLOWING pair. Since
				a thread-switch causes the IReg contents to be lost, we must set
				no-sched to avoid the loss of IReg contents while they are live
				within the block. Furthermore, a WDF instruction, or one with the
				sync-start flag set, will always force an execution-thread switch
				after the pair it is in.
		*/
		for (psInst = psBlock->psBody; psInst; psInst = psNext)
		{
			IMG_UINT32	uIRegsLive;

			ASSERT (psInst->eOpcode != ICALL);
			psNext = psInst->psNext;

			uIRegsLive = UseDefIterateIRegLiveness_Current(&sIRegIterator);
			ASSERT(GetBit(psInst->auFlag, INST_DISABLE_SCHED) == 0);
			
			/*
				Save the offset to the start of the padding inserted for the driver
				following the alpha-calculation code
			*/
			if	(psInst == psState->psMainProgFeedbackPhase0EndInst)
			{
				psState->uMainProgFeedbackPhase0End = uOffset;
			}

			/*
				Execution-thread switching always occurs between instruction pairs, so
				if the IRegs are live on an even instruction (i.e. live at the start of
				a pair), we must set no-sched on the preceeding pair to ensure that their
				contents cannot be lost between the pairs.
			*/
			if	((uOffset % 2) == 0 && uIRegsLive != 0)
			{
				PINST psInsertBefore = NULL;
				IMG_UINT32 uRecordLast = UINT_MAX;
				/*
					Internal registers cannot be live at the start of a block, so if the
					current offset is even, it must also be > 0.

					Also, the IRegs cannot be live immediately after an instruction that
					causes a deschedule, which loses the IReg contents.
				*/
				ASSERT(uOffset >= 2);
				ASSERT(!IsDeschedule(psInst->psPrev));

				/* 
					Insert padding where the IRegs have to be live after a pair that
					also contains a WDF. This allows us to set the no-sched flag on
					one of the inserted NOPs, ensuring that the IRegs are not lost.
					 
					For example, 
					
					e)WDF; o)EFO; *Deschedule* e)psInst 
					
					becomes
					
					e)WDF; o)NOP+NS; *Deschedule* e)NOP; o)EFO; e)psInst
				*/
				if	(psInst->psPrev->psPrev == NULL || IsDeschedule(psInst->psPrev->psPrev))
				{
					ASSERT(psInst->psPrev->psPrev == NULL || 
						   !GetBit(psInst->psPrev->psPrev->auFlag, INST_SYNCSTART));

					/*
						Insert 2 NOPs prior to the preceeding instruction (which is the one
						that will have written to the IRegs e.g. an EFO).
					*/
					psInsertBefore = psInst->psPrev;
					uRecordLast = 0;
					
					//~ psPrev = psInst->psPrev;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if	(uInst == 0)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}

				/* 
					Insert padding where there are too few instructions separating
					a WDF and the beginning of a pair where the IRegs have to be live,
					for us to set the no-sched flag. The flag must go on the pair
					before the one after which we want to disable descheduling.

					For example
					
					o)WDF; *Deschedule* e)INST; o)EFO; e)psInst
					
					becomes
					
					o)WDF; *Deschedule* e)NOP; o)NOP+NS; e)INST; o)EFO; e)psInst
				*/
				else if (psLastInPair == NULL || psLastInPair->eOpcode == IWDF)
				{
					/*
						Insert 2 NOPs after the WDF, which will become the
						target pair for the no-sched flag.
					*/
					psInsertBefore = psInst->psPrev->psPrev;
					uRecordLast = 1;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if (uInst == 1)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}

				/*
					Similarly to the case above, insert padding where there are too
					few instructions separating an instruction with syncstart set, and
					the beginning of a pair where the IRegs have to be live. However,
					to avoid splitting the instruction with SyncStart set, and the
					following one, the two NOPs must be inserted after the instruction
					needing the SyncStart.

					for example:
					
					o)SYNCSTART; *Deschedule* e)DSX; o)EFO e)psInst
					
					becomes
					
					o)SYNCSTART; *Deschedule* e)DSX; o)NOP+NS; e)NOP; o)EFO; e)psInst
				*/
				else if (GetBit(psLastInPair->auFlag, INST_SYNCSTART))
				{
					/*
						Insert the 2 NOPs to form the target pair for the
						no-sched flag.
					*/
					psInsertBefore = psInst->psPrev;
					uRecordLast = 0;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if	(uInst == 0)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}
				/*
					Convert
							SYNCSTART;	DSX;	A;	B; psInst

					->

							SYCNSTART;  DSX;	NOP; NOP; A; B; psInst

				where we'd have to set the NOSCHED flag on the SYNCSTART
				instruction.
				*/
			else if (!SupportNoSched(psLastInPair) &&
					(psLastInPair->psPrev != NULL &&
					 GetBit(psLastInPair->psPrev->auFlag, INST_SYNCSTART)))
				{
					/*
						Insert the 2 NOPs to form the target pair for the
						no-sched flag.
					*/
					psInsertBefore = psLastInPair->psNext;
					uRecordLast = 1;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if	(uInst == 1)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}
				/*
					Convert
							X; PADDING; PADDING; B; psInst
					to
							X; PADDING; PADDING; NOP; NOP; B; psInst
				*/
				else if (psLastInPair->eOpcode == IDRVPADDING &&
						 psInst->psPrev->psPrev->eOpcode == IDRVPADDING)
				{
					/*
						Insert the 2 NOPs to form the target pair for the
						no-sched flag.
					*/
					psInsertBefore = psInst->psPrev;
					uRecordLast = 0;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if	(uInst == 0)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}
				/*
					Convert
							PADDING; PADDING; A; B; psInst
					to
							PADDING; PADDING; NOP; NOP; A; B; psInst
				*/
				else if (psLastInPair->psPrev != NULL &&
						 psLastInPair->psPrev->eOpcode == IDRVPADDING && 
						 psLastInPair->eOpcode == IDRVPADDING)
				{
					/*
						Insert the 2 NOPs to form the target pair for the
						no-sched flag.
					*/
					psInsertBefore = psLastInPair->psNext;
					uRecordLast = 1;
					//~ for (uInst = 0; uInst < 2; uInst++)
					//~ {
						//~ PINST	psNop;

						//~ psNop = AllocateInst(psState, psPrev);

						//~ SetOpcode(psState, psNop, INOP);
						//~ psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						//~ InsertInstBefore(psState, psBlock, psNop, psPrev);

						//~ if	(uInst == 1)
						//~ {
							//~ psLastInPair = psNop;
						//~ }
					//~ }

					//~ uOffset += 2;
				}

				if (psInsertBefore)
				{
					IMG_UINT32 uInst;
					ASSERT (uRecordLast != (IMG_UINT32)-1);
					for (uInst = 0; uInst < 2; uInst++)
					{
						PINST	psNop;

						psNop = AllocateInst(psState, psInsertBefore);

						SetOpcode(psState, psNop, INOP);
						psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;

						InsertInstBefore(psState, psBlock, psNop, psInsertBefore);

						if	(uInst == uRecordLast)
						{
							psLastInPair = psNop;
						}
					}

					uOffset += 2;
				}
				/*
					Since the no-sched flag disables de-scheduling at the end of the
					following pair, to preserve the IRegs for this instruction (which
					is the first in a pair) there must be 2 instruction between it
					and the target pair where we will set the flag.
				*/
				ASSERT(psLastInPair == psInst->psPrev->psPrev->psPrev);

				/*
					We need set NOSCHED on one of the instructions in the previous pair.
				*/
				if (SupportNoSched(psLastInPair))
				{
					ASSERT(!GetBit(psLastInPair->auFlag, INST_SYNCSTART));
					SetBit(psLastInPair->auFlag, INST_NOSCHED, 1);
				}
				else if (psLastInPair->psPrev != NULL && SupportNoSched(psLastInPair->psPrev))
				{
					ASSERT(!GetBit(psLastInPair->psPrev->auFlag, INST_SYNCSTART));
					SetBit(psLastInPair->psPrev->auFlag, INST_NOSCHED, 1);
				}
				else
				{
					PINST	psNop;

					/*
						Earlier phases should not have created a situation where the internal
						registers are live across a syncstart.
					*/
					ASSERT(psLastInPair->psPrev == NULL || 
						   !GetBit(psLastInPair->psPrev->auFlag, INST_SYNCSTART));

					/*
						Insert padding instructions, and set the nosched flag on them
						as appropriate. This transforms the instructions as follows:

						e)A; o)B; e)C; o)D; e)psInst;

						becomes

						e)A; o)NOP[+NS]; e)NOP+NS; o)B; e)C; o)D; e)psInst;

						Instruction B was the last instruction of the target pair for the
						no-sched flag, thus NS needed to be set on B or the instruction
						before it (A) to have the desired effect of disabling descheduling
						between the C+D pair and this instruction (psInst). After inserting
						the NOPs, the second NOP becomes the instruction before B, so the
						NS flag must be set there.

						In addition, if the IRegs need to be live before C (preserved between
						pairs A+B and C+D). To accomplish this, there would be a NS flag set
						in the pair before A+B. However, after adding the NOPs, this pair will
						be too far from C+D (it will now disable descheduling between the 2
						NOPs!). So, to disable descheduling between A+B and C+D as required, 
						we must also set NS 3 instructons back from C - on the first NOP.
					*/
					psNop = AllocateInst(psState, psLastInPair);

					SetOpcode(psState, psNop, INOP);
					psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;
					SetBit(psNop->auFlag, INST_NOSCHED, (uIRegsLiveHistory & 2) ? 1 : 0);

					InsertInstBefore(psState, psBlock, psNop, psLastInPair);

					psNop = AllocateInst(psState, psLastInPair);

					SetOpcode(psState, psNop, INOP);
					psNop->asDest[0].uType	= USC_REGTYPE_DUMMY;
					SetBit(psNop->auFlag, INST_NOSCHED, 1);

					InsertInstBefore(psState, psBlock, psNop, psLastInPair);

					uOffset += 2;

					/*
						Inserting the extra NOPs above has split the original target
						pair (A+B) apart, to give:

						... e)X; o)Y; e)A; o)NOP[+NS]; e)NOP+NS; o)B; e)C; o)D; e)psInst;

						bIRegsLiveInPair indicates whether the IRegs are live
						between A and B. If they are, then we must now ensure
						descheduling is also disabled between the two NOPs (that
						is, between pairs A+NOP, and NOP+B). To do that, the NS
						flag needs to be set on either instruction in the X+Y pair
						before A. Since extra NOPs may then have to be added just 
						to allow NS to be set at that point, we are forced to
						restart processing for this block from the beginning.

						However, if the IRegs are also live between instructions B
						and C, we will have already disabled descheduling between
						the original instruciton pairs A+B and C+D, using an NS
						flag on an instruction in the pair before A+B (i.e. the X+Y
						pair!). Thus, after inserting the NOPs, that NS flag will
						end up disabling descheduling between the A+NOP and NOP+B
						pairs instead, so we do not have to restart processing!
					*/
					if	(bIRegsLiveInPair && (uIRegsLiveHistory & 2) == 0)
					{
						bRestart = IMG_TRUE;
						break; //out of enclosing FOR - i.e. to end of while (bRestart)
					}
				}
			}

			/*
				Update the mask of which internal registers are live.
			*/
			uIRegsLiveHistory <<= 1;
			if (uIRegsLive != 0)
			{
				uIRegsLiveHistory |= 1;
			}
			UseDefIterateIRegLiveness_Next(psState, &sIRegIterator, psInst);

			/*
				Update the information about the last instruction we have processed
			*/
			AppendInstruction(psLayout, psInst->eOpcode);
			
			/*
				If this instruction is the first in a new pair (and not the first
				pair), record that the preceeding pair can be used as the no-sched
				target pair. Also, note whether the IRegs were live in that pair.
			*/
			if	((uOffset % 2) == 0 && (uOffset > (IMG_UINT32)(puInst - psState->puInstructions)/2))
			{
				psLastInPair = psInst->psPrev;	

				bIRegsLiveInPair = IMG_FALSE;
				if (uIRegsLiveHistory & 2)
				{
					bIRegsLiveInPair = IMG_TRUE;
				}
			}

			/* Increment the offset */
			uOffset += 1;
		}
	} while (bRestart);
	/*
		IRegs cannot be live between basic-blocks, so there should be none live
		at the end of this one.
	*/
	ASSERT(UseDefIterateIRegLiveness_Current(&sIRegIterator) == 0);

	/*
		Nosched flag setup is complete for this basic-block, record the index
		for the start of the next one.
	*/
	puInst = psState->puInstructions + (uOffset*2);
	return puInst;
}

typedef struct
{
	IMG_PUINT32 puTempGradChans;
	IMG_PUINT32 puRegArrayGrad;
	IMG_UINT32 uPredGrad;
	IMG_UINT32 uPredForNonFlowControl;
	IMG_UINT32 uIndexGrad;
	IMG_PUINT32 puFPInternalGradChans;
} SKIPINV_STATE, *PSKIPINV_STATE;

static IMG_VOID InitSkipinvState(PINTERMEDIATE_STATE psState, SKIPINV_STATE* psSState)
/******************************************************************************
 FUNCTION		: InitSkipinvState

 DESCRIPTION	: Initialises [an area of memory to be] a SKIPINV_STATE

 PARAMETERS		: psState	- Compiler intermediate state
 
 OUTPUT			: psSState	- SKIPINV_STATE to initialise

 RETURNS		: Nothing
******************************************************************************/
{
	IMG_UINT32 uSize;

	memset(psSState, 0, sizeof(*psSState));

	uSize = sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumRegisters * CHANS_PER_REGISTER);
	psSState->puTempGradChans = UscAlloc(psState, uSize);
	memset(psSState->puTempGradChans, 0, uSize);
	
	uSize = sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumVecArrayRegs);
	psSState->puRegArrayGrad = UscAlloc(psState, uSize);
	memset(psSState->puRegArrayGrad, 0, uSize);

	uSize = sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uGPISizeInScalarRegs * CHANS_PER_REGISTER);
	psSState->puFPInternalGradChans = UscAlloc(psState, uSize);
	memset(psSState->puFPInternalGradChans, 0, uSize);
}

static IMG_VOID DeinitSkipinvState(PINTERMEDIATE_STATE psState, PSKIPINV_STATE psSState)
/******************************************************************************
 FUNCTION		: DeinitSkipinvState

 DESCRIPTION	: IFree [an area of memory to be] a SKIPINV_STATE

 PARAMETERS		: psState	- Compiler intermediate state
 
 OUTPUT			: psSState	- SKIPINV_STATE to initialise

 RETURNS		: Nothing
******************************************************************************/
{
	UscFree(psState, psSState->puRegArrayGrad);
	UscFree(psState, psSState->puTempGradChans);
	UscFree(psState, psSState->puFPInternalGradChans);
}

static
IMG_VOID CopySkipinvState(PINTERMEDIATE_STATE psState,
						  SKIPINV_STATE* psSrc,
						  SKIPINV_STATE* psDst)
/*****************************************************************************
 FUNCTION	: CopySkipinvState
    
 PURPOSE	: Copy a SkipinvState

 PARAMETERS	: psState      - Compiler state
              psSrc        - Source

 OUTPUT     : psDst        - Destination
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psSrc == psDst)
	{
		return;
	}

	memcpy(psDst->puTempGradChans, 
		   psSrc->puTempGradChans, 
		   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumRegisters * CHANS_PER_REGISTER));
	memcpy(psDst->puRegArrayGrad,
		   psSrc->puRegArrayGrad,
		   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumVecArrayRegs));
	psDst->uPredGrad = psSrc->uPredGrad;
	psDst->uPredForNonFlowControl = psSrc->uPredForNonFlowControl;
	psDst->uIndexGrad = psSrc->uIndexGrad;
	memcpy(psDst->puFPInternalGradChans,
		   psSrc->puFPInternalGradChans,
		   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uGPISizeInScalarRegs * CHANS_PER_REGISTER));
}

static IMG_BOOL CompareSkipinvStates(PINTERMEDIATE_STATE psState,
									SKIPINV_STATE *psSState,
									SKIPINV_STATE *psSState2)
/*****************************************************************************
 FUNCTION	: CompareSkipinvStates

 PURPOSE	: Compares two SkipinvStates for equality

 PARAMETERS	: psState			   - Compiler intermediate state
			  psSState1, psSState2 - The two SkipinvStates to compare

 RETURNS	: IMG_TRUE if the two states differ; IMG_FALSE if identical
*****************************************************************************/
{
	if (memcmp(psSState->puTempGradChans,
			   psSState2->puTempGradChans,
			   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumRegisters * CHANS_PER_REGISTER)) ||
		memcmp(psSState->puRegArrayGrad,
			   psSState2->puRegArrayGrad,
			   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uNumVecArrayRegs)))
	{
		return IMG_TRUE;
	}
	
	if (psSState->uIndexGrad != psSState2->uIndexGrad) return IMG_TRUE;

	if (memcmp(psSState->puFPInternalGradChans, 
			   psSState2->puFPInternalGradChans, 
			   sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psState->uGPISizeInScalarRegs * CHANS_PER_REGISTER)) != 0)
	{
		return IMG_TRUE;
	}

	if (psSState->uPredGrad != psSState2->uPredGrad) return IMG_TRUE;

	if (psSState->uPredForNonFlowControl != psSState2->uPredForNonFlowControl) return IMG_TRUE;

	return IMG_FALSE;
}

static IMG_UINT32 OrArray(IMG_UINT32	puMergeDest[],
					      IMG_UINT32	puMergeSrc[],
						  IMG_UINT32	uMergeCount)
/*****************************************************************************
 FUNCTION	: OrArray
    
 PURPOSE	: Or together each element of two arrays.

 PARAMETERS	: puMergeDest		- First array.
			  puMergeSrc		- Second array.
			  uMergeCount		- Count of elements in the arrays.

 RETURNS	: Non-zero if any element of puMergeDest was changed.
*****************************************************************************/
{
	IMG_UINT32	uNew;
	IMG_UINT32	i;

	uNew = 0;
	for (i = 0; i < uMergeCount; i++)
	{
		uNew |= puMergeDest[i] & ~puMergeSrc[i];
		puMergeDest[i] |= puMergeSrc[i];
	}

	return uNew;
}

static IMG_BOOL MergeSkipinvState(PINTERMEDIATE_STATE psState,
								  SKIPINV_STATE* psSState,
								  SKIPINV_STATE* psSState2)
/*****************************************************************************
 FUNCTION	: MergeSkipinvState
    
 PURPOSE	: Merge two skipinvalid states together.

 PARAMETERS	: psState	- Compiler intermediate state
              psSState1	- SkipinvState to modify to become merge result
 			  psSState2	- SkipinvState to read (only) and merge in

 RETURNS	: IMG_TRUE if psSState1 was modified by the merge; IMG_FALSE if
			  it was already equal to the merge result.
*****************************************************************************/
{
	IMG_UINT32 uNew = 0;

	uNew |= OrArray(psSState->puTempGradChans,
				    psSState2->puTempGradChans,
					UINTS_TO_SPAN_BITS(psState->uNumRegisters * CHANS_PER_REGISTER));

	uNew |= OrArray(psSState->puRegArrayGrad,
				    psSState2->puRegArrayGrad,
				    UINTS_TO_SPAN_BITS(psState->uNumVecArrayRegs));

	uNew |= psSState->uIndexGrad & ~psSState2->uIndexGrad;
	psSState->uIndexGrad |= psSState2->uIndexGrad;

	uNew |= OrArray(psSState->puFPInternalGradChans,
					psSState2->puFPInternalGradChans,
					UINTS_TO_SPAN_BITS(psState->uGPISizeInScalarRegs * CHANS_PER_REGISTER));

	uNew |= psSState->uPredGrad &= ~psSState2->uPredGrad;
	psSState->uPredGrad |= psSState2->uPredGrad;

	uNew |= psSState->uPredForNonFlowControl & ~psSState2->uPredForNonFlowControl;
	psSState->uPredForNonFlowControl |= psSState2->uPredForNonFlowControl;

	return (uNew != 0) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: SetSkipInvalidFlagForArgIndex
    
 PURPOSE	: Update the skip-invalid state for any index used by a source
			  or destination whose result is needed for gradient calculations.

 PARAMETERS	: psState		- Compiler state.
			  psSState		- Gradients state.
			  psArg			- Instruction source or destination.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetSkipInvalidFlagForArgIndex(PINTERMEDIATE_STATE	psState,
											  PSKIPINV_STATE		psSState,
											  PARG					psArg)
{
	if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
	{
		ASSERT(psArg->uIndexType == USEASM_REGTYPE_INDEX);
		ASSERT(psArg->uIndexNumber == USC_INDEXREG_LOW || psArg->uIndexNumber == USC_INDEXREG_HIGH);
		SetBit(&psSState->uIndexGrad, psArg->uIndexNumber, 1);
	}
}

static IMG_VOID FlagNonFlowControlPredicate(PINTERMEDIATE_STATE psState,
											PSKIPINV_STATE		psSState,
											PARG				psArg)
/*****************************************************************************
 FUNCTION	: FlagNonFlowControlPredicate
    
 PURPOSE	: Check for a predicate used in a non-branch instruction.

 PARAMETERS	: psState		- Compiler state.
			  psSState		- Gradients state.
			  psArg			- Source argument.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_PREDICATE)
	{
		ASSERT(psArg->uNumber < EURASIA_USE_PREDICATE_BANK_SIZE);
		SetBit(&psSState->uPredForNonFlowControl, psArg->uNumber, 1);
	}
}

/*****************************************************************************
 FUNCTION	: SetSkipInvalidFlagForSource
    
 PURPOSE	: Mark a source argument to an instruction as required to be
			  valid for all instances.

 PARAMETERS	: psState		- Compiler state.
			  psSState		- Gradients state.
			  psArg			- Source argument.
			  uLiveChans	- Channels used from the source argument.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetSkipInvalidFlagForSource(PINTERMEDIATE_STATE	psState,
											PSKIPINV_STATE		psSState,
											PARG				psArg,
											IMG_UINT32			uLiveChans)
{
	IMG_PUINT32	puGradChans = NULL;

	if (psArg->uType == USEASM_REGTYPE_PREDICATE)
	{
		ASSERT(psArg->uNumber < EURASIA_USE_PREDICATE_BANK_SIZE);
		SetBit(&psSState->uPredGrad, psArg->uNumber, 1);
		return;
	}

	if (psArg->uType == USEASM_REGTYPE_TEMP)
	{
		ASSERT(psArg->uNumber < psState->uNumRegisters);
		puGradChans = psSState->puTempGradChans;
	}
	else if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		ASSERT(psArg->uNumber < psState->uGPISizeInScalarRegs);
		puGradChans = psSState->puFPInternalGradChans;
	}
	else if (psArg->uType == USC_REGTYPE_REGARRAY)
	{
		ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
		ASSERT(psSState->puRegArrayGrad != NULL);
		SetBit(psSState->puRegArrayGrad, psArg->uNumber, 1);
	}

	if (puGradChans != NULL)
	{
		IMG_UINT32	uExistingMask;

		uExistingMask = GetRegMask(puGradChans, psArg->uNumber);
		SetRegMask(puGradChans, psArg->uNumber, uExistingMask | uLiveChans);
	}

	SetSkipInvalidFlagForArgIndex(psState, psSState, psArg);
}

static IMG_BOOL IsDestUsedForGradients(PINTERMEDIATE_STATE	psState,
									   PARG					psDest,
									   IMG_UINT32			uDestMask,
									   PSKIPINV_STATE		psSState)
/*****************************************************************************
 FUNCTION	: IsDestUsedForGradients
    
 PURPOSE	: Checks if the destination to an instruction is used for gradient
			  calculations.

 PARAMETERS	: psState		- Compiler state.
			  psDest		- Destination to check.
			  uDestMask		- Mask of channels to check.
			  psSState		- Gradients state.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_BOOL	bDestGrad;

	switch (psDest->uType)
	{
		case USC_REGTYPE_REGARRAY:
		{
			ASSERT(psDest->uNumber < psState->uNumVecArrayRegs);
			ASSERT(psSState->puRegArrayGrad != NULL);
			bDestGrad = GetBit(psSState->puRegArrayGrad, psDest->uNumber) ? IMG_TRUE : IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		{
			ASSERT(psDest->uNumber < psState->uNumRegisters);
			bDestGrad = (GetRegMask(psSState->puTempGradChans, psDest->uNumber) & uDestMask) != 0 ? IMG_TRUE : IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			ASSERT(psDest->uNumber < psState->uGPISizeInScalarRegs);
			bDestGrad = (GetRegMask(psSState->puFPInternalGradChans, psDest->uNumber) & uDestMask) != 0 ? IMG_TRUE : IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_INDEX:
		{
			ASSERT(psDest->uNumber == USC_INDEXREG_LOW || psDest->uNumber == USC_INDEXREG_HIGH);
			bDestGrad = GetBit(&psSState->uIndexGrad, psDest->uNumber) ? IMG_TRUE : IMG_FALSE;
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			ASSERT(psDest->uNumber < EURASIA_USE_PREDICATE_BANK_SIZE);
			bDestGrad = GetBit(&psSState->uPredGrad, psDest->uNumber);
			break;
		}
		default:
		{
			bDestGrad = IMG_FALSE;
			break;
		}
	}
	return bDestGrad;
}

/*****************************************************************************
 FUNCTION	: SetSkipInvalidFlagForInst
    
 PURPOSE	: Set the skip invalid flags for a single instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set skipinv flags for.
			  psSState		- Gradients state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetSkipInvalidFlagForInst(PINTERMEDIATE_STATE	psState, 
										  PINST					psInst,
										  PSKIPINV_STATE		psSState)
{
	IMG_BOOL	bNoSkipInvalid = IMG_FALSE;
	IMG_UINT32	uArg;
	IMG_UINT32	uPred;

	/*
		Skip instructions which won't be included in the final program.
	*/
	if (GetBit(psInst->auFlag, INST_NOEMIT))
	{
		return;
	}

	/*
		Default to onceonly off.
	*/
	SetBit(psInst->auFlag, INST_ONCEONLY, 0);

	if (!SupportsSkipInv(psInst))
	{
		SetBit(psInst->auFlag, INST_SKIPINV, 0);
	}
	else if (/*
				Don't set skip-invalid on SMP/SMPBIAS because these instructions compute gradients.
			 */
			 (psInst->eOpcode == ISMP || psInst->eOpcode == ISMPBIAS) ||
			 /*
				We don't track whether data in memory is needed for gradients so assume it always is.
			 */
			 (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE) != 0 ||
			 (psInst->eOpcode == ISTAD))
	{
		SetBit(psInst->auFlag, INST_SKIPINV, 0);
		bNoSkipInvalid = IMG_TRUE;
	}
	/* Is the destination to this instruction needed for gradients. */
	else
	{
		IMG_UINT32	uDestOffset;

		#if defined(OUTPUT_USPBIN)
		/*
			For these USP pseudo-sample instructions, we use the SkipInv flag
			to indicate whether the destination is used by a future gradient-
			based instruciton.

			NB: This allows the USP to correctly set SkipInv when unpacking.
		*/
		if	(
				(psInst->eOpcode >= ISMP_USP) && 
				(psInst->eOpcode <= ISMPBIAS_USP)
			)
		{
			bNoSkipInvalid = IMG_TRUE;
		}
		#endif /* defined(OUTPUT_USPBIN) */

		SetBit(psInst->auFlag, INST_SKIPINV, 1);
		for (uDestOffset = 0; uDestOffset < psInst->uDestCount; uDestOffset++)
		{
			PARG		psDest = &psInst->asDest[uDestOffset];
			IMG_UINT32	uDestMask = GetDestMaskIdx(psInst, uDestOffset);

			/*
				Don't consider an EFO destination which is never used when setting the SKIPINVALID
				flag.
			*/
			if (psInst->eOpcode == IEFO && uDestOffset == EFO_US_DEST && psInst->u.psEfo->bIgnoreDest)
			{
				continue;
			}

			if (IsDestUsedForGradients(psState, psDest, uDestMask, psSState))
			{	
				SetBit(psInst->auFlag, INST_SKIPINV, 0);
				bNoSkipInvalid = IMG_TRUE;
			}
		}
		/*
			If a predicate is used just for controlling branches (not ALU instructions) and
			the program is running in parallel mode then set the ONCEONLY flag on the TEST
			instruction.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0 &&
			GetBit(psInst->auFlag, INST_SKIPINV) &&
			psState->uNumDynamicBranches == 0)
		{
			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST)
			{
				if ((psInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT) &&
					GetBit(&psSState->uPredForNonFlowControl, psInst->asDest[VTEST_PREDICATE_DESTIDX].uNumber) == 0)
				{
					SetBit(psInst->auFlag, INST_ONCEONLY, 1);
				}
			}
			else
			{
				if ((psInst->uDestCount == TEST_PREDICATE_ONLY_DEST_COUNT) &&
					GetBit(&psSState->uPredForNonFlowControl, psInst->asDest[TEST_PREDICATE_DESTIDX].uNumber) == 0)
				{
					SetBit(psInst->auFlag, INST_ONCEONLY, 1);
				}
			}
		}
	}
	/* If we are unconditionally writing a register then we no longer require gradients for it. */
	if (NoPredicate(psState, psInst))
	{
		IMG_UINT32	uDestOffset;

		for (uDestOffset = 0; uDestOffset < psInst->uDestCount; uDestOffset++)
		{
			PARG	psDest = &psInst->asDest[uDestOffset];

			if (psDest->uType != USEASM_REGTYPE_INDEX)
			{
				IMG_PUINT32	puGradChans;

				if (psDest->uType == USEASM_REGTYPE_PREDICATE)
				{
					ASSERT(psDest->uNumber < EURASIA_USE_PREDICATE_BANK_SIZE);
					SetBit(&psSState->uPredGrad, psDest->uNumber, 0);
					SetBit(&psSState->uPredForNonFlowControl, psDest->uNumber, 0);
				}
				else
				{
					if (psDest->uType == USEASM_REGTYPE_TEMP)
					{
						ASSERT(psDest->uNumber < psState->uNumRegisters);
						puGradChans = psSState->puTempGradChans;
					}	
					else if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
					{
						ASSERT(psDest->uNumber < psState->uGPISizeInScalarRegs);
						puGradChans = psSState->puFPInternalGradChans;
					}
					else
					{
						puGradChans = NULL;
					}

					if (puGradChans != NULL)
					{
						IMG_UINT32	uDestMask;
						IMG_UINT32	uExistingMask;

						uDestMask = GetDestMaskIdx(psInst, uDestOffset);

						uExistingMask = GetRegMask(puGradChans, psDest->uNumber);
						SetRegMask(puGradChans, psDest->uNumber, uExistingMask & ~uDestMask);
					}
				}
			}
			else
			{
				SetBit(&psSState->uIndexGrad, psDest->uNumber, 0);
			}
		}
	}
	/* Is a predicate used in a non-flow control instruction. */
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		if (psInst->apsPredSrc[uPred] != NULL)
		{
			FlagNonFlowControlPredicate(psState, psSState, psInst->apsPredSrc[uPred]);
		}
	}
	for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
	{
		PARG	psArg = &psInst->asArg[uArg];
		FlagNonFlowControlPredicate(psState, psSState, psArg);
	}

	/* Do the sources to this instruction require gradients or it is execution predicate instruction */
	if	(	bNoSkipInvalid
			||
			RequiresGradients(psInst)
			#if defined(EXECPRED)
			||
			(g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_EXECPRED)
			#endif /* defined(EXECPRED)*/
		)
	{
		IMG_UINT32 i;

		for (i = 0; i < psInst->uDestCount; i++)
		{
			ASSERT(psInst->apsOldDest[i] == NULL || EqualArgs(&psInst->asDest[i], psInst->apsOldDest[i]));
			SetSkipInvalidFlagForArgIndex(psState, psSState, &psInst->asDest[i]);
		}
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				SetSkipInvalidFlagForSource(psState, psSState, psInst->apsPredSrc[uPred], USC_ALL_CHAN_MASK /* uLiveChans */);
			}
		}
		for (i = 0; i < psInst->uArgumentCount; i++)
		{
			SetSkipInvalidFlagForSource(psState, psSState, &psInst->asArg[i], GetLiveChansInArg(psState, psInst, i));
		}
	}
}

//fwd decl
static IMG_VOID DoSkipInvalid(PINTERMEDIATE_STATE psState, PSKIPINV_STATE asCallStates, PFUNC psFunc, PSKIPINV_STATE psSkip);

#ifdef DEBUG_SKIPINV
static IMG_VOID PrintSkipInv(PSKIPINV_STATE psSState)
{
	IMG_UINT32 i;
	
	printf("[");
	for (i = 0; i < (sizeof(psSState->puTempGrad) / sizeof(psSState->puTempGrad[0])); i++)
	{
		printf("%s%ld", i ? "," : "{", psSState->puTempGrad[i]);
	}
	printf("} ");

	for (i = 0; i < (sizeof(psSState->puPrimAttrGrad) / sizeof(psSState->puPrimAttrGrad[0])); i++)
	{
		printf("%s%ld", i ? "," : "{", psSState->puPrimAttrGrad[i]);
	}
	printf("} ");
	for (i = 0; i < (sizeof(psSState->puRegArrayGrad) / sizeof(psSState->puRegArrayGrad[0])); i++)
	{
		printf("%s%ld", i ? "," : "{", psSState->puRegArrayGrad[i]);
	}
	printf("} ");

	printf("%ld %ld %ld %ld]",
		psSState->uIndexGrad,
		psSState->uFPInternalGrad,
		psSState->uPredGrad,
		psSState->uPredForNonFlowControl);
}
#endif

static IMG_BOOL SetSkipInvalidDF(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psBlock,
								 IMG_PVOID				pvResult,
								 IMG_PVOID*				ppvArgs,
								 IMG_PVOID				pvSIState)
/******************************************************************************
 FUNCTION		: SetSkipInvalidDF

 DESCRIPTION	: DATAFLOW_FN for setting the SKIPINVALID flag. Use
				  in Backwards mode, with SKIPINV_STATEs as dataflow values.

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- block to analyse (and (un)set/overwrite flag in).
				  pvResult	- PSKIPINV_STATE for value (at beginning) of block
				  ppvArgs	- Array of PSKIPINV_STATEs for psBlock's successors
				  pvSIState	- PSKIPINV_STATE to array of values for functions

 RETURNS		: IMG_TRUE if the value computed into pvResult is different
				  from that existing before; IMG_FALSE if no change.
******************************************************************************/
{
	PSKIPINV_STATE *ppsSuccStates = (PSKIPINV_STATE*)ppvArgs;
	PSKIPINV_STATE psResult = (PSKIPINV_STATE)pvResult;
	PSKIPINV_STATE psBackup = UscAlloc(psState, sizeof(*psBackup));
	PSKIPINV_STATE asCallStates = (PSKIPINV_STATE)pvSIState;
	PINST psInst;
	IMG_BOOL bResult;
#ifdef DEBUG_SKIPINV
	printf("Calculating value for block %ld index %ld - currently ", psBlock, psBlock->uIdx);
	PrintSkipInv(psResult);
#endif
	InitSkipinvState(psState, psBackup);
	CopySkipinvState(psState, psResult, psBackup);

	//1. merge successor states to get state at bottom of block
	if (psBlock->uNumSuccs)
	{
		IMG_UINT32 uSucc;
		IMG_UINT32 uSkip;
	
		/*
			Is the result also one of our arguments.
		*/
		uSkip = USC_UNDEF;
		for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
		{
			if (ppsSuccStates[uSucc] == psResult)
			{
				uSkip = uSucc;
				break;
			}
		}

		/*
			Merge all the other arguments into the result.
		*/
		if (uSkip == USC_UNDEF)
		{
			CopySkipinvState(psState, ppsSuccStates[0], psResult);
			uSkip = 0;
		}

		for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
		{
			if (uSucc != uSkip)
			{
				MergeSkipinvState(psState, psResult, ppsSuccStates[uSucc]);
			}
		}
	}
	else //no successors = exit => use default initial value
		ASSERT (psBlock == psBlock->psOwner->psExit);
	
	//2. condition at end of block - must be valid even for invalid instances (?!)
	if (psState->uNumDynamicBranches > 0)
	{
		switch (psBlock->eType)
		{
		case CBTYPE_UNCOND:
		case CBTYPE_EXIT:
			//nothing to do
			break;
		case CBTYPE_COND:
			SetBit(&psResult->uPredGrad, psBlock->u.sCond.sPredSrc.uNumber, 1);
			break;
		case CBTYPE_SWITCH:
			imgabort();
		case CBTYPE_UNDEFINED:
		case CBTYPE_CONTINUE:
			imgabort();
		}
	}
	
	//3. instructions in block

	/*
		Setup the skip-invalid flag for each instruciton in this
		basic-block, tracking registers that are needed for gradient
		calculation as we go.
	*/
	for (psInst = psBlock->psBodyTail; psInst != NULL; psInst = psInst->psPrev)
	{
		if (psInst->eOpcode == ICALL)
		{
			SetBit(psInst->auFlag, INST_ONCEONLY, 0);

			/*
				Accumulate the overall state at the start of the call
			*/
			MergeSkipinvState(psState, &asCallStates[psInst->u.psCall->psTarget->uLabel], psResult);
			
			ASSERT(NoPredicate(psState, psInst)); //TODO review?!?!
			DoSkipInvalid(psState, asCallStates, psInst->u.psCall->psTarget, psResult);
		}
		else
		{
			SetSkipInvalidFlagForInst(psState, psInst, psResult);
		}
	}
	
	//that leaves the final SKIPINV_STATE (i.e. at entry) in psResult, as required.
	bResult = CompareSkipinvStates(psState, psBackup, psResult);
	DeinitSkipinvState(psState, psBackup);
	UscFree(psState, psBackup);
#ifdef DEBUG_SKIPINV
	printf("    now    ");
	PrintSkipInv(psResult);
	printf("   - %s\n",bResult ? "different" : "same");
#endif
	return bResult;
}

static IMG_VOID DoSkipInvalid(PINTERMEDIATE_STATE	psState,
							  PSKIPINV_STATE		asCallStates,
							  PFUNC					psFunc,
							  PSKIPINV_STATE		psSkip)
/******************************************************************************
 FUNCTION		: DoSkipInvalid

 DESCRIPTION	: Compute which registers need to be valid for all instances
				  (i.e. pixels - even invalid ones), for a particular function.

 PARAMETERS		: psState		- Compiler intermediate state
				  asCallStates	- array to accumulate the registers that need
								  to be valid at the end of each function
				  psFunc		- Function to scan (and set SKIPINV flag in)
 
 INPUT/OUTPUT	: psSkip		- Value to feed in at bottom of function;
								  on return, updated to value at function entry

 RETURNS		: Nothing
******************************************************************************/
{
	PSKIPINV_STATE asWorking = UscAlloc(psState,
								psFunc->sCfg.uNumBlocks * sizeof(SKIPINV_STATE));
	IMG_UINT32 i;

	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++) InitSkipinvState(psState, &asWorking[i]);
	/*
		copy in initial value for exit block
		(normally DF values are for *beginning* of each block,
		 whereas the initial value is actually for the *end* of the exit block,
		 but it's treated specially in SetSkipInvalidDF so should be ok)
	*/
	CopySkipinvState(psState, psSkip, &asWorking[psFunc->sCfg.psExit->uIdx]);
	
	DoDataflow(psState, psFunc, IMG_FALSE,
		sizeof(SKIPINV_STATE), asWorking, SetSkipInvalidDF, asCallStates);

	//get result as the final value for function entry block
	CopySkipinvState(psState, &asWorking[psFunc->sCfg.psEntry->uIdx], psSkip);

	for (i = 0; i < psFunc->sCfg.uNumBlocks; i++) DeinitSkipinvState(psState, &asWorking[i]);
	UscFree(psState, asWorking);
}

static IMG_BOOL SupportSyncStart(PINST psInst)
/*****************************************************************************
 FUNCTION	: SupportSyncStart
    
 PURPOSE	: Check if a given instruction can set the SYNCSTART flag.

 PARAMETERS	: psInst		- Instruction to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_SUPPORTSYNCSTART) == 0)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
static IMG_VOID InitializeDestsBP(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psBlock,
								  IMG_PVOID				pvNull)
/*****************************************************************************
 FUNCTION	: InitializeDestsBP
    
 PURPOSE	: BLOCK_PROC, makes any partially-written destination register be
			  completely initialized on the first write.
			  (Call on all blocks in any order)

 PARAMETERS	: psState	- Compiler intermediate state
			  psBlock	- Block to modify
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: Nothing.

 NOTE		: Provided the shader does not try to access them, uninitialised 
			  components within registers aren't a problem for the HW. However,
			  there is an issue with the SW simulator, where any uninitialised
			  components make the whole register be treated as uninitialised.
*****************************************************************************/
{
	PINST psInst;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		/*
			Modify write-masks where registers are be partially written,
			  to ensure that all components are intialised by the shader.
		*/
		IMG_UINT32	uDestMask;
		IMG_UINT32	uInitialisedChans;
		IMG_UINT32	uLiveChans;
		IMG_UINT32	uArg;

		/*
			Although USP pseudo-samples have destination masks, forcing all
			channels to be written could result in unnecessary samples being
			performed by the USP. So we ignore USP pseudo-samples here.
		*/
		#if defined(OUTPUT_USPBIN)
		if	(psInst->eOpcode == ISMPUNPACK_USP)
		{
			IMG_UINT32 uDestIdx;
			for(uDestIdx =0; uDestIdx<psInst->uDestCount; uDestIdx++)
			{
				uDestMask			= psInst->auDestMask[uDestIdx];
				if	(uDestMask != USC_DESTMASK_FULL)
				{
					uLiveChans			= psInst->auLiveChansInDest[uDestIdx];
					uInitialisedChans	= psInst->u.psSmpUnpack->auInitialisedChansInDest[uDestIdx];
					for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
					{
						if (psInst->asArg[uArg].uType == psInst->asDest[uDestIdx].uType &&
							psInst->asArg[uArg].uNumber == psInst->asDest[uDestIdx].uNumber &&
							psInst->asArg[uArg].uIndexType == USC_REGTYPE_NOINDEX)
						{
							IMG_UINT32 uLiveChansInArg;
							
							uLiveChansInArg = GetLiveChansInArg(psState, psInst, uArg);
							if (uLiveChansInArg & uInitialisedChans)
							{
								break;
							}
						}
					}
					if (uArg == psInst->uArgumentCount)
					{
						if	(((uLiveChans & uInitialisedChans) & ~uDestMask) == 0)
						{
							PINST	psNewInst;
							psNewInst = AllocateInst(psState, psInst);
							SetBit(psNewInst->auFlag, INST_SKIPINV, 
								   GetBit(psInst->auFlag, INST_SKIPINV));
							SetOpcode(psState, psNewInst, IMOV);
							psNewInst->asDest[0] = psInst->asDest[uDestIdx];
							psNewInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
							psNewInst->asArg[0].uNumber = 0;
							InsertInstBefore(psState, psBlock, psNewInst, psInst);
						}
					}					
				}
			}
			continue;
		}
		#endif /* defined(OUTPUT_USPBIN)*/

		/*
			Skip instructions not updating any registers.
		*/
		if (psInst->uDestCount == 0)
		{
			continue;
		}

		/*
			Nothing to do if the destination mask includes all components already
		*/
		uDestMask = psInst->auDestMask[0];
		if	(uDestMask == USC_DESTMASK_FULL)
		{
			continue;
		}

		/*
			The hardware has a special case for SOPWM writing alpha only to be a
			unified store register (the result goes to the blue channel) 
			so don't change the mask.
		*/
		if (psInst->eOpcode == ISOPWM &&
			psInst->asDest[0].eFmt == UF_REGFORMAT_C10 &&
			psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL &&
			psInst->u.psSopWm->bRedChanFromAlpha &&
			psInst->auDestMask[0] == USC_ALL_CHAN_MASK)
		{
			uLiveChans			= psInst->auLiveChansInDest[0];
			uInitialisedChans	= psInst->uInitialisedChansInDest;
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				if (psInst->asArg[uArg].uType == psInst->asDest[0].uType &&
					psInst->asArg[uArg].uNumber == psInst->asDest[0].uNumber &&
					psInst->asArg[uArg].uIndexType == USC_REGTYPE_NOINDEX)
				{
					IMG_UINT32 uLiveChansInArg;
					
					uLiveChansInArg = GetLiveChansInArg(psState, psInst, uArg);
					if (uLiveChansInArg & uInitialisedChans)
					{
						break;
					}
				}
			}
			if (uArg == psInst->uArgumentCount)
			{
				if	(((uLiveChans & uInitialisedChans) & ~uDestMask) == 0)
				{
					PINST	psNewInst;							
					psNewInst = AllocateInst(psState, psInst);
					SetBit(psNewInst->auFlag, INST_SKIPINV, 
						   GetBit(psInst->auFlag, INST_SKIPINV));
					SetOpcode(psState, psNewInst, IMOV);
					psNewInst->asDest[0] = psInst->asDest[0];
					psNewInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psNewInst->asArg[0].uNumber = 0;
					InsertInstBefore(psState, psBlock, psNewInst, psInst);
				}
			}

			continue;
		}

		/*
			If all the live channels for the destination register are written by
			this instruction, then we can safely increase the destination write
			mask to include ALL channels, so that the register is completely
			initialised by this instruciton.

			NB: Future conditional writes of the destination register may leave
				some register components live at this point in the code, even
				though they would not actually be live at runtime (assuming the
				program does not try to read them before it has written them).
				To avoid verification problems cause by uninitialised registers,
				we restrict the live channels in the destination to those that
				could potentially have been written prior to this instruction.
		*/
		uLiveChans			= psInst->auLiveChansInDest[0];
		uInitialisedChans	= psInst->uInitialisedChansInDest;

		/*
			Avoid generating packs to C10/U8 with 3-bits set in the write mask, 
			(regardless of whether the destination has been initialised beforehand
			or not)
		*/
		switch (psInst->eOpcode)
		{
			case IPCKU8U8:
			case IPCKU8F16:
			case IPCKU8F32:
			case IPCKC10C10:
			case IPCKC10F32:
			case IPCKC10F16:
			{
				if	(
						(uDestMask == 0x7) ||
						(uDestMask == 0xB) ||
						(uDestMask == 0xD) ||
						(uDestMask == 0xE)
					)
				{
					if	(((uLiveChans & uInitialisedChans) & ~uDestMask) == 0)
					{
						if	(uDestMask == 0x7)
						{
							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							continue;
						}
						else if	(uDestMask == 0xE)
						{
							SwapPCKSources(psState, psInst);

							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							continue;
						}
						else if	(EqualArgs(&psInst->asArg[0], &psInst->asArg[1]))
						{
							psInst->auDestMask[0] = USC_DESTMASK_FULL;
							continue;
						}
					}
				}

				break;
			}

			default:
			{
				break;
			}
		}

		/*
			Check if the destination register is live after the instruction. In that
			case there is no point in initializing the register here.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			if (psInst->asArg[uArg].uType == psInst->asDest[0].uType &&
				psInst->asArg[uArg].uNumber == psInst->asDest[0].uNumber &&
				psInst->asArg[uArg].uIndexType == USC_REGTYPE_NOINDEX)
			{
				IMG_UINT32 uLiveChansInArg;
				
				uLiveChansInArg = GetLiveChansInArg(psState, psInst, uArg);
				if (uLiveChansInArg & uInitialisedChans)
				{
					break;
				}
			}
		}
		if (uArg < psInst->uArgumentCount)
		{
			/* did 'break' out of preceding loop - go to next instruction */
			continue;
		}
		if	(((uLiveChans & uInitialisedChans) & ~uDestMask) == 0)
		{
			switch (psInst->eOpcode)
			{
				case IUNPCKS16S8:
				case IUNPCKU16U8:
				case IUNPCKU16U16:
				case IUNPCKU16F16:
				case IPCKU16F32:
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
					ASSERT(psInst->auDestMask[0] == 3 || psInst->auDestMask[0] == 12 || psInst->auDestMask[0] == 15);
					if (psInst->auDestMask[0] & 12)
					{
						SwapPCKSources(psState, psInst);
					}
					psInst->auDestMask[0] = USC_DESTMASK_FULL;
					break;
				}

				case IPCKU8U8:
				case IPCKU8F16:
				case IPCKU8F32:
				case IPCKC10C10:
				case IPCKC10F32:
				case IPCKC10F16:
				{
					if (psInst->auDestMask[0] == 1 ||  
						psInst->auDestMask[0] == 3 ||
						psInst->auDestMask[0] == 4 || 
						psInst->auDestMask[0] == 9 ||
						psInst->auDestMask[0] == 12 ||
						EqualPCKArgs(psState, psInst))
					{
						psInst->auDestMask[0] = USC_DESTMASK_FULL;
					}
					else if (psInst->auDestMask[0] == 6 || psInst->auDestMask[0] == 8 || psInst->auDestMask[0] == 2)
					{
						SwapPCKSources(psState, psInst);

						psInst->auDestMask[0] = USC_DESTMASK_FULL;
					}
					else if (psInst->auDestMask[0] != 15)
					{
						PINST	psNewInst;

						ASSERT(psInst->auDestMask[0] == 5 || psInst->auDestMask[0] == 10);

						psNewInst = AllocateInst(psState, psInst);

						SetBit(psNewInst->auFlag, INST_SKIPINV, 
							   GetBit(psInst->auFlag, INST_SKIPINV));
						SetOpcode(psState, psNewInst, IMOV);
						psNewInst->asDest[0] = psInst->asDest[0];
						psNewInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
						psNewInst->asArg[0].uNumber = 0;
						InsertInstBefore(psState, psBlock, psNewInst, psInst);
					}
					break;
				}
				default:
				{
					psInst->auDestMask[0] = USC_DESTMASK_FULL;
					break;
				}
			}
		}
	}
}
#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

/*****************************************************************************
 FUNCTION	: AddSMRForRelativeAddressing
    
 PURPOSE	: Adds an SMR instruction to the start of the program, to limit
			  ranges of relatively addressed input data (actually the texture
			  coordinates).

 PARAMETERS	: psState - Compiler state.
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID AddSMRForRelativeAddressing(PINTERMEDIATE_STATE	psState)
{
	PINST		psSmrInst;
	IMG_UINT32	uArg;
	PCODEBLOCK	psFirstBlock = psState->psMainProg->sCfg.psEntry;

	/*
		No SMR needed for GLSL-derived shader, or if the input data isn't
		relatively addressed
	*/
	if	(
			(psState->uCompilerFlags & UF_GLSL) != 0 ||
			(psState->uFlags & USC_FLAGS_INPUTRELATIVEADDRESSING) == 0 ||
			psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL
		)
	{
		return;
	}

	/*
		Setup and insert an SMR to limit the relative addressing range to
		the indexed input texture-coordinates.
	*/
	psSmrInst = AllocateInst(psState, psFirstBlock->psBody);

	SetOpcode(psState, psSmrInst, ISMR);
	psSmrInst->asDest[0].uType = USC_REGTYPE_DUMMY;
	for (uArg = 0; uArg < 4; uArg++)
	{
		psSmrInst->asArg[uArg].uType = USEASM_REGTYPE_IMMEDIATE;
		psSmrInst->asArg[uArg].uNumber = psState->sShader.psPS->uIndexedInputRangeLength;
	}

	ASSERT(psFirstBlock);//->uType == CBTYPE_BASIC);

	InsertInstBefore(psState, 
					 psFirstBlock, 
					 psSmrInst, 
					 psFirstBlock->psBody);
}

/*****************************************************************************
 FUNCTION	: DetermineNonAnisoTextureStagesBP
    
 PURPOSE	: Detrmine which textures should not be sampled using
			  anisotropic filtering

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID DetermineNonAnisoTextureStagesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
{
	/*
		Scan the instructions, looking for textures that are sampled with an
		explicit LOD. These should have anisotropic  filtering disabled by the driver.
	*/
	PINST psInst;
	PVR_UNREFERENCED_PARAMETER(pvNull);

	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		if	(
				#if defined(OUTPUT_USPBIN)
				psInst->eOpcode == ISMPREPLACE_USP ||
				#endif /* defined(OUTPUT_USPBIN) */
				psInst->eOpcode == ISMPREPLACE
			)
		{
			SetBit(psState->auNonAnisoTexStages, psInst->u.psSmp->uTextureStage, 1);
		}
	}
}

static IMG_VOID SetSkipInvAllInstsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/******************************************************************************
 FUNCTION		: SetSkipInvAllInstsBP

 DESCRIPTION	: BLOCK_PROC, simply sets SKIPINV flag on all instructions
				  supporting it.

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block in which to set flag
				  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS		: Nothing
******************************************************************************/
{
	PINST psInst;
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		SetBit(psInst->auFlag, INST_SKIPINV, (SupportsSkipInv(psInst) ? 1 : 0));
	}
}


/*****************************************************************************
 FUNCTION	: SetupSkipInvalidFlag
    
 PURPOSE	: Setup the 'skip-invalid' instruction execution flag where
			  appropriate, for all instructions within the shader.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: none
*****************************************************************************/
IMG_INTERNAL
IMG_VOID SetupSkipInvalidFlag(PINTERMEDIATE_STATE psState)
{
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PSKIPINV_STATE asCallStates = UscAlloc(psState, psState->uMaxLabel * sizeof(SKIPINV_STATE));
		PFUNC psFunc;
		IMG_UINT32 i;

		/*
			Allocate space for the registers needing gradients at the start of each
			function and in the main program
		*/
		for (i = 0; i < psState->uMaxLabel; i++) InitSkipinvState(psState, &asCallStates[i]);

		/*
			The onceonly flag can't be set on instruction writing the texkill result.
		*/
		if	(psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
		{
			ASSERT(psState->sShader.psPS->psTexkillOutput->auVRegNum[0] == USC_OUTPUT_TEXKILL_PRED);
			SetBit(&asCallStates[psState->psMainProg->uLabel].uPredForNonFlowControl, USC_OUTPUT_TEXKILL_PRED, 1);
		}
	
		/*
			Setup the skip invalid flag for all instrucitons within each
			sunroutine, in nesting order (shallowest to deepest).
		*/
		for (psFunc = psState->psFnOutermost; psFunc; psFunc = psFunc->psFnNestInner)
		{	
			if (psFunc == psState->psSecAttrProg) continue;
			DoSkipInvalid(psState, asCallStates, psFunc, &asCallStates[psFunc->uLabel]);
		}

		for (i = 0; i < psState->uMaxLabel; i++) DeinitSkipinvState(psState, &asCallStates[i]);
		UscFree(psState, asCallStates);

		if (psState->psSecAttrProg)
		{
			/*
				Also set the skipinvalid flag for all instructions in the secondary load program.
				(for vertex shaders, this was done above)
			*/
			DoOnCfgBasicBlocks(psState, &(psState->psSecAttrProg->sCfg), ANY_ORDER, SetSkipInvAllInstsBP, IMG_TRUE/*CALLs*/, NULL);
		}

	}
	else
	{
		DoOnAllBasicBlocks(psState, ANY_ORDER, SetSkipInvAllInstsBP, IMG_TRUE /* bHandlesCalls */, NULL);
	}
}

IMG_INTERNAL
IMG_BOOL CanStartFetch(PINTERMEDIATE_STATE	psState,
					   PINST				psInst)
/*****************************************************************************
 FUNCTION	: CanStartFetch

 PURPOSE	: Check for if an instruction is suitable to be the first instruction
			  in a sequence of memory loads from consecutive addresses which
			  are combined into a single fetch instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: Nothing. 
*****************************************************************************/
{
	PARG		psStaticOffset;
	IMG_UINT32	uStaticOffset;
	PARG		psStride; // PRQA S 3203

	/*
		Only memory loads can be part of fetches.
	*/
	if (psInst->eOpcode != ILOADMEMCONST)
	{
		return IMG_FALSE;
	}

	/*
		Get the source arguments.
	*/
	psStaticOffset = &psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];
	ASSERT(psStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);
	uStaticOffset = psStaticOffset->uNumber + psState->uMemOffsetAdjust * sizeof(IMG_UINT32);

	psStride = &psInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX];  // PRQA S 3199
	ASSERT(psStride->uType == USEASM_REGTYPE_IMMEDIATE);

	/*
		An LDAD instruction with the FETCH flag adds an extra 4 byte onto the memory
		address. So check we can reduce the (unsigned) static offset supplied in the instruction
		by a corresponding amount. 
	*/
	if (uStaticOffset < LONG_SIZE)
	{
		#if defined(SUPPORT_SGX545)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD) 
		{
			/*
				The extended load instruction doesn't add an extra offset so we can
				make this instruction part of a fetch provided we can convert it to
				ELDD/ELDQ later.
			*/
			if (
					!psInst->u.psLoadMemConst->bRelativeAddress ||
					(psStride->uNumber % LONG_SIZE) != 0 || 
					(uStaticOffset % LONG_SIZE) != 0
			   )
			{
				return IMG_FALSE;
			}
		}
		else
		#endif /* defined(SUPPORT_SGX545) */
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL FindFetchesUsingOffset(PINTERMEDIATE_STATE	psState,
								PINST				psConvertInst,
								IMG_PUINT32			puFetchCount,
								PUSC_LIST			psFetchList)
/*****************************************************************************
 FUNCTION	: FindFetchesUsingOffset

 PURPOSE	: Create a linked list of the LOADMEMCONST instructions which use
			  the dynamic offset written by a particular instruction.

 PARAMETERS	: psState				- Compiler state.
			  psConvertInst			- Instruction writing the dynamic offset.
			  puFetchCount			- Returns the count of LOADMEMCONST instructions.
			  psFetchList			- Head/tail of a the list of LOADMEMCONST instructions.

 RETURNS	: TRUE if all the instruction using the dynamic offset were in the same
			  basic block and all of them were LOADMEMCONST instructions.
*****************************************************************************/
{
	PVREGISTER		psRegister;
	PUSEDEF_CHAIN	psUseDefChain;
	PUSC_LIST_ENTRY	psListEntry;

	ASSERT(psConvertInst->uDestCount == 1);
	ASSERT(psConvertInst->asDest[0].uIndexType == USC_REGTYPE_NOINDEX);

	/*
		Reset return data.
	*/
	*puFetchCount = 0;
	InitializeList(psFetchList);

	/*
		Check uses/defines information is available for the conversion instruction's destination.
	*/
	psRegister = psConvertInst->asDest[0].psRegister;
	if (psRegister == NULL)
	{
		return IMG_FALSE;
	}
	psUseDefChain = psRegister->psUseDefChain;
	if (psUseDefChain->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	/*
		Check all the uses/defines of the conversion instruction's destination.
	*/
	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef->eType == DEF_TYPE_INST)
		{
			/*
				Check the only define is by the conversion instruction.
			*/
			if (!(psUseDef->u.psInst == psConvertInst && psUseDef->uLocation == 0))
			{
				return IMG_FALSE;
			}
		}
		else 
		{
			PINST	psUseInst;

			if (psUseDef->eType != USE_TYPE_SRC)
			{
				return IMG_FALSE;
			}

			/*
				Check all uses are in later instructions in the same flow control block.
			*/
			psUseInst = psUseDef->u.psInst;
			if (psUseInst->psBlock != psConvertInst->psBlock)
			{
				return IMG_FALSE;
			}
			if (psUseInst->uBlockIndex <= psConvertInst->uBlockIndex)
			{
				return IMG_FALSE;
			}

			/*
				Check all the uses are as the dynamic offset source to a memory load.
			*/
			if (psUseInst->eOpcode != ILOADMEMCONST)
			{
				return IMG_FALSE;
			}
			if (psUseDef->uLocation != LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX)
			{
				return IMG_FALSE;
			}

			/*
				Add this instruction to the list of instructions to return.
			*/
			(*puFetchCount)++;
			AppendToList(psFetchList, &psUseInst->sAvailableListEntry);
		}
	}

	return IMG_TRUE;
}

static 
IMG_VOID CopyLoadMemConst(PINTERMEDIATE_STATE psState, PINST psDestInst, PINST psSrcInst)
/*****************************************************************************
 FUNCTION	: CopyLoadMemConst

 PURPOSE	: Copy the destinations and flags from a LOADMEMCONST instruction to a
			  LDAD/ELDD/ELDQ instruction.

 PARAMETERS	: psState				- Compiler state.
			  psDestInst			- Instruction to copy the destinations/flags to.
			  psSrcInst				- Instruction to copy the destinations/flags from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;

	/*
		Copy the instruction destinations.
	*/
	SetDestCount(psState, psDestInst, psSrcInst->uDestCount);
	for (uDestIdx = 0; uDestIdx < psDestInst->uDestCount; uDestIdx++)
	{
		MoveDest(psState, psDestInst, uDestIdx, psSrcInst, uDestIdx);
		MovePartiallyWrittenDest(psState, psDestInst, uDestIdx, psSrcInst, uDestIdx);
		psDestInst->auLiveChansInDest[uDestIdx] = psSrcInst->auLiveChansInDest[uDestIdx];
	}

	/*	
		Copy the repeat count.
	*/
	psDestInst->uRepeat = psSrcInst->u.psLoadMemConst->uFetchCount;

	/*
		Copy the instruction's predicate.	
	*/
	CopyPredicate(psState, psDestInst, psSrcInst);

	/*
		Copy the NOEMIT flag.
	*/
	SetBit(psDestInst->auFlag, INST_NOEMIT, GetBit(psSrcInst->auFlag, INST_NOEMIT));

	/*
		Copy the FETCH flag.
	*/
	SetBit(psDestInst->auFlag, INST_FETCH, GetBit(psSrcInst->auFlag, INST_FETCH));

	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		/*
			Copy information about which shader results are read/written by this instruction.
		*/
		psDestInst->uShaderResultHWOperands = psSrcInst->uShaderResultHWOperands;
	}
	#else
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif /* defined(OUTPUT_USPBIN) */
}

#if defined(SUPPORT_SGX545)
static
IMG_VOID MakeELD(PINTERMEDIATE_STATE	psState,
				 PCODEBLOCK				psCodeBlock,
				 PINST					psLoadMemConstInst,
				 IOPCODE				eOpcode)
/*****************************************************************************
 FUNCTION	: MakeELD

 PURPOSE	: Allocate a ELDD or ELDQ instruction and insert it into a basic block, copying
			  the parameters from a LOADMEMCONST instruction. Then drop the
			  LOADMEMCONST instruction.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block to insert the new instruction into.
			  psInst				- LOADMEMCONST instruction.
			  eOpcode				- Opcode for the new instruction. Either
									IELDD or IELDQ.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psELDInst;
	PARG	psELDStaticOffset;

	ASSERT(eOpcode == IELDD || eOpcode == IELDQ);

	psELDInst = AllocateInst(psState, psLoadMemConstInst);

	SetOpcode(psState, psELDInst, eOpcode);

	/*
		Copy the instruction destinations and flags from the LOADMEMCONST
		instruction.
	*/
	CopyLoadMemConst(psState, psELDInst, psLoadMemConstInst);

	/*	
		Copy the bypass cache flag.
	*/
	psELDInst->u.psLdSt->bBypassCache = psLoadMemConstInst->u.psLoadMemConst->bBypassCache;

	/*
		Copy the pointer aliasing flags.
	*/
	psELDInst->u.psLdSt->uFlags = psLoadMemConstInst->u.psLoadMemConst->uFlags;

	/*
		Change the units of the repeat count to 128-bits for ELDQ.
	*/
	if (eOpcode == IELDQ)
	{
		ASSERT((psELDInst->uRepeat % (DQWORD_SIZE / LONG_SIZE)) == 0);
		psELDInst->uRepeat /= (DQWORD_SIZE / LONG_SIZE);
	}

	/*
		Copy the instruction arguments.
	*/
	MoveSrc(psState, psELDInst, ELD_BASE_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_BASE_ARGINDEX);
	MoveSrc(psState, psELDInst, ELD_DYNAMIC_OFFSET_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX);
	MoveSrc(psState, psELDInst, ELD_STATIC_OFFSET_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_STATIC_OFFSET_ARGINDEX);
	MoveSrc(psState, psELDInst, ELD_RANGE_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_MAX_RANGE_ARGINDEX);
	psELDInst->asArg[ELD_DRC_ARGINDEX].uType = USEASM_REGTYPE_DRC;
	psELDInst->asArg[ELD_DRC_ARGINDEX].uNumber = 0;

	/*
		Convert the units of the static offset to 32 bit or 128 bit quantities.
	*/
	psELDStaticOffset = &psELDInst->asArg[ELD_STATIC_OFFSET_ARGINDEX];
	ASSERT(psELDStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);
	if (eOpcode == IELDD)
	{
		ASSERT((psELDStaticOffset->uNumber % LONG_SIZE) == 0);
		psELDStaticOffset->uNumber /= LONG_SIZE;
	}
	else
	{
		ASSERT((psELDStaticOffset->uNumber % DQWORD_SIZE) == 0);
		psELDStaticOffset->uNumber /= DQWORD_SIZE;
	}

	/*
		Insert the ELDD/ELDQ instruction before the LOADMEMCONST
		instruction.
	*/
	InsertInstBefore(psState, psCodeBlock, psELDInst, psLoadMemConstInst);

	/*	
		Drop the LOADMEMCONST instruction.
	*/
	RemoveInst(psState, psCodeBlock, psLoadMemConstInst);
	FreeInst(psState, psLoadMemConstInst);
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_VOID MakeLD(PINTERMEDIATE_STATE	psState,
				IMG_UINT32			uDataSize,
				PCODEBLOCK			psCodeBlock,	
				PINST				psLoadMemConstInst,
				PARG				psOffsetArg)
/*****************************************************************************
 FUNCTION	: MakeLD

 PURPOSE	: Allocate a LDAB/LDAW/LDAD instruction and insert it into a basic block copying
			  the parameters from a LOADMEMCONST instruction. Then drop the
			  LOADMEMCONST instruction.

 PARAMETERS	: psState				- Compiler state.
			  uDataSize				- Size of the data to load.
			  psCodeBlock			- Block to insert the new instruction into.
			  psInst				- LOADMEMCONST instruction.
			  psOffsetArg			- Intermediate register containing the
									memory offset.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psLDInst;
	IOPCODE	eLDOp;

	/*
		Convert the size of the data to load to an opcode.
	*/
	switch (uDataSize)
	{
		case BYTE_SIZE: eLDOp = ILDAB; break;
		case WORD_SIZE: eLDOp = ILDAW; break;
		case LONG_SIZE: eLDOp = ILDAD; break;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case QWORD_SIZE: eLDOp = ILDAQ; break;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default: imgabort();
	}

	/*
		Allocate the LD instruction.
	*/
	psLDInst = AllocateInst(psState, psLoadMemConstInst);
	psState->uOptimizationHint |= USC_COMPILE_HINT_LOCAL_LOADSTORE_USED;
	InsertInstBefore(psState, psCodeBlock, psLDInst, psLoadMemConstInst);

	SetOpcode(psState, psLDInst, eLDOp);

	/*
		Copy the destination argument and instruction flags from the LOADMEMCONST
		instruction.
	*/
	CopyLoadMemConst(psState, psLDInst, psLoadMemConstInst);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Convert the repeat count to qword units.
	*/
	if (eLDOp == ILDAQ)
	{
		psLDInst->uRepeat /= 2;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Copy the range checking enable flag.
	*/
	psLDInst->u.psLdSt->bEnableRangeCheck = psLoadMemConstInst->u.psLoadMemConst->bRangeCheck;

	/*	
		Copy the bypass cache flag.
	*/
	psLDInst->u.psLdSt->bBypassCache = psLoadMemConstInst->u.psLoadMemConst->bBypassCache;

	/*
		Copy the pointer aliasing flags.
	*/
	psLDInst->u.psLdSt->uFlags = psLoadMemConstInst->u.psLoadMemConst->uFlags;

	/*
		Copy the synchronised flag.
	*/
	psLDInst->u.psLdSt->bSynchronised = psLoadMemConstInst->u.psLoadMemConst->bSynchronised;

	/*
		Copy the memory base argument.
	*/
	MoveSrc(psState, psLDInst, MEMLOADSTORE_BASE_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_BASE_ARGINDEX);

	/*
		Copy the memory offset argument.
	*/
	SetSrcFromArg(psState, psLDInst, MEMLOADSTORE_OFFSET_ARGINDEX, psOffsetArg);

	/*
		Copy the maximum range argument.
	*/
	if (psLoadMemConstInst->u.psLoadMemConst->bRangeCheck)
	{
		MoveSrc(psState, psLDInst, MEMLOAD_RANGE_ARGINDEX, psLoadMemConstInst, LOADMEMCONST_MAX_RANGE_ARGINDEX); 			  
	}
	else
	{
		SetSrcUnused(psState, psLDInst, MEMLOAD_RANGE_ARGINDEX);
	}

	/*
		Set the DRC argument to a dummy value. It will be filled in later.
	*/
	psLDInst->asArg[MEMLOAD_DRC_ARGINDEX].uType = USEASM_REGTYPE_DRC;
	psLDInst->asArg[MEMLOAD_DRC_ARGINDEX].uNumber = 0;

	/*	
		Drop the LOADMEMCONST instruction.
	*/
	RemoveInst(psState, psCodeBlock, psLoadMemConstInst);
	FreeInst(psState, psLoadMemConstInst);
}

static
IMG_VOID ScaleDynamicOffset(PINTERMEDIATE_STATE	psState,
							PCODEBLOCK			psCodeBlock,
							IOPCODE				eScaleOpcode,
							PINST				psMemLoadInst,
							IMG_UINT32			uStaticOffset,
							IMG_UINT32			uStrideInBytes,
							IMG_BOOL			bAddStaticOffset)
/*****************************************************************************
 FUNCTION	: ScaleDynamicOffset

 PURPOSE	: Emit instructions to convert a dynamic index to a suitable form
			  for use with the hardware memory addressing instructions.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block to insert the emitted instructions
									into.
			  eScaleOpcode			- Opcode of the hardware instruction to use to
									do the scaling (either IMA16 or IMAE).
			  psInsertBeforeInst	- Memory load instruction whose dynamic index source
									argument is to be modified.
			  uStaticOffset			- Static offset to add onto the scaled value.
			  uStrideInBytes		- The value to scaled the offset by.
			  bAddStaticOffset		- If TRUE add uStaticOffset onto the low
									word of the result and make the high word
									of the result 1. If this mode is used then
									the high word of the value to scale must
									be zero.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psIma16Inst;
	ARG			sNewDynOffset;

	/*
		Allocate a new temporary for the converted dynamic indx.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_F32, &sNewDynOffset);

	psIma16Inst = AllocateInst(psState, psMemLoadInst);

	SetOpcode(psState, psIma16Inst, eScaleOpcode);

	/*
		Insert the IMA16 instruction immediately before the memory load instruction.
	*/
	InsertInstBefore(psState, psCodeBlock, psIma16Inst, psMemLoadInst);

	SetDestFromArg(psState, psIma16Inst, 0 /* uDestIdx */, &sNewDynOffset);
	
	MoveSrc(psState, psIma16Inst, 0 /* uMoveToSrcIdx */, psMemLoadInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX);
	
	psIma16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psIma16Inst->asArg[1].uNumber = uStrideInBytes;

	psIma16Inst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
	if (bAddStaticOffset)
	{
		psIma16Inst->asArg[2].uNumber = uStaticOffset | (1 << 16);
	}
	else
	{
		psIma16Inst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psIma16Inst->asArg[2].uNumber = 0;
	}

	SetSrcFromArg(psState, psMemLoadInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX, &sNewDynOffset);
}

#if defined(SUPPORT_SGX545)
static
IMG_BOOL FinaliseMemConstLoadToELD(PINTERMEDIATE_STATE	psState, 
								   PCODEBLOCK			psCodeBlock, 
								   PINST				psInst,
								   PARG					psDynOffset,
								   IMG_UINT32			uStaticOffset,
								   PARG					psStride)
/*****************************************************************************
 FUNCTION	: FinaliseMemConstLoadToELD

 PURPOSE	: If possible convert an LOADMEMCONST instruction to either ELDD
			  or ELDQ

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block containing the LOADMEMCONST instruction.
			  psInst				- LOADMEMCONST instruction.
			  psDynOffset			- Dynamic offset argument to the LOADMEMCONST
									instruction.
			  uStaticOffset			- Value of the static offset argument.
			  psStride				- Value of the stride for the dynamic offset.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IOPCODE		eOpcode;
		IMG_UINT32	uSizeInBytes;
		IMG_UINT32	uMaxRangeCheck;
	} asDataTypes[] = 
	{
		{IELDQ, DQWORD_SIZE,	SGX545_USE_EXTLD_MAXIMUM_QWORD_RANGE},
		{IELDD, LONG_SIZE,		SGX545_USE_EXTLD_MAXIMUM_DWORD_RANGE},
	};
	IMG_UINT32	uTypeIdx;
	IMG_UINT32	uStride;

	/*
		Get the stride of the dynamic offset.
	*/
	ASSERT(psStride->uType == USEASM_REGTYPE_IMMEDIATE);
	uStride = psStride->uNumber;

	/*
		Check this core supports the extended load (ELDD/ELDQ) instruction.
	*/
	if (!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD))
	{
		return IMG_FALSE;
	}

	/*
		Check the dynamic offset source fits with the bank restrictions on the offset
		source to ELDD/ELDQ.
	*/
	if (!CanUseSource0(psState, psCodeBlock->psOwner->psFunc, psDynOffset))
	{
		return IMG_FALSE;
	}

	for (uTypeIdx = 0; uTypeIdx < (sizeof(asDataTypes) / sizeof(asDataTypes[0])); uTypeIdx++)
	{
		IMG_UINT32	uTypeSizeInBytes = asDataTypes[uTypeIdx].uSizeInBytes;
		IMG_UINT32	uRangeCheck;
		IMG_UINT32	uRepeatInBytes = psInst->u.psLoadMemConst->uFetchCount * LONG_SIZE;

		/*
			Check the stride, static offset and repeat are all multiples of the 
			data type loaded by this version of the extended load instruction.
		*/
		if (
				(uStride % uTypeSizeInBytes) == 0 &&
				(uStaticOffset % uTypeSizeInBytes) == 0 &&
				(uStaticOffset / uTypeSizeInBytes) <= SGX545_USE1_EXTLD_OFFSET_MAX &&
				(uRepeatInBytes % uTypeSizeInBytes) == 0
		   )
		{
			/*
				The hardware always does range checking for the ELDD/ELDQ instruction. If the input
				didn't ask to enable range checking then use the maximum offset value (effectively 
				turning off the range check).
			*/	
			if (psInst->u.psLoadMemConst->bRangeCheck)
			{
				ASSERT(psInst->asArg[LOADMEMCONST_MAX_RANGE_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
				uRangeCheck = psInst->asArg[LOADMEMCONST_MAX_RANGE_ARGINDEX].uNumber;

				ASSERT((uRangeCheck % uTypeSizeInBytes) == 0);
				uRangeCheck /= uTypeSizeInBytes;
			}
			else
			{
				uRangeCheck = asDataTypes[uTypeIdx].uMaxRangeCheck;
			}

			/*
				Update the stored range checking value.
			*/
			SetSrc(psState, psInst, LOADMEMCONST_MAX_RANGE_ARGINDEX, USEASM_REGTYPE_IMMEDIATE, uRangeCheck, UF_REGFORMAT_F32);

			/*
				If the stride for the dynamic offset is larger than that used by
				the instruction then emit an IMAE instruction to multiply
				the dynamic offset by the stride.
			*/
			if (uStride != uTypeSizeInBytes)
			{
				ScaleDynamicOffset(psState, 
								   psCodeBlock, 
								   IIMAE,
								   psInst, 
								   0 /* uStaticOffset */,
								   uStride / uTypeSizeInBytes,
								   IMG_FALSE /* bAddStaticOffset */);
			}

			/*
				Convert the LOADMEMCONST instruction to ELDD or ELDQ.
			*/
			MakeELD(psState, psCodeBlock, psInst, asDataTypes[uTypeIdx].eOpcode);
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX545) */

static
IMG_VOID FinaliseMemConstLoad(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PINST psInst)
/*****************************************************************************
 FUNCTION	: FinaliseMemConstLoad

 PURPOSE	: Convert LOADMEMCONST instruction to final hardware instructions.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block containing the LOADMEMCONST instruction.
			  psInst				- LOADMEMCONST instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psDynOffset;
	PARG		psStaticOffset;
	IMG_UINT32	uStaticOffset;
	PARG		psStride;
	IMG_BOOL	bZeroDynOffset;
	IMG_UINT32	uPrimSize;

	uPrimSize = psInst->u.psLoadMemConst->uDataSize;

	psDynOffset = &psInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX];

	bZeroDynOffset = IMG_FALSE;
	if (psDynOffset->uType == USEASM_REGTYPE_IMMEDIATE && psDynOffset->uNumber == 0)
	{
		bZeroDynOffset = IMG_TRUE;
	}

	psStaticOffset = &psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];
	ASSERT(psStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);
	uStaticOffset = psStaticOffset->uNumber;
	uStaticOffset += psState->uMemOffsetAdjust * LONG_SIZE;

	psStride = &psInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX];
	ASSERT(psStride->uType == USEASM_REGTYPE_IMMEDIATE);

	#if defined(SUPPORT_SGX545)
	/*
		Check for cases where we can use the extended load instruction.
	*/
	if (FinaliseMemConstLoadToELD(psState, psCodeBlock, psInst, psDynOffset, uStaticOffset, psStride))
	{
		return;
	}
	#endif /* defined(SUPPORT_SGX545) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Use qword sized loads if they are supported on this core and the number of dwords to
		fetch is a multiple of two.
	*/
	if (
			(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_LDST_QWORD) != 0 &&
			(psInst->u.psLoadMemConst->uFetchCount % 2) == 0 &&
			(
				GetBit(psInst->auFlag, INST_FETCH) == 0 ||
				uStaticOffset >= QWORD_SIZE
			)
	   )
	{
		uPrimSize = QWORD_SIZE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		If LDAD is used with the fetch flag then the instruction adds the data size onto the memory address. 
		So reduce the static offset correspondingly.
	*/
	if (GetBit(psInst->auFlag, INST_FETCH))
	{
		ASSERT(uStaticOffset >= uPrimSize);
		uStaticOffset -= uPrimSize;
	}

	/*
		Check for a constant load with no dynamic offset.
	*/
	if (bZeroDynOffset)
	{
		ARG	sEncodedStaticOffset;

		/*
			Convert the static offset to the form used by LDAB/LDAW/LDAD.
		*/
		InitInstArg(&sEncodedStaticOffset);
		sEncodedStaticOffset.uType   = USEASM_REGTYPE_IMMEDIATE;
		sEncodedStaticOffset.uNumber = uStaticOffset | (1 << 16);
		sEncodedStaticOffset.eFmt    = UF_REGFORMAT_UNTYPED;

		/*
			Generate a LD instruction from the LOADMEMCONST instruction.
		*/
		MakeLD(psState, uPrimSize, psCodeBlock, psInst, &sEncodedStaticOffset);
		return;
	}

	/*
		Expand the LOADMEMCONST to two instructions

			LOADMEMCONST	DEST, BASE, DOFF, SOFF, STRIDE	// DEST = MEM[BASE + DOFF * STRIDE + SOFF]

		->
			IMA16			TEMP, DOFF, STRIDE, SOFF | (1 << 16)	// TEMP.LOWWORD = DOFF * STRIDE + SOFF
																	// TEMP.HIGHWORD = 1
			LDAD			DEST, BASE, TEMP						// DEST = MEM[BASE + TEMP.LOWWORD * TEMP.HIGHWORD]
																	//		= MEM[BASE + DOFF * STRIDE + SOFF]

		We can assume that the instruction which sets up DOFF has set it's upper 16 bits to zero.
	*/	
	ScaleDynamicOffset(psState, 
					   psCodeBlock, 
					   IIMA16,
					   psInst, 
					   uStaticOffset, 
					   psStride->uNumber,
					   IMG_TRUE /* bAddStaticOffset */);

	/*
		Create a LD with the same base argument as the LOADMEMCONST and using the
		result of the IMA16 instruction as the offset argument.
	*/
	MakeLD(psState, uPrimSize, psCodeBlock, psInst, psDynOffset);
}

static
IMG_VOID OptimizeMemoryAddressCalculationsInst(PINTERMEDIATE_STATE psState, 
											   PCODEBLOCK			psCodeBlock, 
											   PINST				psConvertInst,
											   IMG_BOOL				bConvertFromFloat)
/*****************************************************************************
 FUNCTION	: OptimizeMemoryAddressCalculationsInst

 PURPOSE	: Apply optimizations to memory address calculations in a basic block
			  using a common dynamic offset.

 PARAMETERS	: psState		- Current compilation context
			  psCodeBlock	- Block to optimize
			  psConvertInst	- Instruction writing the dynamic offset.
			  bConvertFromFloat
							- TRUE if the instruction is converting from
							floating point.

 RETURNS	: Nothing
*****************************************************************************/
{	
	IMG_UINT32		uFetchCount;
	USC_LIST		sFetchList;
	IMG_BOOL		bAllRepeatsAreMultiplesOfFour; // PRQA S 3203 2
	IMG_BOOL		bAllStaticOffsetsAreMultiplesOfFour;
	IMG_BOOL		bAllStaticOffsetsAreMultiplesOfSixteen;
	IMG_UINT32		uCommonRelativeStride;
	PARG			psCommonBaseArg;
	PUSC_LIST_ENTRY	psListEntry;
	IMG_BOOL		bFirstInst;

	/*
		Find all the LOADMEMCONST instruction using this dynamic offset.
	*/
	if (!FindFetchesUsingOffset(psState, psConvertInst, &uFetchCount, &sFetchList))
	{
		return;
	}

	ASSERT(uFetchCount >= 0);
	if (uFetchCount == 1)
	{
		PINST	psFetchInst = IMG_CONTAINING_RECORD(sFetchList.psHead, PINST, sAvailableListEntry);
		PARG	psStaticOffset = &psFetchInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];

		ASSERT(psStaticOffset->uType == USEASM_REGTYPE_IMMEDIATE);

		/*
			LDAD does OFFSET.LOWWORD * OFFSET.HIGHWORD so if the static offset is zero we
			can avoid inserting extra instructions when converting LOADMEMCONST->LDAD by
			putting the relative stride in the high word of the register containing the 
			dynamic offset.
		*/
		if (
				psStaticOffset->uNumber == 0 && 
				psState->uMemOffsetAdjust == 1 &&
				GetBit(psFetchInst->auFlag, INST_FETCH)
		   )
		{
			IMG_UINT32	uImmScaleFactor;
			IMG_UINT32	uScaleFactorRegType;
			IMG_UINT32	uScaleFactorRegNum;
			IMG_UINT32	uHwConst;
			IMG_UINT32	uScaleFactor;

			ASSERT(psFetchInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
			uScaleFactor = psFetchInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber;

			/*
				Convert the scale factor to the source format of the instruction converting the dynamic
				offset.
			*/
			if (bConvertFromFloat)
			{
				IMG_FLOAT	fScaleFactor;

				fScaleFactor = (IMG_FLOAT)uScaleFactor;
				uImmScaleFactor = *(IMG_PUINT32)&fScaleFactor;
			}
			else
			{
				uImmScaleFactor = uScaleFactor;
			}

			/*
				Try to get an argument containing the relative stride.
			*/
			uScaleFactorRegType = USC_UNDEF;
			uScaleFactorRegNum = USC_UNDEF;
			if (uImmScaleFactor <= EURASIA_USE_MAXIMUM_IMMEDIATE)
			{
				uScaleFactorRegType = USEASM_REGTYPE_IMMEDIATE;
				uScaleFactorRegNum = uImmScaleFactor;
			}
			else if ((uHwConst = FindHardwareConstant(psState, uImmScaleFactor)) != USC_UNDEF)
			{
				uScaleFactorRegType = USEASM_REGTYPE_FPCONSTANT;
				uScaleFactorRegNum = uHwConst;
			}
			else if (AddStaticSecAttr(psState, uImmScaleFactor, NULL, NULL))
			{
				AddStaticSecAttr(psState, uImmScaleFactor, &uScaleFactorRegType, &uScaleFactorRegNum);
			}

			if (uScaleFactorRegType != USC_UNDEF)
			{
				/*
					Put the relative stride in the second argument to the PCKS16F32/PCKU16U16
					so it gets converted to the same format as the dynamic offset and placed in the 
					high word of the register containing the dynamic offset.
				*/
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psConvertInst->eOpcode == IVPCKS16FLT_EXP)
				{
					IMG_UINT32	uSrc;

					SetSrc(psState, 
						   psConvertInst, 
						   1 * SOURCE_ARGUMENTS_PER_VECTOR + 1, 
						   uScaleFactorRegType, 
						   uScaleFactorRegNum, 
						   UF_REGFORMAT_F32);
					for (uSrc = 1; uSrc < VECTOR_LENGTH; uSrc++)
					{
						SetSrcUnused(psState, psConvertInst, uSrc);
					}
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					SetSrc(psState, psConvertInst, 1 /* uSrcIdx */, uScaleFactorRegType, uScaleFactorRegNum, UF_REGFORMAT_F32);
				}
	
				/*
					Generate a LD instruction from the LOADMEMCONST instruction.
				*/
				MakeLD(psState,
					   psFetchInst->u.psLoadMemConst->uDataSize,
					   psCodeBlock,
					   psFetchInst, 
					   &psFetchInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX]);
				return;
			}
		}
	}

	/*
		Check all the LOADMEMCONST instructions for common characteristics.
	*/
	bAllRepeatsAreMultiplesOfFour = IMG_TRUE; // PRQA S 3198 2
	bAllStaticOffsetsAreMultiplesOfFour = IMG_TRUE;
	bAllStaticOffsetsAreMultiplesOfSixteen = IMG_TRUE;
	psCommonBaseArg = NULL;
	uCommonRelativeStride = USC_UNDEF;
	for (psListEntry = sFetchList.psHead, bFirstInst = IMG_TRUE; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext, bFirstInst = IMG_FALSE)
	{
		PINST		psFetchInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
		IMG_UINT32	uRelativeStride;
		PARG		psRelativeStrideArg;
		PARG		psBaseArg;
		PARG		psStaticOffsetArg;
		IMG_UINT32	uStaticOffset;

		psBaseArg = &psFetchInst->asArg[LOADMEMCONST_BASE_ARGINDEX];

		psStaticOffsetArg = &psFetchInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];
		ASSERT(psStaticOffsetArg->uType == USEASM_REGTYPE_IMMEDIATE);
		uStaticOffset = psStaticOffsetArg->uNumber;

		/*
			Record if any instructions had a static offset which wasn't a multiple of
			four or sixteen.
		*/
		if ((uStaticOffset % 4) != 0)
		{
			bAllStaticOffsetsAreMultiplesOfFour = IMG_FALSE;
		}
		if ((uStaticOffset % 16) != 0)
		{
			bAllStaticOffsetsAreMultiplesOfSixteen = IMG_FALSE;
		}

		psRelativeStrideArg = &psFetchInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX];
		ASSERT(psRelativeStrideArg->uType == USEASM_REGTYPE_IMMEDIATE);
		uRelativeStride = psRelativeStrideArg->uNumber;

		/*
			Record if any instruction had a fetch count which wasn't a multiple of four.
		*/
		if ((psFetchInst->u.psLoadMemConst->uFetchCount % 4) != 0)
		{
			bAllRepeatsAreMultiplesOfFour = IMG_FALSE;
		}

		if (bFirstInst)
		{
			psCommonBaseArg = psBaseArg;
			uCommonRelativeStride = uRelativeStride;
		}
		else
		{
			/*
				Check if this instruction shares a common base address arguments with
				all the previous constant loads.
			*/
			if (psCommonBaseArg != NULL && !EqualArgs(psBaseArg, psCommonBaseArg))
			{
				psCommonBaseArg = NULL;
			}

			/*
				Check if this instruction shares a common relative stride with all
				the previous constant loads.
			*/
			if (uCommonRelativeStride != uRelativeStride)
			{
				uCommonRelativeStride = USC_UNDEF;
			}
		}
	}

	/*
		Check for a case where the CONSTANT_BASE + DOFF * STRIDE calculation can be lifted from each LOADMEMCONST and done
		once when converting the DOFF. 
		
		We can't do this when range checking is enabled because LDAD does the range check against just the 
		(DOFF * STRIDE) + SOFF value. 
		
		There's no point in doing it on cores which support the ELDD/ELDQ instructions because the 
		BASE + DOFF + SOFF calculation can be done directly in these instructions.
	*/
	if (
			#if defined(SUPPORT_SGX545)
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD) == 0 &&
			#endif /* defined(SUPPORT_SGX545) */
			(psState->uCompilerFlags & UF_ENABLERANGECHECK) == 0 && 
			psCommonBaseArg != NULL && 
			psCommonBaseArg->uType == USEASM_REGTYPE_SECATTR &&
			uCommonRelativeStride != USC_UNDEF &&
			uCommonRelativeStride <= EURASIA_USE_MAXIMUM_SIGNED_IMMEDIATE
	   )
	{
		PINST	psIMAEInst;
		ARG		sNewDOff;

		/*
			Change
				CONVERTADDRESS		DOFF, SRC
				[...]
				LOADMEMCONST		DEST, CONSTANT_BASE, DOFF, SOFF
			->
				CONVERTADDRESS		DOFF, SRC
				IMAE				DOFF', DOFF, #STRIDE, CONSTANT_BASE		// DOFF = DOFF * STRIDE + CONSTANT_BASE
				[...]
				LOADMEMCONST		DEST, DOFF', #0, SOFF
		*/
		MakeNewTempArg(psState, UF_REGFORMAT_F32, &sNewDOff);

		psIMAEInst = AllocateInst(psState, psConvertInst);
		
		SetOpcode(psState, psIMAEInst, IIMAE);

		MoveDest(psState, psIMAEInst, 0 /* uMoveToDestIdx */, psConvertInst, 0 /* uMoveFromDestIdx */);

		SetSrcFromArg(psState, psIMAEInst, 0 /* uSrcIdx */, &sNewDOff);
		SetDestFromArg(psState, psConvertInst, 0 /* uDestIdx */, &sNewDOff);

		psIMAEInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psIMAEInst->asArg[1].uNumber = uCommonRelativeStride;

		SetSrcFromArg(psState, psIMAEInst, 2 /* uSrcIdx */, psCommonBaseArg);

		InsertInstBefore(psState, psCodeBlock, psIMAEInst, psConvertInst->psNext);

		/*
			Modify each of the LOADMEMCONST instructions referencing the address value.
		*/
		for (psListEntry = sFetchList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PINST	psFetchInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

			MoveSrc(psState, psFetchInst, LOADMEMCONST_BASE_ARGINDEX, psFetchInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX);
			SetSrc(psState, 
				   psFetchInst, 
				   LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX, 
				   USEASM_REGTYPE_IMMEDIATE, 
				   0 /* uNumber */, 
				   UF_REGFORMAT_F32);
		}

		return;
	}

	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_EXTENDED_LOAD)
	{
		IMG_UINT32	uCommonFactorToRemove = 1;
		 
		/*
			Check for the case where all the LOADMEMCONST instructions can be converted to ELDQ instructions if
			we convert them all to a relative stride of 16.
		*/
		if (bAllRepeatsAreMultiplesOfFour && bAllStaticOffsetsAreMultiplesOfSixteen && (uCommonRelativeStride % 16) == 0)
		{
			uCommonFactorToRemove = uCommonRelativeStride / 16;
		}
		else
		{
			/*
				Check for the case where all the LOADMEMCONST instructions can be converted to ELDD instructions if
				we convert them all to a relative stride of 4.
			*/
			if ((uCommonRelativeStride % 4) == 0 && bAllStaticOffsetsAreMultiplesOfFour)
			{
				uCommonFactorToRemove = uCommonRelativeStride / 4;
			}
		}

		/*
			Remove a common factor from the relative stride of each LOADMEMCONST instruction by multiplying
			the common dynamic offset by the same factor.
		*/
		if (uCommonFactorToRemove > 1)
		{
			PINST	psIma16Inst;
			ARG		sNewDOff;

			for (psListEntry = sFetchList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				PINST	psFetchInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
				PARG	psStrideArg = &psFetchInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX];

				ASSERT(psStrideArg->uType == USEASM_REGTYPE_IMMEDIATE);
				ASSERT((psStrideArg->uNumber % uCommonFactorToRemove) == 0);
				psStrideArg->uNumber /= uCommonFactorToRemove;
			}

			/*
				Generate
					IMA16	DOFF, DOFF, #STRIDE_FACTOR, #0	(DOFF = DOFF * STRIDE_FACTOR)
			*/
			MakeNewTempArg(psState, UF_REGFORMAT_F32, &sNewDOff);

			psIma16Inst = AllocateInst(psState, psConvertInst);
			
			SetOpcode(psState, psIma16Inst, IIMA16);

			SetDestFromArg(psState, psIma16Inst, 0 /* uDestIdx */, &sNewDOff);

			SetSrcFromArg(psState, psIma16Inst, 0 /* uSrcIdx */, &psConvertInst->asDest[0]);

			psIma16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psIma16Inst->asArg[1].uNumber = uCommonFactorToRemove;

			psIma16Inst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
			psIma16Inst->asArg[2].uNumber = 0;

			InsertInstBefore(psState, psCodeBlock, psIma16Inst, psConvertInst->psNext);

			/*
				Modify each of the LOADMEMCONST instructions referencing the address value.
			*/
			for (psListEntry = sFetchList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				PINST	psFetchInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
				
				SetSrcFromArg(psState, psFetchInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX, &sNewDOff);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX545) */
}

IMG_INTERNAL
IMG_VOID FinaliseMemoryLoads(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FinaliseMemoryLoads

 PURPOSE	: Apply some optimizations after the grouping of memory loads into
			  fetches has been set. Then generate final hardware instructions
			  for memory loads.

 PARAMETERS	: psState	- Current compilation context

 RETURNS	: Nothing
*****************************************************************************/
{
	static const struct
	{
		IOPCODE		eOpcode;
		IMG_BOOL	bConvertFromFloat;
	} asAddressConvertOpcodes[] = 
	{
		/* eOpcode			bConvertFromFloat */
		{IPCKS16F32,		IMG_TRUE},
		{IUNPCKU16U16,		IMG_FALSE},
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		{IVPCKS16FLT_EXP,	IMG_TRUE},
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	IMG_UINT32			uOpcode;
	SAFE_LIST_ITERATOR	sIter;

	/*
		Look for instructions converting floating point data to memory
		offsets.
	*/
	for (uOpcode = 0; uOpcode < (sizeof(asAddressConvertOpcodes) / sizeof(asAddressConvertOpcodes[0])); uOpcode++)
	{
		IOPCODE		eOpcode = asAddressConvertOpcodes[uOpcode].eOpcode;
		IMG_BOOL	bConvertFromFloat = asAddressConvertOpcodes[uOpcode].bConvertFromFloat;

		InstListIteratorInitialize(psState, eOpcode, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST	psInst;

			psInst = InstListIteratorCurrent(&sIter);
			if (psInst->auDestMask[0] == USC_ALL_CHAN_MASK && NoPredicate(psState, psInst))
			{
				OptimizeMemoryAddressCalculationsInst(psState, psInst->psBlock, psInst, bConvertFromFloat);
			}
		}
		InstListIteratorFinalise(&sIter);
	}

	/*
		Convert memory loads from constant buffers into final hardware instructions.
	*/
	InstListIteratorInitialize(psState, ILOADMEMCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		FinaliseMemConstLoad(psState, psInst->psBlock, psInst);
	}
	InstListIteratorFinalise(&sIter);
}

static
IMG_BOOL IsStoreOffsetValid(PINTERMEDIATE_STATE psState, PINST psStoreInst, IMG_UINT32 uOffsetToCheck)
/*****************************************************************************
 FUNCTION	: IsStoreOffsetValid

 PURPOSE	: Check if a specified immediate value would be valid as the
			  offset source to a memory store instruction.

 PARAMETERS	: psState			- Current compilation context
			  psStoreInst		- Instruction to check.
			  uOffsetToCheck	- Encoded offset to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return IsImmediateSourceValid(psState, 
								  psStoreInst, 
								  MEMLOADSTORE_OFFSET_ARGINDEX, 
								  0 /* uComponent */,  
								  uOffsetToCheck);
}

IMG_INTERNAL
IMG_VOID FinaliseMemoryStoresBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
FUNCTION	: FinaliseMemoryStoresBP

PURPOSE	: Optimises and finalises memory stores.

PARAMETERS	: psState	- Compiler intermediate state
psBlock	- Block to process
pvNull	- IMG_PVOID to fit callback signature; unused

RETURNS	: Nothing
*****************************************************************************/
{
	typedef struct __MEMST_DATA__ 
	{
		IOPCODE eOpcode;
		ARG sOldBaseAddr;
		PARG psNewBaseAddr;
		IMG_UINT32 uOffset;
		USC_LIST_ENTRY sListEntry;
	} MEMST_DATA, *PMEMST_DATA;

	PINST psInst;
	USC_LIST sMemStChains;
	PUSC_LIST_ENTRY psEntry;
	PVR_UNREFERENCED_PARAMETER(pvNull);

	if(!TestBlockForInstGroup(psBlock, USC_OPT_GROUP_ABS_MEMLDST))
		return;

	InitializeList (&sMemStChains);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		/**
		* For each memst chain, check whether this instruction writes
		* to the base address register.
		*/
		psEntry = sMemStChains.psHead;
		while (psEntry != NULL)
		{
			IMG_UINT32 i;
			PUSC_LIST_ENTRY psNext = psEntry->psNext;
			PMEMST_DATA psMemStData = IMG_CONTAINING_RECORD (psEntry, PMEMST_DATA, sListEntry);
			psEntry = psNext;

			for (i = 0; i < psInst->uDestCount; ++i)
			{
				if (psMemStData->sOldBaseAddr.uType == psInst->asDest[i].uType &&
					psMemStData->sOldBaseAddr.uNumber == psInst->asDest[i].uNumber)
				{
					/* This one does, end the chain. */
					RemoveFromList (&sMemStChains, psEntry->psPrev);
					UscFree (psState, psMemStData);
					break;
				}
			}
		}

		/* Handle any memory store instructions */
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE &&
			g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE)
		{
			IMG_BOOL bNewChain = IMG_TRUE;
			IMG_UINT32 uLowWord;
			IMG_UINT32 uHighWord;
			IMG_UINT32 uMemOffsetInBytes;

			/**
			* Check whether this store can benefit from pre-increment offsetting.
			*/
			if (psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uType != USEASM_REGTYPE_IMMEDIATE)
				continue;
			if (IsStoreOffsetValid(psState, psInst, psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uNumber))
				continue;

			/*
				Turn the encoded offset from the base address back into a simple offset.
			*/
			uLowWord = (psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uNumber >> 0) & 0xFFFF;
			uHighWord = (psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uNumber >> 16) & 0xFFFF;
			uMemOffsetInBytes = uLowWord * uHighWord;

			/**
			* Check if this store can be added to an existing chain.
			*/
			psEntry = sMemStChains.psHead;
			while (psEntry != NULL)
			{
				PUSC_LIST_ENTRY psNext = psEntry->psNext;
				PMEMST_DATA psMemStData = IMG_CONTAINING_RECORD (psEntry, PMEMST_DATA, sListEntry);
				psEntry = psNext;

				/* Check that the data size matches */
				if (psMemStData->eOpcode != psInst->eOpcode)
					continue;

				/* Check for a matching base address */
				if (psMemStData->sOldBaseAddr.uType == psInst->asArg[0].uType &&
					psMemStData->sOldBaseAddr.uNumber == psInst->asArg[0].uNumber)
				{
					/* Calculate the offset difference */
					IMG_INT32	iOffsetDiff = uMemOffsetInBytes - psMemStData->uOffset;
					IMG_UINT32	uAbsOffsetDiff = uAbsOffsetDiff = abs(iOffsetDiff);
					IMG_UINT32	uNewEncodedOffset;

					/**
					* If the offset difference can't be used as an immediate value,
					* try and find another chain to append, else create a new chain.
					*/
					if (uAbsOffsetDiff >= 0xFFFF)
						continue;
					uNewEncodedOffset = ((unsigned)abs(uAbsOffsetDiff)) | (1U << 16);
					if (!IsStoreOffsetValid(psState, psInst, uNewEncodedOffset))
					{
						continue;
					}

					/* Update the chain offset */
					psMemStData->uOffset = uMemOffsetInBytes;

					/* Set the base address to be the current chain base address */
					SetSrcFromArg (psState, psInst, MEMLOADSTORE_BASE_ARGINDEX, psMemStData->psNewBaseAddr);

					/* Set the offset for this store instruction */
					psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uNumber = uNewEncodedOffset;

					/* Allocate a destination for the incremented address */
					SetDestCount (psState, psInst, 2);
					SetDest (psState, psInst, 1, USEASM_REGTYPE_TEMP, GetNextRegister (psState), UF_REGFORMAT_F32);
					psMemStData->psNewBaseAddr = &psInst->asDest[1];

					psInst->u.psLdSt->bPreIncrement = IMG_TRUE;
					if (iOffsetDiff < 0)
					{
						/* Set the pre-decrement flag on the store */
						psInst->u.psLdSt->bPositiveOffset = IMG_FALSE;
					}
					else
					{
						/* Set the pre-increment flag on the store */
						psInst->u.psLdSt->bPositiveOffset = IMG_TRUE;
					}

					/* A new chain won't be required */
					bNewChain = IMG_FALSE;
					break;
				}
			}

			if (bNewChain)
			{
				PINST psMovInst;
				PINST psLimmInst;
				PMEMST_DATA psMemStData;

				/* Record the chain */
				psMemStData = UscAlloc (psState, sizeof (MEMST_DATA));
				psMemStData->eOpcode = psInst->eOpcode;
				psMemStData->sOldBaseAddr = psInst->asArg[0];
				psMemStData->uOffset = uMemOffsetInBytes;
				AppendToList (&sMemStChains, &psMemStData->sListEntry);

				/* Move the base address into a new temp register */
				psMovInst = AllocateInst (psState, IMG_NULL);
				SetOpcode (psState, psMovInst, IMOV);
				SetDest (psState, psMovInst, 0, USEASM_REGTYPE_TEMP, GetNextRegister (psState), UF_REGFORMAT_F32);
				SetSrcFromArg (psState, psMovInst, 0, psInst->asArg);
				InsertInstBefore (psState, psBlock, psMovInst, psInst);

				/* Update the inst data */
				SetSrcFromArg (psState, psInst, 0, psMovInst->asDest);

				/* Convert the offset to an increment */
				psLimmInst = AllocateInst (psState, IMG_NULL);
				SetOpcode (psState, psLimmInst, ILIMM);
				SetDest (psState, psLimmInst, 0, USEASM_REGTYPE_TEMP, GetNextRegister (psState), UF_REGFORMAT_F32);
				SetSrc (psState, psLimmInst, 0, USEASM_REGTYPE_IMMEDIATE, psInst->asArg[MEMLOADSTORE_OFFSET_ARGINDEX].uNumber, UF_REGFORMAT_F32);
				InsertInstBefore (psState, psBlock, psLimmInst, psInst);
				SetSrcFromArg (psState, psInst, MEMLOADSTORE_OFFSET_ARGINDEX, &psLimmInst->asDest[0]);

				/* Allocate a destination for the incremented address */
				SetDestCount (psState, psInst, 2);
				SetDest (psState, psInst, 1, USEASM_REGTYPE_TEMP, GetNextRegister (psState), UF_REGFORMAT_F32);
				psMemStData->psNewBaseAddr = &psInst->asDest[1];

				/* Set the pre-increment flag on the store */
				psInst->u.psLdSt->bPreIncrement = IMG_TRUE;
			}
		}
	}

	/* Free store chains */
	psEntry = sMemStChains.psHead;
	while (psEntry != NULL)
	{
		PUSC_LIST_ENTRY psNext = psEntry->psNext;
		PMEMST_DATA psMemStData = IMG_CONTAINING_RECORD (psEntry, PMEMST_DATA, sListEntry);
		psEntry = psNext;

		/* Memory no longer required, free it */
		UscFree (psState, psMemStData);
	}
}

typedef struct _DRC_INFO
{
	/*
		Index of the earliest instruction before which we must wait for the operations
		associated with this DRC register to finish.
	*/
	IMG_UINT32	uWaitBeforeInstIdx;
	/*
		Estimated index of the instruction where the operations associated with this
		DRC register finish (so a wait after that point costs zero cycles).
	*/
	IMG_UINT32	uCompleteInstIdx;
	/*
		Number of operations currently associated with this DRC register.
	*/
	IMG_UINT32	uNumOperations;
} DRC_INFO, *PDRC_INFO;

typedef struct _WAIT_CONTEXT
{
	/*
		Information about the pending operations associated with each DRC register.
	*/
	DRC_INFO	asDrc[EURASIA_USE_DRC_BANK_SIZE];
} WAIT_CONTEXT, *PWAIT_CONTEXT;

typedef enum
{
	ASYNC_UNIT_TYPE_NONE,
	ASYNC_UNIT_TYPE_TEXTURESAMPLE,
	ASYNC_UNIT_TYPE_MEMORYREAD,
	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	ASYNC_UNIT_TYPE_NORMALISE,
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
	#if defined(SUPPORT_SGX545)
	ASYNC_UNIT_TYPE_INTEGERDIVIDE,
	#endif /* defined(SUPPORT_SGX545) */
} ASYNC_UNIT_TYPE;

static IMG_BOOL IsTextureSample(PINTERMEDIATE_STATE	psState, 
								PINST				psInst)
/*****************************************************************************
 FUNCTION	: IsTextureSample
    
 PURPOSE	: Returns true if it is a texture sample.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the unit type for.
			  
 RETURNS	: IMG_TRUE/IMG_FALSE.
*****************************************************************************/
{
	#if defined(OUTPUT_USPBIN)
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		if ( 
			 ((psInst->eOpcode < ISMP_USP) && (psInst->eOpcode > ISMP_USP_NDR)) && 
			 (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0
		   )
		{
			return IMG_TRUE;
		}
	}
	else
	{
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
		{
			return IMG_TRUE;
		}
	}
	#else
	PVR_UNREFERENCED_PARAMETER(psState);
	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
	{
		return IMG_TRUE;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	return IMG_FALSE;
}

static
ASYNC_UNIT_TYPE GetAsyncUnitType(PINTERMEDIATE_STATE	psState, 
								 PINST					psInst)
/*****************************************************************************
 FUNCTION	: GetAsyncUnitType
    
 PURPOSE	: Get the type of unit which will write the results of an 
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to get the unit type for.
			  
 RETURNS	: The unit type.
*****************************************************************************/
{
	if (IsTextureSample(psState, psInst))
	{
		return ASYNC_UNIT_TYPE_TEXTURESAMPLE;
	}
	else if (psInst->eOpcode == IIDF)
	{
		return ASYNC_UNIT_TYPE_MEMORYREAD;
	}
	#if defined(SUPPORT_SGX545)
	else if (psInst->eOpcode == IIDIV32)
	{
		return ASYNC_UNIT_TYPE_INTEGERDIVIDE;
	}
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	else if (psInst->eOpcode == IFNRM || psInst->eOpcode == IFNRMF16)
	{
		return ASYNC_UNIT_TYPE_NORMALISE;
	}
	#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
	else if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD)
	{
		return ASYNC_UNIT_TYPE_MEMORYREAD;
	}
	else
	{
		return ASYNC_UNIT_TYPE_NONE;
	}
}

IMG_INTERNAL
IMG_BOOL IsAsyncWBInst(PINTERMEDIATE_STATE psState, PINST	psInst)
/*****************************************************************************
 FUNCTION	: IsAsyncWBInst
    
 PURPOSE	: Check for an instruction whose results are written asynchronously
			  to the main pipeline.

 PARAMETERS	: psState       - Compiler state.
			  psInst		- Instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) != 0 ||
			psInst->eOpcode == IIDF ||
			#if defined(SUPPORT_SGX545)
			psInst->eOpcode == IIDIV32 ||
			#endif /*defined(SUPPORT_SGX545)*/
			(IsTextureSample(psState, psInst))
		)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_UINT32 GetWaitInstIdx(PINTERMEDIATE_STATE	psState,
								 PINST					psAsyncInst,
								 IMG_UINT32				uAsyncInstIdx)
/*****************************************************************************
 FUNCTION	: GetWaitInstIdx
    
 PURPOSE	: Get the index of the first point where an instruction
			  be inserted to wait for an operation executed asynchronously
			  to the rest of the pipeline to finish.

 PARAMETERS	: psState			- Compiler state
			  psAsyncInst		- Operation to check.
			  uAsyncInstIdx		- The index of the operation.
			  
 RETURNS	: An instruction index.
*****************************************************************************/
{
	PINST					psInst;
	IMG_UINT32				uInstIdx;
	REGISTER_USEDEF			sAsyncDef;
	#if defined(SUPPORT_SGX545)
	REGISTER_USEDEF			sAsyncUse;
	IMG_BOOL				bCheckAsyncUse;
	#endif /* defined(SUPPORT_SGX545) */
	PINST					psGroupInst;
	IMG_UINT32				uNoschedStartInstIdx;
	IMG_UINT32				uWaitInstIdx;
	ASYNC_UNIT_TYPE			eAsyncInstUnitType;
	IREGLIVENESS_ITERATOR	sIRegIterator;
	IMG_UINT32				uIRegsLive;

	/*
		Two back-to-back LDF instructions causes a big stall in the hardware
		so insert a WDF in between.
	*/
	if (
			GetBit(psAsyncInst->auFlag, INST_FETCH) &&
			psAsyncInst->uRepeat > 0 &&
			psAsyncInst->psNext != NULL &&
			GetBit(psAsyncInst->psNext->auFlag, INST_FETCH) &&
			psAsyncInst->psNext->uRepeat > 0
	   )
	{
		return uAsyncInstIdx + 1;
	}

	/* If this is a LOADMEMCONST with synchronised flag active... */
	if ((g_psInstDesc[psAsyncInst->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE) &&
		(g_psInstDesc[psAsyncInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) &&
		psAsyncInst->u.psLdSt->bSynchronised)
	{
		return uAsyncInstIdx + 1;
	}

	eAsyncInstUnitType = GetAsyncUnitType(psState, psAsyncInst);

	/*
		Create a record of the registers defined by the external operation.
	*/
	InitRegUseDef(&sAsyncDef);
	for (psGroupInst = psAsyncInst; psGroupInst != NULL; psGroupInst = psGroupInst->psGroupNext)
	{
		InstDef(psState, psGroupInst, &sAsyncDef);
	}
	ASSERT(sAsyncDef.bInternalRegsClobbered);
	sAsyncDef.bInternalRegsClobbered = IMG_FALSE;

	/*
		For texture samples on some cores the source registers are read asynchronously so
		we must check for later instruction which overwrite those registers.
	*/
	#if defined(SUPPORT_SGX545)
	bCheckAsyncUse = IMG_FALSE;
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_EXTERNAL_LDST_SMP_UNIT) &&
			(g_psInstDesc[psAsyncInst->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE)
	   )
	{
		IMG_UINT32	puBackendSet[UINTS_TO_SPAN_BITS(SMP_MAXIMUM_ARG_COUNT)];

		/*
			Get the subset of the SMP source arguments which are read asynchronously. The set is
			the same for every instruction in the repeat.
		*/
		GetSmpBackendSrcSet(psState, psAsyncInst, puBackendSet);

		/*
			Create a USE record for the asynchronously read source registers.
		*/
		InitRegUseDef(&sAsyncUse);
		for (psGroupInst = psAsyncInst; psGroupInst != NULL; psGroupInst = psGroupInst->psGroupNext)
		{
			InstUseSubset(psState, psGroupInst, &sAsyncUse, puBackendSet);
		}
		bCheckAsyncUse = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX545) */

	uNoschedStartInstIdx = USC_UNDEF;
	UseDefIterateIRegLiveness_InitializeAtPoint(psState, psAsyncInst->psNext, &sIRegIterator);
	uIRegsLive = 0;
	uWaitInstIdx = USC_UNDEF;
	for (psInst = psAsyncInst->psNext, uInstIdx = uAsyncInstIdx + 1; 
		 psInst != NULL; 
		 psInst = psInst->psNext, uInstIdx++)
	{
		PINST			psGroupInstL;
		IMG_BOOL		bWaitHere;
		IMG_UINT32		uOldIRegsLive;
		ASYNC_UNIT_TYPE	eInstUnitType;

		/*
			Get the type (if any) of the asynchronously unit which will write the results
			of this instruction.
		*/
		eInstUnitType = GetAsyncUnitType(psState, psInst);

		bWaitHere = IMG_FALSE;
		for (psGroupInstL = psInst; psGroupInstL != NULL; psGroupInstL = psGroupInstL->psGroupNext)
		{
			/*
				Check for an instruction using one of the registers defined by
				the asynchronous operation.
			*/
			if (InstUseDefined(psState, &sAsyncDef, psGroupInstL))
			{
				bWaitHere = IMG_TRUE;
				break;
			}
			/*
				Check for an instruction defining one of the registers defined
				by the asynchronous operation. If both instruction write using the
				same asynchronously unit then their writes will still be correctly
				ordered.
			*/
			if (eInstUnitType != eAsyncInstUnitType && InstDefReferenced(psState, &sAsyncDef, psGroupInstL))
			{
				bWaitHere = IMG_TRUE;
				break;
			}
			#if defined(SUPPORT_SGX545)
			/*
				Check for an instruction defining one of registers used by the
				asynchronous operation.
			*/
			if (bCheckAsyncUse && InstDefReferenced(psState, &sAsyncUse, psGroupInstL))
			{
				bWaitHere = IMG_TRUE;
				break;
			}
			#endif /* defined(SUPPORT_SGX545) */
		}
		/*
			If the current instruction is another one which writes its result asynchronously
			we might be forced to insert a WDF before it to avoid having too many results
			associated with a DRC register. If the internal registers are live before the
			instruction then we wouldn't be able to insert a WDF legally so insert it at
			the start of the region where the internal registers are live instead.
		*/
		if (uIRegsLive != 0 && IsAsyncWBInst(psState, psInst))
		{
			bWaitHere = IMG_TRUE;
		}

		/* If this is a mutex release, synchronise now */
		if (psInst->eOpcode == IRELEASE)
		{
			bWaitHere = IMG_TRUE;
		}

		if (bWaitHere)
		{
			/*
				If we are currently in a region where the internal registers are live then
				a WDF can't be inserted here - instead insert it before the first instruction
				which writes the internal registers.
			*/
			if (uIRegsLive != 0)
			{
				uWaitInstIdx = uNoschedStartInstIdx;
			}
			else
			{
				uWaitInstIdx = uInstIdx;
			}
			break;
		}

		/*
			Update the mask of which internal registers are live.
		*/
		uOldIRegsLive = uIRegsLive;
		UseDefIterateIRegLiveness_Next(psState, &sIRegIterator, psInst);
		uIRegsLive = UseDefIterateIRegLiveness_Current(&sIRegIterator);

		/*
			Record the start of a region where scheduling must be disabled.
		*/
		if (uOldIRegsLive == 0 && uIRegsLive != 0)
		{
			uNoschedStartInstIdx = uInstIdx;
		}
	}

	/*
		Free used memory.
	*/
	ClearRegUseDef(psState, &sAsyncDef);
	#if defined(SUPPORT_SGX545)
	if (bCheckAsyncUse)
	{
		ClearRegUseDef(psState, &sAsyncUse);
	}
	#endif /* defined(SUPPORT_SGX545) */

	/*
		If we didn't need to wait anywhere in the current block then wait
		after the last instruction.
	*/
	if (uWaitInstIdx != USC_UNDEF)
	{
		return uWaitInstIdx;
	}
	else
	{
		return uInstIdx;
	}
}

static IMG_VOID InsertWDF(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  IMG_UINT32			uDRCIdx,
						  PINST					psInsertBeforeInst)
/*****************************************************************************
 FUNCTION	: InsertWait
    
 PURPOSE	: Insert an instruction to wait for operations associated with a	
			  particular dependent read counter to finish.

 PARAMETERS	: psState	- Compiler state
			  psBlock	- Basic block to insert the wait instruction into.
			  uDRCIdx	- Index of the dependent read counter to wait for.
			  psInsertBeforeInst
						- Point to insert the wait instruction.
			  
 RETURNS	: None.
 *****************************************************************************/
{
	PINST		psFenceInst; 

	/*
		WDF		DRCi
	*/
	psFenceInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psFenceInst, IWDF);
	psFenceInst->asDest[0].uType = USC_REGTYPE_DUMMY;
	psFenceInst->asDest[0].uNumber = 0;
	psFenceInst->asArg[WDF_DRC_SOURCE].uType = USEASM_REGTYPE_DRC;
	psFenceInst->asArg[WDF_DRC_SOURCE].uNumber = uDRCIdx;
	InsertInstBefore(psState, psBlock, psFenceInst, psInsertBeforeInst);
}

static IMG_VOID InsertWait(PINTERMEDIATE_STATE	psState,
						   PWAIT_CONTEXT		psWaitCtx,
						   PCODEBLOCK			psBlock,
						   IMG_UINT32			uDRCIdx,
						   PINST				psInsertBeforeInst)
/*****************************************************************************
 FUNCTION	: InsertWait
    
 PURPOSE	: Insert an instruction to wait for operations associated with a	
			  particular dependent read counter to finish.

 PARAMETERS	: psState	- Compiler state
			  psWaitCtx	- Stage state.
			  psBlock	- Basic block to insert the wait instruction into.
			  uDRCIdx	- Index of the dependent read counter to wait for.
			  psInsertBeforeInst
						- Point to insert the wait instruction.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PDRC_INFO	psDrc = &psWaitCtx->asDrc[uDRCIdx];

	/*
		Insert the WDF instruction.
	*/
	InsertWDF(psState, psBlock, uDRCIdx, psInsertBeforeInst);

	/*
		Reset information about the operations associated
		with the DRC register.
	*/
	psDrc->uCompleteInstIdx = 0;
	psDrc->uNumOperations = 0;
}

static IMG_VOID SetDependentReadCounter(PINTERMEDIATE_STATE	psState,
										PINST				psInst,
										IMG_UINT32			uDRCIdx)
/*****************************************************************************
 FUNCTION	: SetDependentReadCounter
    
 PURPOSE	: Set the dependent read counter register to be used by an
			  instruction.

 PARAMETERS	: psState	- Compiler state
			  psInst	- Instruction to update.
			  uDRCIdx	- Index of the DRC register the instruction should
						use.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PARG	psDRCArg;

	if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
	{
		psDRCArg = &psInst->asArg[SMP_DRC_ARG_START];
	}
	else if (psInst->eOpcode == IIDF)
	{
		psDRCArg = &psInst->asArg[IDF_DRC_SOURCE];
	}
#if defined(SUPPORT_SGX545)
	else if (psInst->eOpcode == IIDIV32)
	{
		psDRCArg = &psInst->asArg[IDIV32_DRC_ARGINDEX];
	}
#endif /*defined(SUPPORT_SGX545)*/
#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
	else if (psInst->eOpcode == IFNRM || psInst->eOpcode == IFNRMF16)
	{
		psDRCArg = &psInst->asArg[NRM_DRC_SOURCE];
	}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
#if defined(SUPPORT_SGX545)
	else if (psInst->eOpcode == IELDD ||
			 psInst->eOpcode == IELDQ)
	{
		psDRCArg = &psInst->asArg[ELD_DRC_ARGINDEX];
	}
#endif /* defined(SUPPORT_SGX545) */
	else if (psInst->eOpcode == ILDTD)
	{
		psDRCArg = &psInst->asArg[TILEDMEMLOAD_DRC_ARGINDEX];
	}
	else
	{
		psDRCArg = &psInst->asArg[MEMLOAD_DRC_ARGINDEX];
	}
					
	ASSERT(psDRCArg->uType == USEASM_REGTYPE_DRC);
	psDRCArg->uNumber = uDRCIdx;
}

static IMG_VOID CheckInsertWaits(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psBlock,
								 PWAIT_CONTEXT			psWaitCtx,
								 PINST					psInst,
								 IMG_UINT32				uInstIdx)
/*****************************************************************************
 FUNCTION	: CheckInsertWaits
    
 PURPOSE	: Check whether a wait is needed before an instruction.

 PARAMETERS	: psState	- Compiler state
			  psBlock	- Block that contains the instruction.
			  psWaitCtx	- Stage context.
			  psInst	- Instruction to check.
			  uInstIdx	- Index of the instruction.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uDRCIdx;

	for (uDRCIdx = 0; uDRCIdx < EURASIA_USE_DRC_BANK_SIZE; uDRCIdx++)
	{
		PDRC_INFO	psDrc = &psWaitCtx->asDrc[uDRCIdx];

		if (psDrc->uWaitBeforeInstIdx == uInstIdx)
		{
			/*
				Insert a wait instruction.
			*/
			InsertWait(psState, psWaitCtx, psBlock, uDRCIdx, psInst);
		}
	}
}

static PINST InsertIDF(PINTERMEDIATE_STATE psState, 
					   PCODEBLOCK psBlock,
					   PINST psInsertAfterInst)
/*****************************************************************************
 FUNCTION	: InsertIDF
    
 PURPOSE	: Insert an IDF instruction into a basic block.

 PARAMETERS	: psState	- Compiler state
			  psBlock	- Block that to insert into.
			  psInsertAfterInst	
						- Point to insert the instruction at.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST	psFenceInst;

	psFenceInst = AllocateInst(psState, psInsertAfterInst);
	
	SetOpcode(psState, psFenceInst, IIDF);
	psFenceInst->asDest[0].uType = USC_REGTYPE_DUMMY;
	psFenceInst->asDest[0].uNumber = 0;
	psFenceInst->asArg[IDF_DRC_SOURCE].uType = USEASM_REGTYPE_DRC;
	psFenceInst->asArg[IDF_DRC_SOURCE].uNumber = 0;
	psFenceInst->asArg[IDF_PATH_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
	psFenceInst->asArg[IDF_PATH_SOURCE].uNumber = EURASIA_USE1_IDF_PATH_ST;
	InsertInstAfter(psState, psBlock, psFenceInst, psInsertAfterInst);

	return psFenceInst;
}

/*****************************************************************************
 FUNCTION	: InsertWaitsBP
    
 PURPOSE	: BLOCK_PROC, inserts waits for memory reads; call on all blocks
			  in any order

 PARAMETERS	: psState	- Compiler state
			  psBlock	- Block to process
			  pvNull	- IMG_PVOID to fit callback signature; unused
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID InsertWaitsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
{
	PINST			psInst;
	IMG_UINT32		uInstIdx;
	WAIT_CONTEXT	sCtx;
	IMG_UINT32		uDRCIdx;
	PINST			psLastLocalSTInst;

	/*
		Insert data fences to make sure data loaded from memory is consistent
		with data stored.
	*/
	IMG_UINT32 uInst;
	USC_LIST sStores;
	IMG_BOOL bInsertFinalDF;
	PUSC_LIST_ENTRY psEntry;
	PDGRAPH_STATE psDepState;

	typedef struct __STORE_DATA__
	{
		IMG_UINT32 uInst;
		PINST psStoreInst;
		USC_LIST_ENTRY sListEntry;
	} STORE_DATA, *PSTORE_DATA;

	if (psState->uCompilerFlags & UF_OPENCL)
	{
		PINST					psDeschedStartInst;
		IREGLIVENESS_ITERATOR	sIRegIterator;

		InitializeList(&sStores);
		psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);

		UseDefIterateIRegLiveness_Initialize(psState, psBlock, &sIRegIterator);
		psDeschedStartInst = NULL;
		for (psInst = psBlock->psBody, uInst = 0;
			 psInst != NULL;
			 psInst = psInst->psNext, ++uInst)
		{
			if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_ABSOLUTELOADSTORE) != 0)
			{
				if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE)
				{
					/**
					* Record the store.
					*/
					PSTORE_DATA psStoreData = UscAlloc(psState, sizeof (STORE_DATA));
					psStoreData->uInst = uInst;
					psStoreData->psStoreInst = psInst;
					AppendToList(&sStores, &psStoreData->sListEntry);
				}
				else
				{
					/**
					* Check for dependence on a store.
					*/
					psEntry = sStores.psHead;
					while (psEntry != NULL)
					{
						PUSC_LIST_ENTRY psNext = psEntry->psNext;
						PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD(psEntry, PSTORE_DATA, sListEntry);
						psEntry = psNext;

						if (GraphGet(psState, psDepState->psDepGraph, uInst, psStoreData->uInst))
						{
							PINST	psIDFInst;

							/**
							* The instructions interfere, insert a data fence and
							* clear the list of stores.
							*/
							psIDFInst = InsertIDF(psState, psBlock, psDeschedStartInst);
							InsertWait(psState, &sCtx, psBlock, 0, psIDFInst->psNext);

							psEntry = sStores.psHead;
							while (psEntry != NULL)
							{
								PUSC_LIST_ENTRY psNext = psEntry->psNext;
								PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD(psEntry, PSTORE_DATA, sListEntry);
								ASSERT(psStoreData->psStoreInst->uBlockIndex <= psDeschedStartInst->uBlockIndex);
								UscFree(psState, psStoreData);
								psEntry = psNext;
							}

							InitializeList(&sStores);
							break;
						}
					}
				}
			}

			UseDefIterateIRegLiveness_Next(psState, &sIRegIterator, psInst);
			if (UseDefIterateIRegLiveness_Current(&sIRegIterator) == 0)
			{
				psDeschedStartInst = psInst;
			}
		}

		/*
		Check if we need to insert an IDF as the last instruction in the block.
		This is required if there have been any stores to aliased addresses after
		the last load.
		 */
		psEntry = sStores.psHead;
		bInsertFinalDF = IMG_FALSE;
		while (psEntry != NULL)
		{
			PUSC_LIST_ENTRY psNext = psEntry->psNext;
			PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD(psEntry, PSTORE_DATA, sListEntry);
			psEntry = psNext;

			bInsertFinalDF |= !(psStoreData->psStoreInst->u.psLdSt->uFlags & UF_MEM_NOALIAS);

			/* Memory no longer required, free it */
			UscFree(psState, psStoreData);
		}

		if (bInsertFinalDF)
		{
			InsertIDF(psState, psBlock, psBlock->psBodyTail);
		}

		InitializeList(&sStores);
		FreeBlockDGraphState(psState, psBlock);
	}

	/*
		Insert data fences for local load/store instructions.
	*/
	psLastLocalSTInst = NULL;
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_LOCALLOADSTORE)
		{
			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE)
			{
				/*
					Store the last instruction 
				*/
				psLastLocalSTInst = psInst;
			}
			else
			{
				ASSERT(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD);
				if (psLastLocalSTInst != NULL)
				{
					/*
						Insert an IDF instruction after the last preceding store.
					*/
					InsertIDF(psState, psBlock, psLastLocalSTInst);
	
					/*
						We don't need any more IDFs until the next store.
					*/
					psLastLocalSTInst = NULL;
				}
			}
		}
	}

	/*
		Insert an IDF as the last instruction in the block if there have been
		any stores after the last load.
	*/
	if (psLastLocalSTInst != NULL)
	{
		InsertIDF(psState, psBlock, psLastLocalSTInst);
	}

	/*
		Reset the information about external writebacks associated with
		each DRC.
	*/
	for (uDRCIdx = 0; uDRCIdx < EURASIA_USE_DRC_BANK_SIZE; uDRCIdx++)
	{
		sCtx.asDrc[uDRCIdx].uWaitBeforeInstIdx = 0;
		sCtx.asDrc[uDRCIdx].uCompleteInstIdx = 0;
		sCtx.asDrc[uDRCIdx].uNumOperations = 0;
	}

	for (psInst = psBlock->psBody, uInstIdx = 1; psInst != NULL; psInst = psInst->psNext, uInstIdx++)
	{
		/*
			Check if a wait information is needed before this instruction.
		*/
		CheckInsertWaits(psState, psBlock, &sCtx, psInst, uInstIdx);
	
		/*
			Insert a WDF after loads from memory, IDFs, IDIV32's and texture-samples (but
			not USP-pseudo sample instructions, as the USP will add the WDFs for them).
		*/
		if	(IsAsyncWBInst(psState, psInst))
		{
			IMG_UINT32	uThisWaitBeforeInstIdx;
			IMG_UINT32	uThisCompleteInstIdx;
			IMG_UINT32	uBestWaitBeforeInstIdx;
			IMG_UINT32	uBestCompleteInstIdx;
			IMG_UINT32	auWastedCycles[EURASIA_USE_DRC_BANK_SIZE];
			IMG_UINT32	uBestDRCIdx;

			/*
				Get the instruction where we will need to wait for the external
				operation started by the current instruction to finish.
			*/
			uThisWaitBeforeInstIdx = GetWaitInstIdx(psState, psInst, uInstIdx);

			/*
				Estimate the instruction where the current external operation
				will finish.
			*/
			uThisCompleteInstIdx = uInstIdx + 16;

			uBestDRCIdx = USC_UNDEF;
			uBestWaitBeforeInstIdx = 0;
			uBestCompleteInstIdx = 0;
			for (uDRCIdx = 0; uDRCIdx < EURASIA_USE_DRC_BANK_SIZE; uDRCIdx++)
			{
				IMG_UINT32	uNewWaitBeforeInstIdx;
				IMG_UINT32	uNewCompleteInstIdx;
				PDRC_INFO	psDrc = &sCtx.asDrc[uDRCIdx];
				
				if (psDrc->uNumOperations == EURASIA_USE_DRC_MAXCOUNT)
				{
					/*
						If this DRC register already has the maximum number of operations
						pending on it then we'll need to insert a wait before this
						instruction.
					*/
					if (psDrc->uCompleteInstIdx > uInstIdx)
					{
						auWastedCycles[uDRCIdx] = psDrc->uCompleteInstIdx - uInstIdx;
					}
					else
					{
						auWastedCycles[uDRCIdx] = 0;
					}

					/*
						Add on the extra cycles for another wait for the result of this instruction.
					*/
					auWastedCycles[uDRCIdx] += (uThisCompleteInstIdx - uThisWaitBeforeInstIdx);

					/*
						Since we've waited before the instruction the new information about the
						DRC register is only affected by the current instruction.
					*/
					uNewWaitBeforeInstIdx = uThisWaitBeforeInstIdx;
					uNewCompleteInstIdx = uThisCompleteInstIdx;
				}
				else
				{
					/*
						If we associated the current external operation with this
						DRC register where is the new point where we would have to
						wait for operations associated with the DRC register to
						finish.
					*/
					if (psDrc->uWaitBeforeInstIdx > uInstIdx)
					{
						uNewWaitBeforeInstIdx = min(psDrc->uWaitBeforeInstIdx, uThisWaitBeforeInstIdx);
					}
					else
					{
						uNewWaitBeforeInstIdx = uThisWaitBeforeInstIdx;
					}

					/*
						Where is the new point where operations associated with this DRC register are
						estimated to finish.
					*/
					uNewCompleteInstIdx = max(psDrc->uCompleteInstIdx, uThisCompleteInstIdx);

					/*
						Get the estimated number of cycles we would have to wait if the current operation
						was associated with this DRC register.
					*/
					if (uNewCompleteInstIdx > uNewWaitBeforeInstIdx)
					{
						auWastedCycles[uDRCIdx] = uNewCompleteInstIdx - uNewWaitBeforeInstIdx;
					}
					else
					{
						auWastedCycles[uDRCIdx] = 0;
					}
				}

				/*
					Choose the DRC register which minimizes the number of wait cycles.
				*/
				if (uBestDRCIdx == USC_UNDEF || auWastedCycles[uDRCIdx] < auWastedCycles[uBestDRCIdx])
				{
					uBestDRCIdx = uDRCIdx;
					uBestWaitBeforeInstIdx = uNewWaitBeforeInstIdx;
					uBestCompleteInstIdx = uNewCompleteInstIdx;
				}
			}
			ASSERT(uBestDRCIdx != USC_UNDEF);

			/*
				If the maximum number of operations are already pending on the
				chosen DRC register then insert a wait before this instruction.
			*/
			if (sCtx.asDrc[uBestDRCIdx].uNumOperations == EURASIA_USE_DRC_MAXCOUNT)
			{
				InsertWait(psState, &sCtx, psBlock, uBestDRCIdx, psInst);
			}

			/*
				Associate the instruction with the chosen DRC register.
			*/
			SetDependentReadCounter(psState, psInst, uBestDRCIdx);

			/*
				Update the information about the operations pending on the chosen
				DRC register.
			*/
			sCtx.asDrc[uBestDRCIdx].uWaitBeforeInstIdx = uBestWaitBeforeInstIdx;
			sCtx.asDrc[uBestDRCIdx].uCompleteInstIdx = uBestCompleteInstIdx;
			sCtx.asDrc[uBestDRCIdx].uNumOperations++;
		}
	}

	/*
		Check for needing to wait after the last instruction in the block. We never allow
		external writebacks to be pending across flow control.
	*/
	CheckInsertWaits(psState, psBlock, &sCtx, NULL, uInstIdx);

	PVR_UNREFERENCED_PARAMETER(pvNull);
}

/*****************************************************************************
 FUNCTION	: SetSyncStartBeforeInst
    
 PURPOSE	: Set the SyncStart flag before a specific instruction

 PARAMETERS	: psState	- Compiler state,
			  psBlock	- A basic block containin the instruciton to set
						  the SyncStart before
			  psInst	- The instruction to set the SyncStart flag before
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID SetSyncStartBeforeInst(PINTERMEDIATE_STATE	psState, 
									   PCODEBLOCK			psBlock,
									   PINST				psInst)
{
	PINST	psTarget;

	/*
		Insert a NOP instruction for the flag if necessary.
	*/
	if	(!psInst || psInst->psPrev == NULL || !SupportSyncStart(psInst->psPrev))
	{
		psTarget = AllocateInst(psState, psInst);

		SetOpcode(psState, psTarget, INOP);
		psTarget->asDest[0].uType = USC_REGTYPE_DUMMY;
		InsertInstBefore(psState, psBlock, psTarget, psInst);
	}
	else
	{
		#if defined(SUPPORT_SGX545)
		PINST	psPrevPrevInst;

		/*
			On SGX545 consecutive syncstarts are illegal. So instead of

				A+SYNCSTART
				B+SYNCSTART
				C
			do
				A+SYNCSTART+NOSCHED
				B
				C
			The NOSCHED prevents SGX from losing the already established
			synchronization between B and C.
		*/
		psPrevPrevInst = psInst->psPrev->psPrev;
		if (
				(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_EXTERNAL_LDST_SMP_UNIT) &&
				psPrevPrevInst != NULL &&
				GetBit(psPrevPrevInst->auFlag, INST_SYNCSTART)
			)
		{
			ASSERT(psPrevPrevInst->eOpcode != IDRVPADDING);
			if (SupportsNoSched_NoPairs(psState, psPrevPrevInst))
			{
				SetBit(psPrevPrevInst->auFlag, INST_NOSCHED, 1);
			}
			else
			{
				PINST	psNOPInst;

				/*
					If A doesn't support the NOSCHED flag convert
						A+SYNCSTART+NOSCHED
						B
						C
					->
						A
						NOP+SYNCSTART+NOSCHED
						B
						C
				*/
				psNOPInst = AllocateInst(psState, psInst);

				SetOpcode(psState, psNOPInst, INOP);
				psNOPInst->asDest[0].uType = USC_REGTYPE_DUMMY;
				InsertInstBefore(psState, psBlock, psNOPInst, psInst->psPrev);

				SetBit(psNOPInst->auFlag, INST_NOSCHED, 1);
				SetBit(psNOPInst->auFlag, INST_SYNCSTART, 1);
				SetBit(psPrevPrevInst->auFlag, INST_SYNCSTART, 0);
			}
			return;
		}
		#endif /* defined(SUPPORT_SGX545) */

		psTarget = psInst->psPrev;
	}

	/*
		Set the sync-start flag on the preceeding instruction
	*/
	SetBit(psTarget->auFlag, INST_SYNCSTART, 1);
}

/*****************************************************************************
 FUNCTION	: SetSyncStartBP
    
 PURPOSE	: BLOCK_PROC, sets the SyncStart flag before any instructions that
			  require it, as well as any blocks (for nested flow control -
			  according to the bAddSyncAtStart field)

 PARAMETERS	: psState	- Compiler state
			  psBlock	- Block to process
			  pvNull	- IMG_PVOID to fit callback signature; unused
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID SetSyncStartBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
{
	PINST psInst = psBlock->psBody;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	
	if (psBlock->bAddSyncAtStart)
	{
		/*
			Set syncstart at beginning of block (for nested flow control -
			determined by SetupSyncStartInformation in pregalloc.c).
		*/
		SetSyncStartBeforeInst(psState, psBlock, psInst);
		if (psInst == NULL) return;
		//skip checking for *another* sync before first inst even if it needs gradients.
		psInst = psInst->psNext;
	}

	#if defined(OUTPUT_USCHW)
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		/*
			Look through all instruction of every basic-block, for those that
			require synchronisation.
		*/
		for (; psInst; psInst = psInst->psNext)
		{
			if (RequiresGradients(psInst)) SetSyncStartBeforeInst(psState, psBlock, psInst);
		}
	}
	#endif /* defined(OUTPUT_USCHW) */
}

/*****************************************************************************
 FUNCTION	: SetupSyncStartFlag
    
 PURPOSE	: Perform optimisations that related to grouped instructions

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID SetupSyncStartFlag(PINTERMEDIATE_STATE psState)
{
	/* 
		Sync Start are only set for Pixel shaders. 
	*/
	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}
	
	if (psState->uNumDynamicBranches == 0) return;
	
	/*
		Not a lot to do here, as all the complexity (mutilating the CFG to add
		merge/confluence points, setting sync_end on branches (edges), etc.) is
		all done in SetupSyncStartInformation (in pregalloc.c). Specifically,
		this even includes determining which blocks need an extra SYNC at the
		start (for nested flow control), so all this needs do is to set the
		relevant flags (adding NOPs if necessary).
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, SetSyncStartBP, IMG_TRUE, NULL);
}

/*****************************************************************************
 FUNCTION	: PrependPaddingToBlock
    
 PURPOSE	: Prepend instructions to a block that represent instruction slots
			  reserved for the caller to insert code.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to prepend the padding to.
			  uInstCount
						- Count of padding instructions to add.
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID PrependPaddingToBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uInstCount)
{
	IMG_UINT32	uInst;

	for (uInst = 0; uInst < uInstCount; uInst++)
	{
		PINST	psNop;

		psNop = AllocateInst(psState, psBlock->psBody);

		SetOpcode(psState, psNop, IDRVPADDING);
		psNop->asDest[0].uType = USC_REGTYPE_DUMMY;
		psNop->uId = 0;

		InsertInstBefore(psState, 
						 psBlock,
						 psNop,
						 psBlock->psBody);
	}
}

/*****************************************************************************
 FUNCTION	: InsertPaddingInstructions
    
 PURPOSE	: Insert placeholders where the driver needs to insert code.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID InsertPaddingInstructions(PINTERMEDIATE_STATE psState)
{
	/*
		Add padding to the start of the program, as requested
	*/
	if	(psState->psSAOffsets->uPreFeedbackPhasePreambleCount > 0)
	{
		PCODEBLOCK psFirstBlock = psState->psMainProg->sCfg.psEntry;
		if 
		(
			(psState->uFlags2 & USC_FLAGS2_SPLITCALC) &&
			(psState->psMainProg->sCfg.psEntry == psState->psPreSplitBlock) &&
			(psState->psPreSplitBlock != NULL) &&
			(psState->psPreSplitBlock->psBody == NULL)	
		)
		{
			ASSERT(psState->psPreSplitBlock->uNumSuccs == 1);
			psFirstBlock = psState->psPreSplitBlock->asSuccs[0].psDest;
		}

		ASSERT(psFirstBlock);//->uType == CBTYPE_BASIC);

		PrependPaddingToBlock(psState, psFirstBlock, psState->psSAOffsets->uPreFeedbackPhasePreambleCount);
	}

	/*
		Add padding for the start of the post-feedback phase.
	*/
	if	((psState->uCompilerFlags & UF_SPLITFEEDBACK) && (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC))
	{
		PCODEBLOCK	psPostFeedbackBlock;

		ASSERT(psState->psPreFeedbackDriverEpilogBlock != NULL);
		ASSERT(psState->psPreFeedbackDriverEpilogBlock->eType == CBTYPE_UNCOND);
		ASSERT(psState->psPreFeedbackDriverEpilogBlock->uNumSuccs == 1);
		psPostFeedbackBlock = psState->psPreFeedbackDriverEpilogBlock->asSuccs[0].psDest;

		ASSERT(psState->psPreFeedbackDriverEpilogBlock->uNumPreds == 1);
		ASSERT(psState->psPreFeedbackDriverEpilogBlock->asPreds[0].psDest == psState->psPreFeedbackBlock);

		ASSERT(psState->psPreFeedbackBlock->eType == CBTYPE_UNCOND);
		ASSERT(psState->psPreFeedbackBlock->uNumSuccs == 1);
		ASSERT(psState->psPreFeedbackBlock->asSuccs[0].psDest == psState->psPreFeedbackDriverEpilogBlock);

		ASSERT(psPostFeedbackBlock->uNumPreds == 1);
		ASSERT(psPostFeedbackBlock->asPreds[0].psDest == psState->psPreFeedbackDriverEpilogBlock);

		PrependPaddingToBlock(psState, psPostFeedbackBlock, psState->psSAOffsets->uPostFeedbackPhasePreambleCount);
	}

	/*
		Add padding for the start of the post-split phase.
	*/
	if
	(
		(psState->psSAOffsets->uPostSplitPhasePreambleCount > 0) &&
		(psState->uFlags2 & USC_FLAGS2_SPLITCALC) &&
		(psState->psPreSplitBlock != NULL) &&
		(
			(psState->psMainProg->sCfg.psEntry != psState->psPreSplitBlock) ||
			(psState->psPreSplitBlock->psBody != NULL)
		)	
	)
	{
		PCODEBLOCK	psPostSplitBlock;

		ASSERT(psState->psPreSplitBlock != NULL);
		ASSERT(psState->psPreSplitBlock->uNumSuccs == 1);
		psPostSplitBlock = psState->psPreSplitBlock->asSuccs[0].psDest;

		ASSERT(psPostSplitBlock->uNumPreds == 1);
		ASSERT(psPostSplitBlock->asPreds[0].psDest == psState->psPreSplitBlock);

		PrependPaddingToBlock(psState, psPostSplitBlock, psState->psSAOffsets->uPostSplitPhasePreambleCount);
	}

	/*
		Add padding for the caller to insert feedback control code
	*/
	if	(psState->uCompilerFlags & UF_SPLITFEEDBACK)
	{
		if	(psState->psSAOffsets->uFeedbackInstCount > 0)
		{
			PCODEBLOCK	psBlock;
			IMG_UINT32	uInst;

			if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
			{
				psBlock = psState->psPreFeedbackDriverEpilogBlock;
			}
			else
			{
				psBlock = psState->psMainProg->sCfg.psExit;
			}
			ASSERT(psBlock);//->uType == CBTYPE_BASIC);

			for (uInst = 0; uInst < psState->psSAOffsets->uFeedbackInstCount; uInst++)
			{
				PINST	psNop;

				psNop = AllocateInst(psState, IMG_NULL);

				SetOpcode(psState, psNop, IDRVPADDING);
				psNop->asDest[0].uType = USC_REGTYPE_DUMMY;
				psNop->uId = 1;

				if	(uInst == 0)
				{
					psState->psMainProgFeedbackPhase0EndInst = psNop;
				}

				InsertInstBefore(psState, psBlock, psNop, NULL);
			}
		}
	}
}

static IMG_VOID NoSchedAlignToEvenCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout)
{
	if ((((psLayout->puInst) - psState->puInstructions) % 4) == 2)
	{		
		AppendInstruction(psLayout, INOP);
		(psLayout->puInst) += 2;		
	}
}

static IMG_VOID NoSchedBranchCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout,
								IOPCODE eOpcode, IMG_PUINT32 puDestLabel,
								IMG_UINT32 uPredSrc, IMG_BOOL bPredNegate,
								IMG_BOOL bSyncEnd,
								EXECPRED_BRANCHTYPE eExecPredBranchType)
{
	PVR_UNREFERENCED_PARAMETER(eExecPredBranchType);
	CommonBranchCB(psState, psLayout, eOpcode,
				   puDestLabel, uPredSrc,
				   bPredNegate, bSyncEnd);

	psLayout->puInst += 2;
}

/*****************************************************************************
 FUNCTION	: SetupNoSchedFlag
    
 PURPOSE	: Set the no-sched flag where needed

 PARAMETERS	: psState - Compiler state.
			  
 RETURNS	: none

 NOTES		: At exit, all secondary (dual-issued) instructions will be
			  removed from the codeblock and will only be accessible from the
			  primary instruction.
*****************************************************************************/
static IMG_VOID SetupNoSchedFlag(PINTERMEDIATE_STATE psState)
{
	//callback for handling instructions in body - so decide about pairing now:
	LAY_INSTRS pfnInstrs =
		(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_NO_INSTRUCTION_PAIRING)
			? SetNoSchedFlagForBasicBlockNoPairs : SetNoSchedFlagForBasicBlock;
	LAYOUT_PROGRAM_PARTS eLayoutTarget;

	psState->puInstructions = NULL; //base from which offset calculated.
	psState->puSAInstructions = NULL;

	eLayoutTarget = LAYOUT_WHOLE_PROGRAM;
	#if defined(OUTPUT_USPBIN)
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		eLayoutTarget = LAYOUT_SA_PROGRAM;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	LayoutProgram(psState, pfnInstrs, NoSchedBranchCB, NoSchedLabelCB, PointActionsHwCB, NoSchedAlignToEvenCB, eLayoutTarget);
}

/*****************************************************************************
 FUNCTION	: IsPunchthroughFBStillSplit
    
 PURPOSE	: Check if there are still some instructions after the point
			  of feedback to the ISP.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
static IMG_BOOL IsPunchthroughFBStillSplit(PINTERMEDIATE_STATE psState)
{
	if (psState->psPreFeedbackDriverEpilogBlock == NULL) return IMG_FALSE;
	ASSERT (psState->psPreFeedbackDriverEpilogBlock->eType == CBTYPE_UNCOND);
	if (psState->psPreFeedbackDriverEpilogBlock->asSuccs[0].psDest->psBody ||
		psState->psPreFeedbackDriverEpilogBlock->asSuccs[0].psDest->eType != CBTYPE_EXIT)
	{
		/*
			...TODO, really ought to check there's actually code somewhere there
			(i.e. recursively! - or would simply calling MergeBasicBlocks first suffice?)
		*/
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsSampleRateStillSplit
    
 PURPOSE	: Check if there are still some instructions after the point
			  of Sample Rate Split to the ISP.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL IsSampleRateStillSplit(PINTERMEDIATE_STATE psState)
{
	if (psState->psPreSplitBlock == NULL) return IMG_FALSE;
	ASSERT (psState->psPreSplitBlock->eType == CBTYPE_UNCOND);
	if (psState->psPreSplitBlock->asSuccs[0].psDest->psBody ||
		psState->psPreSplitBlock->asSuccs[0].psDest->eType != CBTYPE_EXIT)
	{
		/*
			...TODO, really ought to check there's actually code somewhere there
			(i.e. recursively! - or would simply calling MergeBasicBlocks first suffice?)
		*/
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID FinaliseIndexableTempsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: FinaliseIndexableTempsBP
    
 PURPOSE	: BLOCK_PROC, performs minor changes and optimisation to the
			  intermediate code prior to generating HW instructions

 PARAMETERS	: psState	- Compiler intermediate state
			  psBlock	- Block to process
			  pvNull	- IMG_PVOID to fit callback signature; unused

 RETURNS	: Nothing
*****************************************************************************/
{
	PINST psInst;
	PVR_UNREFERENCED_PARAMETER(pvNull);
	/*
		For each instruction, fill-in pseudo-registers
		that are used for indexable-temp accesses.
	*/
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		IMG_UINT32	i;

		if	(g_psInstDesc[psInst->eOpcode].bHasDest)
		{
			for (i = 0; i < psInst->uDestCount; i++)
			{
				ConvertRegister(psState, &psInst->asDest[i]);
			}
		}

		for (i = 0; i < psInst->uArgumentCount; i++)
		{
			ConvertRegister(psState, &psInst->asArg[i]);
		}
	}
}

static IMG_VOID RemoveNoEmitInstructionsBP(PINTERMEDIATE_STATE	psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: RemoveNoEmitInstructionsBP
    
 PURPOSE	: BLOCK_PROC, removes instructions with the NOEMIT flag

 PARAMETERS	: psState	- Compiler intermediate state
			  psBlock	- block to process
			  pvNull	- IMG_PVOID to fit callback signature; unused
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst, psNextInst;
	
	PVR_UNREFERENCED_PARAMETER(pvNull);

	for (psInst = psBlock->psBody; psInst; psInst = psNextInst)
	{
		psNextInst = psInst->psNext;
		/* Remove no-emit instructions */
		if (GetBit(psInst->auFlag, INST_NOEMIT))
		{
			RemoveInst(psState, psBlock, psInst);
			FreeInst(psState, psInst);
		}
	}
}

static
IMG_VOID PrependSMBO(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: PrependSMBO
    
 PURPOSE	: Insert an SMBO instruction at the start of a block.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psDummySmbo;
	IMG_UINT32	uArg;

	/*
		Create an SMBO instruction which loads the same state as default MOE 
		state i.e. zero for all base offsets.
	*/
	psDummySmbo = AllocateInst(psState, NULL);
	SetOpcode(psState, psDummySmbo, ISMBO);

	psDummySmbo->asDest[0].uType = USC_REGTYPE_DUMMY;
	
	for (uArg = 0; uArg < USC_MAX_MOE_OPERANDS; uArg++)
	{
		psDummySmbo->asArg[uArg].uType = USEASM_REGTYPE_IMMEDIATE;
		psDummySmbo->asArg[uArg].uNumber = 0;
	}

	/*
		Insert the dummy SMBO at the start of the program.
	*/
	InsertInstBefore(psState, psBlock, psDummySmbo, psBlock->psBody);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef enum _VDUAL_FIX_STATUS
{
	/*
		The first ALU instruction in this block is a VDUAL instruction.
	*/
	VDUAL_FIX_STATUS_DUALFIRST,
	/*
		The first ALU instruction in this block isn't a VDUAL instruction.
	*/
	VDUAL_FIX_STATUS_NONDUALFIRST,
	/*
		This block doesn't contain any ALU instructions.
	*/
	VDUAL_FIX_STATUS_NOP,
} VDUAL_FIX_STATUS, *PVDUAL_FIX_STATUS;

typedef struct _VDUAL_FIX_CONTEXT
{
	/*
		The status for each function.
	*/
	PVDUAL_FIX_STATUS	aeFuncStatus;
	/*
		The status for each block in the current function.
	*/
	PVDUAL_FIX_STATUS	aeBlockStatus;
} VDUAL_FIX_CONTEXT, *PVDUAL_FIX_CONTEXT;

static
IMG_VOID GetVDualFixStatus(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext)
/*****************************************************************************
 FUNCTION	: GetVDualFixStatus
    
 PURPOSE	: Check whether a block starts with a VDUAL instruction.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block to process.
			  pvContext		- Pass state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PVDUAL_FIX_CONTEXT	psContext = (PVDUAL_FIX_CONTEXT)pvContext;
	PVDUAL_FIX_STATUS	peBlockStatus = psContext->aeBlockStatus + psBlock->uIdx;
	PINST				psInst;

	*peBlockStatus = VDUAL_FIX_STATUS_NOP;
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if (psInst->eOpcode == IVDUAL)
		{
			*peBlockStatus = VDUAL_FIX_STATUS_DUALFIRST;
			break;
		}
		else if (psInst->eOpcode == ICALL)
		{
			VDUAL_FIX_STATUS	eCalleStatus;
			IMG_UINT32			uLabel;

			uLabel = psInst->u.psCall->psTarget->uLabel;
			ASSERT(uLabel < psState->uMaxLabel);
			eCalleStatus = psContext->aeFuncStatus[uLabel];
			if (eCalleStatus != VDUAL_FIX_STATUS_NOP)
			{
				*peBlockStatus = eCalleStatus;
				break;
			}
		}
		else if (!(psInst->eOpcode == INOP || psInst->eOpcode == IWDF || psInst->eOpcode == IDRVPADDING))
		{
			*peBlockStatus = VDUAL_FIX_STATUS_NONDUALFIRST;
			break;
		}
	}
}

static
IMG_BOOL ProcessBlockVDualFix(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock, 
							  IMG_PVOID				pvResult, 
							  IMG_PVOID*			pvArgs, 
							  IMG_PVOID				pvUserData)
/*****************************************************************************
 FUNCTION	: ProcessBlockVDualFix
    
 PURPOSE	: 

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block to process.
			  pvResult		- On input: the old state of the block from
							  the last time ProcessBlockVDualFix was called.
							  On output: the new state of the block after
							  combining the states of its predecessors.
			  pvArgs		- Array of pointers to the state of each predecessor.
			  pvUserData	- Ignored.
			  
 RETURNS	: TRUE if the resulting state for the block changed.
*****************************************************************************/
{
	PVDUAL_FIX_STATUS	peBlockState = (PVDUAL_FIX_STATUS)pvResult;
	VDUAL_FIX_STATUS	eOrigState;

	PVR_UNREFERENCED_PARAMETER(pvUserData);
	PVR_UNREFERENCED_PARAMETER(psBlock);

	eOrigState = *peBlockState;
	/*
		Where the block is either known to start with a VDUAL instruction
		or with a non-VDUAL ALU instruction then retain that status.
	*/
	if (eOrigState == VDUAL_FIX_STATUS_DUALFIRST ||
		eOrigState == VDUAL_FIX_STATUS_NONDUALFIRST)
	{
		return IMG_FALSE;
	}
	else
	{
		PVDUAL_FIX_STATUS*	apeSuccs = (PVDUAL_FIX_STATUS*)pvArgs;
		IMG_UINT32			uPred;
		IMG_BOOL			bAllNonDuals;
		IMG_BOOL			bAnyDuals;

		ASSERT(eOrigState == VDUAL_FIX_STATUS_NOP);

		/*
			Otherwise check for all successors:-
			(i) Do any start a VDUAL instruction.
			(ii) Do all start with a non-VDUAL instruction.
		*/
		bAllNonDuals = IMG_TRUE;
		bAnyDuals = IMG_FALSE;
		for (uPred = 0; uPred < psBlock->uNumSuccs; uPred++)
		{
			VDUAL_FIX_STATUS	eSuccState = *apeSuccs[uPred];

			if (eSuccState != VDUAL_FIX_STATUS_NONDUALFIRST)
			{
				bAllNonDuals = IMG_FALSE;
			}
			if (eSuccState == VDUAL_FIX_STATUS_DUALFIRST)
			{
				bAnyDuals = IMG_TRUE;
				break;
			}
		}

		if (bAnyDuals)
		{
			*peBlockState = VDUAL_FIX_STATUS_DUALFIRST;
			return IMG_TRUE;
		}
		if (bAllNonDuals)
		{
			*peBlockState = VDUAL_FIX_STATUS_NONDUALFIRST;
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}
}

static
IMG_VOID AddVDualFirstFixToBlock(PINTERMEDIATE_STATE psState, PVDUAL_FIX_STATUS aeBlockStatus, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: AddVDualFirstFixToBlock
    
 PURPOSE	: If a block starts with a VDUAL instruction then insert a dummy
			  instruction before it.

 PARAMETERS	: psState			- Compiler state.
			  aeBlockStatus		- Status for each block in the function.
			  psBlock			- Block to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	VDUAL_FIX_STATUS	eBlockStatus;

	eBlockStatus = aeBlockStatus[psBlock->uIdx];
	if (eBlockStatus == VDUAL_FIX_STATUS_DUALFIRST)
	{
		PrependSMBO(psState, psBlock);

		aeBlockStatus[psBlock->uIdx] = VDUAL_FIX_STATUS_NONDUALFIRST;
	}
}

static
IMG_VOID AddVDualFirstFixToPhaseStart(PINTERMEDIATE_STATE psState, PVDUAL_FIX_STATUS aeBlockStatus, PCODEBLOCK psPrevPhaseEnd)
/*****************************************************************************
 FUNCTION	: AddVDualFirstFixToPhaseStart
    
 PURPOSE	: Where a program is divided into multiple phases check if the fix 
			  for VDUAL instruction is needed at the start of a phase.

 PARAMETERS	: psState			- Compiler state.
			  aeBlockStatus		- Status for each block in the function.
			  psPrevPhaseEnd	- Block immediately preceeding the phase start.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK	psPhaseStart;

	ASSERT(psPrevPhaseEnd->eType == CBTYPE_UNCOND);
	ASSERT(psPrevPhaseEnd->uNumSuccs == 1);
	psPhaseStart = psPrevPhaseEnd->asSuccs[0].psDest;

	ASSERT(psPhaseStart->uNumPreds == 1);
	ASSERT(psPhaseStart->asPreds[0].psDest == psPrevPhaseEnd);

	AddVDualFirstFixToBlock(psState, aeBlockStatus, psPhaseStart);
}

static
IMG_VOID AddVDualFirstFix(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AddVDualFirstFix
    
 PURPOSE	: The hardware has the restrictions that a VDUAL instruction can't
			  be the first one in a program. So add if it is then insert a dummy
			  instruction before it.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PVDUAL_FIX_STATUS	aeFuncStatus;
	IMG_UINT32			uLabel;
	PFUNC				psFunc;

	/*
		Check if the program contains VDUAL instructions at all.
	*/
	if (!TestInstructionUsage(psState, IVDUAL))
	{
		return;
	}

	/*
		Allocate memory for the effect of each function.
	*/
	aeFuncStatus = UscAlloc(psState, sizeof(aeFuncStatus[0]) * psState->uMaxLabel);
	for (uLabel = 0; uLabel < psState->uMaxLabel; uLabel++)
	{
		aeFuncStatus[uLabel] = VDUAL_FIX_STATUS_NOP;
	}

	/*
		Process each function from most to least deeply nested so we process functions
		before their callers.
	*/
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		PVDUAL_FIX_STATUS	aeBlockStatus;
		VDUAL_FIX_CONTEXT	sContext;
		
		aeBlockStatus = UscAlloc(psState, psFunc->sCfg.uNumBlocks * sizeof(aeBlockStatus[0]));

		sContext.aeBlockStatus = aeBlockStatus;
		sContext.aeFuncStatus = aeFuncStatus;

		/*
			Check for each block whether the first ALU instructon is
				(i) A VDUAL instruction.
				(ii) A non-VDUAL instruction.
				or
				(iii) It contains no ALU instructions.
		*/
		ASSERT(psFunc->uLabel < psState->uMaxLabel);
		DoOnCfgBasicBlocks(psState,
						   &(psFunc->sCfg),
						   ANY_ORDER,
						   GetVDualFixStatus,
						   IMG_TRUE /* bHandlesCalls */,
						   (IMG_PVOID)&sContext);

		/*
			Combine the result for each block with that for its successors.
		*/
		DoDataflow(psState, 
				   psFunc, 
				   IMG_FALSE /* bForwards */, 
				   sizeof(aeBlockStatus[0]), 
				   aeBlockStatus, 
				   ProcessBlockVDualFix,
				   IMG_NULL /* pvUserData */);
		
		/*
			If the secondary program starts with a VDUAL instruction then add the fix.
		*/
		if (psFunc == psState->psSecAttrProg)
		{
			AddVDualFirstFixToBlock(psState, aeBlockStatus, psFunc->sCfg.psEntry);
		}
		if (psFunc == psState->psMainProg)
		{
			/*
				If the primary program starts with a VDUAL instruction then add the fix.
			*/
			AddVDualFirstFixToBlock(psState, aeBlockStatus, psFunc->sCfg.psEntry);

			/*
				If the program following the pixel to sample rate change starts with a VDUAL
				instruction then add the fix.
			*/
			if (psState->psPreSplitBlock != NULL)
			{
				AddVDualFirstFixToPhaseStart(psState, aeBlockStatus, psState->psPreSplitBlock);
			}

			/*
				If the program following feedback to the ISP starts with a VDUAL
				instruction then add the fix.
			*/
			if (psState->psPreFeedbackDriverEpilogBlock != NULL)
			{
				AddVDualFirstFixToPhaseStart(psState, aeBlockStatus, psState->psPreFeedbackDriverEpilogBlock);
			}
		}

		UscFree(psState, aeBlockStatus);
	}

	UscFree(psState, aeFuncStatus);
}	
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID AddFixForBRN25804ForBlock(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: AddFixForBRN25804ForBlock
    
 PURPOSE	: If required add the workaround for BRN25804 to a block.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bTestMaskFirst;
	PINST		psInst;

	/*
		If the block doesn't contain any ALU instruction then (for simplicity)
		assume a TESTMASK instruction might be the first in one of the successor
		blocks.
	*/
	bTestMaskFirst = IMG_TRUE;

	/*
		Check if a TESTMASK instruction is the first ALU instruction in the block.
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TESTMASK) != 0)
		{
			/*
				A TESTMASK instruction is the first ALU instruction in the block.
			*/
			break;
		}
		if (!(psInst->eOpcode == INOP || psInst->eOpcode == IWDF || psInst->eOpcode == IDRVPADDING))
		{
			/*
				An instruction other than TESTMASK is the first ALU instruction.
			*/
			bTestMaskFirst = IMG_FALSE;
			break;
		}
	}

	/*
		If TESTMASK isn't the first instruction then no need for a workaround.	
	*/
	if (!bTestMaskFirst)
	{
		return;
	}

	/*
		Add a dummy SMBO instruction to the start of the block.
	*/
	PrependSMBO(psState, psBlock);
}

typedef struct
{
	IMG_BOOL	bUsedInMainProg;
	IMG_BOOL	bUsedInSecProg;
} ISTESTMASKUSED_CONTEXT, *PISTESTMASKUSED_CONTEXT;

static
IMG_VOID IsTESTMASKUsedInBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext)
/*****************************************************************************
 FUNCTION	: IsTESTMASKUsedInBlock
    
 PURPOSE	: Check if the TESTMASK instruction is present in a block.

 PARAMETERS	: psState	- Compiler state.
			  psBlock	- Block to check.
			  pvContext	- Points to information about the program. On return
						updated with information about the block.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_PBOOL				pbResult;
	PINST					psInst;
	PISTESTMASKUSED_CONTEXT	psContext = (PISTESTMASKUSED_CONTEXT)pvContext;

	if (psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		pbResult = &psContext->bUsedInSecProg;
	}
	else
	{
		pbResult = &psContext->bUsedInMainProg;
	}

	/*
		Return early if the result for the program is already known.
	*/
	if (*pbResult)
	{
		return;
	}
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TESTMASK) != 0)
		{
			*pbResult = IMG_TRUE;
			return;
		}
	}
}

/*****************************************************************************
 FUNCTION	: AddFixForBRN25804
    
 PURPOSE	: Insert the workaround for BRN25804.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID AddFixForBRN25804(PINTERMEDIATE_STATE	psState)
{
	ISTESTMASKUSED_CONTEXT	sIsTMUsed;

	/*
		FROM THE BUG TRACKER:

		When the TM instruction is the first instruction in the shader it will create errors because it will not forward 
		the "first" bit to the MOE register setup. This will potentially cause the MOE registers for the program to be 
		uninitialised and this may cause errors in the program. Normally if an instruction is the first one in a slot the 
		MOE setup checks for this and initialises the registers. For TM the wrong bit in the instruction word is checked 
		and the initialisation does not occur. 
	*/

	/*
		Check if the bug is present in this core revision.
	*/
	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_25804) == 0)
	{
		return;
	}

	/*
		Check if the TESTMASK instruction is ever used in the program. If not
		then the bug can't affect the program.
	*/
	sIsTMUsed.bUsedInMainProg = IMG_FALSE;
	sIsTMUsed.bUsedInSecProg = IMG_FALSE;
	DoOnAllBasicBlocks(psState, ANY_ORDER, IsTESTMASKUsedInBlock, IMG_FALSE /* bHandlesCalls */, &sIsTMUsed);

	if (sIsTMUsed.bUsedInMainProg)
	{
		/*
			Add the fix for BRN25804 to the start of the main program.
		*/
		AddFixForBRN25804ForBlock(psState, psState->psMainProg->sCfg.psEntry);

		/*
			Add the fix for BRN25804 to the code that runs after feedback (if it exists).
		*/
		if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
		{
			PCODEBLOCK	psLastPreFBDriverEpilogBlock;
			PCODEBLOCK	psFirstPostFBBlock;

			psLastPreFBDriverEpilogBlock= psState->psPreFeedbackDriverEpilogBlock;
			ASSERT(psLastPreFBDriverEpilogBlock->eType == CBTYPE_UNCOND);
			ASSERT(psLastPreFBDriverEpilogBlock->uNumSuccs == 1);
	
			psFirstPostFBBlock = psLastPreFBDriverEpilogBlock->asSuccs[0].psDest;
	
			AddFixForBRN25804ForBlock(psState, psFirstPostFBBlock);
		}
		/*
			Add the fix for BRN25804 to the code that runs after Split (if it exists).
		*/
		if (psState->uFlags2 & USC_FLAGS2_SPLITCALC)
		{
			PCODEBLOCK	psLastPreSplitBlock;
			PCODEBLOCK	psFirstPostSplitBlock;

			psLastPreSplitBlock = psState->psPreSplitBlock;
			ASSERT(psLastPreSplitBlock->eType == CBTYPE_UNCOND);
			ASSERT(psLastPreSplitBlock->uNumSuccs == 1);
	
			psFirstPostSplitBlock = psLastPreSplitBlock->asSuccs[0].psDest;
	
			AddFixForBRN25804ForBlock(psState, psFirstPostSplitBlock);
		}
	}

	/*
		Add the fix for BRN25804 to the start of the secondary update program.
	*/
	if (sIsTMUsed.bUsedInSecProg)
	{
		AddFixForBRN25804ForBlock(psState, psState->psSecAttrProg->sCfg.psEntry);
	}
}

static
IMG_VOID AddFixForBRN31988ToFunction(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/*****************************************************************************
 FUNCTION	: AddFixForBRN31988ToFunction
    
 PURPOSE	: Insert the workaround for BRN31988 to a particular function.

 PARAMETERS	: psState	- Compiler state.
			  psFunc	- Function to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psMemLdInst;
	PINST		psFuncStart = psFunc->sCfg.psEntry->psBody;
	IMG_UINT32	uDestIdx;
	PINST		psSetpInst;

	/*
		Insert an instruction to set p0 to false.
	*/
	psSetpInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psSetpInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	InsertInstBefore(psState, psFunc->sCfg.psEntry, psSetpInst, psFuncStart);
	SetBit(psSetpInst->auFlag, INST_SKIPINV, 1);
	psSetpInst->u.psTest->eAluOpcode = IAND;
	psSetpInst->asDest[TEST_PREDICATE_DESTIDX].uType = USEASM_REGTYPE_PREDICATE;
	psSetpInst->asDest[TEST_PREDICATE_DESTIDX].uNumber = 0;
	psSetpInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
	psSetpInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psSetpInst->asArg[0].uNumber = 0;
	psSetpInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psSetpInst->asArg[1].uNumber = 0;

	/*
		Insert a memory load predicated by p0.
	*/
	psMemLdInst = AllocateInst(psState, NULL /* psSrcLineInst */);
	InsertInstBefore(psState, psFunc->sCfg.psEntry, psMemLdInst, psFuncStart);
	SetOpcodeAndDestCount(psState, psMemLdInst, ILDAD, 4 /* uDestCount */);
	SetBit(psMemLdInst->auFlag, INST_SKIPINV, 1);
	SetPredicate(psState, psMemLdInst, 0 /* uPredRegNum */, IMG_FALSE /* bPredNegate */);
	for (uDestIdx = 0; uDestIdx < 4; uDestIdx++)
	{
		psMemLdInst->asDest[uDestIdx].uType = USEASM_REGTYPE_PRIMATTR;
		psMemLdInst->asDest[uDestIdx].uNumber = uDestIdx;
	}
	SetSrc(psState, psMemLdInst, MEMLOADSTORE_BASE_ARGINDEX, USEASM_REGTYPE_PRIMATTR, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
	SetSrc(psState, psMemLdInst, MEMLOADSTORE_OFFSET_ARGINDEX, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
	SetSrc(psState, psMemLdInst, MEMLOAD_RANGE_ARGINDEX, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
	SetSrc(psState, psMemLdInst, MEMLOAD_DRC_ARGINDEX, USEASM_REGTYPE_DRC, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);

	/*
		Disable scheduling between the TEST instruction and the memory load. 
		
		This (and aligning the start of program to a cache line bounary) ensures that the TEST instruction doesn't complete 
		before the memory load instruction starts. 

		If it did then the hardware would just skip execution of the memory load completely; whereas we need to execute
		enough to 'fence' memory loads from the PDS.
	*/
	SetBit(psMemLdInst->auFlag, INST_DISABLE_SCHED, 1);

	/*
		Wait for the results of the memory load.
	*/
	InsertWDF(psState, psFunc->sCfg.psEntry, 0 /* uDRCIdx */, psFuncStart);
}

static
IMG_VOID AddFixForBRN31988(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AddFixForBRN31988
    
 PURPOSE	: Insert the workaround for BRN31988.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK	psSAEntry;

	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_31988) == 0)
	{
		return;
	}

	psSAEntry = psState->psSecAttrProg->sCfg.psEntry;
	/*
		If the secondary update program contains any instructions then add the
		fix to the start.
	*/
	if (psSAEntry->psBody != NULL || psSAEntry != psState->psSecAttrProg->sCfg.psExit)
	{
		AddFixForBRN31988ToFunction(psState, psState->psSecAttrProg);
	}
}

#if defined(DEBUG)
/*****************************************************************************
 FUNCTION	: CheckC10Move
    
 PURPOSE	: Check a move instruction for a valid combination of formats/
			  register types.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static
IMG_VOID CheckC10Move(PINTERMEDIATE_STATE psState, PINST psInst)
{
	ASSERT(psInst->eOpcode == IMOV);
	ASSERT(!(psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL && psInst->asDest[0].eFmt == UF_REGFORMAT_C10));
	ASSERT(!(psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL && psInst->asArg[0].eFmt == UF_REGFORMAT_C10));
}
#endif /* defined(DEBUG) */

typedef enum _BRN30853_BLOCK_STATE
{
	/*
		State for a function whose callees haven't been processed yet.
	*/
	BRN30853_BLOCK_STATE_UNKNOWN,
	/*
		This block/function exits with a memory store pending.
	*/
	BRN30853_BLOCK_STATE_STORE,
	/*
		This block/function issues an IDF/memory load and then waits for
		it to complete with no memory stores issued afterwards.
	*/
	BRN30853_BLOCK_STATE_WDF,
	/*
		This block/function contains neither memory stores nor loads.
	*/
	BRN30853_BLOCK_STATE_NONE,
} BRN30853_BLOCK_STATE, *PBRN30853_BLOCK_STATE;

static
IMG_BOOL ProcessBlockBRN30853(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock, 
							  IMG_PVOID				pvResult, 
							  IMG_PVOID*			pvArgs, 
							  IMG_PVOID				pvUserData)
/*****************************************************************************
 FUNCTION	: ProcessBlockBRN30853
    
 PURPOSE	: Insert the workaround for BRN30853 to a function if required.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block to process.
			  pvResult		- On input: the old state of the block from
							  the last time ProcessBlockBRN30853 was called.
							  On output: the new state of the block after
							  combining the states of its predecessors.
			  pvArgs		- Array of pointers to the state of each predecessor.
			  pvUserData	- Ignored.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PBRN30853_BLOCK_STATE	peBlockState = (PBRN30853_BLOCK_STATE)pvResult;
	BRN30853_BLOCK_STATE	eOrigState;

	PVR_UNREFERENCED_PARAMETER(pvUserData);
	PVR_UNREFERENCED_PARAMETER(psBlock);

	eOrigState = *peBlockState;
	if (eOrigState == BRN30853_BLOCK_STATE_STORE ||
		eOrigState == BRN30853_BLOCK_STATE_WDF)
	{
		return IMG_FALSE;
	}
	else
	{
		PBRN30853_BLOCK_STATE*	apePreds = (PBRN30853_BLOCK_STATE*)pvArgs;
		IMG_UINT32				uPred;
		IMG_BOOL				bAllWaits;
		IMG_BOOL				bAnyStores;

		ASSERT(eOrigState == BRN30853_BLOCK_STATE_NONE);

		bAllWaits = IMG_TRUE;
		bAnyStores = IMG_FALSE;
		for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
		{
			BRN30853_BLOCK_STATE	ePredState = *apePreds[uPred];

			if (ePredState != BRN30853_BLOCK_STATE_WDF)
			{
				bAllWaits = IMG_FALSE;
			}
			if (ePredState == BRN30853_BLOCK_STATE_STORE)
			{
				bAnyStores = IMG_TRUE;
				break;
			}
		}

		if (bAnyStores)
		{
			*peBlockState = BRN30853_BLOCK_STATE_STORE;
			return IMG_TRUE;
		}
		if (bAllWaits)
		{
			*peBlockState = BRN30853_BLOCK_STATE_WDF;
			return IMG_TRUE;
		}

		return IMG_FALSE;
	}
}

static
BRN30853_BLOCK_STATE GetBRN30853FunctionState(PINTERMEDIATE_STATE psState, PFUNC psFunc, PBRN30853_BLOCK_STATE aeFuncState)
/*****************************************************************************
 FUNCTION	: GetBRN30853FunctionState
    
 PURPOSE	: Get the effect of a function on memory loads/stores either
				A memory load/IDF is issued and waited for after any
				memory stores.
			  or
				A memory store was issued with no following memory load/IDF
				and WDF.
			  or
			    no effect

 PARAMETERS	: psState		- Compiler state.
			  psFunc		- Function to get the state for.
			  aeFuncState	- For each function: does it exit with memory stores
							pending.
			 
 RETURNS	: The effect of the function on memory loads/stores.
*****************************************************************************/
{
	PBRN30853_BLOCK_STATE	aeBlockState;
	BRN30853_BLOCK_STATE	eExitState;
	IMG_BOOL				bAnyLoadOrStore;
	IMG_UINT32				uBlock;

	aeBlockState = UscAlloc(psState, sizeof(aeBlockState[0]) * psFunc->sCfg.uNumBlocks);
	bAnyLoadOrStore = IMG_FALSE;
	for (uBlock = 0; uBlock < psFunc->sCfg.uNumBlocks; uBlock++)
	{
		PCODEBLOCK				psBlock;
		PINST					psInst;
		/*
			For each DRC: set to TRUE if a memory load or IDF has been issued
			on that drc and not waited for and there have been no memory stores
			since.
		*/
		IMG_BOOL				abMemReadSent[EURASIA_USE_DRC_BANK_SIZE];
		BRN30853_BLOCK_STATE	eBlockState;
		IMG_UINT32				uDrc;

		psBlock = psFunc->sCfg.apsAllBlocks[uBlock];

		eBlockState = BRN30853_BLOCK_STATE_NONE;
		for (uDrc = 0; uDrc < EURASIA_USE_DRC_BANK_SIZE; uDrc++)
		{
			abMemReadSent[uDrc] = IMG_FALSE;
		}

		for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
		{
			/*
				For a call use the already computed effect of the called function.
			*/
			if (psInst->eOpcode == ICALL)
			{
				IMG_UINT32				uLabel;
				BRN30853_BLOCK_STATE	eFuncState;

				uLabel = psInst->u.psCall->psTarget->uLabel;
				ASSERT(uLabel < psState->uMaxLabel);

				eFuncState = aeFuncState[uLabel];
				ASSERT(eFuncState != BRN30853_BLOCK_STATE_UNKNOWN);

				if (eFuncState == BRN30853_BLOCK_STATE_STORE || eFuncState == BRN30853_BLOCK_STATE_WDF)
				{
					eBlockState = eFuncState;
					for (uDrc = 0; uDrc < EURASIA_USE_DRC_BANK_SIZE; uDrc++)
					{
						abMemReadSent[uDrc] = IMG_FALSE;
					}
				}
			}
			/*
				For memory stores flag this is a potential instance of the bug.
			*/
			else if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE) != 0)
			{
				eBlockState = BRN30853_BLOCK_STATE_STORE;
				for (uDrc = 0; uDrc < EURASIA_USE_DRC_BANK_SIZE; uDrc++)
				{
					abMemReadSent[uDrc] = IMG_FALSE;
				}
			}
			/*
				Flag that any memory load or IDF has been issued on a DRC.
			*/
			else if (psInst->eOpcode == IIDF)
			{
				PARG psDRCArg = &psInst->asArg[IDF_DRC_SOURCE];

				ASSERT(psDRCArg->uType == USEASM_REGTYPE_DRC);
				ASSERT(psDRCArg->uNumber < EURASIA_USE_DRC_BANK_SIZE);
				abMemReadSent[psDRCArg->uNumber] = IMG_TRUE;
			}
			else if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMLOAD) != 0)
			{
				PARG psDRCArg = &psInst->asArg[MEMLOAD_DRC_ARGINDEX];

				ASSERT(psDRCArg->uType == USEASM_REGTYPE_DRC);
				ASSERT(psDRCArg->uNumber < EURASIA_USE_DRC_BANK_SIZE);
				abMemReadSent[psDRCArg->uNumber] = IMG_TRUE;
			}
			/*
				If the program waits for a memory load/IDF then all
				preceding stores can't be affected by the bug.
			*/
			else if (psInst->eOpcode == IWDF)
			{
				PARG psDRCArg = &psInst->asArg[WDF_DRC_SOURCE];

				ASSERT(psDRCArg->uType == USEASM_REGTYPE_DRC);
				ASSERT(psDRCArg->uNumber < EURASIA_USE_DRC_BANK_SIZE);

				if (abMemReadSent[psDRCArg->uNumber])
				{
					eBlockState = BRN30853_BLOCK_STATE_WDF;
					abMemReadSent[psDRCArg->uNumber] = IMG_FALSE;
				}
			}
		}

		aeBlockState[psBlock->uIdx] = eBlockState;
		if (eBlockState != BRN30853_BLOCK_STATE_NONE)
		{
			bAnyLoadOrStore = IMG_TRUE;
		}
	}

	/*
		Combine the effect of each block with the effect of it's predecessors in the control
		flow graph.
	*/
	if (bAnyLoadOrStore)
	{
		DoDataflow(psState, 
				   psFunc, 
				   IMG_TRUE /* bForwards */, 
				   sizeof(aeBlockState[0]), 
				   aeBlockState, 
				   ProcessBlockBRN30853,
				   IMG_NULL /* pvUserData */);
	}

	/*
		The state of the exit block is now the overall state of the function.
	*/
	ASSERT(psFunc->sCfg.psExit->uIdx < psFunc->sCfg.uNumBlocks);
	eExitState = aeBlockState[psFunc->sCfg.psExit->uIdx];

	UscFree(psState, aeBlockState);
	aeBlockState = NULL;

	return eExitState;
}

static
IMG_VOID AddFixForBRN30853ToProg(PINTERMEDIATE_STATE psState, PBRN30853_BLOCK_STATE aeFuncState, PFUNC psProg)
/*****************************************************************************
 FUNCTION	: AddFixForBRN30853ToProg
    
 PURPOSE	: Insert the workaround for BRN30853 to a function if required.

 PARAMETERS	: psState		- Compiler state.
			  aeFuncState	- For each function: does it exit with memory stores
							pending.
			  psProg		- Function to add the fix to.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psProg->uLabel < psState->uMaxLabel);
	if (aeFuncState[psProg->uLabel] == BRN30853_BLOCK_STATE_STORE)
	{
		PINST	psIDFInst;

		psIDFInst = InsertIDF(psState, psProg->sCfg.psExit, psProg->sCfg.psExit->psBodyTail);
		SetDependentReadCounter(psState, psIDFInst, 0 /* uDRCIdx */);

		InsertWDF(psState, psProg->sCfg.psExit, 0 /* uDRCIdx */, psProg->sCfg.psExit->psBodyTail);
	}
}

static
IMG_VOID AddFixForBRN30853(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AddFixForBRN30853
    
 PURPOSE	: Insert the workaround for BRN30853.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC					psFunc;
	PBRN30853_BLOCK_STATE	aeFuncState;
	IMG_UINT32				uLabel;
	IMG_UINT32				uStore;
	IMG_BOOL				bStoresPresent;
	static const IOPCODE	aeStoreOpcodes[] = {ISTAB, ISTAW, ISTAD, ISTLB, ISTLW, ISTLD};

	/*
		Check if this core is affected by BRN30853.
	*/
	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_30853) == 0)
	{
		return;
	}

	/*
		Check for a program containing no memory store instructions at all.
	*/
	bStoresPresent = IMG_FALSE;
	for (uStore = 0; uStore < (sizeof(aeStoreOpcodes) / sizeof(aeStoreOpcodes[0])); uStore++)
	{
		if (!SafeListIsEmpty(&psState->asOpcodeLists[aeStoreOpcodes[uStore]]))
		{
			bStoresPresent = IMG_TRUE;
			break;
		}
	}
	if (!bStoresPresent)
	{
		return;
	}

	/*
		Allocate memory for the effect of each function on the DCU.
	*/
	aeFuncState = UscAlloc(psState, sizeof(aeFuncState[0]) * psState->uMaxLabel);
	for (uLabel = 0; uLabel < psState->uMaxLabel; uLabel++)
	{
		aeFuncState[uLabel] = BRN30853_BLOCK_STATE_UNKNOWN;
	}

	/*
		Process functions from most to least deeply nested to we process each function
		after we've processed the functions it calls.
	*/
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		ASSERT(psFunc->uLabel < psState->uMaxLabel);
		aeFuncState[psFunc->uLabel] = GetBRN30853FunctionState(psState, psFunc, aeFuncState);
	}

	/*
		If either the main or secondary program exit with a memory store pending then
		add the fix.
	*/
	AddFixForBRN30853ToProg(psState, aeFuncState, psState->psMainProg);
	AddFixForBRN30853ToProg(psState, aeFuncState, psState->psSecAttrProg);

	UscFree(psState, aeFuncState);
}

/*****************************************************************************
 FUNCTION	: FinaliseShader
    
 PURPOSE	: Perform minor changes and optimisation to the intermediate
			  code prior to generating corresponding HW instructions

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Error code.
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 FinaliseShader(PINTERMEDIATE_STATE psState)
{
	/*
		Check if we still want to split the shader into pre- and post-ISP feedback
		phases. Optimizations might have removed all the instructions from the
		post-ISP feedback phases making splitting useless.
	*/
	if (psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
	{
		if (!IsPunchthroughFBStillSplit(psState))
		{
			ClearFeedbackSplit(psState);
		}
	}
	if (psState->uFlags2 & USC_FLAGS2_SPLITCALC)
	{
		if (!IsSampleRateStillSplit(psState))
		{
			psState->uFlags2 &= ~USC_FLAGS2_SPLITCALC;
			psState->psPreSplitBlock = NULL;
		}
	}

	/*
		Limit relative addressing of the input registers if needed
	*/
	AddSMRForRelativeAddressing(psState);

	psState->auNonAnisoTexStages = CallocBitArray(psState, psState->psSAOffsets->uTextureCount);
	if(psState->uOptimizationHint & USC_COMPILE_HINT_DETERMINE_NONANISO_TEX)
	{
		/*
			Determine which textures should have anisotropic filtering disabled
		*/
		DoOnAllBasicBlocks(psState, ANY_ORDER, DetermineNonAnisoTextureStagesBP,
						IMG_FALSE /*CALLs*/, NULL);
	}

	/*
		Apply various fixes to C10 instructions for limitations of the
		hardware.
	*/
	#if defined(DEBUG)
	ForAllInstructionsOfType(psState, IMOV, CheckC10Move);
	#endif /* defined(DEBUG) */
		
	#if defined(INITIALISE_REGS_ON_FIRST_WRITE)
	if (psState->uFlags & USC_FLAGS_INITIALISEREGSONFIRSTWRITE)
	{
		TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Initialize registers --------\n")));
		/*
			Determine the components of each destination-register that have been
			written prior to each instruction
		*/
		ComputeShaderRegsInitialised(psState);
		/*
			Force partially written registers to be fully initialised
		*/
		DoOnAllBasicBlocks(psState, ANY_ORDER, InitializeDestsBP, IMG_FALSE, NULL);
		
		TESTONLY(PrintIntermediate(psState, "init_dests", IMG_FALSE));
	}
	#endif /* defined(INITIALISE_REGS_ON_FIRST_WRITE) */

	/*
		Finalise the temporary register numbers for indexable-temp accesses
	*/
	TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Post FinaliseIndexableTempAccesses() --------\n")));
	DoOnAllBasicBlocks(psState, ANY_ORDER, FinaliseIndexableTempsBP, IMG_FALSE, NULL);
	TESTONLY(PrintIntermediate(psState, "fin_idxtmp", IMG_TRUE));
	
	/*
		Insert repeating instructions.
	*/
	GroupInstructionsProgram(psState);
	TESTONLY_PRINT_INTERMEDIATE("Instruction Grouping", "groupinst", IMG_FALSE);
	
	/*
		Insert IDF/WDF instructions.
	*/
	TESTONLY(DBG_PRINTF((DBG_MESSAGE, "------- Insert waits --------\n")));
	DoOnAllBasicBlocks(psState, ANY_ORDER, InsertWaitsBP, IMG_FALSE, NULL);
	TESTONLY(PrintIntermediate(psState, "ins_wait", IMG_FALSE));

	#ifdef SRC_DEBUG
	psState->uFlags |= USC_FLAGS_NOSCHED_FLAG;
	#endif /* SRC_DEBUG */

	
	/*
		Actually merely want to *predicate* CALL instructions (rather than branch
		around them), when it's safe to do so (re. syncstart/545 etc.), and leave
		them in their own blocks. (If the blocks are unconditional, layout will
		fall through them anyway.) This is just a specific case of flattening
		conditionals, which is "not yet implemented"!
	*/
	//MergeCallBlocks(psState);
	

	/*
		Remove NO_EMIT Instructions.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, RemoveNoEmitInstructionsBP, IMG_FALSE/*CALLs*/, NULL);

	/*
		Set the sync start flag.
	*/
	SetupSyncStartFlag(psState);

	/*
		Add the workaround for BRN25804.
	*/
	AddFixForBRN25804(psState);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Add a fix for hardware restrictions on the VDUAL instruction.
	*/
	AddVDualFirstFix(psState);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Add the workaround for BRN31988.
	*/
	AddFixForBRN31988(psState);

	/*
		Add the workaround for BRN30853.
	*/
	AddFixForBRN30853(psState);

	/*
		Insert placeholders where the driver needs to insert code.
	*/
	InsertPaddingInstructions(psState);

	if
	(
		(psState->uFlags2 & USC_FLAGS2_SPLITCALC) &&
		(psState->psPreSplitBlock != NULL)	
	)
	{
		if
		(
			(psState->psPreSplitBlock->psBody == NULL) &&
			(psState->psPreSplitBlock != psState->psMainProg->sCfg.psEntry)
		)
		{
			PINST psNop;
			psNop = AllocateInst(psState, NULL);
			SetOpcode(psState, psNop, INOP);
			SetBit(psNop->auFlag, INST_END, 1);
			AppendInst(psState, psState->psPreSplitBlock, psNop);		
		}
		if(psState->psPreSplitBlock->psBody != NULL)
		{
			if(!SupportsEndFlag(psState, psState->psPreSplitBlock->psBodyTail))
			{
				PINST psNop;
				psNop = AllocateInst(psState, NULL);
				SetOpcode(psState, psNop, INOP);
				SetBit(psNop->auFlag, INST_END, 1);
				AppendInst(psState, psState->psPreSplitBlock, psNop);			
			}
			else
			{
				SetBit(psState->psPreSplitBlock->psBodyTail->auFlag, INST_END, 1);
			}
		}
	}	

	/*
		Insert the nosched flag.
	*/
	SetupNoSchedFlag(psState);
	TESTONLY_PRINT_INTERMEDIATE("NOSCHED flag", "nosched", IMG_TRUE);

	return UF_OK;
}
