/******************************************************************************
 * Name         : glsltree.c
 * Author       : James McCarthy
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
 * $Log: glsltree.c $
 *****************************************************************************/
#include "semantic.h"
#include "glsltree.h"
#include "icgen.h"

#include "../parser/debug.h"

#include "error.h"
#include "common.h"
#include "prepro.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* 
   RULES FOR FUNCTIONS

   If a function adds a node then it will

   1. Make the nodes passed up from any lower functions it's children (in a specific order)
   2. Return the newly created node

   Otherwise it will just return the node it received from the lower function.*

   *A function can not recieve more than one node from a lower function without creating a node itself.

   NODE TYPE

   IDENTIFIER      1. Constant expression which is the array size



*/



#define ASTValidateInputEntry_Ret(a, b, ret) if (!ParseTreeValidateBranch(psGLSLTreeContext->psParseContext, a, b)) { return ret; }

#define RULE1(a) a

#define ParseTreeToken(pt) (pt)

#define ASTValidateInputEntry(a, b)

/******************************************************************************
 * Function Name: ASTCheckFeatureVersion
 *
 * Inputs       :
 * Outputs      : 
 * Returns      : 
 * Globals Used : 
 *
 * Description  : Checks if a feature is supported by the currect GLSL language version,
 *				: and outputs a parse tree entry error if not.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTCheckFeatureVersion(GLSLTreeContext		*psGLSLTreeContext,
											   ParseTreeEntry		*psParseTreeEntry,
											   IMG_UINT32			uFeatureVersion,
											   const IMG_CHAR		*pszKeyword,
											   const IMG_CHAR		*pszFeatureDescription /* Optional */ )
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);


	
	IMG_UINT32 uRequiredVersion = uFeatureVersion;
	
	IMG_CHAR szKeywordAndFeature[1024] = "";
	IMG_CHAR szErrorString[1024];
	if(pszKeyword)
	{
		sprintf(szKeywordAndFeature, "'%s' : ", pszKeyword);
	}
	if(pszFeatureDescription)
	{
		strcat(szKeywordAndFeature, pszFeatureDescription);
		strcat(szKeywordAndFeature, " ");
	}

	if(uRequiredVersion > psGLSLTreeContext->uSupportedLanguageVersion)
	{
		sprintf(szErrorString, "%srequires language version %u\n",
			szKeywordAndFeature, uRequiredVersion);

		LogProgramParseTreeError(psCPD->psErrorLog, psParseTreeEntry, szErrorString);

		return IMG_FALSE;
	}


	return IMG_TRUE;
}

#if defined(GLSL_ES)
/******************************************************************************
 * Function Name: IsSamplerTypeSupported
 *
 * Inputs       :
 * Outputs      : -
 * Returns      : is supported
 * Globals Used : -
 *
 * Description  :
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IsSamplerTypeSupported(GLSLTreeContext				*psGLSLTreeContext,
											const ParseTreeEntry			*psFullySpecifiedTypeEntry,
											const GLSLFullySpecifiedType	*psFullySpecifiedType)
{
	if(psFullySpecifiedType->eTypeSpecifier == GLSLTS_SAMPLER3D && !(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_TEXTURE_3D))
	{
		GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);
		LogProgramParseTreeError(psCPD->psErrorLog, psFullySpecifiedTypeEntry, 
									"sampler3D requires the extension GL_OES_texture_3D.\n");
		return IMG_FALSE;
	}

	if(psFullySpecifiedType->eTypeSpecifier == GLSLTS_SAMPLEREXTERNAL && !(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_TEXTURE_EXTERNAL))
	{
		GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);
		LogProgramParseTreeError(psCPD->psErrorLog, psFullySpecifiedTypeEntry, 
									"samplerExternalOES requires the extension GL_OES_EGL_image_external.\n");
		return IMG_FALSE;
	}


	return IMG_TRUE;
}
#endif

/******************************************************************************
 * Function Name: ASTIncreaseScope
 *
 * Inputs       : psGLSLTreeContext
 * Outputs      : -
 * Returns      : IMG_TRUE. FIXME: superfluous
 * Globals Used : -
 *
 * Description  : Increases the scope level for the symbol table
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTIncreaseScope(GLSLTreeContext *psGLSLTreeContext)
{

	/* Increase the scope level for the symbol table */
	IncreaseScopeLevel(psGLSLTreeContext->psSymbolTable);

	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: FreePrecisionModifierData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreePrecisionModifierData(IMG_VOID *pvData)
{
	if (pvData)
	{
		DebugMemFree(pvData);
	}
}

/******************************************************************************
 * Function Name: GetCurrentPrecision
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static GLSLPrecisionQualifier GetCurrentPrecision(GLSLTreeContext   *psGLSLTreeContext,
										   GLSLTypeSpecifier  eTypeSpecifier)
{	
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_CHAR acPrecModName[256];
	IMG_UINT32 uSymbolTableID;
	SymTable *psSymbolTable = psGLSLTreeContext->psSymbolTable;

	/* Don't search symbol table if precision has never been modified */
	if (!psGLSLTreeContext->bShaderHasModifiedPrecision)
	{
		if (eTypeSpecifier == GLSLTS_FLOAT)
		{
			return psGLSLTreeContext->eDefaultFloatPrecision;
		}
		else if (GLSL_IS_INT_SCALAR(eTypeSpecifier))
		{
			return psGLSLTreeContext->eDefaultIntPrecision;
		}
		else if (GLSL_IS_SAMPLER(eTypeSpecifier))
		{
			return psGLSLTreeContext->eDefaultSamplerPrecision;
		}
		else
		{ 
			LOG_INTERNAL_ERROR(("GetCurrentPrecision: Invalid type specifier (%08X)\n", eTypeSpecifier));
			return GLSLPRECQ_UNKNOWN;
		}
	}

	/* Construct the name */
	sprintf(acPrecModName, PRECISION_MODIFIER_STRING, GLSLTypeSpecifierDescTable(eTypeSpecifier));

	if (!FindSymbol(psSymbolTable, acPrecModName, &uSymbolTableID, IMG_FALSE))
	{
		LOG_INTERNAL_ERROR(("GetCurrentPrecision: Could not find current precision\n"));
		return GLSLPRECQ_UNKNOWN;
	}
	else
	{
		GLSLPrecisionModifier *psPrecisionModifier = GetSymbolTableData(psSymbolTable, uSymbolTableID);

#ifdef DEBUG		
		if (psPrecisionModifier->eSymbolTableDataType != GLSLSTDT_PRECISION_MODIFIER)
		{
			LOG_INTERNAL_ERROR(("GetCurrentPrecision: Incorrect data type returned\n"));
			return GLSLPRECQ_UNKNOWN;
		}
#endif
		return psPrecisionModifier->ePrecisionQualifier;
	}
}


