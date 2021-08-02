/******************************************************************************
 * Name         : uspbin.c
 * Title        : USP-Binary output
 * Created      : Nov 2006
 *
 * Copyright    : 2002-2007 by Imagination Technologies Limited.
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
 * $Log: uspbin.c $
 * of 4.
 *  --- Revision Logs Removed --- -iftikhar.ahmad
 *****************************************************************************/
#include "uscshrd.h"
#include "bitops.h"
#include "uspbin.h"
#include "layout.h"
#include "usedef.h"
#include <limits.h>

/*
	State used while building USP-compatible pre-compiled shader data
*/
typedef IMG_VOID (*PFN_BUILD_PC_SHADER_4)(IMG_PVOID*, IMG_UINT32);
typedef IMG_VOID (*PFN_BUILD_PC_SHADER_2)(IMG_PVOID*, IMG_UINT16);
typedef IMG_VOID (*PFN_BUILD_PC_SHADER_1)(IMG_PVOID*, IMG_UINT8);
typedef IMG_VOID (*PFN_BUILD_PC_SHADER_N)(IMG_PVOID*, IMG_UINT32, IMG_PVOID);

typedef struct _BUILD_PC_SHADER_STATE_
{
	PINTERMEDIATE_STATE		psState;

	PFN_BUILD_PC_SHADER_4	pfnWrite4;
	PFN_BUILD_PC_SHADER_2	pfnWrite2;
	PFN_BUILD_PC_SHADER_1	pfnWrite1;
	PFN_BUILD_PC_SHADER_N	pfnWriteN;
	IMG_PVOID				pvData;

	IMG_BOOL				bEfoFmtCtl;
	IMG_BOOL				bColFmtCtl;
	MOE_DATA				asMoeIncSwiz[USC_MAX_MOE_OPERANDS];
	IMG_UINT32				auMoeBaseOffset[USC_MAX_MOE_OPERANDS];

	IMG_UINT32				uShaderSize;
	IMG_UINT32				uProgStartLabelID;
	IMG_UINT32				uPTPhase0EndLabelID;
	IMG_UINT32				uPTPhase1StartLabelID;
	IMG_UINT32				uPTSplitPhase1StartLabelID;
	IMG_BOOL				bProgEndIsLabel;
	IMG_UINT32				uTempResultRegs;
	IMG_UINT32				uPAResultRegs;
	IMG_UINT32				uOutputResultRegs;
	IMG_BOOL				bNoResultRemapping;
	IREGLIVENESS_ITERATOR	sIRegIterator;
} BUILD_PC_SHADER_STATE, *PBUILD_PC_SHADER_STATE;

/****************************************************************************
 FUNCTION	: PCShaderSkip4

 PURPOSE	: Skip 4 bytes in a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderSkip4(IMG_PVOID* ppvPCShader)
{
	*ppvPCShader = (IMG_PUINT8)*ppvPCShader + 4;
}

/****************************************************************************
 FUNCTION	: PCShaderSkip2

 PURPOSE	: Skip a 2 bytes in a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderSkip2(IMG_PVOID* ppvPCShader)
{
	*ppvPCShader = (IMG_PUINT8)*ppvPCShader + 2;
}

/****************************************************************************
 FUNCTION	: PCShaderSkip1

 PURPOSE	: Skip a byte in a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderSkip1(IMG_PVOID*	ppvPCShader)
{
	*ppvPCShader = (IMG_PUINT8)*ppvPCShader + 1;
}

/****************************************************************************
 FUNCTION	: PCShaderSkipN

 PURPOSE	: Skip N bytes in a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data
			  uCount		- The nuymber of bytes of data to skip

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderSkipN(IMG_PVOID* ppvPCShader, IMG_UINT32 uCount)
{
	*ppvPCShader = (IMG_PUINT8)*ppvPCShader + uCount;
}

/****************************************************************************
 FUNCTION	: PCShaderWrite4

 PURPOSE	: Add a 4-byte value to a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data
			  uValue		- The 32-bit value to write

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderWrite4(IMG_PVOID* ppvPCShader, IMG_UINT32 uValue)
{
	IMG_PUINT8	puSrc, puDest;

	puDest	= (IMG_PUINT8)*ppvPCShader;
	puSrc	= (IMG_PUINT8)&uValue;

	puDest[0] = puSrc[0];
	puDest[1] = puSrc[1];
	puDest[2] = puSrc[2];
	puDest[3] = puSrc[3];

	*ppvPCShader = puDest + 4;
}

/****************************************************************************
 FUNCTION	: PCShaderWrite2

 PURPOSE	: Add a 2-byte value to a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data
			  uValue		- The 16-bit value to write

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderWrite2(IMG_PVOID* ppvPCShader, IMG_UINT16	uValue)
{
	IMG_PUINT8	puSrc, puDest;

	puDest	= (IMG_PUINT8)*ppvPCShader;
	puSrc	= (IMG_PUINT8)&uValue;

	puDest[0] = puSrc[0];
	puDest[1] = puSrc[1];

	*ppvPCShader = puDest + 2;
}

/****************************************************************************
 FUNCTION	: PCShaderWrite1

 PURPOSE	: Add a byte value to a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data
			  uValue		- The 16-bit value to write

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderWrite1(IMG_PVOID* ppvPCShader, IMG_UINT8 uValue)
{
	IMG_PUINT8	puSrc, puDest;

	puDest	= (IMG_PUINT8)*ppvPCShader;
	puSrc	= (IMG_PUINT8)&uValue;

	puDest[0] = puSrc[0];

	*ppvPCShader = puDest + 1;
}

/****************************************************************************
 FUNCTION	: PCShaderWriteN

 PURPOSE	: Add a N-bytes of data to a pre-compiled binary shader

 PARAMETERS	: ppvPCShader	- The current location in the PC shader data
			  uCount		- The nuymber of bytes of data to write
			  pvData		- The data to be written

 RETURNS	: The address.
*****************************************************************************/
static IMG_VOID PCShaderWriteN(IMG_PVOID*	ppvPCShader,
							   IMG_UINT32	uCount,
							   IMG_PVOID	pvData)
{
	IMG_PUINT8	puSrc, puDest;

	puDest	= (IMG_PUINT8)*ppvPCShader;
	puSrc	= (IMG_PUINT8)pvData;

	while (uCount--)
	{
		*puDest++ = *puSrc++;
	}

	*ppvPCShader = puDest;
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
	PVR_UNREFERENCED_PARAMETER(uLabel);
	PVR_UNREFERENCED_PARAMETER(pvContext);

	/*
		Dummy function to get Useasm to assemble branch instructions without
		having to record references to labels and then fix them up once
		a final label definition is encountered (which never happens when
		generating USP output, since we don't tell Useasm about the labels
		at all!).
	*/
	return 0;
}

/*****************************************************************************
 FUNCTION	: UseAssemblerError

 PURPOSE	: Called by the assembler to report a problem encoding an instruction.

 PARAMETERS	: pvContext		- The context pointer originally passed to Useasm
							  (actually the USC internal state)
			  psInst		- The problematic instruction
			  pszFmt, ...	- Description of the problem encountered.

 RETURNS	: None.
*****************************************************************************/
static IMG_VOID UseAssemblerError(IMG_PVOID pvContext, 
								  PUSE_INST psInst, 
								  IMG_CHAR	*pszFmt, ...)
{
	PINTERMEDIATE_STATE psState = (PINTERMEDIATE_STATE)pvContext;

	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(pszFmt);
	imgabort();
}

/*****************************************************************************
 FUNCTION	: GetHWOperandsUsedForArg

 PURPOSE	: Get the operands that will be used for the HW instruction
			  generated for an internal one

 PARAMETERS	: psInst	- The instruction of interest
			  uArgIdx	- The intermediate instruction arg of interest
						  (0 = dest, 1+ = sources).

 RETURNS	: Bitmask indicating which HW operand(s) will be used for the
			  specified intermediate instruction argument. Bit 0 = dest,
			  Bits 1-3 = src0-2
*****************************************************************************/
IMG_INTERNAL 
IMG_UINT32 GetHWOperandsUsedForArg(PINST psInst, IMG_UINT32 uArgIdx)
{
	const INST_DESC*	psInstDesc;
	IMG_UINT32			uHWOperandsUsed;
	IMG_UINT32			i;

	psInstDesc		= &g_psInstDesc[psInst->eOpcode];
	uHWOperandsUsed = 0;

	if	(uArgIdx == 0)
	{
		/*
			Check whether the HW destination is used.
		*/
		if	(psInstDesc->bHasDest && psInst->uDestCount > 0)
		{
			uHWOperandsUsed |= 1U << 0;
		}
	}
	else
	{
		const IMG_UINT32	*puMoeArgRemap;
		IMG_UINT32			uArgCount;
		
		/*
			Examine the remapping info for MOE state (which relates intermediate
			args to HW operands), to see which HW operands will be used for
			the specified intermediate source arg.
		*/
		uArgCount		= psInst->uArgumentCount;
		puMoeArgRemap	= psInstDesc->puMoeArgRemap;

		uArgIdx--;
		/*
			Find the HW sources corresponding to this intermediate-source
		*/
		if	(uArgIdx < uArgCount)
		{
			for	(i = 0; i < 3; i++)
			{
				if	(puMoeArgRemap[i] == uArgIdx)
				{
					uHWOperandsUsed |= (2U << i);
				}
			}
		}
	}

	return uHWOperandsUsed;
}

