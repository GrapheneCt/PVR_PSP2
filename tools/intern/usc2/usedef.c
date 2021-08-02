/*************************************************************************
 * Name         : usedef.c
 * Title        : 
 * Created      : Nov 2005
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
 * $Log: usedef.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"

static
IMG_VOID UseDefReplaceUse(PINTERMEDIATE_STATE	psState, 
						  IMG_UINT32			uType, 
						  IMG_UINT32			uNumber,
						  PUSEDEF				psOldUse,
						  PUSEDEF				psNewUse);
static
IMG_VOID UseDefInsertUse(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefToAddTo, PUSEDEF psUseToAdd);
static
IMG_VOID UseDefRemoveUse(PINTERMEDIATE_STATE	psState, 
					     PUSEDEF_CHAIN			psUseDef,
					     PUSEDEF				psUse);

static
USEDEF_CONTAINER_TYPE GetRefContainerType(PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: GetRefContainerType
    
 PURPOSE	: Get the type of object within which a use or define occurs.

 PARAMETERS	: psUseDef			- Use or define.

 RETURNS	: The type.
*****************************************************************************/
{
	switch (psUseDef->eType)
	{
		case USE_TYPE_OLDDEST:
		case USE_TYPE_OLDDESTIDX:
		case USE_TYPE_DESTIDX:
		case USE_TYPE_SRC:
		case USE_TYPE_SRCIDX:
		case USE_TYPE_PREDICATE:
		case DEF_TYPE_INST:
		{
			return USEDEF_CONTAINER_TYPE_INST;
		}

		case USE_TYPE_FIXEDREG:
		case DEF_TYPE_FIXEDREG:
		{
			return USEDEF_CONTAINER_TYPE_FIXEDREG;
		}

		case USE_TYPE_FUNCOUTPUT:
		case DEF_TYPE_FUNCINPUT:
		{
			return USEDEF_CONTAINER_TYPE_FUNCTION;
		}

		case USE_TYPE_SWITCH:
		case USE_TYPE_COND:
		{
			return USEDEF_CONTAINER_TYPE_BLOCK;
		}

		default:
		{
			return USEDEF_CONTAINER_TYPE_UNDEF;
		}
	}
}

static
IMG_INT32 GetInstOrder(PINST psChildInst1, PINST psChildInst2)
/*****************************************************************************
 FUNCTION	: GetInstOrder
    
 PURPOSE	: Get the order of uses/defines associated with two instructions when
			  sorting lists of uses/defines.

 PARAMETERS	: psChildInst1, psChildInst2			- Instructions to compare.

 RETURNS	: The order of the instructions.
*****************************************************************************/
{
	IMG_UINT32	uChildIndex1 = psChildInst1->uGroupChildIndex;
	IMG_UINT32	uChildIndex2 = psChildInst2->uGroupChildIndex;
	PINST		psInst1 = psChildInst1->psGroupParent;
	PINST		psInst2 = psChildInst2->psGroupParent;
	PCODEBLOCK	psInst1Block = psInst1->psBlock;
	PCODEBLOCK	psInst2Block = psInst2->psBlock;
	IMG_BOOL	bInst1InBlock;
	IMG_BOOL	bInst2InBlock;

	bInst1InBlock = (psInst1Block != NULL) ? IMG_TRUE : IMG_FALSE;
	bInst2InBlock = (psInst2Block != NULL) ? IMG_TRUE : IMG_FALSE;
	if (bInst1InBlock != bInst2InBlock)
	{
		return bInst1InBlock - bInst2InBlock;
	}
	if (bInst1InBlock)
	{
		if (psInst1Block == psInst2Block)
		{
			if (psInst1->uBlockIndex != psInst2->uBlockIndex)
			{
				return psInst1->uBlockIndex - psInst2->uBlockIndex;
			}
			if (uChildIndex1 != uChildIndex2)
			{
				return uChildIndex1 - uChildIndex2;
			}
		}
		else
		{
			if (psInst1Block->uGlobalIdx != psInst2Block->uGlobalIdx)
			{
				return psInst1Block->uGlobalIdx - psInst2Block->uGlobalIdx;
			}
		}
	}
	else
	{
		if (psInst1->uGlobalId != psInst2->uGlobalId)
		{
			return psInst1->uGlobalId - psInst2->uGlobalId;
		}
	}
	return 0;
}

static
IMG_INT32 GetContainerOrder(USEDEF_CONTAINER_TYPE	eType1,
							IMG_PVOID				pvContainer1,
							USEDEF_CONTAINER_TYPE	eType2,
							IMG_PVOID				pvContainer2)
/*****************************************************************************
 FUNCTION	: GetContainerOrder
    
 PURPOSE	: Get the order of uses/defines associated with two objects when
			  sorting lists of uses/defines.

 PARAMETERS	: eType1			- First object.
			  pvContainer1
			  eType2			- Second object.
			  pvContainer2

 RETURNS	: The order.
*****************************************************************************/
{
	if (eType1 != eType2)
	{
		return eType1 - eType2;
	}
	else
	{
		switch (eType1)
		{
			case USEDEF_CONTAINER_TYPE_INST:
			{
				return GetInstOrder((PINST)pvContainer1, (PINST)pvContainer2);
			}
			case USEDEF_CONTAINER_TYPE_FUNCTION:
			{
				PFUNC	psFunc1 = (PFUNC)pvContainer1;
				PFUNC	psFunc2 = (PFUNC)pvContainer2;

				return psFunc1->uLabel - psFunc2->uLabel;
			}
			case USEDEF_CONTAINER_TYPE_BLOCK:
			{
				PCODEBLOCK	psBlock1 = (PCODEBLOCK)pvContainer1;
				PCODEBLOCK	psBlock2 = (PCODEBLOCK)pvContainer2;

				return psBlock1->uGlobalIdx - psBlock2->uGlobalIdx;
			}
			case USEDEF_CONTAINER_TYPE_FIXEDREG:
			{
				PFIXED_REG_DATA	psFixedReg1 = (PFIXED_REG_DATA)pvContainer1;
				PFIXED_REG_DATA	psFixedReg2 = (PFIXED_REG_DATA)pvContainer1;

				return psFixedReg1->uGlobalId - psFixedReg2->uGlobalId;
			}
			default:
			{
				return 0;
			}
		}
	}
}

IMG_INTERNAL
IMG_BOOL UseDefIsSSARegisterType(PINTERMEDIATE_STATE psState, IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: UseDefIsSSARegisterType
    
 PURPOSE	: Checks if a register type has the SSA property.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register type.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if ((psState->uFlags2 & USC_FLAGS2_SSA_FORM) != 0 && (uType == USEASM_REGTYPE_TEMP || uType == USEASM_REGTYPE_PREDICATE))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
IMG_BOOL UseDefValidRegisterType(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: UseDefValidRegisterType
    
 PURPOSE	: Checks if we are collecting USE-DEF information for a register
			  type.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register type.
			  psBlock			- If the register type is internal then
								the block containing the reference to the
								register.

 RETURNS	: TRUE if USE-DEF information is available for the register type.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_TEMP ||
		uType == USC_REGTYPE_REGARRAY || 
		uType == USC_REGTYPE_ARRAYBASE)
	{
		if ((psState->uFlags2 & USC_FLAGS2_TEMP_USE_DEF_INFO_VALID) != 0)
		{
			return IMG_TRUE;
		}
	}
	else if (uType == USEASM_REGTYPE_PREDICATE)
	{
		if ((psState->uFlags2 & USC_FLAGS2_PRED_USE_DEF_INFO_VALID) != 0)
		{
			return IMG_TRUE;
		}
	}
	else if (uType == USEASM_REGTYPE_FPINTERNAL && psBlock != NULL)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static
PUSEDEF_CHAIN UseDefBaseGet(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: UseDefBaseGet
    
 PURPOSE	: Get the USE-DEF information for a register.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register type.
			  uNumber			- Register number.
			  psBlock			- If the register is an internal register then
								the block where the register is referenced.

 RETURNS	: A pointer to the USE-DEF information if it exists.
*****************************************************************************/
{
	if (!UseDefValidRegisterType(psState, uType, psBlock))
	{
		return NULL;
	}

	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			PVREGISTER	psTempVReg;

			ASSERT(uNumber < psState->uNumRegisters);
			psTempVReg = (PVREGISTER)ArrayGet(psState, psState->psTempVReg, uNumber);
			return psTempVReg->psUseDefChain;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			PVREGISTER	psPredVReg;

			ASSERT(uNumber < psState->uNumPredicates);
			psPredVReg = (PVREGISTER)ArrayGet(psState, psState->psPredVReg, uNumber);
			return psPredVReg->psUseDefChain;
		}
		case USC_REGTYPE_REGARRAY:
		case USC_REGTYPE_ARRAYBASE:
		{
			PUSC_VEC_ARRAY_REG	psArray;

			ASSERT(uNumber < psState->uNumVecArrayRegs);
			psArray = psState->apsVecArrayReg[uNumber];
			
			if (psArray == NULL)
			{
				return NULL;
			}

			if (uType == USC_REGTYPE_REGARRAY)
			{
				return psArray->psUseDef;
			}
			else
			{
				return psArray->psBaseUseDef;
			}
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			ASSERT(psBlock != NULL);
			ASSERT(uNumber < (sizeof(psBlock->apsIRegUseDef) / sizeof(psBlock->apsIRegUseDef[0])));
			return psBlock->apsIRegUseDef[uNumber];
		}
		default: imgabort();
	}
}

IMG_INTERNAL
PUSEDEF_CHAIN UseDefGet(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: UseDefGet
    
 PURPOSE	: Get the USE-DEF information for a register.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register type.
			  uNumber			- Register number.

 RETURNS	: A pointer to the USE-DEF information if it exists.
*****************************************************************************/
{
	return UseDefBaseGet(psState, uType, uNumber, NULL /* psBlock */);
}

static
IMG_VOID UseDefSet(PINTERMEDIATE_STATE	psState, 
				   IMG_UINT32			uType, 
				   IMG_UINT32			uNumber, 
				   PCODEBLOCK			psBlock, 
				   PUSEDEF_CHAIN		psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefSet
    
 PURPOSE	: Associate a register with USE-DEF information.

 PARAMETERS	: psState		- Compiler state.
			  uType			- Register type.
			  uNumber		- Register number.
			  psBlock		- If the register is an internal register then
							the block where the register is referenced.
			  psUseDef		- USE-DEF information to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(UseDefValidRegisterType(psState, uType, psBlock));

	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			PVREGISTER	psTempVReg;

			ASSERT(uNumber < psState->uNumRegisters);
			psTempVReg = ArrayGet(psState, psState->psTempVReg, uNumber);
			psTempVReg->psUseDefChain = psUseDef;
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			PVREGISTER	psPredVReg;

			ASSERT(uNumber < psState->uNumPredicates);
			psPredVReg = ArrayGet(psState, psState->psPredVReg, uNumber);
			psPredVReg->psUseDefChain = psUseDef;
			break;
		}
		case USC_REGTYPE_REGARRAY:
		case USC_REGTYPE_ARRAYBASE:
		{
			PUSC_VEC_ARRAY_REG	psArray;

			ASSERT(uNumber < psState->uNumVecArrayRegs);
			psArray = psState->apsVecArrayReg[uNumber];
			ASSERT(psArray != NULL);

			if (uType == USC_REGTYPE_REGARRAY)
			{
				ASSERT(psUseDef == NULL || psArray->psUseDef == NULL);
				psArray->psUseDef = psUseDef;
			}
			else
			{
				ASSERT(psUseDef == NULL || psArray->psBaseUseDef == NULL);
				psArray->psBaseUseDef = psUseDef;
			}
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			ASSERT(psBlock != NULL);

			ASSERT(uNumber < (sizeof(psBlock->apsIRegUseDef) / sizeof(psBlock->apsIRegUseDef[0])));

			ASSERT(psUseDef == NULL || psBlock->apsIRegUseDef[uNumber] == NULL);
			psBlock->apsIRegUseDef[uNumber] = psUseDef;
			break;
		}
		default: imgabort();
	}
}