/******************************************************************************
 * Function Name: ModifyDefaultPrecision
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ModifyDefaultPrecision(GLSLTreeContext        *psGLSLTreeContext,
								GLSLPrecisionQualifier  ePrecisionQualifier,
								GLSLTypeSpecifier       eTypeSpecifier)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uSymbolTableID;
	IMG_CHAR acPrecModName[256];
	GLSLPrecisionModifier *psPrecisionModifier;
	SymTable              *psSymbolTable = psGLSLTreeContext->psSymbolTable;

	/* Construct the name */
	sprintf(acPrecModName, PRECISION_MODIFIER_STRING, GLSLTypeSpecifierDescTable(eTypeSpecifier));

	/* Look for a matching symbol at the current scope */
	if (FindSymbol(psSymbolTable,
				   acPrecModName,
				   &uSymbolTableID,
				   IMG_TRUE))
	{
		/* Get the data */
		psPrecisionModifier = GetSymbolTableData(psSymbolTable, uSymbolTableID);

#ifdef DEBUG
		if (psPrecisionModifier->eSymbolTableDataType != GLSLSTDT_PRECISION_MODIFIER)
		{
			LOG_INTERNAL_ERROR(("GetCurrentPrecision: Incorrect data type returned\n"));
			return IMG_FALSE;
		}
#endif

		/* Modify the precision */
		psPrecisionModifier->ePrecisionQualifier  = ePrecisionQualifier;
	}
	else
	{
		/* Alloc mem for the modifier */
		psPrecisionModifier = DebugMemAlloc(sizeof(GLSLPrecisionModifier));

		/* Check alloc succeeded */
		if (!psPrecisionModifier)
		{
			LOG_INTERNAL_ERROR(("AddPrecisionModifierToTable: Failed to allocate memory for precision modifier\n"));
			return IMG_FALSE;
		}

		/* Setup the data */
		psPrecisionModifier->eSymbolTableDataType = GLSLSTDT_PRECISION_MODIFIER;
		psPrecisionModifier->ePrecisionQualifier  = ePrecisionQualifier;
		psPrecisionModifier->eTypeSpecifier       = eTypeSpecifier;

		/* Add the data to the symbol table */
		if(!AddSymbol(psSymbolTable,
					  acPrecModName,
					  psPrecisionModifier,
					  sizeof(GLSLPrecisionModifier),
					  IMG_TRUE,
					  IMG_NULL,
					  FreePrecisionModifierData))
		{
			LOG_INTERNAL_ERROR(("AddPrecisionModifierToTable: Failed to add data to symbol table for '%s'\n", acPrecModName));
			return IMG_FALSE;
		}
	}

	/* Modify the default precision */
	if (eTypeSpecifier == GLSLTS_FLOAT)
	{
		psGLSLTreeContext->eDefaultFloatPrecision = ePrecisionQualifier;
	}
	else if (GLSL_IS_INT_SCALAR(eTypeSpecifier))
	{
		psGLSLTreeContext->eDefaultIntPrecision   = ePrecisionQualifier;
	}
	else if (GLSL_IS_SAMPLER(eTypeSpecifier))
	{
		psGLSLTreeContext->eDefaultSamplerPrecision = ePrecisionQualifier;
	}

	/* Should check to see if this is actually true... */
	psGLSLTreeContext->bShaderHasModifiedPrecision = IMG_TRUE;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTDecreaseScope
 *
 * Inputs       : psGLSLTreeContext
 * Outputs      : -
 * Returns      : IMG_TRUE. FIXME: superfluous
 * Globals Used : -
 *
 * Description  : Decreases the scope level for the symbol table
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTDecreaseScope(GLSLTreeContext *psGLSLTreeContext)
{
	/* Decrease the scope level for the symbol table */
	DecreaseScopeLevel(psGLSLTreeContext->psSymbolTable);

	/* Get the default precision for this scope */
	psGLSLTreeContext->eDefaultFloatPrecision   = GetCurrentPrecision(psGLSLTreeContext, GLSLTS_FLOAT);
	psGLSLTreeContext->eDefaultIntPrecision     = GetCurrentPrecision(psGLSLTreeContext, GLSLTS_INT);
	psGLSLTreeContext->eDefaultSamplerPrecision = GetCurrentPrecision(psGLSLTreeContext, GLSLTS_SAMPLER2D);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTCreateIDENTIFIERNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateIDENTIFIERNode(GLSLTreeContext        *psGLSLTreeContext,
								  ParseTreeEntry         *psIDENTIFIEREntry,
								  IMG_BOOL                bDeclaration,
								  GLSLFullySpecifiedType *psFullySpecifiedType)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_CHAR *pszIdentifierName;

	GLSLNode *psIDENTIFIERNode;

	GLSLFullySpecifiedType sFullySpecifiedType;

	/* Validate supplied entry */
	ASTValidateInputEntry(psIDENTIFIEREntry, SYMGLSL_IDENTIFIER);	

	/* Create the IDENITFIER Node */
	psIDENTIFIERNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psIDENTIFIEREntry);

	/* Check creation went OK */
	ASTValidateNodeCreation(psIDENTIFIERNode);

	/* Get name */
	pszIdentifierName = (IMG_CHAR *)ParseTreeToken(psIDENTIFIEREntry)->pvData;

	if (bDeclaration)
	{
		/* Add it to the symbol table */
		psIDENTIFIERNode->uSymbolTableID = ASTSemAddIdentifierToSymbolTable(psCPD, psGLSLTreeContext, psGLSLTreeContext->psSymbolTable,
																			psIDENTIFIEREntry,
																			pszIdentifierName,
																			psFullySpecifiedType,
																			IMG_FALSE,
																			GLSLBV_NOT_BTIN,
																			(GLSLIdentifierUsage)0,
																			0,
																			IMG_NULL, 
																			psGLSLTreeContext->eProgramType);

		if (!psIDENTIFIERNode->uSymbolTableID)
		{
			psIDENTIFIERNode->eNodeType = GLSLNT_ERROR;
		}
	}
	else
	{
		if (!FindSymbol(psGLSLTreeContext->psSymbolTable, pszIdentifierName, &(psIDENTIFIERNode->uSymbolTableID), IMG_FALSE))
		{
			LogProgramNodeError(GET_CPD_FROM_AST(psGLSLTreeContext)->psErrorLog, 
				psIDENTIFIERNode, "'%s' : undeclared identifer\n", pszIdentifierName);

			psIDENTIFIERNode->eNodeType = GLSLNT_ERROR;
		}
	}

	if(psIDENTIFIERNode->eNodeType != GLSLNT_ERROR)
	{
		GetSymbolInfo(psCPD, psGLSLTreeContext->psSymbolTable,
						   psIDENTIFIERNode->uSymbolTableID,
						   psGLSLTreeContext->eProgramType,
						   &sFullySpecifiedType,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL);

		if((IMG_UINT32)sFullySpecifiedType.ePrecisionQualifier > (IMG_UINT32)psGLSLTreeContext->eNextConsumingOperationPrecision)
		{
			/* In case later on we cant derive the precision for builtin function parameters, save the precision from the return identifier. */
			psGLSLTreeContext->eNextConsumingOperationPrecision = sFullySpecifiedType.ePrecisionQualifier;
		}
	}

	return psIDENTIFIERNode;
}

/******************************************************************************
 * Function Name: ASTCreateIDENTIFIERUseNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateIDENTIFIERUseNode(GLSLTreeContext *psGLSLTreeContext,
									 ParseTreeEntry  *psIDENTIFIEREntry)
{
	return ASTCreateIDENTIFIERNode(psGLSLTreeContext, psIDENTIFIEREntry, IMG_FALSE, IMG_NULL);
}

/******************************************************************************
 * Function Name: ASTCreateFLOATCONSTANTNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateFLOATCONSTANTNode(GLSLTreeContext  *psGLSLTreeContext,
									 ParseTreeEntry   *psFLOATCONSTANTEntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode *psIDENTIFIERNode;

	IMG_CHAR *pszName;

	IMG_FLOAT fData;

	IMG_CHAR szLastChar[2];

	/* Validate supplied entry */
	ASTValidateInputEntry(psFLOATCONSTANTEntry, SYMGLSL_FLOATCONSTANT);

	/* Create the IDENITFIER Node */
	psIDENTIFIERNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psFLOATCONSTANTEntry);

	/* Check creation went OK */
	ASTValidateNodeCreation(psIDENTIFIERNode);

	/* Get name */
	pszName = (IMG_CHAR *)ParseTreeToken(psFLOATCONSTANTEntry)->pvData;

	/* Check for 'f' suffixes on floats */
	szLastChar[0] = pszName[ParseTreeToken(psFLOATCONSTANTEntry)->uSizeOfDataInBytes - 2];
	if (szLastChar[0] == 'f' || szLastChar[0] == 'F')
	{
		szLastChar[1] =  '\0';
		ASTCheckFeatureVersion(psGLSLTreeContext, psFLOATCONSTANTEntry, 120, szLastChar, "suffix for floats");
	}

	/* Set up float constant data node */
	fData = (IMG_FLOAT)strtod(pszName, 0);

	/* Add data to the symbol table */
	if (!AddFloatConstant(psCPD, psGLSLTreeContext->psSymbolTable,
						  fData,
						  GLSLPRECQ_UNKNOWN,
						  IMG_TRUE,
						  &(psIDENTIFIERNode->uSymbolTableID)))
	{
		LOG_INTERNAL_ERROR(("ASTCreateFLOATCONSTANTNode: Failed to add float constant %s to symbol table", pszName));

		return IMG_NULL;
	}

	return psIDENTIFIERNode;
}

/******************************************************************************
 * Function Name: ASTCreateINTCONSTANTNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateINTCONSTANTNode(GLSLTreeContext *psGLSLTreeContext,
								   ParseTreeEntry  *psINTCONSTANTEntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode *psIDENTIFIERNode;

	IMG_CHAR *pszName;

	IMG_INT32 iData;

	/* Validate supplied entry */
	ASTValidateInputEntry(psINTCONSTANTEntry, SYMGLSL_INTCONSTANT);

	/* Create the IDENITFIER Node */
	psIDENTIFIERNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psINTCONSTANTEntry);

	/* Check creation went OK */
	ASTValidateNodeCreation(psIDENTIFIERNode);

	/* Get name */
	pszName = (IMG_CHAR *)ParseTreeToken(psINTCONSTANTEntry)->pvData;

	/* Set up int constant data */
	iData = strtol(pszName, 0, 0);

	/* Add data to the symbol table */
	if (!AddIntConstant(psCPD, psGLSLTreeContext->psSymbolTable,
						iData,
						GLSLPRECQ_UNKNOWN,
						IMG_TRUE
						,
						&(psIDENTIFIERNode->uSymbolTableID)))
	{
		LOG_INTERNAL_ERROR(("ASTCreateINTCONSTANTNode: Failed to add int constant %s to symbol table", pszName));
		
		return IMG_NULL;
	}

	return psIDENTIFIERNode;
}


