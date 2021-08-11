/******************************************************************************
 * Name         : astbuiltin.c
 * Author       : James McCarthy
 * Created      : 16/08/2004
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
 * $Log: astbuiltin.c $
 * 
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "semantic.h"
#include "error.h"
#include "stdio.h"

#ifdef LINUX
#    include "../parser/debug.h"
#else
#    include "..\parser\debug.h"
#endif

#include "string.h"
#include "common.h"
#include "astbuiltin.h"

/*
 * List of identifiers. It holds a list of arbitrary tokens and an index to that list.
 * The indexed tokens are all the identifiers in the token list. See ASTBIBuildIdentifierList.
 */
typedef struct ASTBIIdentifierListTAG
{
	/* List of arbitrary tokens. Ends with TOK_TERMINATEPARSING */
	Token      *psTokenList;

	/* Index into psTokenList containing all the identifiers */
	IMG_UINT32 *puIdentifierIndices;

	/* Number of indices in puIdentifierIndices */
	IMG_UINT32  uNumIdentifierIndices;

} ASTBIIdentifierList;


/******************************************************************************
 * Function Name: ASTBIBuildIdentifierList
 * Inputs       : psTokenList
 * Outputs      : -
 * Returns      : A list of identifiers.
 * Globals Used : -
 * Description  : Generates the list of all identifiers in a list of arbitrary tokens.
 *****************************************************************************/
static ASTBIIdentifierList* ASTBIBuildIdentifierList(GLSLCompilerPrivateData *psCPD, Token *psTokenList)
{
	IMG_UINT32 i = 0;
	ASTBIIdentifierList *psIdentifierList;

	if (!psTokenList)
	{
		return IMG_NULL;
	}

	psIdentifierList = DebugMemAlloc(sizeof(ASTBIIdentifierList));

	if(!psIdentifierList)
	{
		LOG_INTERNAL_ERROR(("ASTBIBuildIdentifierList: Failed to alloc mem for list\n"));
		return IMG_NULL;
	}

	psIdentifierList->psTokenList = psTokenList;
	psIdentifierList->uNumIdentifierIndices = 0;
	
	/* Get the number of identifiers in the token list */
	while (psTokenList[i].eTokenName != TOK_TERMINATEPARSING)
	{
		if (psTokenList[i].eTokenName == TOK_IDENTIFIER)
		{
			psIdentifierList->uNumIdentifierIndices++;
		}
		i++;
	}

	/* Alloc memory for the indices */
	psIdentifierList->puIdentifierIndices = DebugMemAlloc(sizeof(IMG_UINT32) * psIdentifierList->uNumIdentifierIndices);

	if (!psIdentifierList->puIdentifierIndices)
	{
		LOG_INTERNAL_ERROR(("ASTBIBuildIdentifierList: Failed to alloc mem for indices\n"));
		return IMG_NULL;
	}
	

	/* Iterate again the token list to generate the list of identifiers */
	i = 0;
	psIdentifierList->uNumIdentifierIndices = 0;

	while (psTokenList[i].eTokenName != TOK_TERMINATEPARSING)
	{
		if (psTokenList[i].eTokenName == TOK_IDENTIFIER)
		{
			psIdentifierList->puIdentifierIndices[psIdentifierList->uNumIdentifierIndices++] = i;
		}
		i++;
	}

	return psIdentifierList;
}


/******************************************************************************
 * Function Name: ASTBIFreeIdentifierList
 * Inputs       : psIdentifierList
 * Outputs      : -
 * Returns      : IMG_TRUE. (^_^;)
 * Globals Used : -
 * Description  : Frees an identifier list created by ASTBIBuildIdentifierList.
 *****************************************************************************/
