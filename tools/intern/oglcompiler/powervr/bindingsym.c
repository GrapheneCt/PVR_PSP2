/******************************************************************************
 * Name         : bindingsym.c
 * Created      : 24/06/2004
 *
 * Copyright    : 2004-2008 by Imagination Technologies Limited.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: bindingsym.c $
 *****************************************************************************/
#include <stdio.h>
#include <string.h>

#include "../glsl/error.h"
#include "glsl.h"
#include "icode.h"
#include "debug.h"
#include "symtab.h"
#include "ic2uf.h"
#include "common.h"

#include "icuf.h"
#include "icgen.h"



#define IS_DEPTH_TEXDESC(builtin) (builtin >= GLSLBV_PMXDEPTHTEXTUREDESC0 && builtin <= GLSLBV_PMXDEPTHTEXTUREDESC7)

#ifdef DUMP_LOGFILES
static const IMG_CHAR *abyVaryingDesc[] =
{
	"position", "color0", "color1", "tc0", "tc1", "tc2", "tc3", "tc4", "tc5", "tc6", "tc7", 
	"tc8", "tc9", 
	"ptsize", "fog", "clipplane0", "clipplane1", "clipplane2", "clipplane3",
	"clipplane4", "clipplane5"
};
#endif


/*****************************************************************************
 FUNCTION	: CopyConstantData

 PURPOSE	: Copy constant data from the global symbol table to the HW symbol, 

 PARAMETERS	: pvConstantData	- Source data stored in the global symbol table
			  uNumAlloc			- Alloc count for the HW symbol
			  ui32CompUseMask	- Component layout mask corespoinding to uNumAlloc
			  iArraySize		- Array size if it is an array. 0 means non-array
			  eTypeSpecifier    - Type Specifier
			  pvDefaultCompData - Destination data pointer where constant data is copied to

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_BOOL CopyConstantData(IMG_VOID			*pvConstantData,
								 IMG_UINT32			uNumAlloc, 
								 IMG_UINT32			ui32CompUseMask,
								 IMG_INT32			iArraySize,
								 GLSLTypeSpecifier	eTypeSpecifier,
								 IMG_FLOAT			*pfDestDataBase)
{
	IMG_UINT32 i, j;
	IMG_FLOAT  *pfDestData = pfDestDataBase;
	IMG_UINT32 uArraySize = iArraySize ? iArraySize : 1;

	if(GLSL_IS_INT(eTypeSpecifier) || GLSL_IS_BOOL(eTypeSpecifier))
	{
		/* INT */
		IMG_INT32 *piSrcData = (IMG_INT32 *)pvConstantData;

		for(i = 0; i < uArraySize; i++)
		{
			for(j = 0; j < uNumAlloc; j++)
			{
				if(ui32CompUseMask & (1 << j))
				{
#ifdef GLSL_NATIVE_INTEGERS
					pfDestData[j] = *(IMG_FLOAT*)piSrcData; /* Native int support, so put the int directly into the float array. */
#else
					pfDestData[j] = (IMG_FLOAT)(*piSrcData); /* Integers are being emulated by floats, so convert the integer into a float */
#endif

					piSrcData++;
				}
			}

			pfDestData += uNumAlloc;
		}
	}
	else
	{		
		/* FLOAT/MATRIX */
		IMG_FLOAT *pfSrcData = (IMG_FLOAT *)pvConstantData;

		for(i = 0; i < uArraySize; i++)
		{
			for(j = 0; j < uNumAlloc; j++)
			{
				if(ui32CompUseMask & (1 << j))
				{
					pfDestData[j] = (*pfSrcData);

					pfSrcData++;
				}
			}

			pfDestData += uNumAlloc;
		}
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ConvertToHWRegType
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static GLSLHWRegType ConvertToHWRegType(UF_REGTYPE eUFRegType)
{
	
	GLSLHWRegType eRegType = HWREG_FLOAT; 

	/* Work out regtype from UFREG reg type */
	switch(eUFRegType)
	{
		case UFREG_TYPE_TEX:
			eRegType			= HWREG_TEX;
			break;
		case UFREG_TYPE_TEXCOORD:
		case UFREG_TYPE_CONST:
		case UFREG_TYPE_VSINPUT:
		case UFREG_TYPE_VSOUTPUT:
		default:
			eRegType			= HWREG_FLOAT;
			break;
	}

	return eRegType;

}
/******************************************************************************
 * Function Name: ProcessBindingSymbolEntry
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Fill in all the information for a binding symbol except the symbol name 
 *				  which has been filled in before this funciton call
 *****************************************************************************/
static IMG_VOID ProcessBindingSymbolEntry(GLSLCompilerPrivateData *psCPD, 
										  GLSLUniFlexContext	*psUFContext,
										  const HWSYMBOL		*psHWSymbol,
										  IMG_BYTE				**ppbyConstantData,
										  GLSLIdentifierData	*psIdentifierData,
										  GLSLBindingSymbol		*psBindingSymbol,
										  GLSLBindingSymbolList	*psBindingSymbolList)
{
	IMG_FLOAT *pfConstant = IMG_NULL;
	GLSLTypeSpecifier eTypeSpecifier = psIdentifierData->sFullySpecifiedType.eTypeSpecifier;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	psBindingSymbol->eBIVariableID			= psIdentifierData->eBuiltInVariableID;
	psBindingSymbol->eTypeSpecifier			= eTypeSpecifier;
	psBindingSymbol->eTypeQualifier			= psHWSymbol->sFullType.eTypeQualifier;
	psBindingSymbol->ePrecisionQualifier	= psHWSymbol->sFullType.ePrecisionQualifier;
	psBindingSymbol->eVaryingModifierFlags  = psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags;

	if(psIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY)
	{
		DebugAssert(psHWSymbol->iArraySize == 0);

		psBindingSymbol->iActiveArraySize = 1;
		psBindingSymbol->iDeclaredArraySize = 0;
	}
	else
	{
		DebugAssert(psHWSymbol->iArraySize == psIdentifierData->iActiveArraySize);

		psBindingSymbol->iActiveArraySize = psHWSymbol->iArraySize;
		psBindingSymbol->iDeclaredArraySize = psIdentifierData->sFullySpecifiedType.iArraySize;
	}

	psBindingSymbol->sRegisterInfo.eRegType = ConvertToHWRegType(psHWSymbol->eRegType);
	if(GLSL_IS_SAMPLER(eTypeSpecifier))
		psBindingSymbol->sRegisterInfo.u.uBaseComp = psHWSymbol->uTextureUnit;
	else
		psBindingSymbol->sRegisterInfo.u.uBaseComp = psHWSymbol->u.uCompOffset;

	psBindingSymbol->sRegisterInfo.uCompAllocCount = psHWSymbol->uAllocCount;

	if(GLSL_IS_STRUCT(psIdentifierData->sFullySpecifiedType.eTypeSpecifier))
	{
		psBindingSymbol->sRegisterInfo.ui32CompUseMask = 0;
	}
	else
	{
		psBindingSymbol->sRegisterInfo.ui32CompUseMask = psHWSymbol->uCompUseMask;

		if(ppbyConstantData && (*ppbyConstantData))
		{
			IMG_UINT32 uArraySize = psHWSymbol->iArraySize ? psHWSymbol->iArraySize : 1;
			switch(psBindingSymbol->sRegisterInfo.eRegType)
			{
				case HWREG_FLOAT:
				default:
					pfConstant = psBindingSymbolList->pfConstantData;
					pfConstant += psHWSymbol->u.uCompOffset;
					break;
			}

			CopyConstantData((*ppbyConstantData), psHWSymbol->uAllocCount, psBindingSymbol->sRegisterInfo.ui32CompUseMask,
				psHWSymbol->iArraySize, eTypeSpecifier, pfConstant);

			*ppbyConstantData += TYPESPECIFIER_DATA_SIZE(eTypeSpecifier) * uArraySize;
			
		}
	}

	if(GLSL_IS_DEPTH_TEXTURE(psBindingSymbol->eTypeSpecifier))
	{
		IMG_UINT32 i;
		for(i = 0; i < psBindingSymbolList->uNumDepthTextures; i++)
		{
			if(psHWSymbol->uSymbolId == psUFContext->psICProgram->asDepthTexture[i].uTextureSymID)
			{
				psBindingSymbolList->psDepthTextures[i].psTextureSymbol = psBindingSymbol;
			}
		}
	}
	if(IS_DEPTH_TEXDESC(psBindingSymbol->eBIVariableID))
	{
		IMG_UINT32 i;
		for(i = 0; i < psBindingSymbolList->uNumDepthTextures; i++)
		{
			if(psHWSymbol->uSymbolId == psUFContext->psICProgram->asDepthTexture[i].uTexDescSymID)
			{
				psBindingSymbolList->psDepthTextures[i].psTexDescSymbol = psBindingSymbol;
				break;
			}
		}
	}

	/* Initialise num of base type members */
	psBindingSymbol->uNumBaseTypeMembers = 0;
	psBindingSymbol->psBaseTypeMembers = IMG_NULL;


#ifdef DUMP_LOGFILES
	{
		IMG_UINT32 j;
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%-35s %-17s %-9s %-4s %4d/%-4d %4d/%4d.%-3d %-5d ", 
					   psBindingSymbol->pszName, 
					   GLSLTypeQualifierFullDescTable(psBindingSymbol->eTypeQualifier),
					   GLSLPrecisionQualifierFullDescTable[psIdentifierData->sFullySpecifiedType.ePrecisionQualifier],
					   GLSLTypeSpecifierDescTable(psBindingSymbol->eTypeSpecifier),
					   psBindingSymbol->iActiveArraySize,
					   psBindingSymbol->iDeclaredArraySize,
					   psBindingSymbol->sRegisterInfo.u.uBaseComp,
					   psBindingSymbol->sRegisterInfo.u.uBaseComp/REG_COMPONENTS,
					   psBindingSymbol->sRegisterInfo.u.uBaseComp%REG_COMPONENTS,
					   psBindingSymbol->sRegisterInfo.uCompAllocCount);

		if(!GLSL_IS_STRUCT(psIdentifierData->sFullySpecifiedType.eTypeSpecifier))
		{
			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%04X ", psBindingSymbol->sRegisterInfo.ui32CompUseMask);

			if(ppbyConstantData && (*ppbyConstantData))
			{
				DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "{ ");

				for(j = 0; j < psHWSymbol->uTotalAllocCount; j++)
				{
					DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%.4f", pfConstant[j]);

					if(j != psHWSymbol->uTotalAllocCount - 1)
					{
						DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, ", ");
					}
				}

				DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, " }");
			}
		}

		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n");
	}
