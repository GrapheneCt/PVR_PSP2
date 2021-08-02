/*************************************************************************
 * Name         : regpack.c
 * Title        : Eurasia USE Compiler
 * Created      : Dec 2005
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
 *
 * Modifications:-
 * $Log: regpack.c $
 * 
 *  --- Revision Logs Removed --- 
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"
#include "usedef.h"
#include "usetab.h"

static IMG_VOID AddSimpleSecAttr(PINTERMEDIATE_STATE	psState,
								 IMG_UINT32				uAlignment,
							     IMG_UINT32				uNum,
								 UNIFLEX_CONST_FORMAT	eFormat,
								 IMG_UINT32				uBuffer,
								 PINREGISTER_CONST*		ppsConst);
static
PREGISTER_GROUP AddSAProgResultGroup(PINTERMEDIATE_STATE	psState,
									 PSAPROG_RESULT			psResult);

typedef struct _VARIABLE_SA_PAIR
{
	/*
		Index of the variable mapped to this secondary attribute.
	*/
	IMG_UINT32		uVariable;
	/*
		Describes the data loaded into the secondary attribute.
	*/
	PSAPROG_RESULT	psSecAttr;
} VARIABLE_SA_PAIR, *PVARIABLE_SA_PAIR;

typedef enum _ALLOCATION_PRIORITY
{
	ALLOCATION_PRIORITY_HIGH = 0,
	ALLOCATION_PRIORITY_MEDIUM = 1,
	ALLOCATION_PRIORITY_DEFAULT = 2,
	ALLOCATION_PRIORITY_LOW = 3,
	ALLOCATION_PRIORITY_COUNT = 4,
} ALLOCATION_PRIORITY;

/*
	Information about a single secondary attribute.
*/
typedef struct _UNIFORM_SECATTR
{
	/*
		Information about any restrictions on the hardware register number;
	*/
	PREGISTER_GROUP				psGroup;
	/*
		Priority for allocation. Higher priority secondary attribute get smaller register numbers.
	*/
	ALLOCATION_PRIORITY			ePriority;
	/*
		Entry in the list of secondary attributes with the same alignment and psPrevReg == NULL.
	*/
	USC_LIST_ENTRY				sAlignListEntry;
	/*
		Entry in the list of secondary attributes for which we ignored restrictions on hardware
		register order/alignment.
	*/
	USC_LIST_ENTRY				sIgnoredRestrictionsListEntry;
	/*
		Length of the group of secondary attributes which have to be given hardware register numbers
		consecutive with this one.
	*/
	IMG_UINT32					uLength;
	/*
		Points to the information about the input constant to be loaded into the secondary attribute
		by the driver.
	*/
	PINREGISTER_CONST			psConst;
} UNIFORM_SECATTR, *PUNIFORM_SECATTR;

/*
	List of secondary attributes sharing the same alignment restriction.
*/
typedef struct _UNIFORM_SECATTR_ALIGN_LISTS
{
	/*
		Lists for ranges of even and odd length in each allocation priority.
	*/
	USC_LIST	aasLists[2][ALLOCATION_PRIORITY_COUNT];
} UNIFORM_SECATTR_ALIGN_LISTS, *PUNIFORM_SECATTR_ALIGN_LISTS;

/*
	Information about all secondary attributes.
*/
typedef struct _UNIFORM_SECATTR_STATE
{
	/*
		Lists of secondary attributes sharing the same alignment restriction and without any other secondary
		attribute needing to be given a lower hardware register number.
	*/
	UNIFORM_SECATTR_ALIGN_LISTS		asAlignLists[HWREG_ALIGNMENT_COUNT];
} UNIFORM_SECATTR_STATE, *PUNIFORM_SECATTR_STATE;

static
IMG_BOOL IsGroupHead(PUNIFORM_SECATTR psReg)
/*****************************************************************************
 FUNCTION	: IsGroupHead

 PURPOSE    : Check if a secondary attribute has no other secondary attributes
			  which require a lower hardware register number.

 PARAMETERS	: psReg		- Secondary attribute to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psReg->psGroup == NULL || psReg->psGroup->psPrev == NULL) ? IMG_TRUE : IMG_FALSE;
}

typedef struct _SIZE_AND_ALIGN_LIST
{
	USC_LIST	aasLists[HWREG_ALIGNMENT_COUNT][2];
} SIZE_AND_ALIGN_LIST, *PSIZE_AND_ALIGN_LIST;

static
IMG_VOID InitSizeAndAlignList(PSIZE_AND_ALIGN_LIST	psList)
/*****************************************************************************
 FUNCTION	: InitSizeAndAlignList

 PURPOSE    : Initialize a list of registers of the same priority.

 PARAMETERS	: psList	- List to initialize.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uAlignType;

	for (uAlignType = 0; uAlignType < HWREG_ALIGNMENT_COUNT; uAlignType++)
	{
		IMG_UINT32	uLengthType;

		for (uLengthType = 0; uLengthType < 2; uLengthType++)
		{
			InitializeList(&psList->aasLists[uAlignType][uLengthType]);
		}
	}
}

static
PUSC_LIST_ENTRY GetPriorityEntry(HWREG_ALIGNMENT		eAlignment,
								 IMG_UINT32				uPriorityCount,
								 PSIZE_AND_ALIGN_LIST	apsPriorityLists[])
/*****************************************************************************
 FUNCTION	: GetPriorityEntry

 PURPOSE    : Look for the highest priority register of a fixed start alignment.

 PARAMETERS	: eAlignment		- Required start alignment.
			  uPriorityCount	- Count of possible priorities.
			  apsPriorityLists	- List of registers for each possible priority.

 RETURNS	: A pointer to the register or NULL if none of that alignment start
			  available.
*****************************************************************************/
{
	IMG_UINT32	uPriorityIdx;

	for (uPriorityIdx = 0; uPriorityIdx < uPriorityCount; uPriorityIdx++)
	{
		PSIZE_AND_ALIGN_LIST	psPriorityList = apsPriorityLists[uPriorityIdx];
		IMG_UINT32				uLengthMod2;

		for (uLengthMod2 = 0; uLengthMod2 < 2; uLengthMod2++)
		{
			PUSC_LIST	psList = &psPriorityList->aasLists[eAlignment][uLengthMod2];

			if (!IsListEmpty(psList))
			{
				return RemoveListHead(psList);
			}
		}
	}
	return NULL;
}

static
PUSC_LIST_ENTRY GetPriorityEntryOfLength(HWREG_ALIGNMENT		eAlignment,
										 IMG_UINT32				uPriorityCount,
										 PSIZE_AND_ALIGN_LIST	apsPriorityLists[],
										 IMG_UINT32				uLengthMod2)
/*****************************************************************************
 FUNCTION	: GetPriorityEntryOfLength

 PURPOSE    : Look for the highest priority register of a fixed start alignment
			  and length alignment.

 PARAMETERS	: eAlignment		- Required start alignment.
			  uPriorityCount	- Count of possible priorities.
			  apsPriorityLists	- List of registers for each possible priority.
			  uLengthMod2		- Required length alignment.

 RETURNS	: A pointer to the register or NULL if none of that start alignment
			  and length alignment are available.
*****************************************************************************/
{
	IMG_UINT32	uPriorityIdx;

	for (uPriorityIdx = 0; uPriorityIdx < uPriorityCount; uPriorityIdx++)
	{
		PSIZE_AND_ALIGN_LIST	psPriorityList = apsPriorityLists[uPriorityIdx];
		PUSC_LIST				psList = &psPriorityList->aasLists[eAlignment][uLengthMod2];

		if (!IsListEmpty(psList))
		{
			return RemoveListHead(psList);
		}
	}
	return NULL;
}

static
PUSC_LIST_ENTRY AssignInputOutputRegisters(PINTERMEDIATE_STATE	psState,
										   IMG_UINT32			uNextHWReg,
										   IMG_BOOL				bPaddingAvailable,
										   IMG_UINT32			uPriorityCount,
										   PSIZE_AND_ALIGN_LIST	apsPriorityLists[],
										   IMG_PBOOL			pbAddPadding)
/*****************************************************************************
 FUNCTION	: AssignInputOutputRegisters

 PURPOSE    : Assign hardware register numbers to shader inputs or outputs where
			  the inputs or outputs have restrictions on alignment.

 PARAMETERS	: psState		- Compiler state.
			  uNextHWReg	- Number of the next available hardware register.
			  bPaddingAvailable
							- TRUE if some hardware registers can be skipped.
			  uPriority		- Count of possible priorities.
			  apsPriorityLists
							- List of unassigned registers in order of priority.
			  pbAddPadding	- On return: TRUE if some hardware registers must
							be skipped to allow the next assigned registers to
							start at the current alignment.

 RETURNS	: A pointer to the register to be given the next hardware register.
*****************************************************************************/
{
	IMG_UINT32						uAlignIdx;
	static const HWREG_ALIGNMENT	aeEvenOrder[] = {HWREG_ALIGNMENT_EVEN,	HWREG_ALIGNMENT_NONE, HWREG_ALIGNMENT_ODD};
	static const HWREG_ALIGNMENT	aeOddOrder[] = {HWREG_ALIGNMENT_ODD,	HWREG_ALIGNMENT_NONE, HWREG_ALIGNMENT_EVEN};
	static const IMG_UINT32			auEvenFirst[] = {0, 1};
	static const IMG_UINT32			auOddFirst[] = {1, 0};
	HWREG_ALIGNMENT const*			peOrder;
	IMG_UINT32 const*				puLengthOrder;
	PUSC_LIST_ENTRY					psListEntry;

	/*
		Start with the list of entries whose required alignment matches the
		alignment of the next available hardware register number.
	*/
	if ((uNextHWReg % 2) == 0)
	{
		peOrder = aeEvenOrder;
		puLengthOrder = auEvenFirst;
	}
	else
	{
		peOrder = aeOddOrder;
		puLengthOrder = auOddFirst;
	}

	*pbAddPadding = IMG_FALSE;
	for (uAlignIdx = 0; uAlignIdx < 2; uAlignIdx++)
	{
		psListEntry = GetPriorityEntry(peOrder[uAlignIdx], uPriorityCount, apsPriorityLists);
		if (psListEntry != NULL)
		{
			return psListEntry;
		}
	}

	if (bPaddingAvailable)
	{
		/*
			If no registers of the correct alignment were available then return an
			misaligned register and skip some hardware registers.
		*/
		*pbAddPadding = IMG_TRUE;
		psListEntry = GetPriorityEntry(peOrder[2], uPriorityCount, apsPriorityLists);
		ASSERT(psListEntry != NULL);
		return psListEntry;
	}
	else
	{
		IMG_UINT32	uLengthTypeIdx;

		/*	
			Try to return a misaligned register whose length will make the
			next register start aligned.
		*/
		for (uLengthTypeIdx = 0; uLengthTypeIdx < 2; uLengthTypeIdx++)
		{
			psListEntry = GetPriorityEntryOfLength(peOrder[2], 
												   uPriorityCount, 
												   apsPriorityLists, 
												   puLengthOrder[uLengthTypeIdx]);
			if (psListEntry != NULL)
			{
				return psListEntry;
			}
		}
		imgabort();
	}
}

static
IMG_VOID RemoveUniformSecAttrFromAlignList(PINTERMEDIATE_STATE		psState,
										   PUNIFORM_SECATTR_STATE	psUniformSAState,
										   PUNIFORM_SECATTR			psReg)
/*****************************************************************************
 FUNCTION	: RemoveUniformSecAttrFromAlignList

 PURPOSE    : Remove a secondary attribute from the appropriate list for
			  its alignment requirement and allocation priority.

 PARAMETERS	: psState			- Compiler state.
			  psUniformSAState	- Pass specific state.
			  psReg				- Secondary attribute to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFORM_SECATTR_ALIGN_LISTS	psLists;
	HWREG_ALIGNMENT					eAlign;

	ASSERT(IsGroupHead(psReg));

	eAlign = GetNodeAlignment(psReg->psGroup);
	ASSERT(eAlign < HWREG_ALIGNMENT_COUNT);
	psLists = &psUniformSAState->asAlignLists[eAlign];

	ASSERT(psReg->ePriority < ALLOCATION_PRIORITY_COUNT);
	RemoveFromList(&psLists->aasLists[(psReg->uLength % 2)][psReg->ePriority], &psReg->sAlignListEntry);
}

static
IMG_INT32 UniformSACmp(PUSC_LIST_ENTRY	psElem1Entry, PUSC_LIST_ENTRY	psElem2Entry)
/*****************************************************************************
 FUNCTION	: UniformSACmp

 PURPOSE    : Comparison function for inserting secondary attributes in a sorted
			  list.

 PARAMETERS	: psElem1Entry, psElem2Entry	- List elements to compare.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFORM_SECATTR	psElem1 = IMG_CONTAINING_RECORD(psElem1Entry, PUNIFORM_SECATTR, sAlignListEntry);
	PUNIFORM_SECATTR	psElem2 = IMG_CONTAINING_RECORD(psElem2Entry, PUNIFORM_SECATTR, sAlignListEntry);

	/* Sort the elements by length. */
	return (IMG_INT32)(psElem2->uLength - psElem1->uLength);
}

static
IMG_VOID AddUniformSecAttrToAlignList(PINTERMEDIATE_STATE		psState,
									  PUNIFORM_SECATTR_STATE	psUniformSAState,
									  PUNIFORM_SECATTR			psReg)
/*****************************************************************************
 FUNCTION	: AddUniformSecAttrToAlignList

 PURPOSE    : Add a secondary attribute to the appropriate list for
			  its alignment requirement and allocation priority.

 PARAMETERS	: psState			- Compiler state.
			  psUniformSAState	- Pass specific state.
			  psReg				- Secondary attribute to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFORM_SECATTR_ALIGN_LISTS	psLists;
	HWREG_ALIGNMENT					eAlign;

	ASSERT(IsGroupHead(psReg));

	eAlign = GetNodeAlignment(psReg->psGroup);
	ASSERT(eAlign < HWREG_ALIGNMENT_COUNT);
	psLists = &psUniformSAState->asAlignLists[eAlign];

	ASSERT(psReg->ePriority < ALLOCATION_PRIORITY_COUNT);
	InsertInListSorted(&psLists->aasLists[(psReg->uLength % 2)][psReg->ePriority],
					   UniformSACmp,
					   &psReg->sAlignListEntry);
}

static
IMG_VOID SetUniformSecAttrPriority(PINTERMEDIATE_STATE		psState,
								   PUNIFORM_SECATTR_STATE	psUniformSAState,
								   PUNIFORM_SECATTR			psReg, 
								   ALLOCATION_PRIORITY		ePriority)
/*****************************************************************************
 FUNCTION	: SetUniformSecAttrPriority

 PURPOSE    : Set the allocation priority for a secondary attribute.

 PARAMETERS	: psState			- Compiler state.
			  psUniformSAState	- Pass specific state.
			  psReg				- Secondary attribute whose priority is
								to be set.
			  ePriority			- Allocation priority to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psReg->ePriority != ePriority)
	{
		RemoveUniformSecAttrFromAlignList(psState, psUniformSAState, psReg);

		psReg->ePriority = ePriority;
		AddUniformSecAttrToAlignList(psState, psUniformSAState, psReg);
	}
}

static IMG_UINT32 GetUnderlyingRegisterNumber(PINTERMEDIATE_STATE	psState,
											  PARG					psArg)
/*****************************************************************************
 FUNCTION	: GetUnderlyingRegisterNumber
    
 PURPOSE	: Get the register number from an argument ignoring arrays.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Argument.

 RETURNS	: None.
*****************************************************************************/
{
	if (psArg->uType == USEASM_REGTYPE_TEMP)
	{
		return psArg->uNumber;
	}
	else
	{
		PUSC_VEC_ARRAY_REG	psArray;
		IMG_UINT32			uArrayOffsetInElements;

		ASSERT(psArg->uType == USC_REGTYPE_REGARRAY);

		ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
		psArray = psState->apsVecArrayReg[psArg->uNumber];

		uArrayOffsetInElements = (psArg->uArrayOffset * psArray->uChannelsPerDword) / VECTOR_LENGTH;
		ASSERT(uArrayOffsetInElements < psArray->uRegs);
		
		return psArray->uBaseReg + uArrayOffsetInElements;
	}
}

IMG_INTERNAL
IMG_BOOL IsDriverLoadedSecAttr(PINTERMEDIATE_STATE psState, const ARG *psArg)
/*****************************************************************************
 FUNCTION	: IsDriverLoadedSecAttr

 PURPOSE    : Check if a source argument is a secondary attribute loaded with
			  data by the driver.

 PARAMETERS	: psState			- Compiler state.
			  psArg				- Argument to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psArg->uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			if (psArg->psRegister != NULL && psArg->psRegister->psSecAttr != NULL)
			{
				PSAPROG_RESULT	psSecAttr = psArg->psRegister->psSecAttr;
				if (psSecAttr->eType == SAPROG_RESULT_TYPE_DRIVERLOADED || 
					psSecAttr->eType == SAPROG_RESULT_TYPE_DIRECTMAPPED)
				{
					return IMG_TRUE;
				}
			}
			return IMG_FALSE;
		}
		case USC_REGTYPE_REGARRAY:
		{	
			PUSC_VEC_ARRAY_REG	psArray;

			ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
			psArray = psState->apsVecArrayReg[psArg->uNumber];
			ASSERT(psArray != NULL);

			if (psArray->eArrayType == ARRAY_TYPE_DRIVER_LOADED_SECATTR ||
				psArray->eArrayType == ARRAY_TYPE_DIRECT_MAPPED_SECATTR)
			{
				return IMG_TRUE;
			}
			return IMG_FALSE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_VOID FixSARegisterNumbers(PINTERMEDIATE_STATE psState, PREGISTER_GROUP psGroup)
/*****************************************************************************
 FUNCTION	: FixSARegisterNumbers

 PURPOSE    : Fix instructions where we haven't been able to give a driver
			  constant assigned to a secondary attribute a hardware register
	          of the right alignment.

 PARAMETERS	: psState			- Compiler state.
			  psGroup			- Secondary attribute register to fix.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDefChain;
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	psUseDefChain = UseDefGet(psState, USEASM_REGTYPE_TEMP, psGroup->uRegister);
	ASSERT(psUseDefChain != NULL);

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUseDef->eType == DEF_TYPE_INST || psUseDef->eType == USE_TYPE_SRC)
		{
			SetupRegisterGroupsInst(psState, NULL /* psGroupsContext */, psUseDef->u.psInst);
		}
	}
}

static
IMG_BOOL UsedWithFormatSelect(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef)
/*****************************************************************************
 FUNCTION	: UsedWithFormatSelect

 PURPOSE    : Check if an intermediate register is ever used with an instruction
			  which uses the top-bit of the hardware register number to choose
			  between alternate register formats.

 PARAMETERS	: psState			- Compiler state.
			  psUseDef			- Intermediate register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse->eType == USE_TYPE_SRC)
		{
			PINST	psInst = psUse->u.psInst;

			/*
				Check for an instruction where the maximum range of register hardware
				register numbers which can be directly encoded in the instruction
				is restricted.
			*/
			if (psInst->eOpcode == IEFO)
			{
				/*
					The format select for EFO instructions is a global flag so if
					any EFO instruction has F16 sources it's quite likely the flag
					will be on for all instructions.
				*/
				if ((psState->uFlags & USC_FLAGS_EFOS_HAVE_F16_SOURCES) != 0)
				{
					return IMG_TRUE;
				}
			}
			else if (HasF16F32SelectInst(psInst))
			{
				IMG_UINT32	uSrcIdx;

				for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
				{
					if (psInst->asArg[uSrcIdx].eFmt == UF_REGFORMAT_F16)
					{
						return IMG_TRUE;
					}
				}
			}
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID SetSARegisterNumberPreferences(PINTERMEDIATE_STATE		psState,
										PUNIFORM_SECATTR_STATE	psUniformSAState)
/*****************************************************************************
 FUNCTION	: SetSARegisterNumberPreferences

 PURPOSE    : Set up information about which secondary attributes would prefer
			  lower register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psUniformSAState	- State to update with information about
								constraints on hardware register numbers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PSAPROG_STATE	psSAProg = &psState->sSAProg;

	for (psListEntry = psSAProg->sDriverLoadedSAList.psHead;
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PSAPROG_RESULT		psSecAttr;
		PUNIFORM_SECATTR	psReg;

		psSecAttr = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, u1.sDriverConst.sDriverLoadedSAListEntry);
		psReg = psSecAttr->u.psRegAllocData;

		if (IsGroupHead(psReg))
		{
			PFIXED_REG_DATA		psFixedReg;
			PUSEDEF_CHAIN		psUseDef;

			psFixedReg = psSecAttr->psFixedReg;
			ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
			ASSERT(psFixedReg->uConsecutiveRegsCount == 1);

			psUseDef = UseDefGet(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[0]);
			if (UsedWithFormatSelect(psState, psUseDef))
			{
				ALLOCATION_PRIORITY	ePriority;

				if (psReg->psConst->eFormat == UNIFLEX_CONST_FORMAT_F16)
				{
					/*
						F16 format secondary attributes used with format select have the
						most limited range of hardware register numbers.
					*/
					ePriority = ALLOCATION_PRIORITY_HIGH;
				}
				else
				{
					/*
						F32 format secondary attributes used with format select have 
						a less restricted range.
					*/	
					ePriority = ALLOCATION_PRIORITY_MEDIUM;
				}
				SetUniformSecAttrPriority(psState, psUniformSAState, psReg, ePriority);
			}
		}
	}
}

static
PUNIFORM_SECATTR FindEntryOfLength(PUNIFORM_SECATTR_ALIGN_LISTS	psAlignList,
								   IMG_UINT32					uPriorityIdx,
								   IMG_UINT32					uMaximumLength)
/*****************************************************************************
 FUNCTION	: FindEntryOfLength

 PURPOSE    : Look for an unassigned group of secondary attributes of a maximum length.

 PARAMETERS	: psAlignList		- List of unassigned secondary attributes.
			  uPriorityIdx		- Priority to check.
			  uMaximumLength	- Maximum length of the group to return.

 RETURNS	: The first secondary attribute in the group.
*****************************************************************************/
{
	IMG_UINT32	uLengthMod2;

	for (uLengthMod2 = 0; uLengthMod2 < 2; uLengthMod2++)
	{
		PUSC_LIST		psList = &psAlignList->aasLists[uLengthMod2][uPriorityIdx];
		PUSC_LIST_ENTRY	psListEntry;

		/*
			Look for an entry which fits in the available space. The list is in order of decreasing length
			so the first entry we find is also the best fit.
		*/
		for (psListEntry = psList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUNIFORM_SECATTR psSAReg = IMG_CONTAINING_RECORD(psListEntry, PUNIFORM_SECATTR, sAlignListEntry);

			if (psSAReg->uLength <= uMaximumLength)
			{
				RemoveFromList(psList, psListEntry);
				return psSAReg;
			}
		}
	}
	return NULL;
}

static
PUNIFORM_SECATTR FindAlignedEntry(PINTERMEDIATE_STATE		psState,
								  PUNIFORM_SECATTR_STATE	psAllocState,
								  IMG_UINT32				uPaddingRequired,
								  IMG_UINT32				uNextHWReg,
								  IMG_UINT32				uConsecutiveHWRegCount,
								  IMG_PUINT32				puPaddingRequired)
/*****************************************************************************
 FUNCTION	: FindAlignedEntry

 PURPOSE    : Look for a group of secondary attributes which can be assigned the next
			  available hardware register numbers.

 PARAMETERS	: psState			- Compiler state.
			  psAllocState		- Lists of unassigned secondary attributes.
			  uPaddingRequired	- Count of hardware registers to consider skipped.
			  uNextHWReg		- Next available hardware register number.
			  uConsecutiveHWRegCount
								- Count of consecutive hardware registers
								available.
			  puPaddingRequired	- Returns the count of hardware registers to skip.

 RETURNS	: The first secondary attribute in the group.
*****************************************************************************/
{
	HWREG_ALIGNMENT		eStartAlign, eWrongAlign;
	PSAPROG_STATE		psSAProg = &psState->sSAProg;
	PUNIFORM_SECATTR	psSAReg;
	IMG_UINT32			uPriorityIdx;

	/*
		Set default return values.
	*/	
	(*puPaddingRequired) = 0;

	if ((uNextHWReg % 2) == 0)
	{
		eStartAlign = HWREG_ALIGNMENT_EVEN;
		eWrongAlign = HWREG_ALIGNMENT_ODD;
	}
	else
	{
		eStartAlign = HWREG_ALIGNMENT_ODD;
		eWrongAlign = HWREG_ALIGNMENT_EVEN;
	}

	for (uPriorityIdx = 0; uPriorityIdx < ALLOCATION_PRIORITY_COUNT; uPriorityIdx++)
	{
		/*
			Start with the list of entries whose required alignment matches the
			alignment of the next available hardware register number.
		*/
		psSAReg = FindEntryOfLength(&psAllocState->asAlignLists[eStartAlign], uPriorityIdx, uConsecutiveHWRegCount);
		if (psSAReg != NULL)
		{
			return psSAReg;
		}
	
		/*
			Next try entries with no particular alignment requirement.
		*/
		psSAReg = FindEntryOfLength(&psAllocState->asAlignLists[HWREG_ALIGNMENT_NONE], uPriorityIdx, uConsecutiveHWRegCount);
		if (psSAReg != NULL)
		{
			return psSAReg;
		}
	}

	/*
		If there is space to add padding so the next destination for the next loaded secondary attribute starts
		on the other alignment then look for an entry with that alignment.
	*/
	if (
			(psSAProg->uConstSecAttrCount + uPaddingRequired) < psSAProg->uInRegisterConstantLimit && 
			uConsecutiveHWRegCount > 1
	   )
	{
		for (uPriorityIdx = 0; uPriorityIdx < ALLOCATION_PRIORITY_COUNT; uPriorityIdx++)
		{
			psSAReg = FindEntryOfLength(&psAllocState->asAlignLists[eWrongAlign], uPriorityIdx, uConsecutiveHWRegCount - 1);
			if (psSAReg != NULL)
			{
				(*puPaddingRequired) = 1;
				return psSAReg;
			}
		}
	}

	return NULL;
}


static
PUNIFORM_SECATTR SkipFragmentedRegions(PINTERMEDIATE_STATE		psState,
									   PUNIFORM_SECATTR_STATE	psAllocState,
									   IMG_PUINT32				puPaddingRequired)
/*****************************************************************************
 FUNCTION	: SkipFragmentedRegions

 PURPOSE    : Check if its possible to to assign a group of secondary attributes
			  a correctly aligned and consecutive range of hardware registers by
			  skipping ranges of hardware registers which are too small.

 PARAMETERS	: psState			- Compiler state.
			  psAllocState		- Lists of unassigned secondary attributes.
			  puPaddingRequired	- Returns the count of hardware registers to skip.

 RETURNS	: The first secondary attribute in the group.
*****************************************************************************/
{
	PSA_ALLOC_REGION	psRegion;
	PSA_ALLOC_REGION	psRegionEnd;
	IMG_UINT32			uRegionOffset;
	IMG_UINT32			uFragPaddingRequired;
	PSAPROG_STATE		psSAProg = &psState->sSAProg;

	psRegion = &psSAProg->asAllocRegions[psSAProg->uFirstPartialRegion];
	psRegionEnd = &psSAProg->asAllocRegions[psSAProg->uAllocRegionCount];
	uRegionOffset = psSAProg->uPartialRegionOffset;
	uFragPaddingRequired = 0;
	for (;;)
	{
		IMG_UINT32			uAlignPaddingRequired;
		PUNIFORM_SECATTR	psSA;

		/*
			Get the hardware registers to skip to get to the end of this region.
		*/
		uFragPaddingRequired += psRegion->uLength - uRegionOffset;
		
		/*
			Fail if skipping this region would mean we wouldn't have enough hardware
			registers for all the secondary attributes.
		*/
		if ((psSAProg->uConstSecAttrCount + uFragPaddingRequired) >= psSAProg->uInRegisterConstantLimit)
		{
			return NULL;
		}

		/*
			Skip this region.
		*/
		psRegion++;

		/*
			Check for running out of available regions.
		*/
		if (psRegion == psRegionEnd)
		{
			return NULL;
		}

		/*
			Look for a group of secondary attributes which can fit at the start
			of this region.
		*/
		psSA = FindAlignedEntry(psState, 
								psAllocState, 
								uFragPaddingRequired, 
								psRegion->uStart, 
								psRegion->uLength, 
								&uAlignPaddingRequired);
		if (psSA != NULL)
		{
			(*puPaddingRequired) = uFragPaddingRequired + uAlignPaddingRequired;
			return psSA;
		}
	}
}

static
PUNIFORM_SECATTR GetShortestEntry(PUNIFORM_SECATTR_ALIGN_LISTS	psAlignList,
								  PUSC_LIST*					ppsShortestList)
/*****************************************************************************
 FUNCTION	: GetShortestEntry

 PURPOSE    : Get the shortest group of unassigned secondary attrbutes.

 PARAMETERS	: psAlignList		- Secondary attributes not yet assigned hardware
								register numbers.
			  ppsShortestList	- Returns the list of returned secondary attribute
								belongs to.

 RETURNS	: The first secondary attribute in the group.
*****************************************************************************/
{
	IMG_UINT32	uPriorityIdx;

	for (uPriorityIdx = 0; uPriorityIdx < ALLOCATION_PRIORITY_COUNT; uPriorityIdx++)
	{
		IMG_INT32	iLengthMod2;

		for (iLengthMod2 = 1; iLengthMod2 >= 0; iLengthMod2--)
		{
			PUSC_LIST		psList = &psAlignList->aasLists[iLengthMod2][uPriorityIdx];
			PUSC_LIST_ENTRY	psListEntry;

			psListEntry = psList->psTail;
			if (psListEntry != NULL)
			{
				*ppsShortestList = psList;
				return IMG_CONTAINING_RECORD(psListEntry, PUNIFORM_SECATTR, sAlignListEntry);
			}
		}
	}
	return NULL;
}

static
PUNIFORM_SECATTR FindMatchingEntry(PINTERMEDIATE_STATE		psState,
								   PUNIFORM_SECATTR_STATE	psAllocState,
								   IMG_PUINT32				puPaddingRequired,
								   IMG_PBOOL				pbIgnoredRestrictions)
/*****************************************************************************
 FUNCTION	: FindMatchingEntry

 PURPOSE    : Find a secondary attribute to assign the next available hardware
			  register number to.

 PARAMETERS	: psState			- Compiler state.
			  psAllocState		- Secondary attributes not yet assigned hardware
								register numbers.
			  puPaddingRequired	- Returns the count of dummy entries to add.
			  pbIgnoredRestrictions
								- Returns TRUE if there wasn't enough space
								to satisfy restrictions on the alignment or order
								of the returned secondary attribute.

 RETURNS	: The secondary attribute.
*****************************************************************************/
{
	PUNIFORM_SECATTR	psSA;
	PSAPROG_STATE		psSAProg = &psState->sSAProg;
	PSA_ALLOC_REGION	psFirstRegion;
	PUNIFORM_SECATTR	psSmallestSA;
	PUSC_LIST			psSmallestSAList;
	IMG_UINT32			uNextHWReg;
	IMG_UINT32			uConsecutiveHWRegCount;
	IMG_UINT32			uAlignIdx;

	*pbIgnoredRestrictions = IMG_FALSE;

	ASSERT(psSAProg->uFirstPartialRegion < psSAProg->uAllocRegionCount);
	psFirstRegion = &psSAProg->asAllocRegions[psSAProg->uFirstPartialRegion];

	/*
		Get the next available range of secondary attributes.
	*/
	uNextHWReg = psFirstRegion->uStart + psSAProg->uPartialRegionOffset;
	uConsecutiveHWRegCount = psFirstRegion->uLength - psSAProg->uPartialRegionOffset;

	/*
		If the available secondary attribute space is fragmented then assign indexable ranges of secondary
		attributes first. 
	*/
	if (psSAProg->uAllocRegionCount > 1)
	{
		PUSC_LIST_ENTRY		psListEntry;
		PUNIFORM_SECATTR	psBestRangeFirstSA;

		psBestRangeFirstSA = NULL;
		for (psListEntry = psSAProg->sInRegisterConstantRangeList.psHead;
			 psListEntry != NULL;
			 psListEntry = psListEntry->psNext)
		{
			PCONSTANT_INREGISTER_RANGE	psRange;
			PSAPROG_RESULT				psFirstResult;
			PUNIFORM_SECATTR			psFirstSA;

			psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_INREGISTER_RANGE, sListEntry);
			psFirstResult = IMG_CONTAINING_RECORD(psRange->sResultList.psHead, PSAPROG_RESULT, sRangeListEntry);
			psFirstSA = psFirstResult->u.psRegAllocData;

			/*
				Skip ranges already assigned secondary attributes.
			*/
			if (psFirstResult->psFixedReg->sPReg.uNumber != USC_UNDEF)
			{
				continue;
			}
				
			/*
				Skip ranges which don't fit in the range we are currently allocating.
			*/
			if (psFirstSA->uLength > uConsecutiveHWRegCount)
			{
				continue;
			}

			/*
				Pick the range which best fits in the available space.
			*/
			if (psBestRangeFirstSA == NULL || psBestRangeFirstSA->uLength < psFirstSA->uLength)
			{
				psBestRangeFirstSA = psFirstSA;
			}
		}
		if (psBestRangeFirstSA != NULL)
		{
			RemoveUniformSecAttrFromAlignList(psState, psAllocState, psBestRangeFirstSA);
			*puPaddingRequired = 0;
			return psBestRangeFirstSA;
		}
	}

	/*
		Look for an entry with the correct alignment and order.
	*/
	psSA = FindAlignedEntry(psState, 
							psAllocState, 
							0 /* uPaddingRequired */, 
							uNextHWReg, 
							uConsecutiveHWRegCount, 
							puPaddingRequired);
	if (psSA != NULL)
	{
		return psSA;
	}

	/*
		Check if we can skip a region which is too small.
	*/
	psSA = SkipFragmentedRegions(psState, psAllocState, puPaddingRequired);
	if (psSA != NULL)
	{
		return psSA;
	}

	/*
		Otherwise we can't given any secondary attribute the alignment or order it
		needs. So look for the smallest group of secondary attributes so as few as
		possible have the wrong alignment.
	*/
	psSmallestSA = NULL;
	psSmallestSAList = NULL;
	for (uAlignIdx = 0; uAlignIdx < HWREG_ALIGNMENT_COUNT; uAlignIdx++)
	{
		PUSC_LIST	psSAList;

		psSA = GetShortestEntry(&psAllocState->asAlignLists[uAlignIdx], &psSAList);
		if (psSA == NULL)
		{
			continue;
		}
		if (
				psSmallestSA == NULL || 
				psSA->uLength < psSmallestSA->uLength ||
				(
					psSA->uLength == psSmallestSA->uLength &&
					(psSA->uLength % 2) == 1 &&
					(psSmallestSA->uLength % 2) == 0
				)
		   )
		{
			psSmallestSA = psSA;
			psSmallestSAList = psSAList;
		}
	}

	ASSERT(psSmallestSA != NULL);

	/*
		Remove the head of the smallest group of SAs from the list.
	*/
	RemoveFromList(psSmallestSAList, &psSmallestSA->sAlignListEntry);

	*pbIgnoredRestrictions = IMG_TRUE;
	*puPaddingRequired = 0;
	return psSmallestSA;
}

static
IMG_BOOL AllocDriverLoadedSecondaryAttributes(PINTERMEDIATE_STATE	psState, 
											  IMG_UINT32			uCount, 
											  IMG_UINT32			uAlignment,
											  IMG_PUINT32			puFirstAllocatedSA,
											  IMG_PUINT32			puAlignmentPadding)
/*****************************************************************************
 FUNCTION	: AllocDriverLoadedSecondaryAttributes

 PURPOSE    : Allocate secondary attributes to ask the driver to load uniform
			  data into.

 PARAMETERS	: psState			- Compiler state.
			  uCount			- Count of entries to assign.
			  uAlignment		- Required alignment of the entries to assign.
			  puFirstAllocatedSA
								- If non-NULL return the first secondary attribute
								allocated.
			  puAlignmentPadding
								- If non-NULL returns the amount of alignment
								padding required.

 RETURNS	: TRUE if sufficent secondary attributes are available.
*****************************************************************************/
{
	IMG_UINT32			uNextRegion;
	IMG_UINT32			uNextRegionOffset;
	PSAPROG_STATE		psSAProg = &psState->sSAProg;

	/*
		Get the first region with secondary attributes available.
	*/
	uNextRegion = psSAProg->uFirstPartialRegion;
	uNextRegionOffset = psSAProg->uPartialRegionOffset;
	while (uNextRegion < psSAProg->uAllocRegionCount)
	{
		PSA_ALLOC_REGION	psNextRegion;
		IMG_UINT32			uPadding;
		IMG_UINT32			uFirstAvailableSA;
		IMG_UINT32			uAvailableSACount;

		psNextRegion = &psSAProg->asAllocRegions[uNextRegion];

		uFirstAvailableSA = psNextRegion->uStart + uNextRegionOffset;

		/*
			Check how padding we'd need to add so secondary attributes allocated
			from the region start correctly aligned.
		*/
		if (uAlignment > 0 && (uFirstAvailableSA % uAlignment) != 0)
		{
			uPadding = uAlignment - (uFirstAvailableSA - uAlignment);
		}
		else
		{
			uPadding = 0;
		}
		if (puAlignmentPadding != NULL)
		{
			*puAlignmentPadding = uPadding;
		}
		
		/*
			Are enough secondary attributes available in this region?
		*/
		uAvailableSACount = psNextRegion->uLength - uNextRegionOffset - uPadding;
		if (uAvailableSACount >= uCount)
		{
			if (puFirstAllocatedSA != NULL)
			{
				/*
					Return the first allocated secondary attribute.
				*/
				(*puFirstAllocatedSA) = psNextRegion->uStart + uNextRegionOffset + uPadding;

				/*
					Update the next secondary attributes available for allocation.
				*/
				psSAProg->uFirstPartialRegion = uNextRegion;
				psSAProg->uPartialRegionOffset = uNextRegionOffset + uPadding + uCount;
				if (psSAProg->uPartialRegionOffset == psNextRegion->uLength)
				{
					psSAProg->uFirstPartialRegion++;
					psSAProg->uPartialRegionOffset = 0;
				}
			}
			return IMG_TRUE;
		}

		uNextRegion++;
		uNextRegionOffset = 0;
	}

	ASSERT(puFirstAllocatedSA == NULL);
	return IMG_FALSE;
}

static
IMG_VOID AppendToSAGroup(PINTERMEDIATE_STATE	psState,
						 PREGISTER_GROUP		psNextAssigned,
						 PREGISTER_GROUP*		ppsLastAssignedGroup)
