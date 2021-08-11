/******************************************************************************
 Name           : USP_INSTBLOCK.C

 Title          : USP instruction-block handling

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2007 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Management of instruction-blocks used internally by the USP

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.38 $

 Modifications  :

 $Log: usp_instblock.c $
******************************************************************************/
#include <string.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "uspbin.h"
#include "hwinst.h"
#include "uspshrd.h"
#include "uspshader.h"
#include "usp_resultref.h"
#include "usp_instblock.h"

/*****************************************************************************
 Name:		USPInstBlockReset

 Purpose:	Initialise the instrucitons in a block, ready for finalising

 Inputs:	psInstBlock	- The instruction block to reset

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPInstBlockReset(PUSP_INSTBLOCK psInstBlock)
{
	IMG_UINT32		uOrgInstCount;
	IMG_UINT32		uResultRefIdx;
	PUSP_SHADER		psShader;
	PUSP_RESULTREF	psResultRef;

	/*
		Reset the list of instructions forming this block
	*/
	psInstBlock->psFirstInst	= IMG_NULL;
	psInstBlock->psLastInst		= IMG_NULL;
	psInstBlock->uInstCount		= 0;

	/*
		Build an instruction list containing just the pre-compiled
		instructions associated with this block

		NB: By default the MOE state will be constant throughout
			each block of instructions, so all PC instructions can
			have the MOE state associated with the block.
	*/
	uOrgInstCount = psInstBlock->uOrgInstCount;
	if	(uOrgInstCount > 0)
	{
		PHW_INST		psOrgInst;
		PUSP_INSTDESC	psOrgInstDesc;
		PUSP_INST		psInst;
		PUSP_INST		psLastInst;
		PUSP_INST		psFirstInst;

		psFirstInst		= psInstBlock->psInsts;
		psLastInst		= psFirstInst + uOrgInstCount - 1;
		psOrgInst		= psInstBlock->psOrgInsts;
		psOrgInstDesc	= psInstBlock->psOrgInstDescs;

		for	(psInst = psFirstInst; psInst <= psLastInst; psInst = psInst->psNext)
		{
			psInst->psInstBlock	= psInstBlock;
			psInst->sHWInst		= *psOrgInst++;
			psInst->sDesc		= *psOrgInstDesc++;
			psInst->psNext		= psInst + 1;
			psInst->psPrev		= psInst - 1;
		}

		psFirstInst->psPrev	= IMG_NULL;
		psLastInst->psNext	= IMG_NULL;

		psInstBlock->psFirstInst	= psFirstInst;
		psInstBlock->psLastInst		= psLastInst;
		psInstBlock->uInstCount		= uOrgInstCount;
	}

	/*
		Remove and disable any result references from the shader, that
		were added for USP-generated instructions
	*/
	psShader	= psInstBlock->psShader;
	psResultRef = &psInstBlock->psResultRefs[0];

	for	(uResultRefIdx = 0; 
		 uResultRefIdx < psInstBlock->uMaxNonPCInstCount; 
		 uResultRefIdx++)
	{
		if	(psResultRef->bActive)
		{
			USPShaderRemoveResultRef(psShader, psResultRef);
			psResultRef->bActive = IMG_FALSE;
		}
		psResultRef++;
	}

	/*
		Reset the MOE state at the end of the block
	*/
	if	(psInstBlock->psLastInst)
	{
		psInstBlock->sFinalMOEState = psInstBlock->psLastInst->sMOEState;
	}
	else
	{
		psInstBlock->sFinalMOEState = psInstBlock->sInitialMOEState;
	}
}

