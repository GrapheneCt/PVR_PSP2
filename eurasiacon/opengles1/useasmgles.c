/******************************************************************************
 * Name         : useasmgles.c
 *
 * Copyright    : 2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : USEASM interfacing functions
 *
 * Platform     : ANSI
 *
 * $Log: useasmgles.c $
 *
 *****************************************************************************/

#include <stdarg.h>
#include "context.h"
#include "useopt.h"

static IMG_VOID IMG_CALLCONV UseAssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_CHAR *pszFmt, ...) IMG_FORMAT_PRINTF(3, 4);


/***********************************************************************************
 Function Name      : AddInstruction
 Inputs             : psUSEASMInfo, uOpcode, uFlags1, uFlags2, uTest, psArgs, 
 					  ui32NumArgs
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Sets up USEASM instruction structure and adds it to the list
************************************************************************************/
IMG_INTERNAL IMG_VOID AddInstruction(GLES1Context   *gc,
									 GLESUSEASMInfo *psUSEASMInfo,
									 USEASM_OPCODE  uOpcode,
									 IMG_UINT32     uFlags1,
									 IMG_UINT32	    uFlags2,
									 IMG_UINT32     uTest,
									 USE_REGISTER	*psArgs,
									 IMG_UINT32     ui32NumArgs)
{
	USE_INST *psNewInstruction;
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(gc);

	psNewInstruction = GLES1Calloc(gc, sizeof(USE_INST));

	if(!psNewInstruction)
	{
		PVR_DPF((PVR_DBG_ERROR,"AddInstruction(): Failed to allocate memory for new instruction"));

		return;
	}

	psNewInstruction->uOpcode = uOpcode;

	switch(uOpcode)
	{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	    case USEASM_OP_PHASIMM:
#endif
		case USEASM_OP_PADDING:
		case USEASM_OP_SMLSI:
		case USEASM_OP_PCOEFF:
		case USEASM_OP_ATST8:
		case USEASM_OP_DEPTHF:
		{
			psNewInstruction->uFlags1 = uFlags1;

			break;
		}
		case USEASM_OP_SOP2WM:
		case USEASM_OP_LRP1:
		{
			psNewInstruction->uFlags1 = uFlags1 | USEASM_OPFLAGS1_SKIPINVALID | (0x1<<USEASM_OPFLAGS1_MASK_SHIFT);

			break;
		}
		default:
		{
			if(uFlags1 & USEASM_OPFLAGS1_TESTENABLE)
			{
				/* Set skip invalid by default */
				psNewInstruction->uFlags1 = uFlags1 | USEASM_OPFLAGS1_SKIPINVALID;
			}
			else
			{
				/* Set skip invalid by default and repeat of 1 */
				psNewInstruction->uFlags1 = uFlags1 | USEASM_OPFLAGS1_SKIPINVALID | (1<<USEASM_OPFLAGS1_REPEAT_SHIFT);
			}

			break;
		}
	}

	psNewInstruction->uFlags2 = uFlags2;

/*	IMG_UINT32			uFlags3; */

	psNewInstruction->uTest = uTest;

	for(i=0; i<ui32NumArgs; i++)
	{
		psNewInstruction->asArg[i] = psArgs[i];
	}

	if(psUSEASMInfo->psLastUSEASMInstruction)
	{
		/* Add new instruction to the end of the list */
		psUSEASMInfo->psLastUSEASMInstruction->psNext = psNewInstruction;

		psNewInstruction->psPrev = psUSEASMInfo->psLastUSEASMInstruction;

		psNewInstruction->psNext = IMG_NULL;
	}
	else
	{
		/* The new instruction is the first instruction of the list */
		psUSEASMInfo->psFirstUSEASMInstruction = psNewInstruction;

		psNewInstruction->psPrev = IMG_NULL;

		psNewInstruction->psNext = IMG_NULL;
	}

/*	IMG_UINT32			uSourceLine; */
/*	IMG_PCHAR			pszSourceFile;*/

	/* The new instruction is the last instruction of the list */
	psUSEASMInfo->psLastUSEASMInstruction = psNewInstruction;


	if((uFlags2 & USEASM_OPFLAGS2_COISSUE) == 0)
	{
		psUSEASMInfo->ui32NumMainUSEASMInstructions++;
	}
}	