static IMG_BOOL ASTBIFreeIdentifierList(ASTBIIdentifierList *psIdentifierList)
{
	if (psIdentifierList)
	{
		if (psIdentifierList->puIdentifierIndices)
		{
			DebugMemFree(psIdentifierList->puIdentifierIndices);
		}

		DebugMemFree(psIdentifierList);
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTBICheckForUseOfState
 * Inputs       : psIdentifierList, pszIdentifierName
 * Outputs      : -
 * Returns      : IMG_TRUE if the identifier is found in a list of identifiers.
 * Globals Used : -
 * Description  : Looks for the identifier in the identifier list. It returns whether it was found.
                  It takes O(n) time. Should it use a hash table?
 *****************************************************************************/
static IMG_BOOL ASTBICheckForUseOfState(ASTBIIdentifierList *psIdentifierList, IMG_CHAR *pszIdentifierName)
{
	IMG_UINT32 i = 0;

	if (!psIdentifierList)
	{
		return IMG_TRUE;
	}

	for (i = 0; i < psIdentifierList->uNumIdentifierIndices; i++)
	{
		IMG_CHAR *pszTokenData = psIdentifierList->psTokenList[psIdentifierList->puIdentifierIndices[i]].pvData;

		if (strcmp(pszIdentifierName, pszTokenData) == 0)
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

/******************************************************************************
 * Function Name: ASTBIAddConstructor
 * Inputs       : psSymbolTable, eConstructorTypeSpecifier
 * Outputs      : psSymbolTable
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : 
 * Description  : Adds a builtin constructor to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddConstructor(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable,
							 GLSLTypeSpecifier  eConstructorTypeSpecifier)
{
	/* FIXME: get rid of fixed length at some point. */
	IMG_CHAR acConstructorName[128];

	GLSLFunctionDefinitionData sFunctionDefinitionData;

	IMG_UINT32 uReturnDataSymbolID, uConstructorSymbolID;

	GLSLIdentifierData sReturnData;

	/* Create a constructor name */
	sprintf(acConstructorName, CONSTRUCTOR_RETURN_VAL_STRING, GLSLTypeSpecifierDescTable(eConstructorTypeSpecifier));

	/* Init the data to some default values */
	INIT_FULLY_SPECIFIED_TYPE(&sReturnData.sFullySpecifiedType);
	sReturnData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	sReturnData.sFullySpecifiedType.eTypeSpecifier           = eConstructorTypeSpecifier;
	sReturnData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
	sReturnData.eLValueStatus                                = GLSLLV_L_VALUE;
	sReturnData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
	sReturnData.iActiveArraySize                             = -1;
	sReturnData.sFullySpecifiedType.eParameterQualifier      = GLSLPQ_INVALID;
	sReturnData.sFullySpecifiedType.ePrecisionQualifier      = GLSLPRECQ_UNKNOWN;
	sReturnData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
	sReturnData.eIdentifierUsage                             = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_BUILT_IN);
	sReturnData.uConstantDataSize                            = 0;
	sReturnData.pvConstantData                               = IMG_NULL;
	sReturnData.uConstantAssociationSymbolID                 = 0;


	/* Add the data for the return value to the symbol table */
	if (!AddResultData(psCPD, psSymbolTable, 
					   acConstructorName, 
					   &sReturnData,
					   IMG_FALSE,
					   &uReturnDataSymbolID))
	{
		LOG_INTERNAL_ERROR(("ASTAddConstructor: Failed to add return value %s to symbol table", acConstructorName));
		
		return IMG_FALSE;
	}

	/* Create a constructor name */
	sprintf(acConstructorName, CONSTRUCTOR_STRING, GLSLTypeSpecifierDescTable(eConstructorTypeSpecifier));

	sFunctionDefinitionData.eSymbolTableDataType      = GLSLSTDT_FUNCTION_DEFINITION;
	sFunctionDefinitionData.eFunctionType             = GLSLFT_CONSTRUCTOR;
	sFunctionDefinitionData.pszOriginalFunctionName   = acConstructorName;
	sFunctionDefinitionData.uReturnDataSymbolID       = uReturnDataSymbolID;
	sFunctionDefinitionData.sReturnFullySpecifiedType = sReturnData.sFullySpecifiedType;
	sFunctionDefinitionData.bPrototype                = IMG_FALSE;
	sFunctionDefinitionData.uNumParameters            = 0;
	sFunctionDefinitionData.eBuiltInFunctionID        = GLSLBFID_NOT_BUILT_IN;
	sFunctionDefinitionData.uFunctionCalledCount      = 0;
	sFunctionDefinitionData.uMaxFunctionCallDepth     = 0;
	sFunctionDefinitionData.puCalledFunctionIDs       = IMG_NULL;
	sFunctionDefinitionData.uNumCalledFunctions       = 0;
	
	/* Add the constructor as a function to the symbol table */
	if (!AddFunctionDefinitionData(psCPD, psSymbolTable, 
								   acConstructorName, 
								   &sFunctionDefinitionData, 
								   IMG_FALSE,
								   &uConstructorSymbolID))
	{
		LOG_INTERNAL_ERROR(("ASTAddConstructor: Failed to add function %s to symbol table", acConstructorName));

		return IMG_FALSE;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTBIAddFunction
 * Inputs       : psSymbolTable
                  psIdentifierList
                  eBuiltInFunctionID
                  pszFunctionName
                  ppszParamNames
                  uNumVariants
                  uNumParams
                  peReturnTypeSpecifiers
				    ppeParamTypeSpecifiers
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a builtin function to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddFunction(GLSLCompilerPrivateData *psCPD,
						  SymTable               *psSymbolTable,
						  ASTBIIdentifierList    *psIdentifierList,
						  GLSLBuiltInFunctionID   eBuiltInFunctionID,
						  IMG_CHAR               *pszFunctionName,
						  IMG_CHAR              **ppszParamNames,
						  IMG_UINT32              uNumVariants,
						  IMG_UINT32              uNumParams,
						  GLSLTypeSpecifier      *peReturnTypeSpecifiers,
						  GLSLTypeSpecifier     **ppeParamTypeSpecifiers)
{
	IMG_UINT32                  i, j, uReturnDataSymbolID;
	GLSLFullySpecifiedType      *psFullySpecifiedTypes = IMG_NULL;
	GLSLFunctionDefinitionData  sFunctionDefinitionData;
	GLSLIdentifierData          sReturnData, sParameterData;
	IMG_CHAR                    *pszHashedFunctionName = IMG_NULL, *pszReturnValueName = IMG_NULL;

	/* If the function is never used in the code, it is not added to the symbol table */
	if (!ASTBICheckForUseOfState(psIdentifierList, pszFunctionName))
	{
		return IMG_TRUE;
	}

	psFullySpecifiedTypes = DebugMemAlloc(sizeof(GLSLFullySpecifiedType) * uNumParams);

	if(uNumParams && !psFullySpecifiedTypes)
	{
		return IMG_FALSE;
	}

	/* Set up some default values for the return type */
	INIT_FULLY_SPECIFIED_TYPE(&sReturnData.sFullySpecifiedType);
	sReturnData.eSymbolTableDataType         = GLSLSTDT_IDENTIFIER;
	sReturnData.eLValueStatus                = GLSLLV_L_VALUE;
	sReturnData.eArrayStatus                 = GLSLAS_NOT_ARRAY;
	sReturnData.iActiveArraySize             = -1;
	sReturnData.eBuiltInVariableID           = GLSLBV_NOT_BTIN;
	sReturnData.eIdentifierUsage             = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_BUILT_IN);
	sReturnData.uConstantDataSize            = 0;
	sReturnData.pvConstantData               = IMG_NULL;
	sReturnData.uConstantAssociationSymbolID = 0;

	/* Set up some default values for the parameters */
	INIT_FULLY_SPECIFIED_TYPE(&sParameterData.sFullySpecifiedType);
	sParameterData.eSymbolTableDataType                    = GLSLSTDT_IDENTIFIER;
	sParameterData.sFullySpecifiedType.eParameterQualifier = GLSLPQ_IN;
	sParameterData.sFullySpecifiedType.eTypeQualifier      = GLSLTQ_TEMP;
	sParameterData.sFullySpecifiedType.iArraySize          = 0;
	sParameterData.iActiveArraySize                        = -1;
	sParameterData.eArrayStatus                            = GLSLAS_NOT_ARRAY;
	sParameterData.eLValueStatus                           = GLSLLV_L_VALUE;
	sParameterData.eBuiltInVariableID                      = GLSLBV_NOT_BTIN;
	sParameterData.eIdentifierUsage                        = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_BUILT_IN);
	sParameterData.uConstantDataSize                       = 0;
	sParameterData.pvConstantData                          = IMG_NULL;
	sParameterData.uConstantAssociationSymbolID            = 0;

	/* Allocate memory for parameters */
	sFunctionDefinitionData.puParameterSymbolTableIDs  = DebugMemAlloc(uNumParams * sizeof(IMG_UINT32));

	if(uNumParams && !sFunctionDefinitionData.puParameterSymbolTableIDs)
	{
		return IMG_FALSE;
	}

	sFunctionDefinitionData.psFullySpecifiedTypes      = DebugMemAlloc(uNumParams * sizeof(GLSLFullySpecifiedType));

	if(uNumParams && !sFunctionDefinitionData.psFullySpecifiedTypes)
	{
		return IMG_FALSE;
	}

	
	/* Set up some default values for the function definition */
	sFunctionDefinitionData.eSymbolTableDataType  = GLSLSTDT_FUNCTION_DEFINITION;
	sFunctionDefinitionData.eFunctionType         = GLSLFT_BUILT_IN;
	sFunctionDefinitionData.eFunctionFlags        = GLSLFF_VALID_IN_ALL_CASES;
	sFunctionDefinitionData.bPrototype            = IMG_FALSE;
	sFunctionDefinitionData.eBuiltInFunctionID    = eBuiltInFunctionID;
	sFunctionDefinitionData.uFunctionCalledCount  = 0;
	sFunctionDefinitionData.uMaxFunctionCallDepth = 0;
	sFunctionDefinitionData.puCalledFunctionIDs   = IMG_NULL;
	sFunctionDefinitionData.uNumCalledFunctions   = 0;

	
	for (i = 0; i < uNumVariants; i++)
	{
		/* Build up a type specifier list */
		for (j = 0; j < uNumParams; j++)
		{
			INIT_FULLY_SPECIFIED_TYPE(&psFullySpecifiedTypes[j]);
			psFullySpecifiedTypes[j].eTypeSpecifier			= ppeParamTypeSpecifiers[j][i];
		}
		
		/* Get a hashed name for the function */
		pszHashedFunctionName = ASTSemCreateHashedFunctionName(psSymbolTable, pszFunctionName, uNumParams, psFullySpecifiedTypes);

		/* Allocate memory for return name */
		pszReturnValueName = DebugMemAlloc(strlen(pszHashedFunctionName) + 12);

		if(!pszReturnValueName)
		{
			goto FreeLoopMemAndReturnFalse;
		}

		/* Construct the return name */
		sprintf(pszReturnValueName, RETURN_VAL_STRING, pszHashedFunctionName);

		/* Set up return type specifier */
		INIT_FULLY_SPECIFIED_TYPE(&sReturnData.sFullySpecifiedType);
		sReturnData.sFullySpecifiedType.eTypeQualifier = GLSLTQ_TEMP;
		sReturnData.sFullySpecifiedType.eTypeSpecifier = peReturnTypeSpecifiers[i];

		/* Increase the scope level */
		IncreaseScopeLevel(psSymbolTable);

		/* Add the data to the symbol table */

		if (!AddResultData(psCPD, psSymbolTable, 
						   pszReturnValueName, 
						   &sReturnData, 
						   IMG_FALSE,
						   &uReturnDataSymbolID))
		{
			LOG_INTERNAL_ERROR(("ASTBIAddFunction: Failed to add return value %s to symbol table", pszReturnValueName));

FreeLoopMemAndReturnFalse:
			DebugMemFree(pszHashedFunctionName);
			DebugMemFree(pszReturnValueName);

			DebugMemFree(psFullySpecifiedTypes);
			DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
			DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);
			
			return IMG_FALSE;
		}

		/* Set up function definition */
		sFunctionDefinitionData.uNumParameters            = uNumParams;
		sFunctionDefinitionData.uReturnDataSymbolID       = uReturnDataSymbolID;
		sFunctionDefinitionData.sReturnFullySpecifiedType = sReturnData.sFullySpecifiedType;
		sFunctionDefinitionData.pszOriginalFunctionName   = pszFunctionName;

		/* Add parameters to symbol table */
		for (j = 0; j < uNumParams; j++)
		{
			sParameterData.sFullySpecifiedType.eTypeSpecifier			= psFullySpecifiedTypes[j].eTypeSpecifier;
			sParameterData.sFullySpecifiedType.ePrecisionQualifier		= psFullySpecifiedTypes[j].ePrecisionQualifier;
			sParameterData.sFullySpecifiedType.iArraySize				= psFullySpecifiedTypes[j].iArraySize;

			/* Add the data to the symbol table */
			if (!AddParameterData(psCPD, psSymbolTable,
								  ppszParamNames[j],
								  &sParameterData,
								  IMG_FALSE,
								  &sFunctionDefinitionData.puParameterSymbolTableIDs[j]))
			{
				LOG_INTERNAL_ERROR(("ASTBIAddFunction: Failed to add parameter %s to symbol table", ppszParamNames[j]));
				goto FreeLoopMemAndReturnFalse;
			}

			/* Add this data for consistancies sake (even though it should never be used */
			sFunctionDefinitionData.psFullySpecifiedTypes[j] = sParameterData.sFullySpecifiedType;
		}

		/* Decrease the scope level */
		DecreaseScopeLevel(psSymbolTable);

		/* Add the function definition to the symbol table */
		if (!AddFunctionDefinitionData(psCPD, psSymbolTable,
									   pszHashedFunctionName,
									   &sFunctionDefinitionData,
									   IMG_FALSE,
									   IMG_NULL))
		{
			LOG_INTERNAL_ERROR(("ASTBIAddFunction: Failed to add function %s to symbol table", pszHashedFunctionName));
			goto FreeLoopMemAndReturnFalse;
		}

		/* Free mem allocated for function name */
		DebugMemFree(pszHashedFunctionName);
		DebugMemFree(pszReturnValueName);
	}

	DebugMemFree(psFullySpecifiedTypes);
	DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
	DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);

	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: ASTBIAddFunction1
 * Inputs       : psSymbolTable, psIdentifierList, eBuiltInFunctionID,
                  pszFunctionName, pszParam1, uNumVariants, 
				    peReturnTypeSpecifiers, peParamTypeSpecifiers1
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull, IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a one-argument builtin function to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddFunction1(GLSLCompilerPrivateData *psCPD,
							SymTable               *psSymbolTable,
						   ASTBIIdentifierList    *psIdentifierList,
						   GLSLBuiltInFunctionID   eBuiltInFunctionID,
						   IMG_CHAR               *pszFunctionName,
						   IMG_CHAR               *pszParam1,
						   IMG_UINT32              uNumVariants,
						   GLSLTypeSpecifier      *peReturnTypeSpecifiers,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers1)
{
	GLSLTypeSpecifier *ppeParamTypeSpecifiers[1];
	IMG_CHAR          *ppszParamNames[1];

	ppeParamTypeSpecifiers[0] = peParamTypeSpecifiers1;

	ppszParamNames[0] = pszParam1;

	return ASTBIAddFunction(psCPD,
							psSymbolTable,
							psIdentifierList,
							eBuiltInFunctionID,
							pszFunctionName,
							ppszParamNames,
							uNumVariants,
							1,
							peReturnTypeSpecifiers,
							ppeParamTypeSpecifiers);
	
}

/******************************************************************************
 * Function Name: ASTBIAddFunction2
 * Inputs       : psSymbolTable, psIdentifierList, eBuiltInFunctionID,
                  pszFunctionName, pszParam1, pszParam2, uNumVariants, 
				    peReturnTypeSpecifiers, peParamTypeSpecifiers1,
					peParamTypeSpecifiers2
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull, IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a two-argument builtin function to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddFunction2(GLSLCompilerPrivateData *psCPD,
							SymTable               *psSymbolTable,
						   ASTBIIdentifierList    *psIdentifierList,
						   GLSLBuiltInFunctionID   eBuiltInFunctionID,
						   IMG_CHAR               *pszFunctionName,
						   IMG_CHAR               *pszParam1,
						   IMG_CHAR               *pszParam2,
						   IMG_UINT32              uNumVariants,
						   GLSLTypeSpecifier      *peReturnTypeSpecifiers,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers1,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers2)
{
	GLSLTypeSpecifier *ppeParamTypeSpecifiers[2];
	IMG_CHAR          *ppszParamNames[2];

	ppeParamTypeSpecifiers[0] = peParamTypeSpecifiers1;
	ppeParamTypeSpecifiers[1] = peParamTypeSpecifiers2;

	ppszParamNames[0] = pszParam1;
	ppszParamNames[1] = pszParam2;


	return ASTBIAddFunction(psCPD,
							psSymbolTable,
							psIdentifierList,
							eBuiltInFunctionID,
							pszFunctionName,
							ppszParamNames,
							uNumVariants,
							2,
							peReturnTypeSpecifiers,
							ppeParamTypeSpecifiers);
	
}

/******************************************************************************
 * Function Name: ASTBIAddFunction3
 * Inputs       : psSymbolTable, psIdentifierList, eBuiltInFunctionID,
                  pszFunctionName, pszParam1, pszParam2, pszParam3,
				    uNumVariants, peReturnTypeSpecifiers, peParamTypeSpecifiers1,
					peParamTypeSpecifiers2, peParamTypeSpecifiers3
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull, IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a three-argument builtin function to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddFunction3(GLSLCompilerPrivateData *psCPD,
							SymTable               *psSymbolTable,
						   ASTBIIdentifierList    *psIdentifierList,
						   GLSLBuiltInFunctionID   eBuiltInFunctionID,
						   IMG_CHAR               *pszFunctionName,
						   IMG_CHAR               *pszParam1,
						   IMG_CHAR               *pszParam2,
						   IMG_CHAR               *pszParam3,
						   IMG_UINT32              uNumVariants,
						   GLSLTypeSpecifier      *peReturnTypeSpecifiers,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers1,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers2,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers3)
{
	GLSLTypeSpecifier *ppeParamTypeSpecifiers[3];
	IMG_CHAR          *ppszParamNames[3];

	ppeParamTypeSpecifiers[0] = peParamTypeSpecifiers1;
	ppeParamTypeSpecifiers[1] = peParamTypeSpecifiers2;
	ppeParamTypeSpecifiers[2] = peParamTypeSpecifiers3;

	ppszParamNames[0] = pszParam1;
	ppszParamNames[1] = pszParam2;
	ppszParamNames[2] = pszParam3;

	return ASTBIAddFunction(psCPD,
							psSymbolTable,
							psIdentifierList,
							eBuiltInFunctionID,
							pszFunctionName,
							ppszParamNames,
							uNumVariants,
							3,
							peReturnTypeSpecifiers,
							ppeParamTypeSpecifiers);
	
}

/******************************************************************************
 * Function Name: ASTBIAddFunction4
 * Inputs       : psSymbolTable, psIdentifierList, eBuiltInFunctionID,
                  pszFunctionName, pszParam1, pszParam2, pszParam3, pszParam4
				    uNumVariants, pszParam4, peReturnTypeSpecifiers, peParamTypeSpecifiers1,
					peParamTypeSpecifiers2, peParamTypeSpecifiers3, peParamTypeSpecifiers4
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull, IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a four-argument builtin function to the symbol table.
 *****************************************************************************/
static IMG_BOOL ASTBIAddFunction4(GLSLCompilerPrivateData *psCPD,
							SymTable               *psSymbolTable,
						   ASTBIIdentifierList    *psIdentifierList,
						   GLSLBuiltInFunctionID   eBuiltInFunctionID,
						   IMG_CHAR               *pszFunctionName,
						   IMG_CHAR               *pszParam1,
						   IMG_CHAR               *pszParam2,
						   IMG_CHAR               *pszParam3,
						   IMG_CHAR               *pszParam4,
						   IMG_UINT32              uNumVariants,
						   GLSLTypeSpecifier      *peReturnTypeSpecifiers,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers1,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers2,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers3,
						   GLSLTypeSpecifier      *peParamTypeSpecifiers4)
{
	GLSLTypeSpecifier *ppeParamTypeSpecifiers[4];
	IMG_CHAR          *ppszParamNames[4];

	ppeParamTypeSpecifiers[0] = peParamTypeSpecifiers1;
	ppeParamTypeSpecifiers[1] = peParamTypeSpecifiers2;
	ppeParamTypeSpecifiers[2] = peParamTypeSpecifiers3;
	ppeParamTypeSpecifiers[3] = peParamTypeSpecifiers4;

	ppszParamNames[0] = pszParam1;
	ppszParamNames[1] = pszParam2;
	ppszParamNames[2] = pszParam3;
	ppszParamNames[3] = pszParam4;

	return ASTBIAddFunction(psCPD,
							psSymbolTable,
							psIdentifierList,
							eBuiltInFunctionID,
							pszFunctionName,
							ppszParamNames,
							uNumVariants,
							4,
							peReturnTypeSpecifiers,
							ppeParamTypeSpecifiers);
	
}


/******************************************************************************
 * Function Name: ASTBIAddGLState
 * Inputs       : psSymbolTable, psIdentifierList, pszIdentifierName, eBuiltInVariableID,
                  eTypeQualifier, eTypeSpecifier, ePrecisionQualifier, uStructDescSymbolTableID,
				    iArraySize, uDataSize, pvData, eProgramType
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a builtin variable to the symbol table.
 *****************************************************************************/
static IMG_UINT32 ASTBIAddGLState(GLSLCompilerPrivateData *psCPD,
						SymTable               *psSymbolTable,
						 ASTBIIdentifierList    *psIdentifierList,
						 IMG_CHAR               *pszIdentifierName,
						 GLSLBuiltInVariableID   eBuiltInVariableID,
						 GLSLTypeQualifier       eTypeQualifier,
						 GLSLTypeSpecifier       eTypeSpecifier,
						 GLSLPrecisionQualifier  ePrecisionQualifier,
						 IMG_UINT32              uStructDescSymbolTableID,
						 IMG_INT32               iArraySize,
						 IMG_UINT32              uDataSize,
						 void                   *pvData,
						 GLSLProgramType         eProgramType)
{
	GLSLFullySpecifiedType sFullySpecifiedType;
	IMG_UINT32 uSymbolTableID;

	PVR_UNREFERENCED_PARAMETER(uDataSize); 
	PVR_UNREFERENCED_PARAMETER(pvData);

	/* If this piece of state is never referenced in the code, we don't need to add it to the symbol table. */
	if (!ASTBICheckForUseOfState(psIdentifierList, pszIdentifierName))
	{
		return 0;
	}

	INIT_FULLY_SPECIFIED_TYPE(&sFullySpecifiedType);
	sFullySpecifiedType.eTypeQualifier           = eTypeQualifier;
	sFullySpecifiedType.eTypeSpecifier           = eTypeSpecifier;
	sFullySpecifiedType.ePrecisionQualifier      = ePrecisionQualifier;
	sFullySpecifiedType.uStructDescSymbolTableID = uStructDescSymbolTableID;
	sFullySpecifiedType.iArraySize               = iArraySize;

	uSymbolTableID = ASTSemAddIdentifierToSymbolTable(psCPD, IMG_NULL, psSymbolTable,
										 IMG_NULL,
										 pszIdentifierName,
										 &sFullySpecifiedType,
										 IMG_TRUE,
										 eBuiltInVariableID,
										 GLSLIU_BUILT_IN,
										 uDataSize,
										 pvData, 
										 eProgramType);

	if(!uSymbolTableID)
	{
		LOG_INTERNAL_ERROR(("ASTBIAddGLState: Failed to add '%s'\n",
						 pszIdentifierName));
	}


	return uSymbolTableID;
}


/******************************************************************************
 * Function Name: ASTBIResetGLStructureDefinition
 * Inputs       : psStructureDefinitionData
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Resets a struct data type. Frees all memory, etc.
 *****************************************************************************/
static IMG_VOID ASTBIResetGLStructureDefinition(GLSLStructureDefinitionData *psStructureDefinitionData)
{
	IMG_UINT32 i;

	if (psStructureDefinitionData->uNumMembers)
	{
		for (i = 0; i < psStructureDefinitionData->uNumMembers; i++)
		{
			DebugMemFree(psStructureDefinitionData->psMembers[i].pszMemberName);
		}
		
		DebugMemFree(psStructureDefinitionData->psMembers);
	}

	psStructureDefinitionData->uNumMembers = 0;
	psStructureDefinitionData->psMembers = IMG_NULL;
}


/******************************************************************************
 * Function Name: ASTAddGLStructureMember
 * Inputs       : psStructureDefinitionData, pszMemberName, eTypeSpecifier, ePrecisionQualifier 
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds a struct member to a struct data type.
 *****************************************************************************/
static IMG_BOOL ASTAddGLStructureMember(GLSLStructureDefinitionData *psStructureDefinitionData,
								 IMG_CHAR                    *pszMemberName,
								 GLSLTypeSpecifier            eTypeSpecifier,
								 GLSLPrecisionQualifier       ePrecisionQualifier)
{
	GLSLStructureMember *psMember;

	psStructureDefinitionData->uNumMembers++;

	/* Alloc mem for new member */
	/* FIXME: unsafe use of realloc */
	psStructureDefinitionData->psMembers = DebugMemRealloc(psStructureDefinitionData->psMembers, 
														   sizeof(GLSLStructureMember) *  psStructureDefinitionData->uNumMembers);
	if(!psStructureDefinitionData->psMembers)
	{
		return IMG_FALSE;
	}

	psMember = &psStructureDefinitionData->psMembers[psStructureDefinitionData->uNumMembers -1];

	psMember->pszMemberName = DebugMemAlloc(strlen(pszMemberName) + 1);

	if(!psMember->pszMemberName)
	{
		psStructureDefinitionData->uNumMembers--;
		return IMG_FALSE;
	}

	strcpy(psMember->pszMemberName, pszMemberName);

	psMember->sIdentifierData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;


	psMember->sIdentifierData.iActiveArraySize                             = -1;
	psMember->sIdentifierData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;

	INIT_FULLY_SPECIFIED_TYPE(&(psMember->sIdentifierData.sFullySpecifiedType));
	psMember->sIdentifierData.sFullySpecifiedType.eTypeSpecifier           = eTypeSpecifier;
	psMember->sIdentifierData.sFullySpecifiedType. ePrecisionQualifier     = ePrecisionQualifier;

	psMember->sIdentifierData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
	psMember->sIdentifierData.eIdentifierUsage                             = (GLSLIdentifierUsage)0;
	psMember->sIdentifierData.uConstantAssociationSymbolID                 = 0;

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTBIAddBuiltInData
 * Inputs       : psSymbolTable, psTokenList, eProgramType, eExtendFunc, psRP, psCR
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds all builtin variables that may be used into the symbol table. See below.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTBIAddBuiltInData(GLSLCompilerPrivateData *psCPD,
							SymTable                *psSymbolTable, 
							 Token                   *psTokenList, 
							 GLSLProgramType          eProgramType,
							 GLSLExtendFunctionality  eExtendFunc,
							 GLSLRequestedPrecisions *psRP,
							 GLSLCompilerResources   *psCR)
{
	/*
	 The OpenGL Shading Language defines an assortment of built-in convenience functions for scalar and
	  vector operations. Many of these built-in functions can be used in more than one type of shader, but some
	  are intended to provide a direct mapping to hardware and so are available only for a specific type of
	  shader.

	  The built-in functions basically fall into three categories:
	  - They expose some necessary hardware functionality in a convenient way such as accessing
	    a texture map. There is no way in the language for these functions to be emulated by a
	    shader.
	  - They represent a trivial operation (clamp, mix, etc.) that is very simple for the user to write,
	    but they are very common and may have direct hardware support. It is a very hard problem
	    for the compiler to map expressions to complex assembler instructions.
	  - They represent an operation graphics hardware is likely to accelerate at some point. The
	    trigonometry functions fall into this category.

	  Many of the functions are similar to the same named ones in common C libraries, but they support vector
	  input as well as the more traditional scalar input.

	  Applications should be encouraged to use the built-in functions rather than do the equivalent
	  computations in their own shader code since the built-in functions are assumed to be optimal (e.g.,
	  perhaps supported directly in hardware).

	  User code can replace built-in functions with their own if they choose, by simply re-declaring and
	  defining the same name and argument list.

	  When the built-in functions are specified below, where the input arguments (and corresponding output)
	  can be float, vec2, vec3, or vec4, genType is used as the argument. For any specific use of a function, the
	  actual type has to be the same for all arguments and for the return type. Similarly for mat, which can be a
	  mat2, mat3, or mat4.

	*/
	GLSLStructureDefinitionData sStructureDefinitionData;
	IMG_UINT32 uStructSymbolTableID, i;

	GLSLTypeSpecifier aeGenType[]           = {GLSLTS_FLOAT, GLSLTS_VEC2,  GLSLTS_VEC3,  GLSLTS_VEC4};
	GLSLTypeSpecifier aeFloat[]             = {GLSLTS_FLOAT, GLSLTS_FLOAT, GLSLTS_FLOAT, GLSLTS_FLOAT};
	GLSLTypeSpecifier aeBool[]              = {GLSLTS_BOOL,  GLSLTS_BOOL,  GLSLTS_BOOL,  GLSLTS_BOOL};
	GLSLTypeSpecifier aeVec4[]              = {GLSLTS_VEC4,  GLSLTS_VEC4,  GLSLTS_VEC4,  GLSLTS_VEC4};
	GLSLTypeSpecifier aeVec3[]              = {GLSLTS_VEC3,  GLSLTS_VEC3,  GLSLTS_VEC3,  GLSLTS_VEC3};
	GLSLTypeSpecifier aeVec2[]              = {GLSLTS_VEC2,  GLSLTS_VEC2,  GLSLTS_VEC2,  GLSLTS_VEC2};
	GLSLTypeSpecifier aeVec[]               = {GLSLTS_VEC2,  GLSLTS_VEC3,  GLSLTS_VEC4};
	GLSLTypeSpecifier aeBVec[]              = {GLSLTS_BVEC2, GLSLTS_BVEC3, GLSLTS_BVEC4};
	GLSLTypeSpecifier aeIVec[]              = {GLSLTS_IVEC2, GLSLTS_IVEC3, GLSLTS_IVEC4};
	GLSLTypeSpecifier aeSampler2D[]	        = {GLSLTS_SAMPLER2D};
	GLSLTypeSpecifier aeSamplerCube[]   	= {GLSLTS_SAMPLERCUBE};

#define aeGenTypeNoF aeVec
#define aeGenITypeNoI aeIVec
#define aeGenUTypeNoU aeUVec

#if !defined(GLSL_ES)

#define GLSL_NUM_MATRICES  9

	GLSLTypeSpecifier aeMat[]               = {GLSLTS_MAT2X2, GLSLTS_MAT2X3, GLSLTS_MAT2X4, 
											   GLSLTS_MAT3X2, GLSLTS_MAT3X3, GLSLTS_MAT3X4, 
											   GLSLTS_MAT4X2, GLSLTS_MAT4X3, GLSLTS_MAT4X4};
	GLSLTypeSpecifier aeSampler1D[]			= {GLSLTS_SAMPLER1D};
	GLSLTypeSpecifier aeSampler3D[]			= {GLSLTS_SAMPLER3D};
	GLSLTypeSpecifier aeSampler1DShadow[]	= {GLSLTS_SAMPLER1DSHADOW};
	GLSLTypeSpecifier aeSampler2DShadow[]	= {GLSLTS_SAMPLER2DSHADOW};

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	GLSLTypeSpecifier aeSampler2DRect[]		= {GLSLTS_SAMPLER2DRECT};
	GLSLTypeSpecifier aeSampler2DRectShadow[] = {GLSLTS_SAMPLER2DRECTSHADOW};
#endif

#ifdef OGL_TEXTURE_STREAM
	GLSLTypeSpecifier aeSamplerStream[]		= {GLSLTS_SAMPLERSTREAM};
#endif

#else

#define GLSL_NUM_MATRICES  3

	GLSLTypeSpecifier aeMat[]               = {GLSLTS_MAT2X2, GLSLTS_MAT3X3, GLSLTS_MAT4X4};
	GLSLTypeSpecifier aeSamplerStream[]		= {GLSLTS_SAMPLERSTREAM};
	GLSLTypeSpecifier aeSamplerExternal[]	= {GLSLTS_SAMPLEREXTERNAL};

#endif

	ASTBIIdentifierList *psIL = ASTBIBuildIdentifierList(psCPD, psTokenList);
	SymTable            *psST = psSymbolTable;

	sStructureDefinitionData.eSymbolTableDataType = GLSLSTDT_STRUCTURE_DEFINITION;
	sStructureDefinitionData.uNumMembers = 0;
	sStructureDefinitionData.psMembers = IMG_NULL;

	
	/* 
	   Built in variables 
	*/


	/*
	  Vertex Shader Special Variables

	  The variable gl_Position is available only in the vertex language and is intended for writing the
	  homogeneous vertex position. All executions of a well-formed vertex shader must write a value into this
	  variable. It can be written at any time during shader execution. It may also be read back by the shader
	  after being written. This value will be used by primitive assembly, clipping, culling, and other fixed
	  functionality operations that operate on primitives after vertex processing has occurred. Compilers may
	  generate a diagnostic message if they detect gl_Position is not written, or read before being written, but
	  not all such cases are detectable. Results are undefined if a vertex shader is executed and does not write
	  gl_Position.
	   The variable gl_PointSize is available only in the vertex language and is intended for a vertex shader to
	  write the size of the point to be rasterized. It is measured in pixels.
	  The variable gl_ClipVertex is available only in the vertex language and provides a place for vertex shaders
	  to write the coordinate to be used with the user clipping planes. The user must ensure the clip vertex and
	  user clipping planes are defined in the same coordinate space. User clip planes work properly only under
	  linear transform. It is undefined what happens under non-linear transform.
	  These built-in vertex shader variables for communicating with fixed functionality are intrinsically
	  declared with the following types:

	      vec4 gl_Position; // must be written to
	      float gl_PointSize; // may be written to
	      vec4 gl_ClipVertex; // may be written to

	  If gl_PointSize or gl_ClipVertex are not written to, their values are undefined. Any of these variables can
	  be read back by the shader after writing to them, to retrieve what was written. Reading them before
	  writing them results in undefined behavior. If they are written more than once, it is the last value written
	  that is consumed by the subsequent operations.
	  These built-in variables have global scope.
	*/

	if (eProgramType == GLSLPT_VERTEX)
	{
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_Position",   GLSLBV_POSITION,   GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eGLPosition,        0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_PointSize",  GLSLBV_POINTSIZE,  GLSLTQ_VERTEX_OUT, GLSLTS_FLOAT, psRP->eGLPointSize,       0, 0, 0, IMG_NULL, eProgramType);
#if !defined(GLSL_ES)
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_ClipVertex", GLSLBV_CLIPVERTEX, GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat,    0, 0, 0, IMG_NULL, eProgramType);
#endif
	}

	/*
	  Fragment Shader Special Variables

	  The output of the fragment shader is processed by the fixed function operations at the back end of the
	  OpenGL pipeline. Fragment shaders output values to the OpenGL pipeline using the built-in variables
	  gl_FragColor, gl_FragData, and gl_FragDepth, unless the discard keyword is executed.

	  The fragment shader has access to the read-only built-in variable gl_FrontFacing whose value is true if
	  the fragment belongs to a front-facing primitive. One use of this is to emulate two-sided lighting by
	  selecting one of two colors calculated by the vertex shader.
	  The built-in variables that are accessible from a fragment shader are intrinsically given types as follows:
	  
	      vec4 gl_FragCoord;
	      bool gl_FrontFacing;
	      vec4 gl_FragColor;
	      vec4 gl_FragData[gl_MaxDrawBuffers];
	      float gl_FragDepth;
	*/

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddGLState(psCPD, psST, psIL, "gl_FragCoord",	  GLSLBV_FRAGCOORD,	  GLSLTQ_FRAGMENT_IN, GLSLTS_VEC4,  psRP->eBIFragFloat,  0, 0, 0, IMG_NULL, eProgramType);
		{
			/* gl_FrontFacing is not a constant and its not a uniform, but it is read-only, so the l_value status has to be manually set. */
			IMG_UINT32 uSymbolTableID = ASTBIAddGLState(psCPD, psST, psIL, "gl_FrontFacing",  GLSLBV_FRONTFACING, GLSLTQ_TEMP,    GLSLTS_BOOL,  GLSLPRECQ_UNKNOWN,   0, 0, 0, IMG_NULL, eProgramType);
			if(uSymbolTableID)
			{
				GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psST, uSymbolTableID);
				psIdentifierData->eLValueStatus = GLSLLV_NOT_L_VALUE;
			}
		}	
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_FragColor",GLSLBV_FRAGCOLOR,	  GLSLTQ_FRAGMENT_OUT,    GLSLTS_VEC4,  psRP->eBIFragFloat,  0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_FragData",	  GLSLBV_FRAGDATA,	  GLSLTQ_FRAGMENT_OUT,    GLSLTS_VEC4,  psRP->eBIFragFloat,  0, psCR->iGLMaxDrawBuffers, 0, IMG_NULL, eProgramType);
#if !defined(GLSL_ES)
		ASTBIAddGLState(psCPD, psST, psIL, "gl_FragDepth",	  GLSLBV_FRAGDEPTH,	  GLSLTQ_TEMP,    GLSLTS_FLOAT, psRP->eBIFragFloat,  0, 0, 0, IMG_NULL, eProgramType);
