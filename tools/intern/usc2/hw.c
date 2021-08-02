/*************************************************************************
 * Name         : hw.c
 * Title        :  Constructs code to drive assembler (i.e. USEASM)
 * Created      : April 2005
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
 * $Log: hw.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "layout.h"
#include <limits.h>

#if defined (OUTPUT_USCHW)

/*****************************************************************************
 FUNCTION	: UseAssemblerError
    
 PURPOSE	: Called by the assembler to report a problem encoding an instruction.

 PARAMETERS	: pvContext		- Context pointer originally passed to Useasm
							  (actually our internal compilation state).
			  psInst		- The problematic instruction
			  pszFmt, ...	- Description of the problem encountered.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID UseAssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_CHAR *pszFmt, ...)
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;

	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(pszFmt);

	imgabort();
}

/****************************************************************************
 FUNCTION	: UseAssemblerGetLabelAddress
    
 PURPOSE	: Get the address for a label.

 PARAMETERS	: pvContext			- Context
			  dwLabel			- Id of the label
			  
 RETURNS	: The address.
*****************************************************************************/
static IMG_UINT32 IMG_CALLCONV UseAssemblerGetLabelAddress(IMG_PVOID	pvContext,
														   IMG_UINT32	uLabel)
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;
	return psState->puLabels[uLabel];
}

/*****************************************************************************
 FUNCTION	: UseAssemblerSetLabelAddress
    
 PURPOSE	: Set the address for a label.

 PARAMETERS	: pvContext			- Context.
			  dwLabel			- Id of the label
			  dwAddress			- The address to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID IMG_CALLCONV UseAssemblerSetLabelAddress(IMG_PVOID	pvContext,
														 IMG_UINT32	uLabel,
														 IMG_UINT32	uAddress)
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;
	psState->puLabels[uLabel] = uAddress;
}

/*****************************************************************************
 FUNCTION	: UseAssemblerGetLabelName
    
 PURPOSE	: Get the user-friendly name for a label.

 PARAMETERS	: pvContext			- Context.
			  dwLabel			- Id of the label
			  
 RETURNS	: Pointer to the name.
*****************************************************************************/
static IMG_PCHAR IMG_CALLCONV UseAssemblerGetLabelName(IMG_PVOID pvContext, 
													  IMG_UINT32 uLabel)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uLabel);
	return NULL;
}

/*********************************************************************************
 Function			: UseAssemblerRealloc

 Description		: Realloc function for USE by the assembler.
 
 Parameters			: pvContext		- Context
					  pvOldBuf		- Buffer to resize.
					  dwNewSize		- New size for the buffer.
					  uOldSize		- Old size of the buffer

 Globals Effected	: None

 Return				: New buffer.
*********************************************************************************/
static IMG_PVOID IMG_CALLCONV UseAssemblerRealloc(IMG_PVOID		pvContext, 
												  IMG_PVOID		pvOldBuf, 
												  IMG_UINT32	uNewSize, 
												  IMG_UINT32	uOldSize)
{
	IMG_PVOID pvNewBuf;
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;
	if (pvOldBuf == NULL)
	{
		return UscAlloc(psState, uNewSize);
	}
	if (uNewSize == 0)
	{
		if (pvOldBuf != NULL)
		{
			UscFree(psState, pvOldBuf);
		}
		return NULL;
	}
	pvNewBuf = UscAlloc(psState, uNewSize);
	if (pvNewBuf == NULL)
	{
		return NULL;
	}
	memcpy(pvNewBuf, pvOldBuf, min(uNewSize, uOldSize));
	UscFree(psState, pvOldBuf);
	return pvNewBuf;
}

