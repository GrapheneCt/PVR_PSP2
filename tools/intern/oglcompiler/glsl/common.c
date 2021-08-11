/******************************************************************************
 * Name         : common.c
 * Author       : James McCarthy
 * Created      : 20/09/2004
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
 * $Log: common.c $
 *****************************************************************************/

#include "glsltree.h"
#include "common.h"
#include "icgen.h"
#include "../parser/debug.h"
#include "../parser/symtab.h"
#include "error.h"

#include "string.h"
#include <stdio.h>

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTValidateNodeCreationFn(GLSLCompilerPrivateData *psCPD,
												GLSLNode   *psCurrentNode,
												IMG_UINT32 uLineNumber,
												IMG_CHAR   *pszFileName)
{
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	if (!psCurrentNode)
	{
		LOG_INTERNAL_ERROR(("Failed to create node (%s line:%u)\n",
						 pszFileName,
						 uLineNumber));
		
		return IMG_FALSE;
	}

	return IMG_TRUE;	
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL GLSLNode *ASTCreateNewNodeFn(GLSLTreeContext		*psGLSLTreeContext,
											GLSLNodeType		eNodeType,
											ParseTreeEntry		*psParseTreeEntry,
											IMG_UINT32			uLineNumber,
											IMG_CHAR			*pszFileName)
{	
	GLSLNode *psNode = DebugMemAlloc2(sizeof(GLSLNode), uLineNumber, pszFileName);
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	if (!psNode)
	{
		LOG_INTERNAL_ERROR(("Failed to alloc node (%s line:%u)\n",
						 pszFileName,
						 uLineNumber));
		return IMG_NULL;
	}

	psNode->eNodeType				= eNodeType;
	psNode->uNumChildren			= 0;
	psNode->psParent				= IMG_NULL;
	psNode->ppsChildren				= IMG_NULL;
	psNode->uSymbolTableID			= 0;
	psNode->bEvaluated				= IMG_FALSE;
	psNode->psParseTreeEntry		= psParseTreeEntry;
	psNode->psNext					= psGLSLTreeContext->psNodeList;
	psGLSLTreeContext->psNodeList	= psNode;
	
#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_ASTREE,
				   0,
				   "-> %02d %-56s (%d : %s)\n",
				   eNodeType,
				   NODETYPE_DESC(eNodeType),
				   uLineNumber,
				   pszFileName);
#endif

	return psNode;
}

/******************************************************************************
 * Function Name: ASTAddNodeChildFn
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTAddNodeChildFn(GLSLCompilerPrivateData *psCPD,
										GLSLNode     *psCurrentNode,
										GLSLNode     *psChildNode,
										IMG_UINT32    uLineNumber,
										IMG_CHAR     *pszFileName)
{
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	if (psCurrentNode == psChildNode)
	{
		LOG_INTERNAL_ERROR(("ASTCreateNodeChildrenFn: Child cannot be parent!, file:%s, line:%u\n",
						 pszFileName,
						 uLineNumber));
		
		return IMG_FALSE;
	}

	/* Add space to the existing array of child node pointers */
	psCurrentNode->ppsChildren = DebugMemRealloc2(psCurrentNode->ppsChildren,
												  (sizeof(GLSLNode *) * (psCurrentNode->uNumChildren + 1)),
												  uLineNumber, 
												  pszFileName);
	
	/* Check allocation succeeded */
	if (!psCurrentNode->ppsChildren)
	{
		LOG_INTERNAL_ERROR(("ASTCreateNodeChildrenFn: Failed to alloc memory for child node array, file:%s, line:%u\n",
						 pszFileName,
						 uLineNumber));
		
		return IMG_FALSE;
	}

	/* Add this new child node */
	psCurrentNode->ppsChildren[psCurrentNode->uNumChildren] = psChildNode;

	/*
	   Make this node the parent of the child node
	   (have to check if it exists first as it valid to add a NULL node 
	*/
	if (psChildNode)
	{
		psChildNode->psParent = psCurrentNode;
	}

	/* Increase the number of children */
	psCurrentNode->uNumChildren++;


#ifdef DUMP_LOGFILES 
	if (psChildNode)
	{
		DumpLogMessage(LOGFILE_ASTREE, 
					   0,
					   "++ %02d %-23s to node %02d %-23s  (%d : %s)\n",
					   psChildNode->eNodeType,
					   NODETYPE_DESC(psChildNode->eNodeType),
					   psCurrentNode->eNodeType,
					   NODETYPE_DESC(psCurrentNode->eNodeType),
					   uLineNumber,
					   pszFileName);
	}
#endif

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: FreeConstantData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreeConstantData(IMG_VOID *pvData)
{
	if (pvData)
	{
		DebugMemFree(pvData);
	}
}

/******************************************************************************
 * Function Name: AddGenericData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL AddGenericData(GLSLCompilerPrivateData *psCPD,
							   SymTable                *psSymbolTable,
								IMG_CHAR                *pszName,
								IMG_VOID                *pvOriginalData,
								IMG_UINT32              uDataSize,
								GLSLSymbolTableDataType eSymbolDataType,
								IMG_BOOL                 bAllowDuplicates,
								IMG_UINT32              *puSymbolID)
{
	IMG_VOID *pvData;

	GLSLGenericData *psGenericData;

	pvData = DebugMemAlloc(uDataSize);

	if (!pvData)
	{
		LOG_INTERNAL_ERROR(("AddGenericData: Failed to allocate memory for data for '%s'\n",pszName));
		return IMG_FALSE;
	}

	/* copy the data */
	memcpy(pvData, pvOriginalData, uDataSize);
	
	psGenericData = (GLSLGenericData *)pvData;

	if (psGenericData->eSymbolTableDataType != eSymbolDataType)
	{
		LOG_INTERNAL_ERROR(("AddGenericData: Symbol data type incorrect for '%s'\n", pszName));
		
		psGenericData->eSymbolTableDataType = eSymbolDataType;
	}

	/* Add the data to the symbol table */
	if(!AddSymbol(psSymbolTable,
				  pszName,
				  pvData,
				  uDataSize,
				  bAllowDuplicates,
				  puSymbolID,
				  FreeConstantData))
	{
		LOG_INTERNAL_ERROR(("AddGenericData: Failed to add data to symbol table for '%s'\n", pszName));

		*puSymbolID = 0;
		return IMG_FALSE;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: FreeIdentifierData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreeIdentifierData(IMG_VOID *pvData)
{
	GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData *)pvData;

	if (psIdentifierData)
	{
		if (psIdentifierData->pvConstantData)
		{
			DebugMemFree(psIdentifierData->pvConstantData);
		}

		DebugMemFree(psIdentifierData);
	}
}

