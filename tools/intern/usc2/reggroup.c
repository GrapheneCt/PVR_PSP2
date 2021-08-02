/**********************************************************************
 * Name         : reggroup.c
 * Title        : Allocation of all other (non-predicate, non-internal) registers
 * Created      : April 2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: reggroup.c $
 * ./
 *  --- Revision Logs Removed --- 
 *  
 * Increase the cases where a vec4 instruction can be split into two vec2
 * parts.
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"

/*
	Element of the tree REGISTER_GROUP_STATE.psRegisterGroups.
*/
typedef struct _GROUP_PAIR
{
	/* Virtual register number. */
	IMG_UINT32		uRegister;
	/* Information about the register group. */
	PREGISTER_GROUP	psData;
} GROUP_PAIR, *PGROUP_PAIR;

typedef struct _EQUIV_SRC_DATA
{
	PUSC_TREE		psEquivSrc;
} EQUIV_SRC_DATA;

typedef struct _EQUIVALENT_SOURCE
{
	/*
		Register which has been copied.
	*/
	IMG_UINT32	uSourceType;
	IMG_UINT32	uSourceNum;
	/* List of copies of the source data. */
	USC_LIST	sCRegMoveList;
} EQUIVALENT_SOURCE, *PEQUIVALENT_SOURCE;

typedef struct _CREG_MOVE
{
	/* Points to the copied data. */
	PEQUIVALENT_SOURCE	psSource;
	/* Points to the last use in the current block of the MOV destination. */
	PARG				psLastUse;
	/* Index of the instruction where the destination is last used. */
	IMG_UINT32			uLastUseId;
	/* Points to the destination of the MOV. */
	PREGISTER_GROUP		psDest;
	/* Entry in the list of copies of the source data. */
	USC_LIST_ENTRY		sListEntry;
	/* MOV instruction. */
	PINST				psMovInst;
	/* Index of the MOV destination copying the data. */
	IMG_UINT32			uMovDestIdx;
} CREG_MOVE, *PCREG_MOVE;

/*
	Maps an alignment restriction on a hardware register number 
	to the restriction on the immediately previous or following
	register number.
*/
IMG_INTERNAL
const HWREG_ALIGNMENT g_aeOtherAlignment[] =
{
	HWREG_ALIGNMENT_NONE,	/* HWREG_ALIGNMENT_NONE */
	HWREG_ALIGNMENT_ODD,	/* HWREG_ALIGNMENT_EVEN */
	HWREG_ALIGNMENT_EVEN,	/* HWREG_ALIGNMENT_ODD */
	HWREG_ALIGNMENT_UNDEF,	/* HWREG_ALIGNMENT_RESERVED */
};

/*
	Maps a hardware register number alignment enum back to
	the required value of the register number modulus 2.
*/
IMG_INTERNAL
const IMG_UINT32 g_auRequiredAlignment[] = 
{
	USC_UNDEF,	/* HWREG_ALIGNMENT_NONE */
	0,			/* HWREG_ALIGNMENT_EVEN */
	1,			/* HWREG_ALIGNMENT_ODD */
	USC_UNDEF,	/* HWREG_ALIGNMENT_RESERVED */
};

static
IMG_INT32 CmpGroupPair(IMG_PVOID pvE1, IMG_PVOID pvE2)
/*****************************************************************************
 FUNCTION	: CmpGroupPair

 PURPOSE	: Comparison functions for the tree recording details of registers
			  which are part of a group which require consecutive hardware
			  register numbers.

 PARAMETERS	: pvE1, pvE2			- Elements to compare.

 RETURNS	: E1 < E2
*****************************************************************************/
{
	PGROUP_PAIR	psPair1 = (PGROUP_PAIR)pvE1;
	PGROUP_PAIR psPair2 = (PGROUP_PAIR)pvE2;

	return (IMG_INT32)(psPair1->uRegister - psPair2->uRegister);
}

static
IMG_VOID DeleteGroupPair(IMG_PVOID pvState, IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: DeleteGroupPair

 PURPOSE	: Delete the satellite data for an element from the tree recording details of 
			  registers which are part of a group which require consecutive hardware
			  register numbers.

 PARAMETERS	: pvState			- Compiler state.
			  pvElem			- Element to delete.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvState;
	PGROUP_PAIR	psElem = (PGROUP_PAIR)pvElem;

	UscFree(psState, psElem->psData);
}

static
IMG_VOID DeleteRegisterGroup(IMG_PVOID pvState, IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: DeleteRegisterGroup

 PURPOSE	: Delete information about register grouping for a register.

 PARAMETERS	: pvState			- Compiler state.
			  pvElem			- Element to delete.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = (PINTERMEDIATE_STATE)pvState;
	PGROUP_PAIR			psElem = (PGROUP_PAIR)pvElem;
	PREGISTER_GROUP		psGroup = psElem->psData;

	if (psGroup->psPrev == NULL)
	{
		RemoveFromList(&psState->psGroupState->sGroupHeadsList, &psGroup->sGroupHeadsListEntry);
	}

	if (psGroup->psPrev != NULL)
	{
		ASSERT(psGroup->psPrev->psNext == psGroup);
		psGroup->psPrev->psNext = psGroup->psNext;
	}
	if (psGroup->psNext != NULL)
	{
		ASSERT(psGroup->psNext->psPrev == psGroup);
		psGroup->psNext->psPrev = psGroup->psPrev;
	}

	UscFree(psState, psGroup);
}

IMG_INTERNAL
IMG_VOID ReleaseRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister)
/*****************************************************************************
 FUNCTION	: ReleaseRegisterGroup

 PURPOSE	: Delete information about register grouping for a register.

 PARAMETERS	: psState			- Compiler state.
			  uRegister			- Register to delete for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	GROUP_PAIR	sKey;

	sKey.uRegister = uRegister;
	sKey.psData = NULL;
	UscTreeRemove(psState, psState->psGroupState->psRegisterGroups, &sKey, DeleteRegisterGroup, psState);
}

IMG_INTERNAL
PREGISTER_GROUP FindRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister)
/*****************************************************************************
 FUNCTION	: FindRegisterGroup

 PURPOSE	: Gets information about whether there are other virtual registers which
			  require a hardware register number either one less or one more
			  than a specified virtual register.

 PARAMETERS	: psState			- Compiler state.
			  uRegister			- Register to get group information for.

 RETURNS	: A pointer to the group information or NULL if the register isn't
			  a part of any groups.
*****************************************************************************/
{
	GROUP_PAIR	sKey;
	IMG_PVOID	pvNode;

	/*
		Check if we haven't started setting up information about the
		restrictions on the order/alignment of hardware register numbers.
	*/
	if (psState->psGroupState == NULL)
	{
		return NULL;
	}

	sKey.uRegister = uRegister;
	sKey.psData = NULL;
	pvNode = UscTreeGetPtr(psState->psGroupState->psRegisterGroups, &sKey);
	if (pvNode != NULL)
	{
		return ((PGROUP_PAIR)pvNode)->psData;
	}
	else
	{
		return NULL;
	}
}

IMG_INTERNAL
PREGISTER_GROUP AddRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister)
/*****************************************************************************
 FUNCTION	: AddRegisterGroup

 PURPOSE	: Sets up a virtual register so it can be given a consecutive
			  hardware register number.

 PARAMETERS	: psState			- Compiler state.
			  uRegister			- Register to add group information for.

 RETURNS	: A pointer to the newly added group information.
*****************************************************************************/
{
	GROUP_PAIR		sNewElement;
	PREGISTER_GROUP	psNewGroup;
	PVREGISTER		psVRegister;

	if ((psNewGroup = FindRegisterGroup(psState, uRegister)) != NULL)
	{
		return psNewGroup;
	}

	sNewElement.uRegister = uRegister;
	sNewElement.psData = psNewGroup = UscAlloc(psState, sizeof(*sNewElement.psData));

	psNewGroup->psPrev = NULL;
	psNewGroup->psNext = NULL;
	psNewGroup->eAlign = HWREG_ALIGNMENT_NONE;
	psNewGroup->bAlignRequiredByInst = IMG_FALSE;
	psNewGroup->psFixedReg = NULL;
	psNewGroup->uFixedRegOffset = USC_UNDEF;
	psNewGroup->uRegister = uRegister;
	psNewGroup->bOptional = IMG_FALSE;
	psNewGroup->bLinkedByInst = IMG_FALSE;
	psNewGroup->u.pvNULL = NULL;

	AppendToList(&psState->psGroupState->sGroupHeadsList, &psNewGroup->sGroupHeadsListEntry);

	UscTreeAdd(psState, psState->psGroupState->psRegisterGroups, &sNewElement);

	psVRegister = GetVRegister(psState, USEASM_REGTYPE_TEMP, uRegister);
	ASSERT(psVRegister != NULL);
	ASSERT(psVRegister->psGroup == NULL);
	psVRegister->psGroup = psNewGroup;

	return psNewGroup;
}

IMG_INTERNAL
IMG_VOID SetPrecedingSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcToSet, PARG psNextSrc)
/*****************************************************************************
 FUNCTION	: SetPrecedingSource

 PURPOSE	: Set a source to an instruction which will be assigned a hardware
			  register number one less than the register number of a specified
			  source.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to modify.
			  uSrcToSet		- Index of the source argument to modify.
			  psNextSrc		- The source with the next highest register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psNextSrc->uType == USEASM_REGTYPE_TEMP)
	{
		PREGISTER_GROUP	psArgGroup;
		
		psArgGroup = FindRegisterGroup(psState, psNextSrc->uNumber);
		ASSERT(psArgGroup != NULL);
		ASSERT(psArgGroup->psPrev != NULL);
		SetSrc(psState, psInst, uSrcToSet, USEASM_REGTYPE_TEMP, psArgGroup->psPrev->uRegister, UF_REGFORMAT_UNTYPED); 
	}
	else if (psNextSrc->uType == USC_REGTYPE_REGARRAY)
	{
		SetArraySrc(psState, psInst, uSrcToSet, psNextSrc->uNumber, psNextSrc->uArrayOffset - 1, psNextSrc->eFmt);
	}
	else
	{
		ASSERT(psNextSrc->uNumber >= 0);
		SetSrc(psState, psInst, uSrcToSet, psNextSrc->uType, psNextSrc->uNumber - 1, psNextSrc->eFmt);
	}
}

IMG_INTERNAL
HWREG_ALIGNMENT GetSourceArgAlignment(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: GetSourceArgAlignment

 PURPOSE	: Get the required alignment of the hardware register assigned to
			  an intermediate instruction source.

 PARAMETERS	: psState		- Compiler state.
			  psArg			- Register to get the alignment of.

 RETURNS	: The alignment.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_TEMP)
	{
		PREGISTER_GROUP	psArgGroup;
		
		psArgGroup = FindRegisterGroup(psState, psArg->uNumber);
		if (psArgGroup != NULL)
		{
			return psArgGroup->eAlign;
		}
		else
		{
			return HWREG_ALIGNMENT_NONE;
		}
	}
	else if (psArg->uType == USC_REGTYPE_REGARRAY)
	{
		PUSC_VEC_ARRAY_REG	psArray;
		PREGISTER_GROUP		psArrayGroup;

		ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
		psArray = psState->apsVecArrayReg[psArg->uNumber];

		psArrayGroup = FindRegisterGroup(psState, psArray->uBaseReg);
		if (psArg->uIndexType == USC_REGTYPE_NOINDEX || psArray->eArrayType != ARRAY_TYPE_NORMAL)
		{
			HWREG_ALIGNMENT	eAlign;
			if (psArrayGroup == NULL || psArrayGroup->eAlign == HWREG_ALIGNMENT_NONE)
			{
				return HWREG_ALIGNMENT_NONE;
			}
			eAlign = psArrayGroup->eAlign;
			if ((psArg->uArrayOffset % 2) != 0)
			{
				eAlign = g_aeOtherAlignment[eAlign];
			}
			return eAlign;
		}
		else
		{
			if ((psArg->uArrayOffset % 2) == 0)
			{
				return HWREG_ALIGNMENT_EVEN;
			}
			else
			{
				return HWREG_ALIGNMENT_ODD;
			}
		}
	}
	else
	{
		if ((psArg->uNumber % 2) == 0)
		{
			return HWREG_ALIGNMENT_EVEN;
		}
		else
		{
			return HWREG_ALIGNMENT_ODD;
		}
	}
}

IMG_INTERNAL
IMG_VOID SetGroupHardwareRegisterNumber(PINTERMEDIATE_STATE	psState,
										PREGISTER_GROUP		psGroup,
										IMG_BOOL			bIgnoredAlignmentRequirement)
/*****************************************************************************
 FUNCTION	: SetGroupHardwareRegisterNumber

 PURPOSE	: Update the register grouping information after setting the
			  hardware register number.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Virtual register.
			  bIgnoredAlignmentRequirement
								- If TRUE then the hardware register number
								wasn't set to the required alignment for the register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uHardwareRegNum;
	HWREG_ALIGNMENT	eHWRegAlignment;

	for (; psGroup != NULL; psGroup = psGroup->psNext)
	{
		ASSERT(psGroup->psFixedReg->sPReg.uNumber != USC_UNDEF);
		uHardwareRegNum = psGroup->psFixedReg->sPReg.uNumber + psGroup->uFixedRegOffset;
		if ((uHardwareRegNum % 2) == 0)
		{
			eHWRegAlignment = HWREG_ALIGNMENT_EVEN;
		}
		else
		{
			eHWRegAlignment = HWREG_ALIGNMENT_ODD;
		}
		ASSERT(psGroup->eAlign == HWREG_ALIGNMENT_NONE || psGroup->eAlign == eHWRegAlignment || bIgnoredAlignmentRequirement);
		psGroup->eAlign = eHWRegAlignment;
	}
}

static
IMG_BOOL CanUseIntermediateSource(PINTERMEDIATE_STATE	psState,
								  PINST					psInst,
								  IMG_UINT32			uArgIdx,
								  PREGISTER_GROUP		psGroup)
/*****************************************************************************
 FUNCTION	: CanUseIntermediateSource

 PURPOSE	: Checks if the hardware register type of an intermediate register
			  is supported by an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uArgIdx			- Index of the instruction argument to check.
			  psGroup			- Intermediate register.

 RETURNS	: TRUE if the hardware register type is supported.
*****************************************************************************/
{
	IMG_UINT32	uHwRegType;

	if (psGroup != NULL)
	{
		uHwRegType = GetPrecolouredRegisterType(psState, psInst, USEASM_REGTYPE_TEMP, psGroup->uRegister);
	}
	else
	{
		uHwRegType = USEASM_REGTYPE_TEMP;
	}
	return CanUseSrc(psState, psInst, uArgIdx, uHwRegType, USC_REGTYPE_NOINDEX);
}

IMG_INTERNAL
HWREG_ALIGNMENT GetNodeAlignment(PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: GetNodeAlignment

 PURPOSE	: Gets the required alignment of the hardware register assigned
			  to a virtual register.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Virtual register.

 RETURNS	: The required alignment.
*****************************************************************************/
{
	if (psGroup != NULL)
	{
		return psGroup->eAlign;
	}
	else
	{
		return HWREG_ALIGNMENT_NONE;
	}
}

IMG_INTERNAL
IMG_BOOL AreAlignmentsCompatible(HWREG_ALIGNMENT	eAlignment1,
								 HWREG_ALIGNMENT	eAlignment2)
/*****************************************************************************
 FUNCTION	: AreAlignmentsCompatible

 PURPOSE	: Check if two nodes with potentially different alignments can
			  be safely merged.

 PARAMETERS	: eAlignment1		- Alignments to check.
			  eAlignment2

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (eAlignment1 != HWREG_ALIGNMENT_NONE &&
		eAlignment2 != HWREG_ALIGNMENT_NONE &&
		eAlignment1 != eAlignment2)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL CanCoalesceNodeWithQwordAlignedNode(PREGISTER_GROUP		psNode,
											 PREGISTER_GROUP		psOtherNode)
/*****************************************************************************
 FUNCTION	: CanCoalesceNodeWithQwordAlignedNode

 PURPOSE	: Check if a node can be coalesced with another node which requires
			  an even aligned hardware register number.

 PARAMETERS	: psNode			- First node to check.
			  psOtherNode		- Second node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	HWREG_ALIGNMENT	eNodeAlignment;
	HWREG_ALIGNMENT eOtherNodeAlignment;

	eNodeAlignment = GetNodeAlignment(psNode);
	eOtherNodeAlignment = GetNodeAlignment(psOtherNode);
	return AreAlignmentsCompatible(eNodeAlignment, eOtherNodeAlignment);
}

static
IMG_BOOL IsAfter(PREGISTER_GROUP		psNode,
				 PREGISTER_GROUP		psCheck)
/*****************************************************************************
 FUNCTION	: IsAfter

 PURPOSE	: Checks if one node needs to be given a higher register number
			  than another.

 PARAMETERS	: psNode		- Node with the lower register number.
			  psCheck		- Node with the higher register number.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	for (; psNode != NULL; psNode = psNode->psNext)
	{
		if (psNode == psCheck)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_UINT32 GetMaximumRangeLength(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/*****************************************************************************
 FUNCTION	: GetMaximumRangeLength

 PURPOSE	: Gets the maximum possible length of a group of registers with
			  consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psFunc			- Function within which the group will be used.

 RETURNS	: The maximum range length.
*****************************************************************************/
{
	if (
			psFunc == psState->psSecAttrProg && 
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS) != 0
	   )
	{
		return psState->psSAOffsets->uInRegisterConstantLimit;
	}
	else
	{
		return psState->psSAOffsets->uNumAvailableTemporaries;
	}
}

IMG_INTERNAL
IMG_UINT32 GetPrevNodeCount(PREGISTER_GROUP	psNode)
/*****************************************************************************
 FUNCTION	: GetPrevNodeCount

 PURPOSE	: Get the number of nodes in the initial segment of a register
			  group finishing before a given node

 PARAMETERS	: psNode		- Last node in the segment.

 RETURNS	: Count of nodes in the segment.
*****************************************************************************/
{
	IMG_UINT32	uCount;

	if (psNode == NULL)
	{
		return 0;
	}
	psNode = psNode->psPrev;

	uCount = 0;
	for (; psNode != NULL; psNode = psNode->psPrev)
	{
		uCount++;
	}
	return uCount;
}

IMG_INTERNAL
IMG_UINT32 GetNextNodeCount(PREGISTER_GROUP	psNode)
/*****************************************************************************
 FUNCTION	: GetNextNodeCount

 PURPOSE	: Get the number of nodes in the trailing segment of a register
			  group starting after a given node

 PARAMETERS	: psNode		- First node in the segment.

 RETURNS	: Count of nodes in the segment.
*****************************************************************************/
{
	IMG_UINT32	uCount;

	if (psNode == NULL)
	{
		return 0;
	}
	psNode = psNode->psNext;

	uCount = 0;
	for (; psNode != NULL; psNode = psNode->psNext)
	{
		uCount++;
	}
	return uCount;
}

static
IMG_BOOL ArePrecolouredNodeConsecutive(PREGISTER_GROUP		psPrevNode,
									   PREGISTER_GROUP		psNode)