/*****************************************************************************
 FUNCTION	: GetShaderResultInfo

 PURPOSE	: Get information about where the overall results of a shader
			  have been placed

 PARAMETERS	: psState			- Compiler state.
			  puResultRegType	- The register-type used for the results
			  puResultRegNum	- Index of the first register used for
								  the result
			  peResultRegFmt	- The data-format of the shader results
			  puResultRegCount	- The number of consecutive registers used	
								  for the shader results.

 RETURNS	: none
*****************************************************************************/
static IMG_VOID GetShaderResultInfo(PINTERMEDIATE_STATE	psState,
									IMG_PUINT32			puResultRegType,
									IMG_PUINT32			puResultRegNum,
									PUF_REGFORMAT		peResultRegFmt,
									IMG_PUINT32			puResultRegCount)
{
	if	(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		if (psPS->psColFixedReg == NULL)
		{
			*puResultRegType	= USC_UNDEF;
			*puResultRegNum		= USC_UNDEF;
			*peResultRegFmt		= USC_UNDEF;
			*puResultRegCount	= 0;
		}
		else
		{
			PFIXED_REG_DATA	psColOutput = psPS->psColFixedReg;

			*puResultRegType	= psState->psSAOffsets->uPackDestType;
			*puResultRegNum		= psColOutput->auVRegNum[0];
			*peResultRegFmt		= psState->psSAOffsets->ePackDestFormat;
			*puResultRegCount	= psPS->uColOutputCount;
		}
	}
	else if	((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
	{
		if	((psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS) == 0 || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
		{
			*puResultRegType	= USEASM_REGTYPE_OUTPUT;
			*puResultRegNum		= 0;
			*peResultRegFmt		= UF_REGFORMAT_F32;
			*puResultRegCount	= psState->sHWRegs.uNumOutputRegisters;
		}
		else
		{
			*puResultRegType	= USEASM_REGTYPE_PRIMATTR;
			*puResultRegNum		= 0;
			*peResultRegFmt		= UF_REGFORMAT_F32;
			*puResultRegCount	= psState->sHWRegs.uNumOutputRegisters;
		}
	}
}

static IMG_VOID GetShaderAltResultRegs(PINTERMEDIATE_STATE	psState, 
									   IMG_PUINT32			puTempResultReg, 
									   IMG_PUINT32			puPAResultReg, 
									   IMG_PUINT32			puOutputResultReg, 
									   IMG_PBOOL			pbNoRemapping)
{
	PARG				psTempFixedReg;
	PARG				psPAFixedReg;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	*pbNoRemapping = IMG_FALSE;
	if	(
			(psState->uCompilerFlags & UF_SPLITFEEDBACK) != 0 &&
			(psState->psSAOffsets->uFeedbackInstCount > 0)
		)
	{
		if	(psState->bResultWrittenInPhase0)
		{
			*pbNoRemapping = IMG_TRUE;
		}
	}

	/*
		Which entry in the fixed register array represents the shader outputs
		in the temporary bank.
	*/
	psTempFixedReg = NULL;
	if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
	{
		psTempFixedReg = &psPS->psColFixedReg->sPReg;
	}
	else
	{
		if ((psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE) == 0 && psPS->uAltTempFixedReg != USC_UNDEF)
		{
			psTempFixedReg = &psPS->psColFixedReg->asAltPRegs[psPS->uAltTempFixedReg - 1];
		}
	}

	/*
		Copy the register number of the start of the temporary bank shader outputs.
	*/
	*puTempResultReg = 0; 
	if (psTempFixedReg != NULL)
	{
		*puTempResultReg = psTempFixedReg->uNumber;
	}

	/*
		Which entry in the fixed register array represents the shader outputs
		in the primary atttribute bank.
	*/
	psPAFixedReg = NULL;
	if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_PRIMATTR)
	{
		psPAFixedReg = &psPS->psColFixedReg->sPReg;
	}
	else
	{
		if (
				(psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS) == 0 && 
				(psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE) == 0 &&
				psPS->uAltPAFixedReg != USC_UNDEF
		   )
		{
			psPAFixedReg = &psPS->psColFixedReg->asAltPRegs[psPS->uAltPAFixedReg - 1];
		}
	}

	/*
		Copy the register number of the start of the primary attribute shader outputs.
	*/
	*puPAResultReg = 0;
	if (psPAFixedReg != NULL)
	{
		*puPAResultReg = psPAFixedReg->uNumber;
	}

	/*
		The shader outputs in the output register bank 
	*/
	*puOutputResultReg = 0;	
}

/*****************************************************************************
 FUNCTION	: CalcConstTableSizeForMemMappedConsts

 PURPOSE	: Calculate the number of entries for an output constant-mapping
			  table for memory mapped constants.

 PARAMETERS	: psState		- Compiler state.
			  psConstBuf	- Information about the constant buffer.

 RETURNS	: The number of entries needed in the output constant mapping
			  table.
*****************************************************************************/
static IMG_UINT32 CalcConstTableSizeForMemMappedConsts(PINTERMEDIATE_STATE psState, 
													   PCONSTANT_BUFFER psConstBuf)
{
	IMG_UINT32	uTableSize;
	IMG_UINT32	i;

	uTableSize = 0;
	for (i = 0; i < psConstBuf->uRemappedCount; i++)
	{		
		switch (((UNIFLEX_CONST_FORMAT)ArrayGet(psState, psConstBuf->psRemappedFormat, i)))
		{
			case UNIFLEX_CONST_FORMAT_STATIC:
			case UNIFLEX_CONST_FORMAT_F32:
			{
				uTableSize++;
				break;
			}
			case UNIFLEX_CONST_FORMAT_F16:
			{
				uTableSize += 2;
				break;
			}
			case UNIFLEX_CONST_FORMAT_C10:
			{
				IMG_UINT32	uSrcIdx = (IMG_UINT32)ArrayGet(psState, psConstBuf->psRemappedMap, i);
				if ((uSrcIdx & 3) == 3)
				{
					uTableSize++;
				}
				else
				{
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
					{
						uTableSize += 4;
					}
					else
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					{
						uTableSize += 3;
					}
				}
				break;
			}
			case UNIFLEX_CONST_FORMAT_U8:
			{
				uTableSize += 4;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return uTableSize;
}

/*****************************************************************************
 FUNCTION	: CalcConstTableSize

 PURPOSE	: Calculate the number of entries for an output constant-mapping
			  table

 PARAMETERS	: psState		- Compiler state.
			  uConstCount	- The number of constants used
			  psConstList	- List with the index and format for each constant.

 RETURNS	: The number of entries needed in the output constant mapping
			  table.
*****************************************************************************/
static IMG_UINT32 CalcConstTableSize(PINTERMEDIATE_STATE	psState,
									 IMG_UINT32				uConstCount,
									 PUSC_LIST				psConstList)
{
	IMG_UINT32		uTableSize;
	IMG_UINT32		i;
	PUSC_LIST_ENTRY	psListEntry;

	PVR_UNREFERENCED_PARAMETER(psState);

	uTableSize = 0;
	for (i = 0, psListEntry = psConstList->psHead; i < uConstCount; i++, psListEntry = psListEntry->psNext)
	{
		PINREGISTER_CONST	psConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);

		switch (psConst->eFormat)
		{
			case UNIFLEX_CONST_FORMAT_STATIC:
			case UNIFLEX_CONST_FORMAT_F32:
			{
				uTableSize++;
				break;
			}
			case UNIFLEX_CONST_FORMAT_F16:
			{
				uTableSize += 2;
				break;
			}
			case UNIFLEX_CONST_FORMAT_C10:
			{
				IMG_UINT32	uSrcIdx = psConst->uNum;
				if ((uSrcIdx & 3) == 3)
				{
					uTableSize++;
				}
				else
				{
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
					{
						uTableSize += 4;
					}
					else
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					{
						uTableSize += 3;
					}
				}
				break;
			}
			case UNIFLEX_CONST_FORMAT_U8:
			{
				uTableSize += 4;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return uTableSize;
}

/*****************************************************************************
 FUNCTION	: BuildPCBlockHdr

 PURPOSE	: Generate the header for a pre-compiled data block

 PARAMETERS	: psBPCSState	- The PC shader being built
			  eBlockType	- The type of block

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCBlockHdr(PBUILD_PC_SHADER_STATE	psBPCSState,
								USP_PC_BLOCK_TYPE		eBlockType)
{
	/*
		Add a block header to the shader
	*/
	psBPCSState->pfnWrite4(&psBPCSState->pvData, eBlockType);

	/* 
		There will always be a end block at the end of Shader.
	*/
	if(eBlockType != USP_PC_BLOCK_TYPE_END)
	{
		/*
			Record whether the most recently added block was a label
		*/	
		psBPCSState->bProgEndIsLabel = (eBlockType == USP_PC_BLOCK_TYPE_LABEL)
			? IMG_TRUE : IMG_FALSE;
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescHdr

 PURPOSE	: Generate the fixed-size data for the program descriptor
			  block

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uRegConstCount	- The number of entries in the list of
								  in-register constants
			  uMemConstCount	- The number of entries in the list of
								  in-memory constsnts

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildPCProgDescHdr(PBUILD_PC_SHADER_STATE	psBPCSState,
								   IMG_UINT32				uRegConstCount,
								   IMG_UINT32				uMemConstCount)
{
	PINTERMEDIATE_STATE		psState;
	USP_PC_BLOCK_PROGDESC	sProgDesc;
	IMG_UINT32				uResultRegType;
	IMG_UINT32				uResultRegNum;
	UF_REGFORMAT			eResultRegFmt;
	IMG_UINT32				uResultRegCount;
	IMG_UINT32				i;

	psState = psBPCSState->psState;

	/*
		Fill out the fixed-size data for the program description
	*/
	memset(&sProgDesc, 0, sizeof(USP_PC_BLOCK_PROGDESC));

	sProgDesc.uFlags	= 0;
	if	((psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE))
	{
		sProgDesc.uFlags |= USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE;
	}
	if	((psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS))
	{
		sProgDesc.uFlags |= USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS;
	}	
	if	(psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC)
	{
		sProgDesc.uFlags |= USP_PC_PROGDESC_FLAGS_SPLITFEEDBACKCALC;
	}
	if	(psState->uFlags2 & USC_FLAGS2_SPLITCALC)
	{
		sProgDesc.uFlags |= USP_PC_PROGDESC_FLAGS_SPLITCALC;
	}
	sProgDesc.uUFFlags	= psState->uCompilerFlags;

	sProgDesc.uHWFlags = 0;
	if	(psState->uFlags & USC_FLAGS_DEPENDENTREADS)
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_TEXTUREREADS;
	}
	if	((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT))
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_TEXKILL_USED;
	}
	if	((psState->uNumDynamicBranches > 0) || (psState->uFlags2 & USC_FLAGS2_USES_MUTEXES))
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE;
	}
	if	((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT))
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_DEPTHFEEDBACK;
	}
	if	((psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT))
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_OMASKFEEDBACK;
	}
	if	(psBPCSState->bProgEndIsLabel)
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_LABEL_AT_END;
	}
	if (psState->psSecAttrProg && CanSetEndFlag(psState, psState->psSecAttrProg) == IMG_FALSE)
	{
		sProgDesc.uHWFlags |= UNIFLEX_HW_FLAGS_SAPROG_LABEL_AT_END;
	}

	/*
		Setup information about the target device
	*/
	sProgDesc.uTargetDev = psState->psSAOffsets->sTarget.eID;
	sProgDesc.uTargetRev = psState->psSAOffsets->sTarget.uiRev;

	sProgDesc.uBRNCount	 = 0;
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697)
	{
		sProgDesc.uBRNCount++;
	}
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21713)
	{
		sProgDesc.uBRNCount++;
	}
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21784)
	{
		sProgDesc.uBRNCount++;
	}

	sProgDesc.uSAUsageFlags	= 0;

	/*
		Secondary constant buffer never used.
	*/
	sProgDesc.uMemConst2BaseAddrSAReg = 0;

	/*
		Record whether in-register texture-state needs to be loaded for the shader
	*/
	sProgDesc.uTexStateFirstSAReg	= USHRT_MAX;
	sProgDesc.uRegTexStateCount		= 0;

	/*
		Record whether in-memory primary constants are needed for the shader

		NB: We always record the base-address for memory constants as it may be
			needed by the USP when texture-state spills over into memory.
	*/
	if	(uMemConstCount > 0)
	{
		sProgDesc.uSAUsageFlags |= USP_PC_SAUSAGEFLAG_MEMCONSTBASE;
		sProgDesc.uMemConstCount = uMemConstCount;
	}
	else
	{
		sProgDesc.uMemConstCount = 0;
	}

	sProgDesc.uMemConstBaseAddrSAReg	= (IMG_UINT16)psState->psSAOffsets->uConstantBase;
	sProgDesc.uTexStateMemOffset		= psState->asConstantBuffer[UF_CONSTBUFFERID_LEGACY].uRemappedCount;

	/*
		Record whether in-register primary constants are needed for the shader

		NB: The range of SA registers allowed for constants is always recorded, since
			this range now represents the total space available for both constants and 
			texture-state (the USC treats consts and tex-state as the same). Even when
			the shader uses no constants, the USP still needs to know the range of SAs
			available in-case the shader needs texture-state.
	*/
	if	(psState->sSAProg.uConstSecAttrCount > 0)
	{
		sProgDesc.uSAUsageFlags |= USP_PC_SAUSAGEFLAG_REMAPPEDCONSTS;

		sProgDesc.uRegConstsSARegCount	= (IMG_UINT16)psState->sSAProg.uConstSecAttrCount;
		sProgDesc.uRegConstCount		= (IMG_UINT16)uRegConstCount;
	}
	else
	{
		sProgDesc.uRegConstsSARegCount	= 0;
		sProgDesc.uRegConstCount		= 0;
	}

	sProgDesc.uRegConstsFirstSAReg		= (IMG_UINT16)psState->psSAOffsets->uInRegisterConstantOffset;
	sProgDesc.uRegConstsMaxSARegCount	= (IMG_UINT16)psState->psSAOffsets->uInRegisterConstantLimit;

	/*
		Record whether indexable temp-space is needed for the shader
	*/
	if	(psState->uFlags & USC_FLAGS_INDEXABLETEMPS_USED)
	{
		sProgDesc.uSAUsageFlags |= USP_PC_SAUSAGEFLAG_INDEXTEMPMEMBASE;

		sProgDesc.uIndexedTempBaseAddrSAReg	= (IMG_UINT16)psState->psSAOffsets->uIndexableTempBase;
		sProgDesc.uIndexedTempTotalSize		= psState->uIndexableTempArraySize * 
											  psState->uMaxSimultaneousInstances;
	}
	else
	{
		sProgDesc.uIndexedTempBaseAddrSAReg	= USHRT_MAX;
		sProgDesc.uIndexedTempTotalSize		= 0;
	}

	/*
		Record whether the temp-register spill area is needed for the shader
	*/
	if	(psState->uSpillAreaSize > 0)
	{
		sProgDesc.uSAUsageFlags |= USP_PC_SAUSAGEFLAG_SCRATCHMEMBASE;

		sProgDesc.uScratchMemBaseAddrSAReg	= (IMG_UINT16)psState->psSAOffsets->uScratchBase;
		sProgDesc.uScratchAreaSize			= psState->uSpillAreaSize * 
											  psState->uMaxSimultaneousInstances *
											  sizeof(IMG_UINT32);
	}
	else
	{
		sProgDesc.uScratchMemBaseAddrSAReg	= USHRT_MAX;
		sProgDesc.uScratchAreaSize			= 0;
	}

	sProgDesc.uExtraPARegs			= (IMG_UINT16)psState->psSAOffsets->uExtraPARegisters;
	sProgDesc.uPARegCount			= (IMG_UINT16)psState->sHWRegs.uNumPrimaryAttributes;
	sProgDesc.uTempRegCount			= (IMG_UINT16)psState->uTemporaryRegisterCount;
	sProgDesc.uSecTempRegCount		= (IMG_UINT16)psState->uSecTemporaryRegisterCount;
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		sProgDesc.uPSInputCount			= (IMG_UINT16)psState->sShader.psPS->uNrPixelShaderInputs;
	}
	else
	{
		sProgDesc.uPSInputCount			= 0;
	}	
	ASSERT(psState->psSAOffsets->uTextureCount <= USP_MAX_SAMPLER_IDX);
	memset(sProgDesc.auNonAnisoTexStages, 0, sizeof(sProgDesc.auNonAnisoTexStages));
	memcpy(sProgDesc.auNonAnisoTexStages, 
		   psState->auNonAnisoTexStages, 
		   UINTS_TO_SPAN_BITS(psState->psSAOffsets->uTextureCount) * sizeof(IMG_UINT32));
	sProgDesc.iSAAddressAdjust		= (IMG_INT16)-(IMG_INT32)(psState->uMemOffsetAdjust * sizeof(IMG_UINT32));

	/*
		Record where the pixel-shader result has been placed
	*/
	GetShaderResultInfo(psState,
						&uResultRegType,
						&uResultRegNum,
						&eResultRegFmt,
						&uResultRegCount);
	switch(psState->psSAOffsets->eShaderType)
	{
		case USC_SHADERTYPE_PIXEL:
		{
			sProgDesc.uShaderType = USP_PC_SHADERTYPE_PIXEL;
			break;
		}
		case USC_SHADERTYPE_VERTEX:
		{
			sProgDesc.uShaderType = USP_PC_SHADERTYPE_VERTEX;
			break;
		}
		case USC_SHADERTYPE_GEOMETRY:
		{
			sProgDesc.uShaderType = USP_PC_SHADERTYPE_GEOMETRY;
			break;
		}		
		default:
		{
			imgabort();
		}
	}

	if	(sProgDesc.uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		sProgDesc.uPSResultRegNum	= (IMG_UINT16)uResultRegNum;
		sProgDesc.uPSResultRegCount	= (IMG_UINT16)uResultRegCount;

		switch (uResultRegType)
		{
			case USEASM_REGTYPE_TEMP:
			{
				sProgDesc.uPSResultRegType = USP_PC_PSRESULT_REGTYPE_TEMP;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				sProgDesc.uPSResultRegType = USP_PC_PSRESULT_REGTYPE_PA;
				break;
			}
			case USEASM_REGTYPE_OUTPUT:
			default:
			{
				sProgDesc.uPSResultRegType = USP_PC_PSRESULT_REGTYPE_OUTPUT;
				break;
			}
		}

		switch (eResultRegFmt)
		{
			case UF_REGFORMAT_F32:
			{
				sProgDesc.uPSResultRegFmt	= USP_PC_PSRESULT_REGFMT_F32;
				break;
			}
			case UF_REGFORMAT_F16:
			{
				sProgDesc.uPSResultRegFmt	= USP_PC_PSRESULT_REGFMT_F16;
				break;
			}
			case UF_REGFORMAT_C10:
			{
				sProgDesc.uPSResultRegFmt	= USP_PC_PSRESULT_REGFMT_C10;
				break;
			}
			case UF_REGFORMAT_U8:
			default:
			{
				sProgDesc.uPSResultRegFmt	= USP_PC_PSRESULT_REGFMT_U8;
				break;
			}
		}
	}

	/*
		Record where the pixel-shader results could be placed
	*/
	sProgDesc.uPSResultTempReg		= (IMG_UINT16)psBPCSState->uTempResultRegs;
	sProgDesc.uPSResultPAReg		= (IMG_UINT16)psBPCSState->uPAResultRegs;
	sProgDesc.uPSResultOutputReg	= (IMG_UINT16)psBPCSState->uOutputResultRegs;

	/*
		Record ID's of special labels inserted to mark important locations in
		the shader
	*/
	sProgDesc.uProgStartLabelID		= (IMG_UINT16)psBPCSState->uProgStartLabelID;
	sProgDesc.uPTPhase0EndLabelID	= (IMG_UINT16)psBPCSState->uPTPhase0EndLabelID;
	sProgDesc.uPTPhase1StartLabelID	= (IMG_UINT16)psBPCSState->uPTPhase1StartLabelID;
	sProgDesc.uPTSplitPhase1StartLabelID	= (IMG_UINT16)psBPCSState->uPTSplitPhase1StartLabelID;

	sProgDesc.uSAUpdateInstCount	= (IMG_UINT16)psState->uSAProgInstCount;

	/*
		For vertex-shaders record the number of PA registers used for input-data
		(not including pre-sampled texture-data)

		NB: Data indicating which PA regs are used is written as part of the
			variable size data following the header.
	*/	
	if	(sProgDesc.uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		sProgDesc.uVSInputPARegCount = 0;
	}
	else if	(sProgDesc.uShaderType == USP_PC_SHADERTYPE_VERTEX)
	{
		sProgDesc.uVSInputPARegCount = (IMG_UINT16)psState->sShader.psVS->uVSInputPARegCount;
	}
	else if (sProgDesc.uShaderType == USP_PC_SHADERTYPE_GEOMETRY)
	{
		/* 
			Note: Geometry shaders are not yet supported in case of USPBIN output. 
		*/
		sProgDesc.uVSInputPARegCount = 0;
	}

	sProgDesc.uShaderOutputsCount = USC_MAX_SHADER_OUTPUTS;

	/*
		Write the fixed-size data for the program descriptor block
	*/
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uShaderType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uFlags);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uUFFlags);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uHWFlags);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uTargetDev);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uTargetRev);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uBRNCount);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uSAUsageFlags);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uMemConstBaseAddrSAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uMemConst2BaseAddrSAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uScratchMemBaseAddrSAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uTexStateFirstSAReg);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uTexStateMemOffset);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uRegConstsFirstSAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uRegConstsMaxSARegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uIndexedTempBaseAddrSAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uExtraPARegs);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPARegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uTempRegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uSecTempRegCount);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uIndexedTempTotalSize);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSInputCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uRegConstCount);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uMemConstCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uRegTexStateCount);
	for (i = 0; i < (sizeof(sProgDesc.auNonAnisoTexStages) / sizeof(sProgDesc.auNonAnisoTexStages[0])); i++)
	{
		psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.auNonAnisoTexStages[i]);
	}
	for (i = 0; i < (sizeof(sProgDesc.auAnisoTexStages) / sizeof(sProgDesc.auAnisoTexStages[0])); i++)
	{
		psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.auAnisoTexStages[i]);
	}
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sProgDesc.uScratchAreaSize);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.iSAAddressAdjust);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultRegNum);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultRegFmt);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultRegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultTempReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultPAReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPSResultOutputReg);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uProgStartLabelID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPTPhase0EndLabelID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPTPhase1StartLabelID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uPTSplitPhase1StartLabelID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uSAUpdateInstCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uRegConstsSARegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uVSInputPARegCount);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sProgDesc.uShaderOutputsCount);
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescVSInputUsage

 PURPOSE	: Generate the VS input-usage data for a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescVSInputUsage(PBUILD_PC_SHADER_STATE psBPCSState)
{
	PINTERMEDIATE_STATE	psState;

	psState	= psBPCSState->psState;

	/*
		Record the VS input usage
	*/
	if	((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX))
	{
		IMG_UINT32	uPARegsUsed;
		IMG_PUINT32	puVSInputsUsage;
		IMG_UINT32	i;

		uPARegsUsed		= psState->sShader.psVS->uVSInputPARegCount;
		puVSInputsUsage	= psState->sShader.psVS->auVSInputPARegUsage;

		for	(i = 0; i < (uPARegsUsed + 0x1F) >> 5; i++)
		{
			psBPCSState->pfnWrite4(&psBPCSState->pvData, puVSInputsUsage[i]);
		}
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescBRNList

 PURPOSE	: Generate the list of active BRNs

 PARAMETERS	: psBPCSState	- Data used during PC shader construction

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescBRNList(PBUILD_PC_SHADER_STATE psBPCSState)
{
	PINTERMEDIATE_STATE	psState;

	psState	= psBPCSState->psState;

	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21697)
	{
		psBPCSState->pfnWrite4(&psBPCSState->pvData, 21697);
	}
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21713)
	{
		psBPCSState->pfnWrite4(&psBPCSState->pvData, 21713);
	}
	if	(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_21784)
	{
		psBPCSState->pfnWrite4(&psBPCSState->pvData, 21784);
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescPSInputList

 PURPOSE	: Generate the list of iterated or pre-sampled input data for
			  pixel-shaders, as part of the program-descriptor block.

 PARAMETERS	: psBPCSState	- Data used during PC shader construction

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescPSInputList(PBUILD_PC_SHADER_STATE psBPCSState)
{
	PINTERMEDIATE_STATE	psState;
	IMG_UINT32			uPSInputCount;
	PUSC_LIST_ENTRY		psInputListEntry;

	psState			= psBPCSState->psState;

	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}
	uPSInputCount	= psState->sShader.psPS->uNrPixelShaderInputs;

	if	(uPSInputCount == 0)
	{
		return;
	}

	/*
		Write the list of iterated/pre-sampled pixel-shader input data
	*/
	for (psInputListEntry = psState->sShader.psPS->sPixelShaderInputs.psHead;
		 psInputListEntry != NULL;
		 psInputListEntry = psInputListEntry->psNext)
	{
		PPIXELSHADER_INPUT		psInput = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
		USP_PC_PSINPUT_LOAD		sPSInputLoad;
		PUNIFLEX_TEXTURE_LOAD	psUSCTexLoad;

		psUSCTexLoad = &psInput->sLoad;

		/*
			Setup the PS input load to be written...
		*/
		memset(&sPSInputLoad, 0, sizeof(USP_PC_PSINPUT_LOAD));

		sPSInputLoad.uFlags = 0;

		switch (psUSCTexLoad->eIterationType)
		{
			case UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE:
			{
				sPSInputLoad.uCoord = (IMG_UINT16)(USP_PC_PSINPUT_COORD_TC0 + psUSCTexLoad->uCoordinate);
				break;
			}
			case UNIFLEX_ITERATION_TYPE_COLOUR:
			{
				sPSInputLoad.uCoord = (IMG_UINT16)(USP_PC_PSINPUT_COORD_V0 + psUSCTexLoad->uCoordinate);
				break;
			}
			case UNIFLEX_ITERATION_TYPE_FOG:
			{
				sPSInputLoad.uCoord = USP_PC_PSINPUT_COORD_FOG;
				break;
			}
			case UNIFLEX_ITERATION_TYPE_POSITION:
			{
				sPSInputLoad.uCoord = USP_PC_PSINPUT_COORD_POS;
				break;
			}
			case UNIFLEX_ITERATION_TYPE_VPRIM:
			{
				sPSInputLoad.uCoord = USP_PC_PSINPUT_COORD_VPRIM;
				break;
			}
			default: imgabort();
		}

		sPSInputLoad.uDataSize = (IMG_UINT16)psUSCTexLoad->uNumAttributes;

		if	(psUSCTexLoad->uTexture == UNIFLEX_TEXTURE_NONE)
		{
			sPSInputLoad.uFlags	|= USP_PC_PSINPUT_FLAG_ITERATION;

			/*
				If this enrtry was added as padding to avoid the USP overwriting
				PS outputs with extra pre-sampled/iterated input data, flag it 
				as a dummy entry.

				The USP can remove this (and avoid an unnecessary iteration) if
				there are no iterations or non-dependent reads after it once it
				has generated code.
			*/
			if	(psInput->uFlags & PIXELSHADER_INPUT_FLAG_OUTPUT_PADDING)
			{
				sPSInputLoad.uFlags |= USP_PC_PSINPUT_FLAG_PADDING;
			}

			sPSInputLoad.uCoordDim = (IMG_UINT16)psUSCTexLoad->uCoordinateDimension;

			switch (psUSCTexLoad->eFormat)
			{
				default:
				case UNIFLEX_TEXLOAD_FORMAT_F32:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_F32;
					break;
				}
				case UNIFLEX_TEXLOAD_FORMAT_F16:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_F16;
					break;
				}
				case UNIFLEX_TEXLOAD_FORMAT_C10:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_C10;
					break;
				}
				case UNIFLEX_TEXLOAD_FORMAT_U8:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_U8;
					break;
				}
			}

			if	(psUSCTexLoad->bCentroid)
			{
				sPSInputLoad.uFlags |= USP_PC_PSINPUT_FLAG_CENTROID;
			}
		}
		else
		{
			sPSInputLoad.uFlags		= 0;
			sPSInputLoad.uTexture	= (IMG_UINT16)psUSCTexLoad->uTexture;

			if	(psUSCTexLoad->bProjected)
			{
				sPSInputLoad.uFlags |= USP_PC_PSINPUT_FLAG_PROJECTED;
			}
			if	(psUSCTexLoad->bCentroid)
			{
				sPSInputLoad.uFlags |= USP_PC_PSINPUT_FLAG_CENTROID;
			}

			switch (psUSCTexLoad->eFormat)
			{
				case UNIFLEX_TEXLOAD_FORMAT_F32:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_F32;
					break;
				}
				case UNIFLEX_TEXLOAD_FORMAT_F16:
				{
					sPSInputLoad.uFormat = USP_PC_PSINPUT_FMT_F16;
					break;
				}

				/* For C10/U8 format conversion by hardware is not required */
				default:
				{
					sPSInputLoad.uFormat = (IMG_UINT16)0;
					break;
				}
			}

		}

		/*
			...and append it to the PC shader
		*/
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uFlags);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uTexture);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uCoord);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uCoordDim);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uFormat);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPSInputLoad.uDataSize);
	}
}