#ifdef DUMP_LOGFILES
/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID *DumpConstantDataInfo(GLSLCompilerPrivateData *psCPD,
									  SymTable               *psSymbolTable,
									   GLSLFullySpecifiedType *psFullySpecifiedType,
									   IMG_VOID               *pvConstantData)
{
	IMG_UINT32 i = 0;

	IMG_UINT32 uNumComponents = GLSLTypeSpecifierNumElementsTable(psFullySpecifiedType->eTypeSpecifier);

	if (psFullySpecifiedType->iArraySize)
	{
		uNumComponents *= psFullySpecifiedType->iArraySize;
	}

	switch (psFullySpecifiedType->eTypeSpecifier)
	{
		case GLSLTS_IVEC4:
		case GLSLTS_IVEC3:
		case GLSLTS_IVEC2:
		case GLSLTS_INT:
		{
			for (i = 0; i < uNumComponents; i++)
			{
				DumpLogMessage(LOGFILE_SYMTABLE_DATA, 
							   0, 
							   "%d%s ",  
							   ((IMG_INT32 *)pvConstantData)[i], 
							   i < uNumComponents - 1 ? "," : "\0");
			}
			
			return (IMG_VOID *)&(((IMG_INT32 *)pvConstantData)[i]);
		}
		case GLSLTS_BVEC4:
		case GLSLTS_BVEC3:
		case GLSLTS_BVEC2:
		case GLSLTS_BOOL:
		{
			for (i = 0; i < uNumComponents; i++)
			{
				DumpLogMessage(LOGFILE_SYMTABLE_DATA,
							   0,
							   "%s%s ",
							   ((IMG_BOOL *)pvConstantData)[i] ? "true" : "false",
							   i < uNumComponents - 1 ? "," : "\0");
			}

			return (IMG_VOID *)&(((IMG_BOOL *)pvConstantData)[i++]);
		}
		case GLSLTS_VEC4:
		case GLSLTS_VEC3: 
		case GLSLTS_VEC2:
		case GLSLTS_FLOAT:
		case GLSLTS_MAT4X4:
		case GLSLTS_MAT4X3:
		case GLSLTS_MAT4X2:
		case GLSLTS_MAT3X4:
		case GLSLTS_MAT3X3:
		case GLSLTS_MAT3X2:
		case GLSLTS_MAT2X4:
		case GLSLTS_MAT2X3:
		case GLSLTS_MAT2X2:
		{
			for (i = 0; i < uNumComponents; i++)
			{
				DumpLogMessage(LOGFILE_SYMTABLE_DATA, 
							   0, 
							   "%.3f%s ",  
							   ((IMG_FLOAT *)pvConstantData)[i], 
							   i < uNumComponents - 1 ? "," : "\0");
			}

			return (IMG_VOID *)&(((IMG_FLOAT *)pvConstantData)[i]);
		}
		case GLSLTS_STRUCT:
		{
			IMG_VOID *pvCurrentConstantData = pvConstantData;

			GLSLStructureDefinitionData *psStructureDefinition =
				(GLSLStructureDefinitionData *)GetAndValidateSymbolTableData(psSymbolTable,
																			 psFullySpecifiedType->uStructDescSymbolTableID,
																			 GLSLSTDT_STRUCTURE_DEFINITION);
			
			DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "{");
			
			for (i = 0; i < psStructureDefinition->uNumMembers; i++)
			{
				pvCurrentConstantData  = DumpConstantDataInfo(psCPD, psSymbolTable,
															  &(psStructureDefinition->psMembers[i].sIdentifierData.sFullySpecifiedType),
															  pvCurrentConstantData);
			} 

			DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "} ");

			return pvCurrentConstantData;
		}
		default:
			DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "Unrecognised");
			return pvConstantData;
	}
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID DumpFunctionCallInfo(GLSLCompilerPrivateData *psCPD,
									 SymTable               *psSymbolTable,
									IMG_UINT32              uSymbolID)
{
	IMG_CHAR *pszName = GetSymbolName(psSymbolTable, uSymbolID);
	
	GLSLFunctionCallData *psFunctionCallData = GetSymbolTableData(psSymbolTable, uSymbolID);

	if (!psFunctionCallData)
	{
		return;
	}

	if (psFunctionCallData->eSymbolTableDataType != GLSLSTDT_FUNCTION_CALL)
	{
		return;
	}

	/* Dump this info */
	DumpLogMessage(LOGFILE_SYMTABLE_DATA,
				   0,
				   "FC %08X %02X %-46s %-7s %-17s %-7s %-27s    -/%-4d %-16s\n",
				   uSymbolID,
				   psFunctionCallData->eSymbolTableDataType,
				   pszName,
				   GLSLParameterQualifierFullDescTable[psFunctionCallData->sFullySpecifiedType.eParameterQualifier],
				   GLSLTypeQualifierFullDescTable(psFunctionCallData->sFullySpecifiedType.eTypeQualifier),
				   GLSLPrecisionQualifierFullDescTable[psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier],
				   GLSLTypeSpecifierFullDescTable(psFunctionCallData->sFullySpecifiedType.eTypeSpecifier),
				   psFunctionCallData->sFullySpecifiedType.iArraySize,
				   GLSLArrayStatusFullDescTable[psFunctionCallData->eArrayStatus]);
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DumpIdentifierInfo(GLSLCompilerPrivateData *psCPD,
										 SymTable               *psSymbolTable,
										IMG_UINT32              uSymbolID)
{
	IMG_CHAR *pszName = GetSymbolName(psSymbolTable, uSymbolID);

	GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psSymbolTable, uSymbolID);

	if (!psIdentifierData)
	{
		return;
	}

	if (psIdentifierData->eSymbolTableDataType == GLSLSTDT_FUNCTION_CALL)
	{
		DumpFunctionCallInfo(psCPD, psSymbolTable, uSymbolID);

		return;
	}

	if (psIdentifierData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
	{
		return;
	}

	/* Dump this info */
	DumpLogMessage(LOGFILE_SYMTABLE_DATA, 
				   0, 
				   "ID %08X %02X %-46s %-7s %-17s %-7s %-27s %4d/%-4d %-16s %-15s %02X %-1s%-1s%-1s%-1s%-1s%-1s%-1s%-1s%-1s %-5d",
				   uSymbolID,
				   psIdentifierData->eSymbolTableDataType,
				   pszName,
				   GLSLParameterQualifierFullDescTable[psIdentifierData->sFullySpecifiedType.eParameterQualifier],
				   GLSLTypeQualifierFullDescTable(psIdentifierData->sFullySpecifiedType.eTypeQualifier),
				   GLSLPrecisionQualifierFullDescTable[psIdentifierData->sFullySpecifiedType.ePrecisionQualifier],
				   GLSLTypeSpecifierFullDescTable(psIdentifierData->sFullySpecifiedType.eTypeSpecifier),
				   psIdentifierData->iActiveArraySize,
				   psIdentifierData->sFullySpecifiedType.iArraySize,
				   GLSLArrayStatusFullDescTable[psIdentifierData->eArrayStatus],
				   GLSLLValueStatusFullDescTable[psIdentifierData->eLValueStatus],
				   psIdentifierData->eBuiltInVariableID,
				   psIdentifierData->eIdentifierUsage & GLSLIU_WRITTEN			   ? "W\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_READ				   ? "R\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_CONDITIONAL		   ? "C\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_PARAM			   ? "P\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_DYNAMICALLY_INDEXED ? "D\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_INTERNALRESULT      ? "I\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_UNINITIALISED       ? "U\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_ERROR_INITIALISING  ? "E\0" : "_\0",
				   psIdentifierData->eIdentifierUsage & GLSLIU_BUILT_IN            ? "B\0" : "_\0",
				   psIdentifierData->uConstantDataSize);
	
	if (psIdentifierData->uConstantDataSize)
	{
		DumpConstantDataInfo(psCPD, psSymbolTable, &(psIdentifierData->sFullySpecifiedType), psIdentifierData->pvConstantData);
	}

	DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "\n");
}