/***********************************************************************************
 Function Name      : FreeUSEASMInstructionList
 Inputs             : gc, psUSEASMInfo
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Frees the entire USEASM instruction list and any HW code
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeUSEASMInstructionList(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo)
{
	USE_INST *psInstruction, *psNext;

	PVR_UNREFERENCED_PARAMETER(gc);

	psInstruction = psUSEASMInfo->psFirstUSEASMInstruction;

	while(psInstruction)
	{
		psNext = psInstruction->psNext;

		GLES1Free(IMG_NULL, psInstruction);

		psInstruction = psNext;
	}

	psUSEASMInfo->psFirstUSEASMInstruction      = IMG_NULL;
	psUSEASMInfo->psLastUSEASMInstruction       = IMG_NULL;
	psUSEASMInfo->ui32NumMainUSEASMInstructions = 0;

	/* Free generated HW code */
	if(psUSEASMInfo->pui32HWInstructions)
	{
		GLES1Free(IMG_NULL, psUSEASMInfo->pui32HWInstructions);
	}

	psUSEASMInfo->pui32HWInstructions   = IMG_NULL;
	psUSEASMInfo->ui32NumHWInstructions = 0;
}	


/***********************************************************************************
 Function Name      : DuplicateUSEASMInstructionList
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DuplicateUSEASMInstructionList(GLES1Context *gc, GLESUSEASMInfo *psSrcUSEASMInfo, GLESUSEASMInfo *psDstUSEASMInfo)
{
	USE_INST *psInputInstruction, *psOutputInstruction, *psPrev;

	PVR_UNREFERENCED_PARAMETER(gc);

	psInputInstruction = psSrcUSEASMInfo->psFirstUSEASMInstruction;

	psDstUSEASMInfo->psFirstUSEASMInstruction = IMG_NULL;

	psPrev = IMG_NULL;

	/* Fix build warning */
	psOutputInstruction = IMG_NULL;

	while(psInputInstruction)
	{
		psOutputInstruction = GLES1Malloc(gc, sizeof(USE_INST));

		if(!psOutputInstruction)
		{
			USE_INST *psNext;
	
			PVR_DPF((PVR_DBG_ERROR,"DuplicateUSEASMInstrcutionList(): Failed to allocate memory for USEASM instructions"));

			psOutputInstruction = psDstUSEASMInfo->psFirstUSEASMInstruction;

			while(psOutputInstruction)
			{
				psNext = psOutputInstruction->psNext;

				GLES1Free(IMG_NULL, psOutputInstruction);

				psOutputInstruction = psNext;
			}

			psDstUSEASMInfo->psFirstUSEASMInstruction		= IMG_NULL;
			psDstUSEASMInfo->psLastUSEASMInstruction		= IMG_NULL;
			psDstUSEASMInfo->ui32NumMainUSEASMInstructions	= 0;

			return;
		}

		*psOutputInstruction = *psInputInstruction;

		if(!psDstUSEASMInfo->psFirstUSEASMInstruction)
		{
			psDstUSEASMInfo->psFirstUSEASMInstruction = psOutputInstruction;
		}

		psOutputInstruction->psNext = IMG_NULL;

		if(psPrev)
		{
			psPrev->psNext = psOutputInstruction;
		}

		psOutputInstruction->psPrev = psPrev;

		psPrev = psOutputInstruction;
		 
		psInputInstruction = psInputInstruction->psNext;
	}

	psDstUSEASMInfo->psLastUSEASMInstruction = psOutputInstruction;

	psDstUSEASMInfo->ui32NumMainUSEASMInstructions = psSrcUSEASMInfo->ui32NumMainUSEASMInstructions;
}	