/*****************************************************************************
 FUNCTION	: AppendToSAGroup

 PURPOSE    : Link the intermediate register assigned the next secondary attribute
			  hardware register numbers with the intermediate register assigned the
			  immediately previous hardware register number.

 PARAMETERS	: psState			- Compiler state.
			  psNextAssigned	- Current intermediate register.
			  ppsLastGroup		- Points to the last intermediate register to
								be assigned a secondary attribute hardware register
								number.

 RETURNS	: Nothing.
*****************************************************************************/
{	
	PREGISTER_GROUP	psLastAssignedGroup;

	psLastAssignedGroup = *ppsLastAssignedGroup;
	if (psLastAssignedGroup != NULL)
	{
		PFIXED_REG_DATA	psLastFixedReg;

		psLastFixedReg = psLastAssignedGroup->psFixedReg;
		ASSERT(psLastAssignedGroup->uFixedRegOffset == 0);
		ASSERT(psLastFixedReg->uConsecutiveRegsCount == 1);

		if ((psLastFixedReg->sPReg.uNumber + 1) == psNextAssigned->psFixedReg->sPReg.uNumber)
		{
			IMG_BOOL	bRet;

			bRet = AddToGroup(psState, 
						      psLastAssignedGroup->uRegister, 
							  psLastAssignedGroup, 
							  psNextAssigned->uRegister,
							  psNextAssigned, 
							  IMG_FALSE /* bLinkedByInst */,
							  IMG_FALSE /* bOptional */);
			ASSERT(bRet);
		}
	}
	*ppsLastAssignedGroup = psNextAssigned;
}

static
IMG_VOID SetSAHardwareRegisterNumbers(PINTERMEDIATE_STATE	psState,
									  PUNIFORM_SECATTR		psFirstSA,
									  IMG_PUINT32			puAssignedSACount,
									  PREGISTER_GROUP*		ppsLastGroup,
									  PUSC_LIST				psIgnoredRestrictionsList,
									  IMG_BOOL				bIgnoredRestrictions)
/*****************************************************************************
 FUNCTION	: SetSAHardwareRegisterNumbers

 PURPOSE    : Assign the next available hardware register numbers to a group 
			  of secondary attributes.

 PARAMETERS	: psState			- Compiler state.
			  psFirstSA			- First secondary attribute in the group.
			  puAssignedSACount	- Updated with the count of secondary attributes
								assigned hardware register numbers.
			  ppsLastGroup		- Points to the last intermediate register to
								be assigned a secondary attribute hardware register
								number.
			  psIgnoredRestrictionsList
								- List of secondary attribute which weren't given
								correctly ordered/aligned hardware register numbers.
			  bIgnoredRestrictions
								- TRUE if restrictions on the alignment or order
								of the secondary attributes assigned were ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PREGISTER_GROUP		psGroup;
	IMG_UINT32			uFirstSARegNum;
	IMG_UINT32			uCurrentSARegNum;
	PREGISTER_GROUP		psNextGroup;

	ASSERT(IsGroupHead(psFirstSA));

	/*
		Allocate the next available hardware register numbers.
	*/
	AllocDriverLoadedSecondaryAttributes(psState, 
										 psFirstSA->uLength, 
										 0 /* uAlignment */, 
										 &uFirstSARegNum, 
										 NULL /* puAlignmentPadding */);

	/*
		Assign hardware register numbers to a complete block of variables requiring consecutive
		register numbers.
	*/
	for (psGroup = psFirstSA->psGroup, uCurrentSARegNum = uFirstSARegNum; 
		 psGroup != NULL; 
		 psGroup = psNextGroup, uCurrentSARegNum++)
	{
		PFIXED_REG_DATA		psFixedReg;
		PUNIFORM_SECATTR	psCurrentSAReg;

		psNextGroup = psGroup->psNext;
		
		/*
			Remove existing register grouping information so, in the case where we ignored restrictions
			on the alignment/order of hardware register numbers, we can recreate the group with the actual
			order/alignment.
		*/
		RemoveFromGroup(psState, psGroup);

		/*
			Append to the list of secondary attributes assigned hardware register numbers.
		*/
		psCurrentSAReg = psGroup->u.psSA;
		AppendToList(&psState->sSAProg.sInRegisterConstantList, &psCurrentSAReg->psConst->sListEntry);
		
		/*	
			Set the fixed hardware register number for this secondary attribute.
		*/
		psFixedReg = psCurrentSAReg->psConst->psResult->psFixedReg;
		ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList, 
								  psFixedReg, 
								  USEASM_REGTYPE_SECATTR,
								  uCurrentSARegNum);

		/*
			Set the alignment for the secondary attributes.
		*/
		SetGroupHardwareRegisterNumber(psState, psGroup, bIgnoredRestrictions);

		/*
			Flag the just added secondary attribute as having a hardware register number
			immediately after the last assigned secondary attribute.
		*/
		AppendToSAGroup(psState, psGroup, ppsLastGroup);

		/*
			If we were forced to ignore restrictions on the alignment/order of secondary attributes
			then insert moves before any instructions using the secondary attributes.
		*/
		if (bIgnoredRestrictions)
		{
			AppendToList(psIgnoredRestrictionsList, &psCurrentSAReg->sIgnoredRestrictionsListEntry);
		}

		(*puAssignedSACount)++;
	}	
}

static
IMG_VOID AddPaddingSecondaryAttributes(PINTERMEDIATE_STATE	psState, 
									   IMG_UINT32			uPaddingCount,
									   PREGISTER_GROUP*		ppsLastGroup)
/*****************************************************************************
 FUNCTION	: AddPaddingSecondaryAttributes

 PURPOSE    : Add dummy entries to the list of secondary attributes for the
			  driver to load.

 PARAMETERS	: psState			- Compiler state.
			  uPaddingCount		- Count of dummy entries to add.
			  ppsLastGroup		- Points to the last intermediate register to
								be assigned a secondary attribute hardware register
								number.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINREGISTER_CONST	psPaddingConst;	
	PREGISTER_GROUP		psNextGroup;
	IMG_UINT32			uPaddingIdx;

	for (uPaddingIdx = 0; uPaddingIdx < uPaddingCount; uPaddingIdx++)
	{
		IMG_UINT32	uPaddingSARegNum;

		/*
			Grab the next available secondary attribute.
		*/
		AllocDriverLoadedSecondaryAttributes(psState, 
											 1 /* uCount */, 
											 0 /* uAlignment */, 
											 &uPaddingSARegNum,
											 NULL /* puAlignmentPadding */);

		/*
			Add a dummy entry to the list of secondary attributes for the driver to load.
		*/
		AddSimpleSecAttr(psState,
						 0 /* uAlignment */,
				 	     USC_UNDEF /* uNum */,
						 UNIFLEX_CONST_FORMAT_STATIC,
						 USC_UNDEF /* uBuffer */,
						 &psPaddingConst);

		/*
			Set the fixed hardware register for this secondary attribute.
		*/
		ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList, 
								  psPaddingConst->psResult->psFixedReg, 
								  USEASM_REGTYPE_SECATTR,
								  uPaddingSARegNum);

		/*
			Flag the just added secondary attribute as having a hardware register number
			immediately after the last assigned secondary attribute.
		*/	
		psNextGroup = AddSAProgResultGroup(psState, psPaddingConst->psResult);
		AppendToSAGroup(psState, psNextGroup, ppsLastGroup);
	}
}

IMG_INTERNAL
IMG_VOID AllocateSecondaryAttributes(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: AllocateSecondaryAttributes

 PURPOSE    : Assign hardware register numbers to secondary attributes loaded
			  with uniforms by the driver.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UNIFORM_SECATTR_STATE	sUniformSAState;
	IMG_UINT32				uAssignedCount;
	IMG_UINT32				uListIdx;
	IMG_UINT32				uRegOffset;
	PUSC_LIST_ENTRY			psListEntry;
	PUNIFORM_SECATTR		asReg; /* Array with an entry for each secondary attribute loaded with data by the driver. */
	PREGISTER_GROUP			psLastGroup;
	PSAPROG_STATE			psSAProg = &psState->sSAProg;
	USC_LIST				sIgnoredRestrictionsList;

	for (uListIdx = 0; uListIdx < HWREG_ALIGNMENT_COUNT; uListIdx++)
	{
		IMG_UINT32	uLengthMod2;
		for (uLengthMod2 = 0; uLengthMod2 < 2; uLengthMod2++)
		{
			IMG_UINT32	uPriorityIdx;
			for (uPriorityIdx = 0; uPriorityIdx < ALLOCATION_PRIORITY_COUNT; uPriorityIdx++)
			{
				InitializeList(&sUniformSAState.asAlignLists[uListIdx].aasLists[uLengthMod2][uPriorityIdx]);
			}
		}
	}
	asReg = UscAlloc(psState, sizeof(asReg[0]) * psSAProg->uInRegisterConstantCount);

	/*
		Initialize information about the constraints on the hardware
		register numbers for each secondary attribute.
	*/
	for (psListEntry = psSAProg->sDriverLoadedSAList.psHead, uRegOffset = 0;
		 uRegOffset < psSAProg->uInRegisterConstantCount; 
		 psListEntry = psListEntry->psNext, uRegOffset++)
	{
		PUNIFORM_SECATTR	psSAReg = &asReg[uRegOffset];
		PSAPROG_RESULT		psResult;

		psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, u1.sDriverConst.sDriverLoadedSAListEntry);

		ClearListEntry(&psSAReg->sAlignListEntry);
		psSAReg->psConst = psResult->u1.sDriverConst.apsDriverConst[0];
		psSAReg->ePriority = ALLOCATION_PRIORITY_DEFAULT;
		psSAReg->psGroup = FindRegisterGroup(psState, psResult->psFixedReg->auVRegNum[0]);
		ASSERT(psSAReg->psGroup != NULL);
		psSAReg->uLength = 1;

		if (psSAReg->psGroup->psPrev == NULL)
		{
			PUSC_LIST			psAllocList;
			PREGISTER_GROUP		psGroup = psSAReg->psGroup;
			IMG_UINT32			uGroupLength;
			HWREG_ALIGNMENT		eAlignment;
			PREGISTER_GROUP		psNextGroup;

			/*
				Count the number of secondary attributes requiring hardware registers after
				this one.
			*/
			psSAReg->uLength = 0;
			for (psNextGroup = psGroup; psNextGroup != NULL; psNextGroup = psNextGroup->psNext)
			{
				psSAReg->uLength++;
			}

			uGroupLength = psSAReg->uLength;
			eAlignment = psGroup->eAlign;

			ASSERT(eAlignment < HWREG_ALIGNMENT_COUNT);
			psAllocList = &sUniformSAState.asAlignLists[eAlignment].aasLists[uGroupLength % 2][ALLOCATION_PRIORITY_DEFAULT];
			InsertInListSorted(psAllocList, UniformSACmp, &psSAReg->sAlignListEntry);
		}

		/*
			Associate the secondary attribute with information about a block of regisetrs requiring
			consecutive hardware register numbers.
		*/
		psSAReg->psGroup->u.psSA = psSAReg;

		/*
			Associate the secondary attribute with the information about it's register allocation so
			we can update the register allocation information as we scan through the program.
		*/
		psResult->u.psRegAllocData = psSAReg;
	}

	/*
		Give secondary attributes accessed with dynamic indices the lowest allocation priority. This 
		is because accessing secondary attributes with static indices is more expensive for very large 
		register numbers whereas it makes less diference for dynamic indices.
	*/
	for (psListEntry = psSAProg->sInRegisterConstantRangeList.psHead;
		 psListEntry != NULL;
		 psListEntry = psListEntry->psNext)
	{
		PCONSTANT_INREGISTER_RANGE	psRange;
		PSAPROG_RESULT				psFirstResult;

		psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_INREGISTER_RANGE, sListEntry);
		psFirstResult = IMG_CONTAINING_RECORD(psRange->sResultList.psHead, PSAPROG_RESULT, sRangeListEntry);
		SetUniformSecAttrPriority(psState, &sUniformSAState, psFirstResult->u.psRegAllocData, ALLOCATION_PRIORITY_LOW);
	}

	/*
		Get information about the constraints on the hardware register numbers to
		use for uniforms in secondary attributes.
	*/
	SetSARegisterNumberPreferences(psState, &sUniformSAState);

	/*
		Reset the list of constants to be loaded by the driver.
	*/
	InitializeList(&psSAProg->sInRegisterConstantList);

	/*
		Reset information about the secondary attributes available for allocation.
	*/
	psSAProg->uFirstPartialRegion = 0;
	psSAProg->uPartialRegionOffset = 0;

	uAssignedCount = 0;
	psLastGroup = NULL;

	/*
		Assign hardware register numbers to all secondary attributes to be loaded by the driver.
	*/
	InitializeList(&sIgnoredRestrictionsList);
	while (uAssignedCount < psSAProg->uInRegisterConstantCount)
	{
		PUNIFORM_SECATTR	psNextSA;
		IMG_UINT32			uPaddingRequired;
		IMG_BOOL			bIgnoredSecAttrRestrictions = IMG_FALSE;

		/*
			Look for a secondary attribute which fits in the remaining space.
		*/
		psNextSA = 
			FindMatchingEntry(psState, &sUniformSAState, &uPaddingRequired, &bIgnoredSecAttrRestrictions);
		
		/*
			We might need to add padding where the next secondary attribute has a restriction on the
			alignment of the hardware register number or there isn't enough space at the end of the
			current region to fit in it.
		*/
		if (uPaddingRequired > 0)
		{
			AddPaddingSecondaryAttributes(psState, uPaddingRequired, &psLastGroup);
			uAssignedCount += uPaddingRequired;
		}

		/*
			Set the hardware register numbers for the found secondary attribute.
		*/
		SetSAHardwareRegisterNumbers(psState, 
									 psNextSA,
									 &uAssignedCount,
									 &psLastGroup,
									 &sIgnoredRestrictionsList,
									 bIgnoredSecAttrRestrictions);
	}

	/*
		All secondary attributes should have been assigned hardware register numbers.
	*/
	for (uListIdx = 0; uListIdx < HWREG_ALIGNMENT_COUNT; uListIdx++)
	{
		IMG_UINT32	uLengthMod2;
		for (uLengthMod2 = 0; uLengthMod2 < 2; uLengthMod2++)
		{
			IMG_UINT32	uPriorityIdx;

			for (uPriorityIdx = 0; uPriorityIdx < ALLOCATION_PRIORITY_COUNT; uPriorityIdx++)
			{
				ASSERT(IsListEmpty(&sUniformSAState.asAlignLists[uListIdx].aasLists[uLengthMod2][uPriorityIdx]));
			}
		}
	}

	/*
		For all secondary attributes not given correctly ordered/aligned hardware register numbers
		reprocess instructions using them as sources.
	*/
	while ((psListEntry = RemoveListHead(&sIgnoredRestrictionsList)) != NULL)
	{
		PUNIFORM_SECATTR	psSA;

		psSA = IMG_CONTAINING_RECORD(psListEntry, PUNIFORM_SECATTR, sIgnoredRestrictionsListEntry);
		FixSARegisterNumbers(psState, psSA->psGroup);
	}

	/*
		Free used information.
	*/
	UscFree(psState, asReg);

	for (psListEntry = psState->sSAProg.sResultsList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PSAPROG_RESULT	psResult = IMG_CONTAINING_RECORD(psListEntry, PSAPROG_RESULT, sListEntry);

		if (psResult->eType == SAPROG_RESULT_TYPE_CALC)
		{
			ASSERT(psResult->psFixedReg->sPReg.uType == USC_REGTYPE_CALCSECATTR);
			psResult->psFixedReg->sPReg.uType = USEASM_REGTYPE_SECATTR;
		}
	}

	/*
		Flag we have assigned hardware register numbers to secondary attributes.
	*/
	psState->uFlags |= USC_FLAGS_ASSIGNED_SECATTR_REGNUMS;
}

IMG_INTERNAL
IMG_UINT32 FindHardwareConstant(PINTERMEDIATE_STATE	psState,
								IMG_UINT32			uImmediate)
/*****************************************************************************
 FUNCTION	: FindHardwareConstant

 PURPOSE    : Check for a hardware constant with a given value.

 PARAMETERS	: psState		- Compiler state.
			  uImmediate	- The value to check for.

 RETURNS	: The index of the constant or USC_UNDEF if none were found.
*****************************************************************************/
{
	IMG_UINT32	uConstIdx;

	PVR_UNREFERENCED_PARAMETER(psState);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		for (uConstIdx = 0; uConstIdx < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS; uConstIdx++)
		{
			PCUSETAB_SPECIAL_CONST	psConst = &g_asVecHardwareConstants[uConstIdx];

			if (!psConst->bReserved && uImmediate == psConst->auValue[0])
			{
				return uConstIdx << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			}
		}
		return USC_UNDEF;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		if (uImmediate == 0)
		{
			return EURASIA_USE_SPECIAL_CONSTANT_ZERO;
		}
		if (uImmediate == 0x3F800000)
		{
			return EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
		}
		if (uImmediate == 0xFFFFFFFF)
		{
			return EURASIA_USE_SPECIAL_CONSTANT_INT32ONE;
		}

		for (uConstIdx = 0; uConstIdx < (sizeof(g_pfHardwareConstants) / sizeof(g_pfHardwareConstants[0])); uConstIdx++)
		{
			if (uImmediate == *(const IMG_UINT32*)&g_pfHardwareConstants[uConstIdx])
			{
				return uConstIdx;
			}
		}
		return USC_UNDEF;
	}
}

/*********************************************************************************
 Function			: GetHardwareConstantValueu

 Description		: Get the value of a hardware constant.
 
 Parameters			: psState	- Internal compilation state
					  uConstIdx	- Hardware constant number.

 Return				: The value of the constant.
*********************************************************************************/
IMG_INTERNAL
IMG_UINT32 GetHardwareConstantValueu(PINTERMEDIATE_STATE	psState, IMG_UINT32	uConstIdx)
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		IMG_UINT32				uVecIdx;
		IMG_UINT32				uChanIdx;
		PCUSETAB_SPECIAL_CONST	psConst;

		uVecIdx = uConstIdx >> SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
		uChanIdx = uConstIdx & ((1U << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT) - 1);

		ASSERT(uVecIdx < SGXVEC_USE_SPECIAL_NUM_FLOAT_CONSTANTS);
		psConst = &g_asVecHardwareConstants[uVecIdx];

		ASSERT(!psConst->bReserved);
		return psConst->auValue[uChanIdx];
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		switch (uConstIdx)
		{
			case EURASIA_USE_SPECIAL_CONSTANT_INT32ONE: return 0xFFFFFFFF;
			case EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE: return 0x3C003C00;
			default:
			{
				IMG_FLOAT	fValue;

				ASSERT(uConstIdx < USEASM_NUM_HW_SPECIAL_CONSTS);
				fValue = g_pfHardwareConstants[uConstIdx];
				return *(IMG_PUINT32)&fValue;
			}
		}
	}
}