/*****************************************************************************
 Name:		USPInstDescSetup

 Purpose:	Setup a USP instruction description for a HW instruction

 Inputs:	psInstDesc		- The instruciton description to setup
			eOpcode			- The opcode of the HW-instruciton
			psHWInst		- The HW instruction the description if for
			uInstDescFlags	- Additional flags for the instruction-description
			psMOEState		- The MOE state for the instruction

 Outputs:	none

 Returns:	IMG_TRUE on success. IMG_FALSE if there is not enough space.
*****************************************************************************/
static IMG_BOOL USPInstDescSetup(PUSP_INSTDESC	psInstDesc,
								 USP_OPCODE		eOpcode,
								 PHW_INST		psHWInst,
								 IMG_UINT32		uInstDescFlags,
								 PUSP_MOESTATE	psMOEState)
{
	psInstDesc->eOpcode	= eOpcode;
	psInstDesc->uFlags	= uInstDescFlags;

	if	(!HWInstGetPerOperandFmtCtl(psMOEState,
									eOpcode,
									psHWInst,
									&psInstDesc->eFmtCtl))
	{
		return IMG_FALSE;
	}

	if	(HWInstSupportsNoSched(eOpcode))
	{
		psInstDesc->uFlags |= USP_INSTDESC_FLAG_SUPPORTS_NOSCHED;
	}

	if	(HWInstSupportsSyncStart(eOpcode))
	{
		IMG_BOOL bSyncStartSet;

		HWInstGetSyncStart(eOpcode, psHWInst, &bSyncStartSet);
		if	(bSyncStartSet)
		{
			psInstDesc->uFlags |= USP_INSTDESC_FLAG_SYNCSTART_SET;
		}
	}

	if	(HWInstSupportsSyncEnd(eOpcode))
	{
		IMG_BOOL bSyncEndSet;

		HWInstGetSyncEnd(eOpcode, psHWInst, &bSyncEndSet);
		if	(bSyncEndSet)
		{
			psInstDesc->uFlags |= USP_INSTDESC_FLAG_SYNCEND_SET;
		}
	}

	if	(HWInstForcesDeschedule(eOpcode, psHWInst))
	{
		psInstDesc->uFlags |= USP_INSTDESC_FLAG_FORCES_DESCHEDULE;
	}

	if	(HWInstSupportsWriteMask(eOpcode))
	{
		IMG_UINT32 uWriteMask;

		if	(!HWInstDecodeWriteMask(eOpcode, psHWInst, &uWriteMask))
		{
			return IMG_FALSE;
		}

		if	(uWriteMask != 0xF)
		{
			psInstDesc->uFlags |= USP_INSTDESC_FLAG_PARTIAL_RESULT_WRITE;
		}
	}

	if	(HWInstCanUseExtSrc0Banks(eOpcode))
	{
		psInstDesc->uFlags |= USP_INSTDESC_FLAG_SUPPORTS_SRC0_EXTBANKS;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockInserInst

 Purpose:	Insert a USP instruciton into an instruciton block

 Inputs:	psInstBlock		- The instruciotn block to add the instruction to
			psInst			- The instruciton to insert
			psInsertPos		- An existing instruction in the block before
							  which the new instruction will be inserted. If
							  IMG_NULL then the new instruction is appended
							  to the block.

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPInstBlockInserInst(PUSP_INSTBLOCK	psInstBlock,
									  PUSP_INST			psInst,
									  PUSP_INST			psInsertPos)
{
	/*
		Insert or append the new instruction into the list
	*/
	if	(psInsertPos)
	{
		psInst->psPrev = psInsertPos->psPrev;
		if	(psInsertPos->psPrev)
		{
			psInsertPos->psPrev->psNext = psInst;
		}

		psInst->psNext = psInsertPos;
		psInsertPos->psPrev = psInst;

		if	(psInsertPos == psInstBlock->psFirstInst)
		{
			psInstBlock->psFirstInst = psInst;
		}
	}
	else
	{
		PUSP_INST	psLastInst;

		psLastInst = psInstBlock->psLastInst;
		if	(psLastInst)
		{
			psInst->psNext = psLastInst->psNext;
			if	(psLastInst->psNext)
			{
				psLastInst->psNext->psPrev = psInst;
			}

			psInst->psPrev = psLastInst;
			psLastInst->psNext = psInst;
		}
		else
		{
			psInst->psNext = IMG_NULL;
			psInst->psPrev = IMG_NULL;

			psInstBlock->psFirstInst = psInst;
		}

		psInstBlock->psLastInst = psInst;
	}

	psInstBlock->uInstCount++;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockInsertHWInst

 Purpose:	Insert a HW instruction into a block

 Inputs:	psInstBlock		- The instruction block to modify
			psInsertPos		- An instruction before which the new one is
							  inserted. If NULL then the inst is appended.
			eOpcode			- The opcode of the HW-instruciton being added
			psHWInst		- The HW instruction to add 
			uInstDescFlags	- Flags for the instruction-description.
			psContext		- The current USP execution context

 Outputs:	ppsInst			- If non-NULL, set to the created USP instruction

 Returns:	IMG_TRUE on success. IMG_FALSE if there is not enough space.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInstBlockInsertHWInst(PUSP_INSTBLOCK	psInstBlock,
											   PUSP_INST		psInsertPos,
											   USP_OPCODE		eOpcode,
											   PHW_INST			psHWInst,
											   IMG_UINT32		uInstDescFlags,
											   PUSP_CONTEXT		psContext,
											   PUSP_INST*		ppsInst)
{
	IMG_UINT32	uInstCount;
	PUSP_INST	psNewInst;

	/*
		Get a new instruction from the block
	*/
	uInstCount = psInstBlock->uInstCount;
	if	(uInstCount == psInstBlock->uMaxInstCount)
	{
		USP_DBGPRINT(("USPInstBlockInsertHWInst: Block is full\n"));
		return IMG_FALSE;
	}
	else
	{
		psNewInst = &psInstBlock->psInsts[uInstCount];
	}

	/*
		Insert the new instruction into the block at the required position
	*/
	if	(!USPInstBlockInserInst(psInstBlock, psNewInst, psInsertPos))
	{
		USP_DBGPRINT(("USPInstBlockInsertHWInst: Failed to insert inst into block\n"));
		return IMG_FALSE;
	}

	/*
		Setup the USP instruction from the HW-instruction

		NB: Inserting MOE instructions into the middle of a block does
			not cause the MOE state for subsequent instructions to be
			updated. It is up to the rest of the code to ensure that
			the MOE state is correct for following instructions in a 
			block.
	*/
	psNewInst->sHWInst		= *psHWInst;
	psNewInst->psInstBlock	= psInstBlock;
	psNewInst->psResultRef	= IMG_NULL;

	if	(!psNewInst->psPrev)
	{
		psNewInst->sMOEState = psInstBlock->sInitialMOEState;
	}
	else
	{
		if	(!psNewInst->psNext)
		{
			psNewInst->sMOEState = psInstBlock->sFinalMOEState;
		}
		else
		{
			psNewInst->sMOEState = psInsertPos->sMOEState;
		}
	}

	if	(!USPInstDescSetup(&psNewInst->sDesc,
						   eOpcode,
						   psHWInst,
						   uInstDescFlags,
						   &psNewInst->sMOEState))
	{
		USP_DBGPRINT(("USPInstBlockInsertHWInst: Error setting-up instruction desc\n"));
		return IMG_FALSE;
	}

	/*
		If this instruction references the shader result, add a new result-
		reference for it
	*/
	if	(uInstDescFlags & (USP_INSTDESC_FLAG_DEST_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC0_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC2_USES_RESULT))
	{
		PUSP_RESULTREF	psResultRef;
		IMG_UINT32		uResultRefIdx;

		/*
			Get a result-reference for this instruction from the pool of all those
			available in the block
		*/
		psResultRef = &psInstBlock->psResultRefs[0];

		for	(uResultRefIdx = 0; 
			 uResultRefIdx < psInstBlock->uMaxNonPCInstCount; 
			 uResultRefIdx++)
		{
			if	(!psResultRef->bActive)
			{	
				break;
			}
			psResultRef++;
		}

		if	(uResultRefIdx == psInstBlock->uMaxNonPCInstCount)
		{
			USP_DBGPRINT(("USPInstBlockInsertHWInst: Failed to get result-ref for inst\n"));
			return IMG_FALSE;
		}

		/*
			Associate the instruciton with the result-ref
		*/
		if	(!USPResultRefSetInst(psResultRef, psNewInst, psContext))
		{
			USP_DBGPRINT(("USPInstBlockInsertHWInst: Error setting result-ref inst\n"));
			return IMG_FALSE;
		}
		psNewInst->psResultRef = psResultRef;

		/*
			Add the result-ref to the list of all those for the shader
		*/
		if	(!USPShaderAddResultRef(psInstBlock->psShader, psResultRef))
		{
			USP_DBGPRINT(("USPInstBlockInsertHWInst: Error adding result-ref to shader\n"));
			return IMG_FALSE;
		}
	}

	/*
		If we appended the new instruction, and it changes the MOE state,
		update the MOE state at the end of the block to account for it.
	*/
	if	(!psNewInst->psNext)
	{
		if	(HWInstIsMOEControlInst(eOpcode))
		{
			if	(!HWInstUpdateMOEState(eOpcode,
									   psHWInst,
									   &psInstBlock->sFinalMOEState))
			{
				USP_DBGPRINT(("USPInstBlockInsertHWInst: Error updating final MOE state\n"));
				return IMG_FALSE;
			}
		}
	}

	/*
		Return the created USP instruction if required
	*/
	if	(ppsInst)
	{
		*ppsInst = psNewInst;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockAddPCInst

 Purpose:	Append a pre-compiled HW instruction to an instruction-block

 Inputs:	psInstBlock		- The instruction block to modify
			psPCInst		- The pre-compiled HW instruction to add 
			uInstDescFlags	- Flags for the instruction-description
			psContext		- The current USP execution context

 Outputs:	ppsInst			- If non-NULL, set to the created USP instruction

 Returns:	IMG_TRUE on success. IMG_FALSE if there is not enough space.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInstBlockAddPCInst(PUSP_INSTBLOCK	psInstBlock,
											PHW_INST		psPCInst,
											IMG_UINT32		uInstDescFlags,
											PUSP_CONTEXT	psContext,
											PUSP_INST*		ppsInst)
{
	IMG_UINT32		uInstCount;
	PUSP_INST		psNewInst;
	PUSP_INSTDESC	psOrgInstDesc;
	USP_OPCODE		eOpcode;

	/*
		Get a new instruction from the block
	*/
	uInstCount = psInstBlock->uInstCount;
	if	(uInstCount == psInstBlock->uMaxInstCount)
	{
		USP_DBGPRINT(("USPInstBlockAddPCInst: Block is full\n"));
		return IMG_FALSE;
	}
	else
	{
		psNewInst = &psInstBlock->psInsts[uInstCount];
	}

	/*
		Record the original PC instruction, so we can restore it before
		finalisation
	*/
	psInstBlock->psOrgInsts[uInstCount] = *psPCInst;

	/*
		Setup a description of the original instruction
	*/
	uInstDescFlags |= USP_INSTDESC_FLAG_PRECOMPILED_INST;

	if	(!HWInstGetOpcode(psPCInst, &eOpcode))
	{
		USP_DBGPRINT(("USPInstBlockAddPCInst: Error decoding PC-inst opcode\n"));
		return IMG_FALSE;
	}

	psOrgInstDesc = &psInstBlock->psOrgInstDescs[uInstCount];

	if	(!USPInstDescSetup(psOrgInstDesc,
						   eOpcode,
						   psPCInst,
						   uInstDescFlags,
						   &psInstBlock->sFinalMOEState))
	{
		USP_DBGPRINT(("USPInstBlockAddPCInst: Error setting-up instruction desc\n"));
		return IMG_FALSE;
	}

	/*
		Setup a new USP instruction for the PC instruction
	*/
	psNewInst = &psInstBlock->psInsts[uInstCount];

	psNewInst->sHWInst		= *psPCInst;
	psNewInst->psInstBlock	= psInstBlock;
	psNewInst->sDesc		= *psOrgInstDesc;
	psNewInst->sMOEState	= psInstBlock->sFinalMOEState;
	psNewInst->psResultRef	= IMG_NULL;

	/*
		Append the new instruction to the block
	*/
	if	(!USPInstBlockInserInst(psInstBlock, psNewInst, IMG_NULL))
	{
		USP_DBGPRINT(("USPInstBlockAddPCInst: Failed to insert inst into block\n"));
		return IMG_FALSE;
	}

	/*
		If this instruction references the shader result, add a new result-
		reference for it
	*/
	if	(uInstDescFlags & (USP_INSTDESC_FLAG_DEST_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC0_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
						   USP_INSTDESC_FLAG_SRC2_USES_RESULT))
	{
		PUSP_RESULTREF	psResultRef;
		PUSP_SHADER		psShader;

		psShader = psInstBlock->psShader;

		/*
			Create and initialise a new result-reference
		*/
		psResultRef = USPResultRefCreate(psContext, psShader);
		if	(!psResultRef)
		{
			USP_DBGPRINT(("USPInstBlockAddPCInst: Failed to create result-ref\n"));
			return IMG_FALSE;
		}

		/*
			Associate the new instruciton with the created result-ref
		*/
		if	(!USPResultRefSetInst(psResultRef, psNewInst, psContext))
		{
			USP_DBGPRINT(("USPInstBlockAddPCInst: Error setting result-ref inst\n"));
			return IMG_FALSE;
		}
		psNewInst->psResultRef = psResultRef;

		/*
			Add the result-ref to the list of all those for the shader
		*/
		if	(!USPShaderAddResultRef(psShader, psResultRef))
		{
			USP_DBGPRINT(("USPInstBlockAddPCInst: Error adding result-ref to shader\n"));
			return IMG_FALSE;
		}
	}

	/*
		If the new instruction changes the MOE state, update the MOE 
		state at the end of the block to account for it.
	*/
	if	(HWInstIsMOEControlInst(eOpcode))
	{
		if	(!HWInstUpdateMOEState(eOpcode,
								   psPCInst,
								   &psInstBlock->sFinalMOEState))
		{
			USP_DBGPRINT(("USPInstBlockAddPCInst: Error updating final MOE state\n"));
			return IMG_FALSE;
		}
	}

	/*
		Return the created USP instruction if required
	*/
	if	(ppsInst)
	{
		*ppsInst = psNewInst;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockRemoveInst

 Purpose:	Remove a USP-instruction from a block

			NB: This does not update the MOE state at the end of the block
				when removing a MOE instruction.

 Inputs:	psInstBlock	- The instruction block to modify
			psInst		- The instruction to remove from the block

 Outputs:	none

 Returns:	IMG_TRUE on success. IMG_FALSE if there is not enough space.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInstBlockRemoveInst(PUSP_INSTBLOCK	psInstBlock,
											 PUSP_INST		psInst)
{
	PUSP_INST psPrev, psNext;

	/*
		If this instruction references the shader result, remove and 
		disable the associated result-references.
	*/
	if	(psInst->sDesc.uFlags & (USP_INSTDESC_FLAG_DEST_USES_RESULT |
								 USP_INSTDESC_FLAG_SRC0_USES_RESULT |
								 USP_INSTDESC_FLAG_SRC1_USES_RESULT |
								 USP_INSTDESC_FLAG_SRC2_USES_RESULT))
	{
		PUSP_RESULTREF	psResultRef;
		PUSP_SHADER		psShader;

		psShader	= psInstBlock->psShader;
		psResultRef	= psInst->psResultRef;

		if	(psResultRef->bActive)
		{
			if	(!USPShaderRemoveResultRef(psShader, psResultRef))
			{
				return IMG_FALSE;
			}
			psResultRef->bActive = IMG_FALSE;
		}
	}

	/*
		Unlink the instruction from others in the block
	*/
	psPrev = psInst->psPrev;
	psNext = psInst->psNext;

	if	(psNext)
	{
		psNext->psPrev = psPrev;
	}
	if	(psPrev)
	{
		psPrev->psNext = psNext;
	}

	psInst->psNext = psInst->psPrev = IMG_NULL;

	/*
		Update the start and end of the list if needed
	*/
	if	(psInst == psInstBlock->psFirstInst)
	{
		psInstBlock->psFirstInst = psNext;
	}
	if	(psInst == psInstBlock->psLastInst)
	{
		psInstBlock->psLastInst = psPrev;
	}

	psInstBlock->uInstCount--;

	return IMG_TRUE;
}

#define USP_TARGETINST_FIRST				(1)
#define USP_TARGETINST_SECOND				(2)
#define USP_TARGETINST_BOTH					(3)

/*****************************************************************************
 Name:		USPInstBlockSetupNoSchedFlag

 Purpose:	Setup the NoSched flag for the instrucitons within a block

 Inputs:	psInstBlock		- The instruction block to modify
			psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPInstBlockSetupNoSchedFlag(PUSP_INSTBLOCK	psInstBlock,
											 PUSP_CONTEXT	psContext)
#if defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING)
{
	PUSP_INST		psInst;
	PUSP_INST		psPrevInst;
	PUSP_INSTBLOCK	psPrevInstBlock;
	IMG_UINT32		uFirstInstIdx;
	IMG_INT32		iIdxInBlock;
	IMG_BOOL		bFailed;
	IMG_BOOL		bIsBranchDest;

	USP_UNREFERENCED_PARAMETER(psContext);

	bFailed	= IMG_TRUE;

	/*
		Calculate the index of the first instruction of this block, within
		the whole shader.
	*/
	psPrevInstBlock	= psInstBlock->psPrev;
	if	(psPrevInstBlock)
	{
		uFirstInstIdx = psPrevInstBlock->uFirstInstIdx + 
						psPrevInstBlock->uInstCount;
	}
	else
	{
		uFirstInstIdx = 0;
	}
	psInstBlock->uFirstInstIdx = uFirstInstIdx;

	/*
		Align the start of this block to an even-instruction if required...
	*/
	if	(psInstBlock->bAlignStartToPair && (uFirstInstIdx & 1))
	{
		HW_INST	sNOP;

		HWInstEncodeNOPInst(&sNOP);

		if	(!USPInstBlockInsertHWInst(psPrevInstBlock, 
									   IMG_NULL,
									   USP_OPCODE_NOP,
									   &sNOP,
									   0,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		psInstBlock->uFirstInstIdx++;
		uFirstInstIdx++;
	}

	/*
		Nothing else to do for empty blocks
	*/
	if	(psInstBlock->uInstCount == 0)
	{
		return IMG_TRUE;
	}

	/*
		Find the previous non-empty block
	*/
	bIsBranchDest = psInstBlock->bIsBranchDest;
	while (psPrevInstBlock && !psPrevInstBlock->psLastInst)
	{
		if (psPrevInstBlock->bIsBranchDest)
		{
			bIsBranchDest = IMG_TRUE;
		}

		psPrevInstBlock = psPrevInstBlock->psPrev;	
	}

	/*
		If this block isn't a branch destination, join the preceeding
		instructions those for this one, so that they appear as a 
		contiguous list.
		
		NB:	This allows us to set the NoSched flag on instructions in
			preceeding blocks if needed.
	*/
	if	(!bIsBranchDest && psPrevInstBlock)
	{
		PUSP_INST	psPrevInst;
		PUSP_INST	psFirstInst;

		/*
			Connect the two lists
		*/
		psFirstInst = psInstBlock->psFirstInst;
		psPrevInst	= psPrevInstBlock->psLastInst;

		psPrevInst->psNext	= psFirstInst;
		psFirstInst->psPrev = psPrevInst;
	}

	/*
		Find the last instruction within this block, based upon the
		given starting HW instruction index.
	*/
	psInst		= psInstBlock->psLastInst;
	iIdxInBlock	= psInstBlock->uInstCount - 1;

	/*
		Treat blocks that form branch destinations as if they were the first
		block in the shader, to prevent setting the NoSched on instructions
		in the preceding block.
	*/
	if	(bIsBranchDest)
	{
		uFirstInstIdx = 0;	
	}

	/*
		Setup the NoSched flag for instructions in the block.
	*/
	for(; iIdxInBlock >= 0; iIdxInBlock --, psInst = psPrevInst)
	{
		PUSP_INST		psInsertPos;
		PUSP_INSTBLOCK	psInsertPosBlock;
		PUSP_INST		psNSInstruction;
		IMG_UINT32		uNOPInstDescFlags;
		HW_INST			sNOP;

		/*
			Get the preceeding instruction (if any) in this block
		*/
		if	((uFirstInstIdx + iIdxInBlock) >= 1)
		{
			psPrevInst = psInst->psPrev;
		}
		else
		{
			psPrevInst = IMG_NULL;
		}

		/*
			Ignore instructions that dont need descheduling disabled	
		*/
		if	(!(psInst->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE))
		{
			continue;
		}

		/*
			Internal registers can't live at the start of a block or immediately
			after a descheduling instruction.
		*/
		if (psPrevInst == NULL)
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Internal registers live at the start of a block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}
		if (psPrevInst->sDesc.uFlags & USP_INSTDESC_FLAG_FORCES_DESCHEDULE)
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Internal registers live after descheduling instruction\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		/*
			Check if we can set the nosched flag on instruction preceeding the pair of instructions
			between which the internal registers are live.
		*/
		psNSInstruction = psPrevInst->psPrev;
		if (psNSInstruction != NULL)
		{
			if	(psNSInstruction->sDesc.uFlags & USP_INSTDESC_FLAG_SUPPORTS_NOSCHED)
			{
				if	(!HWInstSetNoSched(psNSInstruction->sDesc.eOpcode,
									   &psNSInstruction->sHWInst,
									   IMG_TRUE))
				{
					USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
					goto USPInstBlockSetupNoSchedFlagExit;
				}
				continue;
			}
		}

		/*
			Insert a NOP instruction just before the pair of instructions.
		*/
		psInsertPos = psPrevInst;
		psInsertPosBlock = psInsertPos->psInstBlock;

		/*
			The internal registers can't be live across an instruction which doesn't
			support the nosched flag.
		*/
		if	(psPrevInst->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE)
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Illegal nosched instruction combination\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}
		uNOPInstDescFlags = 0;

		HWInstEncodeNOPInst(&sNOP);

		if	(!USPInstBlockInsertHWInst(psInsertPosBlock,
									   psInsertPos,
									   USP_OPCODE_NOP,
									   &sNOP,
									   uNOPInstDescFlags,
									   psContext,
									   &psNSInstruction))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		/*
			Were the NOPs inserted into the current block?
		*/
		if	(psInsertPosBlock == psInstBlock)
		{
			/*
				Update the index of the current instruction within this
				block, to account for the NOP we have inserted.
			*/
			iIdxInBlock++;
		}
		else
		{
			/*
				Update the start indexes (within the whole program) of all
				following blocks processed so far.
			*/
			do
			{
				psInsertPosBlock = psInsertPosBlock->psNext;
				psInsertPosBlock->uFirstInstIdx++;
			} while (psInsertPosBlock != psInstBlock);
		}

		/*
			Set the NS flag on the chosen NOP
		*/
		if	(!HWInstSetNoSched(USP_OPCODE_NOP,
							   &psNSInstruction->sHWInst,
							   IMG_TRUE))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}
	}

	/*
		NoSched flag setup OK
	*/
	bFailed = IMG_FALSE;

	USPInstBlockSetupNoSchedFlagExit:

	return (IMG_BOOL)!bFailed;
}
#else /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */
{
	PUSP_INST		psEvenInst;
	PUSP_INST		psPrevEvenInst;
	PUSP_INSTBLOCK	psPrevInstBlock;
	IMG_UINT32		uFirstInstIdx;
	IMG_INT32		iIdxInBlock;
	IMG_BOOL		bFailed;
	IMG_BOOL		bIsBranchDest;
	IMG_INT32		iBlockStart;

	USP_UNREFERENCED_PARAMETER(psContext);

	bFailed	= IMG_TRUE;

	/*
		Calculate the index of the first instruciton of this block, within
		the whole shader.
	*/
	psPrevInstBlock	= psInstBlock->psPrev;
	if	(psPrevInstBlock)
	{
		uFirstInstIdx = psPrevInstBlock->uFirstInstIdx + 
						psPrevInstBlock->uInstCount;
	}
	else
	{
		uFirstInstIdx = 0;
	}
	psInstBlock->uFirstInstIdx = uFirstInstIdx;

	/*
		Align the start of this block to an even-instruction if required...
	*/
	if	(psInstBlock->bAlignStartToPair && (uFirstInstIdx & 1))
	{
		HW_INST	sNOP;

		HWInstEncodeNOPInst(&sNOP);

		if	(!USPInstBlockInsertHWInst(psPrevInstBlock, 
									   IMG_NULL,
									   USP_OPCODE_NOP,
									   &sNOP,
									   0,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		psInstBlock->uFirstInstIdx++;
		uFirstInstIdx++;
	}

	/*
		Nothing else to do for empty blocks
	*/
	if	(psInstBlock->uInstCount == 0)
	{
		return IMG_TRUE;
	}

	/*
		Find the previous non-empty block
	*/
	bIsBranchDest = psInstBlock->bIsBranchDest;
	while (psPrevInstBlock && !psPrevInstBlock->psLastInst)
	{
		if (psPrevInstBlock->bIsBranchDest)
		{
			bIsBranchDest = IMG_TRUE;
		}

		psPrevInstBlock = psPrevInstBlock->psPrev;	
	}

	/*
		Avoid an illegal instruction pair between the first instruction
		of this block and the one preceeding it
	*/
	if	((uFirstInstIdx & 1) && psPrevInstBlock)
	{
		PUSP_INST	psEvenInst;
		PUSP_INST	psOddInst;

		psOddInst	= psInstBlock->psFirstInst;
		psEvenInst	= psPrevInstBlock->psLastInst;

		if	(HWInstIsIllegalInstPair(psEvenInst->sDesc.eOpcode,
									 &psEvenInst->sHWInst,
									 psOddInst->sDesc.eOpcode,
									 &psOddInst->sHWInst))
		{
			HW_INST	sNOP;

			HWInstEncodeNOPInst(&sNOP);

			if	(!USPInstBlockInsertHWInst(psPrevInstBlock,
										   IMG_NULL,
										   USP_OPCODE_NOP,
										   &sNOP,
										   0,
										   psContext,
										   IMG_NULL))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}

			psInstBlock->uFirstInstIdx++;
			uFirstInstIdx++;
		}
	}

	/*
		If this block isn't a branch destination, join the preceeding
		instructions those for this one, so that they appear as a 
		contiguous list.
		
		NB:	This allows us to set the NoSched flag on instructions in
			preceeding blocks if needed.
	*/
	if	(!bIsBranchDest && psPrevInstBlock)
	{
		PUSP_INST	psPrevInst;
		PUSP_INST	psFirstInst;

		/*
			Connect the two lists
		*/
		psFirstInst = psInstBlock->psFirstInst;
		psPrevInst	= psPrevInstBlock->psLastInst;

		psPrevInst->psNext	= psFirstInst;
		psFirstInst->psPrev = psPrevInst;
	}

	/*
		Find the last even-indexed instruction within this block, based upon the
		given starting HW instruction index.

		NB: If the block contains 1 instruction that's not evenly aligned, and
			this block has not been linked to the preceding one (a branch
			destination), then psEvenInst will end up as NULL and iIdxInBlock
			will be -1. As a result, the following loop will be skipped, but
			since the IRegs cannot be live prior to the instruction anyway
			(branches force a deschedule) this isn't an issue.
	*/
	psEvenInst	= psInstBlock->psLastInst;
	iIdxInBlock	= psInstBlock->uInstCount - 1;

	if	((uFirstInstIdx + iIdxInBlock) & 1)
	{
		psEvenInst = psEvenInst->psPrev;
		iIdxInBlock--;
	}

	/*
		Treat blocks that form branch destinations as if they were the first
		block in the shader, to prevent setting the NoSched on instructions
		in the preceding block.
	*/
	if	(bIsBranchDest)
	{
		uFirstInstIdx = 0;	
	}

	/*
		Setup the NoSched flag for instructions on even-instruction boundries
	*/
	iBlockStart = 0;
	for(; iIdxInBlock >= iBlockStart; iIdxInBlock -= 2, psEvenInst = psPrevEvenInst)
	{
		PUSP_INST		psInsertPos;
		PUSP_INSTBLOCK	psInsertPosBlock;
		PUSP_INST		apsNSPairInst[2];
		IMG_UINT32		uNSPairInvalid;
		IMG_UINT32		uNSTargetInstMask;
		IMG_UINT32		uNOPInstDescFlags;
		HW_INST			sNOP;

		/*
			Get the preceeding even instruction (if any) in this block
		*/
		if	((uFirstInstIdx + iIdxInBlock) >= 2)
		{
			psPrevEvenInst = psEvenInst->psPrev->psPrev;
		}
		else
		{
			psPrevEvenInst = IMG_NULL;
		}

		/*
			Ignore instructions that dont need descheduling disabled	
		*/
		if	(!(psEvenInst->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE))
		{
			continue;
		}

		/*
			Check whether the immediately preceding pair forces a deschedule

			NB: If the IRegs are live before this even-instruction, then there must
				be at-least 2 instructions preceding it in the whole shader.
		*/
		if	(
				psPrevEvenInst &&
				(psPrevEvenInst->sDesc.uFlags & USP_INSTDESC_FLAG_FORCES_DESCHEDULE)
			)
		{
			/*
				Insert 2 NOPs after the deschedule
			*/
			psInsertPos			= psPrevEvenInst->psNext;
			psInsertPosBlock	= psInsertPos->psInstBlock;

			HWInstEncodeNOPInst(&sNOP);

			if	(!USPInstBlockInsertHWInst(psInsertPosBlock,
										   psInsertPos,
										   USP_OPCODE_NOP,
										   &sNOP,
										   0,
										   psContext,
										   IMG_NULL))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}
			if	(!USPInstBlockInsertHWInst(psInsertPosBlock,
										   psInsertPos,
										   USP_OPCODE_NOP,
										   &sNOP,
										   0,
										   psContext,
										   &psPrevEvenInst))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}

			/*
				Were the NOPs inserted into the current block?
			*/
			if	(psInsertPosBlock == psInstBlock)
			{
				/*
					Update the index of the current instruction within this
					block, to account for the 2 NOPs we have inserted.
				*/
				iIdxInBlock += 2;
			}
			else
			{
				/*
					Update the start indexes (within the whole program) of all
					following blocks processed so far.
				*/
				psInsertPosBlock = psInsertPosBlock->psNext;
				do
				{
					psInsertPosBlock->uFirstInstIdx += 2;
				} while (psInsertPosBlock != psInstBlock);
			}
		}

		/*
			Determine which previous intructions we can put the NS flag on

			NB: It needs to go on either instruction in the pair before the
				preceeding even instruction. That is, either 3 or 4 instructions
				back from the current even one if possible.
		*/
		uNSPairInvalid		= 0x3;
		apsNSPairInst[0]	= IMG_NULL;
		apsNSPairInst[1]	= IMG_NULL;

		if	(psPrevEvenInst)
		{
			apsNSPairInst[1] = psPrevEvenInst->psPrev;
		}
		if	(apsNSPairInst[1])
		{
			/*
				Note whether w can use the instruciton 3 back from this one
			*/
			if	(apsNSPairInst[1]->sDesc.uFlags & USP_INSTDESC_FLAG_SUPPORTS_NOSCHED)
			{
				uNSPairInvalid ^= 0x2;
			}

			/*
				Note whether w can use the instruciton 4 back from this one
			*/
			apsNSPairInst[0] = apsNSPairInst[1]->psPrev;
			if	(apsNSPairInst[0])
			{
				if	(
						(apsNSPairInst[0]->sDesc.uFlags & USP_INSTDESC_FLAG_SUPPORTS_NOSCHED) &&
						!(apsNSPairInst[0]->sDesc.uFlags & USP_INSTDESC_FLAG_FORCES_DESCHEDULE)
					)
				{
					uNSPairInvalid ^= 0x1;
				}
			}

			/*
				If the instruciton 3 back forces a deschedule, then we cannot use
				either instruction
			*/
			if	(apsNSPairInst[1]->sDesc.uFlags & USP_INSTDESC_FLAG_FORCES_DESCHEDULE)
			{
				uNSPairInvalid = 0x3;
			}
		}

		/*
			Set the NS flag on the first in the pair if valid
		*/
		if	(!(uNSPairInvalid & 0x1))
		{
			if	(!HWInstSetNoSched(apsNSPairInst[0]->sDesc.eOpcode,
								   &apsNSPairInst[0]->sHWInst,
								   IMG_TRUE))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}
			continue;
		}

		/*
			Set the NS flag on the second in the pair if valid
		*/
		if	(!(uNSPairInvalid & 0x2))
		{
			if	(!HWInstSetNoSched(apsNSPairInst[1]->sDesc.eOpcode,
								   &apsNSPairInst[1]->sHWInst,
								   IMG_TRUE))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}
			continue;
		}

		/*
			Work out where we should insert the 2 NOPs and set the NS flag...
		*/
		if	(!apsNSPairInst[1])
		{
			if	(!psPrevEvenInst)
			{
				/*
					o)A; e)psInst => o)NOP+NS; e)NOP; o)A; e)psEvenInst
				*/
				psInsertPos = psEvenInst->psPrev;
				uNSTargetInstMask = USP_TARGETINST_FIRST;
			}
			else
			{
				/*
					e)A o)B; e)psInst => e)NOP; o)NOP+NS; e)A; o)B; e)psEvenInst
				*/
				psInsertPos = psPrevEvenInst;
				uNSTargetInstMask = USP_TARGETINST_SECOND;
			}
		}
		else
		{
			/*
				Insert NOPs between the two unsuitable instructions. After these
				are added, the second NOP (i.e. closest to the current
				instruction) will be 4 instrucitons before it, and therefore
				suitable for the NS flag. The other NOP may be useful in
				holding the NS flag for the the next instruction to be
				processed.

				e)A; o)B; e)C; o)D; e)psInst => e)A; o)NOP; e)NOP+NS; o)B; e)C; o)D; e)psInst
			*/
			psInsertPos = apsNSPairInst[1];
			uNSTargetInstMask = USP_TARGETINST_SECOND;

			/*
				Advance the insertion point where necessary...

				1) Instructions with the SyncStart flag set should not be separated
				from their following instruction. Therefore, advance the insert
				point in that case. After doing so, the two inserted NOPs will
				be 4 and 3 instructions before the current instruction, and
				therefore both suitable candidates for the NS flag.

				So instead of:

				e)A+SS; o)B; e)C; o)D; e)psInst => e)A+SS; o)NOP; e)NOP+NS; o)B; e)C; o)D; e)psInst

				we do:

				e)A+SS; o)B; e)C; o)D; e)psInst => e)A+SS; o)B; e)NOP+NS; o)NOP; e)C; o)D; e)psInst

				2) If the second instruction of the pair (the one that will
				immediately follow the NOPs) forces a deschedule, this would
				cancel the effect of any NoSched we add before it. So advance
				the insert point so the NOPs are put after the descheduling
				instruction.

				So instead of

				e)A; o)WDF; e)C; o)D; e);psInst => e)A; o)NOP; e)NOP+NS; o)WDF; e)C; o)D; e)psInst

				we do:

				e)A; o)WDF; e)C; o)D; e);psInst => e)A; o)WDF; e)NOP+NS; o)NOP; e)C; o)D; e)psInst								  lost
			*/
			if	(
			/* 1 */	(
						(apsNSPairInst[0] != IMG_NULL) &&
						(apsNSPairInst[0]->sDesc.uFlags & USP_INSTDESC_FLAG_SYNCSTART_SET)
					) ||
			/* 2 */	(apsNSPairInst[1]->sDesc.uFlags & USP_INSTDESC_FLAG_FORCES_DESCHEDULE)
				)
			{
				psInsertPos = psPrevEvenInst;
				uNSTargetInstMask = USP_TARGETINST_FIRST;

				/*
					Now that we have moved the insert point again, ensure
					that we aren't separating a SyncStart instruction from
					the following one.

					So instead of:

					e)A; o)B+SS; e)C; o)D; e)psInst => e)A; o)B+SS; e)NOP+NS; o)NOP; e)C; o)D; e)psInst

					we do:

					e)A; o)B+SS; e)C; o)D; e)psInst => e)A; o)B+SS; e)C; o)NOP+NS; e)NOP; o)D; e)psInst

					Note that the NS flag must be set on the first of the two NOPs,
					as that is 3 instructions back from the current one. 
				*/
				if	(apsNSPairInst[1]->sDesc.uFlags & USP_INSTDESC_FLAG_SYNCSTART_SET)
				{
					psInsertPos = psInsertPos->psNext;

					/*
						If we have

						e)C; o)D; e)psInst o)psInst+1; e)psInst+2

						with the internal registers live between psInst+1 and psInst+2 then change to

						e)C; o)NOP+NS; e)NOP+NS; o)D; e)psInst; o)psInst+1; e)psInst+2

						NOTE: We are assured then the internal registers aren't live between C and D because C
						needs a syncstart so the internal registers are lost before C and C can't write an
						internal register.
					*/
					if (psEvenInst->psNext != NULL &&
						psEvenInst->psNext->psNext != NULL &&
						(psEvenInst->psNext->psNext->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE))
					{
						uNSTargetInstMask |= USP_TARGETINST_SECOND;
					}
				}
			}
			else
			{
				/*
					If instructions C+D are (partially) in the previous block then set the NOSCHED block
					flag for them here.
				*/
				if (iIdxInBlock < 2 && (psPrevEvenInst->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE))
				{
					uNSTargetInstMask |= USP_TARGETINST_FIRST;
				}
			}
		}

		/*
			Check whether the IRegs are live through the 2 NOPs
		*/
		if	(psInsertPos->sDesc.uFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE)
		{
			uNOPInstDescFlags = USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
		}
		else
		{
			uNOPInstDescFlags = 0;
		}

		/*
			Insert 2 NOPs after the target pair
		*/
		psInsertPosBlock = psInsertPos->psInstBlock;

		HWInstEncodeNOPInst(&sNOP);

		if	(!USPInstBlockInsertHWInst(psInsertPosBlock,
									   psInsertPos,
									   USP_OPCODE_NOP,
									   &sNOP,
									   uNOPInstDescFlags,
									   psContext,
									   &apsNSPairInst[0]))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		if	(!USPInstBlockInsertHWInst(psInsertPosBlock,
									   psInsertPos,
									   USP_OPCODE_NOP,
									   &sNOP,
									   uNOPInstDescFlags,
									   psContext,
									   &apsNSPairInst[1]))
		{
			USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to add NOP to block\n"));
			goto USPInstBlockSetupNoSchedFlagExit;
		}

		/*
			Were the NOPs inserted into the current block?
		*/
		if	(psInsertPosBlock == psInstBlock)
		{
			/*
				Update the index of the current instruction within this
				block, to account for the 2 NOPs we have inserted.
			*/
			iIdxInBlock += 2;
		}
		else
		{
			/*
				Update the start indexes (within the whole program) of all
				following blocks processed so far.
			*/
			do
			{
				psInsertPosBlock = psInsertPosBlock->psNext;
				psInsertPosBlock->uFirstInstIdx += 2;
			} while (psInsertPosBlock != psInstBlock);

			/*
				Partially process the previous block in case inserting NOPs
				has split up a pair of instructions with the internal registers
				live between them.
			*/
			iBlockStart -= 2;
		}

		/*
			Set the NS flag on the chosen NOPs
		*/
		if (uNSTargetInstMask & USP_TARGETINST_FIRST)
		{
			if	(!HWInstSetNoSched(USP_OPCODE_NOP,
								   &apsNSPairInst[0]->sHWInst,
								   IMG_TRUE))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}
		}
		if (uNSTargetInstMask & USP_TARGETINST_SECOND)
		{
			if	(!HWInstSetNoSched(USP_OPCODE_NOP,
								   &apsNSPairInst[1]->sHWInst,
								   IMG_TRUE))
			{
				USP_DBGPRINT(("USPInstBlockSetupNoSchedFlag: Failed to set NoSched on inst\n"));
				goto USPInstBlockSetupNoSchedFlagExit;
			}
		}

		/*
			The previous even instruciton may be a NOP now, so recalculate
		*/
		psPrevEvenInst = psEvenInst->psPrev->psPrev;
	}

	/*
		NoSched flag setup OK
	*/
	bFailed = IMG_FALSE;

	USPInstBlockSetupNoSchedFlagExit:

	return (IMG_BOOL)!bFailed;
}
#endif /* defined(SGX_FEATURE_USE_NO_INSTRUCTION_PAIRING) */

/*****************************************************************************
 Name:		USPInstBlockSetupSyncStartFlag

 Purpose:	Setup the SyncStart flag for the instrucitons within a block

 Inputs:	psBlock		- The instruction block to modify
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPInstBlockSetupSyncStartFlag(PUSP_INSTBLOCK	psBlock,
											   PUSP_CONTEXT		psContext)
{
	PUSP_SHADER		psShader;
	PUSP_PROGDESC	psProgDesc;
	PUSP_INST		psInst;
	PUSP_INST		psPrevInst;

	/*
		SyncStart is not needed for vertex-shaders, or geometry-shaders, or for pixel-shaders that
		are not executed in per-instance mode.
	*/
	psShader	= psBlock->psShader;
	psProgDesc	= psShader->psProgDesc;

	if	((psProgDesc->uShaderType == USP_PC_SHADERTYPE_VERTEX) || (psProgDesc->uShaderType == USP_PC_SHADERTYPE_GEOMETRY))
	{
		return IMG_TRUE;
	}
	if	(!(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE))
	{
		return IMG_TRUE;
	}

	/*
		Determine whether we can use the last instruction of the previous block
	*/
	psPrevInst = IMG_NULL;

	if	(!psBlock->bIsBranchDest && psBlock->psPrev)
	{
		psPrevInst = psBlock->psPrev->psLastInst;
	}

	/*
		Setup the SyncStart flag for all instructions within the block
	*/
	psInst = psBlock->psFirstInst;

	for	(psInst = psBlock->psFirstInst; psInst; psInst = psInst->psNext)
	{
		/*
			Ignore instructions that don't need SyncStart set beforehand
		*/
		if	(HWInstNeedsSyncStartBefore(psInst->sDesc.eOpcode, &psInst->sHWInst))
		{
			IMG_BOOL	bInsertNOP;

			/*
				Determine whether we must insert an instruction to take
				the SyncStart flag
			*/
			bInsertNOP = IMG_TRUE;
			if	(psPrevInst)
			{
				if	(HWInstSupportsSyncStart(psPrevInst->sDesc.eOpcode))
				{
					bInsertNOP = IMG_FALSE;
				}

				if	(
						(psBlock->bIsBranchDest) && 
						(psPrevInst->psInstBlock != psBlock)
					)
				{
					bInsertNOP = IMG_TRUE;
				}
			}

			/*
				Insert a NOP to take the SyncStart flag if needed
			*/
			if	(bInsertNOP)
			{
				HW_INST		sNOP;
				IMG_UINT32	uNOPInstDescFlags;

				HWInstEncodeNOPInst(&sNOP);

				uNOPInstDescFlags = psInst->sDesc.uFlags & 
									USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

				if	(!USPInstBlockInsertHWInst(psBlock,
											   psInst,
											   USP_OPCODE_NOP,
											   &sNOP,
											   uNOPInstDescFlags,
											   psContext,
											   &psPrevInst))
				{
					USP_DBGPRINT(("USPInstBlockSetupSyncStartFlag: Failed to insert NOP for SyncStart\n"));
					return IMG_FALSE;
				}
			}

			/*
				Set the SyncStart flag on the instruction preceeding the
				one needs it
			*/
			if	(!HWInstSetSyncStart(psPrevInst->sDesc.eOpcode,
									 &psPrevInst->sHWInst,
									 IMG_TRUE))
			{
				USP_DBGPRINT(("USPInstBlockSetupSyncStartFlag: Failed to set SyncStart on inst\n"));
				return IMG_FALSE;
			}

			/*
				Flag (since it matters to NOSCHED setup) that this instruction forces a deschedule.
			*/
			psPrevInst->sDesc.uFlags |= USP_INSTDESC_FLAG_FORCES_DESCHEDULE;

			/*
				Flag the SYNCSTART flag is set here.
			*/
			psPrevInst->sDesc.uFlags |= USP_INSTDESC_FLAG_SYNCSTART_SET;
		}

		psPrevInst = psInst;
	}

	return IMG_TRUE;
}