#endif


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL AddIdentifierDataToTable(GLSLCompilerPrivateData *psCPD, 
										 SymTable                *psSymbolTable,
										 IMG_CHAR                *pszName, 
										 GLSLIdentifierData      *psIdentifierData,
										 IMG_BOOL                 bAllowDuplicates, 
										 IMG_UINT32              *puSymbolID)
				{
	GLSLIdentifierData *psData = DebugMemAlloc(sizeof(GLSLIdentifierData));

	if (!psData)
	{
		LOG_INTERNAL_ERROR(("AddIdenitiferDataToTable: Failed to allocate memory for data for '%s'\n",pszName));
		return IMG_FALSE;
	}

	if (psIdentifierData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
	{
		LOG_INTERNAL_ERROR(("AddIdenitiferDataToTable: Symbol table data type for '%s' is incorrect (%08X)\n",
						 pszName,
						 psIdentifierData->eSymbolTableDataType));
		return IMG_FALSE;
	}

	/* Copy the data */
	*psData = *psIdentifierData;

	if (psIdentifierData->pvConstantData)
	{
		psData->pvConstantData = DebugMemAlloc(psIdentifierData->uConstantDataSize);

		if (!psData->pvConstantData)
		{
			LOG_INTERNAL_ERROR((
				"AddIdenitiferDataToTable: Failed to allocate memory for constant data for '%s'\n",pszName));
			return IMG_FALSE;
		}
	}

	/* copy the constant data */
	memcpy(psData->pvConstantData, psIdentifierData->pvConstantData, psIdentifierData->uConstantDataSize);
	
	/* Add the data to the symbol table */
	if(!AddSymbol(psSymbolTable, 
				  pszName, 
				  psData, 
				  sizeof(GLSLIdentifierData), 
				  bAllowDuplicates, 
				  puSymbolID,
				  FreeIdentifierData))
	{
		LOG_INTERNAL_ERROR(("AddIdenitiferDataToTable: Failed to add data to symbol table for '%s'\n", pszName));

		*puSymbolID = 0;
		return IMG_FALSE;
	}

#ifdef DUMP_LOGFILES
	DumpIdentifierInfo(psCPD, psSymbolTable, *puSymbolID);
#endif	
	
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddBoolConstant(GLSLCompilerPrivateData *psCPD,
									  SymTable				*psSymbolTable,
									 IMG_BOOL				 bData,
									 GLSLPrecisionQualifier ePrecisionQualifier,
									 IMG_BOOL				 bAllowDuplicates,
									 IMG_UINT32				*puSymbolTableID)
{
	IMG_CHAR acName[10];

	GLSLIdentifierData sIdentifierData;

	/* Construct the name */
	sprintf(acName, "%s", bData ? "true" : "false");

#ifdef DEBUG
	if (ePrecisionQualifier != GLSLPRECQ_UNKNOWN)
	{
		LOG_INTERNAL_ERROR(("AddBoolConstant: Precision for bools is invalid (%08X)\n", ePrecisionQualifier));
		ePrecisionQualifier = GLSLPRECQ_UNKNOWN;
	}
#endif


	sIdentifierData.eSymbolTableDataType                          = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&(sIdentifierData.sFullySpecifiedType));
	sIdentifierData.sFullySpecifiedType.eTypeQualifier            = GLSLTQ_CONST;
	sIdentifierData.sFullySpecifiedType.eTypeSpecifier            = GLSLTS_BOOL;
	sIdentifierData.sFullySpecifiedType.ePrecisionQualifier       = ePrecisionQualifier;

	sIdentifierData.iActiveArraySize                              = -1;
	sIdentifierData.eArrayStatus                                  = GLSLAS_NOT_ARRAY;
	sIdentifierData.eLValueStatus                                 = GLSLLV_NOT_L_VALUE;
	sIdentifierData.eBuiltInVariableID                            = GLSLBV_NOT_BTIN;	
	sIdentifierData.eIdentifierUsage                              = (GLSLIdentifierUsage)0;
	sIdentifierData.uConstantDataSize                             = sizeof(IMG_BOOL);
	sIdentifierData.pvConstantData                                = (IMG_VOID *)&bData;
	sIdentifierData.uConstantAssociationSymbolID                  = 0;


	/* Add data to the symbol table */
	return AddIdentifierData(psCPD, psSymbolTable,
							 acName,
							 &sIdentifierData,
							 bAllowDuplicates,
							 puSymbolTableID);
}

/******************************************************************************
 * Function Name: AddBoolConstant
 *
 * Inputs       : bData
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add bool constant bData to the symbole table 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddIntConstant(GLSLCompilerPrivateData *psCPD, 
									 SymTable				*psSymbolTable, 
									IMG_INT32				iData, 
									GLSLPrecisionQualifier	ePrecisionQualifier,
									IMG_BOOL				bAllowDuplicates, /* For GL3, ADD_INT_UNSIGNED bit indicates it is an unsigned constant. */
									IMG_UINT32				*puSymbolTableID)
{
	IMG_CHAR acName[30];

	GLSLIdentifierData sIdentifierData;

	sprintf(acName, "%d", iData);

	switch (ePrecisionQualifier)
	{
		case GLSLPRECQ_LOW:
			/* Construct the name */
			strcat(acName, "_low");
			break;
		case GLSLPRECQ_MEDIUM:
			/* Construct the name */
			strcat(acName, "_med");
			break;
		case GLSLPRECQ_HIGH:
			/* Construct the name */
			strcat(acName, "_high");
			break;
		case GLSLPRECQ_UNKNOWN:
			break;
		default:
			LOG_INTERNAL_ERROR(("AddIntConstant: Unrecognised precision (%08X)\n", ePrecisionQualifier));
			break;
	}

	sIdentifierData.eSymbolTableDataType                          = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&(sIdentifierData.sFullySpecifiedType));
	sIdentifierData.sFullySpecifiedType.eTypeQualifier            = GLSLTQ_CONST;
	sIdentifierData.sFullySpecifiedType.ePrecisionQualifier       = ePrecisionQualifier;
	sIdentifierData.sFullySpecifiedType.eTypeSpecifier            = 
																	GLSLTS_INT;

	sIdentifierData.iActiveArraySize                              = -1;
	sIdentifierData.eArrayStatus                                  = GLSLAS_NOT_ARRAY;
	sIdentifierData.eLValueStatus                                 = GLSLLV_NOT_L_VALUE;
	sIdentifierData.eBuiltInVariableID                            = GLSLBV_NOT_BTIN;	
	sIdentifierData.eIdentifierUsage                              = (GLSLIdentifierUsage)0;
	sIdentifierData.uConstantDataSize                             = sizeof(IMG_INT32);
	sIdentifierData.pvConstantData                                = (IMG_VOID *)&iData;
	sIdentifierData.uConstantAssociationSymbolID                  = 0;


	/* Add data to the symbol table */
	return AddIdentifierData(psCPD, psSymbolTable,
							 acName,
							 &sIdentifierData,
							 bAllowDuplicates,
							 puSymbolTableID);
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddFloatConstant(GLSLCompilerPrivateData *psCPD,
									   SymTable					*psSymbolTable,
										IMG_FLOAT					fData,
										GLSLPrecisionQualifier	ePrecisionQualifier,
										IMG_BOOL					bAllowDuplicates,
										IMG_UINT32				*puSymbolTableID)
{
	IMG_CHAR acName[1024];

	GLSLIdentifierData sIdentifierData;

	switch (ePrecisionQualifier)
	{
		case GLSLPRECQ_LOW:
			/* Construct the name */
			sprintf(acName, "%f_low", fData);
			break;
		case GLSLPRECQ_MEDIUM:
			/* Construct the name */
			sprintf(acName, "%f_med", fData);
			break;
		case GLSLPRECQ_HIGH:
			/* Construct the name */
			sprintf(acName, "%f_high", fData);
			break;
		case GLSLPRECQ_UNKNOWN:
			/* Construct the name */
			sprintf(acName, "%f", fData);
			break;
		default:
			LOG_INTERNAL_ERROR(("AddFloatConstant: Unrecognised precision (%08X)\n", ePrecisionQualifier));
			break;
	}

	sIdentifierData.eSymbolTableDataType                          = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&(sIdentifierData.sFullySpecifiedType));
	sIdentifierData.sFullySpecifiedType.eTypeQualifier            = GLSLTQ_CONST;
	sIdentifierData.sFullySpecifiedType.eTypeSpecifier            = GLSLTS_FLOAT;
	sIdentifierData.sFullySpecifiedType.ePrecisionQualifier       = ePrecisionQualifier;

	sIdentifierData.iActiveArraySize                              = -1;
	sIdentifierData.eArrayStatus                                  = GLSLAS_NOT_ARRAY;
	sIdentifierData.eLValueStatus                                 = GLSLLV_NOT_L_VALUE;
	sIdentifierData.eBuiltInVariableID                            = GLSLBV_NOT_BTIN;
	sIdentifierData.eIdentifierUsage                              = (GLSLIdentifierUsage)0;
	sIdentifierData.uConstantDataSize                             = sizeof(IMG_FLOAT);
	sIdentifierData.pvConstantData                                = (IMG_VOID *)&fData;
	sIdentifierData.uConstantAssociationSymbolID                  = 0;


	/* Add data to the symbol table */
	return AddIdentifierData(psCPD, psSymbolTable, 
							 acName, 
							 &sIdentifierData,
							 bAllowDuplicates,
							 puSymbolTableID);

}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddFloatVecConstant(GLSLCompilerPrivateData *psCPD,
										  SymTable				*psSymbolTable,
											IMG_CHAR				*pszName,
											IMG_FLOAT				*pfData,
											IMG_UINT32				uNumComponents,
											GLSLPrecisionQualifier ePrecisionQualifier,
											IMG_BOOL				bAllowDuplicates, 
											IMG_UINT32				*puSymbolTableID)
{
	GLSLIdentifierData sIdentifierData;
	
	sIdentifierData.eSymbolTableDataType                          = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&(sIdentifierData.sFullySpecifiedType));
	sIdentifierData.sFullySpecifiedType.eTypeQualifier            = GLSLTQ_CONST;
	sIdentifierData.sFullySpecifiedType.eTypeSpecifier            = (GLSLTypeSpecifier)(GLSLTS_FLOAT + (uNumComponents - 1));
	sIdentifierData.sFullySpecifiedType.ePrecisionQualifier		  = ePrecisionQualifier;

	sIdentifierData.iActiveArraySize                              = -1;
	sIdentifierData.eArrayStatus                                  = GLSLAS_NOT_ARRAY;
	sIdentifierData.eLValueStatus                                 = GLSLLV_NOT_L_VALUE;
	sIdentifierData.eBuiltInVariableID                            = GLSLBV_NOT_BTIN;
	sIdentifierData.eIdentifierUsage                              = (GLSLIdentifierUsage)0;
	sIdentifierData.uConstantDataSize                             = sizeof(IMG_FLOAT) * uNumComponents;
	sIdentifierData.pvConstantData                                = (IMG_VOID *)pfData;
	sIdentifierData.uConstantAssociationSymbolID                  = 0;


	/* Add data to the symbol table */
	return AddIdentifierData(psCPD, psSymbolTable, 
							 pszName, 
							 &sIdentifierData,
							 bAllowDuplicates,
							 puSymbolTableID);
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddSwizzleData(GLSLCompilerPrivateData *psCPD,
									 SymTable        *psSymbolTable,
									IMG_CHAR        *pszName,
									GLSLSwizzleData *psSwizzleData,
									IMG_BOOL         bAllowDuplicates,
									IMG_UINT32      *puSymbolID)
{
	/* Add the data to the symbol table */
	return AddGenericData(psCPD, psSymbolTable,
						  pszName, 
						  (IMG_VOID *)psSwizzleData,
						  sizeof(GLSLSwizzleData),
						  GLSLSTDT_SWIZZLE,
						  bAllowDuplicates, 
						  puSymbolID);
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddMemberSelectionData(GLSLCompilerPrivateData *psCPD,
											 SymTable                *psSymbolTable,
											IMG_CHAR                *pszName,
											GLSLMemberSelectionData *psMemberSelectionData,
											IMG_BOOL                 bAllowDuplicates, 
											IMG_UINT32              *puSymbolID)
{
	/* Add the data to the symbol table */
	return AddGenericData(psCPD, psSymbolTable,
						  pszName, 
						  (IMG_VOID *)psMemberSelectionData,
						  sizeof(GLSLMemberSelectionData),
						  GLSLSTDT_MEMBER_SELECTION,
						  bAllowDuplicates, 
						  puSymbolID);

}

#ifdef DEBUG
static IMG_VOID CheckIdentifierData(GLSLCompilerPrivateData *psCPD,
								IMG_CHAR           *pszName,
								GLSLIdentifierData *psIdentifierData,
								IMG_CHAR           *pszFileName,
								IMG_UINT32          uLineNumber)
{
	if (psIdentifierData->sFullySpecifiedType.eParameterQualifier > GLSLPQ_INOUT)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid eParameterQualifier (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eParameterQualifier));
	}
	DebugAssert((IMG_UINT32)psIdentifierData->sFullySpecifiedType.eTypeSpecifier == 
		(IMG_UINT32)GLSLTypeSpecifierEnumTable(psIdentifierData->sFullySpecifiedType.eTypeSpecifier));
	if (psIdentifierData->sFullySpecifiedType.eTypeSpecifier >= GLSLTS_NUM_TYPES)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid eTypeSpecifier (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eTypeSpecifier));
	}
	if (psIdentifierData->sFullySpecifiedType.eTypeQualifier >= GLSLTQ_NUM)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid eTypeQualifier (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eTypeQualifier));
	}
	if (psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags > GLSLVMOD_ALL)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid eVaryingModifierFlags (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags));
	}
	if ((psIdentifierData->sFullySpecifiedType.eTypeQualifier != GLSLTQ_VERTEX_OUT) &&
		(psIdentifierData->sFullySpecifiedType.eTypeQualifier != GLSLTQ_FRAGMENT_IN) &&
		psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' non varyings cannot have varying modifier flags(%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eVaryingModifierFlags));
	}															
	if (psIdentifierData->sFullySpecifiedType.ePrecisionQualifier > GLSLPRECQ_HIGH)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid ePrecisionQualifier (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.ePrecisionQualifier));
	}

	if (psIdentifierData->eLValueStatus > GLSLLV_L_VALUE)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid l-value status (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->eLValueStatus));
	}

	if (psIdentifierData->eIdentifierUsage & (GLSLIU_READ | GLSLIU_CONDITIONAL | GLSLIU_PARAM))
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' eIdentifierUsage not initialised  (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->eIdentifierUsage));
	}