/*****************************************************************************
 FUNCTION	: GetConstantTableEntrySize
    
 PURPOSE	: Get the size of an entry in a constant table.

 PARAMETERS	: uSrcIdx		- Constant index.
			  eSrcFormat	- Constant format.
			  
 RETURNS	: The size.
*****************************************************************************/
static IMG_UINT32 GetConstantTableEntrySize(PINTERMEDIATE_STATE		psState,
											IMG_UINT32				uSrcIdx,
											UNIFLEX_CONST_FORMAT	eSrcFormat)
{
	if (eSrcFormat == UNIFLEX_CONST_FORMAT_F32 || 
		eSrcFormat == UNIFLEX_CONST_FORMAT_STATIC ||
		eSrcFormat == UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE || 
		eSrcFormat == UNIFLEX_CONST_FORMAT_RTA_IDX ||
		(psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0)
	{
		return 1;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_F16)
	{
		return 2;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_U8)
	{
		return 4;
	}
	else
	{
		IMG_UINT32	uSrcChan = uSrcIdx & 3;

		if ((uSrcIdx & REMAPPED_CONSTANT_UNPACKEDC10) != 0)
		{
			ASSERT(uSrcChan == 0 || uSrcChan == 2);
			return 2;
		}
		else
		{
			ASSERT(eSrcFormat == UNIFLEX_CONST_FORMAT_C10);
			ASSERT(uSrcChan == 0 || uSrcChan == 3);

			if (uSrcChan == 3)
			{
				return 1;
			}
			else
			{
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
				{
					return 4;
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					return 3;
				}
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: GetConstantTableSize
    
 PURPOSE	: Get the size of a constant table.

 PARAMETERS	: psState			- Compiler state.
			  uConstCount		- Count of constants in the table.
			  peFormat			- Format of the constants in the table.
			  
 RETURNS	: The number of entries in the table.
*****************************************************************************/
static IMG_UINT32 GetConstantTableSize(PINTERMEDIATE_STATE		psState, 
									   IMG_UINT32				uConstCount,
									   USC_ARRAY				psMap[],
									   USC_ARRAY				psFormat[])
{
	IMG_UINT32 i, uOutIdx;

	uOutIdx = 0;
	for (i = 0; i < uConstCount; i++)
	{
		UNIFLEX_CONST_FORMAT	eSrcFormat = (UNIFLEX_CONST_FORMAT)(IMG_UINTPTR_T)ArrayGet(psState, psFormat, i);
		IMG_UINT32				uSrcIdx = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psMap, i);

		uOutIdx += GetConstantTableEntrySize(psState, uSrcIdx, eSrcFormat);
	}
	return uOutIdx;
}

static IMG_VOID AddC10Constant(PUNIFLEX_CONST_LOAD		psConstLoad,
							   IMG_UINT32				uSrcBuf,
							   IMG_UINT32				uSrcIdx,
							   IMG_UINT32				uSrcChan,
							   IMG_UINT32				uSrcShift,
							   IMG_UINT32				uConstDestIdx,
							   IMG_UINT32				uDestShift)
/*****************************************************************************
 FUNCTION	: AddC10Constant
    
 PURPOSE	: Add a C10 format entry to a constant table.

 PARAMETERS	: psConstLoadTable		- The table to add an entry to.
			  puOutIdx				- Points to the index of the next
									unused entry.
			  uSrcBuf				- Source buffer for the entry.
			  uSrcIdx				- Source constant number for the entry.
			  uSrcChan				- Source channel for the entry.
			  uSrcShift				- Source range of bits.
			  uConstDestIdx			- Destination memory offset/secondary
									attribute.
			  uDestShift			- Start of the range of bits in the
									destination memory offset/secondary
									attribute.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	psConstLoad->u.s.uConstsBuffNum	= uSrcBuf;
	psConstLoad->u.s.uSrcIdx		= (IMG_UINT16)(uSrcIdx + uSrcChan);
	psConstLoad->u.s.uSrcShift		= uSrcShift;
	psConstLoad->u.s.uDestShift		= (IMG_UINT16)uDestShift;
	psConstLoad->uDestIdx			= (IMG_UINT16)uConstDestIdx;
	psConstLoad->eFormat			= UNIFLEX_CONST_FORMAT_C10;
}

static PUNIFLEX_CONST_LOAD CopyC10UnpackedConstantTableEntry(PINTERMEDIATE_STATE	psState,
															 IMG_UINT32				uConstDestIdx,
															 IMG_UINT32				uSrcBuffer,
															 IMG_UINT32				uSrcIdx,
															 PUNIFLEX_CONST_LOAD	psConstLoad)
/*****************************************************************************
 FUNCTION	: CopyC10PackedConstantTableEntry
    
 PURPOSE	: Create the entries in the constant table in UNIFLEX_HW corresponding
			  to a single C10 input constant unpacked to allow indexing down
			  to subcomponents..

 PARAMETERS	: psState			- Compiler state.
			  uConstDestIdx		- Destination memory offset/secondary attribute
			  uSrcBuffer		- Constant buffer.
			  uSrcIdx			- Constant number.
			  psConstLoad		- Where to start creating entries.
			  
 RETURNS	: The next unused entry.
*****************************************************************************/
{
	IMG_UINT32	uSrcConst = uSrcIdx & ~(VECTOR_LENGTH - 1U);
	IMG_UINT32	uSrcChan = uSrcIdx & (VECTOR_LENGTH - 1U);
	IMG_UINT32	uChan;

	ASSERT(uSrcChan == 0 || uSrcChan == 2);

	/*
		DEST[0..9]		= CONST.RED		/ CONST.BLUE
		DEST[16..25]	= CONST.GREEN	/ CONST.ALPHA
	*/
	for (uChan = 0; uChan < 2; uChan++)
	{
		AddC10Constant(psConstLoad,
					   uSrcBuffer,
					   uSrcConst, 
					   uSrcChan + uChan, 
					   0 /* uSrcShift */,
					   uConstDestIdx, 
					   16 * uChan /* uDestShift */);
		psConstLoad++;
	}

	return psConstLoad;
}

static PUNIFLEX_CONST_LOAD CopyC10PackedConstantTableEntry(PINTERMEDIATE_STATE	psState,
														   IMG_UINT32			uConstDestIdx,
														   IMG_UINT32			uSrcBuffer,
														   IMG_UINT32			uSrcIdx,
														   PUNIFLEX_CONST_LOAD	psConstLoad)
/*****************************************************************************
 FUNCTION	: CopyC10PackedConstantTableEntry
    
 PURPOSE	: Create the entries in the constant table in UNIFLEX_HW corresponding
			  to a single C10 input constant.

 PARAMETERS	: uConstDestIdx		- Destination memory offset/secondary attribute
			  uSrcBuffer		- Constant buffer.
			  uSrcIdx			- Constant number.
			  psConstLoad		- Where to start creating entries.
			  
 RETURNS	: The next unused entry.
*****************************************************************************/
{
	IMG_UINT32	uSrcConst = uSrcIdx & ~3U;
	IMG_UINT32	uSrcChan = uSrcIdx & 3U;

	if (uSrcChan == 3)
	{
		IMG_UINT32	uSrcShift;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			uSrcShift = 2;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			uSrcShift = 0;
		}

		/*
			Load the alpha channel of the constant into the bottom bits of the dword.
		*/
		AddC10Constant(psConstLoad, 
					   uSrcBuffer, 
					   uSrcConst, 
					   3 /* uSrcChan */, 
					   uSrcShift,
					   uConstDestIdx, 
					   0 /* uDestShift */);
		psConstLoad++;
	}
	else
	{
		IMG_UINT32	uChan;
		IMG_BOOL	bC10RGBA;

		ASSERT(uSrcChan == 0);

		/*
			Which channel order does this constant use?
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA) != 0)
		{
			bC10RGBA = IMG_TRUE;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			bC10RGBA = IMG_FALSE;
		}

		/*
			Load the rgb channels of the constants.
		*/
		for (uChan = 0; uChan < 3; uChan++)
		{
			IMG_UINT32	uDestShift;

			if (bC10RGBA)
			{
				uDestShift = uChan * 10;
			}
			else
			{
				uDestShift = 20 - uChan * 10;
			}

			AddC10Constant(psConstLoad, 
						   uSrcBuffer, 
						   uSrcConst, 
						   uChan, 
						   0 /* uSrcShift */,
						   uConstDestIdx, 
						   uDestShift);
			psConstLoad++;
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			AddC10Constant(psConstLoad, 
						   uSrcBuffer, 
						   uSrcConst, 
						   3 /* uSrcChan */,
						   0 /* uSrcShift */,
						   uConstDestIdx, 
						   30 /* uDestShift */);
			psConstLoad++;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	return psConstLoad;
}

static PUNIFLEX_CONST_LOAD CopyConstantTableEntry(PINTERMEDIATE_STATE		psState,
												  IMG_UINT32				uConstDestIdx,
												  IMG_UINT32				uSrcBuffer,
												  IMG_UINT32				uSrcIdx,
												  UNIFLEX_CONST_FORMAT		eSrcFormat,
												  PUNIFLEX_CONST_LOAD		psConstLoad)
/*****************************************************************************
 FUNCTION	: CopyConstantTableEntry
    
 PURPOSE	: Create the entries in the constant table in UNIFLEX_HW corresponding
			  to a single input constant.

 PARAMETERS	: uConstDestIdx		- Destination memory offset/secondary attribute
			  uSrcBuffer		- Constant buffer.
			  uSrcIdx			- Constant number.
			  eSrcFormat		- Constant format.
			  psConstLoad		- Where to start creating entries.
			  
 RETURNS	: The next unused entry.
*****************************************************************************/
{
	if (eSrcFormat == UNIFLEX_CONST_FORMAT_STATIC)
	{
		psConstLoad->u.uValue			= uSrcIdx;
		psConstLoad->uDestIdx			= (IMG_UINT16)uConstDestIdx;
		psConstLoad->eFormat			= eSrcFormat;	
		psConstLoad++;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_CONSTS_BUFFER_BASE)
	{
		psConstLoad->u.s.uConstsBuffNum		= uSrcBuffer;
		psConstLoad->uDestIdx				= (IMG_UINT16)uConstDestIdx;
		psConstLoad->eFormat				= eSrcFormat;
		psConstLoad++;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_RTA_IDX)
	{
		psConstLoad->u.s.uConstsBuffNum		= UINT_MAX;
		psConstLoad->uDestIdx				= (IMG_UINT16)uConstDestIdx;
		psConstLoad->eFormat				= eSrcFormat;
		psConstLoad++;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_F32 || (psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0)
	{
		psConstLoad->u.s.uSrcIdx			= (IMG_UINT16)uSrcIdx;
		psConstLoad->uDestIdx				= (IMG_UINT16)uConstDestIdx;
		psConstLoad->u.s.uDestShift			= 0;
		psConstLoad->u.s.uSrcShift			= 0;
		psConstLoad->eFormat				= eSrcFormat;
		psConstLoad->u.s.uConstsBuffNum		= uSrcBuffer;
		psConstLoad++;
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_F16)
	{
		IMG_UINT32	uHalf;

		for (uHalf = 0; uHalf < 2; uHalf++)
		{	
			psConstLoad->u.s.uSrcIdx			= (IMG_UINT16)(uSrcIdx + uHalf);
			psConstLoad->uDestIdx				= (IMG_UINT16)uConstDestIdx;
			psConstLoad->u.s.uDestShift			= (IMG_UINT16)(uHalf * 16);
			psConstLoad->u.s.uSrcShift			= 0;
			psConstLoad->eFormat				= UNIFLEX_CONST_FORMAT_F16;
			psConstLoad->u.s.uConstsBuffNum		= uSrcBuffer;
			psConstLoad++;
		}
	}
	else if (eSrcFormat == UNIFLEX_CONST_FORMAT_U8)
	{
		IMG_UINT32	uQuarter;

		for (uQuarter = 0; uQuarter < 4; uQuarter++)
		{	
			psConstLoad->u.s.uSrcIdx			= (IMG_UINT16)(uSrcIdx + uQuarter);
			psConstLoad->u.s.uSrcShift			= 0;
			psConstLoad->uDestIdx				= (IMG_UINT16)uConstDestIdx;
			if (uQuarter<3)
			{
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA)
				{
					psConstLoad->u.s.uDestShift			= (IMG_UINT16)(uQuarter * 8);
				}
				else
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				{
					psConstLoad->u.s.uDestShift			= (IMG_UINT16)(16 - uQuarter * 8);
				}
			}
			else
			{
				psConstLoad->u.s.uDestShift			= (IMG_UINT16)(uQuarter * 8);
			}
			psConstLoad->eFormat				= UNIFLEX_CONST_FORMAT_U8;
			psConstLoad->u.s.uConstsBuffNum		= uSrcBuffer;
			psConstLoad++;
		}
	}
	else
	{
		ASSERT(eSrcFormat == UNIFLEX_CONST_FORMAT_C10);

		if ((uSrcIdx & REMAPPED_CONSTANT_UNPACKEDC10) != 0)
		{
			psConstLoad = 
				CopyC10UnpackedConstantTableEntry(psState, 
												  uConstDestIdx, 
												  uSrcBuffer, 
												  uSrcIdx & ~REMAPPED_CONSTANT_UNPACKEDC10, 
												  psConstLoad);
		}
		else
		{
			psConstLoad = CopyC10PackedConstantTableEntry(psState, uConstDestIdx, uSrcBuffer, uSrcIdx, psConstLoad);
		}
	}

	return psConstLoad;
}

static IMG_UINT32 CopyConstantTable(PINTERMEDIATE_STATE		psState, 
								    PCONSTANT_BUFFER		psConstBuf,
									IMG_UINT32				uConstBufIdx,
									PUNIFLEX_CONST_LOAD*	ppsConstLoadTable,
									IMG_PUINT32				puConstCount,
									IMG_PUINT32				puMaximumConstCount)
/*****************************************************************************
 FUNCTION	: CopyConstantTable
    
 PURPOSE	: Copy the memory constant table for a constant buffer.

 PARAMETERS	: psState			- Compiler state.
			  psConstBuf		- Description describing the constant buffer.
			  uConstBufIdx		- Index of the constant buffer.
			  ppsConstLoadTable - Memory constant table to copy to.
			  puConstCount
			  puMaximumConstCount
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uConstDestIdx, i;
	PUNIFLEX_CONST_LOAD psConstLoadTable;
	PUNIFLEX_CONST_LOAD psConstLoadEntry;
	IMG_UINT32			uConstCount = psConstBuf->uRemappedCount;
	USC_PARRAY			psMap = psConstBuf->psRemappedMap;
	USC_PARRAY			psFormat = psConstBuf->psRemappedFormat;
	
	/*
		If required allocate more space for the constant table.
	*/
	*puConstCount = GetConstantTableSize(psState, uConstCount, psMap, psFormat);
	if ((*puConstCount) > (*puMaximumConstCount))
	{
		if ((*ppsConstLoadTable) != NULL)
		{
			psState->pfnFree(*ppsConstLoadTable);
		}
		*ppsConstLoadTable = psState->pfnAlloc((*puConstCount) * sizeof(UNIFLEX_CONST_LOAD));
		if ((*ppsConstLoadTable) == NULL)
		{
			return UF_ERR_NO_MEMORY;
		}
		*puMaximumConstCount = *puConstCount;
	}
	psConstLoadTable = *ppsConstLoadTable;

	/*
		Convert the constant table into the output format.
	*/
	uConstDestIdx = 0;
	psConstLoadEntry = psConstLoadTable;
	for (i = 0; i < uConstCount; i++)
	{
		UNIFLEX_CONST_FORMAT	eSrcFormat = (UNIFLEX_CONST_FORMAT)(IMG_UINTPTR_T)ArrayGet(psState, psFormat, i);	
		IMG_UINT32				uSrcIdx = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psMap, i);

		psConstLoadEntry = CopyConstantTableEntry(psState,
												  uConstDestIdx,
												  uConstBufIdx,
												  uSrcIdx,
												  eSrcFormat,
												  psConstLoadEntry);
		uConstDestIdx++;
	}
	ASSERT((IMG_UINT32)(psConstLoadEntry - psConstLoadTable) == (*puConstCount));
	return UF_OK;
}

/*****************************************************************************
 FUNCTION	: GetConstantTableSizeForSAMappedConsts
    
 PURPOSE	: Give the size of a constants table for SA mapped constants.

 PARAMETERS	: psState	- Compiler state.
			  uConstCount	- Count of constants in the table.
			  peFormat	- Format of the constants in the table.
			  puConstantsBuffer	- Contains information that mapped constant belongs to which constants buffer.			  
			  
 RETURNS	: The number of entries in the table.
*****************************************************************************/
static IMG_UINT32 GetConstantTableSizeForSAMappedConsts(PINTERMEDIATE_STATE	psState)
{
	IMG_UINT32		uOutIdx;
	PUSC_LIST_ENTRY	psListEntry;

	uOutIdx = 0;
	for (psListEntry = psState->sSAProg.sInRegisterConstantList.psHead;
		 psListEntry != NULL;
		 psListEntry = psListEntry->psNext)
	{
		PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

		uOutIdx += GetConstantTableEntrySize(psState, psConst->uNum, psConst->eFormat);
	}

	return uOutIdx;
}

static IMG_UINT32 CopySAMappedConstantsTable(PINTERMEDIATE_STATE psState,  
											 PUNIFLEX_CONST_LOAD* ppsConstLoadTable, 
											 IMG_PUINT32 puConstCount, 
											 IMG_PUINT32 puMaximumConstCount)
/*****************************************************************************
 FUNCTION	: CopySAMappedConstantsTable
    
 PURPOSE	: Copy the constant table of constants mapped to SA registers.

 PARAMETERS	: psState	- Compiler state.
			  uConstCount	- Count of constants that are mappped to SA registers.
			  puMap	- Contains information that which constants are mapped to what SAs.
			  peFormat	- Format of each mapped constant.
			  puConstantsBuffer	- Contains information that mapped constant belongs to which constants buffer.
			  puSaForBaseOfConstsBuff	- SAs mapped to base addresses of constants buffers.
			  ppsConstLoadTable	- Pointer to constants table to which the information will be copied.
			  puConstCount	- To return the count of constants copied.
			  puConstCount	- To return the maximum constant used.
			  psHw	- Output for the compiled program.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_CONST_LOAD psConstLoadTable;
	PUNIFLEX_CONST_LOAD	psConstLoadEntry;
	PUSC_LIST_ENTRY psListEntry;
	
	/*
		If required allocate more space for the constant table.
	*/	
	*puConstCount = GetConstantTableSizeForSAMappedConsts(psState);
	if ((*puConstCount) > (*puMaximumConstCount))
	{
		if ((*ppsConstLoadTable) != NULL)
		{
			psState->pfnFree(*ppsConstLoadTable);
		}
		*ppsConstLoadTable = psState->pfnAlloc((*puConstCount) * sizeof(UNIFLEX_CONST_LOAD));
		if ((*ppsConstLoadTable) == NULL)
		{
			return UF_ERR_NO_MEMORY;
		}
		*puMaximumConstCount = *puConstCount;
	}
	psConstLoadTable = *ppsConstLoadTable;

	/*
		Convert the constant table into the output format.
	*/
	psConstLoadEntry = psConstLoadTable;
	for (psListEntry = psState->sSAProg.sInRegisterConstantList.psHead;
		 psListEntry != NULL;
		 psListEntry = psListEntry->psNext)
	{
		PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);
		PFIXED_REG_DATA		psDestAttr;
		IMG_UINT32			uDestAttr;
		IMG_UINT32			uConstDestIdx;
		
		psDestAttr = psConst->psResult->psFixedReg;
		uDestAttr = psDestAttr->sPReg.uNumber;

		ASSERT(uDestAttr >= psState->psSAOffsets->uInRegisterConstantOffset);
		uConstDestIdx = uDestAttr - psState->psSAOffsets->uInRegisterConstantOffset;

		psConstLoadEntry = CopyConstantTableEntry(psState,
												  uConstDestIdx,
												  psConst->uBuffer,
												  psConst->uNum,
												  psConst->eFormat,
												  psConstLoadEntry);
	}	
	ASSERT((IMG_UINT32)(psConstLoadEntry - psConstLoadTable) == (*puConstCount));
	return UF_OK;
}

/*****************************************************************************
 FUNCTION	: PVRCleanupUniflexHw
    
 PURPOSE	: Cleanup and reset a UNIFLEX_HW structure, freeing any associated
			  dynamically allocated memory.

 PARAMETERS	: pvContext	- Compiler context.
			  psHw		- The structure to cleanup/reset
			  
 RETURNS	: none
*****************************************************************************/
USC_EXPORT
IMG_VOID IMG_CALLCONV PVRCleanupUniflexHw(IMG_PVOID pvContext, PUNIFLEX_HW psHw)
{
	CleanupUniflexHw((PINTERMEDIATE_STATE)pvContext, psHw);
}

/*****************************************************************************
 FUNCTION	: CleanupUniflexHw
    
 PURPOSE	: Cleanup and reset a UNIFLEX_HW structure, freeing any associated
			  dynamically allocated memory.

 PARAMETERS	: psState	- Compiler state.
			  psHw		- The structure to cleanup/reset
			  
 RETURNS	: none
*****************************************************************************/
IMG_INTERNAL
IMG_VOID CleanupUniflexHw(PINTERMEDIATE_STATE psState, PUNIFLEX_HW psHw)
{
	IMG_UINT32 uConstsBuffNum;

	/*
		Clean up information about which samplers should have anisotropic filtering
		disabled.
	*/
	if (psHw->auNonAnisoTextureStages != NULL)
	{
		psState->pfnFree(psHw->auNonAnisoTextureStages);
	}
	psHw->auNonAnisoTextureStages = NULL;

	/*
		Free texture unpacking information.
	*/
	if (psHw->asTextureUnpackFormat != NULL)
	{
		psState->pfnFree(psHw->asTextureUnpackFormat);
	}
	psHw->asTextureUnpackFormat = NULL;

	/*
		Free memory for non-dependent texture samples and iterations.
	*/
	if (psHw->psNonDependentTextureLoads != NULL)
	{
		psState->pfnFree(psHw->psNonDependentTextureLoads);
	}
	psHw->psNonDependentTextureLoads = NULL;

	/*
		Cleanup the in-register constant mappings if used
	*/
	if	(psHw->psInRegisterConstMap != NULL)
	{
		psState->pfnFree(psHw->psInRegisterConstMap);
	}
	psHw->psInRegisterConstMap = NULL;
	psHw->uInRegisterConstCount	= 0;
	psHw->uMaximumInRegisterConstCount = 0;

	/*
		Cleanup the in-memory constant mappings if used
	*/
	for(uConstsBuffNum = 0; uConstsBuffNum<(psState->uNumOfConstsBuffsAvailable); uConstsBuffNum++)
	{	
		if	(((psHw->psMemRemappingForConstsBuff[uConstsBuffNum]).psConstMap) != NULL)
		{
			psState->pfnFree((psHw->psMemRemappingForConstsBuff[uConstsBuffNum]).psConstMap);
		}
		(psHw->psMemRemappingForConstsBuff[uConstsBuffNum]).psConstMap = NULL;
		(psHw->psMemRemappingForConstsBuff[uConstsBuffNum]).uConstCount = 0;
		(psHw->psMemRemappingForConstsBuff[uConstsBuffNum]).uMaximumConstCount = 0;		
	}

	/*
		Cleanup the SA-update program instructions if used
	*/
	if	(psHw->puSAProgInstructions != NULL)
	{
		psState->pfnFree(psHw->puSAProgInstructions);
	}
	psHw->puSAProgInstructions = NULL;
	psHw->uSAProgInstructionCount = 0;
	psHw->uSAProgMaximumInstructionCount = 0;

	/*
		Cleanup the main program instructions if used
	*/
	if	(psHw->puInstructions != NULL)
	{
		psState->pfnFree(psHw->puInstructions);
	}
	psHw->puInstructions = NULL;
	psHw->uInstructionCount = 0;
	psHw->uMaximumInstructionCount = 0;

	#ifdef SRC_DEBUG
	/* 
		Free memory allocated for source line cycle cost table. 
	*/
	if (psHw->asTableSourceCycle.psSourceLineCost != IMG_NULL)
	{
		psState->pfnFree(psHw->asTableSourceCycle.psSourceLineCost);
		psHw->asTableSourceCycle.psSourceLineCost = IMG_NULL;
	}
	psHw->asTableSourceCycle.uTableCount = 0;
	psHw->asTableSourceCycle.uTotalCycleCount = 0;
	#endif /* SRC_DEBUG */
}

static IMG_PUINT32 CompileInstrsCB(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   IMG_PUINT32			puOffset,
								   PLAYOUT_INFO			psLayout)
/******************************************************************************
 FUNCTION		: CompileInstrsCB

 DESCRIPTION	: LAY_INSTRS callback to actually generate H/W instructions in
				  the prescribed layout.
 
 PARAMETERS		: psState		- Current compilation context
				  psBlock		- Block whose body should be output

 OUTPUT			: puOffset		- instructions are written starting here
 INPUT/OUTPUT	: psLayout		- Updated with information about the opcode of the
							   instruction most recently output (includes NOPs and branches)

 RETURNS		: Location to continue writing instructions (for next block,
				  etc.) - i.e., new value of puOffset after this block's body.
******************************************************************************/
{
	PINST psInst;

	//2. body (copied from hw.c::CompileBlock for CBTYPE_BASIC !)
	for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
	{
		USE_INST asOut[2];

		ASSERT (psInst->eOpcode != ICALL);
		
		asOut[0].psNext = &asOut[1];

		/*
			Flag when dependent reads are present in the program.
		*/
		if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_DEPENDENTTEXTURESAMPLE)
		{
			psState->uFlags |= USC_FLAGS_DEPENDENTREADS;
		}

		/*
			Flag when constant loads from memory are present.
		*/
		if (
				#if defined(SUPPORT_SGX545)
				psInst->eOpcode == IELDD ||
				psInst->eOpcode == IELDQ ||
				#endif /* defined(SUPPORT_SGX545) */
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				psInst->eOpcode == ILDAQ ||
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				psInst->eOpcode == ILDAD || 
				psInst->eOpcode == ILDAW || 
				psInst->eOpcode == ILDAB
			)
		{
			psState->uFlags |= USC_FLAGS_PRIMARYCONSTLOADS;
		}

		/*
			Generate HW instructions for this intermediate one
		*/
		puOffset += EncodeInst(psState,
							   psInst,
							   puOffset,
							   psState->puInstructions);

		AppendInstruction(psLayout, psInst->eOpcode);
	}
	return puOffset;
}

