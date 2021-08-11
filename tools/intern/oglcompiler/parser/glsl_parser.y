%{
/*************************************************************************
 * Name		  : glsl_parser.y
 * Title	  : Parser of GLSL and GLSL ES shaders
 * Author	  : Michael Rennie
 * Created	  : Sept 2007
 *
 * Copyright  : 2002-2007 by Imagination Technologies Limited. All rights reserved.
 *			  : No part of this software, either material or conceptual 
 *			  : may be copied or distributed, transmitted, transcribed,
 *			  : stored in a retrieval system or translated into any 
 *			  : human or computer language in any form by any means,
 *			  : electronic, mechanical, manual or other-wise, or 
 *			  : disclosed to third parties without the express written
 *			  : permission of Imagination Technologies Limited, Unit 8, HomePark
 *			  : Industrial Estate, King's Langley, Hertfordshire,
 *			  : WD4 8LZ, U.K.
 *
 *
 * Modifications:-
 * $Log: glsl_parser.y $
 **************************************************************************/

#include <string.h>

#include "parser.h"
#include "glsltree.h"
#include "semantic.h"
#include "icgen.h"
#include "error.h"
#include "common.h"

#ifdef __linux__
/* We cannot attach IMG_INTERNAL to glsl_parse, so we make everything here IMG_INTERNAL. */
#pragma GCC visibility push(hidden)
#endif

#if defined(_MSC_VER)
#pragma warning (disable:4131)
#pragma warning (disable:4244)
#pragma warning (disable:4701)
#pragma warning (disable:4127)
#pragma warning (disable:4102)
#pragma warning (disable:4706)
#pragma warning (disable:4702)
#endif /* defined(_MSC_VER) */

#ifdef DEBUG
/* We dont really use bison's error messages, but they can be useful for debugging. */
//#define YYDEBUG 1
#define YYERROR_VERBOSE 1 
#endif

/* Define YYMALLOC and YYFREE, so that all bison's allocs go through the oglcompiler's standard routines. */
#define YYMALLOC DebugMemAlloc
#define YYFREE DebugMemFree

typedef struct __TypeQualifierTAG
{
	GLSLTypeQualifier					eTypeQualifier;
	GLSLVaryingModifierFlags			eVaryingModifierFlags;
} __TypeQualifier;

typedef struct __InitDeclaratorListTAG
{
	GLSLNode							*psNode;
	GLSLFullySpecifiedType				*psFullySpecifiedType;
	IMG_BOOL							bInvarianceModifier;
} __InitDeclaratorList;

typedef struct __SingleDeclarationTAG
{
	GLSLNode							*psNode;
	GLSLFullySpecifiedType				*psFullySpecifiedType;
	IMG_BOOL							bInvarianceModifier;
} __SingleDeclaration;

typedef struct __FunctionDefinitionPrototypeTAG
{
	GLSLNode							*psNode;
	ASTFunctionState					*psFunctionState;
} __FunctionDefinitionPrototype;

typedef struct __SelectionRestStatementTAG
{
	GLSLNode							*psIfNode;
	GLSLNode							*psElseNode;
} __SelectionRestStatement;

typedef struct _YYSTYPETAG
{
	Token* psToken;
	union
	{
		/* The parser stores a linked list of its allocs, which is cleared when the 
		   parser context is destroyed. This is needed because if the parsing fails
		   then bison aborts and it is difficult to track the allocated memory. */
		#define PARSER_ALLOC(ptr) {\
			IMG_VOID** pAlloc = DebugMemAlloc(sizeof(*ptr) + sizeof(IMG_VOID*)); \
			pAlloc[0] = psParseContext->pvBisonAllocList; \
			psParseContext->pvBisonAllocList = pAlloc; \
			*(void**)&(ptr) = (IMG_VOID*)(pAlloc + 1); }
		#define UNION_ALLOC(ss, member) {\
			PARSER_ALLOC((ss).u.member); }
			
		GLSLNode						*psNode;
		GLSLFullySpecifiedType			*psFullySpecifiedType;			/* type_specifier_no_prec */
		GLSLTypeSpecifier				eTypeSpecifier;					/* type_specifier_e */
		GLSLPrecisionQualifier			ePrecisionQualifier;			/* precision_qualifier */
		__TypeQualifier					*psTypeQualifier;				/* type_qualifier */
		ASTFunctionCallState			*psFunctionCallState;			/* function_call_generic */
		ASTFunctionState				*psFunctionState;				/* function_header */
		__InitDeclaratorList			*psInitDeclaratorList;			/* init_declarator_list */
		__SingleDeclaration				*psSingleDeclaration;			/* single_declaration */
		struct __StructDeclaratorTAG	*psStructDeclarator;			/* struct_declarator */
		struct __StructDeclarationTAG	*psStructDeclaration;			/* struct_declaration */
		IMG_UINT32						uStructDescSymbolTableID;		/* struct_specifier */
		GLSLIdentifierData				*psParameterDeclaration;		/* parameter_declaration */
		struct __ParameterDeclaratorTAG	*psParameterDeclarator;			/* parameter_declarator */
		GLSLParameterQualifier			eParameterQualifier;			/* parameter_qualifier */
		GLSLTypeQualifier				eTypeQualifier;					/* parameter_type_qualifier */
		__FunctionDefinitionPrototype	*psFunctionDefinitionPrototype;	/* function_definition_prototype */
		__SelectionRestStatement		*psSelectionRestStatement;		/* selection_rest_statement */
		GLSLVaryingModifierFlags		eVaryingModifierFlags;			/* invariant_qualifier_opt &  centroid_qualifier_opt */
	} u;

} _YYSTYPE;
#define YYSTYPE _YYSTYPE

typedef struct __StructDeclaratorTAG
{
	Token								*psIdentifierToken;
	YYSTYPE								sArraySpecifier;
	struct __StructDeclaratorTAG		*psNext;
} __StructDeclarator;

typedef struct __StructDeclarationTAG
{
	GLSLFullySpecifiedType				*psFullySpecifiedType;
	__StructDeclarator					*psDeclarators;
	struct __StructDeclarationTAG		*psNext;
} __StructDeclaration;

typedef struct __ParameterDeclaratorTAG
{
	GLSLFullySpecifiedType				*psFullySpecifiedType;
	Token								*psIdentifierToken;
	YYSTYPE								sArraySpecifier;
} __ParameterDeclarator;



/* Lexer function (yylex) */
static IMG_INT32 glsl_lex (YYSTYPE *lvalp, ParseContext *psParseContext)
{
	TokenName eRet;
	
	lvalp->psToken = &psParseContext->psTokenList[psParseContext->uCurrentToken++];
	
	eRet = lvalp->psToken->eTokenName;
	
#if 0	/* Get rid of TOK_TERMINATEPARSING someday. */
	if(psParseContext->uCurrentToken == psParseContext->uNumTokens)
		return 0;/* No more tokens left. */
#else
	if(eRet == TOK_TERMINATEPARSING)
		return 0;/* No more tokens left. */
#endif

	return eRet;
}

#define psCPD (GET_CPD_FROM_AST(psGLSLTreeContext))

static IMG_BOOL CheckTypeName(GLSLTreeContext *psGLSLTreeContext, IMG_CHAR *pszTokeName)
{
	IMG_CHAR acName[300];
	IMG_UINT32 uSymbolTableID;
	IMG_BOOL bRet;
	GLSLGenericData *psGenericData;
	
	/* The only TOK_TYPE_NAME currently supported is struct type name */
	sprintf(acName, "%s@struct_def",  pszTokeName);
	bRet = FindSymbol(psGLSLTreeContext->psSymbolTable, acName, &uSymbolTableID, IMG_FALSE);
	if (!bRet)
	{
		return IMG_FALSE;
	}
	psGenericData = (GLSLGenericData *)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uSymbolTableID);
	if (psGenericData != IMG_NULL)
	{
		if (psGenericData->eSymbolTableDataType == GLSLSTDT_STRUCTURE_DEFINITION)
		{
			return IMG_TRUE;
		}
	}
	
	return IMG_FALSE;
}

/* Error function (yyerror) */
static IMG_VOID glsl_error (ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext, char const *error)
{
	PVR_UNREFERENCED_PARAMETER(psGLSLTreeContext);
	PVR_UNREFERENCED_PARAMETER(error);
	
	if (psParseContext->uCurrentToken >= psParseContext->uNumTokens)
	{
		LogProgramParserError(psCPD->psErrorLog, IMG_NULL, "Unexpected end of source found\n");
	}
	else
	{
		Token *psToken = &psParseContext->psTokenList[psParseContext->uCurrentToken - 1];

		LogProgramParserError(psCPD->psErrorLog, psToken,
								 "'%s' : syntax error;\n",
								 psToken->pvData);
	}
}



/* This has to be used every time a type_specifier is used because it sets up the default precision of a type. 
   It cannot not be called inside the type_specifier handler because it checks that samplers are uniform or parameters, 
   and uniform is specified by the type_qualifier, which is outside the type_specifier. */
static IMG_VOID __CheckTypeSpecifier(GLSLTreeContext *psGLSLTreeContext, YYSTYPE *ss)
{
	/* Check for correct setup of precision */
	if (GLSL_IS_NUMBER(ss->u.psFullySpecifiedType->eTypeSpecifier) || GLSL_IS_SAMPLER(ss->u.psFullySpecifiedType->eTypeSpecifier))
	{		
		IMG_BOOL bUserDefinedPrecision = ss->u.psFullySpecifiedType->ePrecisionQualifier != GLSLPRECQ_UNKNOWN;
		
		/* Get a default precision if none was specified */
		if (!bUserDefinedPrecision)
		{
			/* This may return unknown precision if default precision has not been set */
			if (GLSL_IS_INT(ss->u.psFullySpecifiedType->eTypeSpecifier))
				ss->u.psFullySpecifiedType->ePrecisionQualifier = psGLSLTreeContext->eDefaultIntPrecision;
			else if (GLSL_IS_FLOAT(ss->u.psFullySpecifiedType->eTypeSpecifier) || GLSL_IS_MATRIX(ss->u.psFullySpecifiedType->eTypeSpecifier))
				ss->u.psFullySpecifiedType->ePrecisionQualifier = psGLSLTreeContext->eDefaultFloatPrecision;
			else if (GLSL_IS_SAMPLER(ss->u.psFullySpecifiedType->eTypeSpecifier))
				ss->u.psFullySpecifiedType->ePrecisionQualifier = psGLSLTreeContext->eDefaultSamplerPrecision;
		}

		/* Raise error if precision is still unknown */
		if (ss->u.psFullySpecifiedType->ePrecisionQualifier == GLSLPRECQ_UNKNOWN)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, ss->psToken, 
									 "'%s' : No precision defined for this type\n", 
									 GLSLTypeSpecifierDescTable(ss->u.psFullySpecifiedType->eTypeSpecifier));
		}

		/* Are we forcing any overrides on user defined precisions? */
		if (bUserDefinedPrecision)
		{
			GLSLPrecisionQualifier eForcedPrecision = GLSLPRECQ_UNKNOWN; 
			/* This may return unknown precision if default precision has not been set */
			if (GLSL_IS_INT(ss->u.psFullySpecifiedType->eTypeSpecifier))
				eForcedPrecision = psGLSLTreeContext->eForceUserIntPrecision;
			else if (GLSL_IS_FLOAT(ss->u.psFullySpecifiedType->eTypeSpecifier) || GLSL_IS_MATRIX(ss->u.psFullySpecifiedType->eTypeSpecifier))
				eForcedPrecision = psGLSLTreeContext->eForceUserFloatPrecision;
			else if (GLSL_IS_SAMPLER(ss->u.psFullySpecifiedType->eTypeSpecifier))
				eForcedPrecision = psGLSLTreeContext->eForceUserSamplerPrecision;
			
			if (eForcedPrecision != GLSLPRECQ_UNKNOWN)
			{
				ss->u.psFullySpecifiedType->ePrecisionQualifier = eForcedPrecision;
			}
		}

		/* Check that samplers are declared as uniform or function parameters. */
		if (GLSL_IS_SAMPLER(ss->u.psFullySpecifiedType->eTypeSpecifier) && 
			ss->u.psFullySpecifiedType->eTypeQualifier != GLSLTQ_UNIFORM &&
			ss->u.psFullySpecifiedType->eParameterQualifier == GLSLPQ_INVALID)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, ss->psToken,
									 "'%s' : samplers must be uniform or a function parameter\n", 
									 GLSLTypeSpecifierDescTable(ss->u.psFullySpecifiedType->eTypeSpecifier));
		}
	}
	/* Check for precision on invalid types */
	else if (ss->u.psFullySpecifiedType->ePrecisionQualifier != GLSLPRECQ_UNKNOWN)
	{
		LogProgramParseTreeError(psCPD->psErrorLog, ss->psToken,
								 "'%s' : Precision not valid for this type\n",
								 GLSLTypeSpecifierDescTable(ss->u.psFullySpecifiedType->eTypeSpecifier));

		/* Set it to unknown */
		ss->u.psFullySpecifiedType->ePrecisionQualifier = GLSLPRECQ_UNKNOWN;
	}
}

static IMG_VOID __CheckArraySpecifierMustHaveSize(GLSLTreeContext		*psGLSLTreeContext,
													IMG_INT32			iArraySize,
													const Token			*psToken)
{
	if (iArraySize == -1)
	{
		LogProgramParseTreeError(psCPD->psErrorLog, psToken, "'[]' : array size must be declared\n");
	}
}
											
