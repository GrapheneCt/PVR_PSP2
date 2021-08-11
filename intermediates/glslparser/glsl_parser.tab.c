
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         glsl_parse
#define yylex           glsl_lex
#define yyerror         glsl_error
#define yylval          glsl_lval
#define yychar          glsl_char
#define yydebug         glsl_debug
#define yynerrs         glsl_nerrs


/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 1 "tools/intern/oglcompiler/parser/glsl_parser.y"

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



/* Line 189 of yacc.c  */
#line 769 "eurasiacon/binary2_540_omap4430_android_release/target/intermediates/glslparser/glsl_parser.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     TOK_INVALID_TOKEN = 258,
     TOK_ARRAY_LENGTH = 259,
     TOK_ATTRIBUTE = 260,
     TOK_BREAK = 261,
     TOK_CENTROID = 262,
     TOK_CONST = 263,
     TOK_CONTINUE = 264,
     TOK_DISCARD = 265,
     TOK_DO = 266,
     TOK_ELSE = 267,
     TOK_FALSE = 268,
     TOK_FLAT = 269,
     TOK_FOR = 270,
     TOK_HIGHP = 271,
     TOK_IF = 272,
     TOK_IN = 273,
     TOK_INOUT = 274,
     TOK_INVARIANT = 275,
     TOK_LOWP = 276,
     TOK_MEDIUMP = 277,
     TOK_OUT = 278,
     TOK_PRECISION = 279,
     TOK_RETURN = 280,
     TOK_SAMPLER1D = 281,
     TOK_SAMPLER2D = 282,
     TOK_SAMPLER3D = 283,
     TOK_SAMPLER1DSHADOW = 284,
     TOK_SAMPLER2DSHADOW = 285,
     TOK_SAMPLER2DRECT = 286,
     TOK_SAMPLER2DRECTSHADOW = 287,
     TOK_SAMPLERCUBE = 288,
     TOK_STRUCT = 289,
     TOK_SUPER_PRECISION = 290,
     TOK_TRUE = 291,
     TOK_UNIFORM = 292,
     TOK_VARYING = 293,
     TOK_VOID = 294,
     TOK_WHILE = 295,
     TOK_BOOL = 296,
     TOK_BVEC2 = 297,
     TOK_BVEC3 = 298,
     TOK_BVEC4 = 299,
     TOK_FLOAT = 300,
     TOK_VEC2 = 301,
     TOK_VEC3 = 302,
     TOK_VEC4 = 303,
     TOK_INT = 304,
     TOK_IVEC2 = 305,
     TOK_IVEC3 = 306,
     TOK_IVEC4 = 307,
     TOK_MAT2X2 = 308,
     TOK_MAT2X3 = 309,
     TOK_MAT2X4 = 310,
     TOK_MAT3X2 = 311,
     TOK_MAT3X3 = 312,
     TOK_MAT3X4 = 313,
     TOK_MAT4X2 = 314,
     TOK_MAT4X3 = 315,
     TOK_MAT4X4 = 316,
     TOK_ASM = 317,
     TOK_CAST = 318,
     TOK_CLASS = 319,
     TOK_DEFAULT = 320,
     TOK_DOUBLE = 321,
     TOK_DVEC2 = 322,
     TOK_DVEC3 = 323,
     TOK_DVEC4 = 324,
     TOK_ENUM = 325,
     TOK_EXTERN = 326,
     TOK_EXTERNAL = 327,
     TOK_FIXED = 328,
     TOK_FVEC2 = 329,
     TOK_FVEC3 = 330,
     TOK_FVEC4 = 331,
     TOK_GOTO = 332,
     TOK_HALF = 333,
     TOK_HVEC2 = 334,
     TOK_HVEC3 = 335,
     TOK_HVEC4 = 336,
     TOK_INLINE = 337,
     TOK_INPUT = 338,
     TOK_INTERFACE = 339,
     TOK_LONG = 340,
     TOK_NAMESPACE = 341,
     TOK_NOINLINE = 342,
     TOK_OUTPUT = 343,
     TOK_PACKED = 344,
     TOK_PUBLIC = 345,
     TOK_SAMPLER3DRECT = 346,
     TOK_SHORT = 347,
     TOK_SIZEOF = 348,
     TOK_STATIC = 349,
     TOK_SWITCH = 350,
     TOK_TEMPLATE = 351,
     TOK_TILDE = 352,
     TOK_THIS = 353,
     TOK_TYPEDEF = 354,
     TOK_UNION = 355,
     TOK_UNSIGNED = 356,
     TOK_USING = 357,
     TOK_VOLATILE = 358,
     TOK_CASE = 359,
     TOK_CHAR = 360,
     TOK_BYTE = 361,
     TOK_INCOMMENT = 362,
     TOK_OUTCOMMENT = 363,
     TOK_COMMENT = 364,
     TOK_AMPERSAND = 365,
     TOK_BANG = 366,
     TOK_CARET = 367,
     TOK_COMMA = 368,
     TOK_COLON = 369,
     TOK_DASH = 370,
     TOK_DOT = 371,
     TOK_DOTDOT = 372,
     TOK_EQUAL = 373,
     TOK_LEFT_ANGLE = 374,
     TOK_LEFT_BRACE = 375,
     TOK_LEFT_BRACKET = 376,
     TOK_LEFT_PAREN = 377,
     TOK_PERCENT = 378,
     TOK_PLUS = 379,
     TOK_QUESTION = 380,
     TOK_RIGHT_ANGLE = 381,
     TOK_RIGHT_BRACE = 382,
     TOK_RIGHT_BRACKET = 383,
     TOK_RIGHT_PAREN = 384,
     TOK_SEMICOLON = 385,
     TOK_SLASH = 386,
     TOK_STAR = 387,
     TOK_VERTICAL_BAR = 388,
     TOK_APOSTROPHE = 389,
     TOK_AT = 390,
     TOK_BACK_SLASH = 391,
     TOK_DOLLAR = 392,
     TOK_HASH = 393,
     TOK_SPEECH_MARK = 394,
     TOK_UNDERSCORE = 395,
     TOK_ADD_ASSIGN = 396,
     TOK_AND_ASSIGN = 397,
     TOK_AND_OP = 398,
     TOK_DEC_OP = 399,
     TOK_DIV_ASSIGN = 400,
     TOK_EQ_OP = 401,
     TOK_INC_OP = 402,
     TOK_GE_OP = 403,
     TOK_LE_OP = 404,
     TOK_LEFT_ASSIGN = 405,
     TOK_LEFT_OP = 406,
     TOK_MOD_ASSIGN = 407,
     TOK_MUL_ASSIGN = 408,
     TOK_NE_OP = 409,
     TOK_OR_ASSIGN = 410,
     TOK_OR_OP = 411,
     TOK_RIGHT_ASSIGN = 412,
     TOK_RIGHT_OP = 413,
     TOK_SUB_ASSIGN = 414,
     TOK_XOR_OP = 415,
     TOK_XOR_ASSIGN = 416,
     TOK_FLOATCONSTANT = 417,
     TOK_BOOLCONSTANT = 418,
     TOK_INTCONSTANT = 419,
     TOK_UINTCONSTANT = 420,
     TOK_IDENTIFIER = 421,
     TOK_TYPE_NAME = 422,
     TOK_PROGRAMHEADER = 423,
     TOK_NEWLINE = 424,
     TOK_CARRIGE_RETURN = 425,
     TOK_TAB = 426,
     TOK_VTAB = 427,
     TOK_LANGUAGE_VERSION = 428,
     TOK_EXTENSION_CHANGE = 429,
     TOK_STRING = 430,
     TOK_ENDOFSTRING = 431,
     TOK_TERMINATEPARSING = 432,
     TOK_SAMPLERSTREAMIMG = 433,
     TOK_SAMPLEREXTERNALOES = 434,
     TOK_SMOOTH = 435,
     TOK_NOPERSPECTIVE = 436,
     TOK_SAMPLERCUBESHADOW = 437,
     TOK_SAMPLER1DARRAY = 438,
     TOK_SAMPLER2DARRAY = 439,
     TOK_SAMPLER1DARRAYSHADOW = 440,
     TOK_SAMPLER2DARRAYSHADOW = 441,
     TOK_ISAMPLER1D = 442,
     TOK_ISAMPLER2D = 443,
     TOK_ISAMPLER3D = 444,
     TOK_ISAMPLERCUBE = 445,
     TOK_ISAMPLER1DARRAY = 446,
     TOK_ISAMPLER2DARRAY = 447,
     TOK_USAMPLER1D = 448,
     TOK_USAMPLER2D = 449,
     TOK_USAMPLER3D = 450,
     TOK_USAMPLERCUBE = 451,
     TOK_USAMPLER1DARRAY = 452,
     TOK_USAMPLER2DARRAY = 453,
     TOK_UINT = 454,
     TOK_UVEC2 = 455,
     TOK_UVEC3 = 456,
     TOK_UVEC4 = 457,
     TOK_HASHHASH = 458
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 1014 "eurasiacon/binary2_540_omap4430_android_release/target/intermediates/glslparser/glsl_parser.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  109
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   3559

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  204
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  95
/* YYNRULES -- Number of rules.  */
#define YYNRULES  262
/* YYNRULES -- Number of states.  */
#define YYNSTATES  378

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   458

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    19,
      21,    23,    28,    32,    35,    38,    40,    42,    44,    47,
      50,    53,    55,    58,    62,    66,    68,    70,    72,    75,
      78,    81,    83,    85,    87,    89,    91,    95,    99,   103,
     105,   109,   113,   115,   119,   123,   125,   129,   133,   137,
     141,   143,   147,   151,   153,   157,   159,   163,   165,   169,
     171,   175,   177,   181,   183,   187,   189,   195,   197,   199,
     203,   205,   207,   209,   211,   213,   215,   217,   219,   221,
     223,   225,   227,   231,   233,   236,   239,   244,   247,   249,
     251,   254,   258,   262,   265,   266,   268,   269,   271,   273,
     275,   280,   284,   286,   292,   294,   297,   300,   304,   305,
     308,   309,   311,   315,   318,   320,   323,   328,   329,   331,
     332,   334,   336,   338,   339,   341,   343,   345,   347,   349,
     351,   353,   355,   358,   361,   364,   366,   368,   370,   372,
     374,   376,   378,   380,   382,   384,   386,   388,   390,   392,
     394,   396,   398,   400,   402,   404,   406,   408,   410,   412,
     414,   416,   418,   420,   422,   424,   426,   428,   430,   432,
     434,   436,   438,   440,   442,   444,   446,   448,   450,   452,
     454,   456,   458,   460,   462,   464,   466,   468,   470,   472,
     474,   476,   478,   484,   485,   487,   489,   492,   496,   498,
     502,   505,   507,   509,   511,   513,   515,   517,   519,   521,
     523,   525,   527,   531,   537,   539,   541,   545,   548,   550,
     553,   557,   559,   561,   564,   568,   570,   573,   575,   578,
     585,   586,   588,   592,   594,   596,   600,   603,   611,   621,
     631,   632,   633,   635,   637,   640,   642,   644,   645,   647,
     649,   652,   655,   658,   662,   665,   667,   670,   672,   674,
     676,   679,   681
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     295,     0,    -1,   166,    -1,   205,    -1,   164,    -1,   165,
      -1,   162,    -1,   163,    -1,   122,   233,   129,    -1,   206,
      -1,   210,    -1,   207,   121,   209,   128,    -1,   207,   116,
     208,    -1,   207,   147,    -1,   207,   144,    -1,   166,    -1,
     233,    -1,   211,    -1,   213,   129,    -1,   212,   129,    -1,
     214,    39,    -1,   214,    -1,   214,   231,    -1,   213,   113,
     231,    -1,   215,   248,   122,    -1,   258,    -1,   166,    -1,
     207,    -1,   147,   216,    -1,   144,   216,    -1,   217,   216,
      -1,   124,    -1,   115,    -1,   111,    -1,    97,    -1,   216,
      -1,   218,   132,   216,    -1,   218,   131,   216,    -1,   218,
     123,   216,    -1,   218,    -1,   219,   124,   218,    -1,   219,
     115,   218,    -1,   219,    -1,   220,   151,   219,    -1,   220,
     158,   219,    -1,   220,    -1,   221,   119,   220,    -1,   221,
     126,   220,    -1,   221,   149,   220,    -1,   221,   148,   220,
      -1,   221,    -1,   222,   146,   221,    -1,   222,   154,   221,
      -1,   222,    -1,   223,   110,   222,    -1,   223,    -1,   224,
     112,   223,    -1,   224,    -1,   225,   133,   224,    -1,   225,
      -1,   226,   143,   225,    -1,   226,    -1,   227,   160,   226,
      -1,   227,    -1,   228,   156,   227,    -1,   228,    -1,   228,
     230,   233,   114,   231,    -1,   125,    -1,   229,    -1,   216,
     232,   231,    -1,   118,    -1,   153,    -1,   145,    -1,   152,
      -1,   141,    -1,   159,    -1,   150,    -1,   157,    -1,   142,
      -1,   161,    -1,   155,    -1,   231,    -1,   233,   113,   231,
      -1,   229,    -1,   236,   130,    -1,   244,   130,    -1,    24,
     259,   257,   130,    -1,   237,   129,    -1,   239,    -1,   238,
      -1,   239,   240,    -1,   238,   113,   240,    -1,   250,   166,
     122,    -1,   241,   243,    -1,    -1,     8,    -1,    -1,    18,
      -1,    23,    -1,    19,    -1,   242,   256,   166,   248,    -1,
     242,   256,   248,    -1,   245,    -1,   244,   113,   166,   248,
     247,    -1,   250,    -1,   246,   247,    -1,    20,   166,    -1,
     250,   166,   248,    -1,    -1,   118,   266,    -1,    -1,   249,
      -1,   121,   234,   128,    -1,   121,   128,    -1,   256,    -1,
     251,   256,    -1,   254,   253,   252,   255,    -1,    -1,     7,
      -1,    -1,   180,    -1,    14,    -1,   181,    -1,    -1,    20,
      -1,     8,    -1,     5,    -1,    18,    -1,    23,    -1,    38,
      -1,    37,    -1,   257,    -1,   259,   257,    -1,   258,   248,
      -1,   260,   248,    -1,    39,    -1,    45,    -1,    49,    -1,
      41,    -1,    46,    -1,    47,    -1,    48,    -1,    42,    -1,
      43,    -1,    44,    -1,    50,    -1,    51,    -1,    52,    -1,
      53,    -1,    57,    -1,    61,    -1,    27,    -1,    28,    -1,
      33,    -1,   166,    -1,   178,    -1,   179,    -1,    26,    -1,
      54,    -1,    55,    -1,    56,    -1,    58,    -1,    59,    -1,
      60,    -1,    29,    -1,    30,    -1,    31,    -1,    32,    -1,
     199,    -1,   200,    -1,   201,    -1,   202,    -1,   187,    -1,
     188,    -1,   189,    -1,   190,    -1,   193,    -1,   194,    -1,
     195,    -1,   196,    -1,   183,    -1,   184,    -1,   191,    -1,
     192,    -1,   197,    -1,   198,    -1,   185,    -1,   186,    -1,
     182,    -1,    16,    -1,    22,    -1,    21,    -1,    34,   261,
     120,   262,   127,    -1,    -1,   166,    -1,   263,    -1,   262,
     263,    -1,   256,   264,   130,    -1,   265,    -1,   264,   113,
     265,    -1,   166,   248,    -1,   231,    -1,   235,    -1,   275,
      -1,   269,    -1,   267,    -1,   279,    -1,   286,    -1,   294,
      -1,   280,    -1,   271,    -1,   273,    -1,   122,   233,   129,
      -1,    95,   270,   120,   272,   127,    -1,   292,    -1,   278,
      -1,   104,   233,   114,    -1,    65,   114,    -1,   120,    -1,
     120,   127,    -1,   274,   278,   127,    -1,   277,    -1,   269,
      -1,   120,   127,    -1,   120,   278,   127,    -1,   268,    -1,
     278,   268,    -1,   130,    -1,   233,   130,    -1,    17,   122,
     233,   129,   281,   283,    -1,    -1,   268,    -1,   282,    12,
     268,    -1,   268,    -1,   233,    -1,   285,   118,   266,    -1,
     250,   166,    -1,    40,   122,   287,   284,   129,   276,   288,
      -1,    11,   287,   268,   288,    40,   122,   233,   129,   130,
      -1,    15,   122,   287,   289,   290,   291,   129,   276,   288,
      -1,    -1,    -1,   279,    -1,   267,    -1,   293,   130,    -1,
     292,    -1,   233,    -1,    -1,   292,    -1,   284,    -1,     9,
     130,    -1,     6,   130,    -1,    25,   130,    -1,    25,   233,
     130,    -1,    10,   130,    -1,   296,    -1,   295,   296,    -1,
     298,    -1,   235,    -1,   173,    -1,   174,   164,    -1,   236,
      -1,   297,   277,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   798,   798,   802,   803,   804,   810,   811,   812,   825,
     826,   827,   847,   867,   871,   878,   882,   894,  1119,  1120,
    1124,  1125,  1129,  1135,  1143,  1158,  1175,  1188,  1189,  1193,
    1197,  1207,  1208,  1209,  1210,  1223,  1224,  1228,  1232,  1243,
    1244,  1248,  1255,  1256,  1264,  1275,  1276,  1280,  1284,  1288,
    1295,  1296,  1300,  1307,  1308,  1319,  1320,  1331,  1332,  1343,
    1344,  1351,  1352,  1359,  1360,  1367,  1368,  1389,  1393,  1394,
    1416,  1421,  1422,  1423,  1431,  1432,  1433,  1441,  1449,  1457,
    1465,  1476,  1480,  1491,  1495,  1500,  1504,  1545,  1549,  1550,
    1554,  1555,  1560,  1598,  1605,  1606,  1610,  1611,  1612,  1613,
    1617,  1629,  1644,  1675,  1701,  1708,  1715,  1725,  1735,  1736,
    1740,  1741,  1744,  1745,  1751,  1758,  1771,  1789,  1792,  1804,
    1807,  1815,  1823,  1835,  1838,  1847,  1856,  1863,  1886,  1902,
    1912,  1919,  1920,  1929,  1973,  1988,  2011,  2012,  2013,  2014,
    2015,  2016,  2017,  2018,  2019,  2020,  2021,  2022,  2023,  2024,
    2025,  2026,  2027,  2028,  2030,  2045,  2046,  2057,  2058,  2059,
    2060,  2061,  2062,  2063,  2064,  2065,  2067,  2068,  2071,  2072,
    2073,  2074,  2075,  2076,  2077,  2078,  2079,  2080,  2081,  2082,
    2083,  2084,  2085,  2086,  2087,  2088,  2089,  2090,  2091,  2096,
    2111,  2112,  2116,  2238,  2239,  2243,  2244,  2258,  2280,  2281,
    2289,  2300,  2304,  2308,  2313,  2320,  2321,  2322,  2323,  2403,
    2404,  2405,  2410,  2420,  2446,  2447,  2451,  2463,  2481,  2489,
    2490,  2500,  2501,  2505,  2509,  2516,  2521,  2528,  2529,  2533,
    2554,  2570,  2578,  2584,  2599,  2600,  2610,  2617,  2631,  2644,
    2664,  2673,  2683,  2684,  2688,  2692,  2693,  2696,  2699,  2700,
    2704,  2705,  2706,  2707,  2708,  2714,  2720,  2728,  2729,  2734,
    2739,  2754,  2771
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "TOK_INVALID_TOKEN", "TOK_ARRAY_LENGTH",
  "TOK_ATTRIBUTE", "TOK_BREAK", "TOK_CENTROID", "TOK_CONST",
  "TOK_CONTINUE", "TOK_DISCARD", "TOK_DO", "TOK_ELSE", "TOK_FALSE",
  "TOK_FLAT", "TOK_FOR", "TOK_HIGHP", "TOK_IF", "TOK_IN", "TOK_INOUT",
  "TOK_INVARIANT", "TOK_LOWP", "TOK_MEDIUMP", "TOK_OUT", "TOK_PRECISION",
  "TOK_RETURN", "TOK_SAMPLER1D", "TOK_SAMPLER2D", "TOK_SAMPLER3D",
  "TOK_SAMPLER1DSHADOW", "TOK_SAMPLER2DSHADOW", "TOK_SAMPLER2DRECT",
  "TOK_SAMPLER2DRECTSHADOW", "TOK_SAMPLERCUBE", "TOK_STRUCT",
  "TOK_SUPER_PRECISION", "TOK_TRUE", "TOK_UNIFORM", "TOK_VARYING",
  "TOK_VOID", "TOK_WHILE", "TOK_BOOL", "TOK_BVEC2", "TOK_BVEC3",
  "TOK_BVEC4", "TOK_FLOAT", "TOK_VEC2", "TOK_VEC3", "TOK_VEC4", "TOK_INT",
  "TOK_IVEC2", "TOK_IVEC3", "TOK_IVEC4", "TOK_MAT2X2", "TOK_MAT2X3",
  "TOK_MAT2X4", "TOK_MAT3X2", "TOK_MAT3X3", "TOK_MAT3X4", "TOK_MAT4X2",
  "TOK_MAT4X3", "TOK_MAT4X4", "TOK_ASM", "TOK_CAST", "TOK_CLASS",
  "TOK_DEFAULT", "TOK_DOUBLE", "TOK_DVEC2", "TOK_DVEC3", "TOK_DVEC4",
  "TOK_ENUM", "TOK_EXTERN", "TOK_EXTERNAL", "TOK_FIXED", "TOK_FVEC2",
  "TOK_FVEC3", "TOK_FVEC4", "TOK_GOTO", "TOK_HALF", "TOK_HVEC2",
  "TOK_HVEC3", "TOK_HVEC4", "TOK_INLINE", "TOK_INPUT", "TOK_INTERFACE",
  "TOK_LONG", "TOK_NAMESPACE", "TOK_NOINLINE", "TOK_OUTPUT", "TOK_PACKED",
  "TOK_PUBLIC", "TOK_SAMPLER3DRECT", "TOK_SHORT", "TOK_SIZEOF",
  "TOK_STATIC", "TOK_SWITCH", "TOK_TEMPLATE", "TOK_TILDE", "TOK_THIS",
  "TOK_TYPEDEF", "TOK_UNION", "TOK_UNSIGNED", "TOK_USING", "TOK_VOLATILE",
  "TOK_CASE", "TOK_CHAR", "TOK_BYTE", "TOK_INCOMMENT", "TOK_OUTCOMMENT",
  "TOK_COMMENT", "TOK_AMPERSAND", "TOK_BANG", "TOK_CARET", "TOK_COMMA",
  "TOK_COLON", "TOK_DASH", "TOK_DOT", "TOK_DOTDOT", "TOK_EQUAL",
  "TOK_LEFT_ANGLE", "TOK_LEFT_BRACE", "TOK_LEFT_BRACKET", "TOK_LEFT_PAREN",
  "TOK_PERCENT", "TOK_PLUS", "TOK_QUESTION", "TOK_RIGHT_ANGLE",
  "TOK_RIGHT_BRACE", "TOK_RIGHT_BRACKET", "TOK_RIGHT_PAREN",
  "TOK_SEMICOLON", "TOK_SLASH", "TOK_STAR", "TOK_VERTICAL_BAR",
  "TOK_APOSTROPHE", "TOK_AT", "TOK_BACK_SLASH", "TOK_DOLLAR", "TOK_HASH",
  "TOK_SPEECH_MARK", "TOK_UNDERSCORE", "TOK_ADD_ASSIGN", "TOK_AND_ASSIGN",
  "TOK_AND_OP", "TOK_DEC_OP", "TOK_DIV_ASSIGN", "TOK_EQ_OP", "TOK_INC_OP",
  "TOK_GE_OP", "TOK_LE_OP", "TOK_LEFT_ASSIGN", "TOK_LEFT_OP",
  "TOK_MOD_ASSIGN", "TOK_MUL_ASSIGN", "TOK_NE_OP", "TOK_OR_ASSIGN",
  "TOK_OR_OP", "TOK_RIGHT_ASSIGN", "TOK_RIGHT_OP", "TOK_SUB_ASSIGN",
  "TOK_XOR_OP", "TOK_XOR_ASSIGN", "TOK_FLOATCONSTANT", "TOK_BOOLCONSTANT",
  "TOK_INTCONSTANT", "TOK_UINTCONSTANT", "TOK_IDENTIFIER", "TOK_TYPE_NAME",
  "TOK_PROGRAMHEADER", "TOK_NEWLINE", "TOK_CARRIGE_RETURN", "TOK_TAB",
  "TOK_VTAB", "TOK_LANGUAGE_VERSION", "TOK_EXTENSION_CHANGE", "TOK_STRING",
  "TOK_ENDOFSTRING", "TOK_TERMINATEPARSING", "TOK_SAMPLERSTREAMIMG",
  "TOK_SAMPLEREXTERNALOES", "TOK_SMOOTH", "TOK_NOPERSPECTIVE",
  "TOK_SAMPLERCUBESHADOW", "TOK_SAMPLER1DARRAY", "TOK_SAMPLER2DARRAY",
  "TOK_SAMPLER1DARRAYSHADOW", "TOK_SAMPLER2DARRAYSHADOW", "TOK_ISAMPLER1D",
  "TOK_ISAMPLER2D", "TOK_ISAMPLER3D", "TOK_ISAMPLERCUBE",
  "TOK_ISAMPLER1DARRAY", "TOK_ISAMPLER2DARRAY", "TOK_USAMPLER1D",
  "TOK_USAMPLER2D", "TOK_USAMPLER3D", "TOK_USAMPLERCUBE",
  "TOK_USAMPLER1DARRAY", "TOK_USAMPLER2DARRAY", "TOK_UINT", "TOK_UVEC2",
  "TOK_UVEC3", "TOK_UVEC4", "TOK_HASHHASH", "$accept",
  "variable_identifier", "primary_expression", "postfix_expression",
  "field_selection", "integer_expression", "function_call",
  "function_call_generic", "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_xor_expression",
  "logical_or_expression", "conditional_expression",
  "TOK_QUESTION_increase_conditional_level", "assignment_expression",
  "assignment_operator", "expression", "constant_expression",
  "declaration", "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declaration", "parameter_type_qualifier_opt",
  "parameter_qualifier_opt", "parameter_declarator",
  "init_declarator_list", "single_declaration",
  "single_declaration_identifier_node", "initializer_opt",
  "array_specifier_opt", "array_specifier", "fully_specified_type",
  "type_qualifier", "centroid_qualifier_opt",
  "interpolation_qualifier_opt", "invariant_qualifier_opt",
  "storage_qualifier", "type_specifier", "type_specifier_no_prec",
  "type_specifier_e", "precision_qualifier", "struct_specifier",
  "TOK_IDENTIFIER_opt", "struct_declaration_list", "struct_declaration",
  "struct_declarator_list", "struct_declarator", "initializer",
  "declaration_statement", "statement", "simple_statement",
  "switch_statement_expression", "switch_statement",
  "switch_statement_list", "case_label",
  "compound_statement_TOK_LEFT_BRACE", "compound_statement",
  "statement_no_new_scope", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_statement_increase_scope", "selection_rest_statement_a",
  "selection_rest_statement", "condition", "condition_identifier_node",
  "iteration_statement", "loop_increase_scope", "loop_decrease_scope",
  "for_init_statement", "for_cond_statement", "for_loop_statement",
  "empty", "conditionopt", "jump_statement", "translation_unit",
  "external_declaration", "function_definition_prototype",
  "function_definition", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   204,   205,   206,   206,   206,   206,   206,   206,   207,
     207,   207,   207,   207,   207,   208,   209,   210,   211,   211,
     212,   212,   213,   213,   214,   215,   215,   216,   216,   216,
     216,   217,   217,   217,   217,   218,   218,   218,   218,   219,
     219,   219,   220,   220,   220,   221,   221,   221,   221,   221,
     222,   222,   222,   223,   223,   224,   224,   225,   225,   226,
     226,   227,   227,   228,   228,   229,   229,   230,   231,   231,
     232,   232,   232,   232,   232,   232,   232,   232,   232,   232,
     232,   233,   233,   234,   235,   235,   235,   236,   237,   237,
     238,   238,   239,   240,   241,   241,   242,   242,   242,   242,
     243,   243,   244,   244,   245,   245,   245,   246,   247,   247,
     248,   248,   249,   249,   250,   250,   251,   252,   252,   253,
     253,   253,   253,   254,   254,   255,   255,   255,   255,   255,
     255,   256,   256,   257,   257,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   258,
     258,   258,   258,   258,   258,   258,   258,   258,   258,   259,
     259,   259,   260,   261,   261,   262,   262,   263,   264,   264,
     265,   266,   267,   268,   268,   269,   269,   269,   269,   269,
     269,   269,   270,   271,   272,   272,   273,   273,   274,   275,
     275,   276,   276,   277,   277,   278,   278,   279,   279,   280,
     281,   282,   283,   283,   284,   284,   285,   286,   286,   286,
     287,   288,   289,   289,   290,   291,   291,   292,   293,   293,
     294,   294,   294,   294,   294,   295,   295,   296,   296,   296,
     296,   297,   298
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     3,     1,
       1,     4,     3,     2,     2,     1,     1,     1,     2,     2,
       2,     1,     2,     3,     3,     1,     1,     1,     2,     2,
       2,     1,     1,     1,     1,     1,     3,     3,     3,     1,
       3,     3,     1,     3,     3,     1,     3,     3,     3,     3,
       1,     3,     3,     1,     3,     1,     3,     1,     3,     1,
       3,     1,     3,     1,     3,     1,     5,     1,     1,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     3,     1,     2,     2,     4,     2,     1,     1,
       2,     3,     3,     2,     0,     1,     0,     1,     1,     1,
       4,     3,     1,     5,     1,     2,     2,     3,     0,     2,
       0,     1,     3,     2,     1,     2,     4,     0,     1,     0,
       1,     1,     1,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     5,     0,     1,     1,     2,     3,     1,     3,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     3,     5,     1,     1,     3,     2,     1,     2,
       3,     1,     1,     2,     3,     1,     2,     1,     2,     6,
       0,     1,     3,     1,     1,     3,     2,     7,     9,     9,
       0,     0,     1,     1,     2,     1,     1,     0,     1,     1,
       2,     2,     2,     3,     2,     1,     2,     1,     1,     1,
       2,     1,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
     123,   189,   124,   191,   190,     0,   157,   151,   152,   164,
     165,   166,   167,   153,   193,   135,   138,   142,   143,   144,
     136,   139,   140,   141,   137,   145,   146,   147,   148,   158,
     159,   160,   149,   161,   162,   163,   150,   154,   259,     0,
     155,   156,   188,   180,   181,   186,   187,   172,   173,   174,
     175,   182,   183,   176,   177,   178,   179,   184,   185,   168,
     169,   170,   171,   258,   261,     0,    89,    94,     0,   102,
     108,   104,     0,   119,   114,   131,   110,     0,   110,   123,
     255,     0,   257,   106,     0,   194,     0,   260,    84,    87,
      94,    95,    90,    96,     0,    85,     0,   105,   110,   115,
     121,   120,   122,   117,     0,   133,   111,   132,   134,     1,
     256,   123,   262,     0,     0,    91,    97,    99,    98,     0,
      93,   110,    34,    33,    32,     0,    31,     0,     0,     6,
       7,     4,     5,     2,     3,     9,    27,    10,    17,     0,
       0,    21,   110,    35,     0,    39,    42,    45,    50,    53,
      55,    57,    59,    61,    63,    65,    68,   201,    25,   109,
      92,   107,   118,     0,   113,    35,    83,     0,     0,     0,
       0,   240,     0,     0,     0,     0,     0,     0,     0,   218,
     223,   227,    81,     0,   202,     0,   110,   205,   225,   204,
     210,   211,   123,   203,   123,   206,   209,   207,   208,    86,
       0,     0,   195,   110,   108,     0,    29,    28,     0,     0,
      14,    13,    19,     0,    18,   135,    22,     0,    70,    74,
      78,    72,    76,    73,    71,    80,    77,    75,    79,     0,
      30,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    67,
       0,     0,   126,   125,   127,   128,   130,   129,   116,   112,
     251,   250,   254,   123,   240,     0,   252,     0,   240,   217,
       0,     0,     0,   219,     0,   228,   123,   224,   226,   110,
       0,   198,   192,   196,   110,   101,   103,     8,    15,    12,
       0,    16,    23,    24,    69,    38,    37,    36,    41,    40,
      43,    44,    46,    47,    49,    48,    51,    52,    54,    56,
      58,    60,    62,    64,     0,   241,   123,     0,   253,   123,
       0,   123,   216,    82,   220,   200,     0,   197,   100,    11,
       0,     0,   243,   242,   123,   230,   124,   234,     0,     0,
       0,   212,     0,   123,   214,   199,    66,     0,   249,   247,
     248,     0,   123,   236,   123,     0,   213,     0,   246,     0,
     245,   244,   233,     0,   229,   222,   241,   221,   235,     0,
     123,   123,   237,     0,   241,   232,   238,   239
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,   134,   135,   136,   289,   290,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   156,   251,   182,   229,   183,
     167,   184,   185,    65,    66,    67,    92,    93,   119,   120,
      68,    69,    70,    97,   105,   106,    71,    72,   163,   103,
      73,   258,    74,    75,   158,    77,    78,    86,   201,   202,
     280,   281,   159,   187,   188,   189,   271,   190,   342,   191,
     192,   193,   366,   367,   194,   195,   196,   352,   363,   364,
     339,   340,   197,   263,   331,   334,   349,   359,   344,   351,
     198,    79,    80,    81,    82
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -344
static const yytype_int16 yypact[] =
{
    2378,  -344,  -141,  -344,  -344,    45,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  -344,  -138,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -127,
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  -344,   -55,   -65,   -16,     1,   -98,  -344,
     -35,   -64,  2472,   -11,  -344,  -344,   -14,  3357,   -14,   251,
    -344,    -7,  -344,  -344,  3357,  -344,    -3,  -344,  -344,  -344,
     119,  -344,  -344,    97,   -33,  -344,  3003,  -344,   -22,  -344,
    -344,  -344,  -344,   132,  2649,  -344,  -344,  -344,  -344,  -344,
    -344,   448,  -344,    15,  2472,  -344,  -344,  -344,  -344,  2472,
    -344,   -14,  -344,  -344,  -344,  3003,  -344,  3003,  3003,  -344,
    -344,  -344,  -344,  -114,  -344,  -344,  -103,  -344,  -344,    17,
     -87,  3180,   -14,   -49,  3003,   -50,   -30,   -63,   -90,   -83,
      38,    37,    19,     7,    -9,  -111,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,    12,  -344,  -344,  -344,    25,    26,    29,
      30,  -344,    33,    39,  2826,    40,    50,    43,  3003,    41,
    -344,  -344,  -344,   -97,  -344,   -55,     4,  -344,  -344,  -344,
    -344,  -344,  1433,  -344,   645,  -344,  -344,  -344,  -344,  -344,
       5,  2426,  -344,  -109,   -35,   -86,  -344,  -344,     6,  3003,
    -344,  -344,  -344,  3003,  -344,    44,  -344,    52,  -344,  -344,
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  3003,
    -344,  3003,  3003,  3003,  3003,  3003,  3003,  3003,  3003,  3003,
    3003,  3003,  3003,  3003,  3003,  3003,  3003,  3003,  3003,  -344,
    3003,  3003,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,  -344,  1433,  -344,  3003,  -344,   -92,  -344,  -344,
    3003,    46,    18,  -344,  3003,  -344,   842,  -344,  -344,   -14,
     -91,  -344,  -344,  -344,   -14,  -344,  -344,  -344,  -344,  -344,
      47,    54,  -344,  -344,  -344,  -344,  -344,  -344,   -50,   -50,
     -30,   -30,   -63,   -63,   -63,   -63,   -90,   -90,   -83,    38,
      37,    19,     7,    -9,    24,  -344,  1817,   -73,  -344,  2191,
     -59,  1039,  -344,  -344,  -344,  -344,     5,  -344,  -344,  -344,
    3003,   136,  -344,  -344,  2004,  -344,  -344,    54,    20,    48,
      60,  -344,    53,  1236,  -344,  -344,  -344,    59,  -344,  3003,
    -344,    55,  1433,  -344,  1630,  3003,  -344,  3003,    54,    58,
    -344,  -344,   171,   172,  -344,  -344,  -344,  -344,  -344,   -53,
    1630,  1433,  -344,    61,  -344,  -344,  -344,  -344
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,  -344,
    -344,  -344,   142,  -344,   -94,   -93,  -117,   -85,   -56,   -52,
     -57,   -48,   -58,   -47,  -344,    91,  -344,   -95,  -344,  -123,
    -344,    10,    11,  -344,  -344,  -344,   107,  -344,  -344,  -344,
    -344,  -344,  -344,    -6,   -74,  -344,  -300,  -344,  -344,  -344,
    -344,  -344,   -66,    14,     0,   195,  -344,  -344,  -344,     3,
    -344,  -124,  -149,  -108,  -189,  -292,  -344,  -344,  -344,  -344,
    -344,  -344,  -163,   128,  -185,  -104,  -344,  -344,  -344,  -344,
    -121,  -344,  -344,  -159,  -343,  -344,  -344,  -344,  -269,  -344,
    -344,  -344,   135,  -344,  -344
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -248
static const yytype_int16 yytable[] =
{
      76,   157,   205,   100,   108,   278,    99,   276,   -26,    91,
      63,    64,   104,   208,   249,    94,   274,   252,   209,   338,
     253,   274,   326,   372,   161,    83,   213,   274,    85,   238,
     254,   377,    95,   275,   338,   255,   239,    87,   318,   327,
     274,   210,   214,   287,   211,   250,   216,   204,   200,   256,
     257,   267,  -154,   203,   274,   272,   335,   284,   240,   241,
     274,     1,   365,   242,    89,   350,     3,     4,   217,   218,
     341,   243,    76,   231,   315,    88,   373,    76,   365,    76,
     360,   232,   233,    96,    76,   234,   291,   278,   236,    63,
      64,   107,   219,   220,   235,   237,   221,    90,   113,   104,
     160,   222,    98,   223,   224,   316,   225,   104,   226,   319,
     227,   186,   228,   111,    76,   116,   117,   114,   292,    76,
     118,   302,   303,   304,   305,   104,   -25,    91,   314,   285,
     -88,   274,   322,   121,   294,   200,   343,   274,   330,   162,
     298,   299,   317,   300,   301,   199,   212,   320,   244,   245,
     247,   248,   246,   259,   278,   264,   260,   306,   307,   261,
     262,   265,   268,   362,   269,   270,   321,   274,   273,   101,
     102,   279,   288,   -20,   293,   329,   347,   354,   355,   323,
     356,   357,   375,  -231,   371,   361,   353,   370,   308,   310,
     312,   376,   186,   309,   186,   166,   337,   115,   286,   311,
      84,    76,   345,   313,   283,   325,   368,   374,   332,   112,
     328,   337,   333,   348,   110,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   358,     0,     0,     0,
       0,     0,     0,     0,   369,   346,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   165,     0,     0,     0,
       0,   109,     0,     0,     0,     0,     0,     0,     0,     0,
     157,     0,     0,   186,     0,     0,     0,     1,     0,   206,
     207,     2,     3,     4,     0,     5,   186,     6,     7,     8,
       9,    10,    11,    12,    13,    14,   230,     0,     0,     0,
      15,     0,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,     0,     0,     0,   186,     0,     0,   186,
       0,   186,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   186,     0,     0,     0,     0,     0,
       0,     0,     0,   186,     0,     0,     0,     0,     0,     0,
       0,     0,   186,     0,   186,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     186,   186,     0,   295,   296,   297,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,     0,   165,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    37,     0,     0,
       0,     0,     0,     0,    38,    39,     0,     0,     0,    40,
      41,     0,     0,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,   168,     0,     0,   169,   170,   171,
       0,     0,     0,   172,     1,   173,     0,     0,     2,     3,
       4,     0,     5,   174,     6,     7,     8,     9,    10,    11,
      12,    13,    14,     0,     0,     0,     0,    15,   175,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
       0,     0,     0,   176,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,     0,   122,     0,     0,     0,     0,
       0,     0,   178,     0,     0,     0,     0,     0,     0,   123,
       0,     0,     0,   124,     0,     0,     0,     0,   179,     0,
     125,     0,   126,     0,     0,   180,     0,     0,   181,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   127,     0,     0,   128,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     129,   130,   131,   132,   133,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    40,    41,     0,     0,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,   168,     0,     0,   169,   170,   171,     0,     0,     0,
     172,     1,   173,     0,     0,     2,     3,     4,     0,     5,
     174,     6,     7,     8,     9,    10,    11,    12,    13,    14,
       0,     0,     0,     0,    15,   175,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,     0,     0,     0,
     176,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,     0,   122,     0,     0,     0,     0,     0,     0,   178,
       0,     0,     0,     0,     0,     0,   123,     0,     0,     0,
     124,     0,     0,     0,     0,   179,     0,   125,     0,   126,
       0,     0,   277,     0,     0,   181,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   127,
       0,     0,   128,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   129,   130,   131,
     132,   133,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    40,    41,     0,     0,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,   168,     0,
       0,   169,   170,   171,     0,     0,     0,   172,     1,   173,
       0,     0,     2,     3,     4,     0,     5,   174,     6,     7,
       8,     9,    10,    11,    12,    13,    14,     0,     0,     0,
       0,    15,   175,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,     0,     0,     0,   176,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   177,     0,   122,
       0,     0,     0,     0,     0,     0,   178,     0,     0,     0,
       0,     0,     0,   123,     0,     0,     0,   124,     0,     0,
       0,     0,   179,     0,   125,     0,   126,     0,     0,   324,
       0,     0,   181,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   127,     0,     0,   128,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   129,   130,   131,   132,   133,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      40,    41,     0,     0,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,   168,     0,     0,   169,   170,
     171,     0,     0,     0,   172,     1,   173,     0,     0,     2,
       3,     4,     0,     5,   174,     6,     7,     8,     9,    10,
      11,    12,    13,    14,     0,     0,     0,     0,    15,   175,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,     0,     0,     0,   176,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   177,     0,   122,     0,     0,     0,
       0,     0,     0,   178,     0,     0,     0,     0,     0,     0,
     123,     0,     0,     0,   124,     0,     0,     0,     0,   179,
       0,   125,     0,   126,     0,     0,  -247,     0,     0,   181,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   127,     0,     0,   128,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   129,   130,   131,   132,   133,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    40,    41,     0,
       0,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,   168,     0,     0,   169,   170,   171,     0,     0,
       0,   172,     1,   173,     0,     0,     2,     3,     4,     0,
       5,   174,     6,     7,     8,     9,    10,    11,    12,    13,
      14,     0,     0,     0,     0,    15,   175,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,     0,     0,
       0,   176,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   177,     0,   122,     0,     0,     0,     0,     0,     0,
     178,     0,     0,     0,     0,     0,     0,   123,     0,     0,
       0,   124,     0,     0,     0,     0,   179,     0,   125,     0,
     126,     0,     0,  -215,     0,     0,   181,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     127,     0,     0,   128,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   129,   130,
     131,   132,   133,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    40,    41,     0,     0,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,   168,
       0,     0,   169,   170,   171,     0,     0,     0,   172,     1,
     173,     0,     0,     2,     3,     4,     0,     5,   174,     6,
       7,     8,     9,    10,    11,    12,    13,    14,     0,     0,
       0,     0,    15,   175,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,     0,     0,     0,   176,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   177,     0,
     122,     0,     0,     0,     0,     0,     0,   178,     0,     0,
       0,     0,     0,     0,   123,     0,     0,     0,   124,     0,
       0,     0,     0,   179,     0,   125,     0,   126,     0,     0,
       0,     0,     0,   181,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   127,     0,     0,
     128,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   129,   130,   131,   132,   133,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    40,    41,     0,     0,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,   168,     0,     0,   169,
     170,   171,     0,     0,     0,   172,     1,   173,     0,     0,
       2,     3,     4,     0,     5,   174,     6,     7,     8,     9,
      10,    11,    12,    13,    14,     0,     0,     0,     0,    15,
     175,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,     0,     0,     0,   176,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   177,     0,   122,     0,     0,
       0,     0,     0,     0,   178,     0,     0,     0,     0,     0,
       0,   123,     0,     0,     0,   124,     0,     0,     0,     0,
     111,     0,   125,     0,   126,     0,     0,     0,     0,     0,
     181,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   127,     0,     0,   128,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   129,   130,   131,   132,   133,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    40,    41,
       0,     0,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     1,     0,     0,     0,     2,     3,     4,
       0,     5,     0,     6,     7,     8,     9,    10,    11,    12,
      13,    14,     0,     0,     0,     0,    15,     0,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   122,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   123,     0,
       0,     0,   124,     0,     0,     0,     0,     0,     0,   125,
       0,   126,     0,     0,     0,     0,     0,   181,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   127,     0,     0,   128,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   129,
     130,   131,   132,   133,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    40,    41,     0,     0,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
       1,     0,     0,     0,   336,     3,     4,     0,     0,     0,
       6,     7,     8,     9,    10,    11,    12,    13,    14,     0,
       0,     0,     0,    15,     0,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   122,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   123,     0,     0,     0,   124,
       0,     0,     0,     0,     0,     0,   125,     0,   126,     0,
       0,     0,     0,     0,  -247,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   127,     0,
       0,   128,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   129,   130,   131,   132,
     133,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    40,    41,     0,     0,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     1,     0,     0,
       0,   336,     3,     4,     0,     0,     0,     6,     7,     8,
       9,    10,    11,    12,    13,    14,     0,     0,     0,     0,
      15,     0,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   122,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   123,     0,     0,     0,   124,     0,     0,     0,
       0,     0,     0,   125,     0,   126,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   127,     0,     0,   128,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   129,   130,   131,   132,   133,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    40,
      41,     0,     0,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     1,     0,     0,     0,     2,     3,
       4,     0,     5,     0,     6,     7,     8,     9,    10,    11,
      12,    13,    14,     0,     0,     0,     0,    15,     0,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
       0,     0,     1,     0,     0,     0,     0,     3,     4,     0,
       0,     0,     6,     7,     8,     9,    10,    11,    12,    13,
      14,     0,     0,     0,     0,    15,     0,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,     1,     0,
       0,     0,     0,     3,     4,     0,     0,     0,     6,     7,
       8,     9,    10,    11,    12,    13,    14,     0,     0,     0,
       0,    15,     0,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    37,     0,     0,     0,     0,     0,
       0,    38,    39,   282,     0,     0,    40,    41,     0,     0,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    37,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    40,    41,     0,     0,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    37,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      40,    41,     0,     0,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     6,     7,     8,     9,    10,
      11,    12,    13,     0,     0,     0,     0,     0,    15,     0,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   122,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,     0,     0,     0,   124,     0,     0,     0,     0,     0,
       0,   125,     0,   126,     0,     0,     0,   164,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   127,     0,     0,   128,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   129,   130,   131,   132,   133,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    40,    41,     0,
       0,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,     6,     7,     8,     9,    10,    11,    12,    13,
       0,     0,     0,     0,     0,    15,     0,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   122,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   123,     0,     0,
       0,   124,     0,     0,     0,     0,     0,     0,   125,     0,
     126,     0,     0,     0,     0,     0,   266,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     127,     0,     0,   128,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   129,   130,
     131,   132,   133,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    40,    41,     0,     0,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     6,
       7,     8,     9,    10,    11,    12,    13,     0,     0,     0,
       0,     0,    15,     0,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     122,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   123,     0,     0,     0,   124,     0,
       0,     0,     0,     0,     0,   125,     0,   126,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   127,     0,     0,
     128,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   129,   130,   131,   132,   133,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    40,    41,     0,     0,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     6,     7,     8,     9,
      10,    11,    12,    13,     0,     0,     0,     0,     0,   215,
       0,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   122,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   123,     0,     0,     0,   124,     0,     0,     0,     0,
       0,     0,   125,     0,   126,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   127,     0,     0,   128,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   129,   130,   131,   132,   133,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    40,    41,
       0,     0,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     6,     7,     8,     9,    10,    11,    12,
      13,    14,     0,     0,     0,     0,    15,     0,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    37,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    40,    41,     0,     0,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62
};

static const yytype_int16 yycheck[] =
{
       0,    96,   125,    14,    78,   194,    72,   192,   122,     8,
       0,     0,   121,   116,   125,   113,   113,     5,   121,   319,
       8,   113,   113,   366,    98,   166,   113,   113,   166,   119,
      18,   374,   130,   130,   334,    23,   126,   164,   130,   130,
     113,   144,   129,   129,   147,   156,   141,   121,   114,    37,
      38,   174,   166,   119,   113,   178,   129,   166,   148,   149,
     113,    16,   354,   146,   129,   334,    21,    22,   142,   118,
     129,   154,    72,   123,   263,   130,   129,    77,   370,    79,
     349,   131,   132,   118,    84,   115,   209,   276,   151,    79,
      79,    77,   141,   142,   124,   158,   145,   113,    84,   121,
     122,   150,   166,   152,   153,   264,   155,   121,   157,   268,
     159,   111,   161,   120,   114,    18,    19,   120,   213,   119,
      23,   238,   239,   240,   241,   121,   122,     8,   251,   203,
     129,   113,   114,   166,   229,   201,   321,   113,   114,     7,
     234,   235,   265,   236,   237,   130,   129,   270,   110,   112,
     143,   160,   133,   128,   343,   122,   130,   242,   243,   130,
     130,   122,   122,   352,   114,   122,   120,   113,   127,   180,
     181,   166,   166,   129,   122,   128,    40,   129,   118,   274,
     127,   122,   371,    12,    12,   130,   166,   129,   244,   246,
     248,   130,   192,   245,   194,   104,   319,    90,   204,   247,
       5,   201,   326,   250,   201,   279,   355,   370,   316,    81,
     284,   334,   316,   334,    79,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   349,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   357,   330,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   104,    -1,    -1,    -1,
      -1,     0,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     355,    -1,    -1,   263,    -1,    -1,    -1,    16,    -1,   127,
     128,    20,    21,    22,    -1,    24,   276,    26,    27,    28,
      29,    30,    31,    32,    33,    34,   144,    -1,    -1,    -1,
      39,    -1,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    -1,    -1,    -1,   316,    -1,    -1,   319,
      -1,   321,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   334,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   343,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   352,    -1,   354,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     370,   371,    -1,   231,   232,   233,   234,   235,   236,   237,
     238,   239,   240,   241,   242,   243,   244,   245,   246,   247,
     248,    -1,   250,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   166,    -1,    -1,
      -1,    -1,    -1,    -1,   173,   174,    -1,    -1,    -1,   178,
     179,    -1,    -1,   182,   183,   184,   185,   186,   187,   188,
     189,   190,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,   202,     6,    -1,    -1,     9,    10,    11,
      -1,    -1,    -1,    15,    16,    17,    -1,    -1,    20,    21,
      22,    -1,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,    -1,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,    -1,
      -1,    -1,   104,    -1,    -1,    -1,    -1,    -1,    -1,   111,
      -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,   120,    -1,
     122,    -1,   124,    -1,    -1,   127,    -1,    -1,   130,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   144,    -1,    -1,   147,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     162,   163,   164,   165,   166,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   178,   179,    -1,    -1,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,     6,    -1,    -1,     9,    10,    11,    -1,    -1,    -1,
      15,    16,    17,    -1,    -1,    20,    21,    22,    -1,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      -1,    -1,    -1,    -1,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    -1,    -1,    -1,
      65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    -1,    97,    -1,    -1,    -1,    -1,    -1,    -1,   104,
      -1,    -1,    -1,    -1,    -1,    -1,   111,    -1,    -1,    -1,
     115,    -1,    -1,    -1,    -1,   120,    -1,   122,    -1,   124,
      -1,    -1,   127,    -1,    -1,   130,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   144,
      -1,    -1,   147,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,   163,   164,
     165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   178,   179,    -1,    -1,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,     6,    -1,
      -1,     9,    10,    11,    -1,    -1,    -1,    15,    16,    17,
      -1,    -1,    20,    21,    22,    -1,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      -1,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    -1,    -1,    -1,    65,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,    97,
      -1,    -1,    -1,    -1,    -1,    -1,   104,    -1,    -1,    -1,
      -1,    -1,    -1,   111,    -1,    -1,    -1,   115,    -1,    -1,
      -1,    -1,   120,    -1,   122,    -1,   124,    -1,    -1,   127,
      -1,    -1,   130,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   144,    -1,    -1,   147,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   162,   163,   164,   165,   166,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     178,   179,    -1,    -1,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,     6,    -1,    -1,     9,    10,
      11,    -1,    -1,    -1,    15,    16,    17,    -1,    -1,    20,
      21,    22,    -1,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    -1,    -1,    -1,    -1,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,    -1,
      -1,    -1,    -1,   104,    -1,    -1,    -1,    -1,    -1,    -1,
     111,    -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,   120,
      -1,   122,    -1,   124,    -1,    -1,   127,    -1,    -1,   130,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   144,    -1,    -1,   147,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   162,   163,   164,   165,   166,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   178,   179,    -1,
      -1,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,     6,    -1,    -1,     9,    10,    11,    -1,    -1,
      -1,    15,    16,    17,    -1,    -1,    20,    21,    22,    -1,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    -1,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
      -1,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    -1,    -1,    -1,    -1,    -1,    -1,
     104,    -1,    -1,    -1,    -1,    -1,    -1,   111,    -1,    -1,
      -1,   115,    -1,    -1,    -1,    -1,   120,    -1,   122,    -1,
     124,    -1,    -1,   127,    -1,    -1,   130,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     144,    -1,    -1,   147,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,   163,
     164,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   178,   179,    -1,    -1,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,     6,
      -1,    -1,     9,    10,    11,    -1,    -1,    -1,    15,    16,
      17,    -1,    -1,    20,    21,    22,    -1,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    -1,    -1,
      -1,    -1,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    -1,    -1,    -1,    65,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,
      97,    -1,    -1,    -1,    -1,    -1,    -1,   104,    -1,    -1,
      -1,    -1,    -1,    -1,   111,    -1,    -1,    -1,   115,    -1,
      -1,    -1,    -1,   120,    -1,   122,    -1,   124,    -1,    -1,
      -1,    -1,    -1,   130,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   144,    -1,    -1,
     147,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   162,   163,   164,   165,   166,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   178,   179,    -1,    -1,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,     6,    -1,    -1,     9,
      10,    11,    -1,    -1,    -1,    15,    16,    17,    -1,    -1,
      20,    21,    22,    -1,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    -1,    -1,    -1,    65,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    -1,    -1,
      -1,    -1,    -1,    -1,   104,    -1,    -1,    -1,    -1,    -1,
      -1,   111,    -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,
     120,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,    -1,
     130,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   144,    -1,    -1,   147,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   162,   163,   164,   165,   166,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   178,   179,
      -1,    -1,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,    16,    -1,    -1,    -1,    20,    21,    22,
      -1,    24,    -1,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    39,    -1,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    97,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   111,    -1,
      -1,    -1,   115,    -1,    -1,    -1,    -1,    -1,    -1,   122,
      -1,   124,    -1,    -1,    -1,    -1,    -1,   130,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   144,    -1,    -1,   147,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,
     163,   164,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   178,   179,    -1,    -1,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202,
      16,    -1,    -1,    -1,    20,    21,    22,    -1,    -1,    -1,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    -1,
      -1,    -1,    -1,    39,    -1,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    97,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   111,    -1,    -1,    -1,   115,
      -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,   124,    -1,
      -1,    -1,    -1,    -1,   130,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   144,    -1,
      -1,   147,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   162,   163,   164,   165,
     166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   178,   179,    -1,    -1,   182,   183,   184,   185,
     186,   187,   188,   189,   190,   191,   192,   193,   194,   195,
     196,   197,   198,   199,   200,   201,   202,    16,    -1,    -1,
      -1,    20,    21,    22,    -1,    -1,    -1,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    -1,    -1,    -1,    -1,
      39,    -1,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   111,    -1,    -1,    -1,   115,    -1,    -1,    -1,
      -1,    -1,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   144,    -1,    -1,   147,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   162,   163,   164,   165,   166,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   178,
     179,    -1,    -1,   182,   183,   184,   185,   186,   187,   188,
     189,   190,   191,   192,   193,   194,   195,   196,   197,   198,
     199,   200,   201,   202,    16,    -1,    -1,    -1,    20,    21,
      22,    -1,    24,    -1,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    -1,    -1,    -1,    -1,    39,    -1,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      -1,    -1,    16,    -1,    -1,    -1,    -1,    21,    22,    -1,
      -1,    -1,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    -1,    -1,    -1,    -1,    39,    -1,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    16,    -1,
      -1,    -1,    -1,    21,    22,    -1,    -1,    -1,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    -1,    -1,    -1,
      -1,    39,    -1,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   166,    -1,    -1,    -1,    -1,    -1,
      -1,   173,   174,   127,    -1,    -1,   178,   179,    -1,    -1,
     182,   183,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   196,   197,   198,   199,   200,   201,
     202,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   178,   179,    -1,    -1,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   166,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     178,   179,    -1,    -1,   182,   183,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,    26,    27,    28,    29,    30,
      31,    32,    33,    -1,    -1,    -1,    -1,    -1,    39,    -1,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    97,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     111,    -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,    -1,
      -1,   122,    -1,   124,    -1,    -1,    -1,   128,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   144,    -1,    -1,   147,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   162,   163,   164,   165,   166,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   178,   179,    -1,
      -1,   182,   183,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,   194,   195,   196,   197,   198,   199,   200,
     201,   202,    26,    27,    28,    29,    30,    31,    32,    33,
      -1,    -1,    -1,    -1,    -1,    39,    -1,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    97,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   111,    -1,    -1,
      -1,   115,    -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,
     124,    -1,    -1,    -1,    -1,    -1,   130,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     144,    -1,    -1,   147,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   162,   163,
     164,   165,   166,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   178,   179,    -1,    -1,   182,   183,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   202,    26,
      27,    28,    29,    30,    31,    32,    33,    -1,    -1,    -1,
      -1,    -1,    39,    -1,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      97,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   111,    -1,    -1,    -1,   115,    -1,
      -1,    -1,    -1,    -1,    -1,   122,    -1,   124,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   144,    -1,    -1,
     147,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   162,   163,   164,   165,   166,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   178,   179,    -1,    -1,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   196,
     197,   198,   199,   200,   201,   202,    26,    27,    28,    29,
      30,    31,    32,    33,    -1,    -1,    -1,    -1,    -1,    39,
      -1,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   111,    -1,    -1,    -1,   115,    -1,    -1,    -1,    -1,
      -1,    -1,   122,    -1,   124,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   144,    -1,    -1,   147,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   162,   163,   164,   165,   166,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   178,   179,
      -1,    -1,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    -1,    -1,    -1,    -1,    39,    -1,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   166,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   178,   179,    -1,    -1,   182,
     183,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   196,   197,   198,   199,   200,   201,   202
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,    16,    20,    21,    22,    24,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    39,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,   166,   173,   174,
     178,   179,   182,   183,   184,   185,   186,   187,   188,   189,
     190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
     200,   201,   202,   235,   236,   237,   238,   239,   244,   245,
     246,   250,   251,   254,   256,   257,   258,   259,   260,   295,
     296,   297,   298,   166,   259,   166,   261,   164,   130,   129,
     113,     8,   240,   241,   113,   130,   118,   247,   166,   256,
      14,   180,   181,   253,   121,   248,   249,   257,   248,     0,
     296,   120,   277,   257,   120,   240,    18,    19,    23,   242,
     243,   166,    97,   111,   115,   122,   124,   144,   147,   162,
     163,   164,   165,   166,   205,   206,   207,   210,   211,   212,
     213,   214,   215,   216,   217,   218,   219,   220,   221,   222,
     223,   224,   225,   226,   227,   228,   229,   231,   258,   266,
     122,   248,     7,   252,   128,   216,   229,   234,     6,     9,
      10,    11,    15,    17,    25,    40,    65,    95,   104,   120,
     127,   130,   231,   233,   235,   236,   258,   267,   268,   269,
     271,   273,   274,   275,   278,   279,   280,   286,   294,   130,
     256,   262,   263,   256,   248,   233,   216,   216,   116,   121,
     144,   147,   129,   113,   129,    39,   231,   248,   118,   141,
     142,   145,   150,   152,   153,   155,   157,   159,   161,   232,
     216,   123,   131,   132,   115,   124,   151,   158,   119,   126,
     148,   149,   146,   154,   110,   112,   133,   143,   160,   125,
     156,   230,     5,     8,    18,    23,    37,    38,   255,   128,
     130,   130,   130,   287,   122,   122,   130,   233,   122,   114,
     122,   270,   233,   127,   113,   130,   278,   127,   268,   166,
     264,   265,   127,   263,   166,   248,   247,   129,   166,   208,
     209,   233,   231,   122,   231,   216,   216,   216,   218,   218,
     219,   219,   220,   220,   220,   220,   221,   221,   222,   223,
     224,   225,   226,   227,   233,   268,   287,   233,   130,   287,
     233,   120,   114,   231,   127,   248,   113,   130,   248,   128,
     114,   288,   267,   279,   289,   129,    20,   233,   250,   284,
     285,   129,   272,   278,   292,   265,   231,    40,   284,   290,
     292,   293,   281,   166,   129,   118,   127,   122,   233,   291,
     292,   130,   268,   282,   283,   269,   276,   277,   266,   233,
     129,    12,   288,   129,   276,   268,   130,   288
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (psParseContext, psGLSLTreeContext, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, psParseContext)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, psParseContext, psGLSLTreeContext); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, psParseContext, psGLSLTreeContext)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    ParseContext *psParseContext;
    GLSLTreeContext *psGLSLTreeContext;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (psParseContext);
  YYUSE (psGLSLTreeContext);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, psParseContext, psGLSLTreeContext)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    ParseContext *psParseContext;
    GLSLTreeContext *psGLSLTreeContext;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, psParseContext, psGLSLTreeContext);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext)