/******************************************************************************
 * Function Name: ASTCreateBOOLCONSTANTNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateBOOLCONSTANTNode(GLSLTreeContext *psGLSLTreeContext,
									ParseTreeEntry  *psBOOLCONSTANTEntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode *psIDENTIFIERNode;

	IMG_CHAR *pszName;

	IMG_BOOL bData;

	/* Validate supplied entry */
	ASTValidateInputEntry(psBOOLCONSTANTEntry, SYMGLSL_BOOLCONSTANT);

	/* Create the IDENITFIER Node */
	psIDENTIFIERNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psBOOLCONSTANTEntry);

	/* Check creation went OK */
	ASTValidateNodeCreation(psIDENTIFIERNode);

	/* Get name */
	pszName = (IMG_CHAR *)ParseTreeToken(psBOOLCONSTANTEntry)->pvData;

	/* Extract data */
	if (strcmp(pszName, "false") == 0)
	{
		bData = IMG_FALSE;
	}
	else if (strcmp(pszName, "true") == 0)
	{
		bData = IMG_TRUE;
	}
	else
	{
		LOG_INTERNAL_ERROR(("ASTCreateBOOLCONSTANTNode: BOOLCONSTANT '%s' is not a recognised value", pszName));
		return IMG_NULL;
	}

	/* Add data to the symbol table */
	if (!AddBoolConstant(psCPD, psGLSLTreeContext->psSymbolTable,
						 bData,
						 GLSLPRECQ_UNKNOWN,
						 IMG_TRUE,
						 &(psIDENTIFIERNode->uSymbolTableID)))
	{
		LOG_INTERNAL_ERROR(("ASTCreateBOOLCONSTANTNode: Failed to add bool constant %s to symbol table", pszName));

		return IMG_NULL;
	}

	return psIDENTIFIERNode;
}

/******************************************************************************
 * Function Name: ASTFindFunction
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 ASTFindFunction(GLSLTreeContext         *psGLSLTreeContext,
						   GLSLNode                *psFunctionCallNode,
						   IMG_CHAR                *pszFunctionName,
						   GLSLFullySpecifiedType  *psFullySpecifiedTypes)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_BOOL   bFoundFunction;
	IMG_UINT32 uFunctionDefinitionSymbolTableID = 0;
	IMG_CHAR   *pszHashedFunctionName;
	IMG_UINT32 uNumParams = psFunctionCallNode->uNumChildren;
#ifdef GLSL_ES
	GLSLFunctionDefinitionData	*psFunctionData;
#endif

	/* Get a hashed name for the function */
	pszHashedFunctionName = ASTSemCreateHashedFunctionName(psGLSLTreeContext->psSymbolTable,
														  pszFunctionName,
														  uNumParams,
														  psFullySpecifiedTypes);

	/* Find the symbol ID of this function using the hashed name */
	bFoundFunction = FindSymbol(psGLSLTreeContext->psSymbolTable, pszHashedFunctionName, &uFunctionDefinitionSymbolTableID, IMG_FALSE);

	/* Free memory allocated for name */
	DebugMemFree(pszHashedFunctionName);

	if (!bFoundFunction)
	{
		/* 
		   Didn't find the function, possible it could be a constructor using a user defined structure,
		   which wouldn't get recognised as a constructor by the grammar. 
		*/
		IMG_CHAR acConstructorName[280];

		/* Create a constructor name */
		sprintf(acConstructorName, CONSTRUCTOR_STRING, pszFunctionName);
		
		/* Search for this constructor */
		bFoundFunction = FindSymbol(psGLSLTreeContext->psSymbolTable, acConstructorName, &uFunctionDefinitionSymbolTableID, IMG_FALSE);
	}

	/* 
	   If function not found and int->float conversion supported then search for alternatively named functions 
	   where a float exists where an int param has been supplied.
	*/
	if (!bFoundFunction && (psGLSLTreeContext->uSupportedLanguageVersion >= 120))
	{
		IMG_UINT32 i, j;
		IMG_UINT32 uNumberFunctionsFound = 0;
		IMG_UINT32 uNumberIntParams = 0;
		IMG_UINT32 uNumberOfCombinations;
		IMG_UINT32 *puIntParams;
		IMG_UINT32 uTempFunctionDefinitionSymbolTableID;

		puIntParams = DebugMemAlloc(sizeof(IMG_UINT32) * uNumParams);

		if(uNumParams && !puIntParams)
		{
			return 0;
		}

		/* Count number of int arguments */
		for (i = 0; i < uNumParams; i++)
		{
			if (GLSL_IS_INT(psFullySpecifiedTypes[i].eTypeSpecifier))
			{
				/* Store an index to this parameter */
				puIntParams[uNumberIntParams] = i;
				uNumberIntParams++;
			}
		}

		/* Caulate number of combinations */
		uNumberOfCombinations = 1 << uNumberIntParams;

		/* 
		   Cycle through all the combinations (except the 1st one - already checked above) 
		   On each iteration we'll convert the parameters to floats whose (shifted) values match 
		   the binary value of the loop counter - this should check all possible combinations..
		*/
		for (i = 1; i < uNumberOfCombinations; i++)
		{
			/* Convert some of the params to floats */
			for (j = 0; j < uNumberIntParams; j++)
			{
				if ((1 << j) & i)
				{
					IMG_UINT32 uParamIndex = puIntParams[j];

					/* Convert to float */
					psFullySpecifiedTypes[uParamIndex].eTypeSpecifier =
						(GLSLTypeSpecifier)(GLSLTS_FLOAT + (psFullySpecifiedTypes[uParamIndex].eTypeSpecifier - GLSLTS_INT));
				}
			}

			/* Get a hashed name for the function */
			pszHashedFunctionName = ASTSemCreateHashedFunctionName(psGLSLTreeContext->psSymbolTable,
																  pszFunctionName,
																  uNumParams,
																  psFullySpecifiedTypes);

			/* Find the symbol ID of this function using the hashed name */
			if (FindSymbol(psGLSLTreeContext->psSymbolTable,
						   pszHashedFunctionName,
						   &(uTempFunctionDefinitionSymbolTableID),
						   IMG_FALSE))
			{
				uNumberFunctionsFound++;
				bFoundFunction = IMG_TRUE;
				uFunctionDefinitionSymbolTableID = uTempFunctionDefinitionSymbolTableID;
			}
			
			/* Free memory allocated for name */
			DebugMemFree(pszHashedFunctionName);

			/* Convert the (converted) params back to int */
			for (j = 0; j < uNumberIntParams; j++)
			{
				if ((1 << j) & i)
				{
					IMG_UINT32 uParamIndex = puIntParams[j];

					/* Convert to back to int */
					psFullySpecifiedTypes[uParamIndex].eTypeSpecifier = 
						(GLSLTypeSpecifier)(GLSLTS_INT + (psFullySpecifiedTypes[uParamIndex].eTypeSpecifier - GLSLTS_FLOAT));
				}
			}
		}

		/* Check for more than match */
		if (uNumberFunctionsFound > 1)
		{
			LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
								"'%s' : more than one function matched this definition once "
								"integer parameters were converted to floats, try being more "
								"explicit with function parameters\n",
								pszFunctionName);

			/* Don't return any function ID if we can't return a unique one */
			uFunctionDefinitionSymbolTableID = 0;
		}

		DebugMemFree(puIntParams);
	}


#ifdef GLSL_ES
	if(bFoundFunction)
	{
		psFunctionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uFunctionDefinitionSymbolTableID);

		/* Check that if the app calls the builtin dFdx, dFdy or fwidth functions then the extension is enabled */
		if(psFunctionData->eFunctionType == GLSLFT_BUILT_IN)
		{
			const IMG_CHAR *pszRequiredExtensionName = IMG_NULL;

			if(!(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_STANDARD_DERIVATIVES) &&
			   (psFunctionData->eBuiltInFunctionID == GLSLBFID_DFDX ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_DFDY ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_FWIDTH))
			{
				pszRequiredExtensionName = "OES_standard_derivatives";
			}

			if(!(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_STANDARD_NOISE) &&
			   (psFunctionData->eBuiltInFunctionID == GLSLBFID_NOISE1 ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_NOISE2 ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_NOISE3 ||
				psFunctionData->eBuiltInFunctionID == GLSLBFID_NOISE4))
			{
				pszRequiredExtensionName = "OES_standard_noise";
			}

			if(!(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_IMG_TEXTURE_STREAM) &&
			   (psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTURESTREAM ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTURESTREAMPROJ))
			{
				pszRequiredExtensionName = "IMG_texture_stream2";
			}

			if(!(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_EXT_TEXTURE_LOD) &&
				(psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTUREGRAD ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTUREPROJGRAD ||
			   ((psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTURELOD ||
			    psFunctionData->eBuiltInFunctionID == GLSLBFID_TEXTUREPROJLOD) && 
				psGLSLTreeContext->eProgramType == GLSLPT_FRAGMENT)))
			{
				pszRequiredExtensionName = "EXT_texture_shader_lod";
			}


			if(pszRequiredExtensionName != IMG_NULL)
			{
				LogProgramError(psCPD->psErrorLog, "Invalid call to function %s: extension %s is not enabled.\n",
					psFunctionData->pszOriginalFunctionName, pszRequiredExtensionName);
				return 0;
			}
		}
	}
#endif

	if (!bFoundFunction)
	{
		LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
							"'%s' : no matching overloaded function found\n",
							pszFunctionName);

		uFunctionDefinitionSymbolTableID = 0;
	}

	return uFunctionDefinitionSymbolTableID;
}


/******************************************************************************
 * Function Name: ASTRegisterFunctionCall
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTRegisterFunctionCall(GLSLTreeContext            *psGLSLTreeContext,
								 GLSLFunctionDefinitionData	*psCalledFunctionData,
								 IMG_UINT32                  uCalledFunctionSymbolTableID)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLFunctionDefinitionData	*psCurrentFnData;

	/* Might be at global scope and processing a constructor in which case we'll have no current function ID */
	if (!psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID)
	{
		return IMG_TRUE;
	}

	/* Retrieve the current function definition */
	psCurrentFnData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
										 psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID);