static IMG_VOID CompileLabelCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout,
					 IMG_UINT32 uDestLabel, IMG_BOOL bSyncEndDest)
/******************************************************************************
 FUNCTION		: CompileLabelCB

 DESCRIPTION	: LAY_LABEL callback to output a label when laying out HW code

PARAMETERS		: psState		- Current compilation context
				  psLayout		- stores/models instructions being generated
							      (inc. other callbacks - like an OOP object)
				  uDestLabel	- number of label to make, i.e. for jumps to it
				  bSyncEndDest	- whether any jumps to the label use sync-end.

 RETURNS		: Nothing
******************************************************************************/
{
	IMG_UINT32 uOutSize;
	/*
		NOSCHED models any padding, etc., required; by reusing NoSchedLabelCB,
		we ensure we output exactly the same padding as it expected.
		(NOPs will actually be output, if necessary, by psLayout->pfnNop,
		 which we expect to point to EncodeNop for HW output)
	*/
	NoSchedLabelCB(psState, psLayout, uDestLabel, bSyncEndDest);
	uOutSize = EncodeLabel(psState->psUseasmContext,
										  psState->psTargetDesc,
										  uDestLabel,
										  psLayout->puInst,
										  psState->puInstructions);
	if (uOutSize == UINT_MAX)
	{
		//error in assembler! flag it...
		psLayout->puInst = NULL;
	}
	else
	{
		ASSERT (uOutSize == 0);
	}
}

