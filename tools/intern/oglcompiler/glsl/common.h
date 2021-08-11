/**************************************************************************
 * Name         : common.h
 * Author       : James McCarthy
 * Created      : 20/09/2004
 *
 * Copyright    : 2004 by Imagination Technologies Limited. All rights reserved.
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
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: common.h $
 **************************************************************************/

IMG_BOOL ASTValidateNodeCreationFn(GLSLCompilerPrivateData *psCPD, 
									GLSLNode   *psCurrentNode,
									IMG_UINT32 uLineNumber, 
									IMG_CHAR   *pszFileName);

GLSLNode *ASTCreateNewNodeFn(GLSLTreeContext		*psGLSLTreeContext,
								GLSLNodeType		eNodeType,
								ParseTreeEntry		*psParseTreeEntry,
								IMG_UINT32			uLineNumber,
								IMG_CHAR			*pszFileName);

IMG_BOOL ASTAddNodeChildFn(GLSLCompilerPrivateData *psCPD,
						   GLSLNode     *psCurrentNode,
						   GLSLNode     *psChildNode, 
						   IMG_UINT32    uLineNumber, 
						   IMG_CHAR     *pszFileName);

#define GET_CPD_FROM_AST(treeContext) ((GLSLCompilerPrivateData*)treeContext->psInitCompilerContext->pvCompilerPrivateData)

#define ASTCreateNewNode(a, b) ASTCreateNewNodeFn(psGLSLTreeContext, a, b, __LINE__, __FILE__)

#define ASTAddNodeChild(a, b) ASTAddNodeChildFn(GET_CPD_FROM_AST(psGLSLTreeContext), a, b, __LINE__, __FILE__)

#define ASTValidateNodeCreation_ret(a,b) if (!ASTValidateNodeCreationFn(GET_CPD_FROM_AST(psGLSLTreeContext), \
											a, __LINE__, __FILE__)) \
										{ return b; }

#define ASTValidateNodeCreation(a)  ASTValidateNodeCreation_ret(a,IMG_NULL)



IMG_BOOL AddFunctionDefinitionData(GLSLCompilerPrivateData    *psCPD,
								   SymTable                   *psSymbolTable,
								   IMG_CHAR                   *pszName, 
								   GLSLFunctionDefinitionData *psFunctionDefinitionData,
								   IMG_BOOL                    bAllowDuplicates, 
								   IMG_UINT32                 *puSymbolID);

IMG_BOOL AddParameterDatafn(GLSLCompilerPrivateData *psCPD,
							SymTable           *psSymbolTable,
							IMG_CHAR           *pszName, 
							GLSLIdentifierData *psIdentifierData,
							IMG_BOOL            bAllowDuplicates, 
							IMG_UINT32         *puSymbolID,
							IMG_CHAR           *pszFileName,
							IMG_UINT32          uLineNumber);

IMG_BOOL AddIdentifierDatafn(GLSLCompilerPrivateData *psCPD,
							 SymTable          *psSymbolTable,
							 IMG_CHAR           *pszName, 
							 GLSLIdentifierData *psIdentifierData,
							 IMG_BOOL            bAllowDuplicates, 
							 IMG_UINT32         *puSymbolID,
							 IMG_CHAR           *pszFileName,
							 IMG_UINT32          uLineNumber);

IMG_BOOL AddResultDatafn(GLSLCompilerPrivateData *psCPD,
						 SymTable           *psSymbolTable,
						 IMG_CHAR           *pszName, 
						 GLSLIdentifierData *psResultData,
						 IMG_BOOL            bAllowDuplicates, 
						 IMG_UINT32         *puSymbolID,
						 IMG_CHAR           *pszFileName,
						 IMG_UINT32          uLineNumber);

#define AddParameterData(a, b, c, d, e, f)  AddParameterDatafn(a, b, c, d, e, f, __FILE__, __LINE__)
#define AddResultData(a, b, c, d, e, f)     AddResultDatafn(a, b, c, d, e, f, __FILE__, __LINE__)
#define AddIdentifierData(a, b, c, d, e, f) AddIdentifierDatafn(a, b, c, d, e, f, __FILE__, __LINE__)

IMG_BOOL AddFunctionCallData(GLSLCompilerPrivateData *psCPD,
							 SymTable             *psSymbolTable,
							 IMG_CHAR             *pszName, 
							 GLSLFunctionCallData *psFunctionCallData,
							 IMG_BOOL              bAllowDuplicates, 
							 IMG_UINT32           *puSymbolID);