static
IMG_VOID AppendToDroppedUsesList(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef)
/*****************************************************************************
 FUNCTION	: AppendToDroppedUsesList
    
 PURPOSE	: Add a register to the list of partially unused registers.

 PARAMETERS	: psState			- Compiler state.
			  psUseDef			- Register to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if ((psState->uFlags2 & USC_FLAGS2_IN_SSA_RENAME_PASS) != 0)
	{
		return;
	}
	if (psUseDef->uType == USEASM_REGTYPE_TEMP || psUseDef->uType == USEASM_REGTYPE_PREDICATE)
	{
		if (!IsEntryInList(&psState->sDroppedUsesTempList, &psUseDef->sDroppedUsesTempListEntry))
		{
			AppendToList(&psState->sDroppedUsesTempList, &psUseDef->sDroppedUsesTempListEntry);
		}
	}
}

static
PUSEDEF_CHAIN UseDefGetOrCreate(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: UseDefGetOrCreate
    
 PURPOSE	: Get the USE-DEF information for a register, allocating it if it
			  doesn't already exist.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register to USE-DEF information for.
			  uNumber
			  psBlock			- If the register is an internal register then
								the block where the register is referenced.

 RETURNS	: A pointer to the USE-DEF information.
*****************************************************************************/
{	
	PUSEDEF_CHAIN	psUseDef;

	/*
		Check if this is a register for which we are collecting USE-DEF information.
	*/
	if (!UseDefValidRegisterType(psState, uType, psBlock))
	{
		return NULL;
	}

	/*
		If USE-DEF information already exists for this register then return it.
	*/
	if ((psUseDef = UseDefBaseGet(psState, uType, uNumber, psBlock)) != NULL)
	{
		return psUseDef;
	}

	/*
		Allocate and initialize the USE-DEF inforamtion.
	*/
	psUseDef = UscAlloc(psState, sizeof(*psUseDef));
	InitializeList(&psUseDef->sList);
	psUseDef->uType = uType;
	psUseDef->uNumber = uNumber;
	psUseDef->eFmt = UF_REGFORMAT_UNTYPED;
	psUseDef->psDef = NULL;
	if (uType == USEASM_REGTYPE_FPINTERNAL)
	{
		psUseDef->psBlock = psBlock;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psUseDef->eIsVector = USEDEF_ISVECTOR_NOTRECORDED;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	else		
	{
		psUseDef->psBlock = NULL;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psUseDef->eIsVector = USEDEF_ISVECTOR_UNKNOWN;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	psUseDef->uIndexUseCount = 0;
	psUseDef->uUseDefCount = 0;
	ClearListEntry(&psUseDef->sDroppedUsesTempListEntry);
	ClearListEntry(&psUseDef->sIndexUseTempListEntry);
	ClearListEntry(&psUseDef->sC10TempListEntry);

	/*
		Add the register to list of registers with no uses.
	*/
	AppendToDroppedUsesList(psState, psUseDef);

	/*
		Update the mapping from register numbers to USE-DEF information.
	*/
	UseDefSet(psState, uType, uNumber, psBlock, psUseDef);

	return psUseDef;
}

static
IMG_VOID UseDefReplace(PUSEDEF psOldUseDef, PUSEDEF psNewUseDef)
/*****************************************************************************
 FUNCTION	: UseDefReplace
    
 PURPOSE	: Update data structures when resizing an array containing USE/DEF
			  records.

 PARAMETERS	: psOldUseDef			- Old record.
			  psNewUseDef			- New record.

 RETURNS	: Nothing.
*****************************************************************************/
{
	*psNewUseDef = *psOldUseDef;
	if (psOldUseDef->psUseDefChain != NULL)
	{
		if (psOldUseDef->psUseDefChain->psDef == psOldUseDef)
		{
			psOldUseDef->psUseDefChain->psDef = psNewUseDef;
		}

		InsertInList(&psOldUseDef->psUseDefChain->sList, &psNewUseDef->sListEntry, &psOldUseDef->sListEntry);
		RemoveFromList(&psOldUseDef->psUseDefChain->sList, &psOldUseDef->sListEntry);
	}
}

IMG_INTERNAL
PARGUMENT_USEDEF UseDefResizeArgArray(PINTERMEDIATE_STATE	psState, 
									  PARGUMENT_USEDEF		asOldUseDef, 
									  IMG_UINT32			uOldCount, 
									  IMG_UINT32			uNewCount)
/*****************************************************************************
 FUNCTION	: UseDefResizeArgArray
    
 PURPOSE	: Grow or shrink an array of USE/DEF records.

 PARAMETERS	: psState			- Compiler state.
			  asOldUseDef		- Array to modify.
			  uOldCount			- Current count of elements in the array.
			  uNewCount			- New count of elements in the array.

 RETURNS	: The new array.
*****************************************************************************/
{
	PARGUMENT_USEDEF	asNewUseDef;
	IMG_UINT32			uCommonCount;
	IMG_UINT32			i;

	asNewUseDef = UscAlloc(psState, sizeof(asNewUseDef[0]) * uNewCount);

	uCommonCount = min(uNewCount, uOldCount);
	for (i = 0; i < uCommonCount; i++)
	{
		UseDefReplace(&asOldUseDef[i].sUseDef, &asNewUseDef[i].sUseDef);
		UseDefReplace(&asOldUseDef[i].sIndexUseDef, &asNewUseDef[i].sIndexUseDef);
	}

	UscFree(psState, asOldUseDef);

	return asNewUseDef;
}

IMG_INTERNAL
PUSEDEF UseDefResizeArray(PINTERMEDIATE_STATE	psState, 
						  PUSEDEF				asOldUseDef, 
						  IMG_UINT32			uOldCount, 
						  IMG_UINT32			uNewCount)
/*****************************************************************************
 FUNCTION	: UseDefResizeArray
    
 PURPOSE	: Grow or shrink an array of USE/DEF records.

 PARAMETERS	: psState			- Compiler state.
			  asOldUseDef		- Array to modify.
			  uOldCount			- Current count of elements in the array.
			  uNewCount			- New count of elements in the array.

 RETURNS	: The new array.
*****************************************************************************/
{
	PUSEDEF		asNewUseDef;
	IMG_UINT32	uCommonCount;
	IMG_UINT32	i;

	asNewUseDef = UscAlloc(psState, sizeof(asNewUseDef[0]) * uNewCount);

	uCommonCount = min(uNewCount, uOldCount);
	for (i = 0; i < uCommonCount; i++)
	{
		UseDefReplace(&asOldUseDef[i], &asNewUseDef[i]);
	}

	UscFree(psState, asOldUseDef);

	return asNewUseDef;
}

IMG_INTERNAL
IMG_INT32 CmpUse(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*****************************************************************************
 FUNCTION	: CmpUse
    
 PURPOSE	: Helper function to sort lists of USE/DEFs.

 PARAMETERS	: psListEntry1				- USE/DEFs to compare.
			  psListEntry2

 RETURNS	: The order of the arguments.
*****************************************************************************/
{
	PUSEDEF					psUseDef1 = IMG_CONTAINING_RECORD(psListEntry1, PUSEDEF, sListEntry);
	PUSEDEF					psUseDef2 = IMG_CONTAINING_RECORD(psListEntry2, PUSEDEF, sListEntry);
	USEDEF_CONTAINER_TYPE	eContainer1;
	USEDEF_CONTAINER_TYPE	eContainer2;
	IMG_INT32				iContainerCmp;
	IMG_INT32				iTypeCmp;

	/*
		Get the type of object in which each use/define occurs.
	*/
	eContainer1 = GetRefContainerType(psUseDef1);
	eContainer2 = GetRefContainerType(psUseDef2);

	/*
		If the two uses/defines are in different objects then use the order of the objects.
	*/
	iContainerCmp = GetContainerOrder(eContainer1, psUseDef1->u.pvData, eContainer2, psUseDef2->u.pvData);
	if (iContainerCmp != 0)
	{
		return iContainerCmp;
	}

	/*
		Sort uses by type.
	*/
	iTypeCmp = psUseDef1->eType - psUseDef2->eType;
	if (iTypeCmp != 0)
	{
		return iTypeCmp;
	}

	/*
		Sort uses by the source/destination index.
	*/
	return psUseDef1->uLocation - psUseDef2->uLocation;
}

static
IMG_VOID UseDefInsertUse(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDefToAddTo, PUSEDEF psUseToAdd)
/*****************************************************************************
 FUNCTION	: UseDefInsertUse
    
 PURPOSE	: Add a structure describing a register use to a USE-DEF chain.

 PARAMETERS	: psState					- Compiler state.
			  psUseDefToAddTo			- USE-DEF chain to update.
			  psUseToAdd				- USE to insert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		If adding a use as an index then add this register to the list of registers
		used as indices.
	*/
	if (psUseToAdd->eType == USE_TYPE_SRCIDX || 
		psUseToAdd->eType == USE_TYPE_DESTIDX || 
		psUseToAdd->eType == USE_TYPE_OLDDESTIDX)
	{
		if (psUseDefToAddTo->uIndexUseCount == 0)
		{
			AppendToList(&psState->sIndexUseTempList, &psUseDefToAddTo->sIndexUseTempListEntry);
		}
		psUseDefToAddTo->uIndexUseCount++;
	}

	/*
		Keep track of the definition point.
	*/
	if (psUseToAdd->eType >= DEF_TYPE_FIRST && psUseToAdd->eType <= DEF_TYPE_LAST)
	{
		if (UseDefIsSSARegisterType(psState, psUseDefToAddTo->uType))
		{
			ASSERT(psUseDefToAddTo->psDef == NULL);
		}

		psUseDefToAddTo->psDef = psUseToAdd;
	}

	/*
		Make a link from the register use back to the register.
	*/
	ASSERT(psUseToAdd->psUseDefChain == NULL);
	psUseToAdd->psUseDefChain = psUseDefToAddTo;

	/*
		Keep uses in the same instruction together.
	*/
	InsertInListSorted(&psUseDefToAddTo->sList, CmpUse, &psUseToAdd->sListEntry);
	psUseDefToAddTo->uUseDefCount++;
}

static
UF_REGFORMAT GetUseFmt(PUSEDEF psUse)
/*****************************************************************************
 FUNCTION	: GetUseFmt
    
 PURPOSE	: Get the register format at a particular usage.

 PARAMETERS	: psUse			- Usage.

 RETURNS	: The register format.
*****************************************************************************/
{
	switch (psUse->eType)
	{
		case USE_TYPE_SRC:
		{
			return psUse->u.psInst->asArg[psUse->uLocation].eFmt;
		}
		case USE_TYPE_OLDDEST:
		{
			return psUse->u.psInst->apsOldDest[psUse->uLocation]->eFmt;
		}
		case USE_TYPE_FIXEDREG:	
		case DEF_TYPE_FIXEDREG:
		{
			return psUse->u.psFixedReg->aeVRegFmt[psUse->uLocation];
		}
		case USE_TYPE_FUNCOUTPUT:
		{
			return psUse->u.psFunc->sOut.asArray[psUse->uLocation].eFmt;
		}
		case DEF_TYPE_FUNCINPUT:
		{
			return psUse->u.psFunc->sIn.asArray[psUse->uLocation].eFmt;
		}
		default:
		{
			return UF_REGFORMAT_UNTYPED;
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
IMG_INTERNAL
IMG_BOOL IsVectorSource(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION	: IsVectorSource
    
 PURPOSE	: Checks if an instruction source argument must be a vector sized
			  register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSrc				- Index of the source to check.

 RETURNS	: TRUE if this source argument must be a vector sized register.
*****************************************************************************/
{
	/*
		If this flag is set then all vector registers have been replaced by bundles of
		scalar registers.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS) != 0)
	{
		return IMG_FALSE;
	}

	if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
	{
		if ((uSrc % SOURCE_ARGUMENTS_PER_VECTOR) == 0)
		{
			return IMG_TRUE;
		}
	}

	if (psInst->eOpcode == IDELTA && psInst->u.psDelta->bVector)
	{
		return IMG_TRUE;
	}

	if (psInst->eOpcode == ICALL && psInst->u.psCall->psTarget->sIn.asArray[uSrc].bVector)
	{
		return IMG_TRUE;
	}

	if (psInst->eOpcode == IFEEDBACKDRIVEREPILOG && psInst->u.psFeedbackDriverEpilog->psFixedReg->bVector)
	{
		return IMG_TRUE;
	}

	if (psInst->eOpcode == ISAVEIREG || psInst->eOpcode == IRESTOREIREG)
	{
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			return IMG_TRUE;
		}
	}

	if (
			(g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE) != 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0
	   )
	{
		PARG	psSrc = &psInst->asArg[uSrc];

		if ((uSrc == SMP_COORD_ARG_START || uSrc == SMP_PROJ_ARG) && psSrc->eFmt != UF_REGFORMAT_C10)
		{
			return IMG_TRUE;
		}
		if (uSrc == SMP_LOD_ARG_START || 
			uSrc == SMP_GRAD_ARG_START ||
			uSrc == SMP_GRAD_ARG_START + 1)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL IsVectorDest(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: IsVectorDest
    
 PURPOSE	: Checks if an instruction destination argument must be a vector sized
			  register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to check.
			  uSrc				- Index of the destination to check.

 RETURNS	: TRUE if this destination argument must be a vector sized register.
*****************************************************************************/
{
	if ((psState->uFlags2 & USC_FLAGS2_NO_VECTOR_REGS) != 0)
	{
		return IMG_FALSE;
	}
	if (psInst->asDest[uDestIdx].uType == USEASM_REGTYPE_PREDICATE)
	{
		return IMG_FALSE;
	}
	if (psInst->eOpcode == IDELTA && psInst->u.psDelta->bVector)
	{
		return IMG_TRUE;
	}
	if (psInst->eOpcode == ICALL && psInst->u.psCall->psTarget->sOut.asArray[uDestIdx].bVector)
	{
		return IMG_TRUE;
	}
	if (InstHasVectorDest(psState, psInst, uDestIdx))
	{
		return IMG_TRUE;
	}
	if (psInst->eOpcode == ILOADCONST && psInst->u.psLoadConst->bVectorResult)
	{
		return IMG_TRUE;
	}
	if (psInst->eOpcode == ISAVEIREG || psInst->eOpcode == IRESTOREIREG)
	{
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
USEDEF_ISVECTOR IsUseDefVector(PINTERMEDIATE_STATE psState, PUSEDEF psUse)
/*****************************************************************************
 FUNCTION	: IsUseDefVector
    
 PURPOSE	: Checks if a particular USE or DEF interprets the source or
			  destination register as a vector.

 PARAMETERS	: psState				- Compiler state.
			  psUse					- USE or DEF.

 RETURNS	: Either yes, no or unknown.
*****************************************************************************/
{
	switch (psUse->eType)
	{
		case USE_TYPE_SRC:
		{
			if (IsVectorSource(psState, psUse->u.psInst, psUse->uLocation))
			{
				return USEDEF_ISVECTOR_YES;
			}
			return USEDEF_ISVECTOR_NO;
		}
		case USE_TYPE_OLDDEST:
		case DEF_TYPE_INST:
		{ 
			if (IsVectorDest(psState, psUse->u.psInst, psUse->uLocation))
			{
				return USEDEF_ISVECTOR_YES;
			}
			return USEDEF_ISVECTOR_NO;
		}
		case USE_TYPE_COND:
		case USE_TYPE_PREDICATE:
		case USE_TYPE_SRCIDX:
		case USE_TYPE_DESTIDX:
		case USE_TYPE_OLDDESTIDX:
		{
			return USEDEF_ISVECTOR_NO;
		}
		case USE_TYPE_FIXEDREG:
		case DEF_TYPE_FIXEDREG:
		{
			if (psUse->u.psFixedReg->bVector)
			{
				return USEDEF_ISVECTOR_YES;
			}
			else
			{
				return USEDEF_ISVECTOR_NO;
			}
		}
		case DEF_TYPE_FUNCINPUT:
		{
			if (psUse->u.psFunc->sIn.asArray[psUse->uLocation].bVector)
			{
				return USEDEF_ISVECTOR_YES;
			}
			else
			{
				return USEDEF_ISVECTOR_NO;
			}
		}
		case USE_TYPE_FUNCOUTPUT:
		{
			if (psUse->u.psFunc->sOut.asArray[psUse->uLocation].bVector)
			{
				return USEDEF_ISVECTOR_YES;
			}
			else
			{
				return USEDEF_ISVECTOR_NO;
			}
		}
		default:
		{
			return USEDEF_ISVECTOR_UNKNOWN;
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
UF_REGFORMAT GetDefFmt(PUSEDEF psDef)
/*****************************************************************************
 FUNCTION	: GetDefFmt
    
 PURPOSE	: Get the register format at a particular definition.

 PARAMETERS	: psUse			- Definition.

 RETURNS	: The register format.
*****************************************************************************/
{
	switch (psDef->eType)
	{
		case DEF_TYPE_INST:
		{
			return psDef->u.psInst->asDest[psDef->uLocation].eFmt;
		}
		case DEF_TYPE_FIXEDREG:	
		{
			return psDef->u.psFixedReg->aeVRegFmt[psDef->uLocation];
		}
		case DEF_TYPE_FUNCINPUT:
		{
			return psDef->u.psFunc->sIn.asArray[psDef->uLocation].eFmt;
		}
		default:
		{
			return UF_REGFORMAT_UNTYPED;
		}
	}
}

IMG_INTERNAL
IMG_UINT32 GetUseChanMask(PINTERMEDIATE_STATE	psState,
						  PUSEDEF				psUse)
/*****************************************************************************
 FUNCTION	: GetUseChanMask
    
 PURPOSE	: Get the mask of channels used from a register at a particular use.

 PARAMETERS	: psState				- Compiler state.
			  psUse					- USE to get the channels for.

 RETURNS	: The mask of channels.
*****************************************************************************/
{
	switch (psUse->eType)
	{
		case USE_TYPE_SRCIDX:
		case USE_TYPE_DESTIDX:
		case USE_TYPE_OLDDESTIDX:
		{
			return USC_XY_CHAN_MASK;
		}
		case USE_TYPE_SRC:
		{
			return GetLiveChansInArg(psState, psUse->u.psInst, psUse->uLocation);
		}
		case USE_TYPE_OLDDEST:
		{
			return GetPreservedChansInPartialDest(psState, psUse->u.psInst, psUse->uLocation);
		}
		case USE_TYPE_SWITCH:
		{
			return USC_ALL_CHAN_MASK;
		}
		case USE_TYPE_COND:
		case USE_TYPE_PREDICATE:
		{
			return USC_ALL_CHAN_MASK;
		}
		case USE_TYPE_FIXEDREG:
		{
			PFIXED_REG_DATA	psFixedReg = psUse->u.psFixedReg;

			ASSERT(psUse->uLocation < psFixedReg->uConsecutiveRegsCount);
			if (psFixedReg->auMask != NULL)
			{
				return psFixedReg->auMask[psUse->uLocation];
			}
			else
			{
				return USC_ALL_CHAN_MASK;
			}
		}
		case USE_TYPE_FUNCOUTPUT:
		{
			PFUNC	psFunc = psUse->u.psFunc;

			ASSERT(psUse->uLocation < psFunc->sOut.uCount);
			return psFunc->sOut.asArray[psUse->uLocation].uChanMask;
		}
		default: imgabort();
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static 
IMG_VOID UseDefUpdateIsVector(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef, PUSEDEF psUseOrDef)
/*****************************************************************************
 FUNCTION	: UseDefUpdateIsVector
    
 PURPOSE	: Updates whether a register is interpreted as a vector when
			  adding a new USE or DEF.

 PARAMETERS	: psState				- Compiler state.
			  psUseDef				- Register to update.
			  psUseOrDef			- New USE or DEF.

 RETURNS	: Nothing.
*****************************************************************************/
{
	USEDEF_ISVECTOR	eUseOrDefIsVector = IsUseDefVector(psState, psUseOrDef);

	if (psUseDef->eIsVector == USEDEF_ISVECTOR_NOTRECORDED)
	{
		return;
	}
	if (psUseDef->eIsVector == USEDEF_ISVECTOR_UNKNOWN)
	{
		psUseDef->eIsVector = eUseOrDefIsVector;
		if (psUseDef->eIsVector == USEDEF_ISVECTOR_YES)
		{
			AppendToList(&psState->sVectorTempList, &psUseDef->sVectorTempListEntry);
		}
	}
	else
	{
		ASSERT(eUseOrDefIsVector == USEDEF_ISVECTOR_UNKNOWN || eUseOrDefIsVector == psUseDef->eIsVector);
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID UseDefSetFmt(PINTERMEDIATE_STATE psState, IMG_UINT32 uTempRegNum, UF_REGFORMAT eFmt)
{
	PUSEDEF_CHAIN	psUseDef;

	psUseDef = UseDefGetOrCreate(psState, USEASM_REGTYPE_TEMP, uTempRegNum, NULL /* psBlock */);
	if (psUseDef->eFmt == UF_REGFORMAT_UNTYPED)
	{
		psUseDef->eFmt = eFmt;
		if (eFmt == UF_REGFORMAT_C10)
		{
			AppendToList(&psState->sC10TempList, &psUseDef->sC10TempListEntry);
		}
	}
	else
	{
		ASSERT(psUseDef->eFmt == eFmt);
	}
}

static
IMG_VOID UseDefUpdateFmt(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef, UF_REGFORMAT eFmt)
/*****************************************************************************
 FUNCTION	: UseDefUpdateFmt
    
 PURPOSE	: Update the format stored for a register is the format at new
			  use or define is more informational.

 PARAMETERS	: psState				- Compiler state.
			  psUseDef				- USE-DEF information to modify.
			  eFmt					- New format.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psUseDef->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		return;
	}
	if (psUseDef->eFmt == UF_REGFORMAT_UNTYPED)
	{
		psUseDef->eFmt = eFmt;
		if (eFmt == UF_REGFORMAT_C10)
		{
			AppendToList(&psState->sC10TempList, &psUseDef->sC10TempListEntry);
		}
	}
	else
	{
		ASSERT(eFmt == UF_REGFORMAT_UNTYPED || psUseDef->eFmt == eFmt);
	}
}

IMG_INTERNAL
PINST UseDefGetInst(PUSEDEF psUseOrDef)
/*****************************************************************************
 FUNCTION	: UseDefGetInst
    
 PURPOSE	: If a use or define of a register is in an instruction then return the
			  instruction.

 PARAMETERS	: psUse			- Use to get the block for.

 RETURNS	: A pointer to the instruction.
*****************************************************************************/
{
	if (
			(
				psUseOrDef->eType >= USE_TYPE_FIRSTINSTUSE && 
				psUseOrDef->eType <= USE_TYPE_LASTINSTUSE
			 ) ||
			 psUseOrDef->eType == DEF_TYPE_INST
	   )
	{
		return psUseOrDef->u.psInst;
	}
	else
	{
		return NULL;
	}
}

static
PCODEBLOCK UseDefGetUseBlock(PUSEDEF psUse)
/*****************************************************************************
 FUNCTION	: UseDefGetUseBlock
    
 PURPOSE	: If a use of a register is in an instruction then get the flow
			  control block containing the instruction.

 PARAMETERS	: psUse			- Use to get the block for.

 RETURNS	: A pointer to the block.
*****************************************************************************/
{
	PINST	psUseInst;

	psUseInst = UseDefGetInst(psUse);
	if (psUseInst != NULL)
	{
		if (psUseInst->psGroupParent != NULL)
		{
			return psUseInst->psGroupParent->psBlock;
		}
		else
		{
			return psUseInst->psBlock;
		}
	}
	else
	{
		return NULL;
	}
}

IMG_INTERNAL
IMG_VOID UseDefAddUse(PINTERMEDIATE_STATE	psState,
					  IMG_UINT32			uType,
					  IMG_UINT32			uNumber,
					  PUSEDEF				psUseToAdd)
/*****************************************************************************
 FUNCTION	: UseDefAddUse
    
 PURPOSE	: Add a new use of a register.

 PARAMETERS	: psState				- Compiler state.
			  uType					- Used register.
			  uNumber
			  psUseToAdd			- Record of the use.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDefToAddTo;

	psUseDefToAddTo = UseDefGetOrCreate(psState, uType, uNumber, UseDefGetUseBlock(psUseToAdd));
	if (psUseDefToAddTo == NULL)
	{
		psUseToAdd->psUseDefChain = NULL;
		return;
	}

	UseDefInsertUse(psState, psUseDefToAddTo, psUseToAdd);

	UseDefUpdateFmt(psState, psUseDefToAddTo, GetUseFmt(psUseToAdd));
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	UseDefUpdateIsVector(psState, psUseDefToAddTo, psUseToAdd);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

IMG_INTERNAL
IMG_VOID UseDefAddArgUses(PINTERMEDIATE_STATE	psState,
						  PCARG					psArg,
						  PARGUMENT_USEDEF		psArgUseDef)
/*****************************************************************************
 FUNCTION	: UseDefAddArgUses
    
 PURPOSE	: Add new uses of a source or partial dest argument.

 PARAMETERS	: psState				- Compiler state.
			  psArg					- Argument.
			  psArgUseDef			- USE records for the arguments.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegisterNumber;

	if (psState->uFlags & USC_FLAGS_MOESTAGE)
	{
		uRegisterNumber = psArg->uNumberPreMoe;
	}
	else
	{
		uRegisterNumber = psArg->uNumber;
	}

	UseDefAddUse(psState,
				 psArg->uType, 
				 uRegisterNumber,
				 &psArgUseDef->sUseDef);
	UseDefAddUse(psState,
				 psArg->uIndexType, 
				 psArg->uIndexNumber,
				 &psArgUseDef->sIndexUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefAddDef(PINTERMEDIATE_STATE	psState,
					  IMG_UINT32			uType,
					  IMG_UINT32			uNumber,
					  PUSEDEF				psDef)
/*****************************************************************************
 FUNCTION	: UseDefAddDef
    
 PURPOSE	: Add a new define of a register.

 PARAMETERS	: psState				- Compiler state.
			  uType					- Defined register.
			  uNumber
			  psDef					- Record describing the location of the
									definition.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;

	psUseDef = UseDefGetOrCreate(psState, uType, uNumber, UseDefGetUseBlock(psDef));
	if (psUseDef == NULL)
	{
		psDef->psUseDefChain = NULL;
		return;
	}

	/*
		Insert the new define.
	*/
	UseDefInsertUse(psState, psUseDef, psDef);

	UseDefUpdateFmt(psState, psUseDef, GetDefFmt(psDef));
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	UseDefUpdateIsVector(psState, psUseDef, psDef);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static
IMG_VOID UseDefBaseDropDef(PINTERMEDIATE_STATE	psState, 
						   PUSEDEF				psDef)
/*****************************************************************************
 FUNCTION	: UseDefBaseDropDef
    
 PURPOSE	: Remove information for a define of a register.

 PARAMETERS	: psState				- Compiler state.
			  psDef					- Location of the define of the register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;

	if (psDef->psUseDefChain == NULL)
	{
		return;
	}

	psUseDef = psDef->psUseDefChain;

	if (!UseDefIsSSARegisterType(psState, psUseDef->uType))
	{
		UseDefRemoveUse(psState, psUseDef, psDef);
		return;
	}

	if (IsSingleElementList(&psUseDef->sList))
	{
		UseDefDelete(psState, psUseDef);
	}
	else
	{
		UseDefRemoveUse(psState, psUseDef, psDef);
	}
}

IMG_INTERNAL
IMG_VOID UseDefDropFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uInput)
/*****************************************************************************
 FUNCTION	: UseDefDropFuncInput
    
 PURPOSE	: Remove information for a define of a register by a function
			  input.

 PARAMETERS	: psState				- Compiler state.
			  psFunc				- Function.
			  uInput				- Index of the input to drop.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC_INOUT	psIn;

	ASSERT(uInput < psFunc->sIn.uCount);
	psIn = &psFunc->sIn.asArray[uInput];

	UseDefBaseDropDef(psState, &psFunc->sIn.asArrayUseDef[uInput]);
}


IMG_INTERNAL
IMG_VOID UseDefDropFuncOutput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uOutput)
/*****************************************************************************
 FUNCTION	: UseDefDropFuncOutput
    
 PURPOSE	: Remove information for a use of a register as a function
			  output.

 PARAMETERS	: psState				- Compiler state.
			  psFunc				- Function.
			  uOutput				- Index of the input to drop.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC_INOUT	psOut;

	ASSERT(uOutput < psFunc->sOut.uCount);
	psOut = &psFunc->sOut.asArray[uOutput];

	UseDefDropUse(psState, &psFunc->sOut.asArrayUseDef[uOutput]);
}

IMG_INTERNAL
IMG_VOID UseDefAddFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uInput)
/*****************************************************************************
 FUNCTION	: UseDefAddFuncInput
    
 PURPOSE	: Add information for a define of a register by a function
			  input.

 PARAMETERS	: psState				- Compiler state.
			  psFunc				- Function.
			  uInput				- Index of the input to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC_INOUT	psIn;

	ASSERT(uInput < psFunc->sIn.uCount);
	psIn = &psFunc->sIn.asArray[uInput];

	UseDefAddDef(psState, psIn->uType, psIn->uNumber, &psFunc->sIn.asArrayUseDef[uInput]);
}


IMG_INTERNAL
IMG_VOID UseDefAddFuncOutput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uOutput)
/*****************************************************************************
 FUNCTION	: UseDefAddFuncOutput
    
 PURPOSE	: Add information for a use of a register as a function
			  output.

 PARAMETERS	: psState				- Compiler state.
			  psFunc				- Function.
			  uOutput				- Index of the input to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC_INOUT	psOut;

	ASSERT(uOutput < psFunc->sOut.uCount);
	psOut = &psFunc->sOut.asArray[uOutput];

	UseDefAddUse(psState, psOut->uType, psOut->uNumber, &psFunc->sOut.asArrayUseDef[uOutput]);
}

static
IMG_VOID UseDefDropDef(PINTERMEDIATE_STATE	psState, 
					   PINST				psInst,
					   IMG_UINT32			uDestIdx)
/*****************************************************************************
 FUNCTION	: UseDefDropDef
    
 PURPOSE	: Remove information for a define of a register by an instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction.
			  uDestIdx				- Index of the destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefBaseDropDef(psState, &psInst->asDestUseDef[uDestIdx].sUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefReplaceArgumentUses(PINTERMEDIATE_STATE	psState, 
								   PARG					psToArg, 
								   PARGUMENT_USEDEF		psToUseRecord,
								   PARGUMENT_USEDEF		psFromUseRecord)
/*****************************************************************************
 FUNCTION	: UseDefReplaceArgumentUses
    
 PURPOSE	: Update USE-DEF information when moving a source or partial destination
			  argument between instructions.

 PARAMETERS	: psState				- Compiler state.
			  psToArg				- Argument to move.
			  psToUseRecord			- New location of the argument.
			  psFromUseRecord		- Old location of the argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefReplaceUse(psState,
					 psToArg->uType,
					 psToArg->uNumber,
					 &psFromUseRecord->sUseDef,
					 &psToUseRecord->sUseDef);
	UseDefReplaceUse(psState,
					 psToArg->uIndexType,
					 psToArg->uIndexNumber,
					 &psFromUseRecord->sIndexUseDef,
					 &psToUseRecord->sIndexUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefDropDest(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx)
/*****************************************************************************
 FUNCTION	: UseDefDropDest
    
 PURPOSE	: Remove a use-def information associated with an instruction destination.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction.
			  uDestIdx				- Index of the destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Remove any use of an index in the destination.
	*/
	UseDefDropUse(psState, &psInst->asDestUseDef[uDestIdx].sIndexUseDef);

	/*
		Remove any define of the destination register.
	*/
	UseDefDropDef(psState, psInst, uDestIdx);
}

IMG_INTERNAL
IMG_VOID UseDefDropFixedRegDef(PINTERMEDIATE_STATE	psState, 
							   PFIXED_REG_DATA		psFixedReg,
							   IMG_UINT32			uRegIdx)
/*****************************************************************************
 FUNCTION	: UseDefDropFixedRegUse
    
 PURPOSE	: Removes a define of a register in the driver prolog.

 PARAMETERS	: psState				- Compiler state.
			  psFixedReg			- Define to remove..
			  uRegIdx

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
	UseDefBaseDropDef(psState, &psFixedReg->asVRegUseDef[uRegIdx]);
}

IMG_INTERNAL
IMG_VOID UseDefAddFixedRegDef(PINTERMEDIATE_STATE	psState,
							  PFIXED_REG_DATA		psFixedReg,
							  IMG_UINT32			uRegIdx)
/*****************************************************************************
 FUNCTION	: UseDefAddFixedRegUse
    
 PURPOSE	: Adds a define of a register in the driver prolog.

 PARAMETERS	: psState				- Compiler state.
			  psFixedReg			- Define to add.
			  uRegIdx

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
	UseDefAddDef(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[uRegIdx], &psFixedReg->asVRegUseDef[uRegIdx]);
}

IMG_INTERNAL
IMG_VOID UseDefDropFixedRegUse(PINTERMEDIATE_STATE	psState, 
							   PFIXED_REG_DATA		psFixedReg,
							   IMG_UINT32			uRegIdx)
/*****************************************************************************
 FUNCTION	: UseDefDropFixedRegUse
    
 PURPOSE	: Remove a use of a register in the driver epilog.

 PARAMETERS	: psState				- Compiler state.
			  psFixedReg			- Use to drop.
			  uRegIdx

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
	UseDefDropUse(psState, &psFixedReg->asVRegUseDef[uRegIdx]);
}

IMG_INTERNAL
IMG_VOID UseDefAddFixedRegUse(PINTERMEDIATE_STATE	psState,
							  PFIXED_REG_DATA		psFixedReg,
							  IMG_UINT32			uRegIdx)
/*****************************************************************************
 FUNCTION	: UseDefAddFixedRegUse
    
 PURPOSE	: Adds a use of a register in the driver epilog.

 PARAMETERS	: psState				- Compiler state.
			  psFixedReg			- Use to add.
			  uRegIdx

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
	UseDefAddUse(psState, psFixedReg->uVRegType, psFixedReg->auVRegNum[uRegIdx], &psFixedReg->asVRegUseDef[uRegIdx]);
}

static
IMG_VOID UseDefRemoveUse(PINTERMEDIATE_STATE	psState, 
					     PUSEDEF_CHAIN			psUseDef,
					     PUSEDEF				psUse)
/*****************************************************************************
 FUNCTION	: UseDefRemoveUse
    
 PURPOSE	: Remove a use from the list associated with a particular register.

 PARAMETERS	: psState				- Compiler state.
			  psUseDef				- Register whose list is to be modified.
			  psUse					- Use to remove

 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Check for removing a use of a register as an index.
	*/
	if (psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_OLDDESTIDX)
	{
		ASSERT(psUseDef->uIndexUseCount > 0);
		psUseDef->uIndexUseCount--;

		/*
			If this register is no longer used as an index then remove from the list of such registers.
		*/
		if (psUseDef->uIndexUseCount == 0)
		{
			RemoveFromList(&psState->sIndexUseTempList, &psUseDef->sIndexUseTempListEntry);
		}
	}

	/*
		If removing the definition of the register then clear the stored pointer to it.	
	*/
	if (psUseDef->psDef == psUse)
	{
		psUseDef->psDef = NULL;
	}

	/*	
		Remove the use from the list associated with the register.
	*/
	RemoveFromList(&psUseDef->sList, &psUse->sListEntry);

	ASSERT(psUse->psUseDefChain == psUseDef);
	psUse->psUseDefChain = NULL;

	ASSERT(psUseDef->uUseDefCount > 0);
	psUseDef->uUseDefCount--;
}

IMG_INTERNAL
IMG_VOID UseDefDropUse(PINTERMEDIATE_STATE	psState,
					   PUSEDEF				psUse)
/*****************************************************************************
 FUNCTION	: UseDefDropUse
    
 PURPOSE	: Remove a use from the list associated with a particular register.

 PARAMETERS	: psState				- Compiler state.
			  psUse					- USE record to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;

	if (psUse->psUseDefChain == NULL)
	{
		return;
	}

	/*
		Get the USE-DEF information for this register.
	*/
	psUseDef = psUse->psUseDefChain;

	/*
		Remove it from the list associated with the register.
	*/
	UseDefRemoveUse(psState, psUseDef, psUse);

	/*
		Add to the list of possibly unused registers.
	*/
	AppendToDroppedUsesList(psState, psUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefDropArgUses(PINTERMEDIATE_STATE	psState,
						   PARGUMENT_USEDEF		psArgUse)
/*****************************************************************************
 FUNCTION	: UseDefDropArgUses
    
 PURPOSE	: Drop use information associated with an argument structure.

 PARAMETERS	: psState				- Compiler state.
			  psArgUse				- USE record associated with the argument.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefDropUse(psState, &psArgUse->sUseDef);
	UseDefDropUse(psState, &psArgUse->sIndexUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefReplaceUse(PINTERMEDIATE_STATE	psState, 
						  IMG_UINT32			uType, 
						  IMG_UINT32			uNumber,
						  PUSEDEF				psOldUse,
						  PUSEDEF				psNewUse)
/*****************************************************************************
 FUNCTION	: UseDefReplaceUse
    
 PURPOSE	: Update USE-DEF information when moving the location of a use.

 PARAMETERS	: psState				- Compiler state.
			  uType					- Register being moved.
			  uNumber
			  psOldUse				- Record for the old location of the use.
			  psNewUse				- Record for the new location of the use.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psNewUseDef;
	PUSEDEF_CHAIN	psOldUseDef;

	/*
		Remove the old use record from the list associated with the register.
	*/
	psOldUseDef = psOldUse->psUseDefChain;
	if (psOldUseDef != NULL)
	{
		UseDefRemoveUse(psState, psOldUseDef, psOldUse);
	}

	/*
		Get the USE-DEF information for the register at the new use.
	*/
	if (uType == USEASM_REGTYPE_FPINTERNAL)
	{
		psNewUseDef = UseDefBaseGet(psState, uType, uNumber, UseDefGetUseBlock(psNewUse));
	}
	else
	{
		psNewUseDef = psOldUseDef;
	}

	if (psNewUseDef != NULL)
	{
		/*
			Check the format of the register matches the format expected at this use.
		*/
		ASSERT(psNewUseDef->uType == USEASM_REGTYPE_FPINTERNAL ||
			   GetUseFmt(psNewUse) == UF_REGFORMAT_UNTYPED || 
			   GetUseFmt(psNewUse) == psNewUseDef->eFmt);

		/*
			Insert the new use record into the list associated with the register.
		*/
		UseDefInsertUse(psState, psNewUseDef, psNewUse);
	}
}

IMG_INTERNAL
IMG_VOID UseDefMoveFuncInput(PINTERMEDIATE_STATE psState, PFUNC psFunc, IMG_UINT32 uToIdx, IMG_UINT32 uFromIdx)
/*****************************************************************************
 FUNCTION	: UseDefMoveFuncInput
    
 PURPOSE	: Move the location of the definition of a function input.

 PARAMETERS	: psState	- Compiler state.
			  psFunc	- Function.
			  uToIdx	- New location.
			  uFromIdx	- Old location.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC_INOUT		psInputToMove;
	PUSEDEF_CHAIN	psUseDef;
	PUSEDEF			psOldDef;
	PUSEDEF			psNewDef;

	if (uToIdx == uFromIdx)
	{
		return;
	}

	ASSERT(uFromIdx < psFunc->sIn.uCount);
	psInputToMove = &psFunc->sIn.asArray[uFromIdx];
	psOldDef = &psFunc->sIn.asArrayUseDef[uFromIdx];
	psNewDef = &psFunc->sIn.asArrayUseDef[uToIdx];

	/*
		Get USE-DEF information for the function input.
	*/
	psUseDef = UseDefGet(psState, psInputToMove->uType, psInputToMove->uNumber);
	ASSERT(psUseDef != NULL);

	/*
		Remove the definition at the old location of the function input.
	*/
	UseDefRemoveUse(psState, psUseDef, psOldDef);

	/*
		Add the definition at the new location of the function input.
	*/
	UseDefInsertUse(psState, psUseDef, psNewDef);
	
	/*
		Move the function input in the function's array of inputs.
	*/
	psFunc->sIn.asArray[uToIdx] = *psInputToMove;

	/*	
		Clear the old entry.
	*/
	psInputToMove->uType = USC_UNDEF;
	psInputToMove->uNumber = USC_UNDEF;
}

IMG_INTERNAL
IMG_VOID UseDefReplaceDef(PINTERMEDIATE_STATE	psState, 
						  IMG_UINT32			uType, 
						  IMG_UINT32			uNumber,
						  PUSEDEF				psFromDef,
						  PUSEDEF				psToDef)
/*****************************************************************************
 FUNCTION	: UseDefReplaceDef
    
 PURPOSE	: Move the location of the definition of a register.

 PARAMETERS	: psState				- Compiler state.
			  uType					- Register which is moving.
			  uNumber
			  psFromDef				- DEF record for the old definition.
			  psToDef				- DEF record for the new definition.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psFromUseDef;
	PUSEDEF_CHAIN	psToUseDef;

	psFromUseDef = UseDefBaseGet(psState, uType, uNumber, UseDefGetUseBlock(psFromDef));
	if (psFromUseDef != NULL)
	{ 
		if (UseDefIsSSARegisterType(psState, uType))
		{
			ASSERT(psFromDef == psFromUseDef->psDef);
		}

		/*
			Remove the old record from the list associated with the register.
		*/
		UseDefRemoveUse(psState, psFromUseDef, psFromDef);
	}

	psToUseDef = UseDefBaseGet(psState, uType, uNumber, UseDefGetUseBlock(psToDef));
	if (psToUseDef != NULL)
	{
		/*
			Add the new record to the list associated with the register.
		*/
		UseDefInsertUse(psState, psToUseDef, psToDef);
	}
}

IMG_INTERNAL
IMG_VOID UseDefReplaceDest(PINTERMEDIATE_STATE	psState,
						   PINST				psMoveToInst,
						   IMG_UINT32			uMoveToDestIdx,
						   PINST				psMoveFromInst,
						   IMG_UINT32			uMoveFromDestIdx)
/*****************************************************************************
 FUNCTION	: UseDefReplaceDest
    
 PURPOSE	: Update USE-DEF information when moving a destination between
			  two instructions.

 PARAMETERS	: psState				- Compiler state.
			  psMoveToInst			- New location of the destination.
			  uMoveToDestIdx
			  psMoveFromInst		- Old location of the destination.
			  uMoveFromDestIdx

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG				psMoveTo;
	PARGUMENT_USEDEF	psMoveToUseDef;
	PARGUMENT_USEDEF	psMoveFromUseDef;

	ASSERT(uMoveToDestIdx < psMoveToInst->uDestCount);
	psMoveTo = &psMoveToInst->asDest[uMoveToDestIdx];
	psMoveToUseDef = &psMoveToInst->asDestUseDef[uMoveToDestIdx];

	psMoveFromUseDef = &psMoveFromInst->asDestUseDef[uMoveFromDestIdx];

	UseDefReplaceDef(psState,
					 psMoveTo->uType,
					 psMoveTo->uNumber,
					 &psMoveFromUseDef->sUseDef,
					 &psMoveToUseDef->sUseDef);
	UseDefReplaceUse(psState,
					 psMoveTo->uIndexType,
					 psMoveTo->uIndexNumber,
					 &psMoveFromUseDef->sIndexUseDef,
					 &psMoveToUseDef->sIndexUseDef);
}

IMG_INTERNAL
IMG_VOID UseDefDelete(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefDelete
    
 PURPOSE	: Delete USE-DEF information for a register.

 PARAMETERS	: psState				- Compiler state.
			  psUseDef				- Register to delete.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY	psNextListEntry;

	UseDefSet(psState, psUseDef->uType, psUseDef->uNumber, psUseDef->psBlock, NULL /* psUseDef */);

	if (IsEntryInList(&psState->sDroppedUsesTempList, &psUseDef->sDroppedUsesTempListEntry))
	{
		RemoveFromList(&psState->sDroppedUsesTempList, &psUseDef->sDroppedUsesTempListEntry);
	}

	if (psUseDef->uIndexUseCount > 0)
	{
		RemoveFromList(&psState->sIndexUseTempList, &psUseDef->sIndexUseTempListEntry);
	}

	if (psUseDef->eFmt == UF_REGFORMAT_C10)
	{
		RemoveFromList(&psState->sC10TempList, &psUseDef->sC10TempListEntry);
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psUseDef->eIsVector == USEDEF_ISVECTOR_YES)
	{
		RemoveFromList(&psState->sVectorTempList, &psUseDef->sVectorTempListEntry);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;

		ASSERT(psUseDef->uUseDefCount > 0);
		psUseDef->uUseDefCount--;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		psUse->psUseDefChain = NULL;
	}
	ASSERT(psUseDef->uUseDefCount == 0);

	UscFree(psState, psUseDef);
}

IMG_INTERNAL
IMG_BOOL UseDefIsReferencedOutsideBlock(PINTERMEDIATE_STATE	psState,
										PUSEDEF_CHAIN		psUseDef,
										PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: UseDefIsReferencedOutsideBlock
    
 PURPOSE	: Checks if a register is used only in a specified block.

 PARAMETERS	: psState				- Compiler state.
			  psUseDef				- Register to check.
			  psBlock				- Block to check.

 RETURNS	: TRUE if the register is used outside of the block.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF			psDef;

	psDef = psUseDef->psDef;
	if (psDef == NULL)
	{
		return IMG_TRUE;
	}
	ASSERT(psDef->eType >= DEF_TYPE_FIRST && psDef->eType <= DEF_TYPE_LAST);
	if (psDef->eType != DEF_TYPE_INST)
	{
		return IMG_TRUE;
	}
	if (psDef->u.psInst->psBlock != psBlock)
	{
		return IMG_TRUE;
	} 
	if (psDef->u.psInst->eOpcode == IDELTA)
	{
		return IMG_TRUE;
	}

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse == psUseDef->psDef)
		{
			continue;
		}
		if (psUse->eType == USE_TYPE_COND || 
			psUse->eType == USE_TYPE_SWITCH || 
			psUse->eType == USE_TYPE_FIXEDREG ||
			psUse->eType == USE_TYPE_FUNCOUTPUT)
		{
			return IMG_TRUE;
		}
		ASSERT(psUse->eType >= USE_TYPE_FIRSTINSTUSE && psUse->eType <= USE_TYPE_LASTINSTUSE);
		if (psUse->u.psInst->psBlock != psBlock)
		{
			return IMG_TRUE;
		}
		if (psUse->u.psInst->eOpcode == IDELTA)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
PARG UseDefGetInstUseLocation(PINTERMEDIATE_STATE	psState,
							  PUSEDEF				psUse,
							  PUSEDEF*				ppsIndexUse)
/*****************************************************************************
 FUNCTION	: UseDefGetInstUseLocation
    
 PURPOSE	: Get a pointer to the argument structure which a use refers to.

 PARAMETERS	: psState		- Compiler state.
			  psUse			- Use to get the argument structure for.
			  ppsIndexUse	-

 RETURNS	: A pointer to the argument structure.
*****************************************************************************/
{
	IMG_UINT32	uUseLocation = psUse->uLocation;
	PARG		psArg;

	switch (psUse->eType)
	{
		case USE_TYPE_SRC:
		case USE_TYPE_SRCIDX: 
		{
			PINST	psUseInst;

			psUseInst = psUse->u.psInst;
			
			ASSERT(uUseLocation < psUseInst->uArgumentCount);
			psArg = &psUseInst->asArg[uUseLocation]; 
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = &psUseInst->asArgUseDef[uUseLocation].sIndexUseDef;
			}
			break;
		}
		case DEF_TYPE_INST:
		case USE_TYPE_DESTIDX: 
		{
			ASSERT(uUseLocation < psUse->u.psInst->uDestCount);
			psArg = &psUse->u.psInst->asDest[uUseLocation];
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = &psUse->u.psInst->asDestUseDef[uUseLocation].sIndexUseDef;
			}
			break;
		}
		case USE_TYPE_OLDDEST: 
		case USE_TYPE_OLDDESTIDX: 
		{
			PINST	psUseInst;

			psUseInst = psUse->u.psInst;

			ASSERT(uUseLocation < psUseInst->uDestCount);
			psArg = psUseInst->apsOldDest[uUseLocation]; 
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = &psUseInst->apsOldDestUseDef[uUseLocation]->sIndexUseDef;
			}
			break;
		}
		case USE_TYPE_SWITCH: 
		{
			psArg = psUse->u.psBlock->u.sSwitch.psArg; 
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = NULL;
			}
			break;
		}
		case USE_TYPE_COND: 
		{
			psArg = &psUse->u.psBlock->u.sCond.sPredSrc; 
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = NULL;
			}
			break;
		}
		case USE_TYPE_PREDICATE: 
		{
			psArg = psUse->u.psInst->apsPredSrc[uUseLocation]; 
			if (ppsIndexUse != NULL)
			{
				*ppsIndexUse = NULL;
			}
			break;
		}
		default: imgabort();
	}

	return psArg;
}

static
IMG_VOID UseDefSubstInstructionUse(PINTERMEDIATE_STATE	psState,
								   PUSEDEF				psUseToReplace,
								   PARG					psReplacement)
/*****************************************************************************
 FUNCTION	: UseDefSubstInstructionUse
    
 PURPOSE	: Replaces one use of a variable in an instruction or
			  flow control block, by another.

 PARAMETERS	: psState				- Compiler state.
			  psUseToReplace		- Use to replace.
			  psReplacement			- Replacement.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF	psIndexUse;
	PARG	psArg;

	/*
		Get a pointer to the argument structure for the use.
	*/
	psArg = UseDefGetInstUseLocation(psState, psUseToReplace, &psIndexUse);

	/*
		If substituting an indexed value then add a new use of the register containing
		the dynamic index.
	*/
	if (psReplacement->uIndexType != USC_REGTYPE_NOINDEX)
	{
		ASSERT(psIndexUse != NULL);

		UseDefAddUse(psState,
					 psReplacement->uIndexType,
					 psReplacement->uIndexNumber,
					 psIndexUse);
	}

	if (psUseToReplace->eType == USE_TYPE_SRCIDX ||
		psUseToReplace->eType == USE_TYPE_DESTIDX ||
		psUseToReplace->eType == USE_TYPE_OLDDESTIDX)
	{
		ASSERT(psReplacement->uIndexType == USC_REGTYPE_NOINDEX);

		if (psReplacement->uType == USEASM_REGTYPE_IMMEDIATE)
		{
			IMG_UINT32	uIndexValue;

			uIndexValue = psReplacement->uNumber * psArg->uIndexStrideInBytes;

			/*
				Clear references to a dynamic index at the argument.
			*/
			psArg->uIndexType = USC_REGTYPE_NOINDEX;
			psArg->uIndexNumber = USC_UNDEF;
			psArg->uIndexArrayOffset = USC_UNDEF;
			psArg->uIndexStrideInBytes = USC_UNDEF;
			psArg->psIndexRegister = NULL;

			/*
				Convert the index to a static offset into the array.
			*/
			if (psArg->uType == USC_REGTYPE_REGARRAY)
			{
				psArg->uArrayOffset += uIndexValue;
			}
			else
			{
				psArg->uNumber += uIndexValue;
			}
		}
		else
		{
			psArg->uIndexType = psReplacement->uType;
			psArg->uIndexNumber = psReplacement->uNumber;
			psArg->uIndexArrayOffset = psReplacement->uArrayOffset;
			psArg->psIndexRegister = psReplacement->psRegister;
		}
	}
	else
	{
		UF_REGFORMAT	eArgFmt;

		eArgFmt = psArg->eFmt;
		*psArg = *psReplacement;
		if (psReplacement->eFmt == UF_REGFORMAT_UNTYPED)
		{
			psArg->eFmt = eArgFmt;
		}
	}
}

IMG_INTERNAL
IMG_VOID UseDefBaseSubstUse(PINTERMEDIATE_STATE psState, 
							PUSEDEF_CHAIN		psUseDefToReplace, 
							PUSEDEF				psUseToReplace, 
							PARG				psReplacement,
							IMG_BOOL			bDontUpdateFmt)
/*****************************************************************************
 FUNCTION	: UseDefBaseSubstUse
    
 PURPOSE	: Replaces one use of a variable by another.

 PARAMETERS	: psState				- Compiler state.
			  psUseDefToReplace		- Currently used variable.
			  psUseToReplace		- Use to replace.
			  psReplacement			- Replacement.
			  bDontUpdateFmt		- Don't update the recorded format
									for the replacement.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDefToReplaceBy;

	psUseDefToReplaceBy = 
		UseDefGetOrCreate(psState, psReplacement->uType, psReplacement->uNumber, UseDefGetUseBlock(psUseToReplace));

	/*
		Update the location of the use with the replacement.
	*/
	switch (psUseToReplace->eType)
	{
		case DEF_TYPE_INST:
		case USE_TYPE_SRC:
		case USE_TYPE_SRCIDX: 
		case USE_TYPE_DESTIDX: 
		case USE_TYPE_OLDDEST: 
		case USE_TYPE_OLDDESTIDX: 
		case USE_TYPE_SWITCH: 
		case USE_TYPE_COND: 
		case USE_TYPE_PREDICATE: 
		{
			UseDefSubstInstructionUse(psState, psUseToReplace, psReplacement);
			break;
		}
		case USE_TYPE_FIXEDREG: 
		case DEF_TYPE_FIXEDREG:
		{
			PFIXED_REG_DATA	psFixedReg = psUseToReplace->u.psFixedReg;
			IMG_UINT32		uRegIdx = psUseToReplace->uLocation;
	
			ASSERT(psReplacement->uType == psFixedReg->uVRegType);
			ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
			ASSERT(psUseDefToReplace == NULL || psFixedReg->auVRegNum[uRegIdx] == psUseDefToReplace->uNumber);
			psFixedReg->auVRegNum[uRegIdx] = psUseDefToReplaceBy->uNumber;
			break;
		}
		case DEF_TYPE_FUNCINPUT:
		{
			PFUNC		psFunc = psUseToReplace->u.psFunc;
			PFUNC_INOUT	psIn;

			ASSERT(psReplacement->uType == USEASM_REGTYPE_TEMP);

			ASSERT(psUseToReplace->uLocation < psFunc->sIn.uCount);
			psIn = &psFunc->sIn.asArray[psUseToReplace->uLocation];
			psIn->uType = psUseDefToReplaceBy->uType;
			psIn->uNumber = psUseDefToReplaceBy->uNumber;
			break;
		}
		case USE_TYPE_FUNCOUTPUT:
		{
			PFUNC		psFunc = psUseToReplace->u.psFunc;
			PFUNC_INOUT	psOut;

			ASSERT(psReplacement->uType == USEASM_REGTYPE_TEMP);

			ASSERT(psUseToReplace->uLocation < psFunc->sOut.uCount);
			psOut = &psFunc->sOut.asArray[psUseToReplace->uLocation];
			psOut->uType = psUseDefToReplaceBy->uType;
			psOut->uNumber = psUseDefToReplaceBy->uNumber;
			break;
		}
		default: imgabort();
	}

	if (psUseDefToReplaceBy != NULL)
	{
		if (!bDontUpdateFmt)
		{
			UseDefUpdateFmt(psState, psUseDefToReplaceBy, GetUseFmt(psUseToReplace));
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		UseDefUpdateIsVector(psState, psUseDefToReplaceBy, psUseToReplace);
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	if (psUseDefToReplace != NULL)
	{
		/*
			Remove the substituted use from the list associated with the old register.
		*/
		UseDefRemoveUse(psState, psUseDefToReplace, psUseToReplace);
	}

	/*
		Add the substituted use to the list associated with the new register.
	*/
	if (psUseDefToReplaceBy != NULL)
	{
		UseDefInsertUse(psState, psUseDefToReplaceBy, psUseToReplace);
	}
	else
	{
		psUseToReplace->psUseDefChain = NULL;
	}
}

IMG_INTERNAL
IMG_VOID UseDefSubstUse(PINTERMEDIATE_STATE psState, 
						PUSEDEF_CHAIN		psUseDefToReplace, 
						PUSEDEF				psUseToReplace, 
						PARG				psReplacement)
/*****************************************************************************
 FUNCTION	: UseDefSubstUse
    
 PURPOSE	: Replaces one use of a variable by another.

 PARAMETERS	: psState				- Compiler state.
			  psUseDefToReplace		- Currently used variable.
			  psUseToReplace		- Use to replace.
			  psReplacement			- Replacement.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefBaseSubstUse(psState,
					   psUseDefToReplace,
					   psUseToReplace,
					   psReplacement,
					   IMG_FALSE /* bDontUpdateFmt */);
}

#ifdef SRC_DEBUG
static
IMG_VOID SubstSourceLine(PINTERMEDIATE_STATE psState, PINST psDestInst, IMG_UINT32 uSrcLineToSubst)
/*****************************************************************************
 FUNCTION	: SubstSourceLine
    
 PURPOSE	: If an instruction marked as internally generated then associate
			  it with a specified line.

 PARAMETERS	: psState				- Compiler state.
			  psDestInst			- Instruction to modify.
			  uSrcLineToSubst		- Source line to associated with the
									instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (uSrcLineToSubst == UNDEFINED_SOURCE_LINE)
	{
		return;
	}
	if (psDestInst->uAssociatedSrcLine == psState->uTotalLines)
	{
		DecrementCostCounter(psState, psDestInst->uAssociatedSrcLine, 1);
		psDestInst->uAssociatedSrcLine = uSrcLineToSubst;
		IncrementCostCounter(psState, psDestInst->uAssociatedSrcLine, 1);
	}	
}
#endif /* SRC_DEBUG */

IMG_INTERNAL
IMG_BOOL UseDefSubstituteIntermediateRegister(PINTERMEDIATE_STATE	psState,
											  PARG					psDest,
											  PARG					psSrc,
											  #if defined(SRC_DEBUG)
											  IMG_UINT32			uSrcLineToSubst,
											  #endif /* defined(SRC_DEBUG) */
											  IMG_BOOL				bExcludePrimaryEpilog,
											  IMG_BOOL				bExcludeSecondaryEpilog,
											  IMG_BOOL				bCheckOnly)
/*****************************************************************************
 FUNCTION	: UseDefSubstituteIntermediateRegister

 PURPOSE	: Substitute all uses of one intermediate register by another.

 PARAMETERS	: psState			- Compiler state.
			  psDest			- Register to be substituted.
			  psSrc				- Replacement register.
			  bExcludePrimaryEpilog	
								- If TRUE fail and don't do the substitution
								if the register to be substituted is used
								in the driver epilog.
			  bExcludeSecondaryEpilog
								- If TRUE fail and don't do the substitution
								if the register to be substituted is a result
								of the secondary program used in the primary
								program.
				bCheckOnly		- If TRUE, don't modify/remove the instruction, 
							 	just check whether it's possible to

 RETURNS	: TRUE if the substitution was performed.
*****************************************************************************/
{
	PVREGISTER		psDestVReg;
	PUSEDEF_CHAIN	psDestUseDef;
	PUSC_LIST_ENTRY	psUseListEntry;
	PUSC_LIST_ENTRY	psNextUseListEntry;

	ASSERT(psDest->uType == USEASM_REGTYPE_TEMP || psDest->uType == USEASM_REGTYPE_PREDICATE);
	psDestVReg = GetVRegister(psState, psDest->uType, psDest->uNumber);
	psDestUseDef = psDestVReg->psUseDefChain;

	if (bExcludePrimaryEpilog || bExcludeSecondaryEpilog)
	{
		for (psUseListEntry = psDestUseDef->sList.psHead; 
			 psUseListEntry != NULL;
			 psUseListEntry = psUseListEntry->psNext)
		{
			PUSEDEF	psDestUse;

			psDestUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
			if (psDestUse->eType == USE_TYPE_FIXEDREG)
			{
				if (psDestUse->u.psFixedReg->bPrimary && bExcludePrimaryEpilog)
				{
					return IMG_FALSE;
				}
				if (!psDestUse->u.psFixedReg->bPrimary && bExcludeSecondaryEpilog)
				{
					return IMG_FALSE;
				}
			}
		}
	}

	if (bCheckOnly)
	{
		return IMG_TRUE;
	}

	/*
		Remove the substituted intermediate register from the list of intermeidate
		registers live out of the secondary update program.
	*/
	if (psDestVReg->psSecAttr != NULL)
	{
		IMG_UINT32	uNumHwRegisters = psDestVReg->psSecAttr->uNumHwRegisters;
		IMG_UINT32	uMask;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IMG_BOOL	bVector;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		IMG_UINT32	uSALiveOutMask;

		if (psDestVReg->psSecAttr->psFixedReg->auMask != NULL)
		{
			uMask = psDestVReg->psSecAttr->psFixedReg->auMask[0];
		}
		else
		{
			uMask = USC_ALL_CHAN_MASK;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		bVector = IMG_FALSE;
		if (psDestUseDef->eIsVector == USEDEF_ISVECTOR_YES)
		{
			bVector = IMG_TRUE;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			Remove the old register from the list of secondary update program results.
		*/
		ASSERT(psDestVReg->psSecAttr->eType == SAPROG_RESULT_TYPE_CALC);
		ASSERT(!psDestVReg->psSecAttr->bPartOfRange);
		DropSAProgResult(psState, psDestVReg->psSecAttr);

		/*
			Update the mask of channels live out of the secondary update program.
		*/
		uSALiveOutMask = 
			GetRegisterLiveMask(psState, 
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
								psDest->uType,
								psDest->uNumber,
								psDest->uArrayOffset);
		if (uSALiveOutMask != 0)
		{
			SetRegisterLiveMask(psState, 
								&psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
								psDest->uType,
								psDest->uNumber,
								psDest->uArrayOffset,
								0 /* uMask */);
			IncreaseRegisterLiveMask(psState, 
									 &psState->psSecAttrProg->sCfg.psExit->sRegistersLiveOut,
									 psSrc->uType,
									 psSrc->uNumber,
									 psSrc->uArrayOffset,
									 uSALiveOutMask);
		}

		/*
			Add the new register to the list of secondary update program results.
		*/
		if (psSrc->uType == USEASM_REGTYPE_TEMP)
		{
			PVREGISTER	psSrcVReg;

			psSrcVReg = GetVRegister(psState, psSrc->uType, psSrc->uNumber);
			if (psSrcVReg->psSecAttr == NULL)
			{
				BaseAddNewSAProgResult(psState,
									   psSrc->eFmt,
									   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									   bVector,
									   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									   0 /* uHwRegAlignment */,
									   uNumHwRegisters,
									   psSrc->uNumber,
									   uMask,
									   SAPROG_RESULT_TYPE_CALC,
									   IMG_FALSE /* bPartOfRange */);
			}
		}
	}

	for (psUseListEntry = psDestUseDef->sList.psHead; 
		 psUseListEntry != NULL;
		 psUseListEntry = psNextUseListEntry)
	{
		PUSEDEF	psDestUse;

		psNextUseListEntry = psUseListEntry->psNext;
		psDestUse = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);
		if (psDestUse == psDestUseDef->psDef)
		{
			continue;
		}

		#if defined(SRC_DEBUG)
		/*
			If this use/define is associated with an instruction then copy 
			source line information to the instruction.
		*/
		if (UseDefIsInstUseDef(psDestUse))
		{
			SubstSourceLine(psState, psDestUse->u.psInst, uSrcLineToSubst);
		}
		#endif /* defined(SRC_DEBUG) */

		UseDefSubstUse(psState, psDestUseDef, psDestUse, psSrc);
	}

	return IMG_TRUE;
}

typedef struct _USEDEF_SETUSES_ITERATOR
{
	/*
		Count of registers in the set.
	*/
	IMG_UINT32				uSetCount;

	/*
		Current position of the iterator.
	*/
	USEDEF_CONTAINER_TYPE	eCurrentContainerType;
	IMG_PVOID				pvCurrentContainer;

	/*
		For each member of the set: a pointer to the current position in
		its list of uses/defines.
	*/
	PUSC_LIST_ENTRY*		apsCurrentRef;

	/*
		For each member of the set: a pointer to the next reference to iterate.
	*/
	PUSC_LIST_ENTRY*		apsNextRef;
} USEDEF_SETUSES_ITERATOR;

static
IMG_VOID UseDefSetUsesIterator_Step(PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Step
    
 PURPOSE	: Update the iterator state to move onto the next container.

 PARAMETERS	: psIter		- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMember;

	psIter->eCurrentContainerType = USEDEF_CONTAINER_TYPE_UNDEF;
	psIter->pvCurrentContainer = NULL;

	for (uMember = 0; uMember < psIter->uSetCount; uMember++)
	{
		PUSC_LIST_ENTRY			psCurrentRef = psIter->apsCurrentRef[uMember];
		USEDEF_CONTAINER_TYPE	eRefContainerType;
		IMG_PVOID				pvRefContainer;
		IMG_BOOL				bLeast;
		PUSEDEF					psUseDef;

		if (psCurrentRef == NULL)
		{
			continue;
		}

		/*
			Get the object which contains the next reference to this set.
		*/
		psUseDef = IMG_CONTAINING_RECORD(psCurrentRef, PUSEDEF, sListEntry);
		eRefContainerType = GetRefContainerType(psUseDef);
		pvRefContainer = psUseDef->u.pvData;

		/*
			Choose the least (in the same ordering used by use/define lists)
			of all the objects.
		*/
		bLeast = IMG_FALSE;
		if (psIter->pvCurrentContainer != NULL)
		{
			IMG_INT32	iOrder;

			iOrder = 
				GetContainerOrder(psIter->eCurrentContainerType, 
								  psIter->pvCurrentContainer, 
								  eRefContainerType, 
								  pvRefContainer);
			if (iOrder > 0)
			{
				bLeast = IMG_TRUE;
			}
		}
		else
		{
			bLeast = IMG_TRUE;
		}
		if (bLeast)
		{
			psIter->eCurrentContainerType = eRefContainerType;
			psIter->pvCurrentContainer = pvRefContainer;
		}
	}
}

IMG_INTERNAL
IMG_VOID UseDefSetUsesIterator_Finalise(PINTERMEDIATE_STATE psState, PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Finalise
    
 PURPOSE	: Free memory associated with an iterator under the uses/defines
			  of a set of intermediate registers.

 PARAMETERS	: psState		- Compiler state.
			  psIter		- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psIter->apsCurrentRef);
	UscFree(psState, psIter->apsNextRef);
	UscFree(psState, psIter);
}

IMG_INTERNAL
PUSEDEF_SETUSES_ITERATOR UseDefSetUsesIterator_Initialize(PINTERMEDIATE_STATE		psState, 
														  IMG_UINT32				uSetCount, 
														  PUSEDEF_CHAIN				apsSet[])
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Initialize
    
 PURPOSE	: Initialize an iterator over the uses/defines of a set of
			  intermediate registers.

 PARAMETERS	: psState		- Compiler state.
			  uSetCount		- Count of intermediate register.
			  apsSet		- Intermediate registers to iterator over.

 RETURNS	: A pointer to the iterator.
*****************************************************************************/
{
	IMG_UINT32	uMember;
	PUSEDEF_SETUSES_ITERATOR	psIter;

	psIter = UscAlloc(psState, sizeof(*psIter));
	psIter->uSetCount = uSetCount;
	psIter->eCurrentContainerType = USEDEF_CONTAINER_TYPE_UNDEF;
	psIter->pvCurrentContainer = NULL;

	/*
		Allocate space to store the current and next reference for each register.
	*/
	psIter->apsCurrentRef = UscAlloc(psState, sizeof(psIter->apsCurrentRef[0]) * uSetCount);
	psIter->apsNextRef = UscAlloc(psState, sizeof(psIter->apsNextRef[0]) * uSetCount);

	/*
		Initialize information about each register.
	*/
	for (uMember = 0; uMember < uSetCount; uMember++)
	{
		if (apsSet[uMember] != NULL)
		{
			psIter->apsCurrentRef[uMember] = apsSet[uMember]->sList.psHead;
			if (psIter->apsCurrentRef[uMember] != NULL)
			{
				psIter->apsNextRef[uMember] = psIter->apsCurrentRef[uMember]->psNext;	
			}
			else
			{
				psIter->apsNextRef[uMember] = NULL;
			}
		}
		else
		{
			psIter->apsCurrentRef[uMember] = psIter->apsNextRef[uMember] = NULL;
		}
	}

	UseDefSetUsesIterator_Step(psIter);

	return psIter;
}

IMG_INTERNAL
IMG_VOID UseDefSetUsesIterator_Next(PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Next
    
 PURPOSE	: Step onto the next object which contains a reference to at least one
			  of the intermediate registers in the set.

 PARAMETERS	: psIter		- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMember;

	/*
		If the current use/define for any member of the set still points
		to the current container then move it onto the next container.
	*/
	for (uMember = 0; uMember < psIter->uSetCount; uMember++)
	{
		while (UseDefSetUsesIterator_Member_Continue(psIter, uMember))
		{
			UseDefSetUsesIterator_Member_Next(psIter, uMember);
		}
	}

	UseDefSetUsesIterator_Step(psIter);
}

IMG_INTERNAL
IMG_BOOL UseDefSetUsesIterator_Continue(PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Continue
    
 PURPOSE	: Check if there are any more objects with references to intermediate
			  registers in the set.

 PARAMETERS	: psState		- Compiler state.
			  psIter		- Iterator.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return psIter->eCurrentContainerType != USEDEF_CONTAINER_TYPE_UNDEF;
}

IMG_INTERNAL
USEDEF_CONTAINER_TYPE UseDefSetUsesIterator_CurrentContainerType(PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_CurrentContainerType
    
 PURPOSE	: Get the type of the current object we are iterating over.

 PARAMETERS	: psIter		- Iterator.

 RETURNS	: The type.
*****************************************************************************/
{
	return psIter->eCurrentContainerType;
}

IMG_INTERNAL
IMG_PVOID UseDefSetUsesIterator_CurrentContainer(PUSEDEF_SETUSES_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_CurrentContainer
    
 PURPOSE	: Get the current object we are iterating over.

 PARAMETERS	: psIter		- Iterator.

 RETURNS	: The object.
*****************************************************************************/
{
	return psIter->pvCurrentContainer;
}

IMG_INTERNAL
PUSEDEF UseDefSetUsesIterator_Member_CurrentUse(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Member_CurrentUse
    
 PURPOSE	: Get the current use/define of a member of the set within the
			  current object.

 PARAMETERS	: psIter		- Iterator.
			  uMember		- Member of the set.

 RETURNS	: A pointer to the use/define.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psCurrentRef = psIter->apsCurrentRef[uMember];

	if (!UseDefSetUsesIterator_Member_Continue(psIter, uMember))
	{
		return NULL;
	}
	return IMG_CONTAINING_RECORD(psCurrentRef, PUSEDEF, sListEntry);
}

IMG_INTERNAL
IMG_VOID UseDefSetUsesIterator_Member_Next(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Member_Next
    
 PURPOSE	: Step onto the next use/define of an intermediate register within
			  the current object.

 PARAMETERS	: psIter		- Iterator.
			  uMember		- Member of the set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!UseDefSetUsesIterator_Member_Continue(psIter, uMember))
	{
		return;
	}
	psIter->apsCurrentRef[uMember] = psIter->apsNextRef[uMember];
	if (psIter->apsCurrentRef[uMember] != NULL)
	{
		psIter->apsNextRef[uMember] = psIter->apsCurrentRef[uMember]->psNext;
	}
	else
	{
		psIter->apsNextRef[uMember] = NULL;
	}
}

IMG_INTERNAL
IMG_BOOL UseDefSetUsesIterator_Member_Continue(PUSEDEF_SETUSES_ITERATOR psIter, IMG_UINT32 uMember)
/*****************************************************************************
 FUNCTION	: UseDefSetUsesIterator_Member_Continue
    
 PURPOSE	: Check if there are any more uses/defines of an intermediate
			  register within the current object.

 PARAMETERS	: psIter		- Iterator.
			  uMember		- Member of the set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF	psCurrentRef;
	
	if (psIter->apsCurrentRef[uMember] == NULL)
	{
		return IMG_FALSE;
	}
	psCurrentRef = IMG_CONTAINING_RECORD(psIter->apsCurrentRef[uMember], PUSEDEF, sListEntry);

	if (psCurrentRef->u.pvData != psIter->pvCurrentContainer)
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

static
IMG_BOOL IsFunctionInputsOnly(PINTERMEDIATE_STATE		psState,
							  IMG_UINT32				uSetCount,
							  PUSEDEF_SETUSES_ITERATOR	psIter)
/*****************************************************************************
 FUNCTION	: IsFunctionInputsOnly
    
 PURPOSE	: Check that references to a set of registers related to a function
			  are only as function inputs (never as function outputs).

 PARAMETERS	: psState		- Compiler state.
			  uSetCount		- Count of registers in the set.
			  psIter		- Iterator currently positioned at references to
							the set in a function.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{	
	IMG_UINT32	uMember;

	for (uMember = 0; uMember < uSetCount; uMember++)
	{
		while (UseDefSetUsesIterator_Member_Continue(psIter, uMember))
		{
			PUSEDEF	psUseDef;
	
			psUseDef = UseDefSetUsesIterator_Member_CurrentUse(psIter, uMember);
			if (psUseDef->eType == USE_TYPE_FUNCOUTPUT)
			{
				return IMG_FALSE;
			}
			ASSERT(psUseDef->eType == DEF_TYPE_FUNCINPUT);

			UseDefSetUsesIterator_Member_Next(psIter, uMember);
		}
	}

	return IMG_TRUE;
}

static	
IMG_BOOL GetUsesInCurrentInst(PINTERMEDIATE_STATE		psState,
							  PINST						psCurrentInst,
							  PUSEDEF_SETUSES_ITERATOR	psIter,
							  IMG_UINT32				uSetCount,
							  IMG_BOOL					(*pfProcess)(PINTERMEDIATE_STATE, PINST, PCSOURCE_VECTOR, IMG_PVOID),
							  IMG_PVOID					pvUserData)
/*****************************************************************************
 FUNCTION	: GetUsesInCurrentInst
    
 PURPOSE	: Call a supplied function for all the uses of a set of registers
			  in the current instruction.

 PARAMETERS	: psState		- Compiler state.
			  psCurrentInst	- Instruction.
			  psIter		- Iterator over all the uses of the set.
			  uSetCount		- Count of registers in the set.
			  pfProcess		- Function to call for each instruction which uses
							some of the registers.
			  pvUserData	- Callback function context.

 RETURNS	: FALSE if the callback function returned FALSE for the instruction or
			  if any of the registers are used other than as sources.
*****************************************************************************/
{
	IMG_UINT32		uMember;
	SOURCE_VECTOR	sUsedSources;
	IMG_BOOL		bUses;

	InitSourceVector(psState, psCurrentInst, &sUsedSources);
	bUses = IMG_FALSE;
	for (uMember = 0; uMember < uSetCount; uMember++)
	{
		while (UseDefSetUsesIterator_Member_Continue(psIter, uMember))
		{
			PUSEDEF	psUseDef;
	
			psUseDef = UseDefSetUsesIterator_Member_CurrentUse(psIter, uMember);
			if (psUseDef->eType == USE_TYPE_SRC)
			{
				ASSERT(psUseDef->uLocation < psCurrentInst->uArgumentCount);
				ASSERT(CheckSourceVector(&sUsedSources, psUseDef->uLocation) == 0);
				AddToSourceVector(&sUsedSources, psUseDef->uLocation);
				bUses = IMG_TRUE;
			}
			else if (psUseDef->eType != DEF_TYPE_INST)
			{
				return IMG_FALSE;
			}
	
			UseDefSetUsesIterator_Member_Next(psIter, uMember);
		}
	}

	if (bUses)
	{
		/*
			Call the supplied function with details of the uses of the set of
			registers in the current instruction.
		*/
		if (!pfProcess(psState, psCurrentInst, &sUsedSources, pvUserData))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL UseDefForAllUsesOfSet(PINTERMEDIATE_STATE	psState,
							   IMG_UINT32			uSetCount,
							   PUSEDEF_CHAIN		apsSet[],
							   IMG_BOOL				(*pfProcess)(PINTERMEDIATE_STATE, PINST, PCSOURCE_VECTOR, IMG_PVOID),
							   IMG_PVOID			pvUserData)
/*****************************************************************************
 FUNCTION	: UseDefForAllUsesOfSet
    
 PURPOSE	: Call a supplied function for each instruction which uses any of a
			  set of registers as sources. 

 PARAMETERS	: psState		- Compiler state.
			  uSetCount		- Count of registers.
			  apsSet		- Pointer to each registers.
			  pfProcess		- Function to call for each instruction which uses
							some of the registers.
			  pvUserData	- Callback function context.

 RETURNS	: FALSE if the callback function returned FALSE for any instruction or
			  if any of the registers are used other than as sources.
*****************************************************************************/
{
	PUSEDEF_SETUSES_ITERATOR	psIter;
	IMG_BOOL					bRet;

	psIter = UseDefSetUsesIterator_Initialize(psState, uSetCount, apsSet);
	bRet = IMG_TRUE;
	while (UseDefSetUsesIterator_Continue(psIter))
	{
		USEDEF_CONTAINER_TYPE	eCurrentContainerType;

		eCurrentContainerType = UseDefSetUsesIterator_CurrentContainerType(psIter);
		switch (eCurrentContainerType)
		{
			case USEDEF_CONTAINER_TYPE_FIXEDREG:
			{
				PFIXED_REG_DATA	psFixedReg;

				psFixedReg = (PFIXED_REG_DATA)UseDefSetUsesIterator_CurrentContainer(psIter);

				/*
					Check the registers in the set are shader inputs only (not shader outputs).
				*/
				if (psFixedReg->bLiveAtShaderEnd)
				{
					bRet = IMG_FALSE;
				}
				break;
			}
			case USEDEF_CONTAINER_TYPE_FUNCTION:
			{
				/*
					Check registers in the set are inputs to the function only.
				*/
				if (!IsFunctionInputsOnly(psState, uSetCount, psIter))
				{
					bRet = IMG_FALSE;
				}
				break;
			}
			case USEDEF_CONTAINER_TYPE_INST:
			{
				PINST	psCurrentInst;

				/*
					Check references in the instruction are only as sources or destinations and pass
					the set of sources where the set is used to the callback function.
				*/
				psCurrentInst = (PINST)UseDefSetUsesIterator_CurrentContainer(psIter);
				if (!GetUsesInCurrentInst(psState, psCurrentInst, psIter, uSetCount, pfProcess, pvUserData))
				{
					bRet = IMG_FALSE;
				}
				break;
			}
			case USEDEF_CONTAINER_TYPE_BLOCK:
			{
				/*
					References to a register in a block are always uses.
				*/
				bRet = IMG_FALSE;
				break;
			}
			default: imgabort();
		}

		if (!bRet)
		{
			break;
		}

		UseDefSetUsesIterator_Next(psIter);
	}
	UseDefSetUsesIterator_Finalise(psState, psIter);

	return bRet;
}

#if defined(UF_TESTBENCH)
static
IMG_BOOL OccursInList(PUSC_LIST psList, PUSC_LIST_ENTRY psListEntryToFound)
/*****************************************************************************
 FUNCTION	: OccursInList
    
 PURPOSE	: Checks if a list entry appears in a list.

 PARAMETERS	: psList				- List to check.
			  psListEntryToFound	- List entry to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		if (psListEntry == psListEntryToFound)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID VerifyUse(PINTERMEDIATE_STATE		psState, 
				   IMG_UINT32				uType, 
				   IMG_UINT32				uNumber, 
				   PVREGISTER				psVReg,
				   PCODEBLOCK				psInstBlock,
				   PUSEDEF					psUse,
				   IMG_PVOID				pvUseData, 
				   USEDEF_TYPE				eUseType, 
				   IMG_UINT32				uUseIdx,
				   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				   USEDEF_ISVECTOR			eUseIsVector,
				   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				   UF_REGFORMAT				eUseFmt)
/*****************************************************************************
 FUNCTION	: VerifyUse
    
 PURPOSE	: Checks that the USE-DEF information corresponding to a use exists.

 PARAMETERS	: psState				- Compiler state.
			  uType					- Used register.
			  uNumber
			  psInstBlock			- If the use is in an instruction then the block
									containing the instruction.
			  psUse					- Use record.
			  psVReg
			  pvUseData				- Location of the use.
			  eUseType
			  uUseIdx
			  eUseFmt				- Format of the data at the use.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;	

	psUseDef = UseDefBaseGet(psState, uType, uNumber, psInstBlock);
	ASSERT(psUse->psUseDefChain == psUseDef);

	if (psUseDef == NULL)
	{
		ASSERT(psVReg == NULL);
		return;
	}

	if (psVReg != NULL)
	{
		ASSERT(psVReg->psUseDefChain == psUseDef);
	}
	else
	{
		if (eUseType >= USE_TYPE_FIRSTINSTUSE && eUseType <= USE_TYPE_LASTINSTUSE)
		{
			ASSERT(uType != USEASM_REGTYPE_TEMP && uType != USEASM_REGTYPE_PREDICATE);
		}
	}

	/*
		Check the format of the register expected at this use matches the format of register.
	*/
	if (UseDefIsSSARegisterType(psState, uType))
	{
		ASSERT(eUseFmt == UF_REGFORMAT_UNTYPED || psUseDef->eFmt == eUseFmt || psUseDef->eFmt == UF_REGFORMAT_UNTYPED);
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check the size of the register expected at this use matches the size of the register.
	*/
	ASSERT(eUseIsVector == USEDEF_ISVECTOR_UNKNOWN || 
		   eUseIsVector == psUseDef->eIsVector ||
		   psUseDef->eIsVector == USEDEF_ISVECTOR_NOTRECORDED);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Check the USE record is correctly initialized.
	*/
	ASSERT(psUse->eType == eUseType);
	ASSERT(psUse->uLocation == uUseIdx);
	ASSERT(psUse->u.pvData == pvUseData);

	/*
		Check the USE record appears in the list associated with the register.
	*/
	ASSERT(OccursInList(&psUseDef->sList, &psUse->sListEntry));

	/*
		If this is a use as an index then check the register is in the list of indices.
	*/
	if (eUseType == USE_TYPE_SRCIDX || eUseType == USE_TYPE_DESTIDX || eUseType == USE_TYPE_OLDDESTIDX)
	{
		ASSERT(psUseDef->uIndexUseCount > 0);
	}
}

static
IMG_VOID VerifyDef(PINTERMEDIATE_STATE		psState, 
				   IMG_UINT32				uType, 
				   IMG_UINT32				uNumber, 
				   PVREGISTER				psVReg,
				   PCODEBLOCK				psInstBlock,
				   PUSEDEF					psDef,
				   IMG_PVOID				pvDefData, 
				   USEDEF_TYPE				eDefType, 
				   IMG_UINT32				uDefIdx,
				   #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				   USEDEF_ISVECTOR			eDefIsVector,
				   #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				   UF_REGFORMAT				eDefFmt)
/*****************************************************************************
 FUNCTION	: VerifyDef
    
 PURPOSE	: Check that a define of a register is consistent.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Defined register.
			  uNumber
			  psVReg
			  psInstBlock		- If the define is in an instruction then
								the flow control block containing the
								instruction.
			  psDef				- DEF record.
			  pvDefData			- Location of the define.
			  eDefType
			  uDefIdx
			  eDefIsVector		- 
			  eDefFmt			- Format of the data at the define.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psDestUseDef;

	psDestUseDef = UseDefBaseGet(psState, uType, uNumber, psInstBlock);
	ASSERT(psDef->psUseDefChain == psDestUseDef);

	if (psDestUseDef == NULL)
	{
		ASSERT(psVReg == NULL);
		return;
	}

	if (psVReg != NULL)
	{
		ASSERT(psVReg->psUseDefChain == psDestUseDef);
	}
	else
	{
		if (eDefType == DEF_TYPE_INST)
		{
			ASSERT(uType != USEASM_REGTYPE_TEMP && uType != USEASM_REGTYPE_PREDICATE);
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	ASSERT(eDefIsVector == USEDEF_ISVECTOR_UNKNOWN || 
		   eDefIsVector == psDestUseDef->eIsVector ||
		   psDestUseDef->eIsVector == USEDEF_ISVECTOR_NOTRECORDED);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Check the USE record is correctly initialized.
	*/
	ASSERT(psDef->eType == eDefType);
	ASSERT(psDef->uLocation == uDefIdx);
	ASSERT(psDef->u.pvData == pvDefData);

	/*
		Check the DEF record appears in the list associated with the register.
	*/
	ASSERT(OccursInList(&psDestUseDef->sList, &psDef->sListEntry));

	if (UseDefIsSSARegisterType(psState, uType))
	{
		ASSERT(eDefFmt == UF_REGFORMAT_UNTYPED || psDestUseDef->eFmt == eDefFmt || psDestUseDef->eFmt == UF_REGFORMAT_UNTYPED);
		ASSERT(psDef == psDestUseDef->psDef);
	}
}

static
IMG_VOID VerifyUseDefBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNULL)
/*****************************************************************************
 FUNCTION	: VerifyUseDefBP
    
 PURPOSE	: Check USE/DEF information is consistent for all instructions in
			  a block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to check.
			  pvNULL			- Ignored.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	PVR_UNREFERENCED_PARAMETER(pvNULL);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32		uSrcIdx;
		IMG_UINT32		uDestIdx;
		IMG_UINT32		uPred;

		for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
		{
			PARG				psSrc = &psInst->asArg[uSrcIdx];
			PARGUMENT_USEDEF	psSrcUseDef = &psInst->asArgUseDef[uSrcIdx];
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			USEDEF_ISVECTOR		eVectorSource;

			eVectorSource = USEDEF_ISVECTOR_NO;
			if (IsVectorSource(psState, psInst, uSrcIdx))
			{
				eVectorSource = USEDEF_ISVECTOR_YES;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

			VerifyUse(psState, 
					  psSrc->uType, 
					  psSrc->uNumber, 
					  psSrc->psRegister,
					  psInst->psBlock,
					  &psSrcUseDef->sUseDef,
					  psInst, 
					  USE_TYPE_SRC, 
					  uSrcIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  eVectorSource,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  psSrc->eFmt);
			VerifyUse(psState,	
					  psSrc->uIndexType, 
					  psSrc->uIndexNumber, 
					  psSrc->psIndexRegister,
					  psInst->psBlock,
					  &psSrcUseDef->sIndexUseDef,
					  psInst, 
					  USE_TYPE_SRCIDX, 
					  uSrcIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  USEDEF_ISVECTOR_NO,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  UF_REGFORMAT_UNTYPED);
		}
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				VerifyUse(psState, 
						  psInst->apsPredSrc[uPred]->uType,
						  psInst->apsPredSrc[uPred]->uNumber, 
						  psInst->apsPredSrc[uPred]->psRegister,
						  psInst->psBlock,
						  psInst->apsPredSrcUseDef[uPred],
						  psInst, 
						  USE_TYPE_PREDICATE, 
						  uPred,
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  USEDEF_ISVECTOR_NO,
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  UF_REGFORMAT_UNTYPED);
			}
		}
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG				psOldDest = psInst->apsOldDest[uDestIdx];
			PARG				psDest = &psInst->asDest[uDestIdx];
			PARGUMENT_USEDEF	psDestDef = &psInst->asDestUseDef[uDestIdx];
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)		
			USEDEF_ISVECTOR		eVectorDest;

			eVectorDest = USEDEF_ISVECTOR_NO;
			if (IsVectorDest(psState, psInst, uDestIdx))
			{
				eVectorDest = USEDEF_ISVECTOR_YES;
			}
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

			if (psOldDest != NULL)
			{
				PARGUMENT_USEDEF	psOldDefUse = psInst->apsOldDestUseDef[uDestIdx];

				VerifyUse(psState, 
						  psOldDest->uType, 
						  psOldDest->uNumber, 
						  psOldDest->psRegister,
						  psInst->psBlock,
						  &psOldDefUse->sUseDef,
						  psInst, 
						  USE_TYPE_OLDDEST, 
						  uDestIdx,
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  eVectorDest,
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  psOldDest->eFmt);
				VerifyUse(psState, 
						  psOldDest->uIndexType, 
						  psOldDest->uIndexNumber, 
						  psOldDest->psIndexRegister,
						  psInst->psBlock,
						  &psOldDefUse->sIndexUseDef,
						  psInst, 
						  USE_TYPE_OLDDESTIDX, 
						  uDestIdx,
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  USEDEF_ISVECTOR_NO,
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  UF_REGFORMAT_UNTYPED);
			}
			VerifyUse(psState, 
					  psDest->uIndexType, 
					  psDest->uIndexNumber, 
					  psDest->psIndexRegister,
					  psInst->psBlock,
					  &psDestDef->sIndexUseDef,
					  psInst, 
					  USE_TYPE_DESTIDX, 
					  uDestIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  USEDEF_ISVECTOR_NO,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  UF_REGFORMAT_UNTYPED);

			VerifyDef(psState,
					  psDest->uType,
					  psDest->uNumber,
					  psDest->psRegister,
					  psInst->psBlock,
					  &psDestDef->sUseDef,
					  psInst,
					  DEF_TYPE_INST,
					  uDestIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  eVectorDest,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  psDest->eFmt);
		}
	}

	if (psBlock->eType == CBTYPE_COND)
	{
		VerifyUse(psState, 
				  psBlock->u.sCond.sPredSrc.uType, 
				  psBlock->u.sCond.sPredSrc.uNumber, 
				  psBlock->u.sCond.sPredSrc.psRegister,
				  NULL /* psInstBlock */,
				  &psBlock->u.sCond.sPredSrcUse,
				  psBlock,
				  USE_TYPE_COND,
				  USC_UNDEF /* uUseIdx */,
				  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				  USEDEF_ISVECTOR_NO,
				  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				  UF_REGFORMAT_UNTYPED);
	}
	else if (psBlock->eType == CBTYPE_SWITCH)
	{
		PARG	psSwitchArg = psBlock->u.sSwitch.psArg;

		VerifyUse(psState, 
				  psSwitchArg->uType, 
				  psSwitchArg->uNumber, 
				  psSwitchArg->psRegister,
				  NULL /* psInstBlock */,
				  &psBlock->u.sSwitch.sArgUse.sUseDef,
				  psBlock,
				  USE_TYPE_SWITCH,
				  USC_UNDEF /* uUseIdx */,
				  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				  USEDEF_ISVECTOR_NO,
				  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				  psSwitchArg->eFmt);
	}
}

static
IMG_VOID VerifyUseEntry(PINTERMEDIATE_STATE	psState,
						PUSEDEF_CHAIN		psUseDef,
						PUSEDEF				psUse)
/*****************************************************************************
 FUNCTION	: VerifyUseEntry
    
 PURPOSE	: Check an entry in the list of uses of a register does in
			  in fact use the register.

 PARAMETERS	: psState			- Compiler state.
			  psUseDef			- Register to check.
			  psUse				- Use to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psUse->eType)
	{
		case USE_TYPE_SRC:
		case USE_TYPE_OLDDEST:
		case USE_TYPE_PREDICATE:
		{
			PARG	psArg;
			PINST	psUseInst = psUse->u.psInst;

			if (psUse->eType == USE_TYPE_SRC)
			{
				ASSERT(psUse->uLocation < psUseInst->uArgumentCount);
				psArg = &psUseInst->asArg[psUse->uLocation];
			}
			else if (psUse->eType == USE_TYPE_PREDICATE)
			{
				psArg = psUseInst->apsPredSrc[psUse->uLocation];
			}
			else
			{
				ASSERT(psUse->uLocation < psUseInst->uDestCount);
				psArg = psUseInst->apsOldDest[psUse->uLocation];
				ASSERT(psArg != NULL);
			}

			ASSERT(psArg->uType == psUseDef->uType);
			ASSERT(psArg->uNumber == psUseDef->uNumber);
			break;
		}
		
		case USE_TYPE_SRCIDX:
		case USE_TYPE_OLDDESTIDX:
		case USE_TYPE_DESTIDX:
		{
			PARG	psArg;
			PINST	psUseInst = psUse->u.psInst;

			if (psUse->eType == USE_TYPE_SRCIDX)
			{
				ASSERT(psUse->uLocation < psUseInst->uArgumentCount);
				psArg = &psUseInst->asArg[psUse->uLocation];
			}
			else if (psUse->eType == USE_TYPE_DESTIDX)
			{
				ASSERT(psUse->uLocation < psUseInst->uDestCount);
				psArg = &psUseInst->asDest[psUse->uLocation];
			}
			else
			{
				ASSERT(psUse->uLocation < psUseInst->uDestCount);
				psArg = psUseInst->apsOldDest[psUse->uLocation];
				ASSERT(psArg != NULL);
			}

			ASSERT(psArg->uIndexType == psUseDef->uType);
			ASSERT(psArg->uIndexNumber == psUseDef->uNumber);
			break;
		}
		case USE_TYPE_FUNCOUTPUT:
		{
			PFUNC		psFunc = psUse->u.psFunc;
			IMG_UINT32	uOutIdx = psUse->uLocation;

			ASSERT(uOutIdx < psFunc->sOut.uCount);
			ASSERT(psFunc->sOut.asArray[uOutIdx].uType == psUseDef->uType);
			ASSERT(psFunc->sOut.asArray[uOutIdx].uNumber == psUseDef->uNumber);
			break;
		}
		case USE_TYPE_FIXEDREG:
		{
			PFIXED_REG_DATA	psFixedReg = psUse->u.psFixedReg;
			IMG_UINT32		uRegIdx = psUse->uLocation;

			ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
			if (psUseDef->uType == USC_REGTYPE_REGARRAY)
			{
				PUSC_VEC_ARRAY_REG	psArray;

				ASSERT(psUseDef->uNumber == psFixedReg->uRegArrayIdx);
				ASSERT(psUseDef->uNumber < psState->uNumVecArrayRegs);
				psArray = psState->apsVecArrayReg[psUseDef->uNumber];
				ASSERT(psFixedReg->uVRegType == psArray->uRegType);
			}
			else
			{
				ASSERT(psFixedReg->uVRegType == psUseDef->uType);
				ASSERT(psFixedReg->auVRegNum[uRegIdx] == psUseDef->uNumber);
			}
			break;
		}
		case USE_TYPE_SWITCH:
		{
			PCODEBLOCK	psBlock = psUse->u.psBlock;

			ASSERT(psBlock->eType == CBTYPE_SWITCH);
			ASSERT(psBlock->u.sSwitch.psArg->uType == psUseDef->uType);
			ASSERT(psBlock->u.sSwitch.psArg->uNumber == psUseDef->uNumber);
			break;
		}
		case USE_TYPE_COND:
		{
			PCODEBLOCK	psBlock = psUse->u.psBlock;

			ASSERT(psBlock->eType == CBTYPE_COND);
			ASSERT(psUseDef->uType == USEASM_REGTYPE_PREDICATE);
			ASSERT(psBlock->u.sCond.sPredSrc.uType == USEASM_REGTYPE_PREDICATE);
			ASSERT(psBlock->u.sCond.sPredSrc.uNumber == psUseDef->uNumber);
			break;
		}
		default: imgabort();
	}
}

static
IMG_VOID VerifyDefEntry(PINTERMEDIATE_STATE	psState,
						PUSEDEF_CHAIN		psUseDef,
						PUSEDEF				psDef)
/*****************************************************************************
 FUNCTION	: VerifyDefEntry
    
 PURPOSE	: Check an entry in the list of defines of a register does in
			  in fact define the register.

 PARAMETERS	: psState			- Compiler state.
			  psUseDef			- Register to check.
			  psDef				- Define to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (psDef->eType)
	{
		case DEF_TYPE_INST:
		{
			PINST		psDefInst = psDef->u.psInst;
			IMG_UINT32	uDestIdx = psDef->uLocation;
			PARG		psDest;

			ASSERT(uDestIdx < psDefInst->uDestCount);
			psDest = &psDefInst->asDest[uDestIdx];

			ASSERT(psDest->uType == psUseDef->uType);
			ASSERT(psDest->uNumber == psUseDef->uNumber);
			break;
		}
		case DEF_TYPE_FUNCINPUT:
		{
			PFUNC		psFunc = psDef->u.psFunc;
			IMG_UINT32	uOutIdx = psDef->uLocation;

			ASSERT(uOutIdx < psFunc->sIn.uCount);
			ASSERT(psFunc->sIn.asArray[uOutIdx].uType == psUseDef->uType);
			ASSERT(psFunc->sIn.asArray[uOutIdx].uNumber == psUseDef->uNumber);
			break;
		}
		case DEF_TYPE_FIXEDREG:
		{
			PFIXED_REG_DATA	psFixedReg = psDef->u.psFixedReg;
			IMG_UINT32		uRegIdx = psDef->uLocation;

			ASSERT(uRegIdx < psFixedReg->uConsecutiveRegsCount);
			if (psUseDef->uType == USC_REGTYPE_REGARRAY)
			{
				PUSC_VEC_ARRAY_REG	psArray;

				ASSERT(psUseDef->uNumber == psFixedReg->uRegArrayIdx);
				ASSERT(psUseDef->uNumber < psState->uNumVecArrayRegs);
				psArray = psState->apsVecArrayReg[psUseDef->uNumber];
				ASSERT(psFixedReg->uVRegType == psArray->uRegType);
			}
			else
			{
				ASSERT(psFixedReg->uVRegType == psUseDef->uType);
				ASSERT(psFixedReg->auVRegNum[uRegIdx] == psUseDef->uNumber);
			}
			break;
		}
		default: imgabort();
	}

}

static
IMG_VOID VerifyUseDefList(PINTERMEDIATE_STATE		psState,
						  PUSEDEF_CHAIN				psUseDefChain,
						  IMG_UINT32				uRegType,
						  IMG_UINT32				uRegNumber)
/*****************************************************************************
 FUNCTION	: VerifyUseDefList
    
 PURPOSE	:

 PARAMETERS	: psState			- Compiler state.
			  psUseDefChain		- List to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psUseDefChain != NULL)
	{
		PUSC_LIST_ENTRY	psListEntry;
		IMG_UINT32		uIndexUseCount;

		ASSERT(psUseDefChain->uType == uRegType);
		ASSERT(psUseDefChain->uNumber == uRegNumber);

		uIndexUseCount = 0;
		for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			
			/*
				Check the entries in the list are still correctly sorted.
			*/
			if (psListEntry->psNext != NULL)
			{
				ASSERT(CmpUse(psListEntry, psListEntry->psNext) <= 0);
			}

			ASSERT(psUseDef->psUseDefChain == psUseDefChain);

			if (psUseDef->eType >= DEF_TYPE_FIRST && psUseDef->eType <= DEF_TYPE_LAST)
			{
				if (UseDefIsSSARegisterType(psState, uRegType))
				{
					ASSERT(psUseDef == psUseDefChain->psDef);
				}

				VerifyDefEntry(psState, psUseDefChain, psUseDef);
			}
			else
			{
				ASSERT(psUseDef->eType >= USE_TYPE_FIRST && psUseDef->eType <= USE_TYPE_LAST);
				VerifyUseEntry(psState, psUseDefChain, psUseDef);
			}

			if (psUseDef->eType == USE_TYPE_SRCIDX || 
				psUseDef->eType == USE_TYPE_DESTIDX || 
				psUseDef->eType == USE_TYPE_OLDDESTIDX)
			{
				uIndexUseCount++;
			}
		}
		ASSERT(psUseDefChain->uIndexUseCount == uIndexUseCount);

		/*
			If this register is used as a dynamic index then check it appears in the list of such registers.
		*/
		if (uIndexUseCount > 0)
		{
			IMG_BOOL	bFound;

			bFound = IMG_FALSE;
			for (psListEntry = psState->sIndexUseTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				if (psListEntry == &psUseDefChain->sIndexUseTempListEntry)
				{
					bFound = IMG_TRUE;
					break;
				}
			}
			ASSERT(bFound);
		}
	}
}