#ifdef DEBUG
	if (!psCurrentFnData || psCurrentFnData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)
	{
		LOG_INTERNAL_ERROR(("ASTRegisterFunctionCall: Failed to retreive function data\n"));
		return IMG_FALSE;
	}
#endif

	/* Increase the memory required for storing the IDs */
	psCurrentFnData->puCalledFunctionIDs = DebugMemRealloc(psCurrentFnData->puCalledFunctionIDs,
														   (psCurrentFnData->uNumCalledFunctions + 1) * sizeof(IMG_UINT32));

	if(!psCurrentFnData->puCalledFunctionIDs)
	{
		return IMG_FALSE;
	}

	/* Add this ID to the list of the called functions */
	psCurrentFnData->puCalledFunctionIDs[psCurrentFnData->uNumCalledFunctions] = uCalledFunctionSymbolTableID;

	/* Increase number of called functions */
	psCurrentFnData->uNumCalledFunctions++;

	/* Store some information about how the function is called */
	if (psCurrentFnData->eFunctionType == GLSLFT_USER)
	{
		if (psGLSLTreeContext->uLoopLevel || psGLSLTreeContext->uConditionLevel)
		{
			psCalledFunctionData->bCalledFromConditionalBlock = IMG_TRUE;
		}
	}
	/* Store information about any built in functions called */
	else if (psCurrentFnData->eFunctionType == GLSLFT_BUILT_IN)
	{
		psGLSLTreeContext->puBuiltInFunctionsCalled = DebugMemRealloc(psGLSLTreeContext->puBuiltInFunctionsCalled,
																	  sizeof(IMG_UINT32)*(psGLSLTreeContext->uNumBuiltInFunctionsCalled+1));

		if (psGLSLTreeContext->uNumBuiltInFunctionsCalled && !psGLSLTreeContext->puBuiltInFunctionsCalled)
		{
			LOG_INTERNAL_ERROR(("ASTRegisterFunctionCall: Failed to realloc mem for built in functions\n"));
			return IMG_FALSE;
		}

		/* store the symbol ID */
		psGLSLTreeContext->puBuiltInFunctionsCalled[psGLSLTreeContext->uNumBuiltInFunctionsCalled] = uCalledFunctionSymbolTableID;
		
		/* Increase the count */
		psGLSLTreeContext->uNumBuiltInFunctionsCalled++;
	}


#ifdef DEBUG
	/* Quick sanity check */
	DebugAssert(GLSLBuiltInFunctionID(psCalledFunctionData->eBuiltInFunctionID) == psCalledFunctionData->eBuiltInFunctionID);
	if (psCalledFunctionData->eBuiltInFunctionID > GLSLBFID_NOT_BUILT_IN)
	{
		LOG_INTERNAL_ERROR(("ASTRegisterFunctionCall: Called function ('%s') build in ID, not valid\n", psCalledFunctionData->pszOriginalFunctionName));
		psCalledFunctionData->eBuiltInFunctionID = GLSLBFID_NOT_BUILT_IN;
	}
#endif

	/* Indicated calls to functions that use gradient calculations */
	psCurrentFnData->bGradientFnCalled = GLSLBuiltInFunctionUseGradients(psCalledFunctionData->eBuiltInFunctionID);
	
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTCreateFunctionNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateFunctionNode(GLSLTreeContext  *psGLSLTreeContext,
								ASTFunctionState *psFunctionState,
								IMG_BOOL          bPrototype)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_CHAR                    *pszFunctionName, *pszHashedFunctionName;
	IMG_UINT32                  uSymbolTableID = 0, i;
	IMG_BOOL                    bFoundSymbol = IMG_FALSE, bMainFunction = IMG_FALSE;
	GLSLNode                    *psFunctionDefinitionNode = IMG_NULL;
	GLSLFunctionDefinitionData  *psFunctionDefinitionData = IMG_NULL;
	GLSLFullySpecifiedType      *psFullySpecifiedTypes;
	ASTFunctionStateParam		*psParameter;

	psFullySpecifiedTypes = DebugMemAlloc(sizeof(GLSLFullySpecifiedType) * psFunctionState->uNumParameters);

	if(psFunctionState->uNumParameters && !psFullySpecifiedTypes)
	{
		return IMG_NULL;
	}

	/* Validate supplied entry */
	ASTValidateInputEntry(psFunctionState->psIDENTIFIEREntry, SYMGLSL_IDENTIFIER);

	pszFunctionName = ParseTreeToken(psFunctionState->psIDENTIFIEREntry)->pvData;

	/* Check main function for errors */
	if (strcmp(pszFunctionName, "main") == 0)
	{
		if (psFunctionState->uNumParameters)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry,
									 "'%s' : function cannot take any parameter(s)\n",
									 pszFunctionName);

			psFunctionState->uNumParameters = 0;
		}

		if (psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
									 "'%s' :  main function cannot return a value\n",
									 GLSLTypeSpecifierFullDescTable(psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier));
		}

		bMainFunction = IMG_TRUE;
	}

	/* build up function name based on parameter types */
	for (i = 0, psParameter = psFunctionState->psParameters; i < psFunctionState->uNumParameters; i++, psParameter = psParameter->psNext)
	{
		psFullySpecifiedTypes[i] = psParameter->sParameterData.sFullySpecifiedType;
	}
	
	/* Get a hashed name for the function */
	pszHashedFunctionName = ASTSemCreateHashedFunctionName(psGLSLTreeContext->psSymbolTable,
														  pszFunctionName,
														  psFunctionState->uNumParameters,
														  psFullySpecifiedTypes);

	DebugMemFree(psFullySpecifiedTypes);

	/* Check to see if this symbol exists already */
	if (FindSymbol(psGLSLTreeContext->psSymbolTable, pszHashedFunctionName, &uSymbolTableID, IMG_FALSE))
	{
		psFunctionDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uSymbolTableID);

#ifdef DEBUG
		if (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)
		{
			LOG_INTERNAL_ERROR(("ASTCreateFunctionNode: Name already exists but type is different\n"));
			
			return IMG_NULL;
		}
#endif

		/* If it was a built in function then replace it */
		if (psFunctionDefinitionData->eFunctionType != GLSLFT_BUILT_IN)
		{
			bFoundSymbol = IMG_TRUE;
		}
		else
		{
			/* Check that the return types match */
			if (!ASTSemCheckTypeSpecifiersMatch(&psFunctionDefinitionData->sReturnFullySpecifiedType,
												&psFunctionState->sReturnData.sFullySpecifiedType))
			{
				/* Error message not quite right, should report name of return type in error */
				LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
										 "'%s' : overloaded functions must have the same return type\n",
										 GLSLTypeSpecifierFullDescTable(psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier));
			}
		}
	}

	/* Add it to the symbol table if it doesn't already exist */
	if (!bFoundSymbol)
	{
		GLSLFunctionDefinitionData sFunctionDefinitionData;

		IMG_CHAR *pszReturnValueName = DebugMemAlloc(strlen(pszHashedFunctionName) + 12);

		if(!pszReturnValueName)
		{
			return IMG_NULL;
		}

		sprintf(pszReturnValueName, RETURN_VAL_STRING, pszHashedFunctionName);

		/* Add the data for the return value to the symbol table */
		if (FindSymbol(psGLSLTreeContext->psSymbolTable, pszReturnValueName, &(sFunctionDefinitionData.uReturnDataSymbolID), IMG_TRUE))
		{
			LOG_INTERNAL_PARSE_TREE_ERROR((psFunctionState->psIDENTIFIEREntry,"ASTCreateFunctionNode: %s already exists (don't think this should ever happen)\n", 
									  pszReturnValueName));

			return IMG_NULL;
		}
		else
		{
			/* Add the data to the symbol table */
			if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable,
							   pszReturnValueName,
							   &(psFunctionState->sReturnData),
							   IMG_FALSE,
							   &(sFunctionDefinitionData.uReturnDataSymbolID)))
			{
				LOG_INTERNAL_ERROR(("ASTCreateFunctionNode: Failed to add return value %s to symbol table", pszReturnValueName));

				return IMG_NULL;
			}
		}

		DebugMemFree(pszReturnValueName);

		sFunctionDefinitionData.eSymbolTableDataType        = GLSLSTDT_FUNCTION_DEFINITION;
		sFunctionDefinitionData.pszOriginalFunctionName     = pszFunctionName;
		sFunctionDefinitionData.eFunctionType               = GLSLFT_USER;
		sFunctionDefinitionData.eFunctionFlags              = GLSLFF_VALID_IN_ALL_CASES;
		sFunctionDefinitionData.bPrototype                  = bPrototype;
		sFunctionDefinitionData.uFunctionCalledCount        = 0;
		sFunctionDefinitionData.uMaxFunctionCallDepth       = 0;
		sFunctionDefinitionData.puCalledFunctionIDs         = IMG_NULL;
		sFunctionDefinitionData.uNumCalledFunctions         = 0;
		sFunctionDefinitionData.bGradientFnCalled           = IMG_FALSE;
		sFunctionDefinitionData.bCalledFromConditionalBlock = IMG_FALSE;
		sFunctionDefinitionData.sReturnFullySpecifiedType   = psFunctionState->sReturnData.sFullySpecifiedType;
		sFunctionDefinitionData.uNumParameters              = psFunctionState->uNumParameters;
		sFunctionDefinitionData.eBuiltInFunctionID          = GLSLBFID_NOT_BUILT_IN;

		sFunctionDefinitionData.puParameterSymbolTableIDs   = DebugMemAlloc(psFunctionState->uNumParameters * sizeof(IMG_UINT32));

		if(psFunctionState->uNumParameters && !sFunctionDefinitionData.puParameterSymbolTableIDs)
		{
			return IMG_NULL;
		}

		sFunctionDefinitionData.psFullySpecifiedTypes       = DebugMemAlloc(psFunctionState->uNumParameters * sizeof(GLSLFullySpecifiedType));
		
		if(psFunctionState->uNumParameters && !sFunctionDefinitionData.psFullySpecifiedTypes)
		{
			DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
			return IMG_NULL;
		}

		for (i = 0, psParameter = psFunctionState->psParameters; i < psFunctionState->uNumParameters; i++, psParameter = psParameter->psNext)
		{
			sFunctionDefinitionData.puParameterSymbolTableIDs[i] = 0;
			sFunctionDefinitionData.psFullySpecifiedTypes[i]     =  psParameter->sParameterData.sFullySpecifiedType; 
		}

		/* Add the function definition to the symbol table */
		if (!AddFunctionDefinitionData(psCPD, psGLSLTreeContext->psSymbolTable,
									   pszHashedFunctionName,
									   &sFunctionDefinitionData,
									   IMG_FALSE,
									   &uSymbolTableID))
		{
			LOG_INTERNAL_ERROR(("ASTCreateFunctionNode: Failed to add function %s to symbol table", pszHashedFunctionName));
	
			DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
			DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);
			return IMG_NULL;
		}

		DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
		DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);

		/* Fetch the data */
		psFunctionDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uSymbolTableID);