/*****************************************************************************
 FUNCTION	: ArePrecolouredNodeConsecutive

 PURPOSE	: Check if two nodes are precoloured to consecutive hardware
			  registers.

 PARAMETERS	: psPrevNode			- Nodes to check.
			  psNode

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PARG		psPrevPReg;
	PARG		psPReg;
	IMG_BOOL	bPrevNodePrecoloured = IsPrecolouredNode(psPrevNode);
	IMG_BOOL	bNodePrecoloured = IsPrecolouredNode(psNode);


	/*
		Check for either node precoloured.
	*/
	if (!bPrevNodePrecoloured && !bNodePrecoloured)
	{
		return IMG_TRUE;
	}

	/*
		Don't mix precoloured and non-precoloured nodes in the same group.
	*/
	if (!bPrevNodePrecoloured || !bNodePrecoloured)
	{
		return IMG_FALSE;
	}

	psPrevPReg = &psPrevNode->psFixedReg->sPReg;
	psPReg = &psNode->psFixedReg->sPReg;

	/*
		Don't make groups from nodes of different register types.
	*/
	if (psPrevPReg->uType != psPReg->uType)
	{
		return IMG_FALSE;
	}

	/*	
		Check for the case of nodes precoloured to a particular hardware
		register type but with any register number.
	*/
	if (psPrevPReg->uNumber == USC_UNDEF || psPReg->uNumber == USC_UNDEF)
	{
		if (!(psPrevPReg->uNumber == USC_UNDEF && psPReg->uNumber == USC_UNDEF))
		{
			return IMG_FALSE;
		}
		if (psPrevNode->psFixedReg != psNode->psFixedReg)
		{
			/*
				Check that the previous node is the last variable in the its fixed register.
			*/
			if (psPrevNode->uFixedRegOffset != (psPrevNode->psFixedReg->uConsecutiveRegsCount - 1))
			{
				return IMG_FALSE;
			}
			/*
				Check the next node is the first variable in its fixed register.
			*/
			if (psNode->uFixedRegOffset != 0)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Check the nodes are at consecutive offsets in the fixed register.
			*/
			if ((psPrevNode->uFixedRegOffset + 1) != psNode->uFixedRegOffset)
			{	
				return IMG_FALSE;
			}
		}
	}
	else
	{
		/*
			For precoloured nodes check the physical register numbers are
			consecutive.
		*/
		if ((psPrevPReg->uNumber + psPrevNode->uFixedRegOffset + 1) != (psPReg->uNumber + psNode->uFixedRegOffset))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsCRegMoveResultDead(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: IsCRegMoveResultDead

 PURPOSE	: Check if the destination of a MOV inserted to copy a source
			  argument is dead.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Current instruction.
			  psGroup			- Destination of the MOV.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PCREG_MOVE	psCRegMove = psGroup->u.psCRegMove;

	/*
		Is this the destination of a MOV at all?
	*/
	if (psCRegMove == NULL)
	{
		return IMG_FALSE;
	}

	/*
		If the destination of the MOV isn't a source to the current instruction then
		it must be dead.
	*/
	ASSERT(psCRegMove->psDest == psGroup);
	ASSERT(psCRegMove->uLastUseId <= psInst->uId);
	if (psCRegMove->uLastUseId == psInst->uId)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ValidGroup(PREGISTER_GROUP*			apsGroup,
					IMG_UINT32					uGroupCount,
					HWREG_ALIGNMENT				eBaseNodeAlignment)
/*****************************************************************************
 FUNCTION	: ValidGroup

 PURPOSE	: Check if a group of registers can be given consecutive register
			  numbers.

 PARAMETERS	: apsGroup			- Registers to check.
			  uGroupCount		- Count of registers.
			  eBaseNodeAlignment
								- Required alignment for the hardware register
								number assigned to the first argument in the
								group.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	HWREG_ALIGNMENT eRequiredNodeAlignment;
	IMG_UINT32		uArg;
	PREGISTER_GROUP	psPrevNode = NULL;
	PREGISTER_GROUP	psBaseNode = NULL;

	/*
		Set the required alignment for the first variable in the group;
	*/
	eRequiredNodeAlignment = eBaseNodeAlignment;

	for (uArg = 0; uArg < uGroupCount; uArg++)
	{
		PREGISTER_GROUP	psNode = apsGroup[uArg];
		HWREG_ALIGNMENT	eExistingNodeAlignment;

		/*
			Check if this node already has restrictions on the alignment of its
			hardware register number which would be incompatible with the alignment
			required because of its position in this group.
		*/
		eExistingNodeAlignment = GetNodeAlignment(psNode);
		if (!AreAlignmentsCompatible(eExistingNodeAlignment, eRequiredNodeAlignment))
		{
			return IMG_FALSE;
		}

		/*
			Check later nodes in the group have a compatible alignment requirement. 
		*/
		if (eRequiredNodeAlignment == HWREG_ALIGNMENT_NONE &&
			eExistingNodeAlignment != HWREG_ALIGNMENT_NONE)
		{
			eRequiredNodeAlignment = eExistingNodeAlignment;
		}
	
		if (uArg != 0)
		{
			/*
				Check if this node already has different register which has to be
				given the next lower hardware register number.
			*/
			if (psNode != NULL && psNode->psPrev != NULL && psNode->psPrev != psPrevNode)
			{
				return IMG_FALSE;
			}
			/*
				Check if the previous node already has a different register which
				has to be given the next higher hardware register number.
			*/
			if (psPrevNode != NULL && psPrevNode->psNext != NULL && psPrevNode->psNext != psNode)
			{
				return IMG_FALSE;
			}

			/*
				Check for nodes already assigned a specific hardware register that the hardware
				registers are consecutive.
			*/
			if (!ArePrecolouredNodeConsecutive(psPrevNode, psNode))
			{
				return IMG_FALSE;
			}
			/*
				Check if the node already appears in the group.
			*/
			/*
				Check if the node already has to be given a lower register
				number than the first node in the group.
			*/
			if (IsAfter(psNode, psBaseNode))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Record the register in the group.
			*/
			psBaseNode = psNode;
		}

		psPrevNode = psNode;
		eRequiredNodeAlignment = g_aeOtherAlignment[eRequiredNodeAlignment];
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsConsecutiveTemps(PINTERMEDIATE_STATE	psState,
							IMG_UINT32			uGroupCount,
							PARG				asSetArg,
							HWREG_ALIGNMENT		eBaseNodeAlignment,
							IMG_BOOL			bReplaceNonTempsByMOVs,
							PREGISTER_GROUP*	ppsFirstNode,
							PREGISTER_GROUP*	ppsLastNode)
/*****************************************************************************
 FUNCTION	: IsConsecutiveTemps

 PURPOSE	: Check if a group of registers can be given consecutive register
			  numbers.

 PARAMETERS	: psState			- Compiler state.
			  uGroupCount		- Count of registers.
			  asSetArg			- Points to the arguments in the group.
			  eBaseNodeAlignment
								- Required alignment for the hardware register
								number assigned to the first argument in the
								group.
			  bReplaceNonTempsByMOVs
								- If TRUE then if the group contains immediate
								constants then treat them like newly allocated
								temporaries.
								  If FALSE return false if the groups contains
							    immediate constants.
			  ppsFirstNode		- If non-NULL returns register group information
								for the first argument.
			  ppsLastNode		- If non-NULL returns register group information
								for the last argument.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32			uArg;
	PREGISTER_GROUP		apsGroup[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];

	ASSERT(uGroupCount <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);
	for (uArg = 0; uArg < uGroupCount; uArg++)
	{
		IMG_UINT32	uPrevArg;

		if (asSetArg[uArg].uType != USEASM_REGTYPE_TEMP)
		{
			if (bReplaceNonTempsByMOVs)
			{
				/*
					Treat the current argument like a temporary with no existing
					information about restrictions on the assigned hardware register
					numbers.
				*/
				apsGroup[uArg] = NULL;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Get information about any existing restrictions on the hardware
				register number of this argument.
			*/
			apsGroup[uArg] = FindRegisterGroup(psState, asSetArg[uArg].uNumber);
		}

		/*
			Check if the node already appears in the group.
		*/
		for (uPrevArg = 0; uPrevArg < uArg; uPrevArg++)
		{
			if (asSetArg[uPrevArg].uNumber == asSetArg[uArg].uNumber)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Check the restrictions on hardware register numbers is compatible with
		this group of registers.
	*/
	if (!ValidGroup(apsGroup, uGroupCount, eBaseNodeAlignment))
	{
		return IMG_FALSE;
	}

	if (ppsFirstNode != NULL)
	{
		*ppsFirstNode = apsGroup[0];
	}
	if (ppsLastNode != NULL)
	{
		*ppsLastNode = apsGroup[uGroupCount - 1];
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsValidGroup(PINTERMEDIATE_STATE	psState,
					  PINST					psInst,
					  IMG_UINT32			uGroupCount,
					  PARG					asSetArg,
					  HWREG_ALIGNMENT		eBaseNodeAlignment,
					  IMG_BOOL				bReplaceNonTempsByMOVs)
/*****************************************************************************
 FUNCTION	: IsValidGroup

 PURPOSE	: Check if a group of registers can be given consecutive register
			  numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to which the registers are
								sources or destinations.
			  uGroupCount		- Count of registers.
			  asSetArg			- Points to the arguments in the group.
			  eBaseNodeAlignment
								- Required alignment for the hardware register
								number assigned to the first argument in the
								group.
			 bReplaceNonTempsByMOVs
								- If TRUE then if the group contains immediate
								constants then treat them like newly allocated
								temporaries.
								  If FALSE return false if the groups contains
							    immediate constants.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PREGISTER_GROUP	psFirstNode, psLastNode;

	/*
		CHeck any existing constraints on the hardware register numbers assigned to these registers
		are compatible with their use in this group.
	*/
	if (!IsConsecutiveTemps(psState, 
							uGroupCount, 
							asSetArg, 
							eBaseNodeAlignment,
							bReplaceNonTempsByMOVs, 
							&psFirstNode,
							&psLastNode))
	{
		return IMG_FALSE;
	}

	/*
		Check the size of the new group.
	*/
	if (!IsPrecolouredNode(psFirstNode))
	{
		IMG_UINT32	uMaximumRangeLength = GetMaximumRangeLength(psState, psInst->psBlock->psOwner->psFunc);
		IMG_UINT32	uGroupCountBeforeHead;
		IMG_UINT32	uGroupCountAfterTail;

		uGroupCountBeforeHead = GetPrevNodeCount(psFirstNode);
		uGroupCountAfterTail = GetNextNodeCount(psLastNode);
		if ((uGroupCountBeforeHead + uGroupCount + uGroupCountAfterTail) >= uMaximumRangeLength)
		{	
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
PEQUIVALENT_SOURCE GetEquivalentSource(PINTERMEDIATE_STATE psState, PUSC_TREE psEquivSrcTree, PARG psArg)
/*****************************************************************************
 FUNCTION	: GetEquivalentSource

 PURPOSE	: Check if an MOV instructions have been inserted to fix consecutive
			  registers with a particular source argument.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcTree	- Details of inserted MOV instructions.
			  psArg				- Source argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	EQUIVALENT_SOURCE	sKey;
	PEQUIVALENT_SOURCE	psNode;

	sKey.uSourceType = psArg->uType;
	sKey.uSourceNum = psArg->uNumber;

	psNode = (PEQUIVALENT_SOURCE)UscTreeGetPtr(psEquivSrcTree, &sKey);
	if (psNode == NULL)
	{
		return NULL;
	}

	ASSERT(psNode->uSourceType == psArg->uType);
	ASSERT(psNode->uSourceNum == psArg->uNumber);

	return psNode;
}

static
PEQUIVALENT_SOURCE CreateEquivalent(PINTERMEDIATE_STATE	psState,
									PUSC_TREE			psEquivSrcTree,
									PARG				psSrcArg)
/*****************************************************************************
 FUNCTION	: CreateEquivalent

 PURPOSE	: Create a structure to describe which temporary registers contain
			  copies of some data.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcTree	- Updated with information about copies.
			  psSrcArg			- Data which has been copied into a temporary.

 RETURNS	: Nothing.
*****************************************************************************/
{
	EQUIVALENT_SOURCE	sNewElem;

	sNewElem.uSourceType = psSrcArg->uType;
	sNewElem.uSourceNum = psSrcArg->uNumber;
	InitializeList(&sNewElem.sCRegMoveList);

	return (PEQUIVALENT_SOURCE)UscTreeAdd(psState, psEquivSrcTree, &sNewElem);
}

static
IMG_VOID RemoveFromEquivalentList(PINTERMEDIATE_STATE psState, PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: RemoveFromEquivalentList

 PURPOSE	: Removes an entry from the list of registers containing a copy of some
			  other register.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Destination for the copy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCREG_MOVE			psCRegMove;

	/*
		Detach information from the copy from the copy destination.
	*/
	psCRegMove = psGroup->u.psCRegMove;
	psGroup->u.psCRegMove = NULL;

	/*
		Remove information about the copy from the list associated with the
		copy source.
	*/
	RemoveFromList(&psCRegMove->psSource->sCRegMoveList, &psCRegMove->sListEntry);

	/*	
		Free information about the copy.
	*/
	UscFree(psState, psCRegMove);
}

static
IMG_VOID AddToEquivalentList(PINTERMEDIATE_STATE	psState,
							 PEQUIVALENT_SOURCE		psCopySource,
							 PARG					psDestArg,
							 PARG					psLastUse,
							 IMG_UINT32				uLastUseId)
/*****************************************************************************
 FUNCTION	: AddToEquivalentList

 PURPOSE	: Add an entry to the list of registers containing a copy of some
			  other register.

 PARAMETERS	: psState			- Compiler state.
			  psCopySource		- Source data for the copy.
			  psDestArg			- Destination for the copy.
			  psLastUse			- Last use of the equivalent data.
			  uLastUseId		- ID of the instruction where the equivalent data
								is last used.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCREG_MOVE			psCRegMove;
	PREGISTER_GROUP		psDest;

	/*
		Add a structure to record which register the MOV destination contains
		of a copy of.
	*/
	psCRegMove = UscAlloc(psState, sizeof(*psCRegMove));
	psCRegMove->psSource = psCopySource;
	psCRegMove->psLastUse = psLastUse;
	psCRegMove->uLastUseId = uLastUseId;

	/*
		Add register grouping information for the MOV destination.
	*/
	ASSERT(psDestArg->uType == USEASM_REGTYPE_TEMP);
	psDest = AddRegisterGroup(psState, psDestArg->uNumber);

	/*
		Link the MOV destination with information about the copy.
	*/
	psCRegMove->psDest = psDest;
	psDest->u.psCRegMove = psCRegMove;

	/*
		Add the MOV destination to the list of copies of the source register.
	*/
	AppendToList(&psCopySource->sCRegMoveList, &psCRegMove->sListEntry);
}

static
IMG_VOID AddEquivalent(PINTERMEDIATE_STATE	psState,
					   PUSC_TREE			psEquivSrcTree,
					   PARG					psDestArg,
					   PARG					psSrcArg,
					   PARG					psLastUse,
					   IMG_UINT32			uLastUseId)
/*****************************************************************************
 FUNCTION	: AddEquivalent

 PURPOSE	: Record a MOV instruction inserted to fix consecutive register sets.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcTree	- Updated with details of the instruction.
			  psDestArg			- Move destination copying the data.
			  psSrcArg			- Source of the MOV.
			  psLastUse			- Last use of the equivalent data.
			  uLastUseId		- ID of the instruction where the equivalent data
								is last used.

 RETURNS	: Nothing.
*****************************************************************************/
{
	EQUIVALENT_SOURCE	sKey;
	PEQUIVALENT_SOURCE	psExistingElem;

	sKey.uSourceType = psSrcArg->uType;
	sKey.uSourceNum = psSrcArg->uNumber;

	/*
		If necessary add a new element to the tree keyed on the copied data.
	*/
	psExistingElem = (PEQUIVALENT_SOURCE)UscTreeGetPtr(psEquivSrcTree, &sKey);
	if (psExistingElem == NULL)
	{
		psExistingElem = CreateEquivalent(psState, psEquivSrcTree, psSrcArg);	
	}

	/*
		Add to the list of registers containing a copy.
	*/
	AddToEquivalentList(psState,
						psExistingElem,
						psDestArg,
						psLastUse,
						uLastUseId);
}

IMG_INTERNAL
IMG_UINT32 GetPrecolouredRegisterType(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: GetPrecolouredRegisterType

 PURPOSE	: Check if an intermediate register will be assigned to a fixed
			  hardware register bank.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- The instruction to which the intermediate register
							is a source or destination.
			  uType			- Intermediate register.
			  uNumber

 RETURNS	: The hardware register bank.
*****************************************************************************/
{
	PREGISTER_GROUP	psGroup;

	switch (uType)
	{
		case USC_REGTYPE_REGARRAY:
		{
			ASSERT(uNumber < psState->uNumVecArrayRegs);
			psGroup = psState->apsVecArrayReg[uNumber]->psGroup;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		{
			PVREGISTER	psVReg;

			psVReg = GetVRegister(psState, uType, uNumber);
			psGroup = psVReg->psGroup;
			break;
		}
		default: 
		{
			psGroup = NULL;
			break;
		}
	}

	/*
		Get the hardware register bank the index will be assigned to.
	*/
	if (psGroup != NULL && psGroup->psFixedReg != NULL)
	{
		IMG_UINT32	uPrecolouredType;
		uPrecolouredType = psGroup->psFixedReg->sPReg.uType;
		if ((uPrecolouredType == USEASM_REGTYPE_SECATTR) && (psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg))
		{
			uPrecolouredType = USEASM_REGTYPE_PRIMATTR;	
		}
		return uPrecolouredType;
	}
	else
	{
		return uType;
	}
}

IMG_INTERNAL
IMG_BOOL IsPrecolouredToSecondaryAttribute(PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: IsPrecolouredNode

 PURPOSE	: Check if a node is precoloured to a secondary attribute.

 PARAMETERS	: psGroup		- Node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PFIXED_REG_DATA	psFixedReg;
	if (psGroup == NULL)
	{
		return IMG_FALSE;
	}
	psFixedReg = psGroup->psFixedReg;
	if (psFixedReg == NULL)
	{
		return IMG_FALSE;
	}
	if (psFixedReg->sPReg.uType != USEASM_REGTYPE_SECATTR && psFixedReg->sPReg.uType != USC_REGTYPE_CALCSECATTR)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsPrecolouredNode(PREGISTER_GROUP psNode)
/*****************************************************************************
 FUNCTION	: IsPrecolouredNode

 PURPOSE	: Check if a node is precoloured to a fixed hardware register.

 PARAMETERS	: psNode		- Node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psNode != NULL && psNode->psFixedReg != NULL) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsNodeInGroup(PREGISTER_GROUP psNode)
/*****************************************************************************
 FUNCTION	: IsNodeInGroup

 PURPOSE	: Check if a node is part of a register group

 PARAMETERS	: psState		   - Compiler state.
			  psNode		   - The node to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psNode != NULL && (psNode->psPrev != NULL || psNode->psNext != NULL)) ? IMG_TRUE : IMG_FALSE;
}

IMG_INTERNAL
PREGISTER_GROUP GetConsecutiveGroup(PREGISTER_GROUP psGroup, DIRECTION eDir)
/*****************************************************************************
 FUNCTION	: GetConsecutiveGroup

 PURPOSE	: Return the register which requires a hardware register number
			 consecutive to a specified register.

 PARAMETERS	: psGroup			- Specified register.
			  eDir				- If BACKWARD: get the register requiring a hardware
								register number one greater.
								  If FORWARD: get the register requiring a hardware
							    register number one more.

 RETURNS	: A pointer to the consecutive register if it exists.
*****************************************************************************/
{
	if (psGroup == NULL)
	{
		return NULL;
	}
	else
	{
		if (eDir == DIRECTION_FORWARD)
		{
			return psGroup->psNext;
		}
		else
		{
			return psGroup->psPrev;
		}
	}
}

static
IMG_VOID BaseSetNodeAlignment(PINTERMEDIATE_STATE	psState,
							  PREGISTER_GROUP		psNode,
							  HWREG_ALIGNMENT		eAlignment,
							  IMG_BOOL				bAlignRequiredByInst)
/*****************************************************************************
 FUNCTION	: BaseSetNodeAlignment

 PURPOSE	: Sets the required alignment for the hardware register number to
			  be assigned to a node.

 PARAMETERS	: psRAData			- Register allocator state.
			  psNode			- Node to set the alignment for.
			  eAlignment		- Alignment to set.
			  bAlignRequiredByInst
								- If TRUE the alignment is required by an
								instruction to which the node is a source or a
								destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(eAlignment == HWREG_ALIGNMENT_NONE || eAlignment == HWREG_ALIGNMENT_EVEN || eAlignment == HWREG_ALIGNMENT_ODD);
	ASSERT(psNode->eAlign == HWREG_ALIGNMENT_NONE || psNode->eAlign == eAlignment);

	psNode->eAlign = eAlignment;
	if (bAlignRequiredByInst)
	{
		psNode->bAlignRequiredByInst = IMG_TRUE;
	}
}

static
IMG_VOID BaseSetNodeAlignmentFromRegisterNumber(PINTERMEDIATE_STATE	psState,
												PREGISTER_GROUP		psNode,
												IMG_UINT32			uHardwareRegisterNumber)
/*****************************************************************************
 FUNCTION	: BaseSetNodeAlignmentFromRegisterNumber

 PURPOSE	: Set the alignment for an intermediate register from the hardware
			  register number assigned to it.

 PARAMETERS	: psRAData			- Register allocator state.
			  psNode			- Node to set the alignment for.
			  uHardwareRegisterNumber
								- Hardware register number to set the alignment
								from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if ((uHardwareRegisterNumber % 2) == 0)						
	{
		BaseSetNodeAlignment(psState, psNode, HWREG_ALIGNMENT_EVEN, IMG_FALSE /* bAlignRequiredByInst */);
	}
	else
	{
		BaseSetNodeAlignment(psState, psNode, HWREG_ALIGNMENT_ODD, IMG_FALSE /* bAlignRequiredByInst */);
	}
}

IMG_INTERNAL
IMG_VOID SetLinkedNodeAlignments(PINTERMEDIATE_STATE	psState,
							     PREGISTER_GROUP		psNode,
								 DIRECTION				eDir,
								 IMG_BOOL				bAlignRequiredByInst)
/*****************************************************************************
 FUNCTION	: SetLinkedNodeAlignments

 PURPOSE	: Set the required alignment of either all the nodes which must
			  have register numbers lower than a given node or all those
			  which must have higher register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psNode			- Node to set the alignment from.
			  eDir				- FORWARD: Set the alignment for all nodes with higher register numbers.
								  BACKWARD: Set the alignment for all nodes with lower register numbers.
			  bAlignRequiredByInst
								- If TRUE the alignment is required because of an instruction to which
								the node is a source or a destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP		psLinkedNode;
	HWREG_ALIGNMENT		eNodeAlignment = psNode->eAlign;
	HWREG_ALIGNMENT		eLinkedNodeAlignment;

	if (eNodeAlignment == HWREG_ALIGNMENT_NONE)
	{
		return;
	}

	psLinkedNode = GetConsecutiveGroup(psNode, eDir);
	eLinkedNodeAlignment = g_aeOtherAlignment[eNodeAlignment];

	while (psLinkedNode != NULL)
	{
		/*
			Set the alignment for the current node.
		*/
		BaseSetNodeAlignment(psState, psLinkedNode, eLinkedNodeAlignment, bAlignRequiredByInst);

		/*
			Move to the node which either has to be given a register number
			one higher or one lower than the current.
		*/
		psLinkedNode = GetConsecutiveGroup(psLinkedNode, eDir);

		/*
			Flip the required alignment:
				ODD -> EVEN
				EVEN -> ODD
		*/
		eLinkedNodeAlignment = g_aeOtherAlignment[eLinkedNodeAlignment];
	}
}

IMG_INTERNAL
IMG_VOID DropLinkAfterNode(PINTERMEDIATE_STATE psState, PREGISTER_GROUP psNode)
/*****************************************************************************
 FUNCTION	: DropLinkAfterNode

 PURPOSE	: 

 PARAMETERS	: psRAData		- Module state.
			  uNode			- Index of the node to drop the link for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psNode->bLinkedByInst = IMG_FALSE;
	psNode->bOptional = IMG_FALSE;

	ASSERT(psNode->psNext->psPrev == psNode);
	psNode->psNext->psPrev = NULL;

	AppendToList(&psState->psGroupState->sGroupHeadsList, &psNode->psNext->sGroupHeadsListEntry);

	psNode->psNext = NULL;
}

IMG_INTERNAL
IMG_VOID SetNodeAlignment(PINTERMEDIATE_STATE	psState,
						  PREGISTER_GROUP		psNode,
						  HWREG_ALIGNMENT		eAlignment,
						  IMG_BOOL				bAlignRequiredByInst)
/*****************************************************************************
 FUNCTION	: SetNodeAlignment

 PURPOSE	: Sets the required alignment for the hardware register number to
			  be assigned to a node. If any other nodes require either lower or
			  higher register numbers than the node then update their
			  alignment informaton too.

 PARAMETERS	: psState			- Compiler state.
			  psNode			- Node to set the alignment for.
			  eAlignment		- Alignment to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(eAlignment == HWREG_ALIGNMENT_EVEN || eAlignment == HWREG_ALIGNMENT_ODD);

	if (psNode->eAlign == HWREG_ALIGNMENT_NONE || bAlignRequiredByInst != psNode->bAlignRequiredByInst)
	{
		/*
			Set the alignment for the node itself.
		*/
		BaseSetNodeAlignment(psState, psNode, eAlignment, bAlignRequiredByInst);

		/*
			For all nodes which require a register number lower than the node set their
			alignments.
		*/
		SetLinkedNodeAlignments(psState, psNode, DIRECTION_BACKWARD, bAlignRequiredByInst);

		/*
			For all nodes which require a register number higher than the node set their
			alignment.
		*/
		SetLinkedNodeAlignments(psState, psNode, DIRECTION_FORWARD, bAlignRequiredByInst);
	}
	else
	{
		ASSERT(psNode->eAlign == eAlignment);
	}
}

IMG_INTERNAL
IMG_VOID AppendToGroup(PINTERMEDIATE_STATE	psState,
                       PREGISTER_GROUP		psFirstNode,
                       PREGISTER_GROUP		psNode,
                       IMG_BOOL				bOptional)
/*****************************************************************************
 FUNCTION   : AppendToGroup

 PURPOSE    : Add a node to the end of a register group.

 PARAMETERS : psState			- Compiler state.
              psFirstNode       - First node in the group.
              psNode            - Node to add.
              bOptional         - Whether this is an optional group

 RETURNS    : Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP	psPrevNode;
	IMG_BOOL		bRet;

	/* Find the end of the group */
	psPrevNode = psFirstNode;
	while (psPrevNode->psNext != NULL)
	{
		psPrevNode = psPrevNode->psNext;
	}

    bRet = AddToGroup(psState, 
					  psPrevNode->uRegister, 
					  psPrevNode, 
					  psNode->uRegister, 
					  psNode, 
					  IMG_FALSE /* bLinkedByInst */,
					  bOptional);
	ASSERT(bRet);
}

IMG_INTERNAL
IMG_BOOL AddToGroup(PINTERMEDIATE_STATE psState,
				    IMG_UINT32			uPrevNode,
					PREGISTER_GROUP		psPrevNode,
					IMG_UINT32			uNode,
					PREGISTER_GROUP		psNode,
					IMG_BOOL			bLinkedByInst,
					IMG_BOOL			bOptional)
/*****************************************************************************
 FUNCTION	: AddToGroup

 PURPOSE	: Add a node to a register group.

 PARAMETERS	: psState			- Compiler state.
			  uPrevNode			- Last node in the group.
			  uNode				- Node to add.
			  bLinkedByInst		- Is this link required by an instruction?
			  bOptional			- Is this an optional group?

 RETURNS	: Nothing.
*****************************************************************************/
{
	HWREG_ALIGNMENT ePrevNodeAlignment, eNodeAlignment;
	IMG_BOOL bDisconnectNext, bDisconnectPrevious;

	/*
		Can't give a variable a register number lower than itself.
	*/
	if (uNode == uPrevNode)
	{
		return IMG_FALSE;
	}

	/*
		Check for either of the nodes being precoloured.
	*/
	if (!ArePrecolouredNodeConsecutive(psPrevNode, psNode))
	{
		return IMG_FALSE;
	}

	/*
		Check for a different node already having to be given the next register number.
	*/
	bDisconnectNext = IMG_FALSE;
	if (psPrevNode != NULL && (psPrevNode->psNext != NULL && psPrevNode->psNext != psNode))
	{
		/*
			Check for an optional link to the next node where we are trying to
			establish a mandatory link.
		*/
		if (psPrevNode->bOptional && !bOptional)
		{
			/*
				Remove the optional link.
			*/
			bDisconnectNext = IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}
	
	bDisconnectPrevious = IMG_FALSE;
	if (psNode != NULL)
	{
		/*
			Check for a different node already having to be given the previous register number.
		*/
		if (psNode->psPrev != NULL && psNode->psPrev != psPrevNode)
		{
			/*
				Check for an optional link to the previous node where we are trying to
				establish a mandatory link.
			*/
			if (psNode->psPrev->bOptional && !bOptional)
			{
				/*
					Remove the optional link.
				*/
				bDisconnectPrevious = IMG_TRUE;
			}
			else
			{	
				return IMG_FALSE;
			}	
		}
		/*
			Check for NODE having to be given a lower register number than PREVNODE.
		*/
		if (IsAfter(psNode, psPrevNode))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check if both NODE and PREVNODE need a double aligned register number.
	*/
	ePrevNodeAlignment = GetNodeAlignment(psPrevNode);
	eNodeAlignment = GetNodeAlignment(psNode);
	if (ePrevNodeAlignment != HWREG_ALIGNMENT_NONE && eNodeAlignment != HWREG_ALIGNMENT_NONE)
	{
		/*
			We can't make the hardware register number for PREVNODE one less than the
			hardware register number for NODE if both PREVNODE and NODE require even
			hardware register numbers.
		*/
		if (ePrevNodeAlignment == eNodeAlignment)
		{
			return IMG_FALSE;
		}
	}

	/*
		Allocate register grouping information for both nodes.
	*/
	if (psPrevNode == NULL)
	{
		psPrevNode = AddRegisterGroup(psState, uPrevNode);
	}
	if (psNode == NULL)
	{
		psNode = AddRegisterGroup(psState, uNode);
	}

	/*
		If one of the nodes already requires a hardware register number with a
		particular alignment then set the alignment for the other node.
	*/
	if (eNodeAlignment != HWREG_ALIGNMENT_NONE)
	{
		SetNodeAlignment(psState, psPrevNode, g_aeOtherAlignment[eNodeAlignment], bLinkedByInst);
	}
	else if (ePrevNodeAlignment != HWREG_ALIGNMENT_NONE)
	{
		SetNodeAlignment(psState, psNode, g_aeOtherAlignment[ePrevNodeAlignment], bLinkedByInst);
	}

	/*
		Remove optional links from the node where they have been superceded by
		mandatory links.
	*/
	if (bDisconnectNext)
	{
		DropLinkAfterNode(psState, psPrevNode);
	}
	if (bDisconnectPrevious)
	{
		DropLinkAfterNode(psState, psNode->psPrev);
	}

	/*
		Flag whether the link between the two nodes is optional (i.e. can be
		ignored without affecting the correctness of the program).
	*/
	if (bOptional)
	{
		if (psPrevNode->psNext == NULL)
		{
			psPrevNode->bOptional = IMG_TRUE;
		}
	}
	else
	{
		psPrevNode->bOptional = IMG_FALSE;
	}

	/*
		Record whether the two nodes are used together in an instruction.
	*/
	if (bLinkedByInst)
	{
		psPrevNode->bLinkedByInst = IMG_TRUE;
	}

	/*
		Make a link between the nodes.
	*/
	psPrevNode->psNext = psNode;
	if (psNode->psPrev == NULL)
	{
		RemoveFromList(&psState->psGroupState->sGroupHeadsList, &psNode->sGroupHeadsListEntry);
	}
	psNode->psPrev = psPrevNode;

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID RemoveFromGroup(PINTERMEDIATE_STATE psState,
						 PREGISTER_GROUP psNode)
/*****************************************************************************
 FUNCTION	: RemoveFromGroup

 PURPOSE	: Remove a node from a register group.

 PARAMETERS	: psState			- Compiler state.
			  psNode			- Node to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psNode->psPrev != NULL)
	{
		DropLinkAfterNode(psState, psNode->psPrev);
	}
	if (psNode->psNext != NULL)
	{
		DropLinkAfterNode(psState, psNode);
	}
}

static
IMG_BOOL CheckForEquivalentSet(PINTERMEDIATE_STATE	psState,
							   PINST				psInst,
							   IMG_UINT32			uArgStart,
							   PARG					asSetArg,
							   PREGISTER_GROUP		psCRegDestGroup,
							   IMG_UINT32			uStartOffset,
							   IMG_UINT32			uArgCount,
							   HWREG_ALIGNMENT		eArgAlign,
							   IMG_PUINT32			puReuseableSourceMask,
							   IMG_PUINT32			puReuseableSourceCount,
							   IMG_PBOOL			pbSomeSourcesOverwritten,
							   PREGISTER_GROUP*		ppsBaseDestGroup)
/*****************************************************************************
 FUNCTION	: CheckForEquivalents

 PURPOSE	: Where a group of arguments to an instruction can't be given consecutive
			  hardware register numbers (because some of the arguments are already
			  used in a different order in other instructions requiring consecutive
			  hardware register numbers) check for other registers containing the
			  the same data as the current arguments.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcTree	- Map from registers to newly added registers which
								contain the same value.
			  asSetArg			- Sets of arguments to check. On success updated
								with the equivalent registers.
			  psCRegDestGroup	- Variable which is known to contain a copy of
								one of the arguments.
			  uStartOffset		- Offset of the argument which has been copied
								into 'psCRegDestGroup'.
			  uArgCount			- Count of arguments in the set.
			  bArgAlign			- Required alignment of the hardware register
								number for the first argument in the group.
			  puReuseableSourceMask
								- Returns a mask of the arguments for which we
								could avoid adding MOVs.
			  puReuseableSourceCount
								- Returns a count of the arguments for which we
								could avoid adding MOVs.
			  pbSomeSourcesOverwritten
								-
			  ppsBaseDestGroup	- Returns the first variable in the group
								including 'psCRegDestGroup'.

 RETURNS	: TRUE if the group of registers including 'psCRegDestGroup' could
			  be used as arguments to the current instruction.
*****************************************************************************/
{
	PREGISTER_GROUP	psGroup;
	IMG_UINT32		uReuseableSourceMask;
	IMG_UINT32		uReuseableSourceCount;
	IMG_UINT32		uArg;
	IMG_INT32		iArg;
	IMG_BOOL		bSomeSourcesOverwritten;

	/*
		At least the first MOV can be reused.
	*/
	uReuseableSourceMask = (1U << uStartOffset);
	uReuseableSourceCount = 1;
	bSomeSourcesOverwritten = IMG_FALSE;

	/*
	 	Check any alignment requirement for the equivalent group is compatible.
	*/
	if (eArgAlign != HWREG_ALIGNMENT_NONE)
	{
		HWREG_ALIGNMENT	eAlign;

		/* Get the alignment required for this source to the instruction. */
		if ((uStartOffset % 2) == 0)
		{
			eAlign = eArgAlign;
		}
		else
		{
			eAlign = g_aeOtherAlignment[eArgAlign];
		}

		if (!AreAlignmentsCompatible(eAlign, psCRegDestGroup->eAlign))
		{
			return IMG_FALSE;
		}
	}

	/*
		Check which MOVs from the complete block have the same sources as the sources to the
		current instruction.
	*/
	for (uArg = uStartOffset + 1, psGroup = psCRegDestGroup->psNext; 
		 uArg < uArgCount;
		 uArg++, psGroup = psGroup->psNext)
	{
		PARG		psSetArg = &asSetArg[uArg];
		IMG_BOOL	bReuseableSource;

		if (psGroup == NULL)
		{
			return IMG_FALSE;
		}
		if (!CanUseIntermediateSource(psState, psInst, uArgStart + uArg, psGroup))
		{
			return IMG_FALSE;
		}

		bReuseableSource = IMG_FALSE;

		/*
			Check if this register is already the correct source argument.
		*/
		if (psSetArg->uType == USEASM_REGTYPE_TEMP && psSetArg->uNumber == psGroup->uRegister)
		{
			bReuseableSource = IMG_TRUE;
		}

		/*
			Check if this register containing a copy of another register.
		*/
		if (psGroup->u.psCRegMove != NULL)
		{
			PCREG_MOVE			psCRegMove = psGroup->u.psCRegMove;
			PEQUIVALENT_SOURCE	psCRegSource = psCRegMove->psSource;

			ASSERT(psCRegMove->psDest == psGroup);

			/*
				Check if this register contains a copy of the current source argument.
			*/
			if (psCRegSource->uSourceType == psSetArg->uType && psCRegSource->uSourceNum == psSetArg->uNumber)
			{
				bReuseableSource = IMG_TRUE;
			}
		}
		
		if (bReuseableSource)
		{
			uReuseableSourceMask |= (1U << uArg);
			uReuseableSourceCount++;
		}
		else
		{
			/*
				If this register doesn't already contain the right data check if we could overwrite it.
			*/
			if (!IsCRegMoveResultDead(psState, psInst, psGroup))
			{
				return IMG_FALSE;
			}
			bSomeSourcesOverwritten = IMG_TRUE;
		}
	}

	/*
		Check for the registers in the group before the first copy of a source argument that we can
		overwrite them.
	*/
	*ppsBaseDestGroup = psCRegDestGroup;
	psGroup = psCRegDestGroup->psPrev;
	for (iArg = (IMG_INT32)(uStartOffset - 1); iArg >= 0; iArg--)
	{
		if (psGroup == NULL)
		{
			return IMG_FALSE;
		}
		if (!IsCRegMoveResultDead(psState, psInst, psGroup))
		{
			return IMG_FALSE;
		}

		*ppsBaseDestGroup = psGroup;
		psGroup = psGroup->psPrev;
		bSomeSourcesOverwritten = IMG_TRUE;
	}

	/*
		Return the mask/count of MOVs whose results we can reuse for this set of source arguments.
	*/
	*puReuseableSourceMask = uReuseableSourceMask;
	*puReuseableSourceCount = uReuseableSourceCount;
	*pbSomeSourcesOverwritten = bSomeSourcesOverwritten;

	return IMG_TRUE;
}

static
IMG_BOOL ReuseExistingSources(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  PARG					asSetArg,
							  IMG_UINT32			uArgStart,
							  IMG_UINT32			uArgCount,
							  HWREG_ALIGNMENT		eArgAlign,
							  IMG_PUINT32			puReuseableSourceMask,
							  IMG_PUINT32			puReuseableSourceCount,
							  IMG_PBOOL				pbSomeSourcesOverwritten,
							  PREGISTER_GROUP*		ppsBaseDestGroup)
/*****************************************************************************
 FUNCTION	: ReuseExistingSources

 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Current instruction.
			  asSetArg			- Current sources.
			  uArgCount			- Count of sources in the group.
			  eArgAlign			- Required alignment of the hardware register
								number for the first argument in the group.
			  puReuseableSourceMask
								- Returns a mask of the arguments for which we
								could avoid adding MOVs.
			  puReuseableSourceCount
								- Returns a count of the arguments for which we
								could avoid adding MOVs.
			  ppsBaseDestGroup	- Returns the first variable in the group
								including 'psCRegDestGroup'.

 RETURNS	: TRUE if the existing sources could be reused.
*****************************************************************************/
{
	PREGISTER_GROUP	psExistingSource;
	IMG_UINT32		uArg;

	/*
		Check for the case where there was an earlier instruction like

			SMP	..., rX, I0, ....

		we would have added a MOV to make.

			MOV rY, I0
			SMP	..., rX, rY, ....

		If the current instruction is

			SMP ...., rX, I1, ....

		then we can insert only one MOV to make

			MOV	rY, I0    ; Overwrites rY
			SMP ..., rX, rY, ....
	*/

	/*
		Look for an non-immediate source.
	*/
	psExistingSource = NULL;
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		if (asSetArg[uArg].uType == USEASM_REGTYPE_TEMP)
		{
			psExistingSource = FindRegisterGroup(psState, asSetArg[uArg].uNumber);
			if (psExistingSource == NULL)
			{
				continue;
			}
			if (!CanUseIntermediateSource(psState, psInst, uArgStart, psExistingSource))
			{
				return IMG_FALSE;
			}
			break;
		}
	}

	/*
		Either no non-immediate sources or the non-immediate sources have never been used
		before.
	*/
	if (psExistingSource == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Check whether we could only insert MOVs to copy immediate sources.
	*/
	return CheckForEquivalentSet(psState, 
								 psInst,
								 uArgStart,
								 asSetArg,
								 psExistingSource,
								 uArg, 
								 uArgCount, 
								 eArgAlign,
								 puReuseableSourceMask, 
								 puReuseableSourceCount,
								 pbSomeSourcesOverwritten,
							     ppsBaseDestGroup);
}

static
IMG_BOOL CheckForEquivalents(PINTERMEDIATE_STATE	psState,
							 PUSC_TREE				psEquivSrcTree,
							 PINST					psInst,
							 ARG					asSetArg[],
							 IMG_UINT32				uArgStart,
							 IMG_UINT32				uArgCount,
							 HWREG_ALIGNMENT		eArgAlign)
/*****************************************************************************
 FUNCTION	: CheckForEquivalents

 PURPOSE	: Where a group of arguments to an instruction can't be given consecutive
			  hardware register numbers (because some of the arguments are already
			  used in a different order in other instructions requiring consecutive
			  hardware register numbers) check for other registers containing the
			  the same data as the current arguments.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcTree	- Map from registers to newly added registers which
								contain the same value.
			  asSetArg			- Sets of arguments to check. On success updated
								with the equivalent registers.
			  uArgCount			- Count of arguments.
			  eArgAlign			- Required alignment of the hardware register
								number for the first argument in the group.

 RETURNS	: TRUE if the existing instruction sources were replaced by registers
			  containing copies of the sources.
*****************************************************************************/
{
	PEQUIVALENT_SOURCE	apsNode[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	IMG_UINT32			uArg;
	PUSC_LIST_ENTRY		psListEntry;
	IMG_UINT32			uBestReuseableSourceCount;
	IMG_UINT32			uBestReuseableSourceMask;
	IMG_BOOL			bBestOverwritingSomeSources;
	PREGISTER_GROUP		psBestBaseGroup;
	PREGISTER_GROUP		psGroup;
	IMG_UINT32			uReuseableSourceCount;
	IMG_UINT32			uReuseableSourceMask;
	IMG_BOOL			bOverwritingSomeSources;
	PREGISTER_GROUP		psBaseGroup;
	
	/*
		Check for each argument to the current instruction whether there are other
		registers containing the same data.
	*/
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		PARG	psSetArg = &asSetArg[uArg];

		if (psSetArg->uIndexType != USC_REGTYPE_NOINDEX || psSetArg->uType == USC_REGTYPE_REGARRAY)
		{
			return IMG_FALSE;
		}
		apsNode[uArg] = GetEquivalentSource(psState, psEquivSrcTree, &asSetArg[uArg]);
	}

	uBestReuseableSourceCount = 0;
	uBestReuseableSourceMask = 0;
	bBestOverwritingSomeSources = IMG_FALSE;
	psBestBaseGroup = NULL;

	/*
		Check how many copy instructions we would need to insert if we kept the
		existing source arguments.
	*/
	if (ReuseExistingSources(psState,
							 psInst,
							 asSetArg,
							 uArgStart,
							 uArgCount,
							 eArgAlign,
							 &uReuseableSourceMask, 
							 &uReuseableSourceCount,
							 &bOverwritingSomeSources,
							 &psBaseGroup))
	{
		uBestReuseableSourceMask = uReuseableSourceMask;
		uBestReuseableSourceCount = uReuseableSourceCount;
		bBestOverwritingSomeSources = bOverwritingSomeSources;
		psBestBaseGroup = psBaseGroup;
	}

	/*
		Look at each register which containing a copy of one of the source arguments.
	*/
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		if (apsNode[uArg] == NULL)
		{
			continue;
		}
		for (psListEntry = apsNode[uArg]->sCRegMoveList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PCREG_MOVE		psCRegMove = IMG_CONTAINING_RECORD(psListEntry, PCREG_MOVE, sListEntry);
			
			ASSERT(psCRegMove->psSource->uSourceType == asSetArg[uArg].uType);
			ASSERT(psCRegMove->psSource->uSourceNum == asSetArg[uArg].uNumber);

			/*
				Check for each register which is part of a group with the register containing a copy whether
				it is either a copy of another source argument in the subset or we could overwrite it with a
				copy.
			*/
			if (!CheckForEquivalentSet(psState, 
									   psInst,
									   uArgStart,
									   asSetArg,
									   psCRegMove->psDest,
									   uArg, 
									   uArgCount, 
									   eArgAlign,
									   &uReuseableSourceMask, 
									   &uReuseableSourceCount,
									   &bOverwritingSomeSources,
									   &psBaseGroup))
			{
				continue;
			}

			/*
				Check if reusing the results of this earlier move would save more instructions than
				the previous best.
			*/
			if (uReuseableSourceCount > uBestReuseableSourceCount)
			{
				uBestReuseableSourceCount = uReuseableSourceCount;
				uBestReuseableSourceMask = uReuseableSourceMask;
				bBestOverwritingSomeSources = bOverwritingSomeSources;
				psBestBaseGroup = psBaseGroup;
			}
		}
	}

	/*
		Check for no earlier moves available to reuse.
	*/
	if (psBestBaseGroup == NULL)
	{
		return IMG_FALSE;
	}

	/*
		If we are going to generate a copy of the equivalent registers then allocate temporary
		registers for the copy.
	*/
	if (bBestOverwritingSomeSources)
	{
		IMG_UINT32	uNewSrcNums;
		
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			If any of the arguments are constant registers we need to make VMOVs.
			If the register is not aligned to EVEN then we can't read it using a MOV.
		*/
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			for (uArg = 0; uArg < uArgCount; uArg++)
			{
				if (asSetArg[uArg].uType == USEASM_REGTYPE_FPCONSTANT)
				{
					return IMG_FALSE;
				}
			}
		}
			
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

		/*
			Allocate temporary register for the copy of the equivalent registers.
		*/
		uNewSrcNums = GetNextRegisterCount(psState, uArgCount);

		for (psGroup = psBestBaseGroup, uArg = 0; uArg < uArgCount; psGroup = psGroup->psNext, uArg++)
		{
			PINST			psCRegMoveInst;
			UF_REGFORMAT	eSrcFmt = asSetArg[uArg].eFmt;

			/*	
				Create a move instruction writing the new equivalent set.
			*/
			psCRegMoveInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psCRegMoveInst, IMOV);
			SetDest(psState, psCRegMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uNewSrcNums + uArg, eSrcFmt);
			InsertInstBefore(psState, psInst->psBlock, psCRegMoveInst, psInst);

			if ((uBestReuseableSourceMask & (1U << uArg)) != 0)
			{
				/*	
					Copy from the temporary already containing a copy of the existing source to the instruction.
				*/
				SetSrc(psState, psCRegMoveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, psGroup->uRegister, eSrcFmt);

				/*
					Add the move destination to the list of copies of the instruction source.
				*/
				if (psGroup->u.psCRegMove != NULL)
				{
					AddToEquivalentList(psState,
										psGroup->u.psCRegMove->psSource,
										&psCRegMoveInst->asDest[0],
										&asSetArg[uArg],
										psInst->uId);
				}
			}
			else
			{
				/*
					Copy from the existing source to the instruction.
				*/
				SetSrcFromArg(psState, psCRegMoveInst, 0 /* uSrcIdx */, &asSetArg[uArg]);

				/*
					Record the move destination contains a copy of the instruction source.
				*/
				AddEquivalent(psState, 
							  psEquivSrcTree, 
							  &psCRegMoveInst->asDest[0], 
							  &psCRegMoveInst->asArg[0], 
							  &asSetArg[uArg],
							  psInst->uId);
			}
			
			/*
				Replace the source argument by the equivalent register.
			*/
			SetSrc(psState, psInst, uArgStart + uArg, USEASM_REGTYPE_TEMP, uNewSrcNums + uArg, eSrcFmt);

			/*
				Remove the equivalent from the list of registers containing a copy of another registers.
			*/
			if (psGroup->u.psCRegMove != NULL)
			{
				RemoveFromEquivalentList(psState, psGroup);
			}
		}

		/*
			Flag that the copies must have consecutive hardware register numbers.
		*/
		MakeGroup(psState, asSetArg, uArgCount, eArgAlign);
	}
	else
	{
		ASSERT(uBestReuseableSourceMask == ((1U << uArgCount) - 1));

		/*
			Update the arguments to the current instruction with the equivalent registers.
		*/
		for (psGroup = psBestBaseGroup, uArg = 0; uArg < uArgCount; psGroup = psGroup->psNext, uArg++)
		{
			if (asSetArg[uArg].uType == USEASM_REGTYPE_TEMP && asSetArg[uArg].uNumber == psGroup->uRegister)
			{
				continue;
			}

			/*
				Replace the source argument by the equivalent register.
			*/
			SetSrc(psState, psInst, uArgStart + uArg, USEASM_REGTYPE_TEMP, psGroup->uRegister, asSetArg[uArg].eFmt);
		}
	

		/*
			Updated the required hardware register alignment for the group.
		*/
		if (eArgAlign != HWREG_ALIGNMENT_NONE)
		{
			SetNodeAlignment(psState, psBestBaseGroup, eArgAlign, IMG_TRUE /* bAlignRequiredByInst */);
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID BaseMakeGroup(PINTERMEDIATE_STATE	psState,
					   IMG_UINT32			auSetArg[],
					   IMG_UINT32			uArgCount,
					   HWREG_ALIGNMENT		eAlignGroup)
/*****************************************************************************
 FUNCTION	: BaseMakeGroup

 PURPOSE	: Add restrictions on hardware register numbers for a group of
			  sources or destinations to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  auSetArg			- Sources or destination register numbers.
			  uArgCount			- Count of arguments in the count.
			  eAlignGroup		- Required alignment for the hardware register
								number assigned to the first argument in the
								group.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uArg;
	PREGISTER_GROUP	psPrevGroup;

	psPrevGroup = NULL;
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		PREGISTER_GROUP	psGroup;

		/*
			Add the data structure to record restrictions on the
			possible hardware register numbers for this argument.
		*/
		psGroup = AddRegisterGroup(psState, auSetArg[uArg]);

		if (uArg != 0)
		{
			IMG_BOOL bRet;

			/*
				Mark that this argument requires a hardware register number
				one more than the previous argument.
			*/
			bRet = AddToGroup(psState, 
							  auSetArg[uArg - 1],
							  psPrevGroup, 
							  auSetArg[uArg],
							  psGroup, 
							  IMG_TRUE /* bLinkedByInst */,
							  IMG_FALSE /* bOptional */);
			ASSERT(bRet);
		}
		else
		{
			/*	
				Set the required alignment of the hardware register numbers for nodes in
				the group.
			*/
			if (eAlignGroup != HWREG_ALIGNMENT_NONE)
			{
				SetNodeAlignment(psState, psGroup, eAlignGroup, IMG_TRUE /* bAlignRequiredByInst */);
			}
		}

		psPrevGroup = psGroup;
	}
}

IMG_INTERNAL
IMG_VOID MakeGroup(PINTERMEDIATE_STATE	psState,
				   ARG					asSetArg[],
				   IMG_UINT32			uArgCount,
				   HWREG_ALIGNMENT		eAlignGroup)
/*****************************************************************************
 FUNCTION	: MakeGroup

 PURPOSE	: Add restrictions on hardware register numbers for a group of
			  sources or destinations to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  asSetArg			- Sources or destinations.
			  uArgCount			- Count of arguments in the count.
			  eAlignGroup		- Required alignment for the hardware register
								number assigned to the first argument in
								the group.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	auSetArg[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	IMG_UINT32	uArg;

	ASSERT(uArgCount <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);

	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		ASSERT(asSetArg[uArg].uType == USEASM_REGTYPE_TEMP);
		auSetArg[uArg] = asSetArg[uArg].uNumber;
	}

	BaseMakeGroup(psState, auSetArg, uArgCount, eAlignGroup);
}

IMG_INTERNAL
IMG_VOID MakePartialDestGroup(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_UINT32			uGroupStart,
							  IMG_UINT32			uGroupCount,
							  HWREG_ALIGNMENT		eGroupAlign)
{
	IMG_UINT32	auSetArg[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	IMG_UINT32	uArg;

	ASSERT(uGroupCount <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);

	for (uArg = 0; uArg < uGroupCount; uArg++)
	{
		PARG	psPartialDest = psInst->apsOldDest[uGroupStart + uArg];

		ASSERT(psPartialDest != NULL);
		ASSERT(psPartialDest->uType == USEASM_REGTYPE_TEMP);
		auSetArg[uArg] = psPartialDest->uNumber;
	}

	BaseMakeGroup(psState, auSetArg, uGroupCount, eGroupAlign);
}

static
IMG_VOID CreateCRegReplacement(PINTERMEDIATE_STATE	psState,
							   PEQUIV_SRC_DATA		psEquivSrcData,
							   PINST				psInst,
							   PINST				psMOVInst,
							   IMG_UINT32			uMOVDestIdx,
							   IMG_UINT32			uReplacementTempNum,
							   IMG_UINT32			uMOVSrcIdx,
							   PARG					psSrc,
							   IMG_UINT32			uOrigSrcIdx)
/*****************************************************************************
 FUNCTION	: CreateCRegReplacement

 PURPOSE	: Set up the arguments to a MOV instruction to copy a source argument
			  where the source argument can't be used directly because of 
			  restrictions on hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Information about which registers contain
								copies of other registers.
			  psInst			- Instruction where the copy of the source
								argument will be used.
			  psMOVInst			- Copy instruction.
		      uMOVDestIdx		- Destination of the copy instruction.
			  uReplacementTempNum
								- Temporary register to copy into.
			  uMOVSrcIdx		- Source of the copy instruction.
			  psSrc				- The source argument to copy.
			  uOrigSrcIdx		- The instruction source argument to replace with 
								  the register containing the copy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eFmt = psInst->asArg[uOrigSrcIdx].eFmt;

	if (psMOVInst != NULL)
	{
		/*
			Copy the original source argument to the MOV instruction.
		*/
		SetSrcFromArg(psState, psMOVInst, uMOVSrcIdx, psSrc);

		/*
			Set the destination of the MOV to the newly allocated temporary.
		*/
		SetDest(psState, psMOVInst, uMOVDestIdx, USEASM_REGTYPE_TEMP, uReplacementTempNum, eFmt);

		/*
			Record that there is a register containing a copy of this source argument.
		*/
		if (psEquivSrcData != NULL)
		{
			AddEquivalent(psState, 
						  psEquivSrcData->psEquivSrc, 
						  &psMOVInst->asDest[uMOVDestIdx],
						  &psMOVInst->asArg[uMOVSrcIdx],
						  &psInst->asArg[uOrigSrcIdx],
						  psInst->uId);
		}
	}

	/*
		Replace the source argument by the new temporary.
	*/
	SetSrc(psState, psInst, uOrigSrcIdx, USEASM_REGTYPE_TEMP, uReplacementTempNum, eFmt);
}


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL SplitInstruction(PINTERMEDIATE_STATE		psState,
						  PINST						psInst,
						  PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: SplitInstruction

 PURPOSE	: Split an instruction to avoid alignment requirements on its destination.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction whose destinations are to be replaced.
			  psDetails		- Details of the subset to replace.

 RETURNS	: Whether the split was successful.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx, uDestIdx2;
	IMG_UINT32	uDestStart = psDetails->uStart;
	IMG_UINT32	uDestEnd = psDetails->uStart + psDetails->uCount;
	PINST		psPartInst[USC_MAXIMUM_REGISTER_GROUP_COUNT];

	ASSERT(psDetails->uCount > 1);
	
	if(psInst->eOpcode == IVDUAL)
	{
		return IMG_FALSE;
	}

	for (uDestIdx = uDestStart; uDestIdx < uDestEnd; uDestIdx++)
	{
		IMG_UINT32		uEffectiveDestMask;

		/*
			If this destintion is masked out then we can skip making a part instruction for it
		*/
		uEffectiveDestMask = psInst->auDestMask[uDestIdx] & psInst->auLiveChansInDest[uDestIdx];
		if (uEffectiveDestMask != 0)
		{
			IMG_UINT32	uArg;

			/*
				VECINST		DEST1+DEST2+DEST3+DEST4, (ARG1,ARG2,ARG3,ARG4).swizzle(..)
			    ->
				PARTINST	DEST1 (ARG1,ARG2,ARG3,ARG4).swizzle(..)
				PARTINST	DEST2 (ARG1,ARG2,ARG3,ARG4).swizzle(..)
				PARTINST	DEST3 (ARG1,ARG2,ARG3,ARG4).swizzle(..)
				PARTINST	DEST4 (ARG1,ARG2,ARG3,ARG4).swizzle(..)
			*/

			psPartInst[uDestIdx] = AllocateInst(psState, psInst);
			SetOpcode(psState, psPartInst[uDestIdx], psInst->eOpcode);
			CopyPredicate(psState, psPartInst[uDestIdx], psInst);

			MoveDest(psState, psPartInst[uDestIdx], 0 /* uMoveToDestIdx */, psInst, uDestIdx);
			CopyPartiallyWrittenDest(psState, psPartInst[uDestIdx], 0 /* uMoveToDestIdx */, psInst, uDestIdx);
			psPartInst[uDestIdx]->auDestMask[0] = psInst->auDestMask[uDestIdx];
			psPartInst[uDestIdx]->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDestIdx];

			for (uArg = 0; uArg < psInst->uArgumentCount; ++uArg)
			{
			    SetSrcFromArg(psState, psPartInst[uDestIdx], uArg, &psInst->asArg[uArg]);
			}

			*psPartInst[uDestIdx]->u.psVec = *psInst->u.psVec;
			
			if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) == 0)
			{
				for (uArg = 0; uArg < GetSwizzleSlotCount(psState, psInst); ++uArg)
				{
					IMG_UINT uMultiplier;
					IMG_UINT uChan;

					if (psPartInst[uDestIdx]->asDest[0].eFmt == UF_REGFORMAT_F32)
					{
						uMultiplier = 1;
					}
					else
					{
						uMultiplier = 2;
					}

					for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
					{
						IMG_UINT uShiftedChan = GetChan(psInst->u.psVec->auSwizzle[uArg], (uDestIdx*uMultiplier + uChan) % VECTOR_LENGTH);
						psPartInst[uDestIdx]->u.psVec->auSwizzle[uArg] = SetChan(psPartInst[uDestIdx]->u.psVec->auSwizzle[uArg], uChan, uShiftedChan);
					}
				}
			}

			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC)
			{
				DropUnusedVectorComponents(psState, psPartInst[uDestIdx], 0);
				if (GetLiveChansInArg(psState, psPartInst[uDestIdx], 0) == 0)
				{
					FixDroppedUnusedVectorComponents(psState, psPartInst[uDestIdx], 0);
				}
			}

			if (!IsSwizzleSetSupported(psState, 
									   psPartInst[uDestIdx]->eOpcode, 
									   psPartInst[uDestIdx], 
									   NULL /* auNewLiveChansInArg */,
									   psPartInst[uDestIdx]->u.psVec->auSwizzle))
			{
				/* Chuck the instructions already made */
				for (uDestIdx2 = uDestStart; uDestIdx2 <= uDestIdx; uDestIdx2++)
				{
					if (psPartInst[uDestIdx2] != NULL)
					{
						/* Move the destination back again */
						MoveDest(psState, psInst, uDestIdx2, psPartInst[uDestIdx2], 0 /* uMoveFromDestIdx */);
						FreeInst(psState, psPartInst[uDestIdx2]);
					}
				}

				/* Give up on splitting */
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Insert a MOV instruction to copy the source for the non-written channel to the
				original destination.
			*/
			if (psInst->apsOldDest[uDestIdx] != NULL)
			{
				psPartInst[uDestIdx] = AllocateInst(psState, psInst);
				SetOpcode(psState, psPartInst[uDestIdx], IMOV);
				CopyPredicate(psState, psPartInst[uDestIdx], psInst);
				MoveDest(psState, psPartInst[uDestIdx], 0 /* uMoveToDestIdx */, psInst, uDestIdx);
				MovePartialDestToSrc(psState, psPartInst[uDestIdx], 0 /* uMoveToSrcIdx */, psInst, uDestIdx);
			}	
			else
			{
				psPartInst[uDestIdx] = NULL;
			}
		}
	}

	/* Managed to split it fully. Now add the new instructions. */
	for (uDestIdx = uDestStart; uDestIdx < uDestEnd; uDestIdx++)
	{
		if (psPartInst[uDestIdx] != NULL)
		{
			InsertInstAfter(psState, psInst->psBlock, psPartInst[uDestIdx], psInst);
		}
	}

	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */


static
IMG_VOID ReplaceDestinationsByTemporaries(PINTERMEDIATE_STATE		psState,
										  PINST						psInst,
										  PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: ReplaceDestinationsByTemporaries

 PURPOSE	: Replace a subset of destination to an instruction by new registers and
			  insert MOVs from the new registers to the original arguments.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction whose destinations are to be replaced.
			  psDetails		- Details of the subset to replace.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uDestStart = psDetails->uStart;
	IMG_UINT32	uDestEnd = psDetails->uStart + psDetails->uCount;

	for (uDestIdx = uDestStart; uDestIdx < uDestEnd; uDestIdx++)
	{
		IMG_UINT32		uEffectiveDestMask;
		IMG_BOOL		bInsertMOV;
		PARG			psDest = &psInst->asDest[uDestIdx];
		UF_REGFORMAT	eDestFmt = psDest->eFmt;
		IMG_UINT32		uReplacementTemp;

		/*
			If this destintion is masked out then we can skip inserting the MOV.
		*/
		uEffectiveDestMask = psInst->auDestMask[uDestIdx] & psInst->auLiveChansInDest[uDestIdx];
		if (uEffectiveDestMask != 0)
		{
			bInsertMOV = IMG_TRUE;
		}
		else
		{
			bInsertMOV = IMG_FALSE;

			/*
				Insert a MOV instruction to copy the source for the non-written channel to the
				original destination.
			*/
			if (psInst->apsOldDest[uDestIdx] != NULL)
			{
				PINST	psMoveInst;

				psMoveInst = AllocateInst(psState, psInst);
				SetOpcode(psState, psMoveInst, IMOV);
				CopyPredicate(psState, psMoveInst, psInst);
				MoveDest(psState, psMoveInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
				MovePartialDestToSrc(psState, psMoveInst, 0 /* uMoveToSrcIdx */, psInst, uDestIdx);
				InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst->psNext);
			}	
		}

		/*
			Allocate a new register.
		*/
		uReplacementTemp = GetNextRegister(psState);

		/*
			Insert a MOV after the instruction to copy from the new register to the
			old destination.
		*/
		if (bInsertMOV)
		{
			PINST		psMoveInst;

			psMoveInst = AllocateInst(psState, psInst);

			if ((psInst->auLiveChansInDest[uDestIdx] & ~psInst->auDestMask[uDestIdx]) != 0)
			{
				MakeSOPWMMove(psState, psMoveInst);
				psMoveInst->auDestMask[0] = psInst->auDestMask[0];
			}
			else
			{
				SetOpcode(psState, psMoveInst, IMOV);
			}

			CopyPredicate(psState, psMoveInst, psInst);

			MoveDest(psState, psMoveInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
			CopyPartiallyWrittenDest(psState, psMoveInst, 0 /* uMoveToDestIdx */, psInst, uDestIdx);
			psMoveInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDestIdx];

			psInst->auLiveChansInDest[uDestIdx] = GetLiveChansInArg(psState, psMoveInst, 0 /* uArg */);
			SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psOldDest */);

			SetSrc(psState, psMoveInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uReplacementTemp, eDestFmt);

			InsertInstBefore(psState, psInst->psBlock, psMoveInst, psInst->psNext);
		}

		/*
			Replace the destination by the new register.
		*/
		SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_TEMP, uReplacementTemp, eDestFmt);
	}

	/*
		Flag the replacement destinations as requiring consecutive hardware register numbers.
	*/
	MakeGroup(psState, psInst->asDest + psDetails->uStart, psDetails->uCount, psDetails->eAlign);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
#define COPIES_PER_PASS				(2)
#define COPIES_PER_PASS_MASK		((1U << COPIES_PER_PASS) - 1)

static
IMG_VOID InsertVectorCopyInst(PINTERMEDIATE_STATE	psState,
							  PEQUIV_SRC_DATA		psEquivSrcData,
							  PCODEBLOCK			psInsertBlock,
							  PINST					psInsertPoint,
							  PINST					psInst,
							  IOPCODE				eCopyOpcode,
							  IMG_UINT32			uSwizzle,
							  IMG_UINT32			uCopyFromStart,
							  IMG_UINT32			auLiveChansInSrc[],
							  IMG_UINT32			uArgStride,
							  UF_REGFORMAT			eSrcFormat,
							  HWREG_ALIGNMENT		eRequiredAlignment,
							  IMG_UINT32			uBaseReplacementTempNum)
/*****************************************************************************
 FUNCTION	: InsertVectorCopyInst

 PURPOSE	: Create an instruction to copy two source arguments.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Updated with the newly added move instructions
			  psInsertBlock		- Block to insert the new instructions into.
			  psInsertPoint		- Existing instruction to insert the new instructions
								before.
			  psInst			- Instruction whose sources are to be copied.
			  eCopyOpcode		- Opcode of the copy instruction.
			  uSwizzle			- Swizzle to use for the instruction.
			  uCopyFromStart	- Start of the sources to copy.
			  auLiveChansInSrc	- Channels used from each source.
			  uArgStride		- Gap between the two source arguments to the
								copy instruction to use.
			  eSrcFormat		- Format of the data to copy.
			  eRequiredAlignment
								- Required aligment for the hardware register
								numbers assigned to the source arguments.
			  uBaseReplacementTempNum
								- Start of the temporary registers to copy into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psVMOVInst;
	IMG_UINT32	uArg;

	/*
		Allocate a VMOV instruction.
	*/
	psVMOVInst = AllocateInst(psState, psInst);
	SetOpcodeAndDestCount(psState, psVMOVInst, eCopyOpcode, COPIES_PER_PASS /* uDestCount */);
	for (uArg = 0; uArg < psVMOVInst->uArgumentCount; uArg++)
	{
		psVMOVInst->asArg[uArg].uType = USC_REGTYPE_UNUSEDSOURCE;
	}
	for (uArg = 0; uArg < COPIES_PER_PASS; uArg++)
	{
		IMG_UINT32	uCopySrc = uCopyFromStart + uArg;

		CreateCRegReplacement(psState, 
							  psEquivSrcData,
							  psInst, 
							  psVMOVInst,
							  uArg /* uMOVDest */,
							  uBaseReplacementTempNum + uArg,
							  1 + uArg * uArgStride /* uMOVSrc */,
							  &psInst->asArg[uCopySrc],
							  uCopySrc);
		psVMOVInst->auDestMask[uArg] = auLiveChansInSrc[uArg];
		psVMOVInst->auLiveChansInDest[uArg] = auLiveChansInSrc[uArg];
	}
	for (uArg = 0; uArg < GetSwizzleSlotCount(psState, psVMOVInst); uArg++)
	{
		psVMOVInst->u.psVec->auSwizzle[uArg] = USEASM_SWIZZLE(X, Y, Z, W);
		psVMOVInst->u.psVec->aeSrcFmt[uArg] = eSrcFormat;
	}
	if (eCopyOpcode == IVPCKFLTFLT_EXP)
	{
		psVMOVInst->u.psVec->uPackSwizzle = uSwizzle;
	}
	InsertInstBefore(psState, psInsertBlock, psVMOVInst, psInsertPoint);

	/*
		Flag that the original source arguments still require consecutive hardware
		register numbers because we've moved them from one vector instruction to
		another.
	*/
	if (eCopyOpcode == IVMOV && psVMOVInst->asArg[1].uType == USEASM_REGTYPE_TEMP)
	{
		MakeGroup(psState, 
				  psVMOVInst->asArg + 1, 
				  COPIES_PER_PASS, 
				  eRequiredAlignment);
	}

	/*
		Flag that the replacement arguments also require consecutive hardware register numbers.
	*/
	MakeGroup(psState, psVMOVInst->asDest, psVMOVInst->uDestCount,  HWREG_ALIGNMENT_EVEN);
}

static
IMG_VOID CreateVectorCopyInst(PINTERMEDIATE_STATE	psState,
							  PEQUIV_SRC_DATA		psEquivSrcData,
							  PCODEBLOCK			psInsertBlock,
							  PINST					psInsertPoint,
							  PINST					psInst,
							  PREGISTER_GROUP_DESC	psDetails,
							  IMG_UINT32			uInsertMoveMask,
							  IMG_PUINT32			puReplaceMask,
							  IMG_UINT32			auLiveChansInSrc[],
							  IMG_UINT32			uBaseReplacementTempNum)
/*****************************************************************************
 FUNCTION	: CreateVectorCopyInst

 PURPOSE	: If possible use vector move instructions when we need to copy
			  data to fix groups of source registers which must be consecutive.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Updated with the newly added move instructions
			  psInsertBlock		- Block to insert the new instructions into.
			  psInsertPoint		- Existing instruction to insert the new instructions
								before.
			  psInst			- Instruction whose sources are to be copied.
			  psDetails			- Details of the sources to copy.
			  uInsertMoveMask	- Mask of the sources to copy.
			  puReplaceMask		- Updated with details of the sources copied.
			  auLiveChansInSrc	- Channels used from each of the sources.
			  uBaseReplacementTempNum
								- Start of the temporary registers to copy into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uPass;
	UF_REGFORMAT	eSrcFormat = psInst->asArg[psDetails->uStart].eFmt;

	for (uPass = 0; uPass < (psDetails->uCount / COPIES_PER_PASS); uPass++)
	{
		IMG_UINT32		uPassCopyMask;
		PARG			apsChan[2];
		PREGISTER_GROUP	apsChanGroup[2];
		IMG_UINT32		uPassStart = uPass * COPIES_PER_PASS;
		IMG_UINT32		uArgStride;
		IMG_UINT32		uChan;
		IOPCODE			eCopyOpcode;
		IMG_UINT32		uSwizzle;
		HWREG_ALIGNMENT	eRequiredAlignment;

		uPassCopyMask = (uInsertMoveMask >> uPassStart) & COPIES_PER_PASS_MASK;

		/*
			Skip if we aren't copying two consecutive intermediate registers.
		*/
		if (uPassCopyMask != COPIES_PER_PASS_MASK)
		{
			continue;
		}

		/*
			Get the details of the two copy sources.
		*/
		for (uChan = 0; uChan < COPIES_PER_PASS; uChan++)
		{
			apsChan[uChan] = &psInst->asArg[psDetails->uStart + uPassStart + uChan];
			if (apsChan[uChan]->uType == USEASM_REGTYPE_TEMP)
			{	
				apsChanGroup[uChan] = FindRegisterGroup(psState, apsChan[uChan]->uNumber);
			}
			else
			{
				apsChanGroup[uChan] = NULL;
			}
		}

		/*
			Check for cases where we can use swizzles on the VMOV instruction to avoid requiring
			the hardware register numbers for the sources to have a specified alignment.
		*/
		eRequiredAlignment = HWREG_ALIGNMENT_NONE;
		if (eSrcFormat == UF_REGFORMAT_F16 && auLiveChansInSrc[uPassStart + 0] != 0 && auLiveChansInSrc[uPassStart + 1] != 0)
		{
			eRequiredAlignment = HWREG_ALIGNMENT_EVEN;
		}
	
		/*
			If the two copy sources are intermediate registers which will be assigned
			consecutive hardware registers then use a simple vector move.
		*/
		if (
				apsChanGroup[0] != NULL &&
				apsChanGroup[1] != NULL &&
				apsChanGroup[0] != apsChanGroup[1] && 
				ValidGroup(apsChanGroup, 2 /* uGroupCount */, eRequiredAlignment)
			)
		{
			eCopyOpcode = IVMOV;
			uArgStride = 1;
			uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
		}
		else
		{
			if (eSrcFormat != UF_REGFORMAT_F32)
			{
				continue;
			}

			/*
				Otherwise use an instruction which combines results from two
				different vectors.
			*/
			eCopyOpcode = IVPCKFLTFLT_EXP;
			uSwizzle = USEASM_SWIZZLE(X, Z, Z, W);
			uArgStride = SOURCE_ARGUMENTS_PER_VECTOR;
		}
				
		/*
			Create an instruction to copy two sources.
		*/
		InsertVectorCopyInst(psState,
							 psEquivSrcData,
							 psInsertBlock,
							 psInsertPoint,
							 psInst,
							 eCopyOpcode,
							 uSwizzle,
							 psDetails->uStart + uPassStart,
							 auLiveChansInSrc + uPassStart,
							 uArgStride,
							 eSrcFormat,
							 eRequiredAlignment,
							 uBaseReplacementTempNum + uPassStart);

		/*
			Remove the two sources from the mask of sources to
			copy using individual move instructions.
		*/
		(*puReplaceMask) &= ~(uPassCopyMask << uPassStart);
	}
}

static
IMG_VOID CreateSOPWMCopyInst(PINTERMEDIATE_STATE	psState,
							 PEQUIV_SRC_DATA		psEquivSrcData,
							 PCODEBLOCK				psInsertBlock,
							 PINST					psInsertPoint,
							 PINST					psInst,
							 PREGISTER_GROUP_DESC	psDetails,
							 IMG_UINT32				auLiveChansInSrc[],
							 IMG_UINT32				uBaseReplacementTempNum)
/*****************************************************************************
 FUNCTION	: CreateSOPWMCopyInst

 PURPOSE	: Move a set of C10 format sources from an instruction to a newly
			  allocated SOPWM instruction which copies them to new intermediate 
			  registers. Then replace the sources by the new intermediate registers.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Updated with the newly added move instructions
			  psInsertBlock		- Block to insert the new instructions into.
			  psInsertPoint		- Existing instruction to insert the new instructions
								before.
			  psInst			- Instruction whose sources are to be copied.
			  psDetails			- Details of the sources to copy.
			  auLiveChansInSrc	- Channels used from each of the sources.
			  uBaseReplacementTempNum
								- Start of the temporary registers to copy into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psSopwmInst;
	IMG_UINT32	uArg;

	/*
		Create a SOPWM instruction.
	*/
	psSopwmInst = AllocateInst(psState, psInst);
	SetOpcodeAndDestCount(psState, psSopwmInst, ISOPWM_VEC, 2 /* uDestCount */);
	for (uArg = 0; uArg < 2; uArg++)
	{
		IMG_UINT32	uCopySrc = psDetails->uStart + uArg;

		CreateCRegReplacement(psState, 
							  psEquivSrcData,
							  psInst, 
							  psSopwmInst,
							  uArg /* uMOVDestIdx */,
							  uBaseReplacementTempNum + uArg,
							  uArg /* uMOVSrcIdx */,
							  &psInst->asArg[uCopySrc],
							  uCopySrc);
		psSopwmInst->auDestMask[uArg] = psSopwmInst->auLiveChansInDest[uArg] = auLiveChansInSrc[uArg];
	}

	/*
		Set the other source to the SOPWM to #0.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		PARG	psSrcArg = &psSopwmInst->asArg[2 + uArg];

		psSrcArg->uType = USEASM_REGTYPE_IMMEDIATE;
		psSrcArg->uNumber = 0;
		psSrcArg->eFmt = UF_REGFORMAT_U8;
	}

	/*
		Make the SOPWM calculation
		DEST.RGB = SRC1.RGB * ONE + SRC2.RGB * ZERO
		DEST.ALPHA = SRC1.ALPHA * ONE * SRC2.ALPHA * ZERO
	*/
	psSopwmInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
	psSopwmInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
	psSopwmInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psSopwmInst->u.psSopWm->bComplementSel2 = IMG_TRUE;
	psSopwmInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
	psSopwmInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
	InsertInstBefore(psState, psInsertBlock, psSopwmInst, psInsertPoint);

	/*
		Flag that the original source arguments still require consecutive hardware
		register numbers because we've moved them from one vector instruction to
		another.
	*/
	if (psSopwmInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		MakeGroup(psState, 
				  psSopwmInst->asArg, 
				  2 /* uGroupCount */, 
				  HWREG_ALIGNMENT_EVEN);
	}

	/*
		Flag that the replacement arguments also require consecutive hardware register numbers.
	*/
	MakeGroup(psState, psSopwmInst->asDest, 2 /* uGroupCount */,  HWREG_ALIGNMENT_EVEN);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_BOOL AddMovesToSecondaryUpdateProgram(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_UINT32			uInsertMoveMask,
										  PREGISTER_GROUP_DESC	psDetails,
										  IMG_UINT32			auLiveChansInSrc[],
										  IMG_UINT32			uBaseReplacementTempNum)