#endif

}
/******************************************************************************
 * Function Name: AddBindingSymbol
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add a binding symbol entry
 *****************************************************************************/
static GLSLBindingSymbol *AddBindingSymbol(GLSLCompilerPrivateData *psCPD,
										   GLSLUniFlexContext		*psUFContext,
										   HWSYMBOL					*psHWSymbol,
										   GLSLIdentifierData		*psIdentifierData,
										   GLSLBindingSymbolList	*psBindingSymbolList)
{
	GLSLBindingSymbol	*psBindingSymbol = IMG_NULL;
	IMG_CHAR *pszName;
	IMG_BYTE *pbyConstantData = psIdentifierData->pvConstantData;

	/* Get a symbol entry */
	if(psBindingSymbolList->uNumBindings == psBindingSymbolList->uMaxBindingEntries)
	{
		IMG_UINT32 uNewEntries = psBindingSymbolList->uMaxBindingEntries + 32;
		psBindingSymbolList->psBindingSymbolEntries = DebugMemRealloc(psBindingSymbolList->psBindingSymbolEntries, 
			uNewEntries * sizeof(GLSLBindingSymbol));
		if(psBindingSymbolList->psBindingSymbolEntries == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("AddTopLevelBindingSymbols: Failed to extend the active symbol list \n"));
			return IMG_NULL;
		}

		psBindingSymbolList->uMaxBindingEntries = uNewEntries;
	}

	psBindingSymbol = &psBindingSymbolList->psBindingSymbolEntries[psBindingSymbolList->uNumBindings];

	psBindingSymbolList->uNumBindings++;

	/* Work out the name and fill in */
	pszName = (IMG_CHAR *)GetSymbolName(psUFContext->psSymbolTable, psHWSymbol->uSymbolId);
	psBindingSymbol->pszName = DebugMemAlloc(strlen(pszName) + 1);
	if(psBindingSymbol->pszName == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("AddTopLevelBindingSymbols: Failed to allocate memory for a symbol name \n"));
		return IMG_NULL;
	}

	strcpy(psBindingSymbol->pszName, pszName);

	/* Fill in other information of the binding symbol */
	ProcessBindingSymbolEntry(psCPD, psUFContext, psHWSymbol, &pbyConstantData, psIdentifierData, psBindingSymbol, psBindingSymbolList);

	return psBindingSymbol;
}