#endif

	}

#if !defined(GLSL_ES)

	/*
	  Vertex Shader Built-In Attributes

	  The following attribute names are built into the OpenGL vertex language and can be used from within a
	  vertex shader to access the current values of attributes declared by OpenGL. All page numbers and
	  notations are references to the OpenGL 1.4 specification.
	  //
	  // Vertex Attributes, p. 19.
	  //
	  attribute vec4  gl_Color;
	  attribute vec4  gl_SecondaryColor;
	  attribute vec3  gl_Normal;
	  attribute vec4  gl_Vertex;
	  attribute vec4  gl_MultiTexCoord0;
	  attribute vec4  gl_MultiTexCoord1;
	  attribute vec4  gl_MultiTexCoord2;
	  attribute vec4  gl_MultiTexCoord3;
	  attribute vec4  gl_MultiTexCoord4;
	  attribute vec4  gl_MultiTexCoord5;
	  attribute vec4  gl_MultiTexCoord6;
	  attribute vec4  gl_MultiTexCoord7;
	  attribute float gl_FogCoord;
	*/
	if (eProgramType == GLSLPT_VERTEX)
	{
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_Color",          GLSLBV_COLOR,          GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_SecondaryColor", GLSLBV_SECONDARYCOLOR, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_Normal",         GLSLBV_NORMAL,         GLSLTQ_VERTEX_IN, GLSLTS_VEC3,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_Vertex",         GLSLBV_VERTEX,         GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord0", GLSLBV_MULTITEXCOORD0, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord1", GLSLBV_MULTITEXCOORD1, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord2", GLSLBV_MULTITEXCOORD2, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord3", GLSLBV_MULTITEXCOORD3, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord4", GLSLBV_MULTITEXCOORD4, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord5", GLSLBV_MULTITEXCOORD5, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord6", GLSLBV_MULTITEXCOORD6, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_MultiTexCoord7", GLSLBV_MULTITEXCOORD7, GLSLTQ_VERTEX_IN, GLSLTS_VEC4,  psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_FogCoord",       GLSLBV_FOGCOORD,       GLSLTQ_VERTEX_IN, GLSLTS_FLOAT, psRP->eBIVertAttribFloat, 0, 0, 0, IMG_NULL, eProgramType);
	}