static IMG_INT32 __ProcessArraySpecifier(GLSLTreeContext			*psGLSLTreeContext,
											const ParseTreeEntry	*psIdentifierEntry,
											const YYSTYPE			*psArraySpecifier,
											IMG_BOOL				bMustHaveSize)
{
	/*
	  array_specifier:
		  LEFT_BRACKET RIGHT_BRACKET
		  LEFT_BRACKET constant_expression RIGHT_BRACKET
	*/

	DebugAssert(psArraySpecifier->psToken);
	
	if (!psArraySpecifier->u.psNode)
	{
		if (bMustHaveSize)
		{
			__CheckArraySpecifierMustHaveSize(psGLSLTreeContext, -1, psArraySpecifier->psToken);
		}

		/* No constant expression so return unsized array */
		return -1;

	}
	else
	{
		IMG_INT32 iArraySize;

		/* Get the array size and destroy the constant expression node */
		if (!ASTSemGetArraySize(psGLSLTreeContext, psArraySpecifier->u.psNode, &iArraySize))
		{
			if (psIdentifierEntry)
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psArraySpecifier->u.psNode->psToken,
										 "'%s' : array size must be a positive integer\n",
										 psIdentifierEntry->pvData);
			}
			else
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psArraySpecifier->u.psNode->psToken, "array size must be a positive integer\n");
			}

			return 0;
		}

		return iArraySize;
	}
}

/* 
	__CreateDeclarationIdentifierNode() and __ProcessDeclarationInitializer() are seperated so that the identifier node 
	is created before the initializer node. This allows us to get the precision of the identifier before we process the initializer.
	It might be needed if we cant derive the precision from the initializer itself, eg:
	highp float ftemp = sin( 0.0 );
*/
static GLSLNode* __CreateDeclarationIdentifierNode(GLSLTreeContext					*psGLSLTreeContext,
													const GLSLFullySpecifiedType	*psFullySpecifiedType,
													Token							*psIDENTIFIEREntry,
													YYSTYPE							*psArraySpecifier)
{
	GLSLNode *psIDENTIFIERNode;

	GLSLFullySpecifiedType sFullySpecifiedType = *psFullySpecifiedType;
	
#if defined(GLSL_ES)
	IsSamplerTypeSupported(psGLSLTreeContext, psIDENTIFIEREntry, psFullySpecifiedType);
#endif	
	
	if(sFullySpecifiedType.eTypeSpecifier == GLSLTS_STRUCT &&
		sFullySpecifiedType.eTypeQualifier != GLSLTQ_UNIFORM)
	{
		GLSLStructureDefinitionData* psStructDefinitionData = (GLSLStructureDefinitionData*)
			GetSymbolTableData(psGLSLTreeContext->psSymbolTable, sFullySpecifiedType.uStructDescSymbolTableID);
		
		if(psStructDefinitionData && /* Struct might be undeclared. */
			psStructDefinitionData->bContainsSamplers /* Structs with samplers must be uniform. */)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry, 
									 "'%s' : Instances of structures containing samplers must be declared as uniform\n", 
									 psIDENTIFIEREntry->pvData);
		}
	}
	/* Are we dealing with an array declaration? */
	if (psArraySpecifier && /* If from a condition, then array_specifier is not allowed. */
		psArraySpecifier->psToken)
	{
		if (sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST)
		{
#ifndef GLSL_ES
			if (psGLSLTreeContext->uSupportedLanguageVersion < 120)
#endif
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIEREntry,
										 "'%s %s' : cannot declare arrays of this type in this version of the language\n",
										 GLSLTypeQualifierFullDescTable(sFullySpecifiedType.eTypeQualifier),
										 GLSLTypeSpecifierFullDescTable(sFullySpecifiedType.eTypeSpecifier));
			}
		}

		/* Has an array size already been declared? */
		if (sFullySpecifiedType.iArraySize)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psArraySpecifier->psToken,
									 "'%s' : Multi dimensional arrays not supported\n",
									 psIDENTIFIEREntry->pvData);
		}
		else
		{
			/* Get the array size */
			sFullySpecifiedType.iArraySize = __ProcessArraySpecifier(psGLSLTreeContext,
																		   psIDENTIFIEREntry,
																		   psArraySpecifier,
																		   IMG_FALSE);
		}
	}
	
	psIDENTIFIERNode = ASTCreateIDENTIFIERNode(psGLSLTreeContext,
												   psIDENTIFIEREntry,
												   IMG_TRUE,
												   &sFullySpecifiedType);

	return psIDENTIFIERNode;
}

static GLSLNode* __ProcessDeclarationInitializer(GLSLTreeContext		*psGLSLTreeContext,
												GLSLNode				*psIDENTIFIERNode,
												YYSTYPE					*psInitializer)
{
	GLSLFullySpecifiedType sFullySpecifiedType;
	
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
						   
	if(psInitializer->psToken)
	{
		GLSLNode *psInitializerNode, *psEQUALNode, *psExpressionNode;

		if (sFullySpecifiedType.iArraySize != 0)
		{
#if !defined(GLSL_ES)
			if (psGLSLTreeContext->uSupportedLanguageVersion < 120)
#endif
			{
				LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIERNode->psToken,
										 "'%s' : cannot initialise arrays in this version of the language\n",
										 psIDENTIFIERNode->psToken->pvData);
			}
		}

		/* Create an expression node (top level parent) */
		psExpressionNode = ASTCreateNewNode(GLSLNT_EXPRESSION, psIDENTIFIERNode->psToken);		if(!psExpressionNode) return NULL;

		/* Create an EQUAL Node (parent) */
		psEQUALNode = ASTCreateNewNode(GLSLNT_EQUAL, psInitializer->psToken);		if(!psEQUALNode) return NULL;

		/* Get the initializer node (right) */
		psInitializerNode = psInitializer->u.psNode;

		/*
		   Create the identifier node

		   Note - Identifier node is created AFTER we have processed the initialiser,
		   this is to be correct with regards to name space rules, whereby the identifier is
		   not allowed to be used to initialise itself.

		   i.e.

		   -- case 1 --

		   int x = x; // Error if x has not been previously defined.

		   -- case 2 -- 

		   int x = 1;
		   {
			   int x = x + 2; // x is initialized to 3, this declaration of x is now the valid one
		   }
	   
		*/
		
		/* Add the left child */
		ASTAddNodeChild(psEQUALNode, psIDENTIFIERNode);
		
		/* Add the right child */
		ASTAddNodeChild(psEQUALNode, psInitializerNode);

		/* Semantic check  */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psEQUALNode, IMG_TRUE);
		
		/* Add the equal child */
		ASTAddNodeChild(psExpressionNode, psEQUALNode);
		
		/* Semantic check  */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psExpressionNode, IMG_FALSE);
		
		return psExpressionNode;
	}
	else
	{
		if (sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psIDENTIFIERNode->psToken, 
									 "'%s' : variables with qualifier 'const' must be initialized\n",
									 psIDENTIFIERNode->psToken->pvData);
		}
		
		/* Return the identifier node */
		return psIDENTIFIERNode;
	}
}

static IMG_VOID __ProcessParameterDeclaration(GLSLTreeContext			*psGLSLTreeContext,
												ParseContext			*psParseContext,
												YYSTYPE					*ss,
												YYSTYPE					*psParameterTypeQualifier,
												YYSTYPE					*psParameterDeclarator)
{
	UNION_ALLOC(*ss, psParameterDeclaration);
	ss->u.psParameterDeclaration->eSymbolTableDataType = GLSLSTDT_IDENTIFIER;
		
	ss->u.psParameterDeclaration->sFullySpecifiedType = *psParameterDeclarator->u.psParameterDeclarator->psFullySpecifiedType;
	ss->u.psParameterDeclaration->sFullySpecifiedType.eTypeQualifier = psParameterTypeQualifier->u.eTypeQualifier;

#if defined(GLSL_ES)
	IsSamplerTypeSupported(psGLSLTreeContext, psParameterDeclarator->psToken, &ss->u.psParameterDeclaration->sFullySpecifiedType);
#endif
	
	/* Did the type specifier have an array size? */
	if (ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize)
	{
		__CheckArraySpecifierMustHaveSize(psGLSLTreeContext, ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize, 
			psParameterDeclarator->u.psParameterDeclarator->psIdentifierToken);
		
		ss->u.psParameterDeclaration->iActiveArraySize				= ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize;
		ss->u.psParameterDeclaration->eArrayStatus					= GLSLAS_ARRAY_SIZE_FIXED;

		/* Was there also an array size attached to the type specifier? */
		if (psParameterDeclarator->u.psParameterDeclarator->sArraySpecifier.psToken)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psParameterDeclarator->u.psParameterDeclarator->sArraySpecifier.psToken,
										"'%s' : Multi dimensional arrays not supported\n",
										psParameterDeclarator->u.psParameterDeclarator->psIdentifierToken->pvData);
		}
	}
	/* Is it an array? */
	else if (psParameterDeclarator->u.psParameterDeclarator->sArraySpecifier.psToken)
	{
		/* Get the array size */
		ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize = __ProcessArraySpecifier(psGLSLTreeContext,
																					psParameterDeclarator->u.psParameterDeclarator->psIdentifierToken,
																					&psParameterDeclarator->u.psParameterDeclarator->sArraySpecifier,
																					IMG_TRUE); 
																					
		ss->u.psParameterDeclaration->iActiveArraySize				= ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize;
		ss->u.psParameterDeclaration->eArrayStatus					= GLSLAS_ARRAY_SIZE_FIXED;
	}
	else
	{
		ss->u.psParameterDeclaration->sFullySpecifiedType.iArraySize = 0;
		ss->u.psParameterDeclaration->iActiveArraySize				= -1;
		ss->u.psParameterDeclaration->eArrayStatus					= GLSLAS_NOT_ARRAY;
	}

	/* Calculate l-value status */
	if (ss->u.psParameterDeclaration->sFullySpecifiedType.eTypeQualifier != GLSLTQ_CONST)
	{
		ss->u.psParameterDeclaration->eLValueStatus = GLSLLV_L_VALUE;
	}
	else
	{
		ss->u.psParameterDeclaration->eLValueStatus = GLSLLV_NOT_L_VALUE;
	}

	/* No constant data associated with this parameter */
	ss->u.psParameterDeclaration->eBuiltInVariableID				= GLSLBV_NOT_BTIN;
	ss->u.psParameterDeclaration->eIdentifierUsage					= (GLSLIdentifierUsage)0;
	ss->u.psParameterDeclaration->uConstantDataSize					= 0;
	ss->u.psParameterDeclaration->pvConstantData					= IMG_NULL;
	ss->u.psParameterDeclaration->uConstantAssociationSymbolID		= 0;
	
	/* function_header_with_parameters will use this token when calling ASTAddFunctionState */
	ss->psToken = psParameterDeclarator->u.psParameterDeclarator->psIdentifierToken;
	
	/* Check for incorrect use of CONST */
	if (ss->u.psParameterDeclaration->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST) 
	{
		if (ss->u.psParameterDeclaration->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INOUT)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psParameterTypeQualifier->psToken,
									 "'const' : qualifier not allowed with inout\n");

			/* Set this to stop semantic check also raising an error */
			ss->u.psParameterDeclaration->sFullySpecifiedType.eTypeQualifier = GLSLTQ_TEMP;
			
		}
		else if (ss->u.psParameterDeclaration->sFullySpecifiedType.eParameterQualifier == GLSLPQ_OUT)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psParameterTypeQualifier->psToken,
									 "'const' : qualifier not allowed with out\n");

			/* Set this to stop semantic check also raising an error */
			ss->u.psParameterDeclaration->sFullySpecifiedType.eTypeQualifier = GLSLTQ_TEMP;
		}
	}
}

static IMG_VOID __AddFunctionState(ParseContext				*psParseContext, 
									GLSLTreeContext			*psGLSLTreeContext,
									ASTFunctionState		*psFunctionState,
									ParseTreeEntry			*psParamIDENTIFIEREntry,
									GLSLIdentifierData		*psParameterData)
{
	ASTFunctionStateParam *psNewParam;
	
	/* Check for occurences of 

	    void main(void)  

		(or something similar)
	*/

	if (!psParamIDENTIFIEREntry && 
		(psParameterData->sFullySpecifiedType.eTypeSpecifier == GLSLTS_VOID) &&
		(psFunctionState->uNumParameters == 0))
	{
		return;
	}

	/* Check for usages of 'void' type specifier */
	if (psParameterData->sFullySpecifiedType.eTypeSpecifier == GLSLTS_VOID)
	{
		if (psParamIDENTIFIEREntry)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, psParamIDENTIFIEREntry,
									 "'%s' : illegal use of type 'void'\n",
									 psParamIDENTIFIEREntry->pvData);
		}
	
		LogProgramParseTreeError(psCPD->psErrorLog, psFunctionState->psIDENTIFIEREntry, 
								 "'void' : cannot be an argument type except for '(void)'\n");
	}

	PARSER_ALLOC(psNewParam);
	psNewParam->sParameterData = *psParameterData;
	psNewParam->psParamIDENTIFIEREntry = psParamIDENTIFIEREntry;
	psNewParam->psNext = NULL;
	if(psFunctionState->psParameters)
	{
		ASTFunctionStateParam *psLastParam = psFunctionState->psParameters;
		while(psLastParam->psNext)
			psLastParam = psLastParam->psNext;
		psLastParam->psNext = psNewParam;
	}
	else
	{
		psFunctionState->psParameters = psNewParam;
	}

	psFunctionState->uNumParameters++;
}

static GLSLNode* __NewOperator(GLSLTreeContext *psGLSLTreeContext, 
	GLSLNodeType eNodeType, Token *psOperatorToken, GLSLNode* psOperand1, GLSLNode* psOperand2)
{
	GLSLNode* psNode = ASTCreateNewNode(eNodeType, psOperatorToken);		if(!psNode) return NULL;
	
	ASTAddNodeChild(psNode, psOperand1);
	if(psOperand2)
		ASTAddNodeChild(psNode, psOperand2);
	ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psNode, IMG_FALSE);
	return psNode;
}

#define __CheckFeatureVersion(a,b,c,d) if(!ASTCheckFeatureVersion(psGLSLTreeContext, a,b,c,d)) YYABORT;

%}

%start translation_unit				/* First rule */

%name-prefix="glsl_"				/* Allow multiple parsers in the same program. */