/*****************************************************************************
 FUNCTION	: AddC10ConstantEntry

 PURPOSE	: Generate a list of constants that may be used by the program,
			  as part of the PC program-descriptor block, for memory mapped 
			  constants.

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uSrcIdx			- The source constant number.
			  uChan				- The source constant channel.
			  uConstDestIdx		- The destination secondary attribute or
								memory offset.
			  uSrcShift			- 

 RETURNS	: void
*****************************************************************************/
static IMG_VOID AddC10ConstantEntry(PBUILD_PC_SHADER_STATE	psBPCSState, 
									IMG_UINT32				uSrcIdx,
									IMG_UINT32				uSrcChan,
									IMG_UINT32				uConstDestIdx,
									IMG_UINT32				uDestShift,
									IMG_UINT32				uSrcShift)
{
	USP_PC_CONST_LOAD sConstLoad;

	/*
		Setup a constant-load entry for this channel of the constant...
	*/
	sConstLoad.uFormat					= USP_PC_CONST_FMT_C10;
	sConstLoad.sSrc.sConst.uSrcIdx		= (IMG_UINT16)(uSrcIdx + uSrcChan);
	sConstLoad.sSrc.sConst.uSrcShift	= (IMG_UINT16)uSrcShift;
	sConstLoad.uDestIdx					= (IMG_UINT16)uConstDestIdx;
	sConstLoad.uDestShift				= (IMG_UINT16)uDestShift;

	/*
		... and append it to the PC shader
	*/
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcShift);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
}

/*****************************************************************************
 FUNCTION	: AddC10UnpackedConstant

 PURPOSE	: Generate the entries in the constant table for loading part of
			  range of C10 data indexed down to individual components.

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uConstDestIdx		- Destination (either secondary attribute or
								memory).
			  uSrcIdx			- The source constant number.

 RETURNS	: void
*****************************************************************************/
static IMG_VOID AddC10UnpackedConstant(PBUILD_PC_SHADER_STATE	psBPCSState,
									  IMG_UINT32				uConstDestIdx,
									  IMG_UINT32				uSrcIdx)
{
	IMG_UINT32	uSrcConst = uSrcIdx & ~(VECTOR_LENGTH - 1);
	IMG_UINT32	uSrcChan = uSrcIdx & (VECTOR_LENGTH - 1);
	IMG_UINT32	uChan;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	const IMG_UINT32 auBGRA[] = {USC_BLUE_CHAN, USC_GREEN_CHAN, USC_RED_CHAN,	USC_ALPHA_CHAN};
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	const IMG_UINT32 auRGBA[] = {USC_RED_CHAN,	USC_GREEN_CHAN,	USC_BLUE_CHAN,	USC_ALPHA_CHAN};
	IMG_UINT32 const* puChanOrder;

	/*
		Which channel order does this constant use?
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psBPCSState->psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA) != 0)
	{
		puChanOrder = auBGRA;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		puChanOrder = auRGBA;
	}

	/*
		DEST[0..9]		= CONST.RED		/ CONST.BLUE
		DEST[16..25]	= CONST.GREEN	/ CONST.ALPHA
	*/
	for (uChan = 0; uChan < 2; uChan++)
	{
		AddC10ConstantEntry(psBPCSState, 
							uSrcConst, 
							uChan, 
							uConstDestIdx, 
							16 * uChan /* uDestShift */, 
							puChanOrder[uSrcChan + uChan] * 10 /* uSrcShift */);
	}
}

/*****************************************************************************
 FUNCTION	: AddC10Constant

 PURPOSE	: Generate the entries in the constant table for loading either the
			  first or second 32-bits of a vec4 C10 constant.

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uConstDestIdx		- Destination (either secondary attribute or
								memory).
			  uSrcIdx			- The source constant number.

 RETURNS	: void
*****************************************************************************/
static IMG_VOID AddC10Constant(PBUILD_PC_SHADER_STATE	psBPCSState,
							   IMG_UINT32				uConstDestIdx,
							   IMG_UINT32				uSrcIdx)
{
	IMG_UINT32			uChan;
	IMG_UINT32			uSrcConst = uSrcIdx & ~3U;
	IMG_UINT32			uSrcChan = uSrcIdx & 3U;
	PINTERMEDIATE_STATE	psState = psBPCSState->psState;

	if (uSrcChan == 0)
	{
		IMG_BOOL	bC10RGBA;

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
			Create entries for the RED, GREEN and BLUE channels.
		*/
		for (uChan = 0; uChan < 3; uChan++)
		{
			IMG_UINT32	uDestShift;

			if (bC10RGBA)
			{
				uDestShift = (IMG_UINT16)(uChan * 10);
			}
			else
			{
				/*
					Otherwise swap to BGRA order.
				*/
				uDestShift = (IMG_UINT16)(20 - uChan * 10);
			}

			AddC10ConstantEntry(psBPCSState, uSrcConst, uChan, uConstDestIdx, uDestShift, 0 /* uSrcShift */);
		}

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Add an extra entry for the first two bits of the ALPHA channel.
		*/
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			AddC10ConstantEntry(psBPCSState, 
								uSrcConst, 
								3 /* uSrcChan */, 
								uConstDestIdx, 
								30 /* uDestShift */, 
								0 /* uSrcShift */);
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}
	else
	{
		IMG_UINT32	uSrcShift;

		ASSERT(uSrcChan == 3);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
		{
			/*
				Put the top eight bits of the ALPHA channel in the LSBs.
			*/
			uSrcShift = 2;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			/*
				Put the ALPHA channel in the LSBs.
			*/
			uSrcShift = 0;
		}

		AddC10ConstantEntry(psBPCSState, uSrcConst, 3 /* uSrcChan */, uConstDestIdx, 0 /* uDestShift */, uSrcShift);
	}
}

static
IMG_VOID AddU8Constant(PBUILD_PC_SHADER_STATE	psBPCSState,
					   IMG_UINT32				uConstDestIdx,
					   IMG_UINT32				uSrcIdx)