/*********************************************************************************
 Function			: GetHardwareConstantValue

 Description		: Get the value of a hardware constant.
 
 Parameters			: psState	- Internal compilation state
					  uConstIdx	- Hardware constant number.

 Return				: The value of the constant.
*********************************************************************************/
IMG_FLOAT IMG_INTERNAL GetHardwareConstantValue(PINTERMEDIATE_STATE	psState, 
												IMG_UINT32			uConstIdx)
{
	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	ASSERT((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0);

	ASSERT(uConstIdx != EURASIA_USE_SPECIAL_CONSTANT_INT32ONE);
	ASSERT(uConstIdx != EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE);

	return g_pfHardwareConstants[uConstIdx];
}

/*********************************************************************************
 Function			: IsStaticC10Value

 Description		: Check if a secondary attribute is C10 format with a specific,
					  static value.

 Parameters			: psState			- Compiler state.
					  fValue			- Value to check for.
					  psArg				- Secondary attribute source argument.
					  uChanMask			- Mask of channels to check.

 Return				: TRUE if the secondary attribute has the fixed value.
*********************************************************************************/
IMG_BOOL IMG_INTERNAL IsStaticC10Value(PINTERMEDIATE_STATE	psState,
									   IMG_FLOAT			fValue,
									   PARG					psArg,
									   IMG_UINT32			uChanMask)
{
	PSAPROG_RESULT			psSecAttr;
	IMG_UINT32				uChan;

	/*
		Check that argument is a secondary attribute containing a uniform
		loaded by the driver.
	*/
	if (psArg->uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}
	psSecAttr = psArg->psRegister->psSecAttr;
	if (psSecAttr == NULL || psSecAttr->eType != SAPROG_RESULT_TYPE_DRIVERLOADED)
	{
		return IMG_FALSE;
	}

	/*
		Check the uniform is in C10 format.
	*/
	if (psArg->eFmt != UF_REGFORMAT_C10)
	{
		return IMG_FALSE;
	}

	/*
		Check each referenced channel from the secondary attribute.
	*/
	for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
	{
		if ((uChanMask & (1U << uChan)) != 0)
		{
			PINREGISTER_CONST		psConst;
			IMG_UINT32				uOffset;

			if (uChan == USC_W_CHAN)
			{
				psConst = psSecAttr->u1.sDriverConst.apsDriverConst[1];
				uOffset = psConst->uNum;
			}
			else
			{
				psConst = psSecAttr->u1.sDriverConst.apsDriverConst[0];
				uOffset = psConst->uNum + uChan;
			}
			ASSERT(psConst->eFormat == UNIFLEX_CONST_FORMAT_C10);

			if (
					!(
						psState->uStaticConstsBuffer == psConst->uBuffer &&
						uOffset < psState->psConstants->uCount && 
						GetBit(psState->psConstants->puConstStaticFlags, uOffset) &&
						fValue == psState->psConstants->pfConst[uOffset]
					 )
				)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_VOID GetDriverLoadedConstantLocation(PINTERMEDIATE_STATE	psState, 
									     PINREGISTER_CONST		psConst,
										 IMG_PUINT32			puArgType,
										 IMG_PUINT32			puArgNum)
/*********************************************************************************
 Function			: GetDriverLoadedConstantLocation

 Description		: Get the register loaded with a constant by the driver.

 Parameters			: psState			- Compiler state.
					  psConst			- Constant to get the location of.
					  puArgType			- Returns the details of the constant location.
					  puArgNum

 Return				: Nothing.
*********************************************************************************/
{
	PFIXED_REG_DATA	psFixedReg = psConst->psResult->psFixedReg;

	ASSERT(psFixedReg->uConsecutiveRegsCount == 1);
	*puArgType = USEASM_REGTYPE_TEMP;
	*puArgNum = psFixedReg->auVRegNum[0];
}

static IMG_BOOL IsStaticConst(PINTERMEDIATE_STATE	psState,
							  PINREGISTER_CONST		psConst,
							  IMG_UINT32			uValue)
/*********************************************************************************
 Function			: IsStaticConst

 Description		: Check if a secondary attribute has a static value

 Parameters			: psState			- Compiler state.
					  psConst			- Secondary attribute 
					  uValue			- Value to check.

 Return				: TRUE or FALSE
*********************************************************************************/
{
	IMG_UINT32				uOffset = psConst->uNum;
	UNIFLEX_CONST_FORMAT	eFormat = psConst->eFormat;
	IMG_UINT32				uBuffer = psConst->uBuffer;	

	/*
		Don't use secondary attributes that aren't live out of the secondary block since we'd
		need to recalculate kill flags in the secondary block.
	*/
	if (psState->psSecAttrProg)
	{
		ASSERT((psState->uFlags2 & USC_FLAGS2_ASSIGNED_TEMPORARY_REGNUMS) == 0);
		if (GetRegisterLiveMask(psState, 
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
								USEASM_REGTYPE_TEMP, 
								psConst->psResult->psFixedReg->auVRegNum[0], 
								0) == 0)
		{
			return IMG_FALSE;
		}
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Don't select vector-sized secondary attributes.
	*/
	if (psConst->psResult->bVector)
	{
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	/*
		Don't select secondary attributes which are part of a dynamically indexed range.
	*/
	if (psConst->psResult->bPartOfRange)
	{
		return IMG_FALSE;
	}
	/*
		Check for a secondary attribute with the same value.
	*/
	if (eFormat == UNIFLEX_CONST_FORMAT_STATIC && uOffset == uValue)
	{
		return IMG_TRUE;
	}
	/*
		Check for a secondary attribute referencing to a static constant channel
		with the same value.
	*/
	if (eFormat == UNIFLEX_CONST_FORMAT_F32)
	{
		if (uBuffer == psState->uStaticConstsBuffer &&
			uOffset < psState->psConstants->uCount && 
			GetBit(psState->psConstants->puConstStaticFlags, uOffset) &&
			uValue == (*(IMG_PUINT32)&psState->psConstants->pfConst[uOffset]))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_BOOL IsStaticConstRange(PINTERMEDIATE_STATE	psState, 
								   PUSC_LIST_ENTRY		psFirstListEntry, 
								   IMG_UINT32			auValue[], 
								   IMG_UINT32			uRangeLength)
/*********************************************************************************
 Function			: IsStaticConstRange

 Description		: Check if a range of secondary attributes have static values.

 Parameters			: psState			- Compiler state.
					  psFirstListEntry	- Start of the range to check.
					  puValue			- Array of static values to check for.
					  uRangeLength		- Number of values to check for.

 Return				: TRUE if each constant in the range has the required static
					  value.
*********************************************************************************/
{
	IMG_UINT32		uIdx;
	PUSC_LIST_ENTRY	psListEntry;

	for (uIdx = 0, psListEntry = psFirstListEntry; uIdx < uRangeLength; uIdx++, psListEntry = psListEntry->psNext)
	{
		PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

		if (!IsStaticConst(psState, psConst, auValue[uIdx]))
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

static IMG_BOOL IsValidSecAttrGroup(PINTERMEDIATE_STATE psState, 
									PUSC_LIST_ENTRY		psFirstListEntry, 
									IMG_UINT32			uGroupLength,
									HWREG_ALIGNMENT		eHwRegAlignment)
/*********************************************************************************
 Function			: IsValidSecAttrGroup

 Description		: Check if a range of secondary attributes can be given
					  consecutive hardware register numbers.

 Parameters			: psState			- Compiler state.
					  psFirstListEntry	- Start of the range to check.
					  uGroupLength		- Length of the range.
					  eHwRegAlignment	- Required alignment for the hardware
										register number assigned to the first
										secondary attribute in the range.

 Return				: TRUE or FALSE.
*********************************************************************************/
{
	IMG_UINT32		uIdx;
	PUSC_LIST_ENTRY	psListEntry;
	PREGISTER_GROUP	apsGroup[USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH];

	ASSERT(uGroupLength <= USC_MAXIMUM_CONSECUTIVE_REGISTER_SET_LENGTH);
	for (uIdx = 0, psListEntry = psFirstListEntry; uIdx < uGroupLength; uIdx++, psListEntry = psListEntry->psNext)
	{
		PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);
		PFIXED_REG_DATA		psConstFixedReg = psConst->psResult->psFixedReg;

		apsGroup[uIdx] = FindRegisterGroup(psState, psConstFixedReg->auVRegNum[0]);
	}
	return ValidGroup(apsGroup, uGroupLength, eHwRegAlignment);
}

/*********************************************************************************
 Function			: FindStaticConst

 Description		: Check for a range of secondary attributes with static values.

 Parameters			: psState			- Compiler state.
					  puValue			- Array of static values to check for.
					  uRangeLength		- Number of values to check for.
					  eHwRegAlignment	- Hardware register alignment required 
										for first Secondary Attribute in the 
										searched group

 Return				: The start of the range found or NULL if no range was found.
*********************************************************************************/
static PINREGISTER_CONST FindStaticConst(PINTERMEDIATE_STATE	psState,
									     IMG_PUINT32			puValue,
										 IMG_UINT32				uRangeLength,
										 HWREG_ALIGNMENT		eHwRegAlignment)
{
	IMG_UINT32		uCount;
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uConstIdx;

	if (psState->sSAProg.uInRegisterConstantCount < uRangeLength)
	{
		return NULL;
	}
	uCount = psState->sSAProg.uInRegisterConstantCount - uRangeLength;

	for (psListEntry = psState->sSAProg.sInRegisterConstantList.psHead, uConstIdx = 0; 
		 uConstIdx <= uCount; 
		 psListEntry = psListEntry->psNext, uConstIdx++)
	{
		PINREGISTER_CONST	psConst;

		/*
			Check for a matching range of secondary attributes with compile-time known
			values.
		*/
		if (!IsStaticConstRange(psState, psListEntry, puValue, uRangeLength))
		{
			continue;
		}

		/*
			Check the secondary attributes have correctly ordered and aligned hardware
			register numbers.
		*/
		if (!IsValidSecAttrGroup(psState, psListEntry, uRangeLength, eHwRegAlignment))
		{
			continue;
		}

		/*
			Return the first constant in the range.
		*/
		psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);
		return psConst;
	}
	return NULL;
}

IMG_INTERNAL
IMG_UINT32 GetSAProgNumResultRegisters(PINTERMEDIATE_STATE	psState,
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   IMG_BOOL				bVector,
									   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   UF_REGFORMAT			eFmt)
/*********************************************************************************
 Function			: GetSAProgNumResultRegisters

 Description		: Get the number of hardware registers required to hold
					  a result from the secondary update program.

 Parameters			: psState			- Compiler state.
					  eFmt				- Format of the output.
					  bVector			- TRUE if the output is a vector register.

 Return				: The number of hardware registers required.
*********************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (eFmt == UF_REGFORMAT_C10)
	{
		/*
			Two 32-bit registers for a 4xC10 value.
		*/
		return 2;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	else if (bVector)
	{
		if (eFmt == UF_REGFORMAT_F32)
		{
			/*
				Four 32-bit registers for a 4xF32 value.
			*/
			return 4;
		}
		else
		{
			/*
				Two 32-bit registers for a 4xF16 value.
			*/
			ASSERT(eFmt == UF_REGFORMAT_F16);
			return 2;
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	else
	{
		return 1;
	}
}

IMG_INTERNAL
IMG_VOID DropSAProgResult(PINTERMEDIATE_STATE psState, PSAPROG_RESULT psResult)
/*********************************************************************************
 Function			: DropSAProgResult

 Description		: Drop the structure representing a result calculated in the
					  secondary update program.

 Parameters			: psState			- Compiler state.
					  psResult			- Secondary update program result to drop.

 Return				: Nothing.
*********************************************************************************/
{
	PSAPROG_STATE	psSAProg = &psState->sSAProg;
	PFIXED_REG_DATA	psFixedReg = psResult->psFixedReg;
	PVREGISTER		psSAResultVReg;

	/*
		Remove from the list of SA program results.
	*/
	RemoveFromList(&psSAProg->sResultsList, &psResult->sListEntry);

	ASSERT(psSAProg->uNumResults > 0);
	psSAProg->uNumResults--;

	ASSERT(psSAProg->uConstSecAttrCount >= psResult->uNumHwRegisters);
	psSAProg->uConstSecAttrCount -= psResult->uNumHwRegisters;

	ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
	ASSERT(psFixedReg->uConsecutiveRegsCount == 1);

	/*
		Clear the link from the intermediate register which contains the result.
	*/
	psSAResultVReg = GetVRegister(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[0]);
	ASSERT(psSAResultVReg->psSecAttr == psResult);
	psSAResultVReg->psSecAttr = NULL;

	/*
		Remove the corresponding fixed register from the list.
	*/
	RemoveFromList(&psState->sSAProg.sFixedRegList, &psFixedReg->sListEntry);

	if (psResult->eType == SAPROG_RESULT_TYPE_DRIVERLOADED)
	{
		IMG_UINT32					uConstIdx;
		PCONSTANT_INREGISTER_RANGE	psDriverConstRange;

		/*
			Remove from the list of results associated with a range.
		*/
		psDriverConstRange = psResult->u1.sDriverConst.psDriverConstRange;
		if (psDriverConstRange != NULL)
		{
			RemoveFromList(&psDriverConstRange->sResultList, &psResult->sRangeListEntry);
		}

		/*
			Clear any references to this result from constants loaded by the driver.
		*/
		for (uConstIdx = 0; uConstIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uConstIdx++)
		{
			PINREGISTER_CONST	psDriverConst = psResult->u1.sDriverConst.apsDriverConst[uConstIdx];

			if (psDriverConst != NULL)
			{
				ASSERT(psDriverConst->psResult == psResult);
				psDriverConst->psResult = NULL;
			}
		}

		/*
			Remove from the list of secondary attribute loaded by the driver.
		*/
		ASSERT((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) == 0);
		RemoveFromList(&psSAProg->sDriverLoadedSAList, &psResult->u1.sDriverConst.sDriverLoadedSAListEntry);
	}

	/*
		Free the fixed register.
	*/
	ReleaseFixedReg(psState, psResult->psFixedReg);

	UscFree(psState, psResult);
}

IMG_INTERNAL
IMG_VOID SetChansUsedFromSAProgResult(PINTERMEDIATE_STATE psState, PSAPROG_RESULT psResult)
/*********************************************************************************
 Function			: SetChansUsedFromSAProgResult

 Description		: Update the number of channels used in the main program from
					  an intermediate register defined in the secondary update
					  program.

 Parameters			: psState			- Compiler state.
					  psResult			- Intermediate register to update.

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uChansUsed;

	uChansUsed = GetRegisterLiveMask(psState, 
									 &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut, 
									 USEASM_REGTYPE_TEMP, 
									 psResult->psFixedReg->auVRegNum[0], 
									 0 /* uArrayOffset */);
	if (uChansUsed == 0)
	{
		DropSAProgResult(psState, psResult);
	}
	else if (uChansUsed != USC_ALL_CHAN_MASK)
	{
		PFIXED_REG_DATA	psFixedReg = psResult->psFixedReg;

		ASSERT(psFixedReg->uConsecutiveRegsCount == 1);
		if (psFixedReg->auMask == NULL)
		{
			psFixedReg->auMask = UscAlloc(psState, sizeof(psFixedReg->auMask[0]));
		}
		psFixedReg->auMask[0] = uChansUsed;
	}
}

static
PREGISTER_GROUP AddSAProgResultGroup(PINTERMEDIATE_STATE	psState,
									 PSAPROG_RESULT			psResult)
/*********************************************************************************
 Function			: AddSAProgResultGroup

 Description		: Set up register grouping information for a secondary attribute
					  added after register grouping has been set up.

 Parameters			: psState			- Compiler state.
					  psResult			- Newly added secondary attribute.

 Return				: A pointer to the register group information for the secondary
					  attribute.
*********************************************************************************/
{
	PREGISTER_GROUP	psGroup;
	PFIXED_REG_DATA	psFixedReg = psResult->psFixedReg;

	/*
		Create register grouping information.
	*/
	psGroup = AddRegisterGroup(psState, psFixedReg->auVRegNum[0]);

	/*
		Flag it as something requiring a fixed hardware register.
	*/
	psGroup->psFixedReg = psFixedReg;
	psGroup->uFixedRegOffset = 0;

	/*
		Set the known alignment of the hardware register for the
		secondary attribute.
	*/
	if (psFixedReg->sPReg.uNumber != USC_UNDEF)
	{
		HWREG_ALIGNMENT	eHwResultAlignment;

		if ((psFixedReg->sPReg.uNumber % 2) == 0)
		{
			eHwResultAlignment = HWREG_ALIGNMENT_EVEN;
		}
		else
		{
			eHwResultAlignment = HWREG_ALIGNMENT_ODD;
		}
		SetNodeAlignment(psState, psGroup, eHwResultAlignment, IMG_TRUE /* bAlignRequiredByInst */);
	}

	return psGroup;
}

IMG_INTERNAL
PSAPROG_RESULT BaseAddNewSAProgResult(PINTERMEDIATE_STATE	psState,
									  UF_REGFORMAT			eFmt,
									  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									  IMG_BOOL				bVector,
									  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									  IMG_UINT32			uHwRegAlignment,
									  IMG_UINT32			uNumHwRegisters,
									  IMG_UINT32			uTempRegNum,
									  IMG_UINT32			uUsedChanMask,
									  SAPROG_RESULT_TYPE	eType,
									  IMG_BOOL				bPartOfRange)
/*********************************************************************************
 Function			: BaseAddNewSAProgResult

 Description		: Add a new output from the secondary update program.

 Parameters			: psState			- Compiler state.
					  eFmt				- Format of the result.
					  bVector			- TRUE if the result is a vector register.
					  uHwRegAlignment	- Required alignment of the hardware
										registers.
					  uNumHwRegisters	- Number of hardware registers needed to
										hold the result.
					  uTempRegNum		- Variable holding the result.
					  uUsedChanMask		- Mask of channels used from the result.
					  eType				- Type of result to add.
					  bPartOfRange		- TRUE if the result is part of a dynamically
										indexed range.

 Return				: A pointer to the new structure describing the result.
*********************************************************************************/
{
	PSAPROG_RESULT	psNewResult;
	PSAPROG_STATE	psSAProg = &psState->sSAProg;
	PFIXED_REG_DATA	psFixedReg;
	IMG_UINT32		uHwResultRegNum;
	PVREGISTER		psVRegister;
	IMG_UINT32		uHwResultRegType;

	ASSERT((psState->uFlags &  USC_FLAGS_POSTCONSTREGPACK) != 0);

	/*
		Create a structure representing the secondary update program
		result.
	*/
	psNewResult = UscAlloc(psState, sizeof(*psNewResult));

	/*
		Assign a hardware register number for the constant.
	*/
	if ((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) != 0 && eType == SAPROG_RESULT_TYPE_DRIVERLOADED)
	{
		AllocDriverLoadedSecondaryAttributes(psState, 
											 uNumHwRegisters, 
											 uHwRegAlignment, 
											 &uHwResultRegNum,
											 NULL /* puAlignmentPadding */);
	}
	else
	{
		/*
			Hardware registers haven't yet been assigned for secondary attributes. We ignore the supplied
			alignment and the caller must associate it with the virtual register representing the
			secondary attribute. The register allocator will then take care of assigned the virtual
			register a correctly aligned hardware register number.
		*/
		uHwResultRegNum = USC_UNDEF;
	}

	if ((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) == 0 && eType == SAPROG_RESULT_TYPE_CALC)
	{
		uHwResultRegType = USC_REGTYPE_CALCSECATTR;
	}
	else
	{
		uHwResultRegType = USEASM_REGTYPE_SECATTR;
	}

	/*
		Create a structure representing a variable which requires a fixed hardware
		register. The hardware register must be a primary attribute.
	*/
	psNewResult->psFixedReg = psFixedReg = 
		AddFixedReg(psState, 
					IMG_FALSE /* bPrimary */,
					eType != SAPROG_RESULT_TYPE_CALC ? IMG_FALSE : IMG_TRUE /* bLiveAtShaderEnd */,
					uHwResultRegType,
					uHwResultRegNum,
					1 /* uConsecutiveRegsCount */);

	psFixedReg->uVRegType = USEASM_REGTYPE_TEMP;
	psFixedReg->auVRegNum[0] = uTempRegNum;
	if (eFmt != UF_REGFORMAT_INVALID)
	{
		psFixedReg->aeVRegFmt[0] = eFmt;
	}
	else
	{
		psFixedReg->aeVRegFmt[0] = UF_REGFORMAT_UNTYPED;
	}

	if (uUsedChanMask != USC_ALL_CHAN_MASK)
	{
		psFixedReg->auMask = UscAlloc(psState, sizeof(psFixedReg->auMask[0]));
		psFixedReg->auMask[0] = uUsedChanMask;
	}

	psVRegister = GetVRegister(psState, USEASM_REGTYPE_TEMP, uTempRegNum);
	ASSERT(psVRegister->psSecAttr == NULL);
	psVRegister->psSecAttr = psNewResult;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psNewResult->bVector = bVector;
	psNewResult->psFixedReg->bVector = bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (!psFixedReg->bLiveAtShaderEnd)
	{
		UseDefAddFixedRegDef(psState, psFixedReg, 0 /* uRegIdx */);
	}
	else
	{
		UseDefAddFixedRegUse(psState, psFixedReg, 0 /* uRegIdx */);
	}

	psNewResult->bPartOfRange = bPartOfRange;

	psNewResult->eFmt = eFmt;
	if (eFmt != UF_REGFORMAT_INVALID)
	{
		UseDefSetFmt(psState, uTempRegNum, eFmt);
	}
	psNewResult->uNumHwRegisters = uNumHwRegisters;

	psNewResult->eType = eType;
	if (eType == SAPROG_RESULT_TYPE_DRIVERLOADED)
	{
		psNewResult->u1.sDriverConst.psDriverConstRange = NULL;
		AppendToList(&psState->sSAProg.sDriverLoadedSAList, &psNewResult->u1.sDriverConst.sDriverLoadedSAListEntry);
	}

	/*
		Flag this variable as requiring a secondary attribute hardware register.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_SETUP_REGISTER_GROUPS) != 0)
	{
		AddSAProgResultGroup(psState, psNewResult);
	}

	/*
		Append to the global list of secondary update program results.
	*/
	AppendToList(&psSAProg->sResultsList, &psNewResult->sListEntry);
	psSAProg->uNumResults++;

	/*
		Track the number of hardware registers used for secondary
		update program results.
	*/
	if (eType != SAPROG_RESULT_TYPE_DIRECTMAPPED)
	{
		psSAProg->uConstSecAttrCount += uNumHwRegisters;
	}
	
	ClearListEntry(&psNewResult->sRangeListEntry);
	psNewResult->u.psRegAllocData = IMG_NULL;
	
	return psNewResult;
}

IMG_INTERNAL
IMG_VOID AddNewSAProgResult(PINTERMEDIATE_STATE	psState,
							UF_REGFORMAT			eFmt,
							#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							IMG_BOOL				bVector,
							#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							IMG_UINT32				uNumHwRegisters,
							PARG					psResult)
/*********************************************************************************
 Function			: AddNewSAProgResult

 Description		: Add a new output from the secondary update program.

 Parameters			: psState			- Compiler state.
					  eFmt				- Format of the result.
					  bVector			- TRUE if the result is a vector register.
					  uNumHwRegisters	- Number of hardware registers needed to
										hold the result.
					  psResult			- Returns the intermediate register
										representing the secondary update program
										result.

 Return				: Nothing.
*********************************************************************************/
{
	/*
		Grab a new temporary register for this result.
	*/
	MakeNewTempArg(psState, eFmt, psResult);

	/*
		Add a new entry to the list of results of the secondary update program.
	*/
	(IMG_VOID)BaseAddNewSAProgResult(psState,
									 eFmt,
									 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									 bVector,
									 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									 0 /* uHwRegAlignment */,
									 uNumHwRegisters,
									 psResult->uNumber,
									 USC_ALL_CHAN_MASK,
									 SAPROG_RESULT_TYPE_CALC,
									 IMG_FALSE /* bPartOfRange */);
}

static const IMG_UINT32	g_auChansPerDword[] = 
{
	F32_CHANNELS_PER_SCALAR_REGISTER,	/* UNIFLEX_CONST_FORMAT_F32 */
	F16_CHANNELS_PER_SCALAR_REGISTER,	/* UNIFLEX_CONST_FORMAT_F16 */
	C10_CHANNELS_PER_SCALAR_REGISTER,	/* UNIFLEX_CONST_FORMAT_C10 */
	U8_CHANNELS_PER_SCALAR_REGISTER,	/* UNIFLEX_CONST_FORMAT_U8 */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_STATIC */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_RTA_IDX */
	USC_UNDEF,							/* UNIFLEX_CONST_FORMAT_UNDEF */
};

static
IMG_VOID AddNewInRegisterConstant(PINTERMEDIATE_STATE			psState,
								  IMG_UINT32					uNum,
								  UNIFLEX_CONST_FORMAT			eFormat,
								  IMG_UINT32					uBuffer,
								  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								  IMG_BOOL						bVector,
								  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								  IMG_UINT32					uAlignment,
								  IMG_UINT32					uUsedDwordMask,
								  IMG_UINT32					uUsedChanMask,
								  IMG_UINT32					uResultRegNum,
								  PCONSTANT_INREGISTER_RANGE	psDriverConstRange,
								  PINREGISTER_CONST*			ppsConst,
								  IMG_PUINT32					puRegNum)
/*********************************************************************************
 Function			: AddNewInRegisterConstant

 Description		: Add a new constant to the list loaded by the driver.

 Parameters			: psState			- Compiler state.
					  uNum				- Constant number to load into the
										secondary attribute.
					  eFormat			- Format to convert the constant to.
					  uBuffer			- Constant buffer to take the constant
										from.
					  bVector
					  uAlignment		- If hardware register numbers have been assigned
										to secondary attributes then the required alignment.
					  uUsedDwordMask	- Mask of the dword parts of a constant
										vector which are referenced.
					  uUsedChanMask		- Mask of channels used from the constant.
					  uResultRegNum		- If before secondary attribute hardware register
										numbers are allocated and not equal to USC_UNDEF then
										a temporary variable to load the constant into.
					  psDriverConstRange
										- If non-NULL then indexable range of secondary
										attributes this one belongs to.
					  ppsConst			- If non-NULL returns a pointer to the
										created secondary attribute load.
					  puRegNum			- If non-NULL returns the location of
										access the loaded secondary attribute.

 Return				: Nothing.
*********************************************************************************/
{
	PSAPROG_RESULT	psNewResult;
	IMG_UINT32		uDwordIdx;
	UF_REGFORMAT	eRegisterFormat;
	IMG_BOOL		bPartOfRange;

	ASSERT((psState->uFlags2 & USC_FLAGS2_ASSIGNED_TEMPORARY_REGNUMS) == 0);

	/*
		Convert the constant format to a register format.
	*/
	switch (eFormat)
	{
		case UNIFLEX_CONST_FORMAT_F32:	eRegisterFormat = UF_REGFORMAT_F32; break;
		case UNIFLEX_CONST_FORMAT_F16:	eRegisterFormat = UF_REGFORMAT_F16; break;
		case UNIFLEX_CONST_FORMAT_C10:	eRegisterFormat = UF_REGFORMAT_C10; break;
		default:						eRegisterFormat = UF_REGFORMAT_INVALID; break;
	}

	/*
		If the caller didn't supply a register to load the constant into then allocate
		one.
	*/
	if (uResultRegNum == USC_UNDEF)
	{
		uResultRegNum = GetNextRegister(psState);
	}

	/*
		Is the constant part of a dynamically indexed range?
	*/
	bPartOfRange = IMG_FALSE;
	if (psDriverConstRange != NULL)
	{
		bPartOfRange = IMG_TRUE;
	}

	/*
		Add an entry to the list of results of the secondary update program.
	*/
	psNewResult = BaseAddNewSAProgResult(psState, 
								 	     eRegisterFormat, 
										 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
										 bVector,
										 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
										 uAlignment,
										 g_auSetBitCount[uUsedDwordMask],
										 uResultRegNum,
										 uUsedChanMask,
										 SAPROG_RESULT_TYPE_DRIVERLOADED,
										 bPartOfRange);

	/*
		Set the range of indexable secondary attribute associated with this constant.
	*/
	psNewResult->u1.sDriverConst.psDriverConstRange = psDriverConstRange;
	if (psDriverConstRange != NULL)
	{
		AppendToList(&psDriverConstRange->sResultList, &psNewResult->sRangeListEntry);
		psNewResult->psFixedReg->uRegArrayIdx = psDriverConstRange->uRegArrayIdx;
	}

	/*
		Add an entry to the list of secondary attributes for the driver to load for
		each referenced dword part of the vector.
	*/
	for (uDwordIdx = 0; uDwordIdx < MAX_SCALAR_REGISTERS_PER_VECTOR; uDwordIdx++)
	{
		if ((uUsedDwordMask & (1U << uDwordIdx)) != 0)
		{
			PINREGISTER_CONST	psNewConst;

			/*
				Create the new request for the driver to load a secondary attribute.
			*/
			psNewConst = UscAlloc(psState, sizeof(*psNewConst));
			if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0)
			{
				psNewConst->uNum = uNum + uDwordIdx * g_auChansPerDword[eFormat];
			}
			else
			{
				psNewConst->uNum = uNum + uDwordIdx * LONG_SIZE;
			}
			psNewConst->eFormat = eFormat;
			psNewConst->uBuffer = uBuffer;

			if (ppsConst != NULL)
			{
				*ppsConst = psNewConst;
			}

			/*
				Associate the secondary attribute load with the secondary update program result.
			*/
			psNewResult->u1.sDriverConst.apsDriverConst[uDwordIdx] = psNewConst;
			psNewConst->psResult = psNewResult;

			AppendToList(&psState->sSAProg.sInRegisterConstantList, &psNewConst->sListEntry);
			psState->sSAProg.uInRegisterConstantCount++;
		}
		else
		{
			if (psNewResult != NULL)
			{
				psNewResult->u1.sDriverConst.apsDriverConst[uDwordIdx] = NULL;
			}
		}
	}

	/*
		Record that the constant is needed *after* the SA Program.
	*/
	SetRegisterLiveMask(psState,
						&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut, 
						USEASM_REGTYPE_TEMP,
						uResultRegNum,
						0 /* uArrayOffset */,
						uUsedChanMask);

	if (puRegNum != NULL)
	{
		*puRegNum = uResultRegNum;
	}
}

static IMG_VOID AddSimpleSecAttr(PINTERMEDIATE_STATE	psState,
								 IMG_UINT32				uAlignment,
							     IMG_UINT32				uNum,
								 UNIFLEX_CONST_FORMAT	eFormat,
								 IMG_UINT32				uBuffer,
								 PINREGISTER_CONST*		ppsConst)
/*********************************************************************************
 Function			: AddSimpleSecAttr

 Description		: Add a secondary attribute to this for the driver to load.

 Parameters			: psState			- Compiler state.
					  uAlignment		- Required alignment of the added 
										secondary attribute.
					  uNum				- Constant number to load into the
										secondary attribute.
					  eFormat			- Format to convert the constant to.
					  uBuffer			- Constant buffer to take the constant
										from.
					  ppsConst			- If non-NULL returns a pointer to the
										created secondary attribute load.

 Return				: Nothing.
*********************************************************************************/
{
	AddNewInRegisterConstant(psState,
						     uNum,
							 eFormat,
							 uBuffer,
							 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							 IMG_FALSE /* bVector */,
							 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							 uAlignment,
							 USC_X_CHAN_MASK /* uUsedDwordMask */,
							 USC_ALL_CHAN_MASK /* uUsedChanMask */,
							 USC_UNDEF /* uResultRegNum */,
							 NULL /* psDriverConstRange */,
							 ppsConst,
							 NULL /* puRegNum */);
}

/*********************************************************************************
 Function			: AddSpillAreaOffsetPlaceholder

 Description		: Add a reserved entry for an offset into the spill area to 
					  the list of secondary attributes to be loaded by the driver.
 
 Parameters			: psState			- Current compiler state.
					  puSecAttr			- Returns the number of the secondary
										attribute
					  ppsConst			- Returns a pointer to the added entry.

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID AddSpillAreaOffsetPlaceholder(PINTERMEDIATE_STATE	psState,
									   PINREGISTER_CONST*	ppsConst)
{
	AddSimpleSecAttr(psState,
					 0 /* uAlignment */,
				     USC_UNDEF /* uNum */,
					 UNIFLEX_CONST_FORMAT_UNDEF,
					 USC_UNDEF /* uBuffer */,
					 ppsConst);
}

/*********************************************************************************
 Function			: ReplaceSpillAreaOffsetByStatic

 Description		: Replace a entry in the list of secondary attributes reserved
					  for a memory offset into the spill area by the actual offset.
 
 Parameters			: psState			- Current compiler state.
					  psConst			- Points to the entry to replace.
					  uNewValue			- Memory offset.

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL
IMG_VOID ReplaceSpillAreaOffsetByStatic(PINTERMEDIATE_STATE	psState,
										PINREGISTER_CONST	psConst,
										IMG_UINT32			uNewValue)
{
	ASSERT(psConst->eFormat == UNIFLEX_CONST_FORMAT_UNDEF);
	psConst->eFormat = UNIFLEX_CONST_FORMAT_STATIC;
	psConst->uBuffer = psState->uStaticConstsBuffer;
	psConst->uNum = uNewValue;
}

/*********************************************************************************
 Function			: AddStaticSecAttrRange

 Description		: Try to add a range of secondary attributes with static values.
 
 Parameters			: psState			- Current compiler state.
					  puValue			- Values of the secondary attributes to add.
					  uCount			- Count of values to add.
					  ppsConst			- If NULL just check if adding the secondary
										attributes is possible.
										  If non-NULL returns the first secondary attribute.
					  eHwRegAlignment	- Hardware register alignment required for 
										first Secondary Attribute in group

 Return				: TRUE if the secondary attribute can be added.
*********************************************************************************/
IMG_INTERNAL
IMG_BOOL AddStaticSecAttrRange(PINTERMEDIATE_STATE		psState,
							   IMG_PUINT32				puValue,
							   IMG_UINT32				uCount,
							   PINREGISTER_CONST*		ppsBaseConst,
							   HWREG_ALIGNMENT			eHwRegAlignment)
{
	PINREGISTER_CONST	psFirstConst;

	/*
		If we found a secondary attribute with the same value then reuse that.
	*/
	psFirstConst = FindStaticConst(psState, puValue, uCount, eHwRegAlignment);
	
	if (psFirstConst == NULL)
	{
		IMG_UINT32	uAlignment;
		IMG_UINT32	uAlignmentPadding;

		/*
			Don't add any more secondary attribute loads after assigning registers for
			the secondary update program. This is because all the secondary attributes
			loaded by the driver need to have consecutive numbers but after register
			allocation the secondary attribute after the existing loads may have
			been assigned to hold a result calculated in the secondary update
			program.
		*/
		if ((psState->uFlags & USC_FLAGS_ASSIGNEDSECPROGREGISTERS) != 0)
		{
			ASSERT(ppsBaseConst == NULL);
			return IMG_FALSE;
		}

		/*
			Get the required alignment for the secondary attributes.
		*/
		uAlignment = 0;
		if (eHwRegAlignment == HWREG_ALIGNMENT_EVEN)
		{
			uAlignment = 2;
		}

		/*
			Check that there are secondary attributes available.
		*/
		if ((psState->uFlags & USC_FLAGS_ASSIGNED_SECATTR_REGNUMS) != 0)
		{
			if (!AllocDriverLoadedSecondaryAttributes(psState, 
													  uCount, 
													  uAlignment, 
													  NULL /* puFirstAllocatedSA */, 
													  &uAlignmentPadding))
			{
				ASSERT(ppsBaseConst == NULL);
				return IMG_FALSE;
			}
		}
		else
		{
			uAlignmentPadding = 0;
		}
		if ((psState->sSAProg.uConstSecAttrCount + uAlignmentPadding + uCount) > psState->sSAProg.uInRegisterConstantLimit)
		{
			ASSERT(ppsBaseConst == NULL);
			return IMG_FALSE;
		}

		/*
			If we aren't just checking then create a new entry in the in-register constant map for the
			floating point constant.
		*/
		if (ppsBaseConst != NULL)
		{
			IMG_UINT32	uIdx;
			for (uIdx = 0; uIdx < uCount; uIdx++)
			{
				AddSimpleSecAttr(psState,
								 uAlignment,
							     puValue[uIdx],
								 UNIFLEX_CONST_FORMAT_STATIC,
								 USC_UNDEF /* uBuffer */,
								 (uIdx == 0) ? &psFirstConst : NULL /* ppsConst */);

				/*
					Align only the first added secondary attribute.
				*/
				uAlignment = 0;
			}
		}
	}

	/*
		Return the details of where to access the constant.
	*/
	if (ppsBaseConst != NULL)
	{
		*ppsBaseConst = psFirstConst;
	}

	return IMG_TRUE;
}


/*********************************************************************************
 Function			: AddStaticSecAttr

 Description		: Try to add a secondary attribute with a static value.
 
 Parameters			: psState			- Current compiler state.
					  uValue			- Value of the secondary attribute.
					  puSecAttr			- If NULL then just check if adding the 
										constant is possible..
										 If non-NULL then returns the number of the
										secondary attribute.

 Return				: TRUE if the secondary attribute can be added.
*********************************************************************************/
IMG_INTERNAL
IMG_BOOL AddStaticSecAttr(PINTERMEDIATE_STATE		psState,
						  IMG_UINT32				uValue,
						  IMG_PUINT32				puArgType,
						  IMG_PUINT32				puArgNum)
{
	IMG_BOOL			bRet;
	PINREGISTER_CONST	psConst = NULL;

	bRet = AddStaticSecAttrRange(psState, 
								 &uValue, 
								 1 /* uCount */, 
								 (puArgNum != NULL) ? &psConst : NULL, 
								 HWREG_ALIGNMENT_NONE);
	if (puArgNum != NULL)
	{
		ASSERT(bRet);
		GetDriverLoadedConstantLocation(psState, psConst, puArgType, puArgNum);
	}
	return bRet;
}

/*********************************************************************************
 Function			: ReplaceHardwareConstBySecAttr

 Description		: Create a secondary attribute with the same value as a
					  hardware constant.
 
 Parameters			: psState			- Current compiler state.
					  uConstIdx			- Hardware constant number.
					  bNegate			- if TRUE use the negated value of the 
										hardware constant.
					  puArgType			- Returns the type to use to access the
										secondary attribute.
					  puArgNum			- If NULL then just check if replacing the
										hardware constant is possible.
										If non-NULL then returns the number of the
										secondary attribute.

 Return				: TRUE if replacement is possible.
*********************************************************************************/
IMG_BOOL IMG_INTERNAL ReplaceHardwareConstBySecAttr(PINTERMEDIATE_STATE	psState,
													IMG_UINT32			uConstIdx,
													IMG_BOOL			bNegate,
													IMG_PUINT32			puArgType,
													IMG_PUINT32			puArgNum)
{
	IMG_UINT32	uValue;

	/*
		Get the value of the hardware constant.
	*/
	if (uConstIdx == EURASIA_USE_SPECIAL_CONSTANT_INT32ONE)
	{
		uValue = 0xffffffff;
		ASSERT(!bNegate);
	}
	else
	{
		IMG_FLOAT	fValue;

		fValue = g_pfHardwareConstants[uConstIdx];
		if (bNegate)
		{
			fValue = -fValue;
		}
		uValue = *(IMG_PUINT32)&fValue;
	}

	return AddStaticSecAttr(psState, uValue, puArgType, puArgNum);
}

static
IMG_INT CBDescCmp(const IMG_VOID* ppvElem1, const IMG_VOID* ppvElem2)
/*********************************************************************************
 Function			: CBDescCmp

 Description		: Comparison function for constant buffers.
 
 Parameters			: ppvElem1, ppvElem2	- Pointers to the elements to compare.

 Return				: -1 if ppvElem1 < ppvElem
					   0 if ppvElem1 == ppvElem
				       1 if ppvElem1 > ppvElem
*********************************************************************************/
{
	const PUNIFLEX_CONSTBUFFERDESC* ppsElem1 = (const PUNIFLEX_CONSTBUFFERDESC*)ppvElem1;
	const PUNIFLEX_CONSTBUFFERDESC* ppsElem2 = (const PUNIFLEX_CONSTBUFFERDESC*)ppvElem2;

	const PUNIFLEX_CONSTBUFFERDESC psElem1 = *ppsElem1;
	const PUNIFLEX_CONSTBUFFERDESC psElem2 = *ppsElem2;

	return (IMG_INT32)(psElem1->uStartingSAReg - psElem2->uStartingSAReg);
}

static
IMG_VOID MarkSAOnlyRegionsReserved(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: MarkSAOnlyRegionsReserved

 Description		: Flag the regions of the range of secondary attributes made
					  available to us by the driver which are used for constant
					  buffers mapped directly to secondary attributes.
 
 Parameters			: psState			- Current compiler state.

 Return				: None.
*********************************************************************************/
{
	IMG_UINT32					uCBIdx;
	IMG_UINT32					uSAOnlyCount;
	PUNIFLEX_CONSTBUFFERDESC*	apsCBDesc;
	PSA_ALLOC_REGION			asAllocRegions;
	IMG_UINT32					uRegionStart;
	IMG_UINT32					uRegionEnd;
	IMG_UINT32					uSAsAvailable;
	IMG_UINT32					uSAOnlyIdx;
	IMG_UINT32					uNextRegion;
	IMG_UINT32					uMaximumConsecutiveSAsAvailable;
	IMG_UINT32					uRegionIdx;

	/*
		Count the number of constant buffers which are mapped directly
		to secondary attributes.
	*/
	uSAOnlyCount = 0;
	for (uCBIdx = 0; uCBIdx < psState->uNumOfConstsBuffsAvailable; uCBIdx++)
	{
		PUNIFLEX_CONSTBUFFERDESC	psCBDesc = &psState->psSAOffsets->asConstBuffDesc[uCBIdx];

		if (psCBDesc->eConstBuffLocation == UF_CONSTBUFFERLOCATION_SAS_ONLY)
		{
			uSAOnlyCount++;
		}
	}
	
	/*
		Check for no constant buffer mapped directly to SAs. The whole of the region reserved
		by the driver is available.
	*/
	if (uSAOnlyCount == 0)
	{
		PSA_ALLOC_REGION	psWholeRegion;

		psWholeRegion = UscAlloc(psState, sizeof(*psWholeRegion));
		psWholeRegion->uStart = psState->psSAOffsets->uInRegisterConstantOffset;
		psWholeRegion->uLength = psState->psSAOffsets->uInRegisterConstantLimit;
		
		psState->sSAProg.uAllocRegionCount = 1;
		psState->sSAProg.asAllocRegions = psWholeRegion;
		psState->sSAProg.uInRegisterConstantLimit = psState->psSAOffsets->uInRegisterConstantLimit;
		psState->sSAProg.uTotalSAsAvailable = psState->psSAOffsets->uInRegisterConstantLimit;
		psState->sSAProg.uMaximumConsecutiveSAsAvailable = psState->psSAOffsets->uInRegisterConstantLimit;
		return;
	}

	/*
		Make an array of just the constant buffers mapped directly to
		secondary attributes.
	*/
	apsCBDesc = UscAlloc(psState, sizeof(apsCBDesc[0]) * uSAOnlyCount);
	uSAOnlyIdx = 0;
	for (uCBIdx = 0; uCBIdx < psState->uNumOfConstsBuffsAvailable; uCBIdx++)
	{
		PUNIFLEX_CONSTBUFFERDESC	psCBDesc = &psState->psSAOffsets->asConstBuffDesc[uCBIdx];

		if (psCBDesc->eConstBuffLocation == UF_CONSTBUFFERLOCATION_SAS_ONLY)
		{
			apsCBDesc[uSAOnlyIdx++] = psCBDesc;
		}
	}
	ASSERT(uSAOnlyCount == uSAOnlyIdx);

	/*
		Sort the array by the starting secondary attribute.
	*/
	qsort(apsCBDesc, uSAOnlyCount, sizeof(apsCBDesc[0]), CBDescCmp);

	/*
		Allocate space for the maximum possible fragmentation of the region of SAs
		reserved by the driver for our use.
	*/
	asAllocRegions = UscAlloc(psState, sizeof(asAllocRegions[0]) * (uSAOnlyCount + 1));
	
	/*
		Work out which regions of the reserved SAs don't overlap with constant
		buffers mapped directly to SAs.
	*/
	uRegionStart = psState->psSAOffsets->uInRegisterConstantOffset;
	uRegionEnd = uRegionStart + psState->psSAOffsets->uInRegisterConstantLimit;
	uNextRegion = 0;
	for (uCBIdx = 0; uCBIdx < uSAOnlyCount; uCBIdx++)
	{
		PUNIFLEX_CONSTBUFFERDESC	psCBDesc = apsCBDesc[uCBIdx];
		IMG_UINT32					uCBStart;
		IMG_UINT32					uCBEnd;

		uCBStart = psCBDesc->uStartingSAReg;
		uCBEnd = uCBStart + psCBDesc->uSARegCount;

		/*
			Skip if the secondary attributes for this constant buffer are before the
			start of the region available for our allocations.
		*/
		if (uCBEnd < uRegionStart)
		{
			continue;
		}
		if (uCBStart > uRegionStart)
		{
			PSA_ALLOC_REGION	psRegion;
			IMG_UINT32			uThisRegionEnd;

			/*
				Create a region from the current start of the reserved region to the start 
				of the secondary attributes for this constant buffer.
			*/
			psRegion = &asAllocRegions[uNextRegion++];
			psRegion->uStart = uRegionStart;
			uThisRegionEnd = min(uCBStart, uRegionEnd);
			psRegion->uLength = uThisRegionEnd - uRegionStart;
		}

		uRegionStart = uCBEnd;
		if (uRegionStart >= uRegionEnd)
		{
			break;
		}
	}

	/*
		Create a region for any space available after the last constant buffer.
	*/
	if (uRegionStart < uRegionEnd)
	{
		PSA_ALLOC_REGION	psRegion;

		psRegion = &asAllocRegions[uNextRegion++];
		psRegion->uStart = uRegionStart;
		psRegion->uLength = uRegionEnd - uRegionStart;
	}
	
	/*
		Shrink the array of available secondary attributes to just the used entries.
	*/
	ASSERT(uNextRegion <= (uSAOnlyCount + 1));
	if (uNextRegion < (uSAOnlyCount + 1))
	{
		ResizeArray(psState, 
					asAllocRegions,
					(uSAOnlyCount + 1) * sizeof(asAllocRegions[0]),
					uNextRegion * sizeof(asAllocRegions[0]),
					(IMG_PVOID*)&asAllocRegions);
	}

	/*
		Store the ranges of secondary attributes available for allocation.
	*/
	psState->sSAProg.uAllocRegionCount = uNextRegion;
	psState->sSAProg.asAllocRegions = asAllocRegions;

	/*
		Get the total secondary attributes available and the largest consecutive range
	*/
	uSAsAvailable = 0;
	uMaximumConsecutiveSAsAvailable = 0;
	for (uRegionIdx = 0; uRegionIdx < psState->sSAProg.uAllocRegionCount; uRegionIdx++)
	{
		PSA_ALLOC_REGION	psRegion = &asAllocRegions[uRegionIdx];

		uSAsAvailable += psRegion->uLength;
		uMaximumConsecutiveSAsAvailable = max(uMaximumConsecutiveSAsAvailable, psRegion->uLength);
	}
	psState->sSAProg.uInRegisterConstantLimit = uSAsAvailable;	
	psState->sSAProg.uTotalSAsAvailable = uSAsAvailable;
	psState->sSAProg.uMaximumConsecutiveSAsAvailable = uMaximumConsecutiveSAsAvailable;

	/*
		Free temporary storage.
	*/
	UscFree(psState, apsCBDesc);
}

IMG_INTERNAL
IMG_VOID InitializeSAProg(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: InitializeSAProg

 Description		: Initialize state for the secondary update program.
 
 Parameters			: psState			- Current compiler state.

 Return				: None.
*********************************************************************************/
{
	PREGISTER_LIVESET	psSAOutputs;
	IMG_UINT32			uSAIdx;
	IMG_UINT32			uCBIdx;

	/*
		Reset information about secondary attribute loaded with data by the
		driver.
	*/
	InitializeList(&psState->sSAProg.sInRegisterConstantList);
	psState->sSAProg.uInRegisterConstantCount = 0;
	psState->sSAProg.uConstSecAttrCount = 0;
	psState->sSAProg.uConstantRangeCount = 0;
	InitializeList(&psState->sSAProg.sInRegisterConstantRangeList);

	InitializeList(&psState->sSAProg.sDriverLoadedSAList);

	/*
		Initialize the size of the pool of secondary attributes available for constant
		data.
	*/
	MarkSAOnlyRegionsReserved(psState);

	/*
		Flag that we haven't allocated a secondary attribute for the render
		target array index yet.
	*/
	psState->sSAProg.psSAForRTAIdx = NULL;

	/*
		Create SA Program. Will be entered as psState->psFnOutermost; since it
		neither CALLs, nor is CALLed by, any other function, this at least
		doesn't break any invariants re. nesting.
	*/
	psState->psSecAttrProg = AllocFunction(psState, "SECONDARY UPDATE");

	/*
		Flag both secondary attributes reserved by the driver as live-out of the secondary update program.
	*/
	psSAOutputs = &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut;
	ClearRegLiveSet(psState, psSAOutputs);
	for (uSAIdx = 0; uSAIdx < psState->psSAOffsets->uInRegisterConstantOffset; uSAIdx++)
	{
		SetRegisterLiveMask(psState,
							psSAOutputs,
							USEASM_REGTYPE_PRIMATTR,
							uSAIdx,
							0 /* uArrayOffset */,
							USC_ALL_CHAN_MASK);
	}
	/*
		Also add ranges of secondary attributes which are mapped directly to a specified constant buffer.
	*/
	for (uCBIdx = 0; uCBIdx < psState->uNumOfConstsBuffsAvailable; uCBIdx++)
	{
		PUNIFLEX_CONSTBUFFERDESC	psCBDesc = &psState->psSAOffsets->asConstBuffDesc[uCBIdx];

		if (psCBDesc->eConstBuffLocation == UF_CONSTBUFFERLOCATION_SAS_ONLY)
		{
			for (uSAIdx = 0; uSAIdx < psCBDesc->uSARegCount; uSAIdx++)
			{
				SetRegisterLiveMask(psState,
									psSAOutputs,
									USEASM_REGTYPE_PRIMATTR,
									psCBDesc->uStartingSAReg + uSAIdx,
									0 /* uArrayOffset */,
									USC_ALL_CHAN_MASK);
			}
		}
	}

	/*
		Reset information about values calculated in the secondary update program.
	*/
	psState->sSAProg.uNumResults = 0;
	InitializeList(&psState->sSAProg.sResultsList);
	InitializeList(&psState->sSAProg.sFixedRegList);

	/*
		Reset information about constant ranges.
	*/
	InitializeList(&psState->sSAProg.sConstantRangeList);

}

static IMG_BOOL IsLastCoordUsed(PINTERMEDIATE_STATE psState, 
								IMG_PUINT32 auChansUsed,
								PPIXELSHADER_INPUT psInput)
/*********************************************************************************
 Function			: IsLastCoordUsed

 Description		: Checks if the last coordinate channel produces by an iteration
					  is used.
 
 Parameters			: psState			- Current compiler state.
					  auChansUsed		- Channels live in the pixel shader inputs.
					  psInput			- Iteration to check.

 Globals Effected	: None

 Return				: TRUE or FALSE.
*********************************************************************************/
{
	PUNIFLEX_TEXTURE_LOAD	psTextureLoad = &psInput->sLoad;
	IMG_UINT32				uLastCoord = psTextureLoad->uCoordinateDimension - 1;
	IMG_BOOL				bIsLastCoord = IMG_FALSE;
	IMG_UINT32				uRegMask;

	ASSERT(psTextureLoad->uTexture == UNIFLEX_TEXTURE_NONE);
	
	switch (psTextureLoad->eFormat)
	{
		case UNIFLEX_TEXLOAD_FORMAT_F32:
		{
			uRegMask = GetRegMask(auChansUsed, uLastCoord);
			bIsLastCoord = (uRegMask != 0) ? IMG_TRUE : IMG_FALSE;
			break;
		}

		case UNIFLEX_TEXLOAD_FORMAT_F16:
		{
			IMG_UINT32	uLastMask = (uLastCoord & 1U) ? USC_DESTMASK_HIGH : USC_DESTMASK_LOW;

			uRegMask = GetRegMask(auChansUsed, uLastCoord >> 1);
			bIsLastCoord = ((uRegMask & uLastMask) != 0) ? IMG_TRUE : IMG_FALSE;
			break;
		}

		case UNIFLEX_TEXLOAD_FORMAT_C10:
		{
			if (uLastCoord == 3)
			{
				uRegMask = GetRegMask(auChansUsed, 1);
				bIsLastCoord = ((uRegMask & 1U) != 0) ? IMG_TRUE : IMG_FALSE;
			}
			else
			{
				IMG_UINT32	uChanForLastCoord;

				uChanForLastCoord = ConvertInputChannelToIntermediate(psState, uLastCoord);

				uRegMask = GetRegMask(auChansUsed, 0);
				bIsLastCoord = ((uRegMask & (1U << uChanForLastCoord)) != 0) ? IMG_TRUE : IMG_FALSE;
			}
			break;
		}

		case UNIFLEX_TEXLOAD_FORMAT_U8:
		{
			uRegMask = GetRegMask(auChansUsed, 0);
			bIsLastCoord = ((uRegMask & (1U << uLastCoord)) != 0) ? IMG_TRUE : IMG_FALSE;
			break;
		}

		default: imgabort();
	}

	return bIsLastCoord;
}

static
PFIXED_REG_DATA AddFixedRegForPixelShaderInput(PINTERMEDIATE_STATE	psState,
											   IMG_UINT32			uRegCount,
											   UF_REGFORMAT			eFmt,
											   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
											   IMG_BOOL				bVector,
											   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
											   IMG_UINT32			uFirstRegNum,
											   IMG_UINT32			uFirstHwRegNum)
/*********************************************************************************
 Function			: AddFixedRegForPixelShaderInput

 Description		: Create the data structure so the results of an iteration
					  or non-dependent texture sample are mapped to fixed registers
					  by the register allocator.
 
 Parameters			: psState			- Current compiler state.
					  uRegCount			- Size of the iteration result.
					  eFmt				- Format of the iteration result.
					  uFirstRegNum		- Start of the intermediate registers for
										the iteration result.
					  uFirstHwRegNum	- Start of the hardware registers for the
										iteration result.

 Globals Effected	: None

 Return				: A pointer to the fixed register data structure.
*********************************************************************************/
{
	PFIXED_REG_DATA	psFixedReg;
	IMG_UINT32		uRegIdx;
	IMG_UINT32		uChansInInput;

	psFixedReg = 
		AddFixedReg(psState, 
					IMG_TRUE /* bPrimary */,
					IMG_FALSE /* bLiveAtShaderEnd */,
					USEASM_REGTYPE_PRIMATTR,
					uFirstHwRegNum,
					uRegCount);

	psFixedReg->uVRegType = USEASM_REGTYPE_TEMP;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psFixedReg->bVector = bVector;
	#endif /*defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (uFirstRegNum == USC_UNDEF)
	{
		uFirstRegNum = GetNextRegisterCount(psState, uRegCount);
	}
	for (uRegIdx = 0; uRegIdx < uRegCount; uRegIdx++)
	{
		psFixedReg->auVRegNum[uRegIdx] = uFirstRegNum + uRegIdx;
		psFixedReg->aeVRegFmt[uRegIdx] = eFmt;
		UseDefAddDef(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[uRegIdx], &psFixedReg->asVRegUseDef[uRegIdx]);
	}

	/*
		Allocate memory to save the channels live in the registers at
		the shader start.
	*/
	uChansInInput = uRegCount * CHANS_PER_REGISTER;
	psFixedReg->puUsedChans = UscAlloc(psState, UINTS_TO_SPAN_BITS(uChansInInput) * sizeof(IMG_UINT32));

	/*
		Set the default values to all channels live.
	*/
	memset(psFixedReg->puUsedChans, 0xFF, UINTS_TO_SPAN_BITS(uChansInInput) * sizeof(IMG_UINT32));

	psFixedReg->bLiveAtShaderEnd = IMG_FALSE;

	return psFixedReg;
}

IMG_INTERNAL
IMG_VOID AddDummyIteration(PINTERMEDIATE_STATE psState, IMG_UINT32 uFlags, IMG_UINT32 uDestPANum)
/*********************************************************************************
 Function			: SetupDummyIteration

 Description		: Create an iteration which writes one primary attribute.
 
 Parameters			: psState	- Compiler state.
					  uFlags	- See PIXELSHADER_INPUT_FLAG_xxx
					  uDestPANum
								- Destination primary attribute.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUNIFLEX_TEXTURE_LOAD	psTextureLoad;
	PPIXELSHADER_INPUT		psDummyInput;
	PPIXELSHADER_STATE		psPS = psState->sShader.psPS;
	PREGISTER_GROUP			psDummyGroup;

	/*
		Reserve space in the primary PDS program to hold the state for this iteration.
	*/
	ASSERT(psPS->uNumPDSConstantsAvailable >= psState->psTargetFeatures->ui32IterationStateSize);
	psPS->uNumPDSConstantsAvailable -= psState->psTargetFeatures->ui32IterationStateSize;

	psDummyInput = UscAlloc(psState, sizeof(*psDummyInput));

	psDummyInput->psFixedReg = 
		AddFixedRegForPixelShaderInput(psState, 
									   1 /* uRegCount */, 
									   UF_REGFORMAT_U8,
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   IMG_FALSE /* bVector */,
									   #endif /*defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   USC_UNDEF /* uFirstRegNum */,
									   uDestPANum);

	psDummyInput->uFlags = uFlags;
	psDummyInput->uAttributeSizeInDwords = 1;

	psTextureLoad = &psDummyInput->sLoad;
	psTextureLoad->uTexture = UNIFLEX_TEXTURE_NONE;
	psTextureLoad->uChunk = 0;
	psTextureLoad->eIterationType = UNIFLEX_ITERATION_TYPE_COLOUR;
	psTextureLoad->uCoordinate = 0;
	psTextureLoad->bCentroid = IMG_FALSE;
	psTextureLoad->bProjected = IMG_FALSE;
	psTextureLoad->eFormat = UNIFLEX_TEXLOAD_FORMAT_U8;
	psTextureLoad->uCoordinateDimension = 4;
	psTextureLoad->uNumAttributes = 1;

	psDummyGroup = AddRegisterGroup(psState, psDummyInput->psFixedReg->auVRegNum[0]);
	psDummyGroup->psFixedReg = psDummyInput->psFixedReg;
	psDummyGroup->uFixedRegOffset = 0;

	AppendToList(&psState->sShader.psPS->sPixelShaderInputs, &psDummyInput->sListEntry);
	psState->sShader.psPS->uNrPixelShaderInputs++;
}

IMG_INTERNAL
PPIXELSHADER_INPUT AddIteratedValue(PINTERMEDIATE_STATE		psState,
								    UNIFLEX_ITERATION_TYPE	eIterationType,
								    IMG_UINT32				uCoordinate,
									UNIFLEX_TEXLOAD_FORMAT	eFormat,
									IMG_UINT32				uNumAttributes,
									IMG_UINT32				uFirstTempRegNum)
/*********************************************************************************
 Function			: AddIteratedValue

 Description		: Add a new iteration to the list of pixel shader inputs.
 
 Parameters			: psState	- Compiler state.
					  eIterationType
								- Type of the iteration to add.
					  uCoordinate
								- Index of the texture coordinate or colour to
								iteration.
					  eFormat	- Format for the result of the iteration.
					  uNumAttributes
								- Number of registers used for the result of
								iteration.
					  uFirstTempRegNum
								- Start of the registers used to hold the
								result of the iteration.

 Globals Effected	: None

 Return				: A pointer to the new iteration.
*********************************************************************************/
{
	PPIXELSHADER_INPUT	psInput;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_BOOL			bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Set up the structure representing the iteration.
	*/
	psInput = UscAlloc(psState, sizeof(*psInput));

	psInput->uFlags = 0;
	psInput->sLoad.eIterationType = eIterationType;
	psInput->sLoad.uCoordinate = uCoordinate;
	psInput->sLoad.eFormat = eFormat;
	psInput->sLoad.uTexture = UNIFLEX_TEXTURE_NONE;
	psInput->sLoad.uChunk = 0;
	psInput->sLoad.bProjected = IMG_FALSE;
	psInput->sLoad.uNumAttributes = uNumAttributes;
	psInput->sLoad.uCoordinateDimension = 4;

	#if defined(OUTPUT_USPBIN)
	/*
		Store seperate pointers to the texture coordinate inputs. We may need to make
		them sources to USP non-dependent texture samples.
	*/
	if (
			eIterationType == UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE &&
			eFormat == UF_REGFORMAT_F32
	   )
	{
		ASSERT(psPS->apsF32TextureCoordinateInputs[uCoordinate] == NULL);
		psPS->apsF32TextureCoordinateInputs[uCoordinate] = psInput;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Copy the centroid flag to iteration.
	*/
	psInput->sLoad.bCentroid = IMG_FALSE;
	if (eIterationType == UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE)
	{
		if (psState->psSAOffsets->uCentroidMask & (1U << uCoordinate))
		{
			psInput->sLoad.bCentroid = IMG_TRUE;
		}
	}

	switch (eFormat)
	{
		case UNIFLEX_TEXLOAD_FORMAT_F16:
		{
			psInput->uAttributeSizeInDwords = SCALAR_REGISTERS_PER_F16_VECTOR;
			psInput->eResultFormat = UF_REGFORMAT_F16;
			break;
		}
		case UNIFLEX_TEXLOAD_FORMAT_F32:
		{
			psInput->uAttributeSizeInDwords = SCALAR_REGISTERS_PER_F32_VECTOR;
			psInput->eResultFormat = UF_REGFORMAT_F32;
			break;
		}
		case UNIFLEX_TEXLOAD_FORMAT_C10:
		{
			psInput->uAttributeSizeInDwords = SCALAR_REGISTERS_PER_C10_VECTOR;
			psInput->eResultFormat = UF_REGFORMAT_C10;
			break;
		}
		default: 
		{
			psInput->uAttributeSizeInDwords = uNumAttributes;
			psInput->eResultFormat = UF_REGFORMAT_UNTYPED;
			break;
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	bVector = IMG_FALSE;
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
			(
				eFormat == UNIFLEX_TEXLOAD_FORMAT_F16 ||
				eFormat == UNIFLEX_TEXLOAD_FORMAT_F32
			)
	   )
	{
		bVector = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Store the start of the virtual registers representing
		the iteration.
	*/
	psInput->psFixedReg = 
		AddFixedRegForPixelShaderInput(psState, 
									   uNumAttributes, 
									   psInput->eResultFormat,
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   bVector,
									   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   uFirstTempRegNum, 
									   USC_UNDEF /* uFirstHwRegNum */);

	/*
		Add to this list of pixel shader inputs.
	*/
	AppendToList(&psPS->sPixelShaderInputs, &psInput->sListEntry);

	psPS->uNrPixelShaderInputs++;

	psPS->uIterationSize += uNumAttributes;

	/*
		Reserve space in the primary PDS program to hold the state for this iteration.
	*/
	ASSERT(psPS->uNumPDSConstantsAvailable >= psState->psTargetFeatures->ui32IterationStateSize);
	psPS->uNumPDSConstantsAvailable -= psState->psTargetFeatures->ui32IterationStateSize;

	return psInput;
}

IMG_INTERNAL
IMG_UINT32 GetIteratedValue(PINTERMEDIATE_STATE			psState,
							UNIFLEX_ITERATION_TYPE		eIterationType,
							IMG_UINT32					uCoordinate,
							UNIFLEX_TEXLOAD_FORMAT		eFormat,
							IMG_UINT32					uNumAttributes)
/*********************************************************************************
 Function			: GetIteratedValue

 Description		: Create an iteration which writes one primary attribute.
 
 Parameters			: psState		- Compiler state.
					  eIterationType
									- Type of the iteration to add.
					  uCoordinate
									- Index of the texture coordinate or colour to
									iteration.
					  eFormat		- Format of the iteration result.
					  uNumAttributes
									- Number of registers used for the iteration
									result.

 Globals Effected	: None

 Return				: The number of the first temporary register containing the
					  iteration result.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psInputListEntry;
	IMG_UINT32		uFirstTempRegNum;

	/*
		Check for an existing iteration of the same data.
	*/
	ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);
	for (psInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
		 psInputListEntry != NULL;
		 psInputListEntry = psInputListEntry->psNext)
	{
		PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);

		if (psInput->sLoad.uTexture == UNIFLEX_TEXTURE_NONE &&
			psInput->sLoad.eIterationType == eIterationType &&
			psInput->sLoad.uCoordinate == uCoordinate &&
			psInput->sLoad.eFormat == eFormat)
		{
			ASSERT(psInput->sLoad.uNumAttributes == uNumAttributes);
			return psInput->psFixedReg->auVRegNum[0];
		}
	}

	/*
		Get some new registers for the iteration result.
	*/
	uFirstTempRegNum = GetNextRegisterCount(psState, uNumAttributes);

	/*
		Add a new iteration.
	*/
	AddIteratedValue(psState, eIterationType, uCoordinate, eFormat, uNumAttributes, uFirstTempRegNum);
	return uFirstTempRegNum;
}

#define PIXELSHADER_INPUT_TYPE_ITERATION	0
#define PIXELSHADER_INPUT_TYPE_SAMPLE		1
#define PIXELSHADER_INPUT_TYPE_COUNT		2

#define MAX_RESULTS_PER_PIXELSHADER_INPUT			(4)

typedef struct
{
	/* Preferred type for the next input to add to the overall list. */
	IMG_UINT32	uNextType;
	/* First destination attribute for the next input added to the overall list. */
	IMG_UINT32	uNextPAIdx;
	/* Count of inputs in the overall list. */
	IMG_UINT32	uInputCount;
	/* Consecutive register information for the current last attribute. */
	PREGISTER_GROUP psLastGroup;
} PIXELSHADER_INPUT_COPY_STATE, *PPIXELSHADER_INPUT_COPY_STATE;

static
IMG_VOID AppendPixelShaderInput(PINTERMEDIATE_STATE				psState,
								PPIXELSHADER_INPUT_COPY_STATE	psCopyState,
								PPIXELSHADER_INPUT				psInput)
/*********************************************************************************
 Function			: AppendPixelShaderInput

 Description		: Add an entry representing a pixel shader input to the end of 
					  the overall list of pixel shader inputs.
 
 Parameters			: psState		- Compiler state.
					  psCopyState	- Information about the copy.
					  psInput		- Input to add.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	PREGISTER_GROUP	psFirstDestGroup = NULL;

	for (;;)
	{
		PREGISTER_GROUP	psBaseDestGroup;
		IMG_UINT32		uDestIdx;

		/*
			Add to the end of the linked list of pixel shader inputs.
		*/
		AppendToList(&psState->sShader.psPS->sPixelShaderInputs, &psInput->sListEntry);

		/*
			Record the destination primary attribute for this pixel shader input.
		*/
		ModifyFixedRegPhysicalReg(&psState->sFixedRegList, 
								  psInput->psFixedReg, 
								  USEASM_REGTYPE_PRIMATTR,
								  psCopyState->uNextPAIdx);

		/*
			Increase the destination attribute for the next input by
			the number of attributes written by this input.
		*/
		psCopyState->uNextPAIdx += psInput->sLoad.uNumAttributes;
		psCopyState->uInputCount++;

		if ((psInput->uFlags & PIXELSHADER_INPUT_FLAG_PART_OF_ARRAY) != 0)
		{
			/*
				Record the maximum primary attribute used in an array.
			*/
			psState->sShader.psPS->uIndexedInputRangeLength = psCopyState->uNextPAIdx;
		}

		ASSERT(psInput->sLoad.uNumAttributes == psInput->psFixedReg->uConsecutiveRegsCount);
		ASSERT(psInput->psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
		psBaseDestGroup = FindRegisterGroup(psState, psInput->psFixedReg->auVRegNum[0]);
		ASSERT(psBaseDestGroup->u.psPSInput == psInput);

		/*
			Record the overall first destination for the whole group of iterations
			whose destinations require consecutive hardware register numbers.
		*/
		if (psFirstDestGroup == NULL)
		{
			psFirstDestGroup = psBaseDestGroup;
		}

		/*
			Make a link between the first destination for this iteration and the
			last destination for the previous iteration.
		*/
		if (psCopyState->psLastGroup != NULL)
		{
			IMG_BOOL		bRet;
			PREGISTER_GROUP	psLastGroup = psCopyState->psLastGroup;

			bRet = AddToGroup(psState,
							  psLastGroup->uRegister,
							  psLastGroup,
							  psBaseDestGroup->uRegister,
							  psBaseDestGroup,
							  IMG_FALSE /* bLinkedByInst */,
							  IMG_FALSE /* bOptional */);
			ASSERT(bRet);
		}

		/*
			Get the last destination for this iteration.
		*/
		for (uDestIdx = 0; uDestIdx < psInput->psFixedReg->uConsecutiveRegsCount; uDestIdx++)
		{
			ASSERT(psBaseDestGroup->uRegister == psInput->psFixedReg->auVRegNum[uDestIdx]);
			psCopyState->psLastGroup = psBaseDestGroup;
			psBaseDestGroup = psBaseDestGroup->psNext;
		}

		/*
			Check for another pixel shader input which requires consecutive hardware register
			numbers with this one.
		*/
		if (psBaseDestGroup == NULL)
		{
			break;
		}

		psInput = psBaseDestGroup->u.psPSInput;
	}

	/*
		Update the register number alignments for the results of this pixel
		shader input.
	*/
	SetGroupHardwareRegisterNumber(psState, 
								   psFirstDestGroup, 
								   IMG_FALSE /* bIgnoredAlignmentRequirement */);

	/*
		Try to interleave the inputs by making the next input
		the opposite type to this one.
	*/
	if (psInput->sLoad.uTexture == UNIFLEX_TEXTURE_NONE)
	{
		psCopyState->uNextType = PIXELSHADER_INPUT_TYPE_SAMPLE;
	}
	else
	{
		psCopyState->uNextType = PIXELSHADER_INPUT_TYPE_ITERATION;
	}
}

static
IMG_VOID AddPixelShaderInputPadding(PINTERMEDIATE_STATE				psState,
									PPIXELSHADER_INPUT_COPY_STATE	psCopyState)
/*********************************************************************************
 Function			: AddPixelShaderInputPadding

 Description		: Increase the number of attributes written by the last
					  iteration to change the alignment of the first destination
					  attribute for the next iteration.
 
 Parameters			: psState		- Compiler state.
					  psCopyState	- Information about the copy.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	PUSC_LIST_ENTRY		psLastInputListEntry;
	PPIXELSHADER_INPUT	psLastInput;
	PFIXED_REG_DATA		psFixedReg;
	PREGISTER_GROUP		psOldLastDestGroup;
	IMG_UINT32			uNewRegNum;
	PREGISTER_GROUP		psNewGroup;
	IMG_UINT32			uOldLastDestNum;
	IMG_BOOL			bRet;
	PUSEDEF				psNewInputDef;

	ASSERT((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0);

	/*
		Get the last entry added to the overall list.
	*/
	psLastInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psTail;
	ASSERT(psLastInputListEntry != NULL);
	psLastInput = IMG_CONTAINING_RECORD(psLastInputListEntry, PPIXELSHADER_INPUT, sListEntry);

	/*
		Increase the number of attributes written by the last entry
		so the first destination attribute for the next entry is aligned.

		We know the last entry wrote less than four primary attributes since
		if the last entry wrote four then it's first destination attribute must
		have been aligned and so the destination attribute for the this entry
		would have been aligned too.
	*/
	ASSERT(psLastInput->sLoad.uNumAttributes < MAX_RESULTS_PER_PIXELSHADER_INPUT);

	psFixedReg = psLastInput->psFixedReg;

	ASSERT(psFixedReg->uConsecutiveRegsCount == psLastInput->sLoad.uNumAttributes);
	ResizeTypedArray(psState, 
					 psFixedReg->auVRegNum, 
					 psLastInput->sLoad.uNumAttributes + 0, 
					 psLastInput->sLoad.uNumAttributes + 1);
	psFixedReg->asVRegUseDef = 
		UseDefResizeArray(psState,
						  psFixedReg->asVRegUseDef,
						  psLastInput->sLoad.uNumAttributes + 0,
						  psLastInput->sLoad.uNumAttributes + 1);
	ResizeTypedArray(psState, 
					 psFixedReg->aeVRegFmt, 
					 psLastInput->sLoad.uNumAttributes + 0, 
					 psLastInput->sLoad.uNumAttributes + 1);

	psFixedReg->auVRegNum[psLastInput->sLoad.uNumAttributes] = uNewRegNum = GetNextRegister(psState);
	psFixedReg->aeVRegFmt[psLastInput->sLoad.uNumAttributes] = psFixedReg->aeVRegFmt[psLastInput->sLoad.uNumAttributes-1];

	psNewInputDef = &psFixedReg->asVRegUseDef[psLastInput->sLoad.uNumAttributes];
	UseDefReset(psNewInputDef, DEF_TYPE_FIXEDREG, psLastInput->sLoad.uNumAttributes, psFixedReg);
	UseDefAddDef(psState, psFixedReg->uVRegType, uNewRegNum, psNewInputDef);

	psFixedReg->uConsecutiveRegsCount++;
	psLastInput->sLoad.uNumAttributes++;
	psCopyState->uNextPAIdx++;

	uOldLastDestNum = psFixedReg->auVRegNum[psLastInput->sLoad.uNumAttributes - 2];
	psOldLastDestGroup = FindRegisterGroup(psState, uOldLastDestNum);

	/*
		Add register grouping information for the register representing the padding primary
		attribute.
	*/
	psNewGroup = AddRegisterGroup(psState, uNewRegNum);
	psNewGroup->psFixedReg = psFixedReg;
	psNewGroup->uFixedRegOffset = psLastInput->sLoad.uNumAttributes - 1;

	/*
		Link the new register with the register representing the previous primary attribute.
	*/
	bRet = AddToGroup(psState, 
					  uOldLastDestNum, 
					  psOldLastDestGroup, 
					  uNewRegNum, 
					  psNewGroup,
					  IMG_FALSE /* bLinkedByInst */,
					  IMG_FALSE /* bOptional */); 
	ASSERT(bRet);
	
	psCopyState->psLastGroup = psNewGroup;
}

IMG_INTERNAL
PPIXELSHADER_INPUT AddOrCreateNonDependentTextureSample(PINTERMEDIATE_STATE		psState,
													   IMG_UINT32				uTexture,
													   IMG_UINT32				uChunk,
													   IMG_UINT32				uCoordinate,
													   IMG_UINT32				uTextureDimensionality,
													   IMG_BOOL					bProjected,
													   IMG_UINT32				uAttributeSizeInRegs,
													   IMG_UINT32				uAttributeSizeInDwords,
													   UF_REGFORMAT				eResultFormat,
													   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
													   IMG_BOOL					bVector,
													   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
													   UNIFLEX_TEXLOAD_FORMAT	eFormat)
/*********************************************************************************
 Function			: AddOrCreateNonDependentTextureSample

 Description		: Initialize a structure representing a set of pixel shader
					  inputs.
 
 Parameters			: psState			- Compiler state.
					  uTexture			- Index of the texture to sample.
					  uChunk			- Index of the texture plane to sample.
					  uCoordinate		- Texture coordinate to use for the sample.
					  uTextureDimensionality
										- Dimensionality of the texture.
					  bProjected		- Is the texture sample projected.
					  uAttributeSizeInRegs	
					  uAttributeSizeInDwords
										- Number of attributes used for the 
										  texture sample result.
					  eFormat			- The conversion format for the 
										  texture sample result.

 Globals Effected	: None

 Return				: Returns the PPIXELSHADER_INPUT entry that is created or 
					  found.
*********************************************************************************/
{
	PPIXELSHADER_INPUT	psInput = NULL;
	PUSC_LIST_ENTRY		psInputListEntry;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	/*
		Look for an existing texture load which matches this one.
	*/
	for (psInputListEntry = psPS->sPixelShaderInputs.psHead;
		 psInputListEntry != NULL;
		 psInputListEntry = psInputListEntry->psNext)
	{
		PUNIFLEX_TEXTURE_LOAD	psTextureLoad;

		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
		psTextureLoad = &psInput->sLoad;

		if (psTextureLoad->uTexture == uTexture &&
			psTextureLoad->uChunk == uChunk &&
			psTextureLoad->eIterationType == UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE &&
			psTextureLoad->uCoordinate == uCoordinate &&
			psTextureLoad->uCoordinateDimension == uTextureDimensionality &&
			((!psTextureLoad->bProjected && !bProjected) ||
			 (psTextureLoad->bProjected && bProjected)) && 
			 psTextureLoad->eFormat == eFormat)
		{
			ASSERT(uAttributeSizeInRegs == psTextureLoad->uNumAttributes);
			ASSERT(uAttributeSizeInDwords == psInput->uAttributeSizeInDwords);
			ASSERT(eResultFormat == psInput->eResultFormat);
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (bVector)
			{
				ASSERT((psInput->uFlags & PIXELSHADER_INPUT_FLAG_NDR_IS_VECTOR) != 0);
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			break;
		}
	}
	if (psInputListEntry == NULL)
	{
		psInput = UscAlloc(psState, sizeof(*psInput));

		psInput->uFlags = 0;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (bVector)
		{
			psInput->uFlags |= PIXELSHADER_INPUT_FLAG_NDR_IS_VECTOR;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		psInput->uAttributeSizeInDwords = uAttributeSizeInDwords;
		psInput->eResultFormat = eResultFormat;

		psInput->sLoad.uTexture = uTexture;
		psInput->sLoad.uChunk = uChunk;
		psInput->sLoad.eIterationType = UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE;
		psInput->sLoad.uCoordinate = uCoordinate;
		psInput->sLoad.uCoordinateDimension = uTextureDimensionality;
		psInput->sLoad.bProjected = bProjected;
		psInput->sLoad.bCentroid = IMG_FALSE;
		if (psState->psSAOffsets->uCentroidMask & (1U << uCoordinate))
		{
			psInput->sLoad.bCentroid = IMG_TRUE;
		}	
		psInput->sLoad.uNumAttributes = uAttributeSizeInRegs;
		psInput->sLoad.eFormat = eFormat;

		psInput->psFixedReg = 
			AddFixedRegForPixelShaderInput(psState, 
										   psInput->sLoad.uNumAttributes, 
										   eResultFormat,
										   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
										   bVector,
										   #endif /*defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
										   USC_UNDEF /* uFirstTempRegNum */, 
										   USC_UNDEF /* uFirstHwRegNum */);

		AppendToList(&psPS->sPixelShaderInputs, &psInput->sListEntry);

		psPS->uIterationSize += uAttributeSizeInRegs;
		psPS->uNrPixelShaderInputs++;

		ASSERT((psState->uCompilerFlags & UF_NONONDEPENDENTSAMPLES) == 0);

		/*
			Reduce the number of PDS constants available.
		*/
		ASSERT(psPS->uNumPDSConstantsAvailable >= psPS->uNumPDSConstantsPerTextureSample);
		psPS->uNumPDSConstantsAvailable -= psPS->uNumPDSConstantsPerTextureSample;
	}

	return psInput;
}

static
IMG_VOID CopyFixedRegData(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psDest, IMG_UINT32 uDestStart, PFIXED_REG_DATA psSrc)
/*********************************************************************************
 Function			: CopyFixedRegData

 Description		: Copy information about the intermediate regisers used by
					  the driver epilog.
 
 Parameters			: psState		- Compiler state.
					  psDest		- Destination for the copy.
					  uDestStart	- Offset within the destination to copy to.
					  psSrc			- Source for the copy.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_UINT32	uReg;

	for (uReg = 0; uReg < psSrc->uConsecutiveRegsCount; uReg++)
	{
		psDest->auVRegNum[uDestStart + uReg] = psSrc->auVRegNum[uReg];
		psDest->aeVRegFmt[uDestStart + uReg] = psSrc->aeVRegFmt[uReg];
		UseDefAddFixedRegUse(psState, psDest, uDestStart + uReg);
		if (psDest->auMask != NULL)
		{
			if (psSrc->auMask != NULL)
			{
				psDest->auMask[uDestStart + uReg] = psSrc->auMask[uReg];
			}
			else
			{
				psDest->auMask[uDestStart + uReg] = USC_ALL_CHAN_MASK;
			}
		}
		else
		{
			ASSERT(psSrc->auMask == NULL);
		}
	}
}

IMG_INTERNAL
IMG_VOID RemoveUnusedPixelShaderInputs(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: RemoveUnusedPixelShaderInputs

 Description		: Remove pixel shader inputs not read in the shader.
 
 Parameters			: psState		- Compiler state.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	PPIXELSHADER_STATE				psPS = psState->sShader.psPS;
	PUSC_LIST_ENTRY					psInputListEntry;
	PUSC_LIST_ENTRY					psNextInputListEntry;

	/*
		If both the colour result and the coverage mask result are put in a primary
		attribute chosen by the register allocator then merge together the information
		about the intermediate registers containing each.
	*/
	if (psPS->psOMaskOutput != NULL && psPS->psOMaskOutput->sPReg.uNumber == USC_UNDEF)
	{
		if (psPS->psColFixedReg == NULL)
		{
			/*
				There is no colour result so just substitute the coverage mask result
				for it.
			*/
			psPS->psColFixedReg = psPS->psOMaskOutput;
			psPS->psOMaskOutput = NULL;
		}
		else
		{
			PFIXED_REG_DATA	psOMaskOutput = psPS->psOMaskOutput;
			PFIXED_REG_DATA	psOldColFixedReg = psPS->psColFixedReg;
			PFIXED_REG_DATA	psNewColFixedReg;

			/*
				Both results should have compatible options.
			*/
			ASSERT(psOldColFixedReg->sPReg.uType == psOMaskOutput->sPReg.uType);
			ASSERT(psOldColFixedReg->sPReg.uNumber == psOMaskOutput->sPReg.uNumber);
			ASSERT(psOldColFixedReg->bPrimary == psOMaskOutput->bPrimary);
			ASSERT(psOldColFixedReg->bLiveAtShaderEnd == psOMaskOutput->bLiveAtShaderEnd);
			ASSERT(psOldColFixedReg->uVRegType == psOMaskOutput->uVRegType);

			/*
				Create a new shader result structure for the merger.
			*/
			psNewColFixedReg = 
				AddFixedReg(psState, 
						    psOldColFixedReg->bPrimary, 
							psOldColFixedReg->bLiveAtShaderEnd,
							psOldColFixedReg->sPReg.uType,
							psOldColFixedReg->sPReg.uNumber,
							psOldColFixedReg->uConsecutiveRegsCount + psOMaskOutput->uConsecutiveRegsCount);

			psNewColFixedReg->uVRegType = psOldColFixedReg->uVRegType;

			if (psOldColFixedReg->auMask != NULL || psOMaskOutput->auMask != NULL)
			{
				psNewColFixedReg->auMask = 
					UscAlloc(psState, sizeof(psNewColFixedReg->auMask[0]) * psNewColFixedReg->uConsecutiveRegsCount);
			}

			/*
				Copy information from the colour result to the new result.
			*/
			CopyFixedRegData(psState, psNewColFixedReg, 0 /* uDestStart */, psOldColFixedReg);

			/*
				Copy information about the coverage result to the new result.
			*/
			CopyFixedRegData(psState, psNewColFixedReg, psOldColFixedReg->uConsecutiveRegsCount, psOMaskOutput);

			/*
				Free the old colour result.
			*/
			ReleaseFixedReg(psState, psOMaskOutput);
			psPS->psOMaskOutput = psOMaskOutput = NULL;

			/*
				Free the old coverage result.
			*/
			ASSERT(psState->sShader.psPS->psFixedHwPARegReg == psOldColFixedReg);
			psState->sShader.psPS->psFixedHwPARegReg = psNewColFixedReg;
			ReleaseFixedReg(psState, psOldColFixedReg);
			psOldColFixedReg = NULL;
			psPS->psColFixedReg = psNewColFixedReg;
		}
	}

	#if defined(OUTPUT_USPBIN)
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		SAFE_LIST_ITERATOR	sIter;
		IMG_BOOL			bAddedExtraSources = IMG_FALSE;

		InstListIteratorInitialize(psState, ISMP_USP_NDR, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PSMP_USP_PARAMS		psUSPSampleData;
			PINST				psUSPSample;
			IMG_UINT32			uTCIdx;
			IMG_BOOL			bTexCoordRead;
			PPIXELSHADER_INPUT	psInput;
			IMG_UINT32			uArg;

			psUSPSample = InstListIteratorCurrent(&sIter);
			ASSERT(psUSPSample->eOpcode == ISMP_USP_NDR);
			psUSPSampleData = &psUSPSample->u.psSmp->sUSPSample;

			/*
				Get the texture coordinate used for this texture sample.
			*/
			uTCIdx = psUSPSampleData->uNDRTexCoord;

			/*
				Get the pixel shader input that is the result of
				iterating the same texture coordinate.
			*/
			psInput = psPS->apsF32TextureCoordinateInputs[uTCIdx];
			if (psInput != NULL)
			{
				ASSERT(psInput->sLoad.uTexture == UNIFLEX_TEXTURE_NONE);
				ASSERT(psInput->sLoad.eIterationType == UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE);
				ASSERT(psInput->sLoad.uCoordinate == uTCIdx);
				ASSERT(psInput->sLoad.eFormat == UNIFLEX_TEXLOAD_FORMAT_F32);

				/*
					Check if some of the channels in the texture coordinate used by the
					texture sample are also used from the iteration result by
					other instructions in the shader.
				*/
				bTexCoordRead = IMG_FALSE;
				for (uArg = 0; uArg < psUSPSample->u.psSmp->uCoordSize; uArg++)
				{	
					IMG_UINT32	uChansUsedFromIteration;
	
					uChansUsedFromIteration = 
						GetRegMask(psInput->psFixedReg->puUsedChans, uArg);
	
					if (GetLiveChansInArg(psState, psUSPSample, NON_DEP_SMP_COORD_ARG_START + uArg) & uChansUsedFromIteration)
					{
						bTexCoordRead = IMG_TRUE;
						break;
					}
				}

				/* 
					The Non Dependent Sample instructions can use iterated texture 
					coordinates in USP if some of their chunks are read using 
					dependent sample instructions. So we need a guarantee that 
					the pa register allocated to texture coordinates will not get 
					re-used for some other purpose before this sample instruction. 
				*/
				if (bTexCoordRead)
				{
					for (uArg = 0; uArg < psUSPSample->u.psSmp->uCoordSize; uArg++)
					{
						/*
							Set the iteration result as an argument to the texture sample.
						*/
						SetSrc(psState, 
							   psUSPSample, 
							   NON_DEP_SMP_COORD_ARG_START + uArg, 
							   USEASM_REGTYPE_TEMP, 
							   psInput->psFixedReg->auVRegNum[uArg], 
							   UF_REGFORMAT_F32);
					}
	
					bAddedExtraSources = IMG_TRUE;
				}
			}
		}
		InstListIteratorFinalise(&sIter);

		/*
			Iteration results were added as extra arguments to USP texture samples so 
			update the liveness of the iteration results.
		*/
		if (bAddedExtraSources)
		{
			DeadCodeElimination(psState, IMG_TRUE /* bFreeBlocks */);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */

	for (psInputListEntry = psPS->sPixelShaderInputs.psHead; 
		 psInputListEntry != NULL; 
		 psInputListEntry = psNextInputListEntry)
	{
		PPIXELSHADER_INPUT		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
		PUNIFLEX_TEXTURE_LOAD	psTextureLoad = &psInput->sLoad;
		IMG_BOOL				bUsedInput = IMG_FALSE;
		IMG_UINT32				uRegIdx;

		psNextInputListEntry = psInputListEntry->psNext;

		if ((psInput->uFlags & PIXELSHADER_INPUT_FLAG_PART_OF_ARRAY) != 0)
		{
			/*
				Don't remove iterations whose destination is part of a dynamically
				addressed range.
			*/
			continue;
		}

		/*
			Recalculate the masks of channels used from pixel shader input in the program.
		*/
		for (uRegIdx = 0; uRegIdx < psTextureLoad->uNumAttributes; uRegIdx++)
		{
			SetRegMask(psInput->psFixedReg->puUsedChans, 
					   uRegIdx,
					   UseDefGetUsedChanMask(psState, psInput->psFixedReg->asVRegUseDef[uRegIdx].psUseDefChain));
		}
	
		/*
			For non-dependent texture samples, and iterated vertex-colours,
			we only remove them if none of the associated PA registers are
			used.

			For iterated texture coordiate sets, we can removed unused
			'excess' coordinates.
		*/
		if	(
				(psTextureLoad->uTexture != UNIFLEX_TEXTURE_NONE) ||
				(psTextureLoad->eIterationType == UNIFLEX_ITERATION_TYPE_COLOUR)
			)
		{
			for (uRegIdx = 0; uRegIdx < psTextureLoad->uNumAttributes; uRegIdx++)
			{
				if (GetRegMask(psInput->psFixedReg->puUsedChans, uRegIdx) != 0)
				{
					bUsedInput = IMG_TRUE;
					break;
				}
			}
		}
		else
		{
			/*
				For a texture coordinate iteration we can drop unused
				'excess' components, if the registers they are in
				are not used.
			*/
			for (;;)
			{
				if (IsLastCoordUsed(psState, psInput->psFixedReg->puUsedChans, psInput))
				{
					break;
				}

				psTextureLoad->uCoordinateDimension--;
				if (psTextureLoad->uCoordinateDimension == 0)
				{
					break;
				}
			}

			if	(psTextureLoad->uCoordinateDimension == 0)
			{
				/*
					Remove the iterated coordinates, since none of the
					associated registers are used.
				*/
				bUsedInput = IMG_FALSE;
			}
			else
			{
				PFIXED_REG_DATA	psFixedReg;
				IMG_UINT32		uRegIdx;

				bUsedInput = IMG_TRUE;

				/*
					Record which registers are used for the iterated
					coordinates now, accounting for a reduction in the
					number of dimensions.
				*/
				switch (psTextureLoad->eFormat)
				{
					case UNIFLEX_TEXLOAD_FORMAT_F16:
					{
						/* 2 components per register */
						psTextureLoad->uNumAttributes = (psTextureLoad->uCoordinateDimension + 1) >> 1;
						break;
					}
					case UNIFLEX_TEXLOAD_FORMAT_C10:
					{
						/* 3 components per register */
						psTextureLoad->uNumAttributes = (psTextureLoad->uCoordinateDimension + 2) / 3;
						break;
					}
					case UNIFLEX_TEXLOAD_FORMAT_F32:
					{
						/* 1 component per register */
						psTextureLoad->uNumAttributes = psTextureLoad->uCoordinateDimension;
						break;
					}
					case UNIFLEX_TEXLOAD_FORMAT_U8:
					{
						/* 4 components per register. */
						psTextureLoad->uNumAttributes = 1;
						break;
					}
					default: imgabort();
				}

				/*
					Update the corresponding fixed register for the reduced size of the
					iteration result.
				*/
				psFixedReg = psInput->psFixedReg;
				for (uRegIdx = psTextureLoad->uNumAttributes; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
				{
					UseDefDropFixedRegDef(psState, psFixedReg, uRegIdx);
				}
				ResizeArray(psState,
							psFixedReg->auVRegNum,
							psFixedReg->uConsecutiveRegsCount * sizeof(psFixedReg->auVRegNum[0]),
							psTextureLoad->uNumAttributes * sizeof(psFixedReg->auVRegNum[0]),
							(IMG_PVOID*)&psFixedReg->auVRegNum);
				psFixedReg->uConsecutiveRegsCount = psTextureLoad->uNumAttributes;
			}
		}
		
		if (!bUsedInput)
		{
			ReleaseFixedReg(psState, psInput->psFixedReg);

			psPS->uNrPixelShaderInputs--;
			RemoveFromList(&psPS->sPixelShaderInputs, &psInput->sListEntry);
			UscFree(psState, psInput);
		}
	}
}

IMG_INTERNAL
IMG_VOID AllocatePixelShaderIterationRegisters(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: AllocatePixelShaderIterationRegisters

 Description		: Assign hardware register numbers for the results of
					  pixel shader iterations/non-dependent texture samples.
 
 Parameters			: psState		- Compiler state.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_UINT32						uPACount;
	IMG_UINT32						uDummyIterationCount;
	IMG_UINT32						uDummyIterIdx;
	PPIXELSHADER_STATE				psPS = psState->sShader.psPS;
	IMG_BOOL						bShaderOutputsToFixedPAs;
	PUSC_LIST_ENTRY					psInputListEntry;
	SIZE_AND_ALIGN_LIST				asIterationList[PIXELSHADER_INPUT_TYPE_COUNT];
	IMG_UINT32						uColourOutputCount;

	/*
		Initialize the sets of pixel shader inputs.
	*/
	InitSizeAndAlignList(&asIterationList[PIXELSHADER_INPUT_TYPE_SAMPLE]);
	InitSizeAndAlignList(&asIterationList[PIXELSHADER_INPUT_TYPE_ITERATION]);

	/*
		Sort the pixel shader inputs into different lists depending on the type and
		the number of attributes written.
	*/
	while ((psInputListEntry = RemoveListHead(&psPS->sPixelShaderInputs)) != NULL)
	{
		IMG_UINT32				uInputType;
		PPIXELSHADER_INPUT		psInput;
		PUNIFLEX_TEXTURE_LOAD	psTextureLoad;
		PREGISTER_GROUP			psFirstDestGroup;
		HWREG_ALIGNMENT			eFirstDestAlignment;
		IMG_UINT32				uGroupLength;
		PREGISTER_GROUP			psDestGroup;
		IMG_UINT32				uLengthClass;

		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
		psTextureLoad = &psInput->sLoad;

		if (psTextureLoad->uTexture != UNIFLEX_TEXTURE_NONE)
		{
			uInputType = PIXELSHADER_INPUT_TYPE_SAMPLE;
		}
		else
		{
			uInputType = PIXELSHADER_INPUT_TYPE_ITERATION;
		}

		/*	
			Get information 
		*/
		ASSERT(psInput->psFixedReg->uConsecutiveRegsCount > 0);
		ASSERT(psInput->psFixedReg->uVRegType == USEASM_REGTYPE_TEMP);
		psFirstDestGroup = FindRegisterGroup(psState, psInput->psFixedReg->auVRegNum[0]);
		eFirstDestAlignment = GetNodeAlignment(psFirstDestGroup);

		/*
			Associate the register group with the pixel shader input.
		*/
		psFirstDestGroup->u.psPSInput = psInput;

		/*
			Skip iterations whose results have some other variables which require lower hardware
			register numbers.
		*/
		if (psFirstDestGroup->psPrev != NULL)
		{
			continue;
		}

		/*
			Count the number of iteration results requiring consecutive register numbers.
		*/
		uGroupLength = 0;
		for (psDestGroup = psFirstDestGroup; psDestGroup != NULL; psDestGroup = psDestGroup->psNext)
		{
			uGroupLength++;
		}
		uLengthClass = uGroupLength % 2;

		/*
			Add to the list of iterations with a matching starting alignment and length.
		*/
		AppendToList(&asIterationList[uInputType].aasLists[eFirstDestAlignment][uLengthClass], &psInput->sListEntry);
	}

	/*
		Interleave texture loads and iterations so the driver can do issues to both the
		TAG and the USE in one DOUTI command.
	*/
	if (psPS->uNrPixelShaderInputs > 0)
	{
		PIXELSHADER_INPUT_COPY_STATE	sCopyState;
		PSIZE_AND_ALIGN_LIST			apsIterationFirst[PIXELSHADER_INPUT_TYPE_COUNT];
		PSIZE_AND_ALIGN_LIST			apsSampleFirst[PIXELSHADER_INPUT_TYPE_COUNT];

		/*
			Set up lists for each possible priority order.
		*/
		apsIterationFirst[0] = &asIterationList[PIXELSHADER_INPUT_TYPE_ITERATION];
		apsIterationFirst[1] = &asIterationList[PIXELSHADER_INPUT_TYPE_SAMPLE];

		apsSampleFirst[0] = &asIterationList[PIXELSHADER_INPUT_TYPE_SAMPLE];
		apsSampleFirst[1] = &asIterationList[PIXELSHADER_INPUT_TYPE_ITERATION];

		sCopyState.uNextPAIdx = 0;
		sCopyState.uNextType = PIXELSHADER_INPUT_TYPE_ITERATION;
		sCopyState.uInputCount = 0;
		sCopyState.psLastGroup = NULL;

		psPS->uIndexedInputRangeLength = 0;

		while (sCopyState.uInputCount < psPS->uNrPixelShaderInputs)
		{
			PUSC_LIST_ENTRY			psNextInputListEntry;
			PSIZE_AND_ALIGN_LIST*	apsPriorityOrder;
			IMG_BOOL				bAddPadding;
			PPIXELSHADER_INPUT		psNextInput;

			/*
				Try to make the type of the next input the opposite of the
				last type.
			*/
			if (sCopyState.uNextType == PIXELSHADER_INPUT_TYPE_ITERATION)
			{
				apsPriorityOrder = apsIterationFirst;
			}
			else
			{
				ASSERT(sCopyState.uNextType == PIXELSHADER_INPUT_TYPE_SAMPLE);
				apsPriorityOrder = apsSampleFirst;
			}

			/*
				Get the input to assign the next hardware register to.
			*/
			psNextInputListEntry = 
				AssignInputOutputRegisters(psState, 
										   sCopyState.uNextPAIdx, 
										   IMG_TRUE /* bPaddingAvailable */,
										   PIXELSHADER_INPUT_TYPE_COUNT,
										   apsPriorityOrder,
										   &bAddPadding);

			/*
				Increase the number of attributes written by the previous iteration
				so the destination for this iteration starts at the required alignment.
			*/
			if (bAddPadding)
			{
				AddPixelShaderInputPadding(psState, &sCopyState);
			}

			/*
				Set the hardware registers for the destinations of this iteration.
			*/
			ASSERT(psNextInputListEntry != NULL);
			psNextInput = IMG_CONTAINING_RECORD(psNextInputListEntry, PPIXELSHADER_INPUT, sListEntry);
			AppendPixelShaderInput(psState, &sCopyState, psNextInput);
		}

		uPACount = sCopyState.uNextPAIdx;
	}
	else
	{
		uPACount = 0;
	}

	/*
		Ensure the primary attributes used for output don't overwrite primary attributes used for the driver's iterations.
		We'll do this by adding enough dummy iterations so the destination for the first driver iteration is larger than
		the number of primary attributes needed to hold the shader results.
	*/
	uDummyIterationCount = 0;
	bShaderOutputsToFixedPAs = IMG_FALSE;
	if ((psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT) == 0)
	{
		uColourOutputCount = psPS->uColOutputCount;
	}
	else
	{
		uColourOutputCount = psPS->uColOutputCount + 1;
	}
	#if defined(OUTPUT_USCHW)
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		if (
				psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_PRIMATTR &&
				(
					psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8 || 
					(psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT)
				)
			)
		{
			if (psState->psSAOffsets->uExtraPARegisters > 0 && uColourOutputCount > uPACount)
			{
				uDummyIterationCount = uColourOutputCount - uPACount;
			}

			bShaderOutputsToFixedPAs = IMG_TRUE;
		}
	}
	#endif /* defined(OUTPUT_USCHW) */

	/*
		Set up extra iterations so the first primary attribute written by the driver's iteration
		is greater than the last register used for the pixel shader result.

		NB: These cannot be removed as unnecessary trailing iterations by the USP
	*/
	for (uDummyIterIdx = 0; uDummyIterIdx < uDummyIterationCount; uDummyIterIdx++)
	{
		AddDummyIteration(psState, 0 /* uFlags */, uPACount);
		uPACount++;
	}

	/*
		Increase the primary attribute count to include registers used for the
		shader result.
	*/
	if (bShaderOutputsToFixedPAs)
	{
		uPACount = max(uPACount, uColourOutputCount);
	}

	/*
		Update the physical register location of the registers
		representing extra iterations inserted by the caller.
	*/
	if (psState->psSAOffsets->uExtraPARegisters > 0)
	{
		ModifyFixedRegPhysicalReg(&psState->sFixedRegList, 
								  psPS->psExtraPAFixedReg,
								  USEASM_REGTYPE_PRIMATTR,
								  uPACount /* uPhysicalRegNum */);
		uPACount += psState->psSAOffsets->uExtraPARegisters;
	}

	/*
		Set the hardware register alignment of the pixel shader input results.
	*/
	if (psPS->uNrPixelShaderInputs > 0)
	{
		PPIXELSHADER_INPUT	psFirstInput;
		PREGISTER_GROUP		psFirstInputGroup;

		psFirstInput = IMG_CONTAINING_RECORD(psPS->sPixelShaderInputs.psHead, PPIXELSHADER_INPUT, sListEntry);
		psFirstInputGroup = FindRegisterGroup(psState, psFirstInput->psFixedReg->auVRegNum[0]);
		ASSERT(psFirstInputGroup != NULL);
		SetGroupHardwareRegisterNumber(psState, psFirstInputGroup, IMG_FALSE /* bIgnoredAlignmentRequirement */);
	}


	/*
		Store the number of primary attributes used.
	*/
	psState->sHWRegs.uNumPrimaryAttributes = uPACount;
}

static IMG_VOID ExpandProjectedSMP_F32(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psBlock,
									   PINST				psInsertBeforeInst,
									   IMG_UINT32			u1OverWTemp,
									   IMG_UINT32			u1OverWChan,
									   IMG_UINT32			uCoordSize,
									   IMG_UINT32			uCoordMask,
									   IMG_UINT32			uOutCoordTempStart,
									   PINST				psCoordSrcInst,
									   IMG_UINT32			uCoordSrcIdx,
									   IMG_UINT32			uInCoordTempStart)
/*********************************************************************************
 Function			: ExpandProjectedSMP_F32

 Description		: Project a set of texture coordinates in F32 format.
 
 Parameters			: psState		- Compiler state.
					  psBlock		- Block to insert the new instructions into.
					  psInsertBeforeInst
									- Point to insert the new instructions.
					  u1OverWTemp	- Number of the temporary containing 1/W in
									F32 format.
					  u1OverWChan	- Channel in the temporary containing 1/W.
					  uCoordSize	- Number of coordinate channels to project.
					  uCoordMask	- Mask of channels used from the last register
									of coordinate data.
					  uOutCoordTempStart
									- Start of the temporaries to write with
									the projected coordinates.
					  psCoordSrcInst - If non-NULL the start of the coordinate
									 channels to project.
					  uCoordSrcIdx
					  uInCoordTempStart
									- If psCoordSrcInst is NULL then the start of the
									temporaries containing the coordinate channels
									to project.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_UINT32	uCoordIdx;

	ASSERT(u1OverWChan == 0);
	ASSERT(uCoordMask == USC_ALL_CHAN_MASK);

	for (uCoordIdx = 0; uCoordIdx < uCoordSize; uCoordIdx++)
	{
		PINST	psMULInst;

		/*
			MUL		OUTCOORDi, INCOORDi, (1/W)
		*/
		psMULInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psMULInst, IFMUL);
		SetDest(psState, psMULInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uOutCoordTempStart + uCoordIdx, UF_REGFORMAT_F32);
		if (psCoordSrcInst != NULL)
		{
			MoveSrc(psState, psMULInst, 0 /* uArgIdx */, psCoordSrcInst, uCoordSrcIdx + uCoordIdx);
		}
		else
		{
			SetSrc(psState, psMULInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uInCoordTempStart + uCoordIdx, UF_REGFORMAT_F32);
		}
		SetSrc(psState, psMULInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psBlock, psMULInst, psInsertBeforeInst);
	}
}

static IMG_VOID ExpandProjectedSMP_F16(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psBlock,
									   PINST				psInsertBeforeInst,
									   IMG_UINT32			u1OverWTemp,
									   IMG_UINT32			u1OverWChan,
									   IMG_UINT32			uCoordSize,
									   IMG_UINT32			uCoordMask,
									   IMG_UINT32			uOutCoordTempStart,
									   PINST				psCoordSrcInst,
									   IMG_UINT32			uCoordSrcIdx,
									   IMG_UINT32			uInCoordTempStart)
/*********************************************************************************
 Function			: ExpandProjectedSMP_F16

 Description		: Project a set of texture coordinates in F16 format.
 
 Parameters			: psState		- Compiler state.
					  psBlock		- Block to insert the new instructions into.
					  psInsertBeforeInst
									- Point to insert the new instructions.
					  u1OverWTemp	- Number of the temporary containing 1/W in
									F16 format.
					  u1OverWChan	- Channel in the temporary containing 1/W.
					  uCoordSize	- Size of the coordinate data to project in
									32-bit registers.
					  uCoordMask	- Mask of channels used from the last register
									of coordinate data.
					  uOutCoordTempStart
									- Start of the temporaries to write with
									the projected coordinates.
					  psCoordSrcInst - If non-NULL the start of the coordinate
									 channels to project.
					  uCoordSrcIdx
					  uInCoordTempStart
									- If psCoordSrcInst is NULL then the start of the
									temporaries containing the coordinate channels
									to project.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_UINT32	uCoordIdx;
	PINST		psPCKInst;
	IMG_UINT32	u1OverWSwizTemp = GetNextRegister(psState);

	/*
		Replicate the 1/W value to both halves of a 32-bit
		register.
	*/
	psPCKInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psPCKInst, IPCKF16F16);

	SetDest(psState, psPCKInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, u1OverWSwizTemp, UF_REGFORMAT_F16);

	SetSrc(psState, psPCKInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, UF_REGFORMAT_F16);
	SetPCKComponent(psState, psPCKInst, 0 /* uArgIdx */, u1OverWChan);
	SetSrc(psState, psPCKInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, UF_REGFORMAT_F16);
	SetPCKComponent(psState, psPCKInst, 1 /* uArgIdx */, u1OverWChan);
	
	InsertInstBefore(psState, psBlock, psPCKInst, psInsertBeforeInst);

	for (uCoordIdx = 0; uCoordIdx < uCoordSize; uCoordIdx++)
	{
		PINST	psMUL16Inst;

		/*
			FMUL16	OUTCOORD, INCOORD, (1/W)
		*/
		psMUL16Inst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psMUL16Inst, IFMUL16);

		if (uCoordIdx == (uCoordSize - 1))
		{
			psMUL16Inst->auLiveChansInDest[0] = uCoordMask;
		}

		SetDest(psState, psMUL16Inst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uOutCoordTempStart + uCoordIdx, UF_REGFORMAT_F16);

		if (psCoordSrcInst != NULL)
		{
			MoveSrc(psState, psMUL16Inst, 0 /* uSrcIdx */, psCoordSrcInst, uCoordSrcIdx + uCoordIdx);
		}
		else
		{
			SetSrc(psState, psMUL16Inst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uInCoordTempStart + uCoordIdx, UF_REGFORMAT_F16);
		}

		SetSrc(psState, psMUL16Inst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWSwizTemp, UF_REGFORMAT_F16);

		InsertInstBefore(psState, psBlock, psMUL16Inst, psInsertBeforeInst);
	}
}