%parse-param {ParseContext *psParseContext}
%parse-param {GLSLTreeContext *psGLSLTreeContext}
%lex-param {ParseContext *psParseContext}

%pure-parser						/* local variables */

%initial-action
{
#if YYDEBUG
	glsl_debug = 1;
#endif
}

%token TOK_INVALID_TOKEN TOK_ARRAY_LENGTH TOK_ATTRIBUTE TOK_BREAK TOK_CENTROID TOK_CONST TOK_CONTINUE TOK_DISCARD TOK_DO TOK_ELSE
%token TOK_FALSE TOK_FLAT TOK_FOR TOK_HIGHP TOK_IF TOK_IN TOK_INOUT TOK_INVARIANT TOK_LOWP
%token TOK_MEDIUMP TOK_OUT TOK_PRECISION TOK_RETURN TOK_SAMPLER1D TOK_SAMPLER2D TOK_SAMPLER3D TOK_SAMPLER1DSHADOW
%token TOK_SAMPLER2DSHADOW 
%token TOK_SAMPLER2DRECT TOK_SAMPLER2DRECTSHADOW
%token TOK_SAMPLERCUBE TOK_STRUCT TOK_SUPER_PRECISION TOK_TRUE TOK_UNIFORM TOK_VARYING 
%token TOK_VOID TOK_WHILE

%token TOK_BOOL TOK_BVEC2 TOK_BVEC3 TOK_BVEC4 TOK_FLOAT TOK_VEC2 TOK_VEC3 TOK_VEC4 TOK_INT TOK_IVEC2 TOK_IVEC3 TOK_IVEC4 
%token TOK_MAT2X2 TOK_MAT2X3 TOK_MAT2X4 TOK_MAT3X2 TOK_MAT3X3 TOK_MAT3X4 TOK_MAT4X2 TOK_MAT4X3 TOK_MAT4X4

/* GL2 reserved words for future implementations */
%token TOK_ASM TOK_CAST TOK_CLASS TOK_DEFAULT TOK_DOUBLE TOK_DVEC2 TOK_DVEC3 TOK_DVEC4 TOK_ENUM TOK_EXTERN TOK_EXTERNAL TOK_FIXED TOK_FVEC2
%token TOK_FVEC3 TOK_FVEC4 TOK_GOTO TOK_HALF TOK_HVEC2 TOK_HVEC3 TOK_HVEC4 TOK_INLINE TOK_INPUT TOK_INTERFACE TOK_LONG TOK_NAMESPACE 
%token TOK_NOINLINE TOK_OUTPUT TOK_PACKED TOK_PUBLIC TOK_SAMPLER3DRECT TOK_SHORT TOK_SIZEOF
%token TOK_STATIC TOK_SWITCH TOK_TEMPLATE TOK_TILDE TOK_THIS TOK_TYPEDEF TOK_UNION TOK_UNSIGNED TOK_USING TOK_VOLATILE TOK_CASE TOK_CHAR
%token TOK_BYTE

/* 
 General comment handling tokens - these are handled as special cases by the parser
 what ever matches these tokens will not be removed from the returned token stream 
*/
%token TOK_INCOMMENT TOK_OUTCOMMENT TOK_COMMENT

/* GL2 chars */
%token TOK_AMPERSAND TOK_BANG TOK_CARET TOK_COMMA TOK_COLON TOK_DASH TOK_DOT TOK_DOTDOT TOK_EQUAL TOK_LEFT_ANGLE TOK_LEFT_BRACE 
%token TOK_LEFT_BRACKET TOK_LEFT_PAREN TOK_PERCENT TOK_PLUS TOK_QUESTION TOK_RIGHT_ANGLE TOK_RIGHT_BRACE TOK_RIGHT_BRACKET TOK_RIGHT_PAREN 
%token TOK_SEMICOLON TOK_SLASH TOK_STAR TOK_VERTICAL_BAR

/* Non GL2 chars */
%token TOK_APOSTROPHE TOK_AT TOK_BACK_SLASH TOK_DOLLAR TOK_HASH TOK_SPEECH_MARK TOK_UNDERSCORE

/* GL2 operators */
%token TOK_ADD_ASSIGN TOK_AND_ASSIGN TOK_AND_OP TOK_DEC_OP TOK_DIV_ASSIGN TOK_EQ_OP TOK_INC_OP TOK_GE_OP TOK_LE_OP TOK_LEFT_ASSIGN 
%token TOK_LEFT_OP TOK_MOD_ASSIGN TOK_MUL_ASSIGN TOK_NE_OP TOK_OR_ASSIGN TOK_OR_OP TOK_RIGHT_ASSIGN TOK_RIGHT_OP TOK_SUB_ASSIGN TOK_XOR_OP 
%token TOK_XOR_ASSIGN 

/* GL2 data types */
%token TOK_FLOATCONSTANT TOK_BOOLCONSTANT TOK_INTCONSTANT TOK_UINTCONSTANT TOK_IDENTIFIER TOK_TYPE_NAME

/* Program header XXX - can remove? */
%token TOK_PROGRAMHEADER

/* Special chars */
%token TOK_NEWLINE TOK_CARRIGE_RETURN TOK_TAB TOK_VTAB

/* Special token to change the current version of the language */
%token TOK_LANGUAGE_VERSION

/* Special token to change the enabled extensions */
%token TOK_EXTENSION_CHANGE

/* Special cases not handled by grammar. Specially handled by parser */
%token TOK_STRING 
%token TOK_ENDOFSTRING

/* Must be declared in grammar as final token to terminate parsing XXX - can remove ? */
%token TOK_TERMINATEPARSING 

/* ES */
%token TOK_SAMPLERSTREAMIMG
%token TOK_SAMPLEREXTERNALOES

/* GLSL 1.30 stuff */
%token TOK_SMOOTH
%token TOK_NOPERSPECTIVE
%token TOK_SAMPLERCUBESHADOW
%token TOK_SAMPLER1DARRAY
%token TOK_SAMPLER2DARRAY
%token TOK_SAMPLER1DARRAYSHADOW
%token TOK_SAMPLER2DARRAYSHADOW
%token TOK_ISAMPLER1D
%token TOK_ISAMPLER2D
%token TOK_ISAMPLER3D
%token TOK_ISAMPLERCUBE
%token TOK_ISAMPLER1DARRAY
%token TOK_ISAMPLER2DARRAY
%token TOK_USAMPLER1D
%token TOK_USAMPLER2D
%token TOK_USAMPLER3D
%token TOK_USAMPLERCUBE
%token TOK_USAMPLER1DARRAY
%token TOK_USAMPLER2DARRAY
%token TOK_UINT
%token TOK_UVEC2
%token TOK_UVEC3
%token TOK_UVEC4
%token TOK_HASHHASH

%%


variable_identifier:
	TOK_IDENTIFIER													{ $$.u.psNode = ASTCreateIDENTIFIERUseNode(psGLSLTreeContext, $1.psToken); }
	;

primary_expression:
	variable_identifier
	| TOK_INTCONSTANT												{ $$.u.psNode = ASTCreateINTCONSTANTNode(psGLSLTreeContext, $1.psToken); }
	| TOK_UINTCONSTANT												
	{
#ifdef GLSL_130
																	  $$.u.psNode = ASTCreateINTCONSTANTNode(psGLSLTreeContext, $1.psToken);
#endif
	}
	| TOK_FLOATCONSTANT												{ $$.u.psNode = ASTCreateFLOATCONSTANTNode(psGLSLTreeContext, $1.psToken); }
	| TOK_BOOLCONSTANT												{ $$.u.psNode = ASTCreateBOOLCONSTANTNode(psGLSLTreeContext, $1.psToken); }
	| TOK_LEFT_PAREN expression TOK_RIGHT_PAREN
	{
		$$.u.psNode = $2.u.psNode;
		/*
		   The 'expression' node type created is changed to be a 'sub-expression' type,
		   this helps at the intermediate code generation level to know when to insert
		   the instructions to perform postfix increment/decrement operations
		*/
		$$.u.psNode->eNodeType = GLSLNT_SUBEXPRESSION;
	}
	;

postfix_expression:
	primary_expression
	| function_call
	| postfix_expression TOK_LEFT_BRACKET integer_expression TOK_RIGHT_BRACKET
	{
		GLSLNode *psIntegerExpressionNode;
	
		GLSLNode *psLeftNode = $1.u.psNode;
		
		/* Create array specifier (PARENT)  node */
		GLSLNode *psArraySpecifierNode = ASTCreateNewNode(GLSLNT_ARRAY_SPECIFIER, $2.psToken);		if(!psArraySpecifierNode) YYABORT;
		
		/* Get the integer expression node (RIGHT) */
		psIntegerExpressionNode = $3.u.psNode;
	
		ASTAddNodeChild(psArraySpecifierNode, psLeftNode);
		ASTAddNodeChild(psArraySpecifierNode, psIntegerExpressionNode);
						
		/* Calculate what the result would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psArraySpecifierNode, IMG_FALSE);
		
		$$.u.psNode = psArraySpecifierNode;
	}
	| postfix_expression TOK_DOT field_selection
	{
		GLSLNode *psFieldNode;
		
		GLSLNode *psLeftNode = $1.u.psNode;
		
		/* Create field selection node */
		GLSLNode *psFieldSelectionNode = ASTCreateNewNode(GLSLNT_FIELD_SELECTION, $2.psToken);		if(!psFieldSelectionNode) YYABORT;
		
		/* process and return field selection entry */
		psFieldNode = ASTProcessFieldSelection(psGLSLTreeContext, $3.psToken, psLeftNode);

		ASTAddNodeChild(psFieldSelectionNode, psLeftNode);
		ASTAddNodeChild(psFieldSelectionNode, psFieldNode);

		/* Calculate what the result would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psFieldSelectionNode, IMG_FALSE);
		
		$$.u.psNode = psFieldSelectionNode;
	}
	| postfix_expression TOK_INC_OP
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_POST_INC, $2.psToken, $1.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	}
	| postfix_expression TOK_DEC_OP
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_POST_DEC, $2.psToken, $1.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	}
	;

field_selection:
	TOK_IDENTIFIER
	;
	
integer_expression:
	expression										   
	{
		/* 
		   The 'expression' node type created is changed to be a 'sub-expression' type,
		   this helps at the intermediate code generation level to know when to insert
		   the instructions to perform postfix increment/decrement operations
		*/
		$$.u.psNode->eNodeType = GLSLNT_SUBEXPRESSION;
	}
	;