/***********************************************************************************
 Function Name      : UseAssemblerError
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID IMG_CALLCONV UseAssemblerError(IMG_PVOID pvContext, PUSE_INST psInst, IMG_CHAR *pszFmt, ...)
{
	va_list ap;
	IMG_CHAR pszTemp[256];

	PVR_UNREFERENCED_PARAMETER(psInst);
	PVR_UNREFERENCED_PARAMETER(pvContext);

	va_start(ap, pszFmt);
	vsprintf(pszTemp, pszFmt, ap);
	PVR_TRACE(("Assembler Error:%s\n", pszTemp));
	va_end(ap);
}


/***********************************************************************************
 Function Name      : UseAssemblerSetLabelAddress
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID IMG_CALLCONV UseAssemblerSetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel, IMG_UINT32 uAddress)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uLabel);
	PVR_UNREFERENCED_PARAMETER(uAddress);
}


/***********************************************************************************
 Function Name      : UseAssemblerGetLabelName
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_PCHAR IMG_CALLCONV UseAssemblerGetLabelName(IMG_PVOID pvContext, IMG_UINT32 uLabel)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uLabel);
	return IMG_NULL;
}


/***********************************************************************************
 Function Name      : UseAssemblerGetLabelAddress
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_UINT32 IMG_CALLCONV UseAssemblerGetLabelAddress(IMG_PVOID pvContext, IMG_UINT32 uLabel)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uLabel);

	return 0;
}


/***********************************************************************************
 Function Name      : UseasmRealloc
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_PVOID IMG_CALLCONV UseasmRealloc(IMG_VOID *pvContext, IMG_VOID *pvOldBuf, IMG_UINT32 uNewSize, IMG_UINT32 uOldSize)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(uOldSize);

	return GLES1Realloc((GLES1Context *)pvContext, pvOldBuf, uNewSize);
}


/***********************************************************************************
 Function Name      : UseasmAlloc
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_PVOID UseasmAlloc(IMG_PVOID pvContext, IMG_UINT32 uSize)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);

	return GLES1Malloc(((USEASM_CONTEXT *)pvContext)->pvContext, uSize);
}


/***********************************************************************************
 Function Name      : UseasmFree
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID UseasmFree(IMG_PVOID pvContext, IMG_PVOID pvBlock)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);

	GLES1Free(IMG_NULL, pvBlock);
}


/***********************************************************************************
 Function Name      : AssembleUSEASMInstrcutions
 Inputs             : gc, psUSEASMInfo
 Outputs            : psUSEASMInfo
 Returns            : -
 Description        : Calls USEASM to generate USE instructions for a list
					  of USEASM instructions
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR AssembleUSEASMInstructions(GLES1Context *gc, GLESUSEASMInfo *psUSEASMInfo)
{
	USE_REGISTER sOutputRegister= {0, USEASM_REGTYPE_OUTPUT, 0, 0, 0};
	IMG_UINT32 auKeepOutputReg[1]= { 0 };
	IMG_UINT32 auKeepTempReg[1]= { 0 };
	IMG_UINT32 auKeepPAReg[1]= { 0 };
	USEASM_CONTEXT sUseasmContext;
	USEOPT_DATA sUseoptData;
	SGX_CORE_INFO sTarget;
	IMG_UINT32 i;

	if(!psUSEASMInfo->psFirstUSEASMInstruction)
	{
		PVR_DPF((PVR_DBG_ERROR,"AssembleUSEASMInstructions(): No USEASM input instructions"));

		psUSEASMInfo->ui32NumHWInstructions = 0;

		return GLES1_GENERAL_MEM_ERROR;
	}

	psUSEASMInfo->pui32HWInstructions = GLES1Malloc(gc, psUSEASMInfo->ui32NumMainUSEASMInstructions * EURASIA_USE_INSTRUCTION_SIZE);

	if(!psUSEASMInfo->pui32HWInstructions)
	{
		PVR_DPF((PVR_DBG_ERROR,"AssembleUSEASMInstructions(): Failed to allocate memory for HW instructions"));

		psUSEASMInfo->ui32NumHWInstructions = 0;

		return GLES1_HOST_MEM_ERROR;
	}


	sTarget.eID = SGX_CORE_ID;

#if defined(SGX_CORE_REV)
	sTarget.uiRev = SGX_CORE_REV;
#else
	sTarget.uiRev = 0;		/* use head revision */