static IMG_VOID ExpandProjectedSMP_C10(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psBlock,
									   PINST				psInsertBeforeInst,
									   IMG_UINT32			u1OverWTemp,
									   IMG_UINT32			u1OverWChan,
									   IMG_UINT32			uCoordSize,
									   IMG_UINT32			uCoordMask,
									   IMG_UINT32			uOutCoordTempStart,
									   PINST				psCoordSrcInst,
									   IMG_UINT32			uCoordSrcIdx,
									   IMG_UINT32			uInCoordTempStart)
/*********************************************************************************
 Function			: ExpandProjectedSMP_C10

 Description		: Project a set of texture coordinates in C10 format.
 
 Parameters			: psState		- Compiler state.
					  psBlock		- Block to insert the new instructions into.
					  psInsertBeforeInst
									- Point to insert the new instructions.
					  u1OverWTemp	- Number of the temporary containing 1/W in
									C10 format.
					  u1OverWChan	- Channel in the temporary containing 1/W.
					  uCoordSize	- Size of the coordinate data to project in
									32-bit registers.
					  uCoordMask	- Mask of channels used from the last register
									of coordinate data.
					  uOutCoordTempStart
									- Start of the temporaries to write with
									the projected coordinates.
					  psCoordSrcInst - If non-NULL the start of the coordinate
									 channels to project.
					  uCoordSrcIdx
					  uInCoordTempStart
									- If psCoordSrcInst is NULL then the start of the
									temporaries containing the coordinate channels
									to project.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	PINST		psPCKInst;
	PINST		psSOPWMInst;
	IMG_UINT32	u1OverWSwizTemp = GetNextRegister(psState);

	/*
		Replicate 1/W to all channels of a register.
	*/
	psPCKInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psPCKInst, IPCKC10C10);

	SetDest(psState, psPCKInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, u1OverWSwizTemp, UF_REGFORMAT_C10);

	SetSrc(psState, psPCKInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, UF_REGFORMAT_C10);
	SetPCKComponent(psState, psPCKInst, 0 /* uArgIdx */, u1OverWChan);
	SetSrc(psState, psPCKInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, UF_REGFORMAT_C10);
	SetPCKComponent(psState, psPCKInst, 1 /* uArgIdx */, u1OverWChan);
	
	InsertInstBefore(psState, psBlock, psPCKInst, psInsertBeforeInst);

	/*
		Multiply the coordinates by 1/W.
	*/
	ASSERT(uCoordSize == 1);

	psSOPWMInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psSOPWMInst, ISOPWM);

	/* New coordinates. */
	psSOPWMInst->auDestMask[0] = psSOPWMInst->auLiveChansInDest[0] = uCoordMask;
	SetDest(psState, psSOPWMInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uOutCoordTempStart, UF_REGFORMAT_C10);

	/* Old coordinates. */
	if (psCoordSrcInst != NULL)
	{
		MoveSrc(psState, psSOPWMInst, 0 /* uCopyToIdx */, psCoordSrcInst, uCoordSrcIdx);
	}
	else
	{
		SetSrc(psState, psSOPWMInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uInCoordTempStart, UF_REGFORMAT_C10);
	}

	/* 1/W */
	SetSrc(psState, psSOPWMInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, u1OverWSwizTemp, UF_REGFORMAT_C10);

	/*
		DEST = SRC1 * SRC2 + SRC2 * ZERO
	*/
	psSOPWMInst->u.psSopWm->uCop = USEASM_INTSRCSEL_ADD;
	psSOPWMInst->u.psSopWm->uAop = USEASM_INTSRCSEL_ADD;
	psSOPWMInst->u.psSopWm->uSel1 = USEASM_INTSRCSEL_SRC2;
	psSOPWMInst->u.psSopWm->uSel2 = USEASM_INTSRCSEL_ZERO;
	psSOPWMInst->u.psSopWm->bComplementSel1 = IMG_FALSE;
	psSOPWMInst->u.psSopWm->bComplementSel2 = IMG_FALSE;

	InsertInstBefore(psState, psBlock, psSOPWMInst, psInsertBeforeInst);
}