/*****************************************************************************
 FUNCTION	: AddU8Constant

 PURPOSE	: Generate the entries in the constant table for loading a vec4 U8 constant.

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uConstDestIdx		- Destination (either secondary attribute or
								memory).
			  uSrcIdx			- The source constant number.

 RETURNS	: void
*****************************************************************************/
{
	IMG_UINT32			uQuarter;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	PINTERMEDIATE_STATE	psState = psBPCSState->psState;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	for (uQuarter = 0; uQuarter < 4; uQuarter++)
	{
		USP_PC_CONST_LOAD sConstLoad;

		/*
			Setup a constant-load entry for this quarter of the constant...
		*/
		sConstLoad.uFormat = USP_PC_CONST_FMT_U8;

		sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)(uSrcIdx + uQuarter);
		sConstLoad.uDestIdx				= (IMG_UINT16)uConstDestIdx;

		if (uQuarter < 3)
		{
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_C10U8_CHAN_ORDER_RGBA)
			{
				sConstLoad.uDestShift	= (IMG_UINT16)(uQuarter * 8);
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				sConstLoad.uDestShift	= (IMG_UINT16)(16 - uQuarter * 8);
			}
		}
		else
		{
			sConstLoad.uDestShift	= (IMG_UINT16)(uQuarter * 8);
		}

		/*
			... and append it to the PC shader
		*/
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescConstListForMemoryMappedConsts

 PURPOSE	: Generate a list of constants that may be used by the program,
			  as part of the PC program-descriptor block, for memory mapped 
			  constants.

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uNumEntries		- The number of entries expected in the list
			  psConstBuf		- Information about the constant buffer.

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescConstListForMemoryMappedConsts(PBUILD_PC_SHADER_STATE psBPCSState, 
															  IMG_UINT32 uNumEntries, 
															  PCONSTANT_BUFFER psConstBuf)
{
	PINTERMEDIATE_STATE	psState;
	IMG_UINT32			uConstDestIdx;
	IMG_UINT32			i;
	USC_PARRAY			psFormat;
	USC_PARRAY			psMap;

	if	(uNumEntries == 0)
	{
		return;
	}

	/*
		Write the list of constants that need to be loaded
	*/
	psState = psBPCSState->psState;
	psFormat = psConstBuf->psRemappedFormat;
	psMap = psConstBuf->psRemappedMap;

	for (uConstDestIdx = 0, i = 0; i < psConstBuf->uRemappedCount; i++)
	{
		USP_PC_CONST_LOAD sConstLoad;

		/*
			Setup constant load entries for this input constant
		*/
		memset(&sConstLoad, 0, sizeof(USP_PC_CONST_LOAD));

		switch (((UNIFLEX_CONST_FORMAT)ArrayGet(psState, psFormat, i)))
		{
			case UNIFLEX_CONST_FORMAT_F32:
			{
				/*
					Setup a constant-load entry for this constant...
				*/
				sConstLoad.uFormat				= USP_PC_CONST_FMT_F32;
				sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)((IMG_UINT32)ArrayGet(psState, psMap, i));
				sConstLoad.uDestIdx				= (IMG_UINT16)uConstDestIdx;
				sConstLoad.uDestShift			= 0;

				/*
					... and append it to the PC shader
				*/
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);

				uConstDestIdx++;
				break;
			}

			case UNIFLEX_CONST_FORMAT_F16:
			{
				IMG_UINT32	uHalf;
				IMG_UINT32	uSrcIdx = (IMG_UINT32)ArrayGet(psState, psMap, i);

				for (uHalf = 0; uHalf < 2; uHalf++)
				{
					/*
						Setup a constant-load entry for this half of the constant...
					*/
					sConstLoad.uFormat = USP_PC_CONST_FMT_F16;

					if ((uSrcIdx & 3) != 3)
					{
						sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)(uSrcIdx + uHalf);
					}
					else
					{
						sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)uSrcIdx;
					}

					sConstLoad.uDestIdx		= (IMG_UINT16)uConstDestIdx;
					sConstLoad.uDestShift	= (IMG_UINT16)(uHalf * 16);

					/*
						... and append it to the PC shader
					*/
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
				}

				uConstDestIdx++;
				break;
			}

			case UNIFLEX_CONST_FORMAT_C10:
			{
				IMG_UINT32	uSrcIdx = (IMG_UINT32)ArrayGet(psState, psMap, i);

				if (uSrcIdx & REMAPPED_CONSTANT_UNPACKEDC10)
				{
					AddC10UnpackedConstant(psBPCSState, uConstDestIdx, uSrcIdx & ~REMAPPED_CONSTANT_UNPACKEDC10);
				}
				else
				{
					AddC10Constant(psBPCSState, uConstDestIdx, uSrcIdx);
				}
				uConstDestIdx++;
				break;
			}

			case UNIFLEX_CONST_FORMAT_U8:
			{
				IMG_UINT32	uSrcIdx = (IMG_UINT32)ArrayGet(psState, psMap, i);

				AddU8Constant(psBPCSState, uConstDestIdx, uSrcIdx);
				uConstDestIdx++;
				break;
			}

			case UNIFLEX_CONST_FORMAT_STATIC:
			{
				/*
					Setup a constant-load entry for this static constant...
				*/
				sConstLoad.uFormat			= USP_PC_CONST_FMT_STATIC;
				sConstLoad.sSrc.uStaticVal	= ((IMG_UINT32)ArrayGet(psState, psMap, i));
				sConstLoad.uDestIdx			= (IMG_UINT16)uConstDestIdx;
				sConstLoad.uDestShift		= 0;

				/*
					... and append it to the PC shader
				*/
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
				psBPCSState->pfnWrite4(&psBPCSState->pvData, sConstLoad.sSrc.uStaticVal);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);

				uConstDestIdx++;
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDescConstList

 PURPOSE	: Generate a list of constants that may be used by the program,
			  as part of the PC program-descriptor block

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uNumEntries		- The number of entries expected in the list
			  uOrgConstCount	- The number of 32-bit constants used
			  puMap				- The original index for each constant used
			  peFormat			- The data format for each constant used

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescConstList(PBUILD_PC_SHADER_STATE psBPCSState,
										 IMG_UINT32				uNumEntries,
										 IMG_UINT32				uOrgConstCount,
										 PUSC_LIST				psOrgConstList)
{
	IMG_UINT32			i;
	PUSC_LIST_ENTRY		psListEntry;

	if	(uNumEntries == 0)
	{
		return;
	}

	/*
		Write the list of constants that need to be loaded
	*/
	for (i = 0, psListEntry = psOrgConstList->psHead; 
		 i < uOrgConstCount; 
		 i++, psListEntry = psListEntry->psNext)
	{
		USP_PC_CONST_LOAD	sConstLoad;
		PINREGISTER_CONST	psOrgConst = IMG_CONTAINING_RECORD(psListEntry, PINREGISTER_CONST, sListEntry);
		IMG_UINT32			uSrcIdx = psOrgConst->uNum;
		PFIXED_REG_DATA		psDestAttr;
		IMG_UINT32			uDestAttr;
		IMG_UINT32			uConstDestIdx;

		/*
			Get the destination for this constant load.
		*/
		psDestAttr = psOrgConst->psResult->psFixedReg;
		uDestAttr = psDestAttr->sPReg.uNumber;
		uConstDestIdx = uDestAttr - psBPCSState->psState->psSAOffsets->uInRegisterConstantOffset;

		/*
			Setup constant load entries for this input constant
		*/
		memset(&sConstLoad, 0, sizeof(USP_PC_CONST_LOAD));

		switch (psOrgConst->eFormat)
		{
			case UNIFLEX_CONST_FORMAT_F32:
			{
				/*
					Setup a constant-load entry for this constant...
				*/
				sConstLoad.uFormat				= USP_PC_CONST_FMT_F32;
				sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)uSrcIdx;
				sConstLoad.uDestIdx				= (IMG_UINT16)uConstDestIdx;
				sConstLoad.uDestShift			= 0;

				/*
					... and append it to the PC shader
				*/
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
				break;
			}

			case UNIFLEX_CONST_FORMAT_F16:
			{
				IMG_UINT32	uHalf;

				for (uHalf = 0; uHalf < 2; uHalf++)
				{
					/*
						Setup a constant-load entry for this half of the constant...
					*/
					sConstLoad.uFormat = USP_PC_CONST_FMT_F16;

					if ((uSrcIdx & 3) != 3)
					{
						sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)(uSrcIdx + uHalf);
					}
					else
					{
						sConstLoad.sSrc.sConst.uSrcIdx	= (IMG_UINT16)uSrcIdx;
					}

					sConstLoad.uDestIdx		= (IMG_UINT16)uConstDestIdx;
					sConstLoad.uDestShift	= (IMG_UINT16)(uHalf * 16);

					/*
						... and append it to the PC shader
					*/
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.sSrc.sConst.uSrcIdx);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
					psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
				}
				break;
			}

			case UNIFLEX_CONST_FORMAT_C10:
			{
				AddC10Constant(psBPCSState, uConstDestIdx, uSrcIdx);
				break;
			}

			case UNIFLEX_CONST_FORMAT_U8:
			{
				AddU8Constant(psBPCSState, uConstDestIdx, uSrcIdx);
				break;
			}

			case UNIFLEX_CONST_FORMAT_STATIC:
			{
				/*
					Setup a constant-load entry for this static constant...
				*/
				sConstLoad.uFormat			= USP_PC_CONST_FMT_STATIC;
				sConstLoad.sSrc.uStaticVal	= uSrcIdx;
				sConstLoad.uDestIdx			= (IMG_UINT16)uConstDestIdx;
				sConstLoad.uDestShift		= 0;

				/*
					... and append it to the PC shader
				*/
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uFormat);
				psBPCSState->pfnWrite4(&psBPCSState->pvData, sConstLoad.sSrc.uStaticVal);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestIdx);
				psBPCSState->pfnWrite2(&psBPCSState->pvData, sConstLoad.uDestShift);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

static IMG_VOID EncodeInstsBP(PINTERMEDIATE_STATE psState,
							  PCODEBLOCK psBlock,
							  IMG_PVOID pvBPCSState)
{
	PBUILD_PC_SHADER_STATE	psBPCSState = (PBUILD_PC_SHADER_STATE)pvBPCSState;
	PINST					psInst;
	for	(psInst = psBlock->psBody;
		 psInst;
		 psInst = psInst->psNext)
	{
		IMG_UINT32	auPCInst[EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32)];

		/*
			Encode a HW instruction from this intermediate one...
		*/
		EncodeInst(psState,
				   psInst,
				   &auPCInst[0],
				   0);

		/*
			...and append it to the PC shader
		*/
		psBPCSState->pfnWriteN(&psBPCSState->pvData,
							   EURASIA_USE_INSTRUCTION_SIZE,
							   auPCInst);
	}
}


/*****************************************************************************
 FUNCTION	: BuildPCProgDescSAUpdateCode

 PURPOSE	: Generate the list of pre-compiled instructions forming the
			  SA-register update program, as part of a PC program-descriptor
			  block

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  uInstCount		- The number of instructions forming the
								  secondary update code
			  psSecAttrBlock	- Codeblock containing the secondary-upload
								  code.

 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCProgDescSAUpdateCode(PBUILD_PC_SHADER_STATE	psBPCSState,
											IMG_UINT32				uInstCount,
											PFUNC					psSecAttrBlock)
{
	PINTERMEDIATE_STATE	psState;

	if	(uInstCount == 0)
	{
		return;
	}

	/*
		Pre-compile the SA update program
	*/
	psState	= psBPCSState->psState;

	DoOnCfgBasicBlocks(psState, &psSecAttrBlock->sCfg, DebugPrintSF,
						   EncodeInstsBP, IMG_TRUE /*ICALLs*/, psBPCSState);
}

static 
IMG_VOID BuildPCProgDescValidShdrOutputs(PBUILD_PC_SHADER_STATE	psBPCSState)
/*****************************************************************************
 FUNCTION	: BuildPCProgDescValidShdrOutputs

 PURPOSE	: Generates a Bit Mask of Shader Outputs that are Valid after 
			  compilation, as part of a PC program-descriptor block

 PARAMETERS	: psBPCSState		- Data used during PC shader construction

 RETURNS	: void
*****************************************************************************/
{
	PINTERMEDIATE_STATE	psState;	
	psState	= psBPCSState->psState;
	{
		IMG_UINT32 uOutputsCountInDwords;
		IMG_UINT32 uBitsInLastDword;
		IMG_UINT32 uDwordIdx;
		uOutputsCountInDwords = USC_MAX_SHADER_OUTPUTS / (sizeof(psState->puPackedShaderOutputs[0])*8);
		for(uDwordIdx = 0; uDwordIdx < uOutputsCountInDwords; uDwordIdx++)
		{
			psBPCSState->pfnWrite4(&psBPCSState->pvData, psState->puPackedShaderOutputs[uDwordIdx]);
		}
		uBitsInLastDword = USC_MAX_SHADER_OUTPUTS % (sizeof(psState->puPackedShaderOutputs[0])*8);
		if(uBitsInLastDword)
		{
			IMG_UINT32	uBitMask;
			IMG_UINT32	uLastDword;
			uBitMask = UINT_MAX;
			uBitMask = (uBitMask << uBitsInLastDword);
			uBitMask = ~uBitMask;
			uLastDword = (psState->puPackedShaderOutputs[uOutputsCountInDwords]) & uBitMask;
			psBPCSState->pfnWrite4(&psBPCSState->pvData, uLastDword);
		}
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCProgDesc

 PURPOSE	: Generate the program descriptor block for a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildPCProgDesc(PBUILD_PC_SHADER_STATE psBPCSState)
{
	PINTERMEDIATE_STATE	psState;
	IMG_UINT32			uRegConstCount;
	IMG_UINT32			uMemConstCount;

	/*
		Calculate the number of entries we will need for the in-register
		and in-memory constants
	*/
	psState = psBPCSState->psState;

	uMemConstCount = CalcConstTableSizeForMemMappedConsts(psState, 
														  &psState->asConstantBuffer[UF_CONSTBUFFERID_LEGACY]);

	uRegConstCount = CalcConstTableSize(psState,
										psState->sSAProg.uInRegisterConstantCount, 
										&psState->sSAProg.sInRegisterConstantList);

	/*
		Write the block-header
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_PROGDESC);

	/*
		Add fixed-size data for the program-descriptor
	*/
	BuildPCProgDescHdr(psBPCSState, uRegConstCount, uMemConstCount);

	/*
		Add the list of active BRNs
	*/
	BuildPCProgDescBRNList(psBPCSState);

	/*
		Add the list of iterated/pre-sampled input data for pixel-shaders
	*/
	BuildPCProgDescPSInputList(psBPCSState);

	/*
		Add the list of memory constants used
	*/
	BuildPCProgDescConstListForMemoryMappedConsts(psBPCSState, 
												  uMemConstCount, 
												  &psState->asConstantBuffer[UF_CONSTBUFFERID_LEGACY]);

	/*
		Add the list of register constants used
	*/
	BuildPCProgDescConstList(psBPCSState,
							 uRegConstCount,
							 psState->sSAProg.uInRegisterConstantCount, 
							 &psState->sSAProg.sInRegisterConstantList);

	/*
		Add the pre-compiled register-constant update program
	*/
	BuildPCProgDescSAUpdateCode(psBPCSState,
								psState->uSAProgInstCount,								
								psState->psSecAttrProg);

	/*
		Build the PA-register usage info for vertex-shader input data
	*/
	BuildPCProgDescVSInputUsage(psBPCSState);
	BuildPCProgDescValidShdrOutputs(psBPCSState);
}

/*****************************************************************************
 FUNCTION	: SetupPCInstDesc

 PURPOSE	: Setup a PC instruction descriptor from an intermediate-inst

 PARAMETERS	: psBPCSState		- Data used during PC shader construction
			  psPCInstDesc		- The PC instruction descriptor to setup
			  psInst			- An intermediate instruction to setup
			  psIRegIterator	- The IRegs live before the instruciton

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID SetupPCInstDesc(PBUILD_PC_SHADER_STATE	psBPCSState,
								PUSP_PC_INSTDESC		psPCInstDesc,
								PINST					psInst,
								PIREGLIVENESS_ITERATOR	psIRegIterator)	
{
	const PINTERMEDIATE_STATE psState = psBPCSState->psState;
	IMG_UINT32	uShaderResultHWOperands;

	psPCInstDesc->uFlags = 0;

	/*
		Flag whether the IRegs are live prior to the instruction
	*/
	if	(UseDefIterateIRegLiveness_Current(psIRegIterator))
	{
		psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}

	/*
		Indicate whether we can set skipinv on this instruction
	*/
	if	(!GetBit(psInst->auFlag, INST_SKIPINV))
	{
		psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_DONT_SKIPINV;
	}

	/*
		Flag whether this instruction references registers that are part of
		the overall shader result
	*/
	uShaderResultHWOperands = psInst->uShaderResultHWOperands;
	if	(uShaderResultHWOperands && (psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE) == 0)
	{
		if	(!psBPCSState->bNoResultRemapping)
		{			
			if	(uShaderResultHWOperands & (1U << 0))
			{
				psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_DEST_USES_RESULT;
			}
			if	(uShaderResultHWOperands & (1U << 1))
			{
				psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_SRC0_USES_RESULT;	
			}
			if	(uShaderResultHWOperands & (1U << 2))
			{
				psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_SRC1_USES_RESULT;	
			}
			if	(uShaderResultHWOperands & (1U << 3))
			{
				psPCInstDesc->uFlags |= USP_PC_INSTDESC_FLAG_SRC2_USES_RESULT;	
			}
		}
	}

	/*
		Update the IRegs live to reflect the effects of the instruction
	*/
	UseDefIterateIRegLiveness_Next(psState, psIRegIterator, psInst);
}

static IMG_UINT32 ConvertMOESwizzleToHwFormat(PMOE_DATA	psMOEData)
/*****************************************************************************
 FUNCTION	: ConvertMOESwizzleToHwFormat

 PURPOSE	: Convert an MOE swizzle in the internal format to the hardware
			  format.

 PARAMETERS	: psMOEData		- Contains the MOE swizzle to convert.

 RETURNS	: The converted swizzle.
*****************************************************************************/
{
	IMG_UINT32	uIteration;
	IMG_UINT32	uSwizzle;

	uSwizzle = 0;
	for (uIteration = 0; uIteration < USC_MAX_SWIZZLE_SLOTS; uIteration++)
	{
		IMG_UINT32	uSel;

		uSel = psMOEData->u.s.auSwizzle[uIteration];
		uSwizzle |= (uSel << (EURASIA_USE_MOESWIZZLE_FIELD_SIZE * uIteration));
	}

	return ((uSwizzle << USP_PC_MOE_INCORSWIZ_SWIZ_SHIFT) & (~USP_PC_MOE_INCORSWIZ_SWIZ_CLRMSK)) |
		   USP_PC_MOE_INCORSWIZ_MODE_SWIZ;
}

/*****************************************************************************
 FUNCTION	: BuildHWCodeBlock

 PURPOSE	: Add initial data for a HW code block to a precompiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  uInstCount	- The number of instructions in the pre-compiled
							  block of code
			  psFirstInst	- The first instruciton for the block

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildHWCodeBlock(PBUILD_PC_SHADER_STATE	psBPCSState,
								 IMG_UINT16				uInstCount,
								 PINST					psFirstInst)
{
	PINTERMEDIATE_STATE	psState;
	USP_PC_BLOCK_HWCODE	sHWCodeBlock;
	PUSP_PC_MOESTATE	psPCMOEState;
	PINST				psInst;
	IMG_UINT32			i;

	/*
		Write the block-header
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_HWCODE);

	/*
		Setup the data describing the block of pre-compiled instructions...
	*/
	sHWCodeBlock.uInstCount = uInstCount;

	/*
		Setup the MOE state at the start of the block
	*/
	psPCMOEState = &sHWCodeBlock.sMOEState;

	psPCMOEState->uMOEFmtCtlFlags = 0;
	if	(psBPCSState->bEfoFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL;
	}
	if	(psBPCSState->bColFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_COLFMTCTL;
	}

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		IMG_UINT32	uIncOrSwiz;
		PMOE_DATA	psMoeIncSwiz;

		uIncOrSwiz = ((1U << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
					 USP_PC_MOE_INCORSWIZ_MODE_INC;

		psMoeIncSwiz = &psBPCSState->asMoeIncSwiz[i];
		switch (psMoeIncSwiz->eOperandMode)
		{
			case MOE_INCREMENT:
			{
				uIncOrSwiz = ((*((IMG_PUINT32)(&psMoeIncSwiz->u.iIncrement)) << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & 
							  (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
							 USP_PC_MOE_INCORSWIZ_MODE_INC;

				break;
			}

			case MOE_SWIZZLE:
			{
				uIncOrSwiz = ConvertMOESwizzleToHwFormat(psMoeIncSwiz);
				break;
			}
		}

		psPCMOEState->auMOEIncOrSwiz[i]	 = (IMG_UINT16)uIncOrSwiz;
		psPCMOEState->auMOEBaseOffset[i] = (IMG_UINT16)psBPCSState->auMoeBaseOffset[i];
	}

	/*
		...and append it to the PC shader
	*/
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sHWCodeBlock.uInstCount);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, psPCMOEState->uMOEFmtCtlFlags);
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEIncOrSwiz[i]);
	}
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEBaseOffset[i]);
	}

	/*
		Setup and write the descriptors for each of the instructions
	*/
	for (i = 0, psInst = psFirstInst; 
		 i < uInstCount; 
		 i++, psInst = psInst->psNext)
	{
		USP_PC_INSTDESC	sPCInstDesc;

		SetupPCInstDesc(psBPCSState, &sPCInstDesc, psInst, &psBPCSState->sIRegIterator);

		psBPCSState->pfnWrite2(&psBPCSState->pvData, sPCInstDesc.uFlags);
	}

	/*
		Assemble the instruction into the HW-code block
	*/
	psState = psBPCSState->psState;

	for (i = 0,	psInst = psFirstInst; 
		 i < uInstCount; 
		 i++, psInst = psInst->psNext)
	{
		IMG_UINT32	auPCInst[EURASIA_USE_INSTRUCTION_SIZE/sizeof(IMG_UINT32)];

		EncodeInst(psState,
				   psInst,
				   &auPCInst[0],
				   0);

		psBPCSState->pfnWriteN(&psBPCSState->pvData,
							   EURASIA_USE_INSTRUCTION_SIZE,
							   auPCInst);
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCLabel

 PURPOSE	: Add a label-block (marking the destination for a branch that
			  needs to be patched at runtime) to a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  uLabelID		- ID of the label to add

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildPCLabelCB(
								PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout, 
								IMG_UINT32 uDestLabel, IMG_BOOL bSyncEndDest)
{
	PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
	USP_PC_BLOCK_LABEL	sLabelBlock;
	
	PVR_UNREFERENCED_PARAMETER(psLayout);
	PVR_UNREFERENCED_PARAMETER(bSyncEndDest);
	/*
		Write the block-header
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_LABEL);

	/*
		Setup the label-block data...
	*/
	sLabelBlock.uLabelID = (IMG_UINT16)uDestLabel;

	/*
		...and append it to the PC shader
	*/
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sLabelBlock.uLabelID);
}