static IMG_VOID CompileBranchCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout,
					  IOPCODE eOpcode, IMG_PUINT32 puDestLabel,
					  IMG_UINT32 uPredSrc, IMG_BOOL bPredNegate,
					  IMG_BOOL bSyncEnd, 
					  EXECPRED_BRANCHTYPE eExecPredBranchType)
/******************************************************************************
 FUNCTION		: CompileBranchCB

 DESCRIPTION	: LAY_BRANCH callback to generate a branch instruction into the
				  final HW code.

 PARAMETERS		: psState		- Current compilation context
				  psLayout		- stores/models instructions being generated
							      (inc. other callbacks - like an OOP object)
				  eOpcode		- opcode of branch instruction (IBR or ICALL)
				  puDestLabel	- pointer to number of label to which to jump
				  uPredSrc		- register upon which to predicate branch
				  bPredNegate	- whether to negate sense of previous
				  bSyncEnd		- whether to jump with the sync_end flag set
				  eExecPredBranchType
								- Execution Predicate Branch Type.

 RETURNS		: Nothing
******************************************************************************/
{
	IMG_UINT32 uDestLabel;
	
	/*
		Main "complexity" is in handling BRN22136/31062, whereby predicated
		branches do not work with sync_end (except after another branch with
		the same predicate, as this "forces" the predicate to be ready).
		Defer to NoSchedBranchCB to handle this, to ensure we handle it in
		exactly the same way as modelled by the NOSCHED phase: if special
		treatment is required to handle the BRN, NoSchedBranchCB will make
		extra/recursive calls to psLayout->pfnBranch, which will come back
		here (so any extra branches are actually output).
	*/
	CommonBranchCB(psState, psLayout, eOpcode, puDestLabel, uPredSrc, bPredNegate, bSyncEnd);
	
	ASSERT(puDestLabel ? IMG_TRUE : (eOpcode == ILAPC));
	uDestLabel = (puDestLabel) ? *puDestLabel : UINT_MAX;
	
	psLayout->puInst +=
		EncodeJump(psState, psState->psUseasmContext,
				   eOpcode, uDestLabel, uPredSrc,
				   bPredNegate,
				   psLayout->puInst,
				   psState->puInstructions,
				   /* UF_ATLEASTONELOOP now handled in verifier only */
				   IMG_FALSE,
				   bSyncEnd,
				   IMG_FALSE, /* bSyncIfNotTaken */
				   eExecPredBranchType);
}