IMG_INTERNAL
IMG_UINT32 ProjectCoordinates(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psBlock,
							  PINST					psInsertBeforeInst,
							  PARG					psProj,
							  PINST					psProjSrcInst,
							  IMG_UINT32			uProjSrcIdx,
							  IMG_UINT32			uProjChannel,
							  IMG_UINT32			uCoordSize,
							  IMG_UINT32			uCoordMask,
							  UF_REGFORMAT			eCoordFormat,
							  PINST					psCoordSrcInst,
							  IMG_UINT32			uCoordSrcIdx,
							  IMG_UINT32			uInCoordTempStart)
/*********************************************************************************
 Function			: ProjectCoordinates

 Description		: Project a set of texture coordinates.
 
 Parameters			: psState		- Compiler state.
					  psBlock		- Block to insert the new instructions into.
					  psInsertBeforeInst
									- Point to insert the new instructions.
					  psProj		- Register containing the value to project by.
					  psProjSrcInst	
					  uProjSrcIdx
					  uProjChannel	- Channel select for the register containing
									the value to project by.
					  uCoordSize	- Size of the coordinate data to project in
									registers.
					  uCoordMask	- Mask of channels used from the last register
									of coordinate data.
					  eCoordFormat	- Format of the coordinate data.
					  uOutCoordTempStart
					  psCoordSrcInst - If non-NULL the start of the coordinate
									 channels to project.
					  uCoordSrcIdx
					  uInCoordTempStart
									- If psCoordSrcInst is NULL then the start of the
									temporaries containing the coordinate channels
									to project.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_VOID		(*pfExpand)(PINTERMEDIATE_STATE, PCODEBLOCK, PINST, IMG_UINT32, IMG_UINT32, IMG_UINT32, IMG_UINT32, IMG_UINT32, PINST, IMG_UINT32, IMG_UINT32);
	IMG_UINT32		u1OverWTemp;
	PINST			psRCPInst;
	IMG_UINT32		uCoordTempStart;
	UF_REGFORMAT	eProjFmt;

	if (psProj != NULL)
	{
		eProjFmt = psProj->eFmt;
	}
	else
	{
		eProjFmt = psProjSrcInst->asArg[uProjSrcIdx].eFmt;
	}

	/*
		Allocate registers for the projected coordinates.
	*/
	uCoordTempStart = GetNextRegisterCount(psState, uCoordSize);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
			(
				eCoordFormat == UF_REGFORMAT_F16 ||
				eCoordFormat == UF_REGFORMAT_F32
			)
	   )
	{
		ApplyProjectionVec(psState,
						   psBlock,
						   psInsertBeforeInst,
						   uCoordTempStart,
						   psProj,
						   psProjSrcInst,
						   uProjSrcIdx,
						   USC_UNDEF /* uProjTemp */,
						   uProjChannel,
						   eCoordFormat,
						   psCoordSrcInst,
						   uCoordSrcIdx,
						   uInCoordTempStart,
						   uCoordMask);
		return uCoordTempStart;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Generate the recipocal of the value to project by.
	*/
	u1OverWTemp = GetNextRegister(psState);

	psRCPInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psRCPInst, IFRCP);
	SetDest(psState, psRCPInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, u1OverWTemp, eProjFmt);
	if (psProj != NULL)
	{
		psRCPInst->asArg[0] = *psProj;
	}
	else
	{
		MoveSrc(psState, psRCPInst, 0 /* uCopyToIdx */, psProjSrcInst, uProjSrcIdx); 
	}
	SetComponentSelect(psState, psRCPInst, 0 /* uArgIdx */, uProjChannel);
	if (eCoordFormat != UF_REGFORMAT_F32)
	{
		SetBit(psRCPInst->auFlag, INST_TYPE_PRESERVE, 1);
	}
	InsertInstBefore(psState, psBlock, psRCPInst, psInsertBeforeInst);

	pfExpand = ExpandProjectedSMP_F32;

	/*
		Generate instructions to multiply the coordinates by 1/W.
	*/
	switch (eCoordFormat)
	{
		case UF_REGFORMAT_F32:
		{
			pfExpand = ExpandProjectedSMP_F32;
			break;
		}
		case UF_REGFORMAT_F16:
		{
			pfExpand = ExpandProjectedSMP_F16;
			break;
		}
		case UF_REGFORMAT_C10:
		{
			pfExpand = ExpandProjectedSMP_C10;
			break;
		}
		default: imgabort();
	}
	pfExpand(psState,
			 psBlock,
			 psInsertBeforeInst,
			 u1OverWTemp,
			 uProjChannel,
			 uCoordSize,
			 uCoordMask,
			 uCoordTempStart,
			 psCoordSrcInst,
			 uCoordSrcIdx,
			 uInCoordTempStart);

	return uCoordTempStart;
}

static IMG_VOID ExpandProjectedSMP(PINTERMEDIATE_STATE	psState,
								   PINST				psSmpInst)
/*********************************************************************************
 Function			: ExpandProjectedSMP

 Description		: Update an SMP instruction to apply projection to the source
					  texture coordinates in separate instructions.
 
 Parameters			: psState		- Compiler state.
					  psSmpInst		- SMP instruction to modify.

 Globals Effected	: None

 Return				: Nothing
*********************************************************************************/
{
	IMG_UINT32		uCoordTempStart;
	UF_REGFORMAT	eCoordFormat = psSmpInst->asArg[SMP_COORD_ARG_START].eFmt;
	IMG_UINT32		uCoordIdx;

	/*
		Generate the projected coordinates.
	*/
	uCoordTempStart = 
		ProjectCoordinates(psState,
						   psSmpInst->psBlock,
						   psSmpInst,
						   NULL /* psProj */,
						   psSmpInst /* psProjSrcInst */,
						   SMP_PROJ_ARG /* uProjSrcIdx */,
						   psSmpInst->u.psSmp->uProjChannel,
						   psSmpInst->u.psSmp->uCoordSize,
						   psSmpInst->u.psSmp->uUsedCoordChanMask,
						   eCoordFormat,
						   psSmpInst,
						   SMP_COORD_ARG_START,
						   USC_UNDEF);
	
	/*
		Replace the current SMP coordinate source by the projected values.
	*/
	for (uCoordIdx = 0; uCoordIdx < psSmpInst->u.psSmp->uCoordSize; uCoordIdx++)
	{
		SetSrc(psState, 
			   psSmpInst, 
			   SMP_COORD_ARG_START + uCoordIdx, 
			   USEASM_REGTYPE_TEMP, 
			   uCoordTempStart + uCoordIdx, 
			   eCoordFormat);
	}

	/*
		Clear the projection source to the SMP instruction.
	*/
	psSmpInst->u.psSmp->bProjected = IMG_FALSE;
	SetSrcUnused(psState, psSmpInst, SMP_PROJ_ARG);
}

static IMG_BOOL CanAddNewNonDependentTextureSamples(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: CanAddNewNonDependentTextureSamples
    
 PURPOSE	: Check if there is space to add more non-dependent texture
			  samples.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		if	((psState->sShader.psPS->uNrPixelShaderInputs == USC_USPBIN_MAXIMUM_PDS_DOUTI_COMMANDS) ||
			(psState->sShader.psPS->uIterationSize == EURASIA_USE_PRIMATTR_BANK_SIZE))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		/*
			Stop once we've run out of space in the primary PDS program data store.
		*/
		if (psState->sShader.psPS->uNumPDSConstantsAvailable < psState->sShader.psPS->uNumPDSConstantsPerTextureSample)
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

#if defined(OUTPUT_USPBIN)
static IMG_BOOL ConvertSMPToNDRUSPBIN(PINTERMEDIATE_STATE	psState, 
									  PINST					psSmpInst, 
									  IMG_UINT32			uTextureCoordinate, 
									  PUSC_LIST				psNDRMovList)
/*****************************************************************************
 FUNCTION	: ConvertSMPToNDRUSPBIN
    
 PURPOSE	: Convert an SMP instruction to a non-dependent texture sample for
			  an OUTPUT_USPBIN target.

 PARAMETERS	: psState				- Compiler state.
			  psSmpInst				- SMP instruction.
			  uTextureCoordinate	- Texture coordinate which is used for the sample.
			  psNDRMovList			- List which any new MOV instructions generated
									are added to.

 RETURNS	: TRUE if the SMP instruction was, completely or partly, converted.
*****************************************************************************/
{
	IMG_UINT32				uArg;
	UNIFLEX_TEXLOAD_FORMAT	eFormat;
	IMG_UINT32				uNumAttributes;
	PPIXELSHADER_INPUT		psNDRInput;

	PVR_UNREFERENCED_PARAMETER(psNDRMovList);

	/*
		Don't convert texture samples inside dynamic flow control to non-dependent for the USP.
		This is because the USP might need to convert the texture sample back to dependent and it can't
		handle the necessary modifications to the program when inserting a texture sample inside dynamic
		flow control.
	*/
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0 && psSmpInst->u.psSmp->bInsideDynamicFlowControl)
	{
		return IMG_FALSE;
	}

	ASSERT(CanAddNewNonDependentTextureSamples(psState));

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT) != 0)
	{
		PARG	psUnpackDest;

		ASSERT(psSmpInst->u.psSmp->sUSPSample.psSampleUnpack != NULL);
		psUnpackDest = &psSmpInst->u.psSmp->sUSPSample.psSampleUnpack->asDest[0];

		switch (psUnpackDest->eFmt)
		{
			case UF_REGFORMAT_F32:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_F32;
				uNumAttributes = 4;
				break;
			}

			case UF_REGFORMAT_F16:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_F16;
				uNumAttributes = 2;
				break;
			}
			
			/* C10/U8 does not require any format conversion on SGX543 */
			default:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
				uNumAttributes = 1;
				break;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
	{
		eFormat = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
		uNumAttributes = 1;
	}

	/*
		Add a new entry to this list of non-dependent texture samples.
	*/
	psNDRInput = 
		AddOrCreateNonDependentTextureSample(psState,
											 psSmpInst->u.psSmp->uTextureStage,
											 0 /* uChunk */,
											 uTextureCoordinate,
											 psSmpInst->u.psSmp->uDimensionality,
											 psSmpInst->u.psSmp->bProjected,
											 uNumAttributes,
											 uNumAttributes,
											 UF_REGFORMAT_UNTYPED,
											 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
											 IMG_FALSE /* bVector */,
											 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
											 eFormat);
		
	/*
		Change the SMP_USP instruction into a SMP_USP_NDR
	*/
	ModifyOpcode(psState, psSmpInst, ISMP_USP_NDR);

	psState->uOptimizationHint |= USC_COMPILE_HINT_OPTIMISE_USP_SMP;

	/*
		Initialise all non-dependent samples with 0 sources
	*/
	for (uArg = 0; uArg < psSmpInst->uArgumentCount; uArg++)
	{
		SetSrc(psState, psSmpInst, uArg, USEASM_REGTYPE_IMMEDIATE, 0 /* uNewNumber */, UF_REGFORMAT_F32);
	}

	/*
		Record the pre-sampled data as the only source
	*/
	for (uArg = 0; uArg < uNumAttributes; uArg++)
	{
		SetSrc(psState, 
			   psSmpInst, 
			   NON_DEP_SMP_TFDATA_ARG_START + uArg /* uSrcIdx */, 
			   USEASM_REGTYPE_TEMP, 
			   psNDRInput->psFixedReg->auVRegNum[uArg],
			   UF_REGFORMAT_UNTYPED);
	}

	psSmpInst->u.psSmp->sUSPSample.bNonDependent	= IMG_TRUE;
	psSmpInst->u.psSmp->uCoordSize					= psSmpInst->u.psSmp->uDimensionality;
	psSmpInst->u.psSmp->sUSPSample.uNDRTexCoord		= uTextureCoordinate;
	psSmpInst->u.psSmp->sUSPSample.bProjected		= psSmpInst->u.psSmp->bProjected;
	psSmpInst->u.psSmp->sUSPSample.bCentroid		= psNDRInput->sLoad.bCentroid;

	return IMG_TRUE;
}
#endif /* defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
static IMG_BOOL ConvertSMPToNDRUSCHW(PINTERMEDIATE_STATE	psState, 
									 PINST					psSmpInst, 
									 IMG_UINT32				uTextureCoordinate, 
									 PUSC_LIST				psNDRMovList)
/*****************************************************************************
 FUNCTION	: ConvertSMPToNDRUSCHW
    
 PURPOSE	: Convert an SMP instruction to a non-dependent texture sample for
			  an OUTPUT_USCHW target.

 PARAMETERS	: psState				- Compiler state.
			  psSmpInst				- SMP instruction.
			  uTextureCoordinate	- Texture coordinate which is used for the sample.
			  psNDRMovList			- List which any new MOV instructions generated
									are added to.

 RETURNS	: TRUE if the SMP instruction was, completely or partly, converted.
*****************************************************************************/
{
	IMG_UINT32				uPlaneIdx;
	IMG_UINT32				uDestOff;
	PSAMPLE_RESULT_CHUNK	psPlane;

	for (uPlaneIdx = 0, uDestOff = 0, psPlane = psSmpInst->u.psSmp->asPlanes; 
		 uPlaneIdx < psSmpInst->u.psSmp->uPlaneCount && CanAddNewNonDependentTextureSamples(psState); 
		 uDestOff += psPlane->uSizeInRegs, uPlaneIdx++, psPlane++)
	{
		IMG_UINT32				uResultCount;
		IMG_BOOL				bPlaneUsed;
		IMG_UINT32				uDestIdx;
		PPIXELSHADER_INPUT		psNDRInput;
		UF_REGFORMAT			eResultFormat;
		
		/*
			Get the size of the results when sampling this plane.
		*/
		uResultCount = psPlane->uSizeInRegs;

		/*
			Check if the destinations corresponding to this plane are used.
		*/
		bPlaneUsed = IMG_FALSE;
		for (uDestIdx = 0; uDestIdx < uResultCount; uDestIdx++)
		{
			if (psSmpInst->auLiveChansInDest[uDestOff + uDestIdx] != 0)
			{
				bPlaneUsed = IMG_TRUE;
				break;
			}
		}

		if (!bPlaneUsed)
		{
			continue;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psPlane->bVector)
		{
			if (psPlane->eFormat == USC_CHANNELFORM_F32)
			{
				eResultFormat = UF_REGFORMAT_F32;
			}
			else
			{
				ASSERT(psPlane->eFormat == USC_CHANNELFORM_F16);
				eResultFormat = UF_REGFORMAT_F16;
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			eResultFormat = UF_REGFORMAT_UNTYPED;
		}

		/*
			Add a new entry to this list of non-dependent texture samples.
		*/
		psNDRInput = 
			AddOrCreateNonDependentTextureSample(psState,
												 psSmpInst->u.psSmp->uTextureStage,
												 psSmpInst->u.psSmp->uFirstPlane + uPlaneIdx,
												 uTextureCoordinate,
												 psSmpInst->u.psSmp->uDimensionality,
												 psSmpInst->u.psSmp->bProjected,
												 uResultCount,
												 psPlane->uSizeInDwords,
												 eResultFormat,
												 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
												 psPlane->bVector,
												 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
												 UNIFLEX_TEXLOAD_FORMAT_UNDEF);
		
		/*
			Generate MOVs from the result of the newly added non-dependent texture sample to
			the	existing destinations of the SMP.
		*/
		for (uDestIdx = 0; uDestIdx < uResultCount; uDestIdx++)
		{
			PINST			psMOVInst;
			UF_REGFORMAT	eFmt = psSmpInst->asDest[uDestOff + uDestIdx].eFmt;

			psMOVInst = AllocateInst(psState, psSmpInst);
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psPlane->bVector)
			{
				ASSERT(uResultCount == 1);

				SetOpcode(psState, psMOVInst, IVMOV);
				psMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
				psMOVInst->u.psVec->aeSrcFmt[0] = eFmt;
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				SetOpcode(psState, psMOVInst, IMOV);
			}

			/* Copy the predicate from the SMP instruction. */
			CopyPredicate(psState, psMOVInst, psSmpInst);

			/*
				Copy the SMP destination.
			*/
			MoveDest(psState, psMOVInst, 0 /* uDestIdx */, psSmpInst, uDestOff + uDestIdx);
			psMOVInst->auDestMask[0] = psSmpInst->auDestMask[uDestOff + uDestIdx];
			psMOVInst->auLiveChansInDest[0] = psSmpInst->auLiveChansInDest[uDestOff + uDestIdx];
			CopyPartiallyWrittenDest(psState, 
									 psMOVInst, 
									 0 /* uCopyToDestIdx */, 
									 psSmpInst, 
									 uDestOff + uDestIdx /* uCopyFromDestIdx */);

			/*
				Set the MOV source to the register containing the result of the non-dependent
				texture sample.
			*/
			SetSrc(psState, 
				   psMOVInst, 
				   0 /* uSrcIdx */, 
				   USEASM_REGTYPE_TEMP, 
				   psNDRInput->psFixedReg->auVRegNum[uDestIdx],
				   eFmt);

			/*
				Insert the MOV instruction in the same place as the original SMP.
			*/
			InsertInstBefore(psState, psSmpInst->psBlock, psMOVInst, psSmpInst->psNext);

			AppendToList(psNDRMovList, &psMOVInst->sAvailableListEntry);
		}
	}
	
	if (uDestOff == psSmpInst->uDestCount)
	{
		/*
			Remove and free the SMP instruction.
		*/	
		RemoveInst(psState, psSmpInst->psBlock, psSmpInst);
		FreeInst(psState, psSmpInst);
	}
	else
	{
		IMG_UINT32	uNewSmpDestCount;
		IMG_UINT32	uDestIdx;

		/*
			Reduce the SMP destination count to just the destinations corresponding
			to the planes whose texture samples we weren't able to convert to non-dependent.
		*/
		uNewSmpDestCount = psSmpInst->uDestCount - uDestOff;
		for (uDestIdx = 0; uDestIdx < uNewSmpDestCount; uDestIdx++)
		{
			MoveDest(psState, psSmpInst, uDestIdx, psSmpInst, uDestOff + uDestIdx);
		}
		SetDestCount(psState, psSmpInst, uNewSmpDestCount);
		psSmpInst->u.psSmp->uPlaneCount -= uPlaneIdx;
		psSmpInst->u.psSmp->uFirstPlane += uPlaneIdx;
		memmove(psSmpInst->u.psSmp->asPlanes,
				psSmpInst->u.psSmp->asPlanes + uPlaneIdx,
				sizeof(psSmpInst->u.psSmp->asPlanes[0]) * psSmpInst->u.psSmp->uPlaneCount);
	}

	return IMG_TRUE;
}
#endif /* defined(OUTPUT_USCHW) */

static IMG_BOOL TryConvertSMPToNonDependentTextureSample(PINTERMEDIATE_STATE	psState,
														 PINST					psSmpInst, 
														 PUSC_LIST				psNDRMovList)
/*****************************************************************************
 FUNCTION	: TryConvertSMPToNonDependentTextureSample
    
 PURPOSE	: Try to convert a SMP instruction to a non-dependent texture
			  sample done by the PDS pixel program.

 PARAMETERS	: psState			- Compiler state.
			  psSmpInst			- SMP instruction.
			  psNDRMovList		- List which any new MOV instructions generated
								are added to.

 RETURNS	: TRUE if the SMP instruction was, completely or partly, converted.
*****************************************************************************/
{
	PARG						psFirstCoord;
	PUSC_LIST_ENTRY				psListEntry;
	PPIXELSHADER_INPUT			psInput;
	IMG_BOOL					bFoundInput;
	IMG_UINT32					uCoordIdx;
	IMG_UINT32					uFirstCoordNum;
	IMG_UINT32					uCoordNum;

	/*
		Returning coefficents or raw sample data isn't supported with non-dependent texture
		samples.
	*/
	if (psSmpInst->u.psSmp->eReturnData != SMP_RETURNDATA_NORMAL)
	{
		return IMG_FALSE;
	}

	/*
		The hardware doesn't support PCF on non-dependent texture samples.
	*/
	if (psSmpInst->u.psSmp->bUsesPCF)
	{
		return IMG_FALSE;
	}

	/*
		Immediate texel-space offsets aren't supported with non-dependent texture samples.
	*/
	if (psSmpInst->u.psSmp->sImmOffsets.bPresent)
	{
		return IMG_FALSE;
	}

	/*
		For texture arrays we need to insert USSE instructions to modify the texture base address
		before sending the state to the hardware.
	*/
	if (psSmpInst->u.psSmp->bTextureArray)
	{
		return IMG_FALSE;
	}

	psFirstCoord = &psSmpInst->asArg[SMP_COORD_ARG_START];
	if (psFirstCoord->uType != USEASM_REGTYPE_TEMP && psFirstCoord->uType != USC_REGTYPE_REGARRAY)
	{
		return IMG_FALSE;
	}
	if (psFirstCoord->uIndexType != USC_REGTYPE_NOINDEX)
	{
		return IMG_FALSE;
	}
	uFirstCoordNum = GetUnderlyingRegisterNumber(psState, psFirstCoord);

	/*
		Check if the source to the SMP is the result of an iteration.
	*/
	bFoundInput = IMG_FALSE;
	psInput = NULL;
	for (psListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
		 psListEntry != NULL;
		 psListEntry = psListEntry->psNext)
	{
		psInput = IMG_CONTAINING_RECORD(psListEntry, PPIXELSHADER_INPUT, sListEntry);

		if (psInput->psFixedReg->auVRegNum[0] == uFirstCoordNum)
		{
			bFoundInput = IMG_TRUE;
			break;
		}
	}
	if (!bFoundInput)
	{
		return IMG_FALSE;
	}

	/*
		Check the iteration is of a texture coordinate.
	*/
	if (psInput->sLoad.uTexture != UNIFLEX_TEXTURE_NONE)
	{
		return IMG_FALSE;
	}
	if (psInput->sLoad.eIterationType != UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE)
	{
		return IMG_FALSE;
	}
	/*	
		The hardware doesn't support non-dependent texture samples with flat
		shaded texture coordinates.
	*/
	uCoordNum = psInput->sLoad.uCoordinate;
	if (GetBit(psState->psSAOffsets->auFlatShadedTextureCoordinates, uCoordNum) != 0)
	{
		return IMG_FALSE;
	}
	if (psInput->sLoad.uNumAttributes < psSmpInst->u.psSmp->uCoordSize)
	{
		return IMG_FALSE;
	}

	/*
		Check the rest of the coordinates are from consecutive channels of the
		same input.
	*/
	for (uCoordIdx = 1; uCoordIdx < psSmpInst->u.psSmp->uCoordSize; uCoordIdx++)
	{
		PARG	psCoord = &psSmpInst->asArg[SMP_COORD_ARG_START + uCoordIdx];

		ASSERT(psCoord->eFmt == psFirstCoord->eFmt);
		if (psCoord->uType != psFirstCoord->uType)
		{
			return IMG_FALSE;
		}
		if (psCoord->uIndexType != USC_REGTYPE_NOINDEX)
		{
			return IMG_FALSE;
		}
		if (psInput->psFixedReg->auVRegNum[uCoordIdx] != GetUnderlyingRegisterNumber(psState, psCoord))
		{
			return IMG_FALSE;
		}
	}

	/*
		If the texture sample is projected check the source for the divide also
		comes from the iteration. 
	*/
	if (psSmpInst->u.psSmp->bProjected)
	{
		PARG	psProjSource = &psSmpInst->asArg[SMP_PROJ_ARG];
		static const struct
		{
			IMG_UINT32	uTCOff;
			IMG_UINT32	uTCChan;
		} asProjLoc[] =
		{
			/* UF_REGFORMAT_F32 */ 
			{
				USC_W_CHAN,										/* uTCOff */							
				0,												/* uTCChan */
			},
			/* UF_REGFORMAT_F16 */ 
			{
				USC_W_CHAN / F16_CHANNELS_PER_SCALAR_REGISTER,	/* uTCOff */
				USC_W_CHAN % F16_CHANNELS_PER_SCALAR_REGISTER	/* uTCChan */
			},
			/* UF_REGFORMAT_C10 */
			{
				0,												/* uTCOff */
				USC_W_CHAN,										/* uTCChan */
			},
		};
		IMG_UINT32	uTCOff;
		IMG_UINT32	uTCChan;
		IMG_UINT32	uProjSourceNumber;

		if (psProjSource->uType != USEASM_REGTYPE_TEMP && psProjSource->uType != USC_REGTYPE_REGARRAY)
		{
			return IMG_FALSE;
		}
		if (psProjSource->uIndexType != USC_REGTYPE_NOINDEX)
		{
			return IMG_FALSE;
		}
		ASSERT(psProjSource->eFmt == psFirstCoord->eFmt);

		/*
			Get the offset of the projection value used by the hardware for texture
			coordinates in this format.
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			uTCOff = 0;
			uTCChan = USC_W_CHAN;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			ASSERT(psProjSource->eFmt < (UF_REGFORMAT)(sizeof(asProjLoc) / sizeof(asProjLoc[0])));
			uTCOff = asProjLoc[psProjSource->eFmt].uTCOff;
			uTCChan = asProjLoc[psProjSource->eFmt].uTCChan;
		}

		/*
			Check the projection value in the instruction matches the one used by the hardware if this
			sample was converted to a non-dependent texture sample.
		*/
		uProjSourceNumber = GetUnderlyingRegisterNumber(psState, psProjSource);
		if (uTCOff > psInput->sLoad.uNumAttributes || uProjSourceNumber != psInput->psFixedReg->auVRegNum[uTCOff])
		{
			return IMG_FALSE;
		}
		if (psSmpInst->u.psSmp->uProjChannel != uTCChan)
		{
			return IMG_FALSE;
		}
	}

	return CHOOSE_USPBIN_OR_USCHW_FUNC(ConvertSMPToNDR, (psState, psSmpInst, psInput->sLoad.uCoordinate, psNDRMovList));
}