/*****************************************************************************
 FUNCTION	: BuildPCBranch

 PURPOSE	: Add a branch-block (i.e. a branch that will need to be patched
			  at runtime) to a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  eOpcode		- Opcode identifying the type of instruction
							  peforming a branch
			  uPredicate	- Predicate controlling the branch
			  bPredNegate	- Whether the sense of the predicate should
							  be inverted
			  uDestLabelID	- ID of the destination baleb for the branch
			  bSyncEnd		- Whether the SyncEnd flag should be set for
							  the branch

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildPCBranchCB(
									PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout, 
									IOPCODE eOpcode, IMG_PUINT32 puDestLabel, 
									IMG_UINT32 uPredicate, IMG_BOOL bPredNegate, 
									IMG_BOOL bSyncEnd,
									EXECPRED_BRANCHTYPE eExecPredBranchType)
{
	PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);	
	USP_PC_BLOCK_BRANCH	sBranchBlock;

	//Handle BRN22136
	CommonBranchCB(psState, psLayout, eOpcode,
				   puDestLabel, uPredicate,
				   bPredNegate, bSyncEnd);

	/*
		Write the header for a branch block
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_BRANCH);

	/*
		Setup the data for the branch...

		NB: Since the address for this branch must be setup by the USP,
			any branch offset calculated by Useasm is ignored.
	*/
	if (eOpcode == ILAPC)
	{
		ASSERT (puDestLabel == NULL);
		sBranchBlock.uDestLabelID = USHRT_MAX;
	}
	else
	{
		ASSERT (puDestLabel != NULL);
		sBranchBlock.uDestLabelID = (IMG_UINT16)(*puDestLabel);
	}

	//psState = psBPCSState->psState;

	EncodeJump(psState,
			   psState->psUseasmContext,
			   eOpcode,
			   sBranchBlock.uDestLabelID,
			   uPredicate,
			   bPredNegate,
			   sBranchBlock.auBaseBranchInst,
			   sBranchBlock.auBaseBranchInst,
			   IMG_FALSE,
			   bSyncEnd,
			   IMG_FALSE,
			   eExecPredBranchType);

	/*
		...and append it to the PC shader
	*/
	psBPCSState->pfnWriteN(&psBPCSState->pvData, 
						   EURASIA_USE_INSTRUCTION_SIZE,
						   sBranchBlock.auBaseBranchInst);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sBranchBlock.uDestLabelID);
}

/*****************************************************************************
 FUNCTION	: GetUSPPCRegFormat

 PURPOSE	: Returns the corresponding USP pre-compiled reg format for a given
 			  USC reg format

 PARAMETERS	: eUSCFmt 	- The USC register format

 RETURNS	: The USP register format
*****************************************************************************/
static USP_PC_SAMPLE_DEST_REGFMT GetUSPPCRegFormat(UF_REGFORMAT eUSCFmt)
{
	switch(eUSCFmt)
	{
		case 	UF_REGFORMAT_F32: return USP_PC_SAMPLE_DEST_REGFMT_F32;
		case	UF_REGFORMAT_F16: return USP_PC_SAMPLE_DEST_REGFMT_F16;
		case 	UF_REGFORMAT_C10: return USP_PC_SAMPLE_DEST_REGFMT_C10;
		case	UF_REGFORMAT_U8:  return USP_PC_SAMPLE_DEST_REGFMT_U8;
		case	UF_REGFORMAT_U16: return USP_PC_SAMPLE_DEST_REGFMT_U16;
		case	UF_REGFORMAT_I16: return USP_PC_SAMPLE_DEST_REGFMT_I16;
		case	UF_REGFORMAT_U32: return USP_PC_SAMPLE_DEST_REGFMT_U32;
		case	UF_REGFORMAT_I32: return USP_PC_SAMPLE_DEST_REGFMT_I32;
		default: 				  return USP_PC_SAMPLE_DEST_REGFMT_UNKNOWN;
	}
}

/*****************************************************************************
 FUNCTION	: GetUSPPCRegType

 PURPOSE	: Returns the corresponding USP pre-compiled reg type for a given
 			  USC reg type

 PARAMETERS	: eUSCType 	- The USC register format

 RETURNS	: The USP PC register format
*****************************************************************************/
static USP_PC_TEXTURE_WRITE_REGTYPE GetUSPPCRegType(USEASM_REGTYPE eUSCType,
													PINTERMEDIATE_STATE psState)
{
	switch(eUSCType)
	{
		case 	USEASM_REGTYPE_TEMP 	: return USP_PC_TEXTURE_WRITE_REGTYPE_TEMP;
		case	USEASM_REGTYPE_PRIMATTR	: return USP_PC_TEXTURE_WRITE_REGTYPE_PA;
		case 	USEASM_REGTYPE_SECATTR	: return USP_PC_TEXTURE_WRITE_REGTYPE_SA;
		case 	USEASM_REGTYPE_FPCONSTANT	: return USP_PC_TEXTURE_WRITE_REGTYPE_SPECIAL;
	    case 	USEASM_REGTYPE_FPINTERNAL	: return USP_PC_TEXTURE_WRITE_REGTYPE_INTERNAL;
		default: UscFail (psState, UF_ERR_GENERIC, "Invalid register format for image write");
	}
}

/*****************************************************************************
 FUNCTION	: BuildTextureWriteBlock

 PURPOSE	: Add an texture-write block to a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  eOpcode		- Opcode identifying the type of instruction
							  peforming an texture write

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildTextureWriteBlock(PBUILD_PC_SHADER_STATE psBPCSState,
									   PINST psInst)
{
	PINTERMEDIATE_STATE 		psState;
	USP_PC_BLOCK_TEXTURE_WRITE	sTextureWriteBlock;

	psState = psBPCSState->psState;
	
	/*
	  Write the header for a sample block
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_TEXTURE_WRITE);

	/*
	  Collect information about the texture write
	 */
	sTextureWriteBlock.uWriteID = psInst->u.psTexWrite->uWriteID;

	sTextureWriteBlock.uBaseAddrRegNum  = psInst->asArg[0].uNumber;
	sTextureWriteBlock.uBaseAddrRegType = GetUSPPCRegType(psInst->asArg[0].uType, psState);
	
	sTextureWriteBlock.uStrideRegNum 	= psInst->asArg[1].uNumber;
	sTextureWriteBlock.uStrideRegType 	= GetUSPPCRegType(psInst->asArg[1].uType, psState);

	/* Co-ordinate registers */
	sTextureWriteBlock.uCoordXRegNum 	= psInst->asArg[2].uNumber;
	sTextureWriteBlock.uCoordXRegType	= GetUSPPCRegType(psInst->asArg[2].uType, psState);

	sTextureWriteBlock.uCoordYRegNum 	= psInst->asArg[3].uNumber;
	sTextureWriteBlock.uCoordYRegType 	= GetUSPPCRegType(psInst->asArg[3].uType, psState);

	/* Colour registers */
	sTextureWriteBlock.uChannelXRegNum 	= psInst->asArg[4].uNumber;
	sTextureWriteBlock.uChannelXRegType	= GetUSPPCRegType(psInst->asArg[4].uType, psState);

	sTextureWriteBlock.uChannelYRegNum 	= psInst->asArg[5].uNumber;
	sTextureWriteBlock.uChannelYRegType	= GetUSPPCRegType(psInst->asArg[5].uType, psState);

	sTextureWriteBlock.uChannelZRegNum 	= psInst->asArg[6].uNumber;
	sTextureWriteBlock.uChannelZRegType	= GetUSPPCRegType(psInst->asArg[6].uType, psState);

	sTextureWriteBlock.uChannelWRegNum 	= psInst->asArg[7].uNumber;
	sTextureWriteBlock.uChannelWRegType	= GetUSPPCRegType(psInst->asArg[7].uType, psState);

	/* Temp reg start number */
	sTextureWriteBlock.uTempRegStartNum = psInst->u.psTexWrite->sTempReg.uNumber;

	/* Register format of channel data registers */
	sTextureWriteBlock.uChannelRegFmt	= GetUSPPCRegFormat(psInst->u.psTexWrite->uChannelRegFmt);

	/* Maximum number of temps we can use */
	sTextureWriteBlock.uMaxNumTemps		= GetUSPTextureWriteTemporaryCount();

	/* Setup instruction description flags */
	SetupPCInstDesc(psBPCSState, 
				    &sTextureWriteBlock.sInstDesc, 
					psInst, 
					&psBPCSState->sIRegIterator);

	/*
	  Append function call information data to the PC shader
	*/
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uWriteID);
	
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uBaseAddrRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uBaseAddrRegType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uStrideRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uStrideRegType);
	
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uCoordXRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uCoordXRegType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uCoordYRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uCoordYRegType);

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelXRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelXRegType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelYRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelYRegType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelZRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelZRegType);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelWRegNum);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelWRegType);

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uTempRegStartNum);

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uChannelRegFmt);

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sTextureWriteBlock.uMaxNumTemps);
	
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sTextureWriteBlock.sInstDesc.uFlags);
}