#endif
	/*
	  Built-In Constants

	  The following built-in constants are provided to vertex and fragment shaders.
	  //
	  // Implementation dependent constants. The example values below
	  // are the minimum values allowed for these maximums.
	  //
	  const int gl_MaxLights = 8;                    // GL 1.0
	  const int gl_MaxClipPlanes = 6;                // GL 1.0
	  const int gl_MaxTextureUnits = 2;              // GL 1.3
	  const int gl_MaxTextureCoords = 2;             // ARB_fragment_program
	  const int gl_MaxVertexAttribs = 16;            // ARB_vertex_shader
	  const int gl_MaxVertexUniformComponents = 512; // ARB_vertex_shader
	  const int gl_MaxVaryingFloats = 32;            // ARB_vertex_shader
	  const int gl_MaxVertexTextureImageUnits = 0;   // ARB_vertex_shader
	  const int gl_MaxCombinedTextureImageUnits = 2; // ARB_vertex_shader
	  const int gl_MaxTextureImageUnits = 2;         // ARB_fragment_shader
	  const int gl_MaxFragmentUniformComponents = 64;// ARB_fragment_shader
	  const int gl_MaxDrawBuffers = 1;               // proposed ARB_draw_buffers
	  const mediump int gl_MaxVertexAttribs = 8             // GLSL-ES 1.00
	  const mediump int gl_MaxVertexUniformVectors = 128    // GLSL-ES 1.00
	  const mediump int gl_MaxVaryingVectors = 8            // GLSL-ES 1.00
	  const mediump int gl_MaxVertexTextureImageUnits = 0   // GLSL-ES 1.00
	  const mediump int gl_MaxCombinedTextureImageUnits = 8 // GLSL-ES 1.00
	  const mediump int gl_MaxTextureImageUnits = 8         // GLSL-ES 1.00
	  const mediump int gl_MaxFragmentUniformVectors = 16   // GLSL-ES 1.00
	  const mediump int gl_MaxDrawBuffers = 1               // GLSL-ES 1.00

	*/
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVertexAttribs",             GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVertexAttribs,             eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxTextureImageUnits",         GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxTextureImageUnits,         eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxDrawBuffers",               GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxDrawBuffers,               eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVertexTextureImageUnits",   GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVertexTextureImageUnits,   eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxCombinedTextureImageUnits", GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxCombinedTextureImageUnits, eProgramType);
#if defined(GLSL_ES)
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVertexUniformVectors",      GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVertexUniformVectors,      eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVaryingVectors",            GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVaryingVectors,            eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxFragmentUniformVectors",    GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxFragmentUniformVectors,    eProgramType);
#else
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxTextureUnits",              GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxTextureUnits,              eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVertexUniformComponents",   GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVertexUniformComponents,   eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxFragmentUniformComponents", GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxFragmentUniformComponents, eProgramType);

	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxLights",                    GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxLights,                    eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxVaryingFloats",             GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxVaryingFloats,             eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxClipPlanes",                GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxClipPlanes,                eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_MaxTextureCoords",             GLSLBV_NOT_BTIN, GLSLTQ_CONST, GLSLTS_INT,  psRP->eBIStateInt, 0, 0, sizeof(IMG_INT32), &psCR->iGLMaxTextureCoords,             eProgramType);

