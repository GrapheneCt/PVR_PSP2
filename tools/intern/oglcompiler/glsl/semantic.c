/******************************************************************************
 * Name         : semantic.c
 * Author       : James McCarthy
 * Created      : 27/07/2004
 *
 * Copyright    : 2004-2009 by Imagination Technologies Limited.
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
 * $Log: semantic.c $
 *****************************************************************************/

#ifdef LINUX
#    include "../parser/debug.h"
#else
#    include "..\parser\debug.h"
#endif

#include "glsltree.h"
#include "error.h"
#include "semantic.h"
#include "common.h"
#include "glslfns.h"
#include "icgen.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>



#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static IMG_VOID ASTSemRemoveChildNode(GLSLTreeContext *psGLSLTreeContext,
							   GLSLNode        *psNode,
							   IMG_UINT32       uChildNum,
							   IMG_BOOL         bRemoveAllChildren);

static IMG_BOOL ASTUpdateParamIdentifierUsage(GLSLTreeContext       *psGLSLTreeContext,
									   GLSLNode              *psNode,
									   GLSLParameterQualifier  eParameterQualifier);


/******************************************************************************
 * Function Name: ASTSemRemoveNode
 * Inputs       : psGLSLTreeContext
                  psNodeToRemove
				    bRemoveAllChildren - Whether to force to delete the childs of the node.
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Removes psNodeToRemove from psGLSLTreeContext.
 *****************************************************************************/
static IMG_BOOL ASTSemRemoveNode(GLSLTreeContext *psGLSLTreeContext,
						  GLSLNode        *psNodeToRemove,
						  IMG_BOOL         bRemoveAllChildren)
{
	IMG_UINT32 i, uNumChildren = psNodeToRemove->uNumChildren;

	if (psNodeToRemove->uNumChildren)
	{
		if (psNodeToRemove->eNodeType == GLSLNT_EXPRESSION    ||
		    psNodeToRemove->eNodeType == GLSLNT_SUBEXPRESSION ||
		    bRemoveAllChildren)
		{
			/*
			   If we've reduced a sub expression / expression to a single costant value then we need to remove the sub expressions
			   child(ren) before we remove it otherwise it will fail. We can detect this as it's single child will have the same symbol
			   table ID.
			*/

			/*
			   Remove the node children, cannot use value of num children inside
			   node as it decremented by the function
			*/

			for (i = 0; i < uNumChildren; i++)
			{
				/* 0 is correct, should NOT be i */
				ASTSemRemoveChildNode(psGLSLTreeContext, psNodeToRemove, 0, bRemoveAllChildren);
			}
		}
		else
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			LOG_INTERNAL_NODE_ERROR((psNodeToRemove,"ASTSemRemoveNode: Cannot remove a child node that has %d children of its own (%08X)\n",
								 psNodeToRemove->uNumChildren,
								 psNodeToRemove->eNodeType));
			return IMG_FALSE;
		}
	}

	/* The node itself is freed when we free the link list. */
	/* DebugMemFree(psNodeToRemove); */

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemRemoveChildNode
 *
 * Inputs       : psGLSLTreeContext
                  psNode
                  uChildNum
                  bRemoveAllChildren - Whether to force to delete the grandchilds.
 * Outputs      : -
 * Returns      : -
 * Description  : Removes the uChildNum-th child from the psNode.
 *****************************************************************************/
static IMG_VOID ASTSemRemoveChildNode(GLSLTreeContext *psGLSLTreeContext,
							   GLSLNode        *psNode,
							   IMG_UINT32       uChildNum,
							   IMG_BOOL         bRemoveAllChildren)
{
	IMG_UINT32 i, j;
	GLSLNode **ppsNewNodes = IMG_NULL;
	GLSLNode *psNodeToRemove;

	if (uChildNum >= psNode->uNumChildren)
	{
		GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

		LOG_INTERNAL_NODE_ERROR((psNode,"ASTRemoveChildNode: Child specified out of range (%d) \n",uChildNum));
		return;
	}

	psNodeToRemove = psNode->ppsChildren[uChildNum];

	/* Remove the node */
	if (!ASTSemRemoveNode(psGLSLTreeContext, psNodeToRemove, bRemoveAllChildren))
	{
		return;
	}

	if (psNode->uNumChildren > 1)
	{
		/* Allocate memory for reduced node array */
		ppsNewNodes = DebugMemAlloc((psNode->uNumChildren - 1) * sizeof(GLSLNode *));

		if(!ppsNewNodes)
		{
			return;
		}

		/* Copy across all nodes except for this one */
		for (i = 0, j = 0; i < psNode->uNumChildren; i++)
		{
			if (i != uChildNum)
			{
				ppsNewNodes[j++] =  psNode->ppsChildren[i];
			}
		}
	}


	DebugMemFree(psNode->ppsChildren);

	psNode->ppsChildren = ppsNewNodes;

	psNode->uNumChildren--;
}

/******************************************************************************
 * Function Name: ASTSemGetResultName
 *
 * Inputs       : psGLSLTreeContext
                  psFullySpecifiedType
 * Outputs      : pszResultName
 * Returns      : -
 *
 * Description  : Generates a unique name into pszResultName. Used for expression reductions, etc.
 *****************************************************************************/
static IMG_VOID ASTSemGetResultName(GLSLTreeContext *psGLSLTreeContext,
									IMG_CHAR *pszResultName,
									GLSLFullySpecifiedType *psFullySpecifiedType)
{
	if (psFullySpecifiedType->eTypeSpecifier == GLSLTS_STRUCT)
	{
		sprintf(pszResultName, RESULT_STRUCT_STRING,
				GetSymbolName(psGLSLTreeContext->psSymbolTable, psFullySpecifiedType->uStructDescSymbolTableID),
				GLSLTypeSpecifierDescTable(psFullySpecifiedType->eTypeSpecifier),
				psGLSLTreeContext->uNumResults);
	}
	else
	{
		sprintf(pszResultName, RESULT_STRING, GLSLTypeSpecifierDescTable(psFullySpecifiedType->eTypeSpecifier), psGLSLTreeContext->uNumResults);
	}


	psGLSLTreeContext->uNumResults++;
}


/******************************************************************************
 * Function Name: ASTSemCreateHashedFunctionName
 *
 * Inputs       : psSymbolTable
                  pszFunctionName  - Name of the function
				    uNumParameters   - Number of arguments to the function.
					psFullySpecifiedType[] - Array of length uNumParameters storing the arguments' types.
 * Outputs      : -
 * Returns      : The hashed name of a function
 * Globals Used : -
 *
 * Description  : Given a function name and its parameters, returns a "hashed function name"
                  that is unique even in function overloading is taking place.
 *****************************************************************************/
IMG_INTERNAL IMG_CHAR *ASTSemCreateHashedFunctionName(SymTable               *psSymbolTable,
										IMG_CHAR               *pszFunctionName,
										IMG_UINT32              uNumParameters,
										GLSLFullySpecifiedType psFullySpecifiedType[/*uNumParameters*/])
{
	IMG_UINT32 i, uNameSize = 0;

	IMG_CHAR *pszHashedFunctionName;

	/**
	 * OGL64 Review.
	 * Use size_t for strlen?
	 */
	uNameSize = (IMG_UINT32)strlen(pszFunctionName) + 5;

	/* Calc size of name */
	for (i = 0; i <  uNumParameters; i++)
	{
		/* add space for type specifier field */
		/**
		 * OGL64 Review.
		 * Use size_t for strlen?
		 */
		uNameSize += (IMG_UINT32)strlen(GLSLTypeSpecifierDescTable(psFullySpecifiedType[i].eTypeSpecifier));

		/* add space for struct names */
		if (psFullySpecifiedType[i].eTypeSpecifier == GLSLTS_STRUCT)
		{
			IMG_CHAR *pszStructName = GetSymbolName(psSymbolTable,
													psFullySpecifiedType[i].uStructDescSymbolTableID);

			if(pszStructName)	/* Check in case the parameter struct type was not found - parser will have already thrown an error. */
			{
				/**
				 * OGL64 Review.
				 * Use size_t for strlen?
				 */
				uNameSize += (IMG_UINT32)strlen(pszStructName);
			}
		}

		/* add space for arrays */
		if (psFullySpecifiedType[i].iArraySize)
		{
			uNameSize += 12;
		}
	}

	/*
	   Allocate memory for function name -
	   This could be slow, might be better to create a static allocation and allocate from that
	*/
	pszHashedFunctionName = DebugMemAlloc(uNameSize);

	if(!pszHashedFunctionName)
	{
		return IMG_NULL;
	}

	sprintf(pszHashedFunctionName, "fn_%s@", pszFunctionName);

	for (i = 0; i <  uNumParameters; i++)
	{
		/* build up function name based on parameter types */
		strcat(pszHashedFunctionName, GLSLTypeSpecifierDescTable(psFullySpecifiedType[i].eTypeSpecifier));

		if (psFullySpecifiedType[i].eTypeSpecifier == GLSLTS_STRUCT)
		{
			IMG_CHAR *pszStructName = GetSymbolName(psSymbolTable,
													psFullySpecifiedType[i].uStructDescSymbolTableID);

			if(pszStructName)
			{
				strcat(pszHashedFunctionName, pszStructName);
			}

		}

		if (psFullySpecifiedType[i].iArraySize)
		{
#if 0
			IMG_CHAR acArrayString[15];
			
			sprintf(acArrayString, "[%lu]", psFullySpecifiedType[i].iArraySize);

			strcat(pszHashedFunctionName, acArrayString);
#else
			strcat(pszHashedFunctionName, "[]");
#endif
		}
	}

	return pszHashedFunctionName;
}


/******************************************************************************
 * Function Name: ASTCheckNodeNumChildren
 *
 * Inputs       : psNode
 * Outputs      : -
 * Returns      : The number of children nodes that a given node is expected to have.
 * Globals Used : -
 *
 * Description  : See above.
 *****************************************************************************/
static IMG_UINT32 ASTCheckNodeNumChildren(GLSLCompilerPrivateData *psCPD, GLSLNode *psNode)
{
	IMG_UINT32 uNumChildren;

	switch (psNode->eNodeType)
	{
		case GLSLNT_EXPRESSION:
		case GLSLNT_SUBEXPRESSION:
		case GLSLNT_FUNCTION_CALL:
		case GLSLNT_IDENTIFIER:
		case GLSLNT_RETURN:
		case GLSLNT_DECLARATION:
		case GLSLNT_IF:
		{
			return psNode->uNumChildren;
		}
		case GLSLNT_CONTINUE:
		case GLSLNT_DISCARD:
		case GLSLNT_BREAK:
		{
			uNumChildren = 0;
			break;
		}
		case GLSLNT_PRE_INC:
		case GLSLNT_PRE_DEC:
		case GLSLNT_NEGATE:
		case GLSLNT_POSITIVE:
		case GLSLNT_NOT:
		case GLSLNT_POST_INC:
		case GLSLNT_POST_DEC:
		case GLSLNT_ARRAY_LENGTH:
		{
			uNumChildren = 1;
			break;
		}
		case GLSLNT_DIVIDE:
		case GLSLNT_ADD:
		case GLSLNT_SUBTRACT:
		case GLSLNT_MULTIPLY:
		case GLSLNT_LESS_THAN:
		case GLSLNT_GREATER_THAN:
		case GLSLNT_LESS_THAN_EQUAL:
		case GLSLNT_GREATER_THAN_EQUAL:
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
		case GLSLNT_LOGICAL_OR:
		case GLSLNT_LOGICAL_XOR:
		case GLSLNT_LOGICAL_AND:
		case GLSLNT_EQUAL:
		case GLSLNT_DIV_ASSIGN:
		case GLSLNT_ADD_ASSIGN:
		case GLSLNT_SUB_ASSIGN:
		case GLSLNT_MUL_ASSIGN:
		case GLSLNT_ARRAY_SPECIFIER:
		case GLSLNT_FIELD_SELECTION:
		case GLSLNT_WHILE:
		case GLSLNT_DO:
		{
			uNumChildren = 2;
			break;
		}

		case GLSLNT_QUESTION:
		{
			uNumChildren = 3;
			break;
		}
		default:
		{
			LOG_INTERNAL_NODE_ERROR((psNode,"ASTCheckNodeNumChildren: Unrecognised operator type (%08X)\n", psNode->eNodeType));
			return 0;
		}
	}

	if (uNumChildren != psNode->uNumChildren)
	{
		LOG_INTERNAL_NODE_ERROR((psNode,"ASTCheckNodeNumChildren: Invalid number of children\n"));
	}

	return psNode->uNumChildren;
}

/******************************************************************************
 * Function Name: ASTSemGetSizeInfo
 *
 * Inputs       : psGLSLTreeContext
                  psFullySpecifiedType
                  bUseArraySize - If the type is an array, whether to return
				                    the size of the base element or the size of the full array.
 * Outputs      :
 * Returns      : Size in bytes of a given type.
 * Globals Used : -
 *
 * Description  : See above.
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 ASTSemGetSizeInfo(GLSLTreeContext        *psGLSLTreeContext,
							 GLSLFullySpecifiedType *psFullySpecifiedType,
							 IMG_BOOL                bUseArraySize)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uSizeInBytes = 0;

	if (psFullySpecifiedType->eTypeSpecifier == GLSLTS_INVALID)
	{
		LOG_INTERNAL_ERROR(("ASTSemGetSizeInfo: Can't get size info for GLSLTS_INVALID\n"));
		return 0;
	}
	else if (psFullySpecifiedType->eTypeSpecifier == GLSLTS_STRUCT)
	{
		/* Get structure defintion data */
		GLSLStructureDefinitionData *psStructureDefinitionData = 
			(GLSLStructureDefinitionData *)GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																		 psFullySpecifiedType->uStructDescSymbolTableID,
																		 GLSLSTDT_STRUCTURE_DEFINITION);

		if (!psStructureDefinitionData)
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			LOG_INTERNAL_ERROR(("ASTSemGetSizeInfo: Could not retrieve struct definition data\n"));
			return 0;
		}

#if 0	/* A structure can contain only texture samplers, in which case it would not be allocated any registers, only texture samplers. */
		if (!psStructureDefinitionData->uStructureSizeInBytes)
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			LOG_INTERNAL_ERROR(("ASTSemGetSizeInfo: Structure size was 0\n"));
			return 0;
		}
#endif

		uSizeInBytes = psStructureDefinitionData->uStructureSizeInBytes;
	}
	else 
	{
		uSizeInBytes = GLSLTypeSpecifierSizeTable(psFullySpecifiedType->eTypeSpecifier);
	}

	if (bUseArraySize && (psFullySpecifiedType->iArraySize > 0))
	{
		uSizeInBytes *= psFullySpecifiedType->iArraySize;
	}

	return uSizeInBytes;
}

/******************************************************************************
 * Function Name: ASTSemGetIdentifierData
 *
 * Inputs       : psGLSLTreeContext
                  psNode
 * Outputs      : puSymbolTableID - The symbol table ID of the given node.
 * Returns      : The identifier data of a given node.
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static GLSLIdentifierData *ASTSemGetIdentifierData(GLSLTreeContext		*psGLSLTreeContext,
													const GLSLNode		*psNode,
													IMG_UINT32			*puSymbolTableID)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psIdentifierData;
	GLSLGenericData    *psGenericData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, psNode->uSymbolTableID);
	IMG_UINT32          uSymbolTableID = psNode->uSymbolTableID;

	if (!psGenericData)
	{
		LOG_INTERNAL_ERROR(("ASTSemGetIdentifierData: Symbol was not found\n"));
		return IMG_NULL;
	}

	if (psGenericData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER)
	{
		psIdentifierData = (GLSLIdentifierData *)psGenericData;
	}
	else if (psGenericData->eSymbolTableDataType == GLSLSTDT_FUNCTION_CALL)
	{
		GLSLFunctionCallData *psFunctionCallData = (GLSLFunctionCallData *)psGenericData;

		GLSLFunctionDefinitionData *psFunctionDefinitionData =
			(GLSLFunctionDefinitionData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
															 psFunctionCallData->uFunctionDefinitionSymbolID);

		if (!psFunctionDefinitionData)
		{
			LOG_INTERNAL_ERROR(("ASTSemGetIdentifierData: Symbol was not found\n"));
			return IMG_NULL;
		}
		if (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)	
		{
			LOG_INTERNAL_ERROR(("ASTSemGetIdentifierData: Symbol was not of right type (%08X)\n", psFunctionDefinitionData->eSymbolTableDataType));
			return IMG_NULL;
		}

		psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, psFunctionDefinitionData->uReturnDataSymbolID);

		uSymbolTableID = psFunctionDefinitionData->uReturnDataSymbolID;

		if (!psIdentifierData)
		{
			LOG_INTERNAL_ERROR(("ASTSemGetIdentifierData: Symbol was not found\n"));
			return IMG_NULL;
		}
		if (psIdentifierData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)	
		{
			LOG_INTERNAL_ERROR(("ASTSemGetIdentifierData: Symbol was not of right type (%08X)\n", psFunctionDefinitionData->eSymbolTableDataType));
			return IMG_NULL;
		}
	}
	else
	{
		return IMG_NULL;
	}

	if (puSymbolTableID)
	{
		*puSymbolTableID = uSymbolTableID;
	}

	return psIdentifierData;
}

/******************************************************************************
 * Function Name: ASTSemConvertMatrix
 *
 * Inputs       : psSrcFullySpecifiedType  - Type of the source matrix
                  psDestFullySpecifiedType - Type of the destination matrix
                  pvSrc - Pointer to the data of the source matrix
 * Outputs      : ppvDest - Pointer to the pointer to the data of the destination matrix.
 * Returns      : The number of components saved into ppvDest
 * Globals Used : -
 *
 * Description  : Stores the top-left submatrix of pvSrc into the top-left submatrix of ppvDst.
                  Updates *ppvDst to point to the end of the written data.
 *****************************************************************************/
static IMG_UINT32 ASTSemConvertMatrix(GLSLFullySpecifiedType  *psSrcFullySpecifiedType,
							   GLSLFullySpecifiedType  *psDestFullySpecifiedType,
							   IMG_VOID                *pvSrc,
							   IMG_VOID               **ppvDest)
{
	IMG_UINT32  i, j;
	IMG_UINT32  uNumSrcCols        = GLSLTypeSpecifierDimensionTable(psSrcFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32  uNumSrcRows        = GLSLTypeSpecifierIndexedTable(psSrcFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32  uNumDestCols       = GLSLTypeSpecifierDimensionTable(psDestFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32  uNumDestRows       = GLSLTypeSpecifierIndexedTable(psDestFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32  uNumSmallestRows   = MIN(uNumSrcRows, uNumDestRows);
	IMG_UINT32  uNumSmallestCols   = MIN(uNumSrcCols, uNumDestCols);
	IMG_UINT32  uNumDestComponents = uNumDestCols * uNumDestRows;

	IMG_FLOAT  *pfSrc       = (IMG_FLOAT*)pvSrc;
	IMG_FLOAT  *pfDest      = (IMG_FLOAT*)*ppvDest;

	/* Set Dest matrix to 0's */
	for (i = 0; i < uNumDestComponents; i++)
	{
		pfDest[i] = 0.0f;
	}

	/* Set diagonal elements to 1.0f */
	for (i = 0; i < uNumDestCols; i++)
	{
		pfDest[(i * uNumDestRows) + i] = 1.0f;
	}

	/* Copy the src matrix to the top left of the dest matrix */
	for (i = 0; i < uNumSmallestCols; i++)
	{
		for (j = 0; j < uNumSmallestRows; j++)
		{
			pfDest[(i * uNumDestRows) + j] = pfSrc[(i * uNumSrcRows) + j];
		}
	}

	/* Update original pointer */
	*ppvDest = (IMG_VOID *)&pfDest[uNumDestComponents];

	/* Return number of components copied */
	return uNumDestComponents;
}

/******************************************************************************
 * Function Name: ASTSemConvertTypes
 *
 * Inputs       : psGLSLTreeContext
                  psSrcFullySpecifiedType
                  psDestFullySpecifiedType
                  pvSrc
                  bExpand
                  
 * Outputs      : ppvDest
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Copies the data of pvSrc into *ppvDest if the type conversion
                  is allowed. Returns the number of components copied. Updates ppvDest
                  to point to the end of the written data.
 *****************************************************************************/
static IMG_UINT32 ASTSemConvertTypes(GLSLTreeContext         *psGLSLTreeContext,
							  GLSLFullySpecifiedType  *psSrcFullySpecifiedType,
							  GLSLFullySpecifiedType  *psDestFullySpecifiedType,
							  IMG_VOID                *pvSrc,
							  IMG_VOID               **ppvDest,
							  IMG_BOOL                 bExpand)
{
	IMG_UINT32 uSrcNumComponents  = GLSLTypeSpecifierNumElementsTable(psSrcFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32 uDestNumComponents = GLSLTypeSpecifierNumElementsTable(psDestFullySpecifiedType->eTypeSpecifier);
	IMG_UINT32 uNumComponentsToCopy;
	IMG_UINT32 i;
	GLSLTypeSpecifier eSrcBaseTypeSpecifier  = GLSLTypeSpecifierBaseTypeTable(psSrcFullySpecifiedType->eTypeSpecifier);
	GLSLTypeSpecifier eDestBaseTypeSpecifier = GLSLTypeSpecifierBaseTypeTable(psDestFullySpecifiedType->eTypeSpecifier);

	if (psSrcFullySpecifiedType->iArraySize)
	{
		uSrcNumComponents *= psSrcFullySpecifiedType->iArraySize;
	}
	if (psDestFullySpecifiedType->iArraySize)
	{
		uDestNumComponents *= psDestFullySpecifiedType->iArraySize;
	}

	if (bExpand)
	{
		if (uSrcNumComponents != 1)
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			LOG_INTERNAL_ERROR(("ASTSemConvertTypes: If expanding there should only be one source component \n"));
			return 0;
		}
		uNumComponentsToCopy = uDestNumComponents;
	}
	/* Matrix conversions (of different sizes) get handled a little differently */
	else if (GLSL_IS_MATRIX(psSrcFullySpecifiedType->eTypeSpecifier)  && 
			 GLSL_IS_MATRIX(psDestFullySpecifiedType->eTypeSpecifier) && 
			 (uSrcNumComponents != uDestNumComponents))
	{
		return ASTSemConvertMatrix(psSrcFullySpecifiedType, 
								   psDestFullySpecifiedType,
								   pvSrc,
								   ppvDest);
	}
	else
	{
		uNumComponentsToCopy = uSrcNumComponents;
	}


	switch (eSrcBaseTypeSpecifier)
	{
		case GLSLTS_FLOAT:
		{
			IMG_FLOAT *pfSrc = (IMG_FLOAT *)pvSrc;
			
			switch (eDestBaseTypeSpecifier)
			{
				case GLSLTS_FLOAT:
				{
					IMG_FLOAT *pfDest = (IMG_FLOAT *)*ppvDest;

					if (GLSL_IS_MATRIX(psDestFullySpecifiedType->eTypeSpecifier) && bExpand)
					{
						IMG_UINT32 uDiagonalCheck = GLSLTypeSpecifierDimensionTable(psDestFullySpecifiedType->eTypeSpecifier) + 1;

						/* Copy the data to the diagonals, zero the rest */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							if ((i % uDiagonalCheck) == 0)
							{
								pfDest[i] = pfSrc[0];
							}
							else
							{
								pfDest[i] = 0.0f;
							}
						}
					}
					else
					{
						/* Copy the data */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							pfDest[i] = pfSrc[bExpand ? 0 : i];
						}
					}

					/* Update original pointer */
					*ppvDest = (IMG_VOID *)&(pfDest[i]);

					break;
				}
				case GLSLTS_INT:
				{
					IMG_INT32 *piDest = (IMG_INT32 *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						piDest[i] = (IMG_INT32)(pfSrc[bExpand ? 0 : i]);
					}
			
					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(piDest[i]);

					break;
				}
				case GLSLTS_BOOL:
				{
					IMG_BOOL *pbDest = (IMG_BOOL *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						pbDest[i] = (pfSrc[bExpand ? 0 : i]) ? IMG_TRUE : IMG_FALSE;
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(pbDest[i]);

					break;
				}
				default:
				{
					GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

					LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Cannot convert float types to type specifier (%08X)\n", 
									 psDestFullySpecifiedType->eTypeSpecifier));
					return 0;
				}
			}

			break;
		}
		case GLSLTS_INT:
		{
			IMG_INT32 *piSrc = (IMG_INT32 *)pvSrc;

			switch (eDestBaseTypeSpecifier)
			{
				case GLSLTS_FLOAT:
				{
					IMG_FLOAT *pfDest = (IMG_FLOAT *)*ppvDest;

					if (GLSL_IS_MATRIX(psDestFullySpecifiedType->eTypeSpecifier) && bExpand)
					{
						IMG_UINT32 uDiagonalCheck = GLSLTypeSpecifierDimensionTable(psDestFullySpecifiedType->eTypeSpecifier) + 1;

						/* Copy the data to the diagonals, zero the rest  */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							if ((i % uDiagonalCheck) == 0)
							{
								pfDest[i] = (IMG_FLOAT)piSrc[0];
							}
							else
							{
								pfDest[i] = 0.0f;
							}
						}
					}
					else
					{
						/* Copy the data */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							pfDest[i] = (IMG_FLOAT)piSrc[bExpand ? 0 : i];
						}
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(pfDest[i]);

					break;
				}
				case GLSLTS_INT:
				{
					IMG_INT32 *piDest = (IMG_INT32 *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						piDest[i] = piSrc[bExpand ? 0 : i];
					}
			
					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(piDest[i]);
					
					break;
				}
				case GLSLTS_BOOL:
				{
					IMG_BOOL *pbDest = (IMG_BOOL *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						pbDest[i] = (piSrc[bExpand ? 0 : i]) ? IMG_TRUE : IMG_FALSE;
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(pbDest[i]);

					break;
				}
				default:
				{
					GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

					LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Cannot convert int types to type specifier (%08X)\n", 
									 psDestFullySpecifiedType->eTypeSpecifier));
					return 0;
				}
			}

			break;
		}
		case GLSLTS_BOOL:
		{
			IMG_BOOL *pbSrc = (IMG_BOOL *)pvSrc;
			
			switch (eDestBaseTypeSpecifier)
			{
				case GLSLTS_FLOAT:

				{
					IMG_FLOAT *pfDest = (IMG_FLOAT *)*ppvDest;
					
					if (GLSL_IS_MATRIX(psDestFullySpecifiedType->eTypeSpecifier) && bExpand)
					{
						IMG_UINT32 uDiagonalCheck = GLSLTypeSpecifierDimensionTable(psDestFullySpecifiedType->eTypeSpecifier) + 1;
						
						/* Copy the data to the diagonals, zero the rest  */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							if ((i % uDiagonalCheck) == 0)
							{ 
								pfDest[i] = (IMG_FLOAT)pbSrc[0];
							}
							else
							{
								pfDest[i] = 0.0f;
							}
						}
					}
					else
					{
						/* Copy the data */
						for (i = 0; i < uNumComponentsToCopy; i++)
						{
							pfDest[i] = (IMG_FLOAT)pbSrc[bExpand ? 0 : i];
						}
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(pfDest[i]);

					break;
				}
				case GLSLTS_INT:
				{
					IMG_INT32 *piDest = (IMG_INT32 *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						piDest[i] = (IMG_INT32)pbSrc[bExpand ? 0 : i];
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(piDest[i]);
					
					break;
				}
				case GLSLTS_BOOL:
				{
					IMG_BOOL *pbDest = (IMG_BOOL *)*ppvDest;

					/* Copy the data */
					for (i = 0; i < uNumComponentsToCopy; i++)
					{
						pbDest[i] = pbSrc[bExpand ? 0 : i];
					}

					/* Update original pointers */
					*ppvDest = (IMG_VOID *)&(pbDest[i]);
					
					break;
				}
				default:
				{
					GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

					LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Cannot covert bool types to type specifier (%08X)\n", 
									 psDestFullySpecifiedType->eTypeSpecifier));
					return 0;
				}

			}

			break;
		}
		case GLSLTS_STRUCT:
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			IMG_BYTE *pbDest = (IMG_BYTE *)*ppvDest;
			GLSLStructureDefinitionData *psStructureDefinitionData;

			if (psDestFullySpecifiedType->eTypeSpecifier != GLSLTS_STRUCT)
			{
				LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Cannot convert structs only straight copies allowed\n"));
				return 0;
			}
			else if (psDestFullySpecifiedType->uStructDescSymbolTableID != psSrcFullySpecifiedType->uStructDescSymbolTableID)
			{
				LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Structure types were different. Cannot copy\n"));
				return 0;
			}

			/* Get the the structure definition */
			psStructureDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																	  psSrcFullySpecifiedType->uStructDescSymbolTableID,
																	  GLSLSTDT_STRUCTURE_DEFINITION);

			/* Copy the structure data */
			memcpy(pbDest, pvSrc, psStructureDefinitionData->uStructureSizeInBytes);

			*ppvDest = (IMG_VOID *)&(pbDest[psStructureDefinitionData->uStructureSizeInBytes]);

			break;
		}

		default:
		{
			GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

			LOG_INTERNAL_ERROR(("ASTSemConvertTypes: Cannot process this src type specifier (%08X)\n", psSrcFullySpecifiedType->eTypeSpecifier));
			return 0;
		}
	}

	return uNumComponentsToCopy;
}