static
IMG_VOID VerifyUseDefLists(PINTERMEDIATE_STATE		psState, 
						   USC_PARRAY				psVRegArray, 
						   IMG_UINT32				uRegType,
						   IMG_UINT32				uCount)
/*****************************************************************************
 FUNCTION	: VerifyUseDefLists
    
 PURPOSE	:

 PARAMETERS	: psState			- Compiler state.
			  psVRegArray		- Array to check.
			  uRegType			- Type of registers to check.
			  uCount			- Count of entries in the array.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	for (uIdx = 0; uIdx < uCount; uIdx++)
	{
		PVREGISTER	psVReg;

		psVReg = ArrayGet(psState, psVRegArray, uIdx);
		if (psVReg != NULL)
		{
			VerifyUseDefList(psState, psVReg->psUseDefChain, uRegType, uIdx);
		}
	}
}

static
IMG_VOID VerifyFixedReg(PINTERMEDIATE_STATE psState, PUSC_LIST psFixedRegList)
/*****************************************************************************
 FUNCTION	: VerifyFixedReg
    
 PURPOSE	: Verify USE-DEF information for registers used/defined in driver epilog/prolog.

 PARAMETERS	: psState			- Compiler state.
			  psFixedRegList	- Linked list of fixed registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psFixedRegList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFIXED_REG_DATA	psFixedReg; 
		IMG_UINT32		uRegIdx;

		psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

		if (psFixedReg->uRegArrayIdx != USC_UNDEF)
		{
			continue;
		}

		for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
		{
			if (psFixedReg->bLiveAtShaderEnd)
			{	
				VerifyUse(psState,
						  psFixedReg->uVRegType,
						  psFixedReg->auVRegNum[uRegIdx],
						  NULL /* psRegister */,
						  NULL /* psInstBlock */,
						  &psFixedReg->asVRegUseDef[uRegIdx],
						  psFixedReg,
						  USE_TYPE_FIXEDREG,
						  uRegIdx,
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  psFixedReg->bVector ? USEDEF_ISVECTOR_YES : USEDEF_ISVECTOR_NO,
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  UF_REGFORMAT_UNTYPED);
			}
			else
			{
				VerifyDef(psState,
						  psFixedReg->uVRegType,
						  psFixedReg->auVRegNum[uRegIdx],
						  NULL /* psRegister */,
						  NULL /* psInstBlock */,
						  &psFixedReg->asVRegUseDef[uRegIdx],
						  psFixedReg,
						  DEF_TYPE_FIXEDREG,
						  uRegIdx,
						  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						  USEDEF_ISVECTOR_UNKNOWN,
						  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						  UF_REGFORMAT_UNTYPED);
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID VerifyUseDef(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: VerifyUseDef
    
 PURPOSE	: Check that information about USEs/DEFs of registers is
			  consistent.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PFUNC		psFunc;
	IMG_UINT32	uArrayIdx;

	if ((psState->uFlags2 & USC_FLAGS2_ASSIGNED_PRIMARY_REGNUMS) != 0)
	{
		return;
	}

	if ((psState->uFlags2 & USC_FLAGS2_PRED_USE_DEF_INFO_VALID) != 0)
	{
		VerifyUseDefLists(psState, psState->psPredVReg, USEASM_REGTYPE_PREDICATE, psState->uNumPredicates);
	}
	if ((psState->uFlags2 & USC_FLAGS2_TEMP_USE_DEF_INFO_VALID) != 0)
	{
		VerifyUseDefLists(psState, psState->psTempVReg, USEASM_REGTYPE_TEMP, psState->uNumRegisters);
	}
	for (uArrayIdx = 0; uArrayIdx < psState->uNumVecArrayRegs; uArrayIdx++)
	{
		PUSC_VEC_ARRAY_REG	psArray = psState->apsVecArrayReg[uArrayIdx];

		if (psArray != NULL)
		{
			VerifyUseDefList(psState, psArray->psUseDef, USC_REGTYPE_REGARRAY, uArrayIdx);
			VerifyUseDefList(psState, psArray->psBaseUseDef, USC_REGTYPE_ARRAYBASE, uArrayIdx);
		}
	}

	/*
		Check USEs in the driver epilog/DEFs in the driver prolog.
	*/
	VerifyFixedReg(psState, &psState->sFixedRegList);
	VerifyFixedReg(psState, &psState->sSAProg.sFixedRegList);

	/*
		Check internal function inputs/outputs.
	*/
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		IMG_UINT32	uParamIdx;

		for (uParamIdx = 0; uParamIdx < psFunc->sIn.uCount; uParamIdx++)
		{
			PFUNC_INOUT	psIn = &psFunc->sIn.asArray[uParamIdx];

			VerifyDef(psState,
					  psIn->uType,
					  psIn->uNumber,
					  NULL /* psRegister */,
					  NULL /* psInstBlock */,
					  &psFunc->sIn.asArrayUseDef[uParamIdx],
					  psFunc,
					  DEF_TYPE_FUNCINPUT,
					  uParamIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  USEDEF_ISVECTOR_UNKNOWN,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  psIn->eFmt);
		}
		for (uParamIdx = 0; uParamIdx < psFunc->sOut.uCount; uParamIdx++)
		{
			PFUNC_INOUT	psOut = &psFunc->sOut.asArray[uParamIdx];

			VerifyUse(psState,
					  psOut->uType,
					  psOut->uNumber,
					  NULL /* psRegister */,
					  NULL /* psInstBlock */,
					  &psFunc->sOut.asArrayUseDef[uParamIdx],
					  psFunc,
					  USE_TYPE_FUNCOUTPUT,
					  uParamIdx,
					  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					  USEDEF_ISVECTOR_UNKNOWN,
					  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					  psOut->eFmt);
			
		}
	}

	/*
		Check USEs/DEFs in all intermediate instructions.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, VerifyUseDefBP, IMG_TRUE /* bIncludeCalls */, NULL /* pvUserData */);
}
#endif /* defined(UF_TESTBENCH) */