#endif /* !defined(GLSL_ES) */

	/*
	  Built-In Uniform State

	  As an aid to accessing OpenGL processing state, the following uniform variables are built into the
	  OpenGL Shading Language. All page numbers and notations are references to the 1.4 specification.
	*/

	/*
	  Matrix state. p. 31, 32, 37, 39, 40.

	  uniform mat4 gl_ModelViewMatrix;
	  uniform mat4 gl_ProjectionMatrix;
	  uniform mat4 gl_ModelViewProjectionMatrix;
	  uniform mat4 gl_TextureMatrix[gl_MaxTextureCoords];
	*/
#if !defined(GLSL_ES)

	ASTBIAddGLState(psCPD, psST, psIL,     "gl_ModelViewMatrix",           GLSLBV_MODELVIEWMATRIX,	          GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL,     "gl_ProjectionMatrix",          GLSLBV_PROJECTMATRIX,             GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_ModelViewProjectionMatrix", GLSLBV_MODELVIEWPROJECTIONMATRIX, GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL,     "gl_TextureMatrix",             GLSLBV_TEXTUREMATRIX,             GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords, 0, IMG_NULL, eProgramType);



	/*
	  Derived matrix state that provides inverse and transposed versions
	  of the matrices above. Poorly conditioned matrices may result
	  in unpredictable values in their inverse forms.

	  uniform mat3 gl_NormalMatrix; // transpose of the inverse of the upper leftmost 3x3 of gl_ModelViewMatrix
	  uniform mat4 gl_ModelViewMatrixInverse;
	  uniform mat4 gl_ProjectionMatrixInverse;
	  uniform mat4 gl_ModelViewProjectionMatrixInverse;
	  uniform mat4 gl_TextureMatrixInverse[gl_MaxTextureCoords];
	  uniform mat4 gl_ModelViewMatrixTranspose;
	  uniform mat4 gl_ProjectionMatrixTranspose;
	  uniform mat4 gl_ModelViewProjectionMatrixTranspose;
	  uniform mat4 gl_TextureMatrixTranspose[gl_MaxTextureCoords];
	  uniform mat4 gl_ModelViewMatrixInverseTranspose;
	  uniform mat4 gl_ProjectionMatrixInverseTranspose;
	  uniform mat4 gl_ModelViewProjectionMatrixInverseTranspose;
	  uniform mat4 gl_TextureMatrixInverseTranspose[gl_MaxTextureCoords];
	*/
	ASTBIAddGLState(psCPD, psST, psIL, "gl_NormalMatrix",                              GLSLBV_NORMALMATRIX,	                          GLSLTQ_UNIFORM, GLSLTS_MAT3X3,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewMatrixInverse",                    GLSLBV_MODELVIEWMATRIXINVERSE,	                  GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ProjectionMatrixInverse",                   GLSLBV_PROJECTMATRIXINVERSE,                      GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewProjectionMatrixInverse",          GLSLBV_MODELVIEWPROJECTIONMATRIXINVERSE,          GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_TextureMatrixInverse",                      GLSLBV_TEXTUREMATRIXINVERSE,                      GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewMatrixTranspose",                  GLSLBV_MODELVIEWMATRIXTRANSPOSE,                  GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ProjectionMatrixTranspose",                 GLSLBV_PROJECTIONMATRIXTRANSPOSE,                 GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewProjectionMatrixTranspose",        GLSLBV_MODELVIEWPROJECTIONMATRIXTRANSPOSE,        GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_TextureMatrixTranspose",                    GLSLBV_TEXTUREMATRIXTRANSPOSE,	                  GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewMatrixInverseTranspose",           GLSLBV_MODELVIEWMATRIXINVERSETRANSPOSE,	          GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ProjectionMatrixInverseTranspose",          GLSLBV_PROJECTMATRIXINVERSETRANSPOSE,             GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ModelViewProjectionMatrixInverseTranspose", GLSLBV_MODELVIEWPROJECTIONMATRIXINVERSETRANSPOSE, GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, 0,                         0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_TextureMatrixInverseTranspose",             GLSLBV_TEXTUREMATRIXINVERSETRANSPOSE,             GLSLTQ_UNIFORM, GLSLTS_MAT4X4,  psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords, 0, IMG_NULL, eProgramType);

	/*
	  Normal scaling p. 39.
	  
	  uniform float gl_NormalScale;
	*/
	
	ASTBIAddGLState(psCPD, psST, psIL, "gl_NormalScale", GLSLBV_NORMALSCALE, GLSLTQ_UNIFORM, GLSLTS_FLOAT, psRP->eBIStateFloat, 0, 0, 0, IMG_NULL, eProgramType);

#endif /* !defined GLSL_ES */


	if (ASTBICheckForUseOfState(psIL, "gl_DepthRange"))
	{
		/*
		  Depth range in window coordinates, p. 33
		  
		  struct gl_DepthRangeParameters 
		  {
		      float near; // n
		      float far; // f
		      float diff; // f - n
		  };
		  
		  uniform gl_DepthRangeParameters gl_DepthRange;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "near", GLSLTS_FLOAT, psRP->eDepthRange);
		ASTAddGLStructureMember(&sStructureDefinitionData, "far",  GLSLTS_FLOAT, psRP->eDepthRange);
		ASTAddGLStructureMember(&sStructureDefinitionData, "diff", GLSLTS_FLOAT, psRP->eDepthRange);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_DepthRangeParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);
		
		ASTBIAddGLState(psCPD, psST, psIL, "gl_DepthRange", GLSLBV_DEPTHRANGE, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0,  0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}


	/*
	  Unlike user-defined varying variables, the built-in varying variables don't have a strict one-to-one
	  correspondence between the vertex language and the fragment language. Two sets are provided, one for
	  each language. Their relationship is described below.
	
	  The following varying variables are available to read from in a fragment shader:
		  varying mediump vec2 gl_PointCoord;

	  The values in gl_PointCoord are two-dimensional coordinates indicating where within a point primitive
	  the current fragment is located. They range from 0.0 to 1.0 across the point. This is described in more
	  detail in Section 3.3.1 Basic Point Rasterization of version 2.0 of the OpenGL Specification, where point
	  sprites are discussed. If the current primitive is not a point, then the values read from gl_PointCoord are
	  undefined.

	*/

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddGLState(psCPD, psST, psIL, "gl_PointCoord",   GLSLBV_POINTCOORD,   GLSLTQ_FRAGMENT_IN, GLSLTS_VEC2, psRP->eGLPointCoord, 0, 0,  0, IMG_NULL, eProgramType);
	}
#if !defined(GLSL_ES)


	/*
	  Clip planes p. 42.
	  
	  uniform vec4 gl_ClipPlane[gl_MaxClipPlanes];
	*/
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ClipPlane", GLSLBV_CLIPPLANE, GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxClipPlanes, 0, IMG_NULL, eProgramType);


	if (ASTBICheckForUseOfState(psIL, "gl_Point"))
	{
		/*
		  Point Size, p. 66, 67.

		  struct gl_PointParameters 
		  {
		      float size;
		      float sizeMin;
		      float sizeMax;
		      float fadeThresholdSize;
		      float distanceConstantAttenuation;
		      float distanceLinearAttenuation;
		      float distanceQuadraticAttenuation;
		  };
		  
		  uniform gl_PointParameters gl_Point;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "size",                         GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "sizeMin",                      GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "sizeMax",                      GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "fadeThresholdSize",            GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "distanceConstantAttenuation",  GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "distanceLinearAttenuation",    GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "distanceQuadraticAttenuation", GLSLTS_FLOAT, psRP->eBIStateFloat);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_PointParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_Point", GLSLBV_POINT, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}


	if (ASTBICheckForUseOfState(psIL, "gl_FrontMaterial") || ASTBICheckForUseOfState(psIL, "gl_BackMaterial"))
	{
		/*
		  Material State p. 50, 55.

		  struct gl_MaterialParameters 
		  {
		      vec4 emission; // Ecm
		      vec4 ambient; // Acm
		      vec4 diffuse; // Dcm
		      vec4 specular; // Scm
		      float shininess; // Srm
		  };

		  uniform gl_MaterialParameters gl_FrontMaterial;
		  uniform gl_MaterialParameters gl_BackMaterial;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "emission",  GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "ambient",   GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "diffuse",   GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "specular",  GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "shininess", GLSLTS_FLOAT, psRP->eBIStateFloat);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_MaterialParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_FrontMaterial", GLSLBV_FRONTMATERIAL, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_BackMaterial",  GLSLBV_BACKMATERIAL,  GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}

	if (ASTBICheckForUseOfState(psIL, "gl_LightSource"))
	{
		/*
		  Light State p 50, 53, 55.
		  
		  struct gl_LightSourceParameters 
		  {
		      vec4 ambient; // Acli
		      vec4 diffuse; // Dcli
		      vec4 specular; // Scli
		      vec4 position; // Ppli
		      vec4 halfVector; // Derived: Hi
		      vec3 spotDirection; // Sdli
		      float spotExponent; // Srli
		      float spotCutoff; // Crli
		      // (range: [0.0,90.0], 180.0)
		      float spotCosCutoff; // Derived: cos(Crli)
		      // (range: [1.0,0.0],-1.0)
		      float constantAttenuation; // K0
		      float linearAttenuation; // K1
		      float quadraticAttenuation;// K2
		  };

		  uniform gl_LightSourceParameters gl_LightSource[gl_MaxLights];
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "ambient",              GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "diffuse",              GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "specular",             GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "position",             GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "halfVector",           GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "spotDirection",        GLSLTS_VEC3,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "spotExponent",         GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "spotCutoff",           GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "spotCosCutoff",        GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "constantAttenuation",  GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "linearAttenuation",    GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "quadraticAttenuation", GLSLTS_FLOAT, psRP->eBIStateFloat);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_LightSourceParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_LightSource", GLSLBV_LIGHTSOURCE, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, psCR->iGLMaxLights, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}
	
	if (ASTBICheckForUseOfState(psIL, "gl_LightModel"))
	{
		
		/*
		  struct gl_LightModelParameters
		  {
		      vec4 ambient; // Acs
		  };

		  uniform gl_LightModelParameters gl_LightModel;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "ambient",   GLSLTS_VEC4, psRP->eBIStateFloat);


		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_LightModelParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_LightModel", GLSLBV_LIGHTMODEL, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);
		
		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}


	if (ASTBICheckForUseOfState(psIL, "gl_FrontLightModelProduct") || ASTBICheckForUseOfState(psIL, "gl_BackLightModelProduct"))
	{
		
		/*
		  Derived state from products of light and material.
		  
		  struct gl_LightModelProducts 
		  {
		      vec4 sceneColor; // Derived. Ecm + Acm * Acs
		  };
		  
		  uniform gl_LightModelProducts gl_FrontLightModelProduct;
		  uniform gl_LightModelProducts gl_BackLightModelProduct;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "sceneColor",   GLSLTS_VEC4, psRP->eBIStateFloat);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_LightModelProducts", 
							   &sStructureDefinitionData, 
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_FrontLightModelProduct", GLSLBV_FRONTLIGHTMODELPRODUCT, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_BackLightModelProduct",  GLSLBV_BACKLIGHTMODELPRODUCT,  GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}

	if (ASTBICheckForUseOfState(psIL, "gl_FrontLightProduct") || ASTBICheckForUseOfState(psIL, "gl_BackLightProduct"))
	{

		/*
		  struct gl_LightProducts 
		  {
		      vec4 ambient; // Acm * Acli
		      vec4 diffuse; // Dcm * Dcli
		      vec4 specular; // Scm * Scli
		  };

		  uniform gl_LightProducts gl_FrontLightProduct[gl_MaxLights];
		  uniform gl_LightProducts gl_BackLightProduct[gl_MaxLights];
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "ambient",   GLSLTS_VEC4, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "diffuse",   GLSLTS_VEC4, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "specular",  GLSLTS_VEC4, psRP->eBIStateFloat);

		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_LightProducts",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_FrontLightProduct", GLSLBV_FRONTLIGHTPRODUCT, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, psCR->iGLMaxLights, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_BackLightProduct",  GLSLBV_BACKLIGHTPRODUCT,  GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, psCR->iGLMaxLights, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}

	/*
	  Texture Environment and Generation, p. 152, p. 40-42.

	  uniform vec4 gl_TextureEnvColor[gl_MaxTextureImageUnits];
	  uniform vec4 gl_EyePlaneS[gl_MaxTextureCoords];
	  uniform vec4 gl_EyePlaneT[gl_MaxTextureCoords];
	  uniform vec4 gl_EyePlaneR[gl_MaxTextureCoords];
	  uniform vec4 gl_EyePlaneQ[gl_MaxTextureCoords];
	  uniform vec4 gl_ObjectPlaneS[gl_MaxTextureCoords];
	  uniform vec4 gl_ObjectPlaneT[gl_MaxTextureCoords];
	  uniform vec4 gl_ObjectPlaneR[gl_MaxTextureCoords];
	  uniform vec4 gl_ObjectPlaneQ[gl_MaxTextureCoords];
	*/

	ASTBIAddGLState(psCPD, psST, psIL, "gl_TextureEnvColor", GLSLBV_TEXTUREENVCOLOR, GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureImageUnits, 0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_EyePlaneS",       GLSLBV_EYEPLANES,       GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_EyePlaneT",       GLSLBV_EYEPLANET,       GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_EyePlaneR",       GLSLBV_EYEPLANER,       GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_EyePlaneQ",       GLSLBV_EYEPLANEQ,       GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ObjectPlaneS",    GLSLBV_OBJECTPLANES,    GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ObjectPlaneT",    GLSLBV_OBJECTPLANET,    GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ObjectPlaneR",    GLSLBV_OBJECTPLANER,    GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);
	ASTBIAddGLState(psCPD, psST, psIL, "gl_ObjectPlaneQ",    GLSLBV_OBJECTPLANEQ,    GLSLTQ_UNIFORM, GLSLTS_VEC4, psRP->eBIStateFloat, 0, psCR->iGLMaxTextureCoords,     0, IMG_NULL, eProgramType);


	if (ASTBICheckForUseOfState(psIL, "gl_Fog"))
	{
		/*
		  Fog p. 161

		  struct gl_FogParameters 
		  {
		      vec4 color;
		      float density;
		      float start;
		      float end;
		      float scale; // Derived: 1.0 / (end - start)
		  };

		  uniform gl_FogParameters gl_Fog;
		*/

		ASTAddGLStructureMember(&sStructureDefinitionData, "color",   GLSLTS_VEC4,  psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "density", GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "start",   GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "end",     GLSLTS_FLOAT, psRP->eBIStateFloat);
		ASTAddGLStructureMember(&sStructureDefinitionData, "scale",   GLSLTS_FLOAT, psRP->eBIStateFloat);


		AddStructureDefinition(psCPD, psST,
							   IMG_NULL,
							   "gl_FogParameters",
							   &sStructureDefinitionData,
							   &uStructSymbolTableID);

		ASTBIAddGLState(psCPD, psST, psIL, "gl_Fog", GLSLBV_FOG, GLSLTQ_UNIFORM, GLSLTS_STRUCT, GLSLPRECQ_UNKNOWN, uStructSymbolTableID, 0, 0, IMG_NULL, eProgramType);

		ASTBIResetGLStructureDefinition(&sStructureDefinitionData);
	}

	/*
	  Unlike user-defined varying variables, the built-in varying variables don't have a strict one-to-one
	  correspondence between the vertex language and the fragment language. Two sets are provided, one for
	  each language. Their relationship is described below.
	  The following built-in varying variables are available to write to in a vertex shader. A particular one
	  should be written to if any functionality in a corresponding fragment shader or fixed pipeline uses it or
	  state derived from it. Otherwise, behavior is undefined.

	      varying vec4 gl_FrontColor;
	      varying vec4 gl_BackColor;
	      varying vec4 gl_FrontSecondaryColor;
	      varying vec4 gl_BackSecondaryColor;
	      varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
	      varying float gl_FogFragCoord;

	  For gl_FogFragCoord, the value written will be used as the "c" value on page 160 of the OpenGL 1.4
	  Specification by the fixed functionality pipeline. For example, if the z-coordinate of the fragment in eye
	  space is desired as "c", then that's what the vertex shader should write into gl_FogFragCoord.
	  As with all arrays, indices used to subscript gl_TexCoord must either be an integral constant expressions,
	  or this array must be re-declared by the shader with a size. The size can be at most gl_MaxTextureCoords.
	  Using indexes close to 0 may aid the implementation in preserving varying resources.
	  The following varying variables are available to read from in a fragment shader. The gl_Color and
	  gl_SecondaryColor names are the same names as attributes passed to the vertex shader. However, there is
	  no name conflict, because attributes are visible only in vertex shaders and the following are only visible in
	  a fragment shader.

	      varying vec4 gl_Color;
	      varying vec4 gl_SecondaryColor;
	      varying vec4 gl_TexCoord[]; // at most will be gl_MaxTextureCoords
	      varying float gl_FogFragCoord;

	  For GLES:

	      varying mediump vec2 gl_PointCoord;


	  The values in gl_Color and gl_SecondaryColor will be derived automatically by the system from
	  gl_FrontColor, gl_BackColor, gl_FrontSecondaryColor, and gl_BackSecondaryColor based on which
	  face is visible. If fixed functionality is used for vertex processing, then gl_FogFragCoord

	  For GLES:
	  The values in gl_PointCoord are two-dimensional coordinates indicating where within a point primitive
	  the current fragment is located. They range from 0.0 to 1.0 across the point. This is described in more
	  detail in Section 3.3.1 Basic Point Rasterization of version 2.0 of the OpenGL Specification, where point
	  sprites are discussed. If the current primitive is not a point, then the values read from gl_PointCoord are
	  undefined.

	*/

	if (eProgramType == GLSLPT_VERTEX)
	{
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_FrontColor",          GLSLBV_FRONTCOLOR,          GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_BackColor",           GLSLBV_BACKCOLOR,           GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_FrontSecondaryColor", GLSLBV_FRONTSECONDARYCOLOR, GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_BackSecondaryColor",  GLSLBV_BACKSECONDARYCOLOR,  GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL,     "gl_TexCoord",            GLSLBV_TEXCOORD,            GLSLTQ_VERTEX_OUT, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, -1, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, IMG_NULL, "gl_FogFragCoord",        GLSLBV_FOGFRAGCOORD,        GLSLTQ_VERTEX_OUT, GLSLTS_FLOAT, psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
	}

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddGLState(psCPD, psST, psIL, "gl_Color",          GLSLBV_COLOR,	        GLSLTQ_FRAGMENT_IN, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_SecondaryColor", GLSLBV_SECONDARYCOLOR, GLSLTQ_FRAGMENT_IN, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_TexCoord",       GLSLBV_TEXCOORD,       GLSLTQ_FRAGMENT_IN, GLSLTS_VEC4,  psRP->eBIVaryingFloat, 0, -1, 0, IMG_NULL, eProgramType);
		ASTBIAddGLState(psCPD, psST, psIL, "gl_FogFragCoord",   GLSLBV_FOGFRAGCOORD,   GLSLTQ_FRAGMENT_IN, GLSLTS_FLOAT, psRP->eBIVaryingFloat, 0, 0,  0, IMG_NULL, eProgramType);
	}
#endif /* !GLSL_ES */

	/*
	   Constructors 
	*/

	ASTBIAddConstructor(psCPD, psST, GLSLTS_FLOAT);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_VEC2);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_VEC3);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_VEC4);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_INT);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_IVEC2);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_IVEC3);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_IVEC4);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_BOOL);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_BVEC2);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_BVEC3);
	ASTBIAddConstructor(psCPD, psST, GLSLTS_BVEC4);

	/* Add all the matrix constructors */
	for (i = 0; i < GLSL_NUM_MATRICES; i++)
	{
		ASTBIAddConstructor(psCPD, psST, aeMat[i]);
	}

	/*
	  Angle and Trigonometry Functions
	  Function parameters specified as angle are assumed to be in units of radians. In no case will any of these
	  functions result in a divide by zero error. If the divisor of a ratio is 0, then results will be undefined.
	  These all operate component-wise. The description is per component.

	  genType radians (genType degrees)

	  genType degrees (genType radians)

	  genType sin (genType angle_in_radians)

	  genType cos (genType angle_in_radians)

	  genType tan (genType angle_in_radians)

	  genType asin (genType x)

	  genType acos (genType x)

	  genType atan (genType y, genType x)

	  genType atan (genType y_over_x)
	*/

	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_RADIANS, "radians", "degrees",       4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_DEGREES, "degrees", "radians",       4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_SIN,     "sin",     "angle",         4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_COS,     "cos",     "angle",         4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TAN,     "tan",     "angle",			4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ASIN,    "asin",    "x",             4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ACOS,    "acos",    "x",             4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ATAN,    "atan",    "y_over_x",      4, aeGenType, aeGenType);

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_ATAN,    "atan",    "y",        "x", 4, aeGenType, aeGenType, aeGenType);

	/*

	  Exponential Functions
	  These all operate component-wise. The description is per component.

	  genType pow (genType x, genType y)

	  genType exp (genType x)

	  genType log (genType x)

	  genType exp2 (genType x)

	  genType log2 (genType x)

	  genType sqrt (genType x)

	  genType inversesqrt (genType x)

	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_POW,         "pow",         "x", "y", 4, aeGenType, aeGenType, aeGenType);

	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_EXP,         "exp",         "x",      4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_LOG,         "log",         "x",      4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_EXP2,        "exp2",        "x",      4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_LOG2,        "log2",        "x",      4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_SQRT,        "sqrt",        "x",      4, aeGenType, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_INVERSESQRT, "inversesqrt", "x",      4, aeGenType, aeGenType);

	/*
	  Common Functions
	  These all operate component-wise. The description is per component.

	  genType abs (genType x)
	  genType sign (genType x)
	  genType floor (genType x)
	  genType ceil (genType x)
	  genType fract (genType x)
	  genType mod (genType x, float y)
	  genType mod (genType x, genType y)
	  genType min (genType x, genType y)
	  genType min (genType x, float y)
	  genType max (genType x, genType y)
	  genType max (genType x, float y)
	  genType clamp (genType x, genType minVal, genType maxVal)
	  genType clamp (genType x, float minVal,   float maxVal)
	  genType mix (genType x, genType y, genType a)
	  genType mix (genType x, genType y, float a)
	  genType step (genType edge, genType x)
	  genType step (float edge, genType x)
	  genType smoothstep (genType edge0, genType edge1, genType x)
	  genType smoothstep (float edge0, float edge1, genType x)

	*/
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ABS,        "abs",        "x",                         4, aeGenType,    aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_SIGN,       "sign",       "x",                         4, aeGenType,    aeGenType);

	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_FLOOR,      "floor",      "x",                         4, aeGenType,    aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_CEIL,       "ceil",       "x",                         4, aeGenType,    aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_FRACT,      "fract",      "x",                         4, aeGenType,    aeGenType);
	

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MOD,        "mod",        "x",     "y",                4, aeGenType,    aeGenType,    aeFloat);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MOD,        "mod",        "x",     "y",                3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MIN,        "min",        "x",     "y",                4, aeGenType,    aeGenType,    aeFloat);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MIN,        "min",        "x",     "y",                3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MAX,        "max",        "x",     "y",                4, aeGenType,    aeGenType,    aeFloat);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MAX,        "max",        "x",     "y",                3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);


	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_CLAMP,      "clamp",      "x",     "minVal", "maxVal", 3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_CLAMP,      "clamp",      "x",     "minVal", "maxVal", 4, aeGenType,    aeGenType,    aeFloat,      aeFloat);

	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_MIX,        "mix",        "x",     "y",      "a",      3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_MIX,        "mix",        "x",     "y",      "a",      4, aeGenType,    aeGenType,    aeGenType,    aeFloat);

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_STEP,       "step",       "edge",  "x",                3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_STEP,       "step",       "edge",  "x",                4, aeGenType,    aeFloat,      aeGenType);

	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_SMOOTHSTEP, "smoothstep", "edge0", "edge1",  "x",      3, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF, aeGenTypeNoF);
	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_SMOOTHSTEP, "smoothstep", "edge0", "edge1",  "x",      4, aeGenType,    aeFloat,   aeFloat,   aeGenType);


	/*
	  Geometric Functions
	  These operate on vectors as vectors, not component-wise.

	  float   length (genType x)
	  float   distance (genType p0, genType p1)
	  float   dot (genType x, genType y)
	  vec3    cross (vec3 x, vec3 y)
	  genType normalize (genType x)
	  vec4    ftransform()
	  genType faceforward (genType N, genType I, genType Nref)

	  genType reflect (genType I, genType N)
	  genType refract(genType I, genType N, float eta)

	*/
	
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_LENGTH,      "length",      "x",               4, aeFloat,   aeGenType);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_DISTANCE,    "distance",    "p0", "p1",        4, aeFloat,   aeGenType, aeGenType);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_DOT,         "dot",         "x",  "y",         4, aeFloat,   aeGenType, aeGenType);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_CROSS,       "cross",       "x",  "y",         1, aeVec3,    aeVec3,    aeVec3);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NORMALIZE,   "normalize",   "x",               4, aeGenType, aeGenType);
	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_FACEFORWARD, "faceforward", "N",  "I", "Nref", 4, aeGenType, aeGenType, aeGenType, aeGenType);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_REFLECT,     "reflect",     "I",  "N",         4, aeGenType, aeGenType, aeGenType);
	ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_REFRACT,     "refract",     "I",  "N", "eta",  4, aeGenType, aeGenType, aeGenType, aeFloat);