/******************************************************************************
 * Function Name: ASTSemReduceConstantConstructor
 *
 * Inputs       : psGLSLTreeContext
                  psFunctionCallNode
                  psFunctionCallData
                  psFunctionDefinitionData
                  uTotalSizeOfParameters
                  bExpandSourceParameter
                  eDestPrec
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Replace a constructor that only has constant arguments
                  with the value that it would return.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantConstructor(GLSLTreeContext            *psGLSLTreeContext,
										 GLSLNode                   *psFunctionCallNode,
										 GLSLFunctionCallData       *psFunctionCallData,
										 GLSLFunctionDefinitionData *psFunctionDefinitionData,
										 IMG_UINT32                  uTotalSizeOfParameters,
										 IMG_BOOL                    bExpandSourceParameter,
										 GLSLPrecisionQualifier      eDestPrec)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_VOID                     *pvDestData, *pvSrcData;
	GLSLIdentifierData           sResultData;
	IMG_UINT32                   i, uNumChildren = psFunctionCallNode->uNumChildren;
	GLSLStructureDefinitionData  *psStructureDefinitionData = IMG_NULL;
	/* FIXME: possible buffer overflow  */
	IMG_CHAR                     acResultName[256];

	/* 
	   Get return type from function call data NOT definition data as there may have been an array 
	   size specified on the constructor 
	*/
	GLSLFullySpecifiedType *psReturnFullySpecifiedType = &psFunctionCallData->sFullySpecifiedType;

	/* The size we want to allocate is the largest of the destination and the source */
	if (uTotalSizeOfParameters < ASTSemGetSizeInfo(psGLSLTreeContext, psReturnFullySpecifiedType, IMG_TRUE))
	{
		uTotalSizeOfParameters = ASTSemGetSizeInfo(psGLSLTreeContext, psReturnFullySpecifiedType, IMG_TRUE);
	}
	
	/* Set up the initial data */
	sResultData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	/* Because we use the total size for costructors we might copy more than we need, but that shouldn't be a problem */
	sResultData.uConstantDataSize                            = uTotalSizeOfParameters;
	sResultData.pvConstantData                               = DebugMemAlloc(uTotalSizeOfParameters);

	if(!sResultData.pvConstantData)
	{
		return IMG_FALSE;
	}

	INIT_FULLY_SPECIFIED_TYPE(&sResultData.sFullySpecifiedType);

	sResultData.sFullySpecifiedType.eTypeSpecifier           = psReturnFullySpecifiedType->eTypeSpecifier;
	sResultData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_CONST;
	sResultData.sFullySpecifiedType.iArraySize               = psReturnFullySpecifiedType->iArraySize;
	sResultData.sFullySpecifiedType.uStructDescSymbolTableID = psReturnFullySpecifiedType->uStructDescSymbolTableID;

	/* User defined constructors have no precision as they are structs */
	if (psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR)
	{
		sResultData.sFullySpecifiedType.ePrecisionQualifier = eDestPrec;
	}

	sResultData.eLValueStatus                                = GLSLLV_NOT_L_VALUE;
	sResultData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;	
	sResultData.eIdentifierUsage                             = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_INTERNALRESULT);
	sResultData.uConstantAssociationSymbolID                 = 0;

	if (sResultData.sFullySpecifiedType.iArraySize)
	{
		sResultData.iActiveArraySize                         = -1;
		sResultData.eArrayStatus                             = GLSLAS_ARRAY_SIZE_FIXED;
	}
	else
	{
		sResultData.iActiveArraySize                         = -1;
		sResultData.eArrayStatus                             = GLSLAS_NOT_ARRAY;
	}

	/* Check alloc suceeded */
	if (!sResultData.pvConstantData)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Failed to alloc memory for constant data\n"));
		return IMG_FALSE;
	}

	if (psFunctionDefinitionData->eFunctionType == GLSLFT_USERDEFINED_CONSTRUCTOR)
	{
		/* Get structure defintion */
		psStructureDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																  psReturnFullySpecifiedType->uStructDescSymbolTableID,
																  GLSLSTDT_STRUCTURE_DEFINITION);
		if (!psStructureDefinitionData)
		{
			return IMG_FALSE;
		}
	}
	
	
	/* Get pointer to start of data */
	pvDestData = (IMG_VOID *)sResultData.pvConstantData;
	
	/* Copy the data */
	for (i = 0; i < psFunctionCallNode->uNumChildren; i++)
	{
		GLSLFullySpecifiedType *psDestFullySpecifiedType;

		/* Fetch the data for this parameter */
		GLSLIdentifierData *psParamData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																		psFunctionCallNode->ppsChildren[i]->uSymbolTableID,
																		GLSLSTDT_IDENTIFIER);
		
		/* DO NOT REMOVE FOR NON DEBUG BUILDS */
		if (!psParamData)
		{
			DebugMemFree(sResultData.pvConstantData);

			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Could not fetch param %d's data\n", i));
			return IMG_FALSE;
		}

		/* Check was valid data type */
		if (psParamData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Param %d was not IDENTIFIER was %08X can't extract constant data\n",
								 i,
								 psParamData->eSymbolTableDataType));

			return IMG_FALSE;
		}

		/* return silently so as not to cascade error messages */
		if (psParamData->eIdentifierUsage & GLSLIU_ERROR_INITIALISING)
		{
			DebugMemFree(sResultData.pvConstantData);
			return IMG_FALSE;
		}

		/* Check for valid data - DO NOT REMOVE FOR NON DEBUG BUILDS */
		if (!psParamData->pvConstantData)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: No constant data to copy for param %d\n", i));
			DebugMemFree(sResultData.pvConstantData);
			return IMG_FALSE;
		}

		/* 
		   Get the destination type specifier.
		   For a structure constructor it will be the next member of the structure
		   For a constructor it's the type of the constructor(!)
		*/
		if (psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR
			|| psFunctionCallData->eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED)
		{
			psDestFullySpecifiedType = psReturnFullySpecifiedType;
		}
		else
		{
			psDestFullySpecifiedType = &(psStructureDefinitionData->psMembers[i].sIdentifierData.sFullySpecifiedType);

			if (psDestFullySpecifiedType->eTypeSpecifier != psParamData->sFullySpecifiedType.eTypeSpecifier)
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Param type does not match member, this should already have been caught\n"));
				return IMG_FALSE;
			}
			if (psDestFullySpecifiedType->iArraySize != psParamData->sFullySpecifiedType.iArraySize)
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Param array size does not match member, this should already have been caught\n"));
				return IMG_FALSE;
			}
		}


		pvSrcData = psParamData->pvConstantData;

		/* Copy the data */
		ASTSemConvertTypes(psGLSLTreeContext,
						   &(psParamData->sFullySpecifiedType),
						   psDestFullySpecifiedType,
						   pvSrcData,
						   &pvDestData,
						   bExpandSourceParameter);
	}

	/* Special case floats and ints to try and get some reuse of existing data */
	switch (sResultData.sFullySpecifiedType.eTypeSpecifier)
	{
		case GLSLTS_FLOAT:
		{
			IMG_FLOAT *pfData = (IMG_FLOAT *)sResultData.pvConstantData;

			if (sResultData.eArrayStatus == GLSLAS_NOT_ARRAY)
			{
				AddFloatConstant(psCPD, psGLSLTreeContext->psSymbolTable,
								 *pfData,
								 sResultData.sFullySpecifiedType.ePrecisionQualifier,
								 IMG_TRUE,
								 &(psFunctionCallNode->uSymbolTableID));
				break;
			}
		}
		/* Fall through to code below */
		case GLSLTS_INT:
		{
			if (sResultData.eArrayStatus == GLSLAS_NOT_ARRAY)
			{
				IMG_INT *piData = (IMG_INT *)sResultData.pvConstantData;

				AddIntConstant(psCPD, psGLSLTreeContext->psSymbolTable,
							   *piData,
							   sResultData.sFullySpecifiedType.ePrecisionQualifier,
							   IMG_TRUE,
							   &(psFunctionCallNode->uSymbolTableID));
				break;
			}
			/* Fall through to code below */
		}
		default:
		{
			/* Get a unique name for this result */
			ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));

			/* Add the data to the symbol table */
			if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable, 
							   acResultName, 
							   &sResultData, 
							   IMG_FALSE,
							   &(psFunctionCallNode->uSymbolTableID)))
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantConstructor: Failed to add result %s to table\n", acResultName));
			}
		}
	}

	/* Free the constant data */
	DebugMemFree(sResultData.pvConstantData);
	
	/* 
	   Remove the node children, cannot use value of num children inside
	   node as it decremented by the function
	*/
	for (i = 0; i < uNumChildren; i++)
	{
		/* 0 is correct, should NOT be i */
		ASTSemRemoveChildNode(psGLSLTreeContext, psFunctionCallNode, 0, IMG_FALSE);
	}

	/* Change the type of the node */
	psFunctionCallNode->eNodeType = GLSLNT_IDENTIFIER;

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTSemInsertConstructor
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode    - The node that has a children that will be replaced by a constructor.
                  uChildToConvert - The child of psResultNode that will be replaced by a ctor.
                  eDestPrec       - Precision of the ctor.
                  eDestTypeSpecifier
                  bSemanticCheck
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Replaces the uChildToConvert-th child of psResultNode with a constructor.
                  This is done to make the whole expression have the same precision and/or
                  to cast from ints to floats (only in lang. ver. 120)
 *****************************************************************************/
static IMG_BOOL ASTSemInsertConstructor(GLSLTreeContext        *psGLSLTreeContext,
								 GLSLNode               *psResultNode,
								 IMG_UINT32              uChildToConvert,
								 GLSLPrecisionQualifier  eDestPrec,
								 GLSLTypeSpecifier       eDestTypeSpecifier,
								 IMG_BOOL                bSemanticCheck)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);
	GLSLNode *psChildNode = psResultNode->ppsChildren[uChildToConvert];
	GLSLIdentifierData *psIdentifierData;
	GLSLFullySpecifiedType *psFullySpecifiedType;
	IMG_BOOL                bConvertPrecision = IMG_TRUE;

	/* Current only support int -> float conversion */
	IMG_BOOL                bConvertDataType = IMG_TRUE;

	/* FIXME: Possible overflow */
	IMG_CHAR acConstructorName[256], acFunctionCallName[256];

	IMG_UINT32 uFunctionDefinitionSymbolTableID;

	GLSLNode             *psFunctionCallNode;
	GLSLFunctionCallData sFunctionCallData;

	/* get the childs identifier data */
	psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psChildNode, IMG_NULL);

	/* Get the types of the specifier */
	if (!psIdentifierData)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemInsertConstructor: Failed to retrieve symbol data\n"));
		return IMG_FALSE;
	}

	/* Get the fully specified type */
	psFullySpecifiedType = &(psIdentifierData->sFullySpecifiedType);

	/* Work out which conversions need doing - casts avoids warnings on compact memory model builds */
	if (eDestPrec == GLSLPRECQ_UNKNOWN || (eDestPrec ==  (GLSLPrecisionQualifier)psFullySpecifiedType->ePrecisionQualifier))
	{
		bConvertPrecision = IMG_FALSE;
		eDestPrec         = psFullySpecifiedType->ePrecisionQualifier;
	}

	if (eDestTypeSpecifier == GLSLTS_INVALID || (eDestTypeSpecifier == (GLSLTypeSpecifier)psFullySpecifiedType->eTypeSpecifier))
	{
		bConvertDataType   = IMG_FALSE;
		eDestTypeSpecifier = psFullySpecifiedType->eTypeSpecifier;
	}

	if (!bConvertPrecision  && !bConvertDataType)
	{
		/* Sometimes a question-mark expression will introduce redundant constructors. */
		return IMG_FALSE;
	}

#ifdef DEBUG
	/* An few sanity checks */

	/* Just check we're dealing with numbers */
	if (!GLSL_IS_NUMBER(psFullySpecifiedType->eTypeSpecifier))
	{
		LOG_INTERNAL_NODE_ERROR((psChildNode,"ASTSemInsertConstructor: '%s' : Cannot convert precision for this type (%s)\n", 
							 GLSLTypeSpecifierDescTable(psFullySpecifiedType->eTypeSpecifier),
							 NODETYPE_DESC(psResultNode->eNodeType)));
		return IMG_FALSE;
	}

	/* Check for unsupported data conversions */
	if (bConvertDataType)
	{
		if (!(GLSL_IS_INT(psFullySpecifiedType->eTypeSpecifier) && GLSL_IS_FLOAT(eDestTypeSpecifier)))
		{
			LOG_INTERNAL_NODE_ERROR((psChildNode,"ASTSemInsertConstructor: Only int to float conversions are supported\n"));
			return IMG_FALSE;
		}
	}
#endif

	{
		/* 
		   Check for trying to convert the precision of data returned by a constructor
		   as we may be able to modify the data returned directly.
		*/

		/* Check if the node that we might modify is in a nested expression - this is needed for example when you make a function call inside a return statement. */
		GLSLNode *psChildToModify = (psChildNode->eNodeType == GLSLNT_EXPRESSION) ? psChildNode->ppsChildren[0] : psChildNode;

		if ((psChildToModify->eNodeType == GLSLNT_FUNCTION_CALL) && bConvertPrecision && !bConvertDataType)
		{
			GLSLFunctionCallData *psFunctionCallData;

			GLSLFunctionDefinitionData *psFunctionDefinitionData;

			/* Get the function call data */
			psFunctionCallData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
															   psChildToModify->uSymbolTableID,
															   GLSLSTDT_FUNCTION_CALL);
			
			if (!psFunctionCallData)
			{
				LOG_INTERNAL_NODE_ERROR((psChildToModify,"ASTSemInsertConstructor: Failed to retrieve function call data\n"));
				return IMG_FALSE;
			}
			
			/* Get the function definition data */
			psFunctionDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																	 psFunctionCallData->uFunctionDefinitionSymbolID,
																	 GLSLSTDT_FUNCTION_DEFINITION);

			if (!psFunctionDefinitionData)
			{
				LOG_INTERNAL_NODE_ERROR((psChildToModify,"ASTSemInsertConstructor: Failed to retrieve function definition data\n"));
				return IMG_FALSE;
			}

			if (psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR)
			{
				/* 
				   Rather than inserting a precision constrcutor on an existing constructor 
				   modify the precision of the data returned by the function call 
				*/
				psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier = eDestPrec;
				
				return IMG_TRUE;
			}
			else if((psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN) &&
				GLSLBuiltInFunctionTextureSampler(psFunctionDefinitionData->eBuiltInFunctionID))
			{
				/* Uniflex supports sample instructions with mismatched precisions.
				   Rather than inserting a constructor here we gently replace the original
				   precision of the sample with the one derived from the whole expression.
				   Blame forwarding: if this doesn't work, it's their fault. They forced us.
				*/
				psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier = eDestPrec;
				return IMG_TRUE;
			}
		}
		/* 
		   For question-mark expressions in addition we have to convert the children nodes too.
		*/
		else if((psChildToModify->eNodeType == GLSLNT_QUESTION) && bConvertPrecision)
		{
			GLSLIdentifierData *psIdentifierData;
			IMG_UINT32 ui32QuestionMarkChild;

			/* Override question-mark precision */
			psIdentifierData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
															   psChildToModify->uSymbolTableID,
															   GLSLSTDT_IDENTIFIER);
			
			if (!psIdentifierData)
			{
				LOG_INTERNAL_NODE_ERROR((psChildToModify,"ASTSemInsertConstructor: Failed to retrieve question mark data\n"));
				return IMG_FALSE;
			}

			psIdentifierData->sFullySpecifiedType.ePrecisionQualifier = eDestPrec;

			/* Override children's precision. May be redundant in some cases. */
			for(ui32QuestionMarkChild = 1; ui32QuestionMarkChild <= 2; ++ui32QuestionMarkChild)
			{
				ASTSemInsertConstructor(psGLSLTreeContext, psChildToModify, ui32QuestionMarkChild, eDestPrec,
						eDestTypeSpecifier, bSemanticCheck);
			}

			return IMG_TRUE;
		}

	} /* Check for special cases where we modify the precision directly instead of creating a constructor for it. */

	/* Create the precision constructor node */
	psFunctionCallNode = ASTCreateNewNode(GLSLNT_FUNCTION_CALL, psChildNode->psParseTreeEntry);

	/* Check creation went OK */
	ASTValidateNodeCreation_ret(psFunctionCallNode, IMG_FALSE);

	/* Add the original data as a child to this node */
	ASTAddNodeChild(psFunctionCallNode, psChildNode);

	/* Swap the references from the result node to the child node to the precision constructor node */
	psResultNode->ppsChildren[uChildToConvert] = psFunctionCallNode;
	psFunctionCallNode->psParent = psResultNode;

	/* Create a constructor name */
	sprintf(acConstructorName, 
			CONSTRUCTOR_STRING, 
			GLSLTypeSpecifierDescTable(eDestTypeSpecifier));
		
	/* Find symbol ID of this constructor */
	if (!FindSymbol(psGLSLTreeContext->psSymbolTable, acConstructorName, &(uFunctionDefinitionSymbolTableID), IMG_FALSE))
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemInsertConstructor: '%s' : no matching constructor found for\n", 
							 acConstructorName));
			
		psFunctionCallNode->eNodeType = GLSLNT_ERROR;
		return IMG_FALSE;
	}

	/* Set up function call data */
	sFunctionCallData.eSymbolTableDataType        = GLSLSTDT_FUNCTION_CALL;
	sFunctionCallData.sFullySpecifiedType         = *psFullySpecifiedType;
	sFunctionCallData.eArrayStatus                = psIdentifierData->eArrayStatus;
	sFunctionCallData.uFunctionDefinitionSymbolID = uFunctionDefinitionSymbolTableID;
	sFunctionCallData.uLoopLevel                  = psGLSLTreeContext->uLoopLevel;
	sFunctionCallData.uConditionLevel             = psGLSLTreeContext->uConditionLevel;

	/* Modify the precision and type qualifier */
	sFunctionCallData.sFullySpecifiedType.ePrecisionQualifier = eDestPrec;
	sFunctionCallData.sFullySpecifiedType.eTypeSpecifier      = eDestTypeSpecifier;
	sFunctionCallData.sFullySpecifiedType.eTypeQualifier      = GLSLTQ_TEMP;

	/* Generate a unique name for it */
	sprintf(acFunctionCallName,
			FUNCTION_CALL_STRING,
			acConstructorName,
			GLSLTypeSpecifierDescTable(sFunctionCallData.sFullySpecifiedType.eTypeSpecifier), 
			psGLSLTreeContext->uNumResults);
		
	psGLSLTreeContext->uNumResults++;

	/* Add the data to the table */
	if (!AddFunctionCallData(psCPD, psGLSLTreeContext->psSymbolTable, 
							 acFunctionCallName, 
							 &sFunctionCallData, 
							 IMG_FALSE,
							 &(psFunctionCallNode->uSymbolTableID)))
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemInsertConstructor: Failed to add function call data %s to symbol table", 
							 acFunctionCallName));
		return IMG_FALSE;
	}

#if 1
	PVR_UNREFERENCED_PARAMETER(bSemanticCheck);

	/* Update the childs identifier usage */
	psIdentifierData->eIdentifierUsage |= GLSLIU_READ;

	/* 
	   If the data is constant, rather than applying a constructor to it to convert its
	   precision, much better to just add some new constant data at the required precision
	   and replace the childs data with that.
	*/
	if (psFullySpecifiedType->eTypeQualifier == GLSLTQ_CONST)
	{
		IMG_UINT32 uTotalSizeOfParameters;

		/* Get the identifier data from the child */
		GLSLIdentifierData *psIdentifierData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																			 psChildNode->uSymbolTableID,
																			 GLSLSTDT_IDENTIFIER);
			
		/* Get the function definition data about the constructor */
		GLSLFunctionDefinitionData *psFunctionDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																							 sFunctionCallData.uFunctionDefinitionSymbolID,
																							 GLSLSTDT_FUNCTION_DEFINITION);
			
		if (!psFunctionDefinitionData)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemInsertConstructor: Failed to retrieve function definition data\n"));
			return IMG_FALSE;
		}

		if (!psIdentifierData)
		{
			return IMG_FALSE;
		}

		uTotalSizeOfParameters = ASTSemGetSizeInfo(psGLSLTreeContext,
												   &psIdentifierData->sFullySpecifiedType,
												   IMG_TRUE);
		/* Reduce this constructor to a value */
		return ASTSemReduceConstantConstructor(psGLSLTreeContext, 
											   psFunctionCallNode,
											   &sFunctionCallData,
											   psFunctionDefinitionData,
											   uTotalSizeOfParameters,
											   IMG_FALSE,
											   eDestPrec);
	}
#else
	if (bSemanticCheck)
	{
		/* Semantic check this new constructor - will also perform constant reduction if required */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psFunctionCallNode, IMG_FALSE);
	}
#endif

#if 0
	printf("Converting (%s) from %s %s to (%s %s)\n",
		   psChildNode->psToken->pvData,
		   GLSLPrecisionQualifierFullDescTable[psFullySpecifiedType->ePrecisionQualifier],
		   GLSLTypeSpecifierDescTable(psFullySpecifiedType->eTypeSpecifier),
		   GLSLPrecisionQualifierFullDescTable[sFunctionCallData.sFullySpecifiedType.ePrecisionQualifier],
		   GLSLTypeSpecifierDescTable(sFunctionCallData.sFullySpecifiedType.eTypeSpecifier));