#ifdef DEBUG
		if (!psFunctionDefinitionData)
		{
			LOG_INTERNAL_PARSE_TREE_ERROR((psFunctionState->psIDENTIFIEREntry,"ASTCreateFunctionNode: Could not get function definition data from symbol table (symbol ID = %08x)\n",
									  uSymbolTableID));
			
			return IMG_NULL;
		}
#endif
	}
	else
	{
		/* Check if previous declaration was  a prototype */
		if (psFunctionDefinitionData->bPrototype)
		{
			/* Check that the return types match */
			if (!ASTSemCheckTypeSpecifiersMatch(&psFunctionDefinitionData->sReturnFullySpecifiedType,
												&psFunctionState->sReturnData.sFullySpecifiedType))
			{
				/* Error message not quite right, should report name of return type in error */
				LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
										 "'%s' : overloaded function must have the same return type\n",
										 GLSLTypeSpecifierFullDescTable(psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier));
			}

			/* Check that the parameter qualifiers match */
			for (i = 0, psParameter = psFunctionState->psParameters; i < psFunctionState->uNumParameters; i++, psParameter = psParameter->psNext)
			{
				if (psFunctionDefinitionData->psFullySpecifiedTypes[i].eParameterQualifier != 
					psParameter->sParameterData.sFullySpecifiedType.eParameterQualifier)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
											 "'%s' : overloaded functions must have the same parameter qualifiers\n",
											 GLSLParameterQualifierFullDescTable[psParameter->sParameterData.sFullySpecifiedType.eParameterQualifier]);
				}

				if (psFunctionDefinitionData->psFullySpecifiedTypes[i].eTypeQualifier != 
					psParameter->sParameterData.sFullySpecifiedType.eTypeQualifier)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
											 "'%s' : overloaded functions must have the same type qualifiers\n",
											 GLSLTypeQualifierFullDescTable(psParameter->sParameterData.sFullySpecifiedType.eTypeQualifier));
				}
			}

			/* Update it's status */
			psFunctionDefinitionData->bPrototype = bPrototype;
		}
		else
		{
			if (!bPrototype)
			{
				LogProgramParseTreeError(psCPD->psErrorLog,  
										 psFunctionState->psIDENTIFIEREntry,
										 "'%s' : function already has a body\n", 
										 pszFunctionName);
			}
		}
	}

	/* Add the parameters to the symbol table */
	if (!bPrototype)
	{
		/* 
		   Increase the scope level here so that the function parameters will get added at the same level
		   as the compound statement that follows it, the scope level gets decreased once the compound statement 
		   has been processed.
		*/
		ASTIncreaseScope(psGLSLTreeContext);

		for (i = 0, psParameter = psFunctionState->psParameters; i < psFunctionState->uNumParameters; i++, psParameter = psParameter->psNext)
		{
			ParseTreeEntry *psParamIDENTIFIEREntry = psParameter->psParamIDENTIFIEREntry;

			IMG_CHAR *pszParameterName;

			if (!psParamIDENTIFIEREntry)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry,
										 "'%s' : expected formal parameter list, not a type list\n",
										 pszFunctionName);

				psFunctionDefinitionData->puParameterSymbolTableIDs[i] = 0;
			}
			else
			{
				pszParameterName = (IMG_CHAR *)(ParseTreeToken(psParamIDENTIFIEREntry)->pvData);
				
				if (FindSymbol(psGLSLTreeContext->psSymbolTable,
							   pszParameterName,
							   &(psFunctionDefinitionData->puParameterSymbolTableIDs[i]),
							   IMG_TRUE))
				{
					LOG_INTERNAL_PARSE_TREE_ERROR((psParamIDENTIFIEREntry,"ASTCreateFunctionNode: %s already exists (don't think this should ever happen)\n",
											  pszParameterName));
				}
				else
				{
					/* Add the data to the symbol table */
					if (!AddParameterData(psCPD, psGLSLTreeContext->psSymbolTable,
										  pszParameterName,
										  &(psParameter->sParameterData),
										  IMG_FALSE,
										  &(psFunctionDefinitionData->puParameterSymbolTableIDs[i])))
					{
						LOG_INTERNAL_ERROR(("ASTCreateFunctionNode: Failed to add parameter %s to symbol table", pszParameterName));
						return IMG_NULL;
					}
				}
			}
			
		}

		/* Create a new function definition node */
		psFunctionDefinitionNode = ASTCreateNewNode(GLSLNT_FUNCTION_DEFINITION, psFunctionState->psIDENTIFIEREntry);

		/* Check allocation went OK */
		ASTValidateNodeCreation(psFunctionDefinitionNode);

		psFunctionDefinitionNode->uSymbolTableID = uSymbolTableID;

		/* If this was the main function then store the symbol ID of it */
		if (bMainFunction)
		{
			psGLSLTreeContext->psMainFunctionNode = psFunctionDefinitionNode;
		}

	}
	
	DebugMemFree(pszHashedFunctionName);
	

	return psFunctionDefinitionNode;
}



/******************************************************************************
 * Function Name: ASTCreateSwizzleNode
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static GLSLNode *ASTCreateSwizzleNode(GLSLTreeContext *psGLSLTreeContext,
							   ParseTreeEntry  *psIDENTIFIEREntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uSymbolTableID;

	GLSLNode *psIDENTIFIERNode;

	IMG_CHAR *pszFieldName;

	IMG_CHAR acSymbolName[15];

	IMG_UINT32 i;

	GLSLSwizzleData sSwizzleData;

#define NUM_FIELD_SETS 3
	const IMG_CHAR acFieldSets[NUM_FIELD_SETS][4] = 
	{
		{'x', 'y', 'z', 'w'},
		{'r', 'g', 'b', 'a'},
		{'s', 't', 'p', 'q'},
	};
	IMG_UINT32 uSet, uField;
	IMG_UINT32 uSetsUsed;

	/* Validate supplied entry */
	ASTValidateInputEntry(psIDENTIFIEREntry, SYMGLSL_IDENTIFIER);

	/* Create the IDENTIFIER Node */
	psIDENTIFIERNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psIDENTIFIEREntry);

	/* Check creation went OK */
	ASTValidateNodeCreation(psIDENTIFIERNode);

	pszFieldName = ParseTreeToken(psIDENTIFIEREntry)->pvData;

	sSwizzleData.eSymbolTableDataType = GLSLSTDT_SWIZZLE;
	sSwizzleData.uNumComponents = 0;

	/* set all components to 0 */
	for (i = 0; i < 4; i++)
	{
		sSwizzleData.uComponentIndex[i] = 0;
	}

	uSetsUsed = 0;
	for (i = 0; i < strlen(pszFieldName); i++)
	{
		if (i >= 4)
		{
			LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : illegal vector field selection\n", pszFieldName);
			psIDENTIFIERNode->eNodeType = GLSLNT_ERROR;
			return psIDENTIFIERNode;
		}

		for(uSet = 0; uSet < NUM_FIELD_SETS; uSet++)
		{
			for(uField = 0; uField < 4; uField++)
			{
				if(pszFieldName[i] == acFieldSets[uSet][uField])
				{
					sSwizzleData.uComponentIndex[i] = uField;
					acSymbolName[i] = (IMG_CHAR)('x' + uField);
					sSwizzleData.uNumComponents++;
					uSetsUsed |= 1 << uSet;
					if(uSetsUsed & (uSetsUsed - 1))/* Check if more than 1 set has been used. */
					{
						LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : vector field components do not come from the same set\n", pszFieldName);
						psIDENTIFIERNode->eNodeType = GLSLNT_ERROR;
						return psIDENTIFIERNode;
					}

					goto NextField;
				}
			}
		} /* for(uSet = 0; uSet < NUM_FIELD_SETS; uSet++) */

		/* Field was not found. */
		LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : illegal vector field selection\n", pszFieldName);
		psIDENTIFIERNode->eNodeType = GLSLNT_ERROR;
		return psIDENTIFIERNode;
NextField:;
	}

	sprintf(&(acSymbolName[sSwizzleData.uNumComponents]), "_@swizzle");

	if (!AddSwizzleData(psCPD, psGLSLTreeContext->psSymbolTable,
						acSymbolName,
						&sSwizzleData,
						IMG_TRUE,
						&uSymbolTableID))
	{
		LOG_INTERNAL_NODE_ERROR((psIDENTIFIERNode,"ASTCreateSwizzleNode: Failed to add swizzle %s to symbol table", acSymbolName));

		return IMG_NULL;
	}

	psIDENTIFIERNode->uSymbolTableID = uSymbolTableID;

	return psIDENTIFIERNode;
}