/*****************************************************************************
 FUNCTION	: BuildSampleBlock

 PURPOSE	: Add a sample block (for both depednent and non-dependent
			  samples) to a pre-compiled shader.

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  psInst		- The USP pseud-sample instruction to generate
							  a PC sample block for

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildSampleBlock(PBUILD_PC_SHADER_STATE	psBPCSState,
								 PINST					psInst)
{
	PINTERMEDIATE_STATE			psState;
	USP_PC_BLOCK_SAMPLE			sSampleBlock;
	PUSP_PC_MOESTATE			psPCMOEState;	
	IMG_UINT32					uFlags;	
	IMG_UINT32					i;
	PARG						psTempReg;
	PSMP_USP_PARAMS				psUSPSample = &psInst->u.psSmp->sUSPSample;

	psState = psBPCSState->psState;

	/*
		Write the header for a sample block
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_SAMPLE);

	/*
		Setup the common data for the sample...
	*/
	uFlags = 0;
	if	(psUSPSample->bNonDependent)
	{
		uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_NONDR;

		if	(psUSPSample->bProjected)
		{
			uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_PROJECTED;
		}
		if	(psUSPSample->bCentroid)
		{
			uFlags |= USP_PC_BLOCK_SAMPLE_FLAG_CENTROID;
		}
	}

	sSampleBlock.uSmpID			= psInst->u.psSmp->sUSPSample.uSmpID;
	sSampleBlock.uFlags			= (IMG_UINT16)uFlags;
	sSampleBlock.uTextureIdx	= (IMG_UINT16)psInst->u.psSmp->uTextureStage;	
	sSampleBlock.uSwizzle		= (IMG_UINT16)psUSPSample->uChanSwizzle;

	if( psState->uCompilerFlags & UF_OPENCL )
	{
		/*
		  OpenCL only has 3 read calls, read_image[f|i|ui] and so we are only
		  interested in those cases.
		*/
		switch(psInst->u.psSmp->sUSPSample.eTexPrecision)
		{
		    case UF_REGFORMAT_F32: sSampleBlock.uTexPrecision = USP_PC_SAMPLE_DEST_REGFMT_F32; break;
		    case UF_REGFORMAT_U32: sSampleBlock.uTexPrecision = USP_PC_SAMPLE_DEST_REGFMT_U32; break;
		    case UF_REGFORMAT_I32: sSampleBlock.uTexPrecision = USP_PC_SAMPLE_DEST_REGFMT_I32; break;
		    default:               sSampleBlock.uTexPrecision = USP_PC_SAMPLE_DEST_REGFMT_UNKNOWN;
		}
	}
	else
	{
		/*
		  Other APIs do not want this behavior.
		 */
		sSampleBlock.uTexPrecision = USP_PC_SAMPLE_DEST_REGFMT_UNKNOWN;
	}

	/*
		Copy the information about the temporary registers available for the
		USP use when unpacking the texture sample result.
	*/
	psTempReg = &psUSPSample->sTempReg;
	if (psTempReg->uType == USEASM_REGTYPE_TEMP)
	{
		sSampleBlock.uSampleTempRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
	}
	else
	{
		ASSERT(psTempReg->uType == USEASM_REGTYPE_PRIMATTR);
		sSampleBlock.uSampleTempRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_PA;
	}
	sSampleBlock.uSampleTempRegNum	= (IMG_UINT16)psTempReg->uNumber;

	sSampleBlock.uIRegsLiveBefore = (IMG_UINT16)UseDefIterateIRegLiveness_Current(&psBPCSState->sIRegIterator);
	sSampleBlock.uC10IRegsLiveBefore = (IMG_UINT16)UseDefIterateIRegLiveness_GetC10Mask(&psBPCSState->sIRegIterator);;

	SetupPCInstDesc(psBPCSState, 
				    &sSampleBlock.sInstDesc, 
					psInst, 
					&psBPCSState->sIRegIterator);

	if (psInst->asDest[0].uType == USEASM_REGTYPE_TEMP)
	{
		sSampleBlock.uBaseSampleDestRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
	}
	else
	{
		ASSERT(psInst->asDest[0].uType == USEASM_REGTYPE_PRIMATTR);
		sSampleBlock.uBaseSampleDestRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_PA;
	}

	sSampleBlock.uBaseSampleDestRegNum = (IMG_UINT16)psInst->asDest[0].uNumberPreMoe;
	
	switch (psInst->asDest[UF_MAX_CHUNKS_PER_TEXTURE].uType)
	{
		case USEASM_REGTYPE_OUTPUT:
		{
			sSampleBlock.uDirectDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			sSampleBlock.uDirectDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			sSampleBlock.uDirectDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		default:
		{
			sSampleBlock.uDirectDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
		}
	}
	sSampleBlock.uDirectDestRegNum = (IMG_UINT16)psInst->asDest[UF_MAX_CHUNKS_PER_TEXTURE].uNumberPreMoe;

	/*
		Setup the MOE state before the sample
	*/
	psPCMOEState = &sSampleBlock.sMOEState;

	psPCMOEState->uMOEFmtCtlFlags = 0;
	if	(psBPCSState->bEfoFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL;
	}
	if	(psBPCSState->bColFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_COLFMTCTL;
	}

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		IMG_UINT32	uIncOrSwiz;
		PMOE_DATA	psMoeIncSwiz;

		uIncOrSwiz = ((1U << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
					 USP_PC_MOE_INCORSWIZ_MODE_INC;

		psMoeIncSwiz = &psBPCSState->asMoeIncSwiz[i];
		switch (psMoeIncSwiz->eOperandMode)
		{
			case MOE_INCREMENT:
			{
				uIncOrSwiz = ((*((IMG_PUINT32)(&psMoeIncSwiz->u.iIncrement)) << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & 
							  (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
							 USP_PC_MOE_INCORSWIZ_MODE_INC;

				break;
			}

			case MOE_SWIZZLE:
			{
				uIncOrSwiz = ConvertMOESwizzleToHwFormat(psMoeIncSwiz);
				break;
			}
		}

		psPCMOEState->auMOEIncOrSwiz[i]	 = (IMG_UINT16)uIncOrSwiz;
		psPCMOEState->auMOEBaseOffset[i] = (IMG_UINT16)psBPCSState->auMoeBaseOffset[i];
	}

	/*
		Append the common sample data to the PC shader
	*/
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uFlags);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, psPCMOEState->uMOEFmtCtlFlags);
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEIncOrSwiz[i]);
	}
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEBaseOffset[i]);
	}

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sSampleBlock.uSmpID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.sInstDesc.uFlags);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uTextureIdx);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uSwizzle);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uBaseSampleDestRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uBaseSampleDestRegNum);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uDirectDestRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uDirectDestRegNum);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uSampleTempRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uSampleTempRegNum);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uIRegsLiveBefore);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uC10IRegsLiveBefore);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleBlock.uTexPrecision);

	/*
		Setup and write the data specific for dependent or non-dependent
		samples...
	*/
	if	(psUSPSample->bNonDependent)
	{
		static const USP_PC_SAMPLE_COORD aePCSampleCoordFromTCIndex[] = 
		{
			USP_PC_SAMPLE_COORD_TC0,
			USP_PC_SAMPLE_COORD_TC1,
			USP_PC_SAMPLE_COORD_TC2,
			USP_PC_SAMPLE_COORD_TC3,
			USP_PC_SAMPLE_COORD_TC4,
			USP_PC_SAMPLE_COORD_TC5,
			USP_PC_SAMPLE_COORD_TC6,
			USP_PC_SAMPLE_COORD_TC7,
			USP_PC_SAMPLE_COORD_TC8,
			USP_PC_SAMPLE_COORD_TC9
		};
		USP_PC_NONDR_SAMPLE_DATA	sNonDRSampleData;
		IMG_UINT32					uTexCoordIdx;

		uTexCoordIdx = psUSPSample->uNDRTexCoord;

		sNonDRSampleData.uCoord		= (IMG_UINT16)aePCSampleCoordFromTCIndex[uTexCoordIdx];
		sNonDRSampleData.uTexDim	= (IMG_UINT16)psInst->u.psSmp->uDimensionality;

		switch (psInst->asArg[0].uType)
		{
			case USEASM_REGTYPE_OUTPUT:
			{
				sNonDRSampleData.uSmpNdpDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
				break;
			}
			case USEASM_REGTYPE_PRIMATTR:
			{
				sNonDRSampleData.uSmpNdpDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
				break;
			}
			case USEASM_REGTYPE_FPINTERNAL:
			{
				sNonDRSampleData.uSmpNdpDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
				break;
			}
			case USEASM_REGTYPE_TEMP:
			default:
			{
				sNonDRSampleData.uSmpNdpDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
			}
		}
		sNonDRSampleData.uSmpNdpDirectSrcRegNum = (IMG_UINT16)psInst->asArg[0].uNumberPreMoe;

		psBPCSState->pfnWrite2(&psBPCSState->pvData, sNonDRSampleData.uCoord);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sNonDRSampleData.uTexDim);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sNonDRSampleData.uSmpNdpDirectSrcRegType);
		psBPCSState->pfnWrite2(&psBPCSState->pvData, sNonDRSampleData.uSmpNdpDirectSrcRegNum);
	}
	else
	{
		USP_PC_DR_SAMPLE_DATA	sDRSampleData;
		IMG_BOOL				bSkipInvState;
		ARG						sDest;
		static const IMG_UINT32	auSmpSources[4] = 
									{SMP_COORD_ARG_START, SMP_STATE_ARG_START, SMP_LOD_ARG_START, SMP_GRAD_ARG_START};
		IMG_UINT32				auSavedSmpRegisterNumbers[4];

		/*
			Force SkipInv to be set on SMP instructions that need gradients
		*/
		bSkipInvState = GetBit(psInst->auFlag, INST_SKIPINV);
		if	(
				(psInst->eOpcode == ISMP_USP) ||
				(psInst->eOpcode == ISMPBIAS_USP)
			)
		{
			SetBit(psInst->auFlag, INST_SKIPINV, 0);
		}

		/*
			Override the destination to avoid issues where it is something
			that isn't directly supported by the SMP instruction.

			NB:	The USP will override the dest anyway when generating the 
				sampling code.
		*/
		sDest = psInst->asDest[0];
		psInst->asDest[0].uType		= USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber	= 0;

		/*
			Change the sources to use the pre-MOE adjustment register numbers.
		*/
		for (i = 0; i < 4; i++)
		{
			PARG	psArg = &psInst->asArg[auSmpSources[i]];

			auSavedSmpRegisterNumbers[i] = psArg->uNumber;
			psArg->uNumber = psArg->uNumberPreMoe;
		}

		EncodeInst(psState,
				   psInst,
				   sDRSampleData.auBaseSmpInst,
				   sDRSampleData.auBaseSmpInst);

		SetBit(psInst->auFlag, INST_SKIPINV, bSkipInvState ? 1: 0);
		psInst->asDest[0] = sDest;

		/*
			Restore modified sources.
		*/
		for (i = 0; i < 4; i++)
		{
			PARG	psArg = &psInst->asArg[auSmpSources[i]];

			psArg->uNumber = auSavedSmpRegisterNumbers[i];
		}

		psBPCSState->pfnWrite4(&psBPCSState->pvData, sDRSampleData.auBaseSmpInst[0]);
		psBPCSState->pfnWrite4(&psBPCSState->pvData, sDRSampleData.auBaseSmpInst[1]);
	}
}

/*****************************************************************************
 FUNCTION	: BuildSampleUnpackBlock

 PURPOSE	: Add a sampleunpack block to a pre-compiled shader.

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  psInst		- The USP pseud-sample instruction to generate
							  a PC sample block for

 RETURNS	: none
*****************************************************************************/
static IMG_VOID BuildSampleUnpackBlock(	PBUILD_PC_SHADER_STATE	psBPCSState,
										PINST					psInst)
{
	PINTERMEDIATE_STATE			psState;
	USP_PC_BLOCK_SAMPLEUNPACK	sSampleUnpackBlock;
	PUSP_PC_MOESTATE			psPCMOEState;
	USP_PC_SAMPLE_DEST_REGTYPE	ePCSampleDestRegType;
	USP_PC_SAMPLE_DEST_REGFMT	ePCSampleDestRegFmt;	
	IMG_UINT32					uChansPerReg;
	IMG_UINT32					uCompMask;
	IMG_UINT32					i;
	PARG						psTempRegSrc;
	PARG						psTempReg;
	PSMPUNPACK_PARAMS			psUSPSampleUnpack = psInst->u.psSmpUnpack;

	psState = psBPCSState->psState;

	/*
		Write the header for a sample block
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_SAMPLEUNPACK);
	
	sSampleUnpackBlock.uSmpID			= psUSPSampleUnpack->uSmpID;
	sSampleUnpackBlock.uMask			= (IMG_UINT16)psUSPSampleUnpack->uChanMask;	
	sSampleUnpackBlock.uLive			= 0;	

	switch (psInst->asArg[UF_MAX_CHUNKS_PER_TEXTURE].uType)
	{
		case USEASM_REGTYPE_OUTPUT:
		{
			sSampleUnpackBlock.uDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			sSampleUnpackBlock.uDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			sSampleUnpackBlock.uDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		default:
		{
			sSampleUnpackBlock.uDirectSrcRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
		}
	}
	sSampleUnpackBlock.uDirectSrcRegNum = (IMG_UINT16)psInst->asArg[UF_MAX_CHUNKS_PER_TEXTURE].uNumberPreMoe;

	/*
		Copy the information about the temporary registers available for the
		USP use when fetching sampled data during unpacking.
	*/
	psTempRegSrc = &(psInst->asArg[0]);
	if (psTempRegSrc->uType == USEASM_REGTYPE_TEMP)
	{
		sSampleUnpackBlock.uBaseSampleUnpackSrcRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
	}
	else
	{
		ASSERT(psTempRegSrc->uType == USEASM_REGTYPE_PRIMATTR);
		sSampleUnpackBlock.uBaseSampleUnpackSrcRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_PA;
	}
	sSampleUnpackBlock.uBaseSampleUnpackSrcRegNum	= (IMG_UINT16)psTempRegSrc->uNumberPreMoe;

	/*
		Copy the information about the temporary registers available for the
		USP for temporary use when unpacking the texture sample.
	*/
	psTempReg = &(psUSPSampleUnpack->sTempReg);
	if (psTempReg->uType == USEASM_REGTYPE_TEMP)
	{
		sSampleUnpackBlock.uSampleUnpackTempRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_TEMP;
	}
	else
	{
		ASSERT(psTempReg->uType == USEASM_REGTYPE_PRIMATTR);
		sSampleUnpackBlock.uSampleUnpackTempRegType	= USP_PC_SAMPLE_TEMP_REGTYPE_PA;
	}
	sSampleUnpackBlock.uSampleUnpackTempRegNum	= (IMG_UINT16)psTempReg->uNumber;

	sSampleUnpackBlock.uIRegsLiveBefore = (IMG_UINT16)UseDefIterateIRegLiveness_Current(&psBPCSState->sIRegIterator);

	SetupPCInstDesc(psBPCSState, 
				    &sSampleUnpackBlock.sInstDesc, 
					psInst, 
					&psBPCSState->sIRegIterator);
	
	switch (psInst->asDest[0].uType)
	{
		case USEASM_REGTYPE_OUTPUT:
		{
			ePCSampleDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_OUTPUT;
			break;
		}
		case USEASM_REGTYPE_PRIMATTR:
		{
			ePCSampleDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_PA;
			break;
		}
		case USEASM_REGTYPE_FPINTERNAL:
		{
			ePCSampleDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_INTERNAL;
			break;
		}
		case USEASM_REGTYPE_TEMP:
		default:
		{
			ePCSampleDestRegType = USP_PC_SAMPLE_DEST_REGTYPE_TEMP;
		}
	}

	switch (psInst->asDest[0].eFmt)
	{
		case UF_REGFORMAT_F16:
		{
			ePCSampleDestRegFmt = USP_PC_SAMPLE_DEST_REGFMT_F16;
			uChansPerReg		= 2;
			uCompMask			= 0x3U;
			break;
		}
		case UF_REGFORMAT_C10:
		{
			ePCSampleDestRegFmt = USP_PC_SAMPLE_DEST_REGFMT_C10;
			uChansPerReg		= 4;
			uCompMask			= 0x1U;
			break;
		}
		case UF_REGFORMAT_U8:
		{
			ePCSampleDestRegFmt = USP_PC_SAMPLE_DEST_REGFMT_U8;
			uChansPerReg		= 4;
			uCompMask			= 0x1U;
			break;
		}
		default:
		case UF_REGFORMAT_F32:
		{
			ePCSampleDestRegFmt = USP_PC_SAMPLE_DEST_REGFMT_F32;
			uChansPerReg		= 1;
			uCompMask			= 0xFU;
			break;
		}
	}

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		IMG_UINT32	uRegNum;
		IMG_UINT32	uRegComp;
		IMG_UINT32	uCompOff;
		IMG_UINT32	uRegOff;

		uCompOff = (i * CHANS_PER_REGISTER) / uChansPerReg;
		uRegOff	 = uCompOff / CHANS_PER_REGISTER;
		uRegNum	 = psInst->asDest[0].uNumberPreMoe + uRegOff;
		uRegComp = uCompOff % CHANS_PER_REGISTER;

		sSampleUnpackBlock.auDestRegType[i] = (IMG_UINT16)ePCSampleDestRegType;
		sSampleUnpackBlock.auDestRegFmt[i]  = (IMG_UINT16)ePCSampleDestRegFmt;
		sSampleUnpackBlock.auDestRegNum[i]  = (IMG_UINT16)uRegNum;
		sSampleUnpackBlock.auDestRegComp[i] = (IMG_UINT16)uRegComp;

		if	((uRegOff < psInst->uDestCount) && psInst->auLiveChansInDest[uRegOff] & (uCompMask << uRegComp))
		{
			sSampleUnpackBlock.uLive |= 1U << i;
		}
	}

	/*
		Setup the MOE state before the sample
	*/
	psPCMOEState = &sSampleUnpackBlock.sMOEState;

	psPCMOEState->uMOEFmtCtlFlags = 0;
	if	(psBPCSState->bEfoFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_EFOFMTCTL;
	}
	if	(psBPCSState->bColFmtCtl)
	{
		psPCMOEState->uMOEFmtCtlFlags |= USP_PC_MOE_FMTCTLFLAG_COLFMTCTL;
	}

	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		IMG_UINT32	uIncOrSwiz;
		PMOE_DATA	psMoeIncSwiz;

		uIncOrSwiz = ((1U << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
					 USP_PC_MOE_INCORSWIZ_MODE_INC;

		psMoeIncSwiz = &psBPCSState->asMoeIncSwiz[i];
		switch (psMoeIncSwiz->eOperandMode)
		{
			case MOE_INCREMENT:
			{
				uIncOrSwiz = ((*((IMG_PUINT32)(&psMoeIncSwiz->u.iIncrement)) << USP_PC_MOE_INCORSWIZ_INC_SHIFT) & 
							  (~USP_PC_MOE_INCORSWIZ_INC_CLRMSK)) |
							 USP_PC_MOE_INCORSWIZ_MODE_INC;

				break;
			}

			case MOE_SWIZZLE:
			{
				uIncOrSwiz = ConvertMOESwizzleToHwFormat(psMoeIncSwiz);
				break;
			}
		}

		psPCMOEState->auMOEIncOrSwiz[i]	 = (IMG_UINT16)uIncOrSwiz;
		psPCMOEState->auMOEBaseOffset[i] = (IMG_UINT16)psBPCSState->auMoeBaseOffset[i];
	}

	/*
		Append the common sample unpack data to the PC shader
	*/

	psBPCSState->pfnWrite2(&psBPCSState->pvData, psPCMOEState->uMOEFmtCtlFlags);
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEIncOrSwiz[i]);
	}
	for	(i = 0; i < USP_PC_MOE_OPERAND_COUNT; i++)
	{
		psBPCSState->pfnWrite2(&psBPCSState->pvData, 
							   psPCMOEState->auMOEBaseOffset[i]);
	}

	psBPCSState->pfnWrite4(&psBPCSState->pvData, sSampleUnpackBlock.uSmpID);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.sInstDesc.uFlags);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uMask);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uLive);

	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if((sSampleUnpackBlock.uMask & (1U << i)) == 0)
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, 0);
		}
		else
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.auDestRegType[i]);
		}
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if((sSampleUnpackBlock.uMask & (1U << i)) == 0)
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, 0);
		}
		else
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.auDestRegNum[i]);
		}
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if((sSampleUnpackBlock.uMask & (1U << i)) == 0)
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, 0);
		}
		else
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.auDestRegFmt[i]);
		}
	}
	for	(i = 0; i < USP_MAX_TEX_CHANNELS; i++)
	{
		/* Ignore missing components */
		if((sSampleUnpackBlock.uMask & (1U << i)) == 0)
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, 0);
		}
		else
		{
			psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.auDestRegComp[i]);
		}
	}

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uDirectSrcRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uDirectSrcRegNum);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uBaseSampleUnpackSrcRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uBaseSampleUnpackSrcRegNum);

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uSampleUnpackTempRegType);
	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uSampleUnpackTempRegNum);	

	psBPCSState->pfnWrite2(&psBPCSState->pvData, sSampleUnpackBlock.uIRegsLiveBefore);
}