#if 0
	if (psIdentifierData->iActiveArraySize != -1)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' not a valid active array size (%d)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->iActiveArraySize));
	}
#endif

	if (psIdentifierData->uConstantAssociationSymbolID)
	{
		LOG_INTERNAL_ERROR(("CheckIdentifierData: %s,%d '%s' should not have an initial constant association symbol ID (%d)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->uConstantAssociationSymbolID));
	}

	if (psIdentifierData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST)
	{
		if (psIdentifierData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INVALID)
		{
#if 0	

			if (!(psIdentifierData->eIdentifierUsage & GLSLIU_ERROR_INITIALISING))
			{
				if (psIdentifierData->pvConstantData == IMG_NULL)
				{
					LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d Constant data should not be NULL for CONST (%08X)\n", 
									 pszFileName,
									 uLineNumber,
									 psIdentifierData->pvConstantData));
				}

				if (psIdentifierData->uConstantDataSize == 0)
				{
					LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d Constant data size should not be 0 for CONST (%08X)\n", 
									 pszFileName,
									 uLineNumber,
									 psIdentifierData->uConstantDataSize));
				}
			}
#endif

		}
		else
		{
			if (psIdentifierData->pvConstantData != IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d Constant data should be NULL for CONST params (%p)\n", 
								 pszFileName,
								 uLineNumber,
								 psIdentifierData->pvConstantData));
			}
			
			if (psIdentifierData->uConstantDataSize != 0)
			{
				LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d Constant data size should be 0 for CONST params(%08X)\n", 
								 pszFileName,
								 uLineNumber,
								 psIdentifierData->uConstantDataSize));
			}
		}
	}
}
#else
#define CheckIdentifierData(a, b, c, d, e)
#endif

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddIdentifierDatafn(GLSLCompilerPrivateData *psCPD,
										SymTable          *psSymbolTable,
										IMG_CHAR           *pszName,
										GLSLIdentifierData *psIdentifierData,
										IMG_BOOL            bAllowDuplicates,
										IMG_UINT32         *puSymbolID,
										IMG_CHAR           *pszFileName,
										IMG_UINT32          uLineNumber)
{
#ifdef DEBUG

	CheckIdentifierData(psCPD, pszName, psIdentifierData, pszFileName, uLineNumber);
	
	if (psIdentifierData->sFullySpecifiedType.eParameterQualifier != GLSLPQ_INVALID)
	{
		LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d Param qualifier for %s was not set to GLSLPQ_INVALID %08X\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psIdentifierData->sFullySpecifiedType.eParameterQualifier));
	}

	switch(psIdentifierData->sFullySpecifiedType.eTypeQualifier)
	{
		/* Calculate l-value status */
		case GLSLTQ_CONST:
		case GLSLTQ_VERTEX_IN:
		case GLSLTQ_UNIFORM:
		{
			if (psIdentifierData->eLValueStatus == GLSLLV_L_VALUE)
			{
				LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d L Value status should be GLSLLV_NOT_L_VALUE (is %08X) for this type qualifier  %08X\n", 
								 pszFileName,
								 uLineNumber,
								 psIdentifierData->eLValueStatus,
								 psIdentifierData->sFullySpecifiedType.eTypeQualifier));
			}
			break;
		}
		/* Can't know anything unless we have program type info */
		case GLSLTQ_VERTEX_OUT:
		case GLSLTQ_FRAGMENT_IN:
			break;
		default:
		{
			if (psIdentifierData->eLValueStatus != GLSLLV_L_VALUE)
			{
				LOG_INTERNAL_ERROR(("AddIdentifierData: %s,%d L Value status should be GLSLLV_L_VALUE (is %08X) for this type qualifier %08X\n", 
								 pszFileName,
								 uLineNumber,
								 psIdentifierData->eLValueStatus,
								 psIdentifierData->sFullySpecifiedType.eTypeQualifier));
			}
			break;
		}
	}
	