#if defined(FIX_HW_BRN_21752)
/*****************************************************************************
 Name:		USPInstBlockApplyFixForBRN21752

 Purpose:	Apply a fix for BRN21752 to an instruction

 Inputs:	psBlock			- Instruction block containing the instruction
			psInst			- The instruction to fix if appropriate
			psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL USPInstBlockApplyFixForBRN21752(PUSP_INSTBLOCK	psBlock,
												PUSP_INST		psInst,
												PUSP_CONTEXT	psContext)
{
	/*
		Check whether this instruction needs a workaround for BRN21752.
		Condition:

		(Opcode	= PCKUNPCK) AND
		((SrcFmt = C10) XOR (DstFmt = C10)) AND
		(DestBank != INTERNAL) &&
		(WriteMask != x111)
	*/
	if	(psInst->sDesc.eOpcode == USP_OPCODE_PCKUNPCK)
	{
		PUSP_MOESTATE		psMOEState;
		USP_FMTCTL			eFmtCtl;
		PHW_INST			psHWInst;
		IMG_UINT32			uWriteMask;
		USP_PCKUNPCK_FMT	eSrcFmt;
		USP_PCKUNPCK_FMT	eDstFmt;
		USP_REG				sDestReg;

		psHWInst	= &psInst->sHWInst;
		psMOEState	= &psInst->sMOEState;

		uWriteMask = HWInstDecodePCKUNPCKInstWriteMask(psHWInst);
		if	((uWriteMask & 0x7) != 0x7)
		{
			eSrcFmt	= HWInstDecodePCKUNPCKInstSrcFormat(psHWInst);
			eDstFmt	= HWInstDecodePCKUNPCKInstDstFormat(psHWInst);

			if	(
					(	
						(eDstFmt == USP_PCKUNPCK_FMT_C10) ||
						(eSrcFmt == USP_PCKUNPCK_FMT_C10) 
					) &&
					(eDstFmt != eSrcFmt)
				)
			{
				if	(!HWInstGetPerOperandFmtCtl(psMOEState,
												USP_OPCODE_PCKUNPCK,
												psHWInst,
												&eFmtCtl))
				{
					USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error getting inst per-operand fmt-ctl\n"));
					return IMG_FALSE;
				}

				if	(!HWInstDecodeDestBankAndNum(eFmtCtl, 
												 USP_OPCODE_PCKUNPCK, 
												 psHWInst, 
												 &sDestReg))
				{
					USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error decoding PCKUNPCK inst dest\n"));
					return IMG_FALSE;
				}

				if	(sDestReg.eType != USP_REGTYPE_INTERNAL)
				{
					IMG_UINT32		uOrgInstDescFlags;
					IMG_UINT32		uNewInstDescFlags;
					PUSP_RESULTREF	psResultRef;
					USP_REG			sSrc1Reg;
					USP_REG			sSrc2Reg;
					USP_REG			sTempDestReg;
					HW_INST			sNewHWInst;

					/*
						Change the existing instruction to do a full rather than partial
						write to a temporary register in order to convert the data.
					*/
					sTempDestReg.eType		= USP_REGTYPE_TEMP;
					sTempDestReg.uNum		= psBlock->psShader->uFinalTempRegCount;
					sTempDestReg.eFmt		= USP_REGFMT_UNKNOWN;
					sTempDestReg.uComp		= 0;
					sTempDestReg.eDynIdx	= USP_DYNIDX_NONE;

					if	(!HWInstEncodePCKUNPCKInstWriteMask(psHWInst, 0xF, IMG_TRUE))
					{
						USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error encoding PCKUNPCK inst write-mask\n"));
						return IMG_FALSE;
					}

					if	(!HWInstEncodeDestBankAndNum(eFmtCtl, 
													 USP_OPCODE_PCKUNPCK,
													 psHWInst,
													 &sTempDestReg))
					{
						USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error encoding PCKUNPCK inst dest\n"));
						return IMG_FALSE;
					}

					/*
						The original PCKUNPCK instruction can no longer modify a result
						register. Update any associated result-reference to account for
						the change
					*/
					uOrgInstDescFlags = psInst->sDesc.uFlags;

					psInst->sDesc.uFlags &= ~USP_INSTDESC_FLAG_DEST_USES_RESULT;

					psResultRef = psInst->psResultRef;
					if	(psResultRef)
					{
						if	(!USPResultRefSetInst(psResultRef, psInst, psContext))
						{
							USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error setting result-ref inst\n"));
							return IMG_FALSE;
						}

						if	(!psResultRef->bActive)
						{
							psInst->psResultRef = IMG_NULL;
						}
					}

					/*
						Encode a second pack instruction to perform the
						partial writeback of the original destination
					*/
					sSrc1Reg = sTempDestReg;
					sSrc2Reg = sTempDestReg;

					switch (eDstFmt)
					{
						case USP_PCKUNPCK_FMT_U8:
						case USP_PCKUNPCK_FMT_S8:
						case USP_PCKUNPCK_FMT_O8:
						case USP_PCKUNPCK_FMT_C10:
						{
							sSrc1Reg.uComp	= 0;
							sSrc2Reg.uComp	= 1;
							break;
						}

						case USP_PCKUNPCK_FMT_U16:
						case USP_PCKUNPCK_FMT_S16:
						case USP_PCKUNPCK_FMT_F16:
						{
							sSrc1Reg.uComp	= 0;
							sSrc2Reg.uComp	= 2;
							break;
						}

						case USP_PCKUNPCK_FMT_F32:
						default:
						{
							sSrc1Reg.uComp	= 0;
							sSrc2Reg.uComp	= 0;
							break;
						}
					}

					if	(!HWInstEncodePCKUNPCKInst(&sNewHWInst,
												   IMG_FALSE,
												   eDstFmt,
												   eDstFmt,
												   IMG_FALSE,
												   uWriteMask,
												   &sDestReg,
												   0,
												   1,
												   &sSrc1Reg,
												   &sSrc2Reg))
					{
						USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}

					/*
						Insert the new instruction after the original PCKUNPCK
					*/
					uNewInstDescFlags = uOrgInstDescFlags;
					uNewInstDescFlags &= ~(USP_INSTDESC_FLAG_PRECOMPILED_INST |
										   USP_INSTDESC_FLAG_SRC0_USES_RESULT |
										   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
										   USP_INSTDESC_FLAG_SRC2_USES_RESULT);

					if	(!USPInstBlockInsertHWInst(psBlock,
												   psInst->psNext,
												   USP_OPCODE_PCKUNPCK,
												   &sNewHWInst,
												   uNewInstDescFlags,
												   psContext,
												   IMG_NULL))
					{
						USP_DBGPRINT(("USPInstBlockApplyFixForBRN21752: Error inserting HW inst into block\n"));
						return IMG_FALSE;
					}
				}
			}
		}
	}

	return IMG_TRUE;
}
#endif /* #if defined(FIX_HW_BRN_21752) */