#ifndef GLSL_ES
	if (eProgramType == GLSLPT_VERTEX)
	{
		ASTBIAddFunction(psCPD, psST, psIL, GLSLBFID_FTRANSFORM,  "ftransform", IMG_NULL, 1, 0, aeVec4, IMG_NULL);
	}
#endif
	

	/*
	  Matrix Functions

	  mat matrixCompMult (mat x, mat y)

	  mat2x2 transpose(mat2x2 m)
	  mat2x3 transpose(mat3x2 m)
	  mat2x4 transpose(mat4x2 m)
	  mat3x2 transpose(mat2x3 m)
	  mat3x3 transpose(mat3x3 m)
	  mat3x4 transpose(mat4x3 m)
	  mat4x2 transpose(mat2x4 m)
	  mat4x3 transpose(mat3x4 m)
	  mat4x4 transpose(mat4x4 m)

	  mat2x2 outerProduct (vec2 c, vec2 r)
	  mat2x3 outerProduct (vec3 c, vec2 r)
	  mat2x4 outerProduct (vec4 c, vec2 r)
	  mat3x2 outerProduct (vec2 c, vec3 r)
	  mat3x3 outerProduct (vec3 c, vec3 r)
	  mat3x4 outerProduct (vec4 c, vec3 r)
	  mat4x2 outerProduct (vec2 c, vec4 r)
	  mat4x3 outerProduct (vec3 c, vec4 r)
	  mat4x4 outerProduct (vec4 c, vec4 r)	  
	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_MATRIXCOMPMULT, "matrixCompMult", "x", "y", GLSL_NUM_MATRICES, aeMat, aeMat, aeMat);

#if !defined(GLSL_ES)

	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[0], &aeMat[0]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[1], &aeMat[3]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[2], &aeMat[6]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[3], &aeMat[1]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[4], &aeMat[4]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[5], &aeMat[7]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[6], &aeMat[2]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[7], &aeMat[5]);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_TRANSPOSE,      "transpose", "m", 1, &aeMat[8], &aeMat[8]);

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_OUTERPRODUCT,   "outerProduct", "c", "r", 3, &aeMat[0], aeGenTypeNoF, aeVec2);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_OUTERPRODUCT,   "outerProduct", "c", "r", 3, &aeMat[3], aeGenTypeNoF, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_OUTERPRODUCT,   "outerProduct", "c", "r", 3, &aeMat[6], aeGenTypeNoF, aeVec4);

#endif

	/*
	  Vector Relational Functions

	  bvec lessThan(vec x, vec y)
	  bvec lessThan(ivec x, ivec y)

	  bvec lessThanEqual(vec x, vec y)
	  bvec lessThanEqual(ivec x, ivec y)

	  bvec greaterThanEqual(vec x, vec y)
	  bvec greaterThanEqual(ivec x, ivec y)
	 
	  bvec equal(vec x, vec y)
	  bvec equal(ivec x, ivec y)
	  bvec equal(bvec x, bvec y)
	  bvec notEqual(vec x, vec y)
	  bvec notEqual(ivec x, ivec y)
	  bvec notEqual(bvec x, bvec y)

	  bool any(bvec x)
	  bool all(bvec x)
	  bvec not(bvec x)

	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_LESSTHAN,         "lessThan",         "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_LESSTHAN,         "lessThan",         "x", "y", 3, aeBVec, aeIVec, aeIVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_LESSTHANEQUAL,    "lessThanEqual",    "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_LESSTHANEQUAL,    "lessThanEqual",    "x", "y", 3, aeBVec, aeIVec, aeIVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_GREATERTHAN,      "greaterThan",      "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_GREATERTHAN,      "greaterThan",      "x", "y", 3, aeBVec, aeIVec, aeIVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_GREATERTHANEQUAL, "greaterThanEqual", "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_GREATERTHANEQUAL, "greaterThanEqual", "x", "y", 3, aeBVec, aeIVec, aeIVec);
	
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_EQUAL,            "equal",            "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_EQUAL,            "equal",            "x", "y", 3, aeBVec, aeIVec, aeIVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_EQUAL,            "equal",            "x", "y", 3, aeBVec, aeBVec, aeBVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_NOTEQUAL,         "notEqual",         "x", "y", 3, aeBVec, aeVec,  aeVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_NOTEQUAL,         "notEqual",         "x", "y", 3, aeBVec, aeIVec, aeIVec);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_NOTEQUAL,         "notEqual",         "x", "y", 3, aeBVec, aeBVec, aeBVec);
	
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ANY,              "any",              "x",      3, aeBool, aeBVec);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_ALL,              "all",              "x",      3, aeBool, aeBVec);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NOT,              "not",              "x",      3, aeBVec, aeBVec);	

	/*
	  Texture Lookup Functions

	  1D textures

	  vec4 texture1D (sampler1D sampler, float coord [, float bias] )
	  vec4 texture1DProj (sampler1D sampler, vec2 coord [, float bias] )
	  vec4 texture1DProj (sampler1D sampler, vec4 coord [, float bias] )
	  vec4 texture1DLod (sampler1D sampler, float coord, float lod)
	  vec4 texture1DProjLod (sampler1D sampler, vec2 coord, float lod)
	  vec4 texture1DProjLod (sampler1D sampler, vec4 coord, float lod)
	*/