#endif
		
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemReduceConstantArraySpecifier
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode - Node that represents an array dereference.
                  puSymbolTableID
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Replaces an array derefereced by a constant with the actual element that is indexed.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantArraySpecifier(GLSLTreeContext *psGLSLTreeContext, 
											GLSLNode        *psResultNode,
											IMG_UINT32      *puSymbolTableID)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode *psIdentifierNode, *psArrayIndexNode;
	GLSLIdentifierData *psIdentifierData, *psArrayIndexIdentifierData, sResultData;
	IMG_UINT32 uIndexedDimension, uDataOffsetInBytes, uDataSizeInBytes;
	IMG_INT32 *piArrayIndexValue;
	IMG_BYTE *pbSrc;
	/* FIXME: possible buffer overflow */
	IMG_CHAR  acResultName[200];


	
	if (psResultNode->eNodeType != GLSLNT_ARRAY_SPECIFIER)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantArraySpecifier: Not array specifier\n" ));
		return IMG_FALSE;
	}
	
	/* Left node is identifier */
	psIdentifierNode = psResultNode->ppsChildren[0];

	/* Right node is array index */
	psArrayIndexNode = psResultNode->ppsChildren[1];

	/* Get identifier info */
	psIdentifierData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable,
													 psIdentifierNode->uSymbolTableID,
													 GLSLSTDT_IDENTIFIER);

	/* Get array index info */
	psArrayIndexIdentifierData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable,
															   psArrayIndexNode->uSymbolTableID,
															   GLSLSTDT_IDENTIFIER);

	/* Quick safety check */
	if (!psIdentifierData->pvConstantData || !psArrayIndexIdentifierData->pvConstantData)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantArraySpecifier: No constant data found for one of the children\n" ));
		return IMG_FALSE;
	}

	/* Setup the data */
	sResultData = *psIdentifierData;

	/* Get array index value */
	piArrayIndexValue = (IMG_INT32 *)psArrayIndexIdentifierData->pvConstantData;

	/* Check index value */
	if (*piArrayIndexValue < 0)
	{
		return IMG_FALSE;
	}

	/* Are we indexing into an array? */
	if (psIdentifierData->sFullySpecifiedType.iArraySize)
	{
		/* Result is not an array */
		sResultData.eArrayStatus = GLSLAS_NOT_ARRAY;
		sResultData.sFullySpecifiedType.iArraySize   = 0;
	}
	/* Must be a vector or matrix */
	else
	{
		/* Get dimension of the indexed result */
		uIndexedDimension = GLSLTypeSpecifierIndexedTable(psIdentifierData->sFullySpecifiedType.eTypeSpecifier);

		/* Check we're dealing with a vector or matrix - only valid types since arrays cannot be constants */
		if (!uIndexedDimension)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantArraySpecifier: Can only apply to Matrices and vectors\n" ));
			return IMG_FALSE;
		}


		/* Modify it to be a scalar/vector of the same type */
		sResultData.sFullySpecifiedType.eTypeSpecifier = (GLSLTypeSpecifier)(GLSLTypeSpecifierBaseTypeTable(psIdentifierData->sFullySpecifiedType.eTypeSpecifier) +
														  (uIndexedDimension - 1));
	}

	/* Calculate offset from which to copy data */
	uDataOffsetInBytes = ASTSemGetSizeInfo(psGLSLTreeContext, &(sResultData.sFullySpecifiedType), IMG_FALSE) * *piArrayIndexValue;

	/* get pointer to src */
	pbSrc = ((IMG_BYTE *)(psIdentifierData->pvConstantData)) + uDataOffsetInBytes;

	/* Get size of data */
	uDataSizeInBytes = ASTSemGetSizeInfo(psGLSLTreeContext, &(sResultData.sFullySpecifiedType), IMG_FALSE);

	/* Allocate memory for a copy of this data */
	sResultData.pvConstantData = DebugMemAlloc(uDataSizeInBytes);

	if(!sResultData.pvConstantData)
	{
		return IMG_FALSE;
	}

	sResultData.uConstantDataSize = uDataSizeInBytes;

	/* Set a non-initialised array size */
	sResultData.iActiveArraySize = -1;

	sResultData.eIdentifierUsage = GLSLIU_INTERNALRESULT;
	sResultData.uConstantAssociationSymbolID = 0;

	/* Copy the data over */
	memcpy(sResultData.pvConstantData, pbSrc, uDataSizeInBytes);

	/* Get a unique name for this result */
	ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));

	/* Add to symbol table */
	if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable,
					   acResultName,
					   &sResultData,
					   IMG_TRUE,
					   puSymbolTableID))
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantArraySpecifier: Failed to add result data %s to symbol table",
							 acResultName));
	}

	DebugMemFree(sResultData.pvConstantData);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTSemReduceConstantFieldSelection
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
 * Outputs      : puSymbolTableID
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Replaces a constant struct's field selection or a constant swizzle
                  with the actual element that is selected/the result of the swizzle.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantFieldSelection(GLSLTreeContext        *psGLSLTreeContext,
											GLSLNode               *psResultNode,
											IMG_UINT32             *puSymbolTableID)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode *psMemberSelectionNode, *psIdentifierNode;
	GLSLIdentifierData *psIdentifierData, sResultData;
	GLSLGenericData *psGenericData;
	IMG_BYTE *pbConstantData;
	IMG_CHAR  acResultName[200];


	if (psResultNode->eNodeType != GLSLNT_FIELD_SELECTION)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Not field selection\n" ));
		return IMG_FALSE;
	}

	if (psResultNode->uNumChildren != 2)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Incorrect number of children\n"));
		return IMG_FALSE;
	}

	/* Left node is identifier */
	psIdentifierNode = psResultNode->ppsChildren[0];

	/* Right node is member selection */
	psMemberSelectionNode = psResultNode->ppsChildren[1];

	/* Get structure instance data */
	psIdentifierData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable,
													 psIdentifierNode->uSymbolTableID,
													 GLSLSTDT_IDENTIFIER);
	if (!psIdentifierData)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Failed to retrieve IDENTIFIER data\n"));
		return IMG_FALSE;
	}

	/* Quick sanity check for constant data */
	if (!psIdentifierData->pvConstantData)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: No constant data\n"));
		return IMG_FALSE;
	}

	/* Get member selection data */
	psGenericData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
									   psMemberSelectionNode->uSymbolTableID);

	if (!psGenericData)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Failed to retrieve left nodes data\n"));
		return IMG_FALSE;
	}


	if (psGenericData->eSymbolTableDataType == GLSLSTDT_MEMBER_SELECTION)
	{
		GLSLMemberSelectionData *psMemberSelectionData;
		GLSLStructureDefinitionData *psStructureDefinitionData;
		GLSLStructureMember *psStructureMember;
		IMG_UINT32 uMemberSizeInBytes;

		/* Get member selection data */
		psMemberSelectionData = (GLSLMemberSelectionData *)psGenericData;
	
		/* Get structure definition data */
		psStructureDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																  psIdentifierData->sFullySpecifiedType.uStructDescSymbolTableID,
																  GLSLSTDT_STRUCTURE_DEFINITION);
		if (!psStructureDefinitionData)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Failed to retrieve STRUCTURE_DEFINITION data\n"));
			return IMG_FALSE;
		}


		/* Get information about the selected member */
		psStructureMember = &(psStructureDefinitionData->psMembers[psMemberSelectionData->uMemberOffset]);

		/* Get a pointer to this members data */
		pbConstantData = ((IMG_BYTE *)psIdentifierData->pvConstantData) + psStructureMember->uConstantDataOffSetInBytes;

		/* Get size of this member */
		uMemberSizeInBytes = ASTSemGetSizeInfo(psGLSLTreeContext, &(psStructureMember->sIdentifierData.sFullySpecifiedType), IMG_TRUE);

		/* Setup the result data */
		sResultData = psStructureMember->sIdentifierData;

		/* Override the type qualifier */
		sResultData.sFullySpecifiedType.eTypeQualifier = GLSLTQ_CONST;

		/* Allocate memory for a copy of this data */
		sResultData.pvConstantData     = DebugMemAlloc(uMemberSizeInBytes);

		if(!sResultData.pvConstantData)
		{
			return IMG_FALSE;
		}

		sResultData.uConstantDataSize  = uMemberSizeInBytes;

		/* Copy the data over */
		memcpy(sResultData.pvConstantData, pbConstantData, uMemberSizeInBytes);
	}
	else if (psGenericData->eSymbolTableDataType == GLSLSTDT_SWIZZLE)
	{
		IMG_UINT32 i, uDataSizeInBytes;

		GLSLSwizzleData *psSwizzleData = (GLSLSwizzleData *)psGenericData;

		/* Get dimension of matrix/vector */
		IMG_UINT32 uDimension = GLSLTypeSpecifierDimensionTable(psIdentifierData->sFullySpecifiedType.eTypeSpecifier);

		/* Setup the result data */
		sResultData = *psIdentifierData;

		/* 
		   Adjust the result type specifier based on the number of components specified 
		   - Relies on the enums for type specifier being declared in sequential size order
		*/
		sResultData.sFullySpecifiedType.eTypeSpecifier = (GLSLTypeSpecifier)(psIdentifierData->sFullySpecifiedType.eTypeSpecifier +
														 psSwizzleData->uNumComponents - uDimension);

		uDataSizeInBytes = ASTSemGetSizeInfo(psGLSLTreeContext, &sResultData.sFullySpecifiedType, IMG_FALSE);
	
		sResultData.pvConstantData = DebugMemAlloc(uDataSizeInBytes);

		if(!sResultData.pvConstantData)
		{
			return IMG_FALSE;
		}

		sResultData.uConstantDataSize = uDataSizeInBytes;

		if (GLSL_IS_BOOL(sResultData.sFullySpecifiedType.eTypeSpecifier))
		{
			IMG_BOOL *pbSrc  = (IMG_BOOL *)psIdentifierData->pvConstantData;
			IMG_BOOL *pbDest = (IMG_BOOL *)sResultData.pvConstantData;

			/* Copy the components */
			for (i = 0; i < psSwizzleData->uNumComponents; i++)
			{
				pbDest[i] = pbSrc[psSwizzleData->uComponentIndex[i]];
			}
	
		}
		else if (GLSL_IS_INT(sResultData.sFullySpecifiedType.eTypeSpecifier))
		{
			IMG_INT32 *piSrc  = (IMG_INT32 *)psIdentifierData->pvConstantData;
			IMG_INT32 *piDest = (IMG_INT32 *)sResultData.pvConstantData;

			/* Copy the components */
			for (i = 0; i < psSwizzleData->uNumComponents; i++)
			{
				piDest[i] = piSrc[psSwizzleData->uComponentIndex[i]];
			}
		}
		else if (GLSL_IS_FLOAT(sResultData.sFullySpecifiedType.eTypeSpecifier))
		{
			IMG_FLOAT *pfSrc  = (IMG_FLOAT *)psIdentifierData->pvConstantData;
			IMG_FLOAT *pfDest = (IMG_FLOAT *)sResultData.pvConstantData;

			/* Copy the components */
			for (i = 0; i < psSwizzleData->uNumComponents; i++)
			{
				pfDest[i] = pfSrc[psSwizzleData->uComponentIndex[i]];
			}
		}
		else
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Unrecognsied type specifier for swizzle\n"));
			return IMG_FALSE;
		}
	}
	else
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Unexpected data for right node\n"));
		return IMG_FALSE;

	}

	sResultData.eIdentifierUsage                        = GLSLIU_INTERNALRESULT;
	sResultData.uConstantAssociationSymbolID            = 0;

	/* Get a unique name for this result */
	ASTSemGetResultName(psGLSLTreeContext, acResultName, &sResultData.sFullySpecifiedType);

	/* Add to symbol table */
	if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable, 
					   acResultName, 
					   &sResultData, 
					   IMG_TRUE,
					   puSymbolTableID))
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReducedConstantFieldSelection: Failed to add result data %s to symbol table", 
							 acResultName));
	}
	
	DebugMemFree(sResultData.pvConstantData);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemReduceConstantMatrixMath
 *
 * Inputs       : psGLSLTreeContext
                  eSrc1TypeSpecifier - Type specifier of the left hand side matrix.
                  eSrc2TypeSpecifier - Type specifier of the right hand side matrix.
                  pfSrc1Data         - Pointer to the data of the left hand side matrix.
                  pfSrc2Data         - Pointer to the data of the right hand side matrix.
                  
 * Outputs      : peDestTypeSpecifier - Type specifier of the result.
                  pfDestData          - Pointer to the data of the result.
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Reduces a constant binary matrix-matrix operation. (Only works for products?)
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantMatrixMath(GLSLTreeContext   *psGLSLTreeContext,
										GLSLTypeSpecifier  eSrc1TypeSpecifier,
										GLSLTypeSpecifier  eSrc2TypeSpecifier,
										GLSLTypeSpecifier *peDestTypeSpecifier,
										const IMG_FLOAT         *pfSrc1Data,
										const IMG_FLOAT         *pfSrc2Data,
										IMG_FLOAT         *pfDestData)
{
	IMG_UINT32 i, j, k;
	IMG_UINT32 uLeftNumCols   = GLSLTypeSpecifierNumColumnsCMTable(eSrc1TypeSpecifier);
	IMG_UINT32 uLeftNumRows   = GLSLTypeSpecifierNumRowsCMTable   (eSrc1TypeSpecifier);
	IMG_UINT32 uRightNumRows  = GLSLTypeSpecifierNumRowsCMTable   (eSrc2TypeSpecifier);
	IMG_UINT32 uRightNumCols  = GLSLTypeSpecifierNumColumnsCMTable(eSrc2TypeSpecifier);
	
	/* If left operand is a vector, treated it as single row matrix - by default it is single column matrix */
	if(!GLSL_IS_MATRIX(eSrc1TypeSpecifier))
	{
		uLeftNumCols = uLeftNumRows;
		uLeftNumRows = 1;
	}

	PVR_UNREFERENCED_PARAMETER(psGLSLTreeContext);

	/* 
	   Rule for matrix multiplies is 
	   
	   M[c1][r1] * M[c2][r2] = M[c2][r1] 
	   
	   if (c1 == r2)
	*/

#ifdef MATCOMP
#error "MATCOMP already defined!"
#endif
#define MATCOMP(rowDim, row, col) (((col) * (rowDim)) + (row))

	for (i = 0; i < uLeftNumRows; i++)
	{
		for (j = 0; j < uRightNumCols; j++)
		{
			pfDestData[MATCOMP(uLeftNumRows, i, j)] = 0.0f;

			for (k = 0; k < uLeftNumCols; k++)
			{
				pfDestData[MATCOMP(uLeftNumRows, i, j)] += (pfSrc1Data[MATCOMP(uLeftNumRows,  i, k)] * 
															pfSrc2Data[MATCOMP(uRightNumRows, k, j)]); 
			}
		}
	}
#undef MATCOMP

	if (peDestTypeSpecifier)
	{
		/* Adjust types depending on arguments */
		if (GLSL_IS_FLOAT(eSrc1TypeSpecifier))
		{
			*peDestTypeSpecifier = (GLSLTypeSpecifier)(eSrc1TypeSpecifier - (uLeftNumCols - uRightNumCols));
		}
		else
		{
			*peDestTypeSpecifier = (GLSLTypeSpecifier)(eSrc2TypeSpecifier - (uRightNumRows - uLeftNumRows));
		}
	}

	return IMG_TRUE;
}




/******************************************************************************
 * Function Name: ASTSemReduceConstantMaths
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                   
 * Outputs      : eResultPrecision
                  puSymbolTableID
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Reduces a constant binary operation or a constant question operator.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantMaths(GLSLTreeContext        *psGLSLTreeContext,
								   GLSLNode               *psResultNode,
								   GLSLPrecisionQualifier  eResultPrecision,
								   IMG_UINT32             *puSymbolTableID)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i;
	GLSLIdentifierData *psChildIdentifierData[3];

	/* 
	   Some of the array sizes for the result data may not look big enough for
	   the work below. However semantic checking should have rejected any result
	   that would cause a result larger than that of the arrays provided to store
	   the size.

	   i.e. 

	   bool a[10], b[10];

	   bool b = a < b;

	   If the code below were to process this it would exceed the size allocated for abResult
	   however the above is invalid so it should never get here
	*/
	IMG_FLOAT   afResult[16];
	IMG_INT32   aiResult[4];
	IMG_BOOL    abResult[1];

	IMG_CHAR  acResultName[200];
	GLSLTypeSpecifier eResultTypeSpecifier;
	GLSLIdentifierData sResultData;
	IMG_BOOL bAllTypesMatch = IMG_TRUE;

#ifdef DEBUG
	if (psResultNode->uNumChildren > 3)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Cannot handle more than 3 children\n"));
		return IMG_FALSE;	
	}
#endif
	
	/* Get type information about all the children */
	for (i = 0; i < psResultNode->uNumChildren; i++)
	{
		GLSLTypeQualifier eTypeQualifier;

		psChildIdentifierData[i] = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																 psResultNode->ppsChildren[i]->uSymbolTableID,
																 GLSLSTDT_IDENTIFIER);

		if (!(psChildIdentifierData[i]))
		{
			return IMG_FALSE;
		}

		eTypeQualifier = psChildIdentifierData[i]->sFullySpecifiedType.eTypeQualifier;

		if (eTypeQualifier != GLSLTQ_CONST)
		{
			/* Uniforms can be assigned to in declarations in language version 1.2 and up */
			if (!(psGLSLTreeContext->uSupportedLanguageVersion >= 120 && eTypeQualifier == GLSLTQ_UNIFORM))
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Type qualifier is not constant\n"));
				return IMG_FALSE;
			}
		}
		if (psChildIdentifierData[i]->sFullySpecifiedType.eParameterQualifier != GLSLPQ_INVALID)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Params cannot be reduced even if they are constants\n"));
			return IMG_FALSE;
		}

		if(((psResultNode->eNodeType != GLSLNT_EQUAL) || (i != 0)) && !psChildIdentifierData[i]->pvConstantData)
		{
			LogProgramNodeError(psCPD->psErrorLog, psResultNode, "'%s' is not constant.\n",
								 GetSymbolName(psGLSLTreeContext->psSymbolTable, 
											   psResultNode->ppsChildren[i]->uSymbolTableID));
			return IMG_FALSE;
		}

		/* Do the halves of the expression have different types? */
		if (i > 0)
		{
			if (psChildIdentifierData[i]->sFullySpecifiedType.eTypeSpecifier != psChildIdentifierData[i - 1]->sFullySpecifiedType.eTypeSpecifier)
			{
				bAllTypesMatch = IMG_FALSE;
			}
		}
	}

	/* 
	   Assignments are a special case where a const can be written to at declaration time.
	   If there is already some data present for the left node then it must have been caused by some incorrect
	   code (i.e. it was not at declaration time) so we'll exit and let the semantic checking catch it.
	*/
	if (psResultNode->eNodeType == GLSLNT_EQUAL)
	{
		/* Error! */
		if (psChildIdentifierData[0]->pvConstantData)
		{
			return IMG_FALSE;
		}
		
		/* Allocate memory for the data */
		psChildIdentifierData[0]->pvConstantData = DebugMemAlloc(psChildIdentifierData[1]->uConstantDataSize);

		/* Copy the data across */
		memcpy(psChildIdentifierData[0]->pvConstantData,
			   psChildIdentifierData[1]->pvConstantData,
			   psChildIdentifierData[1]->uConstantDataSize);

		/* Copy the size */
		psChildIdentifierData[0]->uConstantDataSize = psChildIdentifierData[1]->uConstantDataSize;
		
		/* Symbol ID is that of which the data was assigned to */
		*puSymbolTableID = psResultNode->ppsChildren[0]->uSymbolTableID;
	} 
	/* There is only one case that has more than 2 children */
	else if (psResultNode->uNumChildren > 2)
	{
		IMG_BOOL *pbData = psChildIdentifierData[0]->pvConstantData;
	
		if (psResultNode->eNodeType != GLSLNT_QUESTION)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unrecognised node type for this number of children\n"));
			return IMG_FALSE;
		}

		/* Pick the child depending on the result of the test */
		if (pbData[0])
		{
			*puSymbolTableID = psResultNode->ppsChildren[1]->uSymbolTableID;
		}
		else
		{
			*puSymbolTableID = psResultNode->ppsChildren[2]->uSymbolTableID;
		}
	}
	else 
	{
		IMG_BOOL bLinearAlgebraMultiply = IMG_FALSE;

		/* Check for matrix-matrix and matrix-vector multiplies (but not matrix-float or vector-float multiplies) */
		if (psResultNode->eNodeType == GLSLNT_MULTIPLY)
		{
			if (GLSL_IS_MATRIX(psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier) ||
				GLSL_IS_MATRIX(psChildIdentifierData[1]->sFullySpecifiedType.eTypeSpecifier))
			{
				if (psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier != GLSLTS_FLOAT && 
					psChildIdentifierData[1]->sFullySpecifiedType.eTypeSpecifier != GLSLTS_FLOAT)
				{
					bLinearAlgebraMultiply = IMG_TRUE;
				}
			}
		}

		/* Do linear algebra separately */
		if (bLinearAlgebraMultiply)
		{
			ASTSemReduceConstantMatrixMath(psGLSLTreeContext,
										   psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier,
										   psChildIdentifierData[1]->sFullySpecifiedType.eTypeSpecifier,
										   &eResultTypeSpecifier,
										   psChildIdentifierData[0]->pvConstantData,
										   psChildIdentifierData[1]->pvConstantData,
										   afResult);
		}
		/* Do the halves of the expression have different types? */
		else if (!bAllTypesMatch)
		{
			IMG_UINT32 uScalarIndex = 0, uNonScalarIndex = 0, uNumComponents[2];

			/* Work out which half (if any) is the scalar part */
			uNumComponents[0] = GLSLTypeSpecifierNumElementsTable(psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier); 
			uNumComponents[1] = GLSLTypeSpecifierNumElementsTable(psChildIdentifierData[1]->sFullySpecifiedType.eTypeSpecifier); 

			if (uNumComponents[0] == 1)
			{
				uScalarIndex = 0;
				uNonScalarIndex = 1;
			}
			else if (uNumComponents[1] == 1)
			{
				uScalarIndex = 1;
				uNonScalarIndex = 0;
			}
			else
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Mismatched type specifiers\n"));
				return IMG_FALSE;
			}

			switch (psChildIdentifierData[uScalarIndex]->sFullySpecifiedType.eTypeSpecifier)
			{
				case GLSLTS_FLOAT:
				{
					IMG_FLOAT *pfData[2];
					IMG_FLOAT **ppfNonScalarData;

					pfData[0] = (IMG_FLOAT *)(psChildIdentifierData[0]->pvConstantData);
					pfData[1] = (IMG_FLOAT *)(psChildIdentifierData[1]->pvConstantData);

					/* Get a pointer to the data source that needs to be incremented each loop */
					ppfNonScalarData = &(pfData[uNonScalarIndex]);

					/* Set a default type */
					eResultTypeSpecifier = psChildIdentifierData[uNonScalarIndex]->sFullySpecifiedType.eTypeSpecifier;

					switch (psResultNode->eNodeType)
					{
						case GLSLNT_DIVIDE:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								afResult[i] =  *(pfData[0]) / *(pfData[1]);
								(*ppfNonScalarData)++;
							}
							break;
						}
						case GLSLNT_ADD:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								afResult[i] =  *(pfData[0]) + *(pfData[1]);
								(*ppfNonScalarData)++;
									
							}
							break;
						}
						case GLSLNT_SUBTRACT:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								afResult[i] =  *(pfData[0]) - *(pfData[1]);
								(*ppfNonScalarData)++;
									
							}
							break;
						}
						case GLSLNT_MULTIPLY:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								afResult[i] =  *(pfData[0]) * *(pfData[1]);
								(*ppfNonScalarData)++;
							}
							break;
						}
						default:
						{
							LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unsupported node type\n"));
							return IMG_FALSE;

						}
					}
					break;
				}
				case GLSLTS_INT:
				{
					IMG_INT32 *piData[2];
					IMG_INT32 **ppiNonScalarData;

					piData[0] = (IMG_INT32 *)(psChildIdentifierData[0]->pvConstantData);
					piData[1] = (IMG_INT32 *)(psChildIdentifierData[1]->pvConstantData);

					/* Get a pointer to the data source that needs to be incremented each loop */
					ppiNonScalarData = &(piData[uNonScalarIndex]);


					/* Set a default type */
					eResultTypeSpecifier = psChildIdentifierData[uNonScalarIndex]->sFullySpecifiedType.eTypeSpecifier;

					switch (psResultNode->eNodeType)
					{
						case GLSLNT_DIVIDE:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								aiResult[i] =  *(piData[0]) / *(piData[1]);
								(*ppiNonScalarData)++;
							}
							break;
						}
						case GLSLNT_ADD:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								aiResult[i] =  *(piData[0]) + *(piData[1]);
								(*ppiNonScalarData)++;
							}
							break;
						}
						case GLSLNT_SUBTRACT:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								aiResult[i] =  *(piData[0]) - *(piData[1]);
								(*ppiNonScalarData)++;
							}
							break;
						}
						case GLSLNT_MULTIPLY:
						{
							for (i = 0; i < uNumComponents[uNonScalarIndex]; i++)
							{
								aiResult[i] =  *(piData[0]) * *(piData[1]);
								(*ppiNonScalarData)++;
							}
							break;
						}
						default:
						{
							LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unsupported node type\n"));
							return IMG_FALSE;

						}
					}
					break;
				}
				default:
				{
					LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unsupported type specifier (%08X)\n",
										 psChildIdentifierData[uScalarIndex]->sFullySpecifiedType.eTypeSpecifier));
					return IMG_FALSE;

				}
			}
		}
		/* Type specifiers match so the maths is pretty straightforward */
		else 
		{
			/* Get the number of components to act upon */
			IMG_UINT32 uNumComponents = GLSLTypeSpecifierNumElementsTable(psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier);
			
			/* Account for array size if an array */
			if (psChildIdentifierData[0]->sFullySpecifiedType.iArraySize > 0)
			{
				uNumComponents *= psChildIdentifierData[0]->sFullySpecifiedType.iArraySize;
			}

			/* Set a default type */
			eResultTypeSpecifier = psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier;

			/* Set a default result for bool calcs */
			abResult[0] = IMG_TRUE;

			switch (psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier)
			{
				case GLSLTS_FLOAT:
				case GLSLTS_VEC2:
				case GLSLTS_VEC3:
				case GLSLTS_VEC4:
				case GLSLTS_MAT2X2:
				case GLSLTS_MAT3X3:
				case GLSLTS_MAT4X4:
				case GLSLTS_MAT2X3:
				case GLSLTS_MAT3X2:
				case GLSLTS_MAT3X4:
				case GLSLTS_MAT4X3:
				case GLSLTS_MAT2X4:
				case GLSLTS_MAT4X2:
				{
					IMG_FLOAT *pfData[2];
					
					for (i = 0; i < psResultNode->uNumChildren; i++)
					{
						pfData[i] = (IMG_FLOAT *)(psChildIdentifierData[i]->pvConstantData);
					}

					switch (psResultNode->eNodeType)
					{
						case GLSLNT_PRE_INC:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] + 1.0f;
							}
							break;
						case GLSLNT_PRE_DEC:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] - 1.0f;
							}
							break;
						case GLSLNT_NEGATE:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = -pfData[0][i];
							}
							break;
						case GLSLNT_POSITIVE:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i];
							}
							break;
						case GLSLNT_MULTIPLY:
							if (GLSL_IS_MATRIX(psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier) ||
								GLSL_IS_MATRIX(psChildIdentifierData[1]->sFullySpecifiedType.eTypeSpecifier))
							{
								LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Shouldn't multiply here\n"));
								break;
							}
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] * pfData[1][i];
							}
							break;
						case GLSLNT_DIVIDE:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] / pfData[1][i];
							}
							break;
						case GLSLNT_ADD:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] + pfData[1][i];
							}
							break;
						case GLSLNT_SUBTRACT:
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i] - pfData[1][i];
							}
							break;
						case GLSLNT_LESS_THAN:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] >= pfData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_GREATER_THAN:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;

							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] <= pfData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_LESS_THAN_EQUAL:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;

							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] > pfData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_GREATER_THAN_EQUAL:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;

							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] < pfData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_EQUAL_TO:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;

							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] != pfData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_NOTEQUAL_TO:
						{
							/* Modify the default result */
							abResult[0] = IMG_FALSE;

							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (pfData[0][i] != pfData[1][i])
								{
									abResult[0] = IMG_TRUE;
									break;
								}
							}
							break;
						}
						case GLSLNT_SUBEXPRESSION:
						{
							for (i = 0; i < uNumComponents; i++)
							{
								afResult[i] = pfData[0][i];
							}
							break;
						}
						default:
							return IMG_FALSE;
					}

					break;
				}
				case GLSLTS_INT:
				case GLSLTS_IVEC2:
				case GLSLTS_IVEC3:
				case GLSLTS_IVEC4:
				{
					IMG_INT32 *piData[2];

					for (i = 0; i < psResultNode->uNumChildren; i++)
					{
						piData[i] = (IMG_INT32 *)(psChildIdentifierData[i]->pvConstantData);
					}

					switch (psResultNode->eNodeType)
					{
						case GLSLNT_PRE_INC:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] + 1;
							}
							break;
						case GLSLNT_PRE_DEC:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] - 1;
							}
							break;
						case GLSLNT_NEGATE:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = -piData[0][i];
							}
							break;
						case GLSLNT_POSITIVE:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i];
							}
							break;
						case GLSLNT_MULTIPLY:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] * piData[1][i];
							}
							break;
						case GLSLNT_DIVIDE:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] / piData[1][i];
							}
							break;
						case GLSLNT_ADD:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] + piData[1][i];
							}
							break;
						case GLSLNT_SUBTRACT:
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i] - piData[1][i];
							}
							break;
						case GLSLNT_LESS_THAN:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] >= piData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_GREATER_THAN:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] <= piData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_LESS_THAN_EQUAL:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] > piData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_GREATER_THAN_EQUAL:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] < piData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_EQUAL_TO:
						{
							abResult[0] = IMG_TRUE;
						
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] != piData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_NOTEQUAL_TO:
						{
							abResult[0] = IMG_FALSE;

							eResultTypeSpecifier = GLSLTS_BOOL;

							for (i = 0; i < uNumComponents; i++)
							{
								if (piData[0][i] != piData[1][i])
								{
									abResult[0] = IMG_TRUE;
									break;
								}
							}
							break;
						}
						case GLSLNT_SUBEXPRESSION:
						{
							for (i = 0; i < uNumComponents; i++)
							{
								aiResult[i] = piData[0][i];
							}

							break;
						}
						default:
							return IMG_FALSE;
					}
					break;
				}
				case GLSLTS_BOOL:
				case GLSLTS_BVEC2:
				case GLSLTS_BVEC3:
				case GLSLTS_BVEC4:
				{
					IMG_BOOL *pbData[2];

					for (i = 0; i < psResultNode->uNumChildren; i++)
					{
						pbData[i] = (IMG_BOOL *)(psChildIdentifierData[i]->pvConstantData);
					}

					switch (psResultNode->eNodeType)
					{
						case GLSLNT_NOT:
							for (i = 0; i < uNumComponents; i++)
							{
								abResult[i] = (IMG_BOOL)(!pbData[0][i]);
							}
							break;
						case GLSLNT_EQUAL_TO:
						{
							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (pbData[0][i] != pbData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_NOTEQUAL_TO:
						{
							abResult[0] = IMG_FALSE;

							eResultTypeSpecifier = GLSLTS_BOOL;
						
							for (i = 0; i < uNumComponents; i++)
							{
								if (pbData[0][i] != pbData[1][i])
								{
									abResult[0] = IMG_TRUE;
									break;
								}
							}
							break;
						}
						case GLSLNT_LOGICAL_OR:
							for (i = 0; i < uNumComponents; i++)
							{
								abResult[i] = (IMG_BOOL)(pbData[0][i] || pbData[1][i]);
							}
							break;
						case GLSLNT_LOGICAL_XOR:
							for (i = 0; i < uNumComponents; i++)
							{
								abResult[i] = (IMG_BOOL)(pbData[0][i] != pbData[1][i]);
							}
							break;
						case GLSLNT_LOGICAL_AND:
							for (i = 0; i < uNumComponents; i++)
							{
								abResult[i] = (IMG_BOOL)(pbData[0][i] && pbData[1][i]);
							}
							break;
						case GLSLNT_SUBEXPRESSION:
						{
							for (i = 0; i < uNumComponents; i++)
							{
								abResult[i] = pbData[0][i];
							}
							break;
						}
						default:
							return IMG_FALSE;
					}
					break;
				}
				case GLSLTS_STRUCT:
				{
					IMG_BYTE *pbyData[2];

					for (i = 0; i < psResultNode->uNumChildren; i++)
					{
						pbyData[i] = (IMG_BYTE *)(psChildIdentifierData[i]->pvConstantData);
					}

					/* Set a default type */
					eResultTypeSpecifier = GLSLTS_BOOL;
		
					switch (psResultNode->eNodeType)
					{
				
						case GLSLNT_EQUAL_TO:
						{
							abResult[0] = IMG_TRUE;
						
							for (i = 0; i < psChildIdentifierData[0]->uConstantDataSize; i++)
							{
								if (pbyData[0][i] != pbyData[1][i])
								{
									abResult[0] = IMG_FALSE;
									break;
								}
							}
							break;
						}
						case GLSLNT_NOTEQUAL_TO:
						{
							abResult[0] = IMG_FALSE;

							for (i = 0; i < psChildIdentifierData[0]->uConstantDataSize; i++)
							{
								if (pbyData[0][i] != pbyData[1][i])
								{
									abResult[0] = IMG_TRUE;
									break;
								}
							}
							break;
						}
						default:
						{
							return IMG_FALSE;
						}
					}
					break;
				}
				default:
				{
					LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unrecognised type specifier (%08X)\n", 
										 psChildIdentifierData[0]->sFullySpecifiedType.eTypeSpecifier));
					return IMG_FALSE;
				}
			}
		}

		/* Special case floats and ints to try and get some reuse of existing data */
		switch (eResultTypeSpecifier)
		{
			case GLSLTS_FLOAT:
			{
				AddFloatConstant(psCPD, psGLSLTreeContext->psSymbolTable,
								 afResult[0],
								 eResultPrecision,
								 IMG_TRUE,
								 puSymbolTableID);
				return IMG_TRUE;
			}
			case GLSLTS_INT:
			{
				AddIntConstant(psCPD, psGLSLTreeContext->psSymbolTable,
							   aiResult[0],
							   eResultPrecision,
							   IMG_TRUE,
							   puSymbolTableID);
				return IMG_TRUE;
			}
			case GLSLTS_BOOL:
			{
				AddBoolConstant(psCPD, psGLSLTreeContext->psSymbolTable,
								abResult[0],
								eResultPrecision,
								IMG_TRUE,
								puSymbolTableID);
				return IMG_TRUE;
			}
			default:
				break;
		}

		/* Setup the result data */
		sResultData.eSymbolTableDataType                          = GLSLSTDT_IDENTIFIER;

		INIT_FULLY_SPECIFIED_TYPE(&(sResultData.sFullySpecifiedType));
		sResultData.sFullySpecifiedType.eTypeQualifier            = GLSLTQ_CONST;
		sResultData.sFullySpecifiedType.eTypeSpecifier            = eResultTypeSpecifier;
		sResultData.sFullySpecifiedType.ePrecisionQualifier       = eResultPrecision;

		sResultData.iActiveArraySize                              = -1;
		sResultData.eArrayStatus                                  = GLSLAS_NOT_ARRAY;
		sResultData.eLValueStatus                                 = GLSLLV_NOT_L_VALUE;
		sResultData.uConstantDataSize                             = ASTSemGetSizeInfo(psGLSLTreeContext, &sResultData.sFullySpecifiedType, IMG_TRUE);
		sResultData.eBuiltInVariableID                            = GLSLBV_NOT_BTIN;	
		sResultData.eIdentifierUsage                              = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_INTERNALRESULT);
		sResultData.uConstantAssociationSymbolID                  = 0;

		switch (sResultData.sFullySpecifiedType.eTypeSpecifier)
		{
			case GLSLTS_VEC2:
			case GLSLTS_VEC3:
			case GLSLTS_VEC4:
			case GLSLTS_MAT2X2:
			case GLSLTS_MAT2X3:
			case GLSLTS_MAT2X4:
			case GLSLTS_MAT3X2:
			case GLSLTS_MAT3X3:
			case GLSLTS_MAT3X4:
			case GLSLTS_MAT4X2:
			case GLSLTS_MAT4X3:
			case GLSLTS_MAT4X4:
			{
				sResultData.pvConstantData = (IMG_VOID *)afResult;

				/* Get a unique name for this result */
				ASTSemGetResultName(psGLSLTreeContext, acResultName, &sResultData.sFullySpecifiedType);

				break;
			}
			case GLSLTS_IVEC2:
			case GLSLTS_IVEC3:
			case GLSLTS_IVEC4:
			{
				sResultData.pvConstantData = (IMG_VOID *)aiResult;

				/* Get a unique name for this result */
				ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));

				break;
			}
			case GLSLTS_BVEC2:
			case GLSLTS_BVEC3:
			case GLSLTS_BVEC4:
			{
				sResultData.pvConstantData = (IMG_VOID *)abResult;

				/* Get a unique name for this result */
				ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));

				break;
			}
			default:
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Unrecognised result type specifier  %08X\n",
									 eResultTypeSpecifier));
				return IMG_FALSE;
			}
		}

		/* Add the data to the symbol table */
		if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable,
						   acResultName,
						   &sResultData,
						   IMG_TRUE,
						   puSymbolTableID))
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantMaths: Failed to add result data %s to symbol table",
								 acResultName));

			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemReduceConstantExpression
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                  eResultPrecision
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise. 
 * Description  : Reduce an arbitrary constant binary expression/question operator.
                  This function delegates into the previous.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantExpression(GLSLTreeContext        *psGLSLTreeContext,
										GLSLNode               *psResultNode,
										GLSLPrecisionQualifier  eResultPrecision)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i;
	IMG_UINT32 uSymbolTableID = 0;
	IMG_UINT32 uNumChildren = psResultNode->uNumChildren;
	IMG_BOOL   bRemoveAllChildren = IMG_FALSE;

	/* Field selection reductions are handled elsewhere */
	switch (psResultNode->eNodeType)
	{
		case GLSLNT_FIELD_SELECTION:
		{
			if (!ASTSemReduceConstantFieldSelection(psGLSLTreeContext, psResultNode, &uSymbolTableID))
			{
				return IMG_FALSE;
			}
			break;
		}
		case GLSLNT_ARRAY_SPECIFIER:
		{
			if (!ASTSemReduceConstantArraySpecifier(psGLSLTreeContext, psResultNode, &uSymbolTableID))
			{
				return IMG_FALSE;
			}
			break;
		}
		case GLSLNT_ARRAY_LENGTH:
		{
			GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																							psResultNode->ppsChildren[0]->uSymbolTableID);
			
			/* Get the array size */
			IMG_INT32 iArraySize = psIdentifierData->sFullySpecifiedType.iArraySize;

			/* Add the int constant */
			AddIntConstant(psCPD, psGLSLTreeContext->psSymbolTable,
						   iArraySize,
						   GLSLPRECQ_UNKNOWN,
						   IMG_TRUE,
						   &uSymbolTableID);

			bRemoveAllChildren = IMG_TRUE;

			break;
		}
		case GLSLNT_SUBEXPRESSION:
		case GLSLNT_EQUAL:
		case GLSLNT_PRE_INC:
		case GLSLNT_PRE_DEC:
		case GLSLNT_NEGATE:
		case GLSLNT_POSITIVE:
		case GLSLNT_MULTIPLY:		
		case GLSLNT_DIVIDE:
		case GLSLNT_ADD:
		case GLSLNT_SUBTRACT:
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
		case GLSLNT_LESS_THAN:
		case GLSLNT_GREATER_THAN:
		case GLSLNT_LESS_THAN_EQUAL:
		case GLSLNT_GREATER_THAN_EQUAL:
		case GLSLNT_NOT:
		case GLSLNT_LOGICAL_OR:
		case GLSLNT_LOGICAL_XOR:
		case GLSLNT_LOGICAL_AND:
		case GLSLNT_QUESTION:
		{
			if (!ASTSemReduceConstantMaths(psGLSLTreeContext, psResultNode, eResultPrecision, &uSymbolTableID))
			{
				return IMG_FALSE;
			}

			break;
		}
		case GLSLNT_FUNCTION_CALL:
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReduceConstantExpression: Cannot resolve constant function calls\n"));
			return IMG_FALSE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}

	/* 
	   Remove the node children, cannot use value of num children inside
	   node as it decremented by the function
	*/
	for (i = 0; i < uNumChildren; i++)
	{
		/* 0 is correct, should NOT be i */
		ASTSemRemoveChildNode(psGLSLTreeContext, psResultNode, 0, bRemoveAllChildren);
	}

	/* Set node type to identifier */
	psResultNode->eNodeType      = GLSLNT_IDENTIFIER;
	psResultNode->uSymbolTableID = uSymbolTableID;

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTSemReportWrongOperandType
 *
 * Inputs       : psResultNode
                  pszDescription
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : UTILITY: Logs an error of type "wrong operand".
 *****************************************************************************/