/*
	Opcodes for each compile type to scan looking for texture samples to convert to non-dependent texture samples.
*/
#define NDR_SMP_OPCODE_USCHW			ISMP
#define NDR_SMP_OPCODE_USPBIN			ISMP_USP

static
IMG_VOID GenerateNonDependentTextureSamples(PINTERMEDIATE_STATE	psState)
/*****************************************************************************
 FUNCTION	: GenerateNonDependentTextureSamples
    
 PURPOSE	: Try to lift texture samples which use iterated values as texture
			  coordinates into the pixel shader PDS program.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL			bGeneratedNDSamples;
	SAFE_LIST_ITERATOR	sIter;
	USC_LIST			sNDRMovList;
	IOPCODE				eSMPOpcode;

	/*
		Non-dependent texture samples are supported only by pixel shaders.
	*/
	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}
	
	/*
		Has the user asked us to not generate any non-dependent texture samples?
	*/
	if ((psState->uCompilerFlags & UF_NONONDEPENDENTSAMPLES) != 0)
	{
		return;
	}

	bGeneratedNDSamples = IMG_FALSE;
	InitializeList(&sNDRMovList);

	eSMPOpcode = CHOOSE_USPBIN_OR_USCHW_VALUE(NDR_SMP_OPCODE);
	InstListIteratorInitialize(psState, eSMPOpcode, &sIter);

	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psSmpInst = InstListIteratorCurrent(&sIter);
		
		ASSERT(psSmpInst->eOpcode == eSMPOpcode);

		/*
			No point in carrying on if we've run out of space.
		*/
		if (!CanAddNewNonDependentTextureSamples(psState))
		{
			break;
		}

		/*
			Try to convert this SMP instruction to a non-dependent texture sample. If
			successful the instruction is freed.
		*/
		if (TryConvertSMPToNonDependentTextureSample(psState, psSmpInst, &sNDRMovList))
		{
			bGeneratedNDSamples = IMG_TRUE;
		}
	}
	InstListIteratorFinalise(&sIter);
	
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		if (bGeneratedNDSamples)
		{
			PUSC_LIST_ENTRY	psListEntry;
			PUSC_LIST_ENTRY psNextListEntry;

			/*
				Where we lifted an SMP instruction we replaced it by MOVs from the result of the non-dependent
				texture sample to the original destinations of the SMP instruction. Now try to replace the MOV
				destinations by their sources.
			*/
			for (psListEntry = sNDRMovList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
			{
				PINST		psMovInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

				psNextListEntry = psListEntry->psNext;

				EliminateGlobalMove(psState, 
									psMovInst->psBlock, 
									psMovInst, 
									&psMovInst->asArg[0], 
									&psMovInst->asDest[0], 
									NULL /* psEvalList */);
			}
		}
	}
}

#if defined(OUTPUT_USCHW)
static
IMG_VOID ExpandMultiplaneSMP(PINTERMEDIATE_STATE psState, PINST psSmpInst)
/*****************************************************************************
 FUNCTION	: ExpandMultiplaneSMP
    
 PURPOSE	: Expand an SMP instruction into a separate instruction for sampling
			  each plane.

 PARAMETERS	: psState			- Compiler state.
			  psSmpInst			- SMP instruction to expand.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestOff;
	IMG_INT32	iPlaneIdx;

	uDestOff = psSmpInst->uDestCount;
	for (iPlaneIdx = (IMG_INT32)(psSmpInst->u.psSmp->uPlaneCount - 1); iPlaneIdx >= 0; iPlaneIdx--)
	{
		PINST		psPlaneSmpInst;
		IMG_UINT32	uResultCount;
		IMG_BOOL	bPlaneUsed;
		IMG_UINT32	uDestIdx;

		/*	
			Get the size of the data returned when sampling this plane.
		*/
		uResultCount = psSmpInst->u.psSmp->asPlanes[iPlaneIdx].uSizeInRegs;

		uDestOff -= uResultCount;

		/*
			Get if any of the destinations corresponding to this plane are used.
		*/
		bPlaneUsed = IMG_FALSE;
		for (uDestIdx = 0; uDestIdx < uResultCount; uDestIdx++)
		{
			if (psSmpInst->auLiveChansInDest[uDestOff + uDestIdx] != 0)
			{
				bPlaneUsed = IMG_TRUE;
				break;
			}
		}

		if (!bPlaneUsed)
		{
			if (iPlaneIdx == 0)
			{
				RemoveInst(psState, psSmpInst->psBlock, psSmpInst);
				FreeInst(psState, psSmpInst);
			}
			continue;
		}

		if (iPlaneIdx > 0)
		{
			/*
				Create a copy of the original instruction.
			*/
			psPlaneSmpInst = CopyInst(psState, psSmpInst);

			/*
				Insert the new instruction after the original instruction.
			*/
			InsertInstBefore(psState, psSmpInst->psBlock, psPlaneSmpInst, psSmpInst->psNext);

			/*
				Set the new instruction to sample just the current plane.
			*/
			psPlaneSmpInst->u.psSmp->uFirstPlane += (IMG_UINT32)iPlaneIdx;
			psPlaneSmpInst->u.psSmp->asPlanes[0] = psPlaneSmpInst->u.psSmp->asPlanes[iPlaneIdx];

			/*
				Write just the destinations associated with the current plane.
			*/
			for (uDestIdx = 0; uDestIdx < uResultCount; uDestIdx++)
			{
				MoveDest(psState, psPlaneSmpInst, uDestIdx, psSmpInst, uDestOff + uDestIdx);
				psPlaneSmpInst->auLiveChansInDest[uDestIdx] = psSmpInst->auLiveChansInDest[uDestOff + uDestIdx];
				psPlaneSmpInst->auDestMask[uDestIdx] = psSmpInst->auDestMask[uDestOff + uDestIdx];
			}
		}
		else
		{
			ASSERT(uDestOff == 0);
			psPlaneSmpInst = psSmpInst;
		}

		SetDestCount(psState, psPlaneSmpInst, uResultCount);
		psPlaneSmpInst->u.psSmp->uPlaneCount = 1;
	}
}
#endif /* defined(OUTPUT_USCHW) */

IMG_INTERNAL
IMG_VOID FinaliseTextureSamples(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FinaliseTextureSamples
    
 PURPOSE	: Put texture sample instructions in their final form.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(OUTPUT_USCHW)
	static const IOPCODE aeUSCHWSmpOpcodes[] = {ISMP, ISMPBIAS, ISMPREPLACE, ISMPGRAD};
	#endif /* defined(OUTPUT_USCHW) */
	static const IOPCODE aeAllSmpOpcodes[] = 
	{
		ISMP, ISMPBIAS, ISMPREPLACE, ISMPGRAD,
		#if defined(OUTPUT_USPBIN)
		ISMP_USP, ISMPBIAS_USP, ISMPREPLACE_USP, ISMPGRAD_USP,
		#endif /* defined(OUTPUT_USPBIN) */
	};
	IMG_UINT32 uOpcodeType;

	GenerateNonDependentTextureSamples(psState);

	/*
		Apply fixes for hardware limitations now we definitely know which texture sample
		are going to be dependent.

		Do this before expanding multiplane texture samples so the fix instructions are
		only generated once.
	*/
	for (uOpcodeType = 0; uOpcodeType < (sizeof(aeAllSmpOpcodes) / sizeof(aeAllSmpOpcodes[0])); uOpcodeType++)
	{
		SAFE_LIST_ITERATOR	sIter;
		
		InstListIteratorInitialize(psState, aeAllSmpOpcodes[uOpcodeType], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST	psSmpInst = InstListIteratorCurrent(&sIter);

			if (psSmpInst->u.psSmp->bProjected)
			{
				/*
					The hardware only supports projection on non-dependent texture samples so add
					extra instructions to do it manually.
				*/
				ExpandProjectedSMP(psState, psSmpInst);
			}
		}
		InstListIteratorFinalise(&sIter);
	}

	#if defined(OUTPUT_USCHW)
	for (uOpcodeType = 0; uOpcodeType < (sizeof(aeUSCHWSmpOpcodes) / sizeof(aeUSCHWSmpOpcodes[0])); uOpcodeType++)
	{
		SAFE_LIST_ITERATOR	sIter;
		
		InstListIteratorInitialize(psState, aeUSCHWSmpOpcodes[uOpcodeType], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psSmpInst = InstListIteratorCurrent(&sIter);
	
			ExpandMultiplaneSMP(psState, psSmpInst);
		}
		InstListIteratorFinalise(&sIter);

		/*
			Insert instructions to load texture state from the constant buffer.
		*/
		InstListIteratorInitialize(psState, aeUSCHWSmpOpcodes[uOpcodeType], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psSmpInst = InstListIteratorCurrent(&sIter);
			ARG			asStateArgs[SMP_MAX_STATE_SIZE];
			IMG_UINT32	uStateWordIdx;

			/* Skip if the texture state arguments have already been set up. */
			if (psSmpInst->asArg[SMP_STATE_ARG_START].uType != USC_REGTYPE_UNUSEDSOURCE)
			{
				continue;
			}

			SetupSmpStateArgument(psState,
								  psSmpInst->psBlock,
								  psSmpInst,
								  asStateArgs,
								  psSmpInst->u.psSmp->uTextureStage,
								  psSmpInst->u.psSmp->uFirstPlane,
								  psSmpInst->u.psSmp->bTextureArray,
								  psSmpInst->asArg + SMP_ARRAYINDEX_ARG,
								  &psSmpInst->u.psSmp->sImmOffsets,
								  psSmpInst->u.psSmp->uDimensionality,
								  psSmpInst->u.psSmp->bSampleIdxPresent,
								  psSmpInst->asArg + SMP_SAMPLE_IDX_ARG_START);

			/*
				Copy the state arguments into the instruction.
			*/
			for (uStateWordIdx = 0; uStateWordIdx < psState->uTexStateSize; uStateWordIdx++)
			{
				SetSrcFromArg(psState, psSmpInst, SMP_STATE_ARG_START + uStateWordIdx, &asStateArgs[uStateWordIdx]);
			}

			/*
				We have applied the texture array index by modifying the texture base address
				in the texture state so clear any references to the array index from the
				SMP instruction.
			*/
			if (psSmpInst->u.psSmp->bTextureArray)
			{
				SetSrcUnused(psState, psSmpInst, SMP_ARRAYINDEX_ARG);
				psSmpInst->u.psSmp->bTextureArray = IMG_FALSE;
			}
		}
		InstListIteratorFinalise(&sIter);
	}
	#endif /* defined(OUTPUT_USCHW) */
}


IMG_INTERNAL
IMG_VOID ExpandConditionalTextureSamples(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ExpandConditionalTextureSamples
    
 PURPOSE	:   The hardware doesn't support texture samples requiring gradient calculations inside
				dynamic flow control so add extra instructions to calculate gradients.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	static const IOPCODE aeAllSmpOpcodes[] = 
	{
		ISMP, ISMPBIAS, ISMPREPLACE, ISMPGRAD,
		#if defined(OUTPUT_USPBIN)
		ISMP_USP, ISMPBIAS_USP, ISMPREPLACE_USP, ISMPGRAD_USP,
		#endif /* defined(OUTPUT_USPBIN) */
	};
	IMG_UINT32 uOpcodeType;

	for (uOpcodeType = 0; uOpcodeType < (sizeof(aeAllSmpOpcodes) / sizeof(aeAllSmpOpcodes[0])); uOpcodeType++)
	{
		SAFE_LIST_ITERATOR	sIter;
		
		InstListIteratorInitialize(psState, aeAllSmpOpcodes[uOpcodeType], &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psSmpInst = InstListIteratorCurrent(&sIter);

			/* Is this a sample instruction inside a dynamic flow control block.
			   Samples that were originally in dynamic flow control may have been
			   predicated and moved into the main function, in which case the
			   bInsideDynamicFlowControl flag is no longer correct
			*/
			if (
					(
						#if defined(OUTPUT_USPBIN)
						psSmpInst->eOpcode == ISMP_USP ||
						psSmpInst->eOpcode == ISMPBIAS_USP ||
						#endif /* defined(OUTPUT_USBPIN) */
						psSmpInst->eOpcode == ISMP ||
						psSmpInst->eOpcode == ISMPBIAS
					) &&
					(
						(psSmpInst->u.psSmp->bInsideDynamicFlowControl
						   && psSmpInst->psBlock != psState->psFnOutermost->sCfg.psEntry) ||
						psSmpInst->u.psSmp->bTextureArray
					)
				)
			{
				SAMPLE_GRADIENTS	sGradients;
				PARG				psLODBias = IMG_NULL;

				if (
						#if defined(OUTPUT_USPBIN)
						psSmpInst->eOpcode == ISMPBIAS_USP ||
						#endif /* defined(OUTPUT_USPBIN) */
						psSmpInst->eOpcode == ISMPBIAS
				   )
				{
					psLODBias = &psSmpInst->asArg[SMP_LOD_ARG_START];
				}

				#if defined(OUTPUT_USPBIN)
				if (psSmpInst->eOpcode == ISMP_USP || psSmpInst->eOpcode == ISMPBIAS_USP)
				{
					ModifyOpcode(psState, psSmpInst, ISMPGRAD_USP);
				}
				else
				#endif /* defined(OUTPUT_USPBIN) */
				{
					ModifyOpcode(psState, psSmpInst, ISMPGRAD);
				}

				/* Get the format and layout of the gradient data expected by the hardware. */
				GetGradientDetails(psState, 
								   psSmpInst->u.psSmp->uDimensionality, 
								   psSmpInst->asArg[SMP_COORD_ARG_START].eFmt,
								   &sGradients);

				/*
					Insert instruction to calculate gradients from the SMP instruction's
					texture coordinates.
				*/
				EmitGradCalc(psState,
							 psSmpInst->psBlock,
							 psSmpInst,
							 psSmpInst->u.psSmp->uDimensionality,
							 psSmpInst->asArg[SMP_COORD_ARG_START].eFmt,
							 USC_UNDEF /* uCoordTempStart */, 
							 psSmpInst->asArg + SMP_COORD_ARG_START,
							 psLODBias,
							 &sGradients);
	
				/*
					Add the calculated gradients as sources to the SMP instruction.
				*/
				SetSampleGradientsDetails(psState, psSmpInst, &sGradients);

				/*
					Clear any LOD bias source to the SMP instruction.
				*/
				SetSrcUnused(psState, psSmpInst, SMP_LOD_ARG_START);
			}
		}
		InstListIteratorFinalise(&sIter);
	}
}


static IMG_VOID AddRemappedEntry(PINTERMEDIATE_STATE		psState,
								 IMG_UINT32					uConstsBuffNum,
								 IMG_UINT32					uConstIdx,
								 UF_REGFORMAT				eFormat)
/*****************************************************************************
 FUNCTION	: AddRemappedEntry

 PURPOSE	: Add an entry to the in-memory constant remapping table.

 PARAMETERS	: psState			- Compiler state.
			  uConstsBuffNum	- The constant buffer to add for.
			  uConstIdx			- The index of the constant.
			  eFormat			- The format of the constant.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCONSTANT_BUFFER	psConstBuf = &psState->asConstantBuffer[uConstsBuffNum];
	IMG_UINT32			uEntry = psConstBuf->uRemappedCount;

	ArraySet(psState, 
			 psConstBuf->psRemappedMap, 
			 uEntry, 
			 (IMG_PVOID)(IMG_UINTPTR_T)uConstIdx);
	ArraySet(psState, 
			 psConstBuf->psRemappedFormat, 
			 uEntry, 
			 (IMG_PVOID)(IMG_UINTPTR_T)eFormat);
	psConstBuf->uRemappedCount++;
}

typedef struct _CONST_PACK_RANGEDATA
{
	/*
		Estimated number of cycles saved by moving this entire range to
		secondary attributes.
	*/
	IMG_UINT32				uCost;

	/*
		List of the LOADCONST instructions loading constants in the range.
	*/
	USC_LIST				sUsesList;

	/*	
		TRUE for a range of C10 constants if the ALPHA channel is used from some of the 
		constants.
	*/
	IMG_BOOL				b40Bit;

	/*
		Points to the entry in the list SAPROG_STATE.sConstantRangeList for this
		range.
	*/
	PCONSTANT_RANGE			psRange;

	/*
		Entry in the list CONST_PACK_STATE.sCostSortedRangeList
	*/
	USC_LIST_ENTRY			sCostListEntry;

	/* Number of dwords required for the constants covered by this range. */
	IMG_UINT32				uRangeSizeInDwords;

	/*
		Number of constant channels per-dword for the range.
	*/
	IMG_UINT32				uRangeStep;

	/* TRUE if the addressing on this range mean it can't be moved to secondary attributes. */
	IMG_BOOL				bInvalidForSecAttrs;
} CONST_PACK_RANGE_STATE, *PCONST_PACK_RANGE_STATE;

typedef struct
{
	/*
		List of constant ranges used in the program; sorted by the estimated number of
		cycles saved by moving each range to secondary attributes.
	*/
	USC_LIST				sCostSortedRangeList;

	/*
		List of statically addressed constants used in the program; sorted by the
		estimated number of cycles saved by moving each constant to a secondary
		attributes.
	*/
	USC_LIST				sCostSortedStaticConstList;

	/*
		Per-constant range data specific to this module.
	*/
	PCONST_PACK_RANGE_STATE	asRangeState;

	#if defined(OUTPUT_USPBIN)
	/*
		Bit-mask of the samplers used by the program.
	*/
	IMG_PUINT32				auSamplersUsedMask;
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Count of secondary attributes removed from the pool available for
		constants so we always have enough for memory base address so
		all constant loads can be replaced by memory loads.
	*/
	IMG_UINT32				uNumSecAttrsReservedForBaseAddresses;

	/*
		List of MOV instructions from secondary attribute inserted to replace
		constant loads.
	*/
	USC_LIST				sMOVList;
} CONST_PACK_STATE, *PCONST_PACK_STATE;

static IMG_UINT32 ApplyConstantRemapping(PINTERMEDIATE_STATE		psState,
										 IMG_UINT32					uConstsBuffNum, 
										 IMG_UINT32					uSourceNum,
										 IMG_UINT32					uSourceChan,
										 PCONSTANT_RANGE			psSourceRange,
										 UF_REGFORMAT				eFormat)
/*****************************************************************************
 FUNCTION	: ApplyConstantRemapping

 PURPOSE	: Applies constant remapping.

 PARAMETERS	: psState			- Compiler state.
			  uConstsBuffNum	- Constants buffer to which offset belongs.
			  eRelatveIndex		- Index the access is relative to.
			  uSourceNum		- Base register for the access.
			  uSourceChan		- Base channel for the access.
			  psSourceRange		- Range this access falls into (if any).

 RETURNS	: Remapped memory offset.
*****************************************************************************/
{
	IMG_UINT32					uMemIdx;
	IMG_UINT32					uSourceOffset;
	USC_PARRAY					psConstMap;
	PCONSTANT_BUFFER			psConstBuf = &psState->asConstantBuffer[uConstsBuffNum];

	ASSERT(psConstBuf->psRemappedMap != NULL);
	ASSERT(psConstBuf->psRemappedFormat != NULL);

	uSourceOffset = (uSourceNum << 2) + uSourceChan;
	if (psSourceRange != NULL && psSourceRange->bC10SubcomponentIndex)
	{
		uSourceOffset |= REMAPPED_CONSTANT_UNPACKEDC10;
	}

	/*
		Check if we have already remapped this constant.
	*/
	psConstMap = psConstBuf->psRemappedMap;
	for (uMemIdx = 0; uMemIdx < psConstBuf->uRemappedCount; uMemIdx++)
	{	
		if ((IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psConstMap, uMemIdx) == uSourceOffset)
		{
			return uMemIdx;
		}
	}
	
	/*
		If this constant falls into a user-defined constant range then remap the entire range.
	*/
	if (psSourceRange != NULL)
	{
		IMG_UINT32					uRangeStart, uRangeEnd;
		IMG_UINT32					uRangeStartInComponents, uRangeEndInComponents;
		IMG_UINT32					uConstIdx;
		IMG_UINT32					uComponentsPerRegister;
		PUNIFLEX_CONSTBUFFERDESC	psCBDesc = &psState->psSAOffsets->asConstBuffDesc[uConstsBuffNum];
		PUNIFLEX_RANGE				psRange = &psCBDesc->sConstsBuffRanges.psRanges[psSourceRange->uRangeIdx];
		IMG_UINT32					uRemapEntryFlag = 0;

		ASSERT((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0);
	
		uRangeStart = psRange->uRangeStart;
		uRangeEnd = psRange->uRangeEnd - 1;

		uRangeStartInComponents = uRangeStart << 2;
		uRangeEndInComponents = (uRangeEnd << 2) + 3;

		switch (eFormat)
		{
			case UF_REGFORMAT_F32:
			{
				/*
					Each constant in the range uses four entries
				*/
				uMemIdx += (uSourceNum - uRangeStart) * CHANNELS_PER_INPUT_REGISTER + uSourceChan;

				uComponentsPerRegister = 1;
				break;
			}
			case UF_REGFORMAT_F16:
			{
				ASSERT((uSourceChan % 2) == 0);

				/*
					Each constant in the range uses two entries.
				*/
				uMemIdx += (uSourceNum - uRangeStart) * 2 + (uSourceChan >> 1);

				uComponentsPerRegister = 2;
				break;
			}
			case UF_REGFORMAT_C10:
			{
				if (psSourceRange->bC10SubcomponentIndex)
				{
					ASSERT(uSourceChan == 0);

					uMemIdx += (uSourceNum - uRangeStart) * 2;
					uComponentsPerRegister = 2;
					uRemapEntryFlag = REMAPPED_CONSTANT_UNPACKEDC10;
				}
				else
				{
					ASSERT(uSourceChan == 0 || uSourceChan == 3);

					/*
						Each constant in the range uses two entries.
					*/
					uMemIdx += (uSourceNum - uRangeStart) * 2;

					/*
						The alpha part of the constant is in the next register.
					*/
					if (uSourceChan == 3)
					{
						uMemIdx += LONG_SIZE;
					}

					uComponentsPerRegister = 4;
				}
				break;
			}
			default:
			{
				/* U8 format constants are not suported (yet) */
				imgabort();
			}
		}

		/*
			Add entries to the remapping table for every constant/component in the range.
		*/
		for (uConstIdx = uRangeStartInComponents; uConstIdx <= uRangeEndInComponents; uConstIdx += uComponentsPerRegister)
		{
			AddRemappedEntry(psState, uConstsBuffNum, uConstIdx | uRemapEntryFlag, eFormat);
			/*
				Add a second entry for the top 10-bits of the constant.
			*/
			if (eFormat == UF_REGFORMAT_C10 && !psSourceRange->bC10SubcomponentIndex)
			{
				AddRemappedEntry(psState, uConstsBuffNum, (uConstIdx + 3) | uRemapEntryFlag, eFormat);
			}
		}

		return uMemIdx;
	}
	else
	{
		/*
			Add a new entry to the table for the constant.
		*/
		AddRemappedEntry(psState, uConstsBuffNum, uSourceOffset, eFormat);

		return uMemIdx;
	}
}

static IMG_BOOL ConvertToLIMM(PINTERMEDIATE_STATE	psState,
							  PINST					psInst)
/*********************************************************************************
 Function			: ConvertToLIMM

 Description		: Try to convert a memory load instruction to a LIMM instruction.

 Parameters			: psState			- Current compiler state.
					  psInst			- Instruction to convert.

 Globals Effected	: None

 Return				: TRUE if the instruction was converted.
*********************************************************************************/
{
	if (psInst->u.psLoadConst->bStaticConst)
	{
		IMG_UINT32	uStaticConstValue = psInst->u.psLoadConst->uStaticConstValue;

		SetOpcode(psState, psInst, IMOV);
		psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[0].uNumber = uStaticConstValue;

		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID SetBaseAddressRegForConstsBuffer(PINTERMEDIATE_STATE psState, 
												 PCONST_PACK_STATE	psCPState,
												 PINST				psInst, 
												 IMG_UINT32			uConstsBuffNum)
/*********************************************************************************
 Function			: SetBaseAddressRegForConstsBuffer

 Description		: Set up the argument to a LOADCONST instruction containing the
					  base address of the constant buffer in memory.

 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.
					  psInst			- Instruction to convert.
					  uConstsBuffNum	- Constants buffer to which constant in instruction belongs.

 Globals Effected	: None.

 Return				: Nothing.
*********************************************************************************/
{
	if ((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) != 0)
	{
		PCONSTANT_BUFFER	psConstBuf = &psState->asConstantBuffer[uConstsBuffNum];

		if (psConstBuf->sBase.uType == USC_REGTYPE_DUMMY)
		{
			PINREGISTER_CONST	psBase;

			/*
				We earlier reserved space in the secondary attributes for the
				base address of each constant buffer. Now we've actually
				going to assign a secondary attribute release the reserved space.
			*/
			psState->sSAProg.uInRegisterConstantLimit++;
			ASSERT(psCPState->uNumSecAttrsReservedForBaseAddresses > 0);
			psCPState->uNumSecAttrsReservedForBaseAddresses--;

			/*
				Add a new entry to the in-register constants table for the
				base address of the memory buffer.
			*/
			AddSimpleSecAttr(psState,
							 0 /* uAlignment */,
							 USC_UNDEF /* uNum */,
							 UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE, 
							 uConstsBuffNum,
							 &psBase);

			GetDriverLoadedConstantLocation(psState, 
											psBase, 
											&psConstBuf->sBase.uType, 
											&psConstBuf->sBase.uNumber);
		}
		SetSrc(psState, 
			   psInst, 
			   LOADMEMCONST_BASE_ARGINDEX, 
			   psConstBuf->sBase.uType, 
			   psConstBuf->sBase.uNumber, 
			   psConstBuf->sBase.eFmt);
	}
}

static IMG_VOID GenerateMemoryLoad(PINTERMEDIATE_STATE		psState,
								   PCONST_PACK_STATE		psCPState,
								   PINST					psLCInst,
								   PARG						psDest,
								   IMG_UINT32				uStaticOffset,
								   IMG_BOOL					bLastInExpansion,
								   IMG_UINT32				uLiveChansInDest)
/*********************************************************************************
 Function			: GenerateMemoryLoad

 Description		: Convert a ILOADCONST instruction to a memory load.

 Parameters			: psState			- Current compiler state.
					  psCPState			- Modulate state.
					  psLCInst			- Instruction to convert.
					  psDest			- Destination for the memory load.
					  uStaticOffset		- Static offset into the constant buffer.
					  bLastInExpansion	- TRUE if this is the last LOADMEMCONST
										to be generated from this LOADCONST.
					  uLiveChansInDest	- Mask of channels used from the memory
										load's result.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32					uRelativeStrideInBytes = psLCInst->u.psLoadConst->uRelativeStrideInBytes;
	PINST						psLMCInst;
	IMG_UINT32					uBufferIdx = psLCInst->u.psLoadConst->uBufferIdx;
	PARG						psStaticOffset;
	PUNIFLEX_CONSTBUFFERDESC	psBufDesc = &psState->psSAOffsets->asConstBuffDesc[uBufferIdx];
	IMG_UINT32					uRangeMax;

	/*
		Flag that this constant is going to be loaded from memory (not secondary attributes).
	*/
	psLMCInst = AllocateInst(psState, psLCInst);
	SetOpcode(psState, psLMCInst, ILOADMEMCONST);
	
	if (psDest != NULL)
	{
		SetDestFromArg(psState, psLMCInst, 0 /* uDestIdx */, psDest);
	}
	else
	{
		MoveDest(psState, psLMCInst, 0 /* uMoveToDestIdx */, psLCInst, 0 /* uMoveFromDestIdx */);
	}
	psLMCInst->auLiveChansInDest[0] = uLiveChansInDest;

	/*
		Insert the new instruction immediately before the LOADCONST instruction.
	*/
	InsertInstBefore(psState, psLCInst->psBlock, psLMCInst, psLCInst);

	/*
		Copy the dynamic (calculated at runtime) part of the source offset into the constant buffer.
	*/
	if (!bLastInExpansion)
	{
		SetSrcFromArg(psState, 
					  psLMCInst, 
					  LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX, 
					  &psLCInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX]);
	}
	else
	{
		MoveSrc(psState, psLMCInst, LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX, psLCInst, LOADCONST_DYNAMIC_OFFSET_ARGINDEX);
	}

	/*
		Copy the immediate part of the source offset into the constant buffer.
	*/
	psStaticOffset = &psLMCInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];
	psStaticOffset->uType = USEASM_REGTYPE_IMMEDIATE;
	psStaticOffset->uNumber = uStaticOffset;

	/*
		For program using multiple constant buffers assign a secondary attribute to hold
		the base address of this constant buffer in memory.
	*/
	if ((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) != 0)
	{
		SetBaseAddressRegForConstsBuffer(psState, psCPState, psLMCInst, uBufferIdx);
	}
	else
	{
		/*
			Otherwise set the base address argument to the user supplied
			secondary attribute number.
		*/
		PARG	psBaseArg;

		psBaseArg = &psLMCInst->asArg[LOADMEMCONST_BASE_ARGINDEX];
		InitInstArg(psBaseArg);
		psBaseArg->uType = USEASM_REGTYPE_SECATTR;
		psBaseArg->uNumber = psState->psSAOffsets->uConstantBase;
	}

	/*
		Copy the flag for an instruction where the memory address isn't known at compile time.
	*/
	psLMCInst->u.psLoadMemConst->bRelativeAddress = psLCInst->u.psLoadConst->bRelativeAddress;

	/*
		Set the size of the data to be loaded from memory.
	*/
	psLMCInst->u.psLoadMemConst->uDataSize = LONG_SIZE;

	/*
		We never need to bypass the cache when loading constant data. We never store to the same
		region of memory and the driver will take care of flushing the cache when it writes.
	*/
	psLMCInst->u.psLoadMemConst->bBypassCache = IMG_FALSE;

	/*
		Set the stride of the dynamic index as an argument.
	*/
	InitInstArg(&psLMCInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX]);
	psLMCInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
	if (psLMCInst->u.psLoadMemConst->bRelativeAddress)
	{
		psLMCInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber = uRelativeStrideInBytes;
	}
	else
	{
		psLMCInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber = 0;
	}

	/*
		Should range checking be enabled for the memory load?
	*/
	psLMCInst->u.psLoadMemConst->bRangeCheck = IMG_FALSE;
	if ((psState->uCompilerFlags & UF_ENABLERANGECHECK) != 0 && psLCInst->u.psLoadMemConst->bRelativeAddress)
	{
		psLMCInst->u.psLoadMemConst->bRangeCheck = IMG_TRUE;
	}

	/*
		Convert the range checking value to bytes.
	*/
	uRangeMax = psBufDesc->uRelativeConstantMax * CHANNELS_PER_INPUT_REGISTER * sizeof(IMG_UINT32);

	/*
		Set the value to check offsets against as an arguments to the memory load.
	*/
	SetSrc(psState,
		   psLMCInst,
		   LOADMEMCONST_MAX_RANGE_ARGINDEX,
		   USEASM_REGTYPE_IMMEDIATE,
		   uRangeMax,
		   UF_REGFORMAT_F32);
}

static IMG_VOID UpdateMemoryLoad(PINTERMEDIATE_STATE		psState,
								 PCONST_PACK_STATE			psCPState,
								 PINST						psInst,
								 PARG						psDest,
								 IMG_UINT32					uExtraOffsetInChans,
								 IMG_UINT32					uExtraOffsetInBytes,
								 IMG_BOOL					bLastInExpansion,
								 UF_REGFORMAT				eDestFmt,
								 IMG_UINT32					uLiveChansInDest)
/*********************************************************************************
 Function			: UpdateMemoryLoad

 Description		: Updates memory load instruction after constants have been
					  moved to secondary attributes.
 
 Parameters			: psState				- Current compiler state.
					  psCPState				- Module state.
					  psBlock				- Block containing the memory load instruction.
					  psInst				- Memory load instruction.
					  eDestFmt				- Format of the loaded data.
					  uLiveChansInDest		- Mask of channels used from the
											memory load's result.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uConstNum;
	IMG_UINT32	uMemOffset;
	IMG_UINT32	uBufferIdx = psInst->u.psLoadConst->uBufferIdx;

	/*
		Extract the source offset into the constant buffer.
	*/
	ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
	uConstNum = psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

	if (psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE)
	{
		/*
			The memory offset is already in terms of bytes.
		*/
		uMemOffset = uConstNum + uExtraOffsetInBytes;
	}
	else if (psState->uCompilerFlags & UF_NOCONSTREMAP)
	{
		ASSERT(psInst->asDest[0].eFmt == UF_REGFORMAT_F32);

		/*
			Convert the offset from LONGs to BYTEs.
		*/
		uMemOffset = (uConstNum + uExtraOffsetInChans) * LONG_SIZE;
	}
	else
	{
		IMG_UINT32	uOffset;

		/*
			Remap the offset into memory to point into the packed constant buffer.
		*/
		uConstNum += uExtraOffsetInChans;
		uOffset = ApplyConstantRemapping(psState,
										 uBufferIdx,
										 uConstNum >> 2,
										 uConstNum & 3,
										 psInst->u.psLoadConst->psRange,
										 eDestFmt);

		/*
			Convert the offset from LONGs to BYTEs
		*/
		uMemOffset = uOffset * LONG_SIZE;

		/*
			Apply any extra offset to select an individual C10 channel.
		*/
		if (psInst->u.psLoadConst->uC10CompOffset > 0)
		{
			ASSERT(eDestFmt == UF_REGFORMAT_C10);
			ASSERT(psInst->u.psLoadConst->psRange != NULL);
			ASSERT(psInst->u.psLoadConst->psRange->bC10SubcomponentIndex);
			ASSERT(uExtraOffsetInChans == 0);
			ASSERT(psInst->u.psLoadConst->bRelativeAddress);

			uMemOffset += psInst->u.psLoadConst->uC10CompOffset * WORD_SIZE;
		}
	}

	/*
		Generate a memory load from the ILOADCONST instruction.
	*/
	GenerateMemoryLoad(psState, 
					   psCPState, 
					   psInst,
					   psDest, 
					   uMemOffset,
					   bLastInExpansion,
					   uLiveChansInDest);
}

static
IMG_VOID ExpandMultiMemoryLoad(PINTERMEDIATE_STATE	psState,
							   PCONST_PACK_STATE	psCPState,
							   PINST				psDestInst,
							   IMG_UINT32			uDestInstSrcStart,
							   PINST				psInst,
							   IMG_UINT32			uDwordsPerVector,
							   IMG_UINT32			uChansPerDword,
							   IMG_UINT32 const*	auChansPerDword,
							   UF_REGFORMAT			eDestFmt)
/*********************************************************************************
 Function			: ExpandMultiMemoryLoad

 Description		: Convert a LOADCONST instruction to a sequence of LOADMEMCONST
					  instructions.
 
 Parameters			: psState				- Current compiler state.
					  psCPState				- Module state.
					  psDestInst			- Argument array to fill out with the
					  uDestInstSrcStart		results of the memory load.
					  psInst				- Original LOADCONST instruction.
					  uDwordsPerVector		- Size of the vector to load in dwords.
					  uChansPerDword		- Count of channels in the constant format
											which fit in a single dword.
					  auChansPerDword		- Array of the mask of channels used from
											dword.
					  eDestFmt				- Format of the loaded data.
					  
 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_INT32	iVectorIdx;
	IMG_UINT32	uVectorIdx;
	IMG_UINT32	uLastChanToLoad;

	/*
		Check the index of the last dword referenced from the vector.
	*/
	for (iVectorIdx = (IMG_INT32)(uDwordsPerVector - 1); iVectorIdx >= 0; iVectorIdx--)
	{
		if ((psInst->auLiveChansInDest[0] & auChansPerDword[iVectorIdx]) != 0)
		{
			break;
		}
	}
	ASSERT(iVectorIdx >= 0);
	uLastChanToLoad = (IMG_UINT32)iVectorIdx;

	/*
		Generate a load from each dword in the vector.
	*/
	for (uVectorIdx = 0; uVectorIdx < uDwordsPerVector; uVectorIdx++)
	{
		IMG_UINT32	uLiveChansInComp; 

		uLiveChansInComp = psInst->auLiveChansInDest[0] & auChansPerDword[uVectorIdx];
		if (uLiveChansInComp != 0)
		{
			IMG_BOOL	bLastInExpansion;
			ARG			sScalarArg;

			/*
				Allocate a new register for the result of the memory load.
			*/
			MakeNewTempArg(psState, eDestFmt, &sScalarArg);

			/*
				Set up the source argument to the caller's instruction.
			*/
			SetSrc(psState, 
				   psDestInst, 
				   uDestInstSrcStart + uVectorIdx, 
				   USEASM_REGTYPE_TEMP, 
				   sScalarArg.uNumber, 
				   sScalarArg.eFmt);

			bLastInExpansion = IMG_FALSE;
			if (uVectorIdx == uLastChanToLoad)
			{
				bLastInExpansion = IMG_TRUE;
			}

			/*
				Generate a memory load for the dword.
			*/
			UpdateMemoryLoad(psState, 
						     psCPState, 
							 psInst,
							 &sScalarArg, 
							 uVectorIdx * uChansPerDword /* uExtraStaticOffsetInChans */,
							 uVectorIdx * LONG_SIZE,
							 bLastInExpansion,
							 eDestFmt,
							 GetLiveChansInArg(psState, psDestInst, uDestInstSrcStart + uVectorIdx));
		}
		else
		{
			/*
				Flag the source argument to the caller's instruction as unused.
			*/
			SetSrc(psState, 
				   psDestInst, 
				   uDestInstSrcStart + uVectorIdx, 
				   USC_REGTYPE_UNUSEDSOURCE, 
				   USC_UNDEF, 
				   eDestFmt);
		}
	}
}

static
IMG_VOID ExpandMemoryLoad(PINTERMEDIATE_STATE	psState,
						  PCONST_PACK_STATE		psCPState,
						  PCODEBLOCK			psBlock,
						  PINST					psInst)