#ifdef UF_TESTBENCH
static IMG_VOID CountLoopsBP(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psBlock,
							 IMG_PVOID				pvCount)
/******************************************************************************
  FUNCTION		: CountLoopsBP

  DESCRIPTION	: BLOCK_PROC to count the number of loops (strictly, the number
				  of distinct loop headers - i.e. blocks which dominate one or
				  more of their predecessors).
				  
 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block to check whether is a loop header.

 INPUT/OUTPUT	: pvCount	- IMG_PUINT32 incremented if the block is a header.

 RETURNS		: Nothing
******************************************************************************/
{
	IMG_UINT32 uPred;
	for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
	{
		if (Dominates(psState, psBlock, psBlock->asPreds[uPred].psDest))
		{
			(*((IMG_PUINT32)pvCount))++;
			return;
		}
	}
}
#endif /* UF_TESTBENCH */

static IMG_VOID AlignToEvenHwCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout)
{
	if ((((psLayout->puInst) - psState->puInstructions) % 4) == 2)
	{
		AppendInstruction(psLayout, INOP);
		EncodeNop(psLayout->puInst);		
		(psLayout->puInst) += 2;
	}
}

static
IMG_VOID SetHwShdrPhasesInfo(	PUNIFLEX_HW	psHw, 
								IMG_UINT32	uMainProgStart, 
								IMG_UINT32	uMainProgFeedbackPhase1Start, 
								IMG_UINT32	uMainProgSplitPhase1Start, 
								IMG_UINT32	uTemporaryRegisterCount, 
								IMG_UINT32	uTemporaryRegisterCountPostSplit, 
								IMG_UINT32	uFlags,
								IMG_UINT32	uFlags2)