function_call:
	function_call_generic
	{
		ASTFunctionCallState *psFunctionCallState = $1.u.psFunctionCallState;
		GLSLNode *psFunctionCallNode = psFunctionCallState->psFunctionCallNode;
		
		IMG_UINT32 uFunctionDefinitionSymbolTableID = 0;
		IMG_CHAR *pszFunctionName = IMG_NULL;
		
		IMG_CHAR acFunctionCallName[1024];
		IMG_CHAR acConstructorName[1024];
				
		if (psFunctionCallState->bConstructor)
		{
			if(psFunctionCallState->eConstructorTypeSpecifier == GLSLTS_STRUCT)
			{
				sprintf(acConstructorName, CONSTRUCTOR_STRING, (IMG_CHAR *)psFunctionCallNode->psToken->pvData);
			}
			else
			{
				/* Create a constructor name */
				sprintf(acConstructorName, CONSTRUCTOR_STRING, GLSLTypeSpecifierDescTable(psFunctionCallState->eConstructorTypeSpecifier));
			}

			pszFunctionName = acConstructorName;

			/* Find symbol ID of this constructor */
			if (!FindSymbol(psGLSLTreeContext->psSymbolTable, acConstructorName, &(uFunctionDefinitionSymbolTableID), IMG_FALSE))
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,"ASTProcessFunctionCall: '%s' : no matching constructor found\n", 
									 acConstructorName));

				psFunctionCallNode->eNodeType = GLSLNT_ERROR;
			}
		}
		else
		{
			IMG_UINT32 i;
			GLSLFullySpecifiedType *psFullySpecifiedTypes = DebugMemAlloc(sizeof(GLSLFullySpecifiedType) * psFunctionCallNode->uNumChildren);

			if(psFunctionCallNode->uNumChildren && !psFullySpecifiedTypes)
			{
				$$.u.psNode = IMG_NULL;
				YYABORT;
			}

			pszFunctionName = $1.psToken->pvData;

			/* build up function name based on parameter types */
			for (i = 0; i < psFunctionCallNode->uNumChildren; i++)
			{
				GLSLArrayStatus eArrayStatus;

				/* Did it have a valid symbol table ID - might not if parameter was undefined or expression was badly formed */
				if (!psFunctionCallNode->ppsChildren[i]->uSymbolTableID)
				{
					/* Set it to a default type */
					INIT_FULLY_SPECIFIED_TYPE(&(psFullySpecifiedTypes[i]));
					psFunctionCallNode->eNodeType = GLSLNT_ERROR;
				}
				else
				{
					/* Get type and array size of this parameter (which we use to hash the parameter name) */
					if (!GetSymbolInfo(psCPD, psGLSLTreeContext->psSymbolTable,
									   psFunctionCallNode->ppsChildren[i]->uSymbolTableID,
									   psGLSLTreeContext->eProgramType,
									   &(psFullySpecifiedTypes[i]),
									   IMG_NULL,
									   IMG_NULL,
									   &eArrayStatus,
									   IMG_NULL,
									   IMG_NULL,
									   IMG_NULL))
					{
						LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Failed to retreive parameter %d's data for function '%s' (symbolID %u) (node type %s)\n",
										 i,
										 pszFunctionName,
										 psFunctionCallNode->ppsChildren[i]->uSymbolTableID,
										 NODETYPE_DESC(psFunctionCallNode->ppsChildren[i]->eNodeType)));
										 
						$$.u.psNode = IMG_NULL; 
						break;
					}

					/* Can't pass non fixed array size as argument */
					if (eArrayStatus == GLSLAS_ARRAY_SIZE_NOT_FIXED)
					{
						LogProgramNodeError(psCPD->psErrorLog, psFunctionCallNode->ppsChildren[i],
											"'%s' : cannot pass array of undeclared size as parameter\n", 
											pszFunctionName);
						
						psFunctionCallNode->eNodeType = GLSLNT_ERROR;
					}
				}
			}

			/* Find the symbol ID for this function */
			uFunctionDefinitionSymbolTableID =  ASTFindFunction(psGLSLTreeContext,
																psFunctionCallNode,
																pszFunctionName,
																psFullySpecifiedTypes);

			/* Did we find one? */
			if (!uFunctionDefinitionSymbolTableID)
			{
				psFunctionCallNode->eNodeType = GLSLNT_ERROR;
			}

			DebugMemFree(psFullySpecifiedTypes);
		}
		
		if (psFunctionCallNode->eNodeType != GLSLNT_ERROR)
		{
			/* Retrieve the function definition */
			GLSLFunctionDefinitionData *psFunctionDefinitionData = 
				GetSymbolTableData(psGLSLTreeContext->psSymbolTable, uFunctionDefinitionSymbolTableID);
				
			GLSLFunctionCallData sFunctionCallData;

		#ifdef GLSL_130	
			__CheckFeatureVersion($1.psToken, psFunctionDefinitionData->uFeatureVersion, 
				psFunctionDefinitionData->pszOriginalFunctionName, "function or matching overload");
		#endif
			
		#ifdef DEBUG
			if (!psFunctionDefinitionData)
			{
				LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Failed to retreive function data\n"));
				$$.u.psNode = IMG_NULL;
				break;
			}

			if (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)
			{
				LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Retrieved data was not function definition\n"));
				$$.u.psNode = IMG_NULL;
				break;
			}
		#endif

			/* Indicate that this function has been called */
			ASTRegisterFunctionCall(psGLSLTreeContext, psFunctionDefinitionData, uFunctionDefinitionSymbolTableID);

			/* Set up the function call data */
			sFunctionCallData.eSymbolTableDataType		= GLSLSTDT_FUNCTION_CALL;
			sFunctionCallData.sFullySpecifiedType		 = psFunctionDefinitionData->sReturnFullySpecifiedType;
			sFunctionCallData.uFunctionDefinitionSymbolID = uFunctionDefinitionSymbolTableID;
			sFunctionCallData.uLoopLevel				  = psGLSLTreeContext->uLoopLevel;
			sFunctionCallData.uConditionLevel			 = psGLSLTreeContext->uConditionLevel;

			/* Modify array size if appropriate */
			if (psFunctionCallState->bConstructor)
			{
				sFunctionCallData.sFullySpecifiedType.iArraySize = psFunctionCallState->iConstructorArraySize;
			}

			if (sFunctionCallData.sFullySpecifiedType.iArraySize > 0)
			{
				sFunctionCallData.eArrayStatus = GLSLAS_ARRAY_SIZE_FIXED;
			}
			else if (sFunctionCallData.sFullySpecifiedType.iArraySize < 0)
			{
				sFunctionCallData.eArrayStatus = GLSLAS_ARRAY_SIZE_NOT_FIXED;
			}
			else
			{
				sFunctionCallData.eArrayStatus = GLSLAS_NOT_ARRAY;
			}

		#ifdef DEBUG
			/* Some quick sanity checks */
			{
				GLSLIdentifierData *psReturnData = GetSymbolTableData(psGLSLTreeContext->psSymbolTable,
																	  psFunctionDefinitionData->uReturnDataSymbolID);

				if (!psReturnData)
				{
					LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Failed to get result data for func def %s\n",
								 GetSymbolName(psGLSLTreeContext->psSymbolTable, uFunctionDefinitionSymbolTableID)));
				}

				if (!ASTSemCheckTypeSpecifiersMatch(&psReturnData->sFullySpecifiedType, &psFunctionDefinitionData->sReturnFullySpecifiedType))
				{
					LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Fetched type mismatches for func def %s\n",
									 GetSymbolName(psGLSLTreeContext->psSymbolTable, uFunctionDefinitionSymbolTableID)));
				}
			}
		#endif


			/* Generate a unique name for it */
			sprintf(acFunctionCallName,
					FUNCTION_CALL_STRING,
					pszFunctionName,
					GLSLTypeSpecifierDescTable(sFunctionCallData.sFullySpecifiedType.eTypeSpecifier),
					psGLSLTreeContext->uNumResults);

			psGLSLTreeContext->uNumResults++;

			/* Add this type specifier to the symbol table */
			if (!AddFunctionCallData(psCPD, psGLSLTreeContext->psSymbolTable,
									 acFunctionCallName,
									 &sFunctionCallData,
									 IMG_FALSE,
									 &(psFunctionCallNode->uSymbolTableID)))
			{
				LOG_INTERNAL_NODE_ERROR((psFunctionCallNode,
					"ASTProcessFunctionCall: Failed to add function call data %s to symbol table", acFunctionCallName));

				$$.u.psNode = IMG_NULL;
				break;
			}

			/* Now need to check that the supplied parameters match up to the parameter type qualifiers used in the function definition */
			ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psFunctionCallNode, IMG_FALSE);
		}
		else
		{
			psFunctionCallNode->uSymbolTableID = 0;
		}
		
		$$.u.psNode = psFunctionCallNode;
	}
	;

function_call_generic:
	function_call_header_with_parameters TOK_RIGHT_PAREN
	| function_call_header_no_parameters TOK_RIGHT_PAREN
	;

function_call_header_no_parameters:
	function_call_header TOK_VOID
	| function_call_header
	;

function_call_header_with_parameters:
	function_call_header assignment_expression
	{
		/* Add it as a child to the function call node, with an expression node to hang it off. */
		GLSLNode* psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, $2.psToken, $2.u.psNode, IMG_NULL);		if(!psNode) YYABORT;
		ASTAddNodeChild($1.u.psFunctionCallState->psFunctionCallNode, psNode);
	}
	| function_call_header_with_parameters TOK_COMMA assignment_expression
	{
		GLSLNode* psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, $3.psToken, $3.u.psNode, IMG_NULL);		if(!psNode) YYABORT;
		ASTAddNodeChild($1.u.psFunctionCallState->psFunctionCallNode, psNode);
	}
	;

function_call_header:
	function_identifier array_specifier_opt TOK_LEFT_PAREN
	{
		if($2.psToken)
		{
			/* Check for correct version of language */
			__CheckFeatureVersion($2.psToken, 120, IMG_NULL, "array constructors");

			/* Get the array size */
			$$.u.psFunctionCallState->iConstructorArraySize = 
				__ProcessArraySpecifier(psGLSLTreeContext, IMG_NULL/*fixme*/, &$2, IMG_FALSE);
		}
	}
	;

function_identifier:
	type_specifier_e
	{
		UNION_ALLOC($$, psFunctionCallState);
		
		if ($1.psToken->eTokenName == TOK_TYPE_NAME)
		{
			$$.u.psFunctionCallState->bConstructor = IMG_FALSE;
		}
		else
		{
			$$.u.psFunctionCallState->bConstructor = IMG_TRUE;
			$$.u.psFunctionCallState->eConstructorTypeSpecifier = $1.u.eTypeSpecifier;
			$$.u.psFunctionCallState->iConstructorArraySize = 0;
		}
		
		$$.u.psFunctionCallState->psFunctionCallNode = ASTCreateNewNode(GLSLNT_FUNCTION_CALL, $1.psToken);		if(!$$.u.psFunctionCallState->psFunctionCallNode) YYABORT;
	}
	| TOK_IDENTIFIER
	{
		UNION_ALLOC($$, psFunctionCallState);
		
		$$.u.psFunctionCallState->bConstructor = IMG_FALSE;
		
		$$.u.psFunctionCallState->psFunctionCallNode = ASTCreateNewNode(GLSLNT_FUNCTION_CALL, $1.psToken);		if(!$$.u.psFunctionCallState->psFunctionCallNode) YYABORT;
	}
	;

/* Grammar Note: Constructors look like functions, but lexical analysis recognized most of them as keywords. */

unary_expression:
	postfix_expression
	| TOK_INC_OP unary_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_PRE_INC, $1.psToken, $2.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	}
	| TOK_DEC_OP unary_expression
	{ 
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_PRE_DEC, $1.psToken, $2.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	}
	| unary_operator unary_expression  
	{ 
		ASTAddNodeChild($$.u.psNode, $2.u.psNode);
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	}
	;

/* Grammar Note: No traditional style type casts. */

unary_operator:
	TOK_PLUS														{ $$.u.psNode = ASTCreateNewNode(GLSLNT_POSITIVE, $1.psToken);		if(!$$.u.psNode) YYABORT; }
	| TOK_DASH														{ $$.u.psNode = ASTCreateNewNode(GLSLNT_NEGATE, $1.psToken);		if(!$$.u.psNode) YYABORT; }
	| TOK_BANG														{ $$.u.psNode = ASTCreateNewNode(GLSLNT_NOT, $1.psToken);		if(!$$.u.psNode) YYABORT; }
	| TOK_TILDE														
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = ASTCreateNewNode(GLSLNT_BITWISE_NOT, $1.psToken);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

/* Grammar Note: No '*' or '&' unary ops. Pointers are not supported. */

multiplicative_expression:
	unary_expression
	| multiplicative_expression TOK_STAR unary_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_MULTIPLY, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| multiplicative_expression TOK_SLASH unary_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_DIVIDE, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| multiplicative_expression TOK_PERCENT unary_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "modulus operator", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_MODULUS, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

additive_expression:
	multiplicative_expression
	| additive_expression TOK_PLUS multiplicative_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_ADD, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| additive_expression TOK_DASH multiplicative_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SUBTRACT, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

shift_expression:
	additive_expression
	| shift_expression TOK_LEFT_OP additive_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SHIFT_LEFT, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	| shift_expression TOK_RIGHT_OP additive_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SHIFT_RIGHT, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

relational_expression:
	shift_expression
	| relational_expression TOK_LEFT_ANGLE shift_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LESS_THAN, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| relational_expression TOK_RIGHT_ANGLE shift_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_GREATER_THAN, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| relational_expression TOK_LE_OP shift_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LESS_THAN_EQUAL, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| relational_expression TOK_GE_OP shift_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_GREATER_THAN_EQUAL, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

equality_expression:
	relational_expression
	| equality_expression TOK_EQ_OP relational_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EQUAL_TO, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	| equality_expression TOK_NE_OP relational_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_NOTEQUAL_TO, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

and_expression:
	equality_expression
	| and_expression TOK_AMPERSAND equality_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
	
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_AND, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

exclusive_or_expression:
	and_expression
	| exclusive_or_expression TOK_CARET and_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXCLUSIVE_OR, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

inclusive_or_expression:
	exclusive_or_expression
	| inclusive_or_expression TOK_VERTICAL_BAR exclusive_or_expression
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_INCLUSIVE_OR, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

logical_and_expression:
	inclusive_or_expression
	| logical_and_expression TOK_AND_OP inclusive_or_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_AND, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

logical_xor_expression:
	logical_and_expression
	| logical_xor_expression TOK_XOR_OP logical_and_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_XOR, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

logical_or_expression:
	logical_xor_expression
	| logical_or_expression TOK_OR_OP logical_xor_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_OR, $2.psToken, $1.u.psNode, $3.u.psNode);		if(!$$.u.psNode) YYABORT;
	}
	;

conditional_expression:
	logical_or_expression
	| logical_or_expression TOK_QUESTION_increase_conditional_level expression TOK_COLON assignment_expression
	{			
		/* Create the question node */
		$$.u.psNode = ASTCreateNewNode(GLSLNT_QUESTION, $3.psToken);		if(!$$.u.psNode) YYABORT;
		
		/* Add the logical or expression as the first child */
		ASTAddNodeChild($$.u.psNode, $1.u.psNode);

		/* Add the expression node as the second child */
		ASTAddNodeChild($$.u.psNode, $3.u.psNode);

		/* Add the conditional expression as the third child */
		ASTAddNodeChild($$.u.psNode, $5.u.psNode);

		/* Calculate what the result of this expression would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);

		psGLSLTreeContext->uConditionLevel--;
	}
	;
TOK_QUESTION_increase_conditional_level:
	TOK_QUESTION										{ psGLSLTreeContext->uConditionLevel++; }
	;

assignment_expression:
	conditional_expression
	| unary_expression assignment_operator assignment_expression 
	{
		GLSLNode	   *psAssignmentOperatorNode = $2.u.psNode;	/* Parent Node */
		GLSLNode	   *psUnaryExpressionNode = $1.u.psNode;	   /* Child node 1 */
		GLSLNode	   *psAssignmentExpressionNode = $3.u.psNode;  /* Child node 2 */
		
		/* Add the left and right nodes to the parent */

		/* Left */
		ASTAddNodeChild(psAssignmentOperatorNode, psUnaryExpressionNode);

		/* Right */
		ASTAddNodeChild(psAssignmentOperatorNode, psAssignmentExpressionNode);

		/* Calculate what the result of this expression would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psAssignmentOperatorNode, IMG_FALSE);
		
		$$.u.psNode = psAssignmentOperatorNode;
	}
	;

assignment_operator:
	TOK_EQUAL				
	{
		#define __assignment_operator(NodeType) $$.u.psNode = ASTCreateNewNode(GLSLNT_##NodeType, $1.psToken);		if(!$$.u.psNode) YYABORT;
		__assignment_operator(EQUAL);
	}
	| TOK_MUL_ASSIGN		{ __assignment_operator(MUL_ASSIGN); }
	| TOK_DIV_ASSIGN		{ __assignment_operator(DIV_ASSIGN); }
	| TOK_MOD_ASSIGN		
	{
		__CheckFeatureVersion($1.psToken, 130, "modulus operator", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(MOD_ASSIGN);
	#endif
	}
	| TOK_ADD_ASSIGN		{ __assignment_operator(ADD_ASSIGN); }
	| TOK_SUB_ASSIGN		{ __assignment_operator(SUB_ASSIGN); }
	| TOK_LEFT_ASSIGN
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(LEFT_ASSIGN);
	#endif
	}
	| TOK_RIGHT_ASSIGN
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(RIGHT_ASSIGN);
	#endif
	}
	| TOK_AND_ASSIGN
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(AND_ASSIGN);
	#endif
	}
	| TOK_XOR_ASSIGN		
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(XOR_ASSIGN);
	#endif
	}
	| TOK_OR_ASSIGN			
	{
		__CheckFeatureVersion($1.psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(OR_ASSIGN);
	#endif
	}
	;

expression:
	assignment_expression
	{
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, $1.psToken, $1.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	}
	| expression TOK_COMMA assignment_expression
	{
		ASTAddNodeChild($$.u.psNode, $3.u.psNode);
		
		/* We need to recalculate the type of the node, because another child expression was added which may be of a different type. */
		$$.u.psNode->uSymbolTableID = 0;
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	}
	;