/******************************************************************************
 * Function Name: ASTProcessFieldSelection
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTProcessFieldSelection(GLSLTreeContext   *psGLSLTreeContext,
								   ParseTreeEntry    *psFieldSelectionEntry,
								   GLSLNode          *psLeftNode)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	ParseTreeEntry *psIDENTIFIEREntry;

	IMG_CHAR *pszFieldName;

	GLSLFullySpecifiedType sFullySpecifiedType;

	ASTValidateInputEntry(psFieldSelectionEntry, SYMGLSL_field_selection);

	/*
	  (NOT PART OF THE ORIGINAL GRAMMAR)

	  field_selection:
	      IDENTIFIER
	*/

	psIDENTIFIEREntry = RULE1(psFieldSelectionEntry);

	pszFieldName = (IMG_CHAR *)ParseTreeToken(psIDENTIFIEREntry)->pvData;

	/*
	   We must examine the left node (which has already been set up)
	   to decide if this field selection is a swizzle or the selection
	   of a member within a structure 
	*/

	if (psLeftNode->eNodeType == GLSLNT_ERROR)
	{
		GLSLNode *psMemberSelectionNode;

		LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
								 "'%s' : field selection requires structure, vector, or matrix on left hand side\n",
								 ParseTreeToken(psIDENTIFIEREntry)->pvData);

		/* Create member selection node */
		psMemberSelectionNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psIDENTIFIEREntry);

		/* Check allocation went OK */
		ASTValidateNodeCreation(psMemberSelectionNode);

		psMemberSelectionNode->eNodeType = GLSLNT_ERROR;

		return psMemberSelectionNode;
	}

	/* Get type of the left node */
	if (!GetSymbolInfo(psCPD, psGLSLTreeContext->psSymbolTable,
					   psLeftNode->uSymbolTableID,
					   psGLSLTreeContext->eProgramType,
					   &sFullySpecifiedType,
					   IMG_NULL,
					   IMG_NULL,
					   IMG_NULL,
					   IMG_NULL,
					   IMG_NULL,
					   IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ASTProcessFieldSelection: Failed to retreive symbol data \n"));

		return IMG_NULL;
	}

	if (sFullySpecifiedType.eTypeSpecifier == GLSLTS_STRUCT)
	{
		GLSLNode *psMemberSelectionNode;

		GLSLMemberSelectionData sMemberSelection;

		IMG_UINT32 i;

		GLSLStructureDefinitionData *psStructureDefinitionData;

		IMG_CHAR *pszHashedMemberSelectName, *pszStructName;

#ifdef DEBUG
		if (!sFullySpecifiedType.uStructDescSymbolTableID)
		{
			LOG_INTERNAL_PARSE_TREE_ERROR((psFieldSelectionEntry,"ASTProcessFieldSelection: No structure symbol ID found\n"));
			return IMG_NULL;
		}
#endif

		/* Fetch the structure definition */
		psStructureDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, sFullySpecifiedType.uStructDescSymbolTableID);

#ifdef DEBUG
		/* Check we found it */
		if (!psStructureDefinitionData)
		{
			LOG_INTERNAL_PARSE_TREE_ERROR((psFieldSelectionEntry,"ASTProcessFieldSelection: Failed to retrive structure data \n"));
			return IMG_NULL;
		}

		/* Check it was correct type */
		if (psStructureDefinitionData->eSymbolTableDataType != GLSLSTDT_STRUCTURE_DEFINITION)
		{
			LOG_INTERNAL_PARSE_TREE_ERROR((psFieldSelectionEntry,"ASTProcessFieldSelection: Retrieved data for '%s' was not a structure (%08X)\n",
									  GetSymbolName(psGLSLTreeContext->psSymbolTable, psLeftNode->uSymbolTableID),
									  psStructureDefinitionData->eSymbolTableDataType));
			return IMG_NULL;
		}
#endif

		/* Search through this structure for the member */
		for (i = 0; i <  psStructureDefinitionData->uNumMembers; i++)
		{
			if (strcmp(psStructureDefinitionData->psMembers[i].pszMemberName, pszFieldName) == 0)
			{
				/* Found, Selected Member is 'i' */
				break;
			}
		}

		/* Check we found the member */
		if (i == psStructureDefinitionData->uNumMembers)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
									 "'%s' : no such field in structure\n",
									 pszFieldName);
			return IMG_NULL;
		}

		/* Create member selection node */
		psMemberSelectionNode = ASTCreateNewNode(GLSLNT_IDENTIFIER, psIDENTIFIEREntry);

		/* Check allocation went OK */
		ASTValidateNodeCreation(psMemberSelectionNode);

		/* Set up member selection data type */
		sMemberSelection.eSymbolTableDataType = GLSLSTDT_MEMBER_SELECTION;
		sMemberSelection.uMemberOffset = i;
		sMemberSelection.uStructureInstanceSymbolTableID = psLeftNode->uSymbolTableID;

		/* Get the name of the structure */
		pszStructName = GetSymbolName(psGLSLTreeContext->psSymbolTable, psLeftNode->uSymbolTableID);

		/* Allocate memory the member name */
		pszHashedMemberSelectName = DebugMemAlloc(strlen(pszStructName) + strlen(pszFieldName) + 10);

		if(!pszHashedMemberSelectName)
		{
			return IMG_NULL;
		}

		/* Create a name for this member selection */
		sprintf(pszHashedMemberSelectName, "struct_%s@%s", pszStructName, pszFieldName);

		/* Add this data to the symbol table */
		AddMemberSelectionData(psCPD, psGLSLTreeContext->psSymbolTable,
							   pszHashedMemberSelectName,
							   &sMemberSelection,
							   IMG_TRUE,
							   &(psMemberSelectionNode->uSymbolTableID));

		DebugMemFree(pszHashedMemberSelectName);

		return psMemberSelectionNode;

	}
	else
	{
		return ASTCreateSwizzleNode(psGLSLTreeContext, psIDENTIFIEREntry);
	}
}

/******************************************************************************
 * Function Name: ASTUpdateInvariantStatus
 *
 * Inputs       : psGLSLTreeContext, psIDENTIFIEREntry
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTUpdateInvariantStatus(GLSLTreeContext *psGLSLTreeContext,
								  ParseTreeEntry  *psIDENTIFIEREntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_CHAR *pszIdentifierName;
	IMG_UINT32 ui32SymbolTableID;

	pszIdentifierName = (IMG_CHAR *)ParseTreeToken(psIDENTIFIEREntry)->pvData;

	if (!FindSymbol(psGLSLTreeContext->psSymbolTable, 
					pszIdentifierName, 
					&ui32SymbolTableID, 
					IMG_FALSE))
	{
		LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : undeclared identifer\n", pszIdentifierName);
		return IMG_FALSE;
	}
	else
	{
		GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
																ui32SymbolTableID);
		
		if(psIdentifierData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_VERTEX_OUT)
		{
			psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags |= GLSLVMOD_INVARIANT;
		}
		else if(psIdentifierData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_FRAGMENT_IN)
		{
			LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : invariant status can only be modified in a vertex shader\n", pszIdentifierName);
			return IMG_FALSE;
		}
		else
		{
			LogProgramParseTreeError(psCPD->psErrorLog,  psIDENTIFIEREntry, "'%s' : only varyings can be made invariant\n", pszIdentifierName);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

int glsl_parse (ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext);

extern const IMG_UINT32 guSupportedGLSLVersions[GLSL_NUM_VERSIONS_SUPPORTED];
/******************************************************************************
 * Function Name: ASTProcessLanguageVersion
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTProcessLanguageVersion(GLSLTreeContext *psGLSLTreeContext,
									ParseTreeEntry  *psLanguageVersionEntry)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uData, u;

	/* Validate supplied entry */
	ASTValidateInputEntry(psLanguageVersionEntry, SYMGLSL_language_version);

	/*
	  - Special rule: Use to be able to change the version of the language mid compilation

	  language_version:
	      LANGUAGE_VERSION
	*/

	/* Hack: piggyback on pszStartOfLine to load the language version */
	/**
	 * OGL64 Review.
	 * ?
	 */
	uData = (IMG_UINT32)(IMG_UINTPTR_T)ParseTreeToken(RULE1(psLanguageVersionEntry))->pszStartOfLine;

	for(u = 0; u < GLSL_NUM_VERSIONS_SUPPORTED; u++)
	{
		if(uData == guSupportedGLSLVersions[u])
			break;
	}

	if(u == GLSL_NUM_VERSIONS_SUPPORTED)
	{
		LOG_INTERNAL_ERROR(("ASTProcessLanguageVersion: Incorrect language version (%d)",uData));
	}

	/* Update the supported language version */
	psGLSLTreeContext->uSupportedLanguageVersion = uData;
	
	return IMG_NULL;
}