/*****************************************************************************
 Name:		USPInstBlockApplyBRNFixes

 Purpose:	Finalise the HW instructions to be generated for a block

 Inputs:	psBlock			- The instruction block to modify
			psContext		- The current USP execution context
			bIncludePCInsts	- Whether BRN fixes should be applied to
							  pre-complied instructions within the block. If
							  not, then only USP-generated instructions are
							  targeted.

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static 
IMG_BOOL USPInstBlockApplyBRNFixes(PUSP_INSTBLOCK	psBlock,
								   PUSP_CONTEXT		psContext,
								   IMG_BOOL			bIncludePCInsts)
{
	PUSP_INST psInst;

	PVR_UNREFERENCED_PARAMETER(psContext);

	for	(psInst = psBlock->psFirstInst; psInst; psInst = psInst->psNext)
	{
		PUSP_INSTDESC	psInstDesc;

		/*
			Ignore pre-compiled instructions if required
		*/
		psInstDesc = &psInst->sDesc;
		if	(
				!bIncludePCInsts &&
				(psInstDesc->uFlags & USP_INSTDESC_FLAG_PRECOMPILED_INST)
			)
		{
			continue;
		}

		#if defined(FIX_HW_BRN_21752)
		/*
			Apply a fix for BRN21752
		*/
		if	(!USPInstBlockApplyFixForBRN21752(psBlock, psInst, psContext))
		{
			USP_DBGPRINT(("USPInstBlockApplyBRNFixes: Error applying BRN21752 to inst\n"));
			return IMG_FALSE;
		}
		#endif
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockFinaliseInsts

 Purpose:	Finalise the HW instructions to be generated for a block

 Inputs:	psBlock		- The instruction block to modify
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL 
IMG_BOOL USPInstBlockFinaliseInsts(PUSP_INSTBLOCK	psBlock,
								   PUSP_CONTEXT		psContext)
{
	/*
		Perform code modifications to apply workarounds for BRNs

		NB:	We only do this for instructions generated by the USP here. BRNs
			for pre-compiled instructions are handled when the USP shader is
			generated, as they only need to be processed once.
	*/
	if	(!USPInstBlockApplyBRNFixes(psBlock, psContext, IMG_FALSE))
	{
		USP_DBGPRINT(("USPInstBlockFinaliseInsts: Failed to apply BRN fixes for block\n"));
		return IMG_FALSE;
	}

	/*
		Setup the SyncStart flag for all instructions in the block
	*/
	if	(!USPInstBlockSetupSyncStartFlag(psBlock, psContext))
	{
		USP_DBGPRINT(("USPInstBlockFinaliseInsts: Failed to setup SyncStart flag for block\n"));
		return IMG_FALSE;
	}

	/*
		Setup the NoSched flag for all instructions in the block

		NB: This must be done as the final step, as the addition of any
			further extra instructions after NoSched setup could invalidate
			the NoSched state.
	*/
	if	(!USPInstBlockSetupNoSchedFlag(psBlock, psContext))
	{
		USP_DBGPRINT(("USPInstBlockFinaliseInsts: Failed to setup NoSched flag for block\n"));
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInstBlockCreate

 Purpose:	Create and n USP instruction block

 Inputs:	psContext		- The current USP context
			psShader		- The shader that this block is part-of
			uMaxInstCount	- The maximum number of instructions that can
							  be contained in the block
			uOrgInstCount	- The number of pre-compiled instructions for
							  the new block
			bAddResultRefs	- Whether result-references should be created
							  for the non-precompiled instructions in the
							  block.
			psMOEState		- The initial MOE state at the start of the block

 Outputs:	none

 Returns:	The created USP instruction block
*****************************************************************************/
IMG_INTERNAL PUSP_INSTBLOCK USPInstBlockCreate(PUSP_CONTEXT		psContext,
											   PUSP_SHADER		psShader,
											   IMG_UINT32		uMaxInstCount,
											   IMG_UINT32		uOrgInstCount,
											   IMG_BOOL			bAddResultRefs,
											   PUSP_MOESTATE	psMOEState)
{
	PUSP_INST		psInsts;
	PHW_INST		psOrgInsts;
	PUSP_INSTDESC	psOrgInstDescs;
	PUSP_RESULTREF	psResultRefs;
	IMG_UINT32		uMaxNonPCInstCount;
	IMG_UINT32		uAllocSize;
	PUSP_INSTBLOCK	psInstBlock;
	IMG_BOOL		bFailed;

	bFailed			= IMG_TRUE;
	psInstBlock		= IMG_NULL;
	psInsts			= IMG_NULL;
	psResultRefs	= IMG_NULL;
	psOrgInsts		= IMG_NULL;
	psOrgInstDescs	= IMG_NULL;

	/*
		Add additional instruction space to allow for:

		- Instructions to workaround BRN21752 (must assume every instruction needs it)
		- NOPs added for NoSched setup
		- NOPs added to fix illegal-instruction pairs between blocks
	*/
	uMaxInstCount *= 2;
	uMaxInstCount += ((uMaxInstCount + 3) / 4) * 2;
	uMaxInstCount += 1;

	/*
		Allocate space for the pool of maximum USP instructions
	*/
	uAllocSize = uMaxInstCount * sizeof(USP_INST);
	psInsts = (PUSP_INST)psContext->pfnAlloc(uAllocSize);
	if	(!psInsts)
	{
		USP_DBGPRINT(("USPInstBlockCreate: Failed to alloc pool of %d insts\n", uMaxInstCount));
		goto USPInstBlockCreateExit;
	}

	/*
		Create a pool of result and input references for USP generated instructions
		if required
	*/
	uMaxNonPCInstCount = 0;
	if	(bAddResultRefs)
	{
		uMaxNonPCInstCount = uMaxInstCount - uOrgInstCount;
		if	(uMaxNonPCInstCount > 0)
		{
			IMG_UINT32	i;

			/*
				Allocate enough result-refs for the maximum number of non-PC
				instruciton we may add to this block
			*/
			uAllocSize = uMaxNonPCInstCount * sizeof(USP_RESULTREF);
			psResultRefs = (PUSP_RESULTREF)psContext->pfnAlloc(uAllocSize);
			if	(!psResultRefs)
			{
				USP_DBGPRINT(("USPInstBlockCreate: Failed to alloc pool of %d result-refs\n", uMaxNonPCInstCount));
				goto USPInstBlockCreateExit;
			}

			/*
				Initialise the pool of result-references
			*/
			for	(i = 0; i < uMaxNonPCInstCount; i++)
			{
				USPResultRefSetup(psResultRefs + i, psShader);
			}
		}
	}

	/*
		Allocate space for the list of pre-compiled instructions and their
		descriptors
	*/
	if	(uOrgInstCount > 0)
	{
		uAllocSize = uOrgInstCount * EURASIA_USE_INSTRUCTION_SIZE;
		psOrgInsts = (PHW_INST)psContext->pfnAlloc(uAllocSize);
		if	(!psOrgInsts)
		{
			USP_DBGPRINT(( "USPInstBlockCreate: Failed to alloc block of %d HW insts\n", uOrgInstCount));
			goto USPInstBlockCreateExit;
		}

		uAllocSize = uOrgInstCount * sizeof(USP_INSTDESC);
		psOrgInstDescs = (PUSP_INSTDESC)psContext->pfnAlloc(uAllocSize);
		if	(!psOrgInstDescs)
		{
			USP_DBGPRINT(( "USPInstBlockCreate: Failed to alloc block of %d inst descs\n", uOrgInstCount));
			goto USPInstBlockCreateExit;
		}
	}

	/*
		Allocate an intialise the USP instruction block
	*/
	psInstBlock = (PUSP_INSTBLOCK)psContext->pfnAlloc(sizeof(USP_INSTBLOCK));
	if	(!psInstBlock)
	{
		USP_DBGPRINT(( "USPInstBlockCreate: Failed to alloc USP inst block\n"));
		psContext->pfnFree(psInsts);
		return IMG_NULL;
	}

	memset(psInstBlock, 0, sizeof(USP_INSTBLOCK));

	psInstBlock->psShader			= psShader;
	psInstBlock->psInsts			= psInsts;
	psInstBlock->uMaxInstCount		= uMaxInstCount;
	psInstBlock->uOrgInstCount		= uOrgInstCount;
	psInstBlock->uMaxNonPCInstCount	= uMaxNonPCInstCount;
	psInstBlock->psResultRefs		= psResultRefs;
	psInstBlock->psOrgInsts			= psOrgInsts;
	psInstBlock->psOrgInstDescs		= psOrgInstDescs;
	if	(psMOEState)
	{
		psInstBlock->sInitialMOEState	= *psMOEState;
		psInstBlock->sFinalMOEState		= *psMOEState;
	}

	/*
		Instruction-block created OK
	*/
	bFailed = IMG_FALSE;

	USPInstBlockCreateExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psOrgInstDescs)
		{
			psContext->pfnFree(psOrgInstDescs);
		}
		if	(psOrgInsts)
		{
			psContext->pfnFree(psOrgInsts);
		}
		if	(psResultRefs)
		{
			psContext->pfnFree(psResultRefs);
		}
		if	(psInsts)
		{
			psContext->pfnFree(psInsts);
		}
		if	(psInstBlock)
		{
			psContext->pfnFree(psInstBlock);
			psInstBlock = IMG_NULL;
		}
	}

	return psInstBlock;
}

/*****************************************************************************
 Name:		USPInstBlockDestroy

 Purpose:	Destroy a USP instruction block previously created using
			USPInstBlockCreate

			NB: This does not unlink it from a list

 Inputs:	psInstBlock	- The USP instruction block to destroy
			psContext	- The current USP context

 Outputs:	none

 Returns:	nothing
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPInstBlockDestroy(PUSP_INSTBLOCK	psInstBlock,
										  PUSP_CONTEXT		psContext)
{
	if	(psInstBlock->uOrgInstCount > 0)
	{
		psContext->pfnFree(psInstBlock->psOrgInstDescs);
		psContext->pfnFree(psInstBlock->psOrgInsts);
	}
	if	(psInstBlock->uMaxNonPCInstCount > 0)
	{
		psContext->pfnFree(psInstBlock->psResultRefs);
	}
	if	(psInstBlock->uMaxInstCount > 0)
	{
		psContext->pfnFree(psInstBlock->psInsts);
	}
	psContext->pfnFree(psInstBlock);
}

/******************************************************************************
 End of file (USP_INSTBLOCK.C)
******************************************************************************/