constant_expression:
	conditional_expression
	;

declaration:
	function_prototype TOK_SEMICOLON
	{
		ASTCreateFunctionNode(psGLSLTreeContext, $1.u.psFunctionState, IMG_TRUE);
		$$.u.psNode = IMG_NULL;
	}
	| init_declarator_list TOK_SEMICOLON
	{
		$$.u.psNode = $1.u.psInitDeclaratorList->psNode;
	}
	| TOK_PRECISION precision_qualifier type_specifier_no_prec TOK_SEMICOLON /* ES & 1.30+ only. */
	{
		/* Check for valid types */
		if ($3.u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_FLOAT ||
			$3.u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_INT ||
				GLSL_IS_SAMPLER($3.u.psFullySpecifiedType->eTypeSpecifier))
		{
			GLSLPrecisionQualifier ePrecisionQualifier = $2.u.ePrecisionQualifier;
			
			GLSLPrecisionQualifier eForcedPrecision = GLSLPRECQ_UNKNOWN; 

			/* Adjust precision statement if force has been set */
			if ($3.u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_FLOAT)
				eForcedPrecision                          = psGLSLTreeContext->eForceUserFloatPrecision;
			else if ($3.u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_INT)
				eForcedPrecision                          = psGLSLTreeContext->eForceUserIntPrecision;
			else if (GLSL_IS_SAMPLER($3.u.psFullySpecifiedType->eTypeSpecifier))
				eForcedPrecision                          = psGLSLTreeContext->eForceUserSamplerPrecision;

			if(eForcedPrecision != GLSLPRECQ_UNKNOWN)
			{
				ePrecisionQualifier = eForcedPrecision;
			}

			/* Add this to the symbol table */
			ModifyDefaultPrecision(psGLSLTreeContext,
								   ePrecisionQualifier,
								   $3.u.psFullySpecifiedType->eTypeSpecifier);
		}
		else
		{
			LogProgramParseTreeError(psCPD->psErrorLog, $3.psToken,
									 "'%s' : default precision modifier type can only be float, int or sampler\n",
									 GLSLTypeSpecifierDescTable($3.u.psFullySpecifiedType->eTypeSpecifier));
		}
		
		$$.u.psNode = IMG_NULL;
	}
	;

function_prototype:
	function_declarator TOK_RIGHT_PAREN
	;

function_declarator:
	function_header
	| function_header_with_parameters
	;

function_header_with_parameters:
	function_header parameter_declaration							{ __AddFunctionState(psParseContext, psGLSLTreeContext, $1.u.psFunctionState, $2.psToken, $2.u.psParameterDeclaration); }
	| function_header_with_parameters TOK_COMMA parameter_declaration
																	{ __AddFunctionState(psParseContext, psGLSLTreeContext, $1.u.psFunctionState, $3.psToken, $3.u.psParameterDeclaration); }
	;

function_header:
	fully_specified_type TOK_IDENTIFIER TOK_LEFT_PAREN
	{
		UNION_ALLOC($$, psFunctionState);
		
	#if defined(GLSL_ES)
		IsSamplerTypeSupported(psGLSLTreeContext, $1.psToken, $1.u.psFullySpecifiedType);
	#endif

		$$.u.psFunctionState->psIDENTIFIEREntry										= $2.psToken;
		$$.u.psFunctionState->sReturnData.eSymbolTableDataType						= GLSLSTDT_IDENTIFIER;
		$$.u.psFunctionState->sReturnData.sFullySpecifiedType						= *$1.u.psFullySpecifiedType;
		/* Wierd, grammar says fully specified type, yet type qualifier has no meaning for return values according to spec */
		$$.u.psFunctionState->sReturnData.sFullySpecifiedType.eTypeQualifier		= GLSLTQ_TEMP;
		$$.u.psFunctionState->sReturnData.eLValueStatus								= GLSLLV_L_VALUE;
		$$.u.psFunctionState->sReturnData.eBuiltInVariableID						= GLSLBV_NOT_BTIN;
		$$.u.psFunctionState->sReturnData.eIdentifierUsage							= (GLSLIdentifierUsage)(GLSLIU_INTERNALRESULT | GLSLIU_WRITTEN);
		$$.u.psFunctionState->sReturnData.uConstantDataSize							= 0;
		$$.u.psFunctionState->sReturnData.pvConstantData							= IMG_NULL;
		$$.u.psFunctionState->sReturnData.uConstantAssociationSymbolID				= 0;

		/* Was the return type an array? */
		if ($1.u.psFullySpecifiedType->iArraySize)
		{
			$$.u.psFunctionState->sReturnData.eArrayStatus							= GLSLAS_ARRAY_SIZE_FIXED;
			$$.u.psFunctionState->sReturnData.iActiveArraySize						= $1.u.psFullySpecifiedType->iArraySize;
		}
		else
		{
			$$.u.psFunctionState->sReturnData.eArrayStatus							= GLSLAS_NOT_ARRAY;
			$$.u.psFunctionState->sReturnData.iActiveArraySize						= -1;
		}

		$$.u.psFunctionState->uNumParameters = 0;
		$$.u.psFunctionState->psParameters = IMG_NULL;
	}
	;

parameter_declaration:
	parameter_type_qualifier_opt parameter_declarator
	{
		__ProcessParameterDeclaration(psGLSLTreeContext, psParseContext, &$$, &$1, &$2);
	}
	;

parameter_type_qualifier_opt:
	/* empty */														{ $$.u.eTypeQualifier = GLSLTQ_TEMP; }
	| TOK_CONST														{ $$.u.eTypeQualifier = GLSLTQ_CONST; }
	;

parameter_qualifier_opt:
	/* empty */														{ $$.u.eParameterQualifier = GLSLPQ_IN; }
	| TOK_IN														{ $$.u.eParameterQualifier = GLSLPQ_IN; }
	| TOK_OUT														{ $$.u.eParameterQualifier = GLSLPQ_OUT; }
	| TOK_INOUT														{ $$.u.eParameterQualifier = GLSLPQ_INOUT; }
	;

parameter_declarator:
	parameter_qualifier_opt type_specifier TOK_IDENTIFIER array_specifier_opt
	{
		/* Need to set the parameter qualifier before we check the type, because 
		   __CheckTypeSpecifier checks that uniforms are either uniform or parameters.*/
		$2.u.psFullySpecifiedType->eParameterQualifier = $1.u.eParameterQualifier;
		__CheckTypeSpecifier(psGLSLTreeContext, &$2);
		
		UNION_ALLOC($$, psParameterDeclarator);
		$$.u.psParameterDeclarator->psFullySpecifiedType = $2.u.psFullySpecifiedType;
		$$.u.psParameterDeclarator->psIdentifierToken = $3.psToken;
		$$.u.psParameterDeclarator->sArraySpecifier = $4;
	}
	| parameter_qualifier_opt type_specifier array_specifier_opt
	{
		/* Need to set the parameter qualifier before we check the type, because 
		   __CheckTypeSpecifier checks that uniforms are either uniform or parameters.*/
		$2.u.psFullySpecifiedType->eParameterQualifier = $1.u.eParameterQualifier;
		__CheckTypeSpecifier(psGLSLTreeContext, &$2);
		
		UNION_ALLOC($$, psParameterDeclarator);
		$$.u.psParameterDeclarator->psFullySpecifiedType = $2.u.psFullySpecifiedType;
		$$.u.psParameterDeclarator->psIdentifierToken = IMG_NULL;
		$$.u.psParameterDeclarator->sArraySpecifier = $3;
	}
	;

init_declarator_list:
	single_declaration
	{
		UNION_ALLOC($$, psInitDeclaratorList);
		$$.u.psInitDeclaratorList->bInvarianceModifier = $1.u.psSingleDeclaration->bInvarianceModifier;
		
		if(!$1.u.psInitDeclaratorList->bInvarianceModifier)
		{
			$$.u.psInitDeclaratorList->psFullySpecifiedType = $1.u.psSingleDeclaration->psFullySpecifiedType;

			/* Check if the declaration was just for a type. */
			if($1.u.psSingleDeclaration->psNode)
			{
				/* Create top level declaration list node */
				$$.u.psInitDeclaratorList->psNode = ASTCreateNewNode(GLSLNT_DECLARATION, $1.psToken);		if(!$$.u.psInitDeclaratorList->psNode) YYABORT;
				
				/* Add the single declaration to this node */
				ASTAddNodeChild($$.u.psInitDeclaratorList->psNode, $1.u.psSingleDeclaration->psNode);
				
				/* Calulate what the type the result of this declaration would be */
				ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psInitDeclaratorList->psNode, IMG_FALSE);
			}
			else
			{
				$$.u.psInitDeclaratorList->psNode = IMG_NULL;
			}
		}
		else
		{
			$$.u.psInitDeclaratorList->psNode = IMG_NULL;
		}
	}
	| init_declarator_list TOK_COMMA TOK_IDENTIFIER array_specifier_opt initializer_opt
	{
		if($$.u.psInitDeclaratorList->bInvarianceModifier)
		{
			ASTUpdateInvariantStatus(psGLSLTreeContext, $3.psToken);
		}
		else
		{
			/* __CreateDeclarationIdentifierNode() and __ProcessDeclarationInitializer() are called together here
			   because the initializer already has the fallback precision from the first declaration. */
			GLSLNode* psNextDeclaration = __CreateDeclarationIdentifierNode(psGLSLTreeContext,
				$$.u.psInitDeclaratorList->psFullySpecifiedType, $3.psToken, &$4);			if(!psNextDeclaration) YYABORT;
			
			psNextDeclaration = __ProcessDeclarationInitializer(psGLSLTreeContext, psNextDeclaration, &$5);			if(!psNextDeclaration) YYABORT;
				
			/* Create top level node if it doesn't exist yet */
			if (!$$.u.psInitDeclaratorList->psNode)
				$$.u.psInitDeclaratorList->psNode = ASTCreateNewNode(GLSLNT_DECLARATION, $$.psToken);		if(!$$.u.psInitDeclaratorList->psNode) YYABORT;

			/* Add the next declaration to the list. */
			ASTAddNodeChild($$.u.psInitDeclaratorList->psNode, psNextDeclaration);
		}
	}
	;

single_declaration:
	fully_specified_type
	{
		UNION_ALLOC($$, psSingleDeclaration);
		$$.u.psSingleDeclaration->psNode = IMG_NULL;
		$$.u.psSingleDeclaration->psFullySpecifiedType = $1.u.psFullySpecifiedType;	
		$$.u.psSingleDeclaration->bInvarianceModifier = IMG_FALSE;
	}
	| single_declaration_identifier_node initializer_opt
	{		
		$$.u.psSingleDeclaration->psNode = __ProcessDeclarationInitializer(psGLSLTreeContext,
			$$.u.psSingleDeclaration->psNode,
			&$2);
		if(!$$.u.psSingleDeclaration) YYABORT;
	}
	| TOK_INVARIANT TOK_IDENTIFIER
	{
		ASTUpdateInvariantStatus(psGLSLTreeContext, $2.psToken);
		
		UNION_ALLOC($$, psSingleDeclaration);
		$$.u.psSingleDeclaration->bInvarianceModifier = IMG_TRUE;
	}
	;
	
single_declaration_identifier_node:
	fully_specified_type TOK_IDENTIFIER array_specifier_opt
	{
		UNION_ALLOC($$, psSingleDeclaration);
		$$.u.psSingleDeclaration->psNode = __CreateDeclarationIdentifierNode(psGLSLTreeContext, 
			$1.u.psFullySpecifiedType, $2.psToken, &$3);		if(!$$.u.psSingleDeclaration->psNode) YYABORT;
		$$.u.psSingleDeclaration->psFullySpecifiedType = $1.u.psFullySpecifiedType;
		$$.u.psSingleDeclaration->bInvarianceModifier = IMG_FALSE;
	}
	
initializer_opt:
	/* nothing */													{ $$.psToken = IMG_NULL; }
	| TOK_EQUAL initializer											{ $$.u.psNode = $2.u.psNode; }
	;
	
array_specifier_opt:
	/* nothing */													{ $$.psToken = IMG_NULL; }
	| array_specifier
	;
array_specifier:
	TOK_LEFT_BRACKET constant_expression TOK_RIGHT_BRACKET			{ $$.u.psNode = $2.u.psNode; }
	| TOK_LEFT_BRACKET TOK_RIGHT_BRACKET							{ $$.u.psNode = IMG_NULL; }
	;

/* Grammar Note: No 'enum', or 'typedef'. */

fully_specified_type:
	type_specifier
	{
		/* If no type qualifer specified default is temp */
		$$.u.psFullySpecifiedType->eTypeQualifier = GLSLTQ_TEMP;
		
		__CheckTypeSpecifier(psGLSLTreeContext, &$$);
	}
	| type_qualifier type_specifier
	{ 
		$$.u.psFullySpecifiedType = $2.u.psFullySpecifiedType;
		$$.u.psFullySpecifiedType->eTypeQualifier = $1.u.psTypeQualifier->eTypeQualifier;
		$$.u.psFullySpecifiedType->eVaryingModifierFlags = $1.u.psTypeQualifier->eVaryingModifierFlags;
		
		$$.psToken = $2.psToken;

		__CheckTypeSpecifier(psGLSLTreeContext, &$$);
	}
	;