#ifdef DUMP_LOGFILES

/******************************************************************************
 * Function Name: DumpAbstractSyntaxTree
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID DumpAbstractSyntaxTree(GLSLNode *psASTree, IMG_UINT32 uIndent)
{
	IMG_UINT32 i; 

	DumpLogMessage(LOGFILE_ASTREE, uIndent, "%s\n", NODETYPE_DESC(psASTree->eNodeType));

	for (i = 0; i < psASTree->uNumChildren; i++)
	{
		if (psASTree->ppsChildren[i])
		{
			DumpAbstractSyntaxTree(psASTree->ppsChildren[i], uIndent + 1);
		}
	}

}

#endif


#if 0
IMG_UINT32 uExpressionCount = 0;
IMG_UINT32 uExpressionBranchCount = 0;
IMG_UINT32 uExpressionTokenCount = 0;

IMG_VOID CountExpressionBranches(ParseTreeEntry *psEntry,
								 IMG_BOOL        bProcessingExpression,
								 IMG_BOOL        bDisplayStats)
{
	while (psEntry)
	{
		if (psEntry->uSyntaxSymbolName == SYMGLSL_expression)
		{
			uExpressionCount++;
			bProcessingExpression = IMG_TRUE;
		}

		if (bProcessingExpression)
		{
			uExpressionBranchCount++;

			if (psEntry->psToken)
			{
				uExpressionTokenCount++;
			}
		}

		if (psEntry->psParentRule && psEntry->psPrevRule)
		{
			printf("Foundone!\n");
		}
		
		if (psEntry->psChildRule)
		{
			CountExpressionBranches(psEntry->psChildRule, bProcessingExpression, IMG_FALSE);
		}
		
		psEntry = psEntry->psNextRule;
	}
	
	if (bDisplayStats)
	{
		printf("Expression count was %lu, branches = %lu, expression token count = %lu\n", 
			   uExpressionCount, uExpressionBranchCount, 
			   uExpressionTokenCount);
	}
}
#endif





/******************************************************************************
 * Function Name: CreateGLSLTreeContext
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLTreeContext *CreateGLSLTreeContext(ParseContext            *psParseContext,
									   SymTable                *psSymbolTable,
									   GLSLProgramType          eProgramType,
									   GLSLCompilerWarnings     eEnabledWarnings,
									   GLSLInitCompilerContext *psInitCompilerContext)
{
	GLSLTreeContext         *psGLSLTreeContext = DebugMemCalloc(sizeof(GLSLTreeContext));
	GLSLPreProcessorData    *psData = (GLSLPreProcessorData *)psParseContext->pvPreProcessorData;
	GLSLRequestedPrecisions *psRequestedPrecisions = &(psInitCompilerContext->sRequestedPrecisions);
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;
	
	if (!psGLSLTreeContext)
	{
		return IMG_NULL;
	}

	/* Fill out context */
	psGLSLTreeContext->uNumResults                        = 0;
	psGLSLTreeContext->uNumUnnamedStructures              = 0;
	psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID = 0;
	psGLSLTreeContext->uLoopLevel                         = 0;
	psGLSLTreeContext->uConditionLevel                    = 0;
	psGLSLTreeContext->eProgramType                       = eProgramType;
	psGLSLTreeContext->psInitCompilerContext              = psInitCompilerContext;
	psGLSLTreeContext->psParseContext                     = psParseContext;
	psGLSLTreeContext->psSymbolTable                      = psSymbolTable;
	psGLSLTreeContext->eBuiltInsWrittenTo                 = (GLSLBuiltInVariableWrittenTo)0;
	psGLSLTreeContext->puBuiltInFunctionsCalled           = IMG_NULL;
	psGLSLTreeContext->uNumBuiltInFunctionsCalled         = 0;
	psGLSLTreeContext->psMainFunctionNode                 = IMG_NULL;
	psGLSLTreeContext->bDiscardExecuted                   = IMG_FALSE;
	psGLSLTreeContext->eEnabledWarnings                   = eEnabledWarnings;
	psGLSLTreeContext->uSupportedLanguageVersion          = GLSL_DEFAULT_VERSION_SUPPORT;
	
	/* Set default levels of precision */
	if (eProgramType == GLSLPT_VERTEX)
	{
		psGLSLTreeContext->eDefaultFloatPrecision     = psRequestedPrecisions->eDefaultUserVertFloat;
		psGLSLTreeContext->eDefaultIntPrecision       = psRequestedPrecisions->eDefaultUserVertInt;
		psGLSLTreeContext->eDefaultSamplerPrecision   = psRequestedPrecisions->eDefaultUserVertSampler;
		psGLSLTreeContext->eForceUserFloatPrecision   = psRequestedPrecisions->eForceUserVertFloat;
		psGLSLTreeContext->eForceUserIntPrecision     = psRequestedPrecisions->eForceUserVertInt;
		psGLSLTreeContext->eForceUserSamplerPrecision = psRequestedPrecisions->eForceUserVertSampler;

		psGLSLTreeContext->psBuiltInsReferenced       = &(psCPD->sVertexBuiltInsReferenced);
	}
	else
	{
		psGLSLTreeContext->eDefaultFloatPrecision     = psRequestedPrecisions->eDefaultUserFragFloat;
		psGLSLTreeContext->eDefaultIntPrecision       = psRequestedPrecisions->eDefaultUserFragInt;
		psGLSLTreeContext->eDefaultSamplerPrecision   = psRequestedPrecisions->eDefaultUserFragSampler;
		psGLSLTreeContext->eForceUserFloatPrecision   = psRequestedPrecisions->eForceUserFragFloat;
		psGLSLTreeContext->eForceUserIntPrecision     = psRequestedPrecisions->eForceUserFragInt;
		psGLSLTreeContext->eForceUserSamplerPrecision = psRequestedPrecisions->eForceUserFragSampler;

		psGLSLTreeContext->psBuiltInsReferenced       = &(psCPD->sFragmentBuiltInsReferenced);
	}

	/* Add these to the symbol table at the very highest scope level */
	ModifyDefaultPrecision(psGLSLTreeContext, psGLSLTreeContext->eDefaultFloatPrecision, GLSLTS_FLOAT);
	ModifyDefaultPrecision(psGLSLTreeContext, psGLSLTreeContext->eDefaultIntPrecision,   GLSLTS_INT);
	ModifyDefaultPrecision(psGLSLTreeContext, psGLSLTreeContext->eDefaultSamplerPrecision, GLSLTS_SAMPLER2D);

	/*
	   Make sure all lookups will get values directly from context (instead of symbol table) until
	   the precision is modified by the shader - this is to prevent unecessary symbol table searches
	*/
	psGLSLTreeContext->bShaderHasModifiedPrecision        = IMG_FALSE;

	if (psData)
	{
		psGLSLTreeContext->eEnabledExtensions             = psData->eEnabledExtensions;
	}

	DebugAssert(psParseContext);
	glsl_parse(psParseContext, psGLSLTreeContext);

	/*DecodeNode(psGLSLTreeContext, psGLSLTreeContext->psAbstractSyntaxTree);

	CountExpressionBranches(psGLSLTreeContext->psParseContext->psParseTree, IMG_FALSE, IMG_TRUE); */

	return psGLSLTreeContext;
}


/******************************************************************************
 * Function Name: CheckWhichFunctionsCalled
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL CheckWhichFunctionsCalled(GLSLTreeContext  *psGLSLTreeContext,
								   IMG_UINT32        uSymbolID,
								   IMG_BOOL          bCalledFromConditionalBlock,
								   IMG_UINT32       *puDepth)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i;

	GLSLFunctionDefinitionData *psFunctionDefinitionData;

	if (*puDepth > 1024)
	{
		LogProgramError(psCPD->psErrorLog, "Function call depth > 1024 detected, probable recursion.\n");
		return IMG_FALSE;
	}

	/*
	  If the symbol ID was null then this is probably a constructor called from outside
	  of a function (which is fine) so just move on.
	*/
	if(!uSymbolID)
	{
		return IMG_TRUE;
	}

	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uSymbolID);

#ifdef DEBUG
	if (!psFunctionDefinitionData)
	{
		LOG_INTERNAL_ERROR(("CheckIfFunctionCalled: Symbol was not found\n"));
		return IMG_FALSE;
	}
	
	if (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)	
	{
		LOG_INTERNAL_ERROR(("CheckIfFunctionCalled: Symbol was not of right type (%08X)\n", psFunctionDefinitionData->eSymbolTableDataType));
		return IMG_FALSE;
	}