#endif


	sUseasmContext.pvContext          = (IMG_VOID *)gc;
	sUseasmContext.pvLabelState       = IMG_NULL;
	sUseasmContext.pfnRealloc         = UseasmRealloc;
	sUseasmContext.pfnGetLabelAddress = UseAssemblerGetLabelAddress;
	sUseasmContext.pfnSetLabelAddress = UseAssemblerSetLabelAddress;
	sUseasmContext.pfnGetLabelName    = UseAssemblerGetLabelName;
	sUseasmContext.pfnAssemblerError  = UseAssemblerError;


	sUseoptData.pfnAlloc		= UseasmAlloc;
	sUseoptData.pfnFree			= UseasmFree;

	sUseoptData.uNumTempRegs	= psUSEASMInfo->ui32MaxTempNumber + 1;
	sUseoptData.uNumPARegs		= psUSEASMInfo->ui32MaxPrimaryNumber + 1;
	sUseoptData.uNumOutputRegs	= 1;

	sUseoptData.auKeepTempReg	= auKeepTempReg;
	sUseoptData.auKeepPAReg		= auKeepPAReg;

	for(i=0; i<sUseoptData.uNumPARegs; i++)
	{
		UseoptSetBit(auKeepPAReg, i, IMG_TRUE);
	}

	sUseoptData.auKeepOutputReg	= auKeepOutputReg;

	sUseoptData.psStart			= psUSEASMInfo->psFirstUSEASMInstruction;
	sUseoptData.psProgram		= psUSEASMInfo->psFirstUSEASMInstruction;

	sUseoptData.uNumOutRegs		= 1;
	sUseoptData.asOutRegs		= &sOutputRegister;


#if 0
{
	IMG_UINT32 i, pi[50*8];

	i = UseAssembler(UseAsmGetCoreDesc(&sTarget),
				     psUSEASMInfo->psFirstUSEASMInstruction,
				     pi,
				     0,
				     &sUseasmContext);

	printf("Before:\n");

	UseDisassembler(UseAsmGetCoreDesc(&sTarget), i, pi);
}	
#endif

	/* Optimise program */
	if((psUSEASMInfo->ui32NumMainUSEASMInstructions>1) && !gc->sAppHints.bDisableUSEASMOPT)
	{
		UseoptProgram(&sTarget, &sUseasmContext, &sUseoptData);

		if(sUseoptData.eStatus!=USEOPT_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"AssembleUSEASMInstructions(): UseoptProgram failed (%x)", sUseoptData.eStatus));

			GLES1Free(IMG_NULL, psUSEASMInfo->pui32HWInstructions);

			psUSEASMInfo->pui32HWInstructions   = IMG_NULL;
			psUSEASMInfo->ui32NumHWInstructions = 0;

			return GLES1_GENERAL_MEM_ERROR;
		}
	}

	/* Reset the start pointer in case the first instruction was optimised out */
	psUSEASMInfo->psFirstUSEASMInstruction = sUseoptData.psProgram;

	/* Assemble program */
	psUSEASMInfo->ui32NumHWInstructions = UseAssembler(UseAsmGetCoreDesc(&sTarget),
										   psUSEASMInfo->psFirstUSEASMInstruction,
										   psUSEASMInfo->pui32HWInstructions,
										   0,
										   &sUseasmContext);
#if 0
{
	printf("After:\n");

	UseDisassembler(UseAsmGetCoreDesc(&sTarget), psUSEASMInfo->ui32NumHWInstructions, psUSEASMInfo->pui32HWInstructions);
}	
#endif

	return GLES1_NO_ERROR;
}