/*****************************************************************************
 FUNCTION	: UpdateMOEState

 PURPOSE	: Update the current MOE state to reflect changes made by a
			  MOE instruction

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  psInst		- A MOE instruction

 RETURNS	: none
*****************************************************************************/
static IMG_VOID UpdateMOEState(PBUILD_PC_SHADER_STATE	psBPCSState,
							   PINST					psInst)
{
	IMG_UINT32	i;

	switch (psInst->eOpcode)
	{
		case ISMLSI:
		{
			/*
				Record the new swizzle/increments required
			*/

			for (i = 0; i < 4; i++)
			{
				PMOE_DATA	psMoeIncSwiz;
				IMG_UINT32	bUseSwiz;

				psMoeIncSwiz = &psBPCSState->asMoeIncSwiz[i];

				bUseSwiz = psInst->asArg[i + 4].uNumber;
				if	(!bUseSwiz)
				{
					psMoeIncSwiz->eOperandMode	= MOE_INCREMENT;
					psMoeIncSwiz->u.iIncrement	= psInst->asArg[i].uNumber;
				}
				else
				{
					IMG_UINT32	j;
					IMG_UINT32	uUseasmSwizzle;

					psMoeIncSwiz->eOperandMode	= MOE_SWIZZLE;

					/*
						Convert the swizzle value from the USEASM input format to the
						SMLSI format.
					*/
					uUseasmSwizzle = psInst->asArg[i].uNumber;
					for (j = 0; j < 4; j++)
					{
						IMG_UINT32	uChan;

						uChan = (uUseasmSwizzle >> (USEASM_SWIZZLE_FIELD_SIZE * j)) & USEASM_SWIZZLE_VALUE_MASK;
						psMoeIncSwiz->u.s.auSwizzle[j] = uChan; 
					}
				}
			}

			break;
		}

		case ISMBO:
		{
			/*
				Record the new MOE base-offsets required
			*/
			for (i = 0; i < 4; i++)
			{
				psBPCSState->auMoeBaseOffset[i] = psInst->asArg[i].uNumber;
			}

			break;
		}

		case ISETFC:
		{
			/*
				Record the new format control options required
			*/
			psBPCSState->bEfoFmtCtl = psInst->asArg[0].uNumber == 1;
			psBPCSState->bColFmtCtl = psInst->asArg[1].uNumber == 1;

			break;
		}

		default:
		{
			break;
		}
	}
}

/*****************************************************************************
 FUNCTION	: BuildPCDataForBasicBlock

 PURPOSE	: Add a label-block to a pre-compiled shader

 PARAMETERS	: psBPCSState	- Data used during PC shader construction
			  psBlock		- The basic block to generate PC data for

 RETURNS	: none
*****************************************************************************/
static IMG_PUINT32 BuildPCDataForBasicBlockCB(
												PINTERMEDIATE_STATE	psState, 
												PCODEBLOCK			psBlock, 
												IMG_PUINT32			puOffset, 
												PLAYOUT_INFO		psLayout)
{
	PBUILD_PC_SHADER_STATE 
				psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
	PINST		psFirstPCInst;
	IMG_UINT16	uPCInstCount;
	PINST		psInst;
	IMG_UINT32	i;
	
	PVR_UNREFERENCED_PARAMETER(puOffset);
	PVR_UNREFERENCED_PARAMETER(psLayout);
	/*
		Initialize state for iterating the ranges of instructions over which the internal
		registers are live.
	*/
	UseDefIterateIRegLiveness_Initialize(psState, psBlock, &psBPCSState->sIRegIterator);

	/*
		Update the current MOE state based upon the precalculated MOE state at
		the beginning of this basic-block
	*/
	psBPCSState->bEfoFmtCtl	= psBlock->bEfoFmtCtl;
	psBPCSState->bColFmtCtl	= psBlock->bColFmtCtl;

	for	(i = 0; i < USC_MAX_MOE_OPERANDS; i++)
	{
		psBPCSState->asMoeIncSwiz[i]	= psBlock->asMoeIncSwiz[i];
		psBPCSState->auMoeBaseOffset[i]	= psBlock->auMoeBaseOffset[i];
	}

	/*
		Traverse the instructions of the block, adding blocks of pre-compiled
		instructions and special instrucitons for the USP as appropriate.
	*/
	psFirstPCInst	= IMG_NULL;
	uPCInstCount	= 0;

	for (psInst = psBlock->psBody;
		 psInst != NULL;
		 psInst = psInst->psNext)
	{
		/*
			Look for the end of the alpha-calculation code...
		*/
		if	(psInst == psBPCSState->psState->psMainProgFeedbackPhase0EndInst)
		{
			IMG_UINT32	uPTPhase0EndLabelID;

			/*
				Add the preceeding block of pre-compiled instructions
			*/
			if	(uPCInstCount > 0)
			{
				BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
				uPCInstCount = 0;
			}

			/*
				Add a label to mark the end of the alpha-calc code
			*/
			uPTPhase0EndLabelID = psState->uNextLabel++;
			BuildPCLabelCB(psState, IMG_NULL, uPTPhase0EndLabelID, IMG_FALSE);
			psBPCSState->uPTPhase0EndLabelID = uPTPhase0EndLabelID;
		}

		switch (psInst->eOpcode)
		{
			case ISMP_USP:
			case ISMPBIAS_USP:
			case ISMPREPLACE_USP:
			case ISMPGRAD_USP:
			case ISMP_USP_NDR:
			{
				/*
					Add the preceeding block of pre-compiled instructions
				*/
				if	(uPCInstCount > 0)
				{
					BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
					uPCInstCount = 0;
				}

				/*
					Add a dependent sample block
				*/
				BuildSampleBlock(psBPCSState, psInst);


				break;
			}
			
			case ISMPUNPACK_USP:
			{	
				/*
					Add the preceeding block of pre-compiled instructions
				*/
				if	(uPCInstCount > 0)
				{
					BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
					uPCInstCount = 0;
				}

				/*
					Add a dependent sample block
				*/
				BuildSampleUnpackBlock(psBPCSState, psInst);

				break;
			}

			case ITEXWRITE:
			{
				/*
				  Add the preceeding block of pre-compiled instructions
				*/
				if	(uPCInstCount > 0)
				{
					BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
					uPCInstCount = 0;
				}

				/*
				  Add a function invocation
				 */
				BuildTextureWriteBlock(psBPCSState, psInst);
				
				break;
			}

			case ISMLSI:
			case ISETFC:
			case ISMBO:
			{
				/*
					Add the preceeding block of pre-compiled instructions
				*/
				if	(uPCInstCount > 0)
				{
					BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
					uPCInstCount = 0;
				}

				/*
					Update the current MOE state to reflect changes made by this
					instruction
				*/
				UpdateMOEState(psBPCSState, psInst);

				/*
					Deliberately fall-through to the default case
				*/
			}
			default:
			{
				/*
					Record the first instruction for the next pre-compiled
					code block
				*/
				if	(uPCInstCount == 0)
				{
					psFirstPCInst = psInst;
				}

				uPCInstCount++;
				break;
			}
		}
	}

	/*
		Add the final block of pre-compiled instructions
	*/
	if	(uPCInstCount > 0)
	{
		BuildHWCodeBlock(psBPCSState, uPCInstCount, psFirstPCInst);
	}
	return 0;
}

static IMG_BOOL PointActionsUspBinCB(PINTERMEDIATE_STATE psState, struct _LAYOUT_INFO_ *psLayout, LAYOUT_POINT eLayoutPoint)
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psLayout);
	PVR_UNREFERENCED_PARAMETER(eLayoutPoint);
	switch(eLayoutPoint)
	{
	case LAYOUT_MAIN_PROG_START:		
		{
			PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
			psBPCSState->uProgStartLabelID = psState->uNextLabel++;
			BuildPCLabelCB(psState, psLayout, psBPCSState->uProgStartLabelID, IMG_FALSE);
		}
		break;
	case LAYOUT_POST_FEEDBACK_START:		
		{
			PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
			IMG_UINT32	uPTPhase1StartLabelID;
			uPTPhase1StartLabelID = psState->uNextLabel++;
			BuildPCLabelCB(psState, psLayout, uPTPhase1StartLabelID, IMG_FALSE);
			psBPCSState->uPTPhase1StartLabelID = uPTPhase1StartLabelID;
		}
		break;
	case LAYOUT_POST_SPLIT_START:		
		{
			PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
			IMG_UINT32	uPTSplitPhase1StartLabelID;
			uPTSplitPhase1StartLabelID = psState->uNextLabel++;
			BuildPCLabelCB(psState, psLayout, uPTSplitPhase1StartLabelID, IMG_FALSE);
			psBPCSState->uPTSplitPhase1StartLabelID = uPTSplitPhase1StartLabelID;
		}
		break;
	case LAYOUT_MAIN_PROG_END:
		{
			PBUILD_PC_SHADER_STATE 
						psBPCSState = (PBUILD_PC_SHADER_STATE)(psState->pvBPCSState);
			if	(psState->psMainProgFeedbackPhase0EndInst && !psState->psPreFeedbackBlock)
			{
				IMG_UINT32	uPTPhase1StartLabelID;

				uPTPhase1StartLabelID = psState->uNextLabel++;
				BuildPCLabelCB(psState, psLayout, uPTPhase1StartLabelID, IMG_FALSE);

				psBPCSState->uPTPhase1StartLabelID = uPTPhase1StartLabelID;
			}
		}

	default:
		break;
	}
	return IMG_TRUE;
}

static IMG_VOID AlignToEvenUspBinCB(PINTERMEDIATE_STATE psState, PLAYOUT_INFO psLayout)
{
	PVR_UNREFERENCED_PARAMETER(psState);
	PVR_UNREFERENCED_PARAMETER(psLayout);
	return;
}

/*****************************************************************************
 FUNCTION	: BuildPCShader
    
 PURPOSE	: Generate the pre-compiled shader data

 PARAMETERS	: psPCShaderState	- State used during PC shader construction
			  
 RETURNS	: void
*****************************************************************************/
static IMG_VOID BuildPCShader(PBUILD_PC_SHADER_STATE psBPCSState)
{
	PINTERMEDIATE_STATE	psState;
	IMG_UINT32			uPCDataSize;
	/*
		Write the shader header data
	*/
	uPCDataSize = psBPCSState->uShaderSize - sizeof(USP_PC_SHADER);

	psBPCSState->pfnWrite4(&psBPCSState->pvData, USP_PC_SHADER_ID);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, USP_PC_SHADER_VER);
	psBPCSState->pfnWrite4(&psBPCSState->pvData, uPCDataSize);

	/*
		Build the program-descriptor block
	*/
	BuildPCProgDesc(psBPCSState);

	/*
		In-case the main routine of the program has an early exits, grab a
		label for them.
	*/
	psState = psBPCSState->psState;

	/*
		Build the blocks for each subroutine
	*/
	psState->pvBPCSState = (IMG_PVOID)psBPCSState;
	LayoutProgram(psState, BuildPCDataForBasicBlockCB, BuildPCBranchCB, BuildPCLabelCB, PointActionsUspBinCB, AlignToEvenUspBinCB, LAYOUT_MAIN_PROGRAM);
	/*
		Mark the end of the shader with an END block header
	*/
	BuildPCBlockHdr(psBPCSState, USP_PC_BLOCK_TYPE_END);
}