typedef struct STRUCT_CONTEXT_TAG
{
	/* The first component offset for the top structure */
	IMG_UINT32				uStructBaseOffset;
	IMG_UINT32				uStructTextureUnitBase;

	/* Current member offset, increment it after processing every member, starting from 0 */
	IMG_UINT32				uMemberOffset;

	/* Current length and name */
	IMG_UINT32				uNameLength;
	IMG_CHAR				*psName;

	/* Point to the identified data for the current being processed*/
	GLSLIdentifierData		*psIdentifierData;

	/* Point to current unprocessed constant data */
	IMG_BYTE				*pbyConstantData;

	/*	
		The following fields are the information for the top structure, 
		it doesn't change during recursively procesing 
	*/
	HWSYMBOL				*psStructHWSymbol;
	GLSLStructureAlloc		*psStructStructAlloc;
	GLSLBindingSymbol		*psStructBindingSymbol;

} STRUCT_CONTEXT;
/******************************************************************************
 * Function Name: RecursivelyAddStructMembers
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID RecursivelyAddStructMembers(GLSLCompilerPrivateData *psCPD,
											GLSLUniFlexContext		*psUFContext,
											STRUCT_CONTEXT			*psContext,
											GLSLBindingSymbolList	*psBindingSymbolList)
{
	
	IMG_UINT32 j;
	GLSLIdentifierData		*psIdentifierData = psContext->psIdentifierData, *psMemberIdentifierData;
	HWSYMBOL sMemberHWSymbol = (*psContext->psStructHWSymbol);
	GLSLICProgram			*psICProgram = psUFContext->psICProgram;

	/* If it is a struct, we need to go further (recursively ) */
	GLSLStructureDefinitionData *psStructureDefinitionData;
	IMG_UINT32 uNameStart = psContext->uNameLength;
	IMG_UINT32 i;
	IMG_UINT32 uArraySize = (psIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY) ? 1: psIdentifierData->iActiveArraySize;
	IMG_UINT32 uExtra = 0;
	GLSLStructureAlloc *psTopStructAlloc = psContext->psStructStructAlloc;

	psStructureDefinitionData = (GLSLStructureDefinitionData *) 
		GetSymbolTableData(psICProgram->psSymbolTable, psIdentifierData->sFullySpecifiedType.uStructDescSymbolTableID);

	uNameStart = psContext->uNameLength;

	for(j = 0; j < uArraySize; j++)
	{
		if(psIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY)
		{
			sprintf(psContext->psName + uNameStart, ".");
			uExtra = 1;
		}
		else
		{
			sprintf(psContext->psName + uNameStart, "[%u].", j);
			uExtra = 4;
		}

		for(i = 0; i < psStructureDefinitionData->uNumMembers; i++)
		{
			psMemberIdentifierData = &psStructureDefinitionData->psMembers[i].sIdentifierData;

			memcpy(psContext->psName + uNameStart + uExtra, psStructureDefinitionData->psMembers[i].pszMemberName, 
				strlen(psStructureDefinitionData->psMembers[i].pszMemberName) );

			/**
			 * OGL64 Review.
			 * Use size_t for strlen?
			 */
			psContext->uNameLength = uNameStart + uExtra + (IMG_UINT32)strlen(psStructureDefinitionData->psMembers[i].pszMemberName);
	
			if(GLSL_IS_STRUCT(psMemberIdentifierData->sFullySpecifiedType.eTypeSpecifier))
			{
				psContext->psIdentifierData = psMemberIdentifierData;

				RecursivelyAddStructMembers(psCPD, psUFContext, psContext, psBindingSymbolList);
			}
			else
			{
				GLSLBindingSymbol *psBindingSymbol;
				IMG_UINT32 uStructMemOffset = psContext->uMemberOffset % psTopStructAlloc->uNumBaseTypeMembers;
					
				/* Add Members */
				/* Work out the name for a base type member */
				psBindingSymbol = &psContext->psStructBindingSymbol->psBaseTypeMembers[psContext->uMemberOffset];

				psBindingSymbol->pszName = DebugMemAlloc((psContext->uNameLength) + 1);
				if(psBindingSymbol->pszName == IMG_NULL)
				{
					LOG_INTERNAL_ERROR(("RecursivelyAddStructMembers: Failed to allocate memory for a symbol name \n"));
					return;
				}

				memcpy(psBindingSymbol->pszName, psContext->psName, psContext->uNameLength);
				psBindingSymbol->pszName[psContext->uNameLength] = 0;


				sMemberHWSymbol.u.uCompOffset			= psContext->uStructBaseOffset
														+ psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].uCompOffset;
				sMemberHWSymbol.uTextureUnit			= psContext->uStructTextureUnitBase
														+ psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].uTextureUnitOffset;
				sMemberHWSymbol.iArraySize				= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].iArraySize;
				sMemberHWSymbol.sFullType.eTypeSpecifier= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].sFullType.eTypeSpecifier;
				sMemberHWSymbol.sFullType.ePrecisionQualifier= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].sFullType.ePrecisionQualifier;
				sMemberHWSymbol.uAllocCount				= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].uAllocCount;
				sMemberHWSymbol.uCompUseMask			= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].uCompUseMask;
				sMemberHWSymbol.uTotalAllocCount		= psTopStructAlloc->psBaseMemberAlloc[uStructMemOffset].uTotalAllocCount;

				/* Adding all the information for the entry */
				ProcessBindingSymbolEntry(psCPD,
				                          psUFContext, 
										  &sMemberHWSymbol,
										  &psContext->pbyConstantData,
										  psMemberIdentifierData, 
										  psBindingSymbol, 
										  psBindingSymbolList);
	
				/* Increase the member offset */
				psContext->uMemberOffset++;
				
			}
		}

		/* If statement checks if we are still on the topmost struct, or if we have have recursed to one of the sub-structs. */
		if(psTopStructAlloc->sFullType.uStructDescSymbolTableID == psIdentifierData->sFullySpecifiedType.uStructDescSymbolTableID)
		{
			/* If the topmost structure is used to declare an array, then we need to increment the base offset and texture unit base
			   for each element of the array of topmost structs. If the topmost struct contains arrays, then there will already be a
			   psBaseMemberAlloc[].uCompOffset for each of the member elements, so this must ONLY be done for the TOPMOST structure. */
			psContext->uStructBaseOffset += psTopStructAlloc->uOverallAllocSize;
			psContext->uStructTextureUnitBase += psTopStructAlloc->uTotalTextureUnits;
		}
	}

}

