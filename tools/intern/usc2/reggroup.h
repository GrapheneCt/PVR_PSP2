/*****************************************************************************
 Name           : LAYOUT.H
 
 Title          : USC Program layout interface.
 
 C Author       : Rana-Iftikhar Ahmad
 
 Created        : 21/11/2008
 
 Copyright      : 2002-2007 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.12 $

 Modifications  :

 $Log: reggroup.h $ 
 ./ 
  --- Revision Logs Removed ---  
*****************************************************************************/

#ifndef __USC_REGGROUP_H_
#define __USC_REGGROUP_H_

struct _UNIFORM_SECATTR;
struct _EQUIV_SRC_DATA;
typedef struct _EQUIV_SRC_DATA *PEQUIV_SRC_DATA;

typedef enum 
{
	DIRECTION_FORWARD,
	DIRECTION_BACKWARD,
} DIRECTION;

/* Register grouping data */
typedef struct _REGISTER_GROUP
{
	/*
		Points to the variable requiring the immediate previous hardware register number.
	*/
	struct _REGISTER_GROUP*		psPrev;
	/*
		Points to the variable requiring the immediate following hardware register number.
	*/
	struct _REGISTER_GROUP*		psNext;
	/*
		Points to the required alignment for the hardware register number for this variable.
	*/
	HWREG_ALIGNMENT				eAlign;
	/*
		TRUE if this variable requires a particular alignment because an instruction to which it is
		a source or a destination. FALSE if this variable has a fixed alignment because it is coloured
		to a fixed hardware register.
	*/
	IMG_BOOL					bAlignRequiredByInst;
	/*
		If this variable has a fixed hardware register type and/or number points to the details.
	*/
	PFIXED_REG_DATA				psFixedReg;
	IMG_UINT32					uFixedRegOffset;
	/*
		Number of the virtual register this structure corresponds to.
	*/
	IMG_UINT32					uRegister;
	/*
		TRUE if the link between this variable and the following one is optional: can be removed
		without affecting the correctness of the generated program (but performance might be reduced).
	*/
	IMG_BOOL					bOptional;
	/*
		TRUE if this variable and the one requiring the next hardware register are used together in an
		instruction requiring its arguments to have consecutive hardware register numbers.
	*/
	IMG_BOOL					bLinkedByInst;
	/*
		Entry in the list of group heads (registers with no other register requiring a lower hardware
		register number).
	*/
	USC_LIST_ENTRY				sGroupHeadsListEntry;
	/*
		Points to type-specific information about the variable.
	*/
	union
	{
		struct _UNIFORM_SECATTR*	psSA;
		struct _PIXELSHADER_INPUT*	psPSInput;
		struct _CREG_MOVE*			psCRegMove;
		IMG_PVOID					pvNULL;
	} u;	
} REGISTER_GROUP, *PREGISTER_GROUP;

extern const IMG_UINT32 g_auRequiredAlignment[];
extern const HWREG_ALIGNMENT g_aeOtherAlignment[];
IMG_BOOL CanCoalesceNodeWithQwordAlignedNode(PREGISTER_GROUP		psNode,
											 PREGISTER_GROUP		psOtherNode);
IMG_UINT32 GetNextNodeCount(struct _REGISTER_GROUP*	psNode);
IMG_UINT32 GetPrevNodeCount(struct _REGISTER_GROUP*	psNode);
IMG_BOOL AddToGroup(PINTERMEDIATE_STATE psState,
				    IMG_UINT32			uPrevNode,
					PREGISTER_GROUP		psPrevNode,
					IMG_UINT32			uNode,
					PREGISTER_GROUP		psNode,
					IMG_BOOL			bLinkedByInst,
					IMG_BOOL			bOptional);
IMG_VOID RemoveFromGroup(PINTERMEDIATE_STATE psState,
						 PREGISTER_GROUP psNode);
IMG_VOID SetNodeAlignment(PINTERMEDIATE_STATE	psState,
						  PREGISTER_GROUP		psNode,
						  HWREG_ALIGNMENT		eAlignment,
						  IMG_BOOL				bAlignRequiredByInst);
HWREG_ALIGNMENT GetNodeAlignment(PREGISTER_GROUP psGroup);
IMG_VOID SetLinkedNodeAlignments(PINTERMEDIATE_STATE	psState,
							     PREGISTER_GROUP		psNode,
								 DIRECTION				eDir,
								 IMG_BOOL				bAlignRequiredByInst);