#endif
	
	
	if ((psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR) ||
		(psFunctionDefinitionData->eFunctionType == GLSLFT_USERDEFINED_CONSTRUCTOR))
	{
		return IMG_TRUE;
	}

	/* Increase the count of this function */
	psFunctionDefinitionData->uFunctionCalledCount++;

	/* If called from inside conditional block update it's status */
	if (bCalledFromConditionalBlock)
	{
		psFunctionDefinitionData->bCalledFromConditionalBlock = IMG_TRUE;
	}

	/* Increase maximum call depth of this function if required */
	if (*puDepth > psFunctionDefinitionData->uMaxFunctionCallDepth)
	{
		psFunctionDefinitionData->uMaxFunctionCallDepth = *puDepth;
	}

	(*puDepth)++;

	/* Increase the count of all functions called by this one */
	for (i = 0; i < psFunctionDefinitionData->uNumCalledFunctions; i++)
	{
		/* Recursively check which functions this one calls is called */
		if (!CheckWhichFunctionsCalled(psGLSLTreeContext,
									   psFunctionDefinitionData->puCalledFunctionIDs[i],
									   psFunctionDefinitionData->bCalledFromConditionalBlock,
									   puDepth))
		{
			return IMG_FALSE;
		}
	}

	(*puDepth)--;


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ReOrderTopLevelNodes
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Does a bubble sort to reorder the top level nodes in this order:
 *                declarations then functions in decreasing call depth order. This
 *                allows better/simpler decision making about what to do with functions
 *                at the IC generation level.
 *****************************************************************************/
static IMG_BOOL ReOrderTopLevelNodes(GLSLTreeContext *psGLSLTreeContext)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_BOOL bSwapped;

	GLSLNode *psTopLevelNode = psGLSLTreeContext->psAbstractSyntaxTree;

	IMG_UINT32  i;

	do
	{
		bSwapped = IMG_FALSE;

		for (i = 1; i < psTopLevelNode->uNumChildren; i++)
		{
			GLSLNode *psPrevNode = psTopLevelNode->ppsChildren[i-1];
			GLSLNode *psCurrentNode = psTopLevelNode->ppsChildren[i];

			/* No need to swap */
			if (psPrevNode->eNodeType == GLSLNT_DECLARATION)
			{
				continue;
			}
			/* Swap function def for declaration */
			else if (psCurrentNode->eNodeType == GLSLNT_DECLARATION)
			{
				/* Swap the nodes */
				psTopLevelNode->ppsChildren[i-1] = psCurrentNode;
				psTopLevelNode->ppsChildren[i]   = psPrevNode;
				bSwapped = IMG_TRUE;
				continue;
			}
			else if (psCurrentNode->eNodeType == GLSLNT_FUNCTION_DEFINITION && 
					 psPrevNode->eNodeType == GLSLNT_FUNCTION_DEFINITION)
			{
				GLSLFunctionDefinitionData *psCurrentFunctionDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
																								 psCurrentNode->uSymbolTableID);
				GLSLFunctionDefinitionData *psPrevFunctionDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
																								 psPrevNode->uSymbolTableID);

#ifdef DEBUG
				if (!psCurrentFunctionDefinitionData ||
					!psPrevFunctionDefinitionData    ||
					psCurrentFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION ||
					psPrevFunctionDefinitionData->eSymbolTableDataType    != GLSLSTDT_FUNCTION_DEFINITION)
				{
					LOG_INTERNAL_ERROR(("ReOrderTopLevelNodes: Failed to get function definition data\n"));
					return IMG_FALSE;
				}
#endif

				/*
				   Does the current function have a higher call depth than the previous one
				   Or is the current function main()? Then swap them
				*/
				if ((psCurrentFunctionDefinitionData->uMaxFunctionCallDepth > psPrevFunctionDefinitionData->uMaxFunctionCallDepth) ||
					(psPrevNode == psGLSLTreeContext->psMainFunctionNode))
				{
					/* Then swap them */
					psTopLevelNode->ppsChildren[i-1] = psCurrentNode;
					psTopLevelNode->ppsChildren[i]   = psPrevNode;
					bSwapped = IMG_TRUE;
					continue;
				}
			}
			else
			{
				LOG_INTERNAL_ERROR(("ReOrderTopLevelNodes: Invalid node type found at top level\n"));
				return IMG_FALSE;
			}
		}
	} while (bSwapped);

#ifdef DUMP_LOGFILES
	for (i = 0; i < psTopLevelNode->uNumChildren; i++)
	{
		GLSLNode *psCurrentNode = psTopLevelNode->ppsChildren[i];

		if (psCurrentNode->eNodeType == GLSLNT_FUNCTION_DEFINITION)
		{ 
			IMG_UINT32 uSymbolID = psCurrentNode->uSymbolTableID;

			GLSLFunctionDefinitionData *psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																													uSymbolID);

			DumpLogMessage(LOGFILE_PARSETREE, 0, "Node %u - Function '%s' is called %u times (max call depth = %u)\n", 
						   i,
						   GetSymbolName(psGLSLTreeContext->psSymbolTable, uSymbolID),
						   psFunctionDefinitionData->uFunctionCalledCount,
						   psFunctionDefinitionData->uMaxFunctionCallDepth);
		}
	}
#endif


	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: CheckGLSLTreeCompleteness
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL CheckGLSLTreeCompleteness(GLSLTreeContext *psGLSLTreeContext)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_BOOL bSuccess = IMG_TRUE;
	IMG_UINT32 uSymbolID, uFunctionCallDepth = 0;

	/* Check gl_Position was written to */
	if (psGLSLTreeContext->eProgramType == GLSLPT_VERTEX && 
		!(psGLSLTreeContext->eBuiltInsWrittenTo & GLSLBVWT_POSITION))
	{
		{
			LogProgramError(psCPD->psErrorLog, "gl_Position must be written by all paths through a vertex shader.\n");
			bSuccess = IMG_FALSE;
		}
	}

	if (!(psGLSLTreeContext->psMainFunctionNode))
	{
		LogProgramError(psCPD->psErrorLog, "main() function is missing.\n");
		bSuccess = IMG_FALSE;
	}


#ifdef DUMP_LOGFILES
	DumpAbstractSyntaxTree(psGLSLTreeContext->psAbstractSyntaxTree, 0);
#endif

	/* Reset the GetNext function counters */
	ResetGetNextSymbolCounter(psGLSLTreeContext->psSymbolTable);

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "\n\n-------- Post compilation identifier state -------------\n\n"); 
#endif

	uSymbolID = GetNextSymbol(psGLSLTreeContext->psSymbolTable, IMG_FALSE);

	if (psGLSLTreeContext->psMainFunctionNode)
	{
		/* Work out which functions have been called */
		CheckWhichFunctionsCalled(psGLSLTreeContext, 
								  psGLSLTreeContext->psMainFunctionNode->uSymbolTableID,
								  IMG_FALSE,
								  &uFunctionCallDepth);
	}

	/* Reorder the nodes to make IC Code gen easier */
	ReOrderTopLevelNodes(psGLSLTreeContext);

	/* Look through all the global symbols */
	while (uSymbolID)
	{
		GLSLGenericData *psGenericData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uSymbolID);

#ifdef DUMP_LOGFILES
		DumpIdentifierInfo(psCPD, psGLSLTreeContext->psSymbolTable, uSymbolID);
#endif

		if (psGenericData->eSymbolTableDataType == GLSLSTDT_FUNCTION_DEFINITION)
		{
			GLSLFunctionDefinitionData *psFunctionDefinitionData = (GLSLFunctionDefinitionData *)psGenericData;

			if (psFunctionDefinitionData->bPrototype && psFunctionDefinitionData->uFunctionCalledCount)
			{
				LogProgramError(psCPD->psErrorLog, "Function '%s' has no body.\n", psFunctionDefinitionData->pszOriginalFunctionName);
				bSuccess = IMG_FALSE;
			}
		}

		uSymbolID = GetNextSymbol(psGLSLTreeContext->psSymbolTable, IMG_FALSE);
	}

#ifdef DUMP_LOGFILES
	{
		IMG_UINT32 i;

		GLSLIdentifierList *psBuiltInsReferenced = psGLSLTreeContext->psBuiltInsReferenced;


		DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "\nFound %d references to built in variables\n", psBuiltInsReferenced->uNumIdentifiersReferenced);

		for (i = 0; i < psBuiltInsReferenced->uNumIdentifiersReferenced; i++)
		{
			DumpIdentifierInfo(psCPD, psGLSLTreeContext->psSymbolTable, psBuiltInsReferenced->puIdentifiersReferenced[i]);
		}

		DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "\nFound %d references to built in variables\n", psBuiltInsReferenced->uNumIdentifiersReferenced);

	}
#endif

	return bSuccess;
}

/******************************************************************************
 * Function Name: DestroyGLSLTreeContext
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyGLSLTreeContext(GLSLTreeContext *psGLSLTreeContext)
{
	while(psGLSLTreeContext->psNodeList)
	{
		GLSLNode* psNext = (GLSLNode*)psGLSLTreeContext->psNodeList->psNext;

		if (psGLSLTreeContext->psNodeList->uNumChildren)
			DebugMemFree(psGLSLTreeContext->psNodeList->ppsChildren);
		DebugMemFree(psGLSLTreeContext->psNodeList);

		psGLSLTreeContext->psNodeList = psNext;
	}


	DebugMemFree(psGLSLTreeContext->puBuiltInFunctionsCalled);

	DebugMemFree(psGLSLTreeContext);
}

/******************************************************************************
 End of file (glsltree.c)
******************************************************************************/