/*****************************************************************************
 FUNCTION	: SetHwShdrPhasesInfo
    
 PURPOSE	: Sets output shader phases information in UNIFLEX hardware 
			  structure.

 PARAMETERS	: psState	- Compiler state.
			  psHw		- Output for the compiled program.
			  
 RETURNS	: Error code.
*****************************************************************************/
{
	/*
		Record the instruction offset to the entrypoint for the program
	*/
	psHw->asPhaseInfo[0].uPhaseStart = uMainProgStart;
	psHw->asPhaseInfo[0].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_NONE;
	psHw->asPhaseInfo[0].ePhaseRate = UNIFLEX_PHASE_RATE_PIXEL;
	/*
		Copy the number of temporary registers used.
	*/
	psHw->asPhaseInfo[0].uTemporaryRegisterCount = uTemporaryRegisterCount;
	psHw->uPhaseCount = 1;

	/*
		Set offsets of the beginning of the following phases.
	*/
	if ((uFlags & USC_FLAGS_SPLITFEEDBACKCALC) || (uFlags2 & USC_FLAGS2_SPLITCALC))
	{
		if ((uFlags & USC_FLAGS_SPLITFEEDBACKCALC) && (!(uFlags2 & USC_FLAGS2_SPLITCALC)))
		{
			if (uMainProgFeedbackPhase1Start != psHw->asPhaseInfo[0].uPhaseStart)
			{
				psHw->asPhaseInfo[1].uPhaseStart = uMainProgFeedbackPhase1Start;
				psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
				psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_PIXEL;
				psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCount;
				psHw->uPhaseCount = 2;
			}
			else
			{	
				psHw->asPhaseInfo[0].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
			}
		}
		else if((uFlags2 & USC_FLAGS2_SPLITCALC) && (!(uFlags & USC_FLAGS_SPLITFEEDBACKCALC)))
		{
			if(uMainProgSplitPhase1Start != psHw->asPhaseInfo[0].uPhaseStart)
			{
				psHw->asPhaseInfo[1].uPhaseStart = uMainProgSplitPhase1Start;
				psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_NONE;
				psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
				psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
				psHw->uPhaseCount = 2;
			}
			else
			{
				psHw->asPhaseInfo[0].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
				psHw->asPhaseInfo[0].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
			}
		}
		else
		{
			if(uMainProgFeedbackPhase1Start < uMainProgSplitPhase1Start)
			{
				if(uMainProgFeedbackPhase1Start != psHw->asPhaseInfo[0].uPhaseStart)
				{
					psHw->asPhaseInfo[1].uPhaseStart = uMainProgFeedbackPhase1Start;
					psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_PIXEL;
					psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCount;
					psHw->asPhaseInfo[2].uPhaseStart = uMainProgSplitPhase1Start;
					psHw->asPhaseInfo[2].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_NONE;
					psHw->asPhaseInfo[2].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[2].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 3;
				}
				else
				{	
					psHw->asPhaseInfo[0].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[1].uPhaseStart = uMainProgSplitPhase1Start;
					psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_NONE;
					psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 2;
				}
			}
			else if (uMainProgFeedbackPhase1Start > uMainProgSplitPhase1Start)
			{
				if (uMainProgSplitPhase1Start != psHw->asPhaseInfo[0].uPhaseStart)
				{
					psHw->asPhaseInfo[1].uPhaseStart = uMainProgSplitPhase1Start;
					psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_NONE;
					psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->asPhaseInfo[2].uPhaseStart = uMainProgFeedbackPhase1Start;
					psHw->asPhaseInfo[2].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[2].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[2].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 3;
				}
				else
				{	
					psHw->asPhaseInfo[0].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[1].uPhaseStart = uMainProgFeedbackPhase1Start;
					psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 2;
				}
			}
			else
			{
				if(uMainProgSplitPhase1Start != psHw->asPhaseInfo[0].uPhaseStart)
				{
					psHw->asPhaseInfo[1].uPhaseStart = uMainProgSplitPhase1Start;
					psHw->asPhaseInfo[1].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[1].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[1].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 2;
				}
				else
				{	
					psHw->asPhaseInfo[0].ePhaseExeCnd = UNIFLEX_PHASE_EXECND_FEEDBACK;
					psHw->asPhaseInfo[0].ePhaseRate = UNIFLEX_PHASE_RATE_SAMPLE;
					psHw->asPhaseInfo[0].uTemporaryRegisterCount = uTemporaryRegisterCountPostSplit;
					psHw->uPhaseCount = 1;
				}
			}
		}
	}
	return;
}

