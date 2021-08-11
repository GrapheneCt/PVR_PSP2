/******************************************************************************
 * Name         : semantic.h
 * Author       : James McCarthy
 * Created      : 27/07/2004
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
 * $Log: semantic.h $
 *****************************************************************************/

#ifndef __gl_semantic_h_
#define __gl_semantic_h_

#include "img_types.h"

#include "glsltree.h"

IMG_BOOL ASTSemCheckTypesAndCalculateResult(GLSLTreeContext *psGLSLTreeContext,
											GLSLNode        *psResultNode,
											IMG_BOOL         bDeclaration);

IMG_UINT32 ASTSemAddIdentifierToSymbolTable(GLSLCompilerPrivateData *psCPD,
											GLSLTreeContext		   *psGLSLTreeContext,
											SymTable               *psSymbolTable,
											ParseTreeEntry         *psIDENTIFIEREntry,
											IMG_CHAR               *pszIdentifierName,
											GLSLFullySpecifiedType *psFullySpecifiedType,
											IMG_BOOL                bReservedName,
											GLSLBuiltInVariableID   eBuiltInVariableID,
											GLSLIdentifierUsage     eIdentifierUsage,
											IMG_UINT32              uConstantDataSize,
											IMG_VOID               *pvConstantData,
											GLSLProgramType         eProgramType);

IMG_BOOL ASTSemGetArraySize(GLSLTreeContext *psGLSLTreeContext,
							GLSLNode        *psConstantExpressionNode,
							IMG_INT32       *piArraySize);

IMG_CHAR *ASTSemCreateHashedFunctionName(SymTable               *psSymbolTable,
										IMG_CHAR               *pszFunctionName,
										IMG_UINT32              uNumParameters,
										GLSLFullySpecifiedType *psFullySpecifiedType);

IMG_BOOL ASTSemCheckTypeSpecifiersMatch(const GLSLFullySpecifiedType* psType1,
										const GLSLFullySpecifiedType* psType2);

IMG_UINT32 ASTSemGetSizeInfo(GLSLTreeContext        *psGLSLTreeContext,
							 GLSLFullySpecifiedType *psFullySpecifiedType,
							 IMG_BOOL                bUseArraySize);

IMG_BOOL ASTUpdateConditionalIdentifierUsage(GLSLTreeContext *psGLSLTreeContext,
											 GLSLNode        *psNode);


#endif // __gl_semantic_h_