/******************************************************************************
 * Function Name: ProcessHWSymbolEntry
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Process an entry of symbol reg use list
 *****************************************************************************/
static IMG_VOID ProcessHWSymbolEntry(GLSLCompilerPrivateData *psCPD,
									 GLSLUniFlexContext		*psUFContext,
		  							 HWSYMBOL				*psHWSymbol,
									 GLSLBindingSymbolList	*psBindingSymbolList)
{
	GLSLBindingSymbol *psBindingSymbol;
	GLSLIdentifierData *psIdentifierData;

	/* Get psIndentifierData */
	psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psUFContext->psSymbolTable, psHWSymbol->uSymbolId);
	if(psIdentifierData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessHWSymbolEntry: Failed to get data for identifier %d\n", psHWSymbol->uSymbolId));
		return;
	}

	psBindingSymbol = AddBindingSymbol(psCPD, psUFContext, psHWSymbol, psIdentifierData, psBindingSymbolList);

	if(GLSL_IS_STRUCT(psBindingSymbol->eTypeSpecifier))
	{
		IMG_CHAR			psName[512];
		IMG_UINT32			uNameLength;
		IMG_CHAR			*pszStructName;
		STRUCT_CONTEXT		sContext;
		GLSLStructureAlloc	*psStructAlloc = GetStructAllocInfo(psCPD, psUFContext, &psHWSymbol->sFullType);
		IMG_UINT32			uArraySize = (IMG_UINT32) (psBindingSymbol->iActiveArraySize);

		pszStructName =	(IMG_CHAR *)GetSymbolName(psUFContext->psSymbolTable, psHWSymbol->uSymbolId);

		strcpy(psName, pszStructName);

		/**
		 * OGL64 Review.
		 * Use size_t for strlen?
		 */
		uNameLength = (IMG_UINT32)strlen(pszStructName);

		/*
			Alloc memory for base type members
		*/
		psBindingSymbol->uNumBaseTypeMembers = psStructAlloc->uNumBaseTypeMembers * uArraySize;
		psBindingSymbol->psBaseTypeMembers = DebugMemAlloc(psBindingSymbol->uNumBaseTypeMembers * sizeof(GLSLBindingSymbol));
		if(psBindingSymbol->psBaseTypeMembers == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ProcessHWSymbolEntry: Failed to alloc memory"));
			return ;
		}

		/* Initialise and setup context for processing structure */

		/* Unchanged fields */
		sContext.psStructHWSymbol		= psHWSymbol;
		sContext.psStructStructAlloc	= psStructAlloc;
		sContext.psStructBindingSymbol	= psBindingSymbol;

		/* Changable fields */
		sContext.uStructBaseOffset		= psHWSymbol->u.uCompOffset;
		sContext.uStructTextureUnitBase = psHWSymbol->uTextureUnit;
		sContext.uMemberOffset			= 0;
		sContext.uNameLength			= uNameLength;
		sContext.psName					= psName;
		sContext.psIdentifierData		= psIdentifierData;
		sContext.pbyConstantData		= psIdentifierData->pvConstantData;

		/* Recursive processing structure members */
		RecursivelyAddStructMembers(psCPD, psUFContext, &sContext, psBindingSymbolList);

		/* Doublly check for correctness */
		if(sContext.uMemberOffset != psBindingSymbol->uNumBaseTypeMembers)
		{
			LOG_INTERNAL_ERROR(("ProcessHWSymbolEntry: Number of structure members do not match\n"));
		}

	}

	/* For literal constants etc. we don't need to add to the binding symbol list */
	if( psBindingSymbol->eTypeQualifier == GLSLTQ_INVALID ||
		psBindingSymbol->eTypeQualifier == GLSLTQ_TEMP    ||
		psBindingSymbol->eTypeQualifier == GLSLTQ_CONST )
	{
		if(psBindingSymbol->pszName) DebugMemFree(psBindingSymbol->pszName);

		/* Free base type members */
		if(psBindingSymbol->uNumBaseTypeMembers)
		{
			IMG_UINT32 j;
			for(j = 0; j < psBindingSymbol->uNumBaseTypeMembers; j++)
			{
				if(psBindingSymbol->psBaseTypeMembers[j].pszName)
				{
					DebugMemFree(psBindingSymbol->psBaseTypeMembers[j].pszName);
				}
			}
		
			DebugMemFree(psBindingSymbol->psBaseTypeMembers);

			psBindingSymbol->psBaseTypeMembers = IMG_NULL;
		}

		/* Remove from binding symbol list */
		psBindingSymbolList->uNumBindings--;
	}

}