#else
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);
#endif

	/* Add the data to the symbol table */
	return AddIdentifierDataToTable(psCPD, psSymbolTable,
									pszName,
									(IMG_VOID *)psIdentifierData,
									bAllowDuplicates, 
									puSymbolID);

}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddParameterDatafn(GLSLCompilerPrivateData *psCPD,
										SymTable           *psSymbolTable,
										IMG_CHAR           *pszName,
										GLSLIdentifierData *psParameterData,
										IMG_BOOL            bAllowDuplicates,
										IMG_UINT32         *puSymbolID,
										IMG_CHAR           *pszFileName,
										IMG_UINT32          uLineNumber)
{
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	CheckIdentifierData(psCPD, pszName, psParameterData, pszFileName, uLineNumber);

	if ((psParameterData->sFullySpecifiedType.eParameterQualifier != GLSLPQ_IN) &&
		(psParameterData->sFullySpecifiedType.eParameterQualifier != GLSLPQ_OUT) &&
		(psParameterData->sFullySpecifiedType.eParameterQualifier != GLSLPQ_INOUT))
	{
		LOG_INTERNAL_ERROR(("AddParameterData: %s,%d Param qualifier was not set to in, out or inout %08X\n", 
						 pszFileName,
						 uLineNumber,
						 psParameterData->sFullySpecifiedType.eParameterQualifier));
	}

	if ((psParameterData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_VERTEX_IN) ||
		(psParameterData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_UNIFORM) ||
		(psParameterData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_VERTEX_OUT) ||
		(psParameterData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_FRAGMENT_IN))
	{

			LOG_INTERNAL_ERROR(("AddParameterData: %s,%d L This type qualifier %08X not valid for parameters \n", 
							 pszFileName,
							 uLineNumber,
							 psParameterData->sFullySpecifiedType.eTypeQualifier));
	}

	if (psParameterData->sFullySpecifiedType.eVaryingModifierFlags)
	{
			LOG_INTERNAL_ERROR(("AddParameterData: %s,%d L This varying modifier %08X not valid for parameters \n", 
							 pszFileName,
							 uLineNumber,
							 psParameterData->sFullySpecifiedType.eVaryingModifierFlags));
	}

	if (psParameterData->sFullySpecifiedType.eTypeQualifier != GLSLTQ_CONST)
	{
		if (psParameterData->eLValueStatus != GLSLLV_L_VALUE)
		{
			LOG_INTERNAL_ERROR(("AddParameterData: %s,%d L Value status should be GLSLLV_L_VALUE if param is not const %08X\n", 
							 pszFileName,
							 uLineNumber,
							 psParameterData->eLValueStatus));
		}
	}
	else
	{
		if (psParameterData->eLValueStatus == GLSLLV_L_VALUE)
		{
			LOG_INTERNAL_ERROR(("AddParameterData: %s,%d L Value status should be GLSLLV_NOT_L_VALUE if param is const %08X\n", 
							 pszFileName,
							 uLineNumber,
							 psParameterData->eLValueStatus));
		}	
	}

	if (psParameterData->eBuiltInVariableID != GLSLBV_NOT_BTIN)
	{
		LOG_INTERNAL_ERROR(("AddParameterDatafn: %s,%d BuilitInVariableID for '%s' is incorrect (%08X)\n",
						 pszFileName,
						 uLineNumber,
						 pszName,
						 psParameterData->eBuiltInVariableID));
	}

	/* Add the data to the symbol table */
	return AddIdentifierDataToTable(psCPD,
									psSymbolTable,
									pszName, 
									(IMG_VOID *)psParameterData,
									bAllowDuplicates, 
									puSymbolID);
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddResultDatafn(GLSLCompilerPrivateData *psCPD, 
									  SymTable           *psSymbolTable,
									IMG_CHAR           *pszName, 
									GLSLIdentifierData *psResultData,
									IMG_BOOL            bAllowDuplicates,
									IMG_UINT32         *puSymbolID,
									IMG_CHAR           *pszFileName,
									IMG_UINT32          uLineNumber)
{
	/* Get rid of warning error */
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	CheckIdentifierData(psCPD, pszName, psResultData, pszFileName, uLineNumber);

#if 0
	/* Actually can be allowed if selecting a member of a structure that was passed in as a parameter */
	if (psResultData->sFullySpecifiedType.eParameterQualifier != GLSLPQ_INVALID)
	{
		LOG_INTERNAL_ERROR(("AddResultData: %s,%d Param qualifier was not set to GLSLPQ_INVALID %08X\n",
						 pszFileName,
						 uLineNumber,
						 psResultData->sFullySpecifiedType.eParameterQualifier));
	}

	if ((psResultData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_VERTEX_IN) ||
		(psResultData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_UNIFORM) ||
		(psResultData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_VARYING))
	{

			LOG_INTERNAL_ERROR(("AddResultData: %s,%d L This type qualifier %08X not valid for results \n", 
							 pszFileName,
							 uLineNumber,
							 psResultData->sFullySpecifiedType.eTypeQualifier));
	}
#endif


	/* Add the data to the symbol table */
	return AddIdentifierDataToTable(psCPD,
									psSymbolTable,
									pszName, 
									(IMG_VOID *)psResultData,
									bAllowDuplicates, 
									puSymbolID);
}


/******************************************************************************
 * Function Name: AddFunctionCallData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddFunctionCallData(GLSLCompilerPrivateData *psCPD, 
										  SymTable             *psSymbolTable,
											IMG_CHAR             *pszName, 
											GLSLFunctionCallData *psFunctionCallData,
											IMG_BOOL              bAllowDuplicates, 
											IMG_UINT32           *puSymbolID)
{
	/* Add the data to the symbol table */
	if (!AddGenericData(psCPD,
						psSymbolTable,
						pszName, 
						(IMG_VOID *)psFunctionCallData,
						sizeof(GLSLFunctionCallData),
						GLSLSTDT_FUNCTION_CALL,
						bAllowDuplicates, 
						puSymbolID))
	{
		return IMG_FALSE;
	}

#ifdef DUMP_LOGFILES
	DumpFunctionCallInfo(psCPD, psSymbolTable, *puSymbolID);
#endif

	return IMG_TRUE;

}