static IMG_VOID ASTSemReportWrongOperandType(ErrorLog *psErrorLog,
											 GLSLNode        *psResultNode,
											IMG_CHAR            *pszDescription)
{
	LogProgramNodeError(psErrorLog, psResultNode,
						"'%s' :  Wrong operand type. No operation '%s' exists that takes an operand of type '%s' (and there is no acceptable conversion)\n",
						psResultNode->psToken->pvData,
						psResultNode->psToken->pvData,
						pszDescription);
}

/******************************************************************************
 * Function Name: ASTSemReportWrongOperandTypes
 *
 * Inputs       : psResultNode
                  pszDescription0
                  pszDescription1
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : UTILITY: Logs an error of type "wrong operands".
 *****************************************************************************/
static IMG_VOID ASTSemReportWrongOperandTypes(ErrorLog *psErrorLog,
											 GLSLNode *psResultNode,
									   IMG_CHAR *pszDescription0,
									   IMG_CHAR *pszDescription1)
{

	LogProgramNodeError(psErrorLog, psResultNode,
						"'%s' :  Wrong operand types. No operation '%s' exists that takes a left-hand operand of type '%s' and a right operand of type '%s' (and there is no acceptable conversion)\n",
						psResultNode->psToken->pvData,
						psResultNode->psToken->pvData,
						pszDescription0,
						pszDescription1);
}

/******************************************************************************
 * Function Name: ASTSemReportMismatchedArraySizes
 *
 * Inputs       : psResultNode
                  pszDescription0
                  pszDescription1
                  iArraySize1
                  iArraySize2
 * Outputs      : -
 * Returns      : 
 * Description  : UTILITY: Logs an error of type "array sizes mismatch".
 *****************************************************************************/
static IMG_VOID ASTSemReportMismatchedArraySizes(ErrorLog *psErrorLog,
												 GLSLNode   *psResultNode,
										  IMG_CHAR   *pszDescription0,
										  IMG_CHAR   *pszDescription1,
										  IMG_INT32   iArraySize1,
										  IMG_INT32   iArraySize2)
{
	if (iArraySize1 == 0 || iArraySize2 == 0)
	{
		ASTSemReportWrongOperandTypes(psErrorLog, psResultNode, pszDescription0, pszDescription1);
	}
	else if (iArraySize1 == -1 || iArraySize2 == -1)
	{
		LogProgramNodeError(psErrorLog, psResultNode,
							"'%s' :  Wrong operand types. Array sizes must be explicity declared before use\n",
							psResultNode->psToken->pvData);
	}
	else
	{
		LogProgramNodeError(psErrorLog, psResultNode,
							"'%s' :  Wrong operand types. Array size mismatch (%d vs %d)\n",
							psResultNode->psToken->pvData,
							iArraySize1,
							iArraySize2);
	}
}

/******************************************************************************
 * Function Name: ASTSemReportLValueError
 *
 * Inputs       : psResultNode
                  eLValueStatus
                  eTypeQualifier
                  pszString
 * Outputs      : -
 * Returns      : 
 * Description  : UTILITY: Logs an error of type "L-value".
 *****************************************************************************/
static IMG_VOID ASTSemReportLValueError(GLSLCompilerPrivateData *psCPD,
										GLSLNode         *psResultNode,
								 GLSLLValueStatus  eLValueStatus,
								 GLSLTypeQualifier eTypeQualifier,
								 IMG_CHAR         *pszString)
{
	if (eLValueStatus == GLSLLV_NOT_L_VALUE)
	{
		if (eTypeQualifier == GLSLTQ_CONST || 
			eTypeQualifier == GLSLTQ_UNIFORM ||
			eTypeQualifier == GLSLTQ_VERTEX_IN)
		{
			LogProgramNodeError(psCPD->psErrorLog, psResultNode,
								"'%s' :  l-value required (can't modify a %s)\n",
								pszString,
								GLSLTypeQualifierFullDescTable(eTypeQualifier));
		}
		else
		{
			LogProgramNodeError(psCPD->psErrorLog, psResultNode,
								"'%s' :  l-value required\n", 
								pszString);
		}
	}
	else if (eLValueStatus == GLSLLV_NOT_L_VALUE_DUP_SWIZZLE)
	{
		LogProgramNodeError(psCPD->psErrorLog, psResultNode,
							"'%s' : l-value of swizzle cannot have duplicate components\n",
							pszString);
	}
	else
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemReportLValueError: Incorrect lvalue status %08X\n", eLValueStatus));
	}
}


/******************************************************************************
 * Function Name: ASTSemCheckTypeSpecifiersMatch
 *
 * Inputs       : psType1
                  psType2
 * Outputs      : -
 * Returns      : IMG_TRUE if the two types are the same. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTSemCheckTypeSpecifiersMatch(const GLSLFullySpecifiedType* psType1,
													const GLSLFullySpecifiedType* psType2)
{
	if (psType1->eTypeSpecifier == psType2->eTypeSpecifier)
	{
		if (psType1->eTypeSpecifier == GLSLTS_STRUCT)
		{
			if (psType1->uStructDescSymbolTableID != psType2->uStructDescSymbolTableID)
			{
				return IMG_FALSE;
			}
		}

		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/******************************************************************************
 * Function Name: ASTSemReduceConstantBuiltInFunction
 *
 * Inputs       : psGLSLTreeContext
                  psFunctionCallNode
                  psFunctionDefinitionData
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Reduce a call to a builtin function that only takes constant arguments.
                  Delegates on EmulateBuiltInFunction to compute the result.
 *****************************************************************************/
static IMG_BOOL ASTSemReduceConstantBuiltInFunction(GLSLTreeContext            *psGLSLTreeContext,
											 GLSLNode                   *psFunctionCallNode,
											 GLSLFunctionDefinitionData *psFunctionDefinitionData)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i;

	GLSLIdentifierData sResultData;

	IMG_CHAR acResultName[256];

	IMG_UINT32 uNumChildren = psFunctionCallNode->uNumChildren;

	const IMG_VOID     *pvParamData[3];
	GLSLTypeSpecifier  aeParamTypeSpecifiers[3];
	GLSLTypeSpecifier  eReturnedTypeSpecifier;

	/* Get the type specifier of the result */
	GLSLTypeSpecifier eResultTypeSpecifier = psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier;
	
	/* get the size of the result */
	IMG_UINT32 uResultSizeInBytes = GLSLTypeSpecifierSizeTable(eResultTypeSpecifier);

	if (psFunctionCallNode->uNumChildren > 3)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: Too many children\n"));
		return IMG_FALSE;
	}

	for (i = 0; i < 3; i++)
	{
		pvParamData[i]           = IMG_NULL;
		aeParamTypeSpecifiers[i] = GLSLTS_INVALID; 
	}


	/* Set up the initial data */
	sResultData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	sResultData.uConstantDataSize                            = uResultSizeInBytes;
	sResultData.pvConstantData                               = DebugMemAlloc(uResultSizeInBytes);

	INIT_FULLY_SPECIFIED_TYPE(&(sResultData.sFullySpecifiedType));
	sResultData.sFullySpecifiedType.eTypeSpecifier           = eResultTypeSpecifier;
	sResultData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_CONST;
	sResultData.sFullySpecifiedType.uStructDescSymbolTableID = psFunctionDefinitionData->sReturnFullySpecifiedType.uStructDescSymbolTableID;

	sResultData.iActiveArraySize                             = -1;
	sResultData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
	sResultData.eLValueStatus                                = GLSLLV_NOT_L_VALUE;
	sResultData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;	
	sResultData.eIdentifierUsage                             = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_INTERNALRESULT);
	sResultData.uConstantAssociationSymbolID                 = 0;
	
	/* Check alloc suceeded */
	if (!sResultData.pvConstantData)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: Failed to alloc memory for constant data\n"));
		return IMG_FALSE;
	}

	for (i = 0; i < psFunctionCallNode->uNumChildren; i++)
	{
		/* Fetch the data for this parameter */
		GLSLIdentifierData *psParamData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																		psFunctionCallNode->ppsChildren[i]->uSymbolTableID,
																		GLSLSTDT_IDENTIFIER);


		/* Check was valid data type */
		if (psParamData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: Param was not IDENTIFIER was %08X can't extract constant data\n",
								 psParamData->eSymbolTableDataType));

			return IMG_FALSE;
		}
		
		/* Check for valid data - DO NOT REMOVE FOR NON DEBUG BUILDS*/
		if (!psParamData->pvConstantData)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: No constant data to for param %d\n", i));

			DebugMemFree(sResultData.pvConstantData);

			return IMG_FALSE;
		}

		pvParamData[i]           = psParamData->pvConstantData;
		aeParamTypeSpecifiers[i] = psParamData->sFullySpecifiedType.eTypeSpecifier;
	}

	/* Emulate this function being called */
	EmulateBuiltInFunction(psCPD, psFunctionDefinitionData->eBuiltInFunctionID,
						   aeParamTypeSpecifiers,
						   pvParamData,
						   sResultData.pvConstantData,
						   &eReturnedTypeSpecifier);


#ifdef DEBUG
	/* Quick sanity check - cast avoids warnings on compact memory model builds */
	if (eReturnedTypeSpecifier != (GLSLTypeSpecifier)sResultData.sFullySpecifiedType.eTypeSpecifier)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: Return type did not match\n"));
	}
#endif

	/* Get a unique name for this result */
	ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));

	/* Add the data to the symbol table */
	if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable,
					   acResultName,
					   &sResultData,
					   IMG_FALSE,
					   &(psFunctionCallNode->uSymbolTableID)))
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemReduceConstantBuiltInFunction: Failed to add result %s to table\n", acResultName));
	}

	/* Free the constant data */
	DebugMemFree(sResultData.pvConstantData);

	/* 
	   Remove the node children, cannot use value of num children inside
	   node as it decremented by the function
	*/
	for (i = 0; i < uNumChildren; i++)
	{
		/* 0 is correct, should NOT be i */
		ASTSemRemoveChildNode(psGLSLTreeContext, psFunctionCallNode, 0, IMG_FALSE);
	}
	
	/* Change the type of the node */
	psFunctionCallNode->eNodeType = GLSLNT_IDENTIFIER;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemCheckFunctionCallParameters
 *
 * Inputs       : psGLSLTreeContext
                  psFunctionCallNode
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Checks that the function has the right type and number of arguments.
                  It also determines the precision of the call based on several factors.
                  If all parameters are constant and the function/ctor is builtin, then it reduces the call.
 *****************************************************************************/
static IMG_BOOL ASTSemCheckFunctionCallParameters(GLSLTreeContext *psGLSLTreeContext, 
												  GLSLNode *psFunctionCallNode)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i, uTotalSizeOfParameters = 0, uConstructorSize = 0, uNumComponents = 0;
	IMG_BOOL bUnsizedArrayConstructor = IMG_FALSE, bConstructor;
	GLSLFunctionCallData *psFunctionCallData;
	GLSLFunctionDefinitionData *psFunctionDefinitionData;
	IMG_UINT32 uNumConstantParams = 0;
	
	IMG_BOOL bExpandSourceParameter        = IMG_FALSE;
	IMG_BOOL bConstructingMatrixFromMatrix = IMG_FALSE;
	const IMG_CHAR *pszConstructorString = "constructor";
	const IMG_CHAR *pszFunctionName;
	IMG_BOOL bError = IMG_FALSE;
	GLSLPrecisionQualifier eMaximumPrecQual = GLSLPRECQ_UNKNOWN;
	GLSLPrecisionQualifier eSamplerPrecQual = GLSLPRECQ_UNKNOWN;
	GLSLFullySpecifiedType sSrcFullySpecifiedType, *psSrcFullySpecifiedType;

	psSrcFullySpecifiedType = &sSrcFullySpecifiedType;


	if (psFunctionCallNode->eNodeType != GLSLNT_FUNCTION_CALL)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Expected FUNCTION_CALL node\n"));
		return IMG_FALSE;
	}

	psFunctionCallData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable,
													   psFunctionCallNode->uSymbolTableID,
													   GLSLSTDT_FUNCTION_CALL);
	

	if (!psFunctionCallData)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Failed to retrieve function call data\n"));
		return IMG_FALSE;
	}

	/* 
	   Types and number of parameters will already have been checked by this point (otherwise the hashing of the
	   function name would have produced an incorrect name). Here we need to check that the parameters with a 
	   qualifier of OUT or INOUT can actually be written to.
	*/

	psFunctionDefinitionData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
															 psFunctionCallData->uFunctionDefinitionSymbolID,
															 GLSLSTDT_FUNCTION_DEFINITION);
	
	if (!psFunctionDefinitionData)
	{
		LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Failed to retrieve function definition data\n"));
		return IMG_FALSE;
	}

	/* Is it a constructor */
	bConstructor = (IMG_BOOL)(psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR ||
					psFunctionDefinitionData->eFunctionType == GLSLFT_USERDEFINED_CONSTRUCTOR);

	/* Get function name */
	pszFunctionName = psFunctionDefinitionData->pszOriginalFunctionName;

	/* Work out size of constructor */
	if (bConstructor)
	{
		if (psFunctionCallData->eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
		{
			bUnsizedArrayConstructor = IMG_TRUE;
		}
		else
		{
			uConstructorSize        = ASTSemGetSizeInfo(psGLSLTreeContext, &(psFunctionCallData->sFullySpecifiedType), IMG_TRUE);
		}

		pszFunctionName = pszConstructorString;

		if (psFunctionCallData->eArrayStatus == GLSLAS_NOT_ARRAY &&
			psFunctionDefinitionData->eFunctionType == GLSLFT_USERDEFINED_CONSTRUCTOR)
		{
			if (psFunctionCallNode->uNumChildren != psFunctionDefinitionData->uNumParameters)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'constructor' : Number of constructor parameters does not match the number of structure fields (%d vs %d)\n",
									psFunctionDefinitionData->uNumParameters, psFunctionCallNode->uNumChildren);
				return IMG_FALSE;
			}
		}
	}

	/* Look for the highest precision */
	for (i = 0; i < psFunctionCallNode->uNumChildren; i++)
	{
		/* Get the symbol info */
		if (!GetSymbolInfo(psCPD, psGLSLTreeContext->psSymbolTable,
						   psFunctionCallNode->ppsChildren[i]->uSymbolTableID,
						   psGLSLTreeContext->eProgramType,
						   psSrcFullySpecifiedType,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL,
						   IMG_NULL))
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Failed to get symbol data\n"));
			return IMG_FALSE;
		}
		
		if (GLSL_IS_SAMPLER(psSrcFullySpecifiedType->eTypeSpecifier))
		{
			/* Store sampler precisions independently */
			eSamplerPrecQual = MAX((GLSLPrecisionQualifier)psSrcFullySpecifiedType->ePrecisionQualifier, eSamplerPrecQual);
		}
		else
		{
			/* Store highest precision of the parameters - cast avoids warnings on compact memory model builds */
			eMaximumPrecQual = MAX((GLSLPrecisionQualifier)psSrcFullySpecifiedType->ePrecisionQualifier, eMaximumPrecQual);
		}
	}

	/* Check for special cases when there are no parameters */
	if (!psFunctionCallNode->uNumChildren && (psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN))
	{
		/* Needs to be the same precision as gl_Vertex and gl_ModelViewMatrix */
		if (psFunctionDefinitionData->eBuiltInFunctionID == GLSLBFID_FTRANSFORM)
		{
			/* Really should look up the precision of the implied params here */
			eMaximumPrecQual = GLSLPRECQ_HIGH;
		}
	}

	/* 
	   Constructors and built in functions don't have any default precisions associated with them,
	   therefore if none of the parameters supplied have a precision (i.e. they are all literals)
	   we'll end up generating a function call with no precision for its parameters or return type
	   (which is invalid if they are numbers). In this case pick the low precision for constructors 
	   (which will automatically get converted to the correct precision when the value is used) 
	   or the default precision for built in functions.
	*/
	if (eMaximumPrecQual == GLSLPRECQ_UNKNOWN)
	{
		if (psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR)
		{
			eMaximumPrecQual = GLSLPRECQ_LOW;
		}
		else if (psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN)
		{
			if (GLSL_IS_INT(psFunctionCallData->sFullySpecifiedType.eTypeSpecifier))
			{
				eMaximumPrecQual = psGLSLTreeContext->eDefaultIntPrecision;
			}
			else
			{
				eMaximumPrecQual = psGLSLTreeContext->eDefaultFloatPrecision;
			}
		}

		if(eMaximumPrecQual == GLSLPRECQ_UNKNOWN)
		{
			/* If the precision is still unknown, then try to use the one from the "next consuming operation", 
			   which would be the return value identifier. */
			eMaximumPrecQual = psGLSLTreeContext->eNextConsumingOperationPrecision;
		}
	}

	/* 
	   For constructors and built in functions make the returned precision match that
	   of the parameter with the highest precision. 
	   For a texture function the returned precision is that of the sampler referenced.
	   User defined functions will have a precision defined for their returned data.
	*/
	if ((psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR) ||
		(psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN))
	{
		/* Was it a texture sampler? */
		if (GLSLBuiltInFunctionTextureSampler(psFunctionDefinitionData->eBuiltInFunctionID))
		{
			if(eSamplerPrecQual == GLSLPRECQ_UNKNOWN)
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: eSamplerPrecQual == GLSLPRECQ_UNKNOWN\n"));
				return IMG_FALSE;
			}
			psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier = eSamplerPrecQual;
		}
		else if (GLSL_IS_NUMBER(psFunctionCallData->sFullySpecifiedType.eTypeSpecifier))
		{
			if(eMaximumPrecQual == GLSLPRECQ_UNKNOWN)
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: eMaximumPrecQual == GLSLPRECQ_UNKNOWN\n"));
				return IMG_FALSE;
			}
			psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier = eMaximumPrecQual;
		}
		else 
		{
			psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier = GLSLPRECQ_UNKNOWN;
		}
	}

	/* Loop through all parameters looking for errors */
	for (i = 0; i < psFunctionCallNode->uNumChildren ; i++)
	{
		/* Get the data */
		GLSLIdentifierData *psChildData = ASTSemGetIdentifierData(psGLSLTreeContext, psFunctionCallNode->ppsChildren[i], IMG_NULL);

		if (!psChildData)
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Failed to retrieve child (%d) data\n", i));
			return IMG_FALSE;
		}

		/* Take a copy (as we might modify it so can't use pointer) */
		sSrcFullySpecifiedType = psChildData->sFullySpecifiedType;		
		
		if (psSrcFullySpecifiedType->eTypeQualifier == GLSLTQ_CONST && 
			psSrcFullySpecifiedType->eParameterQualifier == GLSLPQ_INVALID &&	/* We cannot reduce a function call to a constant value if one of the parameters is itself a parameter - its value isnt known until call time. */
			!(psChildData->eIdentifierUsage & GLSLIU_ERROR_INITIALISING))
		{
			uNumConstantParams++;
		}

		if ((psFunctionDefinitionData->eFunctionType == GLSLFT_USER) ||
			(psFunctionDefinitionData->eFunctionType == GLSLFT_USERDEFINED_CONSTRUCTOR) ||
			(psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN))
		{
			GLSLFullySpecifiedType *psDestFullySpecifiedType;
			GLSLParameterQualifier eParameterQualifier;
		
			if(psFunctionCallData->eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED ||
				psFunctionCallData->eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
			{
				psDestFullySpecifiedType = &psFunctionDefinitionData->sReturnFullySpecifiedType;
				eParameterQualifier = GLSLPQ_IN;
			}
			else
			{
				if (psFunctionDefinitionData->bPrototype)
				{
					psDestFullySpecifiedType = &psFunctionDefinitionData->psFullySpecifiedTypes[i];
				}
				else
				{
					
					GLSLIdentifierData *psParameterData = GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																						psFunctionDefinitionData->puParameterSymbolTableIDs[i],
																						GLSLSTDT_IDENTIFIER);	
					if (!psParameterData)
					{
						LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Failed to retrieve parameter (%d) data\n", i));
						return IMG_FALSE;
					}
					psDestFullySpecifiedType = &psParameterData->sFullySpecifiedType;
				}
				eParameterQualifier = psDestFullySpecifiedType->eParameterQualifier;
			}

			/* Check values to be passed out have a valid writable destination */
			if (psDestFullySpecifiedType->eParameterQualifier == GLSLPQ_OUT ||
				psDestFullySpecifiedType->eParameterQualifier == GLSLPQ_INOUT)
			{
				GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																								psFunctionCallNode->ppsChildren[i]->uSymbolTableID);
				
				if (psChildData->eLValueStatus != GLSLLV_L_VALUE)
				{
					LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
										"'Error' : Constant value cannot be passed for 'out' or 'inout' parameters.\n");
				}

				/* If this identifier had an associated constant symbol ID it's now not valid */
				if (psIdentifierData->uConstantAssociationSymbolID)
				{
					
#ifdef DUMP_LOGFILES
					DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "%08X -- Removing Association with data from symbol ID %08X\n", 
								   psFunctionCallNode->ppsChildren[i]->uSymbolTableID, psIdentifierData->uConstantAssociationSymbolID); 