static
IMG_VOID UseDefCheckUnusedArg(PINTERMEDIATE_STATE psState, PINST psInst, PARG psArg)
/*****************************************************************************
 FUNCTION	: UseDefCheckUnusedArg
    
 PURPOSE	: Add any register referenced in an instruction argument to the list of
			  registers which are potentially unused.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psArgUseDef;

	psArgUseDef = UseDefBaseGet(psState, psArg->uType, psArg->uNumber, psInst->psBlock);
	if (psArgUseDef != NULL)
	{
		AppendToDroppedUsesList(psState, psArgUseDef);
	}
}

static
IMG_VOID UseDefCheckUnusedInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: UseDefCheckUnusedInst
    
 PURPOSE	: Check for an instruction whose results are unused and if they
			  are then delete the instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bLive;
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;

	if (GetBit(psInst->auFlag, INST_DUMMY_FETCH))
	{
		return;
	}
	if (psInst->eOpcode == ICALL)
	{
		return;
	}
			
	/*
		Check if any channels written by the instruction are used later.
	*/
	bLive = IMG_FALSE;
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		if ((psInst->auLiveChansInDest[uDestIdx] & psInst->auDestMask[uDestIdx]) != 0)
		{
			bLive = IMG_TRUE;
		}
	}
	if (!bLive)
	{
		/*
			Replace the instruction by MOVs copying the parts of the destinations
			which are not written by the instruction.
		*/
		CopyPartiallyOverwrittenData(psState, psInst);

		/*
			Free the unused instruction.
		*/
		RemoveInst(psState, psInst->psBlock, psInst);
		FreeInst(psState, psInst);
		return;
	}

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psOldDest != NULL)
		{
			UseDefCheckUnusedArg(psState, psInst, psOldDest);
		}
	}
	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		PARG			psSrc = &psInst->asArg[uSrcIdx];

		UseDefCheckUnusedArg(psState, psInst, psSrc);
	}
}