/******************************************************************************
 * Function Name: GenerateBindingSymbolList
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : From HWSYMBOL to generate binding symbol list
 *****************************************************************************/
IMG_INTERNAL GLSLBindingSymbolList *GenerateBindingSymbolList(GLSLCompilerPrivateData *psCPD, GLSLUniFlexContext *psUFContext)
{
	IMG_UINT32 i;
	IMG_UINT32 uMaxBindingEntries = 32;
	GLSLBindingSymbolList *psBindingSymbolList;
	IMG_UINT32 uNumFloatCompsUsed;
#ifdef DUMP_LOGFILES
	IMG_UINT32 uVaryingMask = 0, uTexCoord0Index = 0;
#endif
	GLSLICProgram *psICProgram = psUFContext->psICProgram;

	HWSYMBOL *psHWSymbol;

	/* Create symbol list */
	psBindingSymbolList = DebugMemAlloc(sizeof(GLSLBindingSymbolList));
	if(psBindingSymbolList == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateBindingSymbolList: Failed to allocate memroy for binding symbol list \n"));
		return IMG_NULL;
	}
	
	uNumFloatCompsUsed = UFRegATableGetMaxComponent(&psUFContext->sFloatConstTab);

	/* Init values */
	psBindingSymbolList->uNumBindings	= 0;
	psBindingSymbolList->uNumCompsUsed	= uNumFloatCompsUsed;
	psBindingSymbolList->pfConstantData	= IMG_NULL;

	/* Depth texture info */
	psBindingSymbolList->uNumDepthTextures = psICProgram->uNumDepthTextures;
	psBindingSymbolList->psDepthTextures = DebugMemAlloc(psBindingSymbolList->uNumDepthTextures * sizeof(GLSLDepthTexture));
	if(psICProgram->uNumDepthTextures && psBindingSymbolList->psDepthTextures == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateBindingSymbolList: Failed to allocate memory for depth texture symbol list \n"));
		return IMG_NULL;
	}
	for(i = 0; i < psBindingSymbolList->uNumDepthTextures; i++)
	{
		psBindingSymbolList->psDepthTextures[i].psTextureSymbol = IMG_NULL;
		psBindingSymbolList->psDepthTextures[i].psTexDescSymbol = IMG_NULL;
		psBindingSymbolList->psDepthTextures[i].iOffset = psICProgram->asDepthTexture[i].iOffset;
	}

	/* All symbol entries */
	psBindingSymbolList->psBindingSymbolEntries = DebugMemAlloc(uMaxBindingEntries * sizeof(GLSLBindingSymbol));
	if(psBindingSymbolList->psBindingSymbolEntries == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateBindingSymbolList: Failed to allocate memory for binding symbol list \n"));
		return IMG_NULL;
	}
	psBindingSymbolList->uMaxBindingEntries = uMaxBindingEntries;

	psBindingSymbolList->pfConstantData	= DebugMemCalloc(uNumFloatCompsUsed * sizeof(IMG_FLOAT));
	if( psBindingSymbolList->uNumCompsUsed && 
		psBindingSymbolList->pfConstantData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateBindingSymbolList: Failed to allocate memory for constant data \n"));
		return IMG_NULL;
	}

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "-------------------------\n");
	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Binding Symbol List \n");
	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "-------------------------\n");

	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Name                                Qualifier         Precision Type Actv/Decl Comp/Register Count Mask DefaultData\n");