#endif
					psIdentifierData->uConstantAssociationSymbolID = 0;
					
				}
			}

			/* Check types match */
			if (psDestFullySpecifiedType->eTypeSpecifier != psSrcFullySpecifiedType->eTypeSpecifier)
			{
				/* Are we supporting int->float conversion */
				if (psGLSLTreeContext->uSupportedLanguageVersion >= 120)
				{
					/* Was there an int src and a float dest */
					IMG_BOOL bIntSrcFloatDest = (IMG_BOOL)(GLSL_IS_INT(psSrcFullySpecifiedType->eTypeSpecifier) &&
												 GLSL_IS_FLOAT(psDestFullySpecifiedType->eTypeSpecifier));

					/* Did the dimensions match */
					IMG_BOOL bMatchingDimensions = (IMG_BOOL)(GLSLTypeSpecifierDimensionTable(psDestFullySpecifiedType->eTypeSpecifier) ==
													GLSLTypeSpecifierDimensionTable(psSrcFullySpecifiedType->eTypeSpecifier));

					/* Valid int -> float conversion? */
					if (!(bIntSrcFloatDest && bMatchingDimensions))
					{
						bError = IMG_TRUE;	
					}
				}
				else
				{
					bError = IMG_TRUE;	
				}

				if (bError)
				{
					LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
										"'%s' : Cannot convert parameter %d from '%s' to '%s'\n",
										pszFunctionName,
										i+1,
										GLSLTypeSpecifierFullDescTable(psSrcFullySpecifiedType->eTypeSpecifier),
										GLSLTypeSpecifierFullDescTable(psDestFullySpecifiedType->eTypeSpecifier));
				}
			}
			else if (psDestFullySpecifiedType->eTypeSpecifier == GLSLTS_STRUCT)
			{
				if (psDestFullySpecifiedType->uStructDescSymbolTableID != psSrcFullySpecifiedType->uStructDescSymbolTableID)
				{
					LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
										"'%s' : Cannot convert parameter %d from structure '%s' to structure '%s'\n",
										pszFunctionName,
										i+1,
										GetSymbolName(psGLSLTreeContext->psSymbolTable, psSrcFullySpecifiedType->uStructDescSymbolTableID),
										GetSymbolName(psGLSLTreeContext->psSymbolTable, psDestFullySpecifiedType->uStructDescSymbolTableID));
					bError = IMG_TRUE;
				}				
			}
			else if (psDestFullySpecifiedType->iArraySize != psSrcFullySpecifiedType->iArraySize)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'%s' : Parameter array size does not match function definition (%d vs %d)\n",
									pszFunctionName,
									psDestFullySpecifiedType->iArraySize,
									psSrcFullySpecifiedType->iArraySize);
				bError = IMG_TRUE;
			}

			/* Any int -> float and/or precision conversions required? */
			if (GLSL_IS_NUMBER(psSrcFullySpecifiedType->eTypeSpecifier))
			{
				GLSLPrecisionQualifier eDestPrecQual = psDestFullySpecifiedType->ePrecisionQualifier;
				GLSLPrecisionQualifier eSrcPrecQual  = psSrcFullySpecifiedType->ePrecisionQualifier;

				IMG_BOOL bTypeConversionRequired      = IMG_FALSE;
				IMG_BOOL bPrecisionConversionRequired = IMG_FALSE;

				/* 
				   Convert to dest precision if it exists (will do for all user defined functions)
				   If not, convert precision so that it matches the parameter with the highest precision
				*/
				if (eDestPrecQual == GLSLPRECQ_UNKNOWN)
				{
					eDestPrecQual = eMaximumPrecQual;
				}
				
				/* TYpe conversion required? */
				if (psSrcFullySpecifiedType->eTypeSpecifier != psDestFullySpecifiedType->eTypeSpecifier)
				{
					bTypeConversionRequired = IMG_TRUE;
				}

				/* Precision conversion required? */
				if ((eSrcPrecQual != eDestPrecQual) && (eDestPrecQual != GLSLPRECQ_UNKNOWN))
				{
					bPrecisionConversionRequired = IMG_TRUE;
				}

				if (bTypeConversionRequired || bPrecisionConversionRequired)
				{
					/* Insert a constructor to convert it to the correct type */
					ASTSemInsertConstructor(psGLSLTreeContext, 
											psFunctionCallNode, 
											i,
											eDestPrecQual,
											psDestFullySpecifiedType->eTypeSpecifier,
											IMG_FALSE);
					
					/* Update the src */
					psSrcFullySpecifiedType->eTypeSpecifier      = psDestFullySpecifiedType->eTypeSpecifier;
					psSrcFullySpecifiedType->ePrecisionQualifier = eDestPrecQual;
				}
			}

			/* Update the usage of the identifiers used as parameters */
			ASTUpdateParamIdentifierUsage(psGLSLTreeContext,
										  psFunctionCallNode->ppsChildren[i],
										  eParameterQualifier);
		}
		else if (psFunctionDefinitionData->eFunctionType == GLSLFT_CONSTRUCTOR)
		{
			/* Not allowed to use sampler inside a constructor */
			if (GLSL_IS_SAMPLER(psSrcFullySpecifiedType->eTypeSpecifier))
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'constructor' : cannot convert a sampler\n");
				return IMG_FALSE;
			}

			/* Not allowed to use struct inside a constructor */
			if (psSrcFullySpecifiedType->eTypeSpecifier == GLSLTS_STRUCT)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'constructor' : cannot convert a structure\n");
				return IMG_FALSE;
			}

			/* Are we trying to construct a matrix from a matrix? */
			if (GLSL_IS_MATRIX(psSrcFullySpecifiedType->eTypeSpecifier) &&
				GLSL_IS_MATRIX(psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier) &&
				(psFunctionCallData->sFullySpecifiedType.iArraySize == 0))
			{
				bConstructingMatrixFromMatrix = IMG_TRUE;

#if !defined(GLSL_ES)
				ASTCheckFeatureVersion(psGLSLTreeContext, psFunctionCallNode->psParseTreeEntry, 120, "constructing matrices from other matrices\n", IMG_NULL);
#endif

				/* Only allowed a single matrix should be supplied when constructing matrix from matrix */
				if(psFunctionCallNode->uNumChildren > 1)
				{
					LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
										"'constructor' : only a single matrix should be supplied when constructing matrix from matrix\n");
					return IMG_FALSE;
				}	
			}
	
			if (GLSL_IS_NUMBER(psSrcFullySpecifiedType->eTypeSpecifier) &&
				(psSrcFullySpecifiedType->ePrecisionQualifier == GLSLPRECQ_UNKNOWN))
			{

				if (eMaximumPrecQual != GLSLPRECQ_UNKNOWN)
				{
					/* Insert a constructor to convert it to the correct precision */
					ASTSemInsertConstructor(psGLSLTreeContext, 
											psFunctionCallNode, 
											i,
											eMaximumPrecQual,
											psSrcFullySpecifiedType->eTypeSpecifier,
											IMG_FALSE);
				}
			}

			/* Update the usage of the identifiers used as parameters */
			ASTUpdateParamIdentifierUsage(psGLSLTreeContext,
										  psFunctionCallNode->ppsChildren[i],
										  GLSLPQ_IN);
		}
		else
		{
			LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTSemCheckFunctionCallParameters: Unrecognised function type (%08X)\n",
								 psFunctionDefinitionData->eFunctionType));
			return IMG_FALSE;
		}

		if(bConstructor && !bUnsizedArrayConstructor)
		{
			/* Check that we don't have too much data to construct with */
			if (uTotalSizeOfParameters >= uConstructorSize)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'constructor' : too many arguments\n");
				return IMG_FALSE;
			}
		}

		uTotalSizeOfParameters += ASTSemGetSizeInfo(psGLSLTreeContext, psSrcFullySpecifiedType, IMG_TRUE);

		uNumComponents += GLSLTypeSpecifierNumElementsTable(psSrcFullySpecifiedType->eTypeSpecifier);

		/* Account for array size if an array */
		if (psSrcFullySpecifiedType->iArraySize)
		{
			uNumComponents *= psSrcFullySpecifiedType->iArraySize;
		}
	}/* for (i = 0; i < psFunctionCallNode->uNumChildren ; i++) */

	/* Check that we don't have too little data to construct with */
	if (bConstructor)
	{
		/*
		   If we were dealing with an unsized constructor give it a size
		   based of the amount of data present
		
		   i.e. float[](0.0, 1.0, 3.0);
		*/
		if (bUnsizedArrayConstructor)
		{
			if (!uTotalSizeOfParameters)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode, "'constructor' : no arguments supplied for array\n");
				return IMG_FALSE;
			}

			uConstructorSize = uTotalSizeOfParameters;

			/* Now know the size of the constructor */
			psFunctionCallData->eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;

			psFunctionCallData->sFullySpecifiedType.iArraySize =
				(uTotalSizeOfParameters / ASTSemGetSizeInfo(psGLSLTreeContext,
															&(psFunctionCallData->sFullySpecifiedType),
															IMG_FALSE));
		}
		else if (uTotalSizeOfParameters < uConstructorSize)
		{
			/* It is valid to provide a single component which is expanded to all components of the destination */
			if (uNumComponents == 1)
			{
				bExpandSourceParameter = IMG_TRUE;
			}
			else if (!bConstructingMatrixFromMatrix)
			{
				LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode,
									"'constructor' : not enough data provided for construction\n");
			}
		}
	}

	/* Can this function reduced to a constant value? */
	if (!bError && (psFunctionCallNode->uNumChildren == uNumConstantParams)) 
	{
		/* Is it a built in function */
		IMG_BOOL bBuiltInFunction = (IMG_BOOL)(psFunctionDefinitionData->eFunctionType == GLSLFT_BUILT_IN);

		if (bConstructor)
		{
			/* Reduce this constructor to a value */
			return ASTSemReduceConstantConstructor(psGLSLTreeContext,
												   psFunctionCallNode,
												   psFunctionCallData,
												   psFunctionDefinitionData,
												   uTotalSizeOfParameters,
												   bExpandSourceParameter,
												   psFunctionCallData->sFullySpecifiedType.ePrecisionQualifier);
		}
		else if (bBuiltInFunction && GLSLBuiltInFunctionReducibleToConst(psFunctionDefinitionData->eBuiltInFunctionID))
		{
			/* Reduce this built in function to a constant value */
			return ASTSemReduceConstantBuiltInFunction(psGLSLTreeContext, 
													   psFunctionCallNode,
													   psFunctionDefinitionData);
		}
	}

	/* 
	   Warn about any use of function that makes use of gradient calculations that's inside a conditional block 
	   - this us because not all pixels might take this path so the gradient calc would fail
	*/
	if (psGLSLTreeContext->eEnabledWarnings & GLSLCW_WARN_GRADIENT_CALC_INSIDE_CONDITIONAL)
	{
		if (psGLSLTreeContext->eProgramType == GLSLPT_FRAGMENT)
		{
			if (psFunctionCallData->uLoopLevel || psFunctionCallData->uConditionLevel)
			{
				/* Get the function definition data */
				GLSLFunctionDefinitionData *psFunctionDefinitionData = 
					(GLSLFunctionDefinitionData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																	 psFunctionCallData->uFunctionDefinitionSymbolID);

				if (GLSLBuiltInFunctionUseGradients(psFunctionDefinitionData->eBuiltInFunctionID))
				{
					LogProgramNodeWarning(psCPD->psErrorLog, psFunctionCallNode, 
										  "Calls to any function that may require a gradient calculation inside a conditional block may return undefined results\n");
				}
			}
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemGetArraySize
 *
 * Inputs       : psGLSLTreeContext
                  psConstantExpressionNode
 * Outputs      : piArraySize
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Returns the size of the given array into piArraySize.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTSemGetArraySize(GLSLTreeContext *psGLSLTreeContext,
							GLSLNode        *psConstantExpressionNode,
							IMG_INT32       *piArraySize)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData *)GetAndValidateSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																							   psConstantExpressionNode->uSymbolTableID,
																							   GLSLSTDT_IDENTIFIER);

	IMG_INT32 *piData;

	if (!psIdentifierData ||
	    (psIdentifierData->sFullySpecifiedType.eTypeSpecifier != GLSLTS_INT) ||
	    (psIdentifierData->pvConstantData == IMG_NULL))
	{
		return IMG_FALSE;
	}

	piData = (IMG_INT32 *)psIdentifierData->pvConstantData;

	if (*piData < 1)
	{
		*piArraySize = 0;
		return IMG_FALSE;
	}

	*piArraySize = *piData;
	
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemCheckAndUpdateArraySizes
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                  bDeclaration
                  psFullySpecifiedTypes
                  peArrayStatus
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Semantic check for an operation on arrays.
 *****************************************************************************/
static IMG_BOOL ASTSemCheckAndUpdateArraySizes(GLSLTreeContext        *psGLSLTreeContext, 
										GLSLNode               *psResultNode,
										IMG_BOOL                bDeclaration,
										GLSLFullySpecifiedType *psFullySpecifiedTypes,
										GLSLArrayStatus        *peArrayStatus)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLNode           *psIdentifierNode;
	GLSLNode           *psIntegerExpressionNode;

	GLSLIdentifierData *psIntegerData    = IMG_NULL; 
	GLSLIdentifierData *psIdentifierData = IMG_NULL;

	GLSLArrayStatus     eArrayStatus;
	IMG_INT32          *piArraySize;
	IMG_INT32          *piActiveArraySize = IMG_NULL;
	GLSLTypeSpecifier   eTypeSpecifier;
	IMG_UINT32          uNumChildren = 0;
	IMG_UINT32          i;

	switch (psResultNode->eNodeType)
	{
		case GLSLNT_EQUAL:
		{
			/* Array sizes can be inferred from the size of the initialisers */
			if (bDeclaration && (peArrayStatus[0] == GLSLAS_ARRAY_SIZE_NOT_FIXED))
			{
				/* Get the identifier data */
				psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																			psResultNode->ppsChildren[0]->uSymbolTableID);

				if(!psIdentifierData)
				{
					return IMG_FALSE;
				}

				/* Get the array size from the initialiser */
				psIdentifierData->sFullySpecifiedType.iArraySize = psFullySpecifiedTypes[1].iArraySize;

				/* Change status of array */
				psIdentifierData->eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;
			}

			/* Fall through to code below */
		}
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
			/* Increase number of children and fall through to code below */
			uNumChildren++;
		case GLSLNT_RETURN:
		{
			/* Increase number of children */
			uNumChildren++;

			for (i = 0; i < uNumChildren; i++)
			{
				psIdentifierNode = psResultNode->ppsChildren[i];

				psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psIdentifierNode, IMG_NULL);

				if (!psIdentifierData)
				{
					LOG_INTERNAL_ERROR(("ASTSemCheckAndUpdateArraySize: Symbol was not found\n"));
					return IMG_FALSE;
				}

				if (psIdentifierData->sFullySpecifiedType.iArraySize)
				{
					psIdentifierData->iActiveArraySize = psIdentifierData->sFullySpecifiedType.iArraySize;
					/* Change status of array */
					psIdentifierData->eArrayStatus     = GLSLAS_ARRAY_SIZE_FIXED;

				}
			}
			return IMG_TRUE;
		}
		case GLSLNT_ARRAY_SPECIFIER:
		{
			/* Handled below */
			break;
		}
		default:
		{
			return IMG_TRUE;
		}
	}

	/* Get the two child nodes */
	psIdentifierNode        = psResultNode->ppsChildren[0];
	psIntegerExpressionNode = psResultNode->ppsChildren[1];
	
	/* Get the data */
	psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psIdentifierNode, IMG_NULL);
	psIntegerData    = ASTSemGetIdentifierData(psGLSLTreeContext, psIntegerExpressionNode, IMG_NULL);

	if (!psIdentifierData || !psIntegerData )
	{
			LOG_INTERNAL_ERROR(("ASTSemCheckAndUpdateArraySize: Symbol was not found\n"));
			return IMG_FALSE;
	}

	eArrayStatus = psIdentifierData->eArrayStatus;

	piArraySize = &(psIdentifierData->sFullySpecifiedType.iArraySize);

	piActiveArraySize = &(psIdentifierData->iActiveArraySize);

	eTypeSpecifier = psIdentifierData->sFullySpecifiedType.eTypeSpecifier;

	if (psIntegerData->sFullySpecifiedType.eTypeSpecifier != GLSLTS_INT)
	{
		LOG_INTERNAL_NODE_ERROR((psIntegerExpressionNode,"ASTSemCheckArraySize: Expected INT type specifier (%08X)\n",
							 psIntegerData->eSymbolTableDataType));
		return IMG_FALSE;
	}

	if ((psIntegerData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST) &&
		(psIntegerData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INVALID) )
	{
		IMG_INT32 *piData = (IMG_INT32 *)psIntegerData->pvConstantData;

		if (!piData)
		{
			LOG_INTERNAL_NODE_ERROR((psIntegerExpressionNode,"ASTSemCheckArraySize: Constant data was NULL\n"));
			return IMG_FALSE;
		}

		if(eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED || eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
		{
			/* Are we dealing with a fixed array size */
			if (eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED)
			{
				/* Check array is within range */
				if (*piData >= *piArraySize)
				{
					LogProgramNodeError(psCPD->psErrorLog, psIntegerExpressionNode,
										"Array index '%d' out of bounds. Array size is %d\n",
										*piData, *piArraySize);
					return IMG_FALSE;
				}
			}
			else
			{
				/* Adjust array size if bigger */
				if (*piData >= *piArraySize)
				{
					*piArraySize = (*piData + 1);
				}
			}

			if (piActiveArraySize)
			{
				/* Adjust active size if required */
				if (*piData >= *piActiveArraySize)
				{
					*piActiveArraySize = (*piData + 1);
				}
			}

			if(*piData < 0)
			{
				LogProgramNodeError(psCPD->psErrorLog, psIntegerExpressionNode,
										"Array index '%d' is negative\n", *piData);
				return IMG_FALSE;
			}
		}
		else if (eArrayStatus == GLSLAS_NOT_ARRAY)
		{
			if (*piData >= (IMG_INT32)GLSLTypeSpecifierDimensionTable(eTypeSpecifier))
			{
				LogProgramNodeError(psCPD->psErrorLog, psIntegerExpressionNode,
									"'[' : field selection out of range '%d'\n",
									*piData);
				return IMG_FALSE;
			}

		}
		else
		{
			LOG_INTERNAL_NODE_ERROR((psIdentifierNode,"ASTSemCheckArraySize: Invalid array status\n"));
			return IMG_FALSE;
		}
	}
	/* If it's not a static value we can't check the size */
	else
	{
		/* Check that the array size is fixed */
		if (eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
		{
			LogProgramNodeError(psCPD->psErrorLog, psIntegerExpressionNode, 
								"'[' :  array must be redeclared with a size before being indexed with a variable\n");
			return IMG_FALSE;
		}

		/* Since it's being indexed with a variable we have to assume all elements are used */
		if (piActiveArraySize)
		{
			*piActiveArraySize = *piArraySize;
		}
		
		/* If this was an identifier indicate that it accessed with a dynamic index */
		if (psIdentifierData)
		{
			psIdentifierData->eIdentifierUsage |= GLSLIU_DYNAMICALLY_INDEXED;
		}
	}
	
	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTWarnUnitialisedData
 *
 * Inputs       : psGLSLTreeContext
                  psNode
                  psData
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : UTILITY: Issue a warning if operating with some unitialized data.
 *****************************************************************************/
static IMG_BOOL ASTWarnUnitialisedData(GLSLTreeContext       *psGLSLTreeContext,
								GLSLNode              *psNode,
								GLSLIdentifierData    *psData)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uScopeLevel;

	if (!(psGLSLTreeContext->eEnabledWarnings & GLSLCW_WARN_USE_OF_UNINITIALISED_DATA))
	{
		return IMG_TRUE;
	}

	/* Only warn about temp values */
	if (psData->sFullySpecifiedType.eTypeQualifier != GLSLTQ_TEMP)
	{
		return IMG_TRUE;
	}

	/* Don't warn if already warned or temp has already been written to */
	if (psData->eIdentifierUsage & (GLSLIU_WRITTEN | GLSLIU_INIT_WARNED))
	{
		return IMG_TRUE;
	}

	/* Don't warn about parameters */
	if (psData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_IN || 
		psData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INOUT)
	{
		return IMG_TRUE;
	}

	/* 
	   If the parent node was a field selection or array specifier then
	   we could be on the left hand side of an assignment expression, therefore
	   we can't report an error as the identifier might be about to be written 
	   to. 
	   
	   This means we won't report an 'unintialiased data' error if something is
	   accessed with a swizzle or array specifier, this needs to be improved.
	   
	   vec4 a, b, c;

	   a = b;        // will report an error
	   a.x = c.x;    // won't report an error
	   

	*/
	if (psNode->psParent)
	{
		if (psNode->psParent->eNodeType == GLSLNT_FIELD_SELECTION ||
			psNode->psParent->eNodeType == GLSLNT_ARRAY_SPECIFIER)
		{
			return IMG_TRUE;
		}
	}

	/* 
	   Don't warn about global symbols since they can be written
	   at any point
	*/
	GetSymbolScopeLevel(psGLSLTreeContext->psSymbolTable, 
						psNode->uSymbolTableID,
						&uScopeLevel); 

	if (!uScopeLevel)
	{
		return IMG_TRUE;
	}

	LogProgramNodeWarning(psCPD->psErrorLog, psNode,
						  "'%s' : used without being initialised\n",
						  GetSymbolName(psGLSLTreeContext->psSymbolTable, 
										psNode->uSymbolTableID));

	psData->eIdentifierUsage |= GLSLIU_INIT_WARNED;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTUpdateBuiltInsReferenced
 *
 * Inputs       : psGLSLTreeContext
                  psNode
                  bWritten
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : If psNode is a builtin variable, add it to the list of builtins that have been
                  referenced in this shader.
 *****************************************************************************/
static IMG_BOOL ASTUpdateBuiltInsReferenced(GLSLTreeContext       *psGLSLTreeContext,
									 GLSLNode              *psNode,
									 IMG_BOOL               bWritten)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 uSymbolTableID;

	GLSLIdentifierData *psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psNode, &uSymbolTableID);

	GLSLIdentifierList *psBuiltInsReferenced = psGLSLTreeContext->psBuiltInsReferenced;

	/* This catches all built in state */
	if (psIdentifierData->eIdentifierUsage & GLSLIU_BUILT_IN)
	{	
		/* Do we need to increase the list size? */
		if (psBuiltInsReferenced->uNumIdentifiersReferenced >= psBuiltInsReferenced->uIdentifiersReferencedListSize)
		{
			psBuiltInsReferenced->uIdentifiersReferencedListSize += 20;
			psBuiltInsReferenced->puIdentifiersReferenced = DebugMemRealloc(psBuiltInsReferenced->puIdentifiersReferenced, 
																			psBuiltInsReferenced->uIdentifiersReferencedListSize * sizeof(IMG_UINT32));
			/* Check alloc succeeded */
			if (!psBuiltInsReferenced->puIdentifiersReferenced)
			{
				LOG_INTERNAL_ERROR(("ASTUpdateBuiltInsReferenced: Failed to alloc memory for referenced list\n"));
				psBuiltInsReferenced->uIdentifiersReferencedListSize = 0;
				return IMG_FALSE;
			}
		}

		/* Add it to the list */
		psBuiltInsReferenced->puIdentifiersReferenced[psBuiltInsReferenced->uNumIdentifiersReferenced] = uSymbolTableID;
		psBuiltInsReferenced->uNumIdentifiersReferenced++;

		/* 
		   This catches only explicity named state (i.e. gl_Position) not implicit state
		   (i.e. the variable allocated for the data returned from sin() )
		*/
		if (psIdentifierData->eBuiltInVariableID != GLSLBV_NOT_BTIN)
		{
			if(bWritten)
			{
				switch (psIdentifierData->eBuiltInVariableID)
				{
					case GLSLBV_POSITION:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_POSITION;
						break;
					case GLSLBV_POINTSIZE:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_POINTSIZE;
						break;
					case GLSLBV_CLIPVERTEX:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_CLIPVERTEX;
						break;
					case GLSLBV_FRONTCOLOR:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FRONTCOLOR;
						break;
					case GLSLBV_BACKCOLOR:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_BACKCOLOR;
						break;
					case GLSLBV_FRONTSECONDARYCOLOR:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FRONTSECONDARYCOLOR;
						break;
					case GLSLBV_BACKSECONDARYCOLOR:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_BACKSECONDARYCOLOR;
						break;
					case GLSLBV_TEXCOORD:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_TEXCOORD;
						break;
					case GLSLBV_FRAGCOLOR:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FRAGCOLOR;
						break;
					case GLSLBV_FRAGDATA:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FRAGDATA;
						break;
					case GLSLBV_FRAGDEPTH:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FRAGDEPTH;
						break;
					case GLSLBV_FOGFRAGCOORD:
						psGLSLTreeContext->eBuiltInsWrittenTo |= GLSLBVWT_FOGFRAGCOORD;
						break;
				}
			}
		}
	}
	
	if(bWritten)
	{
		IMG_UINT uFragmentOuptutsWrittenTo = psGLSLTreeContext->eBuiltInsWrittenTo & (GLSLBVWT_FRAGDATA | GLSLBVWT_FRAGCOLOR);


		if(uFragmentOuptutsWrittenTo & (uFragmentOuptutsWrittenTo - 1))
		{
			/* The shader has written to more than one of gl_FragColor, gl_FragData or custom fragment output variables. */
			LogProgramNodeError(psCPD->psErrorLog, psNode, 
				"Fragment shader may not write to more than one of gl_FragColor, gl_FragData or custom fragment output variables\n");
		}
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTUpdateParamIdentifierUsage
 *
 * Inputs       : psGLSLTreeContext
                  psNode
                  eParameterQualifier
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Update the usage (read/write) of the given function parameter.
                  Calls ASTUpdateBuiltInsReferenced in the process.
 *****************************************************************************/
static IMG_BOOL ASTUpdateParamIdentifierUsage(GLSLTreeContext       *psGLSLTreeContext,
									   GLSLNode              *psNode,
									   GLSLParameterQualifier eParameterQualifier)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_BOOL bWritten = IMG_FALSE;

	GLSLIdentifierData *psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psNode, IMG_NULL);

	if (psIdentifierData)
	{
		/* Update this parameter usage flags */
		psIdentifierData->eIdentifierUsage |= GLSLIU_PARAM;
		
		switch (eParameterQualifier)
		{
			case GLSLPQ_OUT:
			{
				psIdentifierData->eIdentifierUsage |= GLSLIU_WRITTEN;
				bWritten = IMG_TRUE;
				break;
			}
			case GLSLPQ_INOUT:
			{
				psIdentifierData->eIdentifierUsage |= GLSLIU_READ | GLSLIU_WRITTEN;
				bWritten = IMG_TRUE;
				break;
			}
			case GLSLPQ_IN:
			{
				ASTWarnUnitialisedData(psGLSLTreeContext, psNode, psIdentifierData);
				psIdentifierData->eIdentifierUsage |= GLSLIU_READ;
				break;
			}
			default:
			{
				LOG_INTERNAL_NODE_ERROR((psNode,"ASTUpdateParamIdentifierUsage: Unhandled parameter qualifier (BCB)\n"));
				break;

			}
		}

		/* If an array is passed as a parameter then we consider all elements to be active as we have to perform a full copy of the array */
		if (psIdentifierData->eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED)
		{
			psIdentifierData->iActiveArraySize = psIdentifierData->sFullySpecifiedType.iArraySize;
		}
		
		/* Update list of built ins referenced */
		ASTUpdateBuiltInsReferenced(psGLSLTreeContext, psNode, bWritten);
	}

	return IMG_TRUE;
}	


