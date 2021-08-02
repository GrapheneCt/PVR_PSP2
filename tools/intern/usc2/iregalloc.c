/*************************************************************************
 * Name         : iregalloc.c
 * Title        : Allocator for Internal Registers
 * Created      : Aug 2006
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
 * $Log: iregalloc.c $
 **************************************************************************/

#include <limits.h>
#include "uscshrd.h"

/*
	Number of 40-bit internal registers available on cores without the vector instruction set.
*/
#define NUM_C10_INTERNAL_REGISTERS			(3)

#define INST_INMERGELIST					INST_LOCAL0

struct _IREGALLOC_CONTEXT;
struct _IREGALLOC_BLOCK;
struct _MERGE_CONTEXT; 

typedef IMG_BOOL (*PFN_MERGE_RESTORE_SOURCE)(PINTERMEDIATE_STATE, struct _IREGALLOC_CONTEXT*, struct _IREGALLOC_BLOCK*, struct _MERGE_CONTEXT*, PINST, IMG_UINT32, IMG_BOOL);
typedef IMG_BOOL (*PFN_MERGE_SAVE_DEST)(PINTERMEDIATE_STATE, struct _IREGALLOC_CONTEXT*, struct _IREGALLOC_BLOCK*, struct _MERGE_CONTEXT*, PINST, IMG_UINT32, IMG_UINT32, IMG_BOOL, IMG_PUINT32);
typedef IMG_BOOL (*PFN_SPLIT_IREG_DEST)(PINTERMEDIATE_STATE, struct _IREGALLOC_CONTEXT*, struct _IREGALLOC_BLOCK*, PINST, IMG_UINT32);
typedef IMG_VOID (*PFN_EXPAND)(PINTERMEDIATE_STATE, PINST);

/*
	Possible values for IREGALLOC_CONTEXT.auPartMasks.
*/
static const IMG_UINT32 g_auC10PartMasks[] = {USC_RGB_CHAN_MASK, USC_ALPHA_CHAN_MASK};
#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static const IMG_UINT32 g_auVectorPartMasks[] = {USC_XY_CHAN_MASK, USC_ZW_CHAN_MASK};
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/*
	The number of instructions needed to restore the contents of an internal register from
	temporary registers.
*/
#define IREGALLOC_SPILL_RESTORE_COST		(1)

typedef enum _INTERVAL_STATE
{
	/* The interval won't be assigned an internal register in the current block. */
	INTERVAL_STATE_UNUSED,
	/* The interval is going to be assigned an internal register in the current block. */
	INTERVAL_STATE_PENDING,
	/* The interval has been assigned an internal register in the current block. */
	INTERVAL_STATE_ASSIGNED,
	/* The interval is currently being assigned an internal register. */
	INTERVAL_STATE_CURRENT
} INTERVAL_STATE, *PINTERVAL_STATE;

typedef enum _INTERVAL_TYPE
{
	/* This interval is for, depending on the core, either a C10 vector or an F32 vector. */
	INTERVAL_TYPE_GENERAL,
	/* This interval is for an internal register already in use before the internal register allocator started. */
	INTERVAL_TYPE_EXISTING_IREG,
	/* This interval is for a carry destination or source to an integer arithmetic instruction. */
	INTERVAL_TYPE_CARRY
} INTERVAL_TYPE, *PINTERVAL_TYPE;

typedef struct _IREG_MERGE
{
	/* Points to the save or restore instruction. */
	PINST			psInst;
	/* 
		USE-DEF information for the intermediate register 
		(representing a value which requires an internal register 
		in the hardware code) to save/restore. 
	*/
	PUSEDEF_CHAIN	psIRegUseDefChain;
	/* Entry in the list of possible internal register merges. */
	USC_LIST_ENTRY	sMergeListEntry;
	/* Count of uses of the intermediate register. */
	IMG_UINT32		uUseCount;
} IREG_MERGE, *PIREG_MERGE;

/*
	Information about an intermediate register to be assigned an internal register.
*/
typedef struct _INTERVAL
{
	/*
		USC_UNDEF or the internal register assigned to this intermediate register.
	*/
	IMG_UINT32			uHwReg;

	/*
		Status of the interval in the current block.
	*/
	INTERVAL_STATE		eState;

	/*
		Type of data contained in the interval.
	*/
	INTERVAL_TYPE		eType;

	/*
		Intermediate register.
	*/
	PUSEDEF_CHAIN		psUseDefChain;

	/*
		If this intermediate register is copied to/from a non-internal register then points to the
		non-internal register.
	*/
	PUSEDEF_CHAIN		psOrigTemp;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		If this interval is defined by a VLOAD then the length of the loaded vector.
	*/
	IMG_UINT32			uOrigVectorLength;

	/*
		If this interval is defined by a VLOAD then the loaded vector.
	*/
	IMG_BOOL			bOrigVectorWasWide;
	ARG					asOrigVector[VECTOR_LENGTH];
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Points to the next reference to the intermediate register in a basic block which is still to be
		processed.
	*/
	PUSC_LIST_ENTRY		psNextRef;

	/*
		Entry in the list of intervals in the whole program to be assigned internal registers.
	*/
	USC_LIST_ENTRY		sNextRefListEntry;

	union
	{
		/*
			(if eState == INTERVAL_STATE_PENDING)
			Entry in the list of intervals still to be assigned an internal register in the current block.
		*/
		USC_LIST_ENTRY		sUsedIntervalListEntry;

		/*
			(if eState == INTERVAL_STATE_ASSIGNED)
			Entry in the list of intervals assigned an internal register in the current block.
		*/
		USC_LIST_ENTRY		sAssignedIntervalListEntry;

		/*
			(if eState == INTERVAL_STATE_UNUSED)
			Entry in the list of intervals inside MergeIRegSave which haven't been checked for
			redundant ISAVEIREG/IRESTOREIREG instructions.
		*/
		USC_LIST_ENTRY		sUncleanIntervalListEntry;
	} u;

	/*
		Entry in the list of intervals which are only used in the current block.
	*/
	USC_LIST_ENTRY		sBlockIntervalListEntry;

	/*
		Entry in the list of all intervals.
	*/
	USC_LIST_ENTRY		sIntervalListEntry;

	/*
		Entry in the list of intervals assigned to a specific internal register in the current block.
	*/
	USC_LIST_ENTRY		sHwRegIntervalListEntry;

	/*
		Mask of internal registers which can be assigned for this intermediate register.
	*/
	IMG_UINT32			uFixedHwRegMask;

	/*
		Either NULL or points to an instruction saving the contents of the intermediate register to a
		non-internal register.
	*/
	PINST				psSaveInst;

	/*
		TRUE if the instruction defining the interval is a copy from a non-internal register.
	*/
	IMG_BOOL			bDefinedByRestore;

	/*
		Either USC_UNDEF or the number of a temporary register containing a copy of the data from
		this interval. The copy temporary register will never be assigned to an internal register so
		it can be used as the source to instructions which don't support the internal register bank.
	*/
	IMG_UINT32			uCopyTempNum;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		List of instructions which are possible dual-issue locations and which use this interval
		in a source or destination argument.
	*/
	USC_LIST			sCandidateList;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		TRUE if the interval is used as the source for unwritten channels in the partial definition
		of another interval in the current basic block.
	*/
	IMG_BOOL			bUsedAsPartialDest;
} INTERVAL, *PINTERVAL;

typedef struct _INTERNAL_REGISTER
{
	/* 
		Interval currently assigned to the internal register or 
		NULL if the internal register is available. 
	*/
	PINTERVAL			psInterval;

	/*
		List of intervals assigned to this interval register (in order
		of their definition within the block).
	*/
	SAFE_LIST			sHwRegIntervalList;
} INTERNAL_REGISTER, *PINTERNAL_REGISTER;

typedef struct _IREGALLOC_BLOCK
{
	/*
		List of intervals which are referenced only in the current block.
	*/
	USC_LIST			sBlockIntervalList;
	/*
		List of intervals still to be assigned internal registers.
	*/
	USC_LIST			sUsedIntervalList;
	/*
		List of intervals which have been assigned internal registers.
	*/
	SAFE_LIST			sAssignedIntervalList;
	/*
		If TRUE then we have assigned internal registers to all intervals in the current block. 
		When merging internal register restores this allows modifications to instructions which might otherwise
		have a negative effect on internal register allocation.
	*/
	IMG_BOOL			bPostAlloc;
} IREGALLOC_BLOCK, *PIREGALLOC_BLOCK;

typedef struct _IREGALLOC_CONTEXT
{
	/*
		List of all the intervals to be assigned internal registers in blocks still to be processed.
	*/
	USC_LIST					sNextRefList;

	/*
		List of all intervals.
	*/
	USC_LIST					sIntervalList;

	/*
		Number of internal registers available for allocation.
	*/
	IMG_UINT32					uNumHwRegs;

	/*
		Bit mask with the bits corresponding to all the available internal registers set.
	*/
	IMG_UINT32					uAllHwRegMask;

	/*
		Information about each internal register available for allocation.
	*/
	PINTERNAL_REGISTER			asHwReg;

	/*
		Target specific function to try and replace a use of an internal register by a use of a
		unified store register.
	*/
	PFN_MERGE_RESTORE_SOURCE	pfMergeRestoreSource;

	/*
		Target specific function to try and replace a define of an internal register by a define of a
		unified store register.
	*/
	PFN_MERGE_SAVE_DEST			pfMergeSaveDest;

	/*
		Target specific function to split an instruction writing an internal register into multiple
		instructions to allow instruction sources which are internal registers by unified store registers.
	*/
	PFN_SPLIT_IREG_DEST			pfSplitIRegDest;

	/*
		Target specific function to convert the generic SAVEIREG instruction to target specific instructions.
	*/
	PFN_EXPAND					pfExpandSave;

	/*
		Target specific function to convert the generic RESTOREIREG instruction to target specific instructions.
	*/
	PFN_EXPAND					pfExpandRestore;

	/*
		Which channels are held in each of the two temporary registers needed to hold
		the contents of an internal register.
	*/
	IMG_UINT32 const*			auPartMasks;

	/*
		Map from intermediate register numbers to interval information.
	*/
	PREGISTER_MAP				psIntervals;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Maps from the intermediate register numbers of secondary attributes containing F16 data to
		a pointer to an instruction in the secondary update program which converts that 
		secondary attribute to F32 format.
	*/
	PREGISTER_MAP				psUpconvertedSecAttrs;

	/*
		List of instructions inserted to convert uniform data from F16 or F32.
	*/
	USC_LIST					sUpconvertInstList;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		List of internal register save/restore instructions for which we are going to try and fold
		into the instructions referencing the internal register.
	*/
	PUSC_LIST					psMergeList;
} IREGALLOC_CONTEXT, *PIREGALLOC_CONTEXT;

/*
	State used when checking whether a set of vector intermediate registers could be moved from
	internal registers to unified store registers.
*/
typedef struct _MERGE_CONTEXT
{
	/*
		The number of extra secondary attributes required to do the merge.
	*/
	IMG_UINT32					uNumSecAttrsUsed;
} MERGE_CONTEXT, *PMERGE_CONTEXT;

static
PINTERVAL AddNewBlockInterval(PINTERMEDIATE_STATE	psState, 
							  PIREGALLOC_CONTEXT	psContext, 
							  PIREGALLOC_BLOCK		psBlockState, 
							  PUSEDEF_CHAIN			psTemp,
							  PUSEDEF_CHAIN			psOrigTemp);
static
IMG_VOID SubstByUnifiedStore(PINTERMEDIATE_STATE	psState,
							 PIREGALLOC_CONTEXT		psContext,
							 PIREGALLOC_BLOCK		psBlockState,
							 PCODEBLOCK				psBlock,
							 PINTERVAL				psInterval);
static
IMG_BOOL CanSubstByUnifiedStore(PINTERMEDIATE_STATE	psState,
								PIREGALLOC_CONTEXT	psContext,
								PIREGALLOC_BLOCK	psBlockState,
								PCODEBLOCK			psBlock,
								PINTERVAL			psInterval);
static
IMG_VOID DropInterval(PINTERMEDIATE_STATE	psState,
					  PIREGALLOC_CONTEXT	psContext,
					  PIREGALLOC_BLOCK		psBlockState, 
					  PINTERVAL				psIntervalToDrop);
static
IMG_VOID AddToUsedIntervalList(PINTERMEDIATE_STATE psState, PIREGALLOC_BLOCK psBlockState, PINTERVAL psInterval);

static
IMG_VOID GetBaseInterval(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psBlock,
						 PUSEDEF_CHAIN			psIntervalUseDefChain,
						 PINST					psRangeStartInst,
						 PUSEDEF_CHAIN*			ppsBaseTemp,
						 PINST*					ppsBaseDefInst,
						 IMG_PUINT32			puBaseDefDestIdx);
static
IMG_BOOL BaseSubstIRegInterval(PINTERMEDIATE_STATE	psState, 
							   PIREGALLOC_CONTEXT	psContext, 
							   PIREGALLOC_BLOCK		psBlockState,
							   PUSEDEF_CHAIN		psIntervalUseDefChain,
							   PUSEDEF_CHAIN		psBaseTemp,
							   PINST				psBaseDefInst,
							   IMG_UINT32			uBaseDefDestIdx,
							   IMG_UINT32			uBaseDefSrcMask,
							   PINST				psRangeStartInst,
							   PINST				psRangeEndInst,
							   IMG_BOOL				bCheckOnly,
							   IMG_PINT32			piMergeExtraInsts,
							   IMG_PUINT32			puMergeExtraSecAttrs,
							   IMG_UINT32			uAlreadyUsedSecAttrs);

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
PUSEDEF_CHAIN TempFromListEntryVec(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY	psListEntry)
{
	PUSEDEF_CHAIN	psTemp;

	psTemp = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sVectorTempListEntry);
	if (psTemp->uType != USEASM_REGTYPE_TEMP)
	{
		return NULL;
	}

	/*
		For shaders with invariant outputs don't allocate internal registers to F16 precision data. 
		Internal registers are always stored at F32 precision so this would change the precision of the
		shader.
	*/
	ASSERT(psTemp->eFmt == UF_REGFORMAT_F16 || psTemp->eFmt == UF_REGFORMAT_F32);
	if (psState->bInvariantShader && psTemp->eFmt == UF_REGFORMAT_F16)
	{
		return NULL;
	}

	return psTemp;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
PUSEDEF_CHAIN TempFromListEntryC10(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY	psListEntry)
{
	PUSEDEF_CHAIN	psTemp;

	PVR_UNREFERENCED_PARAMETER(psState);

	psTemp = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sC10TempListEntry);
	if (psTemp->uType != USEASM_REGTYPE_TEMP)
	{
		return NULL;
	}
	return psTemp;
}

static
IMG_UINT32 UseDefToRef(PINTERMEDIATE_STATE psState, PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefToRef
    
 PURPOSE	: Convert a use or define by an instruction to an integer ID by
			  ease of comparison.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psUseDef			- Use or define.
			  
 RETURNS	: The integer ID.
*****************************************************************************/
{
	IMG_UINT32	uRef;
	PINST		psRefInst;

	ASSERT(UseDefIsInstUseDef(psUseDef));
	psRefInst = psUseDef->u.psInst;

	uRef = psRefInst->uBlockIndex * 2;
	if (psUseDef->eType == DEF_TYPE_INST)
	{
		uRef++;
	}
	return uRef;
}

static
IMG_INT32 GetSaveInstCount(PIREGALLOC_CONTEXT	psContext,
						   IMG_UINT32			uChanMask)
/*****************************************************************************
 FUNCTION	: GetSaveInstCount
    
 PURPOSE	: Get the number of instructions required to save a mask of
			  channels from an internal register.

 PARAMETERS	: psContext			- Internal register allocator context.
			  uChanMask			- The mask of channels to save.
			  
 RETURNS	: The count of instructions.
*****************************************************************************/
{
	IMG_INT32	iSaveInstCount;
	IMG_UINT32	uPart;

	iSaveInstCount = 0;
	for (uPart = 0; uPart < 2; uPart++)
	{
		if ((uChanMask & psContext->auPartMasks[uPart]) != 0)
		{
			iSaveInstCount++;
		}
	}
	return iSaveInstCount;
}


static
IMG_BOOL IsRestoreInst(IOPCODE eOpcode)
/*****************************************************************************
 FUNCTION     : IsRestoreInst
    
 PURPOSE      : Check for an opcode representing a load of data into an
				internal register.

 PARAMETERS   : eOpcode		- Opcode to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (eOpcode == IVLOAD)
	{
		return IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (eOpcode == IRESTOREIREG || eOpcode == IRESTORECARRY)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsMergeableRestoreInst(PINTERMEDIATE_STATE psState, PINTERVAL psInterval, PINST psDefInst)
/*****************************************************************************
 FUNCTION     : IsMergeableRestoreInst
    
 PURPOSE      : Check for cases where an instruction to load data into an internal
				register can be removed if the destination is replaced by a 
				non-internal register.

 PARAMETERS   : psState		- Compiler state.
				psInterval	- Interval to check.
				psDefInst	- Instruction defining the interval.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	/*	
		Check if the interval is defined by an instruction loading data into
		an internal register.
	*/
	if (!IsRestoreInst(psDefInst->eOpcode))
	{
		return IMG_FALSE;
	}
	/*
		If the source data is defined in the secondary update program but the destination is
		used in the partial definition of another intermediate register in the main program then
		we won't be able to coalesce the two registers and will always need an extra move even if
		we merge the restore instruction.
	*/
	if (psInterval->bUsedAsPartialDest && psDefInst->psBlock->psOwner->psFunc != psState->psSecAttrProg)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psDefInst->eOpcode == IVLOAD)
		{
			if (IsUniformVectorSource(psState, psDefInst, 0 /* uSrcIdx */))
			{
				return IMG_FALSE;
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			if (IsUniformSource(psState, &psDefInst->asArg[0]))
			{
				return IMG_FALSE;
			}
		}	
	}
	return IMG_TRUE;
}

static
IMG_VOID DropFromMergeList(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PINST psInst)
/*****************************************************************************
 FUNCTION     : DropFromMergeList
    
 PURPOSE      : If an instruction is part of the list of instructions to try
				and merge then remove it from the list.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psInst			- Instruction to drop.

 RETURNS      : Nothing.
*****************************************************************************/
{
	if (GetBit(psInst->auFlag, INST_INMERGELIST))
	{
		PIREG_MERGE	psMergeData;

		psMergeData = (PIREG_MERGE)psInst->sStageData.pvData;
		ASSERT(psMergeData->psInst == psInst);

		RemoveFromList(psContext->psMergeList, &psMergeData->sMergeListEntry);

		SetBit(psInst->auFlag, INST_INMERGELIST, 0);

		UscFree(psState, psMergeData);
		psInst->sStageData.pvData = NULL;
	}
}

static
IMG_BOOL IsReferenceInBlock(PUSEDEF psRef, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION     : IsReferenceInBlock
    
 PURPOSE      : Check if a use or a define is in a instruction in a specified
				basic block.

 PARAMETERS   : psRef		- Use or define to check.
			    psBlock		- Block to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	if (!UseDefIsInstUseDef(psRef))
	{
		return IMG_FALSE;
	}
	if (psRef->u.psInst->psBlock != psBlock)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
PINTERVAL GetInterval(PIREGALLOC_CONTEXT psContext, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION     : GetInterval
    
 PURPOSE      : Get the data structure for an instruction argument which is
				going to be assigned to an internal register.

 PARAMETERS   : psContext		- Internal register allocator context.
				uNumber			- Temporary register.

 RETURNS      : Either a pointer to the data structure or NULL if the argument
			    is not going to be assigned to an internal register.
*****************************************************************************/
{
	return IntermediateRegisterMapGet(psContext->psIntervals, uNumber);
}

static
PINTERVAL GetIntervalByArg(PIREGALLOC_CONTEXT psContext, PARG psArg)
/*****************************************************************************
 FUNCTION     : GetIntervalByArg
    
 PURPOSE      : Get the data structure for an instruction argument which is
				going to be assigned to an internal register.

 PARAMETERS   : psContext		- Internal register allocator context.
				psArg			- Temporary register.

 RETURNS      : Either a pointer to the data structure or NULL if the argument
			    is not going to be assigned to an internal register.
*****************************************************************************/
{
	if (psArg->uType != USEASM_REGTYPE_TEMP)
	{
		return NULL;
	}
	if (psContext == NULL)
	{
		return NULL;
	}

	return GetInterval(psContext, psArg->uNumber);
}

static
IMG_VOID InitializeInterval(PINTERMEDIATE_STATE psState, 
							PIREGALLOC_CONTEXT	psContext, 
							PINTERVAL			psInterval, 
							PUSEDEF_CHAIN		psTemp,
							PUSEDEF_CHAIN		psOrigTemp)
/*****************************************************************************
 FUNCTION     : InitializeInterval
    
 PURPOSE      : Initializes the data structure for a temporary register which
			    is going to be assigned to an internal register.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psInterval		- Interval data structure to initialize.
				psTemp			- Temporary register.
				psOrigTemp		- If the temporary register is a copy of 
								another temporary register then points to
								the copy.

 RETURNS      : Nothing.
*****************************************************************************/
{
	psInterval->uHwReg = USC_UNDEF;
	psInterval->uFixedHwRegMask = psContext->uAllHwRegMask;
	psInterval->psUseDefChain = psTemp;
	psInterval->psOrigTemp = psOrigTemp;
	psInterval->psSaveInst = NULL;
	psInterval->bDefinedByRestore = IMG_FALSE;
	psInterval->eState = INTERVAL_STATE_UNUSED;
	psInterval->eType = INTERVAL_TYPE_GENERAL;
	psInterval->uCopyTempNum = USC_UNDEF;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	InitializeList(&psInterval->sCandidateList);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	psInterval->bUsedAsPartialDest = IMG_FALSE;

	if (psContext->psIntervals == NULL)
	{
		/*
			Create an array mapping temporary register numbers to the interval data structure.
		*/
		psContext->psIntervals = IntermediateRegisterMapCreate(psState);
	}
	IntermediateRegisterMapSet(psState, psContext->psIntervals, psTemp->uNumber, psInterval);
}

static IMG_BOOL IsIRegArgument(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PARG psArg, IMG_PUINT32 puAssignedIReg)
/*****************************************************************************
 FUNCTION     : IsIRegArgument
    
 PURPOSE      : Check if an instruction source/destination is going to be
				assigned to an internal register.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psArg			- Argument to check.
				puAssignedIReg	- If non-NULL returned the specific internal
								register assigned.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	PINTERVAL	psSourceInterval;

	if (psArg->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		if (puAssignedIReg != NULL)
		{
			*puAssignedIReg = psArg->uNumber;
		}
		return IMG_TRUE;
	}

	psSourceInterval = GetIntervalByArg(psContext, psArg);
	if (psSourceInterval != NULL && psSourceInterval->eState != INTERVAL_STATE_UNUSED)
	{
		if (puAssignedIReg != NULL)
		{
			ASSERT(psSourceInterval->eState == INTERVAL_STATE_ASSIGNED);
			ASSERT(psSourceInterval->uHwReg < psContext->uNumHwRegs);
			*puAssignedIReg = psSourceInterval->uHwReg;
		}
		return IMG_TRUE;
	}
	else
	{
		if (puAssignedIReg != NULL)
		{
			*puAssignedIReg = USC_UNDEF;
		}
		return IMG_FALSE;
	}
}

static IMG_VOID MergeIRegSources(PINTERMEDIATE_STATE psState, 
								 PINST psInst, 
								 IMG_UINT32 uSrcMask)
/*****************************************************************************
 FUNCTION     : MergeIRegSources
    
 PURPOSE      : Replace a mask of internal register sources which are copied
				from a unified store registers by the copy sources.

 PARAMETERS   : psState			- Compiler state.
				psFirstInst		- Instructions to modify.
				uSrcMask		- Mask of sources to replace.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
	{
		if ((uSrcMask & (1U << uSrc)) != 0)
		{
			PARG		psSrc = &psInst->asArg[uSrc];
			PINST		psDefInst;
			IMG_UINT32	uDestIdx;

			ASSERT(psSrc->uType == USEASM_REGTYPE_TEMP);

			psDefInst = UseDefGetDefInst(psState, psSrc->uType, psSrc->uNumber, &uDestIdx);
			
			ASSERT(psDefInst != NULL);
			ASSERT(uDestIdx == 0);
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psDefInst->eOpcode == IVLOAD)
			{
				IMG_UINT32	uSlot;

				ASSERT((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
				uSlot = uSrc / SOURCE_ARGUMENTS_PER_VECTOR;

				ASSERT(g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_VEC);

				CopyVectorSourceArguments(psState, psInst, uSlot, psDefInst, 0 /* uSrcArgIdx */);
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				ASSERT(psDefInst->eOpcode == IRESTOREIREG);
				
				SetSrcFromArg(psState, psInst, uSrc, &psDefInst->asArg[0]);
			}
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID ExpandIRegSaveVector(PINTERMEDIATE_STATE psState, PINST psSaveInst);
static
IMG_VOID UpdateCandidateList(PINTERMEDIATE_STATE psState, PINST psOldCandidate, PINST psNewCandidate);

static
IMG_VOID SetVectorSource(PINTERMEDIATE_STATE	psState,
						 PINST					psInst,
						 IMG_UINT32				uSlot,
						 IMG_BOOL				bWideVector,
						 IMG_UINT32				uVectorLength,
						 PCARG					asVector)
/*****************************************************************************
 FUNCTION     : SetVectorSource
    
 PURPOSE      : Set a source to a vector instruction.

 PARAMETERS   : psState			- Compiler state.
			    psInst			- Instruction.
				uSlot			- Source to set.
				bWideVector		- If TRUE the source is a vector sized
								intermediate register.
				uVectorLength	- Source data to set.
				asVector

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArgBase = uSlot * SOURCE_ARGUMENTS_PER_VECTOR;

	if (bWideVector)
	{
		ASSERT(uVectorLength == 1);
		SetSrcFromArg(psState, psInst, uArgBase, &asVector[0]);
	}
	else
	{
		IMG_UINT32	uArg;

		SetSrcUnused(psState, psInst, uArgBase);
		for (uArg = 0; uArg < uVectorLength; uArg++)
		{
			SetSrcFromArg(psState, psInst, uArgBase + 1 + uArg, &asVector[uArg]);
		}
		for (; uArg < MAX_SCALAR_REGISTERS_PER_VECTOR; uArg++)
		{
			SetSrcUnused(psState, psInst, uArgBase + 1 + uArg);
		}
	}
}

static
IMG_UINT32 GetEffectiveVMOVSwizzle(PINST psInst)
/*****************************************************************************
 FUNCTION     : GetEffectiveVMOVSwizzle
    
 PURPOSE      : Check the swizzle applied by a vector move instruction including
				the effect of increasing the base register number of a unified
				store source by half of a vec4.

 PARAMETERS   : psInst		- Instruction to check.

 RETURNS      : The effective swizzle.
*****************************************************************************/
{
	IMG_UINT32	uEffectiveSwizzle;
	IMG_UINT32	uChan;

	uEffectiveSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		if ((psInst->auDestMask[0] & (1U << uChan)) != 0)
		{
			IMG_UINT32		uSrcChan;
			USEASM_SWIZZLE_SEL	eChanSel;

			uSrcChan = uChan - psInst->u.psVec->uDestSelectShift;

			eChanSel = GetChan(psInst->u.psVec->auSwizzle[0], uSrcChan);
			if (GetBit(psInst->u.psVec->auSelectUpperHalf, 0) == 1)
			{
				if (eChanSel == USEASM_SWIZZLE_SEL_X)
				{
					eChanSel = USEASM_SWIZZLE_SEL_X;
				}
				else if (eChanSel == USEASM_SWIZZLE_SEL_Y)
				{
					eChanSel = USEASM_SWIZZLE_SEL_W;
				}
			}

			uEffectiveSwizzle = SetChan(uEffectiveSwizzle, uChan, eChanSel);
		}
	}

	return uEffectiveSwizzle;
}

static IMG_BOOL IsVMOV(PINST psInst)
/*****************************************************************************
 FUNCTION     : IsVMOV
    
 PURPOSE      : Check for a vector move instruction without any source modifiers
				or swizzles.

 PARAMETERS   : psInst		- Instruction to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	/*
		Check the VMOV has no source modifiers.
	*/
	if (psInst->u.psVec->asSrcMod[0].bNegate || psInst->u.psVec->asSrcMod[0].bAbs)
	{
		return IMG_FALSE;
	}

	/*
		Check the VMOV has an identity source swizzle.
	*/
	if (!CompareSwizzles(GetEffectiveVMOVSwizzle(psInst), USEASM_SWIZZLE(X, Y, Z, W), psInst->auDestMask[0]))
	{
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

static
IMG_BOOL UsesLOD(PINST psInst)
/*****************************************************************************
 FUNCTION     : UsesLOD
    
 PURPOSE      : Checks for a texture sample instruction which uses the LOD bias/replace
			    source.

 PARAMETERS   : psInst		- Instruction to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	#if defined(OUTPUT_USCHW)
	if (psInst->eOpcode == ISMPBIAS || psInst->eOpcode == ISMPREPLACE)
	{
		return IMG_TRUE;
	}
	#endif /* defined(OUTPUT_USCHW) */
	#if defined(OUTPUT_USPBIN)
	if (psInst->eOpcode == ISMPBIAS_USP || psInst->eOpcode == ISMPREPLACE_USP)
	{
		return IMG_TRUE;
	}
	#endif /* defined(OUTPUT_USPBIN) */ 
	return IMG_FALSE;
}

static
IMG_BOOL CheckTextureSampleFormatConsistency(PINTERMEDIATE_STATE	psState,
											 PIREGALLOC_CONTEXT		psContext,
											 PINST					psReader,
											 IMG_UINT32				uReplaceSrcMask,
											 UF_REGFORMAT			eSubstFormat,
											 IMG_UINT32				uArgGroupStart,
											 UF_REGFORMAT*			peCoordFormat)
/*****************************************************************************
 FUNCTION     : CheckTextureSampleFormatConsistency
    
 PURPOSE      : Check that data formats of the sources to a texture sample
				instructions will still be set consistently with the hardware
				restrictions after replacing some internal register sources
				by unified store sources.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psReader		- Instruction to check.
				uReplaceSrcMask	- Mask of sources which are to be converted
								from internal registers to temporary registers.
				eSubstFormat	- Format of the new temporary register sources.
				uArgGroupStart	- Start of the group of arguments to check.
				peCoordFormat	- 

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	UF_REGFORMAT	eCoordFormat = *peCoordFormat;
	UF_REGFORMAT	eNewFormat;
	PARG			psGroupStart;

	psGroupStart = &psReader->asArg[uArgGroupStart];
	if ((uReplaceSrcMask & (1 << uArgGroupStart)) == 0)
	{
		/*	
			If this argument is assigned an internal register then it is always interpreted
			as F32.
		*/
		if (IsIRegArgument(psState, psContext, psGroupStart, NULL /* puAssignedIReg */))
		{
			return IMG_TRUE;
		}
		eNewFormat = psGroupStart->eFmt;
	}
	else
	{
		eNewFormat = eSubstFormat;
	}

	/*
		Only the texture coordinate argument can be in C10 format.
	*/
	ASSERT(!(eNewFormat == UF_REGFORMAT_C10 && uArgGroupStart != SMP_COORD_ARG_START));

	if (eCoordFormat != UF_REGFORMAT_INVALID)
	{
		/*
			If the texture coordinate argument is in the C10 format then the LOD or gradient
			arguments must be in F16 format.
		*/
		if (eCoordFormat == UF_REGFORMAT_C10)
		{
			if (eNewFormat != UF_REGFORMAT_F16)
			{
				return IMG_FALSE;
			}
		}
		else
		{
			/*
				Otherwise the format of all arguments must match.
			*/
			if (eNewFormat != eCoordFormat)
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		*peCoordFormat = eNewFormat;
	}

	return IMG_TRUE;
}

static
IMG_UINT32 ReplaceOtherSources(PINTERMEDIATE_STATE	psState, 
							   PIREGALLOC_CONTEXT	psContext,
							   PIREGALLOC_BLOCK		psBlockState,
							   PINST				psReader, 
							   IMG_BOOL				bAllowNonVector,
							   IMG_UINT32			uSrcMask, 
							   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : ReplaceOtherSources
    
 PURPOSE      : Check which of the  sources to an instruction are defined
			    by instructions copying data from intermediate registers into
				internal registers.

 PARAMETERS   : psState			- Compiler state.
				psReader		- Instruction to check for.
				bAllowNonVector	- 
				uSrcMask		- Mask of sources to check.
				bCheckOnly		- If FALSE then replace the other sources by
								the source to the restore instructions defining them.

 RETURNS      : The mask of sources which are defined by a copy.
*****************************************************************************/
{
	IMG_UINT32	uSrc;
	IMG_UINT32	uFoldSrcMask;

	uFoldSrcMask = 0;
	for (uSrc = 0; uSrc < psReader->uArgumentCount; uSrc++)
	{
		if ((uSrcMask & (1 << uSrc)) != 0)
		{
			PARG		psSrc = &psReader->asArg[uSrc];
			PINST		psDefInst;
			IMG_UINT32	uDefDestIdx;
			IMG_BOOL	bCopy;

			if (psSrc->uType != USEASM_REGTYPE_TEMP)
			{
				continue;
			}

			/*
				Get the instruction which defines the source.
			*/
			psDefInst = UseDefGetDefInst(psState, psSrc->uType, psSrc->uNumber, &uDefDestIdx);
			if (psDefInst == NULL)
			{
				continue;
			}
					
			/*
				Check the defining instruction is just copying from temporary registers into
				an internal register.
			*/
			bCopy = IMG_FALSE;
			if (psDefInst->eOpcode == IRESTOREIREG)
			{
				bCopy = IMG_TRUE;
			}
			if (psDefInst->eOpcode == IVLOAD && bAllowNonVector)
			{
				bCopy = IMG_TRUE;
			}
			if (!bCopy)
			{
				continue;
			}
			ASSERT(uDefDestIdx == 0);
			ASSERT(NoPredicate(psState, psDefInst));
			ASSERT((GetLiveChansInArg(psState, psReader, uSrc) & ~psDefInst->auDestMask[0]) == 0);

			/*
				This source is defined by a copy into an internal register.
			*/
			uFoldSrcMask |= (1 << uSrc);

			/*
				Replace the existing source by the source to the copy instruction.
			*/
			if (!bCheckOnly)
			{
				if (psDefInst->eOpcode == IRESTOREIREG)
				{
					SetSrcFromArg(psState, psReader, uSrc, &psDefInst->asArg[0]);

					ASSERT(psDefInst->asDestUseDef != NULL &&
						   psDefInst->asDestUseDef->sUseDef.psUseDefChain != NULL &&
						   psDefInst->asDestUseDef->sUseDef.psUseDefChain->sList.psHead != NULL);

					if (psDefInst->asDestUseDef->sUseDef.psUseDefChain->sList.psHead->psNext == NULL)
					{
						/* 
							The destination of the restore is not used any more.
							Drop the interval for the restore destination & remove restore.
						*/
						PINTERVAL psRestoreInterval = GetIntervalByArg(psContext, &psDefInst->asDest[0]);
						DropInterval(psState, psContext, psBlockState, psRestoreInterval);

						if (psRestoreInterval->uHwReg != USC_UNDEF)
						{
							PINTERNAL_REGISTER	psHwReg = &psContext->asHwReg[psRestoreInterval->uHwReg];

							if (psHwReg->psInterval == psRestoreInterval)
							{
								psHwReg->psInterval = NULL;
							}
						}

						DropFromMergeList(psState, psContext, psDefInst);

						RemoveInst(psState, psDefInst->psBlock, psDefInst);
						FreeInst(psState, psDefInst);
					}
				}
				else
				{
					IMG_UINT32	uSlot;

					ASSERT(psDefInst->eOpcode == IVLOAD);

					ASSERT((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
					uSlot = uSrc / SOURCE_ARGUMENTS_PER_VECTOR;

					CopyVectorSourceArguments(psState, psReader, uSlot, psDefInst, 0 /* uSrcArgIdx */);
				}
			}
		}
	}

	return uFoldSrcMask;
}

static
IMG_BOOL CheckTextureSampleFormatChange(PINTERMEDIATE_STATE	psState,
										PIREGALLOC_CONTEXT	psContext,
										PINST				psReader,
										IMG_UINT32			uSrcMask,
										UF_REGFORMAT		eSubstFormat)
/*****************************************************************************
 FUNCTION     : CheckTextureSampleFormatChang
    
 PURPOSE      : Check whether the formats of the source arguments to a
			    texture sample instruction will still be set consistently
				when replacing an internal register source.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psReader		- Instruction to check for.
				uSrcMask		- Mask of sources to replace.
				eSubstFormat	- Format of the replacement sources.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	UF_REGFORMAT	eNewCoordFormat;


	eNewCoordFormat = UF_REGFORMAT_INVALID;
	if (!CheckTextureSampleFormatConsistency(psState, 
											 psContext,
											 psReader, 
											 uSrcMask, 
											 eSubstFormat,
											 SMP_COORD_ARG_START, 
											 &eNewCoordFormat))
	{
		return IMG_FALSE;
	}
	if (UsesLOD(psReader))
	{
		if (!CheckTextureSampleFormatConsistency(psState, 
												 psContext,
												 psReader, 
												 uSrcMask, 
												 eSubstFormat,
												 SMP_LOD_ARG_START, 
												 &eNewCoordFormat))
		{
			return IMG_FALSE;
		}
	}
	else if (psReader->u.psSmp->uGradSize > 0)
	{
		if (!CheckTextureSampleFormatConsistency(psState, 
												 psContext,
												 psReader, 
												 uSrcMask, 
												 eSubstFormat,
												 SMP_GRAD_ARG_START, 
												 &eNewCoordFormat))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL MergeTextureSampleArguments(PINTERMEDIATE_STATE	psState,
									 PIREGALLOC_CONTEXT		psContext,
									 PIREGALLOC_BLOCK		psBlockState,
									 PINST					psReader, 
									 IMG_UINT32				uSrcMask,
									 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : MergeTextureSampleArguments
    
 PURPOSE      : Check whether some internal register sources or destinations to 
				a texture sample instruction can be replaced by non-internal registers.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psBlockState	- Internal register allocator state for the current
								basic block.
				psReader		- Instruction to check for.
				uSrcMask		- Mask of sources to replace.
				bCheckOnly		- TRUE to check if merging is possible.
								  FALSE to update the instruction to allow the merges.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32		uSrc;
	UF_REGFORMAT	eSubstFormat;

	/*
		Get the format of the non-internal registers.
	*/
	eSubstFormat = UF_REGFORMAT_INVALID;
	for (uSrc = 0; uSrc < psReader->uArgumentCount; uSrc++)
	{
		if ((uSrcMask & (1 << uSrc)) != 0)
		{
			eSubstFormat = psReader->asArg[uSrc].eFmt;
			break;
		}
	}
	ASSERT(eSubstFormat == UF_REGFORMAT_F16 || eSubstFormat == UF_REGFORMAT_F32);
	
	/*
		Check the formats of the arguments will be set consistently with the hardware
		restrictions after the fold.
	*/
	if (!CheckTextureSampleFormatChange(psState, psContext, psReader, uSrcMask, eSubstFormat))
	{
		return IMG_FALSE;
	}

	/*
		The gradient sources to a 3D SMPGRAD instruction can't be merged independently so
		check if the other source is also a candidate to be merged.
	*/
	if (psReader->u.psSmp->uDimensionality == 3 && psReader->u.psSmp->uGradSize > 0)
	{
		IMG_UINT32	uGradMask;
		IMG_UINT32	uGradMergedSrcMask;
		IMG_UINT32	uGradNonMergedSrcMask;

		ASSERT(psReader->u.psSmp->uGradSize == 2);

		uGradMask = (1 << psReader->u.psSmp->uGradSize) - 1;
		uGradMask <<= SMP_GRAD_ARG_START;

		uGradMergedSrcMask = uSrcMask & uGradMask;
		uGradNonMergedSrcMask = uGradMask & ~uSrcMask;

		if (uGradMergedSrcMask != 0 && uGradNonMergedSrcMask != 0)
		{
			IMG_UINT32	uFoldableNonMergedSrcMask;

			/*
				For all the gradient sources which aren't the same as the
				destination of the current restore instruction see if they are
				also the destinations of other restore instructions.
			*/
			uFoldableNonMergedSrcMask = ReplaceOtherSources(psState,
															psContext, 
															psBlockState, 
															psReader,
															IMG_FALSE /* bAllowNonVector */,
															uGradNonMergedSrcMask, 
															bCheckOnly);
			if (uGradNonMergedSrcMask != uFoldableNonMergedSrcMask)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID ReplaceConvertedSource(PINTERMEDIATE_STATE	psState,
								PINST				psInst,
								IMG_UINT32			uSlotIdx,
								PINST				psLastConvertInst)
/*****************************************************************************
 FUNCTION     : ReplaceConvertedSource
    
 PURPOSE      : Replace an instruction source by the result of another instruction
				converting the same source from F16 to F32.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction whose source is to be replaced.
				uSlotIdx		- Source to replace.
				psLastConvertInst
								- Conversion instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PARG		psConvertResult = &psLastConvertInst->asDest[0];
	IMG_UINT32	uChansToUnpack;
	PINST		psConvertInst;
	IMG_UINT32	uConvertDestIdx;
	IMG_UINT32	uNewLiveChansInDest;

	/*
		Get the mask of channels used from the source before the swizzle is applied.
	*/
	uChansToUnpack = GetLiveChansInArg(psState, psInst, uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR);

	/*
		Replace the existing instruction source by the conversion result.
	*/
	SetSrcFromArg(psState, psInst, uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR, psConvertResult);
	psInst->u.psVec->aeSrcFmt[uSlotIdx] = UF_REGFORMAT_F32;

	/*
		Update the stored masks of channels used from the result of the conversion
		instruction.
	*/
	psConvertInst = psLastConvertInst;
	uConvertDestIdx = 0;
	uNewLiveChansInDest = uChansToUnpack;
	for (;;)
	{
		PARG	psOldDest;

		/*	
			Stop if all the channels used by the current instruction were already
			used by previous instructions.
		*/
		if ((uNewLiveChansInDest & ~psConvertInst->auLiveChansInDest[0]) == 0)
		{
			break;
		}
		psConvertInst->auLiveChansInDest[0] |= uNewLiveChansInDest;

		/*
			Get the mask of channels used from the result of this instruction but not
			written by it.
		*/
		uNewLiveChansInDest = GetPreservedChansInPartialDest(psState, psConvertInst, uConvertDestIdx);
		if (uNewLiveChansInDest == 0)
		{
			break;
		}

		/*
			Find the instruction which writes the source for the unwritten channels.
		*/
		psOldDest = psConvertInst->apsOldDest[uConvertDestIdx];
		ASSERT(psOldDest != NULL);
		ASSERT(psOldDest->uType == USEASM_REGTYPE_TEMP);

		psConvertInst = UseDefGetDefInst(psState, psOldDest->uType, psOldDest->uNumber, &uConvertDestIdx);
		ASSERT(psConvertInst != NULL);
	}

	/*
		Update the mask of channels used from the result of the conversion instruction in the main
		program.
	*/
	IncreaseRegisterLiveMask(psState,
							 &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
							 psConvertResult->uType,
							 psConvertResult->uNumber,
							 psConvertResult->uArrayOffset,
							 uChansToUnpack);
}

static
IMG_VOID GetUsedFormats(IMG_UINT32 uArgCount, VEC_NEW_ARGUMENT asArgs[], IMG_PBOOL pbF16Used, IMG_PBOOL pbF32Used)
/*****************************************************************************
 FUNCTION     : GetUsedFormats
    
 PURPOSE      : Get the formats used in a list of instruction sources and
				destinations.

 PARAMETERS   : uArgCount		- Count of sources/destinations.
				asArgs			- List of sources/destinations.
				pbF16Used		- Returns TRUE if F16 format data was used; FALSE
								otherwise.
				pbF32Used		- Retursn TRUE if F32 format data was used; FALSE
								otherwise.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	*pbF16Used = *pbF32Used = IMG_FALSE;
	for (uArg = 0; uArg < uArgCount; uArg++)
	{
		UF_REGFORMAT	eArgFormat = asArgs[uArg].eFormat;

		if (!asArgs[uArg].bIsIReg)
		{
			if (eArgFormat == UF_REGFORMAT_F16)
			{
				*pbF16Used = IMG_TRUE;
			}
			else if (eArgFormat == UF_REGFORMAT_F32)
			{
				*pbF32Used = IMG_TRUE;
			}
		}
	}
}

static
IMG_BOOL FixF16FormatSources(PINTERMEDIATE_STATE	psState, 
							 PIREGALLOC_CONTEXT		psContext,
							 PMERGE_CONTEXT			psMergeContext,
							 PVEC_NEW_ARGUMENTS		psNewArguments,
							 PINST					psInst, 
							 IMG_PBOOL				pbUpconvertedSources,
							 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : FixF16FormatSources
    
 PURPOSE      : Try to fix an instruction which uses automatic conversions
				to/from F16 (which forces F32 format sources/destinations to be
				internal registers) to use F32 format sources/destination only.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psMergeContext	- State for in the in-progress internal register
								merge.
				psInst			- Instruction to fix.
				bCheckOnly		- If TRUE just check if fixing is possible.

 RETURNS      : TRUE if F32 internal register sources/destination can be safely replaced
				by unified store registers.
*****************************************************************************/
{	
	IMG_BOOL	bDestinationsUseF16, bDestinationsUseF32;
	IMG_BOOL	bSourcesUseF16, bSourcesUseF32;
	IMG_BOOL	bArgumentsUseF16, bArgumentsUseF32;
	IMG_UINT32	uSlotIdx;

	if (pbUpconvertedSources != NULL)
	{
		*pbUpconvertedSources = IMG_FALSE;
	}

	/*
		Nothing to do if the instruction supports a per-source/destination format selection.
	*/
	if (VectorInstPerArgumentF16F32Selection(psInst->eOpcode))
	{
		return IMG_TRUE;
	}

	GetUsedFormats(psNewArguments->uDestCount, psNewArguments->asDest, &bDestinationsUseF16, &bDestinationsUseF32);
	GetUsedFormats(GetSwizzleSlotCount(psState, psInst), psNewArguments->asSrc, &bSourcesUseF16, &bSourcesUseF32);

	/*
		Check for an instruction accessing a mix of F16 and F32 sources/destinations.
	*/
	bArgumentsUseF16 = bDestinationsUseF16 || bSourcesUseF16;
	bArgumentsUseF32 = bDestinationsUseF32 || bSourcesUseF32;
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORF32ONLY) != 0)
	{
		if (!bArgumentsUseF16)
		{
			return IMG_TRUE;
		}
	}
	else
	{
		if (!(bArgumentsUseF16 && bArgumentsUseF32))
		{
			return IMG_TRUE;
		}
	}

	/*
		Only fix cases where a separate conversion from F16->F32 can be inserted in
		the secondary update program.
	*/
	if (bDestinationsUseF16)
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
		There no point to upconverting for instructions in the secondary update program
		because an upconversion instruction costs the same as a restore instruction.
	*/
	if (psInst->psBlock->psOwner->psFunc == psState->psSecAttrProg)
	{
		return IMG_FALSE;
	}

	for (uSlotIdx = 0; uSlotIdx < GetSwizzleSlotCount(psState, psInst); uSlotIdx++)
	{
		PVEC_NEW_ARGUMENT	psSrc = &psNewArguments->asSrc[uSlotIdx];

		if (psSrc->eFormat == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uResultRegCount;
			IMG_UINT32	uSecAttrTempNum;
			PINST		psDefInst;
			IMG_UINT32	uDefDestIdx;
			PARG		psVecArg = &psInst->asArg[uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR];

			ASSERT(!psSrc->bIsIReg);

			uSecAttrTempNum = USC_UNDEF;

			/*
				Update details of the new arguments to the vector instruction.
			*/
			ASSERT(!psNewArguments->asSrc[uSlotIdx].bIsIReg);
			psNewArguments->asSrc[uSlotIdx].eFormat = UF_REGFORMAT_F32;

			/*
				If the source argument is defined by a restore instruction then check the
				source to the restore.
			*/
			psDefInst = UseDefGetDefInst(psState, psVecArg->uType, psVecArg->uNumber, &uDefDestIdx);
			if (psDefInst != NULL && psDefInst->eOpcode == IRESTOREIREG)
			{
				PARG	psRestoreSrc = &psDefInst->asArg[0];

				ASSERT(uDefDestIdx == 0);

				if (!IsUniformSource(psState, psRestoreSrc))
				{
					return IMG_FALSE;
				}

				ASSERT(psRestoreSrc->uType == USEASM_REGTYPE_TEMP);
				uSecAttrTempNum = psRestoreSrc->uNumber;
			}
			else
			{
				/*
					Check the F16 source data to be swizzled is uniform.
				*/
				if (!IsUniformVectorSource(psState, psInst, uSlotIdx))
				{
					return IMG_FALSE;
				}

				if (psVecArg->uType == USEASM_REGTYPE_TEMP)
				{
					uSecAttrTempNum = psVecArg->uNumber;
				}
			}

			/*
				Check if we've already inserted an instruction to convert this source vector.
			*/
			if (uSecAttrTempNum != USC_UNDEF && psContext->psUpconvertedSecAttrs != NULL)
			{
				PINST	psExistingConvertInst;

				psExistingConvertInst = IntermediateRegisterMapGet(psContext->psUpconvertedSecAttrs, uSecAttrTempNum);
				if (psExistingConvertInst != NULL)
				{
					if (!bCheckOnly)
					{
						/*
							Replace the existing instruction source by the conversion result.
						*/
						ReplaceConvertedSource(psState, psInst, uSlotIdx, psExistingConvertInst);
					}
					continue;
				}
			}

			if (bCheckOnly)
			{
				uResultRegCount = psMergeContext->uNumSecAttrsUsed + SCALAR_REGISTERS_PER_F32_VECTOR;
			}
			else
			{
				uResultRegCount = SCALAR_REGISTERS_PER_F32_VECTOR;
			}

			/*
				Check enough secondary attributes are available to hold the result of the conversion.
			*/
			if (!CheckAndUpdateInstSARegisterCount(psState, 
												   SCALAR_REGISTERS_PER_F16_VECTOR /* uSrcRegCount */, 
												   uResultRegCount, 
												   IMG_TRUE /* bCheckOnly */))
			{
				return IMG_FALSE;
			}

			if (bCheckOnly)
			{				
				psMergeContext->uNumSecAttrsUsed += SCALAR_REGISTERS_PER_F32_VECTOR;
			}
			else
			{
				ARG			sConvertResult;
				IMG_UINT32	uPartIdx;
				ARG			sHalfResult;
				PINST		psLastConvertInst = NULL;

				/*
					Allocate a new temporary register for the converted result.
				*/
				MakeNewTempArg(psState, UF_REGFORMAT_F32, &sHalfResult);
				MakeNewTempArg(psState, UF_REGFORMAT_F32, &sConvertResult);
				
				/*
					Create separate instructions to unpack the channel X/Y and Z/W channels.
				*/
				for (uPartIdx = 0; uPartIdx < 2; uPartIdx++)
				{
					PINST		psConvertInst;
					IMG_UINT32	uArgBase;

					psConvertInst = AllocateInst(psState, psInst);

					SetOpcode(psState, psConvertInst, IVMOV);

					psConvertInst->auDestMask[0] = g_auVectorPartMasks[uPartIdx];
					psConvertInst->auLiveChansInDest[0] = 0;

					if (uPartIdx == 0)
					{
						psConvertInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
					}
					else
					{
						psConvertInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, W, Z, W);
						psConvertInst->u.psVec->uDestSelectShift = USC_Z_CHAN;
					}

					/*
						Copy the existing instruction source.
					*/
					uArgBase = uSlotIdx * SOURCE_ARGUMENTS_PER_VECTOR;
					if (uSecAttrTempNum != USC_UNDEF)
					{
						SetSrc(psState, psConvertInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSecAttrTempNum, UF_REGFORMAT_F16);
					}
					else if (psInst->asArg[uArgBase].uType != USC_REGTYPE_UNUSEDSOURCE)
					{
						CopySrc(psState, psConvertInst, 0 /* uCopyToIdx */, psInst, uArgBase);
					}
					else
					{
						IMG_UINT32	uComp;

						SetSrcUnused(psState, psConvertInst, 0 /* uSrcIdx */);
						for (uComp = 0; uComp < SCALAR_REGISTERS_PER_F16_VECTOR; uComp++)
						{
							CopySrc(psState, psConvertInst, 1 + uComp, psInst, uArgBase + 1 + uComp);
						}
					}
					psConvertInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;

					AppendInst(psState, psState->psSecAttrProg->sCfg.psExit, psConvertInst);

					AppendToList(&psContext->sUpconvertInstList, &psConvertInst->sAvailableListEntry);

					if (uPartIdx == 1)
					{
						SetPartiallyWrittenDest(psState, psConvertInst, 0 /* uDestIdx */, &sHalfResult);
						SetDestFromArg(psState, psConvertInst, 0 /* uDestIdx */, &sConvertResult);
						

						/*
							If the conversion source is a vector sized register then record the conversion so
							we can reuse the converted destination if possible.
						*/
						if (psConvertInst->asArg[0].uType == USEASM_REGTYPE_TEMP)
						{
							if (psContext->psUpconvertedSecAttrs == NULL)
							{
								psContext->psUpconvertedSecAttrs = IntermediateRegisterMapCreate(psState);
							}
							IntermediateRegisterMapSet(psState, 
													   psContext->psUpconvertedSecAttrs, 
													   psConvertInst->asArg[0].uNumber,
													   psConvertInst);
						}
					}	
					else
					{
						SetDestFromArg(psState, psConvertInst, 0 /* uDestIdx */, &sHalfResult);
					}

					psLastConvertInst = psConvertInst;
				}

				/*
					Flag the conversion result as live out of the secondary update program.
				*/
				BaseAddNewSAProgResult(psState,
									   UF_REGFORMAT_F32,
									   IMG_TRUE /* bVector */,
									   0 /* uHwRegAlignment */,
									   SCALAR_REGISTERS_PER_F32_VECTOR /* uNumHwRegisters */,
									   sConvertResult.uNumber,
									   0 /* uUsedChanMask */,
									   SAPROG_RESULT_TYPE_CALC,
									   IMG_FALSE /* bPartOfRange */);

				/*
					Replace the existing instruction source by the conversion result.
				*/
				ReplaceConvertedSource(psState, psInst, uSlotIdx, psLastConvertInst);
			}
		}
	}

	if (pbUpconvertedSources != NULL)
	{
		*pbUpconvertedSources = IMG_TRUE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL IsSimpleDestinationSubst(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION     : IsSimpleDestinationSubst
    
 PURPOSE      : Check for cases where an internal register destination can be
				replaced by a unified store destination without otherwise
				modifying the instruction.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction whose destination is to be replaced.
				uDestIdx		- Index of the destination to replace.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	if (GetPreservedChansInPartialDest(psState, psInst, uDestIdx) != 0)
	{
		return IMG_FALSE;
	}
	if ((psInst->auDestMask[uDestIdx] & psInst->auLiveChansInDest[uDestIdx] & USC_ZW_CHAN_MASK) != 0)
	{
		return IMG_FALSE;
	}
	if (InstUsesF16ToF32AutomaticConversions(psState, psInst, NULL /* pbDestIsF16 */, NULL /* puF16SrcMask */))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_UINT32 SourceMaskToSlotMask(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcMask)
/*****************************************************************************
 FUNCTION     : SourceMaskToSlotMask
    
 PURPOSE      : Convert a mask in terms of source arguments to one in terms of
				vector sources.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction to which the mask corresponds.
				uSrcMask		- Mask in terms of source arguments.

 RETURNS      : Mask in terms of vector sources.
*****************************************************************************/
{
	IMG_UINT32	uSlotMask;
	IMG_UINT32	uSlot;

	uSlotMask = 0;
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		if ((uSrcMask & (1U << (uSlot * SOURCE_ARGUMENTS_PER_VECTOR))) != 0)
		{
			uSlotMask |= (1U << uSlot);
		}
	}
	return uSlotMask;
}

static
IMG_VOID GetNewVectorParameters(PINTERMEDIATE_STATE psState, 
								PIREGALLOC_CONTEXT	psContext,
								PINST				psInst, 
								IMG_UINT32			uFoldDestIdx,
								IMG_UINT32			uFoldSrcMask,
								PVEC_NEW_ARGUMENTS	psNewArguments)
/*****************************************************************************
 FUNCTION     : GetNewVectorParameters
    
 PURPOSE      : Returns details of which source/destinations in an instruction
				will be assigned internal registers and also their formats.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction.
				uFoldDestIdx	- Either USC_UNDEF or the index of a destination
								to replace by a unified store register.
				uFoldSrcMask	- Mask of sources to replace by unified store
								registers.
				psNewArguments	- Returns details of the arguments to the modified
								instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	psNewArguments->uDestCount = 0;
	if (psInst->eOpcode != IVTESTBYTEMASK)
	{
		IMG_UINT32	uDest;

		for (uDest = 0; uDest < psInst->uDestCount; uDest++)
		{
			PVEC_NEW_ARGUMENT	psNewDest;
			PARG				psDest = &psInst->asDest[uDest];

			if (psDest->uType == USEASM_REGTYPE_PREDICATE || psDest->uType == USC_REGTYPE_UNUSEDDEST)
			{
				continue;
			}

			psNewDest = &psNewArguments->asDest[psNewArguments->uDestCount];
			if (uDest != uFoldDestIdx && IsIRegArgument(psState, psContext, psDest, NULL /* puAssignedIReg */))
			{
				psNewDest->bIsIReg = IMG_TRUE;
				psNewDest->eFormat = UF_REGFORMAT_F32;
			}
			else
			{
				psNewDest->bIsIReg = IMG_FALSE;
				psNewDest->eFormat = psDest->eFmt;
			}
			psNewArguments->uDestCount++;
		}
	}
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); uSrc++)
	{
		PVEC_NEW_ARGUMENT	psNewSrc = &psNewArguments->asSrc[uSrc];
		PARG				psSrc = &psInst->asArg[uSrc * SOURCE_ARGUMENTS_PER_VECTOR];

		if (psInst->eOpcode == IVDUAL && psInst->u.psVec->sDual.aeSrcUsage[uSrc] == VDUAL_OP_TYPE_NONE)
		{
			ASSERT((uFoldSrcMask & (1 << uSrc)) == 0);

			psNewSrc->bIsIReg = IMG_TRUE;
			psNewSrc->eFormat = UF_REGFORMAT_F32;
			continue;
		}

		if (psInst->eOpcode == IVMOVCU8_FLT && uSrc == 0)
		{
			psNewSrc->bIsIReg = IMG_FALSE;
			psNewSrc->eFormat = UF_REGFORMAT_UNTYPED;
			continue;
		}
		
		if ((uFoldSrcMask & (1U << uSrc)) == 0 && IsIRegArgument(psState, psContext, psSrc, NULL /* puAssignedIReg */))
		{
			psNewSrc->bIsIReg = IMG_TRUE;
			psNewSrc->eFormat = UF_REGFORMAT_F32;
		}
		else
		{
			psNewSrc->bIsIReg = IMG_FALSE;
			psNewSrc->eFormat = psInst->u.psVec->aeSrcFmt[uSrc];
		}
	}
}

typedef struct _SUBSTSWIZZLE_CONTEXT
{
	/*
		Swizzle to substitute.
	*/
	IMG_UINT32			uSubstSwizzle;
	/*
		If non-NULL don't substitute in instructions in a different basic block.
	*/
	PCODEBLOCK			psSubstBlock;
	/*
		If not USC_UNDEF don't substitute in instructions with a higher uBlockIndex.
	*/
	IMG_UINT32			uLastSubstPoint;
	/*
		TRUE if the source to substitute is an internal register.
	*/
	IMG_BOOL			bSourceIsIReg;
	/*
		Format of the source to substitute.
	*/
	UF_REGFORMAT		eSourceFormat;
	PIREGALLOC_CONTEXT	psAllocContext;
} SUBSTSWIZZLE_CONTEXT, *PSUBSTSWIZZLE_CONTEXT;

static
IMG_BOOL SubstSwizzle(PINTERMEDIATE_STATE	psState,
					  PCODEBLOCK			psCodeBlock,
					  PINST					psInst,
					  PARGUMENT_REFS		psArgRefs,
					  PARG					psRegA,
					  PARG					psRegB,
					  IMG_PVOID				pvSubstContext,
					  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : SubstSwizzle
    
 PURPOSE      : Try to substitute a swizzle into a set of instruction sources.

 PARAMETERS   : psState			- Compiler state.
			    psCodeBlock		- Basic block containing the instruction.
				psInst			- Instruction.
				psArgRefs		- Sources to substitute into.
				psRegA
				psRegB
				pvSubstContext	- Points to the swizzle to substitute.
				bCheckOnly		- If TRUE just check if substitution is possible.
								  If FALSE modify the instruction to perform
								  the substitution.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32				uSrc;
	IMG_UINT32				uSlot;
	IMG_UINT32				auNewPreSwizzleLiveChans[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32				auNewSwizzleSet[VECTOR_MAX_SOURCE_SLOT_COUNT];
	PSUBSTSWIZZLE_CONTEXT	psSubstContext = (PSUBSTSWIZZLE_CONTEXT)pvSubstContext;
	IMG_UINT32				uSubstSwizzle = psSubstContext->uSubstSwizzle;
	IMG_BOOL				abNewSelectUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
	VEC_NEW_ARGUMENTS		sNewArguments;
	IMG_BOOL				bRet;
	INST_MODIFICATIONS		sInstMods;

	PVR_UNREFERENCED_PARAMETER(psRegB);
	PVR_UNREFERENCED_PARAMETER(psRegA);
	PVR_UNREFERENCED_PARAMETER(psCodeBlock);

	/*
		When substituting an internal register don't substitute outside of the current block.
	*/
	if (psSubstContext->psSubstBlock != NULL && psInst->psBlock != psSubstContext->psSubstBlock)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	/*
		Don't substitute uses after another define of the source internal register.
	*/
	if (psSubstContext->uLastSubstPoint != USC_UNDEF)
	{
		if (psInst->uBlockIndex > psSubstContext->uLastSubstPoint)
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	/*
		Check for using the move destination other than as a source.
	*/
	if (psArgRefs->bUsedAsIndex || psArgRefs->bUsedAsPartialDest)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		We can apply a limited 'swizzle' to the coordinate source to a texture sample instruction
		by incrementing the base register number by two scalar registers.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0)
	{
		IMG_UINT32	uDim = psInst->u.psSmp->uDimensionality;
		IMG_UINT32	uDimMask = (1 << uDim) - 1;
		IMG_UINT32	uNewSwizzle;
		IMG_UINT32	uOrigSwizzle;

		/*
			Check the move destination is used only in the coordinate source.
		*/
		if (!CheckSourceVector(&psArgRefs->sSourceVector, SMP_COORD_ARG_START))
		{
			return IMG_FALSE;
		}
		for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
		{
			if (uSrc != SMP_COORD_ARG_START && CheckSourceVector(&psArgRefs->sSourceVector, uSrc))
			{
				return IMG_FALSE;
			}
		}

		/*
			Check that the formats of the source arguments will still be set consistently after
			the substitution.
		*/
		if (!CheckTextureSampleFormatChange(psState, 
											psSubstContext->psAllocContext, 
											psInst, 
											1 << SMP_COORD_ARG_START,
											psSubstContext->eSourceFormat))
		{
			return IMG_FALSE;
		}

		/*
			Get the swizzle currently applied to the texture coordinate source.
		*/
		if (psInst->u.psSmp->bCoordSelectUpper)
		{
			uOrigSwizzle = USEASM_SWIZZLE(Z, W, 0, 0);
		}
		else
		{
			uOrigSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
		}

		/*
			Combine the swizzle from the move instruction and the current swizzle.
		*/
		uNewSwizzle = CombineSwizzles(uSubstSwizzle, uOrigSwizzle);

		/*
			Check for a swizzle which is selecting either the bottom or the top
			half of the source.
		*/
		if (CompareSwizzles(uNewSwizzle, USEASM_SWIZZLE(X, Y, Z, W), uDimMask))
		{
			if (!bCheckOnly)
			{
				psInst->u.psSmp->bCoordSelectUpper = IMG_FALSE;
			}
			return IMG_TRUE;
		}
		else
		{
			if (
					uDim <= 2 && 
					CompareSwizzles(uNewSwizzle, USEASM_SWIZZLE(Z, W, 0, 0), uDimMask) && 
					psSubstContext->eSourceFormat == UF_REGFORMAT_F32
			   )
			{
				if (!bCheckOnly)
				{
					psInst->u.psSmp->bCoordSelectUpper = IMG_TRUE;
				}
				return IMG_TRUE;
			}
		}

		return IMG_FALSE;
	}

	/*
		Check the move destination is used only in instructions which
		support a source swizzle.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Set the default new instruction swizzles from the existing ones.
	*/
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		auNewSwizzleSet[uSlot] = psInst->u.psVec->auSwizzle[uSlot];
		abNewSelectUpper[uSlot] = GetBit(psInst->u.psVec->auSelectUpperHalf, uSlot) ? IMG_TRUE : IMG_FALSE;
	}

	/*
		Set up the details of which the destinations/sources will be
		internal registers and their formats in the modified instruction.
	*/
	GetNewVectorParameters(psState, 
						   psSubstContext->psAllocContext, 
						   psInst, 
						   USC_UNDEF /* uFoldDestIdx */,
						   0 /* uFoldSrcMask */,
						   &sNewArguments);

	/*
		For all instruction sources which are the move destination create a new swizzle
		which combines the effects of the move swizzle and the existing source swizzle.
	*/
	for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
	{
		if (CheckSourceVector(&psArgRefs->sSourceVector, uSrc))
		{
			IMG_UINT32			uOrigSwizzle;
			PVEC_NEW_ARGUMENT	psNewSrc;

			ASSERT((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
			uSlot = uSrc / SOURCE_ARGUMENTS_PER_VECTOR;

			psNewSrc = &sNewArguments.asSrc[uSlot];

			if (!psSubstContext->bSourceIsIReg)
			{
				psNewSrc->bIsIReg = IMG_FALSE;
				psNewSrc->eFormat = psSubstContext->eSourceFormat;
			}
			else
			{
				psNewSrc->bIsIReg = IMG_TRUE;
				psNewSrc->eFormat = UF_REGFORMAT_F32;
			}

			/*
				Get the swizzle currently applied by the instruction.
			*/
			uOrigSwizzle = auNewSwizzleSet[uSlot];

			/*
				If we are increasing the register number of the start of the vector
				by two scalar registers this is the same as swizzling X to Z and Y to W.
			*/
			if (abNewSelectUpper[uSlot])
			{
				uOrigSwizzle = CombineSwizzles(USEASM_SWIZZLE(Z, W, 0, 0), uOrigSwizzle);
			}

			/*
				Combine the existing swizzle and the one from the move instruction.
			*/
			auNewSwizzleSet[uSlot] = CombineSwizzles(uSubstSwizzle, uOrigSwizzle);

			/*
				Clear modifications to the register number by default in the modified
				instruction. We might need to reenable it if this source doesn't
				support accessing a vec4.
			*/
			abNewSelectUpper[uSlot] = IMG_FALSE;
		}
	}

	/*
		Record the masks of channels used from each source in the new
		instruction.
	*/
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); uSrc++)
	{
		GetLiveChansInSourceSlot(psState, 
								 psInst, 
								 uSrc, 
								 &auNewPreSwizzleLiveChans[uSrc], 
								 NULL /* puChansUsedPostSwizzle */);
	}

	/*
		Check if the instruction would still be valid with extra internal
		register sources replaced by temporary registers.
	*/
	bRet = IsValidModifiedVectorInst(psState,
									 IMG_TRUE /* bPostAlloc */,
									 psInst,
									 auNewPreSwizzleLiveChans,
									 &sNewArguments,
									 auNewSwizzleSet,
									 abNewSelectUpper,
									 &sInstMods);
	if (!bCheckOnly)
	{
		ASSERT(bRet);

		/*
			Update source formats.
		*/
		for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
		{
			if (CheckSourceVector(&psArgRefs->sSourceVector, uSrc))
			{
				IMG_UINT32	uSlot;

				ASSERT((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
				uSlot = uSrc / SOURCE_ARGUMENTS_PER_VECTOR;

				psInst->u.psVec->aeSrcFmt[uSlot] = psSubstContext->eSourceFormat;
			}
		}

		/*
			Apply any modifications to the instruction to allow sources to be folded.
		*/
		ModifyVectorInst(psState, psInst, &sInstMods);
	}
	
	return bRet;
}

static
IMG_BOOL CheckPossibleFolds(PINTERMEDIATE_STATE	psState,
							PIREGALLOC_CONTEXT	psContext,
							PIREGALLOC_BLOCK	psBlockState,
							PMERGE_CONTEXT		psMergeContext,
							IMG_BOOL			bPostAlloc,
							PINST				psInst,
							IMG_UINT32			uFoldDestIdx,
							IMG_UINT32			uSrcMask,
							IMG_BOOL			bCheckOnly)
/*****************************************************************************
 FUNCTION     : CheckPossibleFolds
    
 PURPOSE      : Check which sources can be folded into a vector ALU instruction.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				bPostAlloc		- TRUE if internal registers have been assigned
								to all intervals in the current basic block.
				psInst			- Instruction to fold into.
				uFoldDestIdx	- Destination to fold.
				uSrcMask		- Mask of sources to fold.

 RETURNS      : TRUE if all sources could be folded.
*****************************************************************************/
{
	IMG_UINT32			auSwizzles[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL			abSelectUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32			uSrc;
	IMG_UINT32			auNewPreSwizzleLiveChans[USC_MAX_NONCALL_DEST_COUNT];
	VEC_NEW_ARGUMENTS	sNewArguments;
	IMG_UINT32			uFoldSlotMask;
	IMG_BOOL			bRet;
	INST_MODIFICATIONS	sInstMods;

	/*
		Start with no modifications to the instruction's swizzles.
	*/
	memcpy(auSwizzles, psInst->u.psVec->auSwizzle, sizeof(auSwizzles));
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); uSrc++)
	{
		abSelectUpper[uSrc] = GetBit(psInst->u.psVec->auSelectUpperHalf, uSrc);
	}

	uFoldSlotMask = SourceMaskToSlotMask(psState, psInst, uSrcMask);

	/*
		For VMOVC the swizzles on the second two sources have to be the same. So if we
		are merging only one then check if the other is also a candidate for merging. Hopefully
		the modifications needed to the swizzles on both sources will then be the same.
	*/
	if ((psInst->eOpcode == IVMOVC) || (psInst->eOpcode == IVMOVCU8_FLT))
	{
		IMG_UINT32	uSrc12FoldSlotMask = uFoldSlotMask & VECTOR_SLOT12_MASK;

		if (uSrc12FoldSlotMask == VECTOR_SLOT1_MASK || uSrc12FoldSlotMask == VECTOR_SLOT2_MASK)
		{
			IMG_UINT32	uOtherFold;
			IMG_UINT32	uOtherFoldMask;
			IMG_UINT32	uFoldMask;

			if (uSrc12FoldSlotMask == VECTOR_SLOT1_MASK)
			{
				uOtherFold = VECTOR_SLOT2;
			}
			else
			{
				ASSERT(uSrc12FoldSlotMask == VECTOR_SLOT2_MASK);
				uOtherFold = VECTOR_SLOT1;
			}
			uOtherFoldMask = 1 << (uOtherFold * SOURCE_ARGUMENTS_PER_VECTOR);

			uFoldMask = ReplaceOtherSources(psState,
											psContext, 
											psBlockState, 
											psInst,
											IMG_TRUE /* bAllowNonVector */,
											uOtherFoldMask, 
											bCheckOnly);
			if (uFoldMask == uOtherFoldMask)
			{
				uFoldSlotMask |= (1 << uOtherFold);
			}
		}
	}

	/*
		Set up the details of which the destinations/sources will be
		internal registers and their formats in the modified instruction.
	*/
	GetNewVectorParameters(psState, 
						   psContext, 
						   psInst, 
						   uFoldDestIdx,
						   uFoldSlotMask, 
						   &sNewArguments);

	/*
		For vector instructions which mix F16 and F32 arguments: F32 data must be read from/written to
		internal registers.
	*/
	if (!FixF16FormatSources(psState, 
							 psContext, 
							 psMergeContext, 
							 &sNewArguments,
							 psInst, 
							 NULL /* pbUpconvertedSources */,
							 bCheckOnly))
	{
		return IMG_FALSE;
	}
	
	/*
		Record the masks of channels used from each source in the new
		instruction.
	*/
	for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); uSrc++)
	{
		GetLiveChansInSourceSlot(psState, 
								 psInst, 
								 uSrc, 
								 &auNewPreSwizzleLiveChans[uSrc], 
								 NULL /* puChansUsedPostSwizzle */);
	}

	/*
		Check if the instruction would still be valid with extra internal
		register sources replaced by temporary registers.
	*/
	bRet = IsValidModifiedVectorInst(psState,
									 bPostAlloc,
									 psInst,
									 auNewPreSwizzleLiveChans,
									 &sNewArguments,
									 auSwizzles,
									 abSelectUpper,
									 &sInstMods);
	if (!bCheckOnly && bRet)
	{
		/*
			Apply any modifications to the instruction to allow sources to be folded.
		*/
		ModifyVectorInst(psState, psInst, &sInstMods);
	}
	return bRet;
}

static
IMG_BOOL MergeSaveVectorDestination(PINTERMEDIATE_STATE	psState,
									PIREGALLOC_CONTEXT	psContext,
									PIREGALLOC_BLOCK	psBlockState,
									PMERGE_CONTEXT		psMergeContext,
									PINST				psWriter,
									IMG_UINT32			uDest,
									IMG_UINT32			uSrcMask,
									IMG_BOOL			bCheckOnly,
									IMG_PUINT32			puMergeInstCount)
/*****************************************************************************
 FUNCTION     : MergeSaveVectorDestination
    
 PURPOSE      : For a internal register destination which is copied into a
				unified store register check if the instruction can write
				the copy destination directly.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psBlockState	- State for the current basic block.
				psMergeContext	- State for the in-progress merge.
				psWriter		- Instruction to merge into.
				uDest			- Index of the destination to merge.
				uSrcMask		- Source arguments which must also be replaced
								by unified store registers.
				bCheckOnly		- If TRUE just check if the merge is possible.
								  If FALSE perform the merge.
			    puMergeInstCount	
								- Returns the number of extra instructions
								generated by the merge.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	PINST				psXYSplitInst, psZWSplitInst;
	PCODEBLOCK			psBlock = psWriter->psBlock;
	IMG_UINT32			uFoldSlotMask;
	VEC_NEW_ARGUMENTS	sNewArguments;
	IMG_BOOL			bUpconvertedSources;

	PVR_UNREFERENCED_PARAMETER(psContext);
	PVR_UNREFERENCED_PARAMETER(psBlockState);

	/*	
		Check for restrictions on replacing internal register sources to texture sample
		instructions by temporary registers.
	*/
	if ((g_psInstDesc[psWriter->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0)
	{
		return MergeTextureSampleArguments(psState, psContext, psBlockState, psWriter, uSrcMask, bCheckOnly);
	}

	if (psWriter->eOpcode == IRESTOREIREG || psWriter->eOpcode == IVLOAD)
	{
		if (!bCheckOnly)
		{
			DropFromMergeList(psState, psContext, psWriter);
			ExpandIRegSaveVector(psState, psWriter);
		}
		return IMG_TRUE;
	}
	ASSERT((g_psInstDesc[psWriter->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0);

	/*
		Don't split instructions with complex destinations.
	*/
	if (psWriter->uDestCount != 1)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	ASSERT(uDest == 0);

	/*
		Simple case where the instruction's results can be written to a temporary register in a single
		instruction.
	*/
	if (
			psWriter->asDest[uDest].eFmt == UF_REGFORMAT_F16 ||
			(psWriter->auDestMask[uDest] & psWriter->auLiveChansInDest[uDest] & USC_ZW_CHAN_MASK) == 0
	   )
	{
		if (!CheckPossibleFolds(psState, 
								psContext,
								psBlockState,
								psMergeContext,
								IMG_FALSE /* bPostAlloc */,
								psWriter,
								uDest,
								uSrcMask,
								bCheckOnly))
		{
			return IMG_FALSE;
		}
		if (!bCheckOnly)
		{
			if (psWriter->asDest[uDest].eFmt != UF_REGFORMAT_F16)
			{
				psWriter->auDestMask[uDest] &= USC_XY_CHAN_MASK;
			}
		}
		return IMG_TRUE;
	}

	/*
		Don't split instructions with complex destinations.
	*/
	if (psWriter->uDestCount != 1)
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	ASSERT(uDest == 0);

	/*
		For VTESTMASK we won't be able to split the instruction unless we can replace the
		sources by non-internal registers.
	*/
	if (psWriter->eOpcode == IVTESTMASK)
	{
		uSrcMask |= ReplaceOtherSources(psState,
									    psContext, 
										psBlockState, 
										psWriter,
										IMG_TRUE /* bAllowNonVector */,
										~uSrcMask, 
										bCheckOnly);
	}

	/*
		Convert the sources to be folded to a mask of vector-sized arguments.
	*/
	uFoldSlotMask = SourceMaskToSlotMask(psState, psWriter, uSrcMask);

	/*
		Get details of the formats of instruction source/destinations in the modified instruction and also
		whether they will be assigned to internal registers.
	*/
	GetNewVectorParameters(psState, 
						   psContext, 
						   psWriter, 
						   uDest,
						   uFoldSlotMask, 
						   &sNewArguments);

	/*
		For vector instructions which mix F16 and F32 arguments: F32 data must be read from/written to
		internal registers.
	*/
	if (!FixF16FormatSources(psState, 
							 psContext, 
							 psMergeContext, 
							 &sNewArguments,
							 psWriter, 
							 &bUpconvertedSources,
							 bCheckOnly))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/*
		Try splitting the vector instruction into two instructions
		writing the first 64-bits of the destination and the
		second 64-bits.
	*/
	if (!SplitVectorInstWithFold(psState, 
								 psWriter, 
								 &sNewArguments,
								 bCheckOnly, 
								 &psXYSplitInst, 
								 &psZWSplitInst))
	{
		return IMG_FALSE;
	}

	/*
		Set the number of extra instructions generated from the split (one extra if we are writing both the
		X or Y, and Z or W channels).
	*/
	if ((psWriter->auDestMask[0] & USC_XY_CHAN_MASK) != 0 && (psWriter->auDestMask[0] & USC_ZW_CHAN_MASK) != 0)
	{
		if (puMergeInstCount != NULL)
		{
			(*puMergeInstCount)++;
		}
	}

	if (!bCheckOnly)
	{
		/*
			Write from the split instructions directly into the registers
			we saving into.
		*/
		if (psXYSplitInst != NULL)
		{
			InsertInstBefore(psState, psBlock, psXYSplitInst, psWriter);
		}
		if (psZWSplitInst != NULL)
		{
			InsertInstBefore(psState, psBlock, psZWSplitInst, psWriter);
		}

		/*
			Free the original instruction.
		*/
		UpdateCandidateList(psState, psWriter, psXYSplitInst != NULL ? psXYSplitInst : psZWSplitInst);
		RemoveInst(psState, psBlock, psWriter);
		FreeInst(psState, psWriter);
	}

	return IMG_TRUE;
}

static
IMG_VOID GetIRegAssignment(PINTERMEDIATE_STATE	psState,
						   PIREGALLOC_CONTEXT	psContext,
						   PINST				psInst,
						   PIREG_ASSIGNMENT		psAssignment)
/*****************************************************************************
 FUNCTION     : GetIRegAssignment
    
 PURPOSE      : Get information about which source and destination arguments
				to an instruction have been assigned internal registers.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psInst			- Instruction.
				psAssignment	- Returns details of the source and destination
								arguments which have been assigned internal
								registers.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSlot;

	psAssignment->bDestIsIReg = IsIRegArgument(psState, psContext, &psInst->asDest[0], NULL /* puAssignedIreg */);
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		IsIRegArgument(psState, 
					   psContext, 
					   &psInst->asArg[uSlot * SOURCE_ARGUMENTS_PER_VECTOR], 
					   &psAssignment->auIRegSource[uSlot]);
	}
}

static
IMG_UINT32 GetIntervalDef(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINTERVAL psInterval)
/*****************************************************************************
 FUNCTION     : GetLastUseOfInterval
    
 PURPOSE      : Get the index in the block of the instruction defining an interval.

 PARAMETERS   : psState				- Compiler state.	
				psBlock				- Current basic block.
				psInterval			- Interval.

 RETURNS      : The index.
*****************************************************************************/
{
	PUSEDEF	psDef;
	PINST	psDefInst;

	ASSERT(psInterval->psUseDefChain->psDef != NULL);
	psDef = psInterval->psUseDefChain->psDef;
	ASSERT(psDef->eType == DEF_TYPE_INST);
	psDefInst = psDef->u.psInst;

	ASSERT(psDefInst->psBlock == psBlock);
	return psDefInst->uBlockIndex;
}

static
IMG_UINT32 GetLastUseOfInterval(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PINTERVAL psInterval)
/*****************************************************************************
 FUNCTION     : GetLastUseOfInterval
    
 PURPOSE      : Get the index in the block of the last instruction using the interval.

 PARAMETERS   : psState				- Compiler state.	
				psBlock				- Current basic block.
				psInterval			- Interval.

 RETURNS      : The index.
*****************************************************************************/
{
	PUSEDEF	psLastUse;

	ASSERT(psInterval->psUseDefChain->sList.psTail != NULL);
	psLastUse = IMG_CONTAINING_RECORD(psInterval->psUseDefChain->sList.psTail, PUSEDEF, sListEntry);

	/*
		Start our search for dual-issue sites from the instruction which contains the last use.
	*/
	ASSERT(psLastUse->eType == DEF_TYPE_INST || psLastUse->eType == USE_TYPE_SRC || psLastUse->eType == USE_TYPE_OLDDEST);
	ASSERT(psLastUse->u.psInst->psBlock == psBlock);

	return psLastUse->u.psInst->uBlockIndex;
}


static
IMG_BOOL FoldVectorSources_PostAlloc(PINTERMEDIATE_STATE	psState,
									 PIREGALLOC_CONTEXT		psContext,
									 PIREGALLOC_BLOCK		psBlockState,
									 PINST					psInst,
									 IMG_UINT32				uSrcMask,
									 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : FoldVectorSources_PostAlloc
    
 PURPOSE      : Fold sources to iregisters in an instruction

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psBlockState	- Internal register allocator state for
								the current basic block.
				psInst			- Instruction to fold into.
				uSrcMask		- Mask of sources to fold.
				bCheckOnly		- If TRUE just check if folding is possible.
								  If FALSE modify the instruction to make the
								  fold possible.
 RETURNS      : TRUE if all sources could be folded.
*****************************************************************************/
{
	PINST			psDualInst;
	IREG_ASSIGNMENT	sAssignment;
	IMG_UINT32		uIReg;
	IMG_UINT32		uAvailableIReg;
	PARG			psDummyIRegDest;
	PUSC_LIST_ENTRY	psDummyIRegInsertPoint;
	PINTERVAL		psMergeInterval;
	IMG_UINT32		uSlotMask;
	IMG_UINT32		uSlot;

	if (psInst->uDestCount != 1)
	{
		return IMG_FALSE;
	}

	/*
		Check which sources/destinations have been assigned internal registers.
	*/
	uSlotMask = SourceMaskToSlotMask(psState, psInst, uSrcMask);
	GetIRegAssignment(psState, psContext, psInst, &sAssignment);
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		if ((uSlotMask & (1 << uSlot)) != 0)
		{
			sAssignment.auIRegSource[uSlot] = USC_UNDEF;
		}
	}

	/*
		Get a pointer to the interval we are currently trying to merge.
	*/
	psMergeInterval = NULL;
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		if ((uSlotMask & (1 << uSlot)) != 0)
		{
			psMergeInterval = GetIntervalByArg(psContext, &psInst->asArg[uSlot * SOURCE_ARGUMENTS_PER_VECTOR]);
			break;
		}
	}
	ASSERT(psMergeInterval != NULL);

	/*	
		Check if there is a spare internal register available to overwrite
		at the dual-issue instruction.
	*/
	uAvailableIReg = USC_UNDEF;
	psDummyIRegInsertPoint = NULL;
	for (uIReg = 0; uIReg < psContext->uNumHwRegs; uIReg++)
	{
		PINTERNAL_REGISTER	psIReg = &psContext->asHwReg[uIReg];
		IMG_BOOL			bThisIRegAvailable;
		SAFE_LIST_ITERATOR	sIter;

		bThisIRegAvailable = IMG_TRUE;
		SafeListIteratorInitialize(&psIReg->sHwRegIntervalList, &sIter);
		for (; SafeListIteratorContinue(&sIter); SafeListIteratorNext(&sIter))
		{
			PINTERVAL	psInterval;
			IMG_UINT32	uIntervalDef;
			IMG_UINT32	uIntervalLastUse;

			psInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, sHwRegIntervalListEntry);

			if (psInterval == psMergeInterval)
			{
				continue;
			}
			
			uIntervalDef = GetIntervalDef(psState, psInst->psBlock, psInterval);
			uIntervalLastUse = GetLastUseOfInterval(psState, psInst->psBlock, psInterval);

			/*
				The list is sorted by the order of the defining instructions in the current
				block so stop once we see a define after the current instruction.
			*/
			if (uIntervalDef > psInst->uBlockIndex)
			{
				psDummyIRegInsertPoint = &psInterval->sHwRegIntervalListEntry;
				break;
			}
			/*
				Skip internal registers whose last use is before the current instruction.
			*/
			if (uIntervalLastUse <= psInst->uBlockIndex)
			{
				continue;
			}

			/*
				Otherwise this interval starts at or before the current instruction and finishes
				afterwards.
			*/
			bThisIRegAvailable = IMG_FALSE;
			break;
		}
		SafeListIteratorFinalise(&sIter);

		if (bThisIRegAvailable)
		{
			uAvailableIReg = uIReg;
			break;
		}
	}

	/*
		Try to create a dual-issue instruction with the sources we want to replace mapped
		to the unified store slot.
	*/
	if (ConvertSingleInstToDualIssue(psState, 
									 psInst, 
									 &sAssignment, 
									 uSlotMask, 
									 uAvailableIReg != USC_UNDEF ? IMG_TRUE : IMG_FALSE,
									 bCheckOnly, 
									 &psDualInst,
									 &psDummyIRegDest))
	{
		if (!bCheckOnly)
		{
			/*
				If the new dual-issue instruction is writing a spare internal register then create an interval for it.
			*/
			if (psDummyIRegDest != NULL)
			{
				PUSEDEF_CHAIN	psDummyIRegUseDef;
				PINTERVAL		psDummyIRegInterval;

				ASSERT(psDummyIRegDest->uType == USEASM_REGTYPE_TEMP);

				psDummyIRegUseDef = UseDefGet(psState, psDummyIRegDest->uType, psDummyIRegDest->uNumber);

				/*
					Allocate a new interval.
				*/
				psDummyIRegInterval = 
					AddNewBlockInterval(psState, psContext, psBlockState, psDummyIRegUseDef, psDummyIRegUseDef);

				/*
					Mark the new interval as assigned to the spare internal register.
				*/
				ASSERT(uAvailableIReg != USC_UNDEF);
				psDummyIRegInterval->eState = INTERVAL_STATE_ASSIGNED;
				psDummyIRegInterval->uHwReg = uAvailableIReg;
				SafeListAppendItem(&psBlockState->sAssignedIntervalList, &psDummyIRegInterval->u.sAssignedIntervalListEntry);

				/*
					Insert the new interval into the list of intervals assigned to a specific internal register.
				*/
				SafeListInsertItemBeforePoint(&psContext->asHwReg[uAvailableIReg].sHwRegIntervalList, 
											  &psDummyIRegInterval->sHwRegIntervalListEntry, 
											  psDummyIRegInsertPoint);
			}

			InsertInstBefore(psState, psInst->psBlock, psDualInst, psInst);

			UpdateCandidateList(psState, psInst, psDualInst);
			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);
		}
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static 
IMG_BOOL FoldVectorSources(PINTERMEDIATE_STATE	psState,
						   PIREGALLOC_CONTEXT	psContext,
						   PIREGALLOC_BLOCK		psBlockState,
						   PMERGE_CONTEXT		psMergeContext,
						   IMG_BOOL				bPostAlloc,
						   PINST				psInst,
						   IMG_UINT32			uSrcMask,
						   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : FoldVectorSources
    
 PURPOSE      : Fold sources to iregisters in an instruction

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator state.
				psBlockState	- Internal register allocator state for
								the current basic block.
				psMergeContext	- Context for the current fold.
				bPostAlloc		- TRUE if internal registers have been assigned
								to all intervals in the current basic block.
				psInst			- Instruction to fold into.
				uSrcMask		- Mask of sources to fold.
				bCheckOnly		- If TRUE just check if folding is possible.
								  If FALSE modify the instruction to make the
								  fold possible.

 RETURNS      : TRUE if all sources could be folded.
*****************************************************************************/
{
	/*
		Can only fold into instructions accessing vectors.
	*/
	ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0);

	/*
		Check if any possible configurations of this instruction allow the sources
		to be folded.
	*/
	if (CheckPossibleFolds(psState, 
						   psContext, 
						   psBlockState,
						   psMergeContext, 
						   bPostAlloc, 
						   psInst, 
						   USC_UNDEF /* uFoldDestIdx */,
						   uSrcMask, 
						   bCheckOnly))
	{
		return IMG_TRUE;
	}

	/*
		Check for cases where the instruction can be modified so some sources can be folded but the remaining
		sources and the destination are then much more difficult to allocate internal registers for. So we only apply
		them after the internal register to use for each interval has been set.
	*/
	if (bPostAlloc && FoldVectorSources_PostAlloc(psState, psContext, psBlockState, psInst, uSrcMask, bCheckOnly))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_BOOL MergeRestoreVectorSource(PINTERMEDIATE_STATE	psState, 
								  PIREGALLOC_CONTEXT	psContext,
								  PIREGALLOC_BLOCK		psBlockState,
								  PMERGE_CONTEXT		psMergeContext,
								  PINST					psReader, 
								  IMG_UINT32			uSrcMask, 
								  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : MergeRestoreVectorSource
    
 PURPOSE      : For instruction sources which are internal registers written
				by an instruction copying from the unified store: try to replace
				the internal register by the copy source.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psBlockState	- Internal register allocator state for
								the current basic block.
				psMergeContext	- Context for the current merge.
				psReader		- Instruction to merge into.
				uSrcMask		- Mask of sources to merge.
				psRestoreSrc	- Copy source.
				bCheckOnly		- If TRUE just check if merging is possible.
								  If FALSE perform the merge.

 RETURNS      : Nothing
*****************************************************************************/
{
	if ((g_psInstDesc[psReader->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
	{
		if (!FoldVectorSources(psState, 
							   psContext, 
							   psBlockState, 
							   psMergeContext, 
							   psBlockState->bPostAlloc, 
							   psReader, 
							   uSrcMask, 
							   bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	else if (psReader->eOpcode == ISAVEIREG)
	{
		/*
			We can always merge into these instructions which will be expanded to a
			pair of VMOVs.
		*/
	}
	else
	{
		ASSERT((g_psInstDesc[psReader->eOpcode].uFlags & DESC_FLAGS_TEXTURESAMPLE) != 0);

		if (!MergeTextureSampleArguments(psState, psContext, psBlockState, psReader, uSrcMask, bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID ExpandVLOAD(PINTERMEDIATE_STATE psState, PINST psVLoadInst)
/*****************************************************************************
 FUNCTION     : ExpandVLOAD
    
 PURPOSE      : 

 PARAMETERS   : psState			- Compiler state.
				psVLoadInst		- VLOAD instruction to modify.

 RETURNS      : Nothing
*****************************************************************************/
{
	ModifyOpcode(psState, psVLoadInst, IVMOV);
}

static
IMG_VOID ExpandIRegRestoreVector(PINTERMEDIATE_STATE psState, PINST psRestoreInst)
/*****************************************************************************
 FUNCTION     : ExpandIRegRestoreVector
    
 PURPOSE      : Expand the generic RESTOREIREG instruction to a vector-core specific
				sequence of intermediate instructions.

 PARAMETERS   : psState		- Compiler state.
				psRestoreInst	- RESTOREIREG instruction to expand.

 RETURNS      : Nothing
*****************************************************************************/
{
	ASSERT(psRestoreInst->uDestCount == 1);
	ASSERT(psRestoreInst->apsOldDest[0] == NULL);
	psRestoreInst->auDestMask[0] = psRestoreInst->auLiveChansInDest[0];

	SetOpcode(psState, psRestoreInst, IVMOV);
	psRestoreInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psRestoreInst->u.psVec->aeSrcFmt[0] = psRestoreInst->asArg[0].eFmt;
}

static
IMG_VOID ExpandIRegSaveVector(PINTERMEDIATE_STATE psState, PINST psSaveInst)
/*****************************************************************************
 FUNCTION     : ExpandIRegSaveVector
    
 PURPOSE      : Expand the generic SAVEIREG instruction to a vector-core specific
				sequence of intermediate instructions.

 PARAMETERS   : psState		- Compiler state.
			    psSaveInst	- SAVEIREG instruction to restore.

 RETURNS      : Nothing
*****************************************************************************/
{
	IMG_UINT32				uHalf;
	IMG_UINT32				auSaveMask[2];
	ARG						sXYTemp;
	UF_REGFORMAT			eSaveFormat = psSaveInst->asDest[0].eFmt;

	ASSERT(NoPredicate(psState, psSaveInst));
	ASSERT((psSaveInst->auLiveChansInDest[0] & ~psSaveInst->auDestMask[0]) == 0);

	if (eSaveFormat == UF_REGFORMAT_F16)
	{
		SetOpcode(psState, psSaveInst, IVMOV);
		psSaveInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psSaveInst->u.psVec->aeSrcFmt[0] = psSaveInst->asArg[0].eFmt;
		return;
	}

	ASSERT(eSaveFormat == UF_REGFORMAT_F32);

	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		auSaveMask[uHalf] = psSaveInst->auLiveChansInDest[0] & g_auVectorPartMasks[uHalf];
	}

	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		PINST		psInst;
		IMG_UINT32	uSaveMask = auSaveMask[uHalf];

		if (uSaveMask == 0)
		{
			continue;
		}

		psInst = AllocateInst(psState, psSaveInst);
		SetOpcode(psState, psInst, IVMOV);

		psInst->auDestMask[0] = uSaveMask;

		if (uHalf == 0)
		{
			psInst->auLiveChansInDest[0] = uSaveMask;
		}
		else
		{
			psInst->auLiveChansInDest[0] = psSaveInst->auLiveChansInDest[0];
		}

		if (uHalf == 0)
		{
			if (auSaveMask[1] == 0)
			{
				MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psSaveInst, 0 /* uMoveFromDestIdx */);
			}
			else
			{
				MakeNewTempArg(psState, eSaveFormat, &sXYTemp);
				SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sXYTemp);
			}
		}
		else
		{
			MoveDest(psState, psInst, 0 /* uMoveToDestIdx */, psSaveInst, 0 /* uMoveFromDestIdx */);
			if (auSaveMask[0] != 0)
			{
				SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, &sXYTemp);
			}
		}

		if (psSaveInst->eOpcode == ISAVEIREG || psSaveInst->eOpcode == IRESTOREIREG)
		{
			SetSrcFromArg(psState, psInst, 0 /* uSrcIdx */, &psSaveInst->asArg[0]);
		}
		else
		{
			ASSERT(psSaveInst->eOpcode == IVLOAD);
			CopyVectorSourceArguments(psState, psInst, 0 /* uDestArgIdx */, psSaveInst, 0 /* uSrcArgIdx */);
		}

		if (uHalf == 0)
		{
			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		}
		else
		{
			psInst->u.psVec->uDestSelectShift = USC_Z_CHAN;
			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, W, Z, W);
		}
		psInst->u.psVec->aeSrcFmt[0] = psInst->asArg[0].eFmt;

		InsertInstBefore(psState, psSaveInst->psBlock, psInst, psSaveInst);
	}

	RemoveInst(psState, psSaveInst->psBlock, psSaveInst);
	FreeInst(psState, psSaveInst);
}

typedef enum _INST_ORDER
{
	INST_ORDER_ANY,
	INST_ORDER_ZERO_BEFORE_ONE,
	INST_ORDER_ONE_BEFORE_ZERO,
} INST_ORDER, *PINST_ORDER;

typedef struct _SPLIT_INST_DATA
{
	IMG_UINT32	uChanMask;
	IMG_UINT32	auSwizzle[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_BOOL	abSelUpper[VECTOR_MAX_SOURCE_SLOT_COUNT];
	IMG_UINT32	auSplitChansUsed[VECTOR_MAX_SOURCE_SLOT_COUNT];
	PINST		psInst;
} SPLIT_INST_DATA, *PSPLIT_INST_DATA;

static
IMG_BOOL SplitIRegDestVector(PINTERMEDIATE_STATE	psState,
							 PIREGALLOC_CONTEXT		psContext,
							 PIREGALLOC_BLOCK		psBlockState,
							 PINST					psInst,
							 IMG_UINT32				uSrcMask)
/*****************************************************************************
 FUNCTION     : SplitIRegDestVector
    
 PURPOSE      : Try and split a vector instruction into multiple instructions
				to make it possible to replace internal register sources by
				unified store registers.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- Internal register allocator state for
									the current basic block.
				psInst				- Instruction to split.
				uSrcMask			- Mask of internal register sources.

 RETURNS      : TRUE if the instruction was successfully split.
*****************************************************************************/
{
	IMG_UINT32			uSrc;
	IMG_UINT32			uInst;
	IMG_UINT32			uIRegOnlySlotMask;
	IMG_UINT32			uActualSecondDestinationMask;
	INST_ORDER			eInstOrder;
	SPLIT_INST_DATA		asSplit[2];
	PSPLIT_INST_DATA	psFirst, psSecond;
	PINTERVAL			psDestInterval;
	PINTERVAL			psPartialDestInterval;
	MERGE_CONTEXT		sMergeContext;
	IMG_UINT32			uWideSlotMask;
	IMG_UINT32			uSlot;
	IMG_UINT32			uSlotMask = SourceMaskToSlotMask(psState, psInst, uSrcMask);
	VEC_NEW_ARGUMENTS	sNewArguments;
	IOPCODE				eNewOpcode;

	ASSERT(psInst->uDestCount == 1);

	/*
		There's no point in splitting the instruction if the mask of channels used from the sources is
		independent of the destination mask.
	*/
	if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0)
	{
		return IMG_FALSE;
	}

	GetNewVectorParameters(psState, psContext, psInst, USC_UNDEF /* uFoldDestIdx */, uSlotMask, &sNewArguments); 

	/*
		For vector instructions which mix F16 and F32 arguments: F32 data must be read from/written to
		internal registers.
	*/
	sMergeContext.uNumSecAttrsUsed = 0;
	if (!FixF16FormatSources(psState, 
							 psContext, 
							 &sMergeContext, 
							 &sNewArguments, 
							 psInst, 
							 NULL /* pbUpconvertedSources */, 
							 IMG_TRUE /* bCheckOnly */))
	{
		return IMG_FALSE;
	}

	/*
		Switch VMAD4 -> VMAD to allow all sources to be non-internal registers.
	*/
	if (psInst->eOpcode == IVMAD4)
	{
		eNewOpcode = IVMAD;
	}
	else
	{
		eNewOpcode = psInst->eOpcode;
	}

	uIRegOnlySlotMask = GetIRegOnlySourceMask(eNewOpcode);
	uWideSlotMask = GetWideSourceMask(eNewOpcode);

	for (uInst = 0; uInst < 2; uInst++)
	{
		PSPLIT_INST_DATA	psSplit = &asSplit[uInst];

		psSplit->uChanMask = 0;

		/*
			By default don't modify any instruction parameters.
		*/
		for (uSrc = 0; uSrc < GetSwizzleSlotCount(psState, psInst); uSrc++)
		{
			psSplit->auSplitChansUsed[uSrc] = 0;
			psSplit->abSelUpper[uSrc] = GetBit(psInst->u.psVec->auSelectUpperHalf, uSrc);	
			psSplit->auSwizzle[uSrc] = psInst->u.psVec->auSwizzle[uSrc];
		}
	}

	/*
		Work out the channels in the destination which need to be assigned
		to each of the split instructions so that only 64-bits is accessed from
		F32, non-unified store sources.
	*/
	for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
	{
		IMG_UINT32			auSrcChanMask[2];	
		IMG_UINT32			uChan;
		IMG_UINT32			uLowChansUsed;
		IMG_UINT32			uHighChansUsed;
		PVEC_NEW_ARGUMENT	psNewSrc = &sNewArguments.asSrc[uSlot];
		IMG_UINT32			uLiveChansInArg;
		
		if (psNewSrc->bIsIReg || psNewSrc->eFormat == UF_REGFORMAT_F16)
		{
			continue;
		}

		/*
			If this slot requires an internal register then it can never be folded.
		*/
		if ((uIRegOnlySlotMask & (1U << uSlot)) != 0)
		{	
			return IMG_FALSE;
		}

		/*
			If only the X/Y or Z/W channels are accessed from this source then ignore it.
		*/
		uLiveChansInArg = GetLiveChansInArg(psState, psInst, uSlot * SOURCE_ARGUMENTS_PER_VECTOR);
		if ((uLiveChansInArg & USC_ZW_CHAN_MASK) == 0)
		{
			continue;
		}
		if (GetBit(psInst->u.psVec->auSelectUpperHalf, uSlot))
		{
			ASSERT((uLiveChansInArg & USC_XY_CHAN_MASK) == 0);
			continue;
		}

		/*
			If the instruction can access a full vec4 from the unified store in this
			source then nothing more to do.
		*/
		if ((uWideSlotMask & (1U << uSlot)) != 0)
		{
			continue;
		}

		/*
			Check which channels in the destination are calculated from the X or Y (respectively
			Z or W) channels from this source.
		*/
		auSrcChanMask[0] = auSrcChanMask[1] = 0;
		uLowChansUsed = uHighChansUsed = 0;
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if ((psInst->auDestMask[0] & (1U << uChan)) != 0)
			{
				USEASM_SWIZZLE_SEL	eChanSel;

				eChanSel = GetChan(psInst->u.psVec->auSwizzle[uSlot], uChan);
				if (eChanSel == USEASM_SWIZZLE_SEL_X || eChanSel == USEASM_SWIZZLE_SEL_Y)
				{
					auSrcChanMask[0] |= (1U << uChan);
					uLowChansUsed |= (1U << eChanSel);
				}
				else if (eChanSel == USEASM_SWIZZLE_SEL_Z || eChanSel == USEASM_SWIZZLE_SEL_W)
				{
					auSrcChanMask[1] |= (1U << uChan);
					uHighChansUsed |= (1U << eChanSel);
				}
			}
		}

		/*
			Check if the mask of channels which are calculated just from the X or Y channels
			in this source are calculated from both X or Y and Z or W in other sources.
		*/
		for (uInst = 0; uInst < 2; uInst++)
		{
			IMG_UINT32	uSrcChanMask = auSrcChanMask[uInst];

			if ((asSplit[0].uChanMask & uSrcChanMask) != 0 && (asSplit[1].uChanMask & uSrcChanMask) != 0)
			{
				return IMG_FALSE;
			}
		}
		for (uInst = 0; uInst < 2; uInst++)
		{
			IMG_UINT32	uPrevChanMask = asSplit[uInst].uChanMask;

			if ((auSrcChanMask[0] & uPrevChanMask) != 0 && (auSrcChanMask[1] & uPrevChanMask) != 0)
			{
				return IMG_FALSE;
			}
		}

		/*
			Store the subsets of the destination channels corresponding to the lower and upper parts of the
			source.
		*/
		for (uInst = 0; uInst < 2; uInst++)
		{	
			IMG_UINT32			uSrcChanMask = auSrcChanMask[uInst];
			IMG_UINT32			uOtherSrcChanMask = auSrcChanMask[1 - uInst];
			IMG_UINT32			uSwizzle;
			IMG_BOOL			bSelUpper;
			IMG_UINT32			uChansUsed;
			PSPLIT_INST_DATA	psDest;

			if (uInst == 0)
			{
				uSwizzle = psInst->u.psVec->auSwizzle[uSlot];
				bSelUpper = IMG_FALSE;
				uChansUsed = uLowChansUsed;
			}
			else
			{
				/* Replace Z by X and W by Y in the source swizzle. */
				uSwizzle = CombineSwizzles(USEASM_SWIZZLE(0, 0, X, Y), psInst->u.psVec->auSwizzle[uSlot]);
				bSelUpper = IMG_TRUE;
				uChansUsed = uHighChansUsed;
			}

			if ((asSplit[0].uChanMask & uOtherSrcChanMask) == 0 && (asSplit[1].uChanMask & uSrcChanMask) == 0)
			{
				psDest = &asSplit[0];
			}
			else
			{
				ASSERT((asSplit[1].uChanMask & uOtherSrcChanMask) == 0);
				psDest = &asSplit[1];
			}

			psDest->uChanMask |= uSrcChanMask;
			psDest->auSwizzle[uSlot] = uSwizzle;
			psDest->abSelUpper[uSlot] = bSelUpper;
			psDest->auSplitChansUsed[uSlot] = uChansUsed;
		}
		ASSERT((asSplit[0].uChanMask & asSplit[1].uChanMask) == 0);
	}

	/*
		If any channels in the destination are calculated using just constant values in all of the
		sources then assign those channels to the first split instruction.
	*/
	if ((asSplit[0].uChanMask | asSplit[1].uChanMask) != psInst->auDestMask[0])
	{
		asSplit[0].uChanMask = (psInst->auDestMask[0] & ~asSplit[1].uChanMask);
	}

	/*
		Check the new swizzles for the split instructions are supported.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		PSPLIT_INST_DATA	psSplit = &asSplit[uInst];
		IMG_UINT32			auNewPreSwizzleLiveChans[VECTOR_MAX_SOURCE_SLOT_COUNT];

		for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
		{
			BaseGetLiveChansInVectorArgument(psState, 
											 psInst, 
											 uSlot * SOURCE_ARGUMENTS_PER_VECTOR, 
											 &psSplit->uChanMask, 
											 &auNewPreSwizzleLiveChans[uSlot],
											 NULL /* puChansUsedPostSwizzle */);
		}

		if (!IsSwizzleSetSupported(psState, eNewOpcode, psInst, auNewPreSwizzleLiveChans, psSplit->auSwizzle))
		{
			return IMG_FALSE;
		}
	}

	/*
		Get the interval data structure for the instruction's destination.
	*/
	psDestInterval = GetIntervalByArg(psContext, &psInst->asDest[0]);
	ASSERT(psDestInterval != NULL);

	/*
		If we are after internal registers have been assigned then check whether the internal
		register assigned to the destination has also been assigned to one of the sources. In
		this case we we need to make sure the first instruction in the split pair doesn't overwrite
		channels which are still to be used by the second instruction in the pair.
	*/
	eInstOrder = INST_ORDER_ANY;
	psPartialDestInterval = NULL;
	if (psDestInterval->eState == INTERVAL_STATE_ASSIGNED)
	{
		for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
		{
			INST_ORDER	eInstOrderHere;
			IMG_BOOL	abOverwrite[2];
			PINTERVAL	psSrcInterval;
			IMG_UINT32	uSlot;

			/* Skip sources which will be replaced by temporary registers. */
			if ((uSrcMask & (1U << uSrc)) != 0)
			{
				continue;
			}

			/* Skip sources which haven't been assigned an internal register. */
			psSrcInterval = GetIntervalByArg(psContext, &psInst->asArg[uSrc]);
			if (psSrcInterval == NULL || psSrcInterval->eState != INTERVAL_STATE_ASSIGNED)
			{
				continue;
			}

			ASSERT((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
			uSlot = uSrc / SOURCE_ARGUMENTS_PER_VECTOR;

			/* Skip sources which have been assigned a different internal register to the destination. */
			if (psSrcInterval->uHwReg != psDestInterval->uHwReg)
			{
				continue;
			}

			/*
				Use this source as the source for the channels not written in the destination
				by the first instruction in the split pair.
			*/
			if (psPartialDestInterval == NULL)
			{
				psPartialDestInterval = psSrcInterval;
			}
			else
			{
				ASSERT(psPartialDestInterval == psSrcInterval);
			}

			/*
				Check if the first split instruction writes to channels which are used
				in the second split instruction.
			*/
			for (uInst = 0; uInst < 2; uInst++)
			{
				PSPLIT_INST_DATA	psFirst = &asSplit[uInst];
				PSPLIT_INST_DATA	psSecond = &asSplit[1 - uInst];

				abOverwrite[uInst] = IMG_FALSE;
				if ((psFirst->uChanMask & psSecond->auSplitChansUsed[uSlot]) != 0)
				{
					abOverwrite[uInst] = IMG_TRUE;
				}
			}

			/* Check for no valid order for the split instructions. */
			if (abOverwrite[0] && abOverwrite[1])
			{
				return IMG_FALSE;
			}	
			/*
				Set the required order for this source.
			*/
			else if (abOverwrite[0])
			{
				eInstOrderHere = INST_ORDER_ONE_BEFORE_ZERO;
			}
			else if (abOverwrite[1])
			{
				eInstOrderHere = INST_ORDER_ZERO_BEFORE_ONE;
			}
			else
			{
				eInstOrderHere = INST_ORDER_ANY;
			}

			/*
				Check if the required order for this source conflicts with
				the order for previous sources.
			*/
			if (eInstOrder != INST_ORDER_ANY && eInstOrderHere != eInstOrder)
			{
				return IMG_FALSE;
			}
			eInstOrder = eInstOrderHere;
		}
	}
	else
	{
		ASSERT(psDestInterval->eState == INTERVAL_STATE_PENDING);
	}

	if (eInstOrder == INST_ORDER_ANY || eInstOrder == INST_ORDER_ZERO_BEFORE_ONE)
	{
		psFirst = &asSplit[0];
		psSecond = &asSplit[1];
	}
	else
	{	
		psFirst = &asSplit[1];
		psSecond = &asSplit[0];
	}

	/*
		Check that the instruction supports a sufficently precise destination mask so the ZW part of the split pair
		won't overwrite the result written by the XY part.
	*/
	uActualSecondDestinationMask = GetMinimalDestinationMask(psState, psInst, 0 /* uDestIdx */, psSecond->uChanMask);
	if ((uActualSecondDestinationMask & psFirst->uChanMask) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Replace any F16 sources to the instruction by the same data but upconverted to F32.
	*/
	FixF16FormatSources(psState, 
						psContext, 
						NULL /* psMergeContext */,
						&sNewArguments,
						psInst, 
						NULL /* pbUpconvertedSourecs */,
						IMG_FALSE /* bCheckOnly */);
	
	/*
		Split the vector instruction into two instructions
		each of which accesses only X or Y, or only Z or W from all
		the sources we are trying to fold.
	*/
	BaseSplitVectorInst(psState,
						psInst,
						psFirst->uChanMask,
						&psFirst->psInst,
						psSecond->uChanMask,
						&psSecond->psInst);

	/*
		Replace the original instruction by the split instructions.
	*/
	if (psFirst->psInst != NULL)
	{
		InsertInstBefore(psState, psInst->psBlock, psFirst->psInst, psInst);
	}
	if (psSecond->psInst != NULL)
	{
		InsertInstBefore(psState, psInst->psBlock, psSecond->psInst, psInst);
	}
	RemoveInst(psState, psInst->psBlock, psInst);
	FreeInst(psState, psInst);

	if (psFirst->psInst != NULL && psSecond->psInst != NULL)
	{
		PARG				psFirstSplitTemp;
		PUSEDEF_CHAIN		psFirstSplitTempUseDef;
		PINTERVAL			psFirstSplitTempInterval;
		PINST				psFirstInst = psFirst->psInst;

		psFirstSplitTemp = &psFirstInst->asDest[0];

		psFirstSplitTempUseDef = UseDefGet(psState, psFirstSplitTemp->uType, psFirstSplitTemp->uNumber);

		psFirstSplitTempInterval = 
			AddNewBlockInterval(psState, psContext, psBlockState, psFirstSplitTempUseDef, psFirstSplitTempUseDef);

		psFirstSplitTempInterval->bUsedAsPartialDest = IMG_TRUE;

		if (psDestInterval->eState == INTERVAL_STATE_PENDING)
		{
			/*
				Mark the newly allocated destination of the first instruction in the split pair as requiring an
				internal register.
			*/
			AddToUsedIntervalList(psState, psBlockState, psFirstSplitTempInterval);
		}
		else
		{
			/*
				Copy information about internal register allocated for the destination of the original instruction
				to the interval for the destination of the first instruction.
			*/
			ASSERT(psDestInterval->eState == INTERVAL_STATE_ASSIGNED);
			psFirstSplitTempInterval->eState = INTERVAL_STATE_ASSIGNED;
			psFirstSplitTempInterval->uHwReg = psDestInterval->uHwReg;

			SafeListAppendItem(&psBlockState->sAssignedIntervalList, &psFirstSplitTempInterval->u.sAssignedIntervalListEntry);

			/*
				Insert the new interval into the list of intervals assigned to a specific internal register.
			*/
			SafeListInsertItemBeforePoint(&psContext->asHwReg[psFirstSplitTempInterval->uHwReg].sHwRegIntervalList, 
										  &psFirstSplitTempInterval->sHwRegIntervalListEntry, 
										  &psDestInterval->sHwRegIntervalListEntry);

			/*
				Flag that the first instruction in the pair is preserving some of the channels
				in its destination.
			*/
			if (psPartialDestInterval != NULL)
			{
				PUSC_LIST_ENTRY	psListEntry, psPrevListEntry;

				if (psFirstInst->apsOldDest[0] == NULL)
				{
					SetPartialDest(psState,
								   psFirstInst,
								   0 /* uDestIdx */,
								   psPartialDestInterval->psUseDefChain->uType,
								   psPartialDestInterval->psUseDefChain->uNumber,
								   psPartialDestInterval->psUseDefChain->eFmt);
				}
				else
				{
					ASSERT(psFirstInst->apsOldDest[0]->uType == psPartialDestInterval->psUseDefChain->uType);
					ASSERT(psFirstInst->apsOldDest[0]->uNumber == psPartialDestInterval->psUseDefChain->uNumber);
					ASSERT(psFirstInst->apsOldDest[0]->eFmt == psPartialDestInterval->psUseDefChain->eFmt);
				}

				/*
					Replace all sources in the second instruction which have been assigned the same hardware register as the 
					original destination by the destination of the first instruction.
				*/
				for (psListEntry = psPartialDestInterval->psUseDefChain->sList.psTail;
					 psListEntry != NULL;
					 psListEntry = psPrevListEntry)
				{
					PUSEDEF	psUse;

					psPrevListEntry = psListEntry->psPrev;
					psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
					ASSERT(UseDefIsInstUseDef(psUse));
					if (psUse->u.psInst != psSecond->psInst)
					{
						break;
					}
					psFirstInst->auLiveChansInDest[0] |= GetUseChanMask(psState, psUse);
					UseDefSubstUse(psState, psPartialDestInterval->psUseDefChain, psUse, &psFirstInst->asDest[0]);
				}
			}
		}
	}

	/*
		Update the swizzles in the split instructions.
	*/
	for (uInst = 0; uInst < 2; uInst++)
	{
		PSPLIT_INST_DATA	psSplit = &asSplit[uInst];
		PINST				psSplitInst = psSplit->psInst;
		IMG_UINT32			uSlot;

		if (psSplitInst == NULL)
		{
			continue;
		}

		if (psSplitInst->eOpcode != eNewOpcode)
		{
			ModifyOpcode(psState, psSplitInst, eNewOpcode);
		}

		/*
			Update the source swizzles.
		*/
		memcpy(psSplitInst->u.psVec->auSwizzle, psSplit->auSwizzle, sizeof(psSplit->auSwizzle));

		/*
			Increase the register number for this source by 2x32-bit registers.
		*/
		for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psSplitInst); uSlot++)
		{
			if (psSplit->abSelUpper[uSlot])
			{
				SetBit(psSplitInst->u.psVec->auSelectUpperHalf, uSlot, 1);
			}
			else
			{
				SetBit(psSplitInst->u.psVec->auSelectUpperHalf, uSlot, 0);
			}
		}

		/*
			Replace internal register sources in the split instruction.
		*/
		MergeIRegSources(psState, psSplitInst, uSrcMask);
	}

	return IMG_TRUE;
}

typedef struct _DUALISSUE_CANDIDATE
{
	/*
		Instruction which is a possible candidate for dual-issuing.
	*/
	PINST			psInst;
	/*
		Information about which instruction source/destination arguments
		are internal registers.
	*/
	IREG_ASSIGNMENT	sIRegAssignment;
	/*
		Entry in the list of possible dual-issue sites.
	*/
	USC_LIST_ENTRY	sMasterListEntry;
	/*
		TRUE if this instruction is a candidate for a dual-issue with the
		current instruction.
	*/
	IMG_BOOL		bInLocalList;
	/*
		Entry in the list of possible dual-issue sites for the current instruction.
	*/
	USC_LIST_ENTRY	sLocalListEntry;
	/*
		List of intervals used in this instruction.
	*/
	USC_LIST		sIntervalList;
	/*
		TRUE if this candidate is writing the same internal register as the current
		restore instruction so the destination will have to be replaced by an
		internal register to allow creating a dual-issue.
	*/
	IMG_BOOL		bDestIRegReplaced;
	/*
		TRUE if this instruction is in the list of instructions which have had a
		source or destination replaced by a non-internal register.
	*/
	IMG_BOOL		bInReconsiderList;
	/*
		Entry in the above list.
	*/
	USC_LIST_ENTRY	sReconsiderListEntry;
} DUALISSUE_CANDIDATE, *PDUALISSUE_CANDIDATE;

typedef struct _DUALISSUE_INTERVAL
{
	/*
		Associated possible site for dual-issuing a load into an internal register.
	*/
	PDUALISSUE_CANDIDATE	psCandidate;
	/*
		Entry in the list of intervals used in at the dual-issue site.
	*/
	USC_LIST_ENTRY			sIntervalListEntry;
	/*
		Associated interval.
	*/
	PINTERVAL				psInterval;
	/*
		Entry in the list of possible dual-issue sites which use this interval.
	*/
	USC_LIST_ENTRY			sCandidateListEntry;
	/*
		TRUE if the interval is used as a destination to this instruction.
	*/
	IMG_BOOL				bDest;
} DUALISSUE_INTERVAL, *PDUALISSUE_INTERVAL;

#define INST_INCANDIDATELIST				INST_LOCAL1

static
IMG_VOID UpdateCandidateList(PINTERMEDIATE_STATE psState, PINST psOldCandidate, PINST psNewCandidate)
/*****************************************************************************
 FUNCTION     : UpdateCandidateList
    
 PURPOSE      : Update the list of possible dual-issue sites when replacing
				one instruction by another.

 PARAMETERS   : psState				- Compiler state.	
				psOldCandidate		- Instruction to free.
				psNewCandidate		- Replacement instruction.

 RETURNS      : The index.
*****************************************************************************/
{
	if (GetBit(psOldCandidate->auFlag, INST_INCANDIDATELIST))
	{
		PDUALISSUE_CANDIDATE	psCandidate;

		psCandidate = (PDUALISSUE_CANDIDATE)psOldCandidate->sStageData.pvData;
		ASSERT(psCandidate->psInst == psOldCandidate);
		psCandidate->psInst = psNewCandidate;
		
		psOldCandidate->sStageData.pvData = NULL;
		SetBit(psOldCandidate->auFlag, INST_INCANDIDATELIST, 0);

		psNewCandidate->sStageData.pvData = psCandidate;
		SetBit(psNewCandidate->auFlag, INST_INCANDIDATELIST, 1);
	}
}

static
IMG_VOID FreeDualIssueCandidate(PINTERMEDIATE_STATE		psState,
								PDUALISSUE_CANDIDATE	psCandidate)
/*****************************************************************************
 FUNCTION     : FreeDualIssueCandidate
    
 PURPOSE      : Free information about a possible site for dual-issuing a
				load into an internal register.

 PARAMETERS   : psState				- Compiler state.	
				psCandidate			- Information to free.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	ASSERT(!psCandidate->bInReconsiderList);

	while ((psListEntry = RemoveListHead(&psCandidate->sIntervalList)) != NULL)
	{
		PDUALISSUE_INTERVAL		psDIInterval;

		psDIInterval = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_INTERVAL, sIntervalListEntry);

		ASSERT(psDIInterval->psCandidate == psCandidate);
		RemoveFromList(&psDIInterval->psInterval->sCandidateList, &psDIInterval->sCandidateListEntry);
		UscFree(psState, psDIInterval);
	}
	UscFree(psState, psCandidate);
}

static
IMG_VOID FreeDualIssueCandidateInst(PINTERMEDIATE_STATE psState, PDUALISSUE_CANDIDATE psCandidate)
/*****************************************************************************
 FUNCTION     : FreeDualIssueCandidateInst
    
 PURPOSE      : Drop an instruction which was a possible site for dual-issuing
			    with a move into an internal register.

 PARAMETERS   : psState				- Compiler state.	
				psCandidate			- Information to free.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PINST	psInst = psCandidate->psInst;

	ASSERT(psInst->sStageData.pvData == psCandidate);
	psInst->sStageData.pvData = NULL;
			
	ASSERT(GetBit(psInst->auFlag, INST_INCANDIDATELIST) == 1);
	SetBit(psInst->auFlag, INST_INCANDIDATELIST, 0);

	FreeDualIssueCandidate(psState, psCandidate);
}

static
IMG_VOID ReconsiderIntervalCandidates(PINTERMEDIATE_STATE	psState,
									  PIREGALLOC_CONTEXT	psContext,
									  PUSC_LIST				psMasterCandidateList,
									  PUSC_LIST				psReconsiderList)
/*****************************************************************************
 FUNCTION     : ReconsiderIntervalCandidates
    
 PURPOSE      : When an interval is replaced by a unified store register
				recheck whether dual-issue sites using the interval in a
				source or destination argument are still valid dual-issue
				instructions.

 PARAMETERS   : psState					- Compiler state.	
				psContext				- Internal register allocator state.
				psMasterCandidateList	- List of possible dual-issue sites.
				psReconsiderList		- List of instructions where some
										sources or destinations have been replaced
										by unified store registers.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	while ((psListEntry = RemoveListHead(psReconsiderList)) != NULL)
	{
		PDUALISSUE_CANDIDATE	psCandidate;

		psCandidate = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_CANDIDATE, sReconsiderListEntry);

		ASSERT(psCandidate->bInReconsiderList);
		psCandidate->bInReconsiderList = IMG_FALSE;

		GetIRegAssignment(psState, psContext, psCandidate->psInst, &psCandidate->sIRegAssignment);

		if (!IsPossibleDualIssueWithVMOV(psState, 
									     psCandidate->psInst,
										 &psCandidate->sIRegAssignment,
										 NULL /* psVMOVInst */,
										 NULL /* psVMOVIRegAssigment */,
										 0 /* uLiveChansInVMOVDest */,
										 IMG_FALSE /* bCreateDual */,
										 NULL /* ppsDualIsseInst */))
		{
			RemoveFromList(psMasterCandidateList, &psCandidate->sMasterListEntry);
			FreeDualIssueCandidateInst(psState, psCandidate);
		}
	}
}

static
IMG_VOID GetReconsiderList(PINTERMEDIATE_STATE	psState,
						   PUSC_LIST			psReconsiderList,
						   PINTERVAL			psInterval)
/*****************************************************************************
 FUNCTION     : GetReconsiderList
    
 PURPOSE      : When an interval is replaced by a unified store register
				get a list of the instructions with modified sources or
				destinations.

 PARAMETERS   : psState					- Compiler state.	
				psReconsiderList		- List to append modified instructions to.
				psInterval				- Interval which has been replaced by
										a unified store register.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psCandidateListEntry;

	while ((psCandidateListEntry = RemoveListHead(&psInterval->sCandidateList)) != NULL)
	{
		PDUALISSUE_INTERVAL		psDIInterval;
		PDUALISSUE_CANDIDATE	psCandidate;

		psDIInterval = IMG_CONTAINING_RECORD(psCandidateListEntry, PDUALISSUE_INTERVAL, sCandidateListEntry);

		ASSERT(psDIInterval->psInterval == psInterval);
		psCandidate = psDIInterval->psCandidate;

		RemoveFromList(&psCandidate->sIntervalList, &psDIInterval->sIntervalListEntry);

		if (!psCandidate->bInReconsiderList)
		{
			psCandidate->bInReconsiderList = IMG_TRUE;
			AppendToList(psReconsiderList, &psCandidate->sReconsiderListEntry);
		}

		UscFree(psState, psDIInterval);
	}
}

typedef struct _RESTORE_INST
{
	USC_LIST_ENTRY		sListEntry;
	PINST				psRestoreInst;
} RESTORE_INST_DATA, *PRESTORE_INST_DATA;

static
IMG_INT32 CmpRestoreInstData(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
{
	PRESTORE_INST_DATA	psElement1 = IMG_CONTAINING_RECORD(psListEntry1, PRESTORE_INST_DATA, sListEntry);
	PRESTORE_INST_DATA	psElement2 = IMG_CONTAINING_RECORD(psListEntry2, PRESTORE_INST_DATA, sListEntry);

	return psElement1->psRestoreInst->uBlockIndex - psElement2->psRestoreInst->uBlockIndex;
}

static
IMG_VOID GetLastUsesAsSrc(PINTERMEDIATE_STATE	psState,
						  PUSEDEF_CHAIN			psUseDefChain,
						  PINST*				ppsLastUsesInst,
						  IMG_PUINT32			puLastUsesSrcMask)
/*****************************************************************************
 FUNCTION     : GetLastUsesAsSrc
    
 PURPOSE      : Get uses of an intermediate register as a source in the last
				instruction where it is used.

 PARAMETERS   : psState				- Compiler state.	
				psUseDefChain		- Intermediate register.
				ppsLastUsesInst		- Returns a pointer to the instruction where
									the intermediate register is last used.
				puLastUsesSrcMask	- Returns a mask of the source arguments
									in the last instruction where the intermediate
									regiser is used.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psLastUsesInst;
	IMG_UINT32		uLastUsesSrcMask;

	psLastUsesInst = NULL;
	uLastUsesSrcMask = 0;
	for (psListEntry = psUseDefChain->sList.psTail; psListEntry != NULL; psListEntry = psListEntry->psPrev)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psUse));

		if (psUse->eType == DEF_TYPE_INST)
		{
			break;
		}

		if (psLastUsesInst != NULL && psUse->u.psInst != psLastUsesInst)
		{
			break;
		}
		psLastUsesInst = psUse->u.psInst;

		ASSERT(psUse->eType == USE_TYPE_SRC);
		ASSERT(psUse->uLocation < BITS_PER_UINT);
		uLastUsesSrcMask |= (1 << psUse->uLocation);
	}
	*ppsLastUsesInst = psLastUsesInst;
	*puLastUsesSrcMask = uLastUsesSrcMask;
}

static
IMG_BOOL CanSubstForNoCost(PINTERMEDIATE_STATE		psState,
						   PIREGALLOC_CONTEXT		psContext,
						   PIREGALLOC_BLOCK			psBlockState,
						   PCODEBLOCK				psBlock,
						   PUSEDEF_CHAIN			psEndTemp,
						   PUSEDEF_CHAIN			psBaseTemp,
						   PINST					psBaseDefInst,
						   IMG_UINT32				uBaseDefDestIdx,
						   IMG_UINT32				uBaseDefSrcMask,
						   PINST					psThisRangeEndInst,
						   IMG_BOOL					bCheckOnly)
/*****************************************************************************
 FUNCTION     : CanSubstForNoCost
    
 PURPOSE      : Check if an interval can be replaced by a non-internal registers
				without requiring any extra instructions or secondary attributes.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- Allocator state for the current basic block.
				psBlock				- Current basic block.
				psEndTemp
				psBaseTemp			- Start of the chain of intermediate registers
									making up the interval.
				psBaseDefInst		- Instruction defining the start of the chain.
				uBaseDefDestIdx		- Destination argument defining the start of the chain.
				uBaseDefSrcMask		- Also substitute this mask of sources in the
									defining instruction.
				psThisRangeEndInst	- Don't check references to the interval in or
									after this instruction.
				bCheckOnly			- If TRUE just check whether substitution is possible.
									  If FALSE apply the substitution.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	IMG_INT32	iMergeNumExtraInsts;
	IMG_UINT32	uMergeNumExtraSecAttrs;
	IMG_BOOL	bRet;

	bRet = BaseSubstIRegInterval(psState,
								 psContext,
								 psBlockState,
								 psEndTemp,
								 psBaseTemp,
								 psBaseDefInst,
								 uBaseDefDestIdx,
								 uBaseDefSrcMask,
								 psBlock->psBody,
								 psThisRangeEndInst,
								 bCheckOnly,
								 &iMergeNumExtraInsts,
								 &uMergeNumExtraSecAttrs,
								 0 /* uAlreadyUsedSecAttrs */);
	if (bRet && iMergeNumExtraInsts <= 0 && uMergeNumExtraSecAttrs == 0)
	{
		return IMG_TRUE;
	}
	ASSERT(bCheckOnly);
	return IMG_FALSE;
}

static
IMG_VOID SubstHwReg(PINTERMEDIATE_STATE	psState, 
					PIREGALLOC_CONTEXT	psContext,
					PIREGALLOC_BLOCK	psBlockState,
					PCODEBLOCK			psBlock,
					PINTERVAL			psStartInterval,
					IMG_UINT32			uLastUseInst,
					PINTERVAL*			ppsEndInterval)
/*****************************************************************************
 FUNCTION     : SubstHwReg
    
 PURPOSE      : Substitute uses of a specific internal register by non-internal
				registers.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- Allocator state for the current basic block.
				psBlock				- Current basic block.
				psStartInterval		- Substitute intervals using the same internal
									register as this interval and starting before.
			    uLastUseInst		- If not USC_UNDEF then substitute intervals
									whose last uses is after this instruction.
									  If USC_UNDEF then just find the first interval
								    which can't be substituted.
				ppsEndInterval		- If non-NULL then returns the first interval
									which can't be substituted.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32			uHwReg;
	SAFE_LIST_ITERATOR	sIter;
	PINST				psNextRangeEndInst;
	IMG_BOOL			bCheckOnly;

	/*
		Just checking for the maximum possible range of substitutions or actually substituting?
	*/
	bCheckOnly = IMG_TRUE;
	if (uLastUseInst != USC_UNDEF)
	{
		bCheckOnly = IMG_FALSE;
	}

	ASSERT(psStartInterval->eState == INTERVAL_STATE_ASSIGNED);

	/*
		Get the hardware register we are substituting for.
	*/
	uHwReg = psStartInterval->uHwReg;
	ASSERT(uHwReg < psContext->uNumHwRegs);

	psNextRangeEndInst = psBlock->psBodyTail;

	/*
		Initialize an iterator at the interval which is assigned to the same hardware register and
		which ends immediately before the start of the moving interval.
	*/
	SafeListIteratorInitializeAtPoint(&psContext->asHwReg[uHwReg].sHwRegIntervalList,
									  &sIter,
									  psStartInterval->sHwRegIntervalListEntry.psPrev);
	if (!SafeListIteratorContinue(&sIter))
	{
		if (ppsEndInterval != NULL)
		{
			*ppsEndInterval = NULL;
		}
		SafeListIteratorFinalise(&sIter);
		return;
	}
	for (;;)
	{
		PINTERVAL		psInterval;
		PINTERVAL		psBaseInterval;
		PUSEDEF_CHAIN	psBaseTemp;
		PINST			psBaseDefInst;
		IMG_UINT32		uBaseDefDestIdx;
		PINST			psLastUsesInst;
		IMG_UINT32		uLastUsesSrcMask;
		IMG_UINT32		uSrcMaskInBaseDefInst;
		PINST			psThisRangeEndInst = psNextRangeEndInst;
		PINTERVAL		psPrevInterval;
		IMG_BOOL		bSubst;
		IMG_UINT32		uLastUse;

		psInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, sHwRegIntervalListEntry);
		
		uLastUse = GetLastUseOfInterval(psState, psBlock, psInterval);
		if (uLastUseInst != USC_UNDEF && uLastUse <= uLastUseInst)
		{
			break;
		}

		ASSERT(psInterval->eState == INTERVAL_STATE_ASSIGNED);
		ASSERT(psInterval->uHwReg == uHwReg);

		/*	
			Find the start of the chain of intermediate registers making up this interval.
		*/
		GetBaseInterval(psState, 
						psBlock, 
						psInterval->psUseDefChain, 
						psBlock->psBody, 
						&psBaseTemp, 
						&psBaseDefInst, 
						&uBaseDefDestIdx);
		
		psBaseInterval = GetInterval(psContext, psBaseTemp->uNumber);

		/*
			Rewind the iterator to the same point.
		*/
		while (&psBaseInterval->sHwRegIntervalListEntry != SafeListIteratorCurrent(&sIter))
		{
			SafeListIteratorPrev(&sIter);
		}

		/*
			Check if there is another interval assigned to the same hardware register and ending before
			the current interval.
		*/
		SafeListIteratorPrev(&sIter);
		psPrevInterval = NULL;
		psLastUsesInst = NULL;
		uLastUsesSrcMask = 0;
		if (SafeListIteratorContinue(&sIter))
		{
			PINTERVAL	psSameHwRegInterval;
			PINST		psSameHwRegLastUsesInst;
			IMG_UINT32	uSameHwRegLastUsesSrcMask;

			psSameHwRegInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, sHwRegIntervalListEntry);
			GetLastUsesAsSrc(psState, psSameHwRegInterval->psUseDefChain, &psSameHwRegLastUsesInst, &uSameHwRegLastUsesSrcMask);

			if (psSameHwRegLastUsesInst != NULL)
			{
				if (!(uLastUseInst != USC_UNDEF && psSameHwRegLastUsesInst->uBlockIndex <= uLastUseInst))
				{
					psPrevInterval = psSameHwRegInterval;
					psLastUsesInst = psSameHwRegLastUsesInst;
					uLastUsesSrcMask = uSameHwRegLastUsesSrcMask;
				}
			}
		}

		/*
			Check if the previous interval is used as a source in the instruction which defines the start of the current
			interval.

			If so then check if these sources can be substituted when we check if the destination can be substituted.
		*/
		if (psLastUsesInst == psBaseDefInst)
		{
			uSrcMaskInBaseDefInst = uLastUsesSrcMask;
			psNextRangeEndInst = psLastUsesInst->psPrev;
		}
		else
		{
			uSrcMaskInBaseDefInst = 0;
			psNextRangeEndInst = psBaseDefInst->psPrev;
		}

		/*
			Check if the current interval can be substituted by non-internal registers.
		*/
		bSubst = 
			CanSubstForNoCost(psState,
							  psContext,
							  psBlockState,
							  psBlock,
							  psInterval->psUseDefChain,
							  psBaseTemp,
							  psBaseDefInst,
							  uBaseDefDestIdx,
							  uSrcMaskInBaseDefInst,
							  psThisRangeEndInst,
							  bCheckOnly);
		if (!bSubst)
		{
			ASSERT(bCheckOnly);

			/*
				Check if the current interval can be substituted by non-internal registers but without substituting
				the previous interval.
			*/
			bSubst = CanSubstForNoCost(psState,
									   psContext,
									   psBlockState,
									   psBlock,
									   psInterval->psUseDefChain,
									   psBaseTemp,
									   psBaseDefInst,
									   uBaseDefDestIdx,
									   0 /* uSrcMaskInBaseDefInst */,
									   psThisRangeEndInst,
									   bCheckOnly);

			if (bSubst)
			{
				/*
					The range of possible substitutions ends at this interval.
				*/
				*ppsEndInterval = psPrevInterval;
			}
			else
			{
				/*
					The range of possible substitutions ends before this interval.
				*/
				*ppsEndInterval = psInterval;
			}
			break;
		}

		if (!SafeListIteratorContinue(&sIter))
		{
			ASSERT(psPrevInterval == NULL);

			/*
				All previous intervals can be substituted.
			*/
			if (ppsEndInterval != NULL)
			{
				*ppsEndInterval = NULL;
			}
			break;
		}
	}
	SafeListIteratorFinalise(&sIter);
}	

static
IMG_VOID GetReconsiderListUsingIntervals(PINTERMEDIATE_STATE	psState,
										 PIREGALLOC_CONTEXT		psContext,
										 PCODEBLOCK				psBlock,
										 PUSC_LIST				psReconsiderList,
										 PINST					psDualIssueInst,
										 PINTERVAL				psStartInterval)
/*****************************************************************************
 FUNCTION     : ReconsiderCandidatesUsingIntervals
    
 PURPOSE      : When moving the define of an interval backwards: create a list
			    of the dual-issue candidates which need to have an internal
				register source or destination replaced by a non-internal register.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlock				- Current basic block.
				psReconsiderList	- Returns the list of modified instructions.
				psDualIssueInst		- Location of the newly created dual-issue
									instruction.
				psStartInterval		- Interval for the destination of the restore
									instruction which has been moved backwards to
									the dual-issue instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	InitializeList(psReconsiderList);

	ASSERT(psStartInterval->eState == INTERVAL_STATE_ASSIGNED);
	ASSERT(psStartInterval->uHwReg < psContext->uNumHwRegs);

	SafeListIteratorInitializeAtPoint(&psContext->asHwReg[psStartInterval->uHwReg].sHwRegIntervalList,
									  &sIter,
									  psStartInterval->sHwRegIntervalListEntry.psPrev);
	for (; SafeListIteratorContinue(&sIter); SafeListIteratorPrev(&sIter))
	{
		IMG_UINT32	uLastUse;
		PINTERVAL	psPrevInterval;

		psPrevInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, sHwRegIntervalListEntry);
		uLastUse = GetLastUseOfInterval(psState, psBlock, psPrevInterval);

		/*
			Stop once we reach an interval before the dual-issue site.
		*/
		if (uLastUse <= psDualIssueInst->uBlockIndex)
		{
			break;
		}

		/*
			For any possible dual-issue sites which use the interval reconsider whether they
			could be validly dual-issued.
		*/
		GetReconsiderList(psState, psReconsiderList, psPrevInterval);
	}
	SafeListIteratorFinalise(&sIter);
}

static
IMG_VOID GetRestoreDefine(PCODEBLOCK			psBlock, 
						  IMG_UINT32			uRegisterType, 
						  PVREGISTER			psRegister, 
						  IMG_PUINT32			puFirstPossibleLocation)
/*****************************************************************************
 FUNCTION     : GetRestoreDefine
    
 PURPOSE      : Update the starting point for locations where a restore instruction
				can be dual-issued to after the instruction defining one of the
				source arguments to the restore instruction.

 PARAMETERS   : psBlock				- Current basic block.
				uRegisterType		- Source argument.
				psRegister
				puFirstPossibleLocation
									- Points to the index of an instruction in
									the current block. 

 RETURNS      : Nothing.
*****************************************************************************/
{
	/*
		Get the instruction which defines the source data.
	*/
	if (uRegisterType == USEASM_REGTYPE_TEMP)
	{
		PINST	psSrcDefInst;

		psSrcDefInst = UseDefGetDefInstFromChain(psRegister->psUseDefChain, NULL /* puDestIdx */);
		if (psSrcDefInst != NULL && psSrcDefInst->psBlock == psBlock)
		{	
			IMG_UINT32	uFirstInstAfterDef = psSrcDefInst->uBlockIndex + 1;

			/*	
				Don't consider any dual-issue sites which are before the definition of the source data.
			*/
			if (uFirstInstAfterDef > (*puFirstPossibleLocation))
			{
				(*puFirstPossibleLocation) = uFirstInstAfterDef;
			}
		}
	}
}

static
IMG_BOOL GetRestoreSrcDefine(PINTERMEDIATE_STATE	psState, 
							 PIREGALLOC_CONTEXT		psContext,
							 PCODEBLOCK				psBlock, 
							 PINST					psRestoreInst,
							 IMG_PUINT32			puFirstPossibleLocation)
/*****************************************************************************
 FUNCTION     : GetRestoreSrcDefine
    
 PURPOSE      : Update the starting point for locations where a restore instruction
				can be dual-issued to after all the instructions defining the source
				data to the restore instruction.

 PARAMETERS   : psState				- Compiler state.
			    psContext			- Internal register allocator state.
				psBlock				- Current basic block.
				psRestoreInst		- Restore instruction.
				puFirstPossibleLocation
									- Points to the index of an instruction in
									the current block. 

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	for (uArg = 0; uArg < psRestoreInst->uArgumentCount; uArg++)
	{
		PARG	psRestoreSrc;

		/*
			Get the source data which is copied into the internal register.
		*/
		psRestoreSrc = &psRestoreInst->asArg[uArg];
		ASSERT(!IsIRegArgument(psState, psContext, psRestoreSrc, NULL /* puAssignedIReg */));

		/*
			To simplify dependence checking: skip instructions loading data which is defined
			more than once.
		*/
		if (IsNonSSAArgument(psState, psRestoreSrc))
		{
			return IMG_FALSE;
		}

		GetRestoreDefine(psBlock, psRestoreSrc->uType, psRestoreSrc->psRegister, puFirstPossibleLocation);
		GetRestoreDefine(psBlock, psRestoreSrc->uIndexType, psRestoreSrc->psIndexRegister, puFirstPossibleLocation);
	}
	return IMG_TRUE;
}

static
IMG_VOID DualIssueInternalRegisterRestores(PINTERMEDIATE_STATE		psState, 
										   PIREGALLOC_CONTEXT		psContext, 
										   PIREGALLOC_BLOCK			psBlockState,
										   PCODEBLOCK				psBlock)
/*****************************************************************************
 FUNCTION     : DualIssueInternalRegisterRestores
    
 PURPOSE      : Try to dual-issue loading into internal registers with
				other instructions.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- State for the current block.
				psBlock				- Current basic block.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PINST			psInst;
	USC_LIST		sMasterCandidateList;
	PUSC_LIST_ENTRY	psListEntry;
	USC_LIST		sRestoreInstList;
	IMG_UINT32		uOpTypeIdx;

	/*
		Create a list of all the instructions loaded data into an internal register in the current basic block,
		sorted by their order of appearance in the block.
	*/
	InitializeList(&sRestoreInstList);
	for (uOpTypeIdx = 0; uOpTypeIdx < 2; uOpTypeIdx++)
	{
		static const IOPCODE	aeOpType[] = {IRESTOREIREG, IVLOAD};
		IOPCODE					eOpType = aeOpType[uOpTypeIdx];
		SAFE_LIST_ITERATOR		sIterator;

		InstListIteratorInitialize(psState, eOpType, &sIterator);
		for (; InstListIteratorContinue(&sIterator); InstListIteratorNext(&sIterator))
		{
			PINST				psRestoreInst;
			PRESTORE_INST_DATA	psRestoreData;

			psRestoreInst = InstListIteratorCurrent(&sIterator);
			if (psRestoreInst->psBlock != psBlock)
			{
				continue;
			}

			psRestoreData = UscAlloc(psState, sizeof(*psRestoreData));
			psRestoreData->psRestoreInst = psRestoreInst;
			InsertInListSorted(&sRestoreInstList, CmpRestoreInstData, &psRestoreData->sListEntry);
		}
		InstListIteratorFinalise(&sIterator);
	}

	/*
		Nothing to do if there are no loads into internal registers.
	*/
	if (IsListEmpty(&sRestoreInstList))
	{
		return;
	}

	/*
		Make of a list of all the instructions which are potential candidates for dual-issuing
		together with a load into an internal register.
	*/
	InitializeList(&sMasterCandidateList);
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IREG_ASSIGNMENT	sIRegAssignment;
		PINTERVAL		asArgInterval[VECTOR_MAX_SOURCE_SLOT_COUNT + 1];
		IMG_UINT32		uSlot;
		
		if (psInst->uDestCount != 1 || (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}
		if (psInst->eOpcode == IVLOAD)
		{
			continue;
		}

		/*
			Get the source/destination arguments which have been assigned internal registers.
		*/
		for (uSlot = 0; uSlot <= GetSwizzleSlotCount(psState, psInst); uSlot++)
		{
			PARG		psArg;
			PINTERVAL	psArgInterval;
			IMG_UINT32	uArgIReg;
			
			if (uSlot == 0)
			{
				psArg = &psInst->asDest[0];
			}
			else
			{
				psArg = &psInst->asArg[(uSlot - 1) * SOURCE_ARGUMENTS_PER_VECTOR];
			}
			
			psArgInterval = GetIntervalByArg(psContext, psArg);

			/*
				Get the internal register assigned to this interval (if any).
			*/
			uArgIReg = USC_UNDEF;
			if (psArgInterval != NULL && psArgInterval->eState == INTERVAL_STATE_ASSIGNED)
			{
				asArgInterval[uSlot] = psArgInterval;

				ASSERT(psArgInterval->uHwReg != USC_UNDEF);
				uArgIReg = psArgInterval->uHwReg;	
			}
			else
			{
				asArgInterval[uSlot] = NULL;
			}

			if (uSlot == 0)
			{
				sIRegAssignment.bDestIsIReg = (uArgIReg != USC_UNDEF) ? IMG_TRUE : IMG_FALSE;
			}
			else
			{
				sIRegAssignment.auIRegSource[uSlot - 1] = uArgIReg;
			}
		}

		/*
			Check for an instruction which could be dual-issued with a VMOV.
		*/
		if (IsPossibleDualIssueWithVMOV(psState, 
									    psInst,
										&sIRegAssignment,
										NULL /* psVMOVInst */,
										NULL /* psVMOVIRegAssigment */,
										0 /* uLiveChansInVMOVDest */,
										IMG_FALSE /* bCreateDual */,
										NULL /* ppsDualIssueInst */))
		{
			PDUALISSUE_CANDIDATE	psCandidate;

			psCandidate = UscAlloc(psState, sizeof(*psCandidate));
			psCandidate->psInst = psInst;
			psCandidate->sIRegAssignment = sIRegAssignment;
			psCandidate->bInLocalList = IMG_FALSE;
			psCandidate->bInReconsiderList = IMG_FALSE;
			ClearListEntry(&psCandidate->sReconsiderListEntry);
			InitializeList(&psCandidate->sIntervalList);
			AppendToList(&sMasterCandidateList, &psCandidate->sMasterListEntry);

			SetBit(psInst->auFlag, INST_INCANDIDATELIST, 1);
			psInst->sStageData.pvData = psCandidate;

			/*
				Create a link between all the intervals used in the instruction and the
				instruction.
			*/
			for (uSlot = 0; uSlot <= GetSwizzleSlotCount(psState, psInst); uSlot++)
			{
				PDUALISSUE_INTERVAL	psDIInterval;
				PINTERVAL			psArgInterval;

				psArgInterval = asArgInterval[uSlot];
				if (psArgInterval != NULL)
				{
					psDIInterval = UscAlloc(psState, sizeof(*psDIInterval));
					psDIInterval->psCandidate = psCandidate;
					psDIInterval->psInterval = psArgInterval;
					AppendToList(&psCandidate->sIntervalList, &psDIInterval->sIntervalListEntry);
					if (uSlot == 0)
					{
						psDIInterval->bDest = IMG_TRUE;
					}
					else
					{
						psDIInterval->bDest = IMG_FALSE;
					}
					AppendToList(&psArgInterval->sCandidateList, &psDIInterval->sCandidateListEntry);
				}
			}
		}
	}

	/*
		Check for all the instructions loading data into an internal register whether we can dual-issue them.
	*/
	while ((psListEntry = RemoveListHead(&sRestoreInstList)) != NULL)
	{
		PINST				psRestoreInst;
		PINTERVAL			psDestInterval;
		IREG_ASSIGNMENT		sRestoreIRegAssignment;
		IMG_UINT32			uFirstPossibleLocation;
		PUSC_LIST_ENTRY		psCandidateListEntry;
		PINTERVAL			psFirstUnsubstInterval;
		PINST				psDeschedInst;
		USC_LIST			sLocalCandidateList;
		PRESTORE_INST_DATA	psRestoreInstData;
		IMG_UINT32			uRestoreDestHwReg;
		SAFE_LIST_ITERATOR	sIter;

		psRestoreInstData = IMG_CONTAINING_RECORD(psListEntry, PRESTORE_INST_DATA, sListEntry);
		psRestoreInst = psRestoreInstData->psRestoreInst;
		UscFree(psState, psRestoreInstData);
		psRestoreInstData = NULL;

		/*
			Nothing to do if there are no possible candidates for dual-issues.
		*/
		if (IsListEmpty(&sMasterCandidateList))
		{
			continue;
		}

		/*
			Get information about the internal register which is the destination of the load.
		*/
		psDestInterval = GetIntervalByArg(psContext, &psRestoreInst->asDest[0]);
		if (psDestInterval == NULL)
		{
			continue;
		}
		ASSERT(psDestInterval->eState == INTERVAL_STATE_ASSIGNED);
		uRestoreDestHwReg = psDestInterval->uHwReg;
		ASSERT(uRestoreDestHwReg != USC_UNDEF);

		/*
			Create a copy of the list of dual-issue candidates available for this particular
			instruction.
		*/
		InitializeList(&sLocalCandidateList);
		for (psCandidateListEntry = sMasterCandidateList.psHead; 
			 psCandidateListEntry != NULL;
			 psCandidateListEntry = psCandidateListEntry->psNext)
		{
			PDUALISSUE_CANDIDATE	psCandidate;

			psCandidate = IMG_CONTAINING_RECORD(psCandidateListEntry, PDUALISSUE_CANDIDATE, sMasterListEntry);

			/*
				The dual-issue candidates are sorted in the same order as instructions in the block
				so stop once we see one after the restore instruction.
			*/
			if (psCandidate->psInst->uBlockIndex >= psRestoreInst->uBlockIndex)
			{
				break;
			}

			psCandidate->bInLocalList = IMG_TRUE;
			psCandidate->bDestIRegReplaced = IMG_FALSE;
			AppendToList(&sLocalCandidateList, &psCandidate->sLocalListEntry);
		}

		/*
			Check backwards (in the order of instructions in the block) for intervals assigned
			to the same internal register. Unless we can replace the previous intervals by
			unified store registers we can't use dual-issue sites before the last use of
			the same internal register.
		*/
		SubstHwReg(psState, 
				   psContext, 
				   psBlockState, 
				   psBlock, 
				   psDestInterval, 
				   USC_UNDEF /* uLastUseInst */, 
				   &psFirstUnsubstInterval);

		SafeListIteratorInitializeAtPoint(&psContext->asHwReg[uRestoreDestHwReg].sHwRegIntervalList,
										  &sIter,
										  psDestInterval->sHwRegIntervalListEntry.psPrev);
		for (; SafeListIteratorContinue(&sIter); SafeListIteratorPrev(&sIter))
		{
			PINTERVAL	psPrevInterval;
			IMG_UINT32	uLastUse;

			psPrevInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, sHwRegIntervalListEntry);
			if (psPrevInterval == psFirstUnsubstInterval)
			{
				break;
			}

			/*
				Get the index of the instruction containing the last use of this interval.
			*/
			uLastUse = GetLastUseOfInterval(psState, psBlock, psPrevInterval);

			/*
				Check all the dual-issue candidates which have the interval
				as a source or destination.
			*/
			for (psCandidateListEntry = psPrevInterval->sCandidateList.psHead;
				 psCandidateListEntry != NULL;
				 psCandidateListEntry = psCandidateListEntry->psNext)
			{
				PDUALISSUE_INTERVAL		psDIInterval;
				PDUALISSUE_CANDIDATE	psCandidate;

				psDIInterval = IMG_CONTAINING_RECORD(psCandidateListEntry, PDUALISSUE_INTERVAL, sCandidateListEntry);
				psCandidate = psDIInterval->psCandidate;
				if (psCandidate->bInLocalList)
				{
					IMG_BOOL	bRemoveFromLocalList;

					bRemoveFromLocalList = IMG_FALSE;
					if (psDIInterval->bDest)
					{
						/*
							This interval is the destination of the dual-issue candidate. We may still be
							able to create a dual-issue instruction but override the information about
							which instruction arguments are internal registers.
						*/
						if (!IsSimpleDestinationSubst(psState, psCandidate->psInst, 0 /* uDestIdx */))
						{
							bRemoveFromLocalList = IMG_TRUE;
						}
						else
						{
							psCandidate->bDestIRegReplaced  = IMG_TRUE;
						}
					}
					else
					{
						/*
							Dual-issue instructions can only have one unified store source so substituting the 
							interval for a temporary register would make it impossible to dual-issue them with a load
							into an internal register (which also requires use of the unified store source).
						*/
						if (psCandidate->psInst->uBlockIndex != uLastUse)
						{
							bRemoveFromLocalList = IMG_TRUE;
						}
					}

					if (bRemoveFromLocalList)
					{
						psCandidate->bInLocalList = IMG_FALSE;
						RemoveFromList(&sLocalCandidateList, &psCandidate->sLocalListEntry);
					}
				}
			}
		}
		SafeListIteratorFinalise(&sIter);

		if (psFirstUnsubstInterval != NULL)
		{
			/*
				Start our search for dual-issue sites from the instruction which contains the last use of the
				previous interval.
			*/
			uFirstPossibleLocation = GetLastUseOfInterval(psState, psBlock, psFirstUnsubstInterval);
		}
		else
		{
			/*
				Start our search from the beginning of the block.
			*/
			uFirstPossibleLocation = 0;
		}

		/*
			Don't consider dual-issue locations before the instructions defining the source
			arguments to the restore instruction.
		*/
		if (!GetRestoreSrcDefine(psState, psContext, psBlock, psRestoreInst, &uFirstPossibleLocation))
		{
			continue;
		}
		
		/*
			Check for the first descheduling instruction before the copy into the internal register.
		*/
		for (psDeschedInst = psRestoreInst->psPrev; psDeschedInst != NULL; psDeschedInst = psDeschedInst->psPrev)
		{
			if (psDeschedInst->uBlockIndex < uFirstPossibleLocation)
			{
				break;
			}
			if (IsDeschedAfterInst(psDeschedInst))
			{
				uFirstPossibleLocation = psDeschedInst->uBlockIndex + 1;
				break;
			}
			else if (IsDeschedBeforeInst(psState, psDeschedInst))
			{
				uFirstPossibleLocation = psDeschedInst->uBlockIndex;
				break;
			}
		}

		/*
			Get the sources/destinations to the restore instruction which are internal registers.
		*/
		sRestoreIRegAssignment.bDestIsIReg = IMG_TRUE;
		sRestoreIRegAssignment.auIRegSource[0] = USC_UNDEF;

		for (psCandidateListEntry = sLocalCandidateList.psHead; 
			 psCandidateListEntry != NULL;
			 psCandidateListEntry = psCandidateListEntry->psNext)
		{
			PDUALISSUE_CANDIDATE	psCandidate;
			IREG_ASSIGNMENT			sCandidateIRegAssignment;
			PINST					psCandidateInst;

			psCandidate = IMG_CONTAINING_RECORD(psCandidateListEntry, PDUALISSUE_CANDIDATE, sLocalListEntry);
			if (psCandidate->psInst->uBlockIndex < uFirstPossibleLocation)
			{
				continue;
			}

			psCandidateInst = psCandidate->psInst;

			sCandidateIRegAssignment = psCandidate->sIRegAssignment;
			if (psCandidate->bDestIRegReplaced)
			{
				sCandidateIRegAssignment.bDestIsIReg = IMG_FALSE;
			}

			/*
				Check if the VMOV and the candidate instruction can be dual-issued.
			*/
			if (IsPossibleDualIssueWithVMOV(psState, 
											psCandidateInst, 
											&sCandidateIRegAssignment,
											psRestoreInst,
											&sRestoreIRegAssignment,
											psRestoreInst->auLiveChansInDest[0], 
											IMG_FALSE /* bCreateDual */,
											NULL /* ppsDualIssueInst */))
			{
				PINST		psDualIssueInst;
				IMG_BOOL	bRet;
				USC_LIST	sReconsiderList;

				/*
					Remove the dual-issue site from the list of possible sites.
				*/
				ASSERT(psCandidateInst->sStageData.pvData == psCandidate);
				psCandidateInst->sStageData.pvData = NULL;
				SetBit(psCandidateInst->auFlag, INST_INCANDIDATELIST, 0);

				RemoveFromList(&sMasterCandidateList, &psCandidate->sMasterListEntry);
				FreeDualIssueCandidate(psState, psCandidate);
				psCandidate = NULL;

				/*
					Get a list of the instructions modified in the next step.
				*/
				GetReconsiderListUsingIntervals(psState, 
												psContext, 
												psBlock, 
												&sReconsiderList,
												psCandidateInst,
												psDestInterval);

				/*
					For any uses of the internal register destination of the restore
					instruction between the original location of the restore and the dual-issue
					site: replace the internal register by a unified store register.
				*/
				SubstHwReg(psState, 
						   psContext, 
						   psBlockState, 
						   psBlock, 
						   psDestInterval, 
						   psCandidateInst->uBlockIndex, 
						   NULL /* ppsEndInterval */);

				/*
					For any uses of the internal register destination of the restore
					instruction between the original location of the restore and the dual-issue
					site: reconsider whether the using instructions are possible dual-issue
					candidates.
				*/
				ReconsiderIntervalCandidates(psState, psContext, &sMasterCandidateList, &sReconsiderList);

				/*
					Create the dual-issue instruction and insert it immediately before
					the candidate instruction.
				*/
				bRet = 
					IsPossibleDualIssueWithVMOV(psState, 
												psCandidateInst, 
												&sCandidateIRegAssignment,
												psRestoreInst,
												&sRestoreIRegAssignment,
												psRestoreInst->auLiveChansInDest[0], 
												IMG_TRUE /* bCreateDual */,
												&psDualIssueInst);
				ASSERT(bRet);

				/*
					Remove and free the two instructions which have been combined
					into a dual-issue.
				*/
				RemoveInst(psState, psBlock, psCandidateInst);
				FreeInst(psState, psCandidateInst);

				RemoveInst(psState, psBlock, psRestoreInst);
				FreeInst(psState, psRestoreInst);
				psRestoreInst = NULL;

				/*
					The restore instruction has been converted to a dual-issue.
				*/
				break;
			}
		}
	}

	/*
		Free used memory.
	*/
	while ((psListEntry = RemoveListHead(&sMasterCandidateList)) != NULL)
	{
		PDUALISSUE_CANDIDATE	psCandidate;

		psCandidate = IMG_CONTAINING_RECORD(psListEntry, PDUALISSUE_CANDIDATE, sMasterListEntry);

		FreeDualIssueCandidateInst(psState, psCandidate);
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID GetChanUse(IMG_UINT32 uChanMask,
					IMG_PBOOL pbRGB,
					IMG_PBOOL pbAlpha)
/*****************************************************************************
 FUNCTION     : GetChanUse
    
 PURPOSE      : Test whether a mask specifies RGB or Alpha channels.

 PARAMETERS   : uChanMask     - Mask to test

 OUTPUT       : pbRGB         - Whether RGB channels are specified
                pbAlpha       - Whether Alpha channels are specified

 RETURNS      : Nothing
*****************************************************************************/
{
	if (pbRGB != NULL)
	{
		if (uChanMask & USC_RGB_CHAN_MASK)
			(*pbRGB) = IMG_TRUE;
		else
			(*pbRGB) = IMG_FALSE;
	}

	if (pbAlpha != NULL)
	{
		if (uChanMask & USC_ALPHA_CHAN_MASK)
			(*pbAlpha) = IMG_TRUE;
		else
			(*pbAlpha) = IMG_FALSE;
	}
}

static
IMG_BOOL FixTestC10ChanSel(PINTERMEDIATE_STATE		psState,
						   PINST					psReader,
						   IMG_UINT32				uToFoldMask,
						   IMG_BOOL					bCheckOnly)
/*****************************************************************************
 FUNCTION     : FixTestC10ChanSel
    
 PURPOSE      : Try to fix a C10 TEST instruction accessing the ALPHA channel from
			    a internal register source so it uses the BLUE channel from
				a unified store source.

 PARAMETERS   : psState			- Compiler state.
				psReader		- Instruction to fix.
				uToFoldMask		- Mask of sources to the instruction we are
								trying to change from internal registers to
								unified store registers.
				bCheckOnly		- If TRUE don't modify the instruction.

 RETURNS      : TRUE if the instruction was or could be updated to allow the fold.
*****************************************************************************/
{
	USEASM_TEST_CHANSEL	eChanSel;
	IMG_UINT32			uCtr;

	ASSERT(g_psInstDesc[psReader->eOpcode].eType == INST_TYPE_TEST);
	ASSERT(psReader->u.psTest->eAluOpcode == IFPADD8 || psReader->u.psTest->eAluOpcode == IFPSUB8);

	eChanSel = psReader->u.psTest->sTest.eChanSel;

	/*
		Check the predicate destination is set from only the ALPHA channel of the ALU
		result and there isn't a unified store destination.
	*/
	if (eChanSel != USEASM_TEST_CHANSEL_C3 || psReader->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		return IMG_FALSE;
	}
	
	/*
		Check for all the sources we aren't folding that they are immediate values so we can 
		change an ALPHA channel select to a BLUE channel select by updating the immediate value.
	*/
	for (uCtr = 0; uCtr < psReader->uArgumentCount; uCtr ++)
	{
		if ((uToFoldMask & (1U << uCtr)) == 0 && psReader->asArg[uCtr].uType != USEASM_REGTYPE_IMMEDIATE)
		{
			return IMG_FALSE;
		}
	}
	
	if (!bCheckOnly)
	{
		/*
			Update the instruction so the predicate destination is set from the BLUE channel
			of the ALU result.
		*/
		psReader->u.psTest->sTest.eChanSel = USEASM_TEST_CHANSEL_C0;

		SetBit(psReader->auFlag, INST_ALPHA, 1);

		/*
			Update the sources so the source for the BLUE channel is the same as the
			old source for the ALPHA channel.
		*/
		for (uCtr = 0; uCtr < psReader->uArgumentCount; uCtr ++)
		{
			if ((uToFoldMask & (1U << uCtr)) == 0)
			{
				PARG		psArg = &psReader->asArg[uCtr];
				IMG_UINT32	uCompData;

				ASSERT(psArg->uType == USEASM_REGTYPE_IMMEDIATE);
				ASSERT(psArg->eFmt == UF_REGFORMAT_U8);

				uCompData = (psArg->uNumber >> 24) & 0xFFU;
				psArg->uNumber &= ~0xFFU;
				psArg->uNumber |= (uCompData << 0);
			}
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL MergeRestoreOneC10Source(PINTERMEDIATE_STATE	psState, 
								  PINST					psReader, 
								  IMG_UINT32			uSrc, 
								  IMG_BOOL				bCheckOnly)
{
	IMG_UINT32	uReadMask;
	IMG_BOOL	bReadRGB, bReadAlpha;
	IOPCODE		eOpcode = psReader->eOpcode;

	/*
		Get the mask of channels used from this occurance of the register.
	*/
	uReadMask = GetLiveChansInArg(psState, psReader, uSrc);
	GetChanUse(uReadMask, &bReadRGB, &bReadAlpha);

	if (bReadRGB && bReadAlpha)
	{
		/*
			Source uses both RGB and alpha channel but may be a
			candidate for splitting.
		*/
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/* Non-maskable move from alpha to rgb */
	if (bReadAlpha &&
		(eOpcode == IMOV ||
		 eOpcode == IMOVC ||
		 eOpcode == IMOVC_C10))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}

	/* Complex op with alpha read */
	if (bReadAlpha &&
		(g_psInstDesc[eOpcode].uFlags & DESC_FLAGS_COMPLEXOP) != 0)
	{
		/* Component in source must be the same as the alpha register component */
		if (USC_BLUE_CHAN != GetComponentSelect(psState, psReader, uSrc))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	/*
		The TEST instruction doesn't support accessing the ALPHA channel from
		a unified store register.
	*/
	if ((g_psInstDesc[psReader->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0 && bReadAlpha)
	{
		if (!FixTestC10ChanSel(psState, psReader, 1U << uSrc, bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL FoldNodeRegWithSOPWM(PINTERMEDIATE_STATE	psState, 
							  PINST					psReader, 
							  IMG_UINT32			uSrcMask, 
							  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : FoldNodeRegWithSOPWM
    
 PURPOSE      : Try to fold a load into an internal register with a SOPWM
				using the internal register as a source by changing the SOPWM
				to another instruction.

 PARAMETERS   : psState		- Compiler state.
				psReader    - Instruction to try and fold.
				uSrcMask	- Mask of instruction sources which are the
							destination of the load.
				bCheckOnly	- If TRUE just check if the fold is possible.
							  If FALSE modify the instruction.

 RETURNS      : TRUE if a load was folded into the instruction.
*****************************************************************************/
{
	IMG_UINT32	uArgIdx;
	IMG_BOOL	bIsMove;
	IMG_BOOL	bMulAlphaSwizzle;
	IMG_BOOL	bConvertMoveToPck;
	IMG_UINT32	uArgToFold;
	IMG_UINT32	uLiveChansInArgToFold;

	/*
		Check the instruction has one of the forms we can merge
		the restore with.
	*/
	if (IsSOPWMMove(psReader))
	{
		bIsMove = IMG_TRUE;
		bMulAlphaSwizzle = IMG_FALSE;
	}
	else if (IsSOPWMMultiply(psReader, &bMulAlphaSwizzle))
	{
		bIsMove = IMG_FALSE;
	}
	else
	{
		return IMG_FALSE;
	}

	/*
		Check the instruction doesn't require a destination mask.
	*/
	bConvertMoveToPck = IMG_FALSE;
	if ((psReader->auLiveChansInDest[0] & ~psReader->auDestMask[0]) != 0)
	{
		if (bIsMove && g_auSetBitCount[psReader->auDestMask[0]] == 2 && psReader->asDest[0].eFmt == UF_REGFORMAT_C10)
		{
			bConvertMoveToPck = IMG_TRUE;
		}
		else
		{
			return IMG_FALSE;
		}
	}

	/*
		Check which arguments to the instruction come from the data to
		be restored.
	*/
	uArgToFold = USC_UNDEF;
	for (uArgIdx = 0; uArgIdx < psReader->uArgumentCount; uArgIdx++)
	{
		if ((uSrcMask & (1U << uArgIdx)) != 0)
		{
			uArgToFold = uArgIdx;
			break;
		}
	}
	ASSERT(uArgToFold != USC_UNDEF);

	/*
		Check this node only occurs once as a source.
	*/
	if ((uSrcMask >> (uArgToFold + 1)) != 0)
	{
		return IMG_FALSE;
	}

	/*
		Check if the argument which didn't match can be
		moved to source 0.
	*/
	if (!bIsMove && !CanUseSource0(psState, psReader->psBlock->psOwner->psFunc, &psReader->asArg[1 - uArgToFold]))
	{	
		return IMG_FALSE;
	}

	/*
		If we aren't referencing both the RGB channels and the alpha channel from this source then leave it
		to the simple merge case.
	*/
	uLiveChansInArgToFold = GetLiveChansInArg(psState, psReader, uArgToFold);
	if ((uLiveChansInArgToFold & USC_RGB_CHAN_MASK) == 0 || (uLiveChansInArgToFold & USC_ALPHA_CHAN_MASK) == 0)
	{
		return IMG_FALSE;
	}

	if (bCheckOnly)
	{
		return IMG_TRUE;
	}

	if (bIsMove)
	{
		ASSERT(uArgToFold == 0);

		SetSrcFromArg(psState, psReader, 1 /* uSrcIdx */, &psReader->asArg[0]);

		if (bConvertMoveToPck)
		{
			IMG_UINT32	uFirstChanWritten;

			uFirstChanWritten = (IMG_UINT32)g_aiSingleComponent[psReader->auDestMask[0] & USC_RGB_CHAN_MASK];
			ASSERT(((1U << uFirstChanWritten) | USC_ALPHA_CHAN_MASK) == psReader->auDestMask[0]);

			SetOpcode(psState, psReader, IPCKC10C10);
			SetPCKComponent(psState, psReader, 0 /* uSrc */, uFirstChanWritten);
			SetPCKComponent(psState, psReader, 1 /* uSrc */, USC_ALPHA_CHAN);
		}
		else
		{
			SetOpcode(psState, psReader, ISOP2);

			psReader->auDestMask[0] = USC_ALL_CHAN_MASK;

			/*
				DESTRGB = SRC1 * ONE + SRC2 * ZERO
			*/
			psReader->u.psSop2->uCSel1 = USEASM_INTSRCSEL_ZERO;
			psReader->u.psSop2->bComplementCSel1 = IMG_TRUE;
			psReader->u.psSop2->uCSel2 = USEASM_INTSRCSEL_ZERO;
			psReader->u.psSop2->bComplementCSel2 = IMG_FALSE;
	
			/*
				DESTA = SRC1 * ZERO + SRC2 * ONE
			*/		
			psReader->u.psSop2->uASel1 = USEASM_INTSRCSEL_ZERO;
			psReader->u.psSop2->bComplementASel1 = IMG_FALSE;
			psReader->u.psSop2->uASel2 = USEASM_INTSRCSEL_ZERO;
			psReader->u.psSop2->bComplementASel2 = IMG_TRUE;

			psReader->u.psSop2->uCOp = USEASM_INTSRCSEL_ADD;
			psReader->u.psSop2->uAOp = USEASM_INTSRCSEL_ADD;
	
			psReader->u.psSop2->bComplementCSrc1 = IMG_FALSE;
			psReader->u.psSop2->bComplementASrc1 = IMG_FALSE;
		}
	}
	else
	{
		/*
			ICOMBC10	TEMP, RGB, ALPHA
			ISOPWM		DEST, TEMP, OTHER

			->

			ISOP3		DEST, OTHER, RGB, ALPHA
		*/
		SetOpcode(psState, psReader, ISOP3);
		if (uArgToFold != 1)
		{
			SwapInstSources(psState, psReader, 0 /* uSrc1Idx */, 1 /* uSrc2Idx */);
		}	
		SetSrcFromArg(psState, psReader, 2 /* uSrcIdx */, &psReader->asArg[1]);

		psReader->auDestMask[0] = USC_ALL_CHAN_MASK;
			
		/*
			DESTRGB = SRC0 * SRC1 + ZERO * SRC2
		*/
		psReader->u.psSop3->uCOp = USEASM_INTSRCSEL_ADD;
		if (bMulAlphaSwizzle)
		{
			psReader->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0ALPHA;
		}
		else
		{
			psReader->u.psSop3->uCSel1 = USEASM_INTSRCSEL_SRC0;
		}
		psReader->u.psSop3->bComplementCSel1 = IMG_FALSE;
		psReader->u.psSop3->uCSel2 = USEASM_INTSRCSEL_ZERO;
		psReader->u.psSop3->bComplementCSel2 = IMG_FALSE;
		psReader->u.psSop3->bNegateCResult = IMG_FALSE;

		/*
			DESTA = (1 - SRC0) * ZERO + SRC0 * SRC2;
		*/
		psReader->u.psSop3->uAOp = USEASM_INTSRCSEL_ADD;
		psReader->u.psSop3->uCoissueOp = USEASM_OP_ALRP;
		psReader->u.psSop3->uASel1 = USEASM_INTSRCSEL_ZERO;
		psReader->u.psSop3->bComplementASel1 = IMG_FALSE;
		psReader->u.psSop3->uASel2 = USEASM_INTSRCSEL_SRC2ALPHA;
		psReader->u.psSop3->bComplementASel2 = IMG_FALSE;
		psReader->u.psSop3->bNegateAResult = IMG_FALSE;
	}

	return IMG_TRUE;
}

static
IMG_BOOL MergeRestoreC10Source(PINTERMEDIATE_STATE	psState, 
							   PIREGALLOC_CONTEXT	psContext,
							   PIREGALLOC_BLOCK		psBlockState,
							   PMERGE_CONTEXT		psMergeContext,
							   PINST				psReader, 
							   IMG_UINT32			uSrcMask, 
							   IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : MergeRestoreC10Source
    
 PURPOSE      : Check if a C10 format source can be switched from an internal
				register to a unified store register.

 PARAMETERS   : psState				- Compiler state.
				psContext			- Internal register allocator state.
				psBlockState		- Internal register allocator state for
									the current basic block.
				psMergeContext		- State for the current merge.
				psReader			- Instruction to merge into.
				uSrcMask			- Mask of sources to the instruction
									which are the intermediate register to
									merge.
				bCheckOnly			- If TRUE just check if the merge is possible.
									  If FALSE modify the instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSrc;
	IMG_BOOL	bMergePossible;

	PVR_UNREFERENCED_PARAMETER(psContext);
	PVR_UNREFERENCED_PARAMETER(psMergeContext);
	PVR_UNREFERENCED_PARAMETER(psBlockState);

	/*
		Always allow merging into SAVEIREG instruction which represent a pair of instructions
		used to save the contents of an internal register.
	*/
	if (psReader->eOpcode == ISAVEIREG)
	{
		return IMG_TRUE;
	}

	/*
		Check if the merge is possible by modifying the instruction to split a source from which
		all channels used into two sources where just the rgb channels are used from one source and
		the just alpha channel from the other.
	*/
	if (FoldNodeRegWithSOPWM(psState, psReader, uSrcMask, bCheckOnly))
	{
		return IMG_TRUE;
	}

	/*
		Convert
			MOV		DEST, SRC
		->
			COMBC10	DEST, SRC.RGB, SRC.ALPHA
	*/
	if (psReader->eOpcode == IMOV)
	{
		if (!bCheckOnly)
		{
			SetOpcode(psState, psReader, ICOMBC10_ALPHA);

			ASSERT(uSrcMask == 1);
			CopySrc(psState, psReader, 1 /* uCopyToSrcIdx */, psReader, 0 /* uCopyFromSrcIdx */);
		}
		return IMG_TRUE;
	}

	/*
		Check whether the merge is possible for each source individually.
	*/
	bMergePossible = IMG_TRUE;
	for (uSrc = 0; uSrc < psReader->uArgumentCount; uSrc++)
	{
		if ((uSrcMask & (1U << uSrc)) != 0)
		{
			if (!MergeRestoreOneC10Source(psState, psReader, uSrc, bCheckOnly))
			{
				bMergePossible = IMG_FALSE;
				break;
			}
		}
	}
	if (bMergePossible)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_VOID GenerateC10Save(PINTERMEDIATE_STATE	psState, 
						 PINST					psInsertBeforeInst, 
						 PINST					psPredSrcInst,
						 PARG					psDest, 
						 IMG_UINT32				uSaveChanMask,
						 PARG					psPartialDest,
						 PARG					psRGBSrc, 
						 PARG					psAlphaSrc,
						 IMG_UINT32				uAlphaComponent)
/*****************************************************************************
 FUNCTION     : GenerateC10Save
    
 PURPOSE      : Generate two instructions to copy data to the RGB channels
				and the alpha channel of an intermediate register.

 PARAMETERS   : psState				- Compiler state.
				psInsertBeforeInst	- Point to insert the instructions.
				psPredSrcInst		- Source for the predicate on the instructions.
				psDest				- Destination for the save.
				uSaveChanMask		- Mask of channels to save.
				psPartialDest		- Source for unsaved channels.
				psRGBSrc			- Source for the RGB channels.
				psAlphaSrc			- Source for the alpha channel.
				uAlphaComponent		- Component to select from the alpha source.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PINST	psFirstInst, psLastInst;

	psFirstInst = psLastInst = NULL;

	/*
		Create a SOPWM instruction to save the RGB channels.
	*/
	if ((uSaveChanMask & USC_RGB_CHAN_MASK) != 0)
	{
		PINST	psRGBSaveInst;

		psRGBSaveInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psRGBSaveInst, ISOPWM);

		psRGBSaveInst->auDestMask[0] = uSaveChanMask & USC_RGB_CHAN_MASK;

		CopyPredicate(psState, psRGBSaveInst, psPredSrcInst);

		SetSrcFromArg(psState, psRGBSaveInst, 0 /* uArg */, psRGBSrc);

		psRGBSaveInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psRGBSaveInst->asArg[1].uNumber = 0;
		psRGBSaveInst->asArg[1].eFmt = UF_REGFORMAT_U8;

		psRGBSaveInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psRGBSaveInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
		psRGBSaveInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		psRGBSaveInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
		psRGBSaveInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psRGBSaveInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psRGBSaveInst, psInsertBeforeInst);

		if (psFirstInst == NULL)
		{
			psFirstInst = psRGBSaveInst;
		}
		psLastInst = psRGBSaveInst;
	}

	/*	
		Create a PCKC10C10 instruction to save the alpha channel.
	*/
	if ((uSaveChanMask & USC_ALPHA_CHAN_MASK) != 0)
	{
		PINST	psAlphaSaveInst;

		psAlphaSaveInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psAlphaSaveInst, IPCKC10C10);

		psAlphaSaveInst->auDestMask[0] = uSaveChanMask & USC_ALPHA_CHAN_MASK;

		CopyPredicate(psState, psAlphaSaveInst, psPredSrcInst);
		
		SetSrcFromArg(psState, psAlphaSaveInst, 0 /* uSrc */, psAlphaSrc);
		SetPCKComponent(psState, psAlphaSaveInst, 0 /* uSrc */, uAlphaComponent);

		psAlphaSaveInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psAlphaSaveInst->asArg[1].uNumber = 0;
		psAlphaSaveInst->asArg[1].eFmt = UF_REGFORMAT_U8;

		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psAlphaSaveInst, psInsertBeforeInst);

		if (psFirstInst == NULL)
		{
			psFirstInst = psAlphaSaveInst;
		}
		psLastInst = psAlphaSaveInst;
	}

	/*	
		If two instructions were generated then allocate an extra temporary to hold the channels written
		by the first instruction and make it the source for channels not written by the second instruction.
	*/
	SetPartiallyWrittenDest(psState, psFirstInst, 0 /* uDestIdx */, psPartialDest);

	SetDestFromArg(psState, psLastInst, 0 /* uDestIdx */, psDest);
	psLastInst->auLiveChansInDest[0] = uSaveChanMask;

	if (psFirstInst != psLastInst)
	{
		ARG		sMaskTemp;

		MakeNewTempArg(psState, UF_REGFORMAT_C10, &sMaskTemp);

		SetPartiallyWrittenDest(psState, psLastInst, 0 /* uDestIdx */, &sMaskTemp);

		SetDestFromArg(psState, psFirstInst, 0 /* uDestIdx */, &sMaskTemp);
		psFirstInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psLastInst, 0 /* uDestIdx */);
	}
}

static
IMG_VOID ExpandIRegSaveC10(PINTERMEDIATE_STATE psState, PINST psSaveInst)
/*****************************************************************************
 FUNCTION     : ExpandIRegSaveC10
    
 PURPOSE      : Expand the generic SAVEIREG instruction to a sequence of
			    intermediate instructions which save a C10 format internal
				register.

 PARAMETERS   : psState			- Compiler state.
				psSaveInst		- SAVEIREG instruction to expand.

 RETURNS      : Nothing.
*****************************************************************************/
{
	ARG		sSaveDest;

	/*
		Capture the destination to the SAVEIREG instruction.
	*/
	sSaveDest = psSaveInst->asDest[0];

	/*
		Clear the SAVEIREG instruction's destination so we don't
		think there are two defines of the save destination.
	*/
	SetDestUnused(psState, psSaveInst, 0 /* uDestIdx */);

	/*
		Generate the expanded sequence of instructions.
	*/
	GenerateC10Save(psState,
					psSaveInst,
					psSaveInst,
					&sSaveDest,
					psSaveInst->auDestMask[0] & psSaveInst->auLiveChansInDest[0],
					psSaveInst->apsOldDest[0],
					&psSaveInst->asArg[0],
					&psSaveInst->asArg[0],
					USC_ALPHA_CHAN);

	/*
		Remove the SAVEIREG instruction from the block and
		free it.
	*/
	RemoveInst(psState, psSaveInst->psBlock, psSaveInst);
	FreeInst(psState, psSaveInst);
}

static
IMG_VOID ExpandIRegRestoreC10(PINTERMEDIATE_STATE psState, PINST psSaveInst)
/*****************************************************************************
 FUNCTION     : ExpandIRegRestoreC10
    
 PURPOSE      : Expand the generic RESTOREIREG instruction to a sequence of
			    intermediate instructions which restore a C10 format internal
				register.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psSaveInst		- RESTOREIREG instruction to expand.

 RETURNS      : Nothing.
*****************************************************************************/
{
	/*
		IREGSAVE	DEST, SRC
			->
		COMBC10		DEST, SRC, SRC
	*/
	if ((psSaveInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) != 0)
	{
		SetOpcode(psState, psSaveInst, ICOMBC10_ALPHA);
		CopySrc(psState, psSaveInst, 1 /* uCopyToSrcIdx */, psSaveInst, 0 /* uCopyFromSrcIdx */);
	}
	else
	{
		SetOpcode(psState, psSaveInst, ICOMBC10);
		SetSrc(psState, psSaveInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_C10);
	}
}

static
IMG_VOID MergeC10MoveDestination(PINTERMEDIATE_STATE psState, PINST psWriter)
/*****************************************************************************
 FUNCTION     : MergeC10MoveDestination
    
 PURPOSE      : For a MOV, RESTOREIREG or COMBC10 instruction writing an internal register
			    split it into two instructions copying just the RGB channels
				and just the alpha channel.

 PARAMETERS   : psState			- Compiler state.
				psWriter		- Instruction to split.

 RETURNS      : Nothing.
*****************************************************************************/
{
	PARG		psRGBSrc, psAlphaSrc;
	IMG_UINT32	uAlphaComponent;
	ARG			sWriterDest;

	psRGBSrc = &psWriter->asArg[0];
	if (psWriter->eOpcode == IMOV || psWriter->eOpcode == IRESTOREIREG)
	{
		psAlphaSrc = &psWriter->asArg[0];
		uAlphaComponent = USC_ALPHA_CHAN;
	}
	else
	{
		ASSERT(psWriter->eOpcode == ICOMBC10);
		psAlphaSrc = &psWriter->asArg[1];
		uAlphaComponent = USC_RED_CHAN;
	}

	sWriterDest = psWriter->asDest[0];
	SetDestUnused(psState, psWriter, 0 /* uDestIdx */);

	GenerateC10Save(psState, 
					psWriter, 
					psWriter, 
					&sWriterDest, 
					psWriter->auLiveChansInDest[0], 
					psWriter->apsOldDest[0], 
					psRGBSrc, 
					psAlphaSrc,
					uAlphaComponent);

	RemoveInst(psState, psWriter->psBlock, psWriter);
	FreeInst(psState, psWriter);
}

static
IMG_VOID SetSeparateRGBAndAlphaDestinations(PINTERMEDIATE_STATE psState, 
											PINST				psRGBWriter, 
											PINST				psAlphaWriter)
/*****************************************************************************
 FUNCTION     : SetSeparateRGBAndAlphaDestinations
    
 PURPOSE      : When splitting a C10 instruction into separate RGB and alpha
				parts to allow merging of an internal register save set the
				destinations of the separated instructions.

 PARAMETERS   : psState			- Compiler state.
				psRGBWriter		- RGB part of the original instruction.
				psAlphaWriter	- Alpha part of the original instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	/*
		Set the alpha instruction's destination from the destination of the
		save instruction.
	*/
	MoveDest(psState, psAlphaWriter, 0 /* uMoveToDestIdx */, psRGBWriter, 0 /* uMoveFromDestIdx */);
	psAlphaWriter->auDestMask[0] = USC_ALPHA_CHAN_MASK;
	InsertInstAfter(psState, psRGBWriter->psBlock, psAlphaWriter, psRGBWriter);

	if ((psRGBWriter->auLiveChansInDest[0] & USC_RGB_CHAN_MASK) == 0)
	{
		/*
			If only the alpha channel of the original instruction's result
			was unused then just drop the RGB instruction.
		*/
		RemoveInst(psState, psRGBWriter->psBlock, psRGBWriter);
		FreeInst(psState, psRGBWriter);
	}
	else
	{
		ARG		sMaskTemp;

		/*
			Create a new temporary register for the channels written by the
			RGB instruction.
		*/
		MakeNewTempArg(psState, UF_REGFORMAT_C10, &sMaskTemp);

		/*
			Set up the alpha instruction to copy the RGB channels from the RGB
			instruction's result to the its destination.
		*/
		SetPartiallyWrittenDest(psState, psAlphaWriter, 0 /* uDestIdx */, &sMaskTemp);
	
		SetDestFromArg(psState, psRGBWriter, 0 /* uDestIdx */, &sMaskTemp);
		psRGBWriter->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psAlphaWriter, 0 /* uDestIdx */);
		psRGBWriter->auDestMask[0] &= USC_RGB_CHAN_MASK;
	}
}

static
IMG_VOID MergeC10PCKDestination(PINTERMEDIATE_STATE psState, PINST psWriter)
/*********************************************************************************
 Function			: MergeC10PCKDestination

 Description		: For a PCKC10C10/PCKC10F16/PCKC10F32 instruction writing an internal
					  register: merge a save of the internal register
					  into a unified store register so the instruction writes
					  the unified store register directly.

 Parameters			: psState			- Compiler state.
					  psWriter			- Instruction to merge with.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uAlphaChanSrc;
	PINST		psAlphaChanWriter;
	IMG_UINT32	uChan;
	ARG			sMaskTemp;
	
	/*
		Find the source to the original PCK instruction which is copied/converted into the ALPHA channel of the
		destination.
	*/
	uAlphaChanSrc = 0;
	for (uChan = 0; uChan < USC_ALPHA_CHAN; uChan++)
	{
		if ((psWriter->auDestMask[0] & (1U << uChan)) != 0)
		{
			uAlphaChanSrc = (uAlphaChanSrc + 1) % PCK_SOURCE_COUNT;
		}
	}

	/*
		Create a new PCK instruction to copy/convert just the ALPHA channel.
	*/
	psAlphaChanWriter = AllocateInst(psState, psWriter);

	/*
		Copy the opcode and scale flag from the original instruction.
	*/
	SetOpcode(psState, psAlphaChanWriter, psWriter->eOpcode);
	psAlphaChanWriter->u.psPck->bScale = psWriter->u.psPck->bScale;

	/*
		Set the new PCK's destination to the destination of the save instruction.
	*/
	MoveDest(psState, psAlphaChanWriter, 0 /* uMoveToDestIdx */, psWriter, 0 /* uMoveFromDestIdx */);
	psAlphaChanWriter->auDestMask[0] = USC_ALPHA_CHAN_MASK;
	psAlphaChanWriter->auLiveChansInDest[0] = psWriter->auLiveChansInDest[0];

	/*
		Copy the predicate from the original instruction.
	*/
	CopyPredicate(psState, psAlphaChanWriter, psWriter);

	/*
		Copy the first source to the new PCK instruction from the source to the original instruction which
		was copied/converted into the ALPHA channel of the destination.
	*/
	CopySrc(psState, psAlphaChanWriter, 0 /* uCopyToSrcIdx */, psWriter, uAlphaChanSrc /* uCopyFromSrcIdx */);
	SetPCKComponent(psState, psAlphaChanWriter, 0 /* uSrcIdx */, GetPCKComponent(psState, psWriter, uAlphaChanSrc));

	/*
		Set the second (unreferenced) source to the new PCK instruction to a safe default.
	*/
	SetSrc(psState, psAlphaChanWriter, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_U8);

	/*
		Insert the new PCK instruction after the original.
	*/
	InsertInstAfter(psState, psWriter->psBlock, psAlphaChanWriter, psWriter);

	/*
		If the original PCK instruction was only writing the ALPHA channel then remove the original instruction.
	*/
	if ((psWriter->auDestMask[0] & USC_RGB_CHAN_MASK) == 0)
	{
		CopyPartiallyWrittenDest(psState, psAlphaChanWriter, 0 /* uMoveToDestIdx */, psWriter, 0 /* uMoveFromDestIdx */);
		RemoveInst(psState, psWriter->psBlock, psWriter);
		FreeInst(psState, psWriter);
		return;
	}

	/*
		Allocate a new temporary for just the RGB channels.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_C10, &sMaskTemp);

	/*
		Set the new temporary as the destination of the original PCK instruction.
	*/
	SetDestFromArg(psState, psWriter, 0 /* uDestIdx */, &sMaskTemp);
	psWriter->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psAlphaChanWriter, 0 /* uDestIdx */);
	psWriter->auLiveChansInDest[0] &= USC_RGB_CHAN_MASK;

	/*
		A PCK with a C10 format destination must write 1, 2 or 4 channels so check if we can update the original PCK
		instruction's destination mask to exclude the ALPHA channel.
	*/
	if (psWriter->auDestMask[0] != USC_ALL_CHAN_MASK)
	{
		ASSERT(g_auSetBitCount[psWriter->auDestMask[0]] == 2);
		psWriter->auDestMask[0] &= ~USC_ALPHA_CHAN_MASK;
		ASSERT(g_aiSingleComponent[psWriter->auDestMask[0]] < USC_ALPHA_CHAN);
	}

	/*
		Set the new PCK instruction to copy the RGB channels written by the original PCK instruction into the
		save instruction's destination.
	*/
	SetPartiallyWrittenDest(psState, psAlphaChanWriter, 0 /* uDestIdx */, &sMaskTemp);
}

static
IMG_BOOL CanMoveSOP2AlphaCalculationToSOPWM(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION     : MoveSOP2AlphaCalculationToSOPWM
    
 PURPOSE      : Check if the ALPHA channel part of a SOP2 operation could be
				implemented using the SOPWM instruction.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	PSOP2_PARAMS	psSop2;

	ASSERT(psInst->eOpcode == ISOP2);
	psSop2 = psInst->u.psSop2;

	/*
		Check for source selector options which aren't supported by the SOPWM instruction.
	*/
	if (psSop2->uASel1 == USEASM_INTSRCSEL_SRC2SCALE ||
		psSop2->uASel1 == USEASM_INTSRCSEL_SCALE ||
		psSop2->uASel2 == USEASM_INTSRCSEL_ZEROS ||
		psSop2->bComplementASrc1)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL CanMoveSOP3AlphaCalculationToSOPWM(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION     : MoveSOP3AlphaCalculationToSOPWM
    
 PURPOSE      : Check if the ALPHA channel part of a SOP3 operation could be
				implemented using the SOPWM instruction.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	PSOP3_PARAMS	psSop3;
	IMG_BOOL		bUsesSource0;

	ASSERT(psInst->eOpcode == ISOP3);
	psSop3 = psInst->u.psSop3;

	/*
		Check for options not supported by SOPWM.
	*/
	if (!(psSop3->uCoissueOp == USEASM_OP_ASOP && !psSop3->bNegateAResult))
	{
		return IMG_FALSE;
	}

	/*
		Check if the alpha calculations uses source 0.
	*/
	bUsesSource0 = IMG_FALSE;
	if (GetLiveChansInSOP3Argument(psInst, 0 /* uArg */, USC_ALPHA_CHAN_MASK) != 0)
	{
		bUsesSource0 = IMG_TRUE;
	}

	/*
		If the source 0 is used then check if both of source 1 and source 2 are referenced
		as well. 
	*/
	if (bUsesSource0)
	{
		if (GetLiveChansInSOP3Argument(psInst, 1 /* uArg */, USC_ALPHA_CHAN_MASK) != 0 &&
			GetLiveChansInSOP3Argument(psInst, 2 /* uArg */, USC_ALPHA_CHAN_MASK) != 0)
		{
			/*
				All three sources are used.
			*/
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
USEASM_INTSRCSEL ReplaceSource0(USEASM_INTSRCSEL eSel, IMG_UINT32 uReplaceSrc)
/*****************************************************************************
 FUNCTION     : ReplaceSource0
    
 PURPOSE      : Replace a selector referencing to source 0 by a selector
			    referencing to another source.

 PARAMETERS   : eSel		- Selector to replace.
				uReplaceSrc	- Index of the replacement source.

 RETURNS      : The replacement selector.
*****************************************************************************/
{
	static const struct
	{
		USEASM_INTSRCSEL eSel;
		USEASM_INTSRCSEL eAlphaSel;
	} asSrcToSel[] = 
	{
		{USEASM_INTSRCSEL_SRC0, USEASM_INTSRCSEL_SRC0ALPHA},
		{USEASM_INTSRCSEL_SRC1, USEASM_INTSRCSEL_SRC1ALPHA},
		{USEASM_INTSRCSEL_SRC2, USEASM_INTSRCSEL_SRC2ALPHA},
	};

	switch (eSel)
	{
		case USEASM_INTSRCSEL_SRC0: return asSrcToSel[uReplaceSrc].eSel;
		case USEASM_INTSRCSEL_SRC0ALPHA: return asSrcToSel[uReplaceSrc].eAlphaSel;
		default: return eSel;
	}
}

static
PINST ConvertC10ALUToSOPWM(PINTERMEDIATE_STATE psState, PINST psWriterInst)
/*****************************************************************************
 FUNCTION     : ConvertC10ALUToSOPWM
    
 PURPOSE      : Create a SOPWM instruction which does the same calculation
				as an existing instruction for the alpha channel.

 PARAMETERS   : psState			- Compiler state.
				psWriterInst	- Instruction to convert.

 RETURNS      : The newly created SOPWM instruction.
*****************************************************************************/
{
	PINST			psSOPWMInst;
	PSOPWM_PARAMS	psSOPWM;

	psSOPWMInst = AllocateInst(psState, psWriterInst);

	SetOpcode(psState, psSOPWMInst, ISOPWM);

	CopyPredicate(psState, psSOPWMInst, psWriterInst);

	psSOPWM = psSOPWMInst->u.psSopWm;
	switch (psWriterInst->eOpcode)
	{
		case ISOP2:
		{
			PSOP2_PARAMS	psSop2 = psWriterInst->u.psSop2;

			ASSERT(!psSop2->bComplementASrc1);

			/*
				Convert the parameters for the SOP2 alpha calculation.
			*/
			psSOPWM->uSel1 = psSop2->uASel1;
			psSOPWM->bComplementSel1 = psSop2->bComplementASel1;
			psSOPWM->uSel2 = psSop2->uASel2;
			psSOPWM->bComplementSel2 = psSop2->bComplementASel2;
			psSOPWM->uCop = psSop2->uAOp;
			psSOPWM->uAop = psSop2->uAOp;

			/*
				Copy the SOP2 sources.
			*/
			CopySrc(psState, psSOPWMInst, 0 /* uMoveToSrcIdx */, psWriterInst, 0 /* uMoveFromSrcIdx */);
			CopySrc(psState, psSOPWMInst, 1 /* uMoveToSrcIdx */, psWriterInst, 1 /* uMoveFromSrcIdx */);
			break;
		}
		case ISOP3:
		{
			PSOP3_PARAMS	psSop3 = psWriterInst->u.psSop3;

			/*
				Convert the parameters for the SOP3 alpha calculation.
			*/
			psSOPWM->uSel1 = psSop3->uASel1;
			psSOPWM->bComplementSel1 = psSop3->bComplementASel1;
			psSOPWM->uSel2 = psSop3->uASel2;
			psSOPWM->bComplementSel2 = psSop3->bComplementASel2;
			psSOPWM->uCop = psSop3->uAOp;
			psSOPWM->uAop = psSop3->uAOp;

			/*
				Check if source 0 is used by the alpha part of the calculation.
			*/
			if (GetLiveChansInSOP3Argument(psWriterInst, 0 /* uArg */, USC_ALPHA_CHAN_MASK) != 0)
			{
				IMG_UINT32	uUnusedSrc;
				IMG_UINT32	uSrc;

				/*
					If source 0 is used then at least one of the other two sources must
					be unreferenced (we checked this condition in CanMoveSOP3AlphaCalculationToSOPWM).
				*/
				uUnusedSrc = USC_UNDEF;
				for (uSrc = 0; uSrc < 1; uSrc++)
				{
					if (GetLiveChansInSOP3Argument(psWriterInst, 1 + uSrc, USC_ALPHA_CHAN_MASK) == 0)
					{
						uUnusedSrc = uSrc;
						break;
					}
				}
				ASSERT(uUnusedSrc != USC_UNDEF);

				/*
					Copy the SOP3 instruction's source 0 to the unused source to the SOPWM instruction.
				*/
				CopySrc(psState, 
					    psSOPWMInst, 
						uUnusedSrc /* uMoveToSrcIdx */, 
						psWriterInst, 
						0 /* uMoveFromSrcIdx */); 
				/*
					Copy used source from the SOP3 instruction's source 1 and 2 to the SOPWM instruction.
				*/
				CopySrc(psState, 
						psSOPWMInst, 
						1 - uUnusedSrc /* uMoveToSrcIdx */, 
						psWriterInst, 
						1 + (1 - uUnusedSrc)); 

				/*
					Replace references to source 0 in source selectors.
				*/
				psSOPWM->uSel1 = ReplaceSource0((USEASM_INTSRCSEL)(psSOPWM->uSel1), 1 + uUnusedSrc);
				psSOPWM->uSel2 = ReplaceSource0((USEASM_INTSRCSEL)(psSOPWM->uSel2), 1 + uUnusedSrc);
			}
			else
			{
				/*
					Copy the SOP3 instruction's source 1 and source 2 to the SOPWM instruction.
				*/
				CopySrc(psState, psSOPWMInst, 0 /* uMoveToSrcIdx */, psWriterInst, 1 /* uMoveFromSrcIdx */);
				CopySrc(psState, psSOPWMInst, 1 /* uMoveToSrcIdx */, psWriterInst, 2 /* uMoveFromSrcIdx */);
			}
			break;
		}
		default: imgabort();
	}

	return psSOPWMInst;
}

static
IMG_BOOL MergeC10SOP2Destination(PINTERMEDIATE_STATE psState, PINST psInst, IMG_BOOL bCheckOnly)
/*********************************************************************************
 Function			: MergeC10SOP2Destination

 Description		: For a SOP2 instruction writing an internal
					  register: try and merge a save of the internal register
					  into a unified store register so the instruction writes
					  the unified store register directly.

 Parameters			: psState			- Compiler state.
					  psWriter			- Instruction to merge with.
					  bCheckOnly		- If TRUE just check if the merge is
										possible.

 Globals Effected	: None

 Return				: TRUE if merging is possible.
*********************************************************************************/
{
	ASSERT(psInst->eOpcode == ISOP2);

	/*
		Check if the ALPHA part of the calculation done by the instruction could be implemented using
		a SOPWM instruction.
	*/
	if (!CanMoveSOP2AlphaCalculationToSOPWM(psState, psInst))
	{
		return IMG_FALSE;
	}

	if (!bCheckOnly)
	{
		PINST	psSOPWMInst;

		/*
			Split the alpha calculation from the existing instruction into a separate SOPWM instruction.
		*/
		psSOPWMInst = ConvertC10ALUToSOPWM(psState, psInst);

		SetSeparateRGBAndAlphaDestinations(psState, psInst, psSOPWMInst);
	}

	return IMG_TRUE;
}

static
IMG_BOOL CanSwitchAlphaToRed(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PARG psArg)
/*****************************************************************************
 FUNCTION     : CanSwitchAlphaToRed
    
 PURPOSE      : Check for a source argument which uses the hardware's special
				mode where reading the alpha channel actually reads the red channel.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator.
				psArg			- Argument to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	if (IsIRegArgument(psState, psContext, psArg, NULL /* puAssignedIReg */) || 
		psArg->uType != USEASM_REGTYPE_TEMP || 
		psArg->eFmt != UF_REGFORMAT_C10)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL MergeC10SOP3Destination(PINTERMEDIATE_STATE	psState, 
								 PIREGALLOC_CONTEXT		psContext, 
								 PINST					psInst, 
								 IMG_BOOL				bCheckOnly)
/*********************************************************************************
 Function			: MergeC10SOP3Destination

 Description		: For a SOP3 instruction writing an internal
					  register: try and merge a save of the internal register
					  into a unified store register so the instruction writes
					  the unified store register directly.

 Parameters			: psState			- Compiler state.
					  psWriter			- Instruction to merge with.
					  bCheckOnly		- If TRUE just check if the merge is
										possible.

 Globals Effected	: None

 Return				: TRUE if merging is possible.
*********************************************************************************/
{
	ASSERT(psInst->eOpcode == ISOP3);

	/*
		Check if the ALPHA part of the calculation done by the instruction could be implemented using
		a SOPWM instruction.
	*/
	if (CanMoveSOP3AlphaCalculationToSOPWM(psState, psInst))
	{
		if (!bCheckOnly)
		{
			PINST	psSOPWMInst;

			/*
				Split the alpha calculation from the existing instruction into a separate SOPWM instruction.
			*/
			psSOPWMInst = ConvertC10ALUToSOPWM(psState, psInst);

			/*
				Set a new temporary as the destination of the original instruction then set up the new SOPWM
				instruction to do a masked update of the new temporary's ALPHA channel and write the save
				instruction's destination.
			*/
			SetSeparateRGBAndAlphaDestinations(psState, psInst, psSOPWMInst);
		}

		return IMG_TRUE;
	}

	/*
		Check if a SOP3 instruction could be generated which calculates the same result as this instruction calculates
		for the ALPHA channel but writes it into the RED channel.
	*/
	if (psInst->u.psSop3->uCoissueOp == USEASM_OP_ALRP)
	{
		/*
			When switching to the red channel check we will still be accessing the
			right data from the second two sources. 

			We don't need to check the first source because the instruction supports
			swizzling the alpha channel into the red channel.
		*/
		if (!CanSwitchAlphaToRed(psState, psContext, &psInst->asArg[1]) ||
			!CanSwitchAlphaToRed(psState, psContext, &psInst->asArg[2]))
		{
			return IMG_FALSE;
		}

		if (!bCheckOnly)
		{
			PINST	psSOP3AlphaInst;

			/*
				Create a copy of the current instruction which calculates just the ALPHA channel part of the result.
			*/
			psSOP3AlphaInst = CopyInst(psState, psInst);
			SetBit(psInst->auFlag, INST_ALPHA, 1);

			SetSeparateRGBAndAlphaDestinations(psState, psInst, psSOP3AlphaInst);
		}

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static
IMG_VOID MergeC10SimpleALUDestination(PINTERMEDIATE_STATE psState, PINST psWriter)
/*********************************************************************************
 Function			: MergeC10SimpleALUDestination

 Description		: For a SOPWM, FPDOT or FPMA instruction writing an internal
					  register: merge a save of the internal register
					  into a unified store register so the instruction writes
					  the unified store register directly.

 Parameters			: psState			- Compiler state.
					  psWriter			- Instruction to merge with.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINST	psAlphaWriter;

	/*
		To merge the instruction we need to generate a new instruction which calculates
		the same result as the original instruction wrote into the alpha channel of the
		destination but writes it into the red channel.

		FPDOT: The calculation generates one result which is broadcast to all channels
		SOPWM: The hardware has a special mode which redirects the ALU result's alpha
		channel into the red channel of the destination.
		FPMA: All the sources have an alpha replicate swizzle.
	*/

	psAlphaWriter = CopyInst(psState, psWriter);

	SetSeparateRGBAndAlphaDestinations(psState, psWriter, psAlphaWriter);
}

static
IMG_BOOL MergeSaveC10Destination(PINTERMEDIATE_STATE	psState,
								 PIREGALLOC_CONTEXT		psContext,
								 PIREGALLOC_BLOCK		psBlockState,
								 PMERGE_CONTEXT			psMergeContext,
								 PINST					psWriter,
								 IMG_UINT32				uDest,
								 IMG_UINT32				uSrcMask,
								 IMG_BOOL				bCheckOnly,
								 IMG_PUINT32			puMergeInstCount)
/*********************************************************************************
 Function			: MergeSaveC10Destination

 Description		: Try to merge a save from an intermediate register which will
					  be assigned to an internal register into a intermediate register
					  which will be assigned to a unified store register with the
					  instruction writing the first intermediate register.

 Parameters			: psState			- Compiler state.
					  psContext			- Register allocator state.
					  psBlockState		- State for the current basic block.
					  psMergeContext	- State for the current merge.
					  psWriter			- Instruction to try and merge with.
					  uDest				- Index of the destination which is the
										intermediate register to be saved.
					  uSrcMask			- Mask of source arguments to replace by
										temporary registers.
					  bCheckOnly		- If TRUE just check if the merge is possible.
										  If FALSE update the instruction to write
										  the destination of the save instruction
										  directly.
					  puMergeInstCount	
										- Returns the number of extra instructions
										generated by the merge.

 Globals Effected	: None

 Return				: TRUE if the save could be merged with the instruction.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psMergeContext);

	#if defined(SUPPORT_SGX530)		
	/*
		BRN21713: MOVC instruction conditionally moving C10 data must have an internal register destination.
	*/
	if ((psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21713) && (psWriter->eOpcode == IMOVC_C10))
	{
		ASSERT(bCheckOnly);
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX530) */

	/*
		If the instruction writes only 32-bits of data then a merge is trivial.
	*/
	if ((psWriter->auDestMask[uDest] & psWriter->auLiveChansInDest[uDest] & USC_ALPHA_CHAN_MASK) == 0)
	{
		/*
			Check any sources which also need to be merged.
		*/
		if (uSrcMask != 0)
		{
			if (!MergeRestoreC10Source(psState,
									   psContext,
									   psBlockState,
									   psMergeContext,
									   psWriter,
									   uSrcMask,
									   bCheckOnly))
			{
				return IMG_FALSE;
			}
		}

		if (!bCheckOnly)
		{
			if (psWriter->eOpcode == IRESTOREIREG)
			{
				ASSERT(uSrcMask == 0);

				SetOpcode(psState, psWriter, ISOPWM);
				psWriter->auDestMask[0] = psWriter->auLiveChansInDest[0];
				psWriter->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
				psWriter->u.psSopWm->bComplementSel1 = IMG_TRUE;
				psWriter->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
				psWriter->u.psSopWm->bComplementSel2 = IMG_FALSE;
				psWriter->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
				psWriter->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
				SetSrc(psState, psWriter, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_U8);
			}
		}
		return IMG_TRUE;
	}

	/*
		Count how many extra instructions are needed if we split this instruction.
	*/
	if (
			psWriter->eOpcode != IRESTOREIREG && 
			(psWriter->auDestMask[uDest] & psWriter->auLiveChansInDest[uDest] & USC_RGB_CHAN_MASK) != 0
		)
	{
		if (puMergeInstCount != NULL)
		{
			(*puMergeInstCount)++;
		}
	}

	switch (psWriter->eOpcode)
	{
		case IMOV:
		case ICOMBC10:
		case IRESTOREIREG:
		{
			ASSERT(uDest == 0);
			if (!bCheckOnly)
			{
				if (psWriter->eOpcode == IRESTOREIREG)
				{
					DropFromMergeList(psState, psContext, psWriter);
				}

				MergeC10MoveDestination(psState, psWriter);
			}
			return IMG_TRUE;
		}
		case IPCKC10C10:
		case IPCKC10F16:
		case IPCKC10F32:
		{
			ASSERT(uDest == 0);
			if (!bCheckOnly)
			{
				MergeC10PCKDestination(psState, psWriter);
			}
			return IMG_TRUE;
		}
		case ISOP2:
		{
			ASSERT(uDest == 0);
			if (uSrcMask != 0)
			{
				return IMG_FALSE;
			}
			return MergeC10SOP2Destination(psState, psWriter,  bCheckOnly);
		}
		case ISOP3:
		{
			ASSERT(uDest == 0);
			if (uSrcMask != 0)
			{
				return IMG_FALSE;
			}
			return MergeC10SOP3Destination(psState, psContext, psWriter, bCheckOnly);
		}
		case ISOPWM:
		case IFPDOT:
		case IFPMA:
		{
			ASSERT(uDest == 0);
			if (uSrcMask != 0)
			{
				return IMG_FALSE;
			}
			if (!bCheckOnly)
			{
				MergeC10SimpleALUDestination(psState, psWriter);
			}
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_BOOL SplitIRegDestC10(PINTERMEDIATE_STATE	psState,
						  PIREGALLOC_CONTEXT	psContext,
						  PIREGALLOC_BLOCK		psBlockState,
						  PINST					psInst,
						  IMG_UINT32			uSrcMask)
/*****************************************************************************
 FUNCTION     : SplitIRegDestC10
    
 PURPOSE      : Try to split a C10 instruction into multiple instructions to
			    allow internal register sources to be replaced by unified
				store register sources.

 PARAMETERS   : psState			- Compiler state.
				psContext		- Internal register allocator context.
				psBlockState	- Internal register allocator state for the
								current basic block.
				psInst			- Instruction to try and split.
				uSrcMask		- Mask of sources which are currently 
								internal registers.

 RETURNS      : TRUE if the split succeeded.
*****************************************************************************/
{
	PINST			psAlphaInst;
	ARG				sMaskTemp;
	PUSEDEF_CHAIN	psMaskTempUseDef;
	PINTERVAL		psMaskTempInterval;

	/*
		Only split instructions which can support a destination write mask (since both of the split
		instructions are writing different parts of the same internal register).
	*/
	if (psInst->eOpcode != ISOPWM)
	{
		return IMG_FALSE;
	}

	/*
		Allocate a new temporary register for the partial result of the first split instruction.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_C10, &sMaskTemp);

	/*
		Create a new instruction to write the alpha channel.
	*/
	psAlphaInst = CopyInst(psState, psInst);
	InsertInstAfter(psState, psInst->psBlock, psAlphaInst, psInst);

	MoveDest(psState, psAlphaInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);
	SetPartiallyWrittenDest(psState, psAlphaInst, 0 /* uDestIdx */, &sMaskTemp);
	psAlphaInst->auDestMask[0] = USC_ALPHA_CHAN_MASK;
	psAlphaInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

	SetDestFromArg(psState, psInst, 0 /* uDestIdx */, &sMaskTemp);
	psInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psAlphaInst, 0 /* uDestIdx */);

	/*
		Mark the new temporary register as needing to be assigned an internal register.
	*/
	psMaskTempUseDef = UseDefGet(psState, sMaskTemp.uType, sMaskTemp.uNumber);

	psMaskTempInterval = AddNewBlockInterval(psState, psContext, psBlockState, psMaskTempUseDef, psMaskTempUseDef);
	AddToUsedIntervalList(psState, psBlockState, psMaskTempInterval);
	psMaskTempInterval->bUsedAsPartialDest = IMG_TRUE;

	/*
		Replace the internal registers sources of both instructions by unified store registers.
	*/
	MergeIRegSources(psState, psInst, uSrcMask);
	MergeIRegSources(psState, psAlphaInst, uSrcMask);

	return IMG_TRUE;
}


static
IMG_BOOL MergeCarryRestore(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcMask)
/*****************************************************************************
 FUNCTION     : MergeCarryRestore
    
 PURPOSE      : Check if internal register sources to an instruction
				could be replaced by non-internal registesr.

 PARAMETERS   : psState			- Compiler state.
				psDefInst		- Instruction to check.
				uSrcMask		- Mask of sources to check.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uSrc;

	for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
	{
		if ((uSrcMask & (1U << uSrc)) != 0)
		{
			if (!CanUseSrc(psState, psInst, uSrc, USEASM_REGTYPE_TEMP, USC_REGTYPE_NOINDEX))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL MergeCarrySave(PINTERMEDIATE_STATE psState, PINST psDefInst, IMG_UINT32 uDestIdx, IMG_UINT32 uSrcMask)
/*****************************************************************************
 FUNCTION     : MergeCarrySave
    
 PURPOSE      : Check if an internal register destination to an instruction
				could be replaced by a non-internal register.

 PARAMETERS   : psState			- Compiler state.
				psDefInst		- Instruction to check.
				uDestIdx		- Destination to check.
				uSrcMask		- Also check if this mask of sources could
								be replaced by non-internal registers.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	if (!CanUseDest(psState, psDefInst, uDestIdx, USEASM_REGTYPE_TEMP, USC_REGTYPE_NOINDEX))
	{
		return IMG_FALSE;
	}
	return MergeCarryRestore(psState, psDefInst, uSrcMask);
}

static
IMG_VOID ToMove(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION     : ToMove
    
 PURPOSE      : Convert an instruction to an IMOV.

 PARAMETERS   : psState			- Compiler state.
				psInst			- Instruction.

 RETURNS      : Nothing.
*****************************************************************************/
{
	SetOpcode(psState, psInst, IMOV);
}

static
IMG_BOOL MergeUsesInInst(PINTERMEDIATE_STATE	psState,
						 PIREGALLOC_CONTEXT		psContext,
						 PIREGALLOC_BLOCK		psBlockState,
						 PMERGE_CONTEXT			psMergeContext,
						 PINTERVAL				psInterval,
						 PINST					psInst,
						 IMG_UINT32				uSrcMask,
						 IMG_BOOL				bCheckOnly)
{
	if (psInterval->eType == INTERVAL_TYPE_GENERAL)
	{
		PFN_MERGE_RESTORE_SOURCE	pfMergeRestoreSource;

		pfMergeRestoreSource = psContext->pfMergeRestoreSource;
		ASSERT(pfMergeRestoreSource != NULL);

		if (!pfMergeRestoreSource(psState, 
								  psContext,
								  psBlockState,
								  psMergeContext,
								  psInst, 
								  uSrcMask,
								  bCheckOnly))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	else
	{
		ASSERT(psInterval->eType == INTERVAL_TYPE_CARRY ||psInterval->eType == INTERVAL_TYPE_EXISTING_IREG);

		if (!MergeCarryRestore(psState, psInst, uSrcMask))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static
IMG_BOOL MergeRestoreOnRange(PINTERMEDIATE_STATE	psState, 
							 PIREGALLOC_CONTEXT		psContext, 
							 PIREGALLOC_BLOCK		psBlockState,
							 PMERGE_CONTEXT			psMergeContext,
							 PINTERVAL				psInterval,
							 PINST					psRangeStartInst,
							 PINST					psRangeEndInst,
							 IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION     : MergeRestoreOnRange
    
 PURPOSE      : Try to replace uses of an internal register by a unified
				store register.

 PARAMETERS   : psState				- Compiler state.
				psContext			- Internal register allocator context.
				psBlockState		- State for the current basic block.
				psMergeContext		- Context for the current replacement.
				psInterval			- Interval to be merged.
				psRangeStartInst	- Inclusive range of instructions to replace
				psRangeEndInst		uses inside.
				bCheckOnly			- If TRUE just check if replacement is possible.
									If FALSE actually modify the instructions to
									do the replacement.

 RETURNS      : TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psLastInst;
	IMG_UINT32		uSrcMask;

	psLastInst = NULL;
	uSrcMask = 0;
	for (psListEntry = psInterval->psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;
		PINST	psUseInst;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psUseDef));
		psUseInst = psUseDef->u.psInst;

		if (psUseInst != psLastInst)
		{
			if (psLastInst != NULL && uSrcMask != 0)
			{
				/*
					Call the target/interval-specific function to merge references in the current instruction.
				*/
				if (!MergeUsesInInst(psState, 
									 psContext, 
									 psBlockState, 
									 psMergeContext, 
									 psInterval, 
									 psLastInst, 
									 uSrcMask,
									 bCheckOnly))
				{
					return IMG_FALSE;
				}
			}
			psLastInst = psUseInst;
			uSrcMask = 0;
		}

		/*
			Skip uses outside the range of instructions.
		*/
		if (psUseInst->uBlockIndex < psRangeStartInst->uBlockIndex)
		{
			continue;
		}
		if (psUseInst->uBlockIndex > psRangeEndInst->uBlockIndex)
		{
			break;
		}

		/*
			Ignore uses not as a source.
		*/
		if (psUseDef->eType == USE_TYPE_OLDDEST || psUseDef->eType == DEF_TYPE_INST) 
		{
			continue;
		}

		/*
			Make a mask of the sources to the current instruction which are the
			internal register.
		*/
		ASSERT(psUseDef->eType == USE_TYPE_SRC);
		ASSERT(psUseDef->uLocation < BITS_PER_UINT);
		ASSERT((uSrcMask & (1U << psUseDef->uLocation)) == 0);
		uSrcMask |= (1U << psUseDef->uLocation);
	}

	if (psLastInst != NULL && uSrcMask != 0)
	{
		/*
			Call the target/interval-specific function to merge references in the current instruction.
		*/
		if (!MergeUsesInInst(psState, psContext, psBlockState, psMergeContext, psInterval, psLastInst, uSrcMask, bCheckOnly))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID SubstVectorLoadSource(PINTERMEDIATE_STATE	psState, 
							   PUSEDEF_CHAIN		psDestUseDefChain, 
							   PINST				psRangeStartInst, 
							   PINST				psRangeEndInst, 
							   IMG_BOOL				bWideVector,
							   IMG_UINT32			uVectorLength,
							   PCARG				asVector)
/*****************************************************************************
 FUNCTION	: SubstVectorLoadSource
    
 PURPOSE	: Substitute references to an intermediate register by the
			  source to a VLOAD instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDestUseDefChain	- Intermediate register to replace.
			  psRangeStartInst	- Start and end of the inclusive range
			  psRangeEndInst	of instructions to substitute references in.
			  bWideVector		- VLOAD instruction source.
			  uVectorLength
			  asVector
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	for (psListEntry = psDestUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF			psUse;
		PINST			psUseInst;
		IMG_UINT32		uSlot;

		psNextListEntry = psListEntry->psNext;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == DEF_TYPE_INST)
		{
			continue;
		}
		ASSERT(psUse->eType == USE_TYPE_SRC);

		psUseInst = psUse->u.psInst;
	
		if (psUseInst->uBlockIndex < psRangeStartInst->uBlockIndex)
		{
			continue;
		}
		if (psUseInst->uBlockIndex > psRangeEndInst->uBlockIndex)
		{
			break;
		}

		ASSERT((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0);
	
		ASSERT((psUse->uLocation % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
		uSlot = psUse->uLocation / SOURCE_ARGUMENTS_PER_VECTOR;

		SetVectorSource(psState, psUseInst, uSlot, bWideVector, uVectorLength, asVector);
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID SubstIntermediateRegister(PINTERMEDIATE_STATE	psState, 
								   PUSEDEF_CHAIN		psDestUseDefChain, 
								   PINST				psRangeStartInst, 
								   PINST				psRangeEndInst, 
								   IMG_BOOL				bExcludeDef,
								   PARG					psSubstSrc)
/*****************************************************************************
 FUNCTION	: SubstIntermediateRegister
    
 PURPOSE	: Substitute references to one intermediate register by another.

 PARAMETERS	: psState			- Compiler state.
			  psDestUseDefChain	- Intermediate register to replace.
			  psRangeStartInst	- Start and end of the inclusive range
			  psRangeEndInst	of instructions to substitute references in.
			  bExcludeDef		- Don't modify the definition of the
								intermediate register.
			  psSubstSrc		- Intermediate register to replace by.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	for (psListEntry = psDestUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;
		PINST	psUseInst;

		psNextListEntry = psListEntry->psNext;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		
		ASSERT(UseDefIsInstUseDef(psUse));
		psUseInst = psUse->u.psInst;

		if (psUseInst->uBlockIndex < psRangeStartInst->uBlockIndex)
		{
			continue;
		}
		if (psUseInst->uBlockIndex > psRangeEndInst->uBlockIndex)
		{
			break;
		}

		if (psUse->eType == DEF_TYPE_INST && bExcludeDef)
		{
			continue;
		}
		
		UseDefSubstUse(psState, psDestUseDefChain, psUse, psSubstSrc);
	}
}

static
IMG_VOID DropInterval(PINTERMEDIATE_STATE	psState,
					  PIREGALLOC_CONTEXT	psContext,
					  PIREGALLOC_BLOCK		psBlockState, 
					  PINTERVAL				psIntervalToDrop)
/*****************************************************************************
 FUNCTION	: DropInterval
    
 PURPOSE	: Remove an interval from the list to be assigned an internal
			  register in the current block.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Pass state.
			  psBlockState		- State for the current basic block.
			  psIntervalToDrop	- Interval to remove.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psIntervalToDrop->eState != INTERVAL_STATE_UNUSED);

	switch (psIntervalToDrop->eState)
	{
		case INTERVAL_STATE_PENDING:
		{
			ASSERT(psIntervalToDrop->uHwReg == USC_UNDEF);
			RemoveFromList(&psBlockState->sUsedIntervalList, &psIntervalToDrop->u.sUsedIntervalListEntry);
			break;
		}
		case INTERVAL_STATE_ASSIGNED:
		{
			ASSERT(psIntervalToDrop->uHwReg != USC_UNDEF);
			SafeListRemoveItem(&psContext->asHwReg[psIntervalToDrop->uHwReg].sHwRegIntervalList,
							   &psIntervalToDrop->sHwRegIntervalListEntry);

			SafeListRemoveItem(&psBlockState->sAssignedIntervalList, &psIntervalToDrop->u.sAssignedIntervalListEntry);
			break;
		}
		case INTERVAL_STATE_CURRENT:
		{
			ASSERT(psIntervalToDrop->uHwReg == USC_UNDEF);
			break;
		}
		default: imgabort();
	}
	psIntervalToDrop->eState = INTERVAL_STATE_UNUSED;
}

typedef struct _RESTORE_USE
{
	USC_LIST_ENTRY	sListEntry;
	PINST			psInst;
	IMG_UINT32		uSrcMask;
	IMG_UINT32		uSrcCount;
	USC_LIST		sRestoreInstList;
} RESTORE_USE, *PRESTORE_USE;

static
IMG_VOID SplitIRegInstructions(PINTERMEDIATE_STATE	psState, 
							   PIREGALLOC_CONTEXT	psContext, 
							   PIREGALLOC_BLOCK		psBlockState,
							   PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: SplitIRegInstructions
    
 PURPOSE	: Look for instances where instructions can be split into multiple
			  instructions to allow internal register sources to be replaced
			  by unified store register sources.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psBlock			- Current basic block.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY			psListEntry;
	USC_LIST				sRestoreUseList;
	static const IOPCODE	aeRestoreOps[] =
	{
		IRESTOREIREG,
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IVLOAD,
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	IMG_UINT32		uOpType;

	/*
		For the all the instructions copying unified store registers into internal registers:-
	*/
	InitializeList(&sRestoreUseList);
	for (uOpType = 0; uOpType < (sizeof(aeRestoreOps) / sizeof(aeRestoreOps[0])); uOpType++)
	{
		IOPCODE				eRestoreOp = aeRestoreOps[uOpType];
		SAFE_LIST_ITERATOR	sIterator;

		InstListIteratorInitialize(psState, eRestoreOp, &sIterator);
		for (; InstListIteratorContinue(&sIterator); InstListIteratorNext(&sIterator))
		{	
			PINST			psRestoreInst;
			PARG			psRestoreDest;
			PINST			psUseInst;
			USEDEF_TYPE		eUseType;
			IMG_UINT32		uUseSrcIdx;
			PRESTORE_USE	psRestoreUse;
			PUSC_LIST_ENTRY	psUseListEntry;
			IMG_BOOL		bAlreadyPresent;
	
			psRestoreInst = InstListIteratorCurrent(&sIterator);
	
			if (psRestoreInst->psBlock != psBlock)
			{
				continue;
			}
		
			ASSERT(psRestoreInst->uDestCount == 1);
			psRestoreDest = &psRestoreInst->asDest[0];

			/*	
				Check the result of the copy is only used in the one instruction as a source.
			*/
			ASSERT(psRestoreDest->uType == USEASM_REGTYPE_TEMP);
			if (!UseDefGetSingleUse(psState, psRestoreDest, &psUseInst, &eUseType, &uUseSrcIdx))
			{
				continue;
			}
			if (eUseType != USE_TYPE_SRC)
			{
				continue;
			}

			/*
				Check if the instruction uses the internal register is suitable for splitting.
			*/
			if (psUseInst->uDestCount != 1 || !IsIRegArgument(psState, psContext, &psUseInst->asDest[0], NULL /* puAssignedIReg */))
			{
				continue;
			}

			/*
				Check if the instruction using the internal register also uses another internal register
				which is also a copy.
			*/
			psRestoreUse = NULL;
			bAlreadyPresent = IMG_FALSE;
			for (psUseListEntry = sRestoreUseList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
			{
				psRestoreUse = IMG_CONTAINING_RECORD(psUseListEntry, PRESTORE_USE, sListEntry);
				if (psRestoreUse->psInst == psUseInst)
				{
					bAlreadyPresent = IMG_TRUE;
					break;
				}
			}
			if (!bAlreadyPresent)
			{
				psRestoreUse = UscAlloc(psState, sizeof(*psRestoreUse));
				psRestoreUse->psInst = psUseInst;
				psRestoreUse->uSrcMask = 0;
				psRestoreUse->uSrcCount = 0;
				InitializeList(&psRestoreUse->sRestoreInstList);
				AppendToList(&sRestoreUseList, &psRestoreUse->sListEntry);
			}
			ASSERT(uUseSrcIdx < (LONG_SIZE * BITS_PER_BYTE));
			psRestoreUse->uSrcMask |= (1U << uUseSrcIdx);
			psRestoreUse->uSrcCount++;
			AppendToList(&psRestoreUse->sRestoreInstList, &psRestoreInst->sAvailableListEntry);
		}
		InstListIteratorFinalise(&sIterator);
	}

	/*
		Split instructions using more one internal register copy.
	*/
	while ((psListEntry = RemoveListHead(&sRestoreUseList)) != NULL)
	{
		PRESTORE_USE	psRestoreUse;

		psRestoreUse = IMG_CONTAINING_RECORD(psListEntry, PRESTORE_USE, sListEntry);
		if (psRestoreUse->uSrcCount > 1)
		{
			PUSC_LIST_ENTRY	psInstListEntry;
			IMG_BOOL		bRet;

			/*
				Try splitting the instruction.
			*/
			bRet = 
				psContext->pfSplitIRegDest(psState, psContext, psBlockState, psRestoreUse->psInst, psRestoreUse->uSrcMask);

			/*
				If splitting succeeded then remove the internal register restore instructions.
			*/
			if (bRet)
			{
				while ((psInstListEntry = RemoveListHead(&psRestoreUse->sRestoreInstList)) != NULL)
				{
					PINST		psRestoreInst;
					PINTERVAL	psRestoreInterval;

					psRestoreInst = IMG_CONTAINING_RECORD(psInstListEntry, PINST, sAvailableListEntry);
	
					psRestoreInterval = GetIntervalByArg(psContext, &psRestoreInst->asDest[0]);
					DropInterval(psState, psContext, psBlockState, psRestoreInterval);

					RemoveInst(psState, psRestoreInst->psBlock, psRestoreInst);
					FreeInst(psState, psRestoreInst);
				}
			}
		}
		UscFree(psState, psRestoreUse);
	}
}

static
PUSEDEF GetUseAsPartialDest(PINTERMEDIATE_STATE	psState,
							PUSEDEF_CHAIN		psUseDefChain,
							PINST				psRangeEndInst,
							IMG_PUINT32			puSrcMask,
							PINST*				ppsPartialDestUseInst)
/*****************************************************************************
 FUNCTION	: GetUseAsPartialDest
    
 PURPOSE	: Check if an intermediate register is used as a source for unwritten
			  channels in a destination. 
					
			  We already fixed all intermediate registers which might be assigned 
			  hardware internal registers so there is at most one such use and all
			  other uses are at the same or earlier instructions.

 PARAMETERS	: psState			- Compiler state.
			  psUseDefChain		- Intermediate register.
			  psRangeEndInst	- Don't check uses after this instruction.
			  puSrcMask			- If the intermediate register is used as a
								partial destination then returns the mask of
								sources to the same instruction where the
								intermediate register is also used.
			  psUseStartListEntry
								- Returns a pointer to the first USE structure
								in the list which is associated with the
								instruction where the intermediate register is
								used as a partial destination.
			  
 RETURNS	: A pointer to the USE structure for the use as a partial destination.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psLastUseInst;
	IMG_UINT32		uSrcMaskInLastInst;
	PUSEDEF			psPartialDestUse;

	uSrcMaskInLastInst = 0;
	psLastUseInst = NULL;
	psPartialDestUse = NULL;
	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psUse));

		if (psUse->u.psInst->uBlockIndex > psRangeEndInst->uBlockIndex)
		{
			break;
		}

		/*
			Check for a use in a different instruction to the last use.
		*/
		if (psUse->u.psInst != psLastUseInst)
		{
			ASSERT(psPartialDestUse == NULL);
			uSrcMaskInLastInst = 0;
		}
		psLastUseInst = psUse->u.psInst;
		
		if (psUse->eType == DEF_TYPE_INST)
		{
			/*
				Skip the definition of the intermediate register.
			*/
		}
		else if (psUse->eType == USE_TYPE_OLDDEST)
		{
			ASSERT(psPartialDestUse == NULL);
			psPartialDestUse = psUse;
		}
		else
		{
			/*
				Build up a mask of the sources in the current instruction
				where the intermediate register is used.
			*/
			ASSERT(psUse->eType == USE_TYPE_SRC);
			ASSERT(psUse->uLocation < BITS_PER_UINT);
			ASSERT((uSrcMaskInLastInst & (1U << psUse->uLocation)) == 0);

			uSrcMaskInLastInst |= (1U << psUse->uLocation);
		}
	}

	if (psPartialDestUse != NULL)
	{
		*puSrcMask = uSrcMaskInLastInst;
		*ppsPartialDestUseInst = psLastUseInst;
		return psPartialDestUse;
	}
	else
	{
		*puSrcMask = 0;
		*ppsPartialDestUseInst = NULL;
		return NULL;
	}
}

static
IMG_BOOL MergeIRegDefine(PINTERMEDIATE_STATE	psState,
						 PIREGALLOC_CONTEXT		psContext,
						 PIREGALLOC_BLOCK		psBlockState,
						 PMERGE_CONTEXT			psMergeContext,
						 PINTERVAL				psIRegInterval,
						 PINST					psDefInst,
						 IMG_UINT32				uDestIdx,
						 IMG_UINT32				uOldDestAsSrcMask,
						 IMG_BOOL				bCheckOnly,
						 IMG_PUINT32			puMergeExtraInsts)
/*****************************************************************************
 FUNCTION	: MergeIRegDefine
    
 PURPOSE	: Try to modify an instruction so its destination can be assigned
			  a hardware unified store register (rather than an internal register).

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psMergeContext	- State for the in-progress merge.
			  psIRegInterval	- Interval for the destination to be merged.
			  psDefInst			- Instruction to modify.
			  uDestIdx			- Destination to modify.
			  uOldDestAsSrcMask	- Mask of sources to the instruction which should
								also be replaced by unified store registers.
			  bCheckOnly		- If TRUE then just check if the modifications
								are possible.
								  If FALSE then apply the modifications.
			  puMergeExtraInsts	- Returns the number of extra instructions
								generated by the modifications.
			  
 RETURNS	: TRUE if merging is possible.
*****************************************************************************/
{
	if (psIRegInterval->eType == INTERVAL_TYPE_GENERAL)
	{
		PFN_MERGE_SAVE_DEST	pfMergeSaveDest;
	
		pfMergeSaveDest = psContext->pfMergeSaveDest;
		ASSERT(pfMergeSaveDest != NULL);

		if (!pfMergeSaveDest(psState, 
							 psContext, 
							 psBlockState,
							 psMergeContext,
							 psDefInst, 
							 uDestIdx, 
							 uOldDestAsSrcMask,
							 bCheckOnly, 
							 puMergeExtraInsts))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	else
	{
		ASSERT(psIRegInterval->eType == INTERVAL_TYPE_CARRY || psIRegInterval->eType == INTERVAL_TYPE_EXISTING_IREG);

		if (!MergeCarrySave(psState, psDefInst, uDestIdx, uOldDestAsSrcMask))
		{
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

typedef struct _INST_RANGE
{
	PCODEBLOCK	psBlock;
	PINST		psRangeStartPrev;
	PINST		psRangeStart;
	PINST		psRangeEndNext;
	PINST		psRangeEnd;
} INST_RANGE, *PINST_RANGE;

static 
IMG_VOID InstRangeInitialize(PINTERMEDIATE_STATE psState, PINST_RANGE psRange, PINST psRangeStart, PINST psRangeEnd)
/*****************************************************************************
 FUNCTION	: InstRangeInitialize
    
 PURPOSE	: Check if a reference to an intermediate register occurs in an
			  instruction in between two other instructions.

 PARAMETERS	: psState			- Compiler state.
			  psRange			- Range to initialize.
			  psRangeStart		- Inclusive start of the range.
			  psRangeEnd		- Inclusive end of the range.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psRange->psBlock = psRangeStart->psBlock;
	ASSERT(psRangeEnd->psBlock == psRange->psBlock);

	psRange->psRangeStart = psRangeStart;
	psRange->psRangeStartPrev = psRangeStart->psPrev;
	psRange->psRangeEnd = psRangeEnd;
	psRange->psRangeEndNext = psRangeEnd->psNext;
}

static
IMG_VOID InstRangeRefresh(PINST_RANGE psRange)
/*****************************************************************************
 FUNCTION	: InstRangeRefresh
    
 PURPOSE	: Update range information if the instructions at the start or the
			  end of the range have been freed.

 PARAMETERS	: psState			- Compiler state.
			  psRange			- Range to update.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psRange->psRangeStartPrev == NULL)
	{
		psRange->psRangeStart = psRange->psBlock->psBody;
	}
	else
	{
		psRange->psRangeStart = psRange->psRangeStartPrev->psNext;
	}
	if (psRange->psRangeEndNext == NULL)
	{
		psRange->psRangeEnd = psRange->psBlock->psBodyTail;
	}
	else
	{
		psRange->psRangeEnd = psRange->psRangeEndNext->psPrev;
	}
}

static
IMG_BOOL InstRangeContainsRef(PINTERMEDIATE_STATE psState, PINST_RANGE psRange, PUSEDEF psRef)
/*****************************************************************************
 FUNCTION	: InstRangeContainsRef
    
 PURPOSE	: Check if a reference to an intermediate register occurs in an
			  instruction in between two other instructions.

 PARAMETERS	: psState			- Compiler state.
			  psRange			- Range to check.
			  psRef				- Intermediate register reference to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psRefInst;

	ASSERT(UseDefIsInstUseDef(psRef));
	psRefInst = psRef->u.psInst;
	ASSERT(psRefInst->psBlock == psRange->psBlock);
	if (psRefInst->uBlockIndex >= psRange->psRangeStart->uBlockIndex && 
		psRefInst->uBlockIndex <= psRange->psRangeEnd->uBlockIndex)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_VOID SubstRestoreOrSave(PINTERMEDIATE_STATE	psState,
							PIREGALLOC_CONTEXT	psContext,
							PINTERVAL			psInterval,
							PINST_RANGE			psSubstRange)
/*****************************************************************************
 FUNCTION	: SubstRestoreOrSave
    
 PURPOSE	: If an instruction is either defined by a restore instruction or
			  used as a source to a save instruction then substitute the
			  restore source/save destination for all references to the
			  interval in a particular range of instructions.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator state.
			  psInterval		- Interval to substitute for.
			  psSubstRange		- Range of instructions to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psInterval->bDefinedByRestore)
	{
		PINST		psDefInst;
		IMG_UINT32	uDefDestIdx;

		ASSERT(psInterval->psSaveInst == NULL);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInterval->psOrigTemp == NULL)
		{
			SubstVectorLoadSource(psState, 
								  psInterval->psUseDefChain,
								  psSubstRange->psRangeStart,
								  psSubstRange->psRangeEnd,
								  psInterval->bOrigVectorWasWide,
								  psInterval->uOrigVectorLength, 
								  psInterval->asOrigVector);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			ARG				sRestoreSrc;

			MakeArg(psState,
					psInterval->psOrigTemp->uType,
					psInterval->psOrigTemp->uNumber,
					psInterval->psOrigTemp->eFmt,
					&sRestoreSrc);

			/*
				Replace all references in the range by another intermediate register.
			*/
			SubstIntermediateRegister(psState, 
									  psInterval->psUseDefChain, 
									  psSubstRange->psRangeStart, 
									  psSubstRange->psRangeEnd, 
									  IMG_TRUE /* bExcludeDef */, 
									  &sRestoreSrc);
		}

		/*
			Free the restore instruction.
		*/
		psDefInst = UseDefGetDefInstFromChain(psInterval->psUseDefChain, &uDefDestIdx);
		ASSERT(IsRestoreInst(psDefInst->eOpcode));
		ASSERT(uDefDestIdx == 0);

		DropFromMergeList(psState, psContext, psDefInst);

		RemoveInst(psState, psDefInst->psBlock, psDefInst);
		FreeInst(psState, psDefInst);
	}
	else if (psInterval->psSaveInst != NULL)
	{
		ARG		sSaveInstDest;
		PINST	psSaveInst = psInterval->psSaveInst;

		/*	
			Free the save instruction.
		*/
		sSaveInstDest = psSaveInst->asDest[0];
		DropFromMergeList(psState, psContext, psSaveInst);
		RemoveInst(psState, psSaveInst->psBlock, psSaveInst);
		FreeInst(psState, psSaveInst);
		psInterval->psSaveInst = NULL;
		InstRangeRefresh(psSubstRange);

		/*
			Replace the source of the save instruction by the destination.
		*/
		SubstIntermediateRegister(psState, 
								  psInterval->psUseDefChain, 
								  psSubstRange->psRangeStart,
								  psSubstRange->psRangeEnd,
								  IMG_FALSE /* bExcludeDef */,
								  &sSaveInstDest);
	}
}

static
IMG_VOID GetBaseInterval(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psBlock,
						 PUSEDEF_CHAIN			psIntervalUseDefChain,
						 PINST					psRangeStartInst,
						 PUSEDEF_CHAIN*			ppsBaseTemp,
						 PINST*					ppsBaseDefInst,
						 IMG_PUINT32			puBaseDefDestIdx)
/*****************************************************************************
 FUNCTION	: GetBaseInterval
    
 PURPOSE	: 

 PARAMETERS	: psState				- Compiler state.
			  psBlock				- Basic block we are currently assigned
									internal registers for.
			  psIntervalUseDefChain	- Intermediate register.
			  psRangeStartInst		- 
			  ppsBaseTemp			- Returns the first intermediate register
									which is linked through partial defines to
									the supplied intermediate register.
			  ppsBaseDefInst		- Returns the instruction where the
									first intermediate register is defined.
			  puBaseDefDestIdx		- Returns the instruction destination where
									the first intermediate register is defined.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psBaseTemp;
	PINST			psBaseDefInst;
	IMG_UINT32		uBaseDefDestIdx;

	/*
		Search backwards through partial defines of the interval until we find an unconditional
		define.
	*/
	psBaseTemp = psIntervalUseDefChain;
	for (;;)
	{
		PARG		psOldDest;

		psBaseDefInst = UseDefGetDefInstFromChain(psBaseTemp, &uBaseDefDestIdx);
		ASSERT(psBaseDefInst != NULL);
		ASSERT(psBaseDefInst->psBlock == psBlock);

		/*
			Check if the definition is outside the range of instructions.
		*/
		if (psBaseDefInst->uBlockIndex < psRangeStartInst->uBlockIndex)
		{
			ASSERT(psBaseTemp == psIntervalUseDefChain);

			/*	
				Don't try and modify the defining instruction.
			*/
			psBaseDefInst = NULL;
			uBaseDefDestIdx = USC_UNDEF;
			break;
		}

		psOldDest = psBaseDefInst->apsOldDest[uBaseDefDestIdx];
		if (psOldDest == NULL)
		{
			break;
		}
		ASSERT(psOldDest->uType == USEASM_REGTYPE_TEMP);

		psBaseTemp = psOldDest->psRegister->psUseDefChain;
	}

	*ppsBaseTemp = psBaseTemp;
	*ppsBaseDefInst = psBaseDefInst;
	*puBaseDefDestIdx = uBaseDefDestIdx;
}

static
IMG_BOOL BaseSubstIRegInterval(PINTERMEDIATE_STATE	psState, 
							   PIREGALLOC_CONTEXT	psContext, 
							   PIREGALLOC_BLOCK		psBlockState,
							   PUSEDEF_CHAIN		psIntervalUseDefChain,
							   PUSEDEF_CHAIN		psBaseTemp,
							   PINST				psBaseDefInst,
							   IMG_UINT32			uBaseDefDestIdx,
							   IMG_UINT32			uBaseDefSrcMask,
							   PINST				psRangeStartInst,
							   PINST				psRangeEndInst,
							   IMG_BOOL				bCheckOnly,
							   IMG_PINT32			piMergeExtraInsts,
							   IMG_PUINT32			puMergeExtraSecAttrs,
							   IMG_UINT32			uAlreadyUsedSecAttrs)
/*****************************************************************************
 FUNCTION	: BaseSubstIRegInterval
    
 PURPOSE	: Try to modify references to an interval in a particular range so
			  they can be assigned non-internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psIntervalUseDefChain
								- Intermediate register register to replace.
			  psBaseTemp		- First intermediate register which is linked to
								the specified intermediate register in a chain
								of partial defines.
			  psBaseDefInst		- Location of the definition of the first intermediate
			  uBaseDefDestIdx	register.
			  uBaseDefSrcMask	- Mask of sources to the definition of the first
								intermediate register which will also be replaced by
								non-internal registers.
			  psRangeStartInst
								- The start of the range of uses to replace.
			  psRangeEndInst	
								- The end of the range of uses of the intermediate register 
								to replace.
			  bCheckOnly		- If TRUE just check if the merge is possible.
								  If FALSE modify the instruction defining the
								  save source.
			  piMergeExtraInsts	- Returns the number of extra instructions 
								generated if the save instruction was merged.
			  puMergeExtraSecAttrs
								- Returns the number of extra secondary
								attributes required if the save instruction
								was merged.
			  uAlreadyUsedSecAttrs 
								- The number of secondary attributes that have already been
								used when saving/restoring this interval
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN		psCurrentTemp;
	PFN_MERGE_SAVE_DEST	pfMergeSaveDest;
	MERGE_CONTEXT		sMergeContext;
	PINST				psCurrentDefInst;
	IMG_UINT32			uCurrentDefDestIdx;
	IMG_UINT32			uPartialDestUsesAsSrcMask;
	USC_LIST			sUncleanIntervalList;
	INST_RANGE			sSubstRange;

	InstRangeInitialize(psState, &sSubstRange, psRangeStartInst, psRangeEndInst);

	pfMergeSaveDest = psContext->pfMergeSaveDest;
	ASSERT(pfMergeSaveDest != NULL);

	if (piMergeExtraInsts != NULL)
	{
		*piMergeExtraInsts = 0;
	}
	if (puMergeExtraSecAttrs != NULL)
	{
		*puMergeExtraSecAttrs = 0;
	}

	sMergeContext.uNumSecAttrsUsed = uAlreadyUsedSecAttrs;

	psCurrentTemp = psBaseTemp;
	psCurrentDefInst = psBaseDefInst;
	uCurrentDefDestIdx = uBaseDefDestIdx;
	uPartialDestUsesAsSrcMask = uBaseDefSrcMask;
	InitializeList(&sUncleanIntervalList);
	while (psCurrentTemp != NULL)
	{
		PINST			psPartialDestUseInst;
		PINTERVAL		psIRegInterval;
		PINST			psNextDefInst;
		IMG_UINT32		uNextDefDestIdx;
		IMG_UINT32		uNextPartialDestUsesAsSrcMask;
		PUSEDEF_CHAIN	psNextTemp;
		PINST			psUsesRangeEndInst;
		PUSEDEF			psOldDestUse;
		INST_RANGE		sSubstUsesRange;

		if (psCurrentTemp != psIntervalUseDefChain)
		{
			PUSEDEF	psFirstRef = IMG_CONTAINING_RECORD(psCurrentTemp->sList.psTail, PUSEDEF, sListEntry);
			PUSEDEF psLastRef = IMG_CONTAINING_RECORD(psCurrentTemp->sList.psHead, PUSEDEF, sListEntry);

			ASSERT(InstRangeContainsRef(psState, &sSubstRange, psFirstRef));
			ASSERT(InstRangeContainsRef(psState, &sSubstRange, psLastRef));
		}

		/*
			Check if this intermediate register is used in a partial define of another register.
		*/
		psOldDestUse = 
			GetUseAsPartialDest(psState, psCurrentTemp, psRangeEndInst, &uNextPartialDestUsesAsSrcMask, &psPartialDestUseInst);
		if (psOldDestUse == NULL)
		{
			ASSERT(uNextPartialDestUsesAsSrcMask == 0);
			ASSERT(psPartialDestUseInst == NULL);
			psUsesRangeEndInst = psRangeEndInst;

			psNextTemp = NULL;

			psNextDefInst = NULL;
			uNextDefDestIdx = USC_UNDEF;
		}
		else
		{
			PARG	psDest;

			ASSERT(psPartialDestUseInst != NULL);
			psUsesRangeEndInst = psPartialDestUseInst->psPrev;

			ASSERT(psOldDestUse->eType == USE_TYPE_OLDDEST);
			psNextDefInst = psOldDestUse->u.psInst;
			ASSERT(psOldDestUse->uLocation < psNextDefInst->uDestCount);
			uNextDefDestIdx = psOldDestUse->uLocation;

			/*
				Replace the intermediate register partially defined from the current register next.
			*/
			psDest = &psNextDefInst->asDest[psOldDestUse->uLocation];
			ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);
			psNextTemp = psDest->psRegister->psUseDefChain;
		}

		InstRangeInitialize(psState, &sSubstUsesRange, sSubstRange.psRangeStart, psUsesRangeEndInst);

		/*
			Get the interval for the current intermediate register.
		*/
		psIRegInterval = GetInterval(psContext, psCurrentTemp->uNumber);

		/*
			If the current interval is saved into a temporary register then record we can remove
			the save instruction if the interval is directly replaced by a temporary register.
		*/
		if (psIRegInterval->psSaveInst != NULL && psIRegInterval->psSaveInst->uBlockIndex <= psRangeEndInst->uBlockIndex)
		{
			PINST		psSaveInst = psIRegInterval->psSaveInst;
			IMG_UINT32	uExpandedSaveInstCount;

			if ((psSaveInst->eOpcode == ISAVEIREG))
			{
				uExpandedSaveInstCount = GetSaveInstCount(psContext, psSaveInst->auLiveChansInDest[0]);
			}
			else
			{
				ASSERT(psSaveInst->eOpcode == ISAVECARRY);
				uExpandedSaveInstCount = 1;
			}
			if (piMergeExtraInsts != NULL)
			{
				(*piMergeExtraInsts) -= (IMG_INT32)uExpandedSaveInstCount;
			}
		}

		/*
			Try to modify the instruction defining the current intermediate register so it
			can write to a non-internal register.
		*/
		if (psCurrentDefInst != NULL)
		{
			if (IsMergeableRestoreInst(psState, psIRegInterval, psCurrentDefInst))
			{
				ASSERT(uCurrentDefDestIdx == 0);
				ASSERT(uPartialDestUsesAsSrcMask == 0);
				if (piMergeExtraInsts != NULL)
				{
					(*piMergeExtraInsts) -= IREGALLOC_SPILL_RESTORE_COST;
				}
			}
			else
			{
				IMG_UINT32	uExtraInstsToMergeDef;

				uExtraInstsToMergeDef = 0;
				if (!MergeIRegDefine(psState, 
									 psContext, 
									 psBlockState,
									 &sMergeContext, 
									 psIRegInterval,
									 psCurrentDefInst, 
									 uCurrentDefDestIdx, 
									 uPartialDestUsesAsSrcMask, 
									 bCheckOnly, 
									 &uExtraInstsToMergeDef))
				{
					ASSERT(bCheckOnly);
					return IMG_FALSE;
				}
				if (piMergeExtraInsts != NULL)
				{
					(*piMergeExtraInsts) += (IMG_INT32)uExtraInstsToMergeDef;
				}
				if (!bCheckOnly)
				{
					psIRegInterval->bDefinedByRestore = IMG_FALSE;
				}
			}

			/*
				Update the start and end of the range of instructions (in case merging the define replaced some
				instructions).
			*/
			InstRangeRefresh(&sSubstRange);
			InstRangeRefresh(&sSubstUsesRange);
		}

		/*
			Try to modify uses of the current intermediate register so they can be non-internal
			registers. 
			
			Don't check any uses in an instruction which is also using the current intermediate register 
			as a source for a partial definition - they will be processed when we replace the intermediate
			register defined at that instruction.
		*/
		if (!MergeRestoreOnRange(psState,
								 psContext,
								 psBlockState,
								 &sMergeContext,
								 psIRegInterval,
								 sSubstUsesRange.psRangeStart,
								 sSubstUsesRange.psRangeEnd,
								 bCheckOnly))
		{	
			ASSERT(bCheckOnly);
			return IMG_FALSE;
		}	

		/*
			Update the start and end of the range of instructions (in case merging uses replaced some
			instructions).
		*/
		InstRangeRefresh(&sSubstRange);

		/*
			Drop the interval for the substituted interval.
		*/
		if (!bCheckOnly)
		{
			DropInterval(psState, psContext, psBlockState, psIRegInterval);
			AppendToList(&sUncleanIntervalList, &psIRegInterval->u.sUncleanIntervalListEntry);
		}

		psCurrentTemp = psNextTemp;
		uPartialDestUsesAsSrcMask = uNextPartialDestUsesAsSrcMask;
		psCurrentDefInst = psNextDefInst;
		uCurrentDefDestIdx = uNextDefDestIdx;
	}

	if (!bCheckOnly)
	{
		PUSC_LIST_ENTRY	psListEntry;

		while ((psListEntry = RemoveListHead(&sUncleanIntervalList)) != NULL)
		{
			PINTERVAL		psUncleanInterval;

			psUncleanInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, u.sUncleanIntervalListEntry);
			SubstRestoreOrSave(psState, psContext, psUncleanInterval, &sSubstRange);
			InstRangeRefresh(&sSubstRange);
		}
	}

	if (puMergeExtraSecAttrs != NULL)
	{
		*puMergeExtraSecAttrs = sMergeContext.uNumSecAttrsUsed;
	}
	
	return IMG_TRUE;
}

static
IMG_BOOL SubstIRegInterval(PINTERMEDIATE_STATE	psState, 
						   PIREGALLOC_CONTEXT	psContext, 
						   PIREGALLOC_BLOCK		psBlockState,
						   PCODEBLOCK			psBlock,
						   PUSEDEF_CHAIN		psIntervalUseDefChain, 
						   PINST				psRangeStartInst,
						   PINST				psRangeEndInst,
						   IMG_BOOL				bCheckOnly,
						   IMG_PINT32			piMergeExtraInsts,
						   IMG_PUINT32			puMergeExtraSecAttrs,
						   IMG_UINT32			uAlreadyUsedSecAttrs)
/*****************************************************************************
 FUNCTION	: SubstIRegInterval
    
 PURPOSE	: Try to modify references to an interval in a particular range so
			  they can be assigned non-internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psBlock			- Current basic block.
			  psIntervalUseDefChain
								- Intermediate register register to replace.
			  psRangeStartInst
								- The start of the range of uses to replace.
			  psRangeEndInst	
								- The end of the range of uses of the intermediate register 
								to replace.
			  bCheckOnly		- If TRUE just check if the merge is possible.
								  If FALSE modify the instruction defining the
								  save source.
			  piMergeExtraInsts	- Returns the number of extra instructions 
								generated if the save instruction was merged.
			  puMergeExtraSecAttrs
								- Returns the number of extra secondary
								attributes required if the save instruction
								was merged.
			  uAlreadyUsedSecAttrs 
								- The number of secondary attributes that have already been
								used when saving/restoring this interval
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN		psBaseTemp;
	PINST				psBaseDefInst;
	IMG_UINT32			uBaseDefDestIdx;

	/*
		Find the start of the chain of partially defined intermediate registers making up
		the interval.
	*/
	GetBaseInterval(psState, psBlock, psIntervalUseDefChain, psRangeStartInst, &psBaseTemp, &psBaseDefInst, &uBaseDefDestIdx);

	/*
		Work forwards from the start of the chain.
	*/
	return BaseSubstIRegInterval(psState,	
								 psContext,
								 psBlockState,
								 psIntervalUseDefChain,
								 psBaseTemp,
								 psBaseDefInst,
								 uBaseDefDestIdx,
								 0 /* uBaseDefSrcMask */,
								 psRangeStartInst,
								 psRangeEndInst,
								 bCheckOnly,
								 piMergeExtraInsts,
								 puMergeExtraSecAttrs,
								 uAlreadyUsedSecAttrs);
}

static
IMG_VOID SubstByUnifiedStore(PINTERMEDIATE_STATE	psState,
							 PIREGALLOC_CONTEXT		psContext,
							 PIREGALLOC_BLOCK		psBlockState,
							 PCODEBLOCK				psBlock,
							 PINTERVAL				psInterval)
/*****************************************************************************
 FUNCTION     : SubstByUnifiedStore
    
 PURPOSE      : For an interval assigned an internal register replace it by
			    a unified store register.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- Internal register allocator state for
									the current basic block.
				psBlock				- Current basic block.
				psInterval			- Interval to substitute.

 RETURNS      : Nothing.
*****************************************************************************/
{	
	IMG_BOOL	bRet;

	bRet = SubstIRegInterval(psState,
							 psContext,
							 psBlockState,
							 psBlock,
							 psInterval->psUseDefChain,
							 psBlock->psBody,
							 psBlock->psBodyTail,
							 IMG_FALSE /* bCheckOnly */,
							 NULL /* piNumMergeExtraInsts */,
							 NULL /* puNumExtraSecAttrs */,
							 0 /* uAlreadyUsedSecAttrs */);
	ASSERT(bRet);
}

static
IMG_BOOL CanSubstByUnifiedStore(PINTERMEDIATE_STATE	psState,
								PIREGALLOC_CONTEXT	psContext,
								PIREGALLOC_BLOCK	psBlockState,
								PCODEBLOCK			psBlock,
								PINTERVAL			psInterval)
/*****************************************************************************
 FUNCTION     : CanSubstByUnifiedStore
    
 PURPOSE      : For an interval assigned an internal register check if the
			    interval could also be assigned a unified store register.

 PARAMETERS   : psState				- Compiler state.	
				psContext			- Internal register allocator state.
				psBlockState		- Internal register allocator state for
									the current basic block.
				psBlock				- Current basic block.
				psInterval			- Interval to substitute.

 RETURNS      : TRUE if the interval can be replaced by a unified store register.
*****************************************************************************/
{
	IMG_INT32		iNumMergeExtraInsts;
	IMG_UINT32		uNumMergeExtraSecAttrs;

	ASSERT(psInterval->eState == INTERVAL_STATE_ASSIGNED);

	if (!SubstIRegInterval(psState,
						   psContext,
						   psBlockState,
						   psBlock,
						   psInterval->psUseDefChain,
						   psBlock->psBody,
						   psBlock->psBodyTail,
						   IMG_TRUE /* bCheckOnly */,
						   &iNumMergeExtraInsts,
						   &uNumMergeExtraSecAttrs,
						   0 /* uAlreadyUsedSecAttrs */))
	{
		return IMG_FALSE;
	}
	if (iNumMergeExtraInsts > 0 || uNumMergeExtraSecAttrs != 0)			
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_VOID TryMergeIRegSave(PINTERMEDIATE_STATE	psState, 
						  PIREGALLOC_CONTEXT	psContext, 
						  PIREGALLOC_BLOCK		psBlockState,
						  PCODEBLOCK			psBlock,
						  PINST					psSaveInst)
/*****************************************************************************
 FUNCTION	: TryMergeIRegSave
    
 PURPOSE	: For an instruction saving data from an internal register try
			  to replace the define of the save source by a define of the
			  save destination.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psBlock			- Current basic block.
			  psSaveInst		- Instruction to try and merge.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG			psSaveInstSrc;
	PUSEDEF_CHAIN	psSaveInstSrcUseDefChain;
	IMG_BOOL		bMergeable;
	IMG_INT32		iMergeExtraInsts;

	ASSERT(psSaveInst->eOpcode == ISAVEIREG || psSaveInst->eOpcode == ISAVECARRY);
	ASSERT(psSaveInst->psBlock == psBlock);
	ASSERT(psSaveInst->uDestCount == 1);
	ASSERT(psSaveInst->uArgumentCount == 1);

	psSaveInstSrc = &psSaveInst->asArg[0];
	ASSERT(psSaveInstSrc->uType == USEASM_REGTYPE_TEMP);	

	if (!IsIRegArgument(psState, psContext, psSaveInstSrc, NULL /* puAssignedIReg */))
	{
		return;
	}

	psSaveInstSrcUseDefChain = UseDefGet(psState, psSaveInstSrc->uType, psSaveInstSrc->uNumber);

	/*	
		Check if it's possible to replace the internal register source of the save instruction
		by the temporary register destination.
	*/
	bMergeable = SubstIRegInterval(psState,
								   psContext,
								   psBlockState,
								   psBlock,
								   psSaveInstSrcUseDefChain,
								   psBlock->psBody,
								   psBlock->psBodyTail,
								   IMG_TRUE /* bCheckOnly */,
								   &iMergeExtraInsts,
								   NULL /* puNumExtraSecAttrs */,
								   0 /* uAlreadyUsedSecAttrs */);
	if (!bMergeable)
	{
		return;
	}

	/*
		Check if merging the save instruction will generate more instructions than
		keeping it.
	*/
	if (iMergeExtraInsts > GetSaveInstCount(psContext, psSaveInst->auLiveChansInDest[0]))
	{
		return;
	}

	/*
		Prepare all references to the source of the save instruction they can be replaced
		by a non-internal register and then replace them by the destination of the save
		instruction.
	*/
	SubstIRegInterval(psState,
					  psContext,
					  psBlockState,
					  psBlock,
					  psSaveInstSrcUseDefChain,
					  psBlock->psBody,
					  psBlock->psBodyTail,
					  IMG_FALSE /* bCheckOnly */,
					  NULL /* puMergeExtraInsts */,
					  NULL /* puNumExtraSecAttrs */,
					  0 /* uAlreadyUsedSecAttrs */);
}

static IMG_VOID TryMergeIRegRestore(PINTERMEDIATE_STATE	psState,
									PIREGALLOC_CONTEXT	psContext,
									PIREGALLOC_BLOCK	psBlockState,
									PINST				psRestoreInst,
									PUSEDEF_CHAIN		psRestoreDestUseDefChain)
/*****************************************************************************
 FUNCTION	: TryMergeIRegRestore
    
 PURPOSE	: For an instruction restoring data into an internal register try
			  to replace the uses of the restore destination by uses of the
			  restore source.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psSaveInst		- Instruction to try and merge.
			  psRestoreDestUseDefChain
								- List of uses of the internal register
								destination.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERVAL	psRestoreInterval;
	IMG_INT32	iMergeExtraInsts;
	PCODEBLOCK	psBlock = psRestoreInst->psBlock;

	psRestoreInterval = GetIntervalByArg(psContext, &psRestoreInst->asDest[0]);
	ASSERT(psRestoreInterval->psUseDefChain == psRestoreDestUseDefChain);

	/*
		Check if replacing the restore destination is possible.
	*/	
	if (!SubstIRegInterval(psState,
						   psContext,
						   psBlockState,
						   psBlock,
						   psRestoreDestUseDefChain,
						   psBlock->psBody,
						   psBlock->psBodyTail,
						   IMG_TRUE /* bCheckOnly */,
						   &iMergeExtraInsts,
						   NULL /* puMergeExtraSecAttrs */,
						   0 /* uAlreadyUsedSecAttrs */))
	{
		return;
	}
	if (iMergeExtraInsts > 0)
	{
		return;
	}

	/*
		Modify all uses of the restore destination so they can be a non-internal
		register then replace them by the source to the restore instruction.
	*/
	SubstIRegInterval(psState,
					  psContext,
					  psBlockState,
					  psRestoreInst->psBlock,
					  psRestoreDestUseDefChain,
					  psBlock->psBody,
					  psBlock->psBodyTail,
					  IMG_FALSE /* bCheckOnly */,
					  NULL /* piMergeExtraInsts */,
					  NULL /* puMergeExtraSecAttrs */,
					  0 /* uAlreadyUsedSecAttrs */);
}

static
IMG_INT32 CmpAccessCount(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*****************************************************************************
 FUNCTION	: CmpAccessCount
    
 PURPOSE	: Helper function to sort two possible merge candidates by their
			  frequency of access.

 PARAMETERS	: psListEntry1			- The candidates to compare.
			  psListEntry2
			  
 RETURNS	: -ve	if the first candidate should be before the second
			  0		if the two candidates can be in any order
			  +ve	if the first candidate should be after the second
*****************************************************************************/
{
	PIREG_MERGE	psMerge1 = IMG_CONTAINING_RECORD(psListEntry1, PIREG_MERGE, sMergeListEntry);
	PIREG_MERGE	psMerge2 = IMG_CONTAINING_RECORD(psListEntry2, PIREG_MERGE, sMergeListEntry);

	return (IMG_INT32)(psMerge1->uUseCount - psMerge2->uUseCount);
}

static
IMG_UINT32 GetUseCount(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefChain)
/*****************************************************************************
 FUNCTION	: GetUseCount
    
 PURPOSE	: Return the number of uses of an intermediate register as a
			  source, including uses of other intermediate register which are
			  partially updated from the first.

 PARAMETERS	: psState				- Compiler state.
			  psUseDefChain			- List of uses and defines.
			  
 RETURNS	: Count of uses as a source.
*****************************************************************************/
{
	IMG_UINT32		uUseCount;

	/*
		Find the start of the chain of intermediate registers which must all be
		simultaneously replaced by non-internal registers.
	*/
	uUseCount = 0;
	for (;;)
	{
		PINST		psDefInst;
		IMG_UINT32	uDefDestIdx;
		PARG		psOldDest;

		psDefInst = UseDefGetDefInstFromChain(psUseDefChain, &uDefDestIdx);
		psOldDest = psDefInst->apsOldDest[uDefDestIdx];
		if (psOldDest == NULL)
		{
			break;
		}
		ASSERT(psOldDest->uType == USEASM_REGTYPE_TEMP);
		psUseDefChain = psOldDest->psRegister->psUseDefChain;
	}

	/*
		Sum the uses for all intermediate registers which are partially defined from one another.
	*/
	for (;;)
	{
		PUSC_LIST_ENTRY	psListEntry;
		PUSEDEF_CHAIN	psNextUseDefChain;

		psNextUseDefChain = NULL;
		for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUse;
	
			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psUse->eType == USE_TYPE_SRC)
			{
				PINST	psUseInst = psUse->u.psInst;

				if (psUseInst->eOpcode != ISAVEIREG)
				{
					uUseCount++;
				}
			}
			if (psUse->eType == USE_TYPE_OLDDEST)
			{
				PARG	psDest;

				ASSERT(psNextUseDefChain == NULL);

				psDest = &psUse->u.psInst->asDest[psUse->uLocation];
				psNextUseDefChain = psDest->psRegister->psUseDefChain;
			}
		}
		if (psNextUseDefChain == NULL)
		{
			break;
		}
		psUseDefChain = psNextUseDefChain;
	}
	return uUseCount;
}

static
IMG_VOID MergeIRegSavesAndRestores(PINTERMEDIATE_STATE psState, 
								   PIREGALLOC_CONTEXT psContext, 
								   PIREGALLOC_BLOCK psBlockState,
								   PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: MergeIRegSavesAndRestores
    
 PURPOSE	: For all instructions restoring data into an internal register and
			  saving data from an internal register try to replace uses/defines of the
			  internal register by the temporary register source/destination.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator context.
			  psBlockState		- State for the current basic block.
			  psBlock			- Current basic block.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY			psListEntry;
	USC_LIST				sMergeList;
	static const IOPCODE	aeRestoreOps[] =
	{
		IRESTOREIREG,
		IRESTORECARRY,
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IVLOAD,
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	static const IOPCODE	aeSaveOps[] = 
	{
		ISAVEIREG,
		ISAVECARRY,
	};
	IMG_UINT32		uOpType;

	/*
		Store a reference to the list of instructions to merge in the context. This is so if the process f
		merging one instruction modifies another we can remove the second from the list.
	*/
	psContext->psMergeList = &sMergeList;

	/*
		Create a list of all instructions restoring data into the internal registers in the current
		basic block sorted by the number of uses of the internal register destination.
	*/
	InitializeList(&sMergeList);
	for (uOpType = 0; uOpType < (sizeof(aeRestoreOps) / sizeof(aeRestoreOps[0])); uOpType++)
	{
		IOPCODE				eRestoreOp = aeRestoreOps[uOpType];
		SAFE_LIST_ITERATOR	sIter;
		
		InstListIteratorInitialize(psState, eRestoreOp, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST			psRestoreInst;
			PIREG_MERGE		psMergeData;
			PUSEDEF_CHAIN	psUseDefChain;
			PARG			psIReg;

			psRestoreInst = InstListIteratorCurrent(&sIter);

			ASSERT(psRestoreInst->uDestCount == 1);

			if (psRestoreInst->psBlock != psBlock)
			{
				continue;
			}

			psIReg = &psRestoreInst->asDest[0];
			ASSERT(psIReg->uType == USEASM_REGTYPE_TEMP);

			if (!IsIRegArgument(psState, psContext, psIReg, NULL /* puAssignedIReg */))
			{
				continue;
			}

			psUseDefChain = UseDefGet(psState, psIReg->uType, psIReg->uNumber);

			psMergeData = UscAlloc(psState, sizeof(*psMergeData));
			psMergeData->psInst = psRestoreInst;
			psMergeData->psIRegUseDefChain = psUseDefChain;
			psMergeData->uUseCount = GetUseCount(psState, psUseDefChain);

			psRestoreInst->sStageData.pvData = (IMG_PVOID)psMergeData;
			SetBit(psRestoreInst->auFlag, INST_INMERGELIST, 1);

			InsertInListSorted(&sMergeList, CmpAccessCount, &psMergeData->sMergeListEntry);
		}
		InstListIteratorFinalise(&sIter);
	}

	/*
		Create a list of all instructions saving data from an internal register.
	*/
	for (uOpType = 0; uOpType < (sizeof(aeSaveOps) / sizeof(aeSaveOps[0])); uOpType++)
	{
		IOPCODE				eSaveOp = aeSaveOps[uOpType];
		SAFE_LIST_ITERATOR	sIter;
		
		InstListIteratorInitialize(psState, eSaveOp, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST			psSaveInst;
			PARG			psIReg;
			PUSEDEF_CHAIN	psUseDefChain;
			PIREG_MERGE		psMergeData;
			PINST			psDefInst;
			IMG_UINT32		uDefDestIdx;

			psSaveInst = InstListIteratorCurrent(&sIter);
	
			ASSERT(psSaveInst->eOpcode == eSaveOp);
			ASSERT(psSaveInst->uDestCount == 1);
			ASSERT(psSaveInst->uArgumentCount == 1);

			if (psSaveInst->psBlock != psBlock)
			{
				continue;
			}

			psIReg = &psSaveInst->asArg[0];
			ASSERT(psIReg->uType == USEASM_REGTYPE_TEMP);

			psUseDefChain = UseDefGet(psState, psIReg->uType, psIReg->uNumber);

			if (!IsIRegArgument(psState, psContext, psIReg, NULL /* puAssignedIReg */))
			{
				continue;
			}

			psDefInst = UseDefGetDefInstFromChain(psUseDefChain, &uDefDestIdx);
			if (psDefInst == NULL)
			{
				continue;
			}

			psMergeData = UscAlloc(psState, sizeof(*psMergeData));
			psMergeData->psInst = psSaveInst;
			psMergeData->psIRegUseDefChain = psUseDefChain;
			psMergeData->uUseCount = GetUseCount(psState, psUseDefChain);


			psSaveInst->sStageData.pvData = (IMG_PVOID)psMergeData;
			SetBit(psSaveInst->auFlag, INST_INMERGELIST, 1);

			InsertInListSorted(&sMergeList, CmpAccessCount, &psMergeData->sMergeListEntry);
		}
		InstListIteratorFinalise(&sIter);
	}

	/*
		Try to replace all uses of the destination of the restore instruction by the restore source and
		all uses of the source to a save instruction by the destination.
	*/
	while ((psListEntry = RemoveListHead(&sMergeList)) != NULL)
	{
		PIREG_MERGE			psMergeData;
		PINST				psInst;

		psMergeData = IMG_CONTAINING_RECORD(psListEntry, PIREG_MERGE, sMergeListEntry);

		psInst = psMergeData->psInst;

		ASSERT(GetBit(psInst->auFlag, INST_INMERGELIST) == 1);
		SetBit(psInst->auFlag, INST_INMERGELIST, 0);

		if (IsRestoreInst(psInst->eOpcode))
		{
			TryMergeIRegRestore(psState,
								psContext,
								psBlockState,
								psInst,
								psMergeData->psIRegUseDefChain);
		}
		else
		{
			TryMergeIRegSave(psState,
							 psContext,
							 psBlockState,
							 psBlock,
							 psInst);
		}

		UscFree(psState, psMergeData);
	}
	
	psContext->psMergeList = NULL;

	/*
		Look for cases where instructions reading internal registers can be split into
		multiple instructions to allow more restores to be folded.
	*/
	SplitIRegInstructions(psState, psContext, psBlockState, psBlock);
}

#define DECL_REF_COMPARISON(NAME, INTERVAL_LIST, REF_TO_COMPARE)											\
static																										\
IMG_INT32 NAME(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)									\
{																											\
	PINTERVAL	psInterval1;																				\
	PINTERVAL	psInterval2;																				\
																											\
	psInterval1 = IMG_CONTAINING_RECORD(psListEntry1, PINTERVAL, INTERVAL_LIST);							\
	psInterval2 = IMG_CONTAINING_RECORD(psListEntry2, PINTERVAL, INTERVAL_LIST);							\
																											\
	return CmpUse(psInterval1->REF_TO_COMPARE, psInterval2->REF_TO_COMPARE);								\
}

DECL_REF_COMPARISON(CmpFirstRef, u.sUsedIntervalListEntry, psUseDefChain->sList.psHead)
DECL_REF_COMPARISON(CmpNextRef, sNextRefListEntry, psNextRef)

static
IMG_VOID AddToUsedIntervalList(PINTERMEDIATE_STATE psState, PIREGALLOC_BLOCK psBlockState, PINTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: AddToUsedIntervalList
    
 PURPOSE	: Add an interval to the list of intervals to be assigned internal
			  registers in the current block.

 PARAMETERS	: psState			- Compiler state.
			  psBlockState		- State for the current basic block.
			  psInterval		- Interval to add.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psInterval->eState == INTERVAL_STATE_UNUSED);
	psInterval->eState = INTERVAL_STATE_PENDING;
	InsertInListSorted(&psBlockState->sUsedIntervalList, CmpFirstRef, &psInterval->u.sUsedIntervalListEntry);
}

static
IMG_UINT32 GetOverlappingFixedRegisterMask(PIREGALLOC_CONTEXT psContext, PUSC_LIST psUsedIntervals, PINTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: GetOverlappingFixedRegisterMask
    
 PURPOSE	: Get the mask of internal registers for which:-
			  (i) An interval can only be assigned that internal register.
			  (ii) That interval is not currently live and will be live simultaneously
			   with a specified interval.

 PARAMETERS	: psContext					- Internal register allocator state.
			  psUsedIntervals			- List of intervals used in future.
			  psInterval				- Specified interval.
			  
 RETURNS	: The mask of internal register.
*****************************************************************************/
{
	IMG_UINT32		uOverlappingFixedRegisterMask;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST		psIntervalUseDefList;
	PUSC_LIST_ENTRY	psInterval_LastRef;

	psIntervalUseDefList = &psInterval->psUseDefChain->sList;
	psInterval_LastRef = psIntervalUseDefList->psTail;

	uOverlappingFixedRegisterMask = 0;
	for (psListEntry = psUsedIntervals->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINTERVAL		psOtherInterval;	
		PUSC_LIST_ENTRY	psOtherInterval_FirstRef;
		PUSC_LIST		psOtherIntervalUseDefList;

		psOtherInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, u.sUsedIntervalListEntry);
		psOtherIntervalUseDefList = &psOtherInterval->psUseDefChain->sList;
		psOtherInterval_FirstRef = psOtherIntervalUseDefList->psHead;

		/*
			Stop once we've seen another interval starting after the last reference to the specified interval
			(the list of intervals is sorted by first reference).
		*/
		if (CmpUse(psInterval_LastRef, psOtherInterval_FirstRef) < 0)
		{
			break;
		}
		if (psOtherInterval->uFixedHwRegMask != psContext->uAllHwRegMask)
		{
			uOverlappingFixedRegisterMask |= psOtherInterval->uFixedHwRegMask;
		}
	}
	return uOverlappingFixedRegisterMask;
}

typedef struct _SPILL_CANDIDATE
{
	/* Internal register to be spilled. */
	IMG_UINT32	uHwReg;
	/* Interval to be spilled. */
	PINTERVAL	psInterval;
	/*
		TRUE if spilling this interval requires modifying the instruction in
		which it is used as the source for unwritten channels in the destination.
	*/
	IMG_BOOL	bSpillPartialDest;
	/* 
		TRUE if there is a future interval simultaneously live with the interval
		we are currently assigned which can be assigned only this internal register.
	*/
	IMG_BOOL	bIsFixed;
	/*
		Try if all references to the interval before the current instruction could
		be replaced by temporary registers - in that case no instructions are
		needed to save the internal register.
	*/
	IMG_BOOL	bMergeSave;
	/*
		Try if all references to the interval after the current instruction could
		be replaced by temporary registers - in that case no instructions are
		needed to restore the internal register.
	*/
	IMG_BOOL	bMergeRestore;
	/*
		The number of extra instructions which have to be inserted if we spill
		this internal register.
	*/
	IMG_INT32	iSpillCost;
	/*
		Location of the instruction next using the internal register.
	*/
	IMG_UINT32	uNextRef;
} SPILL_CANDIDATE, *PSPILL_CANDIDATE;

static
IMG_VOID MkSpillCandidate(PSPILL_CANDIDATE psCandidate)
/*****************************************************************************
 FUNCTION	: MkSpillCandidate
    
 PURPOSE	: Initialize the data structure describing a candidate for spilling.

 PARAMETERS	: psCandidate		- Structure to initialize.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psCandidate->uHwReg = USC_UNDEF;
	psCandidate->psInterval = NULL;
	psCandidate->bSpillPartialDest = IMG_FALSE;
	psCandidate->iSpillCost = 0;
	psCandidate->bMergeSave = IMG_FALSE;
	psCandidate->bMergeRestore = IMG_FALSE;
	psCandidate->bIsFixed = IMG_FALSE;
	psCandidate->psInterval = NULL;
	psCandidate->uNextRef = USC_UNDEF;
}

static
IMG_BOOL IsBetterSpillCandidate(PSPILL_CANDIDATE psBest, PSPILL_CANDIDATE psReplace)
/*****************************************************************************
 FUNCTION	: IsBetterSpillCandidate
    
 PURPOSE	: Compare two internal registers whose existing contents might be
			  spilled so they can be allocated to another interval.

 PARAMETERS	: psBest			- Existing best candidate.
			  psReplace			- Possible replacement.
			  
 RETURNS	: TRUE if the replacement candidate is better than the existing
			  best candidate.
*****************************************************************************/
{
	/*
		If there is no best candidate then always replace.
	*/
	if (psBest->uHwReg == USC_UNDEF)
	{
		return IMG_TRUE;
	}
	/*	
		If the best spill candidate interferes with a fixed use of
		an internal register and the replacement doesn't then replace.
	*/
	if (psBest->bIsFixed != psReplace->bIsFixed)
	{
		return psBest->bIsFixed;
	}
	/*
		Replace the internal register which requires the minimum number
		of instructions to save its contents.
	*/
	if (psBest->iSpillCost > psReplace->iSpillCost)
	{
		return IMG_TRUE;
	}
	else if (psBest->iSpillCost == psReplace->iSpillCost)
	{
		/*
			Replace the internal register whose next use is furthest away.
		*/
		if (psBest->uNextRef < psReplace->uNextRef)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL IsUsedInDifferentBlock(PUSEDEF_CHAIN psUseDefChain, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: IsUsedInDifferentBlock
    
 PURPOSE	: Check if an instruction is referenced outside of a specified
			  block.

 PARAMETERS	: psUseDefChain			- Temporary register to check.
			  psBlock				- Block to check for.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;
		PINST	psInst;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef == psUseDefChain->psDef)
		{
			continue;
		}
		/*
			Check for a reference in the driver prolog/epilog.
		*/
		if (!UseDefIsInstUseDef(psUseDef))
		{
			return IMG_TRUE;
		}
		psInst = psUseDef->u.psInst;
		/*
			Check for a reference in an instruction in a different block.
		*/
		if (psInst->psBlock != psBlock)
		{
			return IMG_TRUE;
		}
		/*
			Treat a reference in a delta instruction as always in a different
			block for simplicity otherwise an internal register will be live
			out of the block and back in at the top.
		*/
		if (psInst->eOpcode == IDELTA)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
PINTERVAL AddNewBlockInterval(PINTERMEDIATE_STATE	psState, 
							  PIREGALLOC_CONTEXT	psContext, 
							  PIREGALLOC_BLOCK		psBlockState, 
							  PUSEDEF_CHAIN			psTemp,
							  PUSEDEF_CHAIN			psOrigTemp)
/*****************************************************************************
 FUNCTION	: AddNewBlockInterval
    
 PURPOSE	: Allocate a new interval data structure for an intermediate register
			  which is only used in the current block.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator state for the
								current block.
			  psTemp			- Intermediate register to allocate the data
								structure for.
			  psOrigTemp		- If the interval is a copy of another interval
								then points to the source for the copy.
			  
 RETURNS	: A pointer to the allocated interval data structure.
*****************************************************************************/
{
	PINTERVAL	psNewInterval;

	psNewInterval = UscAlloc(psState, sizeof(*psNewInterval));

	InitializeInterval(psState, psContext, psNewInterval, psTemp, psOrigTemp);

	AppendToList(&psBlockState->sBlockIntervalList, &psNewInterval->sBlockIntervalListEntry);

	psNewInterval->psNextRef = NULL;

	return psNewInterval;
}

static
IMG_VOID CopyIntervalData(PINTERVAL psDest, PINTERVAL psSrc)
/*****************************************************************************
 FUNCTION	: CopyIntervalData
    
 PURPOSE	: Copy data from one interval to another interval representing
			  a subrange of the first.

 PARAMETERS	: psDest	- Interval to copy to.
			  psSrc		- Interval to copy form.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psDest->eType = psSrc->eType;
	psDest->uFixedHwRegMask = psSrc->uFixedHwRegMask;
}

static
PINTERVAL SaveInterval(PINTERMEDIATE_STATE		psState,
					   PIREGALLOC_CONTEXT		psContext,
					   PIREGALLOC_BLOCK			psBlockState,
					   PCODEBLOCK				psBlock,
					   PUSEDEF_CHAIN			psIntervalToSave,
					   INTERVAL_TYPE			eSaveType,
					   PUSC_LIST_ENTRY			psFirstRefInBlock,
					   PUSC_LIST_ENTRY			psLastRefInBlock,
					   IMG_BOOL					bSaveBefore)
/*********************************************************************************
 Function			: SaveInterval

 Description		: Allocate a new intermediate register, replace all instances of another 
					  intermediate register in a specified range by the new intermediate
					  register and copy the new intermediate register into the existing
					  one after the end of the range.

 Parameters			: psState				- Compiler state.
					  psContext				- Register allocator state.
					  psBlockState			- Register allocator state for the current block.
					  psBlock				- Current block.
					  psIntervalToSave		- Intermediate register to replace.
					  eSaveType				- Type of data containing in the intermediate register.
					  psFirstRefInBlock		- Start of the range to replace.
					  psLastRefInBlock		- End of the range to replace.
					  bSaveBefore			- If TRUE copy before the instruction at the end of the range.
											  If FALSE copy after the instruction at the end of the range.

 Globals Effected	: None

 Return				: The interval data structure for the new intermediate register.
*********************************************************************************/
{
	ARG				sSaveTemp;
	PINST			psSaveInst;
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF			psLastRef;
	PINST			psLastRefInst;
	PUSC_LIST_ENTRY	psNextListEntry;
	IMG_UINT32		uChansUsedFromSave;
	PUSC_LIST_ENTRY	psFirstRefNotToReplace;
	PINTERVAL		psSaveInterval;
	IMG_BOOL		bUsedAsPartialDest;

	/*
		Allocate a new temporary to hold the data to be saved.
	*/
	MakeNewTempArg(psState, psIntervalToSave->eFmt, &sSaveTemp);

	/*	
		Get the last reference to the interval to save in the block.
	*/
	psLastRef = IMG_CONTAINING_RECORD(psLastRefInBlock, PUSEDEF, sListEntry);
	ASSERT(psLastRef->eType == DEF_TYPE_INST || psLastRef->eType == USE_TYPE_SRC || psLastRef->eType == USE_TYPE_OLDDEST);
	psLastRefInst = psLastRef->u.psInst;
	ASSERT(psLastRefInst->psBlock == psBlock);

	/*
		Replace all references to the interval in the block by the new temporary.
	*/
	psFirstRefNotToReplace = psLastRefInBlock->psNext;
	bUsedAsPartialDest = IMG_FALSE;
	for (psListEntry = psFirstRefInBlock; psListEntry != psFirstRefNotToReplace; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psUseDef));
		ASSERT(psUseDef->u.psInst->psBlock == psBlock);

		if (psUseDef->eType == USE_TYPE_OLDDEST)
		{
			bUsedAsPartialDest = IMG_TRUE;
		}

		UseDefSubstUse(psState, psIntervalToSave, psUseDef, &sSaveTemp);
	}

	/*
		Create an instruction copying to the new temporary from the interval.
	*/
	psSaveInst = AllocateInst(psState, psLastRefInst);
	if (bSaveBefore)
	{
		InsertInstBefore(psState, psBlock, psSaveInst, psLastRefInst);
	}
	else
	{
		InsertInstAfter(psState, psBlock, psSaveInst, psLastRefInst);
	}
	switch (eSaveType)
	{
		case INTERVAL_TYPE_GENERAL:	
		{
			SetOpcode(psState, psSaveInst, ISAVEIREG); 
			break;
		}
		case INTERVAL_TYPE_CARRY:
		case INTERVAL_TYPE_EXISTING_IREG:
		{
			SetOpcode(psState, psSaveInst, ISAVECARRY); 
			break;
		}
		default: imgabort();
	}
	SetDest(psState, 
		    psSaveInst, 
		    0 /* uDestIdx */, 
		    psIntervalToSave->uType, 
		    psIntervalToSave->uNumber, 
		    psIntervalToSave->eFmt);
	SetSrcFromArg(psState, psSaveInst, 0 /* uSrcIdx */, &sSaveTemp);

	/*	
		Get the mask of channels used from the interval in the unreplaced uses.
	*/
	uChansUsedFromSave = 0;
	for (psListEntry = psIntervalToSave->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef != psIntervalToSave->psDef)
		{
			uChansUsedFromSave |= GetUseChanMask(psState, psUseDef);
		}
	}
	psSaveInst->auLiveChansInDest[0] = uChansUsedFromSave;

	/*
		Add the new temporary to the list of intervals to be assigned to internal registers.
	*/
	psSaveInterval = 
		AddNewBlockInterval(psState, 
							psContext, 
							psBlockState, 
							UseDefGet(psState, sSaveTemp.uType, sSaveTemp.uNumber),
							psIntervalToSave);

	/*	
		Record if the interval is used in the partial definition of another interval.
	*/
	psSaveInterval->bUsedAsPartialDest = bUsedAsPartialDest;

	/*
		Store a pointer from the interval to the instruction which saves its contents.
	*/
	psSaveInterval->psSaveInst = psSaveInst;

	return psSaveInterval;
}

static
PINTERVAL RestoreInterval(PINTERMEDIATE_STATE	psState, 
						  PIREGALLOC_CONTEXT	psContext,
						  PIREGALLOC_BLOCK		psBlockState,
						  PCODEBLOCK			psBlock,
						  PINTERVAL				psSrcInterval,
						  PUSC_LIST_ENTRY		psFirstRefInBlock, 
						  PUSC_LIST_ENTRY		psFirstRefNotInBlock)
/*********************************************************************************
 Function			: RestoreInterval

 Description		: Create a new interval for a subrange of an existing interval
					  starting with an instruction copying from the source for
					  the old interval into the new one.

 Parameters			: psState				- Compiler state.
					  psContext				- Register allocator state.
					  psBlockState			- Register allocator state for the current block.
					  psBlock				- Current block.
					  psSrcInterval			- Existing interval.
					  psFirstRefInBlock		- Start of the range to replace.
					  psFirstRefNotInBlock	- First reference after the end of the
											range to replace.

 Globals Effected	: None

 Return				: The interval data structure for the new intermediate register.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;
	ARG				sRestoreTemp;
	PUSEDEF			psFirstRef;
	PINST			psFirstRefInst;
	PINST			psRestoreInst;
	IMG_UINT32		uChansUsedFromRestore;
	PINTERVAL		psRestoreInterval;
	PUSEDEF_CHAIN	psTempToReplace = psSrcInterval->psUseDefChain;
	UF_REGFORMAT	eIntervalFormat = psTempToReplace->eFmt;
	INTERVAL_TYPE	eRestoreType = psSrcInterval->eType;
	IMG_BOOL		bUsedAsPartialDest;

	/*
		Allocate a new temporary to hold the restored data.
	*/
	MakeNewTempArg(psState, eIntervalFormat, &sRestoreTemp);

	/*	
		Get the first reference to the interval to restore in the block.
	*/
	psFirstRef = IMG_CONTAINING_RECORD(psFirstRefInBlock, PUSEDEF, sListEntry);
	ASSERT(psFirstRef->eType == USE_TYPE_SRC || psFirstRef->eType == USE_TYPE_OLDDEST);
	psFirstRefInst = psFirstRef->u.psInst;
	ASSERT(psFirstRefInst->psBlock == psBlock);

	/*
		Create an instruction copying from the interval to the new temporary.
	*/
	psRestoreInst = AllocateInst(psState, psFirstRefInst);
	InsertInstBefore(psState, psBlock, psRestoreInst, psFirstRefInst);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psSrcInterval->psOrigTemp == NULL)
	{
		SetOpcode(psState, psRestoreInst, IVLOAD);
		SetVectorSource(psState, 
						psRestoreInst, 
						0 /* uSrcIdx */, 
						psSrcInterval->bOrigVectorWasWide, 
						psSrcInterval->uOrigVectorLength, 
						psSrcInterval->asOrigVector);
		psRestoreInst->u.psVec->aeSrcFmt[0] = eIntervalFormat;
		psRestoreInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PUSEDEF_CHAIN	psRestoreSource = psSrcInterval->psOrigTemp;

		switch (eRestoreType)
		{
			case INTERVAL_TYPE_GENERAL:	
			{
				SetOpcode(psState, psRestoreInst, IRESTOREIREG); 
				break;
			}	
			case INTERVAL_TYPE_CARRY:
			case INTERVAL_TYPE_EXISTING_IREG:
			{
				SetOpcode(psState, psRestoreInst, IRESTORECARRY); 
				break;
			}
			default: imgabort();
		}

		SetSrc(psState, 
			   psRestoreInst, 
			   0 /* uSrcIdx */, 
			   psRestoreSource->uType, 
			   psRestoreSource->uNumber, 
			   psRestoreSource->eFmt);
	}
	SetDestFromArg(psState, psRestoreInst, 0 /* uDestIdx */, &sRestoreTemp);

	/*
		Replace all references to the interval in the block by the new temporary (apart from the
		in the restore instruction).
	*/
	uChansUsedFromRestore = 0;
	bUsedAsPartialDest = IMG_FALSE;
	for (psListEntry = psFirstRefInBlock; psListEntry != psFirstRefNotInBlock; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psUseDef));
		ASSERT(psUseDef->u.psInst->psBlock == psBlock);

		if (psUseDef->eType == USE_TYPE_OLDDEST)
		{
			bUsedAsPartialDest = IMG_TRUE;
		}

		if (psUseDef->u.psInst != psRestoreInst)
		{
			uChansUsedFromRestore |= GetUseChanMask(psState, psUseDef);

			UseDefSubstUse(psState, psTempToReplace, psUseDef, &sRestoreTemp);
		}
	}

	/*
		Set the mask of channels used from the restored interval.
	*/
	psRestoreInst->auLiveChansInDest[0] = /*psRestoreInst->auDestMask[0] = */uChansUsedFromRestore;

	/*
		Add the new temporary to the list of intervals to be assigned to internal registers.
	*/
	psRestoreInterval = 
		AddNewBlockInterval(psState, 
							psContext, 
							psBlockState, 
							UseDefGet(psState, sRestoreTemp.uType, sRestoreTemp.uNumber),
							psSrcInterval->psOrigTemp);

	/*	
		Record if the interval is used in the partial definition of another interval.
	*/
	psRestoreInterval->bUsedAsPartialDest = bUsedAsPartialDest;

	/*
		Add the interval for the remaining references in the block to the list of intervals to be
		assigned internal registers.
	*/		
	AddToUsedIntervalList(psState, psBlockState, psRestoreInterval);

	/*
		Copy information about the original interval to the interval for the references after the restore.
	*/
	CopyIntervalData(psRestoreInterval, psSrcInterval);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psSrcInterval->psOrigTemp == NULL)
	{
		psRestoreInterval->bOrigVectorWasWide = psSrcInterval->bOrigVectorWasWide;
		psRestoreInterval->uOrigVectorLength = psSrcInterval->uOrigVectorLength;
		memcpy(psRestoreInterval->asOrigVector, 
			   psSrcInterval->asOrigVector, 
			   sizeof(psSrcInterval->asOrigVector[0]) * psSrcInterval->uOrigVectorLength);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	psRestoreInterval->bDefinedByRestore = IMG_TRUE;

	return psRestoreInterval;
}

static
IMG_VOID CopyFixedHwReg(PINTERMEDIATE_STATE psState, PINTERVAL psDestInterval, PINTERVAL psSrcInterval)
/*****************************************************************************
 FUNCTION	: CopyFixedHwReg
    
 PURPOSE	: Set one interval to be restricted to, at least, the same
			  subset of the available internal register as another interval.

 PARAMETERS	: psState			- Compiler state.
			  psDestInterval	- Second interval.
			  psSrcInterval		- First interval.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psDestInterval->uFixedHwRegMask &= psSrcInterval->uFixedHwRegMask;
	ASSERT(psDestInterval->uFixedHwRegMask != 0);
}

static
IMG_VOID CopyPartialDest(PINTERMEDIATE_STATE	psState, 
						 PIREGALLOC_CONTEXT		psContext,
						 PIREGALLOC_BLOCK		psBlockState,
						 PINST					psCopyFromInst, 
						 IMG_UINT32				uCopyFromDestIdx,
						 IMG_BOOL				bBefore)
/*********************************************************************************
 Function			: CopyPartialDest

 Description		: Insert an instruction to copy the partial destination into a
					  newly allocated temporary and replace the partial destination by
					  the new temporary.

 Parameters			: psState			- Compiler state.
					  psContext			- Register allocator state.
					  psBlockState		- Register allocator state for the current block.
					  psCopyFromInst	- Partial destination argument to be copied.
					  uCopyFromDestIdx
					  bBefore			- If TRUE insert the copy instruction before the 
					                      original instruction.
										  If FALSE insert it after.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	ARG	sNewDest;

	MakePartialDestCopy(psState, psCopyFromInst, uCopyFromDestIdx, bBefore, &sNewDest);

	/*
		Mark the new temporary as needing to be assigned an internal register.
	*/
	if (psBlockState != NULL)
	{
		PUSEDEF_CHAIN	psNewDestUseDef;
		PINTERVAL		psNewDestInterval;

		psNewDestUseDef = sNewDest.psRegister->psUseDefChain;
		psNewDestInterval = AddNewBlockInterval(psState, psContext, psBlockState, psNewDestUseDef, psNewDestUseDef);
		psNewDestInterval->bUsedAsPartialDest = IMG_TRUE;
		AddToUsedIntervalList(psState, psBlockState, psNewDestInterval);

		if (bBefore)
		{
			PINTERVAL	psOrigDestInterval;

			/*
				If the destination to the original instruction needs to be assigned a fixed hardware register
				then assign the same hardware register to the interval for old value of the destination.
			*/
			psOrigDestInterval = GetIntervalByArg(psContext, &psCopyFromInst->asDest[uCopyFromDestIdx]);
			CopyFixedHwReg(psState, psNewDestInterval, psOrigDestInterval);
		}
	}
}

static
IMG_VOID ProcessPartialDest(PINTERMEDIATE_STATE psState, 
							PIREGALLOC_CONTEXT psContext, 
							PIREGALLOC_BLOCK psBlockState,
							PINTERVAL psInterval)
/*********************************************************************************
 Function			: ProcessPartialDest

 Description		: For any cases where an interval is used as the source for the
					  unwritten channels in a partially/conditionally written destination
					  ensure that that is the last use in the block.

 Parameters			: psState			- Compiler state.
					  psContext			- Register allocator state.
					  psBlockState		- Register allocator state for the current flow
										control block.
					  psInterval		- Interval to check.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psUseListEntry;

	psUseListEntry = psInterval->psUseDefChain->sList.psHead;
	while (psUseListEntry != NULL)
	{
		IMG_UINT32		uPartialDestCount;
		PINST			psInst;
		IMG_BOOL		bLastRef;
		IMG_BOOL		bCopy, bCopyBefore;
		PUSC_LIST_ENTRY	psFirstRefInInst;
		PUSC_LIST_ENTRY	psLastRefInInst;
		PUSC_LIST_ENTRY	psListEntry;
		IMG_BOOL		bLastRefInInst;
		IMG_UINT32		uPartialDestIdx;
		PARG			psNewDest;
		PINTERVAL		psNewDestInterval;
		IMG_BOOL		bInvalidPrecolouring, bNewTempIsGPI;
		
		/*
			Build up a list of all the references in the same instruction.
		*/
		uPartialDestCount = 0;
		uPartialDestIdx = USC_UNDEF;
		bLastRef = IMG_FALSE;
		psInst = NULL;
		psFirstRefInInst = NULL;
		psLastRefInInst = NULL;
		for (;;)
		{
			PUSEDEF	psRef;

			if (psUseListEntry == NULL)
			{
				bLastRef = IMG_TRUE;
				break;
			}

			psRef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			ASSERT(UseDefIsInstUseDef(psRef));

			/*
				Stop once we see a reference in a different instruction.
			*/
			if (psInst != NULL && psRef->u.psInst != psInst)
			{
				bLastRef = IMG_FALSE;
				break;
			}

			if (psFirstRefInInst == NULL)
			{
				psFirstRefInInst = psUseListEntry;
			}
			psLastRefInInst = psUseListEntry;

			psInst = psRef->u.psInst;

			if (psRef->eType == USE_TYPE_OLDDEST)
			{
				uPartialDestIdx = psRef->uLocation;
				uPartialDestCount++;
			}

			psUseListEntry = psUseListEntry->psNext;
		}

		/*
			Nothing to do if the temporary isn't used as a partial destination in this instruction.
		*/
		if (uPartialDestCount == 0)
		{
			continue;
		}

		/*
			Get the other interval which is defined using some channels
			from the current interval.
		*/
		ASSERT(uPartialDestIdx != USC_UNDEF);
		psNewDest = &psInst->asDest[uPartialDestIdx];
		psNewDestInterval = GetIntervalByArg(psContext, psNewDest);
		
		bInvalidPrecolouring = IMG_FALSE;
		bNewTempIsGPI = IMG_TRUE;
		
		/*
			Check if the two intervals have incompatible fixed hardware registers.
		*/
		if (psNewDestInterval == NULL)
		{
			/* 
				Destination is temp, but partial dest is GPI
			*/
			bInvalidPrecolouring = IMG_TRUE;
			bNewTempIsGPI = IMG_FALSE;
		}
		else if ((psNewDestInterval->uFixedHwRegMask & psInterval->uFixedHwRegMask) == 0)
		{
			bInvalidPrecolouring = IMG_TRUE;
		}

		/*
			Check for cases where an internal register couldn't be assigned to the partial destination: either
			the instruction deschedules or all the internal registers would have to be used for the instruction
			source arguments.
		*/
		bCopy = IMG_FALSE;
		bCopyBefore = IMG_FALSE;
		if (IsDeschedulingPoint(psState, psInst))
		{
			/*
				Replace the instruction's destination by a newly allocated temporary register, clear the
				instruction's existing partial destination and insert a move instruction after the instruction
				copying the new destination to the old destination e.g.

				(pZ) SMP		rX[=rY], ....
					->
				(pZ) SMP		rW, ....
				(pZ) MOV		rX[=rY], rW
			*/
			bCopy = IMG_TRUE;
		}
		/*
			Check if the temporary is used as a partial destination but this isn't the last use.
		*/
		else if (uPartialDestCount > 1 || !bLastRef || bInvalidPrecolouring)
		{
			/*
				Insert a move instruction before the current instruction copying from this interval to a newly
				allocated temporary register and use the new register as the source for unwritten channels in
				the current instruction e.g.

				ALU		rX[=rY], ....
					->
				MOV		rW, rY
				ALU		rX[=rW], ....
			*/
			bCopy = IMG_TRUE;
			bCopyBefore = IMG_TRUE;
		}

		if (!bCopy)
		{
			/*
				Set the intervals for the new and old destinations to use the same fixed hardware register.
			*/
			CopyFixedHwReg(psState, psInterval, psNewDestInterval);
			CopyFixedHwReg(psState, psNewDestInterval, psInterval);
			psInterval->bUsedAsPartialDest = IMG_TRUE;
			continue;
		}

		/*
			Fix all of the uses of this temporary as a partial destination in this instruction.
		*/
		psListEntry = psFirstRefInInst;
		bLastRefInInst = IMG_FALSE;
		do
		{
			PUSEDEF	psRef;
			
			psRef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psListEntry == psLastRefInInst)
			{
				bLastRefInInst = IMG_TRUE;
			}
			else
			{
				psListEntry = psListEntry->psNext;
			}
			if (psRef->eType == USE_TYPE_OLDDEST)
			{
				if (bNewTempIsGPI)
				{
					CopyPartialDest(psState, psContext, psBlockState, psInst, psRef->uLocation, bCopyBefore);
				}
				else
				{
					CopyPartialDest(psState, psContext, NULL, psInst, psRef->uLocation, bCopyBefore);
				}
			}
		} while (!bLastRefInInst);
	}
}

static
IMG_UINT32 GetRefBlockIndex(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY psRefListEntry)
/*****************************************************************************
 FUNCTION	: GetRefBlockIndex
    
 PURPOSE	: Get the index of an instruction using or defining an intermediate
			  register in its block (where zero is the first instruction, one is
			  the second instruction, etc).

 PARAMETERS	: psState			- Compiler state.
			  psRefListEntry	- Use or define.
			  
 RETURNS	: The index of the using/defining instruction in its block.
*****************************************************************************/
{
	PUSEDEF	psRef;

	psRef = IMG_CONTAINING_RECORD(psRefListEntry, PUSEDEF, sListEntry);
	ASSERT(UseDefIsInstUseDef(psRef));
	return psRef->u.psInst->uBlockIndex;
}

static
IMG_VOID ExpandOpcode(PINTERMEDIATE_STATE	psState, 
					  PCODEBLOCK			psBlock, 
					  IOPCODE				eOpcodeToExpand, 
					  PFN_EXPAND			pfExpand)
/*****************************************************************************
 FUNCTION	: ExpandOpcode
    
 PURPOSE	: Call a function on all instructions with a specified opcode and
			  in a specified block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Expand instructions in this block only.
			  eOpcodeToExpand	- Opcode of the instructions to expand.
			  pfExpand			- Callback.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;
		
	InstListIteratorInitialize(psState, eOpcodeToExpand, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		if (psInst->psBlock == psBlock)
		{
			pfExpand(psState, psInst);
		}
	}
	InstListIteratorFinalise(&sIter);
}

static
PUSC_LIST_ENTRY SkipNonInstReferences(PUSC_LIST_ENTRY psNextRefListEntry)
/*****************************************************************************
 FUNCTION	: SkipNonInstReferences
    
 PURPOSE	: Skip entries in a list of uses/defines which don't correspond to
			  an intermediate instruction.

 PARAMETERS	: psNextRefListEntry		- Starting point.
			  
 RETURNS	: The first entry after the starting point which is a use or define
			  in an intermediate instruction.
*****************************************************************************/
{
	for (;;)
	{
		PUSEDEF	psNextRef;

		if (psNextRefListEntry == NULL)
		{
			break;
		}
		psNextRef = IMG_CONTAINING_RECORD(psNextRefListEntry, PUSEDEF, sListEntry);
		if (UseDefIsInstUseDef(psNextRef))
		{
			break;
		}
		psNextRefListEntry = psNextRefListEntry->psNext;
	}
	return psNextRefListEntry;
}

static
IMG_UINT32 GetNextRefAfterCurrent(PINTERMEDIATE_STATE psState, 
								  PINTERVAL psInterval, 
								  IMG_UINT32 uCurrentRef,
								  PUSEDEF* ppsNextRef)
/*****************************************************************************
 FUNCTION	: GetNextRefAfterCurrent
    
 PURPOSE	: Get the location of the next reference to an interval after a
			  specified point.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psInterval		- Interval to get the next reference for.
			  uCurrentRef		- Get the next reference after this point.
			  ppsNextRef		- If non-NULL returns details of the next
								reference.
			  
 RETURNS	: The location of the next reference.
*****************************************************************************/
{
	if (psInterval == NULL)
	{
		if (ppsNextRef != NULL)
		{
			*ppsNextRef = NULL;
		}
		return UINT_MAX;
	}
	else
	{
		PUSC_LIST_ENTRY	psNextRefListEntry;

		psNextRefListEntry = psInterval->psUseDefChain->sList.psHead;
		for (;;)
		{
			PUSEDEF		psNextRef;
			IMG_UINT32	uNextRef;

			psNextRef = IMG_CONTAINING_RECORD(psNextRefListEntry, PUSEDEF, sListEntry);
			uNextRef = UseDefToRef(psState, psNextRef);
			if (uNextRef >= uCurrentRef)
			{
				if (ppsNextRef != NULL)
				{
					*ppsNextRef = psNextRef;
				}
				return uNextRef;
			}
			psNextRefListEntry = psNextRefListEntry->psNext;
		}
	}
}

static
IMG_INT32 GetSaveCost(PINTERMEDIATE_STATE	psState,
					  PIREGALLOC_CONTEXT	psContext,
					  PIREGALLOC_BLOCK		psBlockState,
					  PCODEBLOCK			psBlock,
					  PINTERVAL				psInterval,
					  PINST					psRangeEndInst,
					  IMG_PBOOL				pbMergeSave,
					  IMG_UINT32			uAlreadyUsedSecAttrs)
/*****************************************************************************
 FUNCTION	: GetSaveCost
    
 PURPOSE	: Get the number of intermediate instructions required to save the
			  contents of the interval.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator state for the
								current basic block.
			  psBlock			- Current basic block.
			  psInterval		- Interval to save.
			  psRangeEndInst	- Point to save the interval after.
			  pbMergeSave		- Returns TRUE if the save of the internal
								can be merged with the instruction defining
								it.
			  uAlreadyUsedSecAttrs 
								- The number of secondary attributes that have already been
								used when saving/restoring this interval
			  
 RETURNS	: The number of instructions.
*****************************************************************************/
{
	IMG_INT32	iSaveCost;
	PINST		psDefInst;
	IMG_UINT32	uDefDestIdx;
	IMG_INT32	iMergeExtraInstCount;

	*pbMergeSave = IMG_FALSE;

	/*
		Get the instruction defining the interval.
	*/	
	psDefInst = UseDefGetDefInst(psState, 
								 psInterval->psUseDefChain->uType, 
								 psInterval->psUseDefChain->uNumber, 
								 &uDefDestIdx);
	ASSERT(psDefInst != NULL);
	ASSERT(uDefDestIdx < psDefInst->uDestCount);

	/*
		Check if this interval is already defined by a copy from a unified store register
		into an internal register.
	*/
	if (IsMergeableRestoreInst(psState, psInterval, psDefInst))
	{
		iSaveCost = 0;
	}
	else
	{
		/*
			Get the count of instructions required to copy from the internal register
			to temporary registers.
		*/
		iSaveCost = GetSaveInstCount(psContext, psDefInst->auLiveChansInDest[uDefDestIdx]);
	}

	/*
		Check if its possible to save the interval by writing 
		directly to temporary registers from the defining
		instruction.
	*/
	if (SubstIRegInterval(psState,
						  psContext,
						  psBlockState,
						  psBlock,
						  psInterval->psUseDefChain,
						  psBlock->psBody,
						  psRangeEndInst,
						  IMG_TRUE /* bCheckOnly */,
						  &iMergeExtraInstCount,
						  NULL /* puMergeExtraSecAttrs */,
						  uAlreadyUsedSecAttrs))
	{
		if (iMergeExtraInstCount < iSaveCost)
		{
			*pbMergeSave = IMG_TRUE;
			return iMergeExtraInstCount;
		}
	}
	return iSaveCost;
}

static
IMG_VOID GetSpillPartialDestCost(PINTERMEDIATE_STATE	psState,
								 PIREGALLOC_CONTEXT		psContext,
								 PIREGALLOC_BLOCK		psBlockState,
								 PCODEBLOCK				psBlock,
								 PINTERVAL				psInterval,
								 IMG_UINT32				uCurrentRef,
								 IMG_UINT32				uFirstUseRef,
								 PSPILL_CANDIDATE		psCandidate)
/*****************************************************************************
 FUNCTION	: GetSpillPartialDestCost
    
 PURPOSE	: If more intervals are sources/partial destinations to an
			  instruction than there are internal registers then we can make
			  an internal register available by moving the partial destination use
			  to after the using instruction. Check how many extra instructions
			  would need to be inserted in this case.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator per-block state.
			  psBlock		- Current basic block.
			  psInterval	- Interval we are currently assigned an internal
							register for.
			  uCurrentRef	- ID of the define of psInterval 
			  uFirstUseRef	- ID of the first use of psInterval.
			  psCandidate	- Returns information about the spill.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERVAL	psOldDestInterval;
	PUSEDEF		psOldDestUse;
	IMG_UINT32	uLive;

	/*
		Initialize the return data structure.
	*/
	MkSpillCandidate(psCandidate);

	/*
		For all intervals currently live (including the one not yet assigned an internal
		register): find the one which is the source for unwritten channels in the
		destination of the next instruction.

		There must be at least one since there are at least as many internal registers
		as the maximum number of source arguments to any instruction which must be
		internal registers.
	*/
	psOldDestInterval = NULL;
	psOldDestUse = NULL;
	for (uLive = 0; uLive <= psContext->uNumHwRegs; uLive++)
	{	
		PUSEDEF		psNextRef;
		PINTERVAL	psLiveInterval;

		if (uLive == 0)
		{
			psLiveInterval = psInterval;
		}
		else
		{
			psLiveInterval = psContext->asHwReg[uLive - 1].psInterval;
			ASSERT(psLiveInterval != NULL);
		}

		GetNextRefAfterCurrent(psState, psLiveInterval, uCurrentRef + 1, &psNextRef);
		ASSERT(UseDefToRef(psState, psNextRef) <= uFirstUseRef);

		if (psNextRef->eType == USE_TYPE_OLDDEST)
		{
			psOldDestInterval = psLiveInterval;
			psOldDestUse = psNextRef;
			break;
		}
	}
	ASSERT(psOldDestInterval != NULL);

	psCandidate->psInterval = psOldDestInterval;
	psCandidate->uHwReg = psOldDestInterval->uHwReg;
	psCandidate->bSpillPartialDest = IMG_TRUE;

	/*
		We'll always need one instruction for the extra copy instruction inserted
		after the instruction with the partial destination.
	*/
	psCandidate->iSpillCost = 1;

	/*
		Get the extra instructions required to save the contents of the internal register.
	*/
	psCandidate->iSpillCost += 
		GetSaveCost(psState,
					psContext,
					psBlockState,
					psBlock,
					psOldDestInterval,
					psOldDestUse->u.psInst->psPrev,
					&psCandidate->bMergeSave,
					0 /* uAlreadyUsedSecAttrs */);
}

static
IMG_VOID SpillPartialDest(PINTERMEDIATE_STATE	psState, 
						  PIREGALLOC_CONTEXT	psContext, 
						  PIREGALLOC_BLOCK		psBlockState,
						  PINTERVAL				psOldDestInterval,
						  IMG_UINT32			uCurrentRef, 
						  IMG_UINT32			uFirstUseRef)
/*****************************************************************************
 FUNCTION	: SpillPartialDest
    
 PURPOSE	: If more intervals are sources/partial destinations to an
			  instruction than there are internal registers then move
			  the partial destination uses to new MOV instructions inserted
			  after the instruction.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator per-block state.
			  psOldDestInterval
							- Interval which is next used as a partial
							destination.
			  uCurrentRef	- ID of the define of the interval we are
							currently assigning an internal register for.
			  uFirstUseRef	- ID of the first use of the internal we are
							currently assigning an internal register for.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF		psOldDestUse;

	GetNextRefAfterCurrent(psState, psOldDestInterval, uCurrentRef + 1, &psOldDestUse);
	ASSERT(UseDefToRef(psState, psOldDestUse) <= uFirstUseRef);
	ASSERT(psOldDestUse->eType == USE_TYPE_OLDDEST);

	/*
		Convert
			(pX) ALU		DEST[=OLDDEST], ...
		->
			(pX) ALU		TEMP, ...
			(!pX)MOV		DEST[=TEMP], OLDDEST
	*/
	CopyPartialDest(psState,
					psContext,
					psBlockState,
					psOldDestUse->u.psInst,
					psOldDestUse->uLocation,
					IMG_FALSE /* bBefore */);
	psOldDestUse = NULL;
}

static
IMG_BOOL KilledBeforeInst(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, IMG_UINT32 uRefToCheck)
/*****************************************************************************
 FUNCTION	: KilledBeforeInst
    
 PURPOSE	: Check if the last use of at least one of the intervals currently
			  in an internal register is before a specified instruction.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  uRefToCheck	- Instruction to check.
			  
 RETURNS	: TRUE if one of the intervals is killed.
*****************************************************************************/
{
	IMG_UINT32	uHwReg;

	for (uHwReg = 0; uHwReg < psContext->uNumHwRegs; uHwReg++)
	{
		PINTERNAL_REGISTER	psHwReg = &psContext->asHwReg[uHwReg];
		PUSC_LIST_ENTRY		psLastRefListEntry;
		PUSEDEF				psLastRef;
		IMG_UINT32			uLastRef;

		if (psHwReg->psInterval == NULL)
		{
			continue;
		}

		/*
			Get the last reference to the interval in this internal register.
		*/
		psLastRefListEntry = psHwReg->psInterval->psUseDefChain->sList.psTail;
		psLastRef = IMG_CONTAINING_RECORD(psLastRefListEntry, PUSEDEF, sListEntry);
		uLastRef = UseDefToRef(psState, psLastRef);
		if (uLastRef < uRefToCheck)
		{
			return IMG_TRUE;
		}
		
	}
	return IMG_FALSE;
}

static
IMG_BOOL SpillCurrent(PINTERMEDIATE_STATE	psState,
					  PIREGALLOC_CONTEXT	psContext,
					  PIREGALLOC_BLOCK		psBlockState,
					  PCODEBLOCK			psBlock,
					  PINTERVAL				psInterval,
					  PSPILL_CANDIDATE		psSpillCandidate)
/*****************************************************************************
 FUNCTION	: SpillCurrent
    
 PURPOSE	: Check if an interval we are currently trying to assign an
			  internal register could be replaced by a temporary register.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator per-block state.
			  psBlock		- Current basic block.
			  psInterval	- Current interval.
			  psSpillCandidate
							- Returns information about the spill.
			  
 RETURNS	: TRUE if spilling the current interval is possible.
*****************************************************************************/
{
	PUSEDEF		psFirstRefAfterSpill;
	IMG_UINT32	uFirstRefAfterSpill;
	PUSEDEF		psLastRefBeforeSpill;
	IMG_INT32	iMergeExtraInstsBeforeCurrent;
	IMG_INT32	iMergeExtraInstsAfterCurrent;
	PINST		psIntervalDefInst;
	IMG_BOOL	bMergeAfterCurrent;
	IMG_BOOL	bRestoreAfterCurrent;
	IMG_INT32	iRestoreExtraInsts;

	psLastRefBeforeSpill = IMG_CONTAINING_RECORD(psInterval->psUseDefChain->sList.psHead, PUSEDEF, sListEntry);
	psFirstRefAfterSpill = IMG_CONTAINING_RECORD(psInterval->psUseDefChain->sList.psHead->psNext, PUSEDEF, sListEntry);
	uFirstRefAfterSpill = UseDefToRef(psState, psFirstRefAfterSpill);

	ASSERT(psLastRefBeforeSpill->eType == DEF_TYPE_INST);
	psIntervalDefInst = psLastRefBeforeSpill->u.psInst;

	MkSpillCandidate(psSpillCandidate);
	psSpillCandidate->psInterval = psInterval;
	psSpillCandidate->uHwReg = USC_UNDEF;
	psSpillCandidate->bMergeSave = IMG_FALSE;
	psSpillCandidate->bMergeRestore = IMG_FALSE;
	psSpillCandidate->iSpillCost = 0;
	psSpillCandidate->uNextRef = uFirstRefAfterSpill;

	/*
		Check if the instruction defining the interval can be modified to write directly to
		a non-internal register.
	*/
	if (!SubstIRegInterval(psState,
						   psContext,
						   psBlockState,
						   psBlock,
						   psInterval->psUseDefChain,
						   psBlock->psBody,
						   psIntervalDefInst,
						   IMG_TRUE /* bCheckOnly */,
						   &iMergeExtraInstsBeforeCurrent,
						   NULL /* puMergeExtraSecAttrs */,
						   0 /* uAlreadyUsedSecAttrs */))
	{
		return IMG_FALSE;
	}
	psSpillCandidate->bMergeSave = IMG_TRUE;
	psSpillCandidate->iSpillCost += iMergeExtraInstsBeforeCurrent;

	/*
		Check if all uses of the interval can be modified to read directly from non-internal
		registers.
	*/
	bMergeAfterCurrent = 
		SubstIRegInterval(psState,
						  psContext,
						  psBlockState,
						  psBlock,
						  psInterval->psUseDefChain,
						  psIntervalDefInst->psNext,
						  psBlock->psBodyTail,
						  IMG_TRUE /* bCheckOnly */,
						  &iMergeExtraInstsAfterCurrent,
						  NULL /* puMergeExtraSecAttrs */,
						  0 /* uAlreadyUsedSecAttrs */);

	/*
		Check if the last use of an interval currently in an internal register is between
		the current instruction after the first use of this interval.

		Otherwise we don't insert a RESTORE instruction for the current interval to avoid
		an endless loop.
	*/
	bRestoreAfterCurrent = IMG_FALSE;
	iRestoreExtraInsts = INT_MAX;
	if (KilledBeforeInst(psState, psContext, uFirstRefAfterSpill))
	{
		bRestoreAfterCurrent = IMG_TRUE;
		iRestoreExtraInsts = IREGALLOC_SPILL_RESTORE_COST;
	}

	if (!bMergeAfterCurrent && !bRestoreAfterCurrent)
	{
		return IMG_FALSE;
	}
	if (bRestoreAfterCurrent && (!bMergeAfterCurrent || iRestoreExtraInsts < iMergeExtraInstsAfterCurrent))
	{
		psSpillCandidate->bMergeRestore = IMG_FALSE;
		psSpillCandidate->iSpillCost += iRestoreExtraInsts;
	}
	else
	{
		psSpillCandidate->bMergeRestore = IMG_TRUE;
		psSpillCandidate->iSpillCost += iMergeExtraInstsAfterCurrent;
	}
	
	return IMG_TRUE;
}

static
IMG_VOID SpillInterval_MergeSave(PINTERMEDIATE_STATE	psState,
								 PIREGALLOC_CONTEXT		psContext,
								 PIREGALLOC_BLOCK		psBlockState,
								 PCODEBLOCK				psBlock,
								 PINTERVAL				psIntervalToSpill,
								 PINST					psSpillRangeEnd)
/*****************************************************************************
 FUNCTION	: SpillInterval_MergeSave
    
 PURPOSE	: Modify all references to an interval before a specified point
			  so the interval doesn't need to be assigned a unified store
			  register.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator block state.
			  psBlock			- Basic block whose instructions are to be
								assigned internal registers.
			  psIntervalToSpill	- Interval to modify.
			  psSpillRangeEnd	- Inclusive end of the range of references.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bMerged;

	bMerged = SubstIRegInterval(psState,
								psContext,
								psBlockState,
								psBlock,
								psIntervalToSpill->psUseDefChain,
								psBlock->psBody,
								psSpillRangeEnd,
								IMG_FALSE /* bCheckOnly */,
								NULL /* piMergeExtraInsts */,
								NULL /* puNumExtraSecAttrs */,
								0 /* uAlreadyUsedSecAttrs */);
	ASSERT(bMerged);
}

static
IMG_UINT32 GetChansUsedOverRange(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY psRangeStart, PUSC_LIST_ENTRY psRangeEnd)
/*****************************************************************************
 FUNCTION	: GetChansUsedOverRange
    
 PURPOSE	: Get the mask of channels used in all the references to an
			  intermediate register in a specified range.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psRangeStart		- First reference in the range.
			  psRangeEnd		- First reference after the end of the range.
			  
 RETURNS	: The mask of channels.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_INT32		uChansUsed;

	uChansUsed = 0;
	for (psListEntry = psRangeStart; psListEntry != psRangeEnd; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType != DEF_TYPE_INST)
		{
			uChansUsed |= GetUseChanMask(psState, psUse);
		}
	}
	return uChansUsed;
}

static
IMG_VOID SpillInterval(PINTERMEDIATE_STATE	psState,
					   PIREGALLOC_CONTEXT	psContext,
					   PIREGALLOC_BLOCK		psBlockState,
					   PCODEBLOCK			psBlock,
					   PSPILL_CANDIDATE		psSpillData,
					   PINST				psCurrentRefInst,
					   IMG_UINT32			uCurrentRef)
/*****************************************************************************
 FUNCTION	: SpillInterval
    
 PURPOSE	: Split an interval which is live at a point where too many internal
			  registers are live.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator block state.
			  psBlock			- Basic block whose instructions are to be
								assigned internal registers.
			  psSpillData		- Information about the interval to split.
			  psCurrentRefInst	- Split point.
			  uCurrentRef		
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF			psFirstRefAfterSpill;
	PUSEDEF			psLastRefBeforeSpill;
	PUSC_LIST_ENTRY	psLastRefBeforeSpill_ListEntry;
	PINTERVAL		psIntervalToSpill = psSpillData->psInterval;
	PUSC_LIST_ENTRY	psFirstRef = psIntervalToSpill->psUseDefChain->sList.psHead;
	IMG_UINT32		uChansUsedBeforeRestore;
	IMG_UINT32		uChansUsedAfterRestore;

	GetNextRefAfterCurrent(psState, psIntervalToSpill, uCurrentRef + 1, &psFirstRefAfterSpill);
	
	psLastRefBeforeSpill_ListEntry = psFirstRefAfterSpill->sListEntry.psPrev;
	psLastRefBeforeSpill = IMG_CONTAINING_RECORD(psLastRefBeforeSpill_ListEntry, PUSEDEF, sListEntry);

	/*
		Get a mask of the channels used from the spilled interval before the spill point.
	*/
	uChansUsedBeforeRestore = 
		GetChansUsedOverRange(psState, psIntervalToSpill->psUseDefChain->sList.psHead, &psFirstRefAfterSpill->sListEntry);

	/*
		Get a mask of the channels used in the spilled interval in future.
	*/
	uChansUsedAfterRestore = GetChansUsedOverRange(psState, &psFirstRefAfterSpill->sListEntry, NULL /* psRangeEnd */);

	if (psSpillData->bMergeSave)
	{
		PINST	psSpillRangeEnd;

		ASSERT(UseDefIsInstUseDef(psLastRefBeforeSpill));
		psSpillRangeEnd = psLastRefBeforeSpill->u.psInst;

		SpillInterval_MergeSave(psState,
								psContext,
								psBlockState,
								psBlock,
								psIntervalToSpill,
								psSpillRangeEnd);
	}
	else
	/*
		Don't save intervals which are defined by an instruction which copies from a unified store
		register into an internal register.
	*/
	if (!psIntervalToSpill->bDefinedByRestore)
	{
		IMG_BOOL	bSaveBefore;
		PINST		psInstToSaveAfter;
		PINST		psSaveInst = psIntervalToSpill->psSaveInst;

		/*
			If the last reference is as a source to the instruction for which we are trying to
			assign an internal register to one of the destinations then move the save instruction
			before the current instruction.
		*/
		ASSERT(psLastRefBeforeSpill->eType == DEF_TYPE_INST || psLastRefBeforeSpill->eType == USE_TYPE_SRC);
		psInstToSaveAfter = psLastRefBeforeSpill->u.psInst;
		if (psLastRefBeforeSpill->eType == DEF_TYPE_INST || psInstToSaveAfter != psCurrentRefInst)
		{
			bSaveBefore = IMG_FALSE;
		}
		else
		{
			ASSERT(psLastRefBeforeSpill->eType == USE_TYPE_SRC);
			bSaveBefore = IMG_TRUE;
		}

		if (psIntervalToSpill->psSaveInst != NULL)
		{
			PINST		psSaveInstL = psIntervalToSpill->psSaveInst;

			/*
				Also save any additional channels used between the old save point and
				the new.
			*/
			psSaveInstL->auLiveChansInDest[0] |= uChansUsedAfterRestore;

			/*
				If the first reference after the spill was at the save instruction then update the
				first reference.
			*/
			ASSERT(UseDefIsInstUseDef(psFirstRefAfterSpill));
			if (psFirstRefAfterSpill->u.psInst == psSaveInstL)
			{
				ASSERT(psFirstRefAfterSpill->eType == USE_TYPE_SRC);

				if (psFirstRefAfterSpill->sListEntry.psNext != NULL)
				{
					psFirstRefAfterSpill = IMG_CONTAINING_RECORD(psFirstRefAfterSpill->sListEntry.psNext, PUSEDEF, sListEntry);
				}
				else
				{
					psFirstRefAfterSpill = NULL;
				}
			}

			/*
				Move an existing instruction to save the contents of the interval immediately
				after the last reference before the spill point.

				*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*
				*																		   *
				*	This modifies the list of uses/defines for the interval we spilling.   *
				*																		   *
				*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*NOTE*
			*/
			RemoveInst(psState, psBlock, psSaveInstL);
			if (psInstToSaveAfter == psCurrentRefInst)
			{
				InsertInstBefore(psState, psBlock, psSaveInstL, psInstToSaveAfter);
			}
			else
			{
				InsertInstAfter(psState, psBlock, psSaveInstL, psInstToSaveAfter);
			}
		}
		else
		{
			PINTERVAL		psSaveInterval;

			/*
				Save the interval after the last reference before the current instruction.
			*/
			psSaveInterval = 
				SaveInterval(psState, 
							 psContext, 
							 psBlockState,
							 psBlock, 
							 psIntervalToSpill->psUseDefChain, 
							 psIntervalToSpill->eType,
							 psFirstRef, 
							 psLastRefBeforeSpill_ListEntry,
							 bSaveBefore);
			psSaveInst = psSaveInterval->psSaveInst;

			/*
				Move the information about the assigned hardware register to the new interval
				for the references to the spilled interval between the defining instruction and
				the spill point.
			*/
			CopyIntervalData(psSaveInterval, psIntervalToSpill);

			psSaveInterval->uHwReg = psIntervalToSpill->uHwReg;
			psIntervalToSpill->uHwReg = USC_UNDEF;
			
			if (psIntervalToSpill->eState == INTERVAL_STATE_ASSIGNED)
			{
				PINTERNAL_REGISTER	psInternalRegister;

				ASSERT(psSaveInterval->uHwReg != USC_UNDEF);
				psInternalRegister = &psContext->asHwReg[psSaveInterval->uHwReg];

				/*
					Replace the spilled interval in the list of intervals assigned an internal register.
				*/
				psSaveInterval->eState = INTERVAL_STATE_ASSIGNED;
				SafeListRemoveItem(&psBlockState->sAssignedIntervalList, &psIntervalToSpill->u.sAssignedIntervalListEntry);
				SafeListAppendItem(&psBlockState->sAssignedIntervalList, &psSaveInterval->u.sAssignedIntervalListEntry);

				/*
					Replace the spilled interval in the list of intervals assigned this specific internal register.
				*/
				SafeListInsertItemBeforePoint(&psInternalRegister->sHwRegIntervalList, 
											  &psSaveInterval->sHwRegIntervalListEntry,
											  &psIntervalToSpill->sHwRegIntervalListEntry);
				SafeListRemoveItem(&psInternalRegister->sHwRegIntervalList, 
								   &psIntervalToSpill->sHwRegIntervalListEntry);
			}
			else
			{
				ASSERT(psSaveInterval->uHwReg == USC_UNDEF);
				ASSERT(psIntervalToSpill->eState == INTERVAL_STATE_CURRENT);

				AddToUsedIntervalList(psState, psBlockState, psSaveInterval);
			}

			psIntervalToSpill->eState = INTERVAL_STATE_UNUSED;
		}

		ASSERT(psSaveInst != NULL);
	}
	else
	{
		PINST		psDefInst;
		IMG_UINT32	uDefDestIdx;

		psDefInst = UseDefGetDefInstFromChain(psIntervalToSpill->psUseDefChain, &uDefDestIdx);
		ASSERT(IsRestoreInst(psDefInst->eOpcode));
		ASSERT(uDefDestIdx == 0);

		/*
			If there are no uses left of the destination of a restore instruction then
			free the instruction.
		*/
		if (uChansUsedBeforeRestore == 0)
		{
			RemoveInst(psState, psDefInst->psBlock, psDefInst);
			FreeInst(psState, psDefInst);

			/*
				Drop the interval which is the destination of the restore.
			*/
			if (psIntervalToSpill->eState == INTERVAL_STATE_ASSIGNED)
			{
				DropInterval(psState, psContext, psBlockState, psIntervalToSpill);
			}
			else
			{
				ASSERT(psIntervalToSpill->eState == INTERVAL_STATE_CURRENT);
			}
		}
		else
		{
			/*
				Update the mask of channels used from the restore instruction.
			*/
			psDefInst->auLiveChansInDest[0] = uChansUsedBeforeRestore;
		}
	}

	if (psFirstRefAfterSpill != NULL)
	{
		PINTERVAL	psRestoreInterval;

		psRestoreInterval = 
			RestoreInterval(psState,
							psContext,
							psBlockState,
							psBlock,
							psIntervalToSpill,
							&psFirstRefAfterSpill->sListEntry,
							NULL /* psFirstRefNotInBlock */);

		if (psSpillData->bMergeRestore || psSpillData->bSpillPartialDest)
		{
			IMG_BOOL	bMerged;

			/*
				Rather than restoring the interval replace all of the uses
				after the split by a temporary register.
			*/
			bMerged = SubstIRegInterval(psState,
										psContext,
										psBlockState,
										psBlock,
										psRestoreInterval->psUseDefChain,
										psBlock->psBody,
										psBlock->psBodyTail,
										IMG_FALSE /* bCheckOnly */,
										NULL /* piMergeExtraInsts */,
										NULL /* puMergeExtraSecAttrs */,
										0 /* uAlreadyUsedSecAttrs */);
			ASSERT(bMerged);
		}
	}
}

static
IMG_BOOL AssignHardwareRegister(PINTERMEDIATE_STATE	psState, 
								PIREGALLOC_CONTEXT	psContext,
								PIREGALLOC_BLOCK	psBlockState, 
								PCODEBLOCK			psBlock,
								PINTERVAL			psInterval,
								IMG_UINT32			uCurrentRef,
								PINST				psCurrentRefInst,
								IMG_UINT32			uFirstUseRef,
								IMG_PUINT32			puAssignedHwReg)
/*****************************************************************************
 FUNCTION	: AssignHardwareRegister
    
 PURPOSE	: Assign an internal register to an interval.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psBlock			- Basic block we are assigning internal registers
								for.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator per-basic block
								state.
			  psBlock			- Current basic block.
			  psInterval		- Interval to assign for.
			  uCurrentRef		- Location of the first reference to the interval
								(which must be a define).
			  psCurrentRefInst	- Instruction defining the interval.
			  uFirstUseRef		- Location of the first use of the interval.
			  puAssignedHwReg	- Returns the assigned internal register.
			  
 RETURNS	: TRUE if the interval was spilled instead.
*****************************************************************************/
{
	SPILL_CANDIDATE		sBestCandidate;
	SPILL_CANDIDATE		sCurrentCandidate;
	IMG_UINT32			uOverlappingFixedRegisterMask;
	IMG_UINT32			uHwReg;

	/*
		Get the set of intervals which are assigned to fixed internal registers and which are live
		simultaneously with this interval.
	*/
	uOverlappingFixedRegisterMask = GetOverlappingFixedRegisterMask(psContext, &psBlockState->sUsedIntervalList, psInterval);

	MkSpillCandidate(&sBestCandidate);
	for (uHwReg = 0; uHwReg < psContext->uNumHwRegs; uHwReg++)
	{
		PINTERNAL_REGISTER	psHwReg = &psContext->asHwReg[uHwReg];
		PINTERVAL			psHwRegInterval = psHwReg->psInterval;
		PUSEDEF				psNextRef;

		MkSpillCandidate(&sCurrentCandidate);
		sCurrentCandidate.uHwReg = uHwReg;
		sCurrentCandidate.psInterval = psHwRegInterval;

		/*
			Assign only the fixed hardware register required for this interval.
		*/
		if ((psInterval->uFixedHwRegMask & (1U << uHwReg)) == 0)
		{
			continue;
		}

		/*
			Get the index of the next instruction which uses this interval.
		*/
		sCurrentCandidate.uNextRef = GetNextRefAfterCurrent(psState, psHwRegInterval, uCurrentRef, &psNextRef);

		if (psHwRegInterval != NULL)
		{
			IMG_INT32	iRestoreCost;
			PINST		psNextRefInst;
			IMG_UINT32	uMergeExtraSecAttrs;

			ASSERT(UseDefIsInstUseDef(psNextRef));
			psNextRefInst = psNextRef->u.psInst;

			/*
				Check if all the references to the interval after the current instruction can
				be replaced by temporary registers.
			*/
			sCurrentCandidate.bMergeRestore = IMG_FALSE;
			sCurrentCandidate.iSpillCost = IREGALLOC_SPILL_RESTORE_COST;
			if (SubstIRegInterval(psState,
								  psContext,
								  psBlockState,
								  psBlock,
								  psHwRegInterval->psUseDefChain,
								  psNextRefInst,
								  psBlock->psBodyTail,
								  IMG_TRUE /* bCheckOnly */,
								  &iRestoreCost,
								  &uMergeExtraSecAttrs,
								  0 /* uAlreadyUsedSecAttrs */))
			{
				if (iRestoreCost < sCurrentCandidate.iSpillCost)
				{
					sCurrentCandidate.iSpillCost = iRestoreCost;
					sCurrentCandidate.bMergeRestore = IMG_TRUE;
				}
			}

			/*
				Get the cost of saving the contents of this internal register.
			*/
			sCurrentCandidate.iSpillCost += 
				GetSaveCost(psState,
				 		    psContext,
							psBlockState,
							psBlock,
							psHwRegInterval,
							psNextRefInst->psPrev,
							&sCurrentCandidate.bMergeSave,
							uMergeExtraSecAttrs);
		}

		/*
			Skip internal registers which are a destination of the current instruction or which are next
			used at the instruction which is the next use of the interval to be assigned an
			internal register.
		*/
		ASSERT(sCurrentCandidate.uNextRef >= uCurrentRef);
		if (!sCurrentCandidate.bMergeRestore && sCurrentCandidate.uNextRef == uFirstUseRef)
		{
			continue;
		}

		/*
			Check if using this internal register would mean a spill later on because another, overlapping
			interval is always assigned to the same internal register.
		*/
		sCurrentCandidate.bIsFixed = IMG_FALSE;
		if ((uOverlappingFixedRegisterMask & (1U << uHwReg)) != 0)
		{
			sCurrentCandidate.bIsFixed = IMG_TRUE;
		}

		/*
			Compare this candidate for spilling to the existing best candidate.
		*/
		if (IsBetterSpillCandidate(&sBestCandidate, &sCurrentCandidate))
		{
			sBestCandidate = sCurrentCandidate;
		}
	}

	/*
		If we can't spill any registers, this is possible for an instruction which has both
		internal register sources and internal register partial destinations, then get the
		cost of spilling by moving the partial destination updates to separate instructions.
	*/
	if (sBestCandidate.uHwReg == USC_UNDEF)
	{
		ASSERT(psInterval->uFixedHwRegMask == psContext->uAllHwRegMask);

		GetSpillPartialDestCost(psState,
								psContext,
								psBlockState,
								psBlock,
								psInterval,
								uCurrentRef,
								uFirstUseRef,
								&sBestCandidate);
	}

	/*
		Check if replacing the current interval by a temporary register is possible
		and would be better than spilling an internal register already in use.
	*/
	if (sBestCandidate.iSpillCost > 0 && psInterval->uFixedHwRegMask == psContext->uAllHwRegMask)
	{
		if (SpillCurrent(psState,
						 psContext,
						 psBlockState,
						 psBlock,
						 psInterval,
						 &sCurrentCandidate))
		{
			if (IsBetterSpillCandidate(&sBestCandidate, &sCurrentCandidate))
			{
				sBestCandidate = sCurrentCandidate;
			}
		}
	}

	/*
		Convert a use of the spilled interval as a partial destination to a use
		as a source in a separate move instruction.
	*/
	if (sBestCandidate.bSpillPartialDest)
	{
		SpillPartialDest(psState,
						 psContext,
						 psBlockState,
						 sBestCandidate.psInterval,
						 uCurrentRef,
						 uFirstUseRef);
	}

	/*
		Spill either the interval currently using the chosen internal register or the
		current interval.
	*/
	if (sBestCandidate.psInterval != NULL)
	{
		SpillInterval(psState,
					  psContext,
					  psBlockState,
					  psBlock,
					  &sBestCandidate,
					  psCurrentRefInst,
					  uCurrentRef);

		/*
			Mark the spilled internal register as unused.
		*/
		if (sBestCandidate.uHwReg != USC_UNDEF)
		{
			ASSERT(sBestCandidate.uHwReg < psContext->uNumHwRegs);
			psContext->asHwReg[sBestCandidate.uHwReg].psInterval = NULL;
		}
	}

	*puAssignedHwReg = sBestCandidate.uHwReg;
	if (psInterval->eState == INTERVAL_STATE_UNUSED)
	{
		return IMG_TRUE;
	}

	if (sBestCandidate.psInterval == psInterval)
	{
		ASSERT(sBestCandidate.uHwReg == USC_UNDEF);
		return IMG_TRUE;
	}
	else
	{
		ASSERT(sBestCandidate.uHwReg != USC_UNDEF);
		return IMG_FALSE;
	}
}

static
IMG_VOID ProcessLiveInOutInterval(PINTERMEDIATE_STATE	psState, 
								  PCODEBLOCK			psBlock, 
								  PIREGALLOC_CONTEXT	psContext,
								  PIREGALLOC_BLOCK		psBlockState,
								  PINTERVAL				psInterval,
								  PUSC_LIST_ENTRY		psFirstRefInBlock,
								  PUSC_LIST_ENTRY		psLastRefInBlock)
/*****************************************************************************
 FUNCTION	: ProcessLiveInOutInterval
    
 PURPOSE	: Fix an interval which might be live into or and of the current
			  basic block. Internal registers can't be live across flow control
			  so we must split the interval.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psBlock			- Basic block we are assigning internal registers
								for.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator per-basic block
								state.
			  psInterval		- Interval to fix.
			  psFirstRefInBlock	- Range of references to the interval which
			  psLastRefInBlock	are in instructions in the current basic block.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bDefinedInBlock;
	PINST		psDefInst;
	IMG_BOOL	bUsedInDifferentBlock;
	IMG_BOOL	bNewInterval = IMG_FALSE;

	/*
		Skip references to the interval in delta instructions.
	*/
	for (;;)
	{
		PUSEDEF	psFirstUse;

		psFirstUse = IMG_CONTAINING_RECORD(psFirstRefInBlock, PUSEDEF, sListEntry);
		ASSERT(UseDefIsInstUseDef(psFirstUse));
		if (!(psFirstUse->u.psInst->eOpcode == IDELTA || psFirstUse->u.psInst->eOpcode == IFEEDBACKDRIVEREPILOG))
		{
			break;
		}
		if (psFirstRefInBlock == psLastRefInBlock)
		{
			return;
		}
		psFirstRefInBlock = psFirstRefInBlock->psNext;
	}

	/*
		Get the instruction defining the interval.
	*/
	psDefInst = UseDefGetDefInstFromChain(psInterval->psUseDefChain, NULL /* puDestIdx */);
	
	/*
		Check if the interval is defined inside the current block.
	*/
	bDefinedInBlock = IMG_FALSE;
	if (psDefInst != NULL && psDefInst->psBlock == psBlock && psDefInst->eOpcode != IDELTA)
	{
		bDefinedInBlock = IMG_TRUE;
	}

	/*
		Check if the interval is used in a block other than the current.
	*/
	bUsedInDifferentBlock = IsUsedInDifferentBlock(psInterval->psUseDefChain, psBlock);

	/*
		Check for an interval which is live into the current basic block.
	*/
	if (!bDefinedInBlock)
	{
		/*
			Insert an instruction to restore the interval into a newly allocated intermediate
			register before the first reference and replace all references in the current
			block by the new register.
		*/
		RestoreInterval(psState, 
						psContext, 
						psBlockState, 
						psBlock,
						psInterval,
						psFirstRefInBlock, 
						psLastRefInBlock->psNext);

		bNewInterval = IMG_TRUE;
	}

	/*
		Check for an interval which is live out of the current basic block.
	*/
	if (bDefinedInBlock && bUsedInDifferentBlock)
	{
		PINTERVAL	psSaveInterval;

		/*
			Insert an instruction to copy a newly allocated intermediate register into the interval
			register after the last reference and replace all references in the current
			block by the new register.
		*/
		psSaveInterval = 
			SaveInterval(psState, 
						 psContext, 
						 psBlockState,
						 psBlock, 
						 psInterval->psUseDefChain, 
						 psInterval->eType,
						 psFirstRefInBlock, 
						 psLastRefInBlock,
						 IMG_FALSE /* bSaveBefore */);
		AddToUsedIntervalList(psState, psBlockState, psSaveInterval);
		CopyIntervalData(psSaveInterval, psInterval);

		bNewInterval = IMG_TRUE;
	}

	/*
		If we didn't need to restore/save the original interval then add it to the list of intervals
		to be assigned internal registers in the current block.
	*/
	if (!bNewInterval)
	{
		/*
			Add the interval to the list of those used in the current block.
		*/
		AddToUsedIntervalList(psState, psBlockState, psInterval);
	}
}

static
IMG_VOID KillInternalRegisters(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, IMG_UINT32 uCurrentRef)
/*****************************************************************************
 FUNCTION	: KillInternalRegisters
    
 PURPOSE	: Update information about which internal registers are currently
			  used.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psContext			- Internal register allocator state.
			  uCurrentRef		- Point to update the internal register
								information reflect.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uHwReg;

	for (uHwReg = 0; uHwReg < psContext->uNumHwRegs; uHwReg++)
	{
		PINTERNAL_REGISTER	psHwReg = &psContext->asHwReg[uHwReg];
		PUSC_LIST_ENTRY		psLastRefListEntry;
		PUSEDEF				psLastRef;
		IMG_UINT32			uLastRef;

		if (psHwReg->psInterval == NULL)
		{
			continue;
		}

		/*
			Get the last reference to the interval in this internal register.
		*/
		psLastRefListEntry = psHwReg->psInterval->psUseDefChain->sList.psTail;
		psLastRef = IMG_CONTAINING_RECORD(psLastRefListEntry, PUSEDEF, sListEntry);

		/*
			Is the last reference before the current reference?
		*/
		uLastRef = UseDefToRef(psState, psLastRef);
		if (uLastRef < uCurrentRef)
		{
			/* Mark the internal register as unused. */
			psHwReg->psInterval = NULL;
		}
	}
}

static
IMG_BOOL SpillIntervalForDeschedule(PINTERMEDIATE_STATE psState, 
									PIREGALLOC_CONTEXT	psContext,
									PIREGALLOC_BLOCK	psBlockState,
									PCODEBLOCK			psBlock,
									PINST				psDeschedInst, 
									PINTERVAL			psInterval,
									PUSC_LIST_ENTRY*	ppsNextListEntry)
/*****************************************************************************
 FUNCTION	: SpillIntervalForDeschedule
    
 PURPOSE	: Fix an interval which is live across a descheduling instruction
			  by splitting it into multiple subintervals.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator block state.
			  psBlock		- Basic block whose instructions are to be
							assigned internal registers.
			  psDeschedInst	- Descheduling instruction.
			  psInterval	- Interval.
			  ppsNextListEntry
							- Next interval to be processed. This might be
							updated if a new interval was inserted after
							the original interval but before any other
							interval.

 RETURNS	: TRUE if the interval was split.
*****************************************************************************/
{
	IMG_BOOL		bDefinedByRestore;
	PUSC_LIST_ENTRY	psLastRefBeforeDesched;
	PUSC_LIST_ENTRY	psFirstRefAfterDesched;
	PUSC_LIST_ENTRY	psFirstRefInDeschedInst;
	PUSC_LIST_ENTRY	psLastRefInDeschedInst;
	PUSC_LIST_ENTRY	psUseListEntry;
	PINST			psSaveInst;
	PINTERVAL		psSaveBeforeInterval;
	PUSC_LIST		psIntervalUseDefList = &psInterval->psUseDefChain->sList;
	PINST			psDefInst;
	IMG_BOOL		bDropOrigInterval = IMG_TRUE;
	IMG_BOOL		bDeschedBefore = IsDeschedBeforeInst(psState, psDeschedInst);
	IMG_BOOL		bDeschedAfter = IsDeschedAfterInst(psDeschedInst);
	IMG_BOOL		bSaveBefore;

	/*
		Find the last reference to the internal before the descheduling instruction and
		the first reference after.
	*/
	psLastRefBeforeDesched = NULL;
	psFirstRefAfterDesched = NULL;
	psFirstRefInDeschedInst = NULL;
	psLastRefInDeschedInst = NULL;
	bSaveBefore = IMG_FALSE;
	for (psUseListEntry = psIntervalUseDefList->psHead;
		 psUseListEntry != NULL;
		 psUseListEntry = psUseListEntry->psNext)
	{
		PUSEDEF		psUseDef;
		PINST		psUseDefInst;
		IMG_BOOL	bBefore;

		psUseDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		psUseDefInst = UseDefGetInst(psUseDef);
		ASSERT(psUseDefInst != NULL);
		ASSERT(psUseDefInst->psBlock == psBlock);

		bBefore = IMG_FALSE;
		if (psUseDefInst->uBlockIndex < psDeschedInst->uBlockIndex)
		{
			bBefore = IMG_TRUE;
		}
		/*
			Sources to an instruction which only deschedules after it executes can still be
			assigned internal registers.
		*/
		if (
				psUseDefInst->uBlockIndex == psDeschedInst->uBlockIndex &&
				psUseDef->eType == USE_TYPE_SRC &&
				!bDeschedBefore
		   )
		{
			bSaveBefore = IMG_TRUE;
			bBefore = IMG_TRUE;
		}

		if (bBefore)
		{
			psLastRefBeforeDesched = psUseListEntry;
		}
		else if (psUseDefInst->uBlockIndex == psDeschedInst->uBlockIndex)
		{
			if (psFirstRefInDeschedInst == NULL)
			{
				psFirstRefInDeschedInst = psUseListEntry;
			}
			psLastRefInDeschedInst = psUseListEntry;
		}
		else
		{
			/* psUseDefInst->uBlockIndex > psDeschedInst->uBlockIndex */
			psFirstRefAfterDesched = psUseListEntry;
			break;
		}
	}

	/*
		If the instruction only deschedules after its execution and there are no references to the
		interval after the instruction then there is no need to spill.
	*/
	if (!IsDeschedBeforeInst(psState, psDeschedInst) && psFirstRefAfterDesched == NULL)
	{
		return IMG_FALSE;
	}

	/*
		Get the instruction defining the interval.
	*/
	psDefInst = UseDefGetDefInstFromChain(psInterval->psUseDefChain, NULL /* puDestIdx */);
	ASSERT(psDefInst != NULL);
	ASSERT(psDefInst->psBlock == psBlock);

	/*
		If the instruction only deschedules before its execution and it defines the
		interval then there is no need to spill.
	*/
	if (
			psDefInst == psDeschedInst && 
			(
				#if defined(OUTPUT_USPBIN)
				psDeschedInst->eOpcode == ISMPUNPACK_USP ||
				#endif /* defined(OUTPUT_USPBIN) */
				!bDeschedAfter
			)
		)
	{
		ASSERT(psLastRefBeforeDesched == NULL);
		return IMG_FALSE;
	}

	ASSERT(psInterval->eType != INTERVAL_TYPE_EXISTING_IREG);

	/*
		Is the interval a copy of another interval?
	*/
	bDefinedByRestore = IMG_FALSE;
	if (IsRestoreInst(psDefInst->eOpcode))
	{
		bDefinedByRestore = IMG_TRUE;
	}
			
	/*
		Check if the interval finishes with a save of the data.
	*/
	psSaveInst = psInterval->psSaveInst;

	ASSERT(!(bDefinedByRestore && psSaveInst != NULL));
			
	/*
		Save the contents of the interval before the descheduling instruction. 

			INST		I <-
			INST		..., I, ...
			DESCHED
				->
			INST		J <-
			INST		..., J, ...
			SAVE		I <- J
			DESCHED
	*/
	if (psLastRefBeforeDesched != NULL)
	{
		if (psSaveInst != NULL)
		{
			PUSEDEF			psRefToSaveAt;
			PUSC_LIST_ENTRY	psListEntry;
			PINST			psInstToSaveAt;

			/*
				Add all the channels used in or after the descheduling instruction to the mask of
				channels to save.
			*/
			for (psListEntry = psLastRefBeforeDesched->psNext; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

				if (psSaveInst->auLiveChansInDest[0] == USC_ALL_CHAN_MASK)
				{
					break;
				}
				psSaveInst->auLiveChansInDest[0] |= GetUseChanMask(psState, psUse);
			}

			/*
				Update the pointer to the first reference after the deschedule if it
				points to the save instruction.
			*/
			if (&psSaveInst->asArgUseDef[0].sUseDef.sListEntry == psFirstRefAfterDesched)
			{
				psFirstRefAfterDesched = psFirstRefAfterDesched->psNext;
			}

			/*
				Move an existing instruction to save the contents of the interval immediately
				after the last reference before the descheduling instruction; or immediate
				before if the last reference is in the descheduling instruction itself.
			*/
			psRefToSaveAt = IMG_CONTAINING_RECORD(psLastRefBeforeDesched, PUSEDEF, sListEntry);
			ASSERT(UseDefIsInstUseDef(psRefToSaveAt));
			psInstToSaveAt = psRefToSaveAt->u.psInst;

			RemoveInst(psState, psBlock, psSaveInst);
			if (bSaveBefore)
			{
				InsertInstBefore(psState, psBlock, psSaveInst, psInstToSaveAt);
			}
			else
			{
				InsertInstAfter(psState, psBlock, psSaveInst, psInstToSaveAt);
			}
			psSaveInst = NULL;

			bDropOrigInterval = IMG_FALSE;
		}
		else
		{
			if (!bDefinedByRestore)
			{
				/*
					Allocate a new intermediate register, replace all references to the original register
					before the descheduling instruction by the new register and insert an instruction
					to copy from the new register to the original register after the last reference before
					the descheduling instruction.

					The new intermediate register will be assigned to an internal register while the original
					register will be kept in the unified store.
				*/
				psSaveBeforeInterval = 
					SaveInterval(psState, 
								 psContext,
								 psBlockState,
								 psBlock, 
								 psInterval->psUseDefChain, 
								 psInterval->eType,
								 psIntervalUseDefList->psHead, 
								 psLastRefBeforeDesched,
								 bSaveBefore);
				AddToUsedIntervalList(psState, psBlockState, psSaveBeforeInterval);
				CopyIntervalData(psSaveBeforeInterval, psInterval);
			}
			else
			{
				bDropOrigInterval = IMG_FALSE;
			}
		}	
	}
			
	/*
		Process any references to the interval in the descheduling instruction itself.
	*/
	if (psFirstRefInDeschedInst != NULL)
	{
		psUseListEntry = psFirstRefInDeschedInst;
		for (;;)
		{
			PUSEDEF		psUseDef;
			PUSC_LIST_ENTRY	psNextUseListEntry;

			psNextUseListEntry = psUseListEntry->psNext;
			psUseDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			if (psUseDef->eType == DEF_TYPE_INST)
			{
				ASSERT(psLastRefBeforeDesched == NULL);
				ASSERT(bDeschedAfter);

				/*
					If an internal register can't be the destination of this instruction and the 
					result of the instruction is already copied into a unified store register: then replace the
					destination of this instruction by the destination of the copy.
				*/
				if (psSaveInst)
				{
					PUSEDEF	psFirstUseAfterDesched;

					ASSERT((psSaveInst->auLiveChansInDest[0] & ~psDeschedInst->auLiveChansInDest[psUseDef->uLocation]) == 0);

					/*
						Update the first use after the descheduling instruction if it points to the
						save instruction.
					*/
					psFirstUseAfterDesched = IMG_CONTAINING_RECORD(psFirstRefAfterDesched, PUSEDEF, sListEntry);
					ASSERT(UseDefIsInstUseDef(psFirstUseAfterDesched));
					if (psFirstUseAfterDesched->u.psInst == psSaveInst)
					{
						psFirstRefAfterDesched = psFirstRefAfterDesched->psNext;
					}

					/*
						Move the destination from the save instruction to this instruction.
					*/
					MoveDest(psState, psDeschedInst, psUseDef->uLocation, psSaveInst, 0 /* uMoveFromDestIdx */);

					/*
						Drop the save instruction.
					*/
					RemoveInst(psState, psBlock, psSaveInst);
					FreeInst(psState, psSaveInst);
						
					psSaveInst = NULL;
				}
			}
			else
			{
				ASSERT(psUseDef->eType == USE_TYPE_SRC);
				ASSERT(bDeschedBefore);

				/*
					If there is a deschedule before the instruction executes then an internal
					register can never be a source. So set the source to the original temporary (before
					we started inserting saves/restores) which won't now be assigned to an internal register.
				*/
				if (psInterval->psOrigTemp != NULL)
				{
					SetSrc(psState, 
						   psDeschedInst, 
						   psUseDef->uLocation, 
						   psInterval->psOrigTemp->uType,
						   psInterval->psOrigTemp->uNumber,
						   psInterval->psOrigTemp->eFmt);
				}
			}

			if (psUseListEntry == psLastRefInDeschedInst)
			{
				break;
			}
			psUseListEntry = psNextUseListEntry;
		}
	}

	/*
		Remove the original interval (which included the descheduling point) from the list of
		intervals to be assigned internal registers.
	*/
	if (bDropOrigInterval)
	{
		DropInterval(psState, psContext, psBlockState, psInterval);
	}

	/*
		...and restore it afterwards.
	*/
	if (psFirstRefAfterDesched != NULL)
	{
		PINTERVAL	psRestoreAfterInterval;

		ASSERT(psSaveInst == NULL);

		psRestoreAfterInterval = 
			RestoreInterval(psState, 
							psContext, 
							psBlockState, 
							psBlock, 
							psInterval,
							psFirstRefAfterDesched, 
							NULL /* psFirstRefNotInBlock */);

		/*
			Make sure to check the references after the restore in case they overlap
			with any other descheduling instructions.
		*/
		if (psRestoreAfterInterval->u.sUsedIntervalListEntry.psNext == (*ppsNextListEntry))
		{
			(*ppsNextListEntry) = &psRestoreAfterInterval->u.sUsedIntervalListEntry;
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID AddFixedIRegs(PINTERMEDIATE_STATE	psState, 
					   PIREGALLOC_CONTEXT	psContext, 
					   PIREGALLOC_BLOCK		psBlockState, 
					   PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: AddFixedIRegs
    
 PURPOSE	: Replace existing uses/defines of internal registers in a
			  basic block by temporary registers.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator state for the
							basic block.
			  psBlock		- Basic block to replace within.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uHwReg;

	for (uHwReg = 0; uHwReg < psContext->uNumHwRegs; uHwReg++)
	{
		PUSEDEF_CHAIN	psFixedIRegUseDef = psBlock->apsIRegUseDef[uHwReg];

		if (psFixedIRegUseDef != NULL)
		{
			PUSC_LIST_ENTRY	psListEntry;
			PUSC_LIST_ENTRY	psNextListEntry;
			ARG				sReplacementTemp;

			InitInstArg(&sReplacementTemp);
			for (psListEntry = psFixedIRegUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
			{
				PUSEDEF	psUse;
				IMG_BOOL bDef;

				psNextListEntry = psListEntry->psNext;
				psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

				bDef = IMG_FALSE;
				if (psUse->eType == DEF_TYPE_INST)
				{
					/*
						Allocate a replacement temporary register.
					*/
					bDef = IMG_TRUE;
					MakeNewTempArg(psState, UF_REGFORMAT_F32, &sReplacementTemp);
				}

				/*
					Replace the use/define of the internal register.
				*/
				ASSERT(sReplacementTemp.uType != USC_REGTYPE_DUMMY);
				UseDefSubstUse(psState, psFixedIRegUseDef, psUse, &sReplacementTemp);

				if (bDef)
				{
					PINTERVAL		psIRegInterval;
					PUSEDEF_CHAIN	psReplacementTemp;

					/*
						Mark the temporary register as needing to be assigned the fixed internal register
						we are replacing.
					*/
					psReplacementTemp = sReplacementTemp.psRegister->psUseDefChain;
					psIRegInterval = 
						AddNewBlockInterval(psState, psContext, psBlockState, psReplacementTemp, psReplacementTemp);
					psIRegInterval->uFixedHwRegMask = 1U << uHwReg;
					psIRegInterval->eType = INTERVAL_TYPE_EXISTING_IREG;

					AddToUsedIntervalList(psState, psBlockState, psIRegInterval);
				}
			}
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
PINST CreateVLOADInst(PINTERMEDIATE_STATE	psState, 
					  PIREGALLOC_CONTEXT	psContext,
					  PIREGALLOC_BLOCK		psBlockState,
					  PINST					psInsertBeforeInst,
					  UF_REGFORMAT			eSrcFormat, 
					  IMG_BOOL				bSrcVectorWide, 
					  IMG_UINT32			uSrcVectorLength, 
					  PCARG					asSrcVector)
/*****************************************************************************
 FUNCTION	: CreateVLOADInst
    
 PURPOSE	: Create an instruction loading vector data into an intermediate
			  register and prepare the destination to be assigned an
			  internal register.

 PARAMETERS	: psState			- Compiler intermediate state.
			  psContext			- Internal register allocator state.
			  psBlockState		- Internal register allocator state for the
								current basic block.
			  psBlock			- Basic block whose instructions are to be
								assigned internal registers.
			  psInsertBeforeInst
								- Point to insert the new instruction.
			  eSrcFormat		- Format of the loaded data.
			  bSrcVectorWide	- Data to load.
			  uSrcVectorLength
			  asSrcVector
			  
 RETURNS	: A pointer to the new instruction.
*****************************************************************************/
{
	ARG				sNewTemp;
	PUSEDEF_CHAIN	psNewTempUseDef;
	PINTERVAL		psNewTempInterval;
	PINST			psVLoadInst;

	/*
		Allocate a new temporary.
	*/
	MakeNewTempArg(psState, eSrcFormat, &sNewTemp);

	/*
		Allocate an instruction to load the existing source argument.
	*/
	psVLoadInst = AllocateInst(psState, NULL /* psSrcLineInst */);

	/*
		Load the existing source argument into the new temporary.
	*/
	SetOpcode(psState, psVLoadInst, IVLOAD);

	SetDestFromArg(psState, psVLoadInst, 0 /* uDestIdx */, &sNewTemp);
	psVLoadInst->auDestMask[0] = 0;
	psVLoadInst->auLiveChansInDest[0] = 0;

	SetVectorSource(psState, psVLoadInst, 0 /* uSrcIdx */, bSrcVectorWide, uSrcVectorLength, asSrcVector);	
	psVLoadInst->u.psVec->aeSrcFmt[0] = eSrcFormat;
	psVLoadInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	InsertInstBefore(psState, psInsertBeforeInst->psBlock, psVLoadInst, psInsertBeforeInst);

	/*
		Create an interval data structure for the new temporary.
	*/
	psNewTempUseDef = sNewTemp.psRegister->psUseDefChain;
	psNewTempInterval = AddNewBlockInterval(psState, psContext, psBlockState, psNewTempUseDef, NULL /* psOrigTemp */);
	psNewTempInterval->bOrigVectorWasWide = bSrcVectorWide;
	psNewTempInterval->uOrigVectorLength = uSrcVectorLength;
	memcpy(psNewTempInterval->asOrigVector, asSrcVector, sizeof(asSrcVector[0]) * uSrcVectorLength);
	psNewTempInterval->bDefinedByRestore = IMG_TRUE;

	/*
		Add to the list of intervals to be assigned internal registers in this block.
	*/
	AddToUsedIntervalList(psState, psBlockState, psNewTempInterval);

	return psVLoadInst;
}

static
IMG_VOID CreateIntervalsForNonVectorSources(PINTERMEDIATE_STATE		psState, 
											PIREGALLOC_CONTEXT		psContext,
											PIREGALLOC_BLOCK		psBlockState,
											PCODEBLOCK				psBlock)
/*****************************************************************************
 FUNCTION	: CreateIntervalsForNonVectorSources
    
 PURPOSE	: Create intervals for vectors of scalar registers which might
			  need to be loaded into an internal register.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Internal register allocator state for the
							current basic block.
			  psBlock		- Basic block whose instructions are to be
							assigned internal registers.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST					psInst;
	PREGISTER_VECTOR_MAP	psWideLoadIntervalMap;

	/*
		For vector instructions with sources which aren't a single intermediate
		register (e.g. a vector of hardware constants) create a separate VLOAD
		instruction defining a new intermediate register then replace the 
		existing source by the new intermediate register.

		Then either the new intermediate register can be mapping to an
		internal register or the load source can be merged back into the
		instruction where the hardware supports accessing a non-internal
		register directly.
	*/

	psWideLoadIntervalMap = IntermediateRegisterVectorMapCreate(psState);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uSlot;

		/*
			Skip instructions not using vector sources.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) == 0)
		{
			continue;
		}

		/*
			These instructions can't have internal register sources.
		*/
		if (psInst->eOpcode == IVDSX || psInst->eOpcode == IVDSY || psInst->eOpcode == IVPCKS16FLT_EXP)
		{
			continue;
		}

		for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
		{
			PARG			psGPISrc = &psInst->asArg[uSlot * SOURCE_ARGUMENTS_PER_VECTOR];
			PINST			psVLoadInst;
			UF_REGFORMAT	eSrcFormat = psInst->u.psVec->aeSrcFmt[uSlot];
			PARG			asSrcVector;
			IMG_UINT32		uSrcVectorLength;
			IMG_BOOL		bSrcVectorWide;
			IMG_BOOL		bMultipleDefines;
			IMG_UINT32		uArg;
			IMG_UINT32		uChansFromSrc;

			/*
				Skip sources for which we have already created intervals.
			*/
			if (psGPISrc->uType == USEASM_REGTYPE_TEMP)
			{
				continue;
			}

			if (!(eSrcFormat == UF_REGFORMAT_F16 || eSrcFormat == UF_REGFORMAT_F32))
			{
				continue;
			}
			
			if (psGPISrc->uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				uSrcVectorLength = 1;
				asSrcVector = psGPISrc;
				bSrcVectorWide = IMG_TRUE;
			}
			else
			{
				if (eSrcFormat == UF_REGFORMAT_F32)
				{
					uSrcVectorLength = SCALAR_REGISTERS_PER_F32_VECTOR;
				}
				else
				{
					uSrcVectorLength = SCALAR_REGISTERS_PER_F16_VECTOR;
				}
				asSrcVector = psGPISrc + 1;
				bSrcVectorWide = IMG_FALSE;
			}

			/*	
				Check if the source vector includes intermediate registers which
				can be defined more than once.
			*/
			bMultipleDefines = IMG_FALSE;
			for (uArg = 0; uArg < uSrcVectorLength; uArg++)
			{
				if (IsNonSSAArgument(psState, asSrcVector + uArg))
				{
					bMultipleDefines = IMG_TRUE;
					break;
				}
			}

			/*
				Check if we already created a VLOAD instruction for this source.
			*/
			if (bMultipleDefines)
			{
				psVLoadInst = NULL;
			}
			else
			{
				psVLoadInst = IntermediateRegisterVectorMapGet(psWideLoadIntervalMap, uSrcVectorLength, asSrcVector);
			}

			if (psVLoadInst == NULL)
			{
				/*
					Create an instruction to load the source vector into a new temporary.
				*/
				psVLoadInst = 
					CreateVLOADInst(psState, 
									psContext,
									psBlockState,
									psInst,
									eSrcFormat, 
									bSrcVectorWide, 
									uSrcVectorLength, 
									asSrcVector);
				

				/*
					Update the maps from vectors of registers to an instruction loading that vector.
				*/
				if (!bMultipleDefines)
				{
					IntermediateRegisterVectorMapSet(psState, psWideLoadIntervalMap, uSrcVectorLength, asSrcVector, psVLoadInst);
				}
			}
			else
			{
				ASSERT(psVLoadInst->eOpcode == IVLOAD);
				ASSERT(psVLoadInst->uDestCount == 1);
				ASSERT(psVLoadInst->asDest[0].uType == USEASM_REGTYPE_TEMP);
			}

			/*
				Replace the existing source argument by the result of the load instruction.
			*/
			uChansFromSrc = GetLiveChansInArg(psState, psInst, uSlot * SOURCE_ARGUMENTS_PER_VECTOR);
			psVLoadInst->auDestMask[0] |= uChansFromSrc;
			psVLoadInst->auLiveChansInDest[0] |= uChansFromSrc;
			SetupTempVecSource(psState, psInst, uSlot, psVLoadInst->asDest[0].uNumber, eSrcFormat);
		}
	}

	IntermediateRegisterVectorMapDestroy(psState, psWideLoadIntervalMap);
	psWideLoadIntervalMap = NULL;
}

static
IMG_VOID SubstIRegMove(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PIREGALLOC_BLOCK psBlockState, PINST psMoveInst)
/*****************************************************************************
 FUNCTION	: SubstIRegMove
    
 PURPOSE	: Try to replace the destination of a vector move instruction by
			  the source.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlockState	- Allocator state for the current block.
			  psMoveInst	- Move instruction to try and substitute.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG					psSrc;
	PINTERVAL				psSrcInterval;
	PARG					psDest;
	PINTERVAL				psDestInterval;
	SUBSTSWIZZLE_CONTEXT	sSubstContext;

	if (psMoveInst->eOpcode != IVMOV)
	{
		return;
	}

	/*
		Check the move instruction isn't applying a source modifier.
	*/
	if (psMoveInst->u.psVec->asSrcMod[0].bNegate || psMoveInst->u.psVec->asSrcMod[0].bAbs)
	{
		return;
	}

	/*
		Check the move instruction is not predicated.
	*/
	if (!NoPredicate(psState, psMoveInst))
	{
		return;
	}

	psSrc = &psMoveInst->asArg[0];
	if (psSrc->uType != USEASM_REGTYPE_TEMP)
	{
		return;
	}

	psSrcInterval = GetIntervalByArg(psContext, psSrc);

	psDest = &psMoveInst->asDest[0];
	if (psDest->uType != USEASM_REGTYPE_TEMP)
	{
		return;
	}

	psDestInterval = GetIntervalByArg(psContext, psDest);

	sSubstContext.uLastSubstPoint = USC_UNDEF;
	sSubstContext.psSubstBlock = NULL;
	sSubstContext.bSourceIsIReg = IMG_FALSE;
	sSubstContext.eSourceFormat = psMoveInst->u.psVec->aeSrcFmt[0];
	sSubstContext.psAllocContext = psContext;

	/*
		Get the move instruction's source swizzle taking into account we might be
		selecting only the top-half of a vector source/destination.
	*/
	sSubstContext.uSubstSwizzle = GetEffectiveVMOVSwizzle(psMoveInst);

	/*
		Check that the move instruction is completely defining its destination.
	*/
	if (psMoveInst->auLiveChansInDest[0] != psMoveInst->auDestMask[0])
	{
		/*
			Check if both the move source and the source for unwritten channels in the destination are the
			same intermediate register.
		*/
		if (psMoveInst->apsOldDest[0] != NULL && EqualArgs(psMoveInst->apsOldDest[0], &psMoveInst->asArg[0]))
		{
			IMG_UINT32	uChan;

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((psMoveInst->auDestMask[0] & (1 << uChan)) == 0)
				{
					sSubstContext.uSubstSwizzle = SetChan(sSubstContext.uSubstSwizzle, uChan, g_aeChanToSwizzleSel[uChan]);
				}
			}
		}
		else
		{
			return;
		}
	}

	/*
		Check if the move source is assigned to an internal register.
	*/
	if (psSrcInterval != NULL && psSrcInterval->eState == INTERVAL_STATE_ASSIGNED)
	{
		PUSC_LIST_ENTRY	psNextIntervalListEntry;
		PINST			psDeschedInst;

		sSubstContext.bSourceIsIReg = IMG_TRUE;

		/*
			If the source is an internal register then don't substitute if the
			destination is used in a different block.
		*/
		sSubstContext.psSubstBlock = psMoveInst->psBlock;

		/*
			Get the next interval (after the move destination) assigned to the same
			internal register.
		*/
		psNextIntervalListEntry = psSrcInterval->sHwRegIntervalListEntry.psNext;
		if (psNextIntervalListEntry == &psDestInterval->sHwRegIntervalListEntry)
		{
			psNextIntervalListEntry = psNextIntervalListEntry->psNext;
		}
		if (psNextIntervalListEntry != NULL)
		{
			PINTERVAL	psNextInterval;
			PINST		psNextIntervalDefInst;

			psNextInterval = IMG_CONTAINING_RECORD(psNextIntervalListEntry, PINTERVAL, sHwRegIntervalListEntry);
			
			ASSERT(psNextInterval->eState == INTERVAL_STATE_ASSIGNED);
			ASSERT(psNextInterval->uHwReg == psSrcInterval->uHwReg);

			/*	
				Get the start of the next interval.
			*/
			psNextIntervalDefInst = UseDefGetDefInstFromChain(psNextInterval->psUseDefChain, NULL /* puDestIdx */);

			/*
				Don't substitute if the move destination is used after an instruction overwriting the move
				source.
			*/
			ASSERT(psNextIntervalDefInst != NULL);
			sSubstContext.uLastSubstPoint = psNextIntervalDefInst->uBlockIndex;
		}

		/*
			Find the next descheduling instruction after the move instruction.
		*/
		for (psDeschedInst = psMoveInst->psNext; psDeschedInst != NULL; psDeschedInst = psDeschedInst->psNext)
		{
			if (IsDeschedulingPoint(psState, psDeschedInst))
			{
				/*
					Don't substitute if the move destination is used after a descheduling instruction.
				*/
				sSubstContext.uLastSubstPoint = psDeschedInst->uBlockIndex;
				break;
			}
			if (sSubstContext.uLastSubstPoint != USC_UNDEF && psDeschedInst->uBlockIndex >= sSubstContext.uLastSubstPoint)
			{
				break;
			}
		}
	}

	/*
		Check if the source can be substituted for the destination at all uses of the destination.
	*/
	if (DoGlobalMoveReplace(psState,	
							psMoveInst->psBlock, 
							psMoveInst, 
							&psMoveInst->asArg[0], 
							&psMoveInst->asDest[0], 
							SubstSwizzle,
							&sSubstContext))
	{
		if (psDestInterval != NULL && psDestInterval->eState == INTERVAL_STATE_ASSIGNED)
		{
			DropInterval(psState, psContext, psBlockState, psDestInterval);
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID AssignInternalRegistersBlock(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: AssignInternalRegistersBlock
    
 PURPOSE	: Assign internal registers in a single basic block.

 PARAMETERS	: psState		- Compiler intermediate state.
			  psContext		- Internal register allocator state.
			  psBlock		- Basic block whose instructions are to be
							assigned internal registers.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		psListEntry;
	PUSC_LIST_ENTRY		psNextListEntry;
	IREGALLOC_BLOCK		sBlockState;
	IMG_UINT32			uHwReg;
	PINST				psDeschedInst;
	USC_LIST			sDeschedInstList;
	IMG_BOOL			bIsCall;
	SAFE_LIST_ITERATOR	sIter;

	InitializeList(&sBlockState.sUsedIntervalList);
	SafeListInitialize(&sBlockState.sAssignedIntervalList);
	InitializeList(&sBlockState.sBlockIntervalList);
	sBlockState.bPostAlloc = IMG_FALSE;

	/*
		Skip any references to an interval in a call instruction.
	*/
	bIsCall = IMG_FALSE;
	if (IsCall(psState, psBlock))
	{
		bIsCall = IMG_TRUE;
	}

	for (;;)
	{
		PINTERVAL		psInterval;	
		PUSEDEF			psFirstRef;
		PUSC_LIST_ENTRY	psFirstRefInBlock;
		PUSC_LIST_ENTRY	psLastRefInBlock;

		psListEntry = psContext->sNextRefList.psHead;
		if (psListEntry == NULL)
		{
			break;
		}
		psInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, sNextRefListEntry);
		psFirstRef = IMG_CONTAINING_RECORD(psInterval->psNextRef, PUSEDEF, sListEntry);

		/*
			Stop once we've processed all the references in this block to intermediate
			registers needing to be assigned to internal registers.
		*/
		if (!IsReferenceInBlock(psFirstRef, psBlock))
		{
			break;
		}

		/*
			Update the stored next reference for the interval to the next
			reference in a different block.
		*/
		psFirstRefInBlock = psInterval->psNextRef;
		psLastRefInBlock = psFirstRefInBlock;
		for (;;)
		{
			PUSEDEF		psNextRef;

			if (psInterval->psNextRef == NULL)
			{
				break;
			}

			psNextRef = IMG_CONTAINING_RECORD(psInterval->psNextRef, PUSEDEF, sListEntry);
			
			if (!IsReferenceInBlock(psNextRef, psBlock))
			{
				break;
			}

			psLastRefInBlock = psInterval->psNextRef;
			psInterval->psNextRef = psNextRef->sListEntry.psNext;
		}

		/*
			Insert back into the list of intervals which might be assigned to internal registers in future
			blocks.
		*/
		RemoveFromList(&psContext->sNextRefList, &psInterval->sNextRefListEntry);
		psInterval->psNextRef = SkipNonInstReferences(psInterval->psNextRef);
		if (psInterval->psNextRef != NULL)
		{
			InsertInListSorted(&psContext->sNextRefList, CmpNextRef, &psInterval->sNextRefListEntry);
		}

		if (!bIsCall)
		{
			/*
				Internal registers can't be live between basic blocks so, if required, insert instructions to restore
				into internal registers on entry and save from them on exit.
			*/
			ProcessLiveInOutInterval(psState, 
									 psBlock, 
									 psContext, 
									 &sBlockState, 
									 psInterval, 
									 psFirstRefInBlock, 
									 psLastRefInBlock);
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		CreateIntervalsForNonVectorSources(psState, psContext, &sBlockState, psBlock);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	/*
		Return early if no internal registers need to be assigned in the current block.
	*/
	if (IsListEmpty(&sBlockState.sUsedIntervalList) || bIsCall)
	{
		return;
	}

	/*
		Insert extra move instructions so that for any instruction partially/conditionally defining an interval 
		the source for the unwritten channels is last used at the defining instruction.
	*/
	for (psListEntry = sBlockState.sUsedIntervalList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINTERVAL	psInterval;

		psInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, u.sUsedIntervalListEntry);
		ProcessPartialDest(psState, psContext, &sBlockState, psInterval);
	}

	/*
		Create a list of all the descheduling instructions in the current block.
	*/
	InitializeList(&sDeschedInstList);
	for (psDeschedInst = psBlock->psBody; psDeschedInst != NULL; psDeschedInst = psDeschedInst->psNext)
	{
		if (IsDeschedulingPoint(psState, psDeschedInst))
		{
			AppendToList(&sDeschedInstList, &psDeschedInst->sAvailableListEntry);
		}
	}

	/*
		Insert spills where intervals are live across a descheduling instruction.
	*/
	for (psListEntry = sBlockState.sUsedIntervalList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PINTERVAL		psInterval;
		IMG_UINT32		uFirstRefInBlockIndex;
		IMG_UINT32		uLastRefInBlockIndex;
		PUSC_LIST		psIntervalUseDefList;
		PUSC_LIST_ENTRY	psDeschedInstListEntry;

		psNextListEntry = psListEntry->psNext;
		psInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, u.sUsedIntervalListEntry);
		psIntervalUseDefList = &psInterval->psUseDefChain->sList;

		/*
			For all instructions in the current block which deschedule (cause the contents of the
			internal registers to be lost).
		*/
		uFirstRefInBlockIndex = GetRefBlockIndex(psState, psIntervalUseDefList->psHead);
		uLastRefInBlockIndex = GetRefBlockIndex(psState, psIntervalUseDefList->psTail);
		for (psDeschedInstListEntry = sDeschedInstList.psHead; 
			 psDeschedInstListEntry != NULL; 
			 psDeschedInstListEntry = psDeschedInstListEntry->psNext)
		{
			psDeschedInst = IMG_CONTAINING_RECORD(psDeschedInstListEntry, PINST, sAvailableListEntry);

			/*
				The descheduling instructions are sorted by their index in the block so we can stop looking
				once we've seen one with an index larger than the index of the last instruction reading/writing the
				current interval.
			*/
			if (psDeschedInst->uBlockIndex > uLastRefInBlockIndex)
			{
				break;
			}

			/*
				Skip descheduling instructions before the first instruction reading/writing the current interval.
			*/
			if (psDeschedInst->uBlockIndex < uFirstRefInBlockIndex)
			{
				continue;
			}

			/*
				Save and restore the interval around the descheduling instruction.
			*/
			if (SpillIntervalForDeschedule(psState, 
										   psContext, 
										   &sBlockState, 
										   psBlock, 
										   psDeschedInst, 
										   psInterval,
										   &psNextListEntry))
			{
				/*
					All references to this interval have been replaced by shorter intervals.
				*/
				break;
			}
		}
	}

	/* 
		Try to merge all saves and restores of internal registers with
		the instructions defining/using the internal register.
	*/
	MergeIRegSavesAndRestores(psState, psContext, &sBlockState, psBlock);

	/*
		Add information about internal registers already in use in the block.
	*/
	AddFixedIRegs(psState, psContext, &sBlockState, psBlock);

	/*
		Mark that no interval is assigned to any hardware register at the start of the block.
	*/
	for (uHwReg = 0; uHwReg < psContext->uNumHwRegs; uHwReg++)
	{
		PINTERNAL_REGISTER	psHwReg = &psContext->asHwReg[uHwReg];

		psHwReg->psInterval = NULL;
		SafeListInitialize(&psHwReg->sHwRegIntervalList);
	}

	/*
		Assign hardware registers to all intervals.
	*/
	while ((psListEntry = RemoveListHead(&sBlockState.sUsedIntervalList)) != NULL)
	{
		PINTERVAL			psInterval;
		PUSEDEF				psCurrentRef;
		PINST				psCurrentRefInst;
		IMG_UINT32			uCurrentRef;
		IMG_UINT32			uIRegToAssign;
		PINTERNAL_REGISTER	psIRegToAssign;
		PUSEDEF				psFirstUse;
		IMG_UINT32			uFirstUseRef;

		psInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, u.sUsedIntervalListEntry);
		ASSERT(psInterval->uHwReg == USC_UNDEF);

		ASSERT(psInterval->eState == INTERVAL_STATE_PENDING);
		psInterval->eState = INTERVAL_STATE_CURRENT;

		/*
			Get the instruction defining the interval.
		*/
		psCurrentRef = IMG_CONTAINING_RECORD(psInterval->psUseDefChain->sList.psHead, PUSEDEF, sListEntry);
		ASSERT(psCurrentRef->eType == DEF_TYPE_INST);
		psCurrentRefInst = psCurrentRef->u.psInst;
		ASSERT(psCurrentRefInst->psBlock == psBlock);

		/*
			Convert the location of the defining instruction to a integer ID for ease of comparison.
		*/
		uCurrentRef = UseDefToRef(psState, psCurrentRef);

		/*
			Get the first use of the interval.
		*/
		if (psInterval->psUseDefChain->sList.psHead->psNext != NULL)
		{
			psFirstUse = IMG_CONTAINING_RECORD(psInterval->psUseDefChain->sList.psHead->psNext, PUSEDEF, sListEntry);
			ASSERT(psFirstUse->eType == USE_TYPE_SRC || psFirstUse->eType == USE_TYPE_OLDDEST);
			uFirstUseRef = UseDefToRef(psState, psFirstUse);
			ASSERT(uFirstUseRef > uCurrentRef);
		}
		else
		{
			uFirstUseRef = uCurrentRef;
		}

		/*
			Update information about which internal registers are in-use when switching between instructions.
		*/
		KillInternalRegisters(psState, psContext, uCurrentRef);

		/*
			If this destination has a corresponding source for unwritten channels then assign the same internal
			register as was assigned for that source. We fixed the code in the preceeding step so any use of
			an interval as a source for unwritten channels is always the last use.
		*/
		ASSERT(psCurrentRef->uLocation < psCurrentRefInst->uDestCount);
		if (psCurrentRefInst->apsOldDest[psCurrentRef->uLocation] != NULL)
		{
			PARG		psOldDest = psCurrentRefInst->apsOldDest[psCurrentRef->uLocation];
			PINTERVAL	psOldDestInterval;

			psOldDestInterval = GetIntervalByArg(psContext, psOldDest);
			ASSERT(psOldDestInterval != NULL);
			ASSERT(psOldDestInterval->uHwReg < psContext->uNumHwRegs);

			uIRegToAssign = psOldDestInterval->uHwReg;
			psIRegToAssign = &psContext->asHwReg[uIRegToAssign];
		}
		else
		{		
			/*
				Choose an internal register to assign for this interval.
			*/
			if (AssignHardwareRegister(psState, 
									   psContext, 
									   &sBlockState, 
									   psBlock,
									   psInterval, 
									   uCurrentRef, 
									   psCurrentRefInst,
									   uFirstUseRef, 
									   &uIRegToAssign))
			{
				/*
					The current interval was spilt.
				*/
				ASSERT(psInterval->uFixedHwRegMask == psContext->uAllHwRegMask);
				continue;
			}
			psIRegToAssign = &psContext->asHwReg[uIRegToAssign];
		}

		/*
			Mark the current interval as using the internal register.
		*/
		ASSERT((psInterval->uFixedHwRegMask & (1U << uIRegToAssign)) != 0);
		ASSERT(psIRegToAssign->psInterval == NULL);
		psIRegToAssign->psInterval = psInterval;
		psInterval->uHwReg = uIRegToAssign;
		SafeListAppendItem(&psIRegToAssign->sHwRegIntervalList, &psInterval->sHwRegIntervalListEntry);

		/*
			Add the current interval to list of those assigned internal registers.
		*/
		psInterval->eState = INTERVAL_STATE_ASSIGNED;
		SafeListAppendItem(&sBlockState.sAssignedIntervalList, &psInterval->u.sAssignedIntervalListEntry);
	}

	/*
		If we inserted any more saves or restores of internal registers to spill their contents
		then try to merge them.
	*/
	sBlockState.bPostAlloc = IMG_TRUE;
	MergeIRegSavesAndRestores(psState, psContext, &sBlockState, psBlock);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		PINST	psInst, psNextInst;

		/*
			For any move instructions which we inserted to apply swizzles not supported
			directly by an instruction: try to substitute the source swizzle for the 
			destination.

			Other modifications made to instructions during internal register allocation
			might now mean the swizzles are supported.
		*/
		for (psInst = psBlock->psBody; psInst != NULL; psInst = psNextInst)
		{
			psNextInst = psInst->psNext;
			if (psInst->eOpcode == IVMOV && NoPredicate(psState, psInst))
			{
				SubstIRegMove(psState, psContext, &sBlockState, psInst);
			}
		}

		/*
			Look for opportunities to dual-issue loads into internal registers with other
			instructions.
		*/
		DualIssueInternalRegisterRestores(psState, psContext, &sBlockState, psBlock);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		For all intermediate registers assigned internal registers check whether the range of channels used
		is small enough that they could also be assigned a hardware temporary register. 
	*/
	SafeListIteratorInitialize(&sBlockState.sAssignedIntervalList, &sIter);
	for (; SafeListIteratorContinue(&sIter); SafeListIteratorNext(&sIter))
	{
		PINTERVAL		psInterval;

		psInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, u.sAssignedIntervalListEntry);

		if (psInterval->eType != INTERVAL_TYPE_GENERAL)
		{
			continue;
		}

		if (CanSubstByUnifiedStore(psState, psContext, &sBlockState, psBlock, psInterval))
		{
			SubstByUnifiedStore(psState, psContext, &sBlockState, psBlock, psInterval);
		}
	}
	SafeListIteratorFinalise(&sIter);

	/*
		For all intervals in the block replace all references by the assigned hardware register.
	*/
	SafeListIteratorInitialize(&sBlockState.sAssignedIntervalList, &sIter);
	for (; SafeListIteratorContinue(&sIter); SafeListIteratorNext(&sIter))
	{
		PINTERVAL		psInterval;
		PUSC_LIST_ENTRY	psUseListEntry, psNextUseListEntry;
		ARG				sHwReg;

		psInterval = IMG_CONTAINING_RECORD(SafeListIteratorCurrent(&sIter), PINTERVAL, u.sAssignedIntervalListEntry);

		/*
			Reset the flag for whether this interval is to be assigned to an internal register
			in the current block in preparation for processing the next block.
		*/
		ASSERT(psInterval->eState == INTERVAL_STATE_ASSIGNED);
		psInterval->eState = INTERVAL_STATE_UNUSED;

		/*
			Create an argument structure for the internal register assigned to this interval.
		*/
		InitInstArg(&sHwReg);
		sHwReg.uType = USEASM_REGTYPE_FPINTERNAL;
		ASSERT(psInterval->uHwReg != USC_UNDEF);
		sHwReg.uNumber = psInterval->uHwReg;
		sHwReg.eFmt = psInterval->psUseDefChain->eFmt;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			For cores with the vector instruction set: internal register sources are
			always F32.
		*/
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			if (sHwReg.eFmt == UF_REGFORMAT_F16)
			{
				sHwReg.eFmt = UF_REGFORMAT_F32;
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Replace all references to the intermediate register by the internal register.
		*/
		for (psUseListEntry = psInterval->psUseDefChain->sList.psHead; 
			 psUseListEntry != NULL; 
			 psUseListEntry = psNextUseListEntry)
		{
			PUSEDEF	psUseDef;

			psNextUseListEntry = psUseListEntry->psNext;
			psUseDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			ASSERT(UseDefIsInstUseDef(psUseDef));
			ASSERT(psUseDef->u.psInst->psBlock == psBlock);

			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			/*
				Update the stored source formats for vector instruction.
			*/
			if (psInterval->psUseDefChain->eFmt == UF_REGFORMAT_F16 && psUseDef->eType == USE_TYPE_SRC)
			{
				PINST	psUseInst = psUseDef->u.psInst;

				if ((g_psInstDesc[psUseInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
				{
					IMG_UINT32	uSlot;

					ASSERT((psUseDef->uLocation % SOURCE_ARGUMENTS_PER_VECTOR) == 0);
					uSlot = psUseDef->uLocation / SOURCE_ARGUMENTS_PER_VECTOR;

					ASSERT(psUseInst->u.psVec->aeSrcFmt[uSlot] == UF_REGFORMAT_F16);
					psUseInst->u.psVec->aeSrcFmt[uSlot] = UF_REGFORMAT_F32;
				}
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			
			UseDefSubstUse(psState, psInterval->psUseDefChain, psUseDef, &sHwReg);
		}
	}
	SafeListIteratorFinalise(&sIter);

	/*
		Expand internal register saves and restores.
	*/
	ExpandOpcode(psState, psBlock, ISAVEIREG, psContext->pfExpandSave);
	ExpandOpcode(psState, psBlock, IRESTOREIREG, psContext->pfExpandRestore);
	ExpandOpcode(psState, psBlock, IRESTORECARRY, ToMove);
	ExpandOpcode(psState, psBlock, ISAVECARRY, ToMove);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ExpandOpcode(psState, psBlock, IVLOAD, ExpandVLOAD);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Free information about intervals which are only used in this block.
	*/
	while ((psListEntry = RemoveListHead(&sBlockState.sBlockIntervalList)) != NULL)
	{
		PINTERVAL	psInterval;

		psInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, sBlockIntervalListEntry);
		UscFree(psState, psInterval);
	}
}

typedef struct _C10_EXPAND_STATE
{
	REGISTER_REMAP	sRemap;
	USC_LIST		sNewMoveList;
	PUSEDEF_CHAIN	psFirstAlphaTemp;
} C10_EXPAND_STATE, *PC10_EXPAND_STATE;

static
IMG_UINT32 GetAlphaChanReg(PINTERMEDIATE_STATE psState, PC10_EXPAND_STATE psC10ExpandState, IMG_UINT32 uOrigRegNum)
/*****************************************************************************
 FUNCTION	: GetAlphaChanReg
    
 PURPOSE	: Get the temporary register number which holds the alpha channel
			  of a 40-bit temporary register when expanding it into two 32-bit temporary 
			  registers.

 PARAMETERS	: psState			- Compiler state.
			  psC10ExpandState	- State for the C10 expand pass.
			  uOrigRegNum		- 40-bit temporary register.
			  
 RETURNS	: The temporary register.
*****************************************************************************/
{
	IMG_UINT32	uAlphaChanRegNum;
	
	uAlphaChanRegNum = GetRemapLocation(psState, &psC10ExpandState->sRemap, uOrigRegNum);
	if (psC10ExpandState->psFirstAlphaTemp == NULL)
	{
		UseDefSetFmt(psState, uAlphaChanRegNum, UF_REGFORMAT_C10);
		psC10ExpandState->psFirstAlphaTemp = UseDefGet(psState, USEASM_REGTYPE_TEMP, uAlphaChanRegNum);
	}
	return uAlphaChanRegNum;
}

static
IMG_VOID SwitchAlphaChannelToRedInSource(PINTERMEDIATE_STATE psState, PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: SwitchAlphaChannelToRedInSource
    
 PURPOSE	: Update an instruction source to switch references to the ALPHA
			  channel of a C10 format temporary to the RED channel.

 PARAMETERS	: psState		- Compiler state.
			  psUseDef		- Source reference.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	ASSERT(psUseDef->eType == USE_TYPE_SRC);
	psInst = psUseDef->u.psInst;

	switch (psInst->eOpcode)
	{
		case IPCKC10C10:
		case IUNPCKF16C10:
		case IUNPCKF32C10:
		{
			/*
				Update the component select on the instruction source.
			*/
			ASSERT(psUseDef->uLocation < PCK_SOURCE_COUNT);
			ASSERT(psInst->u.psPck->auComponent[psUseDef->uLocation] == USC_ALPHA_CHAN);
			psInst->u.psPck->auComponent[psUseDef->uLocation] = USC_RED_CHAN;
			break;
		}
		case ICOMBC10_ALPHA:
		{
			SetOpcode(psState, psInst, ICOMBC10);
			break;
		}
		case ITESTPRED:
		{
			ASSERT(GetBit(psInst->auFlag, INST_ALPHA) == 1);
			SetBit(psInst->auFlag, INST_ALPHA, 0);
			break;
		}
		default:
		{	
			break;
		}
	}
}

static 
IMG_UINT32 ConvertAlphaSel(IMG_UINT32 uSel)
/*****************************************************************************
 FUNCTION     : ConvertAlphaSel
    
 PURPOSE      : Convert an Alpha SOP selector at an RGB equivalent

 PARAMETERS   : uSel    - Selcctor to convert

 RETURNS      : RGB equivalent of selector
*****************************************************************************/
{
	switch(uSel)
	{
		case USEASM_INTSRCSEL_SRC0:
			return USEASM_INTSRCSEL_SRC0ALPHA;
		case USEASM_INTSRCSEL_SRC1:
			return USEASM_INTSRCSEL_SRC1ALPHA;
		case USEASM_INTSRCSEL_SRC2:
			return USEASM_INTSRCSEL_SRC2ALPHA;
		default:
			break;
	}
	return uSel;
}

static
IMG_VOID SwitchAlphaChannelToRedInDest(PINTERMEDIATE_STATE psState, PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: SwitchAlphaChannelToRedInSDest
    
 PURPOSE	: Update an instruction destination to switch references to the ALPHA
			  channel of a C10 format temporary to the RED channel.

 PARAMETERS	: psState		- Compiler state.
			  psUseDef		- Source reference.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	ASSERT(psUseDef->eType == DEF_TYPE_INST);
	psInst = psUseDef->u.psInst;

	ASSERT(psUseDef->uLocation < psInst->uDestCount);
			
	ASSERT(psInst->auLiveChansInDest[psUseDef->uLocation] == USC_ALPHA_CHAN_MASK);
	psInst->auLiveChansInDest[psUseDef->uLocation] = USC_RED_CHAN_MASK;

	ASSERT(psInst->auDestMask[psUseDef->uLocation] == USC_ALPHA_CHAN_MASK);
	if	((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE) != 0)
	{
		psInst->auDestMask[psUseDef->uLocation] = USC_RED_CHAN_MASK;
	}
	else
	{
		psInst->auDestMask[psUseDef->uLocation] = USC_ALL_CHAN_MASK;
	}

	switch (psInst->eOpcode)
	{
		case IPCKC10C10:
		case IPCKC10F16:
		case IPCKC10F32:
		{
			/*
				The PCK instructions write the converted data from the first source into
				the channel in the destination corresponding to the first set bit in the
				destination mask so we don't need to change anything.
			*/
			ASSERT(g_abSingleBitSet[psInst->auDestMask[0]]);
			break;
		}
		case ISOPWM:
		{
			/*
				Select the hardware's special mode where the ALPHA channel result of the
				instruction calculation is copied into the RED channel of the destination
				register.
			*/
			psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
			psInst->u.psSopWm->bRedChanFromAlpha = IMG_TRUE;
			break;
		}
		case IFPMA:
		{
			PFPMA_PARAMS	psFpma = psInst->u.psFpma;

			/*	
				Copy the parameters for the ALPHA part of the calculation to the
				RGB part. If any of the source selects are USEASM_INTSRCSEL_SRCx
				then replace them by USEASM_INTSRCSEL_SRCxALPHA.
			*/
			psFpma->uCSel0 = ConvertAlphaSel(psFpma->uASel0);
			psFpma->bComplementCSel0 = psFpma->bComplementASel0;
			psFpma->uCSel1 = USEASM_INTSRCSEL_SRC1ALPHA;
			psFpma->bComplementCSel1 = psFpma->bComplementASel1;
			psFpma->uCSel2 = USEASM_INTSRCSEL_SRC2ALPHA;
			psFpma->bComplementCSel2 = psFpma->bComplementASel2;
			break;
		}
		case ISOP3:
		{
			SOP3_PARAMS	sSop3 = *psInst->u.psSop3;
			PLRP1_PARAMS psLrp1;

			ASSERT(sSop3.uCoissueOp == USEASM_OP_ALRP);

			ASSERT(psInst->asArg[1].uType != USEASM_REGTYPE_FPINTERNAL);
			ASSERT(psInst->asArg[2].uType != USEASM_REGTYPE_FPINTERNAL);

			/*
				Move the alpha channel linear interpolate to the red channel.
			*/
			SetOpcode(psState, psInst, ILRP1);
			psLrp1 = psInst->u.psLrp1;
			if (sSop3.uASel1 == USEASM_INTSRCSEL_SRC1 || sSop3.uASel1 == USEASM_INTSRCSEL_SRC1ALPHA)
			{
				psLrp1->uCSel10 = USEASM_INTSRCSEL_SRC1;
			}
			else
			{
				ASSERT(sSop3.uASel1 == USEASM_INTSRCSEL_ZERO);
				psLrp1->uCSel10 = USEASM_INTSRCSEL_ZERO;
			}
			psLrp1->bComplementCSel10 = sSop3.bComplementASel1;
			if (sSop3.uASel2 == USEASM_INTSRCSEL_SRC2 || sSop3.uASel2 == USEASM_INTSRCSEL_SRC2ALPHA)
			{
				psLrp1->uCSel11 = USEASM_INTSRCSEL_SRC2;
			}
			else
			{
				ASSERT(sSop3.uASel2 == USEASM_INTSRCSEL_ZERO);
				psLrp1->uCSel11 = USEASM_INTSRCSEL_ZERO;
			}
			psLrp1->bComplementCSel11 = sSop3.bComplementASel2;
			psLrp1->uCS = USEASM_INTSRCSEL_SRC0ALPHA;

			/*
				Set the calculation for the ALPHA channel (which is unreferenced) to safe defaults.
			*/
			psLrp1->uAOp = USEASM_INTSRCSEL_ADD;
			psLrp1->uASel1 = USEASM_INTSRCSEL_ZERO;
			psLrp1->bComplementASel1 = IMG_FALSE;
			psLrp1->uASel2 = USEASM_INTSRCSEL_ZERO;
			psLrp1->bComplementASel2 = IMG_FALSE;
			break;
		}
		case IFPDOT:
		{
			/*
				The dotproduct instruction broadcasts a single result to every channel in the destination so
				we don't need to update anything when switching from the ALPHA to the RED channel.
			*/
			break;
		}
		case IDELTA:
		{
			IMG_UINT32	uArg;

			ASSERT(psInst->uDestCount == 1);
			ASSERT(psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL);
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				ASSERT(psInst->asArg[uArg].uType != USEASM_REGTYPE_FPINTERNAL);
			}
			break;
		}
		case IMOV:
		{
			ASSERT(psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL);
			if (psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL)
			{
				SetOpcode(psState, psInst, IPCKC10C10);
				SetPCKComponent(psState, psInst, 0 /* uSrcIdx */, USC_ALPHA_CHAN);
				SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_U8);

				ASSERT(psInst->auLiveChansInDest[0] == USC_RED_CHAN_MASK);
				psInst->auDestMask[0] = psInst->auLiveChansInDest[0];
			}
			break;
		}
		default:
		{
			imgabort();
		}
	}
}

static
IMG_VOID SwitchAlphaChannelToRed(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefChain)
/*****************************************************************************
 FUNCTION	: SwitchAlphaChannelToRed
    
 PURPOSE	: Update instruction sources/destinations to switch references to the ALPHA
			  channel of a C10 format temporary to the RED channel.

 PARAMETERS	: psState		- Compiler state.
			  psUseDefChain	- Temporary register to update references for.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		
		switch (psUseDef->eType)
		{
			case USE_TYPE_SRC:
			{
				SwitchAlphaChannelToRedInSource(psState, psUseDef);
				break;
			}
			case DEF_TYPE_INST:
			{
				SwitchAlphaChannelToRedInDest(psState, psUseDef);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

static
IMG_VOID SeparateC10DestAlphaChannel(PINTERMEDIATE_STATE	psState, 
									 PC10_EXPAND_STATE		psC10ExpandState, 
									 PUSEDEF_CHAIN			psUseDefChain,
									 PUSEDEF				psDef)
/*****************************************************************************
 FUNCTION	: SeparateC10DestAlphaChannel
    
 PURPOSE	: Replace a define of the ALPHA channel of a temporary register by
			  define of a separate register.

 PARAMETERS	: psState			- Compiler state.
			  psC10ExpandState	- Context for the C10 expansion phase.
			  psUseDefChain		- Temporary register to update.
			  psDef				- Define to update.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psInst;
	IMG_UINT32	uDestIdx;
	PARG		psOldDest;

	ASSERT(psDef->eType == DEF_TYPE_INST);

	psInst = psDef->u.psInst;
	uDestIdx = psDef->uLocation;

	ASSERT(uDestIdx < psInst->uDestCount);
	psOldDest = psInst->apsOldDest[uDestIdx];

	/*
		Check for writing the ALPHA channel.
	*/
	if (psInst->auDestMask[uDestIdx] == USC_ALPHA_CHAN_MASK)
	{
		/*
			If this instruction is preserving the RGB channels then insert an extra copy instruction.
		*/
		if (psOldDest != NULL && (psInst->auLiveChansInDest[uDestIdx] & USC_RGB_CHAN_MASK) != 0)
		{
			PINST	psRGBMoveInst;

			/*
				Copy the RGB part of the destination.
			*/
			psRGBMoveInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psRGBMoveInst, IMOV);

			MoveDest(psState, psRGBMoveInst, 0 /* uDestIdx */, psInst, uDestIdx);
			psRGBMoveInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDestIdx] & USC_RGB_CHAN_MASK;

			SetSrcFromArg(psState, psRGBMoveInst, 0 /* uSrcIdx */, psOldDest);

			InsertInstAfter(psState, psInst->psBlock, psRGBMoveInst, psInst);

			AppendToList(&psC10ExpandState->sNewMoveList, &psRGBMoveInst->sTempListEntry);
		}

		/*
			Replace the destination by the temporary containing the ALPHA channel.
		*/
		SetDest(psState,
				psInst,
				uDestIdx,
				USEASM_REGTYPE_TEMP,
				GetAlphaChanReg(psState, psC10ExpandState, psUseDefChain->uNumber),
				UF_REGFORMAT_C10);

		/*
			The RGB channels are now live in a separate temporary.
		*/
		psInst->auLiveChansInDest[uDestIdx] &= USC_ALPHA_CHAN_MASK;

		if (psOldDest != NULL)
		{
			/*
				If the instruction was only masking out the RGB channels then clear the
				partial destination.
			*/
			if (GetPreservedChansInPartialDest(psState, psInst, uDestIdx) == 0)
			{
				SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psPartialDest */);
			}
			/*
				Replace the source for unwritten channels by the new temporary containing its ALPHA channel.
			*/
			else if (psOldDest->uType == USEASM_REGTYPE_TEMP)
			{
				SetPartialDest(psState,
							   psInst,
							   uDestIdx,
							   USEASM_REGTYPE_TEMP,
							   GetAlphaChanReg(psState, psC10ExpandState, psOldDest->uNumber),
							   UF_REGFORMAT_C10);
			}
		}
	}
	else
	{
		/*
			The internal register allocator should have ensured any defines of both the RGB channels
			and the ALPHA channel are replaced by internal registers.
		*/
		ASSERT((psInst->auLiveChansInDest[uDestIdx] & psInst->auDestMask[uDestIdx] & USC_ALPHA_CHAN_MASK) == 0);

		/*
			If the instruction is preserving the ALPHA channel then insert an extra copy.
		*/
		if (psOldDest && (psInst->auLiveChansInDest[uDestIdx] & USC_ALPHA_CHAN_MASK) != 0)
		{
			PINST	psAlphaMoveInst;

			/*
				Copy the ALPHA channel from the source for unwritten channels to the
				destination.
			*/
			psAlphaMoveInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psAlphaMoveInst, IMOV);
			SetDest(psState,
					psAlphaMoveInst,
					0 /* uDestIdx */,
					USEASM_REGTYPE_TEMP,
					GetAlphaChanReg(psState, psC10ExpandState, psUseDefChain->uNumber),
					UF_REGFORMAT_C10);
			psAlphaMoveInst->auDestMask[0] = USC_ALPHA_CHAN_MASK;
			psAlphaMoveInst->auLiveChansInDest[0] = USC_ALPHA_CHAN_MASK;
			if (psOldDest->uType == USEASM_REGTYPE_TEMP)
			{
				ASSERT(psOldDest->eFmt == UF_REGFORMAT_C10);
				SetSrc(psState,
					   psAlphaMoveInst,
					   0 /* uSrcIdx */,
					   USEASM_REGTYPE_TEMP,
					   GetAlphaChanReg(psState, psC10ExpandState, psOldDest->uNumber),
					   UF_REGFORMAT_C10);
			}
			else
			{
				SetSrcFromArg(psState, psAlphaMoveInst, 0 /* uSrcIdx */, psOldDest);
			}
			InsertInstAfter(psState, psInst->psBlock, psAlphaMoveInst, psInst);

			AppendToList(&psC10ExpandState->sNewMoveList, &psAlphaMoveInst->sTempListEntry);
		}

		/*
			The ALPHA channel is now live in a separate temporary.
		*/
		psInst->auLiveChansInDest[uDestIdx] &= ~USC_ALPHA_CHAN_MASK;
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_DESTMASKABLE) == 0)
		{
			psInst->auDestMask[uDestIdx] = USC_ALL_CHAN_MASK;
		}
	}
}

static
IMG_VOID SeparateC10AlphaChannel(PINTERMEDIATE_STATE psState, PC10_EXPAND_STATE psC10ExpandState, PUSEDEF_CHAIN psUseDefChain)
/*****************************************************************************
 FUNCTION	: SeparateC10AlphaChannel
    
 PURPOSE	: Replace references to the alpha channel of a temporary register by
			  references to a separate register.

 PARAMETERS	: psState			- Compiler state.
			  psC10ExpandState	- Context for the C10 expansion phase.
			  psUseDefChain		- Temporary register to update.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;

	if (psUseDefChain->uType == USC_REGTYPE_REGARRAY)
	{
		return;
	}

	ASSERT(psUseDefChain->uType == USEASM_REGTYPE_TEMP);

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		switch (psUseDef->eType)
		{
			case DEF_TYPE_INST:
			{
				SeparateC10DestAlphaChannel(psState, psC10ExpandState, psUseDefChain, psUseDef);
				break;
			}
			case USE_TYPE_SRC:
			{
				IMG_UINT32	uReferencedChans;
				
				/*
					Check for use of the ALPHA channel.
				*/
				uReferencedChans = GetLiveChansInArg(psState, psUseDef->u.psInst, psUseDef->uLocation);
				if ((uReferencedChans & USC_ALPHA_CHAN_MASK) != 0)
				{	
					/*
						The internal register allocator should have ensured that any sources
						using both the ALPHA channel and the RGB channels have been replaced by
						internal registers.
					*/
					ASSERT((uReferencedChans & USC_RGB_CHAN_MASK) == 0);

					/*
						Replace the reference by the new temporary.
					*/
					SetSrc(psState,
						   psUseDef->u.psInst,
						   psUseDef->uLocation,
						   USEASM_REGTYPE_TEMP,
						   GetAlphaChanReg(psState, psC10ExpandState, psUseDefChain->uNumber),
						   UF_REGFORMAT_C10);
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

static
IMG_VOID ExpandC10Iterations(PINTERMEDIATE_STATE psState, PC10_EXPAND_STATE psC10ExpandState)
/*****************************************************************************
 FUNCTION	: ExpandC10Iterations
    
 PURPOSE	: Expand registers representing iterations of C10 data from one 40-bit register to
			  two 32-bit registers.

 PARAMETERS	: psState			- Compiler state.
			  psC10ExpandState	- Context for the C10 expansion phase.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PUSC_LIST_ENTRY	psInputListEntry;

		for (psInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext)
		{
			PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);

			if (psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_C10)
			{
				IMG_UINT32	uOldRegNum;
				IMG_UINT32	uOldUsedChans;
				IMG_UINT32	uAlphaChanTemp;

				/*
					Get the register representing the RGB channels.
				*/
				ASSERT(psInput->sLoad.uNumAttributes == 1);
				uOldRegNum = psInput->psFixedReg->auVRegNum[0];

				/*
					Get the register representing the ALPHA channel.
				*/
				uAlphaChanTemp = GetAlphaChanReg(psState, psC10ExpandState, uOldRegNum);

				/*
					Increase the number of registers used for the result of this
					iteration.
				*/
				psInput->sLoad.uNumAttributes = 2;

				/*
					Grow the array recording what channels are used from the pixel 
					shader input.
				*/
				uOldUsedChans = psInput->psFixedReg->puUsedChans[0];
				UscFree(psState, psInput->psFixedReg->puUsedChans);
				psInput->psFixedReg->puUsedChans = 
					UscAlloc(psState, 
							 UINTS_TO_SPAN_BITS(SCALAR_REGISTERS_PER_C10_VECTOR * CHANS_PER_REGISTER) * sizeof(IMG_UINT32));
				SetRegMask(psInput->psFixedReg->puUsedChans, 0 /* uReg */, uOldUsedChans & USC_RGB_CHAN_MASK);
				if (uOldUsedChans & USC_ALPHA_CHAN_MASK)
				{
					SetRegMask(psInput->psFixedReg->puUsedChans, 1 /* uReg */, USC_RED_CHAN_MASK);
				}
				else
				{
					SetRegMask(psInput->psFixedReg->puUsedChans, 1 /* uReg */, 0);
				}
				
				/*
					Grow the array holding the registers which contain the results of
					the iteration.
				*/
				UscFree(psState, psInput->psFixedReg->auVRegNum);
				UscFree(psState, psInput->psFixedReg->aeVRegFmt);
				psInput->psFixedReg->auVRegNum = UscAlloc(psState, sizeof(psInput->psFixedReg->auVRegNum[0]) * 2);
				psInput->psFixedReg->aeVRegFmt = UscAlloc(psState, sizeof(psInput->psFixedReg->aeVRegFmt[0]) * 2);
				psInput->psFixedReg->asVRegUseDef = 
					UseDefResizeArray(psState, psInput->psFixedReg->asVRegUseDef, 1 /* uOldCount */, 2 /* uNewCount */);
				UseDefReset(&psInput->psFixedReg->asVRegUseDef[1], 
						    DEF_TYPE_FIXEDREG,
							1 /* uLocation */, 
							psInput->psFixedReg);
				psInput->psFixedReg->uConsecutiveRegsCount = 2;
				psInput->psFixedReg->auVRegNum[0] = uOldRegNum;
				psInput->psFixedReg->aeVRegFmt[0] = UF_REGFORMAT_C10;
				psInput->psFixedReg->auVRegNum[1] = uAlphaChanTemp;
				psInput->psFixedReg->aeVRegFmt[1] = UF_REGFORMAT_C10;

				/*
					Add USE-DEF information for the alpha channel.
				*/
				UseDefAddFixedRegDef(psState, psInput->psFixedReg, 1 /* uRegIdx */);
			}
		}
	}
}

static
IMG_VOID ExpandC10SecondaryAttributes(PINTERMEDIATE_STATE psState, PC10_EXPAND_STATE psC10ExpandState)
/*****************************************************************************
 FUNCTION	: ExpandC10SecondaryAttributes
    
 PURPOSE	: Expand registers representing secondary attributes containing
			  C10 data from one 40-bit register to two 32-bit registers.

 PARAMETERS	: psState			- Compiler state.
			  psC10ExpandState	- Context for the C10 expansion phase.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		psListEntry;
	PUSC_LIST_ENTRY		psPrevListEntry;
	PREGISTER_LIVESET	psSAOutputs;
	PCODEBLOCK			psSecAttrProg;

	if (psState->sSAProg.uNumResults == 0)
	{
		return;
	}
			
	psSecAttrProg = psState->psSecAttrProg->sCfg.psExit;
	psSAOutputs = &psSecAttrProg->sRegistersLiveOut;
	for (psListEntry = psState->sSAProg.sResultsList.psTail; 
		 psListEntry != NULL; 
		 psListEntry = psPrevListEntry)
	{
		PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);

		psPrevListEntry = psListEntry->psPrev;

		if (psResult->eFmt == UF_REGFORMAT_C10 && !psResult->bPartOfRange)
		{
			IMG_UINT32					uOldRegNum;
			IMG_UINT32					uOldMask;
			IMG_UINT32					uPartIdx;
			PINREGISTER_CONST			apsDriverConst[MAX_SCALAR_REGISTERS_PER_VECTOR];
			IMG_UINT32					uPhysicalRegNum = USC_UNDEF;
			IMG_BOOL					bOldPartOfRange;
			SAPROG_RESULT_TYPE			eOldResultType;
			IMG_UINT32					uAlphaChanTemp;
			PCONSTANT_INREGISTER_RANGE	psOldDriverConstRange = NULL;

			uOldRegNum = psResult->psFixedReg->auVRegNum[0];

			/*
				Get the mask of channels live in the 40-bit register on
				output from the secondary update program.
			*/
			uOldMask = GetRegisterLiveMask(psState,
										   psSAOutputs,
										   USEASM_REGTYPE_TEMP,
										   uOldRegNum,
										   0 /* uArrayOffset */);

			if (psResult->uNumHwRegisters == 1 && (uOldMask & USC_RGB_CHAN_MASK) != 0)
			{
				ASSERT((uOldMask & USC_ALPHA_CHAN_MASK) == 0);
				continue;
			}

			/*
				Remove the 40-bit register from the set of registers live out of
				the secondary update program.
			*/
			if (uOldMask != 0)
			{
				SetRegisterLiveMask(psState,
									psSAOutputs,
									USEASM_REGTYPE_TEMP,
									uOldRegNum,
									0 /* uArrayOffset */,
									0 /* uMask */);
			}

			/*
				Save any references from the old result to any driver loaded constants or
				directly mapped constant buffers.
			*/
			eOldResultType = psResult->eType;
			bOldPartOfRange = psResult->bPartOfRange;
			if (eOldResultType == SAPROG_RESULT_TYPE_DRIVERLOADED)
			{
				memcpy(apsDriverConst, psResult->u1.sDriverConst.apsDriverConst, sizeof(apsDriverConst));
				psOldDriverConstRange = psResult->u1.sDriverConst.psDriverConstRange;
			}
			else if (eOldResultType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
			{
				uPhysicalRegNum = psResult->psFixedReg->sPReg.uNumber;
			}

			/*
				Free the structure representing the old 40-bit secondary
				update program result.
			*/
			DropSAProgResult(psState, psResult);
			psResult = NULL;

			/*
				Allocate a new temporary for the alpha channel of the secondary attribute.
			*/
			uAlphaChanTemp = GetAlphaChanReg(psState, psC10ExpandState, uOldRegNum);

			for (uPartIdx = 0; uPartIdx < 2; uPartIdx++)
			{
				IMG_UINT32	uPartMask;
				IMG_UINT32	uNewMask;
				IMG_UINT32	uPartRegNum;

				if (uPartIdx == 0)
				{
					uPartMask = USC_RGB_CHAN_MASK;
					uNewMask = uOldMask & USC_RGB_CHAN_MASK;
					uPartRegNum = uOldRegNum;
				}
				else
				{
					uPartMask = USC_ALPHA_CHAN_MASK;
					uNewMask = (uOldMask & uPartMask) != 0 ? USC_RED_CHAN_MASK : 0;
					uPartRegNum = uAlphaChanTemp;
				}

				/*
					Update the masks of channels live in this part of the
					40-bit registers.
				*/
				SetRegisterLiveMask(psState,
									psSAOutputs,
									USEASM_REGTYPE_TEMP,
									uPartRegNum,
									0 /* uArrayOffset */,
									uNewMask);

				if (
						(uOldMask & uPartMask) != 0 || 
						eOldResultType == SAPROG_RESULT_TYPE_DIRECTMAPPED ||
						(
							eOldResultType == SAPROG_RESULT_TYPE_DRIVERLOADED &&
							apsDriverConst[uPartIdx] != NULL
						)
					)
				{
					PSAPROG_RESULT	psNewResult;

					ASSERT(uPartRegNum != USC_UNDEF);

					/*
						Create a new SA program result for this part of the
						old 40-bit register.
					*/
					psNewResult = 
						BaseAddNewSAProgResult(psState,
											   UF_REGFORMAT_C10,
											   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
											   IMG_FALSE /* bVector */,
											   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
											   0 /* uHwRegAlignment */,
											   1 /* uNumHwRegisters */,
											   uPartRegNum,
											   uNewMask,
											   eOldResultType,
											   bOldPartOfRange);

					if (eOldResultType == SAPROG_RESULT_TYPE_DRIVERLOADED)
					{
						PINREGISTER_CONST psDriverConst = apsDriverConst[uPartIdx];

						/*
							Copy any reference to a driver loaded secondary attribute.
						*/
						psNewResult->u1.sDriverConst.apsDriverConst[0] = psDriverConst;
						ASSERT(psDriverConst != NULL);

						/*
							Update the back pointer in the driver loaded secondary attribute.
						*/
						ASSERT(psDriverConst->psResult == NULL);
						psDriverConst->psResult = psNewResult;		

						/*
							Copy the pointer to any associated constant range.
						*/
						psNewResult->u1.sDriverConst.psDriverConstRange = psOldDriverConstRange;
					}
					else if (eOldResultType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
					{
						/*
							Update the fixed location of this secondary attribute.
						*/
						ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList,
												  psNewResult->psFixedReg,
												  USEASM_REGTYPE_SECATTR,
												  uPhysicalRegNum + uPartIdx);

					}
				}
			}
		}
	}
}

static
IMG_VOID ExpandC10Move(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandC10Move
    
 PURPOSE	: Expand a move copying C10 data to/from internal registers to
			  a SOPWM instruction which the hardware supports directly.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to expand.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (
			(
				psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL &&
				psInst->asDest[0].eFmt == UF_REGFORMAT_C10
			) ||
			(
				psInst->asArg[0].uType == USEASM_REGTYPE_FPINTERNAL &&
				psInst->asArg[0].eFmt == UF_REGFORMAT_C10
			)
		)
	{
		/*
			Just drop copies between the same source and destination.
		*/
		if (EqualArgs(&psInst->asDest[0], &psInst->asArg[0]))
		{
			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);
			return;
		}

		SetOpcode(psState, psInst, ISOPWM);

		/*
			RESULT = SRC1 * ONE + SRC2 * ZERO
		*/
		psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
		psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
		psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
		psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
		psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

		InitInstArg(&psInst->asArg[1]);
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
	}
}

static
IMG_VOID ExpandCOMBC10_ALPHA(PINTERMEDIATE_STATE psState, PINST psCOMBC10Inst)
/*****************************************************************************
 FUNCTION	: ExpandCOMBC10_ALPHA
    
 PURPOSE	: Modify COMBC10_ALPHA instructions (a special variant of COMBC10)
			  used only during internal register allocator) to COMBC10.

 PARAMETERS	: psState			- Compiler state.
			  psCOMBC10Inst		- Instruction to expand.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(GetLiveChansInArg(psState, psCOMBC10Inst, COMBC10_ALPHA_SOURCE) == 0);
	SetSrc(psState, psCOMBC10Inst, COMBC10_ALPHA_SOURCE, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);

	ModifyOpcode(psState, psCOMBC10Inst, ICOMBC10);
}

static
IMG_VOID ExpandCOMBC10(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: ExpandCOMBC10
    
 PURPOSE	: Modify COMBC10 instructions which aren't directly supported by
			  the hardware.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to expand.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Expand COMBC10 instructions with partial destination write masks.
	*/
	if ((psInst->auLiveChansInDest[0] & ~psInst->auDestMask[0]) != 0 || psInst->asDest[0].uType != USEASM_REGTYPE_FPINTERNAL)
	{
		IMG_UINT32	uEffectiveDestMask = psInst->auDestMask[0] & psInst->auLiveChansInDest[0];

		if ((uEffectiveDestMask & USC_RGB_CHAN_MASK) == 0)
		{
			/*
				Convert

					COMBC10		DEST.1000, ??, ALPHA

				->
					PCKC10C10	DEST.1000, ALPHA, #0
			*/
			SetOpcode(psState, psInst, IPCKC10C10);

			MoveSrc(psState, psInst, 0 /* uMoveToSrcIdx */, psInst, COMBC10_ALPHA_SOURCE);
			SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
		}
		else
		{
			if ((uEffectiveDestMask & USC_ALPHA_CHAN_MASK) != 0)
			{
				PINST	psNewInst;
				ARG		sAlphaTemp;

				/*
					Mark that the instruction writing the RGB channels needs to preserve the contents of the
					ALPHA channel.
				*/
				psInst->auDestMask[0] &= ~USC_ALPHA_CHAN_MASK;

				/*
					Allocate a new intermediate register for just the alpha result.
				*/
				if (psInst->asDest[0].uType == USEASM_REGTYPE_FPINTERNAL)
				{
					sAlphaTemp = psInst->asDest[0];
				}
				else
				{
					ASSERT(psInst->asDest[0].uType == USEASM_REGTYPE_TEMP);

					MakeNewTempArg(psState, UF_REGFORMAT_C10, &sAlphaTemp);
				}

				/*
					Insert an extra instruction to write the alpha channel.

						PCKC10C10	DEST.1000, SRCALPHA, #0
				*/
				psNewInst = AllocateInst(psState, psInst);
				SetOpcode(psState, psNewInst, IPCKC10C10);

				SetDestFromArg(psState, psNewInst, 0 /* uCopyToDestIdx */, &sAlphaTemp);

				psNewInst->auDestMask[0] = psInst->auDestMask[0] & USC_ALPHA_CHAN_MASK;
				psNewInst->auLiveChansInDest[0] = GetPreservedChansInPartialDest(psState, psInst, 0 /* uDestIdx */);
				MovePartiallyWrittenDest(psState, psNewInst, 0 /* uMoveToDestIdx */, psInst, 0 /* uMoveFromDestIdx */);

				CopyPredicate(psState, psNewInst, psInst);

				MoveSrc(psState, psNewInst, 0 /* uMoveToSrcIdx */, psInst, COMBC10_ALPHA_SOURCE);
				SetSrc(psState, psNewInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);

				InsertInstBefore(psState, psInst->psBlock, psNewInst, psInst);

				/*
					Set the original instruction to copy the ALPHA channel from the result of the PCKC10C10 into
					the ALPHA channel of its destination.
				*/
				SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, &sAlphaTemp);
			}

			/*
				Convert
							
					COMBC10		DEST.0RGB, SRCRGB, SRCALPHA

				->
					SOPWM		DEST.0RGB, SRCRGB, #0
			*/
			SetOpcode(psState, psInst, ISOPWM);

			/*
				RESULT = SRC1 * ONE + SRC2 * ZERO
			*/
			psInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSopWm->bComplementSel1 = IMG_TRUE;
			psInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
			psInst->u.psSopWm->bComplementSel2 = IMG_FALSE;
			psInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
			psInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;

			SetSrc(psState, psInst, 1 /* uSrcIdx */, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);
		}
	}
	else
	{
		psInst->auDestMask[0] = USC_ALL_CHAN_MASK;
	}
}

static
IMG_VOID SubstFixedPointComponentMoveCB(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: SubstFixedPointComponentMove
    
 PURPOSE	: Try to substitute the source to a move of a component of a fixed
			  point vector for its destination.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Component move instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	(IMG_VOID)SubstFixedPointComponentMove(psState, psInst);
}

static
IMG_VOID ExpandC10Temporaries(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ExpandC10Temporaries
    
 PURPOSE	: Expand temporary registers containing C10 data from one 40-bit 
			  register to two 32-bit registers.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		psListEntry;
	C10_EXPAND_STATE	sC10ExpandState;
	SAFE_LIST_ITERATOR	sIter;

	/*
		Initialize the map from existing register numbers to the new register number for the ALPHA
		channel.
	*/
	InitializeRemapTable(psState, &sC10ExpandState.sRemap);

	/*
		Initialize the list of new moves inserted during expansion.
	*/
	InitializeList(&sC10ExpandState.sNewMoveList);

	/*
		GetAlphaChanReg() will initialize this to the first temporary created for a C10 alpha channel.
	*/
	sC10ExpandState.psFirstAlphaTemp = NULL;

	/*
		Expand DELTA instructions choosing between C10 inputs; starting from the end of the existing list so
		we don't modify the new DELTA instruction we insert.
	*/
	InstListIteratorInitializeAtEnd(psState, IDELTA, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorPrev(&sIter))
	{
		PINST	psDeltaInst;

		psDeltaInst = InstListIteratorCurrent(&sIter);
		ASSERT(psDeltaInst->eOpcode == IDELTA);

		ASSERT(psDeltaInst->uDestCount == 1);
		if (psDeltaInst->asDest[0].eFmt != UF_REGFORMAT_C10)
		{
			continue;
		}
		if ((psDeltaInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) == 0)
		{
			continue;
		}
		if ((psDeltaInst->auLiveChansInDest[0] & USC_RGB_CHAN_MASK) != 0)
		{
			PINST		psAlphaDelta;
			IMG_UINT32	uSrc;
			PARG		psDest = &psDeltaInst->asDest[0];

			psAlphaDelta = AllocateInst(psState, psDeltaInst);
			SetOpcode(psState, psAlphaDelta, IDELTA);

			ASSERT(psDest->uType == USEASM_REGTYPE_TEMP);
			SetDest(psState, 
					psAlphaDelta, 
					0 /* uDestIdx */, 
					USEASM_REGTYPE_TEMP,
					GetAlphaChanReg(psState, &sC10ExpandState, psDest->uNumber), 
					psDest->eFmt);
			psAlphaDelta->auDestMask[0] = USC_ALPHA_CHAN_MASK;
			psAlphaDelta->auLiveChansInDest[0] = USC_ALPHA_CHAN_MASK;

			psDeltaInst->auLiveChansInDest[0] &= USC_RGB_CHAN_MASK;

			SetArgumentCount(psState, psAlphaDelta, psDeltaInst->uArgumentCount);
			for (uSrc = 0; uSrc < psDeltaInst->uArgumentCount; uSrc++)
			{
				PARG	psArg = &psDeltaInst->asArg[uSrc];

				if (psArg->uType == USEASM_REGTYPE_TEMP)
				{
					ASSERT(psArg->eFmt == UF_REGFORMAT_C10);
					SetSrc(psState,	
						   psAlphaDelta,
						   uSrc,
						   USEASM_REGTYPE_TEMP,
						   GetAlphaChanReg(psState, &sC10ExpandState, psArg->uNumber),
						   UF_REGFORMAT_C10);
				}
				else
				{
					ASSERT(psArg->uType == USEASM_REGTYPE_IMMEDIATE);
					SetSrcFromArg(psState, psAlphaDelta, uSrc, psArg);
				}
			}
			InsertInstAfter(psState, psDeltaInst->psBlock, psAlphaDelta, psDeltaInst);
		}
		else
		{
			psDeltaInst->auDestMask[0] = USC_ALPHA_CHAN_MASK;
		}
	}
	InstListIteratorFinalise(&sIter);

	/*
		Replace 40-bit registers by pairs of 32-bit registers in function input and outputs.
	*/
	ExpandFunctions(psState, &sC10ExpandState.sRemap, USC_RGB_CHAN_MASK /* uFirstC10PairChanMask */);

	/*
		Expand C10 format secondary attributes.
	*/
	ExpandC10SecondaryAttributes(psState, &sC10ExpandState);

	/*
		Expand registers representing iterations of C10 data from one 40-bit register to
		two 32-bit registers.
	*/
	ExpandC10Iterations(psState, &sC10ExpandState);

	/*
		After internal register allocation there should be no uses/defines of C10 temporaries where both the alpha channel
		and the RGB channels are simultaneously referenced. Go through all the uses/defines of C10 temporaries 
		referencing just the alpha channel and replace them by new temporaries.
	*/
	psListEntry = psState->sC10TempList.psHead;
	if (psListEntry != NULL)
	{
		for (;;)
		{
			PUSEDEF_CHAIN	psUseDefChain;
			PUSC_LIST_ENTRY	psNextListEntry;

			psNextListEntry = psListEntry->psNext;
			psUseDefChain = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sC10TempListEntry);

			if (psUseDefChain == sC10ExpandState.psFirstAlphaTemp)
			{
				break;
			}
		
			ASSERT(psUseDefChain->eFmt == UF_REGFORMAT_C10);
			SeparateC10AlphaChannel(psState, &sC10ExpandState, psUseDefChain);	

			if (psNextListEntry == NULL)
			{
				break;
			}
			psListEntry = psNextListEntry;
		} 
	}

	/*
		Perform any instruction updates so the red channel is used instead of the alpha channel for C10
		temporaries.
	*/
	if (sC10ExpandState.psFirstAlphaTemp != NULL)
	{
		PUSC_LIST_ENTRY		psFirstAlphaTemp;

		psFirstAlphaTemp = &sC10ExpandState.psFirstAlphaTemp->sC10TempListEntry;
		for (psListEntry = psFirstAlphaTemp; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF_CHAIN	psUseDefChain;

			psUseDefChain = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sC10TempListEntry);
			SwitchAlphaChannelToRed(psState, psUseDefChain);
		}
	}

	/*
		Expand MOV instruction moving C10 data between an internal register
		and a unified store register.
	*/
	ForAllInstructionsOfType(psState, IMOV, ExpandC10Move);

	/*
		Remove any stray COMBC10_ALPHA instructions where the alpha channel of the
		result wasn't used.
	*/
	ForAllInstructionsOfType(psState, ICOMBC10_ALPHA, ExpandCOMBC10_ALPHA);

	/*
		Flag to later stages that C10 format temporaries are now a maximum of 32-bits wide.
	*/
	psState->uFlags |= USC_FLAGS_POSTC10REGALLOC;

	/*
		If we inserted any moves during expansion then try to remove them now.
	*/
	while ((psListEntry = RemoveListHead(&sC10ExpandState.sNewMoveList)) != NULL)
	{
		PINST		psMoveInst;

		psMoveInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sTempListEntry);

		ASSERT(psMoveInst->eOpcode == IMOV);
		EliminateGlobalMove(psState, 
							psMoveInst->psBlock, 
							psMoveInst, 
							&psMoveInst->asArg[0], 
							&psMoveInst->asDest[0], 
							NULL /* psEvalList */);
	}

	/*
		Free the alpha channel register number map.
	*/
	DeinitializeRemapTable(psState, &sC10ExpandState.sRemap);

	/*
		Expand COMBC10 instructions which aren't supported directly by the hardware.
	*/
	ForAllInstructionsOfType(psState, ICOMBC10, ExpandCOMBC10);

	/*
		For moves of components of fixed-point vectors: try to substitute sources for
		destinations.
	*/
	ForAllInstructionsOfType(psState, IPCKC10C10, SubstFixedPointComponentMoveCB);
}

static
IMG_VOID AddToNextRefList(PIREGALLOC_CONTEXT psContext, PINTERVAL psInterval)
/*****************************************************************************
 FUNCTION	: AddToNextRefList
    
 PURPOSE	: Add an interval to the list of all intervals sorted by their
			  first reference in the program.

 PARAMETERS	: psContext			- Register allocator state.
			  psInterval		- Interval to add.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Skip references to the intermediate register in the driver prolog/epilog.
	*/
	psInterval->psNextRef = SkipNonInstReferences(psInterval->psUseDefChain->sList.psHead);

	if (psInterval->psNextRef == NULL)
	{
		/*
			No instruction in the program uses the intermediate register.
		*/
		return;
	}

	/*
		Create a list of all intermediate registers sorted by their first reference in the program.
	*/
	InsertInListSorted(&psContext->sNextRefList, CmpNextRef, &psInterval->sNextRefListEntry);
}

typedef enum _ARGUMENT_TYPE
{
	ARGUMENT_TYPE_DEST,
	ARGUMENT_TYPE_OLDDEST,
	ARGUMENT_TYPE_SOURCE
} ARGUMENT_TYPE, *PARGUMENT_TYPE;

static
PARG GetArgFromInst(PINTERMEDIATE_STATE psState, PINST psInst, ARGUMENT_TYPE eArgType, IMG_UINT32 uArgIdx)
/*****************************************************************************
 FUNCTION	: GetArgFromInst
    
 PURPOSE	: Get a pointer to an instruction source or destination argument.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to get one of the arguments for.
			  eArgType			- Type of the argument to get.
			  uArgIdx			- Index of the argument to get.
			  
 RETURNS	: A pointer to the argument.
*****************************************************************************/
{
	switch (eArgType)
	{
		case ARGUMENT_TYPE_DEST:
		{
			ASSERT(uArgIdx < psInst->uDestCount);
			return &psInst->asDest[uArgIdx];
		}
		case ARGUMENT_TYPE_OLDDEST:
		{
			ASSERT(uArgIdx < psInst->uDestCount);
			return psInst->apsOldDest[uArgIdx];
		}
		case ARGUMENT_TYPE_SOURCE:
		{
			ASSERT(uArgIdx < psInst->uArgumentCount);
			return &psInst->asArg[uArgIdx];
		}
		default: imgabort();
	}
}

static
IMG_VOID CopyInstructionArgument(PINTERMEDIATE_STATE	psState,
								 PINST					psInst,
								 ARGUMENT_TYPE			eArgType,
								 IMG_UINT32				uArgIdx,
								 UF_REGFORMAT			eSrcFmt,
								 UF_REGFORMAT			eDestFmt,
								 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
								 IMG_BOOL				bVectorResult,
								 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								 PARG					psNewTemp)
/*****************************************************************************
 FUNCTION	: CopyInstructionArgument

 PURPOSE	: For an instruction source or destination argument insert a copy
			  instruction between the current argument and a newly allocated
			  intermediate register. Then replace the current argument by the
			  new intermediate register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to copy one of the arguments for.
			  eArgType			- Type of the argument to copy.
			  uArgIdx			- Index of the argument to copy.
			  eSrcFmt			- Format of the argument to copy.
			  eDestFmt			- Format for the replacement argument.
			  bVectorResult		- If TRUE just a vector move to copy the argument.
			  psNewTemp			- Returns details of the new argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChansUsedFromCopyResult;
	PINST		psCopyInst;

	/*
		Allocate a new temporary register.
	*/
	MakeNewTempArg(psState, eDestFmt, psNewTemp);

	switch (eArgType)
	{
		case ARGUMENT_TYPE_DEST: 
		{
			uChansUsedFromCopyResult = psInst->auLiveChansInDest[uArgIdx]; 
			break;
		}
		case ARGUMENT_TYPE_OLDDEST:
		{
			uChansUsedFromCopyResult = GetPreservedChansInPartialDest(psState, psInst, uArgIdx);
			break;
		}
		case ARGUMENT_TYPE_SOURCE:
		{
			uChansUsedFromCopyResult = GetLiveChansInArg(psState, psInst, uArgIdx);
			break;
		}
		default: imgabort();
	}

	/*
		Create an instruction to copy from the original instruction source or destination to the new temporary register.
	*/
	psCopyInst = AllocateInst(psState, psInst);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
	if (bVectorResult)
	{
		ASSERT((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0);

		SetOpcode(psState, psCopyInst, IVMOV);
		if (eArgType == ARGUMENT_TYPE_DEST)
		{
			psCopyInst->u.psVec->aeSrcFmt[0] = eDestFmt;
		}
		else
		{
			psCopyInst->u.psVec->aeSrcFmt[0] = eSrcFmt;
		}
		psCopyInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psCopyInst->auDestMask[0] = uChansUsedFromCopyResult;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)  */
	{
		ASSERT(!(eArgType == ARGUMENT_TYPE_DEST && psCopyInst->auDestMask[uArgIdx] != USC_ALL_CHAN_MASK));

		PVR_UNREFERENCED_PARAMETER(eSrcFmt);

		SetOpcode(psState, psCopyInst, IMOV);
	}

	psCopyInst->auLiveChansInDest[0] = uChansUsedFromCopyResult;
	switch (eArgType)
	{
		case ARGUMENT_TYPE_DEST:
		{	
			/*	
				Set the new temporary as a source to the copy instruction.
			*/
			SetSrcFromArg(psState, psCopyInst, 0 /* uDestIdx */, psNewTemp);
			break;
		}
		case ARGUMENT_TYPE_OLDDEST:
		case ARGUMENT_TYPE_SOURCE:
		{
			/*
				Set the new temporary as a destination to the copy instruction.
			*/
			SetDestFromArg(psState, psCopyInst, 0 /* uDestIdx */, psNewTemp);
			break;
		}
		default: imgabort();
	}

	switch (eArgType)
	{
		case ARGUMENT_TYPE_DEST:
		{
			/*
				Move the existing destination argument to the new instruction.
			*/
			MoveDest(psState, psCopyInst, 0 /* uMoveToDestIdx */, psInst, uArgIdx);
			CopyPartiallyWrittenDest(psState, psCopyInst, 0 /* uMoveToDestIdx */, psInst, uArgIdx);

			CopyPredicate(psState, psCopyInst, psInst);

			/*
				Set the new temporary as a destination to the original instruction.
			*/
			SetDestFromArg(psState, psInst, uArgIdx, psNewTemp);
			break;
		}
		case ARGUMENT_TYPE_OLDDEST:
		{
			/*
				Move the source for unwritten channel from the original instruction to
				the source of the copy instruction.
			*/
			MovePartialDestToSrc(psState, psCopyInst, 0 /* uMoveToDestIdx */, psInst, uArgIdx);

			/*
				Set the new temporary as the source for the unwritten channels in the original
				instruction.
			*/
			SetPartiallyWrittenDest(psState, psInst, uArgIdx, psNewTemp);
			break;
		}
		case ARGUMENT_TYPE_SOURCE:
		{
			/*
				Move the existing source argument to the new instruction.
			*/
			MoveSrc(psState, psCopyInst, 0 /* uMoveToSrcIdx */, psInst, uArgIdx);
	
			/*
				Set the new temporary as a source to the original instruction.
			*/
			SetSrcFromArg(psState, psInst, uArgIdx, psNewTemp);
			break;
		}
		default: imgabort();
	}

	if (eArgType == ARGUMENT_TYPE_DEST)
	{
		InsertInstAfter(psState, psInst->psBlock, psCopyInst, psInst);
	}
	else
	{
		InsertInstBefore(psState, psInst->psBlock, psCopyInst, psInst);
	}
}

static
PINTERVAL AllocGlobalInterval(PINTERMEDIATE_STATE	psState,
							  PIREGALLOC_CONTEXT	psContext,
							  INTERVAL_TYPE			eIntervalType,
							  PUSEDEF_CHAIN			psIntervalSrc)
/*****************************************************************************
 FUNCTION	: AllocGlobalInterval

 PURPOSE	: Allocate an internal register allocator data structure for
			  an intermediate register which needs to be assigned an
			  internal register.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Register allocator state.
			  eIntervalType		- Type of data stored in the interval.
			  psIntervalSrc		- Intermediate register.

 RETURNS	: The interval data structure.
*****************************************************************************/
{
	PINTERVAL	psInterval;

	psInterval = UscAlloc(psState, sizeof(*psInterval));
	InitializeInterval(psState, psContext, psInterval, psIntervalSrc, psIntervalSrc); 
	psInterval->eType = eIntervalType;
	AppendToList(&psContext->sIntervalList, &psInterval->sIntervalListEntry);
	AddToNextRefList(psContext, psInterval);

	return psInterval;
}

static
IMG_VOID SetFixedColourArg(PINTERMEDIATE_STATE	psState, 
						   PIREGALLOC_CONTEXT	psContext,
						   PINST				psInst, 
						   ARGUMENT_TYPE		eArgType,
						   IMG_UINT32			uArgIdx, 
						   IMG_UINT32			uFixedColourMask)
/*****************************************************************************
 FUNCTION	: SetFixedColourArg

 PURPOSE	: Tell the register allocator to assign a source or destination 
			  argument to an instruction to only a subset of the available
			  internal registers.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Register allocator state.
			  psInst			- Instruction to set a fixed colour for.
			  eArgType			- Type of the instruction argument to set a fixed
								colour for.
			  uArgIdx			- Index of the source or destination argument to 
								set a fixed colour for.
			  uFixedColourMask	- Subset of internal registers available for
								this argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG				sNewTemp;
	PARG			psArg = GetArgFromInst(psState, psInst, eArgType, uArgIdx);
	PINTERVAL		psInterval;
	PINTERVAL		psNewTempInterval;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
	IMG_BOOL		bVectorResult;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Get the interval for the temporary used in the argument.
	*/
	psInterval = GetIntervalByArg(psContext, psArg);
	ASSERT(psInterval != NULL);

	/*
		Check if the interval can be coloured to at least one of the available
		subset.
	*/
	if ((psInterval->uFixedHwRegMask & uFixedColourMask) != 0)
	{
		psInterval->uFixedHwRegMask &= uFixedColourMask;
		return;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
	/*
		Does the interval need to be copied using a vector move?
	*/
	bVectorResult = IMG_FALSE;
	if (psInterval->eType == INTERVAL_TYPE_GENERAL && (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		bVectorResult = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Move the original argument to a newly inserted copy instruction then
		replace the original argument by the result of the copy.
	*/
	CopyInstructionArgument(psState, 
							psInst, 
							eArgType, 
							uArgIdx, 
							psInterval->psUseDefChain->eFmt,
							psInterval->psUseDefChain->eFmt,
							#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
							bVectorResult,
							#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							&sNewTemp);

	/*
		Reposition the original interval in the list of intervals.
	*/
	RemoveFromList(&psContext->sNextRefList, &psInterval->sNextRefListEntry);
	AddToNextRefList(psContext, psInterval);

	/*
		Allocate a new interval data structure and add it to the list of intervals to be assigned internal registers.
	*/
	psNewTempInterval = AllocGlobalInterval(psState, psContext, psInterval->eType, sNewTemp.psRegister->psUseDefChain);

	/*
		Give the new temporary a fixed colour.
	*/
	psNewTempInterval->uFixedHwRegMask = uFixedColourMask;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
static
IMG_VOID SetupFixedColours(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: SetupFixedColours

 PURPOSE	: Updates the internal register allocator state with details of any
			  temporaries requiring fixed internal registers.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Register allocator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	/*
		Cores without the vector instruction set don't have any instructions which
		require fixed internal registers.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0)
	{
		return;
	}

	/*
		On cores with the vector instruction set if the gradient sources to a 
		SMPGRAD instruction are internal registers then they must have
		consecutive register numbers. This is actually implemented by assigning
		the gradient sources to fixed registers i0 and i1 (the other choice would be
		i1 and i2).
	*/
	InstListIteratorInitialize(psState, ISMPGRAD, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST			psSMPGRADInst;
		UF_REGFORMAT	eGradFmt;

		psSMPGRADInst = InstListIteratorCurrent(&sIter);
		ASSERT(psSMPGRADInst->eOpcode == ISMPGRAD);

		eGradFmt = psSMPGRADInst->asArg[SMP_GRAD_ARG_START].eFmt;
		if (psSMPGRADInst->u.psSmp->uDimensionality == 3 && (eGradFmt == UF_REGFORMAT_F16 || eGradFmt == UF_REGFORMAT_F32))
		{
			IMG_UINT32	uGradIdx;

			ASSERT(psSMPGRADInst->u.psSmp->uGradSize == 2);

			for (uGradIdx = 0; uGradIdx < psSMPGRADInst->u.psSmp->uGradSize; uGradIdx++)
			{
 				SetFixedColourArg(psState, 
								  psContext, 
								  psSMPGRADInst, 
								  ARGUMENT_TYPE_SOURCE,
								  SMP_GRAD_ARG_START + uGradIdx, 
								  1 << uGradIdx);
			}
		}
	}
	InstListIteratorFinalise(&sIter);

	/*
		If dual-issue OP1 has one source then the source register for
		the GPI2 source slot is fixed to i2.
	*/
	InstListIteratorInitialize(psState, IVDUAL, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST		psVDUALInst;
		
		psVDUALInst = InstListIteratorCurrent(&sIter);
		ASSERT(psVDUALInst->eOpcode == IVDUAL);

		if (g_asDualIssueOpDesc[psVDUALInst->u.psVec->sDual.ePriOp].uSrcCount == 1 &&
			psVDUALInst->asArg[VDUAL_SRCSLOT_GPI2 * SOURCE_ARGUMENTS_PER_VECTOR].uType == USEASM_REGTYPE_TEMP)
		{
			SetFixedColourArg(psState, 
							  psContext, 
							  psVDUALInst, 
							  ARGUMENT_TYPE_SOURCE,
							  VDUAL_SRCSLOT_GPI2 * SOURCE_ARGUMENTS_PER_VECTOR, 
							  1 << 2 /* uFixedColour */);
		}
	}
	InstListIteratorFinalise(&sIter);
}

static
IMG_VOID FixInternalSrcRegisterBank(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PINST psInst, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: FixInternalSrcRegisterBank

 PURPOSE	: Fix an instruction source which can't be assigned an internal registers.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Register allocator state.
			  psInst		- Instruction.
			  uSrc			- Source argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERVAL		psSrcInterval;
	PARG			psSrc = &psInst->asArg[uSrc];
	PUSEDEF_CHAIN	psCopyUseDefChain;
	PINST			psCopyDefInst;
	IMG_UINT32		uCopyDefDestIdx;

	/*
		Get the interval for the source argument.
	*/
	psSrcInterval = GetIntervalByArg(psContext, psSrc);

	/*
		Is the source argument going to be assigned an internal register?
	*/
	if (psSrcInterval == NULL)
	{
		return;
	}
					
	/*
		Check if there is already another intermediate register containing
		a copy of the source argument.
	*/
	if (psSrcInterval->uCopyTempNum == USC_UNDEF)
	{
		PUSEDEF		psDef;
		PINST		psSaveInst;
		PCODEBLOCK	psInsertBlock;
		PINST		psInsertPoint;

		/*
			Allocate a new intermediate register.
		*/
		psSrcInterval->uCopyTempNum = GetNextRegister(psState);

		psDef = psSrcInterval->psUseDefChain->psDef;
		if (psDef != NULL && psDef->eType == DEF_TYPE_INST)
		{
			PINST	psDefInst;

			/*
				Insert the copy instruction after the instruction
				defining the source argument.
			*/
			psDefInst = psDef->u.psInst;
			psInsertPoint = psDefInst->psNext;
			psInsertBlock = psDefInst->psBlock;
		}
		else
		{
			/*
				Insert the copy instruction at the start of the
				function which contains the DSX/DSY instruction.
			*/
			psInsertBlock = psInst->psBlock->psOwner->psEntry;
			psInsertPoint = psInsertBlock->psBody;
		}

		psSaveInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psSaveInst, ISAVEIREG);
		psSaveInst->auLiveChansInDest[0] = 0;
		psSaveInst->auDestMask[0] = 0;
		SetDest(psState, 
				psSaveInst, 
				0 /* uDestIdx */, 
				USEASM_REGTYPE_TEMP, 
				psSrcInterval->uCopyTempNum, 
				psSrcInterval->psUseDefChain->eFmt);
		SetSrc(psState,
			   psSaveInst,
			   0 /* uSrcIdx */,
			   USEASM_REGTYPE_TEMP,
			   psSrcInterval->psUseDefChain->uNumber,
			   psSrcInterval->psUseDefChain->eFmt);
		InsertInstBefore(psState, psInsertBlock, psSaveInst, psInsertPoint);
	}

	/*
		Replace the existing source argument by a copy which will never be
		assigned an internal register.
	*/
	SetSrc(psState, 
		   psInst, 
		   uSrc, 
		   USEASM_REGTYPE_TEMP, 
		   psSrcInterval->uCopyTempNum, 
		   psSrcInterval->psUseDefChain->eFmt);

	psCopyUseDefChain = UseDefGet(psState, USEASM_REGTYPE_TEMP, psSrcInterval->uCopyTempNum);
	psCopyDefInst = UseDefGetDefInstFromChain(psCopyUseDefChain, &uCopyDefDestIdx);
	psCopyDefInst->auLiveChansInDest[uCopyDefDestIdx] |= GetLiveChansInArg(psState, psInst, uSrc);
	psCopyDefInst->auDestMask[uCopyDefDestIdx] = psCopyDefInst->auLiveChansInDest[uCopyDefDestIdx];

	RemoveFromList(&psContext->sNextRefList, &psSrcInterval->sNextRefListEntry);
	AddToNextRefList(psContext, psSrcInterval);
}

static
IMG_VOID FixInternalDestRegisterBank(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext, PINST psInst, IMG_UINT32 uDest)
/*****************************************************************************
 FUNCTION	: FixInternalDestRegisterBank

 PURPOSE	: Fix an instruction destination which can't be assigned an internal registers.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Register allocator state.
			  psInst		- Instruction.
			  uDest			- Destination number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERVAL		psDestInterval;
	PARG			psDest = &psInst->asDest[uDest];
	PINST			psRestoreInst;
	IMG_UINT32		uCopyTempNum;

	if (psDest->uType != USEASM_REGTYPE_TEMP)
	{
		return;
	}

	/*
		Get the interval for the source argument.
	*/
	psDestInterval = GetInterval(psContext, psDest->uNumber);

	/*
		Is the destination going to be assigned an internal register?
	*/
	if (psDestInterval == NULL)
	{
		return;
	}
					
	/*
		Allocate a new intermediate register.
	*/
	uCopyTempNum = GetNextRegister(psState);

	/*
		Create an instruction copying from the new intermediate register
		into the original instruction destination.
	*/
	psRestoreInst = AllocateInst(psState, psInst);
	SetOpcode(psState, psRestoreInst, IVMOV);
	psRestoreInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[uDest];
	MoveDest(psState, 
			 psRestoreInst, 
			 0 /* uDestIdx */, 
			 psInst,
			 uDest);
	SetSrc(psState,
		   psRestoreInst,
		   0 /* uSrcIdx */,
		   USEASM_REGTYPE_TEMP,
		   uCopyTempNum, 
		   psDestInterval->psUseDefChain->eFmt);
	psRestoreInst->u.psVec->aeSrcFmt[0] =  psDestInterval->psUseDefChain->eFmt;
	psRestoreInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		
	/*
		Insert the copy instruction after this instruction.
	*/
	InsertInstAfter(psState, psInst->psBlock, psRestoreInst, psInst);

	/*
		Replace the existing destination argument by a copy which will never be
		assigned an internal register.
	*/
	SetDest(psState, 
		    psInst, 
			uDest, 
			USEASM_REGTYPE_TEMP, 
			uCopyTempNum, 
			psDestInterval->psUseDefChain->eFmt);

	/*
		Reposition the original destination in the list of interval sorted by
		the next reference.
	*/
	RemoveFromList(&psContext->sNextRefList, &psDestInterval->sNextRefListEntry);
	AddToNextRefList(psContext, psDestInterval);
}

static
IMG_VOID FixInternalRegisterBanks_Vec(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: FixInternalRegisterBanks_Vec

 PURPOSE	: Fix instructions on vector cores which don't support internal registers as
			  source arguments or destinations.

 PARAMETERS	: psState		- Compiler state.
			  psContext		- Register allocator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IOPCODE	aeSrcNonIRegOpcodes[] = {IVDSX, IVDSY, IVPCKS16FLT_EXP};
	static const IOPCODE	aeDestNonIRegOpcodes[] = {IVPCKFLTU8, IVPCKFLTS8, IVPCKFLTS16, IVPCKFLTU16};
	IMG_UINT32				uIdx;

	for (uIdx = 0; uIdx < (sizeof(aeSrcNonIRegOpcodes) / sizeof(aeSrcNonIRegOpcodes[0])); uIdx++)
	{
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitialize(psState, aeSrcNonIRegOpcodes[uIdx], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psInst;
			IMG_UINT32	uSlot;

			psInst = InstListIteratorCurrent(&sIter);
			for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
			{
				FixInternalSrcRegisterBank(psState, psContext, psInst, uSlot * SOURCE_ARGUMENTS_PER_VECTOR);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
	
	for (uIdx = 0; uIdx < (sizeof(aeDestNonIRegOpcodes) / sizeof(aeDestNonIRegOpcodes[0])); uIdx++)
	{
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitialize(psState, aeDestNonIRegOpcodes[uIdx], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psInst;
			IMG_UINT32	uDest;

			psInst = InstListIteratorCurrent(&sIter);

			if (psInst->asDest[0].eFmt != UF_REGFORMAT_F16)
			{
				continue;
			}

			/*
				A pack from fixed point 8-bit to float does support an
				internal register destination.
			*/
			if (psInst->eOpcode == IVPCKFLTU8 && psInst->u.psVec->bPackScale)
			{
				continue;
			}
				
			/* Uses more than one channel. Not allowed for f32 (GPI) dest type. */
			if ((psInst->auDestMask[0] & ~USC_X_CHAN_MASK) == 0)
			{
				continue;
			}

			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				FixInternalDestRegisterBank(psState, psContext, psInst, uDest);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
}

static
IMG_VOID FixF32OnlyArgumentInsts(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixF32OnlyArgumentInsts

 PURPOSE	: Fix instructions on vector cores which support only F32 format
			  sources and destinations.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IOPCODE	aeF32OnlyOpcodes[] = {IVDP2, IVDP_CP};
	IMG_UINT32				uOpType;

	for (uOpType = 0; uOpType < (sizeof(aeF32OnlyOpcodes) / sizeof(aeF32OnlyOpcodes[0])); uOpType++)
	{
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitialize(psState, aeF32OnlyOpcodes[uOpType], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psInst;
			IMG_UINT32	uSlot;

			psInst = InstListIteratorCurrent(&sIter);
			for (uSlot = 0; uSlot < GetSwizzleSlotCount(psState, psInst); uSlot++)
			{
				if (psInst->u.psVec->aeSrcFmt[uSlot] == UF_REGFORMAT_F16)
				{
					ARG	sNewTemp;

					CopyInstructionArgument(psState,
											psInst,
											ARGUMENT_TYPE_SOURCE,
											uSlot * SOURCE_ARGUMENTS_PER_VECTOR,
											UF_REGFORMAT_F16 /* eSrcFmt */,
											UF_REGFORMAT_F32 /* eDestFmt */,
											IMG_TRUE /* bVectorResult */,
											&sNewTemp);
					psInst->u.psVec->aeSrcFmt[uSlot] = UF_REGFORMAT_F32;
				}
			}
			if (psInst->asDest[0].eFmt == UF_REGFORMAT_F16)
			{
				ARG	sNewTemp;

				CopyInstructionArgument(psState,
										psInst,
										ARGUMENT_TYPE_DEST,
										0 /* uArgIdx */,
										UF_REGFORMAT_F16 /* eSrcFmt */,
										UF_REGFORMAT_F32 /* eDestFmt */,
										IMG_TRUE /* bVectorResult */,
										&sNewTemp);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID AddCarryInterval(PINTERMEDIATE_STATE	psState, 
						  PIREGALLOC_CONTEXT	psContext, 
						  PINST					psInst,
						  ARGUMENT_TYPE			eArgType,
						  IMG_UINT32			uArgIdx,
						  IMG_UINT32			uCarryRegNumRange)
/*****************************************************************************
 FUNCTION	: AddCarryInterval
    
 PURPOSE	: For an integer arithmetic instruction reading or writing a
			  carry mark that the intermediate register for the carry must be
			  assigned an internal register.

 PARAMETERS	: psState	- Compiler state.
			  psContext	- Register allocator state.
			  psInst	- Instruction reading or writing the carry.
			  eArgType	- Type of the argument which is a carry.
			  uArgIdx	- Index of the argument which is a carry.
			  uCarryRegNumRange
						- Length of the range of hardware register numbers supported by this
						instruction for a carry argument.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG psCarryArg = GetArgFromInst(psState, psInst, eArgType, uArgIdx);

	if (psCarryArg->uType == USC_REGTYPE_UNUSEDDEST || psCarryArg->uType == USC_REGTYPE_UNUSEDSOURCE)
	{
		return;
	}

	/*
		Only intermediate temporary registers can be assigned internal registers so for other
		register types insert an instruction to copy from the original source/destination to
		a new intermediate register.
	*/
	if (psCarryArg->uType != USEASM_REGTYPE_TEMP)
	{
		ARG	sNewTemp;

		CopyInstructionArgument(psState, 
							    psInst, 
								eArgType, 
								uArgIdx, 
								psCarryArg->eFmt,
								psCarryArg->eFmt,
								#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) 
								IMG_FALSE /* bVectorResult */, 
								#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								&sNewTemp);
		psCarryArg = GetArgFromInst(psState, psInst, eArgType, uArgIdx);
	}

	/*
		Add an interval data structure for the carry source/destination argument.
	*/
	AllocGlobalInterval(psState, psContext, INTERVAL_TYPE_CARRY, psCarryArg->psRegister->psUseDefChain);

	/*
		Flag where the carry source/destination can be assigned a limited range of
		internal registers.
	*/
	if (uCarryRegNumRange < psContext->uNumHwRegs)
	{
		SetFixedColourArg(psState, psContext, psInst, eArgType, uArgIdx, (1U << uCarryRegNumRange) - 1);
	}
}

static
IMG_VOID AddCarryInternalRegisters(PINTERMEDIATE_STATE psState, PIREGALLOC_CONTEXT psContext)
/*****************************************************************************
 FUNCTION	: AddCarryInternalRegisters
    
 PURPOSE	: For all integer arithmetic instruction reading or writing a
			  carry, mark that the intermediate register for the carry must be
			  assigned an internal register.

 PARAMETERS	: psState	- Compiler state.
			  psContext	- Register allocator state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		/* Opcode of the instruction which can read or write a carry flag. */
		IOPCODE		eOpcode;
		/* Index of the destination to the intermediate instruction which represents the carry result. */
		IMG_UINT32	uCarryDest;
		/* Index of the source to the intermediate instruction which represents the carry input. */
		IMG_UINT32	uCarrySrc;
		/* Maximum hardware register number for a carry source or destination to this instruction. */
		IMG_UINT32	uCarryMaxHwRegNum;
	} asCarryWriters[] = 
	{
		/* eOpcode		uCarryDest				uCarrySrc					uCarryMaxHwRegNum				*/
		#if defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		{IIMA32,	IMA32_HIGHDWORD_DEST,		USC_UNDEF,				EURASIA_USE1_IMA32_GPISRC_MAX		},
		#endif /* defined(SUPPORT_SGX545) || defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{IIMAE,		IMAE_CARRYOUT_DEST,			IMAE_CARRYIN_SOURCE,	EURASIA_USE1_IMAE_CISRC_MAX			}
	};
	IMG_UINT32	uCarryType;

	for (uCarryType = 0; uCarryType < (sizeof(asCarryWriters) / sizeof(asCarryWriters[0])); uCarryType++)
	{
		IMG_UINT32			uCarryDest = asCarryWriters[uCarryType].uCarryDest;
		IMG_UINT32			uCarrySrc = asCarryWriters[uCarryType].uCarrySrc;
		IMG_UINT32			uCarryMaxHwRegNum = asCarryWriters[uCarryType].uCarryMaxHwRegNum;
		SAFE_LIST_ITERATOR	sIter;

		InstListIteratorInitialize(psState, asCarryWriters[uCarryType].eOpcode, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST	psCarryInst;
			
			psCarryInst = InstListIteratorCurrent(&sIter);

			if (uCarryDest < psCarryInst->uDestCount)
			{
				PARG	psOldDest;

				/*
					Mark that the carry destination must be assigned an internal register.
				*/
				AddCarryInterval(psState, psContext, psCarryInst, ARGUMENT_TYPE_DEST, uCarryDest, uCarryMaxHwRegNum + 1);

				/*
					Mark that the source for unwritten channels in the destination must be
					assigned an internal register.
				*/
				psOldDest = psCarryInst->apsOldDest[uCarryDest];
				if (psOldDest != NULL)
				{
					AddCarryInterval(psState, 
									 psContext, 
									 psCarryInst,
									 ARGUMENT_TYPE_OLDDEST,
									 uCarryDest,
									 psContext->uNumHwRegs);
				}
			}
			
			/*
				Mark that the carry source must be assigned an internal register.
			*/
			if (uCarrySrc != USC_UNDEF)
			{
				ASSERT(uCarrySrc < psCarryInst->uArgumentCount);
				AddCarryInterval(psState, 
								 psContext, 
								 psCarryInst,
								 ARGUMENT_TYPE_SOURCE,
								 uCarrySrc,
								 uCarryMaxHwRegNum + 1);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
}

IMG_INTERNAL 
IMG_VOID AssignInternalRegisters(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignInternalRegisters
    
 PURPOSE	: Assign internal registers for variables.

 PARAMETERS	: psState	- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY		psListEntry;
	PUSC_LIST			psAssignList;
	PUSEDEF_CHAIN		(*pfTempFromListEntry)(PINTERMEDIATE_STATE, PUSC_LIST_ENTRY);
	IREGALLOC_CONTEXT	sContext;
	IOPCODE				eMoveOpcode;
	IMG_BOOL			(*pfIsMove)(PINST);
	SAFE_LIST_ITERATOR	sIter;

	InitializeList(&sContext.sNextRefList);
	InitializeList(&sContext.sIntervalList);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psAssignList = &psState->sVectorTempList;
		pfTempFromListEntry = TempFromListEntryVec;
		sContext.uNumHwRegs = psState->psTargetFeatures->ui32NumInternalRegisters;
		sContext.pfMergeRestoreSource = MergeRestoreVectorSource;
		sContext.pfMergeSaveDest = MergeSaveVectorDestination;
		sContext.pfExpandSave = ExpandIRegSaveVector;
		sContext.pfExpandRestore = ExpandIRegRestoreVector;
		sContext.pfSplitIRegDest = SplitIRegDestVector;
		sContext.auPartMasks = g_auVectorPartMasks;
		sContext.psUpconvertedSecAttrs = NULL;
		InitializeList(&sContext.sUpconvertInstList);

		eMoveOpcode = IVMOV;
		pfIsMove = IsVMOV;

		/*
			Fix instructions on vector cores which supports only internal
			registers for some arguments but we can't assign internal
			registers to F16 intermediate registers because of invariance.
		*/
		if (psState->bInvariantShader)
		{
			FixF32OnlyArgumentInsts(psState);
		}

		/*
			The DSX/DSY instructions cause a deschedule so we need to fix them not to read/write more
			data than can be referenced from non-internal register sources.
		*/
		FixDSXDSY(psState);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psAssignList = &psState->sC10TempList;
		pfTempFromListEntry = TempFromListEntryC10;
		sContext.uNumHwRegs = NUM_C10_INTERNAL_REGISTERS;
		sContext.pfMergeRestoreSource = MergeRestoreC10Source;
		sContext.pfMergeSaveDest = MergeSaveC10Destination;
		sContext.pfExpandSave = ExpandIRegSaveC10;
		sContext.pfExpandRestore = ExpandIRegRestoreC10;
		sContext.pfSplitIRegDest = SplitIRegDestC10;
		sContext.auPartMasks = g_auC10PartMasks;

		eMoveOpcode = ISOPWM;
		pfIsMove = IsSOPWMMove;
	}

	sContext.uAllHwRegMask = (1U << sContext.uNumHwRegs) - 1;
	sContext.psIntervals = NULL;

	/*
		Ensure if any of the registers we might assign to an internal registers are partially/conditionally
		written that the source for unwritten channels is also a temporary.
	*/
	for (psListEntry = psAssignList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psAssignTemp = pfTempFromListEntry(psState, psListEntry);

		if (psAssignTemp != NULL)
		{
			PINST		psDefInst;
			IMG_UINT32	uDestIdx;

			psDefInst = UseDefGetDefInst(psState, USEASM_REGTYPE_TEMP, psAssignTemp->uNumber, &uDestIdx);

			if (psDefInst != NULL)
			{
				PARG	psOldDest = psDefInst->apsOldDest[uDestIdx];
				
				if (psOldDest != NULL && psOldDest->uType != USEASM_REGTYPE_TEMP)
				{
					CopyPartialDest(psState, 
									&sContext, 
									NULL /* psBlockState */, 
									psDefInst, 
									uDestIdx,
									IMG_TRUE /* bBefore */);
				}
			}
		}
	}

	/*
		Allocate storage for each intermediate register which is too wide to be assigned a hardware
		temporary register (depending on the core either a vec4 C10 or a vec4 F32).
	*/
	for (psListEntry = psAssignList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psAssignTemp = pfTempFromListEntry(psState, psListEntry);
		
		if (psAssignTemp != NULL)
		{
			(IMG_VOID)AllocGlobalInterval(psState, &sContext, INTERVAL_TYPE_GENERAL, psAssignTemp);
		}
	}

	/*
		Also add intermediate registers which are the carry source/destination to an integer arithmetic
		instruction.
	*/
	AddCarryInternalRegisters(psState, &sContext);
	
	/*
		Return early if nothing needs to be assigned an internal register.
	*/
	if (IsListEmpty(&sContext.sIntervalList))
	{
		return;
	}

	/*
		Allocate storage about the interval assigned to each hardware register.
	*/
	sContext.asHwReg = UscAlloc(psState, sizeof(sContext.asHwReg[0]) * sContext.uNumHwRegs);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Process instructions which require their sources to be assigned a fixed internal register.
	*/
	SetupFixedColours(psState, &sContext);

	/*
		Process instructions which don't support internal register sources.
	*/
	FixInternalRegisterBanks_Vec(psState, &sContext);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Assign internal registers for all basic blocks.
	*/
	for (psListEntry = psState->sBlockList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PCODEBLOCK	psBlock;

		psBlock = IMG_CONTAINING_RECORD(psListEntry, PCODEBLOCK, sBlockListEntry);
		AssignInternalRegistersBlock(psState, &sContext, psBlock);
	}

	/*
		Free interval information.
	*/
	while ((psListEntry = RemoveListHead(&sContext.sIntervalList)) != NULL)
	{
		PINTERVAL	psTempInterval;

		psTempInterval = IMG_CONTAINING_RECORD(psListEntry, PINTERVAL, sIntervalListEntry);
		UscFree(psState, psTempInterval);
	}

	/*	
		Free hardware register information.
	*/
	UscFree(psState, sContext.asHwReg);

	/*
		Delete the map between temporary register numbers and interval information.
	*/
	IntermediateRegisterMapDestroy(psState, sContext.psIntervals);
	sContext.psIntervals = NULL;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		PUSC_LIST_ENTRY	psNextListEntry;

		/*
			Remove conversion instructions writing unused parts of a vector.
		*/
		for (psListEntry = sContext.sUpconvertInstList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
		{
			PINST	psUpconvertInst;

			psNextListEntry = psListEntry->psNext;
			psUpconvertInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
			
			ASSERT(psUpconvertInst->eOpcode == IVMOV);
			ASSERT(psUpconvertInst->psBlock->psOwner == &psState->psSecAttrProg->sCfg);

			if ((psUpconvertInst->auDestMask[0] & psUpconvertInst->auLiveChansInDest[0]) == 0)
			{
				CopyPartiallyOverwrittenData(psState, psUpconvertInst);

				RemoveInst(psState, psUpconvertInst->psBlock, psUpconvertInst);
				FreeInst(psState, psUpconvertInst);
			}
			else
			{
				psUpconvertInst->auDestMask[0] &= psUpconvertInst->auLiveChansInDest[0];
				if (GetPreservedChansInPartialDest(psState, psUpconvertInst, 0 /* uDestIdx */) == 0)
				{
					SetPartiallyWrittenDest(psState, psUpconvertInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
				}
			}
		}

		/*
			Delete the map between the temporary register numbers of secondary attributes
			containing F16 data and an instruction converting that secondary attribute
			to F32.
		*/
		if (sContext.psUpconvertedSecAttrs != NULL)
		{
			IntermediateRegisterMapDestroy(psState, sContext.psUpconvertedSecAttrs);
			sContext.psUpconvertedSecAttrs = NULL;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Separate C10 format temporary registers into different registers for the RGB channels and the
		ALPHA channel.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psState->uFlags |= USC_FLAGS_POSTC10REGALLOC;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		ExpandC10Temporaries(psState);
	}

	/*
		If we ended up generating any moves between intermediate registers then replace the
		destination by the source.
	*/
	InstListIteratorInitialize(psState, eMoveOpcode, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psMoveInst;

		psMoveInst = InstListIteratorCurrent(&sIter);

		if (!NoPredicate(psState, psMoveInst))
		{
			continue;
		}
		/*
			Skip moves between non-temporary registers.
		*/
		if (psMoveInst->asArg[0].uType != USEASM_REGTYPE_TEMP)
		{
			continue;
		}
		if (psMoveInst->asDest[0].uType != USEASM_REGTYPE_TEMP)
		{
			continue;
		}
		if (psMoveInst->asArg[0].eFmt != psMoveInst->asDest[0].eFmt)
		{
			continue;
		}

		/*
			Check all channels used from the move destination are coming from the same source.
		*/
		if (psMoveInst->auDestMask[0] != psMoveInst->auLiveChansInDest[0])
		{
			if (!(psMoveInst->apsOldDest[0] != NULL && EqualArgs(psMoveInst->apsOldDest[0], &psMoveInst->asArg[0])))
			{
				continue;
			}
		}

		/*
			For pure moves (without any source modifiers) just do a straightforward substitute of
			the move source for the move destination.
		*/
		if (pfIsMove(psMoveInst))
		{
			if (UseDefSubstituteIntermediateRegister(psState,
													 &psMoveInst->asDest[0],
													 &psMoveInst->asArg[0],
													 #if defined(SRC_DEBUG)
													 psMoveInst->uAssociatedSrcLine,
													 #endif /* defined(SRC_DEBUG) */
													 IMG_TRUE /* bExcludePrimaryEpilog */,
													 IMG_TRUE /* bExcludeSecondaryEpilog */,
													 IMG_FALSE))
			{
				RemoveInst(psState, psMoveInst->psBlock, psMoveInst);
				FreeInst(psState, psMoveInst);
				continue;
			}
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			For cores with the vector instruction set: in an earlier phase we might have
			introduced moves to apply swizzles where an instruction doesn't support a particular swizzle
			on one of its sources. 
			
			If we've split up instructions in order to spill internal registers then we might be able
			to substitute swizzles applied by move instructions back into the instructions
			using the move instruction's destination.
		*/
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			if (!psMoveInst->u.psVec->asSrcMod[0].bNegate && 
				!psMoveInst->u.psVec->asSrcMod[0].bAbs &&
				psMoveInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
			{
				SUBSTSWIZZLE_CONTEXT	sSubstContext;

				/*
					Neither the move source or destination are internal registers so we
					can substitute in instructions in any block.
				*/
				sSubstContext.psSubstBlock = NULL;
				sSubstContext.uLastSubstPoint = USC_UNDEF;

				/*
					Get the move instruction's source swizzle taking into account we might be
					selecting only the top-half of a vector source/destination.
				*/
				sSubstContext.uSubstSwizzle = GetEffectiveVMOVSwizzle(psMoveInst);

				sSubstContext.eSourceFormat = psMoveInst->u.psVec->aeSrcFmt[0];
				sSubstContext.bSourceIsIReg = IMG_FALSE;
				sSubstContext.psAllocContext = NULL;

				/*
					Check for all the instruction using the move's destination if we can
					substitute the move instruction's swizzle.
				*/
				DoGlobalMoveReplace(psState,	
									psMoveInst->psBlock, 
									psMoveInst, 
									&psMoveInst->asArg[0], 
									&psMoveInst->asDest[0], 
									SubstSwizzle,
									&sSubstContext);
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	InstListIteratorFinalise(&sIter);
}

/* EOF */