/*********************************************************************************
 Function			: ExpandMemoryLoad

 Description		: Convert a LOADCONST instruction to a sequence of LOADMEMCONST
					  instructions.
 
 Parameters			: psState				- Current compiler state.
					  psCPState				- Constant packing module state.
					  psBlock				- Flow control block containing the
											instruction to expand.
					  psInst				- Instruction to expand.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	if (psInst->u.psLoadConst->eFormat == UNIFLEX_CONST_FORMAT_C10)
	{
		PINST		psMkVecInst;	
		static const IMG_UINT32	auC10ChansPerDword_NonVec[] =
		{
			USC_RGB_CHAN_MASK,
			USC_ALPHA_CHAN_MASK,
		};
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		static const IMG_UINT32	auC10ChansPerDword_Vec[] =
		{
			USC_ALL_CHAN_MASK,
			USC_ALPHA_CHAN_MASK,
		};
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		IMG_UINT32 const*	auChansPerDword;

		ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);

		/*
			Create a instruction to combine the two 32-bit value loaded from memory into
			a single 40-bit register.
		*/
		psMkVecInst = AllocateInst(psState, psInst);
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			SetOpcode(psState, psMkVecInst, IMKC10VEC);
			auChansPerDword = auC10ChansPerDword_Vec;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetOpcode(psState, psMkVecInst, ICOMBC10);
			
			auChansPerDword = auC10ChansPerDword_NonVec;
		}

		MoveDest(psState, psMkVecInst, 0 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);
		psMkVecInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

		/*
			Load the two parts of the C10 vector.
		*/
		ExpandMultiMemoryLoad(psState,
							  psCPState,
							  psMkVecInst,
							  0 /* uDestInstSrcStart */,
							  psInst,
							  SCALAR_REGISTERS_PER_C10_VECTOR /* uDwordsPerVector */,
							  C10_CHANNELS_PER_SCALAR_REGISTER /* uChansPerDword */,
							  auChansPerDword,
							  UF_REGFORMAT_C10);
		InsertInstBefore(psState, psBlock, psMkVecInst, psInst);
	}
	else
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->u.psLoadConst->bVectorResult)
	{
		UNIFLEX_CONST_FORMAT	eFormat = psInst->u.psLoadConst->eFormat;
		static const IMG_UINT32	auF32ChansPerDword[] =
		{
			USC_X_CHAN_MASK,
			USC_Y_CHAN_MASK,
			USC_Z_CHAN_MASK,
			USC_W_CHAN_MASK,
		};
		static const IMG_UINT32	auF16ChansPerDword[] =
		{
			USC_XY_CHAN_MASK,
			USC_ZW_CHAN_MASK,
		};
		
		IMG_UINT32 const*		auChansPerDword;
		IMG_UINT32				uDwordsPerVector;
		IMG_UINT32				uChansPerDword;
		PINST					psMkVecInst;
		UF_REGFORMAT			eDestFmt;

		ASSERT(eFormat == UNIFLEX_CONST_FORMAT_F16 || eFormat == UNIFLEX_CONST_FORMAT_F32);

		/*
			Create an instruction to combine the scalar values loaded from memory into
			a vector.
		*/
		psMkVecInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMkVecInst, IVMOV);

		psMkVecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psMkVecInst->u.psVec->aeSrcFmt[0] = psInst->asDest[0].eFmt;

		MoveDest(psState, psMkVecInst, 0 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);
		psMkVecInst->auDestMask[0] = psInst->auLiveChansInDest[0];
		psMkVecInst->auLiveChansInDest[0] = psInst->auLiveChansInDest[0];

		switch (eFormat)
		{
			case UNIFLEX_CONST_FORMAT_F16:	
			{
				uDwordsPerVector = SCALAR_REGISTERS_PER_F16_VECTOR; 
				uChansPerDword = F16_CHANNELS_PER_SCALAR_REGISTER; 
				auChansPerDword = auF16ChansPerDword; 
				eDestFmt = UF_REGFORMAT_F16;
				break;
			}
			case UNIFLEX_CONST_FORMAT_F32:	
			{
				uDwordsPerVector = SCALAR_REGISTERS_PER_F32_VECTOR; 
				uChansPerDword = F32_CHANNELS_PER_SCALAR_REGISTER;
				auChansPerDword = auF32ChansPerDword; 
				eDestFmt = UF_REGFORMAT_F32;
				break;
			}
			default: imgabort();
		}

		psMkVecInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;

		/*
			Load each 32-bit register from memory.
		*/
		ExpandMultiMemoryLoad(psState,
							  psCPState,
							  psMkVecInst,
							  1 /* uDestInstSrcStart */,
							  psInst,
							  uDwordsPerVector,
							  uChansPerDword,
							  auChansPerDword,
							  eDestFmt);

		InsertInstBefore(psState, psBlock, psMkVecInst, psInst);
		AppendToList(&psCPState->sMOVList, &psMkVecInst->sAvailableListEntry);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		/*
			Try replacing a memory load by a LIMM instruction for a static constant.
		*/
		if (ConvertToLIMM(psState, psInst))
		{
			return;
		}

		UpdateMemoryLoad(psState,
						 psCPState,
						 psInst,
						 NULL /* psDest */,
						 0 /* uExtraOffsetInDwords */,
						 0 /* uExtraOffsetInBytes */,
						 IMG_TRUE /* bLastInExpansion */,
						 psInst->asDest[0].eFmt,
						 USC_ALL_CHAN_MASK);
	}

	/*
		Drop the ILOADCONST instruction.
	*/
	RemoveInst(psState, psBlock, psInst);
	FreeInst(psState, psInst);
}

IMG_INTERNAL
IMG_VOID CalculateHardwareRegisterLimits(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: CalculateHardwareRegisterLimits

 Description		: Set up information about the ranges of registers used for shader
					  inputs and outputs. 
 
 Parameters			: psState				- Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		/* psState->sHWRegs.uNumPrimaryAttributes is already set up by RemoveUnusedPixelShaderInputs */

		if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_OUTPUT)
		{
			psState->sHWRegs.uNumOutputRegisters = psPS->uColOutputCount;
		}
		else
		{
			psState->sHWRegs.uNumOutputRegisters = 0;
		}
	}
	else if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		IMG_UINT32			uPrimAttr;
#ifdef NON_F32_VERTEX_INPUT
		IMG_UINT32			vertexInputIndex;
		IMG_UINT32			regIndex;
#else
		PFIXED_REG_DATA		psVSInput;
#endif
		IMG_UINT32			uPARegistersUsed;
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

		/*
			Record the maximum primary attribute used
		*/
		psVS->auVSInputPARegUsage = 
			UscAlloc(psState, UINTS_TO_SPAN_BITS(EURASIA_USE_PRIMATTR_BANK_SIZE) * sizeof(IMG_UINT32));

		memset(psVS->auVSInputPARegUsage, 
			   0, 
			   UINTS_TO_SPAN_BITS(EURASIA_USE_PRIMATTR_BANK_SIZE) * sizeof(IMG_UINT32));

		uPARegistersUsed = 0;
#ifdef NON_F32_VERTEX_INPUT
		/*
			We have an array of input vertex register, loop over
			and calculate actual PA usage
		*/
		for (vertexInputIndex = 0; 
			vertexInputIndex < psVS->uVertexShaderInputCount;
			++vertexInputIndex)
		{
			PFIXED_REG_DATA	psVertexInputFixedReg = psVS->aVertexShaderInputsFixedRegs[vertexInputIndex];
			
			/* Must be a primary attribute */
			ASSERT(psVertexInputFixedReg->sPReg.uType == USEASM_REGTYPE_PRIMATTR);
			
			/* Must have been lowered to scalar form */
			ASSERT(psVertexInputFixedReg->bVector == IMG_FALSE);

			/* loop over registers */
			for (regIndex = 0; 
				regIndex < psVertexInputFixedReg->uConsecutiveRegsCount; 
				regIndex++)
			{
				uPrimAttr = psVertexInputFixedReg->sPReg.uNumber + regIndex;

				if (GetRegMask(psVertexInputFixedReg->puUsedChans, regIndex) != 0)
				{
					/* keep a record of the max number of PAs used */
					uPARegistersUsed = uPrimAttr + 1 > uPARegistersUsed ? 
						uPrimAttr + 1 : uPARegistersUsed;

					/* mark used PA registers */
					SetBit(psVS->auVSInputPARegUsage, uPrimAttr, 0);
					psVS->auVSInputPARegUsage[uPrimAttr >> 5] |= 1 << (uPrimAttr & 0x1F);
				}
			}
		}

		/* loop over registers again and drop reference for trailing unused registers*/
		for (vertexInputIndex = 0; 
			vertexInputIndex < psVS->uVertexShaderInputCount;
			++vertexInputIndex)
		{
			IMG_UINT32 registerCount = 0;
			PFIXED_REG_DATA	psVertexInputFixedReg = psVS->aVertexShaderInputsFixedRegs[vertexInputIndex];

			for (regIndex = 0; 
				regIndex < psVertexInputFixedReg->uConsecutiveRegsCount; 
				regIndex++)
			{
				uPrimAttr = psVertexInputFixedReg->sPReg.uNumber + regIndex;
				
				/* If this PA is past the register limit than drop its reference */
				if (uPrimAttr >= uPARegistersUsed)
				{
					if (psVertexInputFixedReg->uRegArrayIdx == USC_UNDEF)
					{
						UseDefDropFixedRegDef(psState, psVertexInputFixedReg, regIndex);
					}
				}
				else
				{
					registerCount++;
				}
			}

			/* Resize the array associated to this register if needed */
			if (psVertexInputFixedReg->uRegArrayIdx != USC_UNDEF)
			{
				PUSC_VEC_ARRAY_REG	psVSInputArray;

				ASSERT(psVertexInputFixedReg->uRegArrayIdx < psState->uNumVecArrayRegs);
				psVSInputArray = psState->apsVecArrayReg[psVertexInputFixedReg->uRegArrayIdx];

				ASSERT(psVSInputArray->uRegs == psVertexInputFixedReg->uConsecutiveRegsCount);
				psVSInputArray->uRegs = registerCount;
			}

			/* Resize the register count on this FIXED_REG if any register has been dropped*/
			psVertexInputFixedReg->uConsecutiveRegsCount = registerCount;
		}

		/* Set the total number of consecutive PAs used */
		psVS->uVSInputPARegCount = uPARegistersUsed;
#else
		psVSInput = psVS->psVertexShaderInputsFixedReg;
		ASSERT(psVSInput->sPReg.uType == USEASM_REGTYPE_PRIMATTR);
		ASSERT(psVSInput->uConsecutiveRegsCount == EURASIA_USE_PRIMATTR_BANK_SIZE); 
		for (uPrimAttr = 0; uPrimAttr < EURASIA_USE_PRIMATTR_BANK_SIZE; uPrimAttr++)
		{
			if (GetRegMask(psVSInput->puUsedChans, uPrimAttr) != 0)
			{
				uPARegistersUsed = uPrimAttr + 1;

				SetBit(psVS->auVSInputPARegUsage, uPrimAttr, 0);
				psVS->auVSInputPARegUsage[uPrimAttr >> 5] |= ((unsigned)1) << (uPrimAttr & 0x1F);
			}
		}
		psVS->uVSInputPARegCount = uPARegistersUsed;
		if (psVSInput->uRegArrayIdx == USC_UNDEF)
		{
			for (uPrimAttr = uPARegistersUsed; uPrimAttr < psVSInput->uConsecutiveRegsCount; uPrimAttr++)
			{
				UseDefDropFixedRegDef(psState, psVSInput, uPrimAttr);
			}
		}
		else
		{
			PUSC_VEC_ARRAY_REG	psVSInputArray;

			ASSERT(psVSInput->uRegArrayIdx < psState->uNumVecArrayRegs);
			psVSInputArray = psState->apsVecArrayReg[psVSInput->uRegArrayIdx];

			ASSERT(psVSInputArray->uRegs == psVSInput->uConsecutiveRegsCount);
			psVSInputArray->uRegs = uPARegistersUsed;
		}
		psVSInput->uConsecutiveRegsCount = uPARegistersUsed;
#endif

		/*
			Account for VS outputs being redirected to PA registers.
		*/
		if (psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS)
		{
			uPARegistersUsed = max(uPARegistersUsed, psVS->uVertexShaderNumOutputs);
			psState->sHWRegs.uNumOutputRegisters = 0;
		}
		else
		{
			psState->sHWRegs.uNumOutputRegisters = psVS->uVertexShaderNumOutputs;
		}

		psState->sHWRegs.uNumPrimaryAttributes = uPARegistersUsed;
	}
	else if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		/*
			For geometry shaders the primary attributes are shared between
			multiple instances so we can never reuse them to hold temporary
			data.
		*/
		psState->sHWRegs.uNumPrimaryAttributes = 0;

		psState->sHWRegs.uNumOutputRegisters = psState->sShader.psVS->uVertexShaderNumOutputs;
	}
}

static IMG_VOID ReplaceRTAIdx(PINTERMEDIATE_STATE	psState, 
							  PCONST_PACK_STATE		psCPState)
/*********************************************************************************
 Function			: ReplaceRTAIdx

 Description		: Replace LOADRTAIDX instructions by moves from the secondary attribute
					  containing the render target array index.
 
 Parameters			: psState		- Current compiler state.
					  psCPState		- Module state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, ILOADRTAIDX, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST		psInst = InstListIteratorCurrent(&sIter);
		IMG_UINT32	uRTAIdxType;
		IMG_UINT32	uRTAIdxNumber;

		ASSERT(psInst->eOpcode == ILOADRTAIDX);

		/*
			Allocate a secondary attribute and ask the driver to put the render target array index into
			it.
		*/
		if (psState->sSAProg.psSAForRTAIdx == NULL)
		{
			AddSimpleSecAttr(psState,
							 0 /* uAlignment */,
							 USC_UNDEF /* uNum */,
							 UNIFLEX_CONST_FORMAT_RTA_IDX, 
							 USC_UNDEF /* uBuffer */,
							 &psState->sSAProg.psSAForRTAIdx);					
		}

		/*
			Convert the LOADRTAIDX instruction to a MOV.
		*/
		SetOpcode(psState, psInst, IMOV);
		GetDriverLoadedConstantLocation(psState,
										psState->sSAProg.psSAForRTAIdx,
										&uRTAIdxType,
										&uRTAIdxNumber);
		SetSrc(psState, psInst, 0 /* uSrcIdx */, uRTAIdxType, uRTAIdxNumber, UF_REGFORMAT_F32);

		/*
			Add to the list of MOVs. We'll try and replace the destination by the
			source at the end of the stage.
		*/
		AppendToList(&psCPState->sMOVList, &psInst->sAvailableListEntry);
	}
	InstListIteratorFinalise(&sIter);
}

typedef struct _LOADCONST_DATA
{
	/*
		Points to the LOADCONST instruction.
	*/
	PINST				psInst;
	
	/*
		Entry either in CONST_PACK_STATE.sCostSortedStaticConstList or in 
		LOADCONST_DATA.sDuplicateList for another LOADCONST.
	*/
	USC_LIST_ENTRY		sListEntry;

	/*
		If this LOADCONST loads a constant from a range then the entry
		in the list of LOADCONST instructions associated with the range.
	*/
	USC_LIST_ENTRY		sRangeListEntry;

	/*	
		List of any other LOADCONST instructions loading the same
		constant.
	*/
	USC_LIST			sDuplicateList;

	/*
		Mask of dwords used from all loads fo this constant.
	*/
	IMG_UINT32			uUsedDwordMask;

	/*
		Mask of vector channels used from all loads of this constant.
	*/
	IMG_UINT32			uUsedChanMask;

	/*
		Estimates number of cycles saved by moving the constant number
		loaded by this instruction into secondary attributes.
	*/
	IMG_UINT32			uCost;

	/*
		Number of secondary attributes required for the constant data
		loaded by this instruction.
	*/
	IMG_UINT32			uNumSAsRequired;
} LOADCONST_DATA, *PLOADCONST_DATA;

static IMG_BOOL AddToStaticConstLoadList(PCONST_PACK_STATE		psCPState,
										 PLOADCONST_DATA		psLCDataToAdd)
/*********************************************************************************
 Function			: AddToStaticConstLoadList

 Description		: Add a load of a statically addressed constant to the overall
					  list. If another load of the same constant is already in the
					  list then merge the two.
 
 Parameters			: psState			- Current compiler state.
					  psLCDataToAdd		- Load to add.

 Globals Effected	: None

 Return				: TRUE if this was the first load of the constant.
*********************************************************************************/
{
	PUSC_LIST_ENTRY		psListEntry;
	IMG_UINT32			uConstNumToAdd = psLCDataToAdd->psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;
	PLOADCONST_PARAMS	psLCParamsToAdd = psLCDataToAdd->psInst->u.psLoadConst;

	for (psListEntry = psCPState->sCostSortedStaticConstList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PLOADCONST_DATA		psLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sListEntry);
		PLOADCONST_PARAMS	psLCParams = psLCData->psInst->u.psLoadConst;
		IMG_UINT32			uConstNum = psLCData->psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

		/*
			Keep the list sorted by the address of the source constant.
		*/
		if (psLCParams->uBufferIdx > psLCParamsToAdd->uBufferIdx)
		{
			break;
		}
		if (psLCParams->uBufferIdx == psLCParamsToAdd->uBufferIdx && uConstNum > uConstNumToAdd)
		{
			break;
		}

		/*
			Check for a load of the same constant.
		*/
		if (psLCParams->uBufferIdx == psLCParamsToAdd->uBufferIdx &&
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psLCParams->bVectorResult == psLCParamsToAdd->bVectorResult &&
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			uConstNum == uConstNumToAdd)
		{
			AppendToList(&psLCData->sDuplicateList, &psLCDataToAdd->sListEntry);
			psLCData->uUsedDwordMask |= psLCDataToAdd->uUsedDwordMask;
			psLCData->uUsedChanMask |= psLCDataToAdd->uUsedChanMask;
			psLCData->uNumSAsRequired = g_auSetBitCount[psLCData->uUsedDwordMask];
			psLCData->uCost += psLCDataToAdd->uCost;
			return IMG_FALSE;
		}
	}

	if (psListEntry == NULL)
	{
		AppendToList(&psCPState->sCostSortedStaticConstList, &psLCDataToAdd->sListEntry);
	}
	else
	{
		InsertInList(&psCPState->sCostSortedStaticConstList, &psLCDataToAdd->sListEntry, psListEntry);
	}

	return IMG_TRUE;
}

static IMG_VOID ReplaceLoadConst(PINTERMEDIATE_STATE	psState,
								 PCONST_PACK_STATE		psCPState,
								 PINST					psInst,
								 IMG_UINT32				uConstantVarNum,
								 IMG_BOOL				bPartOfArray)
/*********************************************************************************
 Function			: ReplaceLoadConst

 Description		: Replace a LOADCONST instruction by a MOV instructions sourcing 
					  a secondary attribute containing the constant.
 
 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.
					  psInst			- LOADCONST instruction to replace.
					  uConstantVarNum	- Secondary attribute containing the
										constant.
					  bPartOfArray		- TRUE if the constant is part of a range.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uIndexType;
	IMG_UINT32	uIndexNumber;
	IMG_UINT32	uIndexArrayOffset;
	IMG_UINT32	uArrayOffsetInDwords;
	IMG_UINT32	uIndexRelativeStrideInBytes;
	PARG		psMovSrc;

	ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);

	ASSERT(psInst->u.psLoadConst->uC10CompOffset == 0);

	if (bPartOfArray)
	{
		IMG_UINT32		uConstNum;
		PCONSTANT_RANGE	psRange;
		IMG_UINT32		uArrayOffsetInComponents;

		psRange = psInst->u.psLoadConst->psRange;
		ASSERT(psRange != NULL);

		/*
			Extract the source offset into the constant buffer.
		*/
		ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
		uConstNum = psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

		/*
			Convert to a dword offset from the start of the range.
		*/
		if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0)
		{
			uArrayOffsetInComponents = uConstNum - psRange->uRangeStart;
			
			switch (psInst->u.psLoadConst->eFormat)
			{
				case UNIFLEX_CONST_FORMAT_F32: 
				{
					uArrayOffsetInDwords = uArrayOffsetInComponents; 
					break;
				}
				case UNIFLEX_CONST_FORMAT_F16: 
				{
					ASSERT((uArrayOffsetInComponents % 2) == 0);
					uArrayOffsetInDwords = uArrayOffsetInComponents / 2; 
					break;
				}
				case UNIFLEX_CONST_FORMAT_C10: 
				{
					ASSERT((uArrayOffsetInComponents % VECTOR_LENGTH) == 0);
				
					/*
						64-bits per C10 vector.
					*/
					uArrayOffsetInDwords = (uArrayOffsetInComponents / VECTOR_LENGTH) * (QWORD_SIZE / LONG_SIZE);
					break;
				}	
				default: imgabort();
			}
		}
		else
		{
			IMG_UINT32	uArrayOffsetInBytes;

			uArrayOffsetInBytes = uConstNum - psRange->uRangeStart;
			ASSERT((uArrayOffsetInBytes % LONG_SIZE) == 0);
			uArrayOffsetInDwords = uArrayOffsetInBytes / LONG_SIZE;
		}
	}
	else
	{
		uArrayOffsetInDwords = USC_UNDEF;
	}

	if (!psInst->u.psLoadConst->bRelativeAddress)
	{
		uIndexType = USC_REGTYPE_NOINDEX;
		uIndexNumber = USC_UNDEF;
		uIndexArrayOffset = USC_UNDEF;
		uIndexRelativeStrideInBytes = USC_UNDEF;
	}
	else
	{
		PARG		psDynOff;

		ASSERT(bPartOfArray);

		/*
			Get the temporary register to use for indexing.
		*/
		psDynOff = &psInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX];
		ASSERT(psDynOff->uType == USEASM_REGTYPE_TEMP);
		uIndexType = USEASM_REGTYPE_TEMP;
		uIndexNumber = psDynOff->uNumber;
		uIndexArrayOffset = psDynOff->uArrayOffset;
		uIndexRelativeStrideInBytes = psInst->u.psLoadConst->uRelativeStrideInBytes;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
			psInst->u.psLoadConst->bVectorResult &&
			(
				psInst->u.psLoadConst->eFormat == UNIFLEX_CONST_FORMAT_F32 ||
				psInst->u.psLoadConst->eFormat == UNIFLEX_CONST_FORMAT_F16
			)
	   )
	{
		IMG_UINT32	uArg;

		SetOpcode(psState, psInst, IVMOV);
		psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psInst->u.psVec->aeSrcFmt[0] = psInst->asDest[0].eFmt;
		for (uArg = 0; uArg < MAX_SCALAR_REGISTERS_PER_VECTOR; uArg++)
		{
			SetSrcUnused(psState, psInst, 1 + uArg);
		}

		/*
			Add to the list of MOVs. We'll try and replace the destination by the
			source at the end of the stage.
		*/
		AppendToList(&psCPState->sMOVList, &psInst->sAvailableListEntry);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psInst, IMOV);

		/*
			Add to the list of MOVs. We'll try and replace the destination by the
			source at the end of the stage.
		*/
		AppendToList(&psCPState->sMOVList, &psInst->sAvailableListEntry);
	}

	psMovSrc = &psInst->asArg[0];
	InitInstArg(psMovSrc);
	if (bPartOfArray)
	{
		SetArraySrc(psState, 
					psInst, 
					0 /* uSrcIdx */, 
					uConstantVarNum, 
					uArrayOffsetInDwords, 
					psInst->asDest[0].eFmt);
		if (uIndexType != USC_REGTYPE_NOINDEX)
		{
			SetSrcIndex(psState, 
						psInst, 
						0 /* uSrcIdx */, 
						uIndexType, 
						uIndexNumber, 
						uIndexArrayOffset, 
						uIndexRelativeStrideInBytes);
		}
	}
	else
	{
		SetSrc(psState, psInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uConstantVarNum, psInst->asDest[0].eFmt);
	}

	if (bPartOfArray && psInst->asDest[0].eFmt == UF_REGFORMAT_C10)
	{
		PINST		psAlphaMoveInst;
		PINST		psCOMBC10Inst;
		IMG_UINT32	uRGBTempNum = GetNextRegister(psState);
		IMG_UINT32	uAlphaTempNum = GetNextRegister(psState);

		psAlphaMoveInst = CopyInst(psState, psInst);

		psAlphaMoveInst->asArg[0].uArrayOffset++;

		InsertInstBefore(psState, psInst->psBlock, psAlphaMoveInst, psInst->psNext);

		psCOMBC10Inst = AllocateInst(psState, psInst);
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			SetOpcode(psState, psCOMBC10Inst, IMKC10VEC);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetOpcode(psState, psCOMBC10Inst, ICOMBC10);
		}

		MoveDest(psState, psCOMBC10Inst, 0 /* uCopyToIdx */, psInst, 0 /* uCopyFromIdx */);

		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uRGBTempNum, UF_REGFORMAT_C10);

		SetDest(psState, psAlphaMoveInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uAlphaTempNum, UF_REGFORMAT_C10);

		SetSrc(psState, psCOMBC10Inst, COMBC10_RGB_SOURCE, USEASM_REGTYPE_TEMP, uRGBTempNum, UF_REGFORMAT_C10);
		SetSrc(psState, psCOMBC10Inst, COMBC10_ALPHA_SOURCE, USEASM_REGTYPE_TEMP, uAlphaTempNum, UF_REGFORMAT_C10);

		InsertInstBefore(psState, psInst->psBlock, psCOMBC10Inst, psAlphaMoveInst->psNext);
	}
}

static IMG_VOID ReplaceLoadConsts(PINTERMEDIATE_STATE	psState,
								  PCONST_PACK_STATE		psCPState,
								  PLOADCONST_DATA		psLCData,
								  IMG_UINT32			uConstantVarNum,
								  IMG_BOOL				bPartOfArray)
/*********************************************************************************
 Function			: ReplaceLoadConsts

 Description		: Replace LOADCONST instructions all loading the same constant
					  by MOV instructions sourcing a secondary attribute containing
					  the constant.
 
 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.
					  psLCData			- LOADCONST instruction to replace.
					  uConstantVarNum	- Secondary attribute containing the
										constant.
					  bPartOfArray		- TRUE if the secondary attribute is part
										of a dynamically indexable array.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/* Replace this LOADCONST instruction. */
	ReplaceLoadConst(psState, psCPState, psLCData->psInst, uConstantVarNum, bPartOfArray);

	/*
		Replace any extra LOADCONST instructions loading the same constant.
	*/
	for (psListEntry = psLCData->sDuplicateList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_DATA		psDupLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sListEntry);

		psNextListEntry = psListEntry->psNext;

		ReplaceLoadConst(psState, psCPState, psDupLCData->psInst, uConstantVarNum, bPartOfArray);

		UscFree(psState, psDupLCData);
	}
}

static IMG_VOID ReplaceStaticAddressLoadConsts(PINTERMEDIATE_STATE	psState,
											   PCONST_PACK_STATE	psCPState,
											   PLOADCONST_DATA		psLCData)
/*********************************************************************************
 Function			: ReplaceStaticAddressLoadConsts

 Description		: Replace a LOADCONST instruction loading a statically
					  addressed constant by a MOV from a secondary
					  attribute containing the constant.
 
 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.
					  psLCData			- LOADCONST instruction to replace.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32				uConstNum;
	PINST					psInst = psLCData->psInst;
	IMG_UINT32				uBaseRegNum;

	/*
		Extract the source offset into the constant buffer.
	*/
	ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
	uConstNum = psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

	AddNewInRegisterConstant(psState,
							 uConstNum,
							 psInst->u.psLoadConst->eFormat,
							 psInst->u.psLoadConst->uBufferIdx,
							 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							 psInst->u.psLoadConst->bVectorResult,
							 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							 0 /* uAlignment */,
							 psLCData->uUsedDwordMask,
							 psLCData->uUsedChanMask,
							 USC_UNDEF /* uResultRegNum */,
							 NULL /* psDriverConstRange */,
							 NULL /* ppsConst */,
							 &uBaseRegNum);

	/*
		Replace LOADCONST instructions referencing this constant by MOV instruction sourcing 
		the secondary attribute.
	*/
	ReplaceLoadConsts(psState, psCPState, psLCData, uBaseRegNum, IMG_FALSE /* bPartOfArray */);

	/*
		Free our data about this constant.
	*/
	UscFree(psState, psLCData);
}

static IMG_VOID ReplaceDynamicAddressLoadConsts(PINTERMEDIATE_STATE	psState,
												PCONST_PACK_STATE	psCPState,
												PCONSTANT_RANGE		psRange,
												PLOADCONST_DATA*	ppsBestStaticAddress)
/*********************************************************************************
 Function			: ReplaceDynamicAddressLoadConsts

 Description		: Replace all LOADCONST instructions loading from a constant
					  range by MOV instructions from secondary attributes containing
					  the constants in the range.
 
 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.
					  psRange			- Range of constants which are in
										secondary attributes.
					  ppsBestStaticAddress
										- If this LOADCONST instruction references
										a constant in the range then set it to
										NULL.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY				psListEntry;
	IMG_UINT32					uConstIdx;
	PCONSTANT_INREGISTER_RANGE	psInRegRange;
	IMG_UINT32					uUsedSACount;
	PUSC_LIST_ENTRY				psNextListEntry;
	IMG_UINT32					uRangeStep;
	IMG_UINT32					uUsedDwordMask;
	PUSC_VEC_ARRAY_REG			psArray;
	IMG_UINT32					uArrayLength;
	IMG_UINT32					uEntryFlag;
	IMG_UINT32					uChannelsPerDword;

	/*
		Allocate a new structure to represent the range of constants moved to secondary attributes.
	*/
	psInRegRange = UscAlloc(psState, sizeof(*psInRegRange));

	psInRegRange->eFormat = psRange->eFormat;
	InitializeList(&psInRegRange->sResultList);
	psInRegRange->uSACount = psRange->psData->uRangeSizeInDwords;


	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psRange->bVector)
	{
		IMG_UINT32	uChansPerDword;

		uRangeStep = VECTOR_LENGTH;
		uChansPerDword = g_auChansPerDword[psRange->eFormat];
		uUsedDwordMask = (1U << (VECTOR_LENGTH / uChansPerDword)) - 1;

		if (psRange->eFormat == UNIFLEX_CONST_FORMAT_F32)
		{
			uChannelsPerDword = F32_CHANNELS_PER_SCALAR_REGISTER;
		}
		else
		{
			ASSERT(psRange->eFormat == UNIFLEX_CONST_FORMAT_F16);
			uChannelsPerDword = F16_CHANNELS_PER_SCALAR_REGISTER;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		uRangeStep = psRange->psData->uRangeStep;
		uUsedDwordMask = USC_X_CHAN_MASK;
		uChannelsPerDword = LONG_SIZE;
	}

	/*
		Add an entry to the list of dynamically indexed variables.
	*/
	uArrayLength = (psRange->uRangeEnd - psRange->uRangeStart + 1) / uRangeStep;
	if (psRange->psData->b40Bit && !psRange->bC10SubcomponentIndex)
	{
		uArrayLength *= 2;
	}
	psInRegRange->uRegArrayIdx = 
		AddNewRegisterArray(psState, 
							ARRAY_TYPE_DRIVER_LOADED_SECATTR, 
							USC_UNDEF, 
							uChannelsPerDword,
							uArrayLength);
	AppendToList(&psState->sSAProg.sInRegisterConstantRangeList, &psInRegRange->sListEntry);

	/*
		Generate secondary attributes with one C10 channel per word for ranges used with C10
		subcomponent indexing.
	*/
	uEntryFlag = 0;
	if (psRange->bC10SubcomponentIndex)
	{
		uEntryFlag = REMAPPED_CONSTANT_UNPACKEDC10;
	}

	/*
		Associate the indexed variable with the constant range.
	*/
	psArray = psState->apsVecArrayReg[psInRegRange->uRegArrayIdx];
	psArray->u.psConstRange = psInRegRange;

	/*
		Add entries to the table of secondary attributes to be loaded by the driver for each
		dword of constant data in the range.
	*/
	uUsedSACount = 0;
	for (uConstIdx = psRange->uRangeStart; uConstIdx <= psRange->uRangeEnd; uConstIdx += uRangeStep)
	{
		AddNewInRegisterConstant(psState, 
								 uConstIdx | uEntryFlag,
								 psRange->eFormat,
								 psRange->uBufferIdx,
								 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								 psRange->bVector,
								 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								 0 /* uAlignment */,
								 uUsedDwordMask,
								 USC_ALL_CHAN_MASK,
								 psArray->uBaseReg + uUsedSACount,
								 psInRegRange,
								 NULL /* ppsConst */,
								 NULL /* puRegNum */);
		uUsedSACount++;

		/*
			For a C10 range where all 40-bits are used add a second entry for
			the ALPHA channel.
		*/
		if (psRange->psData->b40Bit && !psRange->bC10SubcomponentIndex)
		{
			AddNewInRegisterConstant(psState, 
									 (uConstIdx | uEntryFlag) + 3,
									 psRange->eFormat,
									 psRange->uBufferIdx,
									 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									 psRange->bVector,
									 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									 0 /* uAlignment */,
									 uUsedDwordMask,
									 USC_ALL_CHAN_MASK,
									 psArray->uBaseReg + uUsedSACount,
									 psInRegRange,
									 NULL /* ppsConst */,
									 NULL /* puRegNum */);
			uUsedSACount++;
		}
	}
	ASSERT(uArrayLength == uUsedSACount);

	/*
		Replace all static and dynamic references to the range by uses of secondary attributes.
	*/
	for (psListEntry = psRange->psData->sUsesList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_DATA	psLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sRangeListEntry);

		psNextListEntry = psListEntry->psNext;

		if (!psLCData->psInst->u.psLoadConst->bRelativeAddress)
		{
			if ((*ppsBestStaticAddress) == psLCData)
			{
				/*
					If this LOADCONST is currently the best candidate for replacing a static constant load then
					clear the reference to it.
				*/
				(*ppsBestStaticAddress) = NULL;
			}
			else
			{
				/*
					Otherwise remove the LOADCONST from the list of candidates for replacing by secondary
					attributes.
				*/
				RemoveFromList(&psCPState->sCostSortedStaticConstList, &psLCData->sListEntry);
			}
		}

		/*
			Replace the LOADCONST instruction by MOV sourcing a secondary attribute.
		*/
		ReplaceLoadConsts(psState, 
						  psCPState, 
						  psLCData, 
						  psInRegRange->uRegArrayIdx, 
						  IMG_TRUE /* bPartOfArray */);

		UscFree(psState, psLCData);
	}
}

static
IMG_UINT32 ChanMaskToDwordMask(PINTERMEDIATE_STATE psState, IMG_UINT32 uInMask, UNIFLEX_CONST_FORMAT eFormat)
/*****************************************************************************
 FUNCTION	: ChanMaskToDwordMask
    
 PURPOSE	: Generate a mask of dwords from a mask of C10, F16 or F32 channels.

 PARAMETERS	: psState		- Compiler state.
			  uInMask		- Mask of channels.
			  eFormat		- Format of the channels.

 RETURNS	: The generated mask.
*****************************************************************************/
{
	switch (eFormat)
	{
		case UNIFLEX_CONST_FORMAT_C10:
		{
			IMG_UINT32	uUsedDwordMask;

			uUsedDwordMask = 0;
			/*
				The RGB channel are in the first dword.
			*/
			if ((uInMask & USC_RGB_CHAN_MASK) != 0)
			{
				uUsedDwordMask |= USC_X_CHAN_MASK;
			}

			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
			{
				/*
					The ALPHA channel straddles the first and second dwords.
				*/
				if ((uInMask & USC_ALPHA_CHAN_MASK) != 0)
				{
					uUsedDwordMask |= USC_XY_CHAN_MASK;
				}
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				/*
					The ALPHA channel is in the second dword.
				*/
				if ((uInMask & USC_ALPHA_CHAN_MASK) != 0)
				{
					uUsedDwordMask |= USC_Y_CHAN_MASK;
				}
			}
			return uUsedDwordMask;
		}
		case UNIFLEX_CONST_FORMAT_F16:
		{
			IMG_UINT32	uUsedDwordMask;

			/*
				One word per channel.
			*/
			uUsedDwordMask = 0;
			if ((uInMask & USC_XY_CHAN_MASK) != 0)
			{
				uUsedDwordMask |= USC_X_CHAN_MASK;
			}
			if ((uInMask & USC_ZW_CHAN_MASK) != 0)
			{
				uUsedDwordMask |= USC_Y_CHAN_MASK;
			}
			return uUsedDwordMask;
		}
		case UNIFLEX_CONST_FORMAT_F32:
		{
			/*
				One dword per channel.
			*/
			return uInMask;
		}
		default: imgabort();
	}
}

static IMG_VOID SearchConstBufs(PINTERMEDIATE_STATE	psState,
								PCONST_PACK_STATE	psCPState)