/*****************************************************************************
 FUNCTION	: CreateUspBinOutput

 PURPOSE	: Generate USP-compatible binary output from the intermediate code

 PARAMETERS	: psState		- Compiler state.
			  ppsPCShader	- Where to return the created pre-compiled
							  shader data

 RETURNS	: The created pre-compiled shader
*****************************************************************************/
IMG_INTERNAL 
IMG_UINT32 CreateUspBinOutput(PINTERMEDIATE_STATE	psState,
							  PUSP_PC_SHADER*		ppsPCShader)
{
	BUILD_PC_SHADER_STATE	sBPCSState;
	IMG_PVOID				pvPCShader;
	IMG_UINT32				uShaderSize;
	IMG_UINT32				uTempResultRegs = 0;
	IMG_UINT32				uPAResultRegs = 0;
	IMG_UINT32				uOutputResultRegs = 0;
	IMG_BOOL				bNoRemapping = IMG_FALSE;
	USEASM_CONTEXT			sUseasmContext;

	pvPCShader	= IMG_NULL;
	*ppsPCShader = (PUSP_PC_SHADER)pvPCShader;

	/*
		Determine which alternate registers in other 
		banks we can use to put results in.
	*/
	if	(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		GetShaderAltResultRegs(
								psState, 
								&uTempResultRegs, 
								&uPAResultRegs, 
								&uOutputResultRegs, 
								&bNoRemapping);
	}
	else
	{
		uTempResultRegs		= 0;
		uPAResultRegs		= 0;
		uOutputResultRegs	= 0;
		bNoRemapping		= IMG_FALSE;
	}

	/*
		Set up the context for the assembler.
	*/
	psState->psUseasmContext = &sUseasmContext;
	psState->psUseasmContext->pvContext				= (IMG_PVOID)psState;
	psState->psUseasmContext->pfnRealloc			= IMG_NULL;
	psState->psUseasmContext->pfnGetLabelAddress	= UseAssemblerGetLabelAddress;
	psState->psUseasmContext->pfnSetLabelAddress	= IMG_NULL;
	psState->psUseasmContext->pfnGetLabelName		= IMG_NULL;
	psState->psUseasmContext->pfnAssemblerError		= UseAssemblerError;
	psState->psUseasmContext->pvLabelState			= IMG_NULL;

	/*
		Calculate the overall size of the PC shader we will generate
	*/
	sBPCSState.psState					= psState;
	sBPCSState.pfnWrite4				= (PFN_BUILD_PC_SHADER_4)PCShaderSkip4;
	sBPCSState.pfnWrite2				= (PFN_BUILD_PC_SHADER_2)PCShaderSkip2;
	sBPCSState.pfnWrite1				= (PFN_BUILD_PC_SHADER_1)PCShaderSkip1;
	sBPCSState.pfnWriteN				= (PFN_BUILD_PC_SHADER_N)PCShaderSkipN;
	sBPCSState.pvData					= 0;
	sBPCSState.uShaderSize				= 0;
	sBPCSState.uProgStartLabelID		= UINT_MAX;
	sBPCSState.uPTPhase0EndLabelID		= UINT_MAX;
	sBPCSState.uPTPhase1StartLabelID	= UINT_MAX;
	sBPCSState.uPTSplitPhase1StartLabelID 
										= UINT_MAX;
	sBPCSState.bProgEndIsLabel			= IMG_FALSE;
	sBPCSState.uTempResultRegs			= uTempResultRegs;
	sBPCSState.uPAResultRegs			= uPAResultRegs;
	sBPCSState.uOutputResultRegs		= uOutputResultRegs;
	sBPCSState.bNoResultRemapping		= bNoRemapping;

	BuildPCShader(&sBPCSState);

	/*
		Allocate space for the pre-compiled shader 
	*/
	uShaderSize	= (IMG_UINT32)sBPCSState.pvData;

	pvPCShader = psState->pfnAlloc(uShaderSize);
	if	(!pvPCShader)
	{
		USC_ERROR(UF_ERR_NO_MEMORY, "Failed to alloc space for USP PC-shader");
	}
	*ppsPCShader = (PUSP_PC_SHADER)pvPCShader;

	/*
		Generate the pre-compiled shader
	*/
	sBPCSState.psState		= psState;
	sBPCSState.pfnWrite4	= PCShaderWrite4;
	sBPCSState.pfnWrite2	= PCShaderWrite2;
	sBPCSState.pfnWrite1	= PCShaderWrite1;
	sBPCSState.pfnWriteN	= PCShaderWriteN;
	sBPCSState.pvData		= pvPCShader;
	sBPCSState.uShaderSize	= uShaderSize;

	BuildPCShader(&sBPCSState);

	/*
		Check that the generated shader is the expected size
	*/
	ASSERT((IMG_UINT32)((IMG_PUINT8)sBPCSState.pvData - (IMG_PUINT8)pvPCShader) == uShaderSize);

	/*
		No longer handling exceptions
	*/ 
	psState->bExceptionReturnValid = IMG_FALSE;

	/*
		USP PC-shader created OK!
	*/
	psState->psUseasmContext = NULL;

	return UF_OK;
}

/*****************************************************************************
 FUNCTION	: DestroyUspBinOutput

 PURPOSE	: Destroy previously created USP-compatible binary output

 PARAMETERS	: psState		- Compiler state.
			  psPCShader	- The pre-compiled shader data to destroy

 RETURNS	: none
*****************************************************************************/
IMG_INTERNAL 
IMG_VOID DestroyUspBinOutput(PINTERMEDIATE_STATE	psState,
							 PUSP_PC_SHADER			psPCShader)
{
	/*
		PC shader data is allocated as a single block of contigous data.
	*/
	psState->pfnFree(psPCShader);
}

/*****************************************************************************
 FUNCTION	: IsInstSrcShaderResultRef

 PURPOSE	: Tells whether a specific source operand in instruction is a 
			  Shader Result Reference.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Input intermediate instruction.
			  uArgIdx			- Index of the source operand to test.

 RETURNS	: IMG_TRUE - If Source Operand is a Shader Result Reference.
			  IMG_FALSE - Otherwise.
*****************************************************************************/
IMG_INTERNAL 
IMG_BOOL IsInstSrcShaderResultRef(	PINTERMEDIATE_STATE	psState,
									PINST				psInst, 
									IMG_UINT32			uArgIdx)
{	
	IMG_UINT32	uHWOperandsUsed;

	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		return IMG_FALSE;
	}

	uHWOperandsUsed = GetHWOperandsUsedForArg(psInst, uArgIdx + 1);
	if(psInst->uShaderResultHWOperands & uHWOperandsUsed)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsInstDestShaderResult

 PURPOSE	: Tells whether destination operand of instruction is a 
			  Shader Result.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Input intermediate instruction.			  

 RETURNS	: IMG_TRUE - If Destination Operand is a Shader Result.
			  IMG_FALSE - Otherwise.
*****************************************************************************/
IMG_INTERNAL 
IMG_BOOL IsInstDestShaderResult(PINTERMEDIATE_STATE	psState, PINST	psInst)
{	
	IMG_UINT32	uHWOperandsUsed;

	if ((psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) == 0)
	{
		return IMG_FALSE;
	}

	uHWOperandsUsed = GetHWOperandsUsedForArg(psInst, 0);
	if(psInst->uShaderResultHWOperands & uHWOperandsUsed)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID MarkShaderResultWrite(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: MarkShaderResultWrite

 PURPOSE	: Mark an intermediate instruction as defining the shader result.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Intermediate instruction.			  

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uHWOperandsUsed;
	IMG_BOOL	bInPhase1;

	/*
		This arg references a result register, record which HW operands it
		maps to, so that the USP will know which ones to modify.
	*/
	uHWOperandsUsed = GetHWOperandsUsedForArg(psInst, 0);
	psInst->uShaderResultHWOperands |= uHWOperandsUsed;

	/*
		If there is any chance that this instruction could be executed in phase 0
		then mark that the shader result is written in phase 0.
	*/
	ASSERT(psInst->psBlock != NULL);
	bInPhase1 = IMG_FALSE;
	if (psState->psPreFeedbackDriverEpilogBlock != NULL)
	{
		PCODEBLOCK psPostFeedbackBlock;

		ASSERT(psState->psPreFeedbackDriverEpilogBlock->uNumSuccs == 1);
		psPostFeedbackBlock = psState->psPreFeedbackDriverEpilogBlock->asSuccs[0].psDest;

		if (Dominates(psState, psPostFeedbackBlock, psInst->psBlock))
		{
			bInPhase1 = IMG_TRUE;
		}
	}
	if (!bInPhase1)
	{
		psState->bResultWrittenInPhase0 = IMG_TRUE;
	}
}

static IMG_VOID CheckResultDest(PINTERMEDIATE_STATE psState, 
								PINST				psInst, 
								IMG_UINT32			uDestIdx, 
								PFIXED_REG_DATA		psResultFixedReg)
/*****************************************************************************
 FUNCTION	: CheckResultDest

 PURPOSE	: Check for an instruction which defines the shader result that
			  it doesn't also write unrelated registers.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Intermediate instruction.	
			  uDestIdx			- Index of the destination which is a shader
								result.
			  psResultFixedReg	- Shader result.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOtherDestIdx;

	for (uOtherDestIdx = 0; uOtherDestIdx < psInst->uDestCount; uOtherDestIdx++)
	{
		PARG	psOtherDest = &psInst->asDest[uOtherDestIdx];

		if (uOtherDestIdx != uDestIdx && psOtherDest->uType == USEASM_REGTYPE_TEMP)
		{
			PUSEDEF_CHAIN	psOtherDestUseDef;

			psOtherDestUseDef = UseDefGet(psState, psOtherDest->uType, psOtherDest->uNumber);
			if (
					!(
						psOtherDestUseDef->psDef != NULL &&
						psOtherDestUseDef->psDef->eType == DEF_TYPE_FIXEDREG &&
						psOtherDestUseDef->psDef->u.psFixedReg == psResultFixedReg
					 )
				)
			{
				/* 
					Some but not all destinations of an instruction are Shader Results, 
					so result references in this Shader are not patchable.
				*/
				psState->uFlags |= USC_FLAGS_RESULT_REFS_NOT_PATCHABLE;
				return;
			}
		}
	}
}

static IMG_VOID InsertAltOutPutsAndRecordResultRefs(PINTERMEDIATE_STATE psState, 
													PFIXED_REG_DATA		psResultFixedReg)
/*****************************************************************************
 FUNCTION	: InsertAltOutPutsAndRecordResultRefs

 PURPOSE	: Mark all the instructions which define/use the shader result.

 PARAMETERS	: psState			- Compiler state.
			  psResultFixedReg	- Shader result.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uRegIdx;

	for (uRegIdx = 0; uRegIdx < psResultFixedReg->uConsecutiveRegsCount; uRegIdx++)
	{
		PUSEDEF_CHAIN	psUseDefChain;
		PUSC_LIST_ENTRY	psListEntry;

		psUseDefChain = UseDefGet(psState, psResultFixedReg->uVRegType, psResultFixedReg->auVRegNum[uRegIdx]);

		for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PUSEDEF	psUseDef;

			psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psUseDef->eType == DEF_TYPE_INST)
			{
				PINST		psInst = psUseDef->u.psInst;
				IMG_UINT32	uDestIdx = psUseDef->uLocation;

				ASSERT(uDestIdx < psInst->uDestCount);
				if (psInst->auDestMask[uDestIdx] != 0)
				{
					MarkShaderResultWrite(psState, psInst);
				}

				if (psInst->uDestCount > 1)
				{
					CheckResultDest(psState, psInst, uDestIdx, psResultFixedReg);
				}
			}
			else if (psUseDef->eType == USE_TYPE_SRC)
			{
				IMG_UINT32	uCompsRead;
				PINST		psInst = psUseDef->u.psInst;
				IMG_UINT32	uArgIdx = psUseDef->uLocation;

				/*
					Calculate the reg-components read for this argument
				*/
				uCompsRead = GetLiveChansInArg(psState, psInst, uArgIdx);

				if (uCompsRead != 0)
				{
					IMG_UINT32	uHWOperandsUsed;

					/*
						This arg references a result register, record which HW operands it
						maps to, so that the USP will know which ones to modify.
					*/
					uHWOperandsUsed = GetHWOperandsUsedForArg(psInst, uArgIdx + 1);
					psInst->uShaderResultHWOperands |= uHWOperandsUsed;
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID InsertAlternateResults(PINTERMEDIATE_STATE psState)
{
	IMG_UINT32 uResultRegCount;
	IMG_UINT32 uAltResultBanksCount;
	IMG_UINT32 uAltBankTypes[MAX_ALTERNATE_RESULT_BANK_TYPES];
	IMG_UINT32 uAltBankIdx;
	PPIXELSHADER_STATE psPS;
	PFIXED_REG_DATA psColOut;

	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}
	psPS = psState->sShader.psPS;

	psPS->uAltPAFixedReg = psPS->uAltTempFixedReg = USC_UNDEF;

	if (psPS->psColFixedReg == NULL)
	{
		return;
	}
	psColOut = psPS->psColFixedReg;

	uResultRegCount = psColOut->uConsecutiveRegsCount;

	if(!IsListEmpty(&psState->sFixedRegList) && (psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT) == 0)
	{
		{
			if(psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_OUTPUT)
			{
				/* 
					In case of Result in OutPut register we might need to put 
					Result in either PA or Temp register later on.
				*/
				if ((psState->uCompilerFlags & UF_MSAATRANS) == 0)
				{
					uAltResultBanksCount = 2;
					uAltBankTypes[0] = USEASM_REGTYPE_TEMP;
					uAltBankTypes[1] = USEASM_REGTYPE_PRIMATTR;
				}
				else
				{
					uAltResultBanksCount = 1;
					uAltBankTypes[0] = USEASM_REGTYPE_TEMP;					
				}
			}
			else
			{
				if ((psState->uCompilerFlags & UF_MSAATRANS) == 0)
				{
					uAltResultBanksCount = 1;
					if(psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
					{	
						uAltBankTypes[0] = USEASM_REGTYPE_PRIMATTR;
					}
					else
					{
						uAltBankTypes[0] = USEASM_REGTYPE_TEMP;
					}
				}
				else
				{					
					if(psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
					{
						uResultRegCount = 0;
						uAltResultBanksCount = 0;						
					}
					else
					{
						uAltResultBanksCount = 1;
						uAltBankTypes[0] = USEASM_REGTYPE_TEMP;
					}
				}
			}
		}
	}
	else
	{
		uAltResultBanksCount = 0;
	}

	/*
		Set the alternate shader outputs as alternate hardware registers for the
		shader colour output.
	*/
	psColOut->uNumAltPRegs = uAltResultBanksCount;
	psColOut->asAltPRegs = UscAlloc(psState, sizeof(psColOut->asAltPRegs[0]) * uAltResultBanksCount);
	for (uAltBankIdx = 0; uAltBankIdx < uAltResultBanksCount; uAltBankIdx++)
	{
		PARG	psPReg = &psColOut->asAltPRegs[uAltBankIdx];

		InitInstArg(psPReg);
		if (uAltBankTypes[uAltBankIdx] == USEASM_REGTYPE_TEMP)
		{
			psPReg->uType = USEASM_REGTYPE_TEMP;		
			psPReg->uNumber = 0;

			psPS->uAltTempFixedReg = 1 + uAltBankIdx;
		}
		else
		{
			psPReg->uType = USEASM_REGTYPE_PRIMATTR;
			psPReg->uNumber = USC_UNDEF;

			psPS->uAltPAFixedReg = 1 + uAltBankIdx;
		}
	}

	if (uAltResultBanksCount > 0)
	{
		psState->bResultWrittenInPhase0 = IMG_FALSE;
		InsertAltOutPutsAndRecordResultRefs(psState, psColOut);
	}
}

/*****************************************************************************
 FUNCTION	: IsInstReferingShaderResult

 PURPOSE	: Gets information that whether the instruction is Referencing 
			Shader Result registers or not.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Input intermediate instruction.
 RETURNS	: IMG_TRUE if instruction is referencing Shader Result, 
			  IMG_FALSE otherwise.
*****************************************************************************/
IMG_INTERNAL 
IMG_BOOL IsInstReferingShaderResult(PINTERMEDIATE_STATE	psState, PINST psInst)
{	
	if(psState->uFlags & USC_FLAGS_RESULT_REFS_NOT_PATCHABLE)
	{
		/* 
			We are not going to patch Result References, so it 
			is safe to assume that all instructions are not 
			referencing shader result.
		*/
		return IMG_FALSE;
	}
	if ((psInst->uShaderResultHWOperands & (1U << 0)) != 0 || 
		(psInst->uShaderResultHWOperands & (1U << 1)) != 0 || 
		(psInst->uShaderResultHWOperands & (1U << 2)) != 0 || 
		(psInst->uShaderResultHWOperands & (1U << 3)) != 0)
	{
		/* 
			Instruction is referencing a shader result so it can not be 
			grouped with other instructions.
		*/
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

/******************************************************************************
 End of file (uspbin.c)
******************************************************************************/