/******************************************************************************
 * Function Name: FreeFunctionDefinitionData
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreeFunctionDefinitionData(IMG_VOID *pvData)
{
	if (pvData)
	{
		GLSLFunctionDefinitionData *psData = (GLSLFunctionDefinitionData *)pvData;

		if (psData->puParameterSymbolTableIDs)
		{
			DebugMemFree(psData->puParameterSymbolTableIDs);
		}

		if (psData->psFullySpecifiedTypes)
		{
			DebugMemFree(psData->psFullySpecifiedTypes);
		}

		if (psData->puCalledFunctionIDs)
		{
			DebugMemFree(psData->puCalledFunctionIDs);
		}
		
		DebugMemFree(psData->pszOriginalFunctionName);

		DebugMemFree(psData);
	}
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddFunctionDefinitionData(GLSLCompilerPrivateData *psCPD,
												SymTable                   *psSymbolTable,
												IMG_CHAR                   *pszName,
												GLSLFunctionDefinitionData *psFunctionDefinitionData,
												IMG_BOOL                    bAllowDuplicates,
												IMG_UINT32                 *puSymbolID)
{
	GLSLFunctionDefinitionData *psData;

	psData = DebugMemAlloc(sizeof(GLSLFunctionDefinitionData));

	if (!psData)
	{
		LOG_INTERNAL_ERROR((
			"AddFunctionDefinitionData: Failed to allocate memory for function definition data\n"));
		return IMG_FALSE;
	}

	/* Check for parameters whoch have not been initialised to zero */
	if (psFunctionDefinitionData->uFunctionCalledCount  ||
		psFunctionDefinitionData->uMaxFunctionCallDepth ||
		psFunctionDefinitionData->puCalledFunctionIDs   ||
		psFunctionDefinitionData->uNumCalledFunctions)
	{
		LOG_INTERNAL_ERROR(("AddFunctionDefinitionData: Function '%s' Non NULL parameter detected\n", 
						   psFunctionDefinitionData->pszOriginalFunctionName));
	}

	/* Copy the data */
	*psData = *psFunctionDefinitionData;

	/* copy the function name */
	psData->pszOriginalFunctionName = DebugMemAlloc(strlen(psFunctionDefinitionData->pszOriginalFunctionName) + 1);

	if(!psData->pszOriginalFunctionName)
	{
		return IMG_FALSE;
	}

	strcpy(psData->pszOriginalFunctionName, psFunctionDefinitionData->pszOriginalFunctionName);

	/* Copy the parameter information */
	if (psFunctionDefinitionData->uNumParameters)
	{
		/* Allocate memory for the paremeters */
		psData->puParameterSymbolTableIDs  = DebugMemAlloc(sizeof(IMG_UINT32)               * psFunctionDefinitionData->uNumParameters);

		if(!psData->puParameterSymbolTableIDs)
		{
			return IMG_FALSE;
		}

		psData->psFullySpecifiedTypes      = DebugMemAlloc(sizeof(GLSLFullySpecifiedType) * psFunctionDefinitionData->uNumParameters);

		if(!psData->psFullySpecifiedTypes)
		{
			return IMG_FALSE;
		}
		
		/* Copy the data */
		memcpy(psData->puParameterSymbolTableIDs,
			   psFunctionDefinitionData->puParameterSymbolTableIDs,
			   sizeof(IMG_UINT32) * psFunctionDefinitionData->uNumParameters);

		memcpy(psData->psFullySpecifiedTypes, 
			   psFunctionDefinitionData->psFullySpecifiedTypes, 
			   sizeof(GLSLFullySpecifiedType) * psFunctionDefinitionData->uNumParameters);
	}
	else
	{
		psData->puParameterSymbolTableIDs = IMG_NULL;
		psData->psFullySpecifiedTypes     = IMG_NULL;
	}
	
	if (psData->eSymbolTableDataType  != GLSLSTDT_FUNCTION_DEFINITION)
	{
		LOG_INTERNAL_ERROR(("AddFunctionDefinitionData: Symbol data type incorrect\n"));
	
		psData->eSymbolTableDataType = GLSLSTDT_FUNCTION_DEFINITION;
	}

	/* Add the data to the symbol table */
	if(!AddSymbol(psSymbolTable,
				  pszName,
				  psData,
				  sizeof(GLSLFunctionDefinitionData),
				  bAllowDuplicates,
				  puSymbolID,
				  FreeFunctionDefinitionData))
	{
		LOG_INTERNAL_ERROR(("AddFunctionDefinitionData: Failed to add data to symbol table"));

		*puSymbolID = 0;
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreeStructureDefinitionData(IMG_VOID *pvData)
{
	if (pvData)
	{
		IMG_UINT32 i;

		GLSLStructureDefinitionData *psData = (GLSLStructureDefinitionData *)pvData;
	
		/* Destroy the structure member names */
		for (i = 0; i < psData->uNumMembers; i++)
		{
			DebugMemFree(psData->psMembers[i].pszMemberName);
		}

		/* Free the members */
		DebugMemFree(psData->psMembers);

		/* Free the structure itself */
		DebugMemFree(psData);
	}
}

/******************************************************************************
 * Structure Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL AddStructureDefinitionData(GLSLCompilerPrivateData *psCPD,
										   SymTable                    *psSymbolTable,
											IMG_CHAR                    *pszName,
											GLSLStructureDefinitionData *psStructureDefinitionData,
											IMG_BOOL                     bAllowDuplicates, 
											IMG_UINT32                  *puSymbolID)
{
	GLSLStructureDefinitionData *psData;


	psData = DebugMemAlloc(sizeof(GLSLStructureDefinitionData));

	if (!psData)
	{
		LOG_INTERNAL_ERROR((
			"AddStructureDefinitionData: Failed to allocate memory for structure definition data\n"));
		return IMG_FALSE;
	}


	/* Copy the data */
	*psData = *psStructureDefinitionData;

	if (psStructureDefinitionData->uNumMembers)
	{
		IMG_UINT32 i;
		
		/* Allocate memory for the members */
		psData->psMembers = DebugMemAlloc(sizeof(GLSLStructureMember) * psData->uNumMembers);

		if(!psData->psMembers)
		{
			DebugMemFree(psData);
			return IMG_FALSE;
		}

		/* Copy across the members info */
		memcpy(psData->psMembers, 
			   psStructureDefinitionData->psMembers, 
			   sizeof(GLSLStructureMember) * psData->uNumMembers);

		/* Copy across the member names */
		for (i = 0; i < psData->uNumMembers; i++)
		{
			/* FIXME: test for null */
			psData->psMembers[i].pszMemberName = DebugMemAlloc(strlen(psStructureDefinitionData->psMembers[i].pszMemberName) + 1);
			
			strcpy(psData->psMembers[i].pszMemberName, psStructureDefinitionData->psMembers[i].pszMemberName);
		}
	}
	else
	{
		LOG_INTERNAL_ERROR(("AddStructureDefinitionData: Structure had no members!\n"));
		return IMG_FALSE;
	}
	
	if (psData->eSymbolTableDataType  != GLSLSTDT_STRUCTURE_DEFINITION)
	{
		LOG_INTERNAL_ERROR(("AddStructureDefinitionData: Symbol data type incorrect\n"));
		return IMG_FALSE;
	
		/* Do not hide the error -- psData->eSymbolTableDataType = GLSLSTDT_STRUCTURE_DEFINITION; */
	}

	/* Add the data to the symbol table */
	if(!AddSymbol(psSymbolTable,
				  pszName,
				  psData, 
				  sizeof(GLSLStructureDefinitionData),
				  bAllowDuplicates,
				  puSymbolID,
				  FreeStructureDefinitionData))
	{
		LOG_INTERNAL_ERROR(("AddStructureDefinitionData: Failed to add data to symbol table"));

		*puSymbolID = 0;
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddStructureDefinition(GLSLCompilerPrivateData *psCPD,
											 SymTable                    *psSymbolTable,
											ParseTreeEntry              *psParseTreeEntry,
											IMG_CHAR                    *pszStructureName,
											GLSLStructureDefinitionData *psStructureDefinitionData,
											IMG_UINT32                  *puSymbolTableID)
{
	IMG_UINT32 i, j;

	IMG_CHAR acName[300];

	GLSLFunctionDefinitionData sFunctionDefinitionData;

	IMG_UINT32 uReturnDataSymbolID, uConstructorSymbolID;

	GLSLIdentifierData sReturnData;

	/* Check for duplicate member names */
	for (i = 0; i < psStructureDefinitionData->uNumMembers; i++)
	{
		for (j = i+1; j < psStructureDefinitionData->uNumMembers; j++)
		{
			
			if (strcmp(psStructureDefinitionData->psMembers[i].pszMemberName, psStructureDefinitionData->psMembers[j].pszMemberName) == 0)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psParseTreeEntry, 
										 "'struct' : duplicate field name in structure: %s\n",
										 psStructureDefinitionData->psMembers[i].pszMemberName);
			}	
		}	
	}

	/* Hash the name so we get a unique name */
	sprintf(acName, "%s@struct_def",  pszStructureName);

	if (FindSymbol(psSymbolTable, acName, puSymbolTableID, IMG_FALSE))
	{
		LogProgramParseTreeError(psCPD->psErrorLog, psParseTreeEntry, "'%s' : 'struct' type redefinition\n", pszStructureName);
		return IMG_FALSE;
	}

	/* Add the structure definition to the symbol table */
	if (!AddStructureDefinitionData(psCPD, psSymbolTable,
									acName,
									psStructureDefinitionData,
									IMG_FALSE,
									puSymbolTableID))
	{
		LOG_INTERNAL_ERROR((
			"ASTCreateStructureNode: Failed to add structure %s to symbol table", pszStructureName));
		
		return IMG_FALSE;
	}

		/* Need to add a constructor function based on this structure */

	/* Create a constructor name */
	sprintf(acName, CONSTRUCTOR_RETURN_VAL_STRING, pszStructureName);


	sReturnData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&sReturnData.sFullySpecifiedType);
	sReturnData.sFullySpecifiedType.eTypeSpecifier           = GLSLTS_STRUCT;
	sReturnData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
	sReturnData.sFullySpecifiedType.uStructDescSymbolTableID = *puSymbolTableID;

	sReturnData.eLValueStatus                                = GLSLLV_L_VALUE;
	sReturnData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
	sReturnData.iActiveArraySize                             = -1;
	sReturnData.uConstantDataSize                            = 0;
	sReturnData.eIdentifierUsage                             = (GLSLIdentifierUsage)(GLSLIU_INTERNALRESULT | GLSLIU_WRITTEN);
	sReturnData.pvConstantData                               = IMG_NULL;
	sReturnData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
	sReturnData.uConstantAssociationSymbolID                 = 0;


	/* Add the data for the return value to the symbol table */
	if (!AddIdentifierData(psCPD, psSymbolTable, 
						   acName, 
						   &sReturnData,
						   IMG_FALSE,
						   &uReturnDataSymbolID))
	{
		LOG_INTERNAL_ERROR((
			"ASTProcessStructSpecifier: Failed to add return value %s to symbol table", acName));
		
		return IMG_FALSE;
	}
	
	sFunctionDefinitionData.eSymbolTableDataType       = GLSLSTDT_FUNCTION_DEFINITION;
	sFunctionDefinitionData.eFunctionType              = GLSLFT_USERDEFINED_CONSTRUCTOR;
	sFunctionDefinitionData.eFunctionFlags             = GLSLFF_VALID_IN_ALL_CASES;
	sFunctionDefinitionData.uReturnDataSymbolID        = uReturnDataSymbolID;
	sFunctionDefinitionData.sReturnFullySpecifiedType  = sReturnData.sFullySpecifiedType;
	sFunctionDefinitionData.bPrototype                 = IMG_FALSE;
	sFunctionDefinitionData.eBuiltInFunctionID         = GLSLBFID_NOT_BUILT_IN;
	sFunctionDefinitionData.uNumParameters             = psStructureDefinitionData->uNumMembers;

	sFunctionDefinitionData.puParameterSymbolTableIDs  = DebugMemAlloc(psStructureDefinitionData->uNumMembers * sizeof(IMG_UINT32));

	if(!sFunctionDefinitionData.puParameterSymbolTableIDs)
	{
		return IMG_FALSE;
	}

	sFunctionDefinitionData.psFullySpecifiedTypes      = DebugMemAlloc(psStructureDefinitionData->uNumMembers * sizeof(GLSLFullySpecifiedType));

	if(!sFunctionDefinitionData.psFullySpecifiedTypes)
	{
		DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
		return IMG_FALSE;
	}

	sFunctionDefinitionData.uFunctionCalledCount       = 0;
	sFunctionDefinitionData.uMaxFunctionCallDepth      = 0;
	sFunctionDefinitionData.puCalledFunctionIDs        = IMG_NULL;
	sFunctionDefinitionData.uNumCalledFunctions        = 0;
	
	
	/* Add the parameters */
	for (i = 0; i < psStructureDefinitionData->uNumMembers; i++)
	{
		GLSLIdentifierData sParameterData = psStructureDefinitionData->psMembers[i].sIdentifierData;

		/* Create a parameter */
		sParameterData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
		sParameterData.sFullySpecifiedType.eParameterQualifier      = GLSLPQ_IN;
		sParameterData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
		sParameterData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
		sParameterData.eLValueStatus                                = GLSLLV_L_VALUE;
		sParameterData.uConstantDataSize                            = 0;
		sParameterData.pvConstantData                               = IMG_NULL;
		sParameterData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
		sParameterData.eIdentifierUsage                             = (GLSLIdentifierUsage)0;
		sParameterData.uConstantAssociationSymbolID                 = 0;

		/* Set array size */
		if 	(sParameterData.sFullySpecifiedType.iArraySize)
		{
			sParameterData.iActiveArraySize = sParameterData.sFullySpecifiedType.iArraySize;
		}
		else
		{
			sParameterData.iActiveArraySize = -1;
		}


		/* Create a parameter name */
		sprintf(acName, CONSTRUCTOR_PARAM_STRING, i, pszStructureName);

		sFunctionDefinitionData.psFullySpecifiedTypes[i] = sParameterData.sFullySpecifiedType; 

		/* Add the data to the symbol table */
		if (!AddParameterData(psCPD, psSymbolTable, 
							  acName, 
							  &sParameterData, 
							  IMG_FALSE,
							  &sFunctionDefinitionData.puParameterSymbolTableIDs[i]))
		{
			LOG_INTERNAL_ERROR(("ASTProcessStructSpecifier: Failed to add parameter %s to symbol table", acName));
			DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
			DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);
			return IMG_FALSE;
		}
	}

	/* Create a constructor name */
	sprintf(acName, CONSTRUCTOR_STRING, pszStructureName);
	sFunctionDefinitionData.pszOriginalFunctionName    = acName;


	/* Add the constructor as a function to the symbol table */
	if (!AddFunctionDefinitionData(psCPD, psSymbolTable,
								   acName,
								   &sFunctionDefinitionData,
								   IMG_FALSE,
								   &uConstructorSymbolID))
	{
		LOG_INTERNAL_ERROR(("ASTProcessStructSpecifier: Failed to add function %s to symbol table", acName));
		DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
		DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);
		return IMG_FALSE;
	}

	DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
	DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GetSymbolTableDatafn
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID *GetSymbolTableDatafn(GLSLCompilerPrivateData *psCPD,
											SymTable                *psSymbolTable,
											IMG_UINT32               uSymbolTableID,
											IMG_BOOL                 bCheckSymbolTableDataType,
											GLSLSymbolTableDataType  eExpectedSymbolTableDataType,
											IMG_CHAR                *pszFileName,
											IMG_UINT32               uLineNumber)
{
	/* Get the data associated with this symbol ID */
	GLSLGenericData *psGenericData; 
	
	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	if (!uSymbolTableID)
	{
		LOG_INTERNAL_ERROR(("%s:%d - GetSymbolTableDatafn: Symbol ID was 0\n",
						 pszFileName,
						 uLineNumber));
		return IMG_NULL;
	}

	/* Get the data associated with this symbol ID */
	psGenericData = GetSymbolData(psSymbolTable, uSymbolTableID);

	if (!psGenericData)
	{
		LOG_INTERNAL_ERROR(("%s:%d - GetSymbolTableDatafn: Failed to retreive data for provided symbol ID %08X\n",
						 pszFileName,
						 uLineNumber,
						 uSymbolTableID));
		return IMG_NULL;
	}

	if (bCheckSymbolTableDataType)
	{
		if (psGenericData->eSymbolTableDataType != eExpectedSymbolTableDataType)
		{
			LOG_INTERNAL_ERROR(("%s:%d - GetSymbolTableDatafn: Expected data type (%08X) found - (%08X)\n",
							 pszFileName,
							 uLineNumber,
							 eExpectedSymbolTableDataType,
							 psGenericData->eSymbolTableDataType));
			return IMG_NULL;
		}
	}

	return  (IMG_VOID *)psGenericData;
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL GetSymbolInfofn(GLSLCompilerPrivateData *psCPD,
										SymTable                *psSymbolTable,
										IMG_UINT32               uSymbolTableID,
										GLSLProgramType          eProgramType,
										GLSLFullySpecifiedType  *psFullySpecifiedType,
										GLSLLValueStatus        *peLValueStatus,
										IMG_UINT32              *puDimension,
										GLSLArrayStatus         *peArrayStatus,
										IMG_VOID               **ppvConstantData,
										IMG_UINT32              *puConstantDataSize,
										IMG_CHAR                *pszDesc,
										IMG_CHAR                *pszFileName,
										IMG_UINT32               uLineNumber)
{
	GLSLLValueStatus	eLValueStatus        = GLSLLV_NOT_L_VALUE;
	GLSLArrayStatus		eArrayStatus         = GLSLAS_NOT_ARRAY;
	IMG_UINT32 uDimension                    = 0;
	IMG_VOID              *pvConstantData    = IMG_NULL;
	IMG_UINT32             uConstantDataSize = 0; 

	GLSLFullySpecifiedType  sFullySpecifiedType;

	/* Get the data associated with this symbol ID */
	GLSLGenericData *psGenericData = GetSymbolTableData(psSymbolTable, uSymbolTableID);

	PVR_UNREFERENCED_PARAMETER(pszFileName);
	PVR_UNREFERENCED_PARAMETER(uLineNumber);

	if (!psGenericData)
	{
		LOG_INTERNAL_ERROR(("%s:%d - GetSymbolInfofn: Failed to retrieve data\n",
						 pszFileName,
						 uLineNumber));

		return IMG_FALSE;
	}

	/* Set some default values for the fully specified type */
	INIT_FULLY_SPECIFIED_TYPE(&sFullySpecifiedType);

	switch (psGenericData->eSymbolTableDataType)
	{
		case GLSLSTDT_IDENTIFIER:
		{
			GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData *)psGenericData;

			sFullySpecifiedType =  psIdentifierData->sFullySpecifiedType;

			eLValueStatus     = psIdentifierData->eLValueStatus;
			eArrayStatus      = psIdentifierData->eArrayStatus;
			pvConstantData    = psIdentifierData->pvConstantData;
			uConstantDataSize = psIdentifierData->uConstantDataSize;

			break;
		}
		case GLSLSTDT_FUNCTION_CALL:
		{
			GLSLFunctionCallData *psFunctionCallData = (GLSLFunctionCallData *)psGenericData;

			sFullySpecifiedType = psFunctionCallData->sFullySpecifiedType;
			eArrayStatus        = psFunctionCallData->eArrayStatus;

			break;
		}
		case GLSLSTDT_SWIZZLE:
		{
			GLSLSwizzleData *psSwizzleData = (GLSLSwizzleData *)psGenericData;

			if (puDimension)
			{
				*puDimension = psSwizzleData->uNumComponents;
			}

			sFullySpecifiedType.eTypeQualifier = GLSLTQ_CONST;
		
			break;
		}
		case GLSLSTDT_MEMBER_SELECTION:
		{
			GLSLStructureDefinitionData *psStructureDefinitionData;

			GLSLIdentifierData *psMemberIdentifierData;
			
			GLSLMemberSelectionData *psMemberSelectionData = (GLSLMemberSelectionData *)psGenericData;

			/* Recursively call this function to get info about the type qualifier of this instance */
			GetSymbolInfo(psCPD,
						psSymbolTable,
						  psMemberSelectionData->uStructureInstanceSymbolTableID,
						  eProgramType,
						  &sFullySpecifiedType,
						  IMG_NULL,
						  IMG_NULL,
						  IMG_NULL, 
						  IMG_NULL, 
						  IMG_NULL,
						  IMG_NULL);

			/* get the structure definition data */
			psStructureDefinitionData = GetSymbolTableData(psSymbolTable, 
														   sFullySpecifiedType.uStructDescSymbolTableID);
			
			/* Get information about this member */
			psMemberIdentifierData = &(psStructureDefinitionData->psMembers[psMemberSelectionData->uMemberOffset].sIdentifierData);
		
			if (psMemberIdentifierData->sFullySpecifiedType.ePrecisionQualifier > GLSLPRECQ_HIGH)
			{
				LOG_INTERNAL_ERROR(("GetSymbolInfofn: Found dodgy precision for member selection\n"));
			}

			
			/* 
			   Type and parameter qualifiers used when structure was instanced overrides any qualifier used when the structure was defined 
			   so don't copy that.
			*/
			sFullySpecifiedType.eTypeSpecifier           = psMemberIdentifierData->sFullySpecifiedType.eTypeSpecifier;
			sFullySpecifiedType.ePrecisionQualifier      = psMemberIdentifierData->sFullySpecifiedType.ePrecisionQualifier;
			sFullySpecifiedType.uStructDescSymbolTableID = psMemberIdentifierData->sFullySpecifiedType.uStructDescSymbolTableID;
			sFullySpecifiedType.iArraySize               = psMemberIdentifierData->sFullySpecifiedType.iArraySize;

			eArrayStatus = psMemberIdentifierData->eArrayStatus;
		

			if (!(sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST    ||
				  sFullySpecifiedType.eTypeQualifier == GLSLTQ_VERTEX_IN ||
				  sFullySpecifiedType.eTypeQualifier == GLSLTQ_UNIFORM  ||
				  sFullySpecifiedType.eTypeQualifier == GLSLTQ_FRAGMENT_IN))
			{
				eLValueStatus = GLSLLV_L_VALUE;
			}

			break;
		}
		case GLSLSTDT_STRUCTURE_DEFINITION:
		{
			LOG_INTERNAL_ERROR(("GetSymbolInfo: Don't think it's valid to ever look up a structure definition\n"));
			sFullySpecifiedType.uStructDescSymbolTableID = uSymbolTableID;

			break;
		}
		default:
		{
			LOG_INTERNAL_ERROR(("GetSymbolInfo: (%s,%d)Unrecognised symbol data type (%08x)\n", 
							 pszFileName,
							 uLineNumber,
							 psGenericData->eSymbolTableDataType));
			break;
		}
	}

	if (psFullySpecifiedType)
	{
		/* Quick sanity check of any returned structures */
		if ((sFullySpecifiedType.eTypeSpecifier == GLSLTS_STRUCT) && 
			(sFullySpecifiedType.uStructDescSymbolTableID == 0))
		{
			LOG_INTERNAL_ERROR(("Symbol data type %d had bad struct info\n", psGenericData->eSymbolTableDataType));
		}

		*psFullySpecifiedType = sFullySpecifiedType;
	}

	if ((psGenericData->eSymbolTableDataType != GLSLSTDT_SWIZZLE) && 
		(psGenericData->eSymbolTableDataType != GLSLSTDT_STRUCTURE_DEFINITION))
	{
		uDimension = GLSLTypeSpecifierDimensionTable(sFullySpecifiedType.eTypeSpecifier);
	}

	if (puDimension)
	{
		*puDimension = uDimension;
	}

	if (ppvConstantData)
	{
		*ppvConstantData = pvConstantData;
	}

	if (puConstantDataSize)
	{
		*puConstantDataSize = uConstantDataSize;
	}

	if (peLValueStatus)
	{
		*peLValueStatus = eLValueStatus;
	}

	if (peArrayStatus)
	{
		*peArrayStatus = eArrayStatus;
	}

	if (pszDesc)
	{
		pszDesc[0] = '\0';

		if (sFullySpecifiedType.iArraySize)
		{
			sprintf(&(pszDesc[strlen(pszDesc)]),"array of ");
		}

		if (sFullySpecifiedType.eParameterQualifier != GLSLPQ_INVALID)
		{
			sprintf(&(pszDesc[strlen(pszDesc)]),"%s ", GLSLParameterQualifierFullDescTable[sFullySpecifiedType.eParameterQualifier]);
		}
	
		if (sFullySpecifiedType.eTypeQualifier != GLSLTQ_TEMP)
		{
			sprintf(&(pszDesc[strlen(pszDesc)]),
					"%s %s",
					GLSLTypeQualifierFullDescTable(sFullySpecifiedType.eTypeQualifier),
					GLSLTypeSpecifierFullDescTable(sFullySpecifiedType.eTypeSpecifier));
		}
		else
		{
				sprintf(&(pszDesc[strlen(pszDesc)]),
					"%s",
					GLSLTypeSpecifierFullDescTable(sFullySpecifiedType.eTypeSpecifier));
		}
	}


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL AddBuiltInIdentifier(GLSLCompilerPrivateData *psCPD,
										   SymTable              *psSymbolTable,
											IMG_CHAR				*pszName,
											IMG_INT32              iArraySize,
											GLSLBuiltInVariableID	eBuiltInVariableID,
											GLSLTypeSpecifier		eTypeSpecifier,
											GLSLTypeQualifier		eTypeQualifier,
											GLSLPrecisionQualifier ePrecisionQualifier,
											IMG_UINT32			*puSymbolTableID)
{
	GLSLIdentifierData sIdentifierData;

	sIdentifierData.eBuiltInVariableID = eBuiltInVariableID;
	sIdentifierData.iActiveArraySize = -1;

	if (iArraySize == 0)
	{
		sIdentifierData.eArrayStatus = GLSLAS_NOT_ARRAY;
	}
	else if (iArraySize == -1)
	{
		sIdentifierData.eArrayStatus = GLSLAS_ARRAY_SIZE_NOT_FIXED;
	}
	else if (iArraySize > 0)
	{
		sIdentifierData.eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;
		sIdentifierData.iActiveArraySize = iArraySize;
	}

	sIdentifierData.eLValueStatus        = GLSLLV_NOT_L_VALUE;
	sIdentifierData.eSymbolTableDataType = GLSLSTDT_IDENTIFIER;
	sIdentifierData.pvConstantData       = IMG_NULL;
	sIdentifierData.eIdentifierUsage     = (GLSLIdentifierUsage)0;

	INIT_FULLY_SPECIFIED_TYPE(&sIdentifierData.sFullySpecifiedType);
	sIdentifierData.sFullySpecifiedType.eTypeQualifier           = eTypeQualifier;
	sIdentifierData.sFullySpecifiedType.eTypeSpecifier           = eTypeSpecifier;
	sIdentifierData.sFullySpecifiedType.ePrecisionQualifier      = ePrecisionQualifier;
	sIdentifierData.sFullySpecifiedType.iArraySize               = iArraySize;
	sIdentifierData.sFullySpecifiedType.uStructDescSymbolTableID = 0;

	sIdentifierData.uConstantDataSize = 0;


	/* Add data to the symbol table */
	return AddIdentifierDataToTable(psCPD, psSymbolTable,
									pszName,
									&sIdentifierData,
									IMG_TRUE,
									puSymbolTableID);

}

/******************************************************************************
 End of file (common.c)
******************************************************************************/