#endif
	

	psHWSymbol = psUFContext->sHWSymbolTab.psHWSymbolHead;
	while(psHWSymbol)
	{
		if(psHWSymbol->eRegType != UFREG_TYPE_TEMP)
		{
			ProcessHWSymbolEntry(psCPD, psUFContext, psHWSymbol, psBindingSymbolList);
		}

		psHWSymbol = psHWSymbol->psNext;
	}

#ifdef DEBUG
	/* check for symbol setup */
	for(i = 0; i < psBindingSymbolList->uNumDepthTextures; i++)
	{
		if( psBindingSymbolList->psDepthTextures[i].psTextureSymbol == IMG_NULL ||
			psBindingSymbolList->psDepthTextures[i].psTexDescSymbol == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("Binding symbol for depth texture %d has not found\n", i));
			return IMG_NULL;
		}
	}
#endif

#ifdef DUMP_LOGFILES
	if(psBindingSymbolList->uNumDepthTextures)
	{
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n-------------------------\n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Depth textures \n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "-------------------------\n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Samplers                      Desc                      Offset\n");

		for(i = 0; i < psBindingSymbolList->uNumDepthTextures; i++)
		{
			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%-30s%-25s %d\n", 
				psBindingSymbolList->psDepthTextures[i].psTextureSymbol->pszName,
				psBindingSymbolList->psDepthTextures[i].psTexDescSymbol->pszName,
				psBindingSymbolList->psDepthTextures[i].iOffset
				);
		}
	}

	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n-------------------------\n");
	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Varyings and dimensions \n");
	DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "-------------------------\n");
	for(i = 0; i < GLSL_NUM_VARYING_TYPES; i++)
	{
		if(psUFContext->eActiveVaryingMask & (1 << i))
		{
			uVaryingMask = (1 << i);

			if(uVaryingMask == GLSLVM_TEXCOORD0) uTexCoord0Index = i;

			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%-10s", abyVaryingDesc[i]);
			if(uVaryingMask <= GLSLVM_TEXCOORD9 && uVaryingMask >= GLSLVM_TEXCOORD0)
			{
				DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%d", psUFContext->auTexCoordDims[i - uTexCoord0Index]);
			}
			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n");
		}
	}

	{
		IMG_FLOAT *psData;

		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n-------------------------\n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Default Constant/Uniform Data \n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Static Flags: ");
		for(i = 0; i < psUFContext->uConstStaticFlagCount;i++)
		{
			IMG_UINT32 uWordIndex = i/32;
			IMG_UINT32 uWordBit = i%32;
			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%d", (psUFContext->puConstStaticFlags[uWordIndex] >> uWordBit) & 1);
			if(i%4 == 3) DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, " ");
		}
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "\n");
		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "-------------------------\n");

		DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "Register count = %d\n", 
						psBindingSymbolList->uNumCompsUsed/REG_COMPONENTS);

		psData = psBindingSymbolList->pfConstantData;

		for(i = 0; i <  psBindingSymbolList->uNumCompsUsed/REG_COMPONENTS; i++)
		{
			DumpLogMessage(LOGFILE_BINDINGSYMBOLS, 0, "%3d: %10.4f %10.4f %10.4f %10.4f\n", i, 
				psData[0], psData[1], psData[2], psData[3]);

			psData += REG_COMPONENTS;
		}
	}