/******************************************************************************
 * Function Name: ASTUpdateConditionalIdentifierUsage
 *
 * Inputs       : psGLSLTreeContext
                  psNode
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Update the flag indicating that the identifier is accessed conditionally.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTUpdateConditionalIdentifierUsage(GLSLTreeContext *psGLSLTreeContext, 
											 GLSLNode        *psNode)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psIdentifierData;

	psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
													  psNode->uSymbolTableID);

	if (psIdentifierData && psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER)
	{
		ASTWarnUnitialisedData(psGLSLTreeContext, psNode, psIdentifierData);
		
		psIdentifierData->eIdentifierUsage |= GLSLIU_CONDITIONAL;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTUpdateReadIdentifierUsage
 *
 * Inputs       : psGLSLTreeContext
                  psNode
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Marks a node and all of its children as 'read'.
 *****************************************************************************/
static IMG_BOOL ASTUpdateReadIdentifierUsage(GLSLTreeContext *psGLSLTreeContext,
									  GLSLNode        *psNode)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32 i;

	GLSLIdentifierData *psIdentifierData;

	IMG_UINT32 uStart = 0;

	/* 
	   If it's an assigment node and only has one child then it is read AND written (ie. a POST_INC) 
	   otherwise we don't update the 1st child as it's on the left hand side of an assigment expression
	*/
	if (psNode->eNodeType == GLSLNT_EQUAL)
	{
		uStart	= 1; 
	}

	/* Mark all children as 'read' */
	for (i = uStart; i < psNode->uNumChildren; i++)
	{
		GLSLNode *psCurrentNode = psNode->ppsChildren[i];

		if (!psCurrentNode)
		{
			LOG_INTERNAL_NODE_ERROR((psNode,"ASTUpdateReadIdentifierUsage: Null Child!\n"));
			return IMG_FALSE;
		}

		/* Mark any branches on the left as written to */
		if (psCurrentNode->eNodeType == GLSLNT_IDENTIFIER || psCurrentNode->eNodeType == GLSLNT_FUNCTION_CALL)
		{
			psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psCurrentNode, IMG_NULL);

			if (psIdentifierData)
			{
				ASTWarnUnitialisedData(psGLSLTreeContext, psCurrentNode, psIdentifierData);

				psIdentifierData->eIdentifierUsage |= GLSLIU_READ;

				/* Update list of built ins referenced */
				ASTUpdateBuiltInsReferenced(psGLSLTreeContext, psCurrentNode, IMG_FALSE);
			}
		}

		/* Recursively update this nodes children */
		ASTUpdateReadIdentifierUsage(psGLSLTreeContext, psCurrentNode);
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTUpdateIdentifierUsage
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                  bAssignmentNode
                  bDeclaration
                  bError
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Mark the identifier as read/written. Also calls ASTUpdateBuiltInsReferenced.
 ****************************************************************************/
static IMG_BOOL ASTUpdateIdentifierUsage(GLSLTreeContext *psGLSLTreeContext,
								  GLSLNode        *psResultNode,
								  IMG_BOOL         bAssignmentNode,
								  IMG_BOOL         bDeclaration,
								  IMG_BOOL         bError)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psIdentifierData;

	/*
	   Search the rest of the nodes and mark any identifiers found 
	   on the way as 'read'.
	*/
	ASTUpdateReadIdentifierUsage(psGLSLTreeContext, psResultNode);

	/* 
	   Search left hand side of assignment node and mark any identifiers found 
	   on the way as 'written to'.
	*/ 
	if (bAssignmentNode)
	{
		GLSLNode           *psLeftNode  = psResultNode;

		while (psLeftNode->uNumChildren)
		{
			psLeftNode = psLeftNode->ppsChildren[0];

			if (!psLeftNode)
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTUpdateIdentifierUsage: Null Child!\n"));
				return IMG_FALSE;
			}

			/* Mark any branches on the left as written to */
			if (psLeftNode->eNodeType == GLSLNT_IDENTIFIER)
			{
				psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																  psLeftNode->uSymbolTableID);

				if (psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER)
				{
					psIdentifierData->eIdentifierUsage |= GLSLIU_WRITTEN;
				}

				if (bDeclaration && bError)
				{
					psIdentifierData->eIdentifierUsage |= GLSLIU_ERROR_INITIALISING;
				}

				/* Update list of built ins referenced */
				ASTUpdateBuiltInsReferenced(psGLSLTreeContext, psLeftNode, IMG_TRUE);
			}
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemCheckAssignment
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 *
 * Description  : Check semantics of assignment. XXX
 *****************************************************************************/
static IMG_BOOL ASTSemCheckAssignment(GLSLTreeContext *psGLSLTreeContext,
							   GLSLNode        *psResultNode)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psLeftData;
	GLSLNode           *psLeftNode  = psResultNode->ppsChildren[0];
	GLSLNode           *psRightNode = psResultNode->uNumChildren >= 2 ? psResultNode->ppsChildren[1] : IMG_NULL;

	IMG_UINT32 uLevelsSearched = 0;

	/* 
	   Find the real node that is being assigned to 
	   i.e. 
	   myvar[10].x = 1; 
	   (myvar)++;
	   Search down until we find 'myvar' we're not interested in the array specifier, field
	   selection, subexpression or intermediate result nodes. 
	*/ 
	while (psLeftNode->uNumChildren)
	{
		psLeftNode = psLeftNode->ppsChildren[0];
		
		if (!psLeftNode)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckAssignment: Null Child!\n"));
			return IMG_FALSE;
		}

		uLevelsSearched++;
	}

	/* 
	   If we've got here and it wasn't an identifier we've encountered an error that *should* have
	   already been picked up by the previous semantic checking
	*/
	if (psLeftNode->eNodeType != GLSLNT_IDENTIFIER)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckAssignment: Unexpected node type! - %d\n", psLeftNode->eNodeType));
		return IMG_FALSE;
	}


	psLeftData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
														  psLeftNode->uSymbolTableID);
	

	if (psLeftData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
	{
		return IMG_TRUE;
	}

	/* 
	   Only deal with something that doesn't involve self modification (i.e. not i++ or i*=3)
	   as it's hard to be sure if this is constant modification (might be inside a loop) 
	   Also don't deal with identifiers that were below the immediate level as we have probably
	   gone past as swizzle or array specifier which will change the type of the identifier, hence
	   we can't setup an assocciation.
	*/
	if (psResultNode->eNodeType == GLSLNT_EQUAL && !uLevelsSearched && psRightNode->uSymbolTableID)
	{
		IMG_UINT32 uScopeLevel = 0;

		GLSLIdentifierData *psRightData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																				   psRightNode->uSymbolTableID);


		GetSymbolScopeLevel(psGLSLTreeContext->psSymbolTable, 
							psLeftNode->uSymbolTableID,
							&uScopeLevel); 

		/* Check for writes of constants to temp values */
		if ((psLeftData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_TEMP) && 
			(psRightData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST))
		{
			/*
			   If we're inside a loop then we cannot setup any data associations as their value could change each iteration of the loop.
			   If we're inside a conditional block that's above the scope level of the symbol we're
			   dealing with we can't setup any constant associations this temp value might have 
			   since we can not be sure of it's value for future uses outside of the current block (we can be sure of it's value
			   inside the current conditional block but we (currently) have no way of removing the association
			   once the block has ended so we remove the association straight away).
			*/
			if (!psGLSLTreeContext->uLoopLevel && (psGLSLTreeContext->uConditionLevel == (uScopeLevel - 1))) 
			{
				/* Associate the data on the right hand side of the expression with the variable on the left */
				psLeftData->uConstantAssociationSymbolID = psRightNode->uSymbolTableID;
				
#ifdef DUMP_LOGFILES
				DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "%08X -- Associating with data from symbol ID %08X (SL = %d, CL = %d)\n", 
							   psLeftNode->uSymbolTableID, 
							   psRightNode->uSymbolTableID,
							   uScopeLevel,
							   psGLSLTreeContext->uConditionLevel); 
#endif
				return IMG_TRUE;
			}
		}
	}

	/* Remove the association */
	if (psLeftData->uConstantAssociationSymbolID)
	{

#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_SYMTABLE_DATA, 0, "%08X -- Removing Association with data from symbol ID %08X\n", 
					   psLeftNode->uSymbolTableID, psLeftData->uConstantAssociationSymbolID); 
#endif
		psLeftData->uConstantAssociationSymbolID = 0;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ASTSemCheckForReplacementWithConstantData
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                  uChildIndex
 * Outputs      : psResultNode
 * Returns      : -
 * Description  : XXX - Unknown.
 *****************************************************************************/
static IMG_VOID ASTSemCheckForReplacementWithConstantData(GLSLTreeContext *psGLSLTreeContext, 
												   GLSLNode        *psResultNode,
												   IMG_UINT32       uChildIndex)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLIdentifierData *psIdentifierData;

	/* Don't replace inside loop statements as they don't necessarily happen in sequence */
	if (psGLSLTreeContext->uLoopLevel)
	{
		return;
	}

	/* 
	   Don't replace temp values on the left hand side of an expression  
	   or for subexpressions as we don't know the intended use (i.e. might be (x)++)
	*/
	if ((psResultNode->eNodeType == GLSLNT_EQUAL ||
		 psResultNode->eNodeType == GLSLNT_DIV_ASSIGN ||
		 psResultNode->eNodeType == GLSLNT_ADD_ASSIGN ||
		 psResultNode->eNodeType == GLSLNT_SUB_ASSIGN ||
		 psResultNode->eNodeType == GLSLNT_MUL_ASSIGN ||		
		 psResultNode->eNodeType == GLSLNT_POST_INC ||
		 psResultNode->eNodeType == GLSLNT_POST_DEC ||
		 psResultNode->eNodeType == GLSLNT_PRE_INC ||
		 psResultNode->eNodeType == GLSLNT_PRE_DEC ||
		 psResultNode->eNodeType == GLSLNT_SUBEXPRESSION ||
		 psResultNode->eNodeType == GLSLNT_FIELD_SELECTION ||
		 psResultNode->eNodeType == GLSLNT_ARRAY_SPECIFIER) &&
		uChildIndex == 0)
	{
		return;
	}


	psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																psResultNode->ppsChildren[uChildIndex]->uSymbolTableID);

	if (psIdentifierData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
	{
		return;
	}

	/* Replace the symbol ID with the constant data it's been associated with */
	if (psIdentifierData->uConstantAssociationSymbolID)
	{
#ifdef DUMP_LOGFILES
		DumpLogMessage(LOGFILE_SYMTABLE_DATA,
					   0,
					   "%08X -- Replacing with symbol ID %08X\n",
					   psResultNode->ppsChildren[uChildIndex]->uSymbolTableID,
					   psIdentifierData->uConstantAssociationSymbolID); 
#endif

		psResultNode->ppsChildren[uChildIndex]->uSymbolTableID = psIdentifierData->uConstantAssociationSymbolID;

	}
}

/******************************************************************************
 * Function Name: ASTSemGetResultPrecision
 *
 * Inputs       : psGLSLTreeContext
                  uNumChildren
                  psResultNode
                  psFullySpecifiedTypes
 * Outputs      : psResultFullySpecifiedType - Type of the result of the expression. Its precision is updated by this function.
                  puChildrenToConvert    - A list of children whose precision must be updated to pePrecisionToConvertTo.
                  pePrecisionToConvertTo - The precision the marked children need to be converted to.
 * Returns      : The number of childrens that require conversion to *pePrecisionToConvertTo. See puChildrenToConvert.
 * Description  : Determine the precision of an expression. Not to be used with function calls.
 *****************************************************************************/
static IMG_UINT32 ASTSemGetResultPrecision(GLSLTreeContext        *psGLSLTreeContext,
									IMG_UINT32              uNumChildren,
									GLSLNode               *psResultNode,
									GLSLFullySpecifiedType *psResultFullySpecifiedType,
									GLSLFullySpecifiedType psFullySpecifiedTypes[/*uNumChildren*/],
									IMG_UINT32             puChildrenToConvert[/*uNumChildren*/],
									GLSLPrecisionQualifier *pePrecisionToConvertTo)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	GLSLPrecisionQualifier ePrecisionForResult = GLSLPRECQ_UNKNOWN;
	IMG_UINT32 uNumChildrenToConvert           = 0;

	/* Set a default precision for the result */
	psResultFullySpecifiedType->ePrecisionQualifier = GLSLPRECQ_UNKNOWN;

	switch (uNumChildren)
	{
		case 0:
		{
			return 0;
		}
		case 1:
		{
			switch (psResultNode->eNodeType)
			{
				case GLSLNT_RETURN:
				{
					
					/* Get the returned data for the current function definition */ 
					GLSLIdentifierData *psIdentifierData = ASTSemGetIdentifierData(psGLSLTreeContext, psResultNode, IMG_NULL);

					if (!psIdentifierData)
					{
						LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemGetResultPrecision: Failed to get identifier data\n"));
						return 0;
					}

					/* Does the data being returned match the precision declared by the function definition? */
					if (psFullySpecifiedTypes[0].ePrecisionQualifier != psIdentifierData->sFullySpecifiedType.ePrecisionQualifier)
					{
						/* Convert the data to the same precision as that specified by the function return type */
						puChildrenToConvert[uNumChildrenToConvert++]= 0;
					}

					/* Precision always inherited from returned data */
					ePrecisionForResult = psIdentifierData->sFullySpecifiedType.ePrecisionQualifier;
					break;
				}
				default:
				{
					/* Precision inherited from only child */
					ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;
					break;
				}
			}
			break;
		}
		case 2:
		{
			switch (psResultNode->eNodeType)
			{
				case GLSLNT_EQUAL:
				case GLSLNT_DIV_ASSIGN:
				case GLSLNT_ADD_ASSIGN:
				case GLSLNT_SUB_ASSIGN:
				case GLSLNT_MUL_ASSIGN:
				{
					/* Only convert if types are different */
					if (psFullySpecifiedTypes[0].ePrecisionQualifier != psFullySpecifiedTypes[1].ePrecisionQualifier)
					{
						/* Always convert right hand side */
						puChildrenToConvert[uNumChildrenToConvert++]= 1;
					}

					/* Precision always inherited from left hand side of expression */
					ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;

					break;
				}
				case GLSLNT_FIELD_SELECTION:
				{
					/* 
					   No conversion for structure member or swizzle  operations as we're just 
					   looking up some data 
					*/

					/* Member selection from selection - Precision is inherited from member */
					if (psFullySpecifiedTypes[0].eTypeSpecifier == GLSLTS_STRUCT)
					{
						ePrecisionForResult = psFullySpecifiedTypes[1].ePrecisionQualifier;
					}
					/* Swizzle - Precision is inherited from data being swizzled */
					else
					{
						ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;
					}
					break;
				}
				case GLSLNT_ARRAY_SPECIFIER:
				{
					/* No conversion required as we're just looking up some data*/

					/* Precision is inherited from array */
					ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;
					break;
				}
				default:
				{
					/* 
					   Regular mathmatical expression so pick highest precision (unless they match in
					   which case no conversion required)
					*/
					if (psFullySpecifiedTypes[0].ePrecisionQualifier > psFullySpecifiedTypes[1].ePrecisionQualifier)
					{
						ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;
						puChildrenToConvert[uNumChildrenToConvert++]= 1;
					}
					else if (psFullySpecifiedTypes[0].ePrecisionQualifier < psFullySpecifiedTypes[1].ePrecisionQualifier)
					{
						ePrecisionForResult = psFullySpecifiedTypes[1].ePrecisionQualifier;
						puChildrenToConvert[uNumChildrenToConvert++]= 0;
					}
					/* Same type, result is the same precision */
					else
					{
						ePrecisionForResult = psFullySpecifiedTypes[0].ePrecisionQualifier;
					}
					break;
				}
			}
			break;
		}
		case 3:
		{	
			/* Should be question only */
			if (psResultNode->eNodeType != GLSLNT_QUESTION)
			{
				LOG_INTERNAL_ERROR(("ASTSemGetResultPrecision: Unrecognised node with 3 children\n"));
				return 0;
			}
			/* Pick highest of precisions */
			if (psFullySpecifiedTypes[1].ePrecisionQualifier > psFullySpecifiedTypes[2].ePrecisionQualifier)
			{
				ePrecisionForResult = psFullySpecifiedTypes[1].ePrecisionQualifier;
				puChildrenToConvert[uNumChildrenToConvert++]= 2;
			}
			else if (psFullySpecifiedTypes[1].ePrecisionQualifier < psFullySpecifiedTypes[2].ePrecisionQualifier)
			{
				ePrecisionForResult = psFullySpecifiedTypes[2].ePrecisionQualifier;
				puChildrenToConvert[uNumChildrenToConvert++]= 1;
			}
			/* Precisions must match - check for unknown values */
			else 
			{
				/* 
				   If both are unknown (can happen if both are literals) then both 
				   need converting to a known precision 
					   - use the default for this data type 
				*/
				if (psFullySpecifiedTypes[1].ePrecisionQualifier == GLSLPRECQ_UNKNOWN)
				{
					IMG_BOOL bDoConversion = IMG_FALSE;

					if (GLSL_IS_INT(psResultFullySpecifiedType->eTypeSpecifier))
					{
						ePrecisionForResult = psGLSLTreeContext->eDefaultIntPrecision;

						bDoConversion = IMG_TRUE;
					}
					else if (GLSL_IS_FLOAT(psResultFullySpecifiedType->eTypeSpecifier))
					{
						ePrecisionForResult = psGLSLTreeContext->eDefaultFloatPrecision;
						
						bDoConversion = IMG_TRUE;
					}

					/* 
					   Default precision might also be unknown in which case no 
					   point doing conversion 
					*/
					if (bDoConversion && (ePrecisionForResult != GLSLPRECQ_UNKNOWN))
					{
						/* Convert both */
						puChildrenToConvert[uNumChildrenToConvert++]= 1;
						puChildrenToConvert[uNumChildrenToConvert++]= 2;
					}
					
				}
				/* Types match with known precision no conversion required */
				else
				{
					ePrecisionForResult = psFullySpecifiedTypes[1].ePrecisionQualifier;
				}
			}
			break;
		}
		default:
		{
			LOG_INTERNAL_ERROR(("ASTSemGetResultPrecision: Unrecognised number of children for node\n"));
			return 0;
		}
	}

	/* No point converting to unknown */
	if (ePrecisionForResult == GLSLPRECQ_UNKNOWN)
	{
		return 0;
	}
	else
	{
		/* Does the result need a precision? */
		if (GLSL_IS_NUMBER(psResultFullySpecifiedType->eTypeSpecifier) ||
		    GLSL_IS_SAMPLER(psResultFullySpecifiedType->eTypeSpecifier))
		{
			/* Indicate the precision of the result */
			psResultFullySpecifiedType->ePrecisionQualifier = ePrecisionForResult;
		}

		*pePrecisionToConvertTo = ePrecisionForResult;

		return uNumChildrenToConvert;
	}
}

/******************************************************************************
 * Function Name: ASTSemCheckTypesAndCalculateResult
 *
 * Inputs       : psGLSLTreeContext
                  psResultNode
                  bDeclaration
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Description  : Checks that the types of the node are compatible, then computes
                  the results's type and precision. Also does constant expression reduction.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ASTSemCheckTypesAndCalculateResult(GLSLTreeContext *psGLSLTreeContext, 
											GLSLNode        *psResultNode,
											IMG_BOOL         bDeclaration)
{
	GLSLCompilerPrivateData *psCPD = GET_CPD_FROM_AST(psGLSLTreeContext);

	IMG_UINT32              uNumChildren;
	IMG_UINT32              i;
	IMG_UINT32              uNumErrorNodes = 0;
	IMG_UINT32              uErrorNodeIndices[4];
	IMG_UINT32              uNumCorrectNodes = 0;
	IMG_UINT32              uCorrectNodeIndices[4];
	IMG_UINT32              uDimensions[4];
	IMG_BOOL                bAllErrorNodes = IMG_FALSE;
	GLSLGenericData        *psChildGenericData;
	GLSLFullySpecifiedType  asChildFullySpecifiedType[4];
	GLSLArrayStatus         aeArrayStatus[4];
	GLSLIdentifierData      sResultData;
	GLSLLValueStatus        aeChildLValueStatus[4];
	GLSLPrecisionQualifier  ePrecisionToConvertTo = GLSLPRECQ_UNKNOWN;
	IMG_UINT32              auPrecisionConversionChildren[4];
	IMG_UINT32              uNumChildrenRequiringPrecisionConstructor = 0;
	IMG_CHAR                acResultName[256];
	IMG_CHAR                acDescriptions[4][256];
	IMG_BOOL                bAllConstants = IMG_TRUE;
	IMG_BOOL                bError = IMG_FALSE;
	IMG_BOOL                bAssignmentOperation = IMG_FALSE;
	IMG_BOOL                bGenResultData = IMG_TRUE;
	IMG_BOOL                bArrayPresent                = IMG_FALSE;
	IMG_BOOL                bArraySizesMatch             = IMG_TRUE;
	IMG_BOOL                bArraySizesDefined           = IMG_TRUE;
	IMG_BOOL                bArraySizesMatchedAndDefined = IMG_FALSE;
	IMG_BOOL                bArraySizesOK                = IMG_TRUE;
	IMG_BOOL                bAllTypeSpecifiersMatch      = IMG_TRUE;
	IMG_BOOL                bAllBaseTypesMatch           = IMG_TRUE;
	IMG_BOOL                bAllNumbers                  = IMG_TRUE;

	/* Process function calls */
	if (psResultNode->eNodeType == GLSLNT_FUNCTION_CALL)
	{
		return ASTSemCheckFunctionCallParameters(psGLSLTreeContext, psResultNode);
	}

	/* Set up some default values */
	sResultData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;

	INIT_FULLY_SPECIFIED_TYPE(&(sResultData.sFullySpecifiedType));
	sResultData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;

	sResultData.iActiveArraySize                             = -1;
	sResultData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
	sResultData.eLValueStatus                                = GLSLLV_NOT_L_VALUE;	
	sResultData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;	
	sResultData.eIdentifierUsage                             = (GLSLIdentifierUsage)(GLSLIU_WRITTEN | GLSLIU_INTERNALRESULT);
	sResultData.uConstantDataSize                            = 0;
	sResultData.pvConstantData                               = IMG_NULL;
	sResultData.uConstantAssociationSymbolID                 = 0;


	/* Get the number of children for this node */
	uNumChildren = ASTCheckNodeNumChildren(psCPD, psResultNode);
	
	if (psResultNode->uSymbolTableID)
	{
		LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Type has already been calculated for this node\n"));
		return IMG_FALSE;
	}
	

	/* Special handling for sub expressions and expressions */
	if ((psResultNode->eNodeType == GLSLNT_SUBEXPRESSION) || 
		(psResultNode->eNodeType == GLSLNT_EXPRESSION) ||
		(psResultNode->eNodeType == GLSLNT_DECLARATION))												
	{
		GLSLNode *psLastChildNode = psResultNode->ppsChildren[psResultNode->uNumChildren - 1];

		/* Check this node was valid */
		if (psLastChildNode->eNodeType == GLSLNT_ERROR)
		{
			/* If not then return */
			return IMG_TRUE;
		}

		/* 
		   Sub expressions get the same symbol ID as the top level node of the sub expression  
		*/
		psResultNode->uSymbolTableID = psLastChildNode->uSymbolTableID;

		if (!psResultNode->uSymbolTableID)
		{
			if (psResultNode->eNodeType == GLSLNT_SUBEXPRESSION)
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Last SubExpression child (%d) should always have a symbol ID\n",
									 psResultNode->uNumChildren));
			}
			else if (psResultNode->eNodeType == GLSLNT_EXPRESSION)
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Last Expression child (%d) should always have a symbol ID\n",
									 psResultNode->uNumChildren));
			}
			else
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Last declaration child (%d) should always have a symbol ID\n",
									 psResultNode->uNumChildren));
			}
			return IMG_FALSE;
		}

		psChildGenericData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, psResultNode->uSymbolTableID);

		if (!psChildGenericData)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Failed to retrieve generic date for child of (sub)expression,declaration\n"));
		}

		return IMG_TRUE;
	}

	/* Get type information about all the children */
	for (i = 0; i < uNumChildren; i++)
	{
		GLSLNode *psChildNode = psResultNode->ppsChildren[i];

		if (psChildNode && (psChildNode->eNodeType != GLSLNT_ERROR))
		{
			if (psChildNode->uSymbolTableID)
			{
				/* Check if this data can be replaced with constant data */
				ASTSemCheckForReplacementWithConstantData(psGLSLTreeContext, psResultNode, i);

				/* Get the types of the specifier */
				if (!GetSymbolInfo(psCPD, psGLSLTreeContext->psSymbolTable,
								   psChildNode->uSymbolTableID,
								   psGLSLTreeContext->eProgramType,
								   &(asChildFullySpecifiedType[i]),
								   &(aeChildLValueStatus[i]),
								   &(uDimensions[i]),
								   &(aeArrayStatus[i]),
								   IMG_NULL, 
								   IMG_NULL,
								   acDescriptions[i]))
				{
					LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Failed to retrieve symbol data\n"));
					return IMG_FALSE;
				}
			}
			else
			{
				INIT_FULLY_SPECIFIED_TYPE(&(asChildFullySpecifiedType[i]));

				aeChildLValueStatus[i]                      = GLSLLV_L_VALUE;
				uDimensions[i]                              = 1;
				
				strcpy(acDescriptions[i],"no-symbol-id"); 
			}

			uCorrectNodeIndices[uNumCorrectNodes] = i; 

			uNumCorrectNodes++;

		}
		else
		{
			/* default type for errors is float */
			INIT_FULLY_SPECIFIED_TYPE(&(asChildFullySpecifiedType[i]));
			asChildFullySpecifiedType[i].eTypeSpecifier = GLSLTS_FLOAT;

			aeChildLValueStatus[i]                      = GLSLLV_L_VALUE;
			uDimensions[i]                              = 1;

			strcpy(acDescriptions[i],"float");

			uErrorNodeIndices[uNumErrorNodes] = i; 

			uNumErrorNodes++;
		}

		/* Constants parameters are not really constants for our purposes */
		if ((asChildFullySpecifiedType[i].eTypeQualifier != GLSLTQ_CONST) || 
			(asChildFullySpecifiedType[i].eParameterQualifier != GLSLPQ_INVALID))
		{
			bAllConstants = IMG_FALSE;
		}

		/* Check for presence of array */
		if (asChildFullySpecifiedType[i].iArraySize != 0)
		{
			bArrayPresent = IMG_TRUE;
		}
		/* Check for undefined array sizes */
		if (asChildFullySpecifiedType[i].iArraySize == -1)
		{
			bArraySizesDefined = IMG_FALSE;
		}

		/* Check for presence of non numeric data */
		if (!GLSL_IS_NUMBER(asChildFullySpecifiedType[i].eTypeSpecifier))
		{
			bAllNumbers = IMG_FALSE;
		}

		/* Check for mismatched array sizes */
		if (i)
		{
			/* Check for mismatched array sizes */
			if (asChildFullySpecifiedType[i].iArraySize != asChildFullySpecifiedType[i-1].iArraySize)
			{
				bArraySizesMatch = IMG_FALSE;
			}

			/* Did the base types match? */
			if (GLSLTypeSpecifierBaseTypeTable(asChildFullySpecifiedType[i].eTypeSpecifier) == 
				GLSLTypeSpecifierBaseTypeTable(asChildFullySpecifiedType[i-1].eTypeSpecifier))
			{
				/* Did the type specifiers match? */
				if (!ASTSemCheckTypeSpecifiersMatch(&asChildFullySpecifiedType[i],
													&asChildFullySpecifiedType[i-1]))
				{
					bAllTypeSpecifiersMatch = IMG_FALSE;
				}
			}
			else
			{
				bAllBaseTypesMatch      = IMG_FALSE;
				bAllTypeSpecifiersMatch = IMG_FALSE;
			}
		}
	}

	/* Sort out array status */
	if (bArraySizesMatch && bArraySizesDefined)
	{
		bArraySizesMatchedAndDefined = IMG_TRUE;
	}
	
	if (psGLSLTreeContext->uSupportedLanguageVersion >= 120)
	{
		bArraySizesOK = bArraySizesMatchedAndDefined;
	}
	else
	{
		bArraySizesOK = (IMG_BOOL)(!bArrayPresent);
	}

	if (uNumErrorNodes == uNumChildren)
	{
		bAllErrorNodes = IMG_TRUE;
	}