IMG_INTERNAL
IMG_UINT32 UseDefGetUsedChanMask(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefGetUsedChanMask
    
 PURPOSE	: Get the overall mask of channels used from a register at all
			  use sites.

 PARAMETERS	: psState		- Compiler state.
			  psUseDef		- Register to get the used channels for.

 RETURNS	: The mask of used channels.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uUsedChanMask;

	uUsedChanMask = 0;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType >= USE_TYPE_FIRST && psUse->eType <= USE_TYPE_LAST)
		{
			uUsedChanMask |= GetUseChanMask(psState, psUse);
			if (uUsedChanMask == USC_ALL_CHAN_MASK)
			{
				break;
			}
		}
	}
	
	return uUsedChanMask;
}

IMG_INTERNAL
IMG_VOID UseDefDropUnusedTemps(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: UseDefDropUnusedTemps
    
 PURPOSE	: Check for any instructions with unused results and delete the
			  instructions.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	while (!IsListEmpty(&psState->sDroppedUsesTempList))
	{
		PUSC_LIST_ENTRY	psListEntry;
		USC_LIST sUnusedInstList;

		InitializeList(&sUnusedInstList);
		while ((psListEntry = RemoveListHead(&psState->sDroppedUsesTempList)) != NULL)
		{
			PUSEDEF_CHAIN	psUseDef;
			PUSEDEF			psDef;

			psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sDroppedUsesTempListEntry);
			ClearListEntry(psListEntry);

			psDef = psUseDef->psDef;
	
			if (psDef != NULL && psDef->eType == DEF_TYPE_INST)
			{
				PINST		psDefInst = psDef->u.psInst;
				IMG_UINT32	uDefIdx = psDef->uLocation;
				IMG_UINT32	uUsedChanMask;

				uUsedChanMask = UseDefGetUsedChanMask(psState, psUseDef);
				if (psDefInst->auLiveChansInDest[uDefIdx] != uUsedChanMask)
				{
					if (GetBit(psDefInst->auFlag, INST_ISUNUSED) == 0)
					{
						AppendToList(&sUnusedInstList, &psDefInst->sAvailableListEntry);
						SetBit(psDefInst->auFlag, INST_ISUNUSED, 1);
					}
					psDefInst->auLiveChansInDest[uDefIdx] = uUsedChanMask;
				}
			}
		}
		
		while ((psListEntry = RemoveListHead(&sUnusedInstList)) != NULL)
		{
			PINST	psInst;

			psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
			SetBit(psInst->auFlag, INST_ISUNUSED, 0);
			UseDefCheckUnusedInst(psState, psInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID UseDefInitialize(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: UseDefInitialize
    
 PURPOSE	: Initializes data structures used to record USE-DEF information.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	InitializeList(&psState->sIndexUseTempList);
	InitializeList(&psState->sC10TempList);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	InitializeList(&psState->sVectorTempList);
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	psState->uFlags2 |= USC_FLAGS2_TEMP_USE_DEF_INFO_VALID | USC_FLAGS2_PRED_USE_DEF_INFO_VALID;
}

IMG_INTERNAL
IMG_VOID UseDefFreeAll(PINTERMEDIATE_STATE psState, IMG_UINT32 uType)
/*****************************************************************************
 FUNCTION	: UseDefFreeAll
    
 PURPOSE	: Frees all USE-DEF information for a particular register type.

 PARAMETERS	: psState		- Compiler state.
			  uType			- Register type to free information for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uCount;
	IMG_UINT32	uIdx;

	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			uCount = psState->uNumRegisters;
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			uCount = psState->uNumPredicates;
			break;
		}
		case USC_REGTYPE_REGARRAY:
		case USC_REGTYPE_ARRAYBASE:
		{
			uCount = psState->uNumVecArrayRegs;
			break;
		}
		default: imgabort();
	}

	for (uIdx = 0; uIdx < uCount; uIdx++)
	{
		PUSEDEF_CHAIN	psUseDef;

		psUseDef = UseDefGet(psState, uType, uIdx);
		if (psUseDef != NULL)
		{
			UseDefDelete(psState, psUseDef);
		}
	}

	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:
		{
			ASSERT((psState->uFlags2 & USC_FLAGS2_TEMP_USE_DEF_INFO_VALID) != 0);
			psState->uFlags2 &= ~USC_FLAGS2_TEMP_USE_DEF_INFO_VALID;
			break;
		}
		case USEASM_REGTYPE_PREDICATE:
		{
			ASSERT((psState->uFlags2 & USC_FLAGS2_PRED_USE_DEF_INFO_VALID) != 0);
			psState->uFlags2 &= ~USC_FLAGS2_PRED_USE_DEF_INFO_VALID;
			break;
		}
		case USC_REGTYPE_REGARRAY:
		case USC_REGTYPE_ARRAYBASE:
		{
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_VOID UseDefDeinitialize(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: UseDefDeinitialize
    
 PURPOSE	: Deinitializes data structures used to record USE-DEF information.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefFreeAll(psState, USC_REGTYPE_REGARRAY);
	UseDefFreeAll(psState, USC_REGTYPE_ARRAYBASE);
	UseDefFreeAll(psState, USEASM_REGTYPE_TEMP);
}

IMG_INTERNAL
IMG_BOOL UseDefIsDef(PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefIsDef
    
 PURPOSE	: Check if a USE-DEF structure represents a definition.

 PARAMETERS	: psUseDef		- Structure to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psUseDef->eType >= DEF_TYPE_FIRST && psUseDef->eType <= DEF_TYPE_LAST)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL UseDefIsUse(PUSEDEF psUseDef)
/*****************************************************************************
 FUNCTION	: UseDefIsUse
    
 PURPOSE	: Check if a USE-DEF structure represents a use.

 PARAMETERS	: psUseDef		- Structure to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psUseDef->eType >= USE_TYPE_FIRST && psUseDef->eType <= USE_TYPE_LAST)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL UseDefGetSingleRegisterUse(PINTERMEDIATE_STATE psState,
									PVREGISTER psRegister,
									IMG_PVOID* ppvUse,
									PUSEDEF_TYPE peUseType,
									IMG_PUINT32 puUseSrcIdx)
/*****************************************************************************
 FUNCTION	: UseDefGetSingleUse
    
 PURPOSE	: Checks for a register which is used only once.

 PARAMETERS	: psState		- Compiler state.
			  psRegister	- Register to check.
			  ppvUse		- Returns the type specific pointer to the 
							container of the use.
			  peUseType		- Returns the type of use.
			  puUseSrcIdx	- Returns the location of the use.

 RETURNS	: TRUE if the register is used only once.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSEDEF			psUse;
	PUSC_LIST_ENTRY	psUseListEntry;

	PVR_UNREFERENCED_PARAMETER(psState);

	if (psRegister == NULL)
	{
		return IMG_FALSE;
	}

	psUseDef = psRegister->psUseDefChain;
	if (!(psUseDef->uType == USEASM_REGTYPE_TEMP || psUseDef->uType == USEASM_REGTYPE_PREDICATE))
	{
		return IMG_FALSE;
	}

	psUse = NULL;
	for (psUseListEntry = psUseDef->sList.psHead; psUseListEntry != NULL; psUseListEntry = psUseListEntry->psNext)
	{	
		PUSEDEF	psUseOrDef;

		psUseOrDef = IMG_CONTAINING_RECORD(psUseListEntry, PUSEDEF, sListEntry);

		if (psUseOrDef == psUseDef->psDef)
		{
			continue;
		}

		/*
			Check for more than one use.
		*/
		if (psUse != NULL)
		{
			return IMG_FALSE;
		}

		psUse = psUseOrDef;
	}

	/*
		Check for no uses at all.
	*/
	if (psUse == NULL)
	{
		return IMG_FALSE;
	}
	
	*ppvUse = psUse->u.pvData;
	*peUseType = psUse->eType;
	*puUseSrcIdx = psUse->uLocation;
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL UseDefGetSingleUse(PINTERMEDIATE_STATE psState, 
					        PARG psDest,
						    PINST* ppsUseInst,
						    PUSEDEF_TYPE peUseType,
						    IMG_PUINT32 puUseSrcIdx)
/*****************************************************************************
 FUNCTION	: UseDefGetSingleUse
    
 PURPOSE	: Checks for a register which is used only once.

 PARAMETERS	: psState		- Compiler state.
			  psDest		- Register to check.
			  ppsUseInst	- Returns the instruction where the register is used.
			  peUseType		- Returns the type of use.
			  puUseSrcIdx	- Returns the location of the use.

 RETURNS	: TRUE if the register is used only once.
*****************************************************************************/
{
	IMG_PVOID	pvUse;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseLocation;

	if (!UseDefGetSingleRegisterUse(psState, psDest->psRegister, &pvUse, &eUseType, &uUseLocation))
	{
		return IMG_FALSE;
	}
	if (!(eUseType >= USE_TYPE_FIRSTINSTUSE && eUseType <= USE_TYPE_LASTINSTUSE))
	{
		return IMG_FALSE;
	}
	*ppsUseInst = (PINST)pvUse;
	*peUseType = eUseType;
	*puUseSrcIdx = uUseLocation;
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_UINT32 UseDefGetSingleSourceUse(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefGetSingleSourceUse
    
 PURPOSE	: Returns the source to an instruction where a register is used.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  psDest		- Register to check.

 RETURNS	: The index of the source or USC_UNDEF if the register is also used
			  in a different instruction, used more than once or used not as
			  a source.
*****************************************************************************/
{
	PINST		psUseInst;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseSrcIdx;

	if (!UseDefGetSingleUse(psState, psDest, &psUseInst, &eUseType, &uUseSrcIdx))
	{
		return USC_UNDEF;
	}
	if (psUseInst == psInst && eUseType == USE_TYPE_SRC)
	{
		return uUseSrcIdx;
	}
	return USC_UNDEF;
}

IMG_INTERNAL
IMG_BOOL UseDefIsPredicateSourceOnly(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefIsPredicateSourceOnly
    
 PURPOSE	: Checks if a register is used only as a predicate to control
			  the execution of a specified instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  psDest		- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST		psUseInst;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseSrcIdx;

	if (!UseDefGetSingleUse(psState, psDest, &psUseInst, &eUseType, &uUseSrcIdx))
	{
		return IMG_FALSE;
	}
	if (psUseInst == psInst && eUseType == USE_TYPE_PREDICATE)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL UseDefIsPartialDestOnly(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uDestIdx, PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefIsPartialDestOnly
    
 PURPOSE	: Checks if a register is used only as the source for conditionally/partially
			  written channels in a specified instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uDestIdx		- Destination to check.
			  psDest		- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PINST		psUseInst;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseSrcIdx;

	if (!UseDefGetSingleUse(psState, psDest, &psUseInst, &eUseType, &uUseSrcIdx))
	{
		return IMG_FALSE;
	}
	if (psUseInst == psInst && eUseType == USE_TYPE_OLDDEST && uUseSrcIdx == uDestIdx)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL UseDefIsSingleSourceUse(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx, PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefIsSingleSourceUse
    
 PURPOSE	: Checks if a register is used only in a specified source and
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uSrcIdx		- Index of the source to check.
			  psDest		- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (UseDefGetSingleSourceUse(psState, psInst, psDest) == uSrcIdx)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_BOOL NextGPIUseIsDef(PINTERMEDIATE_STATE psState, IMG_UINT32 uIRegIdx, PINST psInst)
/*****************************************************************************
 FUNCTION	: NextGPIUseIsDef
    
 PURPOSE	: Checks if the next use of an internal register after (and including)
			  a given instruction is a def (it can also be a use at the same time).

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uSrcIdx		- Index of the source to check.
			  psInst		- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psIRegUseDefChain;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psUseInst, psNextUseInst, psNextDefInst;

	psIRegUseDefChain = psInst->psBlock->apsIRegUseDef[uIRegIdx];

	/*
		Check for no uses/defines of the internal register in this block.
	*/
	ASSERT(psIRegUseDefChain != NULL);

	psNextUseInst = psNextDefInst = NULL;

	/*
		Look for the first instruction which either uses or defines the internal register and is either the
		specified instruction or a later instruction.
	*/
	for (psListEntry = psIRegUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		psUseInst = UseDefGetInst(psUseDef);
		ASSERT(psUseInst != NULL);
		psUseInst = psUseInst->psGroupParent;

		ASSERT(psUseInst->psBlock == psInst->psBlock);
		if (psUseInst->uBlockIndex >= psInst->uBlockIndex)
		{
			if (UseDefIsUse(psUseDef))
			{
				if (psNextUseInst == NULL)
				{
					psNextUseInst = psUseInst;
				}
			}
			else
			{
				ASSERT(UseDefIsDef(psUseDef));
				if (psNextDefInst == NULL)
				{
					psNextDefInst = psUseInst;
				}
			}
		}

		if (psNextUseInst != NULL && psNextDefInst != NULL)
		{
			break;
		}
	}

	if (psNextDefInst == NULL)
	{
		return IMG_FALSE;
	}

	if (psNextUseInst == NULL)
	{
		return IMG_TRUE;
	}

	return (psNextUseInst->uBlockIndex >= psNextDefInst->uBlockIndex);
}

IMG_INTERNAL
IMG_BOOL UseDefIsSingleSourceRegisterUse(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uSrcIdx)
/*****************************************************************************
 FUNCTION	: UseDefIsSingleSourceRegisterUse
    
 PURPOSE	: Checks if a register is used only in a specified source and
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uSrcIdx		- Index of the source to check.
			  psRegister	- Register to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_PVOID	pvUse;
	USEDEF_TYPE	eUseType;
	IMG_UINT32	uUseLocation;
	PVREGISTER	psRegister;

	ASSERT(uSrcIdx < psInst->uArgumentCount);

	if (psInst->asArg[uSrcIdx].uType == USEASM_REGTYPE_FPINTERNAL)
	{
		if (!NextGPIUseIsDef(psState, psInst->asArg[uSrcIdx].uNumber, psInst))
		{
			return IMG_FALSE;
		}
	}
	else
	{
		psRegister = psInst->asArg[uSrcIdx].psRegister;

		if (!UseDefGetSingleRegisterUse(psState, psRegister, &pvUse, &eUseType, &uUseLocation))
		{
			return IMG_FALSE;
		}
		if (!(eUseType == USE_TYPE_SRC && ((PINST)pvUse) == psInst && uUseLocation == uSrcIdx))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

static
IMG_BOOL UseDefGetSourceOrPartialDestUseMask(PINTERMEDIATE_STATE	psState, 
											 PINST					psInst, 
											 IMG_UINT32				uType,
											 IMG_UINT32				uNumber,
											 IMG_PUINT32			puSrcMask,
											 IMG_PUINT32			puPartialDestMask)
/*****************************************************************************
 FUNCTION	: UseDefGetSourceOrPartialDestUseMask
    
 PURPOSE	: Returns the mask of sources or partial destinations to an instruction which use a register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uType			- Register to check.
			  uNumber
			  puSrcMask		- Returns the mask of sources using the register.
			  puPartialDestMask
							- Returns the mask of partial destinations using the register.

 RETURNS	: IMG_FALSE if the register is used in a different instruction or not as a source or
			  partial destination.
****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32		uSrcMask;
	IMG_UINT32		uPartialDestMask;

	if (uType != USEASM_REGTYPE_TEMP)
	{
		return IMG_FALSE;
	}

	psUseDef = UseDefGet(psState, uType, uNumber);

	uSrcMask = uPartialDestMask = 0;
	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse == psUseDef->psDef)
		{
			ASSERT(UseDefIsDef(psUse));
			continue;
		}

		ASSERT(UseDefIsUse(psUse));
		if (psUse->eType != USE_TYPE_SRC && psUse->eType != USE_TYPE_OLDDEST)
		{
			return IMG_FALSE;
		}
		if (psUse->u.psInst != psInst)
		{
			return IMG_FALSE;
		}
		ASSERT(psUse->uLocation <= BITS_PER_UINT);
		if (psUse->eType == USE_TYPE_SRC)
		{
			uSrcMask |= (1U << psUse->uLocation);
		}
		else
		{
			uPartialDestMask |= (1U << psUse->uLocation);
		}
	}
 
	*puSrcMask = uSrcMask;
	*puPartialDestMask = uPartialDestMask;

	return IMG_TRUE;
}	

IMG_INTERNAL
IMG_UINT32 UseDefGetRegisterSourceUseMask(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: UseDefGetRegisterSourceUseMask
    
 PURPOSE	: Returns the mask of sources to an instruction which use a register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  uType			- Register to check.
			  uNumber

 RETURNS	: Either the mask of sources or USC_UNDEF if the register is used
			  in a different instruction or not as a source.
*****************************************************************************/
{
	IMG_UINT32	uSrcMask;
	IMG_UINT32	uPartialDestMask;

	if (!UseDefGetSourceOrPartialDestUseMask(psState, psInst, uType, uNumber, &uSrcMask, &uPartialDestMask))
	{
		return USC_UNDEF;
	}
	if (uSrcMask == 0 || uPartialDestMask != 0)
	{
		return USC_UNDEF;
	}
	return uSrcMask;
}

IMG_INTERNAL
IMG_UINT32 UseDefGetSourceUseMask(PINTERMEDIATE_STATE psState, PINST psInst, PARG psDest)
/*****************************************************************************
 FUNCTION	: UseDefGetSourceUseMask
    
 PURPOSE	: Returns the mask of sources to an instruction which use a register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to check.
			  psDest		- Register to check.

 RETURNS	: Either the mask of sources or USC_UNDEF if the register is used
			  in a different instruction or not as a source.
*****************************************************************************/
{
	return UseDefGetRegisterSourceUseMask(psState, psInst, psDest->uType, psDest->uNumber);
}	

IMG_INTERNAL
PFUNC UseDefGetUseLocation(PUSEDEF psUse)
/*****************************************************************************
 FUNCTION	: UseDefGetUseLocation
    
 PURPOSE	: Returns the function which contains a use of a register.

 PARAMETERS	: psUse			- Use to get the location of.

 RETURNS	: A pointer to the function containing the use.
*****************************************************************************/
{
	if (
			(
				psUse->eType >= USE_TYPE_FIRSTINSTUSE && 
				psUse->eType <= USE_TYPE_LASTINSTUSE
			 ) ||
			 psUse->eType == DEF_TYPE_INST
	   )
	{
		return psUse->u.psInst->psBlock->psOwner->psFunc;
	}
	else if (psUse->eType == USE_TYPE_COND || psUse->eType == USE_TYPE_SWITCH)
	{
		return psUse->u.psBlock->psOwner->psFunc;
	}
	else
	{
		return NULL;
	}
}

IMG_INTERNAL
PINST UseDefGetDefInstFromChain(PUSEDEF_CHAIN psUseDef, IMG_PUINT32 puDestIdx)
/*****************************************************************************
 FUNCTION	: UseDefGetDefInstFromChain
    
 PURPOSE	: Returns the instruction which defines a register.

 PARAMETERS	: psUseDef		- Identifies the register.
			  puDestIdx		- If non-NULL returns the index of the destination
							which defines the register.

 RETURNS	: Either the instruction which defines the register or NULL if
			  the register is either uninitialized or defined in the driver
			  prolog.
*****************************************************************************/
{
	PUSEDEF			psDef;

	psDef = psUseDef->psDef;

	if (psDef == NULL || psDef->eType != DEF_TYPE_INST)
	{
		return NULL;
	}

	if (puDestIdx != NULL)
	{
		*puDestIdx = psDef->uLocation;
	}

	return psDef->u.psInst;
}

IMG_INTERNAL
PINST UseDefGetDefInst(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber, IMG_PUINT32 puDestIdx)
/*****************************************************************************
 FUNCTION	: UseDefGetDefInst
    
 PURPOSE	: Returns the instruction which defines a register.

 PARAMETERS	: psState		- Compiler state.
			  uType			- Identifies the register.
			  uNumber
			  puDestIdx		- If non-NULL returns the index of the destination
							which defines the register.

 RETURNS	: Either the instruction which defines the register or NULL if
			  the register is either uninitialized or defined in the driver
			  prolog.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psUseDef;

	psUseDef = UseDefGet(psState, uType, uNumber);
	if (psUseDef == NULL)
	{
		return NULL;
	}

	return UseDefGetDefInstFromChain(psUseDef, puDestIdx);
}

static IMG_VOID UseDefRemoveFromLists(PINTERMEDIATE_STATE	psState, 
									  PCODEBLOCK			psOldBlock,
									  PUSEDEF				psUseOrDef,
									  IMG_UINT32			uRegisterType,
									  IMG_UINT32			uRegisterNumber)
/*****************************************************************************
 FUNCTION	: UseDefRemoveInstArgs
    
 PURPOSE	: Removes a USE/DEF record from the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psOldBlock		- Points to the block the instruction was
								previously inserted in.
			  psUseOrDef		- USE/DEF record.
			  uRegisterType		- Used/defined register.
			  uRegisterNumber

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psOldUseDefChain;

	if (uRegisterType == USEASM_REGTYPE_FPINTERNAL)
	{	
		psOldUseDefChain = UseDefBaseGet(psState, uRegisterType, uRegisterNumber, psOldBlock);

		ASSERT(psUseOrDef->psUseDefChain == psOldUseDefChain);
	}
	else
	{
		psOldUseDefChain = psUseOrDef->psUseDefChain;
	}

	if (psOldUseDefChain != NULL)
	{
		RemoveFromList(&psOldUseDefChain->sList, &psUseOrDef->sListEntry);

		ASSERT(psOldUseDefChain->uUseDefCount > 0);
		psOldUseDefChain->uUseDefCount--;
	}
}

static IMG_VOID UseDefInsertIntoLists(PINTERMEDIATE_STATE	psState, 
									  PCODEBLOCK			psNewBlock,
									  PUSEDEF				psUseOrDef,
									  IMG_UINT32			uRegisterType,
									  IMG_UINT32			uRegisterNumber)
/*****************************************************************************
 FUNCTION	: UseDefInsertIntoLists
    
 PURPOSE	: Adds a USE/DEF record to the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psOldBlock		- Points to the block the instruction is inserted
								into.
			  psUseOrDef		- USE/DEF record.
			  uRegisterType		- Used/defined register.
			  uRegisterNumber

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psNewUseDefChain;

	if (uRegisterType == USEASM_REGTYPE_FPINTERNAL)
	{	
		psNewUseDefChain = UseDefGetOrCreate(psState, uRegisterType, uRegisterNumber, psNewBlock);
		psUseOrDef->psUseDefChain = psNewUseDefChain;
	}
	else
	{
		psNewUseDefChain = psUseOrDef->psUseDefChain;
	}

	if (psNewUseDefChain != NULL)
	{
		InsertInListSorted(&psNewUseDefChain->sList, CmpUse, &psUseOrDef->sListEntry);
		psNewUseDefChain->uUseDefCount++;
	}
}

static IMG_VOID UseDefRemoveArgFromLists(PINTERMEDIATE_STATE	psState, 
										 PCODEBLOCK				psOldBlock,
										 PARG					psArg,
										 PARGUMENT_USEDEF		psArgUseOrDef)
/*****************************************************************************
 FUNCTION	: UseDefRemoveInstArgs
    
 PURPOSE	: Removes all USE/DEF information associated with an instruction
			  argument from the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psOldBlock		- Points to the block the instruction was
								previously inserted in.
			  psArg				- Instruction argument.
			  psArgUseOrDef		- USE/DEF information for the argument.
			  

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegisterNumber;

	if (psState->uFlags & USC_FLAGS_MOESTAGE)
	{
		uRegisterNumber = psArg->uNumberPreMoe;
	}
	else
	{
		uRegisterNumber = psArg->uNumber;
	}

	UseDefRemoveFromLists(psState, 
						  psOldBlock, 
						  &psArgUseOrDef->sUseDef, 
						  psArg->uType, 
						  uRegisterNumber);
	UseDefRemoveFromLists(psState, 
						  psOldBlock, 
						  &psArgUseOrDef->sIndexUseDef,
						  psArg->uIndexType, 
						  psArg->uIndexNumber);
}

static IMG_VOID UseDefInsertArgIntoLists(PINTERMEDIATE_STATE	psState, 
										 PCODEBLOCK				psNewBlock,
										 PARG					psArg,
										 PARGUMENT_USEDEF		psArgUseOrDef)
/*****************************************************************************
 FUNCTION	: UseDefInsertArgIntoLists
    
 PURPOSE	: Inserts all USE/DEF information associated with an instruction
			  argument into the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psNewBlock		- Points to the block the instruction is
								inserted in.
			  psArg				- Instruction argument.
			  psArgUseOrDef		- USE/DEF information for the argument.
			  

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegisterNumber;

	if (psState->uFlags & USC_FLAGS_MOESTAGE)
	{
		uRegisterNumber = psArg->uNumberPreMoe;
	}
	else
	{
		uRegisterNumber = psArg->uNumber;
	}

	UseDefInsertIntoLists(psState, 
						  psNewBlock, 
						  &psArgUseOrDef->sUseDef, 
						  psArg->uType, 
						  uRegisterNumber);
	UseDefInsertIntoLists(psState, 
						  psNewBlock, 
						  &psArgUseOrDef->sIndexUseDef,
						  psArg->uIndexType, 
						  psArg->uIndexNumber);
}

static
IMG_VOID UseDefRemoveInstArgs(PINTERMEDIATE_STATE psState, PINST psInst, PCODEBLOCK psOldBlock)
/*****************************************************************************
 FUNCTION	: UseDefRemoveInstArgs
    
 PURPOSE	: Removes all USE/DEF information associated with an instruction
			  from the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psOldBlock		- Points to the block the instruction was
								previously inserted in.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;
	IMG_UINT32	uPred;

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		UseDefRemoveArgFromLists(psState, 
								 psOldBlock, 
								 &psInst->asDest[uDestIdx], 
								 &psInst->asDestUseDef[uDestIdx]);
		if (psInst->apsOldDestUseDef[uDestIdx] != NULL)
		{
			UseDefRemoveArgFromLists(psState, 
									 psOldBlock,
									 psInst->apsOldDest[uDestIdx],
									 psInst->apsOldDestUseDef[uDestIdx]);
		}
	}
	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		UseDefRemoveArgFromLists(psState, 
								 psOldBlock, 
								 &psInst->asArg[uSrcIdx], 
								 &psInst->asArgUseDef[uSrcIdx]);
	}
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		if (psInst->apsPredSrcUseDef[uPred] != NULL)
		{
			UseDefRemoveFromLists(psState, 
								  psOldBlock,
								  psInst->apsPredSrcUseDef[uPred], 
								  psInst->apsPredSrc[uPred]->uType, 
								  psInst->apsPredSrc[uPred]->uNumber);
		}
	}
}

static
IMG_VOID UseDefInsertInstArgs(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: UseDefInsertInstArgs
    
 PURPOSE	: Inserts all USE/DEF information associated with an instruction
			  into the list for the used/defined register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uSrcIdx;
	PCODEBLOCK	psNewBlock;
	IMG_UINT32	uPred;

	if (psInst->psGroupParent == NULL)
	{
		psNewBlock = psInst->psBlock;
	}
	else
	{
		psNewBlock = psInst->psGroupParent->psBlock;
	}

	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		UseDefInsertArgIntoLists(psState, 
								 psNewBlock, 
								 &psInst->asDest[uDestIdx], 
								 &psInst->asDestUseDef[uDestIdx]);
		if (psInst->apsOldDestUseDef[uDestIdx] != NULL)
		{
			UseDefInsertArgIntoLists(psState, 
									 psNewBlock, 
									 psInst->apsOldDest[uDestIdx],
									 psInst->apsOldDestUseDef[uDestIdx]);
		}
	}
	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		UseDefInsertArgIntoLists(psState, 
								 psNewBlock,
								 &psInst->asArg[uSrcIdx], 
								 &psInst->asArgUseDef[uSrcIdx]);
	}

	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		if (psInst->apsPredSrcUseDef[uPred] != NULL)
		{
			UseDefInsertIntoLists(psState, 
								  psNewBlock,
								  psInst->apsPredSrcUseDef[uPred], 
								  psInst->apsPredSrc[uPred]->uType, 
								  psInst->apsPredSrc[uPred]->uNumber);
		}
	}
}

IMG_INTERNAL
IMG_VOID UseDefModifyInstructionBlock(PINTERMEDIATE_STATE psState, PINST psInst, PCODEBLOCK psOldBlock)
/*****************************************************************************
 FUNCTION	: UseDefModifyInstructionBlock
    
 PURPOSE	: Updates USE/DEF information when removing from an instruction
			  or inserting it into a flow control block.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psOldBlock		- Points to the block the instruction was
								previously inserted in.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefRemoveInstArgs(psState, psInst, psOldBlock);
	UseDefInsertInstArgs(psState, psInst);
}

IMG_INTERNAL
IMG_BOOL UseDefIsNextReferenceAsUse(PINTERMEDIATE_STATE psState, PUSC_LIST_ENTRY psListEntry)
/*****************************************************************************
 FUNCTION	: UseDefIsNextReferenceAsUse
    
 PURPOSE	: Checks whether the next reference to an internal register
			  is a use (not a define).

 PARAMETERS	: psState			- Compiler state.
			  psListEntry		- Point to start searching from.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	for ( ; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUseDef->eType == DEF_TYPE_INST)
		{
			return IMG_FALSE;
		}
		else
		{
			ASSERT(psUseDef->eType == USE_TYPE_SRC || psUseDef->eType == USE_TYPE_OLDDEST);

			if (GetUseChanMask(psState, psUseDef) != 0)
			{
				return IMG_TRUE;
			}
		}
	}
	
	/*
		No more uses before the end of the block.
	*/
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID UseDefIterateIRegLiveness_Next(PINTERMEDIATE_STATE		psState, 
										PIREGLIVENESS_ITERATOR	psIterator,
										PINST					psCurrentInst)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_Next
    
 PURPOSE	: Updates the iterator state to the internal registers which are
			  live before the next instruction.

 PARAMETERS	: psState			- Compiler state.
			  psIterator		- Iterator state.
			  psCurrentInst		- Instruction which has just been processed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIRegIdx;

	for (uIRegIdx = 0; uIRegIdx < psState->uGPISizeInScalarRegs; uIRegIdx++)
	{
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psIterator->apsUseDefEntries[uIRegIdx]; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUseDef;
			PINST	psUseInst;

			psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			psUseInst = UseDefGetInst(psUseDef);
			ASSERT(psUseInst != NULL);

			/*
				Stop once we see a use/define in a different instruction.
			*/
			if (psUseInst->psGroupParent != psCurrentInst)
			{
				break;
			}

			if (psUseDef->eType == DEF_TYPE_INST)
			{
				IMG_UINT32	uDestIdx;
				PARG		psDest;

				uDestIdx = psUseDef->uLocation;
				ASSERT(uDestIdx < psUseInst->uDestCount);
				psDest = &psUseInst->asDest[uDestIdx];

				/*
					If the internal register is defined at the current instruction then it is live
					before the next instruction.
				*/
				if ((psUseInst->auDestMask[uDestIdx] & psUseInst->auLiveChansInDest[uDestIdx]) != 0)
				{
					psIterator->uIRegLiveMask |= (1U << uIRegIdx);
					if (psDest->eFmt == UF_REGFORMAT_C10)
					{
						psIterator->uIRegC10Mask |= (1U << uIRegIdx);
					}
					if (GetBit(psUseInst->auFlag, INST_SKIPINV))
					{
						psIterator->uIRegSkipInvalidMask |= (1U << uIRegIdx);
					}
				}
			}
			else
			{
				ASSERT(psUseDef->eType == USE_TYPE_SRC || psUseDef->eType == USE_TYPE_OLDDEST);
			}
		}


		/*
			Store the first use/define either at or after the next instruction.
		*/
		psIterator->apsUseDefEntries[uIRegIdx] = psListEntry;

		/*
			If there are no more uses of the internal register (before the next define or the
			end of the block) then it is no longer live.
		*/
		if (!UseDefIsNextReferenceAsUse(psState, psListEntry))
		{
			psIterator->uIRegLiveMask &= ~(1U << uIRegIdx);
			psIterator->uIRegC10Mask &= ~(1U << uIRegIdx);
			psIterator->uIRegSkipInvalidMask &= ~(1U << uIRegIdx);
		}
	}

	/*
		Check no internal registers are live out of the block.
	*/
	if (psCurrentInst->psNext == NULL)
	{
		ASSERT(psIterator->uIRegLiveMask == 0);
	}
}

IMG_INTERNAL
IMG_VOID UseDefGetIRegLivenessSpanOverInterval(PINTERMEDIATE_STATE	psState,
											   PINST				psStartInst,
											   PINST				psEndInst, 
											   IMG_UINT32			uIReg,
											   PINST 				*ppsLastDefForSpan,
											   PINST				*ppsLastRefForSpan)
/*****************************************************************************
 FUNCTION	: UseDefGetIRegLivenessSpanOverInterval
    
 PURPOSE	: Returns most recent defining instruction and last referencing 
			  Instruction for an given interval and IReg.
			  
			  Computes a covering span for given interval for IReg
			  
			  IReg can be defined it self in interval, in that case referenced
			  instruction near end or after the end of interval is return.
			  
 PARAMETERS	: psState			- Compiler state.
			  psIterator		- Iterator state.
			  psCurrentInst		- Instruction which has just been processed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uSpanStart, uSpanEnd;
	IREGLIVENESS_ITERATOR	sIRegIterator;
	
	PCODEBLOCK psCodeBlock;
	
	PINST psInst;
	PINST psPrevInst = IMG_NULL;
	PINST psLastDefForSpan = IMG_NULL;
	PINST psLastRefForSpan = IMG_NULL;
	PINST psLastDef = IMG_NULL;
	PINST psLastRef = IMG_NULL;
	PINST psLastDefLive = IMG_NULL;
	PINST psLastRefLive = IMG_NULL;
	
	uSpanStart	= psStartInst->uBlockIndex;
	uSpanEnd	= psEndInst->uBlockIndex;
	psCodeBlock	= psStartInst->psBlock;
	
	UseDefIterateIRegLiveness_Initialize(psState, psCodeBlock, &sIRegIterator);
	for (psInst = psCodeBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		
		IMG_UINT32 uIRegsLive = UseDefIterateIRegLiveness_Current(&sIRegIterator);
		
		if((uIRegsLive & (1U << uIReg)))
		{
			/*
				psLastDef, psLastRef are tracking defining and referencing instruction 
				for current instruction (psInst) respectively.
			*/
			if(psLastDef == IMG_NULL)
			{
				psLastDef = psPrevInst;
				if(psInst->uBlockIndex <= uSpanEnd)
				{
					psLastDefLive = psLastDef;
				}
			}
			psLastRef = psInst;
		}
		else
		{
			if(psLastDef)
			{
				psLastRef = psPrevInst;
				psLastRefLive = psLastRef;
			}
			psLastDef = IMG_NULL;
			psLastRef = IMG_NULL;
		}
		if(psInst->uBlockIndex >= uSpanStart)
		{
			if(psInst->uBlockIndex <= uSpanEnd && psLastDefForSpan == IMG_NULL)
			{
				psLastDefForSpan = psLastDef;
			}
			if(psLastDef == psLastDefLive)
			{
				psLastRefForSpan = psLastRef;
			}
		}
		
		UseDefIterateIRegLiveness_Next(psState, &sIRegIterator, psInst);
		psPrevInst = psInst;
	}
	
	/* Set results */
	*ppsLastDefForSpan = psLastDefForSpan;
	*ppsLastRefForSpan = psLastRefForSpan;
}

static
PUSEDEF UseDefGetLastDefine(PUSC_LIST_ENTRY psListEntry)
/*****************************************************************************
 FUNCTION	: UseDefGetLastDefine
    
 PURPOSE	: Returns the first define in a list of uses/defines.

 PARAMETERS	: psListEntry		- Point in the list to begin searching.

 RETURNS	: A pointer to the define information or NULL if there are no
			  previous defines.
*****************************************************************************/
{
	while (psListEntry != NULL)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUseDef->eType >= DEF_TYPE_FIRST && psUseDef->eType <= DEF_TYPE_LAST)
		{
			return psUseDef;
		}

		psListEntry = psListEntry->psPrev;
	}
	return NULL;
}