/*********************************************************************************
 Function			: SearchConstBufs

 Description		: Set up information about LOADCONST instruction so we can
					  decide which constant data to move to secondary attributes.
 
 Parameters			: psState			- Current compiler state.
					  psCPState			- Module state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;

	InstListIteratorInitialize(psState, ILOADCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST				psInst = InstListIteratorCurrent(&sIter);
		PLOADCONST_PARAMS	psParams = psInst->u.psLoadConst;
		IMG_BOOL			bNeverUseSAs;
		IMG_UINT32			uBufferIdx;
		IMG_UINT32			uConstNum;
		PLOADCONST_DATA		psLCData;
		IMG_BOOL			bNonDuplicateConstant;

		/*
			Extract the source constant buffer number.
		*/
		uBufferIdx = psParams->uBufferIdx;

		/*
			Extract the source offset into the constant buffer.
		*/
		ASSERT(psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
		uConstNum = psInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

		/* Keep track that which constants buffers are used in program */
		ASSERT(uBufferIdx < UF_CONSTBUFFERID_COUNT);
		psState->asConstantBuffer[uBufferIdx].bInUse = IMG_TRUE;

		/*
			Record where the ALPHA channel was used from a range of C10 format constants.
		*/
		if (psParams->psRange != NULL)
		{
			PCONSTANT_RANGE	psRange = psParams->psRange;

			ASSERT(psRange->eFormat == UNIFLEX_CONST_FORMAT_UNDEF || psRange->eFormat == psParams->eFormat);
			psRange->eFormat = psParams->eFormat;

			if (
					psRange->eFormat == UNIFLEX_CONST_FORMAT_C10 &&
					(psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0
			   )
			{
				IMG_BOOL	bAlphaUsed;

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psParams->bVectorResult)
				{
					bAlphaUsed = (psInst->auLiveChansInDest[0] & USC_ALPHA_CHAN_MASK) != 0 ? IMG_TRUE : IMG_FALSE;
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					bAlphaUsed = (uConstNum & 3) == 3 ? IMG_TRUE : IMG_FALSE;
				}

				if (bAlphaUsed)
				{
					psRange->psData->b40Bit = IMG_TRUE;
				}
			}
		}

		/*
			Where the offsets into the constant buffer are in terms of bytes check whether this constant 
			access would still be valid if any range it is part of was moved to secondary attributes.
		*/
		if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0 && psParams->psRange != NULL)
		{
			IMG_UINT32					uRangeStart;

			uRangeStart = psParams->psRange->uRangeStart;

			/*
				Check that both the static offset and the dynamic offset are a 
				multiple of the size of a secondary attribute (32 bits).
			*/
			if (
					((uConstNum - uRangeStart) % LONG_SIZE != 0) || 
					(psInst->u.psLoadConst->uRelativeStrideInBytes % LONG_SIZE) != 0
			   )
			{
				psParams->psRange->psData->bInvalidForSecAttrs = IMG_TRUE;
			}
		}

		/*
			Check for loading an entire C10 vector from an unaligned address. Because of the limitation of
			addressing secondary attributes it is much more efficent to load from memory.
		*/
		if (psParams->uC10CompOffset > 0)
		{
			psParams->psRange->psData->bInvalidForSecAttrs = IMG_TRUE;
		}

		bNeverUseSAs = IMG_FALSE;
		if (psParams->bRelativeAddress && psParams->psRange == NULL)
		{
			/*
				If we don't know the range of constants referenced by this LOADCONST
				then it can't be replaced by secondary attributes.
			*/
			bNeverUseSAs = IMG_TRUE;
		}
		if (psState->psSAOffsets->asConstBuffDesc[uBufferIdx].eConstBuffLocation != UF_CONSTBUFFERLOCATION_DONTCARE)
		{
			/*
				The user is telling us not to move any constants in this constant buffer to
				secondary attributes.
			*/
			bNeverUseSAs = IMG_TRUE;
		}

		if (bNeverUseSAs)
		{
			continue;
		}

		psLCData = UscAlloc(psState, sizeof(*psLCData));
		if (
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				psParams->bVectorResult ||
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				psParams->eFormat == UNIFLEX_CONST_FORMAT_C10
		   )
		{
			psLCData->uUsedDwordMask = ChanMaskToDwordMask(psState, psInst->auLiveChansInDest[0], psParams->eFormat);
		}
		else
		{
			psLCData->uUsedDwordMask = 1;
		}

		/*
			If this constant has a known value then the LOADCONST can be replaced by
			a immediate load; otherwise we'll replace it by a load from memory.
		*/
		psLCData->uCost = psInst->u.psLoadConst->bStaticConst ? 1U : 5U;
		psLCData->uCost *= g_auSetBitCount[psLCData->uUsedDwordMask];

		psLCData->uNumSAsRequired = g_auSetBitCount[psLCData->uUsedDwordMask];

		psLCData->uUsedChanMask = psInst->auLiveChansInDest[0];

		psLCData->psInst = psInst;

		InitializeList(&psLCData->sDuplicateList);

		if (psParams->bRelativeAddress)
		{
			bNonDuplicateConstant = IMG_TRUE;
			ClearListEntry(&psLCData->sListEntry);
		}
		else
		{
			/*
				Add to the overall list of LOADCONST instructions.
			*/
			bNonDuplicateConstant = AddToStaticConstLoadList(psCPState, psLCData);
		}

		if (psParams->psRange != NULL)
		{
			/*
				Include the number of cycles saved by replacing this LOADCONST by
				secondary attributes in the overall saving by replacing the
				entire range.
			*/
			psParams->psRange->psData->uCost += psLCData->uCost;

			if (bNonDuplicateConstant)
			{
				/*
					Add to this list of LOADCONST instruction loading constants in this
					range.
				*/
				AppendToList(&psParams->psRange->psData->sUsesList, &psLCData->sRangeListEntry);
			}
		}
		else
		{
			ClearListEntry(&psLCData->sRangeListEntry);
		}
	}
	InstListIteratorFinalise(&sIter);

	#if defined(OUTPUT_USPBIN)
	/*
		Record which samplers are used by the program.
	*/
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		IMG_UINT32				uOpIdx;
		static const IOPCODE	aeUSPSampleOpcodes[] = 
									{ISMP_USP, ISMPBIAS_USP, ISMPREPLACE_USP, ISMPGRAD_USP, ISMP_USP_NDR};

		for (uOpIdx = 0; uOpIdx < (sizeof(aeUSPSampleOpcodes) / sizeof(aeUSPSampleOpcodes[0])); uOpIdx++)
		{
			InstListIteratorInitialize(psState, aeUSPSampleOpcodes[uOpIdx], &sIter);
			for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
			{
				PINST	psInst = InstListIteratorCurrent(&sIter);

				ASSERT(psInst->eOpcode == aeUSPSampleOpcodes[uOpIdx]);
				ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0);
				
				ASSERT(psInst->u.psSmp->uTextureStage < psState->psSAOffsets->uTextureCount);
				SetBit(psCPState->auSamplersUsedMask, psInst->u.psSmp->uTextureStage, 1);
			}
			InstListIteratorFinalise(&sIter);
		}
	}
	#endif /* defined(OUTPUT_USPBIN) */
}

static
IMG_INT32 RangeCostCompare(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*********************************************************************************
 Function			: StaticConstCostCompare

 Description		: Sorting comparison function for dynamically addressed
					  constant ranges.
 
 Parameters			: psListEntry1, psListEntry2	- Ranges to compare.

 Globals Effected	: None

 Return				: ENTRY1 < ENTRY2
*********************************************************************************/
{
	PCONST_PACK_RANGE_STATE	psRange1, psRange2;

	psRange1 = IMG_CONTAINING_RECORD(psListEntry1, PCONST_PACK_RANGE_STATE, sCostListEntry);
	psRange2 = IMG_CONTAINING_RECORD(psListEntry2, PCONST_PACK_RANGE_STATE, sCostListEntry);

	return (IMG_INT32)(psRange2->uCost - psRange1->uCost);
}

static
IMG_INT32 StaticConstCostCompare(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*********************************************************************************
 Function			: StaticConstCostCompare

 Description		: Sorting comparison function for statically addressed
					  constants.
 
 Parameters			: psListEntry1, psListEntry2	- Constants to compare.

 Globals Effected	: None

 Return				: ENTRY1 < ENTRY2
*********************************************************************************/
{
	PLOADCONST_DATA		psLCData1, psLCData2;

	psLCData1 = IMG_CONTAINING_RECORD(psListEntry1, PLOADCONST_DATA, sListEntry);
	psLCData2 = IMG_CONTAINING_RECORD(psListEntry2, PLOADCONST_DATA, sListEntry);

	if (psLCData1->uCost == psLCData2->uCost)
	{
		PINST	psLCInst1 = psLCData1->psInst;
		PINST	psLCInst2 = psLCData2->psInst;

		/*
			Sort by increasing constant buffer index then by increasing offset within the
			buffer.
		*/
		if (psLCInst1->u.psLoadConst->uBufferIdx == psLCInst2->u.psLoadConst->uBufferIdx)
		{
			PARG	psStaticOff1 = &psLCInst1->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX];
			PARG	psStaticOff2 = &psLCInst2->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX];

			return (IMG_INT32)(psStaticOff1->uNumber - psStaticOff2->uNumber);
		}
		else
		{
			return (IMG_INT32)(psLCInst1->u.psLoadConst->uBufferIdx - psLCInst2->u.psLoadConst->uBufferIdx);
		}
	}
	else
	{
		/*
			Sort in decreasing order of the instructions saved by moving to the secondary
			attributes.
		*/
		return (IMG_INT32)(psLCData2->uCost - psLCData1->uCost);
	}
}

static
IMG_VOID FreeLCData(PINTERMEDIATE_STATE	psState, PLOADCONST_DATA psLCData)
/*********************************************************************************
 Function			: FreeLCData

 Description		: Free data associated with a constant which is a candidate
					  for loading into a secondary attribute.
 
 Parameters			: psState		- Compiler state.
					  psLCData		- Structure to free.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/*
		Free the structures associated with other places where this constant is loaded.
	*/
	for (psListEntry = psLCData->sDuplicateList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_DATA		psDupLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sListEntry);

		psNextListEntry = psListEntry->psNext;
		UscFree(psState, psDupLCData);
	}

	UscFree(psState, psLCData);
}

static
IMG_VOID FreeRange(PINTERMEDIATE_STATE psState, PCONST_PACK_RANGE_STATE psRange)
/*********************************************************************************
 Function			: FreeRange

 Description		: Free data associated with a range of constants accessed with
					  dynamic indices which is a candidate for loading into a secondary attribute.
 
 Parameters			: psState		- Compiler state.
					  psRange		- Structure representing the range.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	/*
		Free all the data associated with relative accesses to this range. Static ones might still be
		replaced by secondary attributes so don't free them.
	*/
	for (psListEntry = psRange->sUsesList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_DATA	psLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sRangeListEntry);

		psNextListEntry = psListEntry->psNext;
		if (psLCData->psInst->u.psLoadConst->bRelativeAddress)
		{
			FreeLCData(psState, psLCData);
		}
	}
}

static
IMG_VOID ProcessSAOnlyConstantRange(PINTERMEDIATE_STATE psState, PCONST_PACK_STATE psCPState, PCONSTANT_RANGE psRange)
/*********************************************************************************
 Function			: ProcessSAOnlyConstantRange

 Description		: For a range of dynamically indexed constants in a constant
					  buffer mapped directly to secondary attributes: replace all LOADCONST
					  instructions from constants in the range by moves from registers
					  representing secondary attributes.
 
 Parameters			: psState		- Compiler state.
					  psCPState		- Module state.
					  psRange		- Structure representing the range.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PCONSTANT_BUFFER			psBuffer;
	PUNIFLEX_CONSTBUFFERDESC	psDesc;
	IMG_UINT32					uRegArrayIdx;
	PUSC_VEC_ARRAY_REG			psArray;
	IMG_UINT32					uSAIdx;
	PUSC_LIST_ENTRY				psListEntry;
	PUSC_LIST_ENTRY				psNextListEntry;
	IMG_UINT32					uArrayLengthInRegs;
	IMG_UINT32					uArrayLengthInBytes;
	IMG_UINT32					uRangeStartInRegs;
	IMG_UINT32					uRangeStep;
	IMG_UINT32					uChannelsPerDword;

	psBuffer = &psState->asConstantBuffer[psRange->uBufferIdx];
	psDesc = &psState->psSAOffsets->asConstBuffDesc[psRange->uBufferIdx];

	/*
		Skip ranges from constant buffers not mapped directly to secondary attributes.
	*/
	if (psDesc->eConstBuffLocation != UF_CONSTBUFFERLOCATION_SAS_ONLY)
	{
		return;			
	}

	uArrayLengthInBytes = psRange->uRangeEnd - psRange->uRangeStart + 1;

	ASSERT((uArrayLengthInBytes % LONG_SIZE) == 0);
	uArrayLengthInRegs = uArrayLengthInBytes / LONG_SIZE;

	/*
		Get the start of the secondary attributes containing constants from the range.
	*/
	ASSERT((psRange->uRangeStart % LONG_SIZE) == 0);
	uRangeStartInRegs = psRange->uRangeStart / LONG_SIZE;
	uRangeStartInRegs += psDesc->uStartingSAReg;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For cores with the vector instruction map each element of the array to two or four
		secondary attributes.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 && psRange->bVector)
	{
		if (psRange->eFormat == UF_REGFORMAT_F32)
		{
			uRangeStep = SCALAR_REGISTERS_PER_F32_VECTOR;
			uChannelsPerDword = F32_CHANNELS_PER_SCALAR_REGISTER;
		}
		else
		{
			ASSERT(psRange->eFormat == UF_REGFORMAT_F16);
			uRangeStep = SCALAR_REGISTERS_PER_F16_VECTOR;
			uChannelsPerDword = F16_CHANNELS_PER_SCALAR_REGISTER;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		uRangeStep = 1;
		uChannelsPerDword = LONG_SIZE;
	}

	uArrayLengthInRegs /= uRangeStep;
	
	/*
		Create an array to represent the range of secondary attributes.
	*/
	uRegArrayIdx = AddNewRegisterArray(psState, 
									   ARRAY_TYPE_DIRECT_MAPPED_SECATTR, 
									   USC_UNDEF /* uArrayNum */, 
									   uChannelsPerDword,
									   uArrayLengthInRegs);

	psArray = psState->apsVecArrayReg[uRegArrayIdx];
	psArray->u.eDirectMappedFormat = (UF_REGFORMAT)psRange->eFormat;
	
	/*
		Create an intermediate register live out of the secondary update program for each
		element of the array.
	*/
	for (uSAIdx = 0; uSAIdx < uArrayLengthInRegs; uSAIdx++)
	{
		IMG_UINT32		uArrayElementRegNum;
		PSAPROG_RESULT	psResult;

		uArrayElementRegNum = psArray->uBaseReg + uSAIdx;

		psResult = 
			BaseAddNewSAProgResult(psState,
								   (UF_REGFORMAT)psRange->eFormat, 
								   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								   psRange->bVector,
								   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								   0 /* uHwRegAlignment */,
								   0 /* uNumHwRegisters */,
								   uArrayElementRegNum,
								   USC_ALL_CHAN_MASK,
								   SAPROG_RESULT_TYPE_DIRECTMAPPED,
								   IMG_TRUE /* bPartOfRange */);

		/*
			Set the fixed hardware register of this element of the array.
		*/
		ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList,
								  psResult->psFixedReg,
								  USEASM_REGTYPE_SECATTR,
								  uRangeStartInRegs + uSAIdx * uRangeStep);

		/*
			Mark each vector in the array as fully referenced.
		*/
		psResult->u1.sDirectMapped.uChanMask = USC_ALL_CHAN_MASK;
	}

	/*
		Replace all LOADCONST instructions loading constants from this range by references to
		the array.
	*/
	for (psListEntry = psBuffer->sLoadConstList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_PARAMS	psLoadConst;
		PINST				psLoadConstInst;
			
		psNextListEntry = psListEntry->psNext;

		psLoadConst = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_PARAMS, sBufferListEntry);
		psLoadConstInst = psLoadConst->psInst;

		if (psLoadConst->psRange == psRange)
		{
			ReplaceLoadConst(psState,
							 psCPState,
							 psLoadConstInst,
							 uRegArrayIdx,
							 IMG_TRUE /* bPartOfArray */);
		}
	}

	/*
		Free the information about the range of constants mapped directly to secondary
		attributes.
	*/
	ASSERT(psState->sSAProg.uConstantRangeCount > 0);
	psState->sSAProg.uConstantRangeCount--;
	RemoveFromList(&psState->sSAProg.sConstantRangeList, &psRange->sListEntry);
	UscFree(psState, psRange);
}

static
IMG_VOID ProcessSAOnlyConstantBuffer(PINTERMEDIATE_STATE psState, PCONST_PACK_STATE psCPState, IMG_UINT32 uBufferIdx)
/*********************************************************************************
 Function			: ProcessSAOnlyConstantBuffer

 Description		: For a constant buffer mapped directly to secondary attributes: replace all LOADCONST
					  instructions from statically addressed constants in the buffer by moves from registers
					  representing secondary attributes.
 
 Parameters			: psState		- Compiler state.
					  psCPState		- Module state.
					  uBufferIdx	- Index of the constant buffer.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PCONSTANT_BUFFER			psBuffer = &psState->asConstantBuffer[uBufferIdx];
	PUNIFLEX_CONSTBUFFERDESC	psDesc;
	PUSC_LIST_ENTRY				psListEntry;
	PUSC_LIST_ENTRY				psNextListEntry;
	PSAPROG_RESULT				psResult;
	IMG_UINT32					uPrevConstOffsetInBytes;

	psDesc = &psState->psSAOffsets->asConstBuffDesc[uBufferIdx];

	/*
		Skip constant buffers not mapped directly to secondary attributes.
	*/
	if (psDesc->eConstBuffLocation != UF_CONSTBUFFERLOCATION_SAS_ONLY)
	{
		return;			
	}

	/*
		Skip unused constant buffers.
	*/
	if (IsListEmpty(&psBuffer->sLoadConstList))
	{
		return;
	}

	/*
		Process all LOADCONST instructions loading constants from the buffer.
	*/
	psResult = NULL;
	uPrevConstOffsetInBytes = USC_UNDEF;
	for (psListEntry = psBuffer->sLoadConstList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PLOADCONST_PARAMS	psLoadConst;
		PINST				psLoadConstInst;
		IMG_UINT32			uConstOffsetInBytes;
		IMG_UINT32			uTempRegNum;

		psNextListEntry = psListEntry->psNext;

		psLoadConst = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_PARAMS, sBufferListEntry);
		psLoadConstInst = psLoadConst->psInst;

		ASSERT(psLoadConst->psRange == NULL);

		/*
			Extract the source offset into the constant buffer.
		*/
		ASSERT(psLoadConstInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uType == USEASM_REGTYPE_IMMEDIATE);
		uConstOffsetInBytes = psLoadConstInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

		/*
			Check if we have already seen this constant offset.
		*/
		if (psResult == NULL || uPrevConstOffsetInBytes != uConstOffsetInBytes)
		{
			IMG_UINT32	uConstOffsetInDwords;

			/*
				Create an intermediate register to represent the secondary attribute.
			*/
			uTempRegNum = GetNextRegister(psState);
			psResult =
				BaseAddNewSAProgResult(psState,
									   psLoadConstInst->asDest[0].eFmt, 
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   psLoadConst->bVectorResult,
									   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   0 /* uHwRegAlignment */,
									   0 /* uNumHwRegisters */,
									   uTempRegNum,
									   USC_ALL_CHAN_MASK,
									   SAPROG_RESULT_TYPE_DIRECTMAPPED,
									   IMG_FALSE /* bPartOfRange */);

			ASSERT((uConstOffsetInBytes % LONG_SIZE) == 0);
			uConstOffsetInDwords = uConstOffsetInBytes / LONG_SIZE;
			ASSERT(uConstOffsetInDwords < psDesc->uSARegCount);

			/*
				Initialize the mask of channels which are mapped to secondary attributes.
			*/
			psResult->u1.sDirectMapped.uChanMask = 0;

			/*
				Set the fixed hardware register of the intermediate register.
			*/
			ModifyFixedRegPhysicalReg(&psState->sSAProg.sFixedRegList,
									  psResult->psFixedReg,
									  USEASM_REGTYPE_SECATTR,
									  psDesc->uStartingSAReg + uConstOffsetInDwords);

			uPrevConstOffsetInBytes = uConstOffsetInBytes;
		}
		else
		{
			uTempRegNum = psResult->psFixedReg->auVRegNum[0];
		}

		/*
			Update the mask of channels within the intermediate register which are mapped
			to secondary attributes.
		*/
		psResult->u1.sDirectMapped.uChanMask |= psLoadConstInst->auDestMask[0];

		/*
			Replace the LOADCONST instruction by a MOV.
		*/
		ReplaceLoadConst(psState,
						 psCPState,
						 psLoadConstInst,
						 uTempRegNum,
						 IMG_FALSE /* bPartOfArray */);
	}
}

IMG_INTERNAL 
IMG_VOID PackConstantRegisters(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: PackConstantRegisters

 Description		: Attempts to pack non-temporary registers.
 
 Parameters			: psState			- Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32				uNumSecAttrsAvailable;
	
	CONST_PACK_STATE		sCPState;
		
	/* Constants buffers in use */	
	IMG_UINT32				uI;	

	PUSC_LIST_ENTRY			psListEntry;

	PCONST_PACK_RANGE_STATE	psBestRange;
	PLOADCONST_DATA			psBestStaticConst;

	PUSC_LIST_ENTRY			psNextListEntry;
	SAFE_LIST_ITERATOR		sIter;

	#if defined(OUTPUT_USPBIN)
	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) != 0)
	{
		sCPState.auSamplersUsedMask = CallocBitArray(psState, psState->psSAOffsets->uTextureCount);
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Reset the list of MOVs inserted.
	*/
	InitializeList(&sCPState.sMOVList);

	/*
		Flag we have started constant register packing.
	*/
	psState->uFlags |= USC_FLAGS_POSTCONSTREGPACK;

	/*
		Process constant buffers which are always mapped to secondary attributes.
	*/
	for (psListEntry = psState->sSAProg.sConstantRangeList.psHead; 
		 psListEntry != NULL; 
		 psListEntry = psNextListEntry)
	{
		PCONSTANT_RANGE	psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_RANGE, sListEntry);

		psNextListEntry = psListEntry->psNext;
		ProcessSAOnlyConstantRange(psState, &sCPState, psRange);
	}
	for (uI = 0; uI < psState->uNumOfConstsBuffsAvailable; uI++)
	{
		ProcessSAOnlyConstantBuffer(psState, &sCPState, uI);
	}

	/*
		Initialize information about each range of constants which can be dynamically
		indexed.
	*/
	sCPState.asRangeState = 
		UscAlloc(psState, psState->sSAProg.uConstantRangeCount * sizeof(sCPState.asRangeState[0]));
	InitializeList(&sCPState.sCostSortedRangeList);
	for (uI = 0, psListEntry = psState->sSAProg.sConstantRangeList.psHead; 
		 uI < psState->sSAProg.uConstantRangeCount; 
		 uI++, psListEntry = psListEntry->psNext)
	{
		PCONST_PACK_RANGE_STATE		psCPRange = &sCPState.asRangeState[uI];
		PCONSTANT_RANGE				psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_RANGE, sListEntry);
		PUNIFLEX_CONSTBUFFERDESC	psConstBuffDesc = &psState->psSAOffsets->asConstBuffDesc[psRange->uBufferIdx];
		PUNIFLEX_RANGE				psInputRange = &psConstBuffDesc->sConstsBuffRanges.psRanges[psRange->uRangeIdx];
		IMG_UINT32					uRangeStart = psInputRange->uRangeStart;
		IMG_UINT32					uRangeEnd = psInputRange->uRangeEnd;

		psRange->psData = psCPRange;

		InitializeList(&psCPRange->sUsesList);
		psCPRange->uCost = 0;
		psCPRange->psRange = psRange;
		psCPRange->b40Bit = IMG_FALSE;
		psCPRange->bInvalidForSecAttrs = IMG_FALSE;

		AppendToList(&sCPState.sCostSortedRangeList, &psCPRange->sCostListEntry);

		/*
			For the present always use two registers for C10 constants in a relatively addressed range
			to simplify addressing.
		*/
		if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) == 0 && psRange->eFormat == UNIFLEX_CONST_FORMAT_C10)
		{
			psCPRange->b40Bit = IMG_TRUE;
		}

		/*
			Given the format of the constants in the range calculate the size of the 
			secondary attributes/memory required for the range.
		*/
		if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0)
		{
			IMG_UINT32	uRangeSizeInBytes;

			uRangeSizeInBytes = uRangeEnd - uRangeStart;
			psCPRange->uRangeSizeInDwords = (uRangeSizeInBytes + LONG_SIZE - 1) / LONG_SIZE;
			psCPRange->uRangeStep = 4;
		}
		else
		{
			IMG_UINT32	uRangeSizeInChans;
			IMG_UINT32	uRangeSizeInDwords = 0;
			IMG_UINT32	uRangeStep = 0;

			uRangeSizeInChans = (uRangeEnd - uRangeStart) << 2;

			switch (psRange->eFormat)
			{
				case UNIFLEX_CONST_FORMAT_F16: 
				{
					uRangeSizeInDwords = uRangeSizeInChans >> 1; 
					uRangeStep = 2;
					break;
				}
				case UNIFLEX_CONST_FORMAT_C10:
				{
					if (psCPRange->psRange->bC10SubcomponentIndex)
					{
						uRangeSizeInDwords = uRangeSizeInChans >> 1;
						uRangeStep = 2;
					}
					else
					{
						if (psCPRange->b40Bit)
						{
							uRangeSizeInDwords = uRangeSizeInChans >> 1;
						}
						else
						{
							uRangeSizeInDwords = uRangeSizeInChans >> 2;
						}
						uRangeStep = 4;
					}
					break;
				}
				case UNIFLEX_CONST_FORMAT_F32:
				{
					uRangeSizeInDwords = uRangeSizeInChans;
					uRangeStep = 1;
					break;
				}
				default: imgabort();
			}

			psCPRange->uRangeSizeInDwords = uRangeSizeInDwords;
			psCPRange->uRangeStep = uRangeStep;
		}
	}

	InitializeList(&sCPState.sCostSortedStaticConstList);

	/*
		Reserve space for temporary values used when adjusting various base addresses
		for the pipe number. For cores without a seperate allocation for temporaries we
		can't use hardware temporary registers in the secondary update program so reserve secondary
		attributes.
	*/
	psState->sSAProg.uMaxTempRegsUsedByAnyInst = 0;
	psState->sSAProg.uNumSecAttrsReservedForTemps = 0;
	if (
			(psState->uCompilerFlags & UF_EXTRACTCONSTANTCALCS) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_UNIFIED_TEMPS_AND_PAS) != 0
	   )
	{
		IMG_UINT32	uMinimumSecondaryTemps;

		/*
			The minimum number of temporary registers which need to be available in the
			secondary update program.
		*/
		if (psState->uMPCoreCount > 1)
		{
			/*
				One to hold the per-pipeline stride and one to hold the concatenation of the core number and
				pipe number when calculating the base address of the scratch buffer in memory.
			*/
			uMinimumSecondaryTemps = 2;
		}
		else
		{
			/*
				One to hold the per-pipeline stride when calculating the base address of the
				scratch buffer in memory.
			*/
			uMinimumSecondaryTemps = 1;
		}

		ASSERT(psState->sSAProg.uInRegisterConstantLimit >= uMinimumSecondaryTemps);
		psState->sSAProg.uInRegisterConstantLimit -= uMinimumSecondaryTemps;
		psState->sSAProg.uNumSecAttrsReservedForTemps += uMinimumSecondaryTemps;
	}

	/*
		If the program contains references to the render target array index then assign
		a secondary attribute for the driver to load it into.
	*/
	ReplaceRTAIdx(psState, &sCPState);

	/*
		Gather information about which offsets into the constant buffer are used
		by the program.
	*/
	SearchConstBufs(psState, &sCPState);	

	/*
		Sort the list of constant ranges by the estimated cycles saved if the
		whole range was moved to secondary attributes.
	*/
	InsertSortList(&sCPState.sCostSortedRangeList, RangeCostCompare);

	/*
		Sort the list of constants accessed with static addresses by the estimated
		cycles saved if the individual constant was moved to a secondary attributes.
	*/
	InsertSortList(&sCPState.sCostSortedStaticConstList, StaticConstCostCompare);

	/*
		Reserve space to store base addresses of constants buffers used in program.
	*/
	sCPState.uNumSecAttrsReservedForBaseAddresses = 0;
	if (psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS)
	{
		for (uI = 0; uI < psState->uNumOfConstsBuffsAvailable; uI++)
		{
			PCONSTANT_BUFFER			psConstBuf = &psState->asConstantBuffer[uI];
			PUNIFLEX_CONSTBUFFERDESC	psUfConstBuf = &psState->psSAOffsets->asConstBuffDesc[uI];

			/*
				Reserve secondary attributes for in-use buffers which don't have
				a secondary attribute explicitly allocated for their base address and which
				aren't mapped directly to secondary attributes. In the latter case we never
				need to generate memory loads for the buffer.
			*/
			if (
					psConstBuf->bInUse && 
					psConstBuf->sBase.uType == USC_REGTYPE_DUMMY &&
					psUfConstBuf->eConstBuffLocation != UF_CONSTBUFFERLOCATION_SAS_ONLY
				)
			{				
				ASSERT(psState->sSAProg.uInRegisterConstantLimit > 0);
				psState->sSAProg.uInRegisterConstantLimit--;
				sCPState.uNumSecAttrsReservedForBaseAddresses++;
			}
		}
	}

	/*
		Move some constants to secondary attributes.
	*/
	uNumSecAttrsAvailable = psState->sSAProg.uInRegisterConstantLimit - psState->sSAProg.uConstSecAttrCount;
	psBestRange = NULL;
	psBestStaticConst = NULL;
	while (uNumSecAttrsAvailable > 0)
	{
		/*
			Look for a constant range which will fit in the remaining secondary
			attributes.
		*/
		while (psBestRange == NULL || 
			   psBestRange->uRangeSizeInDwords > uNumSecAttrsAvailable ||
			   psBestRange->uRangeSizeInDwords > psState->sSAProg.uMaximumConsecutiveSAsAvailable ||
			   psBestRange->bInvalidForSecAttrs)
		{
			PUSC_LIST_ENTRY	psBestRangeListEntry;

			if (psBestRange != NULL)
			{
				FreeRange(psState, psBestRange);
				psBestRange = NULL;
			}

			psBestRangeListEntry = RemoveListHead(&sCPState.sCostSortedRangeList);
			if (psBestRangeListEntry == NULL)
			{
				break;
			}
			psBestRange = IMG_CONTAINING_RECORD(psBestRangeListEntry, PCONST_PACK_RANGE_STATE, sCostListEntry); 
		}
		
		/*
			Look for a static constant which will fit in the remaining secondary attributes.
		*/
		while (psBestStaticConst == NULL ||
			   psBestStaticConst->uNumSAsRequired > uNumSecAttrsAvailable)
		{
			PUSC_LIST_ENTRY	psBestStaticConstListEntry;

			if (psBestStaticConst != NULL)
			{
				FreeLCData(psState, psBestStaticConst);
				psBestStaticConst = NULL;
			}

			psBestStaticConstListEntry = RemoveListHead(&sCPState.sCostSortedStaticConstList);
			if (psBestStaticConstListEntry == NULL)
			{
				break;
			}
			psBestStaticConst = IMG_CONTAINING_RECORD(psBestStaticConstListEntry, PLOADCONST_DATA, sListEntry);
		}
		if (psBestRange == NULL && psBestStaticConst == NULL)
		{
			break;
		}

		if (psBestRange != NULL && (psBestStaticConst == NULL || psBestRange->uCost >= psBestStaticConst->uCost))
		{
			/*
				Replace all (static and dynamic) accesses to the range by secondary
				attributes.
			*/
			ReplaceDynamicAddressLoadConsts(psState, &sCPState, psBestRange->psRange, &psBestStaticConst);
			uNumSecAttrsAvailable -= psBestRange->uRangeSizeInDwords;
			psBestRange = NULL;
		}
		else
		{
			/*
				Replace all static accesses to this constant by a secondary attribute.
			*/
			uNumSecAttrsAvailable -= psBestStaticConst->uNumSAsRequired;
			ReplaceStaticAddressLoadConsts(psState, &sCPState, psBestStaticConst);
			psBestStaticConst = NULL;
		}
	}
	if (psBestStaticConst != NULL)
	{
		FreeLCData(psState, psBestStaticConst);
		psBestStaticConst = NULL;
	}
	if (psBestRange != NULL)
	{
		FreeRange(psState, psBestRange);
		psBestRange = NULL;
	}
	ASSERT(psState->sSAProg.uInRegisterConstantLimit >= psState->sSAProg.uConstSecAttrCount);

	/*
		Free data for constant loads which weren't moved to secondary attributes.
	*/
	while ((psListEntry = RemoveListHead(&sCPState.sCostSortedStaticConstList)) != NULL)
	{
		PLOADCONST_DATA	psLCData = IMG_CONTAINING_RECORD(psListEntry, PLOADCONST_DATA, sListEntry);
			
		FreeLCData(psState, psLCData);
	}
	while ((psListEntry = RemoveListHead(&sCPState.sCostSortedRangeList)) != NULL)
	{
		PCONST_PACK_RANGE_STATE	psRange = IMG_CONTAINING_RECORD(psListEntry, PCONST_PACK_RANGE_STATE, sCostListEntry);

		FreeRange(psState, psRange);
	}

	/*
		For any remaining constant loads convert them to loads from a constant buffer
		in memory.
	*/
	{
		IMG_UINT32 uConstsBuffNum;

		for (uConstsBuffNum = 0; uConstsBuffNum < psState->uNumOfConstsBuffsAvailable; uConstsBuffNum++)
		{
			PCONSTANT_BUFFER	psConstBuf = &psState->asConstantBuffer[uConstsBuffNum];

			/* Allocate some memory */
			ASSERT(psConstBuf->psRemappedMap == IMG_NULL);
			ASSERT(psConstBuf->psRemappedFormat == IMG_NULL);
	
			psConstBuf->psRemappedMap = 
				NewArray(psState, USC_MIN_ARRAY_CHUNK, (IMG_PVOID)0, sizeof(IMG_UINT32));
			psConstBuf->psRemappedFormat = 
				NewArray(psState, USC_MIN_ARRAY_CHUNK, (IMG_PVOID)0, sizeof(IMG_UINT32));		
		}
	}
	InstListIteratorInitialize(psState, ILOADCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst = InstListIteratorCurrent(&sIter);

		ExpandMemoryLoad(psState, &sCPState, psInst->psBlock, psInst);
	}
	InstListIteratorFinalise(&sIter);

	/*
		We have now generated all memory loads from constant buffers so any secondary attributes
		reserved for base address of constant buffers but not used are no longer required.
	*/
	psState->sSAProg.uInRegisterConstantLimit += sCPState.uNumSecAttrsReservedForBaseAddresses;
	ASSERT((psState->sSAProg.uInRegisterConstantLimit + psState->sSAProg.uNumSecAttrsReservedForTemps) == psState->sSAProg.uTotalSAsAvailable);

	#if defined(OUTPUT_USPBIN)
	/*
		Check which USP texture samplers will be definitely be able to load their texture state
		from secondary attributes. We can use this information to avoid reserving temporary registers
		to load texture state from memory.
	*/
	if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
	{
		IMG_UINT32				uSamplerIdx;
		IMG_UINT32				uMaxStatePerSampler = UF_MAX_CHUNKS_PER_TEXTURE * psState->uTexStateSize;
		IMG_UINT32				uOpIdx;
		static const IOPCODE	aeUSPSampleOpcodes[] = {ISMP_USP, 
														ISMPBIAS_USP, 
														ISMPREPLACE_USP, 
														ISMPGRAD_USP, 
														ISMP_USP_NDR};

		psState->sSAProg.uTextureStateAssignedToSAs = 0;
		psState->sSAProg.uNumSAsReservedForTextureState = 0;
		for (uSamplerIdx = 0; uSamplerIdx < psState->psSAOffsets->uTextureCount; uSamplerIdx++)
		{
			/*
				Stop once there isn't any more space in the secondary attributes.
			*/
			ASSERT(psState->sSAProg.uInRegisterConstantLimit >= psState->sSAProg.uConstSecAttrCount);
			if ((psState->sSAProg.uInRegisterConstantLimit - psState->sSAProg.uConstSecAttrCount) < uMaxStatePerSampler)
			{
				break;
			}
			/*
				Skip unused samplers.
			*/
			if (GetBit(sCPState.auSamplersUsedMask, uSamplerIdx) == 0)
			{
				continue;
			}
			/*
				Don't use these secondary attributes for anything else.
			*/
			psState->sSAProg.uInRegisterConstantLimit -= uMaxStatePerSampler;
			psState->sSAProg.uNumSAsReservedForTextureState += uMaxStatePerSampler;
			/*
				Flag that this sampler will always be able to take its texture state from
				secondary attributes so we don't need to reserve any temporaries to be
				able to load the state from memory.
			*/
			psState->sSAProg.uTextureStateAssignedToSAs |= (1U << uSamplerIdx);
		}

		/*
			Allocate temporaries for the USP to use when expanding texture samples. 
		*/
		for (uOpIdx = 0; uOpIdx < (sizeof(aeUSPSampleOpcodes) / sizeof(aeUSPSampleOpcodes[0])); uOpIdx++)
		{
			InstListIteratorInitialize(psState, aeUSPSampleOpcodes[uOpIdx], &sIter);
			for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
			{
				PINST		psInst = InstListIteratorCurrent(&sIter);
				PARG		psTempReg;
				IMG_UINT32	uTempRegCount;

				ASSERT(psInst->eOpcode == aeUSPSampleOpcodes[uOpIdx]);
				ASSERT((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_USPTEXTURESAMPLE) != 0);

				psTempReg = &psInst->u.psSmp->sUSPSample.sTempReg;

				uTempRegCount = GetUSPPerSampleTemporaryCount(psState, psInst->u.psSmp->uTextureStage, psInst);

				ASSERT(psTempReg->uType == USC_REGTYPE_DUMMY);
				psTempReg->uType = USEASM_REGTYPE_TEMP;
				psTempReg->uNumber = GetNextRegisterCount(psState, uTempRegCount);
			}
			InstListIteratorFinalise(&sIter);
		}

		/*
			Allocate temporaries for the USP to use when expanding texture writes.
		*/
		InstListIteratorInitialize(psState, ITEXWRITE, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psInst = InstListIteratorCurrent(&sIter);
			PARG		psTempReg;
			IMG_UINT32	uTempRegCount;

			psTempReg = &psInst->u.psTexWrite->sTempReg;

			uTempRegCount = GetUSPTextureWriteTemporaryCount();

			ASSERT(psTempReg->uType == USC_REGTYPE_DUMMY);
			psTempReg->uType = USEASM_REGTYPE_TEMP;
			psTempReg->uNumber = GetNextRegisterCount(psState, uTempRegCount);
		}
		InstListIteratorFinalise(&sIter);

		InstListIteratorInitialize(psState, ISMPUNPACK_USP, &sIter);
		for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
		{
			PINST		psInst = InstListIteratorCurrent(&sIter);
			PARG		psTempReg;
			IMG_UINT32	uTempRegCount;

			ASSERT(psInst->eOpcode == ISMPUNPACK_USP);

			psTempReg = &psInst->u.psSmpUnpack->sTempReg;

			uTempRegCount = GetUSPPerSampleUnpackTemporaryCount();

			ASSERT(psTempReg->uType == USC_REGTYPE_DUMMY);
			psTempReg->uType = USEASM_REGTYPE_TEMP;
			psTempReg->uNumber = GetNextRegisterCount(psState, uTempRegCount);
		}
		InstListIteratorFinalise(&sIter);

		/*
			Free information about referenced samplers.
		*/
		UscFree(psState, sCPState.auSamplersUsedMask);
		sCPState.auSamplersUsedMask = NULL;
	}
	#endif /* defined(OUTPUT_USPBIN) */

	/*
		Free information about constant ranges.
	*/
	while ((psListEntry = RemoveListHead(&psState->sSAProg.sConstantRangeList)) != NULL)
	{
		PCONSTANT_RANGE	psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_RANGE, sListEntry);
		
		UscFree(psState, psRange);
	}

	/*
		Free storage.
	*/
	UscFree(psState, sCPState.asRangeState);

	/*
		If we replaced any LOADCONST instructions by moves from secondary attributes: now try to
		replace the move destination by the secondary attribute.
	*/
	for (psListEntry = sCPState.sMOVList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PINST		psMovInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);

		psNextListEntry = psListEntry->psNext;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psMovInst->eOpcode == IVMOV)
		{
			EliminateVMOV(psState, psMovInst, IMG_FALSE /* bCheckOnly */);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			EliminateGlobalMove(psState, 
								psMovInst->psBlock, 
								psMovInst, 
								&psMovInst->asArg[0], 
								&psMovInst->asDest[0], 
								NULL /* psEvalList */);
		}
	}
}

/* EOF */