/*****************************************************************************
 FUNCTION	: AddMovesToSecondaryUpdateProgram

 PURPOSE	: Checks if copies of a set of source arguments into new temporary 
			  registers can be inserted in the secondary update program.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction whose arguments are to be replaced.
			  uInsertMoveMask	- Mask of the sources to be copied.
			  psDetails			- Details of the sources to replace.
			  auLiveChansInSrc	- Masks of channels used from each source.
			  uBaseReplacementTempNum
								- Start of the range of temporary registers to copy
								into.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	/*
		Check the secondary attribute bank is supported by this instruction source.
	*/
	if (!CanUseSrc(psState, psInst, psDetails->uStart, USEASM_REGTYPE_SECATTR, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}

	if (psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		return IMG_FALSE;
	}

	/*
		Check we are allowed to move calculations to the secondary update
		program.
	*/
	if ((psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if there are enough secondary attributes to hold the result of the swizzle.
	*/
	if (!CheckAndUpdateInstSARegisterCount(psState, psDetails->uCount, psDetails->uCount, IMG_TRUE))
	{
		return IMG_FALSE;
	}

	for (uSrc = 0; uSrc < psDetails->uCount; uSrc++)
	{
		PARG		psSrc = &psInst->asArg[psDetails->uStart + uSrc];
		IMG_UINT32	uCopyDestTempNum = uBaseReplacementTempNum + uSrc;

		BaseAddNewSAProgResult(psState,
							   psSrc->eFmt,
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							   IMG_FALSE /* bVector */,
#endif //defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							   0 /* uHwRegAlignment */,
							   1 /* uNumHwRegisters */,
							   uCopyDestTempNum,
							   0 /* uUsedChanMask */,
							   SAPROG_RESULT_TYPE_CALC,
							   IMG_FALSE /* bPartOfRange */);

		if ((uInsertMoveMask & (1 << uSrc)) != 0)
		{
			SetRegisterLiveMask(psState,
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
								USEASM_REGTYPE_TEMP,
								uCopyDestTempNum,
								0 /* uArrayOffset */,
								auLiveChansInSrc[uSrc]);
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID ReplaceSourcesByTemporaries(PINTERMEDIATE_STATE		psState,
									 PEQUIV_SRC_DATA			psEquivSrcData,
									 PINST						psInst,
									 PREGISTER_GROUP_DESC		psDetails,
									 IMG_UINT32					uReplaceMask,
									 IMG_BOOL					bValidGroup)
/*****************************************************************************
 FUNCTION	: ReplaceSourcesByTemporaries

 PURPOSE	: Replace a subset of sources to an instruction by new registers and
			  insert MOVs to  the new registers to the original arguments.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Updated with the newly added move instructions.
			  psInst			- Instruction whose arguments are to be replaced.
			  psDetails			- Details of the sources to replace.
			  uReplaceMask		- 
			  bValidGroup		- If TRUE the arguments can be given consecutive hardware
								register numbers but have an invalid register bank for the
								current instruction.

 RETURNS	: TRUE if already set up temporary registers were reused to replace the
			  arguments.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uInsertMoveMask;
	IMG_UINT32	auLiveChansInSrc[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	PARG		asSetArg = psInst->asArg + psDetails->uStart;
	IMG_BOOL	bOutOfRangeImmediates;
	IMG_UINT32	uInsertMoveCount;
	IMG_BOOL	bAllUniform;
	IMG_UINT32	uBaseReplacementTempNum;
	PCODEBLOCK	psInsertBlock;
	PINST		psInsertPoint;

	PVR_UNREFERENCED_PARAMETER(bValidGroup);
	
	/*
		Check if we can reuse the results of MOV instructions inserted to fix previous
		instructions.
	*/
	if (psEquivSrcData != NULL &&
		CheckForEquivalents(psState, 
							psEquivSrcData->psEquivSrc, 
							psInst,
							asSetArg, 
							psDetails->uStart,
							psDetails->uCount, 
							psDetails->eAlign))
	{
		return;
	}

	/*
		Check which source arguments in the set are actually referenced by the instruction. We can
		avoid creating MOVs for them.
	*/
	uInsertMoveMask = 0;
	uInsertMoveCount = 0;
	bAllUniform = IMG_TRUE;
	bOutOfRangeImmediates = IMG_FALSE;
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		IMG_UINT32	uSrcIdx = psDetails->uStart + uArg;
		PARG		psSrc = &psInst->asArg[uSrcIdx];

		if ((uReplaceMask & (1U << uArg)) == 0)
		{
			continue;
		}

		auLiveChansInSrc[uArg] = GetLiveChansInArg(psState, psInst, uSrcIdx);
		if (auLiveChansInSrc[uArg] != 0)
		{
			uInsertMoveMask |= (1U << uArg);
			uInsertMoveCount++;

			if (!IsUniformSource(psState, psSrc))
			{
				bAllUniform = IMG_FALSE;
			}

			if (psSrc->uType == USEASM_REGTYPE_IMMEDIATE && psSrc->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
			{
				bOutOfRangeImmediates = IMG_TRUE;
			}
		}
	}

	/*
		Allocate new temporary registers to hold the copied source arguments.
	*/
	uBaseReplacementTempNum = GetNextRegisterCount(psState, psDetails->uCount);

	/*
		Check if we could insert the copy instructions into the secondary
		update program.
	*/
	psInsertBlock = psInst->psBlock;
	psInsertPoint = psInst;
	if (bAllUniform && uReplaceMask == (IMG_UINT32)((1U << psDetails->uCount) - 1))
	{
		if (AddMovesToSecondaryUpdateProgram(psState,
											 psInst,
											 uInsertMoveMask,
											 psDetails,
											 auLiveChansInSrc,
											 uBaseReplacementTempNum))
		{
			psInsertBlock = psState->psSecAttrProg->sCfg.psExit;
			psInsertPoint = NULL;
		}
	}

	/*
		Create MOV instructions to copy the original source arguments to new temporary registers and use the
		new temporary registers as replacement source arguments for the instruction requiring consecutive
		hardware registers.
	*/

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check for cases where we could use a vector instruction to move multiple dwords
		of data simultaneously.
	*/
	if (
			!bOutOfRangeImmediates &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
			uInsertMoveCount > 1
		)
	{
		/*
			Use SOPWM to copy C10 format data.
		*/
		if (bValidGroup && asSetArg[0].eFmt == UF_REGFORMAT_C10)
		{
			ASSERT(uInsertMoveMask == 3);

			CreateSOPWMCopyInst(psState, 
								psEquivSrcData, 
								psInsertBlock, 
								psInsertPoint, 
								psInst, 
								psDetails, 
								auLiveChansInSrc, 
								uBaseReplacementTempNum);
			return;
		}
	
		/*
			Use a vector MOV instruction for F16/F32 data.
		*/
		if (
				(
					asSetArg[0].eFmt == UF_REGFORMAT_F16 || 
					asSetArg[0].eFmt == UF_REGFORMAT_F32
				 ) &&
				 uInsertMoveCount > 1
		   )
		{
			CreateVectorCopyInst(psState, 
								 psEquivSrcData, 
								 psInsertBlock,
								 psInsertPoint,
								 psInst, 
								 psDetails, 
								 uInsertMoveMask,
								 &uReplaceMask,
								 auLiveChansInSrc,
								 uBaseReplacementTempNum);
		}
	}

	ASSERT(psInst->asArg[psDetails->uStart].uType != USEASM_REGTYPE_FPINTERNAL);

	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	/*
		Create a seperate scalar MOV instruction for each source argument.
	*/
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		PINST		psMoveInst;
		ARG			sSrc;
		IMG_UINT32	uReplacementTempNum = uBaseReplacementTempNum + uArg;

		if ((uReplaceMask & (1U << uArg)) == 0)
		{
			continue;
		}

		if ((uInsertMoveMask & (1U << uArg)) == 0)
		{
			CreateCRegReplacement(psState, 
								  psEquivSrcData,
								  psInst, 
								  NULL /* psMOVInst */,
								  USC_UNDEF /* uMOVDest */, 
								  uReplacementTempNum,
								  USC_UNDEF /* uMOVSrc */, 
								  NULL /* psSrc */,
								  psDetails->uStart + uArg);
			continue;
		}

		/*	
			Get the original source argument to copy.
		*/
		sSrc = psInst->asArg[psDetails->uStart + uArg];

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			In cores with the vector instruction set scalar instructions (like MOV) can't address
			all of the hardware constants. So replace a hardware constant by an immediate with
			the same value.
		*/
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			if (sSrc.uType == USEASM_REGTYPE_FPCONSTANT)
			{
				IMG_UINT32	uConstValue;

				uConstValue = GetHardwareConstantValueu(psState, sSrc.uNumber);

				InitInstArg(&sSrc);
				sSrc.uType = USEASM_REGTYPE_IMMEDIATE;
				sSrc.uNumber = uConstValue;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Create a move instruction copying the current source argument into a temporary register then
			replace the source argument by the temporary register.
		*/
		psMoveInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMoveInst, IMOV);
		CreateCRegReplacement(psState, 
							  psEquivSrcData,
							  psInst, 
							  psMoveInst,
							  0 /* uMOVDest */,
							  uReplacementTempNum,
							  0 /* uMOVSrc */,
							  &sSrc,
							  psDetails->uStart + uArg);
		InsertInstBefore(psState, psInsertBlock, psMoveInst, psInsertPoint);
	}

	MakeGroup(psState, asSetArg, psDetails->uCount, psDetails->eAlign);
}
static
IMG_BOOL ReplaceFPConstantsBySecAttrs(PINTERMEDIATE_STATE		psState,
									  PINST						psInst,
									  PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: ReplaceFPConstantsBySecAttrs

 PURPOSE	: Replace a set of floating point constant registers by
			  secondary attributes containing the same values.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction whose sources are to be replaced.
			  psDetails		- Details of the set to replace.

 RETURNS	: TRUE if the register numbers were made consecutive.
*****************************************************************************/
{
	IMG_UINT32			puValue[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];
	IMG_UINT32			uArg;
	PARG				asArgs = psInst->asArg + psDetails->uStart;
	IMG_BOOL			bAllImmedsSameVal = IMG_TRUE;
	IMG_UINT32			uConstsToAdd;
	IMG_UINT32			uOrigCount = psDetails->uCount;
	PINREGISTER_CONST	psFirstConst;
	PINREGISTER_CONST	psConst;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL			bSwizzRemapPossible = IMG_FALSE;
	#endif	/* (SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psDetails->uCount <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);

	if ((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if the instruction supports the secondary attribute register bank.
	*/
	if (!CanUseSrc(psState, psInst, psDetails->uStart, USEASM_REGTYPE_SECATTR, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}
	
	/*	
		Get the values of the constants.
	*/
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		PARG	psArg = &asArgs[uArg];

		if (psArg->uType == USEASM_REGTYPE_FPCONSTANT)
		{
			bAllImmedsSameVal = IMG_FALSE;
			puValue[uArg] = GetHardwareConstantValueu(psState, psArg->uNumber);
		}
		else if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			if(bAllImmedsSameVal == IMG_TRUE)
			{
				if((uArg > 0) && (psArg->uNumber != puValue[uArg-1]))
				{
					bAllImmedsSameVal = IMG_FALSE;
				}
			}
			puValue[uArg] = psArg->uNumber;
		}
		else
		{
			return IMG_FALSE;
		}
	}

	uConstsToAdd = psDetails->uCount;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if(
			(bAllImmedsSameVal == IMG_TRUE) &&
			(psDetails->uCount > 1) && 
			((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0) &&
			HasIndependentSourceSwizzles(psState, psInst)
	  )
	{	
		IMG_UINT32	uMatchedSwizzle;
		if(IsSwizzleSupported(psState, 
							  psInst, 
							  psInst->eOpcode, 
							  (psDetails->uStart - 1) / SOURCE_ARGUMENTS_PER_VECTOR, 
							  USEASM_SWIZZLE(X, X, X, X), 
							  USC_DESTMASK_FULL, 
							  &uMatchedSwizzle) == IMG_TRUE)
		{
			uConstsToAdd = 1;
			bSwizzRemapPossible = IMG_TRUE;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Try to add secondary attributes with the same values.
	*/
	if (!AddStaticSecAttrRange(psState, puValue, uConstsToAdd, NULL /* ppsFirstConst */, psDetails->eAlign))
	{
		return IMG_FALSE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (bSwizzRemapPossible == IMG_TRUE)
	{
		IMG_UINT32	uVecIdx;

		uVecIdx = (psDetails->uStart - 1) / SOURCE_ARGUMENTS_PER_VECTOR;
		psInst->u.psVec->auSwizzle[uVecIdx] = USEASM_SWIZZLE(X, X, X, X);

		psDetails->uStart = uVecIdx * SOURCE_ARGUMENTS_PER_VECTOR + 1;
		asArgs = psInst->asArg + psDetails->uStart;
		psDetails->uCount = 1;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	AddStaticSecAttrRange(psState, puValue, uConstsToAdd, &psFirstConst, psDetails->eAlign);

	/*
		Use the secondary attributes for the SMP instructions.
	*/
	psConst = psFirstConst;
	for (uArg = 0; uArg < uConstsToAdd; uArg++)
	{
		IMG_UINT32	uArgType, uArgNum;

		GetDriverLoadedConstantLocation(psState, psConst, &uArgType, &uArgNum);
		SetSrc(psState, psInst, psDetails->uStart + uArg, uArgType, uArgNum, asArgs[uArg].eFmt);

		psConst = IMG_CONTAINING_RECORD(psConst->sListEntry.psNext, PINREGISTER_CONST, sListEntry);
	}
	for (; uArg < uOrigCount; uArg++)
	{
		SetSrcUnused(psState, psInst, psDetails->uStart + uArg);	
	}

	/*
		Add the restrictions on the order/alignment of the secondary attributes.
	*/
	MakeGroup(psState, asArgs, uConstsToAdd, psDetails->eAlign);

	return IMG_TRUE;
}

static
IMG_BOOL TryFixNonConsecutiveFPConstants(PINTERMEDIATE_STATE		psState,
										 PINST						psInst,
										 PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: FixNonConsecutiveFPConstants

 PURPOSE	: Try to fix a group of registers from the floating point
			  constant bank to give them consecutive register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to which the constants are sources.
			  psDetails		- Details of the group to fix.

 RETURNS	: TRUE if the register numbers were made consecutive.
*****************************************************************************/
{
	IMG_UINT32	uRegNum;
	IMG_UINT32	uArg;
	PARG		asArgs = psInst->asArg + psDetails->uStart;

	if (psDetails->eAlign != HWREG_ALIGNMENT_NONE)
	{
		return IMG_FALSE;
	}

	/*
		Check if the floating point constant bank is supported by this instruction.
	*/
	if (!CanUseSrc(psState, psInst, psDetails->uStart, USEASM_REGTYPE_FPCONSTANT, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}

	/*
		Check if all the arguments are the same hardware
		constant.
	*/
	uRegNum = USC_UNDEF;
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		if (asArgs[uArg].uType != USEASM_REGTYPE_FPCONSTANT)
		{
			return IMG_FALSE;
		}
		if (uArg == 0)
		{
			uRegNum = asArgs[uArg].uNumber;
		}
		else
		{
			if (asArgs[uArg].uNumber != uRegNum)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		For the hardware constants ONE and ZERO the next
		three constants have the same value.
	*/
	if (uRegNum != EURASIA_USE_SPECIAL_CONSTANT_ZERO &&
		uRegNum != EURASIA_USE_SPECIAL_CONSTANT_FLOAT1)
	{
		return IMG_FALSE;
	}
	if (psDetails->uCount > 4)
	{
		return IMG_FALSE;
	}

	/*
		Renumber the arguments to give them consecutive register numbers.
	*/
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		asArgs[uArg].uNumber = uRegNum + uArg;
	}
	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
typedef struct _VECTOR_PERM
{
	IMG_UINT32	auPerm[4];
	IMG_UINT32	uSwizzle;
} VECTOR_PERM, *PVECTOR_PERM;
typedef VECTOR_PERM const* PCVECTOR_PERM;

static
IMG_VOID MakeVPCKFLTFLT_EXP(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
							IMG_UINT32			uPackSwizzle,
							PCVECTOR_PERM		psPerm,
							PARG				asNewArgs)
/*****************************************************************************
 FUNCTION	: MakeVPCKFLTFLT_EXP

 PURPOSE	: Convert a VMOV instruction to a VPCKFLTFLT_EXP.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to modify.
			  uPackSwizzle	- Swizzle to control which channels in the destination
							are copied from which source arguments.
			  psPerm		- Order of the source arguments for the new instruction.
			  asNewArgs		- Source arguments for the new instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	ModifyOpcode(psState, psInst, IVPCKFLTFLT_EXP);
	psInst->u.psVec->uPackSwizzle = CombineSwizzles(psPerm->uSwizzle, uPackSwizzle);

	for (uArg = 0; uArg < 2; uArg++)
	{
		IMG_UINT32			uArgBase = uArg * SOURCE_ARGUMENTS_PER_VECTOR;
		IMG_UINT32 const *	auOrder = psPerm->auPerm + uArg * 2;
		IMG_UINT32			uChan;

		for (uChan = 0; uChan < 2; uChan++)
		{
			IMG_UINT32	uSrc;

			uSrc = auOrder[uChan];
			if (uSrc == USC_UNDEF)
			{
				SetSrcUnused(psState, psInst, uArgBase + 1 + uChan);
				break;
			}
			else
			{
				SetSrcFromArg(psState, psInst, uArgBase + 1 + uChan, &asNewArgs[uSrc]);
			}
		}

		if (uChan == 2)
		{
			MakeGroup(psState, psInst->asArg + uArgBase + 1, 2 /* uArgCount */, HWREG_ALIGNMENT_EVEN);
		}

		SetSrcUnused(psState, psInst, uArgBase);
		for (; uChan < VECTOR_LENGTH; uChan++)
		{
			SetSrcUnused(psState, psInst, uArgBase + 1 + uChan);
		}

		psInst->u.psVec->auSwizzle[uArg] = USEASM_SWIZZLE(X, Y, Z, W);
		psInst->u.psVec->aeSrcFmt[uArg] = UF_REGFORMAT_F32;
	}
}

static
IMG_BOOL ValidPair(IMG_UINT32 const* auValidPairings, IMG_UINT32 const* auPair)
{
	if ((auValidPairings[auPair[0]] & (1U << auPair[1])) != 0)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL SetPartialDestToFixSourceGroup(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SetPartialDestToFixSourceGroup

 PURPOSE	: Try to set the old dest of a VMOV instruction so one arguments can be
			  removed, and therefore the sources can be correctly aligned.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to try and fix.

 RETURNS	: TRUE if the instruction was fixed.
*****************************************************************************/
{
	IMG_UINT32 uChan;

	/*
		Convert

			VMOV	A, {B, C}.XYZW
		->
			VMOV	A.XY[=C], {B, __}.XY

		In other words rather than copying the contents of C from a source argument use it as the source
		for a partially written destination.
	*/

	if (psInst->uDestCount != 1 || psInst->apsOldDest[0] != NULL)
	{
		return IMG_FALSE;
	}
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F16)
	{
		return IMG_FALSE;
	}

	/*
		Check the second source argument isn't swizzled.
	*/
	for (uChan = 0; uChan < F16_CHANNELS_PER_SCALAR_REGISTER; ++uChan)
	{
		USEASM_SWIZZLE_SEL	eSel = GetChan(psInst->u.psVec->auSwizzle[0], uChan);
		USEASM_SWIZZLE_SEL	eChanSel = g_aeChanToSwizzleSel[F16_CHANNELS_PER_SCALAR_REGISTER + uChan];

		if (eSel == USEASM_SWIZZLE_SEL_Z || eSel == USEASM_SWIZZLE_SEL_W)
		{
			if (eSel != eChanSel)
			{
				return IMG_FALSE;
			}
		}
	}

	SetPartialDest(psState, psInst, 0, psInst->asArg[2].uType, psInst->asArg[2].uNumber, psInst->asArg[2].eFmt);
	SetSrcUnused(psState, psInst, 2);

	for (uChan = 0; uChan < VECTOR_LENGTH; ++uChan)
	{
		USEASM_SWIZZLE_SEL eSel = GetChan(psInst->u.psVec->auSwizzle[0], uChan);

		if (eSel == USEASM_SWIZZLE_SEL_Z || eSel == USEASM_SWIZZLE_SEL_W)
		{
			psInst->auDestMask[0] &= ~(USC_XY_CHAN_MASK << (uChan * WORD_SIZE));
		}
	}
	
	MakeGroupsForInstSources(psState, psInst);

	return IMG_TRUE;
}

static
IMG_BOOL FixVMOVSourceGroup(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_GROUP_DESC	psDetails)
/*****************************************************************************
 FUNCTION	: FixVMOVSourceGroup

 PURPOSE	: Try to modify a VMOV instruction so its source arguments can be
			  given correctly aligned and ordered hardware register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to try and fix.
			  psDetails		- Details of the group of VMOV source arguments
							which must be given consecutive hardware register
							numbers.

 RETURNS	: TRUE if the instruction was fixed.
*****************************************************************************/
{
	PARG			asSetArg = psInst->asArg + psDetails->uStart;
	IMG_UINT32		uChansUsedPreSwizzle;
	PREGISTER_GROUP	apsGroup[VECTOR_LENGTH];
	ARG				asSquashedArgs[VECTOR_LENGTH];
	IMG_UINT32		uSquashSwizzle;
	IMG_UINT32		uArg;
	IMG_UINT32		uNewCount;
	IMG_UINT32		uNewSwizzle;

	ASSERT(psDetails->uStart == 1);
	ASSERT(psDetails->uCount <= VECTOR_LENGTH);

	/*
		Get the mask of channels referenced from the subset with and without applying the swizzle.
	*/
	GetLiveChansInSourceSlot(psState, psInst, 0 /* uSlotIdx */, &uChansUsedPreSwizzle, NULL /* puChansUsedPostSwizzle */);
	
	if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16)
	{
		return SetPartialDestToFixSourceGroup(psState, psInst);
	}

	if (psInst->u.psVec->aeSrcFmt[0] != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}

	/*	
		Get information about any existing restrictions on the possible hardware register numbers
		for the source arguments.
	*/
	uNewCount = 0;
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		PARG	psSrc;

		psSrc = asSetArg + uArg;
		if (psSrc->uType != USEASM_REGTYPE_TEMP)
		{
			return IMG_FALSE;
		}
		if (GetLiveChansInArg(psState, psInst, psDetails->uStart + uArg) == 0)
		{
			continue;
		}
		apsGroup[uNewCount] = FindRegisterGroup(psState, psSrc->uNumber);
		uNewCount++;
	}

	/*
		Remove any duplicates from the argument list by creating a new instruction swizzle.
	*/
	uSquashSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
	uNewCount = 0;
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		IMG_UINT32	uPrevArg;

		if (GetLiveChansInArg(psState, psInst, psDetails->uStart + uArg) == 0)
		{
			continue;
		}
		for (uPrevArg = 0; uPrevArg < uNewCount; uPrevArg++)
		{
			if (EqualArgs(asSetArg + uArg, asSquashedArgs + uPrevArg))
			{
				break;
			}
		}
		if (uPrevArg == uNewCount)
		{
			asSquashedArgs[uNewCount] = asSetArg[uArg];
			uNewCount++;
		}
		uSquashSwizzle = SetChan(uSquashSwizzle, uArg, g_aeChanToSwizzleSel[uPrevArg]);
	}
	uNewSwizzle = CombineSwizzles(uSquashSwizzle, psInst->u.psVec->auSwizzle[0]);

	if (uNewCount == 2)
	{
		static const VECTOR_PERM sTwoPerm = {{0, USC_UNDEF, 1, USC_UNDEF}, USEASM_SWIZZLE(X, Z, X, X)};
			
		/*
			For a VMOV with two scalar arguments convert

				VMOV		DEST, {SRC0, SRC1, UNUSED, UNUSED}
			->
				VPCKFLTFLT	DEST, {SRC0, UNUSED, UNUSED, UNUSED}, {SRC1, UNUSED, UNUSED, UNUSED}

			Neither of the VPCKFLTFLT source arguments have any restrictions on their hardware register
			number if 
		*/
		MakeVPCKFLTFLT_EXP(psState, 
						   psInst, 
						   uNewSwizzle,
						   &sTwoPerm,
						   asSquashedArgs);
		return IMG_TRUE;
	}
	else if (uNewCount == 3)
	{
		static const VECTOR_PERM asThreePerms[] = 
		{
			{{0, 1, 2, USC_UNDEF}, USEASM_SWIZZLE(X, Y, Z, X)},
			{{0, 2, 1, USC_UNDEF}, USEASM_SWIZZLE(X, Z, Y, X)},
			{{1, 0, 2, USC_UNDEF}, USEASM_SWIZZLE(Y, X, Z, X)},
			{{1, 2, 0, USC_UNDEF}, USEASM_SWIZZLE(Z, X, Y, X)},
			{{2, 0, 1, USC_UNDEF}, USEASM_SWIZZLE(Y, Z, X, X)},
			{{2, 1, 0, USC_UNDEF}, USEASM_SWIZZLE(Z, Y, X, X)},
		};
		IMG_UINT32 uPerm;

		/*
			Check if any two of the source arguments can be given consecutive hardware registers with the
			first one aligned.
		*/
		for (uPerm = 0; uPerm < (sizeof(asThreePerms) / sizeof(asThreePerms[0])); uPerm++)
		{
			PCVECTOR_PERM psPerm = &asThreePerms[uPerm];

			for (uArg = 0; uArg < 3; uArg++)
			{
				PREGISTER_GROUP	apsFirstPair[2];

				apsFirstPair[0] = apsGroup[psPerm->auPerm[0]];
				apsFirstPair[1] = apsGroup[psPerm->auPerm[1]];
				if (ValidGroup(apsFirstPair, 2 /* uGroupCount */, HWREG_ALIGNMENT_EVEN))
				{
					/*
						Convert
							VMOV		DEST, {SRC0, SRC1, SRC2, UNUSED}
						->
							VPCKFLTFLT	DEST, {SRC0, SRC1, UNUSED, UNUSED}, {SRC2, UNUSED, UNUSED, UNUSEC}
					*/
					MakeVPCKFLTFLT_EXP(psState, 
									   psInst, 
									   uNewSwizzle,
									   psPerm,
									   asSquashedArgs);
					return IMG_TRUE;
				}
			}
		}
	}
	else if (psDetails->uCount == 4)
	{
		IMG_UINT32	auValidPairings[VECTOR_LENGTH];		
		static const VECTOR_PERM asFourPerms[] = 
		{
			{{0, 1, 2, 3}, USEASM_SWIZZLE(X, Y, Z, W)},
			{{0, 1, 3, 2}, USEASM_SWIZZLE(X, Y, W, Z)},
			{{0, 2, 1, 3}, USEASM_SWIZZLE(X, Z, Y, W)},
			{{0, 2, 3, 1}, USEASM_SWIZZLE(X, W, Y, Z)},
			{{0, 3, 1, 2}, USEASM_SWIZZLE(X, Z, W, Y)},
			{{0, 3, 2, 1}, USEASM_SWIZZLE(X, W, Z, Y)},
			{{1, 0, 2, 3}, USEASM_SWIZZLE(Y, X, Z, W)},
			{{1, 0, 3, 2}, USEASM_SWIZZLE(Y, X, W, Z)},
			{{1, 2, 0, 3}, USEASM_SWIZZLE(Z, X, Y, W)},
			{{1, 2, 3, 0}, USEASM_SWIZZLE(W, X, Y, Z)},
			{{1, 3, 0, 2}, USEASM_SWIZZLE(Z, X, W, Y)},
			{{1, 3, 2, 0}, USEASM_SWIZZLE(W, X, Z, Y)},
			{{2, 0, 1, 3}, USEASM_SWIZZLE(Y, Z, X, W)},
			{{2, 0, 3, 1}, USEASM_SWIZZLE(Y, W, X, Z)},
			{{2, 1, 0, 3}, USEASM_SWIZZLE(Z, Y, X, W)},
			{{2, 1, 3, 0}, USEASM_SWIZZLE(W, Y, X, Z)},
			{{2, 3, 0, 1}, USEASM_SWIZZLE(Z, W, X, Y)},
			{{2, 3, 1, 0}, USEASM_SWIZZLE(W, Z, X, Y)},
			{{3, 0, 1, 2}, USEASM_SWIZZLE(Y, Z, W, X)},
			{{3, 0, 2, 1}, USEASM_SWIZZLE(Y, W, Z, X)},
			{{3, 1, 0, 2}, USEASM_SWIZZLE(Z, Y, W, X)},
			{{3, 1, 2, 0}, USEASM_SWIZZLE(W, Y, Z, X)},
			{{3, 2, 0, 1}, USEASM_SWIZZLE(Z, W, Y, X)},
			{{3, 2, 1, 0}, USEASM_SWIZZLE(W, Z, Y, X)},
		};
		IMG_UINT32			uPerm;

		/*
			Calculate which pairs of source arguments can be given consecutive hardware register numbers.
		*/
		for (uArg = 0; uArg < 4; uArg++)
		{
			PREGISTER_GROUP	apsPairGroup[2];
			IMG_UINT32		uOtherArg;

			apsPairGroup[0] = apsGroup[uArg];
			auValidPairings[uArg] = 0;
			for (uOtherArg = 0; uOtherArg < 4; uOtherArg++)
			{
				if (uOtherArg == uArg)
				{
					continue;
				}
				apsPairGroup[1] = apsGroup[uOtherArg];
				if (!ValidGroup(apsPairGroup, 2 /* uGroupCount */, HWREG_ALIGNMENT_EVEN))
				{
					continue;
				}
				auValidPairings[uArg] |= (1U << uOtherArg);
			}
		}

		/*
			Check if there are any partitions of the source arguments into two pairs where both of the
			arguments in each pair can be given consecutive hardware register numbers.
		*/
		for (uPerm = 0; uPerm < (sizeof(asFourPerms) / sizeof(asFourPerms[0])); uPerm++)
		{
			PCVECTOR_PERM psPerm = &asFourPerms[uPerm];
			for (uArg = 0; uArg < uNewCount; uArg++)
			{
				if (ValidPair(auValidPairings, psPerm->auPerm + 0) && ValidPair(auValidPairings, psPerm->auPerm + 2))
				{
					MakeVPCKFLTFLT_EXP(psState, 
									   psInst, 
									   uNewSwizzle,
									   psPerm,
									   asSquashedArgs);
					return IMG_TRUE;
				}
			}
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL UpdateSwizzleToMatchExistingGroup(PINTERMEDIATE_STATE		psState,
										   PINST					psInst,
										   PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: UpdateSwizzleToMatchExistingGroup

 PURPOSE    : Where some of the arguments to an instruction have existing
			  restrictions on their hardware register numbers which are 
			  incompatible with the restrictions required by this instruction
			  try modifying the instruction's swizzle to remove the incompatibility.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to update.
			  psDetails	- Details of the arguments.

 RETURNS	: TRUE if the instruction was updated so the subset is now valid.
*****************************************************************************/
{
	IMG_INT32		aiSel[VECTOR_LENGTH];
	IMG_INT32		iMinSel, iMaxSel;
	IMG_UINT32		uSwizzle;
	PREGISTER_GROUP	psMinSelGroup;
	PREGISTER_GROUP	psBaseSelGroup;
	IMG_UINT32		uFoundChanMask;
	IMG_UINT32		uVecArgIdx;
	IMG_UINT32		uOffsetInVec;
	HWREG_ALIGNMENT	eCorrectAlignment, eWrongAlignment;
	IMG_UINT32		uChan;
	IMG_UINT32		uDir;
	PREGISTER_GROUP	psGroup;
	IMG_INT32		iSelAdjust;
	IMG_UINT32		uArg;
	IMG_UINT32		uMaxSel;
	PARG			asSetArg = psInst->asArg + psDetails->uStart;
	PREGISTER_GROUP	psFirstArgGroup;
	IMG_UINT32		uBaseArgIdx;

	/*
		Skip instructions without source swizzles.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Get the swizzle slot that the register group corresponds to.
	*/
	ASSERT(psDetails->uStart >= 1);
	uVecArgIdx = (psDetails->uStart - 1) / SOURCE_ARGUMENTS_PER_VECTOR;
	uOffsetInVec = (psDetails->uStart - 1) % SOURCE_ARGUMENTS_PER_VECTOR;

	if (psInst->u.psVec->aeSrcFmt[uVecArgIdx] != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}

	if (asSetArg[0].uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	psFirstArgGroup = FindRegisterGroup(psState, asSetArg[0].uNumber);

	/*
		Check the hardware register bank of the group is supported by the instruction.
	*/
	if (!CanUseIntermediateSource(psState, psInst, psDetails->uStart, psFirstArgGroup))
	{
		return IMG_FALSE;
	}

	/*
		If the existing restriction information is NULL then the group must be invalid because some
		arguments occur more than once. Check if we can fix this by using a different swizzle.
	*/
	if (psFirstArgGroup == NULL)
	{
		IMG_UINT32	uArg;
		IMG_UINT32	uReplicateSwizzle;
		IMG_UINT32	uReplicateArgMask;
		IMG_UINT32	uNewSwizzle;
		IMG_UINT32	uNewArgCount;

		uReplicateSwizzle = 0;
		uReplicateArgMask = 0;
		for (uArg = 0; uArg < psDetails->uCount; uArg++)
		{
			PARG		psArg = &asSetArg[uArg];
			IMG_UINT32	uPrevArg;

			/*
				Fail if some arguments have already been used in
				an instruction with restrictions on its arguments.
			*/
			if (psArg->uType != USEASM_REGTYPE_TEMP)
			{
				return IMG_FALSE;
			}
			psGroup = FindRegisterGroup(psState, psArg->uNumber);
			if (psGroup != NULL)
			{
				return IMG_FALSE;
			}

			/*
				Check if this argument is the same as a previous argument.
			*/
			ASSERT(psArg->uType == USEASM_REGTYPE_TEMP);
			for (uPrevArg = 0; uPrevArg < uArg; uPrevArg++)
			{
				PARG	psPrevArg = &asSetArg[uPrevArg];

				ASSERT(psPrevArg->uType == USEASM_REGTYPE_TEMP);
				if (psPrevArg->uNumber == psArg->uNumber)
				{
					uReplicateArgMask |= (1U << uArg);
					break;
				}
			}

			/*	
				Create a new swizzle which selects the previous arguments into this channel.
			*/
			uReplicateSwizzle = SetChan(uReplicateSwizzle, uOffsetInVec + uArg, uOffsetInVec + uPrevArg);
		}

		/*
			Combine the new swizzle with the existing instruction swizzle.
		*/
		uNewSwizzle = CombineSwizzles(uReplicateSwizzle, psInst->u.psVec->auSwizzle[uVecArgIdx]);

		/*	
			Check if the resulting swizzle is supported by the instruction.
		*/
		if (!IsModifiedSwizzleSetSupported(psState,
										   psInst,
										   psInst->eOpcode,
										   uVecArgIdx,
										   uNewSwizzle,
										   &uNewSwizzle))
		{
			return IMG_FALSE;
		}

		/*
			Reduce the size of the subset to just the non-replicated arguments.
		*/
		uNewArgCount = psDetails->uCount;
		while (uNewArgCount > 0)
		{
			if ((uReplicateArgMask & (1U << (uNewArgCount - 1))) == 0)
			{
				break;
			}
			SetSrcUnused(psState, psInst, psDetails->uStart + uNewArgCount - 1);
			uNewArgCount--;
		}

		/*
			Replace arguments which were the same as earlier arguments by
			fresh temporaries. We updated the swizzle to these arguments aren't
			actually referenced by the instruction.
		*/
		for (uArg = 0; uArg < uNewArgCount; uArg++)
		{
			PARG	psArg = &asSetArg[uArg];

			if ((uReplicateArgMask & (1U << uArg)) != 0)
			{
				SetSrc(psState, 
					   psInst, 
					   psDetails->uStart + uArg, 
					   USEASM_REGTYPE_TEMP, 
					   GetNextRegister(psState),
					   psArg->eFmt);
			}
		}

		/*
			Update the instruction swizzle.
		*/
		psInst->u.psVec->auSwizzle[uVecArgIdx] = uNewSwizzle;

		/*
			Flag that the arguments require consecutive hardware register numbers.
		*/
		MakeGroup(psState,
				  psInst->asArg + psDetails->uStart,
				  uNewArgCount,
				  psDetails->eAlign);

		return IMG_TRUE;
	}

	/*
		Check if the source arguments are already in a group but a different
		order.
	*/

	aiSel[0] = 0;
	iMinSel = 0;
	psMinSelGroup = psFirstArgGroup;
	uFoundChanMask = (1U << 0);

	/*
		Check for channels which are the same as the first argument.
	*/
	for (uChan = 0; uChan < psDetails->uCount; uChan++)
	{
		if (asSetArg[uChan].uType != USEASM_REGTYPE_TEMP)
		{
			return IMG_FALSE;
		}
		if (asSetArg[uChan].uNumber == asSetArg[0].uNumber)
		{
			aiSel[uChan] = 0;
			uFoundChanMask |= (1U << uChan);
		}
		else
		{
			aiSel[uChan] = -1;
		}
	}

	iMaxSel = 0;

	for (uDir = 0; uDir < 2; uDir++)
	{
		IMG_INT32	iIdx;

		psGroup = psFirstArgGroup;
		iIdx = 0;

		for (;;)
		{
			/*
				Move to the next variable with a consecutive register number.
			*/
			if (uDir == 0)
			{
				psGroup = psGroup->psNext;
				iIdx++;
			}
			else
			{
				psGroup = psGroup->psPrev;
				iIdx--;
			}
			if (psGroup == NULL || iIdx <= -(IMG_INT32)VECTOR_LENGTH || iIdx >= VECTOR_LENGTH)
			{
				break;
			}

			/*
				Check if the variable is one of the source arguments in the current group.
			*/
			for (uChan = 0; uChan < psDetails->uCount; uChan++)
			{
				if (psGroup->uRegister == asSetArg[uChan].uNumber)
				{
					ASSERT(aiSel[uChan] == -1);
					aiSel[uChan] = iIdx;
					uFoundChanMask |= (1U << uChan);

					if (iIdx < iMinSel)
					{
						/*
							Record the source argument which has to be given the lowest hardware
							register number.
						*/
						iMinSel = iIdx;
						psMinSelGroup = psGroup;
					}

					iMaxSel = max(iMaxSel, iIdx);
				}
			}
		}
	}

	/*
		Check if the difference between the register numbers accessed is larger than
		that supported by the hardware.
	*/
	if ((iMaxSel - iMinSel + 1) > VECTOR_LENGTH)
	{
		return IMG_FALSE;
	}
	if (uFoundChanMask != (IMG_UINT32)((1U << psDetails->uCount) - 1))
	{
		return IMG_FALSE;
	}

	ASSERT(iMinSel <= 0);

	iSelAdjust = -iMinSel;
	psBaseSelGroup = psMinSelGroup;

	if ((uOffsetInVec % 2) == 0)
	{
		eCorrectAlignment = HWREG_ALIGNMENT_EVEN;
		eWrongAlignment = HWREG_ALIGNMENT_ODD;
	}
	else
	{
		eCorrectAlignment = HWREG_ALIGNMENT_ODD;
		eWrongAlignment = HWREG_ALIGNMENT_EVEN;
	}

	if (psMinSelGroup->eAlign == eWrongAlignment)
	{
		iSelAdjust++;
		psBaseSelGroup = psMinSelGroup->psPrev;
		if (psBaseSelGroup == NULL)
		{
			return IMG_FALSE;
		}
	}

	uMaxSel = iMaxSel + iSelAdjust;
	/*
		Check if the maximum register offset required is within the range
		supported by the hardware.
	*/
	if (uMaxSel > USC_W_CHAN)
	{
		return IMG_FALSE;
	}
	/*
		If the register offset is larger than 64-bits check the instruction supports accessing
		a 128-bit vector from the unified store in this source slot.
	*/
	if (uMaxSel >= USC_Z_CHAN)
	{
		IMG_UINT32	uWideSourceMask;

		uWideSourceMask = GetWideSourceMask(psInst->eOpcode);
		if ((uWideSourceMask & (1U << uVecArgIdx)) == 0)
		{
			return IMG_FALSE;
		}
	}

	/*
		Create a new swizzle so the source arguments can be replaced in an order compatible with the instructions
		already referencing them.
	*/
	uSwizzle = 0;
	for (uChan = 0; uChan < psDetails->uCount; uChan++)
	{
		IMG_INT32	iSel;

		iSel = aiSel[uChan] + iSelAdjust;
		ASSERT(iSel >= 0);
		ASSERT(iSel <= VECTOR_LENGTH);
		
		uSwizzle = SetChan(uSwizzle, uOffsetInVec + uChan, USEASM_SWIZZLE_SEL_X + iSel);
	}

	/*
		Combine the swizzle for the reordered argument with the existing swizzle.
	*/
	uSwizzle = CombineSwizzles(uSwizzle, psInst->u.psVec->auSwizzle[uVecArgIdx]);

	/*
		Check if the swizzle is supported by the instruction.
	*/
	if (!IsModifiedSwizzleSetSupported(psState,
									   psInst,
									   psInst->eOpcode,
									   uVecArgIdx,
									   uSwizzle,
									   &uSwizzle))
	{
		return IMG_FALSE;
	}
	psInst->u.psVec->auSwizzle[uVecArgIdx] = uSwizzle;

	SetNodeAlignment(psState, psBaseSelGroup, HWREG_ALIGNMENT_EVEN, IMG_TRUE /* bAlignRequiredByInst */);

	/*
		Copy the existing group in the order of the hardware register numbers over the
		existing source arguments.
	*/
	uBaseArgIdx = VEC_MOESOURCE_ARGINDEX(uVecArgIdx);
	for (uArg = 0, psGroup = psBaseSelGroup; uArg <= uMaxSel; uArg++, psGroup = psGroup->psNext)
	{
		PARG	psArg = &psInst->asArg[uBaseArgIdx + uArg];

		SetSrc(psState, psInst, uBaseArgIdx + uArg, USEASM_REGTYPE_TEMP, psGroup->uRegister, psArg->eFmt);
	}
	/*
		Clear any unreferenced parts of the source arguments.
	*/
	for (; uArg < VECTOR_LENGTH; uArg++)
	{
		SetSrcUnused(psState, psInst, uBaseArgIdx + uArg);
	}
	
	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_INT32 EquivalentSourceCmp(IMG_PVOID pvE1, IMG_PVOID pvE2)
/*****************************************************************************
 FUNCTION	: EquivalentSourceCmp

 PURPOSE    : Comparison function for the tree maps registers to other
			  registers containing the same data.

 PARAMETERS	: pvE1, pvE2	- The element to compare.

 RETURNS	: E1 < E2 
*****************************************************************************/
{
	PEQUIVALENT_SOURCE	psE1 = (PEQUIVALENT_SOURCE)pvE1;
	PEQUIVALENT_SOURCE	psE2 = (PEQUIVALENT_SOURCE)pvE2;

	if (psE1->uSourceType == psE2->uSourceType)
	{
		return (IMG_INT32)(psE1->uSourceNum - psE2->uSourceNum);
	}
	else
	{
		return (IMG_INT32)(psE1->uSourceType - psE2->uSourceType);
	}
}

static
IMG_BOOL IsConsecutiveNonTemps(PINTERMEDIATE_STATE	psState,
							   ARG					asSetArg[],
							   IMG_UINT32			uArgCount,
							   HWREG_ALIGNMENT		eAlignGroup)
/*****************************************************************************
 FUNCTION	: IsConsecutiveNonTemps

 PURPOSE    : Check if a group of non-temporary registers are valid as sources
			  to an instruction which requires its arguments to have consecutive
			  hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  asSetArg			- Arguments to check.
			  uArgCount			- Count of arguments to check.
			  eAlignGroup		- Required alignment for hardware register number
								assigned to the first argument in the group..

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uRegType = USC_UNDEF;
	IMG_UINT32	uFirstRegNum = USC_UNDEF;
	IMG_UINT32	uArgIdx;

	for (uArgIdx = 0; uArgIdx < uArgCount; uArgIdx++)
	{
		PARG	psArg = &asSetArg[uArgIdx];

		/*
			Check for an instruction which requires the first register in the
			group to have an aligned hardware register number.
		*/
		if (uArgIdx == 0 && eAlignGroup != HWREG_ALIGNMENT_NONE)
		{
			IMG_UINT32	uRequiredAlignment;

			switch (eAlignGroup)
			{
				case HWREG_ALIGNMENT_EVEN: uRequiredAlignment = 0; break;
				case HWREG_ALIGNMENT_ODD: uRequiredAlignment = 1; break;
				default: imgabort();
			}

			if ((uArgIdx == 0) && (psArg->uType != USEASM_REGTYPE_IMMEDIATE) && ((psArg->uNumber % 2) != uRequiredAlignment))
			{
				return IMG_FALSE;
			}
		}

		if (uArgIdx == 0)
		{
			uRegType = psArg->uType;
			uFirstRegNum = psArg->uNumber;
		}
		else
		{
			/*
				Check for a mix of different register types in this group.
			*/
			if (psArg->uType != uRegType)
			{
				return IMG_FALSE;
			}

			/*
				Check for using a mix of different index registers.
			*/
			if (psArg->uIndexType != asSetArg[0].uIndexType || psArg->uIndexNumber != asSetArg[0].uIndexNumber)
			{
				return IMG_FALSE;
			}

			/*
				Check for non-consecutive register numbers in this group.
			*/
			if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
			{
				if (psArg->uNumber != uFirstRegNum)
				{
					return IMG_FALSE;
				}
			}
			else if (psArg->uType == USC_REGTYPE_REGARRAY)
			{
				/* Check all the arguments belong to the same array. */
				if (psArg->uNumber != uFirstRegNum)
				{
					return IMG_FALSE;
				}
				/* Check the arguments use consecutive offsets into the array. */
				if (psArg->uArrayOffset != (asSetArg[0].uArrayOffset + uArgIdx))
				{
					return IMG_FALSE;
				}
			}
			else
			{
				if (psArg->uNumber != (uFirstRegNum + uArgIdx))
				{
					return IMG_FALSE;
				}
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_VOID IsValidNonTempGroup(PINTERMEDIATE_STATE	psState,
							 PINST					psInst,
							 IMG_UINT32				uBankCheckArg,
							 PARG					asSetArg,
							 IMG_UINT32				uArgCount,
							 HWREG_ALIGNMENT		eAlignGroup,
							 IMG_PBOOL				pbValidGroup,
							 IMG_PBOOL				pbValidBank)
/*****************************************************************************
 FUNCTION	: IsValidNonTempGroup

 PURPOSE    : Check if a group of non-temporary registers are valid as sources
			  to an instruction which requires its arguments to have consecutive
			  hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uBankCheckArg		- Index of the arguments to use to check
								whether the register bank of group is supported
								by the instruction.
			  asSetArg			- Arguments to check.
			  uArgCount			- Count of arguments to check.
			  eAlignGroup		- Required alignment for hardware register number
								assigned to the first argument in the group.
			  pbValidGroup		- Returns TRUE if the arguments have correct
								register numbers.
			  pbValidBank		- Returns TRUE if the arguments have a register bank
								supported by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	*pbValidGroup = IsConsecutiveNonTemps(psState, asSetArg, uArgCount, eAlignGroup);
	if ((*pbValidGroup))
	{
		IMG_UINT32	uHwRegisterType;

		*pbValidBank = IMG_TRUE;
		uHwRegisterType = GetPrecolouredRegisterType(psState, psInst, asSetArg[0].uType, asSetArg[0].uNumber);
		if (!CanUseSrc(psState, psInst, uBankCheckArg, uHwRegisterType, asSetArg[0].uIndexType))
		{
			*pbValidBank = IMG_FALSE;
			return;
		}
		if (asSetArg[0].uType == USEASM_REGTYPE_IMMEDIATE)
		{
			if (!IsImmediateSourceValid(psState, 
										psInst, 
										uBankCheckArg, 
										GetComponentSelect(psState, psInst, uBankCheckArg), 
										asSetArg[0].uNumber))
			{
				*pbValidBank = IMG_FALSE;
				return;
			}
		}
	}
	else
	{
		*pbValidBank = IMG_FALSE;
	}
}


static
IMG_VOID IsValidTempGroup(PINTERMEDIATE_STATE	psState,
						  PINST					psInst,
						  IMG_UINT32			uBankCheckArg,
						  PARG					asSetArg,
						  IMG_UINT32			uArgCount,
						  HWREG_ALIGNMENT		eAlignGroup,
						  IMG_PBOOL				pbValidGroup,
						  IMG_PBOOL				pbValidBank)
/*****************************************************************************
 FUNCTION	: IsValidTempGroup

 PURPOSE    : Check if a group of temporary registers are valid as sources
			  to an instruction which requires its arguments to have consecutive
			  hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uBankCheckArg		- Index of the arguments to use to check
								whether the register bank of group is supported
								by the instruction.
			  asSetArg			- Arguments to check.
			  uArgCount			- Count of arguments to check.
			  eAlignGroup		- Required alignment for the hardware register
								assigned to the first argument in the group.
			  pbValidGroup		- Returns TRUE if the arguments have correct
								register numbers.
			  pbValidBank		- Returns TRUE if the arguments have a register bank
								supported by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Check if we can ask the register allocator to give the arguments consecutive
		hardware register numbers. This might not be possible if they have already
		been used in a different order in another instruction.
	*/
	*pbValidGroup = IsValidGroup(psState, 
								 psInst, 
								 uArgCount, 
								 asSetArg, 
								 eAlignGroup,
								 IMG_FALSE /* bReplaceNonTempsByMOVs */);
	if ((*pbValidGroup))
	{
		IMG_UINT32		uRegType;

		/*
			Check if the hardware register bank of these arguments is supported
			by the instruction.
		*/
		uRegType = GetPrecolouredRegisterType(psState, psInst, asSetArg[0].uType, asSetArg[0].uNumber);
		if (CanUseSrc(psState, psInst, uBankCheckArg, uRegType, asSetArg[0].uIndexType))
		{
			*pbValidBank = IMG_TRUE;
		}
		else
		{
			*pbValidBank = IMG_FALSE;
		}
	}
	else
	{
		*pbValidBank = IMG_FALSE;
	}
}

static
IMG_VOID IsValidRegisterGroup(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_UINT32			uBankCheckArg,
							  PARG					asSetArg,
							  IMG_UINT32			uArgCount,
							  HWREG_ALIGNMENT		eAlignGroup,
							  IMG_PBOOL				pbValidGroup,
							  IMG_PBOOL				pbValidBank)
/*****************************************************************************
 FUNCTION	: IsValidRegisterGroup

 PURPOSE    : Check if a group of registers are valid as sources
			  to an instruction which requires its arguments to have consecutive
			  hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uBankCheckArg		- Index of the arguments to use to check
								whether the register bank of group is supported
								by the instruction.
			  asSetArg			- Arguments to check.
			  uArgCount			- Count of arguments to check.
			  eAlignGroup		- Required alignment for hardware register number
								assigned to the first argument in the group.
			  pbValidGroup		- Returns TRUE if the arguments have correct
								register numbers.
			  pbValidBank		- Returns TRUE if the arguments have a register bank
								supported by the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (asSetArg[0].uType == USEASM_REGTYPE_TEMP)
	{
		IsValidTempGroup(psState, 
						 psInst, 
						 uBankCheckArg, 
						 asSetArg, 
						 uArgCount,
						 eAlignGroup,
						 pbValidGroup,
						 pbValidBank);
	}
	else
	{
		IsValidNonTempGroup(psState, 
							psInst, 
							uBankCheckArg, 
							asSetArg, 
							uArgCount,
							eAlignGroup,
							pbValidGroup,
							pbValidBank);
	}
}

IMG_INTERNAL
IMG_BOOL IsValidArgumentSet(PINTERMEDIATE_STATE		psState,
							PARG					apsSet[],
							IMG_UINT32				uCount,
							HWREG_ALIGNMENT			eAlign)
/*****************************************************************************
 FUNCTION	: IsValidArgumentSet

 PURPOSE	: Check if a group of registers would be valid as sources or
			  destinations to an instruction which requires its sources or
			  destinations to have consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  apsSet			- Arguments to check.
			  uCount			- Count of arguments.
			  eAlign			- Required alignment of the hardware register
								number assigned to the first argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uGroupIdx;
	ARG			asSet[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];

	if (uCount == 0)
	{
		return IMG_TRUE;
	}

	ASSERT(uCount < USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);
	for (uGroupIdx = 0; uGroupIdx < uCount; uGroupIdx++)
	{
		asSet[uGroupIdx] = *apsSet[uGroupIdx];
	}

	if (apsSet[0]->uType == USEASM_REGTYPE_TEMP)
	{
		return IsConsecutiveTemps(psState, 
								  uCount, 
								  asSet, 
								  eAlign, 
								  IMG_FALSE /* bReplaceNonTempsByMOVs */, 
								  NULL /* ppsFirstNode */, 
								  NULL /* ppsLastNode */);
	}
	else
	{
		return IsConsecutiveNonTemps(psState, asSet, uCount, eAlign);
	}
}

IMG_INTERNAL
PEQUIV_SRC_DATA CreateRegisterGroupsContext(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: CreateRegisterGroupsContext

 PURPOSE	: Create state to track we can reuse MOVs inserted to fix incompatible
			  restrictions on hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
	
 RETURNS	: The created context.
*****************************************************************************/
{
	PEQUIV_SRC_DATA	psData;

	psData = UscAlloc(psState, sizeof(*psData));
	psData->psEquivSrc = UscTreeMake(psState, sizeof(EQUIVALENT_SOURCE), EquivalentSourceCmp);
	return psData;
}

static
IMG_VOID DeleteEquivSrc(IMG_PVOID pvState, IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: DeleteEquivSrc

 PURPOSE	: Delete an element from the tree of MOVs.

 PARAMETERS	: psState			- Compiler state.
			  pvElem			- Element to delete.
	
 RETURNS	: The created context.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = (PINTERMEDIATE_STATE)pvState;
	PEQUIVALENT_SOURCE	psElem = (PEQUIVALENT_SOURCE)pvElem;
	PUSC_LIST_ENTRY		psListEntry;
	PUSC_LIST_ENTRY		psNextListEntry;

	for (psListEntry = psElem->sCRegMoveList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PCREG_MOVE	psCRegMove = IMG_CONTAINING_RECORD(psListEntry, PCREG_MOVE, sListEntry);

		psNextListEntry = psListEntry->psNext;

		ASSERT(psCRegMove->psSource == psElem);

		ASSERT(psCRegMove->psDest->u.psCRegMove == psCRegMove);
		psCRegMove->psDest->u.psCRegMove = NULL;

		UscFree(psState, psCRegMove);
	}
}

IMG_INTERNAL
IMG_VOID DestroyRegisterGroupsContext(PINTERMEDIATE_STATE psState, PEQUIV_SRC_DATA psData)
/*****************************************************************************
 FUNCTION	: CreateRegisterGroupsContext

 PURPOSE	: Destroy state created to track we can reuse MOVs inserted to fix incompatible
			  restrictions on hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psData			- Context to destroy.
	
 RETURNS	: Nothing.
*****************************************************************************/
{
	UscTreeDeleteAll(psState, psData->psEquivSrc, DeleteEquivSrc, (IMG_PVOID)psState /* pvIterData */);
	UscFree(psState, psData);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static const IMG_UINT32 g_auLeftShiftSwizzles[] = 
{
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(0, X, Y, Z),
	USEASM_SWIZZLE(0, 0, X, Y),
	USEASM_SWIZZLE(0, 0, 0, X),
};
static const IMG_UINT32 g_auRightShiftSwizzles[] = 
{
	USEASM_SWIZZLE(X, Y, Z, W),
	USEASM_SWIZZLE(Y, Z, W, 0),
	USEASM_SWIZZLE(Z, W, 0, 0),
	USEASM_SWIZZLE(W, 0, 0, 0),
};

static
IMG_UINT32 GetShiftedSwizzle(IMG_INT32 iShift, IMG_UINT32 uOrigSwizzle)
/*****************************************************************************
 FUNCTION	: GetShiftedSwizzle

 PURPOSE	: Create a new swizzle which shifts all the channels in a register
			  up or down before applying the original swizzle.

 PARAMETERS	: iShift		- Distance to shift the swizzles.
			  uOrigSwizzle	- Original swizzle.
	
 RETURNS	: The swizzle which includes the shift.
*****************************************************************************/
{
	IMG_UINT32	uShiftSwizzle;

	if (iShift < 0)
	{
		uShiftSwizzle = g_auRightShiftSwizzles[-iShift];
	}
	else
	{
		uShiftSwizzle = g_auLeftShiftSwizzles[iShift];
	}
	return CombineSwizzles(uShiftSwizzle, uOrigSwizzle);
}

static
IMG_BOOL IsShiftedSwizzleSupported(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_UINT32			uDestArgIdx,
								   IMG_UINT32			uSrcArgIdx,
								   IMG_INT32			iShift,
								   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: IsShiftedSwizzleSupported

 PURPOSE	: Check if an instruction supports a swizzle which shifts the
			  source channels accessed either up or down.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check for.
			  uDestArgIdx		- Index of the source slot to check.
			  uSrcArgIdx		- Index of the source slot containing the swizzle
								to shift.
			  iShift			- Shift to check.
			  bCheckOnly		- If FALSE update the instruction swizzle with the
								shifted swizzle.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uShiftedSwizzle;
	IMG_UINT32	uNewSwizzle;
	IMG_BOOL	bRet;

	uShiftedSwizzle = GetShiftedSwizzle(iShift, psInst->u.psVec->auSwizzle[uSrcArgIdx]);
	bRet = IsModifiedSwizzleSetSupported(psState,
										 psInst,
										 psInst->eOpcode,
										 uDestArgIdx,
										 uShiftedSwizzle,
										 &uNewSwizzle);
	if (!bCheckOnly)
	{
		ASSERT(bRet);
		psInst->u.psVec->auSwizzle[uDestArgIdx] = uNewSwizzle;
	}
	return bRet;
}	

/*
	Map a nibble to first/last set bit in its bit pattern.
*/
										/* 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
static const IMG_UINT32	auFirstSetBit[] = {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};
static const IMG_UINT32	auLastSetBit[] =  {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};

static
IMG_VOID FixConsecutiveRegisterSwizzlesCB(PINTERMEDIATE_STATE	psState,
										  PINST					psInst,
										  IMG_BOOL				bDest,
										  IMG_UINT32			uArgStart,
										  IMG_UINT32			uArgCount,
										  HWREG_ALIGNMENT		eArgAlign,
										  IMG_PVOID				pvNULL)
/*****************************************************************************
 FUNCTION	: FixConsecutiveRegisterSwizzlesCB

 PURPOSE	: Where an instruction supports multiple source swizzles update the
			  swizzles depending on the alignment of the hardware register numbers
			  of a group of arguments.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to fix.
			  bDest				- TRUE if the arguments are destinations.
								  FALSE if the arguments are sources.
			  uArgStart			- Start of the group in the destinations or
								sources array.
			  uArgCount			- Count of arguments in the array.
			  eArgAlign			- Required alignment for the hardware register
								number of the first argument in the group.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uChansUsedPostSwizzle;
	IMG_INT32		iSwizzleShift;
	IMG_INT32		iArgShift;
	IMG_UINT32		uVecArgIdx;
	IMG_UINT32		uFirstUsedChan;
	IMG_UINT32		uLastUsedChan;
	IMG_UINT32		uFirstUsedArg;
	IMG_UINT32		uLastUsedArg;
	PARG			psFirstUsedChan;
	IMG_UINT32		uChan;
	UF_REGFORMAT	eSrcFmt;
	IMG_UINT32		uBaseRegNum;
	IMG_BOOL		bArgAlign;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	ASSERT(!bDest);

	/*
		Skip empty subsets.
	*/
	if (uArgCount == 0 || (uArgCount == 1 && eArgAlign == HWREG_ALIGNMENT_NONE))
	{
		return;
	}

	/*
		Skip instructions without source swizzles.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
	{
		return;
	}

	/*
		Immediate sources don't have alignment restrictions.
	*/
	if (psInst->asArg[uArgStart].uType == USEASM_REGTYPE_IMMEDIATE)
	{
		return;
	}

	/*
		Skip sets of sources which haven't been assigned hardware registers.
	*/
	if (psInst->psBlock->psOwner->psFunc != psState->psSecAttrProg)
	{
		PARG			psArgStart;
		PVREGISTER		psArgStartRegister;

		psArgStart = &psInst->asArg[uArgStart];
		psArgStartRegister = psArgStart->psRegister;

		/*
			Check for a temporary which will be coloured to a secondary attribute.
		*/
		if (
				psArgStartRegister != NULL && 
				psArgStartRegister->psGroup != NULL &&
				psArgStartRegister->psGroup->psFixedReg != NULL &&
				!psArgStartRegister->psGroup->psFixedReg->bPrimary
			)
		{
			return;
		}

		/*
			Check for an indexable array which will be coloured to secondary attributes.
		*/
		if (psArgStart->uType == USC_REGTYPE_REGARRAY)
		{
			PUSC_VEC_ARRAY_REG	psArray;

			ASSERT(psArgStart->uNumber < psState->uNumVecArrayRegs);
			psArray = psState->apsVecArrayReg[psArgStart->uNumber];

			if (psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR || 
				psArray->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR)
			{
				return;
			}
		}
	}

	switch (eArgAlign)
	{
		case HWREG_ALIGNMENT_ODD:	return;
		case HWREG_ALIGNMENT_EVEN:	bArgAlign = IMG_TRUE; break;
		case HWREG_ALIGNMENT_NONE:	bArgAlign = IMG_FALSE; break;
		default: imgabort();
	}

	ASSERT(uArgStart >= 1);
	ASSERT(((uArgStart - 1) % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uVecArgIdx = (uArgStart - 1) / SOURCE_ARGUMENTS_PER_VECTOR;

	eSrcFmt = psInst->u.psVec->aeSrcFmt[uVecArgIdx];
	if (eSrcFmt == UF_REGFORMAT_C10)
	{
		return;
	}
	ASSERT(eSrcFmt == UF_REGFORMAT_F16 || eSrcFmt == UF_REGFORMAT_F32);

	/*
		Get the mask of channels referenced from the subset with and without applying the swizzle.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uVecArgIdx, NULL /* puChansUsedPreSwizzle */, &uChansUsedPostSwizzle);

	/*
		Get the range of channel referenced.
	*/
	uFirstUsedChan = auFirstSetBit[uChansUsedPostSwizzle];
	uLastUsedChan = auLastSetBit[uChansUsedPostSwizzle];

	/*
		Convert to a range of source arguments referenced.
	*/
	if (eSrcFmt == UF_REGFORMAT_F16)
	{
		uFirstUsedArg = uFirstUsedChan / 2;
		uLastUsedArg = uLastUsedChan / 2;
	}
	else
	{
		uFirstUsedArg = uFirstUsedChan;
		uLastUsedArg = uLastUsedChan;
	}
	psFirstUsedChan = &psInst->asArg[uArgStart + uFirstUsedArg];

	/*
		Get the hardware register number assigned to the first referenced argument.
	*/
	if (psFirstUsedChan->uType == USC_REGTYPE_REGARRAY)
	{
		uBaseRegNum = psFirstUsedChan->uArrayOffset;
		ASSERT(psFirstUsedChan->uNumber < psState->uNumVecArrayRegs);
		uBaseRegNum += psState->apsVecArrayReg[psFirstUsedChan->uNumber]->uBaseReg;
	}
	else
	{
		uBaseRegNum = psFirstUsedChan->uNumber;
	}

	iArgShift = iSwizzleShift = 0;
	if (eSrcFmt == UF_REGFORMAT_F16)
	{
		if ((uBaseRegNum % 2) != (uFirstUsedArg % 2))
		{
			if (uFirstUsedChan <= USC_Y_CHAN)
			{
				/*
					Shift
						(r1, __) -> (__, r1)
					and change selectors in the swizzle to access Z/W instead of X/Y.
				*/
				ASSERT(uLastUsedChan <= USC_Y_CHAN);
				iSwizzleShift = -2;
				iArgShift = -1;
			}
			else
			{
				/*
					Shift
						(__, r0) -> (r0, __)
						and change selectors in the swizzle to access X/Y instead of Z/W.
				*/
				ASSERT(uFirstUsedChan >= USC_Z_CHAN);
				ASSERT(uLastUsedChan <= USC_W_CHAN);
				iSwizzleShift = 2;
				iArgShift = 1;
			}
		}
	}
	else
	{
		if (uFirstUsedChan == USC_X_CHAN)
		{
			/*
				If the first argument has the wrong alignment then subtract 1 from all the
				register numbers in the subset and increase all the swizzle selectors by
				one channel e.g.

				(r1, r2, r3, r4).xyz_		->  (r0, r1, r2, r3).yzw_
			*/
			if (bArgAlign && (uBaseRegNum % 2) != 0)
			{
				ASSERT(uLastUsedChan < (VECTOR_LENGTH - 1));
				iSwizzleShift = iArgShift = -1;
			}
		}
		else
		{
			IMG_BOOL	bMisaligned;

			/*
				Check for the wrong alignment of the hardware register number of the
				first argument.
			*/
			bMisaligned = IMG_FALSE;
			if ((uBaseRegNum % 2) != (uFirstUsedChan % 2))
			{
				bMisaligned = IMG_TRUE;
			}

			/*
				Modify the swizzles and the argument numbers so the register group is valid e.g.

				(_, _, r0, r1).zw__   -> (r0, r1, __, __).xy__
	
				or
	
				(_, _, r1, r2).zw__	  -> (r0, r1, r2, __).yz__
			*/
			if (uBaseRegNum < uFirstUsedChan || bMisaligned)
			{
				iSwizzleShift = uFirstUsedChan;
				if ((uFirstUsedChan % 2) == 0)
				{
					if (bMisaligned)
					{
						iSwizzleShift--;
					}
				}
				else
				{
					if (!bMisaligned)
					{
						iSwizzleShift--;
					}
				}
				iArgShift = iSwizzleShift;
			}
		}
	}

	if (iSwizzleShift != 0)
	{
		IsShiftedSwizzleSupported(psState,
								  psInst,
								  uVecArgIdx,
								  uVecArgIdx,
								  iSwizzleShift,
								  IMG_FALSE /* bCheckOnly */);
		
		if (psInst->asArg[uArgStart + uFirstUsedArg].uType == USEASM_REGTYPE_FPINTERNAL)
		{
			IMG_UINT32 uArg;
			for (uArg = uArgStart + uFirstUsedArg - iArgShift; uArg < uArgStart - iArgShift + uLastUsedArg + 1; ++uArg)
			{
				CopySrc(psState, psInst, uArg, psInst, uArgStart + uFirstUsedArg);
			}
		}
		else
		{
			memmove(&psInst->asArg[uArgStart + uFirstUsedArg - iArgShift],
					&psInst->asArg[uArgStart + uFirstUsedArg],
					(uLastUsedArg - uFirstUsedArg + 1) * sizeof(psInst->asArg[0]));
		}

		uFirstUsedArg = uFirstUsedArg - iArgShift;
		uLastUsedArg = uLastUsedArg - iArgShift;
	}

	if (uFirstUsedArg > 0)
	{
		/*
			Fill out unreferenced parts of the subset with hardware registers which
			are consecutive with the existing arguments. This makes it easiest for later
			phases which only need to look at the first arguments in the subset.
		*/
		for (uChan = 0; uChan < uFirstUsedArg; uChan++)
		{
			PARG		psChanArg = &psInst->asArg[uArgStart + uChan];
			IMG_UINT32	uRegNumAdjust = (uFirstUsedArg - uChan);
			
			if (psInst->asArg[uArgStart + uFirstUsedArg].uType == USEASM_REGTYPE_FPINTERNAL)
			{
				SetSrc(psState,
					   psInst, 
					   uArgStart + uChan, 
					   USEASM_REGTYPE_FPINTERNAL, 
					   psInst->asArg[uArgStart + uFirstUsedArg].uNumber - uRegNumAdjust, 
					   psInst->asArg[uArgStart + uFirstUsedArg].eFmt);
			}
			else
			{
				*psChanArg = psInst->asArg[uArgStart + uFirstUsedArg];
				if (psChanArg->uType == USC_REGTYPE_REGARRAY)
				{
					psChanArg->uArrayOffset -= uRegNumAdjust;
				}
				else
				{
					psChanArg->uNumber -= uRegNumAdjust;
				}
			}
		}
	}
	if ((uLastUsedArg + 1) < VECTOR_LENGTH)
	{
		/*
			Reset any source arguments in the now unreferenced tail of the subset.
		*/
		for (uChan = uLastUsedArg + 1; uChan < VECTOR_LENGTH; uChan++)
		{
			SetSrcUnused(psState, psInst, uArgStart + uChan);
		}
	}
}

IMG_INTERNAL
IMG_VOID FixConsecutiveRegisterSwizzlesInst(PINTERMEDIATE_STATE psState,
											PINST				psInst)
/*****************************************************************************
 FUNCTION	: FixConsecutiveRegisterSwizzlesInst

 PURPOSE	: Where an instruction supports multiple source swizzles update the
			  swizzles depending on the alignment of the hardware register numbers
			  of the sources.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0)
	{
		IMG_BOOL	bAddEvenDest = IMG_FALSE;

		/*
			Update an instruction which we treated as writing a single register with
			an odd aligned register number so it writes the current destination in the
			Y channel.
		*/
		if (psInst->u.psVec->bWriteYOnly)
		{
			ASSERT(psInst->uDestCount == 2);
			ASSERT(psInst->auDestMask[0] == 0);
			ASSERT(psInst->asDest[0].uType == USC_REGTYPE_UNUSEDDEST);
	
			psInst->u.psVec->bWriteYOnly = IMG_FALSE;

			bAddEvenDest = IMG_TRUE;
		}
		/*
			For an instruction which generate a single channel result replicated to all written
			channels in the destination there are no alignment restrictions if only a single
			32-bit register is written. If we ended up actually assigned an odd hardware register
			number then update the instruction to move the odd register to the Y channel.
		*/
		else if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0)
		{
			if ((psInst->asDest[0].uNumber % 2) == 1)
			{
				ASSERT(psInst->uDestCount == 1);

				SetDestCount(psState, psInst, 2 /* uDestCount */);
				MoveDest(psState, psInst, 1 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
				CopyPartiallyWrittenDest(psState, psInst, 1 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
				psInst->auDestMask[1] = psInst->auDestMask[0];
				psInst->auLiveChansInDest[1] = psInst->auLiveChansInDest[0];

				psInst->auDestMask[0] = 0;
				psInst->auLiveChansInDest[0] = 0;

				bAddEvenDest = IMG_TRUE;
			}
		}

		/*
			Add the previous even aligned hardware register as the first destination.
		*/
		if (bAddEvenDest)
		{
			PARG	psSecondDest;

			psSecondDest = &psInst->asDest[1];
			ASSERT((psSecondDest->uNumber % 2) == 1);

			SetDest(psState, psInst, 0 /* uDestIdx */, psSecondDest->uType, psSecondDest->uNumber - 1, psSecondDest->eFmt);
			SetPartialDest(psState, psInst, 0 /* uDestIdx */, psSecondDest->uType, psSecondDest->uNumber - 1, psSecondDest->eFmt);
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ProcessSourceRegisterGroups(psState, psInst, FixConsecutiveRegisterSwizzlesCB, NULL);
}

typedef struct _SWIZZLE_SHIFT
{
	IMG_UINT32	uShiftInRegs;
	IMG_UINT32	uShiftInChans;
	IMG_UINT32	uUsedChanCount;
} SWIZZLE_SHIFT, *PSWIZZLE_SHIFT;

static
IMG_VOID GetSwizzleShift(PINTERMEDIATE_STATE	psState,
						 PINST					psInst,
						 IMG_UINT32				uSwizzleSlot,
						 PSWIZZLE_SHIFT			psShift)
/*****************************************************************************
 FUNCTION	: GetSwizzleShift

 PURPOSE	: Check for a source vector where an initial part of the vector
			  is unreferenced.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSwizzleSlot		- Source argument to check.
			  psShift			- Returns details of the unreferenced part.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uFirstUsedChan, uLastUsedChan;
	IMG_UINT32		uChansUsedPostSwizzle;
	UF_REGFORMAT	eSrcFmt = psInst->u.psVec->aeSrcFmt[uSwizzleSlot];
	IMG_UINT32		uVecArgStart = uSwizzleSlot * SOURCE_ARGUMENTS_PER_VECTOR + 1;

	/*
		Get the mask of channels referenced from the subset with and without applying the swizzle.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uSwizzleSlot, NULL /* puChansUsedPreSwizzle */, &uChansUsedPostSwizzle);

	/*
		Get the range of arguments referenced.
	*/
	uFirstUsedChan = auFirstSetBit[uChansUsedPostSwizzle];
	uLastUsedChan = auLastSetBit[uChansUsedPostSwizzle];
	psShift->uUsedChanCount = uLastUsedChan - uFirstUsedChan + 1;

	/*
		Internal registers already have fixed hardware register numbers.
	*/
	if (psInst->asArg[uVecArgStart].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		psShift->uShiftInChans = psShift->uShiftInRegs = 0;
		return;
	}

	/*
		Get the number of whole (32-bit) registers which are unreferenced at the start of hte
		vector.
	*/
	if (eSrcFmt == UF_REGFORMAT_F16 && uFirstUsedChan >= USC_Z_CHAN)
	{
		psShift->uShiftInChans = 2;
		psShift->uShiftInRegs = 1;
	}
	else if (eSrcFmt == UF_REGFORMAT_F32 && uFirstUsedChan >= USC_Y_CHAN)
	{
		psShift->uShiftInChans = psShift->uShiftInRegs = uFirstUsedChan;
	}
	else
	{
		psShift->uShiftInChans = psShift->uShiftInRegs = 0;
	}
}

static
IMG_VOID ShiftSourceArguments(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_UINT32			uSwizzleSlot,
							  PSWIZZLE_SHIFT		psShift)
/*****************************************************************************
 FUNCTION	: SetSwizzleShift

 PURPOSE	: Shifts a source vector so the referenced channels are at the
			  start of the vector.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uSwizzleSlot		- Source argument to modify.
			  psShift			- Details of the unreferenced part.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegIdx;
	IMG_UINT32	uVecArgStart = uSwizzleSlot * SOURCE_ARGUMENTS_PER_VECTOR + 1;

	/*
		Shuffle the source arguments making up the vector so there are no unused channels at
		the start.
	*/
	for (uRegIdx = 0; uRegIdx < psShift->uUsedChanCount; uRegIdx++)
	{
		MoveSrc(psState,
				psInst,
				uVecArgStart + uRegIdx,
				psInst, 
				uVecArgStart + uRegIdx + psShift->uShiftInRegs);
	}

	/*
		Mark the rest of the vector as unused.
	*/
	for (; uRegIdx < VECTOR_LENGTH; uRegIdx++)
	{
		SetSrcUnused(psState, psInst, uVecArgStart + uRegIdx);
	}
}

static
IMG_VOID SetSwizzleShift(PINTERMEDIATE_STATE	psState,
						 PINST					psInst,
						 IMG_UINT32				uSwizzleSlot,
						 PSWIZZLE_SHIFT			psShift)
/*****************************************************************************
 FUNCTION	: SetSwizzleShift

 PURPOSE	: Shifts a source vector so the referenced channels are at the
			  start of the vector.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uSwizzleSlot		- Source argument to modify.
			  psShift			- Details of the unreferenced part.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Apply an extra swizzle which undoes the effect of shifting the source
		arguments.
	*/
	IsShiftedSwizzleSupported(psState, 
							  psInst, 
							  uSwizzleSlot, 
							  uSwizzleSlot,
							  psShift->uShiftInChans /* iShift */,
							  IMG_FALSE /* bCheckOnly */);

	/*
		Move the source arguments down or up.
	*/
	ShiftSourceArguments(psState, psInst, uSwizzleSlot, psShift);
}

static
IMG_VOID NormaliseSwizzles(PINTERMEDIATE_STATE		psState,
						   PINST					psInst,
						   IMG_UINT32				uSwizzleSlot)
/*****************************************************************************
 FUNCTION	: NormaliseSwizzles

 PURPOSE	: Try to normalise an instruction so the channels referenced from
			  the vector source argument start from the X channel.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uSwizzleSlot		- Source argument to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SWIZZLE_SHIFT sShift;

	/*
		Get the length of any leading, unreferenced part of this vector.
	*/
	GetSwizzleShift(psState, 
					psInst, 
					uSwizzleSlot, 
					&sShift);	

	if (sShift.uShiftInChans > 0)
	{
		/*
			Check if the swizzle can be modified to move the referenced channels to
			the start of the vector.
		*/
		if (!IsShiftedSwizzleSupported(psState, 
									   psInst, 
									   uSwizzleSlot, 
									   uSwizzleSlot,
									   sShift.uShiftInChans /* iShift */,
									   IMG_TRUE /* bCheckOnly */))
		{
			return;
		}
		
		/*
			Shift the referenced channels down.
		*/
		SetSwizzleShift(psState,
						psInst,
						uSwizzleSlot,
						&sShift);
	}
}

static
IMG_VOID NormaliseVTESTSwizzles(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: NormaliseVTESTSwizzles

 PURPOSE	: Try to normalise a VTEST instruction so the channels referenced from
			  the vector source arguments start from the X channel.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SWIZZLE_SHIFT	asShift[2];
	IMG_UINT32		auSwizzle[2];
	IMG_UINT32		uSrc;

	ASSERT(IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst));
	
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		/*	
			Get the possible shift for the sources.
		*/
		GetSwizzleShift(psState, 
						psInst, 
						uSrc, 
						&asShift[uSrc]);

		/*
			Create a new swizzle for the source incorporated the shift.
		*/
		auSwizzle[uSrc] = GetShiftedSwizzle(asShift[uSrc].uShiftInChans, psInst->u.psVec->auSwizzle[uSrc]);
	}

	if (asShift[0].uShiftInChans == 0 && asShift[1].uShiftInChans == 0)
	{
		return;
	}

	if (!IsSwizzleSetSupported(psState, psInst->eOpcode, psInst, NULL /* auNewLiveChansInArg */, auSwizzle))
	{
		return;
	}

	/*
		Modify both sources to shift the referenced channels down.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		if (asShift[uSrc].uShiftInChans == 0)
		{
			continue;
		}

		psInst->u.psVec->auSwizzle[uSrc] = auSwizzle[uSrc];
		ShiftSourceArguments(psState, psInst, uSrc, &asShift[uSrc]);
	}
}

static
IMG_VOID NormaliseVMOVCSwizzles(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: NormaliseSwizzles

 PURPOSE	: Try to normalise a VMOVC instruction so the channels referenced from
			  the vector source argument start from the X channel.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SWIZZLE_SHIFT	asShift[2];
	IMG_UINT32		uSrc;

	/*
		Normalise the first source independently.			
	*/
	NormaliseSwizzles(psState, psInst, 0 /* uSwizzleSlot */);

	/*	
		Get the possible shifts for the second two sources.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		GetSwizzleShift(psState, 
						psInst, 
						1 + uSrc, 
						&asShift[uSrc]);	
	}

	/*
		Check if both sources have the same shift.
	*/
	if (asShift[0].uShiftInChans == asShift[1].uShiftInChans && 
		asShift[0].uShiftInRegs == asShift[1].uShiftInRegs &&
		asShift[0].uShiftInChans > 0)
	{
		IMG_UINT32	auSwizzleSet[3];

		ASSERT(asShift[0].uUsedChanCount == asShift[1].uUsedChanCount);

		/*
			Don't move a source argument to the X (even aligned) position if it is
			already known to require odd alignment.
		*/
		ASSERT(asShift[0].uShiftInRegs == 1);
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			PARG psSrc = &psInst->asArg[(1 + uSrc) * SOURCE_ARGUMENTS_PER_VECTOR + 2];

			if (psSrc->uType == USEASM_REGTYPE_TEMP)
			{
				PREGISTER_GROUP	psSrcGroup = FindRegisterGroup(psState, psSrc->uNumber);

				if (psSrcGroup != NULL && psSrcGroup->eAlign == HWREG_ALIGNMENT_ODD)
				{
					return;
				}
			}
		}

		/*
			Check if the new swizzles are supported by the instruction.
		*/
		auSwizzleSet[0] = psInst->u.psVec->auSwizzle[0];
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			auSwizzleSet[1 + uSrc] = GetShiftedSwizzle(asShift[uSrc].uShiftInChans, psInst->u.psVec->auSwizzle[1 + uSrc]);
		}
		if (!IsSwizzleSetSupported(psState, psInst->eOpcode, psInst, NULL /* auNewLiveChansInArg */, auSwizzleSet))
		{
			return;
		}

		/*
			Modify both sources to shift the referenced channels down.
		*/
		for (uSrc = 0; uSrc < 2; uSrc++)
		{
			psInst->u.psVec->auSwizzle[1 + uSrc] = auSwizzleSet[1 + uSrc];
			ShiftSourceArguments(psState, psInst, 1 + uSrc, &asShift[uSrc]);
		}
	}
}

static
IMG_VOID NormaliseSwizzlesInst(PINTERMEDIATE_STATE		psState,
							   PINST					psInst)
/*****************************************************************************
 FUNCTION	: NormaliseSwizzles

 PURPOSE	: Try to normalise an instruction so the channels referenced from
			  the vector source argument start from the X channel.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (HasIndependentSourceSwizzles(psState, psInst))
	{
		IMG_UINT32	uSrcSlot;

		for (uSrcSlot = 0; uSrcSlot < GetSwizzleSlotCount(psState, psInst); uSrcSlot++)
		{
			NormaliseSwizzles(psState, psInst, uSrcSlot);
		}
	}
	else
	{
		if ((psInst->eOpcode == IVMOVC) || (psInst->eOpcode == IVMOVCU8_FLT))
		{
			/*
				VMOVC has the special restriction that the swizzles on the last two arguments
				must be the same.
			*/
			NormaliseVMOVCSwizzles(psState, psInst);
		}		
		else
		{
			ASSERT(psInst->eOpcode == IVTEST);
			NormaliseVTESTSwizzles(psState, psInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID AdjustRegisterGroupForSwizzle(PINTERMEDIATE_STATE		psState,
									   PINST					psInst,
									   IMG_UINT32				uDestSwizzleSlot,
									   PREGISTER_GROUP_DESC		psOutDetails,
									   PREGISTER_GROUP_DESC		psInDetails)
/*****************************************************************************
 FUNCTION	: AdjustRegisterGroupForSwizzle

 PURPOSE	: Where an instruction supports multiple source swizzles check 
			  whether we could make use of different swizzles to remove some
			  of the restrictions on the hardware register numbers to assign
			  to a subset of the source arguments.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to fix.
			  puArgStart		- The start of the argument subset.
			  puArgCount		- Count of arguments in the subset.
			  pbArgAlign		- TRUE if the first argument in the subset must
								have an even aligned hardware register number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uFirstUsedChan, uLastUsedChan;
	IMG_UINT32		uVecArgIdx, uChansUsedPostSwizzle;
	IMG_UINT32		uArgStart = psInDetails->uStart;
	IMG_UINT32		uArgCount = psInDetails->uCount;
	UF_REGFORMAT	eSrcFmt;

	/*
		Set the default to no change in the argument group.
	*/
	*psOutDetails = *psInDetails;

	/*
		Map back to the source swizzle slot.
	*/
	ASSERT(uArgStart >= 1);
	ASSERT(((uArgStart - 1) % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
	uVecArgIdx = (uArgStart - 1) / SOURCE_ARGUMENTS_PER_VECTOR;

	if (IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst))
	{
		/*
			For simplicity don't handle swapping VTEST sources.	
		*/
		ASSERT(uDestSwizzleSlot == uVecArgIdx);

		/*
			VTEST has the special restriction that the first source can use any replicate swizzle
			and the second source must use either the same swizzle as the first or an XXXX swizzle.

			So we can adjust the swizzle on the first source provided the second source is using
			an XXXX swizzle.
		*/
		if (uDestSwizzleSlot != 0 || psInst->u.psVec->auSwizzle[1] != USEASM_SWIZZLE(X, X, X, X))
		{
			return;
		}
	}
	else
	{
		/*
			VMOVC has the special restriction that the swizzles on the last two arguments
			must be the same. We don't handle requiring two variables to have the same
			(but not fixed) alignment so skip this instruction.
		*/
		if (!HasIndependentSourceSwizzles(psState, psInst))
		{
			return;
		}
	}

	/*
		C10 channels all use some bits from the first source register so we can't modify
		the register group.
	*/
	eSrcFmt = psInst->u.psVec->aeSrcFmt[uVecArgIdx];
	if (eSrcFmt == UF_REGFORMAT_C10)
	{
		return;
	}
	ASSERT(eSrcFmt == UF_REGFORMAT_F16 || eSrcFmt == UF_REGFORMAT_F32);

	/*
		Internal registers and immediates already have fixed hardware register numbers.
	*/
	if (psInst->asArg[uArgStart].uType == USEASM_REGTYPE_FPINTERNAL ||
		psInst->asArg[uArgStart].uType == USEASM_REGTYPE_IMMEDIATE)
	{
		return;
	}

	ASSERT(uArgCount <= VECTOR_LENGTH);

	/*
		Get the mask of channels referenced from the subset with and without applying the swizzle.
	*/
	GetLiveChansInSourceSlot(psState, psInst, uVecArgIdx, NULL /* puChansUsedPreSwizzle */, &uChansUsedPostSwizzle);

	/*
		Get the range of arguments referenced.
	*/
	uFirstUsedChan = auFirstSetBit[uChansUsedPostSwizzle];
	uLastUsedChan = auLastSetBit[uChansUsedPostSwizzle];

	/*
		If a full vector of channels is used then the restrictions on the
		arguments can't be loosened.
	*/
	if (uFirstUsedChan == USC_X_CHAN && uLastUsedChan == USC_W_CHAN)
	{
		return;
	}

	if (eSrcFmt == UF_REGFORMAT_F16)
	{
		if (uFirstUsedChan >= USC_X_CHAN && uLastUsedChan <= USC_Y_CHAN)
		{
			/*
				For F16 we can access an unaligned source by shifting the swizzle by 2 e.g.

				((r1.LOW, r1.HIGH), (r2.LOW, r2.HIGH)).xy__ 
					->
				((r0.LOW, r0.HIGH), (r1.LOW, r1.HIGH)).zw__
			*/
			if (!IsShiftedSwizzleSupported(psState, 
										   psInst, 
										   uDestSwizzleSlot, 
										   uVecArgIdx,
										   -2 /* iShift */,
										   IMG_TRUE /* bCheckOnly */))
			{
				return;
			}

			ASSERT(psOutDetails->uCount == 1);
			psOutDetails->eAlign = HWREG_ALIGNMENT_NONE;
			return;
		}
	}
	else
	{
		if (uFirstUsedChan == USC_X_CHAN)
		{
			IMG_UINT32		uWideSourceMask;

			/*
				We can remove an alignment restriction by shifting the sources e.g.

				(r1, r2, r3, __).xyz_	->	(__, r1, r2, r3).yzw_
			*/

			/*
				Check that the instruction supports accessing a 128-bit vector from
				this source.
			*/
			uWideSourceMask = GetWideSourceMask(psInst->eOpcode);
			if ((uWideSourceMask & (1U << uVecArgIdx)) == 0)
			{
				ASSERT(uLastUsedChan <= USC_Y_CHAN);
				if (uLastUsedChan == USC_Y_CHAN)
				{
					return;
				}
			}

			/*
				Check that the modified swizzle needed to access the shifted sources
				is supported.
			*/
			if (!IsShiftedSwizzleSupported(psState, 
										   psInst, 
										   uDestSwizzleSlot, 
										   uVecArgIdx,
										   -1 /* iShift */,
										   IMG_TRUE /* bCheckOnly */))
			{
				return;
			}

			/*
				Don't require any particular alignment for the first source argument
				in the subset.
			*/
			psOutDetails->eAlign = HWREG_ALIGNMENT_NONE;
		}
	}
}

static
IMG_BOOL UpdateSwizzleForVectorDest(PINTERMEDIATE_STATE		psState,
									PINST					psInst,
									PREGISTER_GROUP_DESC	psDetails)
/*****************************************************************************
 FUNCTION	: UpdateSwizzleForVectorDest

 PURPOSE	: Try to fix an instruction which has incompatible alignment
			  restrictions on its destination.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to form mandatory groups for.
			  psDetails			- Information about the restrictions on
								the destination.

 RETURNS	: TRUE if the instruction was successfully fixed.
*****************************************************************************/
{
	PREGISTER_GROUP	psBaseGroup;
	PARG			psSingleDest;
	IMG_UINT32		uSingleDest;
	IMG_UINT32		uDestIdx;
	HWREG_ALIGNMENT	eRequiredDestAlignment;
	HWREG_ALIGNMENT	eBaseGroupAlign;

	if (psDetails->eAlign == HWREG_ALIGNMENT_NONE)
	{
		return IMG_FALSE;
	}
	if (psDetails->uStart != 0)
	{
		return IMG_FALSE;
	}
	if (!(psDetails->uCount == 1 || psDetails->uCount == 2))
	{
		return IMG_FALSE;
	}

	/*
		Check the instruction writes only a single 32-bit register.
	*/
	uSingleDest = USC_UNDEF;
	for (uDestIdx = 0; uDestIdx < psDetails->uCount; uDestIdx++)
	{
		if (psInst->auDestMask[uDestIdx] != 0)
		{
			if (uSingleDest != USC_UNDEF)
			{
				return IMG_FALSE;
			}
			uSingleDest = uDestIdx;
		}
	}
	ASSERT(uSingleDest != USC_UNDEF);
	psSingleDest = &psInst->asDest[uSingleDest];

	/*
		Get the required alignment of the single destination based on its position
		in the destination array.
	*/
	if ((uSingleDest % 2) == 0)
	{
		eRequiredDestAlignment = HWREG_ALIGNMENT_EVEN;
	}
	else
	{
		eRequiredDestAlignment = HWREG_ALIGNMENT_ODD;
	}

	/*
		Get information about the existing restrictions on the hardware register
		assigned to the destination.
	*/
	if (psSingleDest->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	psBaseGroup = FindRegisterGroup(psState, psSingleDest->uNumber);
	if (psBaseGroup == NULL)
	{
		return IMG_FALSE;
	}
	eBaseGroupAlign = GetNodeAlignment(psBaseGroup);
	if (eBaseGroupAlign == HWREG_ALIGNMENT_NONE || eBaseGroupAlign == eRequiredDestAlignment)
	{
		return IMG_FALSE;
	}

	/*
		Look for an instruction where we can update the swizzles so the data which
		was written into the first destination is now written into the second.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) == 0)
	{
		return IMG_FALSE;
	}
	if (psInst->asDest[0].eFmt != UF_REGFORMAT_F32)
	{
		return IMG_FALSE;
	}
	if (!SwapVectorDestinations(psState, psInst, uSingleDest, 1 - uSingleDest, IMG_TRUE /* bCheckOnly */))
	{
		return IMG_FALSE;
	}

	if (uSingleDest == 0 && psBaseGroup->psPrev == NULL)
	{
		return IMG_FALSE;
	}

	if (uSingleDest == 0)
	{
		ASSERT(GetNodeAlignment(psBaseGroup->psPrev) == HWREG_ALIGNMENT_EVEN);

		/*
			Make the current destination the destination for the Y 
			channel of the source.
		*/
		ASSERT(psInst->uDestCount == 1);
		SetDestCount(psState, psInst, 2 /* uNewDestCount */);
		MoveDest(psState, psInst, 1 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		CopyPartiallyWrittenDest(psState, psInst, 1 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
		psInst->auDestMask[1] = psInst->auDestMask[0];
		psInst->auLiveChansInDest[1] = psInst->auLiveChansInDest[0];

		/*
			The (unwritten) destination for the X channel will be the previous
			variable in the register group.
		*/
		SetDestUnused(psState, psInst, 0 /* uDestIdx */);
		psInst->auDestMask[0] = 0;
		psInst->auLiveChansInDest[0] = 0;
		SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartialDest */);

		/*
			Update the instruction so the result which was written into the X channel
			is now written into the Y channel.
		*/
		psInst->u.psVec->bWriteYOnly = IMG_TRUE;	
	}
	else
	{
		if (psInst->apsOldDest[0] != NULL)
		{
			PINST	psMOVInst;

			psMOVInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psMOVInst, IMOV);
			MoveDest(psState, psMOVInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
			psMOVInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];
			MovePartialDestToSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psInst, 0 /* uMoveFromDestIdx */);
			CopyPredicate(psState, psMOVInst, psInst);
			InsertInstAfter(psState, psInst->psBlock, psMOVInst, psInst);
		}

		/*
			Swap the second destination to the first.
		*/
		MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psInst, 1 /* uMoveFromDestIdx */);
		CopyPartiallyWrittenDest(psState, psInst, 0 /* uMoveToDestIdx */, psInst, 1 /* uMoveFromDestIdx */);
		psInst->auDestMask[0] = psInst->auDestMask[1];
		psInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[1];

		SetDestCount(psState, psInst, 1 /* uNewDestCount */);
	}

	/*
		Update the instruction source swizzles for the change to the destination.
	*/
	SwapVectorDestinations(psState, psInst, uSingleDest, 1 - uSingleDest, IMG_FALSE /* bCheckOnly */);

	return IMG_TRUE;
}

static
IMG_BOOL TryCombiningVMOVWithAlignedVMOV(PINTERMEDIATE_STATE		psState,
										 PINST						psOddInst)
/*****************************************************************************
 FUNCTION	: TryCombiningVMOVWithAlignedVMOV

 PURPOSE	: Try to combine a VMOV instruction whose swizzle has been shifted
			 due to alignment issues with another (aligned) VMOV to the
			 adjacent register.

 PARAMETERS	: psState			- Compiler state.
			  psOddInst			- VMOV Instruction that writes only to the second
								destination

 RETURNS	: Whether the combining was successful, ie whether instruction was replaced.
*****************************************************************************/
{
	PREGISTER_GROUP			psNode;
	IMG_UINT32				uPrevReg;
	PUSEDEF_CHAIN			psUseDef;
	PINST					psEvenInst;
	PUSC_LIST_ENTRY			psUseListEntry;
	IMG_BOOL				bHasUse;
	IMG_UINT32				uSrc;
	PINST					psCombinedMov;
	UF_REGFORMAT			eDestFmt;
	UF_REGFORMAT			eSrcFmt;
	REGISTER_GROUPS_DESC	sEvenInstGroups;
	REGISTER_GROUPS_DESC	sOddInstGroups;
	IMG_UINT32				uArgStart;
	IMG_UINT32				uCommonArgCount;
	IMG_UINT32				uNewSwizzle;
	IMG_UINT32				uChansToCopy;
	IMG_UINT32				uCopyStart;
	IMG_UINT32				uChan;
	IMG_UINT32				uArg;
	IMG_UINT32				uEvenInstArgCount;
	IMG_UINT32				uOddInstArgCount;
	IMG_BOOL				bNonSSASources;

	/*
		Check the first instruction is writing a single 32-bit register in the odd-aligned destination.
	*/
	if (psOddInst->eOpcode != IVMOV || psOddInst->uDestCount < 2 || psOddInst->auDestMask[0] != 0)
	{
		return IMG_FALSE;
	}

	/* Find the register that is just before the destination */
	if (psOddInst->asDest[1].uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	psNode = FindRegisterGroup(psState, psOddInst->asDest[1].uNumber);
	if (psNode == NULL || psNode->psPrev == NULL)
	{
		return IMG_FALSE;
	}
	uPrevReg = psNode->psPrev->uRegister;
	psUseDef = UseDefGet(psState, psOddInst->asDest[1].uType, uPrevReg);
	if (psUseDef->psDef == NULL || psUseDef->psDef->eType != DEF_TYPE_INST)
	{
		return IMG_FALSE;
	}

	/* The instruction writing to the adjacent register */
	psEvenInst = psUseDef->psDef->u.psInst;

	/*
		Check the instruction writing the adjacent register is also a VMOV.
	*/
	if (psEvenInst->eOpcode != IVMOV)
	{
		return IMG_FALSE;
	}
	if (psEvenInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}
	if (psEvenInst->asDest[0].uType != psOddInst->asDest[1].uType)
	{
		return IMG_FALSE;
	}
	if (psEvenInst->asDest[0].eFmt != psOddInst->asDest[1].eFmt)
	{
		return IMG_FALSE;
	}
	eDestFmt = psEvenInst->asDest[0].eFmt;

	ASSERT(psOddInst->asDest[0].uType == USC_REGTYPE_UNUSEDDEST);
	ASSERT(psOddInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE);
	ASSERT(psEvenInst->asArg[0].uType == USC_REGTYPE_UNUSEDSOURCE);

	if (psEvenInst->u.psVec->aeSrcFmt[0] != psOddInst->u.psVec->aeSrcFmt[0])
	{
		return IMG_FALSE;
	}
	eSrcFmt = psEvenInst->u.psVec->aeSrcFmt[0];

	/*
		Check the execution of both instructions is controlled by the
		same predicate.
	*/
	if (!EqualPredicates(psOddInst, psEvenInst))
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions are members of the same basic block and the instruction
		writing the even aligned register is earlier in the block.
	*/
	if (psEvenInst->psBlock != psOddInst->psBlock)
	{
		return IMG_FALSE;
	}
	if (psEvenInst->uBlockIndex >= psOddInst->uBlockIndex)
	{
		return IMG_FALSE;
	}

	/*
		Check both instructions have the same source arguments.
	*/
	GetSourceRegisterGroups(psState, psEvenInst, &sEvenInstGroups);
	GetSourceRegisterGroups(psState, psOddInst, &sOddInstGroups);

	ASSERT(sEvenInstGroups.uCount == 1);
	ASSERT(sOddInstGroups.uCount == 1);
	ASSERT(sEvenInstGroups.asGroups[0].uStart == sOddInstGroups.asGroups[0].uStart);
	uArgStart = sEvenInstGroups.asGroups[0].uStart;
	uEvenInstArgCount = sEvenInstGroups.asGroups[0].uCount;
	uOddInstArgCount = sOddInstGroups.asGroups[0].uCount;
	uCommonArgCount = min(uEvenInstArgCount, uOddInstArgCount);

	bNonSSASources = IMG_FALSE;
	for (uArg = 0; uArg < uCommonArgCount; uArg++)
	{
		if (!EqualArgs(&psEvenInst->asArg[uArgStart + uArg], &psOddInst->asArg[uArgStart + uArg]))
		{
			return IMG_FALSE;
		}
		if (!UseDefIsSSARegisterType(psState, psEvenInst->asArg[uArgStart + uArg].uType))
		{
			bNonSSASources = IMG_TRUE;
			break;
		}
	}

	if (bNonSSASources)
	{
		/*
			If the odd instruction has sources which might be defined more than once then check that we
			are not moving the odd instruction backwards over a previous define or use.
		*/
		if (psEvenInst->psNext != psOddInst)
		{
			USEDEF_RECORD		sInbetweenInstsUseDef;
			IMG_BOOL			bInvalidMove;

			InitUseDef(&sInbetweenInstsUseDef);
			ComputeInstUseDef(psState, &sInbetweenInstsUseDef, psEvenInst->psNext, psOddInst->psPrev);

			bInvalidMove = IMG_FALSE;
			if (InstUseDefined(psState, &sInbetweenInstsUseDef.sDef, psOddInst) ||
				InstDefReferenced(psState, &sInbetweenInstsUseDef.sUse, psOddInst))
			{
				bInvalidMove = IMG_TRUE;
			}

			ClearUseDef(psState, &sInbetweenInstsUseDef);

			if (bInvalidMove)
			{
				return IMG_FALSE;
			}
		}
	}
	else if (uOddInstArgCount > uEvenInstArgCount)
	{
		/*
			Otherwise check the (unique) define point for the sources to the odd instruction isn't 
			between the even instruction and the odd instruction.
		*/
		for (uArg = uEvenInstArgCount; uArg < uOddInstArgCount; uArg++)
		{
			PARG	psArg = &psOddInst->asArg[uArgStart + uArg];
			PINST	psDefInst;

			psDefInst = UseDefGetDefInst(psState, psArg->uType, psArg->uNumber, NULL /* puDestIdx */);
			if (
					psDefInst != NULL && 
					psDefInst->psBlock == psOddInst->psBlock && 
					psDefInst->uBlockIndex >= psEvenInst->uBlockIndex
				)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
		Merge the swizzles from the two instructions.
	*/
	if (eDestFmt == UF_REGFORMAT_F32)
	{
		uChansToCopy = F32_CHANNELS_PER_SCALAR_REGISTER;
		uCopyStart = USC_Y_CHAN;
	}
	else
	{
		ASSERT(eDestFmt == UF_REGFORMAT_F16);

		uChansToCopy = F16_CHANNELS_PER_SCALAR_REGISTER;
		uCopyStart = USC_Z_CHAN;
	}
	uNewSwizzle = psEvenInst->u.psVec->auSwizzle[0];
	for (uChan = 0; uChan < F16_CHANNELS_PER_SCALAR_REGISTER; uChan++)
	{
		USEASM_SWIZZLE_SEL	eOddSel;

		eOddSel = GetChan(psOddInst->u.psVec->auSwizzle[0], uCopyStart + uChan);
		uNewSwizzle = SetChan(uNewSwizzle, uCopyStart + uChan, eOddSel);
	}

	/* 
		For simplicity, only combine them if the instruction's destination is not read
		by any other instruction. We can then reorder it freely (within the block).
		Could improve this to check whether the instruction can be moved by using a
		dependency graph.
	*/
	bHasUse = IMG_FALSE;
	for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
		if (psUse == psUseDef->psDef || psUse->eType == USE_TYPE_FIXEDREG)
		{
			continue;
		}

		bHasUse = IMG_TRUE;
		break;
	}

	if (bHasUse)
	{
		return IMG_FALSE;
	}

	/* Go ahead and combine them. */
	psCombinedMov = AllocateInst(psState, psOddInst);

	SetOpcodeAndDestCount(psState, psCombinedMov, IVMOV, 2 /* uDestCount */);

	psCombinedMov->u.psVec->auSwizzle[0] = uNewSwizzle;
	psCombinedMov->u.psVec->aeSrcFmt[0] = eSrcFmt;

	SetSrcUnused(psState, psCombinedMov, 0 /* uSrcIdx */);
	for (uSrc = 1; uSrc < psCombinedMov->uArgumentCount; ++uSrc)
	{
		if (psOddInst->asArg[uSrc].uType != USC_REGTYPE_UNUSEDSOURCE)
		{
			ASSERT((psEvenInst->asArg[uSrc].uType == USC_REGTYPE_UNUSEDSOURCE) || 
				   EqualArgs(&psEvenInst->asArg[uSrc], &psOddInst->asArg[uSrc]));

			CopySrc(psState, psCombinedMov, uSrc, psOddInst, uSrc);
		}
		else
		{
			CopySrc(psState, psCombinedMov, uSrc, psEvenInst, uSrc);
		}
	}


	/*
		Copy the even-aligned destination from the old instruction to the new one.
	*/
	MoveDest(psState, psCombinedMov, 0, psEvenInst, 0);
	CopyPartiallyWrittenDest(psState, psCombinedMov, 0, psEvenInst, 0);
	psCombinedMov->auLiveChansInDest[0] = psEvenInst->auLiveChansInDest[0];
	psCombinedMov->auDestMask[0] = psEvenInst->auDestMask[0];

	/*
		Copy the odd-aligned destination from the old instruction to the new one.
	*/
	MoveDest(psState, psCombinedMov, 1, psOddInst, 1);
	CopyPartiallyWrittenDest(psState, psCombinedMov, 1, psOddInst, 1);
	psCombinedMov->auLiveChansInDest[1] = psOddInst->auLiveChansInDest[1];
	psCombinedMov->auDestMask[1] = psOddInst->auDestMask[1];

	InsertInstAfter(psState, psOddInst->psBlock, psCombinedMov, psOddInst);

	RemoveInst(psState, psEvenInst->psBlock, psEvenInst);
	FreeInst(psState, psEvenInst);

	RemoveInst(psState, psOddInst->psBlock, psOddInst);
	FreeInst(psState, psOddInst);

	return IMG_TRUE;
}

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID SetupRegisterGroupInstDest(PINTERMEDIATE_STATE		psState,
									PINST					psInst,
									IMG_UINT32				uGroupStart,
									IMG_UINT32				uGroupCount,
									HWREG_ALIGNMENT			eGroupAlign,
									IMG_PBOOL				pbRemovedInst,
									IMG_PBOOL				pbMisalignedDest)
/*****************************************************************************
 FUNCTION	: SetupRegisterGroupInstDest

 PURPOSE	: Form a mandatory group for an instruction which requires its destinations
			  to have consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to form mandatory groups for.
			  psDetails			- Details of the destinations which require
								consecutive hardware register numbers.
			  pbRemovedInst		- Whether the instruction was removed.
			  pbMisalignedDest	- Set to TRUE on return if the instruction had
								a misaligned destination hardware register number
								which was fixed by reshuffling the destinations.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_GROUP_DESC	sDetails;

	*pbRemovedInst = IMG_FALSE;

	if (uGroupCount == 0 || (uGroupCount == 1 && eGroupAlign == HWREG_ALIGNMENT_NONE))
	{
		return;
	}

	sDetails.uStart = uGroupStart;
	sDetails.uCount = uGroupCount;
	sDetails.eAlign = eGroupAlign;

	/*
		GPI registers already have consecutive hardware register numbers.
	*/
	if (psInst->asDest[uGroupStart].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}

	/*
		Check if the register allocator can give the destinations consecutive
		hardware register numbers.
	*/
	if (IsValidGroup(psState, 
					 psInst, 
					 uGroupCount, 
					 psInst->asDest + uGroupStart, 
					 eGroupAlign,
					 IMG_FALSE /* bReplaceNonTempsByMOVs */))
	{
		MakeGroup(psState, psInst->asDest + uGroupStart, uGroupCount, eGroupAlign);
		return;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check for a vector instruction writing a single variable where the instruction
		requires the hardware register number of its destination to be even but the
		hardware register number of the variable is already restricted to be odd. We
		might be able to update the instruction so the current destination is the
		second destination (and can therefore be odd) and the result which was written into the
		first destination is now written into the second.
	*/
	if (UpdateSwizzleForVectorDest(psState, psInst, &sDetails))
	{
		if (pbMisalignedDest != NULL)
		{
			*pbMisalignedDest = IMG_TRUE;
		}
		return;
	}

	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0 && uGroupCount > 1)
	{
		if (SplitInstruction(psState, psInst, &sDetails))
		{
			*pbRemovedInst = IMG_TRUE;
			return;
		}
	}
	#else
	PVR_UNREFERENCED_PARAMETER(pbMisalignedDest);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	ReplaceDestinationsByTemporaries(psState, psInst, &sDetails);
}

static
IMG_BOOL SetupRegisterGroupsInstDest(PINTERMEDIATE_STATE	psState,
									 PINST					psInst,
									 IMG_PBOOL				pbMisalignedDest)
/*****************************************************************************
 FUNCTION	: SetupRegisterGroupsInstDest

 PURPOSE	: Form mandatory groups for an instruction which requires its destinations
			  to have consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to form mandatory groups for.
			  pbMisalignedDest	- Set to TRUE on return if the instruction had
								a misaligned destination hardware register number
								which was fixed by reshuffling the destinations.

 RETURNS	: Whether the instruction was removed.
*****************************************************************************/
{
	if (pbMisalignedDest != NULL)
	{
		*pbMisalignedDest = IMG_FALSE;
	}

	/*
		For each subset of the destinations that require consecutive hardware register
		numbers.
	*/
	if	(!GetBit(psInst->auFlag, INST_NOEMIT))
	{
		REGISTER_GROUPS_DESC	sDesc;
		IMG_UINT32				uGroupIdx;

		GetDestRegisterGroups(psState, psInst, &sDesc);

		for (uGroupIdx = 0; uGroupIdx < sDesc.uCount; uGroupIdx++)
		{
			PREGISTER_GROUP_DESC	psGroup = &sDesc.asGroups[uGroupIdx];
			IMG_BOOL				bInstRemoved;

			SetupRegisterGroupInstDest(psState, 
									   psInst, 
									   psGroup->uStart, 
									   psGroup->uCount, 
									   psGroup->eAlign, 
									   &bInstRemoved,
									   pbMisalignedDest);

			if (bInstRemoved)
			{
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}

static
IMG_BOOL ReplaceNonTemporarySourcesByTemporaries(PINTERMEDIATE_STATE		psState,
												 PEQUIV_SRC_DATA			psEquivSrcData,
												 PINST						psInst,
												 PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: ReplaceNonTemporarySourcesByTemporaries

 PURPOSE	: Check if we could make a group of source arguments valid by
			  replacing immediates by temporaries containing the same data.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Information about move instructions inserted to
								fix previous instructions.
			  psInst			- Instruction to form mandatory groups for.
			  psDetails			- Details of the group to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_BOOL	bSomeTempSources;
	IMG_UINT32	uNonTempMask;
	IMG_UINT32	uRegType;
	PARG		psFirstTempArg = NULL;

	bSomeTempSources = IMG_FALSE;
	uNonTempMask = 0;
	for (uArg = 0; uArg < psDetails->uCount; uArg++)
	{
		PARG	psArg = &psInst->asArg[psDetails->uStart + uArg];

		if (psArg->uType == USEASM_REGTYPE_TEMP)
		{
			psFirstTempArg = psArg;
			bSomeTempSources = IMG_TRUE;
		}
		else
		{
			uNonTempMask |= (1U << uArg);
		}
	}
	if (!bSomeTempSources || uNonTempMask == 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if the sources would be valid with immediates replaced.
	*/
	if (!IsValidGroup(psState, 
					  psInst, 
					  psDetails->uCount, 
					  psInst->asArg + psDetails->uStart, 
					  psDetails->eAlign, 
					  IMG_TRUE /* bReplaceNonTempsByMOVs */))
	{
		return IMG_FALSE;
	}

	/*
		Check the register bank of the non-immediate sources is supported.
	*/
	uRegType = GetPrecolouredRegisterType(psState, psInst, psFirstTempArg->uType, psFirstTempArg->uNumber);
	if (!CanUseSrc(psState, psInst, psDetails->uStart, uRegType, psFirstTempArg->uIndexType))
	{
		return IMG_FALSE;
	}

	/*
		Insert MOV instructions to replace the immediates.
	*/
	ReplaceSourcesByTemporaries(psState,
								psEquivSrcData,
								psInst,
								psDetails,
								uNonTempMask,
								IMG_TRUE /* bValidGroup */);

	return IMG_TRUE;
}

typedef struct _GROUPS_TO_CHECK
{
	struct
	{
		/*
			Details of the location of this group.
		*/
		REGISTER_GROUP_DESC		sDetails;

		/*
			Source to the instruction to check against.
		*/
		IMG_UINT32				uSrcGroupIdx;
	}						asGroupsToCheck[USC_MAXIMUM_REGISTER_GROUP_COUNT];

	/*
		Count of groups of source arguments with restrictions on their
		hardware register numbers.
	*/
	IMG_UINT32	uGroupCount;
} GROUPS_TO_CHECK, *PGROUPS_TO_CHECK;

static
IMG_VOID StoreGroupsToCheckCB(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_BOOL				bDest,
							  IMG_UINT32			uGroupStart,
							  IMG_UINT32			uGroupCount,
							  HWREG_ALIGNMENT		eGroupAlign,
							  IMG_PVOID				pvGroups)
/*****************************************************************************
 FUNCTION	: StoreGroupsToCheckCB

 PURPOSE	: Create an array of all the groups of instruction sources
			  which have restrictions on their hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  bDest				- TRUE if the arguments are destinations.
								  FALSE if the arguments are sources.
			  uGroupStart		- Start of the group within the destinations
								or sources array.
			  uGroupCount		- Length of the group.
			  eGroupAlign		- Required alignment for the hardware register
								number of the first argument in the group.
			  pvGroups			- Points to the array to store the register
								groups into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PGROUPS_TO_CHECK	psGroups = (PGROUPS_TO_CHECK)pvGroups;

	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psInst);
	
	ASSERT(!bDest);

	if (uGroupCount == 0 || (uGroupCount == 1 && eGroupAlign == HWREG_ALIGNMENT_NONE))
	{
		return;
	}

	psGroups->asGroupsToCheck[psGroups->uGroupCount].sDetails.uStart = uGroupStart;
	psGroups->asGroupsToCheck[psGroups->uGroupCount].sDetails.uCount = uGroupCount;
	psGroups->asGroupsToCheck[psGroups->uGroupCount].sDetails.eAlign = eGroupAlign;
	psGroups->asGroupsToCheck[psGroups->uGroupCount].uSrcGroupIdx = psGroups->uGroupCount;
	psGroups->uGroupCount++;
}

static
IMG_VOID SetupSourceGroup(PINTERMEDIATE_STATE psState, PINST psInst, PREGISTER_GROUP_DESC psDetails)
/*****************************************************************************
 FUNCTION	: SetupSourceGroup

 PURPOSE	: Record details of a group of source arguments to an instruction
			  which require consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psDetails			- Subset of the source arguments.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG	psBaseArg = psInst->asArg + psDetails->uStart;

	if (psBaseArg->uType == USEASM_REGTYPE_TEMP)
	{
		MakeGroup(psState,
				  psInst->asArg + psDetails->uStart,
				  psDetails->uCount,
				  psDetails->eAlign);
	}
	else if (psBaseArg->uType == USC_REGTYPE_REGARRAY)
	{
		if (psDetails->eAlign != HWREG_ALIGNMENT_NONE)
		{
			IMG_UINT32		uVecIdx;
			PREGISTER_GROUP	psBaseGroup;

			uVecIdx = psBaseArg->uNumber;
			ASSERT(uVecIdx < psState->uNumVecArrayRegs);
			psBaseGroup = AddRegisterGroup(psState, psState->apsVecArrayReg[uVecIdx]->uBaseReg);
			SetNodeAlignment(psState, psBaseGroup, psDetails->eAlign, IMG_TRUE /* bAlignRequiredByInst */);
		}
	}
}

IMG_INTERNAL
IMG_VOID MakeGroupsForInstSources(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: MakeGroupsForInstSources

 PURPOSE	: Add restrictions on the possible hardware register number
			  assigned to the source arguments to an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to create groups for.
	
 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_GROUPS_DESC	sDesc;
	IMG_UINT32				uGroup;

	GetSourceRegisterGroups(psState, psInst, &sDesc);
	for (uGroup = 0; uGroup < sDesc.uCount; uGroup++)
	{
		SetupSourceGroup(psState, psInst, &sDesc.asGroups[uGroup]);
	}
}


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_BOOL SwapGroupForBankRestrictions(PINTERMEDIATE_STATE		psState, 
									  PINST						psInst, 
									  PGROUPS_TO_CHECK			psGroups,
									  PREGISTER_GROUP_DESC		psDetails)
/*****************************************************************************
 FUNCTION	: SwapGroupForBankRestrictions

 PURPOSE	: Try and reorder the source arguments to an instruction so the
			  register banks are supported.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psGroups			- All subsets of source arguments which require
								consecutive hardware register numbers.
			  psDetails			- Subset of the source arguments.

 RETURNS	: TRUE if the sources were swapped.
*****************************************************************************/
{
	IMG_BOOL				bValidSecondSrcGroup, bValidSecondSrcBank;
	REGISTER_GROUP_DESC		sSecondSrcSwizzledDetails;
	IMG_UINT32				uSecondGroupIdx;
	IMG_UINT32				uSecondGroupStart;
	SOP3_COMMUTE_MODE		eSOP3CommuteMode = SOP3_COMMUTE_MODE_INVALID;
	IMG_UINT32				uFirstGroupStart = psDetails->uStart;
	
	if (psInst->eOpcode == ISOP3_VEC)
	{
		PCSOURCE_ARGUMENT_PAIR	psCommutablePair;
		IMG_UINT32				uGroupIdx;

		/*
			Check for a SOP3 instruction where a pair of sources commute.
		*/
		psCommutablePair = GetCommutableSOP3Sources(psState, psInst, &eSOP3CommuteMode);
		if (psCommutablePair == NULL)
		{
			return IMG_FALSE;
		}

		/*
			Check the source 0's register bank against the banks available by the other source of the pair.
		*/
		ASSERT(psCommutablePair->uFirstSource == 0);
		uSecondGroupStart = psCommutablePair->uSecondSource;

		/*
			Map the other source to an index into the list of groups to check.
		*/
		uSecondGroupIdx = USC_UNDEF;
		for (uGroupIdx = 0; uGroupIdx < psGroups->uGroupCount; uGroupIdx++)
		{
			PREGISTER_GROUP_DESC	psOtherGroup = &psGroups->asGroupsToCheck[uGroupIdx].sDetails;

			if (psOtherGroup->uStart == psCommutablePair->uSecondSource)
			{
				uSecondGroupIdx = uGroupIdx;
				break;
			}
		}

		/*
			Restrictions on SOP3 arguments can't be modified by changing the
			swizzles.
		*/
		sSecondSrcSwizzledDetails = *psDetails;
	}
	else if (VectorSources12Commute(psInst))
	{
		/*
			For simplicity don't swap the sources to VTEST because of the interdependence
			on the swizzles on different sources.
		*/
		if (IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst))
		{
			return IMG_FALSE;
		}

		/*
			Check the register bank in the second source.
		*/
		uSecondGroupIdx = 1;
		uSecondGroupStart = VEC_MOESOURCE_ARGINDEX(1);
		
		/*
			Get the restrictions on the source arguments in the second source.
		*/
		AdjustRegisterGroupForSwizzle(psState, 
									  psInst, 
									  1 /* uDestSwizzleSlot */, 
									  &sSecondSrcSwizzledDetails, 
									  psDetails);
		ASSERT(sSecondSrcSwizzledDetails.uStart == VEC_MOESOURCE_ARGINDEX(0));
	}
	else 
	{
		return IMG_FALSE;
	}

	/*
		Would this be a valid group in the second source?
	*/
	IsValidRegisterGroup(psState,
						 psInst,
						 uSecondGroupStart,
						 psInst->asArg + sSecondSrcSwizzledDetails.uStart,
						 sSecondSrcSwizzledDetails.uCount,
						 sSecondSrcSwizzledDetails.eAlign,
						 &bValidSecondSrcGroup,
						 &bValidSecondSrcBank);
	if (bValidSecondSrcGroup && bValidSecondSrcBank)
	{
		if (psInst->eOpcode == ISOP3_VEC)
		{
			/*
				Update the SOP3 calculation to swap the two sources.
			*/
			CommuteSOP3Sources(psState, psInst, eSOP3CommuteMode);
		}
		else
		{
			/*
				Check other restrictions on swapping the sources and if possible swap them.
			*/
			if (!TrySwapVectorSources(psState, psInst, 0 /* uArg1Idx */, 1 /* uArg2Idx */, IMG_TRUE /* bCheckSwizzles */))
			{
				return IMG_FALSE;
			}
		}
		
		/*
			Flag the variables in the first group as requiring consecutive hardware register
			numbers.
		*/
		sSecondSrcSwizzledDetails.uStart = uSecondGroupStart;
		SetupSourceGroup(psState, psInst, &sSecondSrcSwizzledDetails);

		/*
			Check what was the second group (now moved to the old position of the first group)
			on the next iteration.
		*/
		if (uSecondGroupIdx != USC_UNDEF)
		{
			ASSERT(uSecondGroupIdx < psGroups->uGroupCount);
			psGroups->asGroupsToCheck[uSecondGroupIdx].uSrcGroupIdx = 0;
			psGroups->asGroupsToCheck[uSecondGroupIdx].sDetails.uStart = uFirstGroupStart;
		}

		return IMG_TRUE;
	}

	return IMG_FALSE;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID SetupRegisterGroupsInstSource(PINTERMEDIATE_STATE	psState,
									   PEQUIV_SRC_DATA		psEquivSrcData,
									   PINST				psInst)
/*****************************************************************************
 FUNCTION	: SetupRegisterGroupsInstSource

 PURPOSE	: Form mandatory groups for an instruction which requires its sources
			  to have consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Information about move instructions inserted to
								fix previous instructions.
			  psInst			- Instruction to form mandatory groups for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uGroupIdx;
	GROUPS_TO_CHECK	sGroups;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
	{
		/*
			Try to update the instruction's source swizzles so there are no unreferenced
			channels at the start of the source vector.
		*/
		NormaliseSwizzlesInst(psState, psInst);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Get details of all the groups of arguments to this instruction which require
		consecutive hardware register numbers.
	*/
	sGroups.uGroupCount = 0;
	ProcessSourceRegisterGroups(psState, psInst, StoreGroupsToCheckCB, (IMG_PVOID)&sGroups);

	/*
		Check if no group of sources to the instruction have special restrictions.
	*/
	if (sGroups.uGroupCount == 0)
	{
		return;
	}

	/*
		For each type of argument that needs consecutive registers.
	*/
	for (uGroupIdx = 0; uGroupIdx < sGroups.uGroupCount; uGroupIdx++)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IMG_UINT32				uSrcGroupIdx = sGroups.asGroupsToCheck[uGroupIdx].uSrcGroupIdx;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		PREGISTER_GROUP_DESC	psDetails = &sGroups.asGroupsToCheck[uGroupIdx].sDetails;
		REGISTER_GROUP_DESC		sSwizzledDetails;
		IMG_BOOL				bValidGroup;
		IMG_BOOL				bValidBank;

		if (psDetails->uCount == 0)
		{
			continue;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
		{
			/*
				Check if we could remove some of the restrictions by modifying the instruction's
				swizzle.
			*/
			AdjustRegisterGroupForSwizzle(psState, psInst, uSrcGroupIdx, &sSwizzledDetails, psDetails);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			sSwizzledDetails = *psDetails;
		}

		/*
			Check if the source arguments can be given hardware register with the required
			restrictions.
		*/
		IsValidRegisterGroup(psState,
							 psInst,
							 sSwizzledDetails.uStart,
							 psInst->asArg + sSwizzledDetails.uStart,
							 sSwizzledDetails.uCount,
							 sSwizzledDetails.eAlign,
							 &bValidGroup,
							 &bValidBank);

		if (bValidGroup && bValidBank)
		{
			/*
				For temporaries record the required hardware register numbers.
			*/
			SetupSourceGroup(psState, psInst, &sSwizzledDetails);

			continue;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Check for instructions where only the register bank is unsupported. 
		*/
		if (uGroupIdx == 0 && bValidGroup && !bValidBank)
		{
			ASSERT(uSrcGroupIdx == 0);		

			/*
				If the first source to the instruction commutes with another source then check
				whether the register bank would be supported in the other source.
			*/
			if (SwapGroupForBankRestrictions(psState, psInst, &sGroups, psDetails))
			{
				continue;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		if (psInst->asArg[psDetails->uStart].uType != USEASM_REGTYPE_TEMP)
		{
			/*
				Try to make use of ranges of hardware constants with the same value.
			*/
			if (TryFixNonConsecutiveFPConstants(psState, psInst, &sSwizzledDetails))
			{
				continue;
			}
			/*
				Try replacing immediate data by secondary attributes.
			*/
			if (ReplaceFPConstantsBySecAttrs(psState, psInst, &sSwizzledDetails))
			{
				continue;
			}
		}
		else
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			/*
				For VMOV we might be able to change to another copy instructions with less
				restrictions on the ordering/alignment of the source arguments.
			*/
			if (psInst->eOpcode == IVMOV && FixVMOVSourceGroup(psState, psInst, &sSwizzledDetails))
			{
				continue;
			}

			/*
				Try updating the instruction swizzle to match earlier uses of the same arguments
				in a different order.
			*/
			if (UpdateSwizzleToMatchExistingGroup(psState, psInst, &sSwizzledDetails))
			{
				continue;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		}

		/*
			Try replacing just immediate sources by temporary registers containing the same data.
		*/
		if (ReplaceNonTemporarySourcesByTemporaries(psState, psEquivSrcData, psInst, &sSwizzledDetails))
		{
			continue;
		}

		/*
			Replace all the sources by temporary registers containing the same data.
		*/
		ReplaceSourcesByTemporaries(psState,
									psEquivSrcData,
									psInst,
									&sSwizzledDetails,
									(1U << sSwizzledDetails.uCount) - 1,
									bValidGroup);
	}
}


IMG_INTERNAL
IMG_VOID SetupRegisterGroupsInst(PINTERMEDIATE_STATE	psState,
								 PEQUIV_SRC_DATA		psEquivSrcData,
								 PINST					psInst)
/*****************************************************************************
 FUNCTION	: SetupRegisterGroupsInst

 PURPOSE	: Form mandatory groups for an instruction which requires its arguments 
			  to have consecutive hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psEquivSrcData	- Information about move instructions inserted to
								fix previous instructions.
			  psInst			- Instruction to form mandatory groups for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (SetupRegisterGroupsInstDest(psState, psInst, NULL /* pbMisalignedDest */))
	{
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
	}
	else
    {
		SetupRegisterGroupsInstSource(psState, psEquivSrcData, psInst);
    }
}


static
IMG_VOID SetupRegisterGroupsBP(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   IMG_PVOID			pvNULL)
/*****************************************************************************
 FUNCTION	: SetupRegisterGroupsBP

 PURPOSE	: Form mandatory groups for instructions in a basic block which
			  require their arguments to have consecutive hardware register
			  numbers.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Basic block to process.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST			psInst;
	PINST			psNextInst;
	PEQUIV_SRC_DATA	psEquivSrcData;
	IMG_UINT32		uId;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	psEquivSrcData = CreateRegisterGroupsContext(psState);

	for (psInst = psBlock->psBody, uId = 0; psInst != NULL; psInst = psNextInst, uId++)
	{
		IMG_UINT32	uDestIdx;
		IMG_BOOL	bMisalignedDest;

		psNextInst = psInst->psNext;

		/*
			Record the index of the instruction in the block.
		*/
		psInst->uId = uId;

		/*
			Check and fix any groups of consecutive registers in the instruction destinations.
		*/
		if (SetupRegisterGroupsInstDest(psState, psInst, &bMisalignedDest))
		{
		    /* Instruction has been replaced. */
			psNextInst = psInst->psNext;

			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);

		    continue;
		}

		/*
			Check and fix any groups of consecutive registers in the instruction sources.
		*/
		SetupRegisterGroupsInstSource(psState, psEquivSrcData, psInst);
		
		#if defined(OUTPUT_USPBIN)
		if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
		{
			/*
				Add to the intereference graph the extra temporaries used when the
			    USP expands the texture sample.
			*/
			if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0)
			{
				IMG_UINT32		uTempIdx;
				IMG_UINT32		uPrevTempRegNum;
				PREGISTER_GROUP	psPrevTempRegGroup;
				IMG_UINT32		uTempCount;

				uTempCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);
				uPrevTempRegNum = USC_UNDEF;
				psPrevTempRegGroup = NULL;
				for (uTempIdx = 0; uTempIdx < uTempCount; uTempIdx++)
				{
					IMG_UINT32		uTempRegNum = psInst->u.psSmp->sUSPSample.sTempReg.uNumber + uTempIdx;
					PREGISTER_GROUP	psTempRegGroup;

					psTempRegGroup = AddRegisterGroup(psState, uTempRegNum);

					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					/*
						For cores with the vector instruction make the reserved registers start on at an even
						aligned register number. This makes it easier for the USP which might need to use the
						reserved registers as the destination of an SMP unpacking to F16/F32 or in some other instruction
						which requires an even aligned destination.
					*/
					if (uTempIdx == 0 && (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
					{
						SetNodeAlignment(psState, psTempRegGroup, HWREG_ALIGNMENT_EVEN, IMG_TRUE /* bAlignRequiredByInst */);
					}
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

					/*
						Make a group so all the USP temporaries are given consecutive
						numbers.
					*/
					if (uTempIdx != 0)
					{
						IMG_BOOL	bRet;

						bRet = AddToGroup(psState,
										  uPrevTempRegNum,
										  psPrevTempRegGroup, 
										  uTempRegNum,
										  psTempRegGroup, 
										  IMG_TRUE /* bLinkedByInst */,
										  IMG_FALSE /* bOptional */);
						ASSERT(bRet);
					}
					uPrevTempRegNum = uTempRegNum;
					psPrevTempRegGroup = psTempRegGroup;
				}
			}

			/*
				Add to the intereference graph the extra temporaries used when the
			    USP expands the texture sample unpack.
			*/
			if (psInst->eOpcode == ISMPUNPACK_USP)
			{
				IMG_UINT32		uTempIdx;
				IMG_UINT32		uPrevTempRegNum;
				PREGISTER_GROUP	psPrevTempRegGroup;
				IMG_UINT32		uTempCount;

				uTempCount = GetUSPPerSampleUnpackTemporaryCount();
				uPrevTempRegNum = USC_UNDEF;
				psPrevTempRegGroup = NULL;
				for (uTempIdx = 0; uTempIdx < uTempCount; uTempIdx++)
				{
					IMG_UINT32		uTempRegNum = psInst->u.psSmpUnpack->sTempReg.uNumber + uTempIdx;
					PREGISTER_GROUP	psTempRegGroup;

					psTempRegGroup = AddRegisterGroup(psState, uTempRegNum);

					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					/*
						For cores with the vector instruction make the reserved registers start on at an even
						aligned register number. This makes it easier for the USP which might need to use the
						reserved registers as the destination of an SMP unpacking to F16/F32 or in some other instruction
						which requires an even aligned destination.
					*/
					if (uTempIdx == 0 && (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
					{
						SetNodeAlignment(psState, psTempRegGroup, HWREG_ALIGNMENT_EVEN, IMG_TRUE /* bAlignRequiredByInst */);
					}
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

					/*
						Make a group so all the USP temporaries are given consecutive
						numbers.
					*/
					if (uTempIdx != 0)
					{
						IMG_BOOL	bRet;

						bRet = AddToGroup(psState,
										  uPrevTempRegNum,
										  psPrevTempRegGroup, 
										  uTempRegNum,
										  psTempRegGroup, 
										  IMG_TRUE /* bLinkedByInst */,
										  IMG_FALSE /* bOptional */);
						ASSERT(bRet);
					}
					uPrevTempRegNum = uTempRegNum;
					psPrevTempRegGroup = psTempRegGroup;
				}
			}

			/*
				Add to the intereference graph the extra temporaries used when the
			    USP expands the texture write.
			*/
			if (psInst->eOpcode == ITEXWRITE)
			{
				IMG_UINT32		uTempIdx;
				IMG_UINT32		uPrevTempRegNum;
				PREGISTER_GROUP	psPrevTempRegGroup;
				IMG_UINT32		uTempCount;

				uTempCount = GetUSPTextureWriteTemporaryCount();
				uPrevTempRegNum = USC_UNDEF;
				psPrevTempRegGroup = NULL;
				for (uTempIdx = 0; uTempIdx < uTempCount; uTempIdx++)
				{
					IMG_UINT32		uTempRegNum = psInst->u.psTexWrite->sTempReg.uNumber + uTempIdx;
					PREGISTER_GROUP	psTempRegGroup;

					psTempRegGroup = AddRegisterGroup(psState, uTempRegNum);

					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					/*
						For cores with the vector instruction make the reserved registers start on at an even
						aligned register number. This makes it easier for the USP which might need to use the
						reserved registers as the destination of an SMP unpacking to F16/F32 or in some other instruction
						which requires an even aligned destination.
					*/
					if (uTempIdx == 0 && (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
					{
						SetNodeAlignment(psState, psTempRegGroup, HWREG_ALIGNMENT_EVEN, IMG_TRUE /* bAlignRequiredByInst */);
					}
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

					/*
						Make a group so all the USP temporaries are given consecutive
						numbers.
					*/
					if (uTempIdx != 0)
					{
						IMG_BOOL	bRet;

						bRet = AddToGroup(psState,
										  uPrevTempRegNum,
										  psPrevTempRegGroup, 
										  uTempRegNum,
										  psTempRegGroup, 
										  IMG_TRUE /* bLinkedByInst */,
										  IMG_FALSE /* bOptional */);
						ASSERT(bRet);
					}
					uPrevTempRegNum = uTempRegNum;
					psPrevTempRegGroup = psTempRegGroup;
				}
			}
		}
		#endif /* defined(OUTPUT_USPBIN) */

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (bMisalignedDest)
		{
			/*
				For an instruction writing a single intermediate register with an
				odd aligned hardware register number try to combine it with the
				instruction writing the intermediate register with a hardware
				register number one less.
			*/
			if (TryCombiningVMOVWithAlignedVMOV(psState, psInst))
			{
				continue;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			If we are overwriting a register then clear information about other registers
			which contain the same value.
		*/
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];

			if (psDest->uType == USEASM_REGTYPE_TEMP)
			{
				EQUIVALENT_SOURCE	sKey;

				sKey.uSourceType = USEASM_REGTYPE_TEMP;
				sKey.uSourceNum = psDest->uNumber;
				UscTreeRemove(psState, psEquivSrcData->psEquivSrc, &sKey, DeleteEquivSrc, psState /* pvIterData */);
			}
		}
	}

	DestroyRegisterGroupsContext(psState, psEquivSrcData);
}

static
IMG_VOID SetCopyInstSrcDest(PINTERMEDIATE_STATE	psState,
							PFIXED_REG_DATA		psFixedVReg,
							IMG_UINT32			uConsecutiveRegIdx,
							PINST				psCopyInst,
							IMG_UINT32			uDestIdx,
							IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: SetCopyInstSrcDest

 PURPOSE	: Set the source and destination to an instruction copying data
			  to fix a conflict between an intermediate register used in
			  multiple shader outputs.

 PARAMETERS	: psState				- Compiler state.
			  psFixedVReg			- Shader output.
			  uConsecutiveRegIdx	- 
			  psCopyInst			- Instruction to update.
			  uDestIdx				- Index of the destination to set.
			  uSrcIdx				- Index of the source to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uOldRegNum;
	PUSEDEF			psOldUse;
	UF_REGFORMAT	eFmt;
	ARG				sCopyTemp;

	uOldRegNum = psFixedVReg->auVRegNum[uConsecutiveRegIdx];
	psOldUse = &psFixedVReg->asVRegUseDef[uConsecutiveRegIdx];
	eFmt = psOldUse->psUseDefChain->eFmt;

	MakeNewTempArg(psState, eFmt, &sCopyTemp);

	if (psFixedVReg->aeUsedForFeedback != NULL && psFixedVReg->aeUsedForFeedback[uConsecutiveRegIdx] != FEEDBACK_USE_TYPE_NONE)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psOldUse->psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUse;

			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psUse->eType == USE_TYPE_SRC && psUse->u.psInst->eOpcode == IFEEDBACKDRIVEREPILOG)
			{
				UseDefSubstUse(psState, psOldUse->psUseDefChain, psUse, &sCopyTemp);
				break;
			}
		}
	}

	SetDestFromArg(psState, psCopyInst, uDestIdx, &sCopyTemp);
	SetSrc(psState, psCopyInst, uSrcIdx, USEASM_REGTYPE_TEMP, uOldRegNum, eFmt);

	UseDefSubstUse(psState, psOldUse->psUseDefChain, psOldUse, &sCopyTemp);
}

static
IMG_VOID InsertShaderEndMov(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedVReg, IMG_UINT32 uRegIdx, PINST psMOVInst)
/*****************************************************************************
 FUNCTION	: InsertShaderEndMov

 PURPOSE	: Insert an instruction to copy data into a shader output in
			  the appropriate block.

 PARAMETERS	: psState			- Compiler state.
			  psFixedVReg		- Shader output.
			  uRegIdx
			  psMOVInst			- Move instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK	psInsertBlock;

	psInsertBlock = GetShaderEndBlock(psState, psFixedVReg, uRegIdx);
	AppendInst(psState, psInsertBlock, psMOVInst);
}

static
IMG_VOID GroupFixedReg(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedVReg)
/*****************************************************************************
 FUNCTION	: GroupFixedReg

 PURPOSE	: Form a mandatory group for a shader input or output.

 PARAMETERS	: psState			- Compiler state.
			  psFixedVReg		- Shader input or output.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uPrevNode;
	PREGISTER_GROUP	psPrevNodeGroup;
	IMG_UINT32		uConsecutiveRegIdx;
	IMG_BOOL		bRet;
	IMG_PUINT32		auInvalid;
	IMG_PUINT32		auInputCopy;
	IMG_BOOL		bAnyInvalid;

	if (psFixedVReg->uVRegType != USEASM_REGTYPE_TEMP)
	{
		return;
	}

	/*
		Check for each intermediate register which is part of this shader input or output if it
		has already been seen as part of a different shader input or output.
	*/
	auInvalid = NULL;
	auInputCopy = NULL;
	bAnyInvalid = IMG_FALSE;
	for (uConsecutiveRegIdx = 0; 
		 uConsecutiveRegIdx < psFixedVReg->uConsecutiveRegsCount; 
		 uConsecutiveRegIdx++)
	{
		IMG_UINT32		uNode;
		PREGISTER_GROUP	psNodeGroup;
		IMG_UINT32		uPrevRegIdx;
		IMG_BOOL		bInvalid;

		bInvalid = IMG_FALSE;

		uNode = psFixedVReg->auVRegNum[uConsecutiveRegIdx];

		for (uPrevRegIdx = 0; uPrevRegIdx < uConsecutiveRegIdx; uPrevRegIdx++)
		{
			if (psFixedVReg->auVRegNum[uPrevRegIdx] == uNode)
			{
				bInvalid = IMG_TRUE;
				break;
			}
		}

		psNodeGroup = FindRegisterGroup(psState, uNode);
		if (psNodeGroup != NULL && psNodeGroup->psFixedReg != NULL)
		{
			bInvalid = IMG_TRUE;
		}

		if (bInvalid)
		{
			ASSERT(!(psNodeGroup != NULL && psNodeGroup->psFixedReg->bPrimary && !psFixedVReg->bPrimary));
			if (!bAnyInvalid)
			{
				auInvalid = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psFixedVReg->uConsecutiveRegsCount));
				memset(auInvalid, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psFixedVReg->uConsecutiveRegsCount));

				auInputCopy = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psFixedVReg->uConsecutiveRegsCount));
				memset(auInputCopy, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psFixedVReg->uConsecutiveRegsCount));

				bAnyInvalid = IMG_TRUE;
			}

			SetBit(auInvalid, uConsecutiveRegIdx, 1);
			if (psNodeGroup != NULL && !psNodeGroup->psFixedReg->bLiveAtShaderEnd)
			{
				PVREGISTER	psInput = GetVRegister(psState, USEASM_REGTYPE_TEMP, uNode);
				IMG_PVOID	pvUse;
				USEDEF_TYPE	eUseType;
				IMG_UINT32	uUseLocation;

				if (UseDefGetSingleRegisterUse(psState, psInput, &pvUse, &eUseType, &uUseLocation))
				{
					ASSERT(pvUse == psFixedVReg);
					ASSERT(eUseType == USE_TYPE_FIXEDREG);
					ASSERT(uUseLocation == uConsecutiveRegIdx);

					SetBit(auInputCopy, uConsecutiveRegIdx, 1);
				}
			}
		}
	}

	/*
		For any intermediate registers which are part of two shader inputs or outputs allocate
		new intermediate registers, insert extra instructions to copy from the old register into the new 
		and replace the old register by the new in this shader output.
	*/
	if (bAnyInvalid)
	{
		PINST				psEntryPointInst;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		PFEEDBACK_USE_TYPE	aeUsedForFeedback = psFixedVReg->aeUsedForFeedback;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			The list of shader inputs/outputs is sorted with shader inputs first. So we will
			always see conflicts for intermediate registers which are both inputs and outputs
			at a use of the intermediate register as an output.
		*/
		ASSERT(psFixedVReg->bLiveAtShaderEnd);
		uConsecutiveRegIdx = 0;
		psEntryPointInst = NULL;
		while (uConsecutiveRegIdx < psFixedVReg->uConsecutiveRegsCount)
		{
			if (GetBit(auInvalid, uConsecutiveRegIdx) == 1)
			{
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				/*
					If both this and the next shader output needed to be copied then use a VMOV
					to copy both at once.
				*/
				if (
						(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
						((psFixedVReg->sPReg.uNumber + uConsecutiveRegIdx) % 2) == 0 &&
						uConsecutiveRegIdx < (psFixedVReg->uConsecutiveRegsCount - 1) &&
						GetBit(auInvalid, uConsecutiveRegIdx + 1) == 1 &&
						(
							aeUsedForFeedback == NULL ||
							aeUsedForFeedback[uConsecutiveRegIdx] == aeUsedForFeedback[uConsecutiveRegIdx + 1]
						)
					)
				{
					PINST			psVMOVInst;
					IMG_UINT32		uRegIdx;
					IMG_BOOL		bInsertAtStart;

					bInsertAtStart = IMG_FALSE;
					if (
							psFixedVReg->bPrimary && 
							GetBit(auInputCopy, uConsecutiveRegIdx) && 
							GetBit(auInputCopy, uConsecutiveRegIdx + 1)
						)
					{
						bInsertAtStart = IMG_TRUE;
					}

					psVMOVInst = AllocateInst(psState, NULL /* psSrcLineInst */);
					SetOpcodeAndDestCount(psState, psVMOVInst, IVMOV, 2 /* uDestCount */);
					SetSrcUnused(psState, psVMOVInst, 0 /* uSrcIdx */);
					for (uRegIdx = 0; uRegIdx < 2; uRegIdx++)
					{
						SetCopyInstSrcDest(psState, 
										   psFixedVReg, 
										   uConsecutiveRegIdx + uRegIdx, 
										   psVMOVInst, 
										   uRegIdx /* uDestIdx */, 
										   1 + uRegIdx /* uSrcIdx */);
					}
					for (; uRegIdx < VECTOR_LENGTH; uRegIdx++)
					{
						SetSrcUnused(psState, psVMOVInst, 1 + uRegIdx);
					}
					psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
					psVMOVInst->u.psVec->aeSrcFmt[0] = psVMOVInst->asDest[0].eFmt;
					if (bInsertAtStart)
					{
						InsertInstAfter(psState, 
										 psState->psMainProg->sCfg.psEntry, 
										 psVMOVInst, 
										 psEntryPointInst);
						psEntryPointInst = psVMOVInst;
					}
					else
					{
						InsertShaderEndMov(psState, psFixedVReg, uConsecutiveRegIdx, psVMOVInst);
					}

					uConsecutiveRegIdx += 2;
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					PINST		psMOVInst;
					
					psMOVInst = AllocateInst(psState, NULL /* psSrcLineInst */);
					SetOpcode(psState, psMOVInst, IMOV);
					SetCopyInstSrcDest(psState, 
									   psFixedVReg, 
									   uConsecutiveRegIdx, 
									   psMOVInst, 
									   0 /* uDestIdx */, 
									   0 /* uSrcIdx */);
					if (psFixedVReg->bPrimary && GetBit(auInputCopy, uConsecutiveRegIdx))
					{
						InsertInstAfter(psState, 
										psState->psMainProg->sCfg.psEntry, 
										psMOVInst, 
										psEntryPointInst);
						psEntryPointInst = psMOVInst;
					}
					else
					{
						InsertShaderEndMov(psState, psFixedVReg, uConsecutiveRegIdx, psMOVInst);
					}

					#if defined(OUTPUT_USPBIN)
					if (
							(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0 &&
							psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL &&
							psFixedVReg == psState->sShader.psPS->psColFixedReg &&
							psFixedVReg->uNumAltPRegs > 0 
					   )
					{
						MarkShaderResultWrite(psState, psMOVInst);
					}
					#endif /* defined(OUTPUT_USPBIN) */

					uConsecutiveRegIdx++;
				}
			}
			else
			{
				uConsecutiveRegIdx++;
			}
		}

		UscFree(psState, auInputCopy);
		auInputCopy = NULL;

		UscFree(psState, auInvalid);
		auInvalid = NULL;
	}
			
	uPrevNode = USC_UNDEF;
	psPrevNodeGroup = NULL;
	for (uConsecutiveRegIdx = 0; 
		 uConsecutiveRegIdx < psFixedVReg->uConsecutiveRegsCount; 
		 uConsecutiveRegIdx++)
	{
		IMG_UINT32		uNode;
		PREGISTER_GROUP	psNodeGroup;

		uNode = psFixedVReg->auVRegNum[uConsecutiveRegIdx];

		/*
			Add register grouping information for the intermediate register.
		*/
		psNodeGroup = AddRegisterGroup(psState, uNode);

		/*
			Associate the intermediate register with the shader input/output.
		*/
		ASSERT(psNodeGroup->psFixedReg == NULL);
		psNodeGroup->psFixedReg = psFixedVReg;
		psNodeGroup->uFixedRegOffset = uConsecutiveRegIdx;

		if (uConsecutiveRegIdx != 0)
		{
			/*	
				Record that this intermediate register must be given the next hardware register after
				the previous register in the same shader input/output.
			*/
			bRet = AddToGroup(psState, 
							  uPrevNode,
							  psPrevNodeGroup, 
							  uNode,
							  psNodeGroup, 
							  IMG_FALSE /* bLinkedByInst */,
							  IMG_FALSE /* bOptional */);
			ASSERT(bRet);
		}

		/*
			Record the alignment of the hardware register number required for this node.
		*/
		if (psFixedVReg->sPReg.uNumber != USC_UNDEF)
		{
			IMG_UINT32	uPhysicalRegNum;

			uPhysicalRegNum = psFixedVReg->sPReg.uNumber + uConsecutiveRegIdx;		
			BaseSetNodeAlignmentFromRegisterNumber(psState, psNodeGroup, uPhysicalRegNum);
		}
		#if (defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)) && defined(OUTPUT_USPBIN)
		/*
			For OUTPUT_USPBIN builds targetting cores with the vector instruction set, even where the 
			hardware register numbers of the shader result in the primary attributes isn't known yet, force
			the first hardware register number to be even. This allows the patcher to change the hardware
			register type of the shader result from temporary or output (both of are always assigned hardware 
			registers starting from zero and therefore automatically have even alignment) to the primary 
			attributes. This is only needed on cores with the vector instructions where some instructions are 
			restricted to only sources or destinations with even hardware register numbers.
		*/
		else if (
					(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && 
					psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL &&
					(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0 &&
					(psFixedVReg == psState->sShader.psPS->psColFixedReg)
				)
		{	
			BaseSetNodeAlignmentFromRegisterNumber(psState, psNodeGroup, uConsecutiveRegIdx);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) && defined(OUTPUT_USPBIN) */

		uPrevNode = uNode;
		psPrevNodeGroup = psNodeGroup;
	}
}

static
IMG_VOID GroupFixedRegs(PINTERMEDIATE_STATE psState, PUSC_LIST psFixedRegList)
/*****************************************************************************
 FUNCTION	: GroupFixedRegs

 PURPOSE	: Form mandatory groups for shader inputs and outputs.

 PARAMETERS	: psState			- Compiler state.
			  psFixedRegList	- List of shader inputs or outputs to form
								groups from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psFixedRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)	
	{
		PFIXED_REG_DATA psFixedVReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

		GroupFixedReg(psState, psFixedVReg);
	}
}

static
IMG_VOID GroupArrayRegs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: GroupArrayRegs

 PURPOSE	: Form mandatory groups from register arrays and shader inputs
			  and outputs.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uVecIdx;

	/* Make mandatory groups for register arrays */
	if (psState->apsVecArrayReg != NULL)
	{
		for (uVecIdx = 0; uVecIdx < psState->uNumVecArrayRegs; uVecIdx ++)
		{
			PUSC_VEC_ARRAY_REG	psArray = psState->apsVecArrayReg[uVecIdx];
			if (psArray != NULL)
			{
				const IMG_UINT32 uNumArrayRegs = psArray->uRegs;
				IMG_UINT32 uBaseReg = psArray->uBaseReg;
				PREGISTER_GROUP psPrevNodeGroup;
				IMG_UINT32 uCtr;

				/*
					Make a group from all the registers in the array.
				*/
				psPrevNodeGroup = psArray->psGroup = AddRegisterGroup(psState, uBaseReg);
				for (uCtr = 1; uCtr < uNumArrayRegs; uCtr ++)
				{
					PREGISTER_GROUP	psNodeGroup;

					psNodeGroup = AddRegisterGroup(psState, uBaseReg + uCtr);
					AddToGroup(psState, 
							   uBaseReg + uCtr - 1,
							   psPrevNodeGroup, 
							   uBaseReg + uCtr,
							   psNodeGroup, 
							   IMG_FALSE /* bLinkedByInst */,
							   IMG_FALSE /* bOptional */);
					psPrevNodeGroup = psNodeGroup;
				}
			}
		}
	}

	/*
		Make groups for shader inputs/outputs.
	*/
	GroupFixedRegs(psState, &psState->sSAProg.sFixedRegList);
	GroupFixedRegs(psState, &psState->sFixedRegList);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For cores with the vector instruction set iterations writing more than one attribute
		require their starting destination attribute to be even aligned.
	*/
	if (
			psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0
	   )
	{
		PUSC_LIST_ENTRY	psInputListEntry;

		for (psInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext)
		{
			PPIXELSHADER_INPUT		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);

			ASSERT(psInput->sLoad.uNumAttributes == psInput->psFixedReg->uConsecutiveRegsCount);
			if (psInput->sLoad.uNumAttributes > 1)
			{
				PREGISTER_GROUP		psFirstIterationDest;

				psFirstIterationDest = FindRegisterGroup(psState, psInput->psFixedReg->auVRegNum[0]);
				SetNodeAlignment(psState, psFirstIterationDest, HWREG_ALIGNMENT_EVEN, IMG_FALSE /* bAlignRequiredByInst */);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

IMG_INTERNAL
IMG_VOID SetupRegisterGroups(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	  : SetupRegisterGroups

 PURPOSE	  : Set up information about groups of virtual registers requiring
				consecutive hardware register numbers.

 PARAMETERS	  : pvState			  - Compiler state.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	psState->uFlags2 |= USC_FLAGS2_SETUP_REGISTER_GROUPS;

	/*
		Create a tree to map virtual registers to information about any register group the virtual
		register is part of.
	*/
	psState->psGroupState = UscAlloc(psState, sizeof(*psState->psGroupState));
	psState->psGroupState->psRegisterGroups = UscTreeMake(psState, sizeof(GROUP_PAIR), CmpGroupPair); 
	InitializeList(&psState->psGroupState->sGroupHeadsList);

	/*
		Create register groups for shader inputs/outputs and for register arrays.
	*/
	GroupArrayRegs(psState);

	/*
		Create register groups for instructions which require some of their arguments to have consecutive
		hardware register numbers.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, SetupRegisterGroupsBP, IMG_FALSE /* bHandlesCALLs */, NULL /* pvUserData */);

	TESTONLY_PRINT_INTERMEDIATE("Fixed Consecutive Registers", "fix_creg", IMG_TRUE);
}

IMG_INTERNAL
IMG_VOID ReleaseRegisterGroups(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	  : ReleaseRegisterGroups

 PURPOSE	  : Free information about groups of virtual registers requiring
				consecutive hardware register numbers.

 PARAMETERS	  : pvState			  - Compiler state.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	IMG_UINT32 i;
	
	UscTreeDeleteAll(psState, psState->psGroupState->psRegisterGroups, DeleteGroupPair, (IMG_PVOID)psState);
	UscFree(psState, psState->psGroupState);
	
	for(i = 0; i < psState->uNumRegisters; i++)
	{
		PVREGISTER psElement;
		psElement = (PVREGISTER)ArrayGet(psState, psState->psTempVReg, i);
		psElement->psGroup = IMG_NULL;
	}
	for(i = 0; i < psState->uNumPredicates; i++)
	{
		PVREGISTER psElement;
		psElement = (PVREGISTER)ArrayGet(psState, psState->psPredVReg, i);
		psElement->psGroup = IMG_NULL;
	}
}

#if defined(UF_TESTBENCH)
static
IMG_VOID PrintRegisterGroupElemCB(IMG_PVOID pvState, IMG_PVOID pvBasePair)
/*****************************************************************************
 FUNCTION	  : PrintRegisterGroupElemCB

 PURPOSE	  : Print a group of registers that need consecutive register
			    numbers after register allocation.

 PARAMETERS	  : pvState			  - Compiler state.
				pvBasePair		  - Start of the group of registers.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState = (PINTERMEDIATE_STATE)pvState;
	PGROUP_PAIR			psBasePair = (PGROUP_PAIR)pvBasePair;
	PREGISTER_GROUP		psBaseNode = psBasePair->psData;
	PREGISTER_GROUP		psOtherNode;
	IMG_UINT32			uGroupCount;
	IMG_UINT32			uStringLength;
	IMG_PCHAR			pszBuf;
	IMG_PCHAR			pszTemp;

	/*
		Skip nodes we already printed.
	*/
	if (psBaseNode->psPrev != NULL || psBaseNode->psNext == NULL)
	{
		return;
	}

	/*
		Count the number of nodes in the group.
	*/
	uGroupCount = 0;
	for (psOtherNode = psBaseNode; psOtherNode != NULL; psOtherNode = psOtherNode->psNext)
	{
		uGroupCount++;
	}

	/*
		Print the complete group.
	*/
	uStringLength = 10 + uGroupCount * 19;
	pszBuf = UscAlloc(psState, uStringLength);
	pszTemp = pszBuf;
	*pszBuf = '\0';
	for (psOtherNode = psBaseNode; psOtherNode != NULL; psOtherNode = psOtherNode->psNext)
	{
		if (psOtherNode == psBaseNode)
		{
			pszTemp += sprintf(pszTemp, "(");
		}
		else
		{
			pszTemp += sprintf(pszTemp, " ->%s ", psOtherNode->psPrev->bOptional ? "(opt)" : "");
		}

		pszTemp += sprintf(pszTemp, "r%u", psOtherNode->uRegister);
	}
	pszTemp += sprintf(pszTemp, ")");
	ASSERT(strlen(pszTemp) <= uStringLength);
	
	DBG_PRINTF((DBG_TEST_DATA, "%s", pszBuf));
	
	UscFree(psState, pszBuf);
}

IMG_INTERNAL
IMG_VOID PrintRegisterGroups(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	  : PrintRegisterGroups

 PURPOSE	  : Print the groups of registers that need consecutive register
			    numbers after register allocation.

 PARAMETERS	  : psState			  - Compiler state.

 RETURNS	  : Nothing.
*****************************************************************************/
{
	DBG_PRINTF((DBG_TEST_DATA, "Register Groups"));
	DBG_PRINTF((DBG_TEST_DATA, "----------------"));
	UscTreeIterInOrder(psState, psState->psGroupState->psRegisterGroups, PrintRegisterGroupElemCB, (IMG_PVOID)psState);
	DBG_PRINTF((DBG_TEST_DATA, "------------------------------\n"));
}
#endif /* defined(UF_TESTBENCH) */

#if defined(DEBUG)
static
IMG_VOID CheckValidRegisterGroup(PINTERMEDIATE_STATE psState, PARG asArgs, IMG_UINT32 uArgCount, HWREG_ALIGNMENT eArgAlign)
/*****************************************************************************
 FUNCTION	: CheckValidRegisterGroup

 PURPOSE	: Checks that a group of arguments to an instruction which
			  require consecutive hardware register numbers are valid.

 PARAMETERS	: psState			- Compiler state.
			  asArgs			- Start of the arguments in the group.
			  uArgCount			- Count of the arguments in the group.
			  eArgAlign			- Required alignment for the hardware register
								number of the first argument in the group.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psBaseArg;
	IMG_UINT32	uArg;

	psBaseArg = &asArgs[0];

	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		PARG	psArg = &asArgs[uArg];

		/*
			Skip intermediate registers.
		*/
		if (psArg->psRegister != NULL)
		{
			continue;
		}

		ASSERT(psArg->uType == psBaseArg->uType);

		if (psArg->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			/*
				For immediate arguments check the register numbers
				are all the same.
			*/
			ASSERT(psArg->uNumber == psBaseArg->uNumber);
		}
		else if (psArg->uType == USC_REGTYPE_REGARRAY)
		{
			ASSERT(psArg->uNumber == psBaseArg->uNumber);
			ASSERT(psArg->uArrayOffset == (psBaseArg->uArrayOffset + uArg));
		}
		else
		{
			IMG_UINT32	uAlignment;

			ASSERT(psArg->uNumber == (psBaseArg->uNumber + uArg));
			uAlignment = (psArg->uNumber % 2) - (uArg % 2);
			switch (eArgAlign)
			{
				case HWREG_ALIGNMENT_EVEN: ASSERT(uAlignment == 0); break;
				case HWREG_ALIGNMENT_ODD: ASSERT(uAlignment == 1); break;
				case HWREG_ALIGNMENT_NONE: break;
				default: imgabort();
			}
		}
	}
}

static
IMG_VOID CheckValidRegisterGroupCB(PINTERMEDIATE_STATE	psState,
								   PINST				psInst,
								   IMG_BOOL				bDest,
								   IMG_UINT32			uGroupStart,
								   IMG_UINT32			uGroupCount,
								   HWREG_ALIGNMENT		eGroupAlign,
								   IMG_PVOID			pvContext)
{
	PARG	asArgs;

	PVR_UNREFERENCED_PARAMETER(pvContext);

	if (bDest)
	{
		asArgs = psInst->asDest + uGroupStart;
	}
	else
	{
		asArgs = psInst->asArg + uGroupStart;
	}

	CheckValidRegisterGroup(psState,
							asArgs,
							uGroupCount,
							eGroupAlign);
}

IMG_INTERNAL
IMG_VOID CheckValidRegisterGroupsInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CheckValidRegisterGroupsInst

 PURPOSE	: Checks that all groups of arguments to an instruction which
			  require consecutive register numbers are valid.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_NOEMIT))
	{
		return;
	}

	/*
		Check destinations.
	*/
	ProcessDestRegisterGroups(psState, psInst, CheckValidRegisterGroupCB, NULL);

	/*
		Check sources.
	*/
	ProcessSourceRegisterGroups(psState, psInst, CheckValidRegisterGroupCB, NULL);
}
#endif /* defined(DEBUG) */

IMG_INTERNAL
IMG_VOID ForAllRegisterGroups(PINTERMEDIATE_STATE psState, PROCESS_REGISTER_GROUP pfProcess, IMG_PVOID pvUserData)
/*****************************************************************************
 FUNCTION	: ForAllRegisterGroups

 PURPOSE	: Call a function for each group of virtual registers requiring
			  consecutive hardware register numbers.

 PARAMETERS	: psState		- Compiler state.
			  pfProcess		- Function to call.
			  pvUserData	- Context parameter for the function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psState->psGroupState->sGroupHeadsList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PREGISTER_GROUP	psGroup;

		psGroup = IMG_CONTAINING_RECORD(psListEntry, PREGISTER_GROUP, sGroupHeadsListEntry);
		pfProcess(psState, psGroup, pvUserData);
	}
}

/* EOF */