#if !defined(GLSL_ES)

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture1D",		"sampler", "coord",			1, aeVec4, aeSampler1D, aeFloat);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture1DProj",	"sampler", "coord",			1, aeVec4, aeSampler1D, aeVec2);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture1DProj",	"sampler", "coord",			1, aeVec4, aeSampler1D, aeVec4);

	/* Add bias variants */
	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture1D",		"sampler", "coord", "bias",	1, aeVec4, aeSampler1D, aeFloat, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture1DProj",	"sampler", "coord", "bias",	1, aeVec4, aeSampler1D, aeVec2, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture1DProj",	"sampler", "coord", "bias",	1, aeVec4, aeSampler1D, aeVec4, aeFloat);
	}

	/* Add LOD variants */
	if (eProgramType == GLSLPT_VERTEX || (eExtendFunc & GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT))
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"texture1DLod",		"sampler", "coord", "lod",  1, aeVec4, aeSampler1D, aeFloat, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture1DProjLod", "sampler", "coord", "lod",  1, aeVec4, aeSampler1D, aeVec2,  aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture1DProjLod", "sampler", "coord", "lod",  1, aeVec4, aeSampler1D, aeVec4,  aeFloat);
	}

	/* Add gradient variants */
	if (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS)
	{
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,		"texture1DGradARB",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler1D, aeFloat, aeFloat, aeFloat);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture1DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler1D, aeVec2,  aeFloat, aeFloat);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture1DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler1D, aeVec4,  aeFloat, aeFloat);
	}

#endif /* !defined GLSL_ES */

	/*
	  2D textures

	  vec4 texture2D (sampler2D sampler, vec2 coord [, float bias] )
	  vec4 texture2DProj (sampler2D sampler, vec3 coord [, float bias] )
	  vec4 texture2DProj (sampler2D sampler, vec4 coord [, float bias] )
	  vec4 texture2DLod (sampler2D sampler, vec2 coord, float lod)
	  vec4 texture2DProjLod (sampler2D sampler, vec3 coord, float lod)
	  vec4 texture2DProjLod (sampler2D sampler, vec4 coord, float lod)
	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture2D",		"sampler", "coord",			1, aeVec4, aeSampler2D, aeVec2);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord",			1, aeVec4, aeSampler2D, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord",			1, aeVec4, aeSampler2D, aeVec4);

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURERECT,		"texture2DRect",		"sampler", "coord",			1, aeVec4, aeSampler2DRect, aeVec2);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURERECTPROJ,	"texture2DRectProj",	"sampler", "coord",			1, aeVec4, aeSampler2DRect, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURERECTPROJ,	"texture2DRectProj",	"sampler", "coord",			1, aeVec4, aeSampler2DRect, aeVec4);
#endif

	/* Add bias variants */
	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture2D",		"sampler", "coord", "bias",	1, aeVec4, aeSampler2D, aeVec2, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord", "bias",	1, aeVec4, aeSampler2D, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord", "bias",	1, aeVec4, aeSampler2D, aeVec4, aeFloat);
	}

	/* Add LOD variants */
	if (eProgramType == GLSLPT_VERTEX || (eExtendFunc & GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT))
	{
#if defined(GLSL_ES)
		if(eProgramType == GLSLPT_FRAGMENT)
		{
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"texture2DLodEXT",		"sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec2, aeFloat);
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture2DProjLodEXT", "sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec3, aeFloat);
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture2DProjLodEXT", "sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec4, aeFloat);
		}
		else