type_qualifier:
	invariant_qualifier_opt interpolation_qualifier_opt centroid_qualifier_opt storage_qualifier
	{
		$$ = $4;
		$$.u.psTypeQualifier->eVaryingModifierFlags |= $1.u.eVaryingModifierFlags | $2.u.eVaryingModifierFlags | $3.u.eVaryingModifierFlags;
		
		if (($$.u.psTypeQualifier->eTypeQualifier != GLSLTQ_VERTEX_OUT) &&
			($$.u.psTypeQualifier->eTypeQualifier != GLSLTQ_FRAGMENT_IN) &&
			$$.u.psTypeQualifier->eVaryingModifierFlags)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, $$.psToken, 
									 "'%s' : non varyings cannot have varying modifier qualifiers\n", 
									 $$.psToken->pvData);
		}			
	}
	;
	
centroid_qualifier_opt:
	/* empty */
	{
		$$.u.eVaryingModifierFlags = GLSLVMOD_NONE;
	}
	| TOK_CENTROID
	{
		__CheckFeatureVersion($1.psToken, 120, "centroid", IMG_NULL);
	
	#ifndef GLSL_ES
		$$.u.eVaryingModifierFlags = GLSLVMOD_CENTROID;
	#endif
	}
	;
	
interpolation_qualifier_opt:
	/* empty */
	{
		$$.u.eVaryingModifierFlags = GLSLVMOD_NONE;
	}
	| TOK_SMOOTH
	{
		__CheckFeatureVersion($1.psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.eVaryingModifierFlags = GLSLVMOD_NONE;
	#endif
	}
	| TOK_FLAT
	{
		__CheckFeatureVersion($1.psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.eVaryingModifierFlags = GLSLVMOD_FLAT;
	#endif
	}
	| TOK_NOPERSPECTIVE
	{
		__CheckFeatureVersion($1.psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		$$.u.eVaryingModifierFlags = GLSLVMOD_NOPERSPECTIVE;
	#endif
	}
	;
	
invariant_qualifier_opt:
	/* empty */
	{
		$$.u.eVaryingModifierFlags = GLSLVMOD_NONE;
	}
	| TOK_INVARIANT
	{
		$$.u.eVaryingModifierFlags = GLSLVMOD_INVARIANT;
	}
	;
	

	
storage_qualifier:
	TOK_CONST
	{
		#define __ProcessTypeQualifier(ss, _eTypeQualifier, _eVaryingModifierFlags) {\
			UNION_ALLOC(ss, psTypeQualifier); \
			ss.u.psTypeQualifier->eTypeQualifier = (_eTypeQualifier); \
			ss.u.psTypeQualifier->eVaryingModifierFlags = (_eVaryingModifierFlags);}
			
		__ProcessTypeQualifier($$, GLSLTQ_CONST, GLSLVMOD_NONE);
	}
	| TOK_ATTRIBUTE
	{
	#ifdef GLSL_130
		__CheckFeatureVersion($1.psToken, FEATURE_VERSION(0, 130), "attribute", IMG_NULL);
	#endif
		__ProcessTypeQualifier($$, GLSLTQ_VERTEX_IN, GLSLVMOD_NONE);
	}
	| TOK_IN
	{
		__CheckFeatureVersion($1.psToken, 130, "shader in & out declarations", IMG_NULL);
		
		/* The new shader input and output declarations via in & out are mapped to the existing type qualifiers,
		   except fragment shader custom output variables which are mapped to GLSLTQ_FRAGMENT_OUT. */
		   
	#define eForcedVaryingModifierFlags (\
			(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_INVARIANTALL) ? \
			GLSLVMOD_INVARIANT : GLSLVMOD_NONE)

	#ifdef GLSL_130
		if(psGLSLTreeContext->eProgramType == GLSLPT_VERTEX)
		{
			/* Attributes */
			__ProcessTypeQualifier($$, GLSLTQ_VERTEX_IN, GLSLVMOD_NONE);
		}
		else
		{
			__ProcessTypeQualifier($$, GLSLTQ_FRAGMENT_IN, eForcedVaryingModifierFlags);
		}
	#endif
	}
	| TOK_OUT
	{
		__CheckFeatureVersion($1.psToken, 130, "shader in & out declarations", IMG_NULL);
		
	#ifdef GLSL_130
		if(psGLSLTreeContext->eProgramType == GLSLPT_VERTEX)
		{
			/* Varyings */
			__ProcessTypeQualifier($$, GLSLTQ_VERTEX_OUT, eForcedVaryingModifierFlags);
		}
		else
		{
			__ProcessTypeQualifier($$, GLSLTQ_FRAGMENT_OUT, GLSLVMOD_NONE);
		}
	#endif
	}
	| TOK_VARYING
	{
	#ifdef GLSL_130
		__CheckFeatureVersion($1.psToken, FEATURE_VERSION(0, 130), "varying", IMG_NULL);
	#endif
	
		#define eVaryingTypeQualifier (psGLSLTreeContext->eProgramType == GLSLPT_VERTEX) ? GLSLTQ_VERTEX_OUT : GLSLTQ_FRAGMENT_IN
		
		__ProcessTypeQualifier($$, eVaryingTypeQualifier, eForcedVaryingModifierFlags);
	}
	| TOK_UNIFORM
	{
		__ProcessTypeQualifier($$, GLSLTQ_UNIFORM, GLSLVMOD_NONE);
	}
	;
	
type_specifier:
	type_specifier_no_prec	
	| precision_qualifier type_specifier_no_prec
	{
		$$.u.psFullySpecifiedType = $2.u.psFullySpecifiedType;
		$$.u.psFullySpecifiedType->ePrecisionQualifier = $1.u.ePrecisionQualifier;
		$$.psToken = $2.psToken;
	}
	;
	
type_specifier_no_prec:
	type_specifier_e array_specifier_opt
	{
		UNION_ALLOC($$, psFullySpecifiedType);
		INIT_FULLY_SPECIFIED_TYPE($$.u.psFullySpecifiedType);
		$$.u.psFullySpecifiedType->eTypeSpecifier = $1.u.eTypeSpecifier;
		
		if($1.u.eTypeSpecifier == GLSLTS_STRUCT)
		{
			/* The name of a structure was referenced. */
			
			IMG_CHAR *pszStructName = $1.psToken->pvData;
			IMG_CHAR acName[1024];
			IMG_UINT32 uSymbolID;

			sprintf(acName, "%s@struct_def",  pszStructName);

			/* Find the structure definition */
			if (FindSymbol(psGLSLTreeContext->psSymbolTable,
							acName,
							&uSymbolID,
							IMG_FALSE))
			{
				$$.u.psFullySpecifiedType->uStructDescSymbolTableID = uSymbolID;
			}
			else
			{
				LogProgramTokenError(psCPD->psErrorLog, $1.psToken, "'%s' : undeclared identifier\n", pszStructName);
			}
		}
		
	__type_specifier_no_prec_array_specifier_opt:
		if($2.psToken)
		{
#ifdef GLSL_ES
			__CheckFeatureVersion($2.psToken, 0xFFFFFFFF, "array syntax", IMG_NULL);/* This kind of array syntax is never supported for ES. */
#else
			__CheckFeatureVersion($2.psToken, 120, "array syntax", IMG_NULL);
#endif		
			/* At this moment we dont know if it is permissable to specify an empty '[]' array specifier, so we pass IMG_FALSE 
			   as the bMustHaveSize parameter, and expect the higher up rules to check it with __CheckArraySpecifierMustHaveSize()
			   if they must have the size (for example struct and parameter declarations). */
			$$.u.psFullySpecifiedType->iArraySize = __ProcessArraySpecifier(psGLSLTreeContext, IMG_NULL/*fixme*/, &$2, IMG_FALSE);
		}
	}
	| struct_specifier array_specifier_opt
	{
		/* A structure was defined here, and we return a psFullySpecifiedType because we may try to declare an instance
		   of the structure in the same statement. */
		   
		UNION_ALLOC($$, psFullySpecifiedType);
		INIT_FULLY_SPECIFIED_TYPE($$.u.psFullySpecifiedType);
		$$.u.psFullySpecifiedType->eTypeSpecifier = GLSLTS_STRUCT;
		$$.u.psFullySpecifiedType->uStructDescSymbolTableID = $1.u.uStructDescSymbolTableID;
		
		goto __type_specifier_no_prec_array_specifier_opt;
	}
	;
	
type_specifier_e:
	TOK_VOID
	{
		#define __type_specifier_e(ss, e)		(ss).u.eTypeSpecifier = GLSLTS_##e;
	#ifdef GLSL_130
		#define __type_specifier_e_130(ss, e)	(ss).u.eTypeSpecifier = GLSLTS_##e;
	#else
		#define __type_specifier_e_130(ss, e)
	#endif
	#ifdef SUPPORT_GL_TEXTURE_RECTANGLE
		#define __type_specifier_e_texrect(ss, e)	(ss).u.eTypeSpecifier = GLSLTS_##e;
	#else
		#define __type_specifier_e_texrect(ss, e)
	#endif
	#ifdef GLSL_ES
		#define __type_specifier_e_ES(ss, e)	(ss).u.eTypeSpecifier = GLSLTS_##e;
		#define __type_specifier_e_GL(ss, e)
	#else
		#define __type_specifier_e_ES(ss, e)
		#define __type_specifier_e_GL(ss, e)	(ss).u.eTypeSpecifier = GLSLTS_##e;
	#endif
	
		__type_specifier_e($$, VOID);
	}
	| TOK_FLOAT														{ __type_specifier_e($$, FLOAT); }
	| TOK_INT														{ __type_specifier_e($$, INT); }
	| TOK_BOOL														{ __type_specifier_e($$, BOOL); }
	| TOK_VEC2														{ __type_specifier_e($$, VEC2); }
	| TOK_VEC3														{ __type_specifier_e($$, VEC3); }
	| TOK_VEC4														{ __type_specifier_e($$, VEC4); }
	| TOK_BVEC2														{ __type_specifier_e($$, BVEC2); }
	| TOK_BVEC3														{ __type_specifier_e($$, BVEC3); }
	| TOK_BVEC4														{ __type_specifier_e($$, BVEC4); }
	| TOK_IVEC2														{ __type_specifier_e($$, IVEC2); }
	| TOK_IVEC3														{ __type_specifier_e($$, IVEC3); }
	| TOK_IVEC4														{ __type_specifier_e($$, IVEC4); }
	| TOK_MAT2X2													{ __type_specifier_e($$, MAT2X2); }	
	| TOK_MAT3X3													{ __type_specifier_e($$, MAT3X3); }
	| TOK_MAT4X4													{ __type_specifier_e($$, MAT4X4); }
	| TOK_SAMPLER2D													{ __type_specifier_e($$, SAMPLER2D); }
	| TOK_SAMPLER3D													{ __type_specifier_e($$, SAMPLER3D); }
	| TOK_SAMPLERCUBE												{ __type_specifier_e($$, SAMPLERCUBE); }
	/* Changed from original grammar TYPE_NAME, which can't be distinguised from an identifier at lexical analysis stage. */
	| TOK_IDENTIFIER												
	{
		if (CheckTypeName(psGLSLTreeContext, $1.psToken->pvData))
		{
			$1.psToken->eTokenName = TOK_TYPE_NAME;
			__type_specifier_e($$, STRUCT); 
		}
		else
		{
			LogProgramTokenError(psCPD->psErrorLog, $1.psToken, "'%s' : undeclared identifier\n", $1.psToken->pvData);
			yyerror(psParseContext, psGLSLTreeContext, "");
		} 
	}
	
	/* ES-only stuff. */
	| TOK_SAMPLERSTREAMIMG											{ __type_specifier_e($$, SAMPLERSTREAM); }
	| TOK_SAMPLEREXTERNALOES										{ __type_specifier_e($$, SAMPLEREXTERNAL); }
/*	| TOK_SAMPLEREXTERNALOES										
	{
		__CheckFeatureVersion($1.psToken, psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_TEXTURE_EXTERNAL ? 100 : 130,
			$1.psToken->pvData, "undeclared identifier. Have you enabled the correct extension?");
		__type_specifier_e($$, SAMPLEREXTERNAL);

	}*/

	
	/* GL-only stuff. */
	| TOK_SAMPLER1D													{ __type_specifier_e_GL($$, SAMPLER1D); }
	| TOK_MAT2X3													{ __type_specifier_e_GL($$, MAT2X3); }
	| TOK_MAT2X4													{ __type_specifier_e_GL($$, MAT2X4); }
	| TOK_MAT3X2													{ __type_specifier_e_GL($$, MAT3X2); }
	| TOK_MAT3X4													{ __type_specifier_e_GL($$, MAT3X4); }
	| TOK_MAT4X2													{ __type_specifier_e_GL($$, MAT4X2); }
	| TOK_MAT4X3													{ __type_specifier_e_GL($$, MAT4X3); }
	| TOK_SAMPLER1DSHADOW											{ __type_specifier_e_GL($$, SAMPLER1DSHADOW); }
	| TOK_SAMPLER2DSHADOW											{ __type_specifier_e_GL($$, SAMPLER2DSHADOW); }
	
	| TOK_SAMPLER2DRECT												{ __type_specifier_e_texrect($$, SAMPLER2DRECT); }
	| TOK_SAMPLER2DRECTSHADOW										{ __type_specifier_e_texrect($$, SAMPLER2DRECTSHADOW); }
	
	/* GLSL 1.30 stuff */
	| TOK_UINT														{ __type_specifier_e_130($$, UINT); }
	| TOK_UVEC2														{ __type_specifier_e_130($$, UVEC2); }
	| TOK_UVEC3														{ __type_specifier_e_130($$, UVEC3); }
	| TOK_UVEC4														{ __type_specifier_e_130($$, UVEC4); }
	| TOK_ISAMPLER1D												{ __type_specifier_e_130($$, ISAMPLER1D); }
	| TOK_ISAMPLER2D												{ __type_specifier_e_130($$, ISAMPLER2D); }
	| TOK_ISAMPLER3D												{ __type_specifier_e_130($$, ISAMPLER3D); }
	| TOK_ISAMPLERCUBE												{ __type_specifier_e_130($$, ISAMPLERCUBE); }
	| TOK_USAMPLER1D												{ __type_specifier_e_130($$, USAMPLER1D); }
	| TOK_USAMPLER2D												{ __type_specifier_e_130($$, USAMPLER2D); }
	| TOK_USAMPLER3D												{ __type_specifier_e_130($$, USAMPLER3D); }
	| TOK_USAMPLERCUBE												{ __type_specifier_e_130($$, USAMPLERCUBE); }
	| TOK_SAMPLER1DARRAY											{ __type_specifier_e_130($$, SAMPLER1DARRAY); }
	| TOK_SAMPLER2DARRAY											{ __type_specifier_e_130($$, SAMPLER2DARRAY); }
	| TOK_ISAMPLER1DARRAY											{ __type_specifier_e_130($$, ISAMPLER1DARRAY); }
	| TOK_ISAMPLER2DARRAY											{ __type_specifier_e_130($$, ISAMPLER2DARRAY); }
	| TOK_USAMPLER1DARRAY											{ __type_specifier_e_130($$, USAMPLER1DARRAY); }
	| TOK_USAMPLER2DARRAY											{ __type_specifier_e_130($$, USAMPLER2DARRAY); }
	| TOK_SAMPLER1DARRAYSHADOW										{ __type_specifier_e_130($$, SAMPLER1DARRAYSHADOW); }
	| TOK_SAMPLER2DARRAYSHADOW										{ __type_specifier_e_130($$, SAMPLER2DARRAYSHADOW); }
	| TOK_SAMPLERCUBESHADOW											{ __type_specifier_e_130($$, SAMPLERCUBESHADOW); }
	;
	
	
precision_qualifier:
	TOK_HIGHP
	{
		$$.u.ePrecisionQualifier = GLSLPRECQ_HIGH;
		
__precision_qualifier:
		
		/* The GLSL ES compiler always supports precision, but the desktop GL requires version 1.30. */
#ifdef GLSL_ES
		__CheckFeatureVersion($1.psToken, psGLSLTreeContext->eEnabledExtensions & GLSLEXT_IMG_PRECISION ? 0 : 130,
			GLSLPrecisionQualifierFullDescTable[$$.u.ePrecisionQualifier], "precision");
#else
		__CheckFeatureVersion($1.psToken, 130, 
			GLSLPrecisionQualifierFullDescTable[$$.u.ePrecisionQualifier], "precision");
#endif
	}
	| TOK_MEDIUMP													{ $$.u.ePrecisionQualifier = GLSLPRECQ_MEDIUM; goto __precision_qualifier; }
	| TOK_LOWP														{ $$.u.ePrecisionQualifier = GLSLPRECQ_LOW; goto __precision_qualifier; }
	;

struct_specifier:
	TOK_STRUCT TOK_IDENTIFIER_opt TOK_LEFT_BRACE struct_declaration_list TOK_RIGHT_BRACE
	{
		GLSLStructureDefinitionData sStructureDefinitionData;
		IMG_UINT32 uSymbolTableID, i;
		
		IMG_CHAR *pszStructureName;
		IMG_CHAR acUnnamedStructureMem[1024];
		
		__StructDeclaration	*psDeclaration;
		__StructDeclarator *psDeclarator;
			
		if ($2.psToken)
		{
			/* Get structure name */
			pszStructureName = $2.psToken->pvData;
		}
		else
		{
			pszStructureName = acUnnamedStructureMem;

			/* Create a unique name for this structure */
			sprintf(pszStructureName, "UnnamedStruct@%u", psGLSLTreeContext->uNumUnnamedStructures);

			psGLSLTreeContext->uNumUnnamedStructures++;
		}
		
		sStructureDefinitionData.eSymbolTableDataType  = GLSLSTDT_STRUCTURE_DEFINITION;
		sStructureDefinitionData.uStructureSizeInBytes = 0;
		sStructureDefinitionData.uNumMembers = 0;
		sStructureDefinitionData.psMembers = IMG_NULL;
		sStructureDefinitionData.bContainsSamplers = IMG_FALSE;
		
		/* Process the list of structure members */
		for(psDeclaration = $4.u.psStructDeclaration; psDeclaration; psDeclaration = psDeclaration->psNext)
		{
			GLSLStructureDefinitionData* psDeclarationStructDef;
			
			/* Check if this structure definition contains samplers, or any of its sub-structures contain any. */
			if(GLSL_IS_SAMPLER(psDeclaration->psFullySpecifiedType->eTypeSpecifier) ||
				(psDeclaration->psFullySpecifiedType->eTypeSpecifier == GLSLTS_STRUCT && 
					(psDeclarationStructDef = (GLSLStructureDefinitionData*)GetSymbolTableData(psGLSLTreeContext->psSymbolTable, 
					psDeclaration->psFullySpecifiedType->uStructDescSymbolTableID)) && 
					psDeclarationStructDef->bContainsSamplers))
			{
				sStructureDefinitionData.bContainsSamplers = IMG_TRUE;
			}
			
			for(psDeclarator = psDeclaration->psDeclarators; psDeclarator; psDeclarator = psDeclarator->psNext)
			{
				GLSLStructureMember *psStructureMember;
				
				/* Allocate memory for new structure member */
				sStructureDefinitionData.psMembers = DebugMemRealloc(sStructureDefinitionData.psMembers, ((sStructureDefinitionData.uNumMembers + 1) * sizeof(GLSLStructureMember)));
				psStructureMember = &sStructureDefinitionData.psMembers[sStructureDefinitionData.uNumMembers];

				/* Don't think this should ever be used (since the members don't get added to the symbol table) but is neater to set up */
				psStructureMember->sIdentifierData.eSymbolTableDataType = GLSLSTDT_IDENTIFIER;
					
				/* Set up the type */
				psStructureMember->sIdentifierData.sFullySpecifiedType = *psDeclaration->psFullySpecifiedType;

				/* None of these fields are used but they need to be set up so they don't catch some error checking code */
				psStructureMember->sIdentifierData.eLValueStatus				= GLSLLV_NOT_L_VALUE;
				psStructureMember->sIdentifierData.eBuiltInVariableID		   = GLSLBV_NOT_BTIN;
				psStructureMember->sIdentifierData.eIdentifierUsage			 = (GLSLIdentifierUsage)0;
				psStructureMember->sIdentifierData.pvConstantData			   = IMG_NULL;
				psStructureMember->sIdentifierData.uConstantDataSize			= 0;
				psStructureMember->sIdentifierData.uConstantAssociationSymbolID = 0;


				/* Record how far (in bytes) into the structure this member is */
				psStructureMember->uConstantDataOffSetInBytes = sStructureDefinitionData.uStructureSizeInBytes;

				/* Process the struct declarator */
				if(psDeclarator->sArraySpecifier.psToken)
				{
					/* Get the array size */
					IMG_INT32 iArraySize = __ProcessArraySpecifier(psGLSLTreeContext, psDeclarator->psIdentifierToken, &psDeclarator->sArraySpecifier, IMG_TRUE);

					/* Structure members that are arrays are always considered to have all elements active */
					psStructureMember->sIdentifierData.sFullySpecifiedType.iArraySize	= iArraySize;
					psStructureMember->sIdentifierData.eArrayStatus						= GLSLAS_ARRAY_SIZE_FIXED;
					psStructureMember->sIdentifierData.iActiveArraySize					= iArraySize;
				}
				else
				{
					psStructureMember->sIdentifierData.sFullySpecifiedType.iArraySize	= 0;
					psStructureMember->sIdentifierData.iActiveArraySize					= -1;
					psStructureMember->sIdentifierData.eArrayStatus						= GLSLAS_NOT_ARRAY;
				}			
				psStructureMember->pszMemberName = DebugMemAlloc(strlen(psDeclarator->psIdentifierToken->pvData) + 1);	
				strcpy(psStructureMember->pszMemberName, psDeclarator->psIdentifierToken->pvData);

				/* Increase number of structure members */
				sStructureDefinitionData.uNumMembers++;

				/* Increase size of structure */
				sStructureDefinitionData.uStructureSizeInBytes += ASTSemGetSizeInfo(psGLSLTreeContext, psDeclaration->psFullySpecifiedType, IMG_TRUE);
			}
		}
		
		/* Add the structure definition and all associated constructors to the symbol table */
		AddStructureDefinition(psCPD, psGLSLTreeContext->psSymbolTable,
							   $4.psToken,
							   pszStructureName,
							   &sStructureDefinitionData,
							   &uSymbolTableID);

		/* Destroy the structure member names */
		for (i = 0; i < sStructureDefinitionData.uNumMembers; i++)
		{
			DebugMemFree(sStructureDefinitionData.psMembers[i].pszMemberName);
		}

		/* Free the members */
		DebugMemFree(sStructureDefinitionData.psMembers);
		
		$$.u.uStructDescSymbolTableID = uSymbolTableID;
	}	
	;
	
TOK_IDENTIFIER_opt:
	/* nothing */													{ $$.psToken = IMG_NULL; }
	| TOK_IDENTIFIER
	;

struct_declaration_list:
	struct_declaration
	| struct_declaration_list struct_declaration
	{
	#define LINKED_LIST_ADD(member, list, node) {\
		__##member *ps##member = list.u.ps##member;\
		while(ps##member->psNext)\
			ps##member = ps##member->psNext;\
		ps##member->psNext = node.u.ps##member;}
		
		/* Add the next struct_declaration to the end of the list. */
		LINKED_LIST_ADD(StructDeclaration, $1, $2);
	}
	;

struct_declaration:
	type_specifier struct_declarator_list TOK_SEMICOLON
	{
		if(GLSL_IS_SAMPLER($1.u.psFullySpecifiedType->eTypeSpecifier))
		{
			/* For structs containing sampler members, we set the member type qualifier to uniform to prevent
			   error messages, and later force any instances of the struct to be uniform. */
			$1.u.psFullySpecifiedType->eTypeQualifier = GLSLTQ_UNIFORM;
		}
		
		__CheckTypeSpecifier(psGLSLTreeContext, &$1);
		
		UNION_ALLOC($$, psStructDeclaration);
		
		$$.u.psStructDeclaration->psFullySpecifiedType = $1.u.psFullySpecifiedType;
		$$.u.psStructDeclaration->psDeclarators = $2.u.psStructDeclarator;
		$$.u.psStructDeclaration->psNext = IMG_NULL;
		
		__CheckArraySpecifierMustHaveSize(psGLSLTreeContext, $$.u.psStructDeclaration->psFullySpecifiedType->iArraySize, $$.psToken);
	}
	;

struct_declarator_list:
	struct_declarator
	| struct_declarator_list TOK_COMMA struct_declarator
	{
		/* Add the next struct_declarator to the end of the list. */
		LINKED_LIST_ADD(StructDeclarator, $1, $3);
	}
	;

struct_declarator:
	TOK_IDENTIFIER array_specifier_opt
	{
		UNION_ALLOC($$, psStructDeclarator);
		
		$$.u.psStructDeclarator->psIdentifierToken = $1.psToken;
		$$.u.psStructDeclarator->sArraySpecifier = $2;
		$$.u.psStructDeclarator->psNext = IMG_NULL;
	}
	;

initializer:
	assignment_expression
	;

declaration_statement:
	declaration
	;

statement:
	compound_statement
	{
		/* Whenever a statement is finished, reset the fallback precision to unknown. */
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	}
	| simple_statement
	{
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	}
	;

simple_statement:
	declaration_statement
	| expression_statement
	| iteration_statement
	| jump_statement
	{
		/*
		  jump_statement:
		  CONTINUE SEMICOLON
		  BREAK SEMICOLON
		  RETURN SEMICOLON
		  RETURN expression SEMICOLON
		  DISCARD SEMICOLON // Fragment shader only.
		*/
		
		GLSLNodeType eNodeType;
		
		GLSLNode *psJumpNode, *psExpressionNode = NULL;
		
		switch ($1.psToken->eTokenName)
		{
			case TOK_CONTINUE:
			{
				if (!psGLSLTreeContext->uLoopLevel)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, $1.psToken,
											 "'continue' : statement only allowed in loops\n");
				}
				eNodeType = GLSLNT_CONTINUE;
				break;
			}
			case TOK_BREAK:
			{
				
			#ifdef GLSL_130
				if (!psGLSLTreeContext->uLoopLevel && !psGLSLTreeContext->uSwitchLevel)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, $1.psToken,
											 "'break' : statement only allowed in loops or switch statements\n");
				}
			#else
				if (!psGLSLTreeContext->uLoopLevel)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, $1.psToken,
											 "'break' : statement only allowed in loops\n");
				}
			#endif
				
				eNodeType = GLSLNT_BREAK;
				break;
			}
			case TOK_RETURN:
			{
				eNodeType = GLSLNT_RETURN;
				
				/* Get an expression node if return statement specified it. */
				if ($1.u.psNode)									
					psExpressionNode = $1.u.psNode;
					
				break;
			}
			case TOK_DISCARD:
			{
				eNodeType = GLSLNT_DISCARD;
				break;
			}
			default: LOG_INTERNAL_ERROR(("simple_statement: Unrecognised jump statement rule\n"));
		} /* switch ($1.psToken->eTokenName) */
		
		/* Create the jump node */
		psJumpNode = ASTCreateNewNode(eNodeType, $1.psToken);		if(!psJumpNode) YYABORT;

		/* Add the expression node as a child if it exists */
		if (psExpressionNode)
			ASTAddNodeChild(psJumpNode, psExpressionNode);
		
		if(eNodeType != GLSLNT_RETURN)/* For GLSLNT_RETURN, semantic checking is done after the function definition. */
		{
			/* Calulatewhat the type the result of this expression would be */
			ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psJumpNode, IMG_FALSE);
		}

		$$.u.psNode = psJumpNode;
	}
	| selection_statement
	| switch_statement
	| case_label
	;
	