#endif

	return psBindingSymbolList;

}


/******************************************************************************
 * Function Name: DestroyBindingSymbolList
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Destroy active binding symbol list
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyBindingSymbolList(GLSLBindingSymbolList *psBindingSymbolList)
{
	IMG_UINT32 i, j;
	GLSLBindingSymbol *psBindingSymbol;

	for(i = 0; i < psBindingSymbolList->uNumBindings; i++)
	{
		psBindingSymbol = &psBindingSymbolList->psBindingSymbolEntries[i];

		if(psBindingSymbol->pszName) DebugMemFree(psBindingSymbol->pszName);

		/* Free base type members */
		if(psBindingSymbol->uNumBaseTypeMembers)
		{
			for(j = 0; j < psBindingSymbol->uNumBaseTypeMembers; j++)
			{
				if(psBindingSymbol->psBaseTypeMembers[j].pszName)
				{
					DebugMemFree(psBindingSymbol->psBaseTypeMembers[j].pszName);
				}
			}
		
			DebugMemFree(psBindingSymbol->psBaseTypeMembers);
		}

	}

	DebugMemFree(psBindingSymbolList->psBindingSymbolEntries);

	if(psBindingSymbolList->pfConstantData)
	{
		DebugMemFree(psBindingSymbolList->pfConstantData);
	}

	DebugMemFree(psBindingSymbolList->psDepthTextures);

	DebugMemFree(psBindingSymbolList);
}

/******************************************************************************
 End of file (bindingsym.c)
******************************************************************************/