#endif
		{
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"texture2DLod",		"sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec2, aeFloat);
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture2DProjLod", "sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec3, aeFloat);
			ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture2DProjLod", "sampler", "coord", "lod",  1, aeVec4, aeSampler2D, aeVec4, aeFloat);
		}
	}

	/* Add gradient variants */
	if (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS)
	{
#if defined(GLSL_ES)
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,		"texture2DGradEXT",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler2D, aeVec2,  aeVec2, aeVec2);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture2DProjGradEXT",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler2D, aeVec3,  aeVec2, aeVec2);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture2DProjGradEXT",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler2D, aeVec4,  aeVec2, aeVec2);
#else
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,		"texture2DGradARB",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler2D, aeVec2,  aeVec2, aeVec2);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture2DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler2D, aeVec3,  aeVec2, aeVec2);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture2DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler2D, aeVec4,  aeVec2, aeVec2);
#endif
	}

#if !defined(GLSL_ES)

	/*
	  3D textures

	  vec4 texture3D (sampler3D sampler, vec3 coord [, float bias] )
	  vec4 texture3DProj (sampler3D sampler, vec4 coord [, float bias] )
	  vec4 texture3DLod (sampler3D sampler, vec3 coord, float lod)
	  vec4 texture3DProjLod (sampler3D sampler, vec4 coord, float lod)
	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture3D",		"sampler", "coord",			1, aeVec4, aeSampler3D, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture3DProj",	"sampler", "coord",			1, aeVec4, aeSampler3D, aeVec4);

	/* Add bias variants */
	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture3D",		"sampler", "coord", "bias",	1, aeVec4, aeSampler3D, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture3DProj",	"sampler", "coord", "bias",	1, aeVec4, aeSampler3D, aeVec4, aeFloat);
	}

	/* Add LOD variants */
	if (eProgramType == GLSLPT_VERTEX || (eExtendFunc & GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT))
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"texture3DLod",		"sampler", "coord", "lod",  1, aeVec4, aeSampler3D, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"texture3DProjLod", "sampler", "coord", "lod",  1, aeVec4, aeSampler3D, aeVec4, aeFloat);
	}

	/* Add gradient variants */
	if (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS)
	{
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,		"texture3DGradARB",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler3D, aeVec3,  aeVec3, aeVec3);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,	"texture3DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler3D, aeVec4,  aeVec3, aeVec3);
	}

#endif /* !defined GLSL_ES */

	/*
	  Cube map texture look up 

	  vec4 textureCube (samplerCube sampler, vec3 coord [, float bias] )
	  vec4 textureCubeLod (samplerCube sampler, vec3 coord, float lod)
	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,			"textureCube",		"sampler", "coord",			1, aeVec4, aeSamplerCube, aeVec3);

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"textureCube",		  "sampler", "coord", "bias",	1, aeVec4, aeSamplerCube, aeVec3, aeFloat);
	}
	
	if (eProgramType == GLSLPT_VERTEX || (eExtendFunc & GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT))
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,	"textureCubeLod",	  "sampler", "coord", "lod",  1, aeVec4, aeSamplerCube, aeVec3, aeFloat);
	}

	/* Add gradient variant */
	if (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS)
	{
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,	"textureCubeGradARB", "sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSamplerCube, aeVec3,  aeVec3, aeVec3);
	}


#if !defined(GLSL_ES)
	/* 
	  Depth texture comparison
	
	  vec4 shadow1D (sampler1DShadow sampler, vec3 coord [, float bias] )
	  vec4 shadow2D (sampler2DShadow sampler, vec3 coord [, float bias] )
	  vec4 shadow1DProj (sampler1DShadow sampler, vec4 coord [, float bias] )
	  vec4 shadow2DProj (sampler2DShadow sampler, vec4 coord [, float bias] )
	  vec4 shadow1DLod (sampler1DShadow sampler, vec3 coord, float lod)
	  vec4 shadow2DLod (sampler2DShadow sampler, vec3 coord, float lod)
	  vec4 shadow1DProjLod(sampler1DShadow sampler, vec4 coord, float lod)
	  vec4 shadow2DProjLod(sampler2DShadow sampler, vec4 coord, float lod)
	*/

	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"shadow1D",			"sampler", "coord",			1, aeVec4, aeSampler1DShadow, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"shadow2D",			"sampler", "coord",			1, aeVec4, aeSampler2DShadow, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"shadow1DProj",     "sampler", "coord",			1, aeVec4, aeSampler1DShadow, aeVec4);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"shadow2DProj",     "sampler", "coord",			1, aeVec4, aeSampler2DShadow, aeVec4);

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURERECT,		"shadow2DRect",		"sampler", "coord",			1, aeVec4, aeSampler2DRectShadow, aeVec3);
	ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURERECTPROJ,	"shadow2DRectProj", "sampler", "coord",			1, aeVec4, aeSampler2DRectShadow, aeVec4);
#endif

	/* Add bias variants */
	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"shadow1D",			"sampler", "coord", "bias",	1, aeVec4, aeSampler1DShadow, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"shadow2D",			"sampler", "coord", "bias",	1, aeVec4, aeSampler2DShadow, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"shadow1DProj",     "sampler", "coord", "bias",	1, aeVec4, aeSampler1DShadow, aeVec4, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"shadow2DProj",     "sampler", "coord", "bias",	1, aeVec4, aeSampler2DShadow, aeVec4, aeFloat);
	}

	/* Add LOD variants */
	if (eProgramType == GLSLPT_VERTEX || (eExtendFunc & GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT))
	{
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"shadow1DLod",		"sampler", "coord", "lod",  1, aeVec4, aeSampler1DShadow, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTURELOD,		"shadow2DLod",		"sampler", "coord", "lod",  1, aeVec4, aeSampler2DShadow, aeVec3, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"shadow1DProjLod",	"sampler", "coord", "lod",  1, aeVec4, aeSampler1DShadow, aeVec4, aeFloat);
		ASTBIAddFunction3(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJLOD,	"shadow2DProjLod",	"sampler", "coord", "lod",  1, aeVec4, aeSampler2DShadow, aeVec4, aeFloat);
	}

	/* Add gradient variants */
	if (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS)
	{
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,	"shadow1DGradARB",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler1DShadow, aeVec3,  aeFloat, aeFloat);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,"shadow1DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler1DShadow, aeVec4,  aeFloat, aeFloat);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREGRAD,	"shadow2DGradARB",		"sampler", "P", "dPdx", "dPdy", 1, aeVec4, aeSampler2DShadow, aeVec3,  aeVec2,  aeVec2);
		ASTBIAddFunction4(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJGRAD,"shadow2DProjGradARB",	"sampler", "P", "dPdx", "dPdy",	1, aeVec4, aeSampler2DShadow, aeVec4,  aeVec2,  aeVec2);
	}

#endif /* !defined GLSL_ES */


#if defined(GLSL_ES) || defined(OGL_TEXTURE_STREAM)
	/*
	  Stream textures

	  vec4 textureStreamIMG (samplerStreamIMG sampler, vec2 coord)
	  vec4 textureStreamProjIMG (samplerStreamIMG sampler, vec3 coord)
	  vec4 textureStreamProjIMG (samplerStreamIMG sampler, vec4 coord)
	*/

	if ((eProgramType == GLSLPT_FRAGMENT) && (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_STREAM))
	{
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURESTREAM,		"textureStreamIMG",		"sampler", "coord",			1, aeVec4, aeSamplerStream, aeVec2);
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURESTREAMPROJ,	"textureStreamProjIMG",	"sampler", "coord",			1, aeVec4, aeSamplerStream, aeVec3);
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURESTREAMPROJ,	"textureStreamProjIMG",	"sampler", "coord",			1, aeVec4, aeSamplerStream, aeVec4);
	}

#endif /* defined(GLSL_ES) || defined(OGL_TEXTURE_STREAM) */

#if defined(GLSL_ES)
	/*
	  External textures

	  vec4 texture2D (samplerExternalOES sampler, vec2 coord)
	  vec4 texture2DProj (samplerExternalOES sampler, vec3 coord)
	  vec4 texture2DProj (samplerExternalOES sampler, vec4 coord)
	*/

	if ((eProgramType == GLSLPT_FRAGMENT) && (eExtendFunc & GLSL_EXTENDFUNC_SUPPORT_TEXTURE_EXTERNAL))
	{
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTURE,		"texture2D",		"sampler", "coord",			1, aeVec4, aeSamplerExternal, aeVec2);
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord",			1, aeVec4, aeSamplerExternal, aeVec3);
		ASTBIAddFunction2(psCPD, psST, psIL, GLSLBFID_TEXTUREPROJ,	"texture2DProj",	"sampler", "coord",			1, aeVec4, aeSamplerExternal, aeVec4);
	}

#endif /* defined(GLSL_ES) || defined(OGL_TEXTURE_STREAM) */


	/*
	  Fragment (only) Processing Functions

	  genType dFdx (genType p)
	  genType dFdy (genType p)
	  genType fwidth (genType p)

	*/

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_DFDX,    "dFdx",   "p", 4, aeGenType, aeGenType);
		ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_DFDY,    "dFdy",   "p", 4, aeGenType, aeGenType);
		ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_FWIDTH,  "fwidth", "p", 4, aeGenType, aeGenType);
	}

	/*
	  Noise Functions

	  float noise1 (genType x)
	  vec2 noise2 (genType x)
	  vec3 noise3 (genType x)
	  vec4 noise4 (genType x)
	
	*/

	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NOISE1, "noise1", "x", 4, aeFloat, aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NOISE2, "noise2", "x", 4, aeVec2,  aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NOISE3, "noise3", "x", 4, aeVec3,  aeGenType);
	ASTBIAddFunction1(psCPD, psST, psIL, GLSLBFID_NOISE4, "noise4", "x", 4, aeVec4,  aeGenType);


	ASTBIFreeIdentifierList(psIL);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTBIResetBuiltInData
 * Inputs       : psSymbolTable, psBuiltInsReferenced
 * Outputs      : psSymbolTable, psBuiltInsReferenced
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Resets the identifier usage of the builtins in the list.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTBIResetBuiltInData(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, GLSLIdentifierList *psBuiltInsReferenced)
{
	IMG_UINT32 i;

	for (i = 0; i < psBuiltInsReferenced->uNumIdentifiersReferenced; i++)
	{
		/* Get the data */
		GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psSymbolTable,
																  psBuiltInsReferenced->puIdentifiersReferenced[i]);


		if (psIdentifierData)
		{
			IMG_CHAR *pszSymbolName = GetSymbolName(psSymbolTable, psBuiltInsReferenced->puIdentifiersReferenced[i]);

			/* Reset the identifier usage flags */
			psIdentifierData->eIdentifierUsage = GLSLIU_BUILT_IN;
			psIdentifierData->iActiveArraySize = -1;

			/* return values are always 'written' - better was of doing this? */
			if (strncmp(pszSymbolName, RETURN_VAL_STRING, RETURN_VAL_STRING_LENGTH) == 0)
			{
				psIdentifierData->eIdentifierUsage |= GLSLIU_WRITTEN; 
			}
		
			switch (psIdentifierData->eBuiltInVariableID)
			{
				case GLSLBV_TEXCOORD:
				{
					psIdentifierData->sFullySpecifiedType.iArraySize = -1;
					psIdentifierData->eArrayStatus                   = GLSLAS_ARRAY_SIZE_NOT_FIXED;
					break;
				}
				default:
					break;
			}
		}
		else
		{
			printf("ASTBIResetBuiltInData: Failed to retrieve data for %08X\n", 
							 psBuiltInsReferenced->puIdentifiersReferenced[i]);
		}
	}

	/* Reset the list counter */
	psBuiltInsReferenced->uNumIdentifiersReferenced = 0;

	return IMG_TRUE;
}
/******************************************************************************
 End of file (astbuiltin.c)
******************************************************************************/