switch_statement_expression:
	TOK_LEFT_PAREN expression TOK_RIGHT_PAREN
	{
	#ifdef GLSL_130
		$$.u.psNode = $2.u.psNode;
		psGLSLTreeContext->uSwitchLevel++;
	#endif
	}
	;
	
switch_statement:
	TOK_SWITCH switch_statement_expression TOK_LEFT_BRACE switch_statement_list TOK_RIGHT_BRACE
	{
		/* For non-130 the lexer will interpret switch & case as reserved keywords, so we never need to 
		   check the version for switch & case. */
	#ifdef GLSL_130
		__CheckFeatureVersion($1.psToken, 130, "switch", IMG_NULL);
		
		$$.u.psNode = ASTCreateNewNode(GLSLNT_SWITCH, $1.psToken);		if(!$$.u.psNode) YYABORT;
		
		if(!$4.u.psNode)
		{
			$4.u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, $3.psToken);		if(!$$.u.psNode) YYABORT;
		}
		
		ASTUpdateConditionalIdentifierUsage(psGLSLTreeContext, $2.u.psNode);
		ASTAddNodeChild($$.u.psNode, $2.u.psNode);
		ASTAddNodeChild($$.u.psNode, $4.u.psNode);
		
		psGLSLTreeContext->uSwitchLevel--;
		
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	#endif
	}
	;
	