IMG_INTERNAL
IMG_UINT32 CompileToHw(PINTERMEDIATE_STATE psState, PUNIFLEX_HW psHw)
/*****************************************************************************
 FUNCTION	: CompileToHw
    
 PURPOSE	: Generate HW-compatible binary output from the intermediate code

 PARAMETERS	: psState	- Compiler state.
			  psHw		- Output for the compiled program.
			  
 RETURNS	: Error code.
*****************************************************************************/
{
	IMG_UINT32		i;	
	IMG_UINT32		uErrCode;
	USEASM_CONTEXT	sUseasmContext;
	IMG_UINT32		uConstsBuffNum;

	/*
		Useasm context not setup yet
	*/
	psState->puLabels			= IMG_NULL;
	psState->psUseasmContext	= IMG_NULL;

	/*
		Check for too many instructions.
	*/
	if (psState->uMainProgInstCount >= (IMG_UINT32)
			 (1 << (psState->psTargetFeatures->ui32NumProgramCounterBits - 1)))
	{
		USC_ERROR(UF_ERR_TOO_MANY_INSTS, "Too many instructions in the main program");
	}
	if (psState->uSAProgInstCount >= (IMG_UINT32)
			 (1 << (psState->psTargetFeatures->ui32NumProgramCounterBits - 1)))
	{
		USC_ERROR(UF_ERR_TOO_MANY_INSTS, "Too many instructions in the secondary update program");
	}

	/*
		Record instruction offsets where the caller should insert alpha-test
		code
	*/
	psHw->uFeedbackPhase0End	= psState->uMainProgFeedbackPhase0End;
	SetHwShdrPhasesInfo(
		psHw, 
		psState->uMainProgStart, 
		psState->uMainProgFeedbackPhase1Start, 
		psState->uMainProgSplitPhase1Start, 
		psState->uTemporaryRegisterCount, 
		psState->uTemporaryRegisterCountPostSplit, 
		psState->uFlags, 
		psState->uFlags2);

	/*
		Allocate space for the encoded instructions.
	*/
	if	(psState->uMainProgInstCount > psHw->uMaximumInstructionCount)
	{
		IMG_PUINT32	puHWInsts;

		puHWInsts = psHw->puInstructions;
		if	(puHWInsts != NULL)
		{
			psState->pfnFree(puHWInsts);

			psHw->puInstructions = NULL;
			psHw->uMaximumInstructionCount = 0;
		}

		puHWInsts = (IMG_PUINT32)psState->pfnAlloc(psState->uMainProgInstCount * 
												   EURASIA_USE_INSTRUCTION_SIZE);
		if (puHWInsts == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Failed to alloc space for USSE code");
		}

		psHw->puInstructions = puHWInsts;
		psHw->uMaximumInstructionCount = psState->uMainProgInstCount;
	}
	psState->puInstructions = psHw->puInstructions;
	psHw->uInstructionCount = psState->uMainProgInstCount;

	/*
		Allocate space for the encoded secondary load program.
	*/
	if	(psState->uSAProgInstCount > psHw->uSAProgMaximumInstructionCount)
	{
		IMG_PUINT32	puHWInsts;

		puHWInsts = psHw->puSAProgInstructions;
		if (puHWInsts != NULL)
		{
			psState->pfnFree(puHWInsts);

			psHw->puSAProgInstructions = NULL;
			psHw->uSAProgMaximumInstructionCount = 0;
		}

		puHWInsts = (IMG_PUINT32)psState->pfnAlloc(psState->uSAProgInstCount * 
												   EURASIA_USE_INSTRUCTION_SIZE);
		if (puHWInsts == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Failed to alloc space for SA-update USSE code");
		}

		psHw->puSAProgInstructions = puHWInsts;
		psHw->uSAProgMaximumInstructionCount = psState->uSAProgInstCount;
	}
	psState->puSAInstructions = psHw->puSAProgInstructions;
	psHw->uSAProgInstructionCount = psState->uSAProgInstCount;

	/*
		Allocate space to hold label addresses.
	*/
	if	(psState->uMainProgLabelCount)
	{
		psState->puLabels = 
			(IMG_PUINT32)UscAlloc(psState, psState->uMainProgLabelCount * sizeof(IMG_UINT32));
		for (i = 0; i < psState->uMainProgLabelCount; i++)
		{
			psState->puLabels[i] = UINT_MAX;
		}
	}

	/*
		Set up the context for the assembler.
	*/
	psState->psUseasmContext = &sUseasmContext;
	psState->psUseasmContext->pvContext = (IMG_PVOID)psState;
	psState->psUseasmContext->pfnRealloc = UseAssemblerRealloc;
	psState->psUseasmContext->pfnGetLabelAddress = UseAssemblerGetLabelAddress;
	psState->psUseasmContext->pfnSetLabelAddress = UseAssemblerSetLabelAddress;
	psState->psUseasmContext->pfnGetLabelName = UseAssemblerGetLabelName;
	psState->psUseasmContext->pvLabelState = IMG_NULL;
	psState->psUseasmContext->pfnAssemblerError = UseAssemblerError;

	/*
		TODO.
		UF_TESTBENCH builds require dominance info (below, for verifier).
		However, performing MergeBB's here *only*for*testbench*builds* would
		make testbench and non-testbench builds diverge significantly, which
		seems highly dangerous (testbench builds are the only ones whose output
		we verify!). Thus, although it shouldn't be needed for non-UF_TESTBENCH
		builds, we will call MergeBBs here in both builds for safety.

		When we update the verifier such that we don't need to pass in the loop
		header info, we can take this out :-).
	*/
	MergeAllBasicBlocks(psState);

#ifdef UF_TESTBENCH
	{
		IMG_UINT32 uNumLoopHeaders = 0;
		
		DoOnAllBasicBlocks(psState, ANY_ORDER, CountLoopsBP,
						   IMG_TRUE/*calls fine*/, &uNumLoopHeaders);
		psState->puVerifLoopHdrs = psState->pfnAlloc(
										 uNumLoopHeaders * sizeof(IMG_UINT32));
		psState->uNumVerifLoopHdrs = 0; //used to iterate filling through array
	}
#endif

	/*
		Encode the hardware instructions.
	*/
	LayoutProgram(psState, CompileInstrsCB, CompileBranchCB,
				  CompileLabelCB, PointActionsHwCB, AlignToEvenHwCB, LAYOUT_WHOLE_PROGRAM);
	
	/*
		Free temporary data.
	*/
	UscFree(psState, psState->puLabels);
	psState->puLabels = NULL;

	/*
		Copy the list of non-dependent texture loads.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL && psState->sShader.psPS->uNrPixelShaderInputs > 0)
	{
		IMG_UINT32			uInputIdx;
		PUSC_LIST_ENTRY		psInputListEntry;
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		psHw->uNrNonDependentTextureLoads = psPS->uNrPixelShaderInputs;
		psHw->psNonDependentTextureLoads = 
			psState->pfnAlloc(sizeof(psHw->psNonDependentTextureLoads[0]) * psHw->uNrNonDependentTextureLoads);
		if (psHw->psNonDependentTextureLoads == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Error creating non-dependent texture load list");
		}

		for (psInputListEntry = psPS->sPixelShaderInputs.psHead, uInputIdx = 0;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext, uInputIdx++)
		{
			PPIXELSHADER_INPUT	psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);

			psHw->psNonDependentTextureLoads[uInputIdx] = psInput->sLoad;
		}

	}
	else
	{
		psHw->uNrNonDependentTextureLoads = 0;
		psHw->psNonDependentTextureLoads = NULL;
	}

	/*
		Copy the number of temporary registers used in secondary update program.
	*/
	psHw->uSecTemporaryRegisterCount = psState->uSecTemporaryRegisterCount;

	/*
		Copy the number of output registers used.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX ||
		psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		psHw->uOutputRegisterCount = psState->sShader.psVS->uVertexShaderNumOutputs;
	}
	else
	{
		psHw->uOutputRegisterCount = 0;
	}

	/*
		Copy the number of primary attribute registers used.
	*/
	psHw->uPrimaryAttributeCount = psState->sHWRegs.uNumPrimaryAttributes;

	/*
		Copy the constant remapping table.
	*/
	for (uConstsBuffNum = 0; uConstsBuffNum < psState->uNumOfConstsBuffsAvailable; uConstsBuffNum++)
	{	
		PCONSTANT_BUFFER	psConstBuf = &psState->asConstantBuffer[uConstsBuffNum];

		if (psConstBuf->bInUse)
		{
			uErrCode = CopyConstantTable(psState,
										 psConstBuf,
										 uConstsBuffNum,
										 &psHw->psMemRemappingForConstsBuff[uConstsBuffNum].psConstMap,
										 &psHw->psMemRemappingForConstsBuff[uConstsBuffNum].uConstCount,
										 &psHw->psMemRemappingForConstsBuff[uConstsBuffNum].uMaximumConstCount);
			if (uErrCode != UF_OK)
			{
				USC_ERROR(UF_ERR_NO_MEMORY, "Error creating memory constant mapping table");
			}
		}
	}	

	/*
		Copy the in-register constant table.
	*/
	uErrCode = CopySAMappedConstantsTable(psState, 											
										  &psHw->psInRegisterConstMap, 
										  &psHw->uInRegisterConstCount, 
										  &psHw->uMaximumInRegisterConstCount);
	if (uErrCode != UF_OK)
	{
		USC_ERROR(UF_ERR_NO_MEMORY, "Error creating register constant mapping table");
	}

	/*
		Set up the overall count of secondary attributes used for constants.
	*/
	psHw->uConstSecAttrCount = psState->sSAProg.uConstSecAttrCount;

	/*
		Set up the returned flags.
	*/
	psHw->uFlags = 0;
	if (CanSetEndFlag(psState, psState->psMainProg) == IMG_FALSE)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_LABEL_AT_END;
	}
	if (psState->psSecAttrProg && CanSetEndFlag(psState, psState->psSecAttrProg) == IMG_FALSE)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_SAPROG_LABEL_AT_END;
	}
	for (i = 0; i < psHw->uNrNonDependentTextureLoads; i++)
	{
		if (psHw->psNonDependentTextureLoads[i].uTexture != UNIFLEX_TEXTURE_NONE)
		{
			psHw->uFlags |= UNIFLEX_HW_FLAGS_TEXTUREREADS;
		}
	}
	if	(psState->uFlags & USC_FLAGS_DEPENDENTREADS)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_TEXTUREREADS;
	}
	if	((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT))
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_TEXKILL_USED;
	}
	if	(psState->uNumDynamicBranches > 0)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE;
	}
	if	((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT))
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_DEPTHFEEDBACK;
	}
	if	(psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_OMASKFEEDBACK;
	}
	if (psState->uFlags & USC_FLAGS_PRIMARYCONSTLOADS)
	{
		psHw->uFlags |= UNIFLEX_HW_FLAGS_PRIMARYCONSTLOADS;
	}

	psHw->uIndexableTempTotalSize = psState->uIndexableTempArraySize * psState->uMaxSimultaneousInstances;

	psHw->iSAAddressAdjust = -(IMG_INT32)(psState->uMemOffsetAdjust * sizeof(IMG_UINT32));
	psHw->psProgramParameters = psState->psSAOffsets;

	if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) && (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_PRIMATTR))
	{
		if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8)
		{
			PFIXED_REG_DATA	psOut = psState->sShader.psPS->psFixedHwPARegReg;

			ASSERT(psOut->sPReg.uType == USEASM_REGTYPE_PRIMATTR);
			ASSERT(psOut->sPReg.uNumber < psState->sHWRegs.uNumPrimaryAttributes);
			psHw->uPAOutputRegNum = psOut->sPReg.uNumber;
		}
		else
		{
			psHw->uPAOutputRegNum = 0;
		}
	}
	else
	{
		psHw->uPAOutputRegNum = USC_UNDEF;
	}

	/*
		Copy information about which texture stages should have anisotropic filtering disabled.
	*/
	if (psState->psSAOffsets->uTextureCount > 0)
	{
		IMG_UINT32		uNonAnisoTextureStagesArraySize;

		uNonAnisoTextureStagesArraySize = UINTS_TO_SPAN_BITS(psState->psSAOffsets->uTextureCount) * sizeof(IMG_UINT32);
		psHw->auNonAnisoTextureStages = psState->pfnAlloc(uNonAnisoTextureStagesArraySize);
		if (psHw->auNonAnisoTextureStages == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Error creating auNonAnisoTexStages");
		}
		memcpy(psHw->auNonAnisoTextureStages, psState->auNonAnisoTexStages, uNonAnisoTextureStagesArraySize);
	}
	else
	{
		psHw->auNonAnisoTextureStages = NULL;
	}

	psHw->uSpillAreaSize = 
		psState->uSpillAreaSize * psState->uMaxSimultaneousInstances * sizeof(IMG_UINT32);
	psHw->uTextureStateSize = psState->uTexStateSize;

	/*
		Copy information about the format conversion mode to set in the texture state.
	*/
	if (psState->psSAOffsets->uTextureCount > 0)
	{
		psHw->asTextureUnpackFormat = 
			psState->pfnAlloc(sizeof(psHw->asTextureUnpackFormat[0]) * psState->psSAOffsets->uTextureCount);
		if (psHw->asTextureUnpackFormat == NULL)
		{
			USC_ERROR(UF_ERR_NO_MEMORY, "Error creating texture unpacking formats");
		}
		memcpy(psHw->asTextureUnpackFormat, 
			   psState->asTextureUnpackFormat, 
			   sizeof(psHw->asTextureUnpackFormat[0]) * psState->psSAOffsets->uTextureCount);
	}
	else
	{
		psHw->asTextureUnpackFormat = NULL;
	}

	/*
		Record the VS input usage
	*/
	if	(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

		for	(i = 0; i < (psVS->uVSInputPARegCount + 0x1F) >> 5; i++)
		{
			psHw->auVSInputsUsed[i] = psVS->auVSInputPARegUsage[i];
		}
		for (; i < EURASIA_USE_PRIMATTR_BANK_SIZE >> 5; i++)
		{
			psHw->auVSInputsUsed[i] = 0;
		}
	}

	#ifdef SRC_DEBUG
	/* 
		Successfully compiled! Generated source line cycle cost table.
	*/
	CreateSourceCycleCountTable(psState, &psHw->asTableSourceCycle);
	#endif /* SRC_DEBUG */

	for(i = 0; i < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; i++)
	{
		psHw->auValidShaderOutputs[i] = psState->puPackedShaderOutputs[i];
	}

	/******************** All UNIFLEX_HW setup above this line. ************/

	/*
		No longer handling exceptions
	*/ 
	psState->bExceptionReturnValid = IMG_FALSE;

	/*
		UNIFLEX_HW structure created OK
	*/
	psState->psUseasmContext = NULL;

	return UF_OK;
}

#endif /* defined(OUTPUT_USCHW) */