IMG_BOOL AddSwizzleData(GLSLCompilerPrivateData *psCPD,
						SymTable        *psSymbolTable,
						IMG_CHAR        *pszName, 
						GLSLSwizzleData *psSwizzleData,
						IMG_BOOL         bAllowDuplicates, 
						IMG_UINT32      *puSymbolID);

IMG_BOOL AddMemberSelectionData(GLSLCompilerPrivateData *psCPD,
								SymTable                *psSymbolTable,
								IMG_CHAR                *pszName, 
								GLSLMemberSelectionData *psMemberSelectionData,
								IMG_BOOL                 bAllowDuplicates, 
								IMG_UINT32              *puSymbolID);

IMG_BOOL AddBoolConstant(GLSLCompilerPrivateData *psCPD,
						 SymTable				*psSymbolTable, 
						 IMG_BOOL				bData, 
						 GLSLPrecisionQualifier ePrecisionQualifier,
						 IMG_BOOL				bAllowDuplicates, 
						 IMG_UINT32				*puSymbolTableID);

IMG_BOOL AddIntConstant(GLSLCompilerPrivateData *psCPD,
						SymTable				*psSymbolTable, 
						IMG_INT32				iData, 
						GLSLPrecisionQualifier	ePrecisionQualifier,
						IMG_BOOL				bAllowDuplicates, 
						IMG_UINT32				*puSymbolTableID);

IMG_BOOL AddFloatConstant(GLSLCompilerPrivateData *psCPD,
						  SymTable					*psSymbolTable, 
						  IMG_FLOAT					fData, 
						  GLSLPrecisionQualifier	ePrecisionQualifier,
						  IMG_BOOL					bAllowDuplicates, 
						  IMG_UINT32				*puSymbolTableID);

IMG_BOOL AddFloatVecConstant(GLSLCompilerPrivateData *psCPD,
							 SymTable				*psSymbolTable, 
							 IMG_CHAR				*pszName, 
							 IMG_FLOAT				*pfData,
							 IMG_UINT32				uNumComponents,
							 GLSLPrecisionQualifier ePrecisionQualifier,
							 IMG_BOOL				bAllowDuplicates, 
							 IMG_UINT32				*puSymbolTableID);

IMG_BOOL AddBuiltInIdentifier(GLSLCompilerPrivateData *psCPD,
							  SymTable               *psSymbolTable,
							  IMG_CHAR				*pszName,
							  IMG_INT32               iArraySize,
							  GLSLBuiltInVariableID	eBuiltInVariableID,
							  GLSLTypeSpecifier		eTypeSpecifier,
							  GLSLTypeQualifier		eTypeQualifier,
							  GLSLPrecisionQualifier ePrecisionQualifier,
							  IMG_UINT32			*puSymbolID);

IMG_BOOL AddStructureDefinition(GLSLCompilerPrivateData *psCPD,
								SymTable                    *psSymbolTable,
								ParseTreeEntry              *psParseTreeEntry,
								IMG_CHAR                    *pszStructureName,
								GLSLStructureDefinitionData *psStructureDefinitionData,
								IMG_UINT32                  *puSymbolTableID);

IMG_BOOL GetSymbolInfofn(GLSLCompilerPrivateData *psCPD,
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
						 IMG_UINT32               uLineNumber);

#define GetSymbolInfo(a, b, c, d, e, f, g, h, i, j, k) GetSymbolInfofn(a, b, c, d, e, f, g, h, i, j, k, __FILE__, __LINE__) 

IMG_VOID *GetSymbolTableDatafn(GLSLCompilerPrivateData *psCPD,
							   SymTable                *psSymbolTable,
							   IMG_UINT32               uSymbolTableID,
							   IMG_BOOL                 bCheckSymbolTableDataType,
							   GLSLSymbolTableDataType  eExpectedSymbolTableDataType,
							   IMG_CHAR                *pszFileName,
							   IMG_UINT32               uLineNumber);

#define GetSymbolTableData(a, b) GetSymbolTableDatafn(psCPD, a, b, IMG_FALSE, (GLSLSymbolTableDataType)0, __FILE__, __LINE__)

#define GetAndValidateSymbolTableData(a, b, c) GetSymbolTableDatafn(psCPD, a, b, IMG_TRUE, c, __FILE__, __LINE__)

#ifdef DUMP_LOGFILES
IMG_VOID DumpIdentifierInfo(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID);
#endif