#if !defined(GLSL_ES)
	/* 
	   Are there any implicit conversions (int -> float) we should do first? 
	   (only supported in language versions 1.20+)
	*/
	if (!bAllBaseTypesMatch && (psGLSLTreeContext->uSupportedLanguageVersion >= 120))
	{
		IMG_UINT32 uNodeToConvert = 0;
		IMG_UINT32 uFloatNode     = 0;
		IMG_UINT32 uNumInts       = 0;
		
		/* Check if this operation allows implicit conversions */
		if (NODETYPE_ITOF(psResultNode->eNodeType))
		{
			/* 
			   for 2 child expresions check both
			   for 3 child expressions (QUESTION only) check 1 & 2 
			*/
			for (i = uNumChildren - 2; i < uNumChildren; i++)
			{
				if (GLSL_IS_INT(asChildFullySpecifiedType[i].eTypeSpecifier))
				{
					uNodeToConvert = i;
					uNumInts++;
				}
				else
				{
					uFloatNode = i;
				}
			}

			/* Quick sanity check */
			if ((uNumInts == 0) || (uNumInts > 2))
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Incorrect number of children (%d) for int to float conversion\n",
									 uNumInts));
			}

			switch (psResultNode->eNodeType)
			{
				case GLSLNT_EQUAL:
				case GLSLNT_DIV_ASSIGN:
				case GLSLNT_ADD_ASSIGN:
				case GLSLNT_SUB_ASSIGN:
				case GLSLNT_MUL_ASSIGN:
				{
					if (uNodeToConvert == 0)
					{
						break;
					}
				}
				case GLSLNT_QUESTION:
				{
					if (uNumInts == 2)
					{
						break;
					}
				}
				default:
				{
					GLSLTypeSpecifier eDestTypeSpecifier = GLSLTS_FLOAT + (uDimensions[uNodeToConvert] - 1);

					ASTSemInsertConstructor(psGLSLTreeContext, 
											psResultNode, 
											uNodeToConvert,
											GLSLPRECQ_UNKNOWN,
											eDestTypeSpecifier,
											IMG_FALSE);

					asChildFullySpecifiedType[uNodeToConvert].eTypeSpecifier = eDestTypeSpecifier;

					bAllBaseTypesMatch      = IMG_TRUE;

					if (asChildFullySpecifiedType[uNodeToConvert].eTypeSpecifier == asChildFullySpecifiedType[uFloatNode].eTypeSpecifier)
					{
						bAllTypeSpecifiersMatch = IMG_TRUE;
					}
				}
			}
		}
	}
#endif

	/*
	  Taken from spec 
	  When the operands are of a different type they must fit into one of the following rules:
	  
	  - One of the arguments is a float (i.e. a scalar), in which case the result is as if the scalar value
	  was replicated into a vector or matrix before being applied.

	  - Left argument is a floating-point vector and the right is a matrix with a compatible
	  dimension in which case the * operator will do a row vector matrix multiplication.
	  integer expression required
	  - Left argument is a matrix and the right is a floating-point vector with a compatible
	  dimension in which case the * operator will do a column vector matrix multiplication.
	*/

	switch (psResultNode->eNodeType)
	{
		/*
		  The arithmetic unary operators negate (-), post- and pre-increment and decrement (-- and
		  ++) that operate on integer or floating-point values (including vectors and matrices). These
		  result with the same type they operated on. For post- and pre-increment and decrement, the
		  expression must be one that could be assigned to (an l-value). Pre-increment and predecrement
		  add or subtract 1 or 1.0 to the contents of the expression they operate on, and the
		  value of the pre-increment or pre-decrement expression is the resulting value of that
		  modification. Post-increment and post-decrement expressions add or subtract 1 or 1.0 to
		  the contents of the expression they operate on, but the resulting expression has the
		  expression's value before the post-increment or post-decrement was executed.
		*/
		case GLSLNT_NEGATE:
		case GLSLNT_POSITIVE:
		{
			if (bAllNumbers && !bArrayPresent)
			{
				sResultData.sFullySpecifiedType.eTypeSpecifier = asChildFullySpecifiedType[0].eTypeSpecifier;
			}
			else
			{
				/* Report type mismatch */
				ASTSemReportWrongOperandType(psCPD->psErrorLog, psResultNode, acDescriptions[0]);
				
				sResultData.sFullySpecifiedType.eTypeSpecifier =  GLSLTS_FLOAT;

				bError = IMG_TRUE;
			}

			if (psResultNode->eNodeType == GLSLNT_POSITIVE)
			{
				/* For positive, inherit child's L-Value */
				sResultData.eLValueStatus = aeChildLValueStatus[0];
			}
			else
			{
				sResultData.eLValueStatus = GLSLLV_NOT_L_VALUE;
			}

			break;
		}
		case GLSLNT_PRE_INC:
		case GLSLNT_PRE_DEC:
		case GLSLNT_POST_INC:
		case GLSLNT_POST_DEC:
		{
			bAssignmentOperation = IMG_TRUE;

			if (bAllNumbers && !bArrayPresent)
			{
				sResultData.sFullySpecifiedType.eTypeSpecifier = asChildFullySpecifiedType[0].eTypeSpecifier;
			}
			else
			{
				/* Report type mismatch */
				ASTSemReportWrongOperandType(psCPD->psErrorLog, psResultNode, acDescriptions[0]);

				sResultData.sFullySpecifiedType.eTypeSpecifier =  GLSLTS_FLOAT;

				bError = IMG_TRUE;
			}

			if (aeChildLValueStatus[0] != GLSLLV_L_VALUE)
			{
				ASTSemReportLValueError(psCPD, psResultNode, 
										aeChildLValueStatus[0], 
										asChildFullySpecifiedType[0].eTypeQualifier,
										psResultNode->psToken->pvData);

				bError = IMG_TRUE;
			}

			break;
		}
		case GLSLNT_ARRAY_LENGTH:
		{
			GLSLNode *psChildNode = psResultNode->ppsChildren[0];

			if (!asChildFullySpecifiedType[0].iArraySize)
			{
				LogProgramNodeError(psCPD->psErrorLog, psChildNode, 
									"'%s' : .length() requires array on left hand side\n",
									psChildNode->psToken->pvData);
				bError = IMG_TRUE;
			}

			/* 
			   Anything that has an array length on it is actually a constant (even though
			   it doesn't look like it 
			*/
			bAllConstants = IMG_TRUE;

			break;
		}
		/*
		  The logical unary operator not (!). It operates only on a Boolean expression and results in a
		  Boolean expression. To operate on a vector, use the built-in function not.
		*/
		case GLSLNT_NOT:
		{
			if (bAllErrorNodes)
			{
				/* If this node generated an error the default result type is float */
				asChildFullySpecifiedType[0].eTypeSpecifier = GLSLTS_FLOAT;
				strcpy(acDescriptions[0],"float");

			}

			if ((asChildFullySpecifiedType[0].eTypeSpecifier != GLSLTS_BOOL) || bArrayPresent)
			{
				/* Report type mismatch */
				ASTSemReportWrongOperandType(psCPD->psErrorLog, psResultNode, acDescriptions[0]);
				bError = IMG_TRUE;
			}

			sResultData.sFullySpecifiedType.eTypeSpecifier =  GLSLTS_BOOL;

			break;
		}
		case GLSLNT_FIELD_SELECTION:
		{
			GLSLIdentifierData *psIdentifierData, *psGenericData;
			GLSLNode *psLeftNode, *psRightNode;

			if(psResultNode->uNumChildren != 2)
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Field selection requires a left hand side and a right hand side\n"));
				bError = IMG_TRUE;
				return IMG_FALSE;
			}

			psLeftNode  = psResultNode->ppsChildren[0];
			psRightNode = psResultNode->ppsChildren[1];
						
			if (uNumErrorNodes)
			{
				/* Result is always inherits the type of the left node even if that came from an error */
				sResultData.sFullySpecifiedType.eTypeSpecifier           = asChildFullySpecifiedType[0].eTypeSpecifier;
				sResultData.sFullySpecifiedType.uStructDescSymbolTableID = asChildFullySpecifiedType[0].uStructDescSymbolTableID;
				bError = IMG_TRUE;
				break;
			}
			
			psIdentifierData = (GLSLIdentifierData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
																		psLeftNode->uSymbolTableID);

			psGenericData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
											   psRightNode->uSymbolTableID);

			if (asChildFullySpecifiedType[0].iArraySize)
			{
				LogProgramNodeError(psCPD->psErrorLog, psRightNode, 
									"'.' : cannot apply dot operator to an array\n");
			}
			
			if (psGenericData->eSymbolTableDataType == GLSLSTDT_SWIZZLE)
			{
				GLSLSwizzleData *psSwizzleData = (GLSLSwizzleData *)psGenericData;

				if (!GLSL_IS_VECTOR(asChildFullySpecifiedType[0].eTypeSpecifier))
				{
					LogProgramNodeError(psCPD->psErrorLog, psRightNode, 
										"'%s' : field selection requires structure or vector on left hand side\n",
										psRightNode->psToken->pvData);
	
					bError = IMG_TRUE;
					break;
				}

				/* Check if component access's are within range */
				for (i = 0; i < psSwizzleData->uNumComponents; i++)
				{
					if (psSwizzleData->uComponentIndex[i] >= uDimensions[0])
					{
						LogProgramNodeError(psCPD->psErrorLog, psRightNode, 
											"'%s' : vector field selection out of range\n",
											psRightNode->psToken->pvData);
						bError = IMG_TRUE;
					}
				}

				/* 
				   Adjust the result type specifier based on the number of components specified 
				   - Relies on the enums for type specifier being declared in sequential size order
				*/
				sResultData.sFullySpecifiedType.eTypeSpecifier = (GLSLTypeSpecifier)(asChildFullySpecifiedType[0].eTypeSpecifier + (psSwizzleData->uNumComponents - uDimensions[0]));
				
				/* Swizzles l-value inherited from left child */ 
				sResultData.eLValueStatus = aeChildLValueStatus[0];

				/* Check for duplication of swizzle values which will make it negate it's l-value status */
				if (aeChildLValueStatus[0] == GLSLLV_L_VALUE)
				{
					for (i = 0; i < psSwizzleData->uNumComponents; i++)
					{
						IMG_UINT32 j;
						
						for (j = i; j > 0; j--) 
						{
							if (psSwizzleData->uComponentIndex[j-1] == psSwizzleData->uComponentIndex[i])
							{
								sResultData.eLValueStatus = GLSLLV_NOT_L_VALUE_DUP_SWIZZLE;
							}
						}
					}
				}
			}
			else if (psGenericData->eSymbolTableDataType == GLSLSTDT_MEMBER_SELECTION)
			{
				sResultData.sFullySpecifiedType                = asChildFullySpecifiedType[1];
				sResultData.eLValueStatus                      = aeChildLValueStatus[1];
				sResultData.eArrayStatus                       = aeArrayStatus[1];
			}
			else
			{
				LogProgramNodeError(psCPD->psErrorLog, psRightNode, 
									"'.' : cannot apply dot operator to a %s\n",
									acDescriptions[0]);
				bError = IMG_TRUE;
			}

			/* If the left node was an identifier then we need to preserve the built in variable ID so we can spot writes to gl_Position */
			if (psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER)
			{
				sResultData.eBuiltInVariableID = psIdentifierData->eBuiltInVariableID;
			}

			break;
		}
		case GLSLNT_ARRAY_SPECIFIER:
		{
			GLSLNode *psLeftNode = psResultNode->ppsChildren[0];
			
			/* Check index is of type int */
			if (asChildFullySpecifiedType[1].eTypeSpecifier != GLSLTS_INT)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode, 
									"'[%s]' : integer expression required\n",
									psResultNode->ppsChildren[1]->psToken->pvData);
				bError = IMG_TRUE;
			}

			/* Check left side of type array matrix or vector */
			if (asChildFullySpecifiedType[0].iArraySize)
			{
				/* 
				   If it's of type array then the result inherits the type of the array 
				   - This needs to come before any other  
				*/
				sResultData.sFullySpecifiedType = asChildFullySpecifiedType[0];
				
				/* The result of an array index is not an array */
				sResultData.sFullySpecifiedType.iArraySize = 0;

#if !defined(GLSL_ES)
				/* GLSL_ES require constant index expressions, while GLSL requires constant integral expressions.
				 * For GLSL ES we'll catch sampler index errors later (after loop unrolling).
				 */

				if(GLSL_IS_SAMPLER(asChildFullySpecifiedType[0].eTypeSpecifier) &&
					asChildFullySpecifiedType[1].eTypeQualifier != GLSLTQ_CONST)
				{
					/* Only constants are allowed to index into arrays of samplers. */
					LogProgramNodeError(psCPD->psErrorLog, psResultNode, 
											"'[%s]' : arrays of samplers may only be indexed by a constant integer expression\n",
											psResultNode->ppsChildren[1]->psToken->pvData);
					bError = IMG_TRUE;
				}
#endif
			}
			/* If applied to matrix or vector then it modifies the type specifier */
			else if ((GLSL_IS_NUMBER(asChildFullySpecifiedType[0].eTypeSpecifier) || 
					  GLSL_IS_BOOL(asChildFullySpecifiedType[0].eTypeSpecifier)) 
					 && (uDimensions[0] > 1))
			{
				/* Get dimension of the indexed result */
				IMG_UINT32 uIndexedDimension = GLSLTypeSpecifierIndexedTable(asChildFullySpecifiedType[0].eTypeSpecifier);

				/* Modify it to be a scalar/vector of the same type */
				sResultData.sFullySpecifiedType.eTypeSpecifier = (GLSLTypeSpecifier)((GLSLTypeSpecifierBaseTypeTable(asChildFullySpecifiedType[0].eTypeSpecifier) +
																  (uIndexedDimension - 1)));
			}
			/* Left side is not of type array matrix or vector so raise error */
			else
			{
				if (psLeftNode->psToken)
				{
					LogProgramNodeError(psCPD->psErrorLog, psResultNode, 
										"'%s' : left of '[' is not of type array, matrix, or vector\n", 
										psLeftNode->psToken->pvData);
					bError = IMG_TRUE;
				}
				else
				{
					LogProgramNodeError(psCPD->psErrorLog, psResultNode, 
										"'' : left of '[' is not of type array, matrix, or vector\n");
					bError = IMG_TRUE;
				}
				
				if (uErrorNodeIndices[0] != 0)
				{
					sResultData.sFullySpecifiedType = asChildFullySpecifiedType[0];
				}
				else
				{
					sResultData.sFullySpecifiedType.eTypeSpecifier = GLSLTS_FLOAT;
				}
			}

			/* l-value is inherited */
			sResultData.eLValueStatus                  = aeChildLValueStatus[0];

			break;
		}
	
		/* 
		   The arithmetic binary operators add (+), subtract (-), multiply (*), and divide (/), that
		   operate on integer and floating-point typed expressions (including vectors and matrices).
		   The two operands must be the same type, or one can be a scalar float and the other a float
		   vector or matrix, or one can be a scalar integer and the other an integer vector. Additionally,
		   for multiply (*), one can be a vector and the other a matrix with the same dimensional size
		   of the vector. These result in the same fundamental type (integer or float) as the expressions
		   they operate on. If one operand is scalar and the other is a vector or matrix, the scalar is
		   applied component-wise to the vector or matrix, resulting in the same type as the vector or
		   matrix. Dividing by zero does not cause an exception but does result in an unspecified
		   value. Multiply (*) applied to two vectors yields a component-wise multiply. Multiply (*)
		   applied to two matrices yields a linear algebraic matrix multiply, not a component-wise
		   multiply. Use the built-in functions dot, cross, and matrixCompMult to get, respectively,
		   vector dot product, vector cross product, and matrix component-wise multiplication.
		*/
		case GLSLNT_DIVIDE:
		case GLSLNT_ADD:
		case GLSLNT_SUBTRACT:
		case GLSLNT_MULTIPLY:
		{
			GLSLTypeSpecifier eLeftTypeSpecifier  = asChildFullySpecifiedType[0].eTypeSpecifier;
			GLSLTypeSpecifier eRightTypeSpecifier = asChildFullySpecifiedType[1].eTypeSpecifier;

			/* If all nodes were invalid then return a type of float */
			if (bAllErrorNodes)
			{
				sResultData.sFullySpecifiedType.eTypeSpecifier =  GLSLTS_FLOAT;
				break;
			}
			/* If one of the nodes was invalid then set a default type for it */
			else if (uNumErrorNodes)
			{
				/* If the correct node was of the correct type then it inherits its type otherwise it gets float */
				if (GLSL_IS_NUMBER(asChildFullySpecifiedType[uCorrectNodeIndices[0]].eTypeSpecifier)) 
				{
					asChildFullySpecifiedType[uErrorNodeIndices[0]] = asChildFullySpecifiedType[uCorrectNodeIndices[0]];
					strcpy(acDescriptions[uErrorNodeIndices[0]], acDescriptions[uCorrectNodeIndices[0]]);					
					
					/* The default return type is always that of the left node */ 
					sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;
				}
				else
				{
					asChildFullySpecifiedType[uErrorNodeIndices[0]].eTypeSpecifier = GLSLTS_FLOAT;
					strcpy(acDescriptions[uErrorNodeIndices[0]], "float");

					/* The default return type is always that of the left node */ 
					sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;

					/* Report type mismatch */
					ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
					bError = IMG_TRUE;
					break;
				}
			}
			/* Both nodes were valid, check there types were OK for a mulitplicative expression */
			else
			{
				/* The default return type is always that of the left node */ 
				sResultData.sFullySpecifiedType.eTypeSpecifier  = eLeftTypeSpecifier;

				/* Check they are both of the right type */
				if (!bAllNumbers || bArrayPresent)
				{
					/* Report type mismatch */
					ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
					bError = IMG_TRUE;
					break;
				}
			}

			/* Check the fundamental types match */
			if (bAllBaseTypesMatch)
			{
				/* Do they match exactly */
				if (bAllTypeSpecifiersMatch)
				{
					/* Yes - then result inherits the same type */
					sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;
					break;
				}
				/* Is either one a scalar int? */
				else if (GLSL_IS_INT(eLeftTypeSpecifier))
				{
					/* Check for a scalar int and a vector int */
					if (GLSL_IS_INT_SCALAR(eLeftTypeSpecifier) && (GLSL_IS_INT(eRightTypeSpecifier)))
					{
						/* Return the vector type */
						sResultData.sFullySpecifiedType.eTypeSpecifier = eRightTypeSpecifier;
						break;
					}
					/* Check for a vector int and a scalar int */
					else if ((GLSL_IS_INT(eLeftTypeSpecifier)) && GLSL_IS_INT_SCALAR(eRightTypeSpecifier))
					{
						/* Return the vector type */
						sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;
						break;
					}
				}
				else 
				{
					/* Is either one a scalar float? */
					if ((eLeftTypeSpecifier == GLSLTS_FLOAT) || (eRightTypeSpecifier == GLSLTS_FLOAT))
					{
						/* Check for a scalar float and a vector float */
						if ((eLeftTypeSpecifier == GLSLTS_FLOAT) && (GLSL_IS_FLOAT(eRightTypeSpecifier)))
						{
							/* Return the vector type */
							sResultData.sFullySpecifiedType.eTypeSpecifier = eRightTypeSpecifier;
							break;
						}
						/* Check for a vector float and a scalar float */
						else if ((GLSL_IS_FLOAT(eLeftTypeSpecifier)) && (eRightTypeSpecifier == GLSLTS_FLOAT))
						{
							/* Return the vector type */
							sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;
							break;
						}
						/* Check for a scalar float and a matrix */
						else if ((eLeftTypeSpecifier == GLSLTS_FLOAT) && (GLSL_IS_MATRIX(eRightTypeSpecifier)))
						{
							/* Return the matrix type */ 
							sResultData.sFullySpecifiedType.eTypeSpecifier = eRightTypeSpecifier;
							break;
						}
						/* Check for a matrix and scalar float and a matrix */
						else if ((GLSL_IS_MATRIX(eLeftTypeSpecifier)) && (eRightTypeSpecifier == GLSLTS_FLOAT))
						{
							/* Return the matrix type */ 
							sResultData.sFullySpecifiedType.eTypeSpecifier = eLeftTypeSpecifier;
							break;
						}
					}
					/* Multiply can also support matrix and vector multiplies */
					else if (psResultNode->eNodeType == GLSLNT_MULTIPLY)
					{
						/* 
						   Matrix by matrix multiply where matrices are different sizes (otherwise would have taken very first 'if' branch) 
						   OR Matrix by vector multiply
						*/
						if ((GLSL_IS_MATRIX(eLeftTypeSpecifier) && GLSL_IS_MATRIX(eRightTypeSpecifier)) ||
							(GLSL_IS_MATRIX(eLeftTypeSpecifier) && GLSL_IS_FLOAT(eRightTypeSpecifier))  ||
							(GLSL_IS_FLOAT(eLeftTypeSpecifier)  && GLSL_IS_MATRIX(eRightTypeSpecifier)))
						{
							/* 
							   Rule for matrix multiplies is 
						   
							   M[c1][r1] * M[c2][r2] = M[c2][r1] 
						   
							   if (c1 == r2)
							*/

							IMG_UINT32 uLeftNumCols   = GLSLTypeSpecifierNumColumnsCMTable(eLeftTypeSpecifier);
							IMG_UINT32 uLeftNumRows   = GLSLTypeSpecifierNumRowsCMTable   (eLeftTypeSpecifier);
							IMG_UINT32 uRightNumRows  = GLSLTypeSpecifierNumRowsCMTable   (eRightTypeSpecifier);
							IMG_UINT32 uRightNumCols  = GLSLTypeSpecifierNumColumnsCMTable(eRightTypeSpecifier);
							IMG_INT32  iResultAdjustment;
							GLSLTypeSpecifier eResultTypeSpecifier;

							/* If left operand is a vector, treated it as single row matrix - by default it is single column matrix */
							if(!GLSL_IS_MATRIX(eLeftTypeSpecifier))
							{
								uLeftNumCols = uLeftNumRows;
								uLeftNumRows = 1;
							}

							/* Adjust types depending on arguments */
							if (GLSL_IS_FLOAT(eLeftTypeSpecifier))
							{
								eResultTypeSpecifier = eLeftTypeSpecifier;

								iResultAdjustment = uLeftNumCols - uRightNumCols;
							}
							else
							{
								eResultTypeSpecifier = eRightTypeSpecifier;
							
								iResultAdjustment = uRightNumRows - uLeftNumRows;
							}

							/* Check for correct matrix dimensions */
							if (uLeftNumCols == uRightNumRows)
							{
								sResultData.sFullySpecifiedType.eTypeSpecifier = (GLSLTypeSpecifier)((eResultTypeSpecifier - iResultAdjustment));
								break;
							}
						}
					}
				} 
			}

			/* Report type mismatch */
			ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
			bError = IMG_TRUE;
			break;
		}
		/* 
		   The relational operators greater than (>), less than (<), greater than or equal (>=), and less
		   than or equal (<=) operate only on scalar integer and scalar floating-point expressions. The
		   result is scalar Boolean. The operands' types must match. To do component-wise
		   comparisons on vectors, use the built-in functions lessThan, lessThanEqual,
		   greaterThan, and greaterThanEqual.
		*/
		case GLSLNT_LESS_THAN:
		case GLSLNT_GREATER_THAN:
		case GLSLNT_LESS_THAN_EQUAL:
		case GLSLNT_GREATER_THAN_EQUAL:
		{
			/* Result is always a bool, regardless of any errors */
			sResultData.sFullySpecifiedType.eTypeSpecifier = GLSLTS_BOOL;

			if (bAllTypeSpecifiersMatch && !bArrayPresent)
			{
				IMG_BOOL bBothFloats = (IMG_BOOL)((asChildFullySpecifiedType[0].eTypeSpecifier == GLSLTS_FLOAT) && 
										(asChildFullySpecifiedType[1].eTypeSpecifier == GLSLTS_FLOAT));
				IMG_BOOL bBothInts   = (IMG_BOOL)(GLSL_IS_INT_SCALAR(asChildFullySpecifiedType[0].eTypeSpecifier) && 
										GLSL_IS_INT_SCALAR(asChildFullySpecifiedType[1].eTypeSpecifier));
				
				if (bBothFloats || bBothInts)
				{
					break;
				}
			}

			/* Report type mismatch */
			ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
			bError = IMG_TRUE;
			break;
		}
		/*
		  The equality operators equal (==), and not equal (!=) operate on all types except arrays*.
		  They result in a scalar Boolean. For vectors, matrices, and structures, all components of the
		  operands must be equal for the operands to be considered equal. To get component-wise
		  equality results for vectors, use the built-in functions equal and notEqual.

		  * - Supported in language version 120 and above
		  */
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
		{
			/* Result is always a bool, regardless of any errors */
			sResultData.sFullySpecifiedType.eTypeSpecifier = GLSLTS_BOOL;

			/* Check for assigning one array to another */
			if ((psGLSLTreeContext->uSupportedLanguageVersion >= 120) && !bArraySizesOK)
			{
				ASTSemReportMismatchedArraySizes(psCPD->psErrorLog, psResultNode, 
												 acDescriptions[0], 
												 acDescriptions[1], 
												 asChildFullySpecifiedType[0].iArraySize, 
												 asChildFullySpecifiedType[1].iArraySize);
				bError = IMG_TRUE;
			}
			else if (!bAllTypeSpecifiersMatch || !bArraySizesOK || 
				GLSL_IS_SAMPLER(asChildFullySpecifiedType[0].eTypeSpecifier) ||
				GLSL_IS_SAMPLER(asChildFullySpecifiedType[1].eTypeSpecifier))
			{
				/* Report type mismatch */
				ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
				bError = IMG_TRUE;
			}

			break;
		}
		/* 
		   The logical binary operators and (&&), or ( | | ), and exclusive or (^^). They operate only
		   on two Boolean expressions and result in a Boolean expression. And (&&) will only
		   evaluate the right hand operand if the left hand operand evaluated to true. Or ( | | ) will only
		   evaluate the right hand operand if the left hand operand evaluated to false. Exclusive or
		   (^^) will always evaluate both operands.
		*/
		case GLSLNT_LOGICAL_OR:
		case GLSLNT_LOGICAL_XOR:
		case GLSLNT_LOGICAL_AND:
		{
			if (asChildFullySpecifiedType[0].eTypeSpecifier != GLSLTS_BOOL || 
				asChildFullySpecifiedType[1].eTypeSpecifier != GLSLTS_BOOL ||
				bArrayPresent)
			{
				/* Report type mismatch */
				ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
				bError = IMG_TRUE;
			}
		
			sResultData.sFullySpecifiedType.eTypeSpecifier = GLSLTS_BOOL;
			break;
		}
		/*
		  The ternary selection operator (?:). It operates on three expressions (exp1 ? exp2 : exp3).
		  This operator evaluates the first expression, which must result in a scalar Boolean. If the
		  result is true, it selects to evaluate the second expression, otherwise it selects to evaluate the
		  third expression. Only one of the second and third expressions is evaluated. The second
		  and third expressions must be the same type, but can be of any type other than an array. The
		  resulting type is the same as the type of the second and third expressions.
		*/
		case GLSLNT_QUESTION:
		{
			if (asChildFullySpecifiedType[0].eTypeSpecifier != GLSLTS_BOOL)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode, "'' : boolean expression expected\n");
				bError = IMG_TRUE;
			}
			
			if (!ASTSemCheckTypeSpecifiersMatch(&asChildFullySpecifiedType[1], &asChildFullySpecifiedType[2]) || bArrayPresent)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode,
									"':' :  wrong operand types  no operation ':' exists that takes a left-hand operand of type '%s' and a right operand of type '%s' (or there is no acceptable conversion)\n",
									acDescriptions[1],
									acDescriptions[2]);
				bError = IMG_TRUE;
			}

			sResultData.sFullySpecifiedType.eTypeSpecifier = asChildFullySpecifiedType[1].eTypeSpecifier;

			break;
		}
		/*
		  Children must be of the same type 
		  Result inherits type
		*/
		case GLSLNT_EQUAL:    
		{
			bAssignmentOperation = IMG_TRUE;

			/* Result is always inherits the type of the left node even if that came from an error */
			sResultData.sFullySpecifiedType.eTypeSpecifier           = asChildFullySpecifiedType[0].eTypeSpecifier;
			sResultData.sFullySpecifiedType.uStructDescSymbolTableID = asChildFullySpecifiedType[0].uStructDescSymbolTableID;
			sResultData.sFullySpecifiedType.iArraySize               = asChildFullySpecifiedType[0].iArraySize;

#ifndef GLSL_ES
			/* Check for assigning one array to another */
			if ((psGLSLTreeContext->uSupportedLanguageVersion >= 120) && !bArraySizesOK)
			{
				/* An array size can be inferred by the size of the initialiser */
				if (bDeclaration && (aeArrayStatus[0] == GLSLAS_ARRAY_SIZE_NOT_FIXED))
				{
					bArraySizesOK = IMG_TRUE;
				}
				else
				{
					ASTSemReportMismatchedArraySizes(psCPD->psErrorLog, psResultNode,
													 acDescriptions[0],
													 acDescriptions[1],
													 asChildFullySpecifiedType[0].iArraySize,
													 asChildFullySpecifiedType[1].iArraySize);
					bError = IMG_TRUE;
					break;
				}
			}
#endif

			if (aeChildLValueStatus[0] != GLSLLV_L_VALUE)
			{
				if (bDeclaration)
				{
					IMG_BOOL bAssignmentOK = (IMG_BOOL)(asChildFullySpecifiedType[0].eTypeQualifier == GLSLTQ_CONST);

#ifndef GLSL_ES
					if ((psGLSLTreeContext->uSupportedLanguageVersion >= 120) && 
						(asChildFullySpecifiedType[0].eTypeQualifier == GLSLTQ_UNIFORM))
					{
						bAssignmentOK = IMG_TRUE;
						
						/* 
						   This constant data needs to get associated with this uniform so we'll signal 
						   both as constants so that ReduceConstantExpression() gets called
						*/
						bAllConstants  = IMG_TRUE;
					}
#endif


					/* Can write to const identifiers at declaration time */
					if (!bAssignmentOK)
					{
						LogProgramNodeError(psCPD->psErrorLog, psResultNode,
											"'%s' :  cannot initialize this type of qualifier\n",
											GLSLTypeQualifierFullDescTable(asChildFullySpecifiedType[0].eTypeQualifier));
						bError = IMG_TRUE;
					}
					/* Check right hand side of expression evaluates to constant */
					else if (asChildFullySpecifiedType[1].eTypeQualifier != GLSLTQ_CONST)
					{
						LogProgramNodeError(psCPD->psErrorLog, psResultNode, 
											"'=' :  assigning non-constant to 'const %s'\n", acDescriptions[1]);
						bError = IMG_TRUE;
					}
				}
				else
				{
					ASTSemReportLValueError(psCPD, psResultNode,
											aeChildLValueStatus[0],
											asChildFullySpecifiedType[0].eTypeQualifier,
											"assign");
					bError = IMG_TRUE;
				}
			}

			/* Do they match exactly */
			if (!bAllTypeSpecifiersMatch || !bArraySizesOK)
			{
				if (bDeclaration && (asChildFullySpecifiedType[0].eTypeQualifier == GLSLTQ_CONST))
				{
					LogProgramNodeError(psCPD->psErrorLog, psResultNode,
										"'const' :  non-matching types for const initializer\n");

					/* 
					   If this was a declaration that had an error remove constant status from the left hand child otherwise 
					   we'll keep getting errors when we try to access the constant data 
					*/
					bAllConstants  = IMG_FALSE;
					
				}
				else
				{
					LogProgramNodeError(psCPD->psErrorLog, psResultNode,
										"'assign' :  cannot convert from '%s' to '%s'\n",
										acDescriptions[1],
										acDescriptions[0]);
				}
				bError = IMG_TRUE;
			}

			break;
		}
		/*
		  The assignments ASTSemCheckTypesAndCalculateResult: modulus into (%=), left shift by (<<=), right shift by (>>=), inclusive or
		  into ( |=), and exclusive or into ( ^=).
		*/
		case GLSLNT_DIV_ASSIGN:
		case GLSLNT_ADD_ASSIGN:
		case GLSLNT_SUB_ASSIGN:
		case GLSLNT_MUL_ASSIGN:
		{
			bAssignmentOperation = IMG_TRUE;

			/* Result is always inherits the type of the left node even if that came from an error */
			sResultData.sFullySpecifiedType.eTypeSpecifier           = asChildFullySpecifiedType[0].eTypeSpecifier;
			sResultData.sFullySpecifiedType.uStructDescSymbolTableID = asChildFullySpecifiedType[0].uStructDescSymbolTableID;
			sResultData.sFullySpecifiedType.iArraySize               = asChildFullySpecifiedType[0].iArraySize;

			if (aeChildLValueStatus[0] != GLSLLV_L_VALUE)
			{
				ASTSemReportLValueError(psCPD, psResultNode,
										aeChildLValueStatus[0],
										asChildFullySpecifiedType[0].eTypeQualifier,
										"assign");
				bError = IMG_TRUE;
			}

			/* Check they are both of the right type */
			if (bAllNumbers && !bArrayPresent)
			{
				if (bAllBaseTypesMatch)
				{
					/* Do they match exactly */
					if (bAllTypeSpecifiersMatch)
					{
						break;
					}
					/* Check for a vector int and a scalar int */
					if (GLSL_IS_INT(asChildFullySpecifiedType[0].eTypeSpecifier) && GLSL_IS_INT_SCALAR(asChildFullySpecifiedType[1].eTypeSpecifier))
					{
						break;
					}
					{
						/* Check for a scalar float and a vector float */
						if (GLSL_IS_FLOAT(asChildFullySpecifiedType[0].eTypeSpecifier) && (asChildFullySpecifiedType[1].eTypeSpecifier == GLSLTS_FLOAT))
						{
							break;
						}
						/* Check for a matrix and a scalar float */
						else if (GLSL_IS_MATRIX(asChildFullySpecifiedType[0].eTypeSpecifier) && (asChildFullySpecifiedType[1].eTypeSpecifier == GLSLTS_FLOAT))
						{
							break;
						}
						/* Is it a multiply? */
						else if (psResultNode->eNodeType == GLSLNT_MUL_ASSIGN)
						{
							/* Check for float vector and matrix of corresponding dimension (for multiplies only) */
							if (GLSL_IS_FLOAT(asChildFullySpecifiedType[0].eTypeSpecifier) && GLSL_IS_MATRIX(asChildFullySpecifiedType[1].eTypeSpecifier))
							{ 
								if (uDimensions[0] == uDimensions[1])
								{
									break;
								}
							}
						}
					}
				}
			}
#if 1
			/* Condtions passed so it must have been an error */
			ASTSemReportWrongOperandTypes(psCPD->psErrorLog, psResultNode, acDescriptions[0], acDescriptions[1]);
#else
			LogProgramNodeError(psCPD->psErrorLog, psResultNode,
								"'assign' :  cannot convert from '%s' to '%s'\n",
								acDescriptions[1],
								acDescriptions[0]);
#endif
			bError = IMG_TRUE;
			break;
		}
		case GLSLNT_RETURN:
		{
			/* 
			   No result to create but must make sure that the return type matches that specified by the 
			   function definition, the return node inherits the symbol ID of the function return value
			*/

			/* Get the symbol data for the current function definition */
			GLSLFunctionDefinitionData *psFunctionDefinitionData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
																					  psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID);

			if ((!psFunctionDefinitionData) || (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION))
			{
				LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Failed to get current function data\n"));
				bError = IMG_TRUE;
				return IMG_FALSE;
			}
			
			if (uNumChildren == 0)
			{
				if (psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID)
				{
					LogProgramNodeError(psCPD->psErrorLog, psResultNode,"'return' : non-void function must return a value\n");
					bError = IMG_TRUE;

					return IMG_FALSE;
				}
				else
				{
					/* Update function return status */
					psGLSLTreeContext->bFunctionReturnStatus = IMG_TRUE;

					return IMG_TRUE;
				}
			}
			else if (psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier == GLSLTS_VOID)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode,"'return' : void function cannot return a value\n");
				bError = IMG_TRUE;
			}
			else if (!ASTSemCheckTypeSpecifiersMatch(&asChildFullySpecifiedType[0], &psFunctionDefinitionData->sReturnFullySpecifiedType))
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode, "'return' : function return is not matching type\n");
				bError = IMG_TRUE;
			}
			else if (asChildFullySpecifiedType[0].iArraySize != psFunctionDefinitionData->sReturnFullySpecifiedType.iArraySize)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode, "'return' : function return has wrong array size\n");
				bError = IMG_TRUE;
			}
			

			psResultNode->uSymbolTableID = psFunctionDefinitionData->uReturnDataSymbolID;

			/* Update function return status */
			psGLSLTreeContext->bFunctionReturnStatus = IMG_TRUE;

			/* no data to generate */
			bGenResultData = IMG_FALSE;
		
			break;
		}
		case GLSLNT_WHILE:
		case GLSLNT_DO:
		case GLSLNT_IF:
		{
	
			/* First node is condition, second is statement */
			if (asChildFullySpecifiedType[0].eTypeSpecifier != GLSLTS_BOOL)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode->ppsChildren[0],
									"'' : boolean expression expected.\n");
				
				bError = IMG_TRUE;
				return IMG_FALSE;
			}

			return IMG_TRUE;

		}
		case GLSLNT_BREAK:
		case GLSLNT_CONTINUE:
		{
			/* Nothing to do I think */
			return IMG_TRUE;
		}
		case GLSLNT_DISCARD:
		{
			if (psGLSLTreeContext->eProgramType != GLSLPT_FRAGMENT)
			{
				LogProgramNodeError(psCPD->psErrorLog, psResultNode, "'discard' : only supported in fragment programs\n");

				return IMG_FALSE;
			}

			psGLSLTreeContext->bDiscardExecuted = IMG_TRUE;

			return IMG_TRUE;
		}
		default:
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Failed to recognise node type\n"));
			bError = IMG_TRUE;
		}
	}

	/* 
	   Check for writes to inbuilt varyings and check if writes of constant data to temps
	   can be stored for later optimisations 
	*/
	if (bAssignmentOperation)
	{
		if (psResultNode->ppsChildren[0]->eNodeType != GLSLNT_ERROR && !bError)
		{
			ASTSemCheckAssignment(psGLSLTreeContext, psResultNode);
		}
	}

	/* Get the precision of the result */
	if (!bError)
	{
		uNumChildrenRequiringPrecisionConstructor = ASTSemGetResultPrecision(psGLSLTreeContext,
																			 uNumChildren,
																			 psResultNode,
																			 &sResultData.sFullySpecifiedType,
																			 asChildFullySpecifiedType,
																			 auPrecisionConversionChildren,
																			 &ePrecisionToConvertTo);
	}

	/* 
	   If all are constants we can try and reduce the expression 
	*/
	if (bAllConstants && !bError)
	{
		/* Try and reduce the expression */
		if (ASTSemReduceConstantExpression(psGLSLTreeContext,
										   psResultNode, 
										   sResultData.sFullySpecifiedType.ePrecisionQualifier))
		{
			/* No precision conversions or result data required if reduction was successful */
			uNumChildrenRequiringPrecisionConstructor = 0;
			bGenResultData = IMG_FALSE;
		}
	}

	/* Insert any precision constructors */
	if (!bError)
	{
		for (i = 0; i < uNumChildrenRequiringPrecisionConstructor; i++)
		{
			ASTSemInsertConstructor(psGLSLTreeContext,
									psResultNode,
									auPrecisionConversionChildren[i],
									ePrecisionToConvertTo,
									GLSLTS_INVALID,
									IMG_FALSE);
		}
	}

	if (!bError && !uNumErrorNodes)
	{
		/* Check and update any array sizes if appropriate */
		ASTSemCheckAndUpdateArraySizes(psGLSLTreeContext,
									   psResultNode,
									   bDeclaration,
									   asChildFullySpecifiedType,
									   aeArrayStatus);
	}

	/* Update the usage flags as appropriate */
	ASTUpdateIdentifierUsage(psGLSLTreeContext, psResultNode, bAssignmentOperation, bDeclaration, bError);

	if (bGenResultData)
	{

		/* Get a unique name for this result */
		ASTSemGetResultName(psGLSLTreeContext, acResultName, &(sResultData.sFullySpecifiedType));
		
#ifdef EXTRA_DEBUG
		/* Quick sanity check on precision of result */
		if (!bError && 
			GLSL_IS_NUMBER(sResultData.sFullySpecifiedType.eTypeSpecifier) &&
			sResultData.sFullySpecifiedType.ePrecisionQualifier == GLSLPRECQ_UNKNOWN)
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Adding numeric result '%s' without precision for node '%s'\n",
								 acResultName,
								 NODETYPE_DESC(psResultNode->eNodeType)));
		}