IMG_VOID SetupRegisterGroups(PINTERMEDIATE_STATE psState);
IMG_VOID ReleaseRegisterGroups(PINTERMEDIATE_STATE psState);
PREGISTER_GROUP FindRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister);
IMG_VOID SetPrecedingSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcToSet, PARG psNextSrc);
HWREG_ALIGNMENT GetSourceArgAlignment(PINTERMEDIATE_STATE psState, PARG psArg);
IMG_VOID MakeGroupsForInstSources(PINTERMEDIATE_STATE psState, PINST psInst);
IMG_BOOL IsPrecolouredNode(PREGISTER_GROUP psNode);
IMG_BOOL IsNodeInGroup(PREGISTER_GROUP uNode);
IMG_VOID AppendToGroup(PINTERMEDIATE_STATE psState,
                       PREGISTER_GROUP psFirstNode,
                       PREGISTER_GROUP psNode,
                       IMG_BOOL bOptional);
IMG_VOID DropLinkAfterNode(PINTERMEDIATE_STATE psState, PREGISTER_GROUP psNode);
PREGISTER_GROUP AddRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister);
IMG_VOID ReleaseRegisterGroup(PINTERMEDIATE_STATE psState, IMG_UINT32 uRegister);
IMG_VOID SetGroupHardwareRegisterNumber(PINTERMEDIATE_STATE	psState,
										PREGISTER_GROUP		psGroup,
										IMG_BOOL			bIgnoredAlignmentRequirement);
IMG_VOID SetupRegisterGroupsInst(PINTERMEDIATE_STATE		psState,
								 struct _EQUIV_SRC_DATA*	psEquivSrcData,
								 PINST						psInst);
struct _EQUIV_SRC_DATA* CreateRegisterGroupsContext(PINTERMEDIATE_STATE psState);
IMG_VOID DestroyRegisterGroupsContext(PINTERMEDIATE_STATE psState, struct _EQUIV_SRC_DATA* psEquivSrcData);
PREGISTER_GROUP GetConsecutiveGroup(PREGISTER_GROUP psGroup, DIRECTION eDir);
IMG_VOID MakeGroup(PINTERMEDIATE_STATE	psState,
				   ARG					asSetArg[],
				   IMG_UINT32			uArgCount,
				   HWREG_ALIGNMENT		eGroupAlign);
IMG_VOID MakePartialDestGroup(PINTERMEDIATE_STATE	psState,
							  PINST					psInst,
							  IMG_UINT32			uGroupStart,
							  IMG_UINT32			uGroupCount,
							  HWREG_ALIGNMENT		eGroupAlign);
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_VOID FixConsecutiveRegisterSwizzlesInst(PINTERMEDIATE_STATE psState, PINST psInst);
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
#if defined(UF_TESTBENCH)
IMG_VOID PrintRegisterGroups(PINTERMEDIATE_STATE psState);
#endif /* defined(UF_TESTBENCH) */
#if defined(DEBUG)
IMG_VOID CheckValidRegisterGroupsInst(PINTERMEDIATE_STATE psState, PINST psInst);
#endif /* defined(DEBUG) */

typedef IMG_VOID (*PROCESS_REGISTER_GROUP)(PINTERMEDIATE_STATE, PREGISTER_GROUP, IMG_PVOID);
IMG_VOID ForAllRegisterGroups(PINTERMEDIATE_STATE psState, PROCESS_REGISTER_GROUP pfProcess, IMG_PVOID pvUserData);
IMG_BOOL IsValidArgumentSet(PINTERMEDIATE_STATE		psState,
							PARG					apsSet[],
							IMG_UINT32				uCount,
							HWREG_ALIGNMENT			eAlign);
IMG_BOOL IsPrecolouredToSecondaryAttribute(PREGISTER_GROUP psGroup);
IMG_UINT32 GetPrecolouredRegisterType(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uType, IMG_UINT32 uNumber);
IMG_BOOL AreAlignmentsCompatible(HWREG_ALIGNMENT	eAlignment1,
								 HWREG_ALIGNMENT	eAlignment2);
IMG_BOOL ValidGroup(PREGISTER_GROUP*			apsGroup,
					IMG_UINT32					uGroupCount,
					HWREG_ALIGNMENT				eBaseNodeAlignment);
IMG_VOID AdjustRegisterGroupForSwizzle(PINTERMEDIATE_STATE		psState,
									   PINST					psInst,
									   IMG_UINT32				uDestSwizzleSlot,
									   PREGISTER_GROUP_DESC		psOutDetails,
									   PREGISTER_GROUP_DESC		psInDetails);

#endif /* __USC_REGGROUP_H_ */

/* EOF */