#else
static void
yy_reduce_print (yyvsp, yyrule, psParseContext, psGLSLTreeContext)
    YYSTYPE *yyvsp;
    int yyrule;
    ParseContext *psParseContext;
    GLSLTreeContext *psGLSLTreeContext;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , psParseContext, psGLSLTreeContext);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, psParseContext, psGLSLTreeContext); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, psParseContext, psGLSLTreeContext)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    ParseContext *psParseContext;
    GLSLTreeContext *psGLSLTreeContext;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (psParseContext);
  YYUSE (psGLSLTreeContext);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (ParseContext *psParseContext, GLSLTreeContext *psGLSLTreeContext)
#else
int
yyparse (psParseContext, psGLSLTreeContext)
    ParseContext *psParseContext;
    GLSLTreeContext *psGLSLTreeContext;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

/* User initialization code.  */

/* Line 1242 of yacc.c  */
#line 699 "tools/intern/oglcompiler/parser/glsl_parser.y"
{
#if YYDEBUG
	glsl_debug = 1;
#endif
}

/* Line 1242 of yacc.c  */
#line 3170 "eurasiacon/binary2_540_omap4430_android_release/target/intermediates/glslparser/glsl_parser.tab.c"

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:

/* Line 1455 of yacc.c  */
#line 798 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateIDENTIFIERUseNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken); ;}
    break;

  case 4:

/* Line 1455 of yacc.c  */
#line 803 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateINTCONSTANTNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken); ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 805 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
#ifdef GLSL_130
																	  (yyval).u.psNode = ASTCreateINTCONSTANTNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken);
#endif
	;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 810 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateFLOATCONSTANTNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken); ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 811 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateBOOLCONSTANTNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken); ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 813 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode;
		/*
		   The 'expression' node type created is changed to be a 'sub-expression' type,
		   this helps at the intermediate code generation level to know when to insert
		   the instructions to perform postfix increment/decrement operations
		*/
		(yyval).u.psNode->eNodeType = GLSLNT_SUBEXPRESSION;
	;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 828 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode *psIntegerExpressionNode;
	
		GLSLNode *psLeftNode = (yyvsp[(1) - (4)]).u.psNode;
		
		/* Create array specifier (PARENT)  node */
		GLSLNode *psArraySpecifierNode = ASTCreateNewNode(GLSLNT_ARRAY_SPECIFIER, (yyvsp[(2) - (4)]).psToken);		if(!psArraySpecifierNode) YYABORT;
		
		/* Get the integer expression node (RIGHT) */
		psIntegerExpressionNode = (yyvsp[(3) - (4)]).u.psNode;
	
		ASTAddNodeChild(psArraySpecifierNode, psLeftNode);
		ASTAddNodeChild(psArraySpecifierNode, psIntegerExpressionNode);
						
		/* Calculate what the result would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psArraySpecifierNode, IMG_FALSE);
		
		(yyval).u.psNode = psArraySpecifierNode;
	;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 848 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode *psFieldNode;
		
		GLSLNode *psLeftNode = (yyvsp[(1) - (3)]).u.psNode;
		
		/* Create field selection node */
		GLSLNode *psFieldSelectionNode = ASTCreateNewNode(GLSLNT_FIELD_SELECTION, (yyvsp[(2) - (3)]).psToken);		if(!psFieldSelectionNode) YYABORT;
		
		/* process and return field selection entry */
		psFieldNode = ASTProcessFieldSelection(psGLSLTreeContext, (yyvsp[(3) - (3)]).psToken, psLeftNode);

		ASTAddNodeChild(psFieldSelectionNode, psLeftNode);
		ASTAddNodeChild(psFieldSelectionNode, psFieldNode);

		/* Calculate what the result would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psFieldSelectionNode, IMG_FALSE);
		
		(yyval).u.psNode = psFieldSelectionNode;
	;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 868 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_POST_INC, (yyvsp[(2) - (2)]).psToken, (yyvsp[(1) - (2)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 872 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_POST_DEC, (yyvsp[(2) - (2)]).psToken, (yyvsp[(1) - (2)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 883 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* 
		   The 'expression' node type created is changed to be a 'sub-expression' type,
		   this helps at the intermediate code generation level to know when to insert
		   the instructions to perform postfix increment/decrement operations
		*/
		(yyval).u.psNode->eNodeType = GLSLNT_SUBEXPRESSION;
	;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 895 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTFunctionCallState *psFunctionCallState = (yyvsp[(1) - (1)]).u.psFunctionCallState;
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
				(yyval).u.psNode = IMG_NULL;
				YYABORT;
			}

			pszFunctionName = (yyvsp[(1) - (1)]).psToken->pvData;

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
										 
						(yyval).u.psNode = IMG_NULL; 
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
			__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, psFunctionDefinitionData->uFeatureVersion, 
				psFunctionDefinitionData->pszOriginalFunctionName, "function or matching overload");
		#endif
			
		#ifdef DEBUG
			if (!psFunctionDefinitionData)
			{
				LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Failed to retreive function data\n"));
				(yyval).u.psNode = IMG_NULL;
				break;
			}

			if (psFunctionDefinitionData->eSymbolTableDataType != GLSLSTDT_FUNCTION_DEFINITION)
			{
				LOG_INTERNAL_ERROR(("ASTProcessFunctionCall: Retrieved data was not function definition\n"));
				(yyval).u.psNode = IMG_NULL;
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

				(yyval).u.psNode = IMG_NULL;
				break;
			}

			/* Now need to check that the supplied parameters match up to the parameter type qualifiers used in the function definition */
			ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psFunctionCallNode, IMG_FALSE);
		}
		else
		{
			psFunctionCallNode->uSymbolTableID = 0;
		}
		
		(yyval).u.psNode = psFunctionCallNode;
	;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 1130 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Add it as a child to the function call node, with an expression node to hang it off. */
		GLSLNode* psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, (yyvsp[(2) - (2)]).psToken, (yyvsp[(2) - (2)]).u.psNode, IMG_NULL);		if(!psNode) YYABORT;
		ASTAddNodeChild((yyvsp[(1) - (2)]).u.psFunctionCallState->psFunctionCallNode, psNode);
	;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 1136 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode* psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, (yyvsp[(3) - (3)]).psToken, (yyvsp[(3) - (3)]).u.psNode, IMG_NULL);		if(!psNode) YYABORT;
		ASTAddNodeChild((yyvsp[(1) - (3)]).u.psFunctionCallState->psFunctionCallNode, psNode);
	;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 1144 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		if((yyvsp[(2) - (3)]).psToken)
		{
			/* Check for correct version of language */
			__CheckFeatureVersion((yyvsp[(2) - (3)]).psToken, 120, IMG_NULL, "array constructors");

			/* Get the array size */
			(yyval).u.psFunctionCallState->iConstructorArraySize = 
				__ProcessArraySpecifier(psGLSLTreeContext, IMG_NULL/*fixme*/, &(yyvsp[(2) - (3)]), IMG_FALSE);
		}
	;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 1159 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psFunctionCallState);
		
		if ((yyvsp[(1) - (1)]).psToken->eTokenName == TOK_TYPE_NAME)
		{
			(yyval).u.psFunctionCallState->bConstructor = IMG_FALSE;
		}
		else
		{
			(yyval).u.psFunctionCallState->bConstructor = IMG_TRUE;
			(yyval).u.psFunctionCallState->eConstructorTypeSpecifier = (yyvsp[(1) - (1)]).u.eTypeSpecifier;
			(yyval).u.psFunctionCallState->iConstructorArraySize = 0;
		}
		
		(yyval).u.psFunctionCallState->psFunctionCallNode = ASTCreateNewNode(GLSLNT_FUNCTION_CALL, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psFunctionCallState->psFunctionCallNode) YYABORT;
	;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 1176 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psFunctionCallState);
		
		(yyval).u.psFunctionCallState->bConstructor = IMG_FALSE;
		
		(yyval).u.psFunctionCallState->psFunctionCallNode = ASTCreateNewNode(GLSLNT_FUNCTION_CALL, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psFunctionCallState->psFunctionCallNode) YYABORT;
	;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 1190 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_PRE_INC, (yyvsp[(1) - (2)]).psToken, (yyvsp[(2) - (2)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 1194 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { 
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_PRE_DEC, (yyvsp[(1) - (2)]).psToken, (yyvsp[(2) - (2)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 1198 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { 
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(2) - (2)]).u.psNode);
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 1207 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateNewNode(GLSLNT_POSITIVE, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT; ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 1208 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateNewNode(GLSLNT_NEGATE, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT; ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 1209 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = ASTCreateNewNode(GLSLNT_NOT, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT; ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 1211 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_BITWISE_NOT, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 1225 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_MULTIPLY, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 1229 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_DIVIDE, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 1233 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "modulus operator", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_MODULUS, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 1245 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_ADD, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 1249 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SUBTRACT, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 1257 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SHIFT_LEFT, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 1265 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_SHIFT_RIGHT, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 1277 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LESS_THAN, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 1281 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_GREATER_THAN, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 1285 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LESS_THAN_EQUAL, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 1289 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_GREATER_THAN_EQUAL, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 1297 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EQUAL_TO, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 1301 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_NOTEQUAL_TO, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 54:

/* Line 1455 of yacc.c  */
#line 1309 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "bitwise operators", IMG_NULL);
	
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_AND, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 56:

/* Line 1455 of yacc.c  */
#line 1321 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXCLUSIVE_OR, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 58:

/* Line 1455 of yacc.c  */
#line 1333 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (3)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_INCLUSIVE_OR, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 60:

/* Line 1455 of yacc.c  */
#line 1345 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_AND, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 62:

/* Line 1455 of yacc.c  */
#line 1353 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_XOR, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 64:

/* Line 1455 of yacc.c  */
#line 1361 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_LOGICAL_OR, (yyvsp[(2) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psNode, (yyvsp[(3) - (3)]).u.psNode);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 66:

/* Line 1455 of yacc.c  */
#line 1369 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {			
		/* Create the question node */
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_QUESTION, (yyvsp[(3) - (5)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		
		/* Add the logical or expression as the first child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(1) - (5)]).u.psNode);

		/* Add the expression node as the second child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(3) - (5)]).u.psNode);

		/* Add the conditional expression as the third child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(5) - (5)]).u.psNode);

		/* Calculate what the result of this expression would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);

		psGLSLTreeContext->uConditionLevel--;
	;}
    break;

  case 67:

/* Line 1455 of yacc.c  */
#line 1389 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { psGLSLTreeContext->uConditionLevel++; ;}
    break;

  case 69:

/* Line 1455 of yacc.c  */
#line 1395 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode	   *psAssignmentOperatorNode = (yyvsp[(2) - (3)]).u.psNode;	/* Parent Node */
		GLSLNode	   *psUnaryExpressionNode = (yyvsp[(1) - (3)]).u.psNode;	   /* Child node 1 */
		GLSLNode	   *psAssignmentExpressionNode = (yyvsp[(3) - (3)]).u.psNode;  /* Child node 2 */
		
		/* Add the left and right nodes to the parent */

		/* Left */
		ASTAddNodeChild(psAssignmentOperatorNode, psUnaryExpressionNode);

		/* Right */
		ASTAddNodeChild(psAssignmentOperatorNode, psAssignmentExpressionNode);

		/* Calculate what the result of this expression would be */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psAssignmentOperatorNode, IMG_FALSE);
		
		(yyval).u.psNode = psAssignmentOperatorNode;
	;}
    break;

  case 70:

/* Line 1455 of yacc.c  */
#line 1417 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		#define __assignment_operator(NodeType) (yyval).u.psNode = ASTCreateNewNode(GLSLNT_##NodeType, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		__assignment_operator(EQUAL);
	;}
    break;

  case 71:

/* Line 1455 of yacc.c  */
#line 1421 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __assignment_operator(MUL_ASSIGN); ;}
    break;

  case 72:

/* Line 1455 of yacc.c  */
#line 1422 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __assignment_operator(DIV_ASSIGN); ;}
    break;

  case 73:

/* Line 1455 of yacc.c  */
#line 1424 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "modulus operator", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(MOD_ASSIGN);
	#endif
	;}
    break;

  case 74:

/* Line 1455 of yacc.c  */
#line 1431 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __assignment_operator(ADD_ASSIGN); ;}
    break;

  case 75:

/* Line 1455 of yacc.c  */
#line 1432 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __assignment_operator(SUB_ASSIGN); ;}
    break;

  case 76:

/* Line 1455 of yacc.c  */
#line 1434 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(LEFT_ASSIGN);
	#endif
	;}
    break;

  case 77:

/* Line 1455 of yacc.c  */
#line 1442 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(RIGHT_ASSIGN);
	#endif
	;}
    break;

  case 78:

/* Line 1455 of yacc.c  */
#line 1450 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(AND_ASSIGN);
	#endif
	;}
    break;

  case 79:

/* Line 1455 of yacc.c  */
#line 1458 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(XOR_ASSIGN);
	#endif
	;}
    break;

  case 80:

/* Line 1455 of yacc.c  */
#line 1466 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "bitwise operators", IMG_NULL);
		
	#ifdef GLSL_130
		__assignment_operator(OR_ASSIGN);
	#endif
	;}
    break;

  case 81:

/* Line 1455 of yacc.c  */
#line 1477 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_EXPRESSION, (yyvsp[(1) - (1)]).psToken, (yyvsp[(1) - (1)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 82:

/* Line 1455 of yacc.c  */
#line 1481 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(3) - (3)]).u.psNode);
		
		/* We need to recalculate the type of the node, because another child expression was added which may be of a different type. */
		(yyval).u.psNode->uSymbolTableID = 0;
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	;}
    break;

  case 84:

/* Line 1455 of yacc.c  */
#line 1496 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTCreateFunctionNode(psGLSLTreeContext, (yyvsp[(1) - (2)]).u.psFunctionState, IMG_TRUE);
		(yyval).u.psNode = IMG_NULL;
	;}
    break;

  case 85:

/* Line 1455 of yacc.c  */
#line 1501 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = (yyvsp[(1) - (2)]).u.psInitDeclaratorList->psNode;
	;}
    break;

  case 86:

/* Line 1455 of yacc.c  */
#line 1505 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Check for valid types */
		if ((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_FLOAT ||
			(yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_INT ||
				GLSL_IS_SAMPLER((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier))
		{
			GLSLPrecisionQualifier ePrecisionQualifier = (yyvsp[(2) - (4)]).u.ePrecisionQualifier;
			
			GLSLPrecisionQualifier eForcedPrecision = GLSLPRECQ_UNKNOWN; 

			/* Adjust precision statement if force has been set */
			if ((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_FLOAT)
				eForcedPrecision                          = psGLSLTreeContext->eForceUserFloatPrecision;
			else if ((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier == GLSLTS_INT)
				eForcedPrecision                          = psGLSLTreeContext->eForceUserIntPrecision;
			else if (GLSL_IS_SAMPLER((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier))
				eForcedPrecision                          = psGLSLTreeContext->eForceUserSamplerPrecision;

			if(eForcedPrecision != GLSLPRECQ_UNKNOWN)
			{
				ePrecisionQualifier = eForcedPrecision;
			}

			/* Add this to the symbol table */
			ModifyDefaultPrecision(psGLSLTreeContext,
								   ePrecisionQualifier,
								   (yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier);
		}
		else
		{
			LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(3) - (4)]).psToken,
									 "'%s' : default precision modifier type can only be float, int or sampler\n",
									 GLSLTypeSpecifierDescTable((yyvsp[(3) - (4)]).u.psFullySpecifiedType->eTypeSpecifier));
		}
		
		(yyval).u.psNode = IMG_NULL;
	;}
    break;

  case 90:

/* Line 1455 of yacc.c  */
#line 1554 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __AddFunctionState(psParseContext, psGLSLTreeContext, (yyvsp[(1) - (2)]).u.psFunctionState, (yyvsp[(2) - (2)]).psToken, (yyvsp[(2) - (2)]).u.psParameterDeclaration); ;}
    break;

  case 91:

/* Line 1455 of yacc.c  */
#line 1556 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __AddFunctionState(psParseContext, psGLSLTreeContext, (yyvsp[(1) - (3)]).u.psFunctionState, (yyvsp[(3) - (3)]).psToken, (yyvsp[(3) - (3)]).u.psParameterDeclaration); ;}
    break;

  case 92:

/* Line 1455 of yacc.c  */
#line 1561 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psFunctionState);
		
	#if defined(GLSL_ES)
		IsSamplerTypeSupported(psGLSLTreeContext, (yyvsp[(1) - (3)]).psToken, (yyvsp[(1) - (3)]).u.psFullySpecifiedType);
	#endif

		(yyval).u.psFunctionState->psIDENTIFIEREntry										= (yyvsp[(2) - (3)]).psToken;
		(yyval).u.psFunctionState->sReturnData.eSymbolTableDataType						= GLSLSTDT_IDENTIFIER;
		(yyval).u.psFunctionState->sReturnData.sFullySpecifiedType						= *(yyvsp[(1) - (3)]).u.psFullySpecifiedType;
		/* Wierd, grammar says fully specified type, yet type qualifier has no meaning for return values according to spec */
		(yyval).u.psFunctionState->sReturnData.sFullySpecifiedType.eTypeQualifier		= GLSLTQ_TEMP;
		(yyval).u.psFunctionState->sReturnData.eLValueStatus								= GLSLLV_L_VALUE;
		(yyval).u.psFunctionState->sReturnData.eBuiltInVariableID						= GLSLBV_NOT_BTIN;
		(yyval).u.psFunctionState->sReturnData.eIdentifierUsage							= (GLSLIdentifierUsage)(GLSLIU_INTERNALRESULT | GLSLIU_WRITTEN);
		(yyval).u.psFunctionState->sReturnData.uConstantDataSize							= 0;
		(yyval).u.psFunctionState->sReturnData.pvConstantData							= IMG_NULL;
		(yyval).u.psFunctionState->sReturnData.uConstantAssociationSymbolID				= 0;

		/* Was the return type an array? */
		if ((yyvsp[(1) - (3)]).u.psFullySpecifiedType->iArraySize)
		{
			(yyval).u.psFunctionState->sReturnData.eArrayStatus							= GLSLAS_ARRAY_SIZE_FIXED;
			(yyval).u.psFunctionState->sReturnData.iActiveArraySize						= (yyvsp[(1) - (3)]).u.psFullySpecifiedType->iArraySize;
		}
		else
		{
			(yyval).u.psFunctionState->sReturnData.eArrayStatus							= GLSLAS_NOT_ARRAY;
			(yyval).u.psFunctionState->sReturnData.iActiveArraySize						= -1;
		}

		(yyval).u.psFunctionState->uNumParameters = 0;
		(yyval).u.psFunctionState->psParameters = IMG_NULL;
	;}
    break;

  case 93:

/* Line 1455 of yacc.c  */
#line 1599 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__ProcessParameterDeclaration(psGLSLTreeContext, psParseContext, &(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]));
	;}
    break;

  case 94:

/* Line 1455 of yacc.c  */
#line 1605 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eTypeQualifier = GLSLTQ_TEMP; ;}
    break;

  case 95:

/* Line 1455 of yacc.c  */
#line 1606 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eTypeQualifier = GLSLTQ_CONST; ;}
    break;

  case 96:

/* Line 1455 of yacc.c  */
#line 1610 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eParameterQualifier = GLSLPQ_IN; ;}
    break;

  case 97:

/* Line 1455 of yacc.c  */
#line 1611 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eParameterQualifier = GLSLPQ_IN; ;}
    break;

  case 98:

/* Line 1455 of yacc.c  */
#line 1612 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eParameterQualifier = GLSLPQ_OUT; ;}
    break;

  case 99:

/* Line 1455 of yacc.c  */
#line 1613 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.eParameterQualifier = GLSLPQ_INOUT; ;}
    break;

  case 100:

/* Line 1455 of yacc.c  */
#line 1618 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Need to set the parameter qualifier before we check the type, because 
		   __CheckTypeSpecifier checks that uniforms are either uniform or parameters.*/
		(yyvsp[(2) - (4)]).u.psFullySpecifiedType->eParameterQualifier = (yyvsp[(1) - (4)]).u.eParameterQualifier;
		__CheckTypeSpecifier(psGLSLTreeContext, &(yyvsp[(2) - (4)]));
		
		UNION_ALLOC((yyval), psParameterDeclarator);
		(yyval).u.psParameterDeclarator->psFullySpecifiedType = (yyvsp[(2) - (4)]).u.psFullySpecifiedType;
		(yyval).u.psParameterDeclarator->psIdentifierToken = (yyvsp[(3) - (4)]).psToken;
		(yyval).u.psParameterDeclarator->sArraySpecifier = (yyvsp[(4) - (4)]);
	;}
    break;

  case 101:

/* Line 1455 of yacc.c  */
#line 1630 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Need to set the parameter qualifier before we check the type, because 
		   __CheckTypeSpecifier checks that uniforms are either uniform or parameters.*/
		(yyvsp[(2) - (3)]).u.psFullySpecifiedType->eParameterQualifier = (yyvsp[(1) - (3)]).u.eParameterQualifier;
		__CheckTypeSpecifier(psGLSLTreeContext, &(yyvsp[(2) - (3)]));
		
		UNION_ALLOC((yyval), psParameterDeclarator);
		(yyval).u.psParameterDeclarator->psFullySpecifiedType = (yyvsp[(2) - (3)]).u.psFullySpecifiedType;
		(yyval).u.psParameterDeclarator->psIdentifierToken = IMG_NULL;
		(yyval).u.psParameterDeclarator->sArraySpecifier = (yyvsp[(3) - (3)]);
	;}
    break;

  case 102:

/* Line 1455 of yacc.c  */
#line 1645 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psInitDeclaratorList);
		(yyval).u.psInitDeclaratorList->bInvarianceModifier = (yyvsp[(1) - (1)]).u.psSingleDeclaration->bInvarianceModifier;
		
		if(!(yyvsp[(1) - (1)]).u.psInitDeclaratorList->bInvarianceModifier)
		{
			(yyval).u.psInitDeclaratorList->psFullySpecifiedType = (yyvsp[(1) - (1)]).u.psSingleDeclaration->psFullySpecifiedType;

			/* Check if the declaration was just for a type. */
			if((yyvsp[(1) - (1)]).u.psSingleDeclaration->psNode)
			{
				/* Create top level declaration list node */
				(yyval).u.psInitDeclaratorList->psNode = ASTCreateNewNode(GLSLNT_DECLARATION, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psInitDeclaratorList->psNode) YYABORT;
				
				/* Add the single declaration to this node */
				ASTAddNodeChild((yyval).u.psInitDeclaratorList->psNode, (yyvsp[(1) - (1)]).u.psSingleDeclaration->psNode);
				
				/* Calulate what the type the result of this declaration would be */
				ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psInitDeclaratorList->psNode, IMG_FALSE);
			}
			else
			{
				(yyval).u.psInitDeclaratorList->psNode = IMG_NULL;
			}
		}
		else
		{
			(yyval).u.psInitDeclaratorList->psNode = IMG_NULL;
		}
	;}
    break;

  case 103:

/* Line 1455 of yacc.c  */
#line 1676 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		if((yyval).u.psInitDeclaratorList->bInvarianceModifier)
		{
			ASTUpdateInvariantStatus(psGLSLTreeContext, (yyvsp[(3) - (5)]).psToken);
		}
		else
		{
			/* __CreateDeclarationIdentifierNode() and __ProcessDeclarationInitializer() are called together here
			   because the initializer already has the fallback precision from the first declaration. */
			GLSLNode* psNextDeclaration = __CreateDeclarationIdentifierNode(psGLSLTreeContext,
				(yyval).u.psInitDeclaratorList->psFullySpecifiedType, (yyvsp[(3) - (5)]).psToken, &(yyvsp[(4) - (5)]));			if(!psNextDeclaration) YYABORT;
			
			psNextDeclaration = __ProcessDeclarationInitializer(psGLSLTreeContext, psNextDeclaration, &(yyvsp[(5) - (5)]));			if(!psNextDeclaration) YYABORT;
				
			/* Create top level node if it doesn't exist yet */
			if (!(yyval).u.psInitDeclaratorList->psNode)
				(yyval).u.psInitDeclaratorList->psNode = ASTCreateNewNode(GLSLNT_DECLARATION, (yyval).psToken);		if(!(yyval).u.psInitDeclaratorList->psNode) YYABORT;

			/* Add the next declaration to the list. */
			ASTAddNodeChild((yyval).u.psInitDeclaratorList->psNode, psNextDeclaration);
		}
	;}
    break;

  case 104:

/* Line 1455 of yacc.c  */
#line 1702 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psSingleDeclaration);
		(yyval).u.psSingleDeclaration->psNode = IMG_NULL;
		(yyval).u.psSingleDeclaration->psFullySpecifiedType = (yyvsp[(1) - (1)]).u.psFullySpecifiedType;	
		(yyval).u.psSingleDeclaration->bInvarianceModifier = IMG_FALSE;
	;}
    break;

  case 105:

/* Line 1455 of yacc.c  */
#line 1709 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {		
		(yyval).u.psSingleDeclaration->psNode = __ProcessDeclarationInitializer(psGLSLTreeContext,
			(yyval).u.psSingleDeclaration->psNode,
			&(yyvsp[(2) - (2)]));
		if(!(yyval).u.psSingleDeclaration) YYABORT;
	;}
    break;

  case 106:

/* Line 1455 of yacc.c  */
#line 1716 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTUpdateInvariantStatus(psGLSLTreeContext, (yyvsp[(2) - (2)]).psToken);
		
		UNION_ALLOC((yyval), psSingleDeclaration);
		(yyval).u.psSingleDeclaration->bInvarianceModifier = IMG_TRUE;
	;}
    break;

  case 107:

/* Line 1455 of yacc.c  */
#line 1726 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psSingleDeclaration);
		(yyval).u.psSingleDeclaration->psNode = __CreateDeclarationIdentifierNode(psGLSLTreeContext, 
			(yyvsp[(1) - (3)]).u.psFullySpecifiedType, (yyvsp[(2) - (3)]).psToken, &(yyvsp[(3) - (3)]));		if(!(yyval).u.psSingleDeclaration->psNode) YYABORT;
		(yyval).u.psSingleDeclaration->psFullySpecifiedType = (yyvsp[(1) - (3)]).u.psFullySpecifiedType;
		(yyval).u.psSingleDeclaration->bInvarianceModifier = IMG_FALSE;
	;}
    break;

  case 108:

/* Line 1455 of yacc.c  */
#line 1735 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).psToken = IMG_NULL; ;}
    break;

  case 109:

/* Line 1455 of yacc.c  */
#line 1736 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = (yyvsp[(2) - (2)]).u.psNode; ;}
    break;

  case 110:

/* Line 1455 of yacc.c  */
#line 1740 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).psToken = IMG_NULL; ;}
    break;

  case 112:

/* Line 1455 of yacc.c  */
#line 1744 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode; ;}
    break;

  case 113:

/* Line 1455 of yacc.c  */
#line 1745 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = IMG_NULL; ;}
    break;

  case 114:

/* Line 1455 of yacc.c  */
#line 1752 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* If no type qualifer specified default is temp */
		(yyval).u.psFullySpecifiedType->eTypeQualifier = GLSLTQ_TEMP;
		
		__CheckTypeSpecifier(psGLSLTreeContext, &(yyval));
	;}
    break;

  case 115:

/* Line 1455 of yacc.c  */
#line 1759 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { 
		(yyval).u.psFullySpecifiedType = (yyvsp[(2) - (2)]).u.psFullySpecifiedType;
		(yyval).u.psFullySpecifiedType->eTypeQualifier = (yyvsp[(1) - (2)]).u.psTypeQualifier->eTypeQualifier;
		(yyval).u.psFullySpecifiedType->eVaryingModifierFlags = (yyvsp[(1) - (2)]).u.psTypeQualifier->eVaryingModifierFlags;
		
		(yyval).psToken = (yyvsp[(2) - (2)]).psToken;

		__CheckTypeSpecifier(psGLSLTreeContext, &(yyval));
	;}
    break;

  case 116:

/* Line 1455 of yacc.c  */
#line 1772 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval) = (yyvsp[(4) - (4)]);
		(yyval).u.psTypeQualifier->eVaryingModifierFlags |= (yyvsp[(1) - (4)]).u.eVaryingModifierFlags | (yyvsp[(2) - (4)]).u.eVaryingModifierFlags | (yyvsp[(3) - (4)]).u.eVaryingModifierFlags;
		
		if (((yyval).u.psTypeQualifier->eTypeQualifier != GLSLTQ_VERTEX_OUT) &&
			((yyval).u.psTypeQualifier->eTypeQualifier != GLSLTQ_FRAGMENT_IN) &&
			(yyval).u.psTypeQualifier->eVaryingModifierFlags)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, (yyval).psToken, 
									 "'%s' : non varyings cannot have varying modifier qualifiers\n", 
									 (yyval).psToken->pvData);
		}			
	;}
    break;

  case 117:

/* Line 1455 of yacc.c  */
#line 1789 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_NONE;
	;}
    break;

  case 118:

/* Line 1455 of yacc.c  */
#line 1793 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 120, "centroid", IMG_NULL);
	
	#ifndef GLSL_ES
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_CENTROID;
	#endif
	;}
    break;

  case 119:

/* Line 1455 of yacc.c  */
#line 1804 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_NONE;
	;}
    break;

  case 120:

/* Line 1455 of yacc.c  */
#line 1808 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_NONE;
	#endif
	;}
    break;

  case 121:

/* Line 1455 of yacc.c  */
#line 1816 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_FLAT;
	#endif
	;}
    break;

  case 122:

/* Line 1455 of yacc.c  */
#line 1824 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "interpolation qualifiers", IMG_NULL);
		
	#ifdef GLSL_130
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_NOPERSPECTIVE;
	#endif
	;}
    break;

  case 123:

/* Line 1455 of yacc.c  */
#line 1835 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_NONE;
	;}
    break;

  case 124:

/* Line 1455 of yacc.c  */
#line 1839 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.eVaryingModifierFlags = GLSLVMOD_INVARIANT;
	;}
    break;

  case 125:

/* Line 1455 of yacc.c  */
#line 1848 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		#define __ProcessTypeQualifier(ss, _eTypeQualifier, _eVaryingModifierFlags) {\
			UNION_ALLOC(ss, psTypeQualifier); \
			ss.u.psTypeQualifier->eTypeQualifier = (_eTypeQualifier); \
			ss.u.psTypeQualifier->eVaryingModifierFlags = (_eVaryingModifierFlags);}
			
		__ProcessTypeQualifier((yyval), GLSLTQ_CONST, GLSLVMOD_NONE);
	;}
    break;

  case 126:

/* Line 1455 of yacc.c  */
#line 1857 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#ifdef GLSL_130
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, FEATURE_VERSION(0, 130), "attribute", IMG_NULL);
	#endif
		__ProcessTypeQualifier((yyval), GLSLTQ_VERTEX_IN, GLSLVMOD_NONE);
	;}
    break;

  case 127:

/* Line 1455 of yacc.c  */
#line 1864 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "shader in & out declarations", IMG_NULL);
		
		/* The new shader input and output declarations via in & out are mapped to the existing type qualifiers,
		   except fragment shader custom output variables which are mapped to GLSLTQ_FRAGMENT_OUT. */
		   
	#define eForcedVaryingModifierFlags (\
			(psGLSLTreeContext->eEnabledExtensions & GLSLEXT_OES_INVARIANTALL) ? \
			GLSLVMOD_INVARIANT : GLSLVMOD_NONE)

	#ifdef GLSL_130
		if(psGLSLTreeContext->eProgramType == GLSLPT_VERTEX)
		{
			/* Attributes */
			__ProcessTypeQualifier((yyval), GLSLTQ_VERTEX_IN, GLSLVMOD_NONE);
		}
		else
		{
			__ProcessTypeQualifier((yyval), GLSLTQ_FRAGMENT_IN, eForcedVaryingModifierFlags);
		}
	#endif
	;}
    break;

  case 128:

/* Line 1455 of yacc.c  */
#line 1887 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, "shader in & out declarations", IMG_NULL);
		
	#ifdef GLSL_130
		if(psGLSLTreeContext->eProgramType == GLSLPT_VERTEX)
		{
			/* Varyings */
			__ProcessTypeQualifier((yyval), GLSLTQ_VERTEX_OUT, eForcedVaryingModifierFlags);
		}
		else
		{
			__ProcessTypeQualifier((yyval), GLSLTQ_FRAGMENT_OUT, GLSLVMOD_NONE);
		}
	#endif
	;}
    break;

  case 129:

/* Line 1455 of yacc.c  */
#line 1903 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#ifdef GLSL_130
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, FEATURE_VERSION(0, 130), "varying", IMG_NULL);
	#endif
	
		#define eVaryingTypeQualifier (psGLSLTreeContext->eProgramType == GLSLPT_VERTEX) ? GLSLTQ_VERTEX_OUT : GLSLTQ_FRAGMENT_IN
		
		__ProcessTypeQualifier((yyval), eVaryingTypeQualifier, eForcedVaryingModifierFlags);
	;}
    break;

  case 130:

/* Line 1455 of yacc.c  */
#line 1913 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		__ProcessTypeQualifier((yyval), GLSLTQ_UNIFORM, GLSLVMOD_NONE);
	;}
    break;

  case 132:

/* Line 1455 of yacc.c  */
#line 1921 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psFullySpecifiedType = (yyvsp[(2) - (2)]).u.psFullySpecifiedType;
		(yyval).u.psFullySpecifiedType->ePrecisionQualifier = (yyvsp[(1) - (2)]).u.ePrecisionQualifier;
		(yyval).psToken = (yyvsp[(2) - (2)]).psToken;
	;}
    break;

  case 133:

/* Line 1455 of yacc.c  */
#line 1930 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psFullySpecifiedType);
		INIT_FULLY_SPECIFIED_TYPE((yyval).u.psFullySpecifiedType);
		(yyval).u.psFullySpecifiedType->eTypeSpecifier = (yyvsp[(1) - (2)]).u.eTypeSpecifier;
		
		if((yyvsp[(1) - (2)]).u.eTypeSpecifier == GLSLTS_STRUCT)
		{
			/* The name of a structure was referenced. */
			
			IMG_CHAR *pszStructName = (yyvsp[(1) - (2)]).psToken->pvData;
			IMG_CHAR acName[1024];
			IMG_UINT32 uSymbolID;

			sprintf(acName, "%s@struct_def",  pszStructName);

			/* Find the structure definition */
			if (FindSymbol(psGLSLTreeContext->psSymbolTable,
							acName,
							&uSymbolID,
							IMG_FALSE))
			{
				(yyval).u.psFullySpecifiedType->uStructDescSymbolTableID = uSymbolID;
			}
			else
			{
				LogProgramTokenError(psCPD->psErrorLog, (yyvsp[(1) - (2)]).psToken, "'%s' : undeclared identifier\n", pszStructName);
			}
		}
		
	__type_specifier_no_prec_array_specifier_opt:
		if((yyvsp[(2) - (2)]).psToken)
		{
#ifdef GLSL_ES
			__CheckFeatureVersion((yyvsp[(2) - (2)]).psToken, 0xFFFFFFFF, "array syntax", IMG_NULL);/* This kind of array syntax is never supported for ES. */
#else
			__CheckFeatureVersion((yyvsp[(2) - (2)]).psToken, 120, "array syntax", IMG_NULL);
#endif		
			/* At this moment we dont know if it is permissable to specify an empty '[]' array specifier, so we pass IMG_FALSE 
			   as the bMustHaveSize parameter, and expect the higher up rules to check it with __CheckArraySpecifierMustHaveSize()
			   if they must have the size (for example struct and parameter declarations). */
			(yyval).u.psFullySpecifiedType->iArraySize = __ProcessArraySpecifier(psGLSLTreeContext, IMG_NULL/*fixme*/, &(yyvsp[(2) - (2)]), IMG_FALSE);
		}
	;}
    break;

  case 134:

/* Line 1455 of yacc.c  */
#line 1974 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* A structure was defined here, and we return a psFullySpecifiedType because we may try to declare an instance
		   of the structure in the same statement. */
		   
		UNION_ALLOC((yyval), psFullySpecifiedType);
		INIT_FULLY_SPECIFIED_TYPE((yyval).u.psFullySpecifiedType);
		(yyval).u.psFullySpecifiedType->eTypeSpecifier = GLSLTS_STRUCT;
		(yyval).u.psFullySpecifiedType->uStructDescSymbolTableID = (yyvsp[(1) - (2)]).u.uStructDescSymbolTableID;
		
		goto __type_specifier_no_prec_array_specifier_opt;
	;}
    break;

  case 135:

/* Line 1455 of yacc.c  */
#line 1989 "tools/intern/oglcompiler/parser/glsl_parser.y"
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
	
		__type_specifier_e((yyval), VOID);
	;}
    break;

  case 136:

/* Line 1455 of yacc.c  */
#line 2011 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), FLOAT); ;}
    break;

  case 137:

/* Line 1455 of yacc.c  */
#line 2012 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), INT); ;}
    break;

  case 138:

/* Line 1455 of yacc.c  */
#line 2013 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), BOOL); ;}
    break;

  case 139:

/* Line 1455 of yacc.c  */
#line 2014 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), VEC2); ;}
    break;

  case 140:

/* Line 1455 of yacc.c  */
#line 2015 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), VEC3); ;}
    break;

  case 141:

/* Line 1455 of yacc.c  */
#line 2016 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), VEC4); ;}
    break;

  case 142:

/* Line 1455 of yacc.c  */
#line 2017 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), BVEC2); ;}
    break;

  case 143:

/* Line 1455 of yacc.c  */
#line 2018 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), BVEC3); ;}
    break;

  case 144:

/* Line 1455 of yacc.c  */
#line 2019 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), BVEC4); ;}
    break;

  case 145:

/* Line 1455 of yacc.c  */
#line 2020 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), IVEC2); ;}
    break;

  case 146:

/* Line 1455 of yacc.c  */
#line 2021 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), IVEC3); ;}
    break;

  case 147:

/* Line 1455 of yacc.c  */
#line 2022 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), IVEC4); ;}
    break;

  case 148:

/* Line 1455 of yacc.c  */
#line 2023 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), MAT2X2); ;}
    break;

  case 149:

/* Line 1455 of yacc.c  */
#line 2024 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), MAT3X3); ;}
    break;

  case 150:

/* Line 1455 of yacc.c  */
#line 2025 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), MAT4X4); ;}
    break;

  case 151:

/* Line 1455 of yacc.c  */
#line 2026 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), SAMPLER2D); ;}
    break;

  case 152:

/* Line 1455 of yacc.c  */
#line 2027 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), SAMPLER3D); ;}
    break;

  case 153:

/* Line 1455 of yacc.c  */
#line 2028 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), SAMPLERCUBE); ;}
    break;

  case 154:

/* Line 1455 of yacc.c  */
#line 2031 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		if (CheckTypeName(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken->pvData))
		{
			(yyvsp[(1) - (1)]).psToken->eTokenName = TOK_TYPE_NAME;
			__type_specifier_e((yyval), STRUCT); 
		}
		else
		{
			LogProgramTokenError(psCPD->psErrorLog, (yyvsp[(1) - (1)]).psToken, "'%s' : undeclared identifier\n", (yyvsp[(1) - (1)]).psToken->pvData);
			yyerror(psParseContext, psGLSLTreeContext, "");
		} 
	;}
    break;

  case 155:

/* Line 1455 of yacc.c  */
#line 2045 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), SAMPLERSTREAM); ;}
    break;

  case 156:

/* Line 1455 of yacc.c  */
#line 2046 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e((yyval), SAMPLEREXTERNAL); ;}
    break;

  case 157:

/* Line 1455 of yacc.c  */
#line 2057 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), SAMPLER1D); ;}
    break;

  case 158:

/* Line 1455 of yacc.c  */
#line 2058 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT2X3); ;}
    break;

  case 159:

/* Line 1455 of yacc.c  */
#line 2059 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT2X4); ;}
    break;

  case 160:

/* Line 1455 of yacc.c  */
#line 2060 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT3X2); ;}
    break;

  case 161:

/* Line 1455 of yacc.c  */
#line 2061 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT3X4); ;}
    break;

  case 162:

/* Line 1455 of yacc.c  */
#line 2062 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT4X2); ;}
    break;

  case 163:

/* Line 1455 of yacc.c  */
#line 2063 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), MAT4X3); ;}
    break;

  case 164:

/* Line 1455 of yacc.c  */
#line 2064 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), SAMPLER1DSHADOW); ;}
    break;

  case 165:

/* Line 1455 of yacc.c  */
#line 2065 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_GL((yyval), SAMPLER2DSHADOW); ;}
    break;

  case 166:

/* Line 1455 of yacc.c  */
#line 2067 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_texrect((yyval), SAMPLER2DRECT); ;}
    break;

  case 167:

/* Line 1455 of yacc.c  */
#line 2068 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_texrect((yyval), SAMPLER2DRECTSHADOW); ;}
    break;

  case 168:

/* Line 1455 of yacc.c  */
#line 2071 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), UINT); ;}
    break;

  case 169:

/* Line 1455 of yacc.c  */
#line 2072 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), UVEC2); ;}
    break;

  case 170:

/* Line 1455 of yacc.c  */
#line 2073 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), UVEC3); ;}
    break;

  case 171:

/* Line 1455 of yacc.c  */
#line 2074 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), UVEC4); ;}
    break;

  case 172:

/* Line 1455 of yacc.c  */
#line 2075 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLER1D); ;}
    break;

  case 173:

/* Line 1455 of yacc.c  */
#line 2076 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLER2D); ;}
    break;

  case 174:

/* Line 1455 of yacc.c  */
#line 2077 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLER3D); ;}
    break;

  case 175:

/* Line 1455 of yacc.c  */
#line 2078 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLERCUBE); ;}
    break;

  case 176:

/* Line 1455 of yacc.c  */
#line 2079 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLER1D); ;}
    break;

  case 177:

/* Line 1455 of yacc.c  */
#line 2080 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLER2D); ;}
    break;

  case 178:

/* Line 1455 of yacc.c  */
#line 2081 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLER3D); ;}
    break;

  case 179:

/* Line 1455 of yacc.c  */
#line 2082 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLERCUBE); ;}
    break;

  case 180:

/* Line 1455 of yacc.c  */
#line 2083 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), SAMPLER1DARRAY); ;}
    break;

  case 181:

/* Line 1455 of yacc.c  */
#line 2084 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), SAMPLER2DARRAY); ;}
    break;

  case 182:

/* Line 1455 of yacc.c  */
#line 2085 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLER1DARRAY); ;}
    break;

  case 183:

/* Line 1455 of yacc.c  */
#line 2086 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), ISAMPLER2DARRAY); ;}
    break;

  case 184:

/* Line 1455 of yacc.c  */
#line 2087 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLER1DARRAY); ;}
    break;

  case 185:

/* Line 1455 of yacc.c  */
#line 2088 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), USAMPLER2DARRAY); ;}
    break;

  case 186:

/* Line 1455 of yacc.c  */
#line 2089 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), SAMPLER1DARRAYSHADOW); ;}
    break;

  case 187:

/* Line 1455 of yacc.c  */
#line 2090 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), SAMPLER2DARRAYSHADOW); ;}
    break;

  case 188:

/* Line 1455 of yacc.c  */
#line 2091 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { __type_specifier_e_130((yyval), SAMPLERCUBESHADOW); ;}
    break;

  case 189:

/* Line 1455 of yacc.c  */
#line 2097 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.ePrecisionQualifier = GLSLPRECQ_HIGH;
		
__precision_qualifier:
		
		/* The GLSL ES compiler always supports precision, but the desktop GL requires version 1.30. */
#ifdef GLSL_ES
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, psGLSLTreeContext->eEnabledExtensions & GLSLEXT_IMG_PRECISION ? 0 : 130,
			GLSLPrecisionQualifierFullDescTable[(yyval).u.ePrecisionQualifier], "precision");
#else
		__CheckFeatureVersion((yyvsp[(1) - (1)]).psToken, 130, 
			GLSLPrecisionQualifierFullDescTable[(yyval).u.ePrecisionQualifier], "precision");
#endif
	;}
    break;

  case 190:

/* Line 1455 of yacc.c  */
#line 2111 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.ePrecisionQualifier = GLSLPRECQ_MEDIUM; goto __precision_qualifier; ;}
    break;

  case 191:

/* Line 1455 of yacc.c  */
#line 2112 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.ePrecisionQualifier = GLSLPRECQ_LOW; goto __precision_qualifier; ;}
    break;

  case 192:

/* Line 1455 of yacc.c  */
#line 2117 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLStructureDefinitionData sStructureDefinitionData;
		IMG_UINT32 uSymbolTableID, i;
		
		IMG_CHAR *pszStructureName;
		IMG_CHAR acUnnamedStructureMem[1024];
		
		__StructDeclaration	*psDeclaration;
		__StructDeclarator *psDeclarator;
			
		if ((yyvsp[(2) - (5)]).psToken)
		{
			/* Get structure name */
			pszStructureName = (yyvsp[(2) - (5)]).psToken->pvData;
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
		for(psDeclaration = (yyvsp[(4) - (5)]).u.psStructDeclaration; psDeclaration; psDeclaration = psDeclaration->psNext)
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
							   (yyvsp[(4) - (5)]).psToken,
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
		
		(yyval).u.uStructDescSymbolTableID = uSymbolTableID;
	;}
    break;

  case 193:

/* Line 1455 of yacc.c  */
#line 2238 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).psToken = IMG_NULL; ;}
    break;

  case 196:

/* Line 1455 of yacc.c  */
#line 2245 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#define LINKED_LIST_ADD(member, list, node) {\
		__##member *ps##member = list.u.ps##member;\
		while(ps##member->psNext)\
			ps##member = ps##member->psNext;\
		ps##member->psNext = node.u.ps##member;}
		
		/* Add the next struct_declaration to the end of the list. */
		LINKED_LIST_ADD(StructDeclaration, (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));
	;}
    break;

  case 197:

/* Line 1455 of yacc.c  */
#line 2259 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		if(GLSL_IS_SAMPLER((yyvsp[(1) - (3)]).u.psFullySpecifiedType->eTypeSpecifier))
		{
			/* For structs containing sampler members, we set the member type qualifier to uniform to prevent
			   error messages, and later force any instances of the struct to be uniform. */
			(yyvsp[(1) - (3)]).u.psFullySpecifiedType->eTypeQualifier = GLSLTQ_UNIFORM;
		}
		
		__CheckTypeSpecifier(psGLSLTreeContext, &(yyvsp[(1) - (3)]));
		
		UNION_ALLOC((yyval), psStructDeclaration);
		
		(yyval).u.psStructDeclaration->psFullySpecifiedType = (yyvsp[(1) - (3)]).u.psFullySpecifiedType;
		(yyval).u.psStructDeclaration->psDeclarators = (yyvsp[(2) - (3)]).u.psStructDeclarator;
		(yyval).u.psStructDeclaration->psNext = IMG_NULL;
		
		__CheckArraySpecifierMustHaveSize(psGLSLTreeContext, (yyval).u.psStructDeclaration->psFullySpecifiedType->iArraySize, (yyval).psToken);
	;}
    break;

  case 199:

/* Line 1455 of yacc.c  */
#line 2282 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Add the next struct_declarator to the end of the list. */
		LINKED_LIST_ADD(StructDeclarator, (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
	;}
    break;

  case 200:

/* Line 1455 of yacc.c  */
#line 2290 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psStructDeclarator);
		
		(yyval).u.psStructDeclarator->psIdentifierToken = (yyvsp[(1) - (2)]).psToken;
		(yyval).u.psStructDeclarator->sArraySpecifier = (yyvsp[(2) - (2)]);
		(yyval).u.psStructDeclarator->psNext = IMG_NULL;
	;}
    break;

  case 203:

/* Line 1455 of yacc.c  */
#line 2309 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Whenever a statement is finished, reset the fallback precision to unknown. */
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	;}
    break;

  case 204:

/* Line 1455 of yacc.c  */
#line 2314 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	;}
    break;

  case 208:

/* Line 1455 of yacc.c  */
#line 2324 "tools/intern/oglcompiler/parser/glsl_parser.y"
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
		
		switch ((yyvsp[(1) - (1)]).psToken->eTokenName)
		{
			case TOK_CONTINUE:
			{
				if (!psGLSLTreeContext->uLoopLevel)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(1) - (1)]).psToken,
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
					LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(1) - (1)]).psToken,
											 "'break' : statement only allowed in loops or switch statements\n");
				}
			#else
				if (!psGLSLTreeContext->uLoopLevel)
				{
					LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(1) - (1)]).psToken,
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
				if ((yyvsp[(1) - (1)]).u.psNode)									
					psExpressionNode = (yyvsp[(1) - (1)]).u.psNode;
					
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
		psJumpNode = ASTCreateNewNode(eNodeType, (yyvsp[(1) - (1)]).psToken);		if(!psJumpNode) YYABORT;

		/* Add the expression node as a child if it exists */
		if (psExpressionNode)
			ASTAddNodeChild(psJumpNode, psExpressionNode);
		
		if(eNodeType != GLSLNT_RETURN)/* For GLSLNT_RETURN, semantic checking is done after the function definition. */
		{
			/* Calulatewhat the type the result of this expression would be */
			ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psJumpNode, IMG_FALSE);
		}

		(yyval).u.psNode = psJumpNode;
	;}
    break;

  case 212:

/* Line 1455 of yacc.c  */
#line 2411 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#ifdef GLSL_130
		(yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode;
		psGLSLTreeContext->uSwitchLevel++;
	#endif
	;}
    break;

  case 213:

/* Line 1455 of yacc.c  */
#line 2421 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* For non-130 the lexer will interpret switch & case as reserved keywords, so we never need to 
		   check the version for switch & case. */
	#ifdef GLSL_130
		__CheckFeatureVersion((yyvsp[(1) - (5)]).psToken, 130, "switch", IMG_NULL);
		
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_SWITCH, (yyvsp[(1) - (5)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		
		if(!(yyvsp[(4) - (5)]).u.psNode)
		{
			(yyvsp[(4) - (5)]).u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, (yyvsp[(3) - (5)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		}
		
		ASTUpdateConditionalIdentifierUsage(psGLSLTreeContext, (yyvsp[(2) - (5)]).u.psNode);
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(2) - (5)]).u.psNode);
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(4) - (5)]).u.psNode);
		
		psGLSLTreeContext->uSwitchLevel--;
		
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	#endif
	;}
    break;

  case 216:

/* Line 1455 of yacc.c  */
#line 2452 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#ifdef GLSL_130
		if (!psGLSLTreeContext->uSwitchLevel)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(1) - (3)]).psToken,
									 "'case' : statement only allowed in switch statements\n");
		}
		
		(yyval).u.psNode = __NewOperator(psGLSLTreeContext, GLSLNT_CASE, (yyvsp[(1) - (3)]).psToken, (yyvsp[(2) - (3)]).u.psNode, IMG_NULL);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 217:

/* Line 1455 of yacc.c  */
#line 2464 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
	#ifdef GLSL_130
		if (!psGLSLTreeContext->uSwitchLevel)
		{
			LogProgramParseTreeError(psCPD->psErrorLog, (yyvsp[(1) - (2)]).psToken,
									 "'default' : statement only allowed in switch statements\n");
		}
		
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_DEFAULT, (yyvsp[(1) - (2)]).psToken);		if(!(yyval).u.psNode) YYABORT;
	#endif
	;}
    break;

  case 218:

/* Line 1455 of yacc.c  */
#line 2482 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { 
		/* Increase the scope level */ 
		ASTIncreaseScope(psGLSLTreeContext);
	;}
    break;

  case 219:

/* Line 1455 of yacc.c  */
#line 2489 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = IMG_NULL; ;}
    break;

  case 220:

/* Line 1455 of yacc.c  */
#line 2491 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode;
		
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
	;}
    break;

  case 223:

/* Line 1455 of yacc.c  */
#line 2506 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, (yyvsp[(1) - (2)]).psToken);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 224:

/* Line 1455 of yacc.c  */
#line 2510 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode;
	;}
    break;

  case 225:

/* Line 1455 of yacc.c  */
#line 2517 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_STATEMENT_LIST, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(1) - (1)]).u.psNode);
	;}
    break;

  case 226:

/* Line 1455 of yacc.c  */
#line 2522 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(2) - (2)]).u.psNode);
	;}
    break;

  case 227:

/* Line 1455 of yacc.c  */
#line 2528 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = IMG_NULL; ;}
    break;

  case 229:

/* Line 1455 of yacc.c  */
#line 2534 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode *psExpressionNode;
		
		ASTDecreaseScope(psGLSLTreeContext);
		psGLSLTreeContext->uConditionLevel--;
		
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_IF, (yyvsp[(1) - (6)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		
		psExpressionNode = (yyvsp[(3) - (6)]).u.psNode;
		ASTUpdateConditionalIdentifierUsage(psGLSLTreeContext, psExpressionNode);
		ASTAddNodeChild((yyval).u.psNode, psExpressionNode);
		
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(6) - (6)]).u.psSelectionRestStatement->psIfNode);
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(6) - (6)]).u.psSelectionRestStatement->psElseNode);
		
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	;}
    break;

  case 230:

/* Line 1455 of yacc.c  */
#line 2554 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		psGLSLTreeContext->uConditionLevel++;
		
		/*	Increasing scope around if and else even though not specified by the grammar, as a simple statement
			could contain a declaration. ie.

			int x=0;

			if(x==0)
				int x=1;
		*/
		ASTIncreaseScope(psGLSLTreeContext);
	;}
    break;

  case 231:

/* Line 1455 of yacc.c  */
#line 2571 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTDecreaseScope(psGLSLTreeContext);
		ASTIncreaseScope(psGLSLTreeContext);
	;}
    break;

  case 232:

/* Line 1455 of yacc.c  */
#line 2579 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psSelectionRestStatement);
		(yyval).u.psSelectionRestStatement->psIfNode = (yyvsp[(1) - (3)]).u.psNode;
		(yyval).u.psSelectionRestStatement->psElseNode = (yyvsp[(3) - (3)]).u.psNode;
	;}
    break;

  case 233:

/* Line 1455 of yacc.c  */
#line 2585 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psSelectionRestStatement);
		(yyval).u.psSelectionRestStatement->psIfNode = (yyvsp[(1) - (1)]).u.psNode;
		(yyval).u.psSelectionRestStatement->psElseNode = IMG_NULL;
	;}
    break;

  case 235:

/* Line 1455 of yacc.c  */
#line 2601 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Change $3 to pass the initializer GLSLNode, but with the TOK_EQUAL token - this mirrors what initializer_opt does. */
		(yyvsp[(3) - (3)]).psToken = (yyvsp[(2) - (3)]).psToken;
		
		(yyval).u.psNode = __ProcessDeclarationInitializer(psGLSLTreeContext, (yyval).u.psNode, &(yyvsp[(3) - (3)]));			if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 236:

/* Line 1455 of yacc.c  */
#line 2611 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = __CreateDeclarationIdentifierNode(psGLSLTreeContext, (yyvsp[(1) - (2)]).u.psFullySpecifiedType, (yyvsp[(2) - (2)]).psToken, NULL);		if(!(yyval).u.psNode) YYABORT;
	;}
    break;

  case 237:

/* Line 1455 of yacc.c  */
#line 2618 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Create an iteration  node */
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_WHILE, (yyvsp[(1) - (7)]).psToken);		if(!(yyval).u.psNode) YYABORT;

		/* Add the condition node as the first child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(4) - (7)]).u.psNode);

		/* Add the statement no new scope node as the second child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(6) - (7)]).u.psNode);

		/* Check condition is OK */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	;}
    break;

  case 238:

/* Line 1455 of yacc.c  */
#line 2632 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_DO, (yyvsp[(1) - (9)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		
		/* Add the Expression node as the 1st child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(7) - (9)]).u.psNode);

		/* Add the Statement node as the 2nd child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(3) - (9)]).u.psNode);
		
		/* Check condition is OK */
		ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, (yyval).u.psNode, IMG_FALSE);
	;}
    break;

  case 239:

/* Line 1455 of yacc.c  */
#line 2645 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Create the iteration node */
		(yyval).u.psNode = ASTCreateNewNode(GLSLNT_FOR, (yyvsp[(1) - (9)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		
		/* Add the for init statement node as the first child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(4) - (9)]).u.psNode);

		/* Add the for cond statement node as the second child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(5) - (9)]).u.psNode);

		/* Add the for loop statement node as the third child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(6) - (9)]).u.psNode);

		/* Add the statement no new scope node as the fourth child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(8) - (9)]).u.psNode);
	;}
    break;

  case 240:

/* Line 1455 of yacc.c  */
#line 2664 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Increase the scope level */
		ASTIncreaseScope(psGLSLTreeContext);

		/* Indicate we're inside a loop */
		psGLSLTreeContext->uLoopLevel++;
	;}
    break;

  case 241:

/* Line 1455 of yacc.c  */
#line 2673 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
				
		/* Indicate we've exited the loop */
		psGLSLTreeContext->uLoopLevel--;
	;}
    break;

  case 247:

/* Line 1455 of yacc.c  */
#line 2696 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = IMG_NULL; ;}
    break;

  case 252:

/* Line 1455 of yacc.c  */
#line 2706 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = IMG_NULL; /* Indicate that no expression is returned. */ ;}
    break;

  case 253:

/* Line 1455 of yacc.c  */
#line 2707 "tools/intern/oglcompiler/parser/glsl_parser.y"
    { (yyval).u.psNode = (yyvsp[(2) - (3)]).u.psNode; ;}
    break;

  case 255:

/* Line 1455 of yacc.c  */
#line 2715 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		psGLSLTreeContext->psAbstractSyntaxTree = (yyval).u.psNode = ASTCreateNewNode(GLSLNT_SHADER, (yyvsp[(1) - (1)]).psToken);		if(!(yyval).u.psNode) YYABORT;
		if((yyvsp[(1) - (1)]).u.psNode)/* Node may be NULL, for example TOK_LANGUAGE_VERSION. */
			ASTAddNodeChild((yyval).u.psNode, (yyvsp[(1) - (1)]).u.psNode);
	;}
    break;

  case 256:

/* Line 1455 of yacc.c  */
#line 2721 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		if((yyvsp[(2) - (2)]).u.psNode)/* Node may be NULL. */
			ASTAddNodeChild((yyval).u.psNode, (yyvsp[(2) - (2)]).u.psNode);
	;}
    break;

  case 258:

/* Line 1455 of yacc.c  */
#line 2730 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Whenever a global declaration list is finished, reset the fallback precision to unknown. */
		psGLSLTreeContext->eNextConsumingOperationPrecision = GLSLPRECQ_UNKNOWN;
	;}
    break;

  case 259:

/* Line 1455 of yacc.c  */
#line 2735 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		ASTProcessLanguageVersion(psGLSLTreeContext, (yyvsp[(1) - (1)]).psToken);
		(yyval).u.psNode = IMG_NULL;
	;}
    break;

  case 260:

/* Line 1455 of yacc.c  */
#line 2740 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		/* Update the enabled extensions.

		   Hack: piggyback on pszStartOfLine to store a bitmask.
		*/
		psGLSLTreeContext->eEnabledExtensions = (GLSLExtension)(IMG_UINTPTR_T)(yyvsp[(2) - (2)]).psToken->pszStartOfLine;
		
		(yyval).u.psNode = IMG_NULL;
	;}
    break;

  case 261:

/* Line 1455 of yacc.c  */
#line 2755 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		UNION_ALLOC((yyval), psFunctionDefinitionPrototype);
		
		/* Create the function node, will also increase the scope level */
		(yyval).u.psFunctionDefinitionPrototype->psNode = ASTCreateFunctionNode(psGLSLTreeContext, (yyvsp[(1) - (1)]).u.psFunctionState, IMG_FALSE);
		if(!(yyval).u.psFunctionDefinitionPrototype->psNode)
			YYABORT;
			
		(yyval).u.psFunctionDefinitionPrototype->psFunctionState = (yyvsp[(1) - (1)]).u.psFunctionState;
		
		/* Make this function the current one */
		psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID = (yyval).u.psFunctionDefinitionPrototype->psNode->uSymbolTableID;
	;}
    break;

  case 262:

/* Line 1455 of yacc.c  */
#line 2772 "tools/intern/oglcompiler/parser/glsl_parser.y"
    {
		GLSLNode* psNode;
		
		(yyval).u.psNode = (yyvsp[(1) - (2)]).u.psFunctionDefinitionPrototype->psNode;
	
		/* Add the compound statement as a child */
		ASTAddNodeChild((yyval).u.psNode, (yyvsp[(2) - (2)]).u.psNode);
		
		/* Initialize function return status to false - semantic checking of return nodes will set it to true. */
		psGLSLTreeContext->bFunctionReturnStatus = IMG_FALSE;
		
		/* Go through all the function's nodes looking for return statements. */
		for(psNode = psGLSLTreeContext->psNodeList;
			psNode != (yyval).u.psNode; psNode = psNode->psNext)
		{
			if(psNode->eNodeType == GLSLNT_RETURN)
				ASTSemCheckTypesAndCalculateResult(psGLSLTreeContext, psNode, IMG_FALSE);
		}
		
		/* Check function returned a value */
		if (!psGLSLTreeContext->bFunctionReturnStatus && 
			((yyvsp[(1) - (2)]).u.psFunctionDefinitionPrototype->psFunctionState->sReturnData.sFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID))
		{
			LogProgramParseTreeError(psCPD->psErrorLog,  (yyval).u.psNode->psToken,
										"'%s' : function does not return a value\n",
										(yyval).u.psNode->psToken->pvData);
		}
		
		/* Reset function definition symbol ID */
		psGLSLTreeContext->uCurrentFunctionDefinitionSymbolID = 0;
		
		/* Decrease the scope level */
		ASTDecreaseScope(psGLSLTreeContext);
	;}
    break;



/* Line 1455 of yacc.c  */
#line 6101 "eurasiacon/binary2_540_omap4430_android_release/target/intermediates/glslparser/glsl_parser.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (psParseContext, psGLSLTreeContext, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (psParseContext, psGLSLTreeContext, yymsg);
	  }
	else
	  {
	    yyerror (psParseContext, psGLSLTreeContext, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, psParseContext, psGLSLTreeContext);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, psParseContext, psGLSLTreeContext);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (psParseContext, psGLSLTreeContext, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, psParseContext, psGLSLTreeContext);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, psParseContext, psGLSLTreeContext);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 2809 "tools/intern/oglcompiler/parser/glsl_parser.y"