#endif

		/* Add the data to the symbol table */
		if (!AddResultData(psCPD, psGLSLTreeContext->psSymbolTable,
						   acResultName,
						   &sResultData,
						   IMG_FALSE,
						   &(psResultNode->uSymbolTableID)))
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Failed to add result %s to table\n", acResultName));
		}
		
		/* Sanity check on any created structures */
		if ((sResultData.sFullySpecifiedType.eTypeSpecifier == GLSLTS_STRUCT) &&
			(sResultData.sFullySpecifiedType.uStructDescSymbolTableID == 0))
		{
			LOG_INTERNAL_NODE_ERROR((psResultNode,"ASTSemCheckTypesAndCalculateResult: Node type %d had bad struct info\n", psResultNode->eNodeType));
		}
	}

	/* Free any mem allocated */
	if (sResultData.pvConstantData)
	{
		DebugMemFree(sResultData.pvConstantData);
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ASTSemAddIdentifierToSymbolTable
 *
 * Inputs       : psSymbolTable
                  psIDENTIFIEREntry
                  pszIdentifierName
                  bReservedName
                  eBuiltInVariableID
                  eIdentifierUsage
                  uConstantDataSize
                  pvConstantData
                  eProgramType
 * Outputs      : -
 * Returns      : The symbol table ID of the inserted symbol if successfull. Zero otherwise.
 * Description  : Inserts an identifier in the symbol table and performs semantic checks.
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 ASTSemAddIdentifierToSymbolTable(GLSLCompilerPrivateData *psCPD,
										    GLSLTreeContext		   *psGLSLTreeContext,
											SymTable               *psSymbolTable,
											ParseTreeEntry         *psIDENTIFIEREntry,
											IMG_CHAR               *pszIdentifierName,
											GLSLFullySpecifiedType *psFullySpecifiedType,
											IMG_BOOL                bReservedName,
											GLSLBuiltInVariableID   eBuiltInVariableID,
											GLSLIdentifierUsage     eIdentifierUsage,
											IMG_UINT32              uConstantDataSize,
											IMG_VOID               *pvConstantData /*uConstantDataSize bytes*/,
											GLSLProgramType         eProgramType)
{
	GLSLIdentifierData sIdentifierData;

	IMG_UINT32 uSymbolTableID;
	IMG_BOOL bShaderInOrOut;

	PVR_UNREFERENCED_PARAMETER(psGLSLTreeContext);

	/* Check to see if this name exists already at the current scope */
	if (FindSymbol(psSymbolTable, pszIdentifierName, &uSymbolTableID, IMG_TRUE))
	{
		/* Arrays can be redeclared if the first declaration did not contain an array size */
		if (psFullySpecifiedType->iArraySize)
		{
			GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psSymbolTable, uSymbolTableID);

			if (psIdentifierData->eSymbolTableDataType != GLSLSTDT_IDENTIFIER)
			{
				LOG_INTERNAL_PARSE_TREE_ERROR((psIDENTIFIEREntry,"ASTAddIdentifierToSymbolTable: Name exists but type is not IDENTIFIER\n"));

				return 0;
			}

			/* Check array status */
			if (psIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
										 "'%s' : declaring non-array as array\n",
										 pszIdentifierName);
				return 0;
			}
			/* Has array size already been fixed? */
			else if (psIdentifierData->eArrayStatus == GLSLAS_ARRAY_SIZE_FIXED)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
										 "'%s' : redeclaration of array with size\n", 
										 pszIdentifierName);
				return 0;
			}
			/* Array size can be modified, check new value is valid */
			else if (psIdentifierData->eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
			{
				/* Check types match */
				if (psIdentifierData->sFullySpecifiedType.eTypeSpecifier != psFullySpecifiedType->eTypeSpecifier)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
											 "'%s' : redeclaration of array with different type\n", 
											 pszIdentifierName);
					return 0;
				}
				/* Check if new array size is large enough */
				else if (psFullySpecifiedType->iArraySize < psIdentifierData->sFullySpecifiedType.iArraySize)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
											 "'%s' : higher index value already used for the array\n", 
											 pszIdentifierName);
					return 0;
				}
				else if (psFullySpecifiedType->iArraySize != -1)
				{
					/* fix the array size */
					psIdentifierData->sFullySpecifiedType.iArraySize = psFullySpecifiedType->iArraySize;

					psIdentifierData->eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;

					return uSymbolTableID;
				}
				else
				{
					/* Uh, just another declaration without specifying its size */
					return uSymbolTableID;
				}
			}
			else
			{
				LOG_INTERNAL_PARSE_TREE_ERROR((psIDENTIFIEREntry,"ASTAddIdentifierToSymbolTable: Unrecognised array status\n"));
				
				return 0;
			}
		}
		else
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, "'%s' : redefinition\n", pszIdentifierName);

			return 0;
		}
	}

	if (!bReservedName)
	{
		/* Check if it's using a reserved name */
		if (strncmp(pszIdentifierName,"gl_", 3) == 0)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, "'gl_' : reserved built-in name\n");
			
			return 0;
		}
	}


	/* Setup the identifier entry */
	sIdentifierData.eSymbolTableDataType         = GLSLSTDT_IDENTIFIER;
	sIdentifierData.sFullySpecifiedType          = *psFullySpecifiedType;
	sIdentifierData.iActiveArraySize             = -1;
	sIdentifierData.eBuiltInVariableID           = eBuiltInVariableID;
	sIdentifierData.eIdentifierUsage             = eIdentifierUsage;
	sIdentifierData.uConstantDataSize            = uConstantDataSize;
	sIdentifierData.pvConstantData               = pvConstantData;
	sIdentifierData.uConstantAssociationSymbolID = 0;


	/*
	  Attribute
	  The attribute qualifier is used to declare variables that are passed to a vertex shader from OpenGL on a
	  per-vertex basis. It is an error to declare an attribute variable in any type of shader other than a vertex
	  shader. Attribute variables are read-only as far as the vertex shader is concerned. Values for attribute
	  variables are passed to a vertex shader through the OpenGL vertex API or as part of a vertex array. They
	  convey vertex attributes to the vertex shader and are expected to change on every vertex shader run. The
	  attribute qualifier can be used only with the data types float, vec2, vec3, vec4, mat2, mat3, and mat4.
	  Attribute variables cannot be declared as arrays or structures.
	*/

	bShaderInOrOut = psFullySpecifiedType->eTypeQualifier == GLSLTQ_VERTEX_IN || 
		psFullySpecifiedType->eTypeQualifier == GLSLTQ_VERTEX_OUT || 
		psFullySpecifiedType->eTypeQualifier == GLSLTQ_FRAGMENT_IN
		;

	if (bShaderInOrOut)
	{
		if(!(GLSLTypeSpecifierBaseTypeTable(psFullySpecifiedType->eTypeSpecifier) == GLSLTS_FLOAT))
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
									 "'%s' : may only be a float, floating-point vector, or matrix\n",
									 GLSLTypeQualifierFullDescTable(psFullySpecifiedType->eTypeQualifier));
			
			return 0;
		}
		if (psFullySpecifiedType->eTypeQualifier == GLSLTQ_VERTEX_IN)
		{
			if (eProgramType != GLSLPT_VERTEX)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
										 "'%s' :  supported in vertex shaders only\n",
										 GLSLTypeQualifierFullDescTable(psFullySpecifiedType->eTypeQualifier));
				return 0;
			}
		
			if (psFullySpecifiedType->iArraySize != 0)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
										 "'%s %s' : cannot declare arrays of this type\n",
										 GLSLTypeQualifierFullDescTable(psFullySpecifiedType->eTypeQualifier),
										 GLSLTypeSpecifierFullDescTable(psFullySpecifiedType->eTypeSpecifier));

				return 0;
			}
		}
	}

	if (bShaderInOrOut || psFullySpecifiedType->eTypeQualifier == GLSLTQ_UNIFORM)
		
	{
		if (GetScopeLevel(psSymbolTable) != 0)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
									 "'%s' : only allowed at global scope\n",
									 GLSLTypeQualifierFullDescTable(psFullySpecifiedType->eTypeQualifier));
			return 0;
		}
	}

	if (psFullySpecifiedType->eTypeSpecifier == GLSLTS_VOID)
	{
		LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
								 "'%s' : illegal use of type 'void'\n",
								 pszIdentifierName);
		return 0;
	}


	if (psFullySpecifiedType->iArraySize == 0)
	{
		sIdentifierData.eArrayStatus = GLSLAS_NOT_ARRAY;
	}
	else if (psFullySpecifiedType->iArraySize == -1)
	{
		sIdentifierData.eArrayStatus = GLSLAS_ARRAY_SIZE_NOT_FIXED;
	}
	else if (psFullySpecifiedType->iArraySize > 0)
	{
		sIdentifierData.eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;
	}
	else
	{
		LOG_INTERNAL_PARSE_TREE_ERROR((psIDENTIFIEREntry,"ASTAddIdentifierToSymbolTable: Invalid array size\n"));

		return 0;
	}

	/* Calculate l-value status */
	if (psFullySpecifiedType->eTypeQualifier == GLSLTQ_CONST		||
		  psFullySpecifiedType->eTypeQualifier == GLSLTQ_VERTEX_IN	||
		  psFullySpecifiedType->eTypeQualifier == GLSLTQ_UNIFORM	||
		  psFullySpecifiedType->eTypeQualifier == GLSLTQ_FRAGMENT_IN)
	{
		sIdentifierData.eLValueStatus = GLSLLV_NOT_L_VALUE;
	}
	else
	{
		sIdentifierData.eLValueStatus = GLSLLV_L_VALUE;
	}

	/* Add the data to the symbol table */
	if (!AddIdentifierData(psCPD, psSymbolTable,
						   pszIdentifierName,
						   &sIdentifierData,
						   IMG_FALSE,
						   &uSymbolTableID))
	{
		LOG_INTERNAL_PARSE_TREE_ERROR((psIDENTIFIEREntry,"ASTAddIdentifierToSymbolTable: Failed to add IDENTIFIER %s to symbol table", pszIdentifierName));

		return 0;
	}

	return uSymbolTableID;

}

/******************************************************************************
 End of file (semantic.c)
******************************************************************************/