IMG_INTERNAL
PINST UseDefGetLastUse(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: UseDefGetLastUse
    
 PURPOSE	: Returns the last use in a list of uses/defines.

 PARAMETERS	: psState	- Compiler state.
			  uType		- Type of the register
			  uNumber	- Number of the register

 RETURNS	: The instruction that is the last one to use the given register.
*****************************************************************************/
{
	PUSC_LIST_ENTRY psListEntry;
	PUSEDEF_CHAIN	psUseDef = UseDefGet(psState, uType, uNumber);
	PINST			psLastUseInst = NULL;

	if (psUseDef == NULL)
	{
		return NULL;
	}

	for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUseDef;

		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (UseDefIsUse(psUseDef))
		{
			PINST psInst = UseDefGetInst(psUseDef);

			if (psLastUseInst == NULL || psInst->uBlockIndex > psLastUseInst->uBlockIndex)
			{
				psLastUseInst = psInst;
			}
		}
	}

	return psLastUseInst;
}

IMG_INTERNAL
IMG_VOID UseDefIterateIRegLiveness_InitializeAtPoint(PINTERMEDIATE_STATE	psState, 
													 PINST					psPoint, 
													 PIREGLIVENESS_ITERATOR	psIterator)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_InitializeAtPoint
    
 PURPOSE	: Initialize an iterator returning the set of internal registers live
			  before each instruction in a block.

 PARAMETERS	: psState		- Compiler state.
			  psPoint		- Start the iteration immediately before this instruction.
			  psIterator	- Iterator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIRegIdx;

	psIterator->uIRegLiveMask = 0;
	psIterator->uIRegC10Mask = 0;
	psIterator->uIRegSkipInvalidMask = 0;
	for (uIRegIdx = 0; uIRegIdx < psState->uGPISizeInScalarRegs; uIRegIdx++)
	{
		psIterator->apsUseDefEntries[uIRegIdx] = NULL;
	}

	if (psPoint == NULL)
	{
		return;
	}

	ASSERT(psPoint->psBlock != NULL);
	for (uIRegIdx = 0; uIRegIdx < psState->uGPISizeInScalarRegs; uIRegIdx++)
	{
		PUSEDEF_CHAIN	psIRegUseDefChain;
		PUSC_LIST_ENTRY	psListEntry;
		PINST			psUseInst;

		psIRegUseDefChain = psPoint->psBlock->apsIRegUseDef[uIRegIdx];

		/*
			Check for no uses/defines of the internal register in this block.
		*/
		if (psIRegUseDefChain == NULL)
		{
			continue;
		}

		/*
			Look for the first instruction which either uses or defines the internal register and is either the
			specified instruction or a later instruction.
		*/
		for (psListEntry = psIRegUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUseDef;

			psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

			psUseInst = UseDefGetInst(psUseDef);
			ASSERT(psUseInst != NULL);
			psUseInst = psUseInst->psGroupParent;

			ASSERT(psUseInst->psBlock == psPoint->psBlock);
			if (psUseInst->uBlockIndex >= psPoint->uBlockIndex)
			{
				break;
			}
		}

		/*
			Store the first reference to the internal register either at or after the specified instruction.
		*/
		psIterator->apsUseDefEntries[uIRegIdx] = psListEntry;

		/*
			If there is a use at or after the instruction we found then the corresponding define must be at an
			earlier instruction than the specified one so this internal register is live before the
			specified instruction.
		*/
		if (UseDefIsNextReferenceAsUse(psState, psListEntry))
		{
			PUSEDEF		psLastDef;

			/*
				Get the instruction which defines the internal register.
			*/
			psLastDef = UseDefGetLastDefine(psListEntry);

			if (psLastDef != NULL)
			{
				PINST		psLastDefInst;

				/*
					Flag the internal register as live.
				*/
				psIterator->uIRegLiveMask |= (1U << uIRegIdx);
				
				/*
					Record internal registers which were written with C10 format data.
				*/
				ASSERT(psLastDef->eType == DEF_TYPE_INST);
				psLastDefInst = psLastDef->u.psInst;
				ASSERT(psLastDef->uLocation < psLastDefInst->uDestCount);
				if (psLastDefInst->asDest[psLastDef->uLocation].eFmt == UF_REGFORMAT_C10)
				{
					psIterator->uIRegC10Mask |= (1U << uIRegIdx);
				}

				/*
					Record internal registers which were written by instruction with the SKIPINVALID
					flag set.
				*/
				if (GetBit(psLastDefInst->auFlag, INST_SKIPINV))
				{
					psIterator->uIRegSkipInvalidMask |= (1U << uIRegIdx);
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID UseDefIterateIRegLiveness_Initialize(PINTERMEDIATE_STATE		psState, 
											  PCODEBLOCK				psBlock, 
											  PIREGLIVENESS_ITERATOR	psIterator)
/*****************************************************************************
 FUNCTION	: UseDefIterateIRegLiveness_Initialize
    
 PURPOSE	: Initialize the iterator state to the first instruction in a block.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to iterate through.
			  psIterator		- Iterator state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UseDefIterateIRegLiveness_InitializeAtPoint(psState, psBlock->psBody, psIterator);
}

IMG_INTERNAL
IMG_UINT32 UseDefGetIRegsLiveBeforeInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: UseDefGetIRegsLiveBeforeInst
    
 PURPOSE	: Returns the mask of internal registers live immediate before an
			  instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.

 RETURNS	: The mask.
*****************************************************************************/
{
	IREGLIVENESS_ITERATOR	sIterator;

	UseDefIterateIRegLiveness_InitializeAtPoint(psState, psInst, &sIterator);
	return sIterator.uIRegLiveMask;
}

IMG_INTERNAL
IMG_VOID UseDefGetIRegStateBeforeInst(PINTERMEDIATE_STATE psState, PINST psInst, PIREG_STATE psIRegState)
/*****************************************************************************
 FUNCTION	: UseDefGetIRegsLiveBeforeInst
    
 PURPOSE	: Returns a information about the state of the internal registers
			  immediately before an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction.
			  psIRegState	- Returns information about the internal registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IREGLIVENESS_ITERATOR	sIterator;

	UseDefIterateIRegLiveness_InitializeAtPoint(psState, psInst, &sIterator);
	psIRegState->uLiveMask = sIterator.uIRegLiveMask;
	psIRegState->uC10Mask = sIterator.uIRegC10Mask;
	psIRegState->uSkipInvalidMask = sIterator.uIRegSkipInvalidMask;
}

IMG_INTERNAL
IMG_BOOL UseDefIsIRegLiveInInternal(PINTERMEDIATE_STATE psState, 
									IMG_UINT32			uIRegIdx, 
									PINST				psIntervalStart, 
									PINST				psIntervalEnd)
/*****************************************************************************
 FUNCTION	: UseDefIsIRegLiveInInternal
    
 PURPOSE	: Checks if a specified interval register is live between a
			  pair of instructions.

 PARAMETERS	: psState		- Compiler state.
			  uIRegIdx		- Internal register to check.
			  psIntervalStart
							- The interval starts at this instruction's defines.
			  psIntervalEnd	- The interval ends at this instruction's uses.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PCODEBLOCK		psBlock;
	PUSC_LIST_ENTRY	psListEntry;

	psBlock = psIntervalStart->psBlock;

	if (psIntervalEnd == NULL)
	{
		/* A register that is defined, but never used. Could be the dest of an efo. */
		/* Extra check: the register isn't already used as a dest. */
		IMG_UINT32 uDest;
		for (uDest = 0; uDest < psIntervalStart->uDestCount; ++uDest)
		{
			if (psIntervalStart->asDest[uDest].uType == USEASM_REGTYPE_FPINTERNAL &&
				psIntervalStart->asDest[uDest].uNumber == uIRegIdx)
			{
				return IMG_TRUE;
			}
		}

		psIntervalEnd = psIntervalStart;
	}

	ASSERT(psIntervalEnd->psBlock == psBlock);

	/*
		Check for no uses of the internal registers in the block.
	*/
	if (psBlock->apsIRegUseDef[uIRegIdx] == NULL)
	{
		return IMG_FALSE;
	}

	for (psListEntry = psBlock->apsIRegUseDef[uIRegIdx]->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psIRegUseDef;
		PINST	psUseInst;

		psIRegUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(psIRegUseDef->eType == DEF_TYPE_INST ||
			   psIRegUseDef->eType == USE_TYPE_SRC ||
			   psIRegUseDef->eType == USE_TYPE_OLDDEST);

		psUseInst = psIRegUseDef->u.psInst;

		/*
			Skip uses/defines which are before the interval.
		*/
		if (psUseInst->uBlockIndex < psIntervalStart->uBlockIndex)
		{
			continue;
		}

		if (psIRegUseDef->eType == DEF_TYPE_INST)
		{
			/*
				Check for a define of the internal register inside the interval. 
			*/
			if (psUseInst->uBlockIndex < psIntervalEnd->uBlockIndex)
			{
				return IMG_TRUE;
			}
			/*
				Otherwise the internal register isn't used/defined inside the interval and is
				completely defined after it so the interval isn't live inside the
				interval.
			*/
			return IMG_FALSE;
		}
		else
		{
			/*
				The interval starts at the first instruction's defines so skip uses at the
				first instruction.
			*/
			if (psUseInst->uBlockIndex == psIntervalStart->uBlockIndex)
			{
				continue;
			}
			/*
				The internal register is either used inside the interval or used after it with the
				corresponding definition before the interval so the internal register is live inside
				the interval.
			*/
			return IMG_TRUE;
		}
	}

	/*
		No uses/defines after the start of the interval and before the end of the block.
	*/
	return IMG_FALSE;
}

/* EOF */