switch_statement_list:
	empty
	| statement_list
	;
	
case_label:
	TOK_CASE expression TOK_COLON
	{
	#ifdef GLSL_130
		if (!psGLSLTreeContext->uSwitchLevel)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, $1.psToken,
									 "'case' : statement only allowed in switch statements\n");
		}
		
		$$.u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_CASE, $1.psToken, $2.u.psNode, IMG_NULL);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	| TOK_DEFAULT TOK_COLON
	{
	#ifdef GLSL_130
		if (!psGLSLTreeContext->uSwitchLevel)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, $1.psToken,
									 "'default' : statement only allowed in switch statements\n");
		}
		
		$$.u.psNode = ASTCreateNewNode(GLSLNT_DEFAULT, $1.psToken);		if(!$$.u.psNode) YYABORT;
	#endif
	}
	;

/* Grammar Note: No labeled statements; 'goto' is not supported. */


compound_statement_TOK_LEFT_BRACE:
	TOK_LEFT_BRACE
	{ 
		/* Increase the scope level */ 
		ASTIncreaseScope(psGLSLTreeContext);
	}
	;
	
compound_statement:
	TOK_LEFT_BRACE TOK_RIGHT_BRACE									{ $$.u.psNode = IMG_NULL; }
	| compound_statement_TOK_LEFT_BRACE statement_list TOK_RIGHT_BRACE
	{
		$$.u.psNode = $2.u.psNode;
		
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
	}
	;

statement_no_new_scope:
	compound_statement_no_new_scope
	| simple_statement
	;

compound_statement_no_new_scope:
	TOK_LEFT_BRACE TOK_RIGHT_BRACE
	{
		$$.u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, $1.psToken);		if(!$$.u.psNode) YYABORT;
	}
	| TOK_LEFT_BRACE statement_list TOK_RIGHT_BRACE
	{
		$$.u.psNode = $2.u.psNode;
	}
	;

statement_list:
	statement
	{
		$$.u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, $1.psToken);		if(!$$.u.psNode) YYABORT;
		ASTAddNodeChild($$.u.psNode, $1.u.psNode);
	}
	| statement_list statement
	{
		ASTAddNodeChild($$.u.psNode, $2.u.psNode);
	}
	;

expression_statement:
	TOK_SEMICOLON													{ $$.u.psNode = IMG_NULL; }
	| expression TOK_SEMICOLON
	;

selection_statement:
	TOK_IF TOK_LEFT_PAREN expression TOK_RIGHT_PAREN selection_statement_increase_scope selection_rest_statement
	{
		GLSLNode *psExpressionNode;
		
		ASTDecreaseScope(psGLSLTreeContext);
		psGLSLTreeContext->uConditionLevel--;
		
		$$.u.psNode = ASTCreateNewNode(GLSLNT_IF, $1.psToken);		if(!$$.u.psNode) YYABORT;
		
		psExpressionNode = $3.u.psNode;
		ASTUpdateConditionalIdentifierUsage(psGLSLTreeContext, psExpressionNode);
		ASTAddNodeChild($$.u.psNode, psExpressionNode);
		
		ASTAddNodeChild($$.u.psNode, $6.u.psSelectionRestStatement->psIfNode);
		ASTAddNodeChild($$.u.psNode, $6.u.psSelectionRestStatement->psElseNode);
		
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	}
	;
	
selection_statement_increase_scope:
	{
		psGLSLTreeContext->uConditionLevel++;
		
		/*	Increasing scope around if and else even though not specified by the grammar, as a simple statement
			could contain a declaration. ie.

			int x=0;

			if(x==0)
				int x=1;
		*/
		ASTIncreaseScope(psGLSLTreeContext);
	}
	;
	
selection_rest_statement_a:
	statement
	{
		ASTDecreaseScope(psGLSLTreeContext);
		ASTIncreaseScope(psGLSLTreeContext);
	}
	;
	
selection_rest_statement:
	selection_rest_statement_a TOK_ELSE statement
	{
		UNION_ALLOC($$, psSelectionRestStatement);
		$$.u.psSelectionRestStatement->psIfNode = $1.u.psNode;
		$$.u.psSelectionRestStatement->psElseNode = $3.u.psNode;
	}
	| statement
	{
		UNION_ALLOC($$, psSelectionRestStatement);
		$$.u.psSelectionRestStatement->psIfNode = $1.u.psNode;
		$$.u.psSelectionRestStatement->psElseNode = IMG_NULL;
	}
	;
	

	
/* Note: selection_rest_statement has been merged with selection_statement */

/* Grammar Note: No 'switch'. Switch statements not supported. */

condition:
	expression
	| condition_identifier_node TOK_EQUAL initializer
	{
		/* Change $3 to pass the initializer GLSLNode, but with the TOK_EQUAL token - this mirrors what initializer_opt does. */
		$3.psToken = $2.psToken;
		
		$$.u.psNode = __ProcessDeclarationInitializer(psGLSLTreeContext, $$.u.psNode, &$3);			if(!$$.u.psNode) YYABORT;
	}
	;

condition_identifier_node:
	fully_specified_type TOK_IDENTIFIER
	{
		$$.u.psNode = __CreateDeclarationIdentifierNode(psGLSLTreeContext, $1.u.psFullySpecifiedType, $2.psToken, NULL);		if(!$$.u.psNode) YYABORT;
	}
	;
	
iteration_statement:
	TOK_WHILE TOK_LEFT_PAREN loop_increase_scope condition TOK_RIGHT_PAREN statement_no_new_scope loop_decrease_scope
	{
		/* Create an iteration  node */
		$$.u.psNode = ASTCreateNewNode(GLSLNT_WHILE, $1.psToken);		if(!$$.u.psNode) YYABORT;

		/* Add the condition node as the first child */
		ASTAddNodeChild($$.u.psNode, $4.u.psNode);

		/* Add the statement no new scope node as the second child */
		ASTAddNodeChild($$.u.psNode, $6.u.psNode);

		/* Check condition is OK */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	}
	| TOK_DO loop_increase_scope statement loop_decrease_scope TOK_WHILE TOK_LEFT_PAREN expression TOK_RIGHT_PAREN TOK_SEMICOLON	
	{
		$$.u.psNode = ASTCreateNewNode(GLSLNT_DO, $1.psToken);		if(!$$.u.psNode) YYABORT;
		
		/* Add the Expression node as the 1st child */
		ASTAddNodeChild($$.u.psNode, $7.u.psNode);

		/* Add the Statement node as the 2nd child */
		ASTAddNodeChild($$.u.psNode, $3.u.psNode);
		
		/* Check condition is OK */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, $$.u.psNode, IMG_FALSE);
	}
	| TOK_FOR TOK_LEFT_PAREN loop_increase_scope for_init_statement for_cond_statement for_loop_statement TOK_RIGHT_PAREN statement_no_new_scope loop_decrease_scope
	{
		/* Create the iteration node */
		$$.u.psNode = ASTCreateNewNode(GLSLNT_FOR, $1.psToken);		if(!$$.u.psNode) YYABORT;
		
		/* Add the for init statement node as the first child */
		ASTAddNodeChild($$.u.psNode, $4.u.psNode);

		/* Add the for cond statement node as the second child */
		ASTAddNodeChild($$.u.psNode, $5.u.psNode);

		/* Add the for loop statement node as the third child */
		ASTAddNodeChild($$.u.psNode, $6.u.psNode);

		/* Add the statement no new scope node as the fourth child */
		ASTAddNodeChild($$.u.psNode, $8.u.psNode);
	}
	;
	
loop_increase_scope:
	{
		/* Increase the scope level */
		ASTIncreaseScope(psGLSLTreeContext);

		/* Indicate we're inside a loop */
		psGLSLTreeContext->uLoopLevel++;
	}
	;
loop_decrease_scope:
	{
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
				
		/* Indicate we've exited the loop */
		psGLSLTreeContext->uLoopLevel--;
	}
	;
	
for_init_statement:
	expression_statement
	| declaration_statement
	;

for_cond_statement:
	conditionopt TOK_SEMICOLON
	;

for_loop_statement:	
	empty
	| expression
	;

empty:																{ $$.u.psNode = IMG_NULL; }
	;
conditionopt:
	empty
	| condition
	;
	
jump_statement:
	TOK_CONTINUE TOK_SEMICOLON
	| TOK_BREAK TOK_SEMICOLON
	| TOK_RETURN TOK_SEMICOLON										{ $$.u.psNode = IMG_NULL; /* Indicate that no expression is returned. */ }
	| TOK_RETURN expression TOK_SEMICOLON							{ $$.u.psNode = $2.u.psNode; }
	| TOK_DISCARD TOK_SEMICOLON
	;

/* Grammar Note: No 'goto'. Gotos are not supported. */
	
translation_unit:
	external_declaration
	{
		psGLSLTreeContext->psAbstractSyntaxTree = $$.u.psNode = ASTCreateNewNode(GLSLNT_SHADER, $1.psToken);		if(!$$.u.psNode) YYABORT;
		if($1.u.psNode)/* Node may be NULL, for example TOK_LANGUAGE_VERSION. */
			ASTAddNodeChild($$.u.psNode, $1.u.psNode);
	}
	| translation_unit external_declaration
	{
		if($2.u.psNode)/* Node may be NULL. */
			ASTAddNodeChild($$.u.psNode, $2.u.psNode);
	}
	;

external_declaration:
	function_definition
	| declaration
	{
		/* Whenever a global declaration list is finished, reset the fallback precision to unknown. */
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	}
	| TOK_LANGUAGE_VERSION
	{
		ASTProcessLanguageVersion(psGLSLTreeContext, $1.psToken);
		$$.u.psNode = IMG_NULL;
	}
	| TOK_EXTENSION_CHANGE TOK_INTCONSTANT
	{
		/* Update the enabled extensions.

		   Hack: piggyback on pszStartOfLine to store a bitmask.
		*/
		psGLSLTreeContext->eEnabledExtensions = (GLSLExtension)(IMG_UINTPTR_T)$2.psToken->pszStartOfLine;
		
		$$.u.psNode = IMG_NULL;
	}
	;

/* This guarantees that the function node is created before the child nodes are created. 
   Needed so that identifiers used in the function can be matched up to parameters. */
function_definition_prototype:
	function_prototype
	{
		UNION_ALLOC($$, psFunctionDefinitionPrototype);
		
		/* Create the function node, will also increase the scope level */
		$$.u.psFunctionDefinitionPrototype->psNode = ASTCreateFunctionNode(psGLSLTreeContext, $1.u.psFunctionState, IMG_FALSE);
		if(!$$.u.psFunctionDefinitionPrototype->psNode)
			YYABORT;
			
		$$.u.psFunctionDefinitionPrototype->psFunctionState = $1.u.psFunctionState;
		
		/* Make this function the current one */
		psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID = $$.u.psFunctionDefinitionPrototype->psNode->uSymbolTableID;
	}
	;
	
function_definition:
	function_definition_prototype compound_statement_no_new_scope
	{
		GLSLNode* psNode;
		
		$$.u.psNode = $1.u.psFunctionDefinitionPrototype->psNode;
	
		/* Add the compound statement as a child */
		ASTAddNodeChild($$.u.psNode, $2.u.psNode);
		
		/* Initialize function return status to false - semantic checking of return nodes will set it to true. */
		psGLSLTreeContext->bFunctionReturnStatus = IMG_FALSE;
		
		/* Go through all the function's nodes looking for return statements. */
		for(psNode = psGLSLTreeContext->psNodeList;
			psNode != $$.u.psNode; psNode = psNode->psNext)
		{
			if(psNode->eNodeType == GLSLNT_RETURN)
				ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psNode, IMG_FALSE);
		}
		
		/* Check function returned a value */
		if (!psGLSLTreeContext->bFunctionReturnStatus && 
			($1.u.psFunctionDefinitionPrototype->psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID))
		{
			LogProgramParseTreeError(psCPD->psErrorLog,  $$.u.psNode->psToken,
										"'%s' : function does not return a value\n",
										$$.u.psNode->psToken->pvData);
		}
		
		/* Reset function definition symbol ID */
		psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID = 0;
		
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
	}
;


%%
