/******************************************************************************
 * Name         : icgen.c
 * Created      : 24/08/2004
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
 * $Log: icgen.c $
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include "glsltree.h"
#include "icode.h"

#include "../parser/debug.h"
#include "../parser/symtab.h"

#include "error.h"
#include "icgen.h"
#include "icbuiltin.h"
#include "icunroll.h"
#include "common.h"
#include "icemul.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define IS_PARENT_ASSIGNMENT(node) (node->psParent && node->psParent->eNodeType == GLSLNT_EQUAL)
#define GET_PARENT_ASSIGNMENT_LEFT(node) (node->psParent->ppsChildren[0])

#define USE_IFC 1

/*
** Add intructions for those with no operands
** such as: GLSLIC_OP_ELSE, GLSLIC_OP_ENDIF, GLSLIC_OP_BREAK, GLSLIC_OP_CONSTINUE,
**		    GLSLIC_OP_CONTDEST, GLSLIC_OP_RET, GLSLIC_OP_DISCARD etc.  
*/
#define ADD_ICODE_INSTRUCTION(cpd, program, eICOpCode, pszLine)				\
{																		\
	ICAddICInstruction0(cpd, program, eICOpCode, pszLine);					\
}

/* For instruction with one src only. no dest such as if ifno loop, endloop etc */
#define ADD_ICODE_INSTRUCTION_1(cpd, program, opcode, node, pszLine)		\
{																	\
	GLSLICOperandInfo sChildOperand;								\
																	\
	ICProcessNodeOperand(cpd, program, node, &sChildOperand);			\
																	\
	ICAddICInstruction1(cpd, program,									\
						opcode,										\
						pszLine,									\
						&sChildOperand);							\
																	\
	ICFreeOperandOffsetList(&sChildOperand);						\
}

/* 
** Add instruction if(node)
*/
#define ADD_ICODE_INSTRUCTION_IF(cpd, program, node, pszLine)				\
	ADD_ICODE_INSTRUCTION_1(cpd, program, GLSLIC_OP_IF, node, pszLine)

#define ADD_ICODE_INSTRUCTION_IFNOT(cpd, program, node, pszLine)				\
	ADD_ICODE_INSTRUCTION_1(cpd, program, GLSLIC_OP_IFNOT, node, pszLine)

#define ADD_ICODE_INSTRUCTION_IFP(cpd, program, symid, negate, line)			\
	ICAddICInstructionIFP(cpd, program, GLSLIC_OP_IF, line, symid, negate)

#define ADD_ICODE_INSTRUCTION_ENDIF(cpd, program, line)						\
	ADD_ICODE_INSTRUCTION(cpd, program, GLSLIC_OP_ENDIF, line)

#define ADD_ICODE_INSTRUCTION_ELSE(cpd, program, line)						\
	ADD_ICODE_INSTRUCTION(cpd, program, GLSLIC_OP_ELSE, line)
/* 
** Add instruction: MOV
*/
#define ADD_ICODE_INSTRUCTION_MOV(cpd, program, psDstNode, psSrcNode, pszLine)	\
{																			\
	GLSLICOperandInfo		sSrcOperand;									\
																			\
	ICProcessNodeOperand(psCPD, program, psSrcNode, &sSrcOperand);					\
																			\
	ICAddICInstruction2a(cpd, program,											\
					GLSLIC_OP_MOV,											\
					pszLine,												\
					psDstNode->uSymbolTableID,								\
					&sSrcOperand);											\
	ICFreeOperandOffsetList(&sSrcOperand);									\
}



#define ADD_ICODE_INSTRUCTION_LOOP(cpd, program, symid, line) AddICodeLOOP(cpd, program, symid, IMG_TRUE, line)

#define ADD_ICODE_INSTRUCTION_ENDLOOP(cpd, program, symid, line) AddICodeLOOP(cpd, program, symid, IMG_FALSE, line)


#if 0
#define DUMP_PROCESSING_NODE_INFO(program, node)						\
{																		\
	IMG_UINT32 i;														\
	if(node->uSymbolTableID)											\
	{																	\
		DumpLogMessage(LOGFILE_ICODE, 1, "Process %s(%s): ",			\
			NODETYPE_DESC(node->eNodeType),								\
			GetSymbolName(program->psSymbolTable, node->uSymbolTableID));\
	}																	\
	else																\
	{																	\
		DumpLogMessage(LOGFILE_ICODE, 1, "Process %s: ",				\
			NODETYPE_DESC(node->eNodeType));							\
	}																	\
																		\
	for(i = 0; i < node->uNumChildren; i++)								\
	{																	\
		if(node->ppsChildren[i] == IMG_NULL)							\
		{																\
			DumpLogMessage(LOGFILE_ICODE, 0, "NULLchild ");				\
		}																\
		else															\
		{																\
			if(node->ppsChildren[i]->uSymbolTableID)					\
			{															\
				DumpLogMessage(LOGFILE_ICODE, 0, "%s(%s) ",				\
					NODETYPE_DESC(node->ppsChildren[i]->eNodeType),		\
					GetSymbolName(program->psSymbolTable, node->ppsChildren[i]->uSymbolTableID)); \
			}															\
			else														\
			{															\
				DumpLogMessage(LOGFILE_ICODE, 0, "%s ",					\
					NODETYPE_DESC(node->ppsChildren[i]->eNodeType));	\
			}															\
		}																\
	}																	\
	DumpLogMessage(LOGFILE_ICODE, 0, "\n");								\
}	
#endif

#define DUMP_PROCESSING_NODE_INFO(program, node)


/* Debug strings are typically removed in release mode to reduce the size of the binaries */
#if defined(DEBUG) || defined(DUMP_LOGFILES)
	#define GLSLIC_OP_(op) GLSLIC_OP_##op, #op,
#else
	#define GLSLIC_OP_(op)
#endif

/* 
** A table for intermediate code information, handy to use 
*/
IMG_INTERNAL const ICodeOpTable asICodeOpTable[]=
{
	/* op code						bHasDest	uNumSrcOperands */
	{GLSLIC_OP_(NOP)				IMG_FALSE,	0},
	{GLSLIC_OP_(MOV)				IMG_TRUE,	1},

	{GLSLIC_OP_(ADD)				IMG_TRUE,	2},
	{GLSLIC_OP_(SUB)				IMG_TRUE,	2},
	{GLSLIC_OP_(MUL)				IMG_TRUE,	2},
	{GLSLIC_OP_(DIV)				IMG_TRUE,	2},

	/* test and comparison ops */
	{GLSLIC_OP_(SEQ)				IMG_TRUE,	2},
	{GLSLIC_OP_(SNE)				IMG_TRUE,	2},
	{GLSLIC_OP_(SLT)				IMG_TRUE,	2},
	{GLSLIC_OP_(SLE)				IMG_TRUE,	2},
	{GLSLIC_OP_(SGT)				IMG_TRUE,	2},
	{GLSLIC_OP_(SGE)				IMG_TRUE,	2},

	{GLSLIC_OP_(NOT)				IMG_TRUE,	1},

	/* conditional execution ops */
	{GLSLIC_OP_(IF)					IMG_FALSE,	1},
	{GLSLIC_OP_(IFNOT)				IMG_FALSE,	1},

	{GLSLIC_OP_(IFLT)				IMG_FALSE,	2}, 
	{GLSLIC_OP_(IFLE)				IMG_FALSE,	2}, 
	{GLSLIC_OP_(IFGT)				IMG_FALSE,	2}, 
	{GLSLIC_OP_(IFGE)				IMG_FALSE,	2}, 
	{GLSLIC_OP_(IFEQ)				IMG_FALSE,	2}, 
	{GLSLIC_OP_(IFNE)				IMG_FALSE,	2}, 

	{GLSLIC_OP_(ELSE)				IMG_FALSE,	0},
	{GLSLIC_OP_(ENDIF)				IMG_FALSE,	0},

	/* procedure call/return ops */
	{GLSLIC_OP_(LABEL)				IMG_FALSE,	1},
	{GLSLIC_OP_(RET)				IMG_FALSE,	0},
	{GLSLIC_OP_(CALL)				IMG_FALSE,	1},

	/* loop ops */
	{GLSLIC_OP_(LOOP)				IMG_FALSE,	0},
	{GLSLIC_OP_(STATICLOOP)			IMG_FALSE,	0},
	{GLSLIC_OP_(ENDLOOP)			IMG_FALSE,	0},
	{GLSLIC_OP_(CONTINUE)			IMG_FALSE,	0},
	{GLSLIC_OP_(CONTDEST)			IMG_FALSE,	0},
	{GLSLIC_OP_(BREAK)				IMG_FALSE,	0},

	{GLSLIC_OP_(DISCARD)			IMG_FALSE,	0},

	/* Potential extension ops */
	{GLSLIC_OP_(SANY)				IMG_TRUE,	1},
	{GLSLIC_OP_(SALL)				IMG_TRUE,	1},

	{GLSLIC_OP_(ABS)				IMG_TRUE,	1},

	{GLSLIC_OP_(MIN)				IMG_TRUE,	2}, 
	{GLSLIC_OP_(MAX)				IMG_TRUE,	2},
	{GLSLIC_OP_(SGN)				IMG_TRUE,	1},

	{GLSLIC_OP_(RCP)				IMG_TRUE,	1},
	{GLSLIC_OP_(RSQ)				IMG_TRUE,	1},

	{GLSLIC_OP_(LOG)				IMG_TRUE,	1},
	{GLSLIC_OP_(LOG2)				IMG_TRUE,	1},
	{GLSLIC_OP_(EXP)				IMG_TRUE,	1},
	{GLSLIC_OP_(EXP2)				IMG_TRUE,	1},

	{GLSLIC_OP_(DOT)				IMG_TRUE,	2}, 
	{GLSLIC_OP_(CROSS)				IMG_TRUE,	2},

	{GLSLIC_OP_(SIN)				IMG_TRUE,	1},
	{GLSLIC_OP_(COS)				IMG_TRUE,	1},

	{GLSLIC_OP_(FLOOR)				IMG_TRUE,	1},
	{GLSLIC_OP_(CEIL)				IMG_TRUE,	1},

	{GLSLIC_OP_(DFDX)				IMG_TRUE,	1},
	{GLSLIC_OP_(DFDY)				IMG_TRUE,	1},

	{GLSLIC_OP_(TEXLD)				IMG_TRUE,	2},
	{GLSLIC_OP_(TEXLDP)				IMG_TRUE,	2},
	{GLSLIC_OP_(TEXLDB)				IMG_TRUE,	3},
	{GLSLIC_OP_(TEXLDL)				IMG_TRUE,	3},
	{GLSLIC_OP_(TEXLDD)				IMG_TRUE,	4},

	{GLSLIC_OP_(POW)				IMG_TRUE,	2},
	{GLSLIC_OP_(FRC)				IMG_TRUE,	1},
	{GLSLIC_OP_(MOD)				IMG_TRUE,	2},

	{GLSLIC_OP_(TAN)				IMG_TRUE,	1},
	{GLSLIC_OP_(ASIN)				IMG_TRUE,	1},
	{GLSLIC_OP_(ACOS)				IMG_TRUE,	1},
	{GLSLIC_OP_(ATAN)				IMG_TRUE,	1},

	{GLSLIC_OP_(MAD)				IMG_TRUE,	3},
	{GLSLIC_OP_(MIX)				IMG_TRUE,	3},

	{GLSLIC_OP_(NRM3)				IMG_TRUE,	1},

};

#if defined(DEBUG) || defined(DUMP_LOGFILES)
#define GLSLNT_(nt) GLSLNT_##nt, "GLSLNT_"#nt, 
#else
#define GLSLNT_(nt)
#endif

/* 
   GLSL IMPLICIT CONVERSIONS

   Structure constructors (for members)
   Array constructors

   =  += -= *= /= %=   -- If left float and right int convert right to float 
      +  -  *  /  %    -- If one side int and one side float convert int side to float
   <  >  <= >=         -- If one side int and one side float convert int side to float
   == !=               -- If one side int and one side float convert int side to float
   ?                   -- If left side float convert 2nd and/or 3rd expression to float if int
*/
#ifndef GLSL_ES
#define NT_ITOF(bNodeAllowsIntToFloatConversion) ,IMG_##bNodeAllowsIntToFloatConversion
#else
#define NT_ITOF(bNodeAllowsIntToFloatConversion)
#endif

IMG_INTERNAL const NodeTypeTable asNodeTable[] =
{
	/* Primary expression types	*/
	{GLSLNT_(IDENTIFIER)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* postfix expression types	*/		
	{GLSLNT_(FIELD_SELECTION)		GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(ARRAY_SPECIFIER)		GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(POST_INC)				GLSLIC_OP_ADD			NT_ITOF(FALSE)},
	{GLSLNT_(POST_DEC)				GLSLIC_OP_SUB			NT_ITOF(FALSE)},
	{GLSLNT_(FUNCTION_CALL)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(ARRAY_LENGTH)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* unary expression types	*/		
	{GLSLNT_(PRE_INC)				GLSLIC_OP_ADD			NT_ITOF(FALSE)},
	{GLSLNT_(PRE_DEC)				GLSLIC_OP_SUB			NT_ITOF(FALSE)},
	{GLSLNT_(NEGATE)				GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(POSITIVE)				GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(NOT)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* multiplicative expression types	*/
	{GLSLNT_(MULTIPLY)				GLSLIC_OP_MUL			NT_ITOF(TRUE)},
	{GLSLNT_(DIVIDE)				GLSLIC_OP_DIV			NT_ITOF(TRUE)},

	/* additive expression types	*/
	{GLSLNT_(ADD)					GLSLIC_OP_ADD			NT_ITOF(TRUE)},
	{GLSLNT_(SUBTRACT)				GLSLIC_OP_SUB			NT_ITOF(TRUE)},



	/* relational expression types	*/
	{GLSLNT_(LESS_THAN)				GLSLIC_OP_SLT			NT_ITOF(TRUE)},		
	{GLSLNT_(GREATER_THAN)			GLSLIC_OP_SGT			NT_ITOF(TRUE)},		
	{GLSLNT_(LESS_THAN_EQUAL)		GLSLIC_OP_SLE			NT_ITOF(TRUE)},		
	{GLSLNT_(GREATER_THAN_EQUAL)	GLSLIC_OP_SGE			NT_ITOF(TRUE)},		

	/* equality expression types	*/
	{GLSLNT_(EQUAL_TO)				GLSLIC_OP_SEQ			NT_ITOF(TRUE)},		
	{GLSLNT_(NOTEQUAL_TO)			GLSLIC_OP_SNE			NT_ITOF(TRUE)},		


	/* logical expression types	*/
	{GLSLNT_(LOGICAL_OR)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(LOGICAL_XOR)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(LOGICAL_AND)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* conditional expression	*/	
	{GLSLNT_(QUESTION)				GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* assignment operator types	*/
	{GLSLNT_(EQUAL)					GLSLIC_OP_MOV			NT_ITOF(TRUE)},
	{GLSLNT_(MUL_ASSIGN)			GLSLIC_OP_MUL			NT_ITOF(TRUE)},
	{GLSLNT_(DIV_ASSIGN)			GLSLIC_OP_DIV			NT_ITOF(TRUE)},
	{GLSLNT_(ADD_ASSIGN)			GLSLIC_OP_ADD			NT_ITOF(TRUE)},
	{GLSLNT_(SUB_ASSIGN)			GLSLIC_OP_SUB			NT_ITOF(TRUE)},

	/* expression types	*/	
	{GLSLNT_(SUBEXPRESSION)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(EXPRESSION)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* jump statement types	*/
	{GLSLNT_(CONTINUE)				GLSLIC_OP_CONTINUE		NT_ITOF(FALSE)},
	{GLSLNT_(BREAK)					GLSLIC_OP_BREAK			NT_ITOF(FALSE)},
	{GLSLNT_(DISCARD)				GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(RETURN)				GLSLIC_OP_RET			NT_ITOF(FALSE)},	

	/* flow control	*/
	{GLSLNT_(FOR)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(WHILE)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(DO)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(IF)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	{GLSLNT_(STATEMENT_LIST)		GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	{GLSLNT_(FUNCTION_DEFINITION)	GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},
	{GLSLNT_(DECLARATION)			GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	/* top level node	*/
	{GLSLNT_(SHADER)				GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)},

	{GLSLNT_(ERROR)					GLSLIC_OP_FORCEDWORD	NT_ITOF(FALSE)}		
};					

/* 
** Default swiz mask. 
** When a default swiz mask applies to an operand, actually full swiz mask applies.
*/
IMG_INTERNAL const GLSLICVecSwizWMask sDefaultSwizMask = 
{
	0, {GLSLIC_VECCOMP_X, GLSLIC_VECCOMP_X, GLSLIC_VECCOMP_X, GLSLIC_VECCOMP_X}
};

static IMG_BOOL PostEvaluateNode(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psOperandInfo);
static IMG_BOOL IsNodeDirectOperand(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode);
static IMG_VOID ProcessNode(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcssRightHand);

#ifdef SRC_DEBUG
/******************************************************************************
 * Function Name: GetNodeSourceLineNumber
 *
 * Inputs       : psNode
 * Outputs      : 
 * Returns      : IMG_UINT32 source line number 
 * Globals Used : -
 *
 * Description  : Returns the source line number for the given GLSLNode. 
 *                Returns COMP_UNDEFINED_SOURCE_LINE if psNode == IMG_NULL or
 *				  psNode->psToken == IMG_NULL.
 *****************************************************************************/
static IMG_UINT32 GetNodeSourceLineNumber(GLSLNode *psNode)
{
	if((psNode == IMG_NULL) || 
		((psNode->psToken) == IMG_NULL))
	{
		return COMP_UNDEFINED_SOURCE_LINE;
	}
	else
	{
		return psNode->psToken->uLineNumber;
	}
}

/******************************************************************************
 * Function Name: SearchNodeSourceLineForward
 *
 * Inputs       : psNode, psExcludeNode
 * Outputs      : 
 * Returns      : IMG_UINT32 source line number 
 * Globals Used : -
 *
 * Description  : Searches for source line number in the childrens of given
 *                psNode. psExcludeNode is not included in the search.
 *****************************************************************************/
static IMG_UINT32 SearchNodeSourceLineForward(GLSLNode *psNode, 
											  GLSLNode *psExcludeNode)
{
	IMG_UINT32 i;
	IMG_UINT32 uTemp;

	uTemp = GetNodeSourceLineNumber(psNode);

	if(uTemp != COMP_UNDEFINED_SOURCE_LINE)
	{
		return uTemp;
	}

	/* Now Search all the childrens of this node. */
	for(i=0; i<(psNode->uNumChildren); i++)
	{
		/* do we have a exclude node */
		if((psExcludeNode == IMG_NULL) || 
			(psExcludeNode != (psNode->ppsChildren[i])))
		{
			uTemp = GetNodeSourceLineNumber(psNode->ppsChildren[i]);
			if( uTemp != COMP_UNDEFINED_SOURCE_LINE)
			{
				return uTemp;
			}	
		}
	}

	/* Now traverse the childrens. */
	for(i=0; i<(psNode->uNumChildren); i++)
	{
		if((psExcludeNode == IMG_NULL) || 
			(psExcludeNode != (psNode->ppsChildren[i])))
		{
			return SearchNodeSourceLineForward((psNode->ppsChildren[i]), 
				psExcludeNode);
		}
	}
	return COMP_UNDEFINED_SOURCE_LINE;
}

/******************************************************************************
 * Function Name: SearchNodeSourceLineBackward
 *
 * Inputs       : psNode
 * Outputs      : 
 * Returns      : IMG_UINT32 source line number 
 * Globals Used : -
 *
 * Description  : Searches for source line number in the parent and grand 
 *                parents of given psNode.
 *****************************************************************************/
static IMG_UINT32 SearchNodeSourceLineBackward(GLSLNode *psNode)
{
	GLSLNode *psExcludeNode;
	GLSLNode *psParentNode;
	IMG_UINT32 uTemp;

	psParentNode = psNode->psParent;

	psExcludeNode = psNode;

	while(psParentNode)
	{
		uTemp = 
			SearchNodeSourceLineForward(psParentNode, psExcludeNode);
		
		if(uTemp != COMP_UNDEFINED_SOURCE_LINE)
		{
			return uTemp;
		}
		/* Move one step up in tree */
		psExcludeNode = psParentNode;
		psParentNode = psParentNode->psParent;
	}
	return COMP_UNDEFINED_SOURCE_LINE;
}

/******************************************************************************
 * Function Name: SearchNodeSourceLine
 *
 * Inputs       : psNode
 * Outputs      : 
 * Returns      : IMG_UINT32 source line number 
 * Globals Used : -
 *
 * Description  : Searches for source line number in the psNode, its childrens 
 *                , parent and grand parents of psNode.
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 SearchNodeSourceLine(GLSLNode *psNode)
{
	IMG_UINT32 uTemp;

	uTemp = SearchNodeSourceLineForward(psNode, IMG_NULL);

	if(uTemp != COMP_UNDEFINED_SOURCE_LINE)
	{
		return uTemp;
	}

	uTemp = SearchNodeSourceLineBackward(psNode);

	if(uTemp != COMP_UNDEFINED_SOURCE_LINE)
	{
		return uTemp;
	}

	return COMP_UNDEFINED_SOURCE_LINE;
}
#endif /* SRC_DEBUG */

/******************************************************************************
 * Function Name: ICAddTemporary
 *
 * Inputs       : eTypeSpecifier
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Create a temp symbol with specific type eTypeSpecifier, 
 *				  Return the symbol ID
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddTemporary(GLSLCompilerPrivateData *psCPD,
									 GLSLICProgram				*psICProgram,
									GLSLTypeSpecifier			eTypeSpecifier,
									GLSLPrecisionQualifier	ePrecisionQualifier,
									IMG_UINT32				*puSymbolID)
{
	IMG_UINT32 uSymbolID;
	IMG_CHAR pszResultName[20];
	GLSLIdentifierData sData;
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	/* Set up data */
	sData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	sData.eLValueStatus                                = GLSLLV_NOT_L_VALUE;

	INIT_FULLY_SPECIFIED_TYPE(&(sData.sFullySpecifiedType));
	sData.sFullySpecifiedType.eTypeSpecifier           = eTypeSpecifier;
	sData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
	sData.sFullySpecifiedType.ePrecisionQualifier      = ePrecisionQualifier;

	sData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
	sData.eIdentifierUsage                             = (GLSLIdentifierUsage)0;
	sData.iActiveArraySize							   = -1;
	sData.eArrayStatus								   = GLSLAS_NOT_ARRAY;
	sData.uConstantDataSize                            = 0;
	sData.pvConstantData                               = IMG_NULL;
	sData.uConstantAssociationSymbolID                 = 0;

	sprintf( pszResultName, "tmp%s@%u", GLSLTypeSpecifierDescTable(eTypeSpecifier), psICContext->uTempIssusedNo);

	if (!AddResultData(psCPD, psICProgram->psSymbolTable, 
					   pszResultName, 
					   &sData, 
					   IMG_FALSE, 
					   &uSymbolID))
	{
		LOG_INTERNAL_ERROR(("ICAddTemporary: Failed to add temporary %s to symbol table", pszResultName));
		*puSymbolID = 0;
		return IMG_FALSE;
	}

	psICContext->uTempIssusedNo++;

	*puSymbolID = uSymbolID;

	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: ICAddTemporary
 *
 * Inputs       : eTypeSpecifier
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Create a temp symbol with specific type eTypeSpecifier, 
 *				  Return the symbol ID
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddBoolPredicate(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 *puSymbolID)
{
	IMG_UINT32 uSymbolID;
	IMG_CHAR pszResultName[20];
	GLSLIdentifierData sData;
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	/* Set up data */
	sData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	sData.eLValueStatus                                = GLSLLV_NOT_L_VALUE;

	INIT_FULLY_SPECIFIED_TYPE(&(sData.sFullySpecifiedType));
	sData.sFullySpecifiedType.eTypeSpecifier           = GLSLTS_BOOL;
	sData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
	sData.sFullySpecifiedType.ePrecisionQualifier      = GLSLPRECQ_UNKNOWN;

	sData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;
	sData.eIdentifierUsage                             = GLSLIU_BOOL_AS_PREDICATE;
	sData.iActiveArraySize							   = -1;
	sData.eArrayStatus								   = GLSLAS_NOT_ARRAY;
	sData.uConstantDataSize                            = 0;
	sData.pvConstantData                               = IMG_NULL;
	sData.uConstantAssociationSymbolID                 = 0;

	sprintf( pszResultName, "pred@%u", psICContext->uTempIssusedNo);

	if (!AddResultData(psCPD, psICProgram->psSymbolTable, 
					   pszResultName, 
					   &sData, 
					   IMG_FALSE, 
					   &uSymbolID))
	{
		LOG_INTERNAL_ERROR(("ICAddTemporary: Failed to add temporary %s to symbol table", pszResultName));
		*puSymbolID = 0;
		return IMG_FALSE;
	}

	psICContext->uTempIssusedNo++;

	*puSymbolID = uSymbolID;

	return IMG_TRUE;
}


#if defined(DEBUG) || defined(DUMP_LOGFILES)
/******************************************************************************
 * Function Name: DumpICOeprand
 *
 * Inputs       : str, uEnd, psICProgram, psOperand
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Dump an operand of an instruction to str
 *****************************************************************************/
static IMG_UINT32 DumpICOperand(IMG_CHAR *str, IMG_UINT32 uEnd, SymTable *psSymbolTable, GLSLICOperand *psOperand)
{
	IMG_CHAR Name[256];
	IMG_INT32 i = 0;
	IMG_UINT32 j;

	/* Print "!" if negate */
	if(psOperand->eInstModifier & GLSLIC_MODIFIER_NEGATE)
	{
		i = sprintf(Name, "-");
	}

	/* Print name for symbolID */
	if(psOperand->uSymbolID)
	{
		i += sprintf(Name + i, "%s", GetSymbolName(psSymbolTable, psOperand->uSymbolID));
	}
	else
	{
		i +=sprintf(Name + i, "ERROR");
	}

	/* Array notation */

	for(j = 0; j < psOperand->uNumOffsets; j++)
	{
		i += sprintf(Name + i, "[");

		if(psOperand->psOffsets[j].uOffsetSymbolID)
		{
			i += sprintf(Name + i, "%s", GetSymbolName(psSymbolTable, psOperand->psOffsets[j].uOffsetSymbolID));
		}
		else
		{
			i += sprintf(Name + i, "%u", psOperand->psOffsets[j].uStaticOffset);
		}
		i += sprintf(Name + i, "]");
	}
	
	/* Print out the swizzle mask */
	if(psOperand->sSwizWMask.uNumComponents)
	{
		IMG_CHAR swizzle[4][2] ={"x", "y", "z", "w"};
		IMG_UINT32 j;

		i += sprintf(Name+ i, ".");
		for(j = 0; j < psOperand->sSwizWMask.uNumComponents; j++)
		{
			i += sprintf(Name + i, "%s", swizzle[psOperand->sSwizWMask.aeVecComponent[j]]);
		}
	}

	uEnd += sprintf(str + uEnd, "%-20s ", Name);

	return uEnd;
}
#endif /* defined(DEBUG) || defined(DUMP_LOGFILES) */


#ifdef DUMP_LOGFILES

/******************************************************************************
 * Function Name: ICDumpICInstruction
 *
 * Inputs       : instr, uInstrNum
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Dump one intermediate code instruction
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICDumpICInstruction(IMG_UINT32			uLogFile,
										SymTable     		*psSymbolTable, 
										IMG_UINT32			uIndent, 
										GLSLICInstruction	*psICInstr, 
										IMG_UINT32			uInstrNum,
										IMG_BOOL			bDumpOriginalLine,
										IMG_BOOL			bDumpSymbolID)
{
	IMG_CHAR str[512];
	IMG_UINT32 i = 0, j;
	IMG_UINT32 uNumSrcs;
	IMG_UINT32 uCommentsStart = 90;

	/* Original line of the code associated with this psICInstr */
	if(bDumpOriginalLine/* && psICInstr->pszOriginalLine*/)
	{
		i = 0; 
		j = 0;
#if 0
		while(psICInstr->pszOriginalLine[j]==' ' || psICInstr->pszOriginalLine[j] == '\t')
		{
			j++;
		}
		
		for(i = 0; i < uCommentsStart; i++)
		{
			str[i] = ' ';
		}
	
		i += sprintf(str + i, "; ");
#endif	
		if(psICInstr->pszOriginalLine)
		{
			for(; psICInstr->pszOriginalLine[j] != '\n' && psICInstr->pszOriginalLine[j] != '\0' && i < 511; i++, j++)
			{
				str[i] = psICInstr->pszOriginalLine[j];
			}
			str[i] = '\0';

			DumpLogMessage(LOGFILE_ICODE, 0, "\n%s\n", str);
			DumpLogMessage(LOGFILE_ICODE, 0, "========================\n");
		}
		else
		{
			DumpLogMessage(LOGFILE_ICODE, 0, "\n");
		}
	}

	/* Indent for IF-ELSE_END and LOOP-ENDLOOP */
	i = 0;
	i = sprintf(str + i,"%4u: ", uInstrNum);

	for(j = 0; j < uIndent * 2; j++)
	{
		str[i++] = ' ';
	}
	
	/* Predicate */
	if(psICInstr->uPredicateBoolSymID)
	{
		if(psICInstr->bPredicateNegate)
		{
			i += sprintf(str + i, "!");
		}

		i += sprintf(str + i, "%s ", GetSymbolName(psSymbolTable, psICInstr->uPredicateBoolSymID));

	}

	/* OPCODE */
	i += sprintf(str + i, "%-8s", ICOP_DESC(psICInstr->eOpCode));

	/* Operands */
	uNumSrcs = ICOP_NUM_SRCS(psICInstr->eOpCode);
	for(j = 0; j < uNumSrcs + 1; j++)
	{
		if(!ICOP_HAS_DEST(psICInstr->eOpCode) && j == DEST) continue;
		
		i = DumpICOperand(str, i,  psSymbolTable, &psICInstr->asOperand[j]);
	}

	if(bDumpSymbolID)
	{
		/* Comments showing symbol name and ID */
		for(j = i; j < uCommentsStart; j++)
		{
			str[j] = ' ';
		}
		i = j;
		i += sprintf(str + i, "; ");

		for(j =0; j < uNumSrcs + 1; j++)
		{
			if(!ICOP_HAS_DEST(psICInstr->eOpCode) && j == DEST) continue;

			if(psICInstr->asOperand[j].uSymbolID)
			{
				IMG_CHAR *psSymbolName = GetSymbolName(psSymbolTable, psICInstr->asOperand[j].uSymbolID);
				i += sprintf(str + i, "%s=0x%X ", psSymbolName, psICInstr->asOperand[j].uSymbolID);
			}
			else
			{
				i += sprintf(str + i, "ERROR:ZERO ID ");
			}
		}
	}

	if(psICInstr->eOpCode == GLSLIC_OP_LABEL)
	{
		DumpLogMessage(uLogFile, 0, "\n");
	}
	DumpLogMessage(uLogFile, 0, "%s\n", str);
}
#endif


/******************************************************************************
 * Function Name: GetICOpCode
 *
 * Inputs       : eNodeType
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return an icode op code from a node type
 *****************************************************************************/
static GLSLICOpcode GetICOpCode(GLSLNodeType eNodeType)
{
	GLSLICOpcode eOpcode;
	
	eOpcode = NODETYPE_ICODEOP(eNodeType);

	//DebugAssert(eOpcode == ICOP_ICOP(eOpcode));

	return eOpcode;
}

/******************************************************************************
 * Function Name: ICInitICOperand
 *
 * Inputs       : uSymbolID
 * Outputs      : psOperand
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Take a symbol ID and Setup an default IC operand
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICInitICOperand(IMG_UINT32 uSymbolID, GLSLICOperand *psOperand)
{
	psOperand->eInstModifier  = GLSLIC_MODIFIER_NONE;
	psOperand->sSwizWMask     = sDefaultSwizMask;
	psOperand->uNumOffsets    = 0;
	psOperand->psOffsets      = IMG_NULL;
	psOperand->uSymbolID      = uSymbolID;
}

/******************************************************************************
 * Function Name: ICInitOperandInfo
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Initilise an operand info structure with basic symbol ID. 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICInitOperandInfo(IMG_UINT32 uSymbolID, GLSLICOperandInfo *psOperandInfo)
{
	psOperandInfo->uSymbolID       = uSymbolID;
	psOperandInfo->sSwizWMask      = sDefaultSwizMask;
	psOperandInfo->eInstModifier   = GLSLIC_MODIFIER_NONE;
	psOperandInfo->psOffsetList    = IMG_NULL;
	psOperandInfo->psOffsetListEnd = IMG_NULL;
}

/******************************************************************************
 * Function Name: ICSetupICOperand
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Setup an IC Operand from operand info.
 *****************************************************************************/
static IMG_VOID ICSetupICOperand(GLSLCompilerPrivateData *psCPD, const GLSLICOperandInfo *psOperandInfo, GLSLICOperand *psICOperand)
{
	psICOperand->eInstModifier = psOperandInfo->eInstModifier;
	psICOperand->sSwizWMask    = psOperandInfo->sSwizWMask;
	psICOperand->uSymbolID     = psOperandInfo->uSymbolID;

	if(psOperandInfo->psOffsetList)
	{
		GLSLICOperandOffsetList *walker;
		IMG_UINT32 i = 0;

		/* count how many offsets */
		walker = psOperandInfo->psOffsetList;
		while(walker)
		{
			i++;
			walker = walker->psNext;
		}
		psICOperand->uNumOffsets = i;

		/* pass over the offset information */
		if(psICOperand->uNumOffsets)
		{
			psICOperand->psOffsets = DebugMemAlloc(psICOperand->uNumOffsets * sizeof(GLSLICOperandOffset));
			if(psICOperand->psOffsets == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("ICSetupICOperand: Failed to get memory for IC operand offsets \n"));
				psICOperand->uNumOffsets = 0;
				return;
			}
		}
		else
		{
			psICOperand->psOffsets = IMG_NULL;
		}

		i = 0;
		walker = psOperandInfo->psOffsetList;
		while(walker)
		{
			psICOperand->psOffsets[i++] = walker->sOperandOffset;
			walker = walker->psNext;
		}
	}
	else
	{
		psICOperand->uNumOffsets = 0;
		psICOperand->psOffsets = IMG_NULL;
	}
}

/******************************************************************************
 * Function Name: ICAddOperandOffset
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add new offset to the operand info structure
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICAddOperandOffset(GLSLICOperandInfo *psOperandInfo, 
										 IMG_UINT32 uStaticOffset, 
										 IMG_UINT32 uOffsetSymbolID)
{
	GLSLICOperandOffsetList *psListEnd = DebugMemAlloc(sizeof(GLSLICOperandOffsetList));

	psListEnd->psNext = IMG_NULL;
	psListEnd->sOperandOffset.uStaticOffset = uStaticOffset;
	psListEnd->sOperandOffset.uOffsetSymbolID = uOffsetSymbolID;

	if(psOperandInfo->psOffsetList)
	{
		psOperandInfo->psOffsetListEnd->psNext = psListEnd;
	}
	else
	{
		psOperandInfo->psOffsetList = psListEnd;
	}

	psOperandInfo->psOffsetListEnd = psListEnd;
}



/******************************************************************************
 * Function Name: ICSetupICOperand
 *
 * Inputs       : psOperandInfo
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Free the offset list attached to the operand
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICFreeOperandOffsetList(GLSLICOperandInfo *psOperandInfo)
{
	GLSLICOperandOffsetList *psList = psOperandInfo->psOffsetList;

	GLSLICOperandOffsetList *psTemp;
	while(psList)
	{
		psTemp = psList;
		psList = psList->psNext;
		DebugMemFree(psTemp);
	}

	psOperandInfo->psOffsetList = psOperandInfo->psOffsetListEnd = IMG_NULL;
}


/******************************************************************************
 * Function Name: ICProcessNodeOperand
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : psOperand
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Process node and output operand info.
 *				  Note: It might insert IC instructions while processing
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICProcessNodeOperand(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram     *psICProgram,
											GLSLNode			*psNode, 
											GLSLICOperandInfo	*psOperandInfo)
{
	GLSLGenericData *psGenericData;

	psGenericData = (GLSLGenericData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);

	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessNodeOperand: Failed to get symbol data for ID:%u\n", psNode->uSymbolTableID));
		return;
	}

	/* Evaluated ? default setting for this operand applies */
	if(psNode->bEvaluated)
	{
		ICInitOperandInfo(psNode->uSymbolTableID, psOperandInfo);
	}
	else
	{
		switch(psGenericData->eSymbolTableDataType)
		{
		case GLSLSTDT_FUNCTION_CALL:
		case GLSLSTDT_IDENTIFIER:
		{
			if(!PostEvaluateNode(psCPD, psICProgram, psNode, psOperandInfo))
			{
				/* It should not fail at any time */
				LOG_INTERNAL_ERROR(("ICProcessNodeOperand: Failed to post-evaluate the node"));
			}
			break;
		}
		case GLSLSTDT_FUNCTION_DEFINITION:
		case GLSLSTDT_SWIZZLE:
		case GLSLSTDT_MEMBER_SELECTION:
		case GLSLSTDT_STRUCTURE_DEFINITION:
			LOG_INTERNAL_ERROR(("ICProcessNodeOperand: definition, selection etc. cannot directly be an operand\n"));
			break;
		default:
			{
				LOG_INTERNAL_ERROR(("ICProcessNodeOperand: Unknown symbol data type \n"));
				break;
			}
		}
	}	
}

/******************************************************************************
 * Function Name: UnhookInstructions
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : psOperand
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Unhook continuous instructions from the linked list. 
 *****************************************************************************/
static IMG_VOID UnhookInstructions(GLSLICProgram     *psICProgram,
									GLSLICInstruction *psStart,
									GLSLICInstruction *psEnd)
{
	if(psStart == psICProgram->psInstrHead && psEnd == psICProgram->psInstrTail)
	{
		psICProgram->psInstrHead = IMG_NULL;
		psICProgram->psInstrTail = IMG_NULL;
	}
	else if(psStart == psICProgram->psInstrHead)
	{
		psICProgram->psInstrHead = psEnd->psNext;
		psICProgram->psInstrHead->psPrev = IMG_NULL;
	}
	else if(psEnd == psICProgram->psInstrTail)
	{	
		psICProgram->psInstrTail = psStart->psPrev;
		psICProgram->psInstrTail->psNext = IMG_NULL;
	}
	else
	{
		psEnd->psNext->psPrev = psStart->psPrev;
		psStart->psPrev->psNext = psEnd->psNext;
	}
}

/******************************************************************************
 * Function Name: InsertInstructionsAfter
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : psOperand
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Insert instructions to the linked list after psAfter
 *****************************************************************************/
static IMG_VOID InsertInstructionsAfter(GLSLICProgram     *psICProgram,
										GLSLICInstruction *psAfter,
										GLSLICInstruction *psInsertStart,
										GLSLICInstruction *psInsertEnd)
{
	if(psAfter)
	{
		if(psAfter == psICProgram->psInstrTail)
		{
			psInsertStart->psPrev = psICProgram->psInstrTail;
			psICProgram->psInstrTail->psNext = psInsertStart;

			psICProgram->psInstrTail = psInsertEnd;
			psICProgram->psInstrTail->psNext = IMG_NULL;
		}
		else
		{
			psInsertEnd->psNext = psAfter->psNext;

			psAfter->psNext->psPrev = psInsertEnd;

			psAfter->psNext = psInsertStart;
			psInsertStart->psPrev = psAfter;
		}
	}
	else
	{
		psInsertEnd->psNext = psICProgram->psInstrHead;

		/* Add to the top */
		if(psICProgram->psInstrHead)
		{
			psICProgram->psInstrHead->psPrev = 	psInsertEnd;
		}
		else
		{
			psICProgram->psInstrTail = psInsertEnd;	
		}
		psICProgram->psInstrHead = psInsertStart;
		psICProgram->psInstrHead->psPrev = IMG_NULL;

	}
}


/******************************************************************************
 * Function Name: FreeICInstruction
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_VOID FreeICInstruction(GLSLICProgram *psICProgram, GLSLICInstruction *psICInstr)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	IMG_UINT32 i;

	for(i = 0; i < ICOP_NUM_SRCS(psICInstr->eOpCode) + 1; i++)
	{
		if(!ICOP_HAS_DEST(psICInstr->eOpCode) && i==DEST) continue;

		if(psICInstr->asOperand[i].uNumOffsets)
		{
			DebugMemFree(psICInstr->asOperand[i].psOffsets);
		}
	}

	DebugFreeHeapItem(psICContext->psInstructionHeap, psICInstr);
}

/******************************************************************************
 * Function Name: ICRemoveInstruction
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICRemoveInstruction(GLSLICProgram     *psICProgram, GLSLICInstruction *psICInstr)
{
	UnhookInstructions(psICProgram, psICInstr, psICInstr);

	FreeICInstruction(psICProgram, psICInstr);
}

/******************************************************************************
 * Function Name: ICRemoveInstructionRange
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICRemoveInstructionRange(GLSLICProgram     *psICProgram,
												GLSLICInstruction *psStart,
												GLSLICInstruction *psEnd)
{
	GLSLICInstruction *psInstr,  *psTemp;

	UnhookInstructions(psICProgram, psStart, psEnd);

	psEnd->psNext = IMG_NULL;
	psInstr = psStart;
	while(psInstr)
	{
		psTemp = psInstr;
		psInstr = psInstr->psNext;
		FreeICInstruction(psICProgram, psTemp);
	}

}

static IMG_VOID IncrementConditionLevel(GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	psICContext->iConditionLevel++;
}

static IMG_VOID DecrementConditionLevel(GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	psICContext->iConditionLevel--;
}

/******************************************************************************
 * Function Name: ICGetNewInstruction
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return a new intructin to be filled in
 *****************************************************************************/
static GLSLICInstruction *ICGetNewInstruction(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext     *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLICInstruction *psICInstr;

	psICInstr = DebugAllocHeapItem(psICContext->psInstructionHeap);
	if(psICInstr == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICGetNewInstruction: Failed to ger memory for an instruction"));
		return IMG_NULL;
	}

	/* Zero all members */
	memset(psICInstr, 0, sizeof(GLSLICInstruction));

	#ifdef SRC_DEBUG
	psICInstr->uSrcLine = psCPD->uCurSrcLine;
	/* Uninitalized source line number */
	DebugAssert((psICInstr->uSrcLine) != COMP_UNDEFINED_SOURCE_LINE);
	#endif /* SRC_DEBUG */

	/* Hook into the list */
	InsertInstructionsAfter(psICProgram, psICProgram->psInstrTail, psICInstr, psICInstr);

	return psICInstr;
}


/*****************************************************************************
 FUNCTION	: GetSymbolInformation

 PURPOSE	: Get builtin variable ID from its symbol ID

 PARAMETERS	: psUFContext				- UF context.
			  uId						- Id to get the builtin ID for.
			  
 RETURNS	: Builtin variable ID
*****************************************************************************/
IMG_INTERNAL IMG_BOOL ICGetSymbolInformation(GLSLCompilerPrivateData *psCPD,
											 SymTable				*psSymbolTable,
											IMG_UINT32				uSymbolID,
											GLSLBuiltInVariableID	*peBuiltinID,
											GLSLFullySpecifiedType	**ppsFullType,
											IMG_INT32				*piArraySize,
											GLSLIdentifierUsage		*peIdentifierUsage,
											IMG_VOID				**ppvConstantData)

{
	GLSLGenericData			*psGenericData;
	GLSLFullySpecifiedType	*psFullType			= IMG_NULL;
	IMG_INT32				iArraySize			= 0;
	GLSLBuiltInVariableID	eBuiltinID			= GLSLBV_NOT_BTIN;
	IMG_VOID				*pvConstantData		= IMG_NULL;
	GLSLIdentifierUsage		eIdentifierUsage	= (GLSLIdentifierUsage)0;

	/* Get generic data */
	psGenericData = (GLSLGenericData *) GetSymbolTableData(psSymbolTable, uSymbolID);
	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GetSymbolInformation: Failed to symbol table data for id=%d\n", uSymbolID));
		return IMG_FALSE;
	}

	switch (psGenericData->eSymbolTableDataType)
	{
		case GLSLSTDT_IDENTIFIER: 
			{
				GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData*)psGenericData;
	
				psFullType = &psIdentifierData->sFullySpecifiedType;

				eBuiltinID = psIdentifierData->eBuiltInVariableID;
	
				if(psIdentifierData->eArrayStatus != GLSLAS_NOT_ARRAY)
				{
					iArraySize = psIdentifierData->iActiveArraySize;

					DebugAssert(iArraySize != -1);
				}

				eIdentifierUsage = psIdentifierData->eIdentifierUsage;
				
				pvConstantData = psIdentifierData->pvConstantData;

				break;
			}

		case GLSLSTDT_FUNCTION_DEFINITION: 
			LOG_INTERNAL_ERROR(("GetSymbolInformation: Function definition cannot be an operand \n")); 

			return IMG_FALSE;

		case GLSLSTDT_FUNCTION_CALL: 
		{
			GLSLFunctionCallData *psData;
			psData = (GLSLFunctionCallData *)psGenericData;

			psFullType = &psData->sFullySpecifiedType;

			eBuiltinID = GLSLBV_NOT_BTIN;

			iArraySize = psData->sFullySpecifiedType.iArraySize;

			eIdentifierUsage |= GLSLIU_INTERNALRESULT;

			break;
		}

		case GLSLSTDT_SWIZZLE: 
			LOG_INTERNAL_ERROR(("GetSymbolInformation: Swizzle data type cannot be an operand \n"));
			return IMG_FALSE;
		default: 
			return IMG_FALSE;
	}

	if(ppsFullType)				*ppsFullType = psFullType;
	if(peBuiltinID)				*peBuiltinID = eBuiltinID;
	if(piArraySize)				*piArraySize = iArraySize;
	if(peIdentifierUsage)		*peIdentifierUsage = eIdentifierUsage;
	if(ppvConstantData)			*ppvConstantData = pvConstantData;

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICGetSymbolFullType
 *
 * Inputs       : psSymbolTable, uSymbolID
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return the type specifier of a symbol ID 
 *****************************************************************************/
IMG_INTERNAL GLSLFullySpecifiedType *ICGetSymbolFullType(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID)
{

	GLSLFullySpecifiedType *psFullType;

	if(!ICGetSymbolInformation(psCPD, psSymbolTable, uSymbolID, IMG_NULL, &psFullType, IMG_NULL, IMG_NULL, IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICGetSymbolFullType: Failed to get symbol information\n"));
		return IMG_NULL;
	}

	return psFullType;
}

/******************************************************************************
 * Function Name: ICGetSymbolTypeSpecifier
 *
 * Inputs       : psSymbolTable, uSymbolID
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return the type specifier of a symbol ID 
 *****************************************************************************/
IMG_INTERNAL GLSLTypeSpecifier ICGetSymbolTypeSpecifier(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID)
{
	GLSLFullySpecifiedType *psFullType = ICGetSymbolFullType(psCPD, psSymbolTable, uSymbolID);

	return psFullType->eTypeSpecifier;

}

/******************************************************************************
 * Function Name: ICGetSymbolPrecision
 *
 * Inputs       : psSymbolTable, uSymbolID
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return the type specifier of a symbol ID 
 *****************************************************************************/
IMG_INTERNAL GLSLPrecisionQualifier ICGetSymbolPrecision(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID)
{
	GLSLFullySpecifiedType *psFullType = ICGetSymbolFullType(psCPD, psSymbolTable, uSymbolID);

	return psFullType->ePrecisionQualifier;

}

/******************************************************************************
 * Function Name: ICGetSymbolBIVariableID
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return builtin ID of a symbol ID 
 *****************************************************************************/
IMG_INTERNAL GLSLBuiltInVariableID ICGetSymbolBIVariableID(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID)
{
	GLSLBuiltInVariableID eBuiltinID = GLSLBV_NOT_BTIN;

	if(!ICGetSymbolInformation(psCPD, psSymbolTable, uSymbolID, &eBuiltinID, IMG_NULL, IMG_NULL, IMG_NULL, IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICGetSymbolFullType: Failed to get symbol information\n"));
	}

	return eBuiltinID;
}


/******************************************************************************
 * Function Name: GetIndexedType
 *
 * Inputs       : psSymbolTable, sType, uOffset, piArraySize
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return the fully specified type and array size when a [] applies to a 
 *				  specific type. uOffset is used only when the base type is a struct.
 *****************************************************************************/
IMG_INTERNAL GLSLFullySpecifiedType GetIndexedType(GLSLCompilerPrivateData			*psCPD,
													SymTable						*psSymbolTable,
													const GLSLFullySpecifiedType	*psBaseType,
													IMG_UINT32						uOffset,
													IMG_INT32						*piArraySize)
{
	
	GLSLFullySpecifiedType sSubscriptType;

	sSubscriptType = *psBaseType;
	*piArraySize = 0;

	switch(psBaseType->eTypeSpecifier)
	{
	case GLSLTS_INVALID:
	case GLSLTS_VOID:
		LOG_INTERNAL_ERROR(("GetIndexedType: invalid or void cannot have subscript \n"));
		break;
	case GLSLTS_FLOAT:
		LOG_INTERNAL_ERROR(("GetIndexedType: No more subscript for float \n"));
		break;
	case GLSLTS_VEC2:
	case GLSLTS_VEC3:
	case GLSLTS_VEC4:
		sSubscriptType.eTypeSpecifier = GLSLTS_FLOAT;
		break;
	case GLSLTS_INT:
		LOG_INTERNAL_ERROR(("GetIndexedType: No more subscript for int \n"));
		break;
	case GLSLTS_IVEC2:
	case GLSLTS_IVEC3:
	case GLSLTS_IVEC4:
		sSubscriptType.eTypeSpecifier = GLSLTS_INT;
		break;
	case GLSLTS_BOOL:
		LOG_INTERNAL_ERROR(("GetIndexedType: No more subscript for bool \n"));
		break;
	case GLSLTS_BVEC2:
	case GLSLTS_BVEC3:
	case GLSLTS_BVEC4:
		sSubscriptType.eTypeSpecifier = GLSLTS_BOOL;
		break;
	case GLSLTS_MAT2X2:
	case GLSLTS_MAT3X2:
	case GLSLTS_MAT4X2:
		sSubscriptType.eTypeSpecifier = GLSLTS_VEC2;
		break;
	case GLSLTS_MAT2X3:
	case GLSLTS_MAT3X3:
	case GLSLTS_MAT4X3:
		sSubscriptType.eTypeSpecifier = GLSLTS_VEC3;
		break;
	case GLSLTS_MAT2X4:
	case GLSLTS_MAT3X4:
	case GLSLTS_MAT4X4:
		sSubscriptType.eTypeSpecifier = GLSLTS_VEC4;
		break;
	case GLSLTS_SAMPLER1D:
	case GLSLTS_SAMPLER2D:
	case GLSLTS_SAMPLER3D:
	case GLSLTS_SAMPLERCUBE:
	case GLSLTS_SAMPLER1DSHADOW:
	case GLSLTS_SAMPLER2DSHADOW:
#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	case GLSLTS_SAMPLER2DRECT:
	case GLSLTS_SAMPLER2DRECTSHADOW:
#endif
	case GLSLTS_SAMPLERSTREAM:
	case GLSLTS_SAMPLEREXTERNAL:
		LOG_INTERNAL_ERROR(("GetIndexedType: No more subscript for samplers \n"));
		break;
	case GLSLTS_STRUCT:
		{
			/* Get the structure definition data */
			GLSLStructureDefinitionData *psStructureDefinitionData 
				= GetSymbolTableData(psSymbolTable, psBaseType->uStructDescSymbolTableID);

			GLSLIdentifierData *psMemberIdentifierData;

			if(psStructureDefinitionData == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("GetIndexedType: Failed to get symbol data for ID:%u\n", psBaseType->uStructDescSymbolTableID));
				return sSubscriptType;
			}

			/* Get information about this member */
			psMemberIdentifierData = &(psStructureDefinitionData->psMembers[uOffset].sIdentifierData);
		
			sSubscriptType = psMemberIdentifierData->sFullySpecifiedType;

			if(psMemberIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY)
			{
				*piArraySize = 0;
			}
			else
			{
				*piArraySize = psMemberIdentifierData->iActiveArraySize;
			}

			break;
		}
	default:
		break;
	}

	return sSubscriptType;
}


/******************************************************************************
 * Function Name: IsSymbolIntConstant
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return TRUE and the int value if ID is an int constant
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IsSymbolIntConstant(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID, IMG_INT32 *piData)
{
	GLSLGenericData *psGenericData;

	GLSLIdentifierData *psIdentifierData;

	psGenericData = GetSymbolTableData(psSymbolTable, uSymbolID);

	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("IsSymbolIntConstant: Failed to get symbol data for ID:%u\n", uSymbolID));
		return IMG_FALSE;
	}
	
	psIdentifierData = (GLSLIdentifierData *)psGenericData;

	if(psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER) 
	{
		if (GLSL_IS_INT_SCALAR(psIdentifierData->sFullySpecifiedType.eTypeSpecifier) &&
			(psIdentifierData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST))
		{
			(*piData) =  (*(IMG_INT32 *)psIdentifierData->pvConstantData);
			
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

/******************************************************************************
 * Function Name: IsSymbolLValue
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IsSymbolLValue(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID, IMG_BOOL *bSingleInteger)
{
	GLSLGenericData *psGenericData;

	GLSLIdentifierData *psIdentifierData;

	psGenericData = GetSymbolTableData(psSymbolTable, uSymbolID);

	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("IsSymbolLValue: Failed to get symbol data for ID:%u\n", uSymbolID));
		return IMG_FALSE;;
	}
	
	psIdentifierData = (GLSLIdentifierData *)psGenericData;

	if(psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER) 
	{
		if(psIdentifierData->eLValueStatus == GLSLLV_L_VALUE)
		{
			if(GLSL_IS_INT_SCALAR(psIdentifierData->sFullySpecifiedType.eTypeSpecifier) && 	
				psIdentifierData->eArrayStatus == GLSLAS_NOT_ARRAY)
			{
				*bSingleInteger = IMG_TRUE;
			}
			else
			{
				*bSingleInteger = IMG_FALSE;
			}
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}
/******************************************************************************
 * Function Name: IsSymbolBoolConstant
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return TRUE and the bool value if ID is an bool constant
 *****************************************************************************/
static IMG_BOOL IsSymbolBoolConstant(GLSLCompilerPrivateData *psCPD, SymTable *psSymbolTable, IMG_UINT32 uSymbolID, IMG_BOOL *pbData)
{
	GLSLGenericData *psGenericData;

	GLSLIdentifierData *psIdentifierData;

	psGenericData = GetSymbolTableData(psSymbolTable, uSymbolID);

	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("IsSymbolBoolConstant: Failed to get symbol data for ID:%u\n", uSymbolID));
		return IMG_FALSE;;
	}

	psIdentifierData = (GLSLIdentifierData *)psGenericData;

	if(psIdentifierData->eSymbolTableDataType == GLSLSTDT_IDENTIFIER) 
	{
		if ((psIdentifierData->sFullySpecifiedType.eTypeSpecifier == GLSLTS_BOOL) &&
			(psIdentifierData->sFullySpecifiedType.eTypeQualifier == GLSLTQ_CONST))
		{
			*pbData =  *((IMG_BOOL *)psIdentifierData->pvConstantData);
			
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}



/******************************************************************************
 * Function Name: ValidateICInstruction
 *
 * Inputs       : psICProgram, psICInstr
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : validate an instruction and data type checking
 *****************************************************************************/
static IMG_VOID ValidateICInstruction(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLICInstruction *psICInstr)
{
	IMG_UINT32	i;
	IMG_UINT32	uNumSrcs;

	if(psICInstr->eOpCode >= GLSLIC_OP_NUM)
	{
		LOG_INTERNAL_ERROR(("ValidateICInstruction: OPCODE = %u is invalid\n", psICInstr->eOpCode));
	
		ICRemoveInstruction(psICProgram, psICInstr);
		
		return;
	}

	uNumSrcs = ICOP_NUM_SRCS(psICInstr->eOpCode);

	for(i = 0; i < uNumSrcs + 1; i++)
	{
		if(!ICOP_HAS_DEST(psICInstr->eOpCode) && i == DEST) continue;

		if(psICInstr->asOperand[i].uSymbolID == 0)
		{
			LOG_INTERNAL_ERROR(("ValidateICInstruction: ERROR: ZERO ID in one of operands:\n"));
#if defined(DEBUG) || defined(DUMP_LOGFILES)
			{
				IMG_CHAR	str[256];
				IMG_INT32		k = 0;
				IMG_UINT32	j;
				k = sprintf(str, "%-8s ", ICOP_DESC(psICInstr->eOpCode));
				for(j = 0; j < uNumSrcs + 1; j++)
				{
					if(!ICOP_HAS_DEST(psICInstr->eOpCode) && j == DEST) continue;

					k = DumpICOperand(str, k,  psICProgram->psSymbolTable, &psICInstr->asOperand[j]);
				}
				LOG_INTERNAL_ERROR(("%s\n", str));
			}
#endif

			return;
		}
	}

	/*
	** Check whether operand data types, the numbers of components etc. are
	** compatible for a specific opcode. Since semantic checking has been done,
	** the code below is only included in DEBUG build. 
	*/ 
#ifdef DEBUG
	{
		GLSLFullySpecifiedType asFullType[MAX_OPRDS];
		GLSLTypeSpecifier	aeOperandType[MAX_OPRDS];
		GLSLTypeSpecifier	aeComponentType[MAX_OPRDS];
		IMG_UINT32			auNumComponents[MAX_OPRDS]; /* Num of components */
		IMG_UINT32			auNumMatRows[MAX_OPRDS], auNumMatCols[MAX_OPRDS];
		IMG_BOOL			abMatrix[MAX_OPRDS];
		IMG_BOOL			bPass = IMG_TRUE;
		IMG_UINT32			j;
		IMG_INT32			aiArraySize[MAX_OPRDS];

		if(psICInstr->eOpCode != GLSLIC_OP_CALL && psICInstr->eOpCode != GLSLIC_OP_LABEL) 
		{
			for(i = 0; i < uNumSrcs + 1; i++)
			{
				GLSLFullySpecifiedType *psFullType;

				if(!ICOP_HAS_DEST(psICInstr->eOpCode) && i == DEST) continue;

				if(	!ICGetSymbolInformation(psCPD, psICProgram->psSymbolTable,	/* psSymbolTable */
											psICInstr->asOperand[i].uSymbolID,	/* uSymbolTableID */
											IMG_NULL,							/* peBuiltinID */
										    &psFullType,						/* psFullySpecifiedType */
										    &aiArraySize[i],					/* piArraySize */
											IMG_NULL,							/* pbDyanmiccalllyIndexed */
											IMG_NULL)							/* ppvConstantData */
				  )
				{
					LOG_INTERNAL_ERROR(("ValidateICInstruction: Failed to get symbol info \n"));
					bPass = IMG_FALSE;
					break;
				}

				asFullType[i] = *psFullType;

				aeOperandType[i] = asFullType[i].eTypeSpecifier;

				if(aeOperandType[i] == GLSLTS_INVALID || aeOperandType[i] == GLSLTS_VOID)
				{
					bPass = IMG_FALSE;
					break;
				}

				aiArraySize[i] = asFullType[i].iArraySize;

				/* Find out data type for all operands after offsets */
				if(psICInstr->asOperand[i].uNumOffsets)
				{
					GLSLFullySpecifiedType sIndexedType = asFullType[i];

					for(j = 0; j < psICInstr->asOperand[i].uNumOffsets; j++)
					{
						if(aiArraySize[i])
						{
							aiArraySize[i] = 0;
						}
						else
						{
							sIndexedType = GetIndexedType(psCPD, psICProgram->psSymbolTable, 
														  &sIndexedType, 
														  psICInstr->asOperand[i].psOffsets[j].uStaticOffset,
														  &aiArraySize[i]);
						}
					}

					asFullType[i] = sIndexedType;
				}

				if(!bPass) break;
			
				aeOperandType[i] = asFullType[i].eTypeSpecifier;

				/* Find component type for all operands */
				if(aeOperandType[i]  != GLSLTS_STRUCT)
				{
					aeComponentType[i] = TYPESPECIFIER_BASE_TYPE(aeOperandType[i]);
				}

				/* The number of components for all operands */
				auNumComponents[i] = TYPESPECIFIER_NUM_COMPONENTS(aeOperandType[i]);

				if(GLSL_IS_VECTOR(aeOperandType[i]) && !aiArraySize[i])
				{
					/* If it is a vector, work out the type after swiz */
					if(psICInstr->asOperand[i].sSwizWMask.uNumComponents)
					{
						IMG_UINT32 uComponents = psICInstr->asOperand[i].sSwizWMask.uNumComponents;

						auNumComponents[i] = uComponents;
						aeOperandType[i] = CONVERT_TO_VECTYPE (aeOperandType[i], uComponents);
					}
				}

				/* Is it a matrix ? */
				if (GLSL_IS_MATRIX(aeOperandType[i]))
				{
					abMatrix[i] = IMG_TRUE;
				}
				else
				{
					abMatrix[i] = IMG_FALSE;
				}

				auNumMatRows[i] = TYPESPECIFIER_NUM_ROWS(aeOperandType[i]);
				auNumMatCols[i] = TYPESPECIFIER_NUM_COLS(aeOperandType[i]);

				if((aiArraySize[i] || aeOperandType[i] == GLSLTS_STRUCT))
				{
					if(psICInstr->eOpCode != GLSLIC_OP_MOV &&
					  psICInstr->eOpCode != GLSLIC_OP_SEQ &&
					  psICInstr->eOpCode != GLSLIC_OP_SNE)
					{
						/* arrays, structure are be operands only for GLSLIC_OP_MOV, SEQ and SNE */
						bPass = IMG_FALSE;
						break;
					}
				}
			}
		}

		if(bPass)
		{
			bPass = IMG_FALSE;

			switch(psICInstr->eOpCode)
			{
			case GLSLIC_OP_MOV:
				if( aiArraySize[SRCA] || aiArraySize[DEST] || 
					aeOperandType[SRCA] == GLSLTS_STRUCT ||
					aeOperandType[DEST] == GLSLTS_STRUCT)
				{
					/* Both operands are arrays, the type should be the same. */
					if (aiArraySize[DEST] == aiArraySize[SRCA] &&
						aeOperandType[DEST] == aeOperandType[SRCA])
					{
						bPass = IMG_TRUE;
						break;
					}
				}
				else
				{
					/* Rule 1: source is scaler, destination can be any type (scaler, vec, matrix )*/
					if(auNumComponents[SRCA] == 1)
					{
						bPass = IMG_TRUE;
						break;
					}
					/* 
					** Rule 2: a matrix can be constructed from any dimension of a matrix 
					*/
					else if(abMatrix[SRCA] && abMatrix[DEST])
					{
						bPass = IMG_TRUE;
						break;
					}
					/* Rule 3: vector operation, must have the same number of components */ 
					else if( auNumComponents[SRCA] == auNumComponents[DEST] && !abMatrix[SRCA] && !abMatrix[DEST])
					{
						bPass = IMG_TRUE;
						break;
					}
				}

				break;
			case GLSLIC_OP_ADD:	
			case GLSLIC_OP_SUB:	
			case GLSLIC_OP_MUL:
			case GLSLIC_OP_DIV:
				/* 
				** Rule 1: SRCA is a scaler
				*/
				if(auNumComponents[SRCA] == 1)
				{
					if(	aeOperandType[DEST] == aeOperandType[SRCB] &&
						aeComponentType[SRCB] == aeComponentType[SRCA] )
					{
						bPass = IMG_TRUE;
						break;
					}
				}

				/*
				** Rule 2: SRCB is a scaler 
				*/
				if(auNumComponents[SRCB] == 1)
				{
					if(aeOperandType[DEST] == aeOperandType[SRCA] &&
						aeComponentType[SRCA] == aeComponentType[SRCB])
					{
						bPass = IMG_TRUE;
					}
				}

				/*
				** Rule 3 for GLSLIC_OP_MUL: matrix multiplication
				*/
				if(psICInstr->eOpCode == GLSLIC_OP_MUL && (abMatrix[SRCA] || abMatrix[SRCB]))
				{
					/* SrcA is mat and SrcB is vecotr */
					if( abMatrix[SRCA] && GLSL_IS_VECTOR(aeOperandType[SRCB]) )
					{
						if(auNumMatCols[SRCA] == auNumComponents[SRCB] &&
						   GLSL_IS_VECTOR(aeOperandType[DEST]) &&
						   auNumComponents[DEST] == auNumMatRows[SRCA])
						{
							bPass = IMG_TRUE;
							break;
						}
					}
					/* SrcA is vec and SrcB is mat */
					else if(abMatrix[SRCB] && GLSL_IS_VECTOR(aeOperandType[SRCA]))
					{
						if(auNumMatRows[SRCB] == auNumComponents[SRCA] &&
						   GLSL_IS_VECTOR(aeOperandType[DEST]) &&
						   auNumComponents[DEST] == auNumMatCols[SRCB])
						{
							bPass = IMG_TRUE;
							break;
						}
					}
					else if(abMatrix[SRCA] && abMatrix[SRCB])
					{
						/*
							m[c2][r1] = m1[c1][r1] * m2[c2][r2];  c1 == r2
						*/
						if(abMatrix[DEST] &&
						   auNumMatCols[SRCA] == auNumMatRows[SRCB] &&
						   auNumMatCols[DEST] == auNumMatCols[SRCB] &&
						   auNumMatRows[DEST] == auNumMatRows[SRCA])
						{
							bPass = IMG_TRUE;
							break;
						}
					}
				}
				else
				{
					/* Rule 4: */
					if (aeOperandType[DEST] == aeOperandType[SRCB] &&
						aeOperandType[DEST] == aeOperandType[SRCA]
						)
					{

						bPass = IMG_TRUE;
						break;

					}
				}
					
				break;
			
			case GLSLIC_OP_SEQ:	
			case GLSLIC_OP_SNE:
			case GLSLIC_OP_SLT:	
			case GLSLIC_OP_SLE:	
			case GLSLIC_OP_SGT:	
			case GLSLIC_OP_SGE:	
				if((psICInstr->eOpCode == GLSLIC_OP_SEQ || psICInstr->eOpCode == GLSLIC_OP_SNE ) && 
					aeOperandType[DEST] == GLSLTS_BOOL)
				{
					if(aeOperandType[SRCB] == aeOperandType[SRCA]) 
					{
						bPass = IMG_TRUE;
					}
				}
				else if(GLSL_IS_BOOL(aeComponentType[DEST])
					|| GLSL_IS_FLOAT(aeComponentType[DEST])) /* step() does a GLSLIC_OP_SGE and returns floats */
				{
					if(GLSL_IS_VECTOR(aeOperandType[DEST]) && 
						(GLSL_IS_VECTOR(aeOperandType[SRCA]) || GLSL_IS_VECTOR(aeOperandType[SRCB]) ))
					{
						if(auNumComponents[SRCA] == 1 &&
							auNumComponents[DEST] == auNumComponents[SRCB] )
						{
							bPass = IMG_TRUE;
						}
						else if(auNumComponents[SRCB] == 1 &&
								auNumComponents[DEST] == auNumComponents[SRCA] )
						{
							bPass = IMG_TRUE;
						}
						else if(aeOperandType[SRCB] == aeOperandType[SRCA] && 
								auNumComponents[DEST] == auNumComponents[SRCA])
						{
							bPass = IMG_TRUE;
						}
					}
					else if(GLSL_IS_SCALAR(aeOperandType[DEST]) && 
							aeOperandType[SRCB] == aeOperandType[SRCA] &&
							!abMatrix[SRCA])
					{
						bPass = IMG_TRUE;
					}
				}
				break;
			case GLSLIC_OP_NOT:
				if( GLSL_IS_BOOL(aeOperandType[SRCA]) &&
					aeOperandType[SRCA] == aeOperandType[DEST])
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_IF:
			case GLSLIC_OP_IFNOT:
				if(aeOperandType[SRCA] == GLSLTS_BOOL)
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_IFGT:
			case GLSLIC_OP_IFGE:
			case GLSLIC_OP_IFLT:
			case GLSLIC_OP_IFLE:
			case GLSLIC_OP_IFEQ:
			case GLSLIC_OP_IFNE:
				if(GLSL_IS_SCALAR(aeOperandType[SRCA]) && GLSL_IS_SCALAR(aeOperandType[SRCB]))
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_ELSE:
			case GLSLIC_OP_ENDIF:
			case GLSLIC_OP_LABEL:
			case GLSLIC_OP_RET:
			case GLSLIC_OP_CALL:
			case GLSLIC_OP_LOOP:
			case GLSLIC_OP_STATICLOOP:
			case GLSLIC_OP_ENDLOOP:
			case GLSLIC_OP_CONTINUE:
			case GLSLIC_OP_CONTDEST:
			case GLSLIC_OP_BREAK:
			case GLSLIC_OP_DISCARD:
				bPass = IMG_TRUE;
				break;


			case GLSLIC_OP_SANY:
			case GLSLIC_OP_SALL:
				if( aeOperandType[DEST] == GLSLTS_BOOL &&
					aeComponentType[SRCA] == GLSLTS_BOOL &&
					auNumComponents[SRCA] > 1 )
				{
					bPass = IMG_TRUE;
				}
				break;

			case GLSLIC_OP_MIN:	
			case GLSLIC_OP_MAX:	
				/* 
				** Rule 1: SRCA is a scaler
				*/
				if(auNumComponents[SRCA] == 1)
				{
					if(	aeOperandType[DEST] == aeOperandType[SRCB] &&
						aeComponentType[SRCB] == aeComponentType[SRCA] &&
						!abMatrix[SRCB] )
					{
						bPass = IMG_TRUE;
					}
				}

				/*
				** Rule 2: SRCB is a scaler
				*/
				if(!bPass)
				{
					if(auNumComponents[SRCB] == 1)
					{
						if(aeOperandType[DEST] == aeOperandType[SRCA] &&
							aeComponentType[SRCA] == aeComponentType[SRCB] &&
							!abMatrix[SRCA] )
						{
							bPass = IMG_TRUE;
						}
					}
				}

				/* 
				** Rule 3: Neither of SRCA and SRCB are scaler, 
				*/
				if(!bPass)
				{
					if (aeOperandType[SRCB] == aeOperandType[SRCA] &&
						aeOperandType[DEST] == aeOperandType[SRCA] &&
						!abMatrix[SRCA]
						)
					{
						bPass = IMG_TRUE;
					}
				}

				break;
			case GLSLIC_OP_RCP:
			case GLSLIC_OP_RSQ:
			case GLSLIC_OP_LOG:
			case GLSLIC_OP_LOG2:
			case GLSLIC_OP_EXP:
			case GLSLIC_OP_EXP2:
			case GLSLIC_OP_SIN:
			case GLSLIC_OP_COS:
			case GLSLIC_OP_FLOOR:
			case GLSLIC_OP_CEIL:
			case GLSLIC_OP_DFDX:
			case GLSLIC_OP_DFDY:
				/* Rule: source and dest are the same type and one of genType */
				if( aeOperandType[DEST] == aeOperandType[SRCA] &&
					GLSL_IS_FLOAT(aeOperandType[SRCA]))					
				{
					bPass = IMG_TRUE;
				}
				break;

			case GLSLIC_OP_ABS:
			case GLSLIC_OP_SGN:
				/* Rule: source and dest are the same type and one of genType */
				if( aeOperandType[DEST] == aeOperandType[SRCA] &&
					(GLSL_IS_FLOAT(aeOperandType[SRCA])
					))
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_DOT:
				/* Rule: sources are the same type and one of genType, dest is a float */
				if( aeOperandType[SRCB] == aeOperandType[SRCA] &&
					GLSL_IS_FLOAT(aeOperandType[SRCA]) &&
					aeOperandType[DEST] == GLSLTS_FLOAT)
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_CROSS:	
				/* Rule: sources and dest are vec3 */
				if( aeOperandType[SRCB] == aeOperandType[SRCA] &&
					aeOperandType[DEST] == aeOperandType[SRCA] &&
					aeOperandType[SRCA] == GLSLTS_VEC3)
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_NRM3:
				/* Rule source and dest are vec3. */
				if( aeOperandType[DEST] == aeOperandType[SRCA] &&
					aeOperandType[SRCA] == GLSLTS_VEC3)
				{
					bPass = IMG_TRUE;
				}
			case GLSLIC_OP_TEXLD:
			case GLSLIC_OP_TEXLDB:
			case GLSLIC_OP_TEXLDL:
			case GLSLIC_OP_TEXLDD:
				if( (aeOperandType[DEST] == GLSLTS_VEC4
					) && GLSLTypeSpecifierDimensionTable(aeOperandType[SRCA]) == auNumComponents[SRCB])
				{
					if(psICInstr->eOpCode == GLSLIC_OP_TEXLDD)
					{
						if(/* aeOperandType[SRCB] == aeOperandType[SRCC] && Loosen restriction for GLSL 1.30 */ 
							aeOperandType[SRCC] == aeOperandType[SRCD])
						{
							bPass = IMG_TRUE;
						}
					}
					else if(psICInstr->eOpCode == GLSLIC_OP_TEXLD)
					{
						bPass = IMG_TRUE;
					}
					else if(psICInstr->eOpCode == GLSLIC_OP_TEXLDL)
					{
						bPass = IMG_TRUE;
					}
					else if(aeOperandType[SRCC] == GLSLTS_FLOAT)
					{
						bPass = IMG_TRUE;
					}	
				}
				break;
			case GLSLIC_OP_TEXLDP:
				if(aeOperandType[DEST] == GLSLTS_VEC4)
				{
					switch(aeOperandType[SRCA])
					{
					case GLSLTS_SAMPLER1D:
						if(aeOperandType[SRCB] >= GLSLTS_VEC2 && aeOperandType[SRCB] <= GLSLTS_VEC4)
						{
							bPass = IMG_TRUE;
						}
						break;
					case GLSLTS_SAMPLER2D:
						if(aeOperandType[SRCB] >= GLSLTS_VEC3  && aeOperandType[SRCB] <= GLSLTS_VEC4)
						{
							bPass = IMG_TRUE;
						}
						break;

					case GLSLTS_SAMPLER3D:
						if(aeOperandType[SRCB] == GLSLTS_VEC4)
						{
							bPass = IMG_TRUE;
						}
						break;
					case GLSLTS_SAMPLER1DSHADOW:
					case GLSLTS_SAMPLER2DSHADOW:
						if(	aeOperandType[SRCB] == GLSLTS_VEC4)
						{
							bPass = IMG_TRUE;
						}	
						break;
					default:
						LOG_INTERNAL_ERROR(("ValidateICInstruction: SrcA has to be a sampler for texture look up ops !\n"));
						break;
					}
				}
				break;

			/* Extensive op */
			case GLSLIC_OP_POW:
				/* Rule: both sources and dest are the same type, one of genTypes */
				if( aeOperandType[SRCB] == aeOperandType[SRCA] &&
					aeOperandType[DEST] == aeOperandType[SRCA] &&
					GLSL_IS_FLOAT(aeOperandType[SRCA]) )
				{
					bPass = IMG_TRUE;
				}
				break;
			case GLSLIC_OP_FRC:
			case GLSLIC_OP_MOD:
			case GLSLIC_OP_TAN:
			case GLSLIC_OP_ASIN:
			case GLSLIC_OP_ACOS:
			case GLSLIC_OP_ATAN:


				/* Rule: source and dest are the same type and one of genTypes */
				if( aeOperandType[DEST] == aeOperandType[SRCA] &&
					GLSL_IS_FLOAT(aeOperandType[SRCA]) )
				{
					bPass = IMG_TRUE;
				}
				break;

			case GLSLIC_OP_MAD:
			case GLSLIC_OP_MIX:
				/*
				** Rule 1: SRCA is not a scalar
				*/
				if(auNumComponents[SRCA] > 1)
				{
					if(	aeOperandType[DEST] == aeOperandType[SRCA] &&
						(aeOperandType[SRCB] == aeOperandType[SRCA] || auNumComponents[SRCB] == 1) &&
						(aeOperandType[SRCC] == aeOperandType[SRCA] || auNumComponents[SRCC] == 1
						) )
					{
						bPass = IMG_TRUE;
					}
				}

				/*
				** Rule 2: SRCB is not a scalar 
				*/
				if(!bPass && auNumComponents[SRCB] > 1)
				{
					if(	aeOperandType[DEST] == aeOperandType[SRCB] &&
						(aeOperandType[SRCA] == aeOperandType[SRCB] || auNumComponents[SRCA] == 1) &&
						(aeOperandType[SRCC] == aeOperandType[SRCB] || auNumComponents[SRCC] == 1) )
					{
						bPass = IMG_TRUE;
					}
				}

				/* Rule 3: SRCC is not a scalar */
				if(!bPass && auNumComponents[SRCC] > 1)
				{
					if(	aeOperandType[DEST] == aeOperandType[SRCC] )
					{
						if( (auNumComponents[SRCA] > 1 && aeOperandType[SRCA] == aeOperandType[SRCC]) || 
							(auNumComponents[SRCB] > 1 && aeOperandType[SRCB] == aeOperandType[SRCC]) ||
							(auNumComponents[SRCA] == 1 && auNumComponents[SRCB] == 1) )
						{
							bPass = IMG_TRUE;
						}
					}
				}

				if(!bPass &&
					auNumComponents[SRCA] == 1 &&
					auNumComponents[SRCB] == 1 &&
					auNumComponents[SRCC] == 1 &&
					auNumComponents[DEST] == 1 )
				{
					bPass = IMG_TRUE;
				}

				break;

			default:
				LOG_INTERNAL_ERROR(("ValidateICInstruction: unknown opcode = %u\n", psICInstr->eOpCode));
				break;

			}
		}

		if(!bPass)
		{
			IMG_CHAR	str[256];
			IMG_INT32		i;
			IMG_UINT32	j;
			LOG_INTERNAL_ERROR(("ValidateICInstruction: Data types are not compatible:\n"));
			i = sprintf(str, "%-8s ", ICOP_DESC(psICInstr->eOpCode));
			for(j = 0; j < uNumSrcs + 1; j++)
			{
				if(!ICOP_HAS_DEST(psICInstr->eOpCode) && j == 0) continue;

				i = DumpICOperand(str, i,  psICProgram->psSymbolTable, &psICInstr->asOperand[j]);
			}

			LOG_INTERNAL_ERROR(("%s\n", str));
		}
	}
	
#endif /*#ifdef DEBUG */

}



/******************************************************************************
 * Function Name: CloneICodeInstructions
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Clone IC instructions from uStart to uStart + uCount
 *****************************************************************************/
IMG_INTERNAL IMG_VOID CloneICodeInstructions(GLSLCompilerPrivateData *psCPD,
											 GLSLICProgram		*psICProgram,
												GLSLICInstruction	*psStart,
												GLSLICInstruction	*psEnd,
												GLSLICInstruction	**ppsClonedStart,
												GLSLICInstruction	**ppsClonedEnd)
{
	GLSLICInstruction *psOrigInstr;
	GLSLICInstruction *psInstr;
	IMG_UINT32 j;

	*ppsClonedStart = IMG_NULL;
	*ppsClonedEnd = IMG_NULL;

	for(psOrigInstr = psStart; ; psOrigInstr = psOrigInstr->psNext)
	{
		psInstr = ICGetNewInstruction(psCPD, psICProgram);

		/* General infor */
		psInstr->pszOriginalLine		= psOrigInstr->pszOriginalLine;
		psInstr->eOpCode				= psOrigInstr->eOpCode;
		psInstr->bPredicateNegate		= psOrigInstr->bPredicateNegate;
		psInstr->uPredicateBoolSymID	= psOrigInstr->uPredicateBoolSymID;

		/* Operands */
		for(j = 0; j < ICOP_NUM_SRCS(psOrigInstr->eOpCode) + 1; j++)
		{
			if(!ICOP_HAS_DEST(psOrigInstr->eOpCode) && j==0) continue;

			psInstr->asOperand[j] = psOrigInstr->asOperand[j];

			if(psOrigInstr->asOperand[j].uNumOffsets)
			{
				psInstr->asOperand[j].psOffsets = DebugMemAlloc(psOrigInstr->asOperand[j].uNumOffsets * sizeof(GLSLICOperandOffset));
				if(psInstr->asOperand[j].psOffsets == IMG_NULL)
				{
					LOG_INTERNAL_ERROR(("CloneICodeInstructions: Failed to get memory\n"));
					return ;
				}
				memcpy(psInstr->asOperand[j].psOffsets, psOrigInstr->asOperand[j].psOffsets, (psOrigInstr->asOperand[j].uNumOffsets * sizeof(GLSLICOperandOffset)));
			}
			else
			{
				DebugAssert(psInstr->asOperand[j].psOffsets == IMG_NULL);
			}
		}

		if(psOrigInstr == psStart) *ppsClonedStart = psInstr;

		if(psOrigInstr == psEnd) 
		{
			*ppsClonedEnd = psInstr;

			break;
		}
	}
}

/******************************************************************************
 * Function Name: ICAddICInstruction0
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with no operand
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction0(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
										GLSLICOpcode		eICOpcode,
										IMG_CHAR			*pszLineStart)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction0: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction1
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with one operand
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction1(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											GLSLICOperandInfo	*psOperandSRC)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction1: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}
	
	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRC */
	ICSetupICOperand(psCPD, psOperandSRC, &psICInstr->asOperand[SRCA]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstruction1a
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with one operand, an alternative to ICAddICInstruction1.
 *				: Default setting applies to the operand
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction1a(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32		uSymbolID)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction1a: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRC */
	ICInitICOperand(uSymbolID, &psICInstr->asOperand[SRCA]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction2
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with two operands
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction2(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											GLSLICOperandInfo	*psOperandDEST,
											GLSLICOperandInfo	*psOperandSRCA)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction2: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* DEST */
	ICSetupICOperand(psCPD, psOperandDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstruction2a
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with two operands, an alternative to ICAddICInstruction2,
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction2a(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32			uSymbolIDDEST,
											GLSLICOperandInfo	*psOperandSRCA)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction2a: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstruction2b
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with two operands, an alternative to ICAddICInstruction2,
 *				: For both operands, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction2b(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32		uSymbolIDDEST,
											IMG_UINT32		uSymboIDSRCA)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction2a: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICInitICOperand(uSymboIDSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction2c
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with two operands, an alternative to ICAddICInstruction2,
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction2c(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											GLSLICOperandInfo *psOperandDEST,
											IMG_UINT32			uSymbolIDSRCA)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction2c: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}
	
	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICInitICOperand(uSymbolIDSRCA, &psICInstr->asOperand[SRCA]);

	/* DEST */
	ICSetupICOperand(psCPD, psOperandDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction3
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											GLSLICOperandInfo	*psOperandDEST,
											GLSLICOperandInfo	*psOperandSRCA,
											GLSLICOperandInfo	*psOperandSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}
	
	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICSetupICOperand(psCPD, psOperandSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICSetupICOperand(psCPD, psOperandDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction3a
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: an alternative to ICAddICInstruction3,
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3a(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32			uSymbolIDDEST,
											GLSLICOperandInfo	*psOperandSRCA,
											GLSLICOperandInfo	*psOperandSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3a: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICSetupICOperand(psCPD, psOperandSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstruction3b
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: an alternative to ICAddICInstruction3,
 *				: For SRCB and DEST operands, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3b(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32			uSymbolIDDEST,
											GLSLICOperandInfo	*psOperandSRCA,
											IMG_UINT32			uSymbolIDSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3b: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICInitICOperand(uSymbolIDSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction3c
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: an alternative to ICAddICInstruction3,
 *				: For SRCB and DEST operands, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3c(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32			uSymbolIDDEST,
											IMG_UINT32			uSymbolIDSRCA,
											GLSLICOperandInfo	*psOperandSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3c: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICInitICOperand(uSymbolIDSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICSetupICOperand(psCPD, psOperandSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction3d
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: an alternative to ICAddICInstruction3,
 *				: For SRCB and DEST operands, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3d(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32		uSymbolIDDEST,
											IMG_UINT32		uSymbolIDSRCA,
											IMG_UINT32		uSymbolIDSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3d: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICInitICOperand(uSymbolIDSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICInitICOperand(uSymbolIDSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstruction3e
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: an alternative to ICAddICInstruction3,
 *				: For SRCB and DEST operands, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction3e(GLSLCompilerPrivateData *psCPD,
										   GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_CHAR			*pszLineStart,
											GLSLICOperandInfo *psDestOperand,
											IMG_UINT32		uSymbolIDSRCA,
											IMG_UINT32		uSymbolIDSRCB)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3e: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICInitICOperand(uSymbolIDSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICInitICOperand(uSymbolIDSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICSetupICOperand(psCPD, psDestOperand, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction4
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction4(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
										GLSLICOpcode		 eICOpcode,
										IMG_CHAR			*pszLineStart,
										GLSLICOperandInfo	*psOperandDEST,
										GLSLICOperandInfo	*psOperandSRCA,
										GLSLICOperandInfo	*psOperandSRCB,
										GLSLICOperandInfo  *psOperandSRCC)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction4: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICSetupICOperand(psCPD, psOperandSRCB, &psICInstr->asOperand[SRCB]);

	/* SRCC */
	ICSetupICOperand(psCPD, psOperandSRCC, &psICInstr->asOperand[SRCC]);

	/* DEST */
	ICSetupICOperand(psCPD, psOperandDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstruction
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstruction(GLSLCompilerPrivateData *psCPD,
										 GLSLICProgram		*psICProgram,
										GLSLICOpcode		eICOpcode,
										IMG_UINT32			uNumSources,
										IMG_CHAR			*pszLineStart,
										GLSLICOperandInfo	*psDestOperand,
										GLSLICOperandInfo	*psOperands)
{
	GLSLICInstruction *psICInstr;
	IMG_UINT32 i;

	/* Make sure the number of operands match */
	DebugAssert(ICOP_NUM_SRCS(eICOpcode) == uNumSources);

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}
	
	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	for(i = 0; i < uNumSources; i++)
	{
		ICSetupICOperand(psCPD, &psOperands[i], &psICInstr->asOperand[SRCA + i]);
	}

	/* DEST */
	if(ICOP_HAS_DEST(eICOpcode))
	{
		ICSetupICOperand(psCPD, psDestOperand, &psICInstr->asOperand[DEST]);
	}

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddICInstructiona
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add an IC instruction with three operands, 
 *				: For destination operand, default setting applies.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddICInstructiona(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram		*psICProgram,
											GLSLICOpcode		eICOpcode,
											IMG_UINT32			uNumSources,
											IMG_CHAR			*pszLineStart,
											IMG_UINT32			uSymbolIDDEST,
											GLSLICOperandInfo	*psOperands)
{
	GLSLICInstruction *psICInstr;
	IMG_UINT32 i;

	/* Make sure the number of operands match */
	DebugAssert(ICOP_NUM_SRCS(eICOpcode) == uNumSources);

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstructiona: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	for(i = 0; i < uNumSources; i++)
	{
		ICSetupICOperand(psCPD, &psOperands[i], &psICInstr->asOperand[SRCA + i]);
	}

	/* DEST */
	if(ICOP_HAS_DEST(eICOpcode))
	{
		ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);
	}

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}



/******************************************************************************
 * Function Name: ProcessNodeBasicOp
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_ADD
 *								GLSLNT_SUBTRACT
 *								GLSLNT_MULTIPLY
 *								GLSLNT_DIVIDE
 *								GLSLNT_MODULUS
 *****************************************************************************/
static IMG_VOID ProcessNodeBasicOp(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcess)
{
	GLSLNode				*psLeft, *psRight;
	GLSLICOperandInfo		sLeftOperand, sRightOperand, sDestOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	
	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* 
	** If the parent is assignment and at the stage of front end node process, 
	** we want to delay the code generation until the assignement node is visited (GLSLNT_EQUAL) 
	*/
	if(!bPostProcess && IS_PARENT_ASSIGNMENT(psNode))
	{
		return;
	}

	/* DEST Operand */
	if(bPostProcess)
	{
		/* At the post process stage, take the left node of assignment as the destination operand */

		GLSLNode *psDest;

		DebugAssert(IS_PARENT_ASSIGNMENT(psNode));
		DebugAssert(psNode->bEvaluated == IMG_FALSE);
		
		psDest = GET_PARENT_ASSIGNMENT_LEFT(psNode);

		ICProcessNodeOperand(psCPD, psICProgram, psDest, &sDestOperand);
	}
	else
	{
		/* Not at post stage, take the current node as destination operand */

		ICInitOperandInfo(psNode->uSymbolTableID, &sDestOperand);

		/* Indicate the Symbol ID of this node has been assigned and ready to use (as source when neccessary) */
		psNode->bEvaluated = IMG_TRUE;
	}

	/* SRCA and SRCB */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];
	ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

	/* Add IC instruction */
	ICAddICInstruction3(psCPD, psICProgram, 
						GetICOpCode(psNode->eNodeType), 
						pszLineStart, 
						&sDestOperand, 
						&sLeftOperand, 
						&sRightOperand);

	/* Free offsets attached to the left and right operands */
	ICFreeOperandOffsetList(&sLeftOperand);
	ICFreeOperandOffsetList(&sRightOperand);
	ICFreeOperandOffsetList(&sDestOperand);
}

#if 0
/******************************************************************************
 * Function Name: IsRValueNode
 *
 * Inputs       : psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return IMG_TRUE if it is an r-value node which is on the right  
 *				  hand side of an assignment
 *****************************************************************************/
static IMG_BOOL IsRValueNode(GLSLNode *psNode)
{
	GLSLNode *parent = psNode->psParent;

	if(!parent) return IMG_FALSE;

	if( parent->eNodeType == GLSLNT_STATEMENT_LIST ||
		parent->eNodeType == GLSLNT_SHADER ||
		parent->eNodeType == GLSLNT_FUNCTION_DEFINITION
	  )
	{
		return IMG_FALSE;
	}
	else if(parent->eNodeType == GLSLNT_EXPRESSION ||
			parent->eNodeType == GLSLNT_SUBEXPRESSION ||
			parent->eNodeType == GLSLNT_POSITIVE ||
			parent->eNodeType == GLSLNT_DECLARATION )
	{
		return IsRValueNode(parent);
	}

	return IMG_TRUE;
}

#endif


/******************************************************************************
 * Function Name: ProcessNodeBasicOpAssign
 *
 * Inputs       : psICProgram, psNode
 * Outputs      :  
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_MUL_ASSIGN
 *								GLSLNT_DIV_ASSIGN
 *								GLSLNT_MOD_ASSIGN
 *								GLSLNT_ADD_ASSIGN
 *								GLSLNT_SUB_ASSIGN
 *****************************************************************************/

static IMG_VOID ProcessNodeBasicOpAssign(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psLeft, *psRight;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLICOperandInfo sLeftOperand, sRightOperand;
	
	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get the left and right children */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* Process left and right operands */
	ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

	/* Make sure the left operand is not negate */
	DebugAssert(!(sLeftOperand.eInstModifier & GLSLIC_MODIFIER_NEGATE));

	/* Add IC instruction */
	ICAddICInstruction3(psCPD, psICProgram,
					GetICOpCode(psNode->eNodeType),
					pszLineStart,
					&sLeftOperand,
					&sLeftOperand,
					&sRightOperand);

	/* Free offsets attached to the left and right operands */
	ICFreeOperandOffsetList(&sLeftOperand);
	ICFreeOperandOffsetList(&sRightOperand);
}

/******************************************************************************
 * Function Name: ProcessNodeEQUAL
 *
 * Inputs       : psNode
 * Outputs      : psICProgram
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_EQUAL
 *****************************************************************************/

static IMG_VOID ProcessNodeEQUAL(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode				*psLeft, *psRight;
	GLSLICOperandInfo		sLeftOperand;
	GLSLICOperandInfo		sRightOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get left and right children */
	psLeft  = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* Process left operands */
	ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);

	/* Make sure the left operand does not contain negate */
	DebugAssert(!(sLeftOperand.eInstModifier & GLSLIC_MODIFIER_NEGATE));

	/*
	** Can we directly evaluate the node by post traversing the node? 
	** If not, we need to post process the right node and store the result to left node
	*/
	if(PostEvaluateNode(psCPD, psICProgram, psRight, &sRightOperand))
	{
		/* Directly move the right to the left */
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, &sLeftOperand, &sRightOperand);

		ICFreeOperandOffsetList(&sRightOperand);
	}
	else
	{
		/* Post processing the righ node and assign the result to the left node */
		ProcessNode(psCPD, psICProgram, psRight, IMG_TRUE);
	}

	/* Free offsets attached to the left operands */
	ICFreeOperandOffsetList(&sLeftOperand);
}


/******************************************************************************
 * Function Name: ProcessNodePreIncDec
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_PRE_INC
 *								GLSLNT_PRE_DEC
 *****************************************************************************/
static IMG_VOID ProcessNodePreIncDec(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode				*child = psNode->ppsChildren[0];
	IMG_UINT32				uONESymbolID;
	GLSLICOperandInfo		sChildOperand;
	GLSLICOperandInfo		sONEOperand;
	GLSLTypeSpecifier		eTypeSpecifier;
	IMG_CHAR *pszLineStart	= GET_SHADERCODE_LINE(psNode);
	GLSLFullySpecifiedType	*psFullType;
	GLSLPrecisionQualifier	ePrecision;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child operand */
	ICProcessNodeOperand(psCPD, psICProgram, child, &sChildOperand);

	/* SRCB is constant 1 or 1.0 */
	psFullType		= ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, child->uSymbolTableID);
	eTypeSpecifier	= psFullType->eTypeSpecifier;
	ePrecision		= psFullType->ePrecisionQualifier;

	if( GLSL_IS_INT(eTypeSpecifier))
	{
		if(!AddIntConstant(psCPD, psICProgram->psSymbolTable, 1, ePrecision, IMG_TRUE
							,
							&uONESymbolID))
		{
			LOG_INTERNAL_ERROR(("ProcessNodePreIncDec: cannot add int constant 1\n"));
		}
	}
	else
	{
		AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, ePrecision, IMG_TRUE, &uONESymbolID);
	}
	ICInitOperandInfo(uONESymbolID, &sONEOperand);

	ICAddICInstruction3(psCPD, psICProgram,
					GetICOpCode(psNode->eNodeType),
					pszLineStart,
					&sChildOperand,
					&sChildOperand,
					&sONEOperand);

	ICFreeOperandOffsetList(&sChildOperand);
}

/******************************************************************************
 * Function Name: PostProcessNodeIncDec
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_POST_INC
 *								GLSLNT_POST_DEC
 *****************************************************************************/
static IMG_VOID PostProcessNodeIncDec(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode			*child = psNode->ppsChildren[0];
	IMG_UINT32			uONESymbolID;
	GLSLICOperandInfo		sChildOperand;
	GLSLICOperandInfo		sONEOperand;
	GLSLTypeSpecifier	eTypeSpecifier;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLFullySpecifiedType *psFullType;
	GLSLPrecisionQualifier ePrecision;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);
	
	/* Process child operand */
	ICProcessNodeOperand(psCPD, psICProgram, child, &sChildOperand);

	/* SRCB = 1 */
	psFullType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, child->uSymbolTableID);
	eTypeSpecifier = psFullType->eTypeSpecifier;
	ePrecision = psFullType->ePrecisionQualifier;

	if(GLSL_IS_INT(eTypeSpecifier))
	{
		AddIntConstant(psCPD, psICProgram->psSymbolTable, 1, ePrecision, IMG_TRUE
						,
						&uONESymbolID);
	}
	else
	{
		AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, ePrecision, IMG_TRUE, &uONESymbolID);
	}
	ICInitOperandInfo(uONESymbolID, &sONEOperand);

	ICAddICInstruction3(psCPD, psICProgram,
					GetICOpCode(psNode->eNodeType),
					pszLineStart,
					&sChildOperand,
					&sChildOperand,
					&sONEOperand);

	ICFreeOperandOffsetList(&sChildOperand);
}

/******************************************************************************
 * Function Name: ProcessNodePostIncDec
 *
 * Inputs       : psNode
 * Outputs      : psICProgram
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_POST_INC
 *								GLSLNT_POST_DEC
 *****************************************************************************/
static IMG_VOID ProcessNodePostIncDec(GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	PVR_UNREFERENCED_PARAMETER(psNode);
	PVR_UNREFERENCED_PARAMETER(psICProgram);
}

/******************************************************************************
 * Function Name: InsertPostfixCode
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Insert postfix op by visiting its all offspring, postorder.
 *****************************************************************************/
static IMG_VOID InsertPostfixCode(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	IMG_UINT32 i;

	if(!psNode) return;

	if(psNode->eNodeType == GLSLNT_SUBEXPRESSION)
	{
		/* only visit the right most child */
		InsertPostfixCode(psCPD, psICProgram, psNode->ppsChildren[psNode->uNumChildren - 1]);
	}
	else if (psNode->eNodeType == GLSLNT_EXPRESSION)
	{
		return;
	}
	else
	{
		for(i = 0; i < psNode->uNumChildren; i++)
		{
			InsertPostfixCode(psCPD, psICProgram, psNode->ppsChildren[i]);
		}
	}

	if(psNode->eNodeType == GLSLNT_POST_INC || psNode->eNodeType == GLSLNT_POST_DEC)
	{
		PostProcessNodeIncDec(psCPD, psICProgram, psNode);	
	}
}


/******************************************************************************
 * Function Name: InsertPostfix
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Check whether needs to insert post fix and insert fix if it does.
				  This needs to be called when a node has been processed(visited).
 *****************************************************************************/
static IMG_VOID InsertPostfix(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	/*
		(Email from James on 7 July 2004 regarding how to construct AST for postfix)

		Rules for when to insert postfix instructions;

		1. When you return to an expression node.
		2. When you return to a sub expression node IF that sub expression node has further children to process.

		Further examples 


		a = b++ + b;          :  a = 2, b = 2

		a = (b++) + b;        :  a = 2, b = 2

		a = (b++,b++) + b;    :  a = 4, b = 3

		a = (b++ + b);        :  a = 2, b = 2

		a = (b++, b);         :  a = 2, b = 2

		a = (b++ + b) + b;    :  a = 3, b = 2

		a = b++, a = b;       :  a = 2, b = 2

		a = b++ + b++;        :  a = 2, b = 3

		a = b++, b;           :  a = 1, b = 2

		a = (b++ + b, b) + b; :  a = 4, b = 2

	*/

	GLSLNode *psParent = psNode->psParent;

	if(psParent->eNodeType == GLSLNT_EXPRESSION ||
		( psParent->eNodeType == GLSLNT_SUBEXPRESSION && psNode != psParent->ppsChildren[psParent->uNumChildren - 1])
		)
	{
		InsertPostfixCode(psCPD, psICProgram, psNode);
	}
}


/******************************************************************************
 * Function Name: ProcessNodeComparisonOp
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_EQUAL_TO
 *								GLSLNT_NOTEQUAL_TO
 *								GLSLNT_LESS_THAN 
 *								GLSLNT_GREATER_THAN 
 *								GLSLNT_LESS_THAN_EQUAL
 *								GLSLNT_GREATER_THAN_EQUAL
 *****************************************************************************/
static IMG_VOID ProcessNodeComparisonOp(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcess)
{
	GLSLNode				*psLeft, *psRight;
	GLSLICOperandInfo		sLeftOperand, sRightOperand, sDestOperand;
	IMG_CHAR *pszLineStart  = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* 
	** If the parent is assignment and at the stage of front end node process, 
	** we want to delay the code generation until the assignement node is visited (GLSLNT_EQUAL) 
	*/
	if(!bPostProcess && IS_PARENT_ASSIGNMENT(psNode))
	{
		return;
	}
	
	/* DEST Operand */
	if(bPostProcess)
	{
		/* At the post process stage, take the left node of assignment as the destination operand */

		GLSLNode *psDest;

		DebugAssert(IS_PARENT_ASSIGNMENT(psNode));
		DebugAssert(psNode->bEvaluated == IMG_FALSE);
		
		psDest = GET_PARENT_ASSIGNMENT_LEFT(psNode);

		ICProcessNodeOperand(psCPD, psICProgram, psDest, &sDestOperand);
	}
	else
	{
		/* Not at post stage, take the current node as destination operand */

		ICInitOperandInfo(psNode->uSymbolTableID, &sDestOperand);

		/* Indicate the Symbol ID of this node has been assigned and ready to use (as source when neccessary) */
		psNode->bEvaluated = IMG_TRUE;
	}

	/* SRCA and SRCB */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

	/* Add IC instruction */
	ICAddICInstruction3(psCPD, psICProgram,
						GetICOpCode(psNode->eNodeType),
						pszLineStart,
						&sDestOperand,
						&sLeftOperand,
						&sRightOperand);

	/* Free the offset list attached to the left and right operands */
	ICFreeOperandOffsetList(&sLeftOperand);
	ICFreeOperandOffsetList(&sRightOperand);
	ICFreeOperandOffsetList(&sDestOperand);
}

/******************************************************************************
 * Function Name: ICIsSymbolScalar
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Is symbol scalar
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICIsSymbolScalar(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 uSymbolID)
{
	GLSLFullySpecifiedType *psFullType;
	IMG_INT32 iArraySize;

	if(!ICGetSymbolInformation(psCPD, psICProgram->psSymbolTable, 
							   uSymbolID, 
							   IMG_NULL,					/* peBuiltinID */
							   &psFullType,					/* ppsFullType */
							   &iArraySize,					/* piArraySize, */
							   IMG_NULL,					/* peIdentifierUsage */
							   IMG_NULL))					/* ppvConstantData*/
	{
		LOG_INTERNAL_ERROR(("IsSymbolInternalResult: Failed to get symbol informtion \n"));
		return IMG_FALSE;
	}

	if(GLSL_IS_SCALAR(psFullType->eTypeSpecifier) && !iArraySize)
	{
		return IMG_TRUE;
	}
	
	return IMG_FALSE;
}


#if USE_IFC

/******************************************************************************
 * Function Name: GenerateOptimisedConditionCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate condition check code by using IFC
 *****************************************************************************/
static IMG_VOID GenerateOptimisedConditionCode(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram	*psICProgram, 
											   GLSLNodeType		eCompareNodeType,
											   GLSLNode			*psCompareLeft,
											   GLSLNode			*psCompareRight,
											   IMG_BOOL			bIFNot)
{
	GLSLICOpcode eICOpcode = GLSLIC_OP_NOP;
	GLSLICOperandInfo sOperands[2];
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psCompareLeft);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psCompareLeft);

	switch(eCompareNodeType)
	{
		default:
		case GLSLNT_LESS_THAN:
			eICOpcode = bIFNot ? GLSLIC_OP_IFGE : GLSLIC_OP_IFLT;
			break;
		case GLSLNT_GREATER_THAN:
			eICOpcode = bIFNot ? GLSLIC_OP_IFLE : GLSLIC_OP_IFGT;
			break;
		case GLSLNT_LESS_THAN_EQUAL:
			eICOpcode = bIFNot ? GLSLIC_OP_IFGT : GLSLIC_OP_IFLE;
			break;
		case GLSLNT_GREATER_THAN_EQUAL:
			eICOpcode = bIFNot ? GLSLIC_OP_IFLT : GLSLIC_OP_IFGE;
			break;
		case GLSLNT_EQUAL_TO:
			eICOpcode = bIFNot ? GLSLIC_OP_IFNE : GLSLIC_OP_IFEQ;
			break;
		case GLSLNT_NOTEQUAL_TO:
			eICOpcode = bIFNot ? GLSLIC_OP_IFEQ : GLSLIC_OP_IFNE;
			break;
	}

	ICTraverseAST(psCPD, psICProgram, psCompareLeft);
	ICTraverseAST(psCPD, psICProgram, psCompareRight);

	ICProcessNodeOperand(psCPD, psICProgram, psCompareLeft, &sOperands[0]);
	ICProcessNodeOperand(psCPD, psICProgram, psCompareRight, &sOperands[1]);

	ICAddICInstructiona(psCPD, psICProgram, eICOpcode, 2, psLineStart, 0, &sOperands[0]);
}

/******************************************************************************
 * Function Name: IsSimpleConditionCheck
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Check if a conditin check is very simple (comparison of two scalars)
 *				  If so, return the node type for comparision, the left comparison node 
 *				  and the right comparision node.
 *****************************************************************************/
static IMG_BOOL IsSimpleConditionCheck(GLSLCompilerPrivateData *psCPD, 
									   GLSLICProgram *psICProgram, 
									   GLSLNode		 *psCondition,
									   GLSLNodeType	 *peCompareNodeType,
									   GLSLNode      **ppsLeftNode,
									   GLSLNode      **ppsRightNode)
{
	switch(psCondition->eNodeType)
	{
		case GLSLNT_EXPRESSION:
		case GLSLNT_SUBEXPRESSION:
		{
			if(psCondition->uNumChildren == 1)
			{
				/* Go further to check */
				return IsSimpleConditionCheck(psCPD, psICProgram, psCondition->ppsChildren[0], peCompareNodeType, ppsLeftNode, ppsRightNode);
			}
			break;
		}
		case GLSLNT_LESS_THAN: 
		case GLSLNT_GREATER_THAN:
		case GLSLNT_LESS_THAN_EQUAL:
		case GLSLNT_GREATER_THAN_EQUAL:
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
		{
			if( psCondition->uNumChildren == 2			&&
				ICIsSymbolScalar(psCPD, psICProgram, psCondition->ppsChildren[0]->uSymbolTableID) && 
				ICIsSymbolScalar(psCPD, psICProgram, psCondition->ppsChildren[1]->uSymbolTableID) )
			{
				*peCompareNodeType	= psCondition->eNodeType;
				*ppsLeftNode		= psCondition->ppsChildren[0];
				*ppsRightNode		= psCondition->ppsChildren[1];
				return IMG_TRUE;
			}
			break;
		}
		default:
			break;
	}

	return IMG_FALSE;
}

#endif

/******************************************************************************
 * Function Name: ICAddICInstruction3aWithPred
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate i code with dest, srca and srcb and a predicate 
 *****************************************************************************/
static IMG_BOOL ICAddICInstruction3aWithPred(GLSLCompilerPrivateData *psCPD, 
											 GLSLICProgram		*psICProgram,
											 GLSLICOpcode		eICOpcode,
											 IMG_CHAR			*pszLineStart,
											 IMG_UINT32			uSymbolIDDEST,
											 GLSLICOperandInfo	*psOperandSRCA,
											 GLSLICOperandInfo	*psOperandSRCB,
											 IMG_UINT32			uPredicateSymID,
											 IMG_BOOL			bPredicateNegate)
{
											
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction3aWithPred: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode				= eICOpcode;

	/* Predicate */
	psICInstr->uPredicateBoolSymID	= uPredicateSymID;
	psICInstr->bPredicateNegate		= bPredicateNegate;

	/* SRCA */
	ICSetupICOperand(psCPD, psOperandSRCA, &psICInstr->asOperand[SRCA]);

	/* SRCB */
	ICSetupICOperand(psCPD, psOperandSRCB, &psICInstr->asOperand[SRCB]);

	/* DEST */
	ICInitICOperand(uSymbolIDDEST, &psICInstr->asOperand[DEST]);

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddICInstructionIFP
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
static IMG_BOOL ICAddICInstructionIFP(GLSLCompilerPrivateData	*psCPD, 
									  GLSLICProgram				*psICProgram,
									  GLSLICOpcode				eICOpcode,
									  IMG_CHAR					*pszLineStart,
									  IMG_UINT32				uSymbolID,
									  IMG_BOOL					bNegate)
{
	GLSLICInstruction *psICInstr;

	psICInstr = ICGetNewInstruction(psCPD, psICProgram);
	if(psICInstr == IMG_NULL) 
	{
		LOG_INTERNAL_ERROR(("ICAddICInstruction1a: Failed to get a new instruction \n"));
		return IMG_FALSE;
	}

	/* OPCODE */
	psICInstr->eOpCode = eICOpcode;

	/* SRC */
	ICInitICOperand(uSymbolID, &psICInstr->asOperand[SRCA]);
	if(bNegate)
	{
		psICInstr->asOperand[SRCA].eInstModifier = GLSLIC_MODIFIER_NEGATE;
	}

	psICInstr->pszOriginalLine = pszLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: GenerateCodeForPredicateXOR
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate code for predicates: p0 = p0 ^^ p1
 *****************************************************************************/
static IMG_VOID GenerateCodeForPredicateXOR(GLSLCompilerPrivateData *psCPD, 
											GLSLICProgram	*psICProgram, 
											IMG_UINT32		uP0SymID, 
											IMG_UINT32		uP1SymID, 
											IMG_CHAR		*pszLineStart)
{
	/* p0 = p0 ^^ p1 */

	/* Since there is no support for predicate operation, we need to emulate it */
	/*	p1      p0      result
		true    true    false (!p0)
		true    false   true  (!p0)
		false   true    true  (p0)
		false   false   false (p0)

		ie: p1 p0=!p0

  */
	IMG_UINT32 uZeroSymID, uOneSymID;

	/* Add 0 and 1 constants */
	if(!AddIntConstant(psCPD, psICProgram->psSymbolTable, 0, GLSLPRECQ_LOW, IMG_TRUE, &uZeroSymID))
	{
		LOG_INTERNAL_ERROR(("GenerateCodeForPredicateXOR: Failed to add int 0 to the symbol table"));
		return;
	}

	if(!AddIntConstant(psCPD, psICProgram->psSymbolTable, 1, GLSLPRECQ_LOW, IMG_TRUE, &uOneSymID))
	{
		LOG_INTERNAL_ERROR(("GenerateCodeForPredicateXOR: Failed to add int 0 to the symbol table"));
		return;
	}

	/* if p1 */
	ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uP1SymID, IMG_FALSE, pszLineStart);

		/* if p0 */
		ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uP0SymID, IMG_FALSE, pszLineStart);

			/* p0 = fasle (0 > 1 ) */
			ICAddICInstruction3d(psCPD, psICProgram, GLSLIC_OP_SGT, pszLineStart, uP0SymID, uZeroSymID, uOneSymID);

		/* else */
		ADD_ICODE_INSTRUCTION_ELSE(psCPD, psICProgram, pszLineStart);

			/* p0 = true ( 0 < 1 ) */
			ICAddICInstruction3d(psCPD, psICProgram, GLSLIC_OP_SLT, pszLineStart, uP0SymID, uZeroSymID, uOneSymID);

		/* endif */
		ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);

	/* endif */
	ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);

}


/******************************************************************************
 * Function Name: GenerateSetPredicateCode
 *
 * Inputs       : psNode			- Current node
				  uResultSymID		- Symbol ID for the result 
				  uPredicateSymID	- Symbol id to control whether the code to be generated 
									  is executed 
				  bPredicateNegate	- True if the above predicate needs negation.
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Core function to be called recursively to generate condition code 
				  and the final result is represented as a predicate (uResultSymID). 
				  uPredicateSymID controls whether the new code is executed based the predicate. 
 *****************************************************************************/
static IMG_VOID GenerateSetPredicateCode(GLSLCompilerPrivateData *psCPD, 
										 GLSLICProgram		*psICProgram, 
										  GLSLNode			*psNode, 
										  IMG_UINT32		uResultSymID, 
										  IMG_UINT32		uPredicateSymID,
										  IMG_BOOL			bPredicateNegate)
{
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLICOperandInfo sLeftOperand, sRightOperand;
	IMG_BOOL bTraverseLeft = IMG_FALSE, bTraverseRight = IMG_FALSE;
	IMG_UINT32 bUseIFBlock = IMG_FALSE;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	switch(psNode->eNodeType)
	{	
		case GLSLNT_EXPRESSION:
		case GLSLNT_SUBEXPRESSION:
		{
			IMG_UINT32 i;
			
			DebugAssert(psNode->uNumChildren);

			for(i = 0; i < psNode->uNumChildren - 1; i++)
			{
				/* This expression's children is a list of comma-seperated expressions, so the predicate is set to the last one, but the others
				   still have to be processed even though they are not used in the final expression. */
				ICTraverseAST(psCPD, psICProgram, psNode->ppsChildren[i]);
			}
						
			/* The resulting expression comes from the last child expression. */
			i = psNode->uNumChildren - 1;

			/* Go further to check */
			GenerateSetPredicateCode(psCPD, psICProgram, psNode->ppsChildren[i], uResultSymID, uPredicateSymID, bPredicateNegate);
			InsertPostfix(psCPD, psICProgram, psNode->ppsChildren[i]);
			
			break;
		}
		case GLSLNT_LESS_THAN: 
		case GLSLNT_GREATER_THAN:
		case GLSLNT_LESS_THAN_EQUAL:
		case GLSLNT_GREATER_THAN_EQUAL:
		case GLSLNT_EQUAL_TO:
		case GLSLNT_NOTEQUAL_TO:
		{
			GLSLNode *psLeft = psNode->ppsChildren[0], *psRight = psNode->ppsChildren[1];

			DebugAssert(psNode->uNumChildren == 2);

			if( !IsNodeDirectOperand(psCPD, psICProgram, psLeft))
			{
				bTraverseLeft = IMG_TRUE;
			}

			if(	!IsNodeDirectOperand(psCPD, psICProgram, psRight) )
			{
				bTraverseRight = IMG_TRUE;
			}

			if( uPredicateSymID)
			{
				if( bTraverseLeft || bTraverseRight  )
				{
					/* This mean one USC instruction is not enough, we will use IFP block */ 

					bUseIFBlock = IMG_TRUE;
				}
			}

			/* IF p1 */
			if(bUseIFBlock)
			{
				ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredicateSymID, bPredicateNegate, pszLineStart);
			}

			/* Code for the left and right operands */
			if(bTraverseLeft) 
			{
				ICTraverseAST(psCPD, psICProgram, psLeft);	
			}

			if(bTraverseRight)
			{
				ICTraverseAST(psCPD, psICProgram, psRight);
			}

			ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);
			ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

			/* p1 SGT p0 left right, p0 - uResultSymID, p1 - uPredicateSymID */
			ICAddICInstruction3aWithPred(psCPD, psICProgram,
										 NODETYPE_ICODEOP(psNode->eNodeType),
										 pszLineStart,
										 uResultSymID,
										 &sLeftOperand,
										 &sRightOperand,
										 (bUseIFBlock ? 0 : uPredicateSymID),
										 (bUseIFBlock ? IMG_FALSE : bPredicateNegate) );

			if(bUseIFBlock)
			{
				ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);
			}

			ICFreeOperandOffsetList(&sLeftOperand);
			ICFreeOperandOffsetList(&sRightOperand);
	
			break;
		}
		case GLSLNT_LOGICAL_XOR:
		{
			GLSLNode *psLeftNode = psNode->ppsChildren[0], *psRightNode = psNode->ppsChildren[1];
			IMG_BOOL bPredicateNegate = (psNode->eNodeType == GLSLNT_LOGICAL_OR) ? IMG_TRUE : IMG_FALSE;
			IMG_UINT32 uRightPredicateSymID;

			/* if p */
			if(uPredicateSymID)
			{
				ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredicateSymID, bPredicateNegate, pszLineStart);
			}

			/* Code for left hand side */
			GenerateSetPredicateCode(psCPD, psICProgram, psLeftNode, uResultSymID, 0, IMG_FALSE);

			/* We need to use a different symbod id for the right hand result */
			if(!ICAddBoolPredicate(psCPD, psICProgram, &uRightPredicateSymID))
			{
				LOG_INTERNAL_ERROR(("ProcessNodeFOR: Failed to add predicate"));
				return;
			}

			/* Code for right hand side */
			GenerateSetPredicateCode(psCPD, psICProgram, psRightNode, uRightPredicateSymID, 0, IMG_FALSE);
			
			/* p0 = p0 ^ p1, p0 = uResultSymID, p1 = uRightPredicateSymID */
			GenerateCodeForPredicateXOR(psCPD, psICProgram, uResultSymID, uRightPredicateSymID, pszLineStart);

			if(uPredicateSymID)
			{
				ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);
			}

			break;
		}
		case GLSLNT_LOGICAL_OR: 
		case GLSLNT_LOGICAL_AND:
		{
			GLSLNode *psLeftNode = psNode->ppsChildren[0], *psRightNode = psNode->ppsChildren[1];
			IMG_BOOL bPredicateNegate = (psNode->eNodeType == GLSLNT_LOGICAL_OR) ? IMG_TRUE : IMG_FALSE;

			if(uPredicateSymID)
			{
				ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredicateSymID, bPredicateNegate, pszLineStart);
			}

			/* Code for left hand side */
			GenerateSetPredicateCode(psCPD, psICProgram, psLeftNode, uResultSymID, 0, IMG_FALSE);

			/* Code for right hand side */
			GenerateSetPredicateCode(psCPD, psICProgram, psRightNode, uResultSymID, uResultSymID, bPredicateNegate);
			
			if(uPredicateSymID)
			{
				ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);
			}

			break;
		}
		default:
		{	
			IMG_UINT32 uFalseSymID;
			GLSLICOperandInfo sOperand, sFalseOperand;
			IMG_BOOL bTraverseNode = IMG_FALSE;

			if(!IsNodeDirectOperand(psCPD, psICProgram, psNode))
			{
				bTraverseNode = IMG_TRUE;

				if(uPredicateSymID) bUseIFBlock = IMG_TRUE; 
			}

			if(bUseIFBlock)
			{
				ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredicateSymID, bPredicateNegate, pszLineStart);
			}

			if(bTraverseNode)
			{
				ICTraverseAST(psCPD, psICProgram, psNode);	
			}

			ICProcessNodeOperand(psCPD, psICProgram, psNode, &sOperand);

			/* Add bool constant for false */
			if(!AddBoolConstant(psCPD, psICProgram->psSymbolTable, IMG_FALSE, GLSLPRECQ_UNKNOWN, IMG_TRUE, &uFalseSymID))
			{
				LOG_INTERNAL_ERROR(("Failed to add bool constant for false"));
				return;
			}
			ICInitOperandInfo(uFalseSymID, &sFalseOperand);
			
			/* SNE p b 0 */
			ICAddICInstruction3aWithPred(psCPD, psICProgram, 
										 GLSLIC_OP_SNE,
										 pszLineStart,
										 uResultSymID,
										 &sOperand,
										 &sFalseOperand,
										 (bUseIFBlock ? 0 : uPredicateSymID),
										 (bUseIFBlock ? IMG_FALSE : bPredicateNegate) );

			if(bUseIFBlock)
			{
				/* ENDIF */
				ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);
			}

			ICFreeOperandOffsetList(&sOperand);
	
			break;
		}		
	}	
}

/******************************************************************************
 * Function Name: ICTraverseCondition
 *
 * Inputs       : psNode - Current node
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : New traversing code for condition (if and loop), the result 
				  is always represented as a predicate
 *****************************************************************************/
static IMG_VOID ICTraverseCondition(GLSLCompilerPrivateData *psCPD, 
									GLSLICProgram *psICProgram, GLSLNode *psCondition, IMG_UINT32 *puPredBoolSymID)
{
	if(!ICAddBoolPredicate(psCPD, psICProgram, puPredBoolSymID))
	{
		LOG_INTERNAL_ERROR(("ProcessNodeFOR: Failed to add predicate"));
		return;
	}

	/* Gernate test code and evaluate all result to symbol uPredicateSymbolID */
	GenerateSetPredicateCode(psCPD, psICProgram, psCondition, (*puPredBoolSymID), 0, IMG_FALSE);

	InsertPostfix(psCPD, psICProgram, psCondition);
}


/******************************************************************************
 * Function Name: ProcessNodeQUESTION
 *
 * Inputs       : psNode, psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_QUESTION
 *****************************************************************************/
static IMG_VOID ProcessNodeQUESTION(GLSLCompilerPrivateData *psCPD, 
									GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psQuestion, *psExpr1, *psExpr2;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
#if USE_IFC
	GLSLNodeType eCompareNodeType;
	GLSLNode *psCompareLeft, *psCompareRight;
#else
	IMG_UINT32 uPredQuestionSymID;
#endif

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get question, expression1 and expression2 children */
	psQuestion = psNode->ppsChildren[0];
	psExpr1 = psNode->ppsChildren[1];
	psExpr2 = psNode->ppsChildren[2];

#if USE_IFC
	/* Evaluate question */
	if(IsSimpleConditionCheck(psCPD, psICProgram, psQuestion, &eCompareNodeType, &psCompareLeft, &psCompareRight))
	{
		GenerateOptimisedConditionCode(psCPD, psICProgram, eCompareNodeType, psCompareLeft, psCompareRight, IMG_FALSE);
	}
	else
	{
		ICTraverseAST(psCPD, psICProgram, psQuestion);

		#ifdef SRC_DEBUG
		if(psCPD->uCurSrcLine == COMP_UNDEFINED_SOURCE_LINE)
		{
			/* Get the source line number again */
			psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psQuestion);
		}
		#endif /* SRC_DEBUG */

		/* IF question */
		ADD_ICODE_INSTRUCTION_IF(psCPD, psICProgram, psQuestion, pszLineStart);
	}
#else
	ICTraverseCondition(psCPD, psICProgram, psQuestion, &uPredQuestionSymID);

	ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredicateSymID, IMG_FALSE, pszLineStart);

#endif

	/* Increment condition level */
	IncrementConditionLevel(psICProgram);

		/* Evaluate expression 1 */
		ICTraverseAST(psCPD, psICProgram, psExpr1);

		/*
		** Add assignemnt instruction: MOV DEST SRCA
		*/
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psExpr1, pszLineStart);

	/* ELSE */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ELSE, pszLineStart);

		/* Evaluate expression 2 */
		ICTraverseAST(psCPD, psICProgram, psExpr2);

		/*
		** Add assignemnt instruction: MOV DEST SRCA
		*/
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psExpr2,	pszLineStart);

	/* ENDIF*/
	ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);

	/* Decrement condition level */
	DecrementConditionLevel(psICProgram);

	/* Indicate the Symbol ID of this node is ready to use. */
	psNode->bEvaluated = IMG_TRUE;

}
/******************************************************************************
 * Function Name: ProcessNodeLogicalOR
 *
 * Inputs       : psNode, psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_LOGICAL_OR 
 *****************************************************************************/
static IMG_VOID ProcessNodeLogicalOR(GLSLCompilerPrivateData *psCPD, 
									 GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psLeft, *psRight;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get left and right children */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* Evaluate left expression */
	ICTraverseAST(psCPD, psICProgram, psLeft);

	/* IF(!psLeft) */
	ADD_ICODE_INSTRUCTION_IFNOT(psCPD, psICProgram, psLeft, pszLineStart);

		/* evaluate right */
		ICTraverseAST(psCPD, psICProgram, psRight);

		/* Add instruction for assignment node = right */
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psRight, pszLineStart);

	/* ELSE */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ELSE, pszLineStart);

		/* Add instruction for assignment node = left */
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psLeft, pszLineStart);

	/* ENDIF */
	ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);

	/* Indicate the Symbol ID of this node is ready to use. */
	psNode->bEvaluated = IMG_TRUE;

}


/******************************************************************************
 * Function Name: ProcessNodeLogicalXOR
 *
 * Inputs       : psNode, psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_LOGICAL_XOR 
 *****************************************************************************/
static IMG_VOID ProcessNodeLogicalXOR(GLSLCompilerPrivateData *psCPD, 
									  GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode		*psLeft, *psRight;
	GLSLICOperandInfo	sLeftOperand, sRightOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get left and right children */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* Evaluate left expression */
	ICTraverseAST(psCPD, psICProgram, psLeft);

	/* Evaluate right expression */
	ICTraverseAST(psCPD, psICProgram, psRight);

#if 0
	/* IF(psLeft)*/
	ADD_ICODE_INSTRUCTION_IF(psCPD, psICProgram, psLeft, pszLineStart);

		/* node = ! right */
		{
			IMG_UINT32 uFALSESymbolID;

			/* SRCA */
			ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

			/* SRCB */
			if( !AddBoolConstant(psCPD, psICProgram->psSymbolTable, "false", IMG_FALSE, GLSLPRECQ_UNKNOWN, IMG_TRUE, &uFALSESymbolID))
			{
				LOG_INTERNAL_ERROR(("ProcessNodeLogicalXOR: Failed to add bool constant false to the table !\n"));
				return;
			}

			ICAddICInstruction3b(psCPD, psICProgram,
							GLSLIC_OP_SEQ,
							pszLineStart,
							psNode->uSymbolTableID,
							&sRightOperand,
							uFALSESymbolID);

			/* Free offset list */
			ICFreeOperandOffsetList(&sRightOperand);

		}

	/* ELSE */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ELSE, pszLineStart);

		/* node = right */
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psRight, pszLineStart);

	/* ENDIF */
	ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);
#else
	ICProcessNodeOperand(psCPD, psICProgram, psLeft, &sLeftOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psRight, &sRightOperand);

	ICAddICInstruction3a(psCPD, psICProgram, GLSLIC_OP_SNE, pszLineStart, psNode->uSymbolTableID, &sLeftOperand, &sRightOperand);
#endif
	/* Indicate the Symbol ID of this node is ready to use. */
	psNode->bEvaluated = IMG_TRUE;

}


/******************************************************************************
 * Function Name: ProcessNodeLogicalAND
 *
 * Inputs       : psNode, psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_LOGICAL_AND
 *****************************************************************************/
static IMG_VOID ProcessNodeLogicalAND(GLSLCompilerPrivateData *psCPD, 
									  GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psLeft, *psRight;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* evaluate left expression */
	ICTraverseAST(psCPD, psICProgram, psLeft);

	/* IF(psLeft) */
	ADD_ICODE_INSTRUCTION_IF(psCPD, psICProgram, psLeft, pszLineStart);

		/* evaluate right */
		ICTraverseAST(psCPD, psICProgram, psRight);

		/* Add MOV instructin : node = right */
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psRight,	pszLineStart);

	/* ELSE */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ELSE, pszLineStart);

		/* Add MOV instructin : node = left */
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psLeft, pszLineStart);

	/* ENDIF */
	ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, pszLineStart);

	/* Indicate the Symbol ID of this node is ready to use. */
	psNode->bEvaluated = IMG_TRUE;

}

/******************************************************************************
 * Function Name: ProcessNodeNOT
 *
 * Inputs       : psNode, psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_NOT
 *****************************************************************************/
static IMG_VOID ProcessNodeNOT(GLSLCompilerPrivateData *psCPD, 
							   GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcess)
{
	GLSLNode				*psChild = psNode->ppsChildren[0];
	GLSLICOperandInfo		sChildOperand, sDestOperand;
	IMG_CHAR *pszLineStart	= GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);
	
	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* 
	** If the parent is assignment and at the stage of front end node process, 
	** we want to delay the code generation until the assignement node is visited (GLSLNT_EQUAL) 
	*/
	if(!bPostProcess && IS_PARENT_ASSIGNMENT(psNode))
	{
		return;
	}
	
	/* DEST Operand */
	if(bPostProcess)
	{
		/* At the post process stage, take the left node of assignment as the destination operand */

		GLSLNode *psDest;

		DebugAssert(IS_PARENT_ASSIGNMENT(psNode));
		DebugAssert(psNode->bEvaluated == IMG_FALSE);
		
		psDest = GET_PARENT_ASSIGNMENT_LEFT(psNode);

		ICProcessNodeOperand(psCPD, psICProgram, psDest, &sDestOperand);
	}
	else
	{
		/* Not at post stage, take the current node as destination operand */

		ICInitOperandInfo(psNode->uSymbolTableID, &sDestOperand);

		/* Indicate the Symbol ID of this node has been assigned and ready to use (as source when neccessary) */
		psNode->bEvaluated = IMG_TRUE;
	}

	/* SRCA */
	ICProcessNodeOperand(psCPD, psICProgram, psChild, &sChildOperand);

	/* Adding IC instruction */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_NOT, pszLineStart, &sDestOperand, &sChildOperand);

	/* Free offsets */
	ICFreeOperandOffsetList(&sChildOperand);
	ICFreeOperandOffsetList(&sDestOperand);

}


/******************************************************************************
 * Function Name: ProcessNodeCONTINUE
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_CONTINUE
 *****************************************************************************/
static IMG_VOID ProcessNodeCONTINUE(GLSLCompilerPrivateData *psCPD, 
									GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_CONTINUE, pszLineStart);
}

/******************************************************************************
 * Function Name: ProcessNodeBREAK
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_CONTINUE
 *****************************************************************************/
static IMG_VOID ProcessNodeBREAK(GLSLCompilerPrivateData *psCPD, 
								 GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_BREAK, pszLineStart);
}

/******************************************************************************
 * Function Name: ProcessNodeSUBEXPRESSION
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_CONTINUE
 *****************************************************************************/
static IMG_VOID ProcessNodeSUBEXPRESSION(GLSLCompilerPrivateData *psCPD, 
										 GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psLastChild;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get rid of compiler warning */
	PVR_UNREFERENCED_PARAMETER(psCPD);
	PVR_UNREFERENCED_PARAMETER(psICProgram);

	psLastChild = psNode->ppsChildren[psNode->uNumChildren - 1];

	DebugAssert(psLastChild);

	/* Inherit evaluation status from its last child */
	psNode->bEvaluated = psLastChild->bEvaluated;		
}


/******************************************************************************
 * Function Name: ProcessNodeEXPRESSION
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_CONTINUE
 *****************************************************************************/
static IMG_VOID ProcessNodeEXPRESSION(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psLastChild;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get rid of warning */
	PVR_UNREFERENCED_PARAMETER(psCPD);
	PVR_UNREFERENCED_PARAMETER(psICProgram);

	psLastChild = psNode->ppsChildren[psNode->uNumChildren - 1];

	DebugAssert(psLastChild);

	/* Inherit evaluation status from its last child */
	psNode->bEvaluated = psLastChild->bEvaluated;
}


/******************************************************************************
 * Function Name: ICCombineTwoConstantSwizzles
 *
 * Inputs       : psICProgram, psNode, 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Combine two constant swiz, the result swiz write to the first swiz
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICCombineTwoConstantSwizzles(GLSLICVecSwizWMask *psFirstSwiz, 
													GLSLICVecSwizWMask *psSecondSwiz)
{
	IMG_UINT32 i;

	if(psSecondSwiz->uNumComponents == 0)
	{	
		return;
	}
	else
	{
		if(psFirstSwiz->uNumComponents == 0)
		{
			*psFirstSwiz = *psSecondSwiz;
		}
		else
		{
			GLSLICVecSwizWMask sFirstSwiz = *psFirstSwiz;

			psFirstSwiz->uNumComponents = psSecondSwiz->uNumComponents;
			for(i = 0; i < psSecondSwiz->uNumComponents; i++)
			{
				psFirstSwiz->aeVecComponent[i] = 
					sFirstSwiz.aeVecComponent[psSecondSwiz->aeVecComponent[i]];
			}
		}
	}
}


/******************************************************************************
 * Function Name: ICAdjustOperandLastOffset
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : modify the last offset of the operand info structure
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICAdjustOperandLastOffset(GLSLICOperandInfo *psOperandInfo, 
												IMG_UINT32 uNewStaticOffset, 
												IMG_UINT32 uNewOffsetSymbolID)
{
	GLSLICOperandOffsetList *psListEnd = psOperandInfo->psOffsetListEnd;

	psListEnd->sOperandOffset.uStaticOffset = uNewStaticOffset;
	psListEnd->sOperandOffset.uOffsetSymbolID = uNewOffsetSymbolID;	
}

/******************************************************************************
 * Function Name: ProcessConstructorForBasicType
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Procoss constructor for GLSL base type specifier.
 *****************************************************************************/
static IMG_VOID ProcessConstructorForBasicType(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram	*psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	GLSLNode *child;
	GLSLFunctionCallData *psFunctionCallData;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLTypeSpecifier eDestTypeSpecifier, eChildTypeSpecifier;
	IMG_UINT32 uNumComponentsInBatch;
	IMG_UINT32 uDestColIndex = 0, uDestComponentIndex = 0;
	IMG_UINT32 uDestComponentsLeft;
	IMG_UINT32 uDestComponentsProcessed = 0;
	IMG_UINT32 uDestRows, uDestCols;
	IMG_UINT32 i, j;
	GLSLICVecSwizWMask sOriginalDestSwizMask;
	IMG_BOOL bMoveToTemp = IMG_FALSE;
	GLSLICOperandInfo sTDestOperandInfo, *psTDestOperand = psDestOperand;
	GLSLICOperandInfo *psChildOperands = IMG_NULL;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get Function call data */
	psFunctionCallData = (GLSLFunctionCallData *) GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessConstructorMatrix: Failed to get symbol data for ID: %u\n", psNode->uSymbolTableID));
		return;
	}
	eDestTypeSpecifier = psFunctionCallData->sFullySpecifiedType.eTypeSpecifier;

	/* Check if one move can do the work */
	if(psNode->uNumChildren == 1)
	{
		IMG_BOOL bOneMove = IMG_FALSE;

		GLSLFullySpecifiedType *psFullType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
		GLSLTypeSpecifier eChildTypeSpecifier = psFullType->eTypeSpecifier;

		if(!psFullType->iArraySize) 
		{
			if(GLSL_IS_SCALAR(eDestTypeSpecifier))
			{
				if(GLSL_IS_SCALAR(eChildTypeSpecifier)) bOneMove = IMG_TRUE;
			}
			else if(GLSL_IS_VECTOR(eDestTypeSpecifier))
			{
				if(GLSL_IS_SCALAR(eChildTypeSpecifier))
				{
					bOneMove = IMG_TRUE;
				}
				else if(GLSL_IS_VECTOR(eChildTypeSpecifier) && (TYPESPECIFIER_NUM_COMPONENTS(eDestTypeSpecifier) == TYPESPECIFIER_NUM_COMPONENTS(eChildTypeSpecifier)))
				{
					bOneMove = IMG_TRUE;
				}
			}
			else if(GLSL_IS_MATRIX(eDestTypeSpecifier))
			{
				if(GLSL_IS_SCALAR(eChildTypeSpecifier) || GLSL_IS_MATRIX(eChildTypeSpecifier))
				{
					bOneMove = IMG_TRUE;
				}
			}

			if(bOneMove)
			{
				/* One move can do the job according to the operand rules in icode.h */
				GLSLICOperandInfo	sChildOperand;

				ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[0], &sChildOperand);
				
				ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV,	pszLineStart, psDestOperand, &sChildOperand);	

				ICFreeOperandOffsetList(&sChildOperand);

				return;
			}
		}
	}

	/* Get constructor number of components */
	uDestRows = TYPESPECIFIER_NUM_ROWS(eDestTypeSpecifier);
	uDestCols = TYPESPECIFIER_NUM_COLS(eDestTypeSpecifier);
	uDestComponentsLeft = uDestRows * uDestCols;

	/* Process child operand first */
	psChildOperands = DebugMemAlloc(sizeof(GLSLICOperandInfo) * psNode->uNumChildren);
	if(psChildOperands == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("Failed to allocate memory"));
		return;
	}

	/* Check whether destination is used as child source, if so we need to move to temp first */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &psChildOperands[i]);

		if(psChildOperands[i].uSymbolID == psDestOperand->uSymbolID)
		{
			bMoveToTemp = IMG_TRUE;
		}
	}

	if(bMoveToTemp)
	{
		ICInitOperandInfo(psNode->uSymbolTableID, &sTDestOperandInfo);
		psTDestOperand = &sTDestOperandInfo;
	}

	if(GLSL_IS_MATRIX(eDestTypeSpecifier))
	{
		ICAddOperandOffset(psTDestOperand, 0, 0);
	}

	sOriginalDestSwizMask = psTDestOperand->sSwizWMask;

	for(i = 0; i < psNode->uNumChildren; i++)
	{
		GLSLICVecSwizWMask sOriginalChildSwizMask;
		IMG_UINT32 uChildRows, uChildCols, uChildComponentsUsed = 0, uChildComponentsLeft;
		IMG_UINT32 uChildColIndex, uChildComponentIndex;
		GLSLFullySpecifiedType *psFullType;
		IMG_INT32 iArraySize;
		IMG_INT32 ii;
		GLSLICOperandOffset *psArrayOffset		= IMG_NULL;
		GLSLICOperandOffset *psMatrixColOffset	= IMG_NULL;

		/* Get Child node */
		child = psNode->ppsChildren[i];

		/* Get the type specifier and array size */
		if(!ICGetSymbolInformation(psCPD, psICProgram->psSymbolTable, child->uSymbolTableID, IMG_NULL, &psFullType, &iArraySize, IMG_NULL, IMG_NULL))
		{
			LOG_INTERNAL_ERROR(("ProcessConstructorForBasicType: Failed to retrieve symbol information\n"));
			return;
		}

		eChildTypeSpecifier = psFullType->eTypeSpecifier;

		/* Save a copy of original child swiz mask */
		sOriginalChildSwizMask = psChildOperands[i].sSwizWMask;
		

		if(iArraySize)
		{
			ICAddOperandOffset(&psChildOperands[i], 0, 0);
			psArrayOffset = &psChildOperands[i].psOffsetListEnd->sOperandOffset;
		}

		if(GLSL_IS_MATRIX(eChildTypeSpecifier))
		{
			ICAddOperandOffset(&psChildOperands[i], 0, 0);
			psMatrixColOffset = &psChildOperands[i].psOffsetListEnd->sOperandOffset;
		}

		uChildRows = TYPESPECIFIER_NUM_ROWS(eChildTypeSpecifier);
		uChildCols = TYPESPECIFIER_NUM_COLS(eChildTypeSpecifier);

		for(ii = 0; ii < (iArraySize ? iArraySize : 1); ii++)
		{
			uChildComponentsLeft = uChildRows * uChildCols;

			while(uChildComponentsLeft)
			{
				uDestColIndex = uDestComponentsProcessed/uDestRows;
				uDestComponentIndex = uDestComponentsProcessed%uDestRows;

				uChildColIndex = uChildComponentsUsed / uChildRows;
				uChildComponentIndex = uChildComponentsUsed % uChildRows;

				uNumComponentsInBatch = MIN(uDestRows, uChildRows);

				/* Cannot cross two dest columns */
				if(uNumComponentsInBatch + uDestComponentIndex > uDestRows)
				{
					uNumComponentsInBatch = uDestRows - uDestComponentIndex; 
				}
				if(uNumComponentsInBatch > uDestComponentsLeft) uNumComponentsInBatch = uDestComponentsLeft;


				/* Cannot cross two src columns */
				if(uNumComponentsInBatch + uChildComponentIndex > uChildRows )
				{
					uNumComponentsInBatch = uChildRows - uChildComponentIndex; 
				}
				if(uNumComponentsInBatch > uChildComponentsLeft) uNumComponentsInBatch = uChildComponentsLeft;
				
				/* SRCA, modify array offset if it is an array */
				if(psArrayOffset)
				{
					psArrayOffset->uStaticOffset = ii;
				}

				/* SRCA, modify colum offset if it is an matrix */
				if(psMatrixColOffset)
				{
					psMatrixColOffset->uStaticOffset = uChildColIndex;
				}

				/* SRCA, modify swiz mask if the number of components is less than uChildRows */
				psChildOperands[i].sSwizWMask = sOriginalChildSwizMask;
				if(uNumComponentsInBatch < uChildRows)
				{
					GLSLICVecSwizWMask sSecondSwizzle;
					sSecondSwizzle.uNumComponents = uNumComponentsInBatch;
					
					for(j = 0; j < uNumComponentsInBatch; j++)
					{
						sSecondSwizzle.aeVecComponent[j] = (GLSLICVecComponent)(uChildComponentIndex + j);
					}
					ICCombineTwoConstantSwizzles(&psChildOperands[i].sSwizWMask, &sSecondSwizzle);
				}

				/* DEST */
				psTDestOperand->sSwizWMask= sOriginalDestSwizMask;
				if(GLSL_IS_MATRIX(eDestTypeSpecifier))
				{
					ICAdjustOperandLastOffset(psTDestOperand, uDestColIndex, 0);
				}
				else
				{
					DebugAssert(uDestColIndex == 0);
				}

				/* DEST, modify swiz mask if the numbe of components is less than uDestRows */
				if(uNumComponentsInBatch < uDestRows)
				{
					GLSLICVecSwizWMask sSecondSwizzle;

					sSecondSwizzle.uNumComponents = uNumComponentsInBatch;
					for(j = 0; j < uNumComponentsInBatch; j++)
					{
						sSecondSwizzle.aeVecComponent[j] = (GLSLICVecComponent)(uDestComponentIndex + j);
					}

					ICCombineTwoConstantSwizzles(&psTDestOperand->sSwizWMask, &sSecondSwizzle);
				}

				/* Actual move instruction */
				ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psTDestOperand, &psChildOperands[i]);

				/* Update the number of components left */
				uDestComponentsLeft		 -= uNumComponentsInBatch;
				uDestComponentsProcessed += uNumComponentsInBatch;
				uChildComponentsUsed	 += uNumComponentsInBatch;
				uChildComponentsLeft	 -= uNumComponentsInBatch;

				if(uDestComponentsLeft == 0) break;
			}

			if(uDestComponentsLeft == 0) break;
		}

		if(uDestComponentsLeft == 0) break;
	}

	/* Move to the destination if we've moved to a temp */
	if(bMoveToTemp)
	{
		/* Restore original setting for temp */
		ICInitOperandInfo(psNode->uSymbolTableID, psTDestOperand);

		/* Move temp to dest */
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, psTDestOperand);
	}

	/* Free memory for child operands */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICFreeOperandOffsetList(&psChildOperands[i]);
	}

	DebugMemFree(psChildOperands);

}

static IMG_VOID ProcessArrayConstructor(GLSLCompilerPrivateData *psCPD,
										GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLNode *child;
	GLSLFunctionCallData *psFunctionCallData;
	GLSLICOperandInfo sChildOperand;
	IMG_UINT32 i;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get Function call data */
	psFunctionCallData = (GLSLFunctionCallData *) GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessConstructorMatrix: Failed to get symbol data for ID: %u\n", psNode->uSymbolTableID));
		return;
	}

	DebugAssert(psFunctionCallData->sFullySpecifiedType.iArraySize == (IMG_INT32) psNode->uNumChildren);

	/* Add offset to allow array element access */
	ICAddOperandOffset(psDestOperand, 0, 0);

	for(i = 0; i < psNode->uNumChildren; i++)
	{
		child = psNode->ppsChildren[i];

		ICProcessNodeOperand(psCPD, psICProgram, child, &sChildOperand);

		ICAdjustOperandLastOffset(psDestOperand, i, 0);

		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, &sChildOperand);

		ICFreeOperandOffsetList(&sChildOperand);
	}
}
/******************************************************************************
 * Function Name: ICProcessConstructor
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type: 	
 *****************************************************************************/
static IMG_VOID ICProcessConstructor(GLSLCompilerPrivateData *psCPD,
									 GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFunctionCallData	*psFunctionCallData;

	/* Get Function call data */
	psFunctionCallData = (GLSLFunctionCallData *) GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessConstructor: Failed to get symbol data for ID: %u\n", psNode->uSymbolTableID));
		return;
	}

	/* User defined structure won't come here, should call ICProcessStructureConstructor */
	DebugAssert(!GLSL_IS_STRUCT(psFunctionCallData->sFullySpecifiedType.eTypeSpecifier));

	if(psFunctionCallData->sFullySpecifiedType.iArraySize)
	{
		/* Array constructor */
		ProcessArrayConstructor(psCPD, psICProgram, psNode, psDestOperand);
	}
	else
	{
		/* Basic type constructor */
		ProcessConstructorForBasicType(psCPD, psICProgram, psNode, psDestOperand);
	}

}


/******************************************************************************
 * Function Name: ICProcessSturctureConstructor
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : generate icode for structure constructor
 *****************************************************************************/
static IMG_VOID ICProcessSturctureConstructor(GLSLCompilerPrivateData *psCPD,
											  GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	IMG_UINT32					i;
	GLSLFunctionCallData		*psFunctionCallData;
	GLSLStructureDefinitionData *psStructionDefinitionData;
	GLSLICOperandInfo			sOperandInfo;
	GLSLICInstruction			*psICInstr;

	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psFunctionCallData = (GLSLFunctionCallData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessSturctureConstructor(1): Failed to get symbol data for ID: %u\n", psNode->uSymbolTableID));
		return;
	}

	DebugAssert(psFunctionCallData->sFullySpecifiedType.eTypeSpecifier == GLSLTS_STRUCT);

	psStructionDefinitionData = (GLSLStructureDefinitionData *) 
		GetSymbolTableData(psICProgram->psSymbolTable, psFunctionCallData->sFullySpecifiedType.uStructDescSymbolTableID);
	if(psStructionDefinitionData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessSturctureConstructor(2): Failed to get symbol data for ID: %u\n", 
			psFunctionCallData->sFullySpecifiedType.uStructDescSymbolTableID));
		return;
	}

	DebugAssert(psStructionDefinitionData->uNumMembers == psNode->uNumChildren);

	/* Add operand offset to allow member access */
	ICAddOperandOffset(psDestOperand, 0, 0);

	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &sOperandInfo);

		psICInstr = ICGetNewInstruction(psCPD, psICProgram);
		if(psICInstr == IMG_NULL) 
		{
			LOG_INTERNAL_ERROR(("ICAddICInstruction2: Failed to get a new instruction \n"));
			return ;
		}

		/* OPCODE */
		psICInstr->eOpCode = GLSLIC_OP_MOV;

		/* SRCA */
		ICSetupICOperand(psCPD, &sOperandInfo, &psICInstr->asOperand[SRCA]);

		/* DEST */
		ICAdjustOperandLastOffset(psDestOperand, i, 0);
		ICSetupICOperand(psCPD, psDestOperand, &psICInstr->asOperand[DEST]);

		/* misc */
		psICInstr->pszOriginalLine = pszLineStart;

		ValidateICInstruction(psCPD, psICProgram, psICInstr);

		ICFreeOperandOffsetList(&sOperandInfo);
	}
}


static IMG_BOOL MergeTwoIdentifierUsage(GLSLCompilerPrivateData *psCPD,
										GLSLICProgram *psICProgram,
										IMG_UINT32 uFirstSymbolID,
										IMG_UINT32 uSecondSymbolID)
{
	GLSLGenericData	    *psGenericData;
	GLSLIdentifierUsage  uIdentifierUsage2 = (GLSLIdentifierUsage)0;
	GLSLIdentifierData  *psIdentifierData  = IMG_NULL;

	/* For identifier 1 */
	psGenericData = (GLSLGenericData *) GetSymbolTableData(psICProgram->psSymbolTable, uFirstSymbolID);
	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("MergeTwoIdentifierUsage: Failed to symbol table data for id=%d\n", uFirstSymbolID));
		return IMG_FALSE;
	}

	switch (psGenericData->eSymbolTableDataType)
	{
		case GLSLSTDT_IDENTIFIER: 
		{
			psIdentifierData = (GLSLIdentifierData*)psGenericData;
			break;
		}

		case GLSLSTDT_FUNCTION_CALL: 
		{
			//GLSLFunctionCallData *psData;
			//psData = (GLSLFunctionCallData *)psGenericData;

			/* FIXME don't know what should we do */

			break;
		}

		default: 
			psIdentifierData = IMG_NULL;
	}

	/* For identifier 2 */
	psGenericData = (GLSLGenericData *) GetSymbolTableData(psICProgram->psSymbolTable, uSecondSymbolID);
	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("MergeTwoIdentifierUsage: Failed to symbol table data for id=%d\n", uSecondSymbolID));
		return IMG_FALSE;
	}

	switch (psGenericData->eSymbolTableDataType)
	{
		case GLSLSTDT_IDENTIFIER: 
		{
			GLSLIdentifierData *psIdentifierData = (GLSLIdentifierData*)psGenericData;
			uIdentifierUsage2 = psIdentifierData->eIdentifierUsage;
			break;
		}
		case GLSLSTDT_FUNCTION_CALL: 
		{
			//GLSLFunctionCallData *psData;
			//psData = (GLSLFunctionCallData *)psGenericData;

			/* FIXME don't know what should we do */
			uIdentifierUsage2 = (GLSLIdentifierUsage)0;
			break;
		}
		default:
			break;
	}

	if(psIdentifierData)
	{
		psIdentifierData->eIdentifierUsage |= uIdentifierUsage2;
	}

	return IMG_TRUE;

}


/******************************************************************************
 * Function Name: ReplaceInlinedInstOperand
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Modify inlined instruction operand with an argument
 *****************************************************************************/
static IMG_VOID ReplaceInlinedInstOperand(GLSLCompilerPrivateData *psCPD, 
										  GLSLICProgram *psICProgram,
										  GLSLICOperand *psBeingReplacedOperand,
										  GLSLICOperand *psReplaceWithOperand)
{
	GLSLICOperand sNewOperand; /* The merged operand */ 
	IMG_UINT32 i;

	if(!MergeTwoIdentifierUsage(psCPD, psICProgram, psReplaceWithOperand->uSymbolID, psBeingReplacedOperand->uSymbolID))
	{
		LOG_INTERNAL_ERROR(("ReplaceInlinedInstOperand: Failed to merge two identifiers usage\n"));
		return;
	}

	/* Initialy it set to the operand replaced with. */
	sNewOperand = *psReplaceWithOperand;

	/* Merge two offsets list */
	sNewOperand.uNumOffsets = psReplaceWithOperand->uNumOffsets + psBeingReplacedOperand->uNumOffsets;
	if(sNewOperand.uNumOffsets)
	{
		sNewOperand.psOffsets = DebugMemAlloc(sNewOperand.uNumOffsets * sizeof(GLSLICOperandOffset));
		if(sNewOperand.psOffsets == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("Failed to allocated memory\n"));
			return;
		}
	}

	for(i = 0; i < psReplaceWithOperand->uNumOffsets; i++)
	{
		sNewOperand.psOffsets[i] = psReplaceWithOperand->psOffsets[i];
	}

	for(i = 0; i < psBeingReplacedOperand->uNumOffsets; i++)
	{
		sNewOperand.psOffsets[i + psReplaceWithOperand->uNumOffsets] = psBeingReplacedOperand->psOffsets[i];
	}

	/* Merge two swizzles */
	ICCombineTwoConstantSwizzles(&sNewOperand.sSwizWMask, &psBeingReplacedOperand->sSwizWMask);

	/* Merge negate modifier */
	if(psBeingReplacedOperand->eInstModifier & GLSLIC_MODIFIER_NEGATE) 
	{
		sNewOperand.eInstModifier ^= GLSLIC_MODIFIER_NEGATE;
	}

	/* Free the second offset list */
	if(psBeingReplacedOperand->uNumOffsets)
	{
		DebugMemFree(psBeingReplacedOperand->psOffsets);
	}

	*psBeingReplacedOperand = sNewOperand;	
}

/******************************************************************************
 * Function Name: ICProcessInlinedFunctionCall
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate code for inlined fuction call
 *****************************************************************************/
static IMG_VOID ICProcessInlinedFunctionCall(GLSLCompilerPrivateData *psCPD, 
											 GLSLICProgram				*psICProgram,
											 GLSLNode					*psNode,
											 GLSLFunctionDefinitionData	*psFunctionDefinitionData,
											 GLSLICShaderChild			*psShaderChild)
{
	GLSLICContext       *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLICInstruction	*psClonedStart, *psClonedEnd, *psInstr;
	GLSLICOperand		*psArgumentOperand = IMG_NULL; 
	IMG_BOOL			*pbNeedReplace = IMG_NULL;
	IMG_UINT32 i, j, k, s;

	/* If function contain no instruction, just return; */
	if(psShaderChild->bEmptyBody) return;

	/* Process all children's operands */
	if(psNode->uNumChildren)
	{
		GLSLICOperandInfo sOperand;
		GLSLIdentifierData *psParamData;

		psArgumentOperand = DebugMemAlloc(psNode->uNumChildren * sizeof(GLSLICOperand));
		if(psArgumentOperand == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to get memory for argument list \n"));
			return;
		}

		pbNeedReplace = DebugMemAlloc(psNode->uNumChildren * sizeof(IMG_BOOL));
		if(pbNeedReplace == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to get memory for argument list \n"));
			return;
		}

		for(i = 0; i < psNode->uNumChildren; i++)
		{
			ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &sOperand);
			ICSetupICOperand(psCPD, &sOperand, &psArgumentOperand[i]);

			psParamData = ( GLSLIdentifierData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionDefinitionData->puParameterSymbolTableIDs[i]); 
			if(psParamData == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to get symbol data for ID: %u\n",
					psFunctionDefinitionData->puParameterSymbolTableIDs[i]));
				return;
			}

			/* 
				For paramenters with in qualifier and the function body writes to the variable
				and we cannot use the original variable, we need to copy it to a temp. 
			*/
			if (psParamData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_IN &&
				psParamData->eIdentifierUsage & GLSLIU_WRITTEN )
			{

				ICAddICInstruction2a(psCPD, psICProgram,
									GLSLIC_OP_MOV,
									NULL,
									psFunctionDefinitionData->puParameterSymbolTableIDs[i],
									&sOperand);

				pbNeedReplace[i] = IMG_FALSE;
			}
			else
			{
				pbNeedReplace[i] = IMG_TRUE;
			}

			ICFreeOperandOffsetList(&sOperand);
		}	
	}

	/* Copy the function body instructions over */
	CloneICodeInstructions(psCPD, psICProgram,
						   psShaderChild->psCodeStart,
						   psShaderChild->psCodeEnd,
						   &psClonedStart,
						   &psClonedEnd);

	/* Examine each instruction one by one and modify operands when neccessary */
	for(psInstr = psClonedStart; ; psInstr = psInstr->psNext)
	{
		for(j = 0; j < ICOP_NUM_SRCS(psInstr->eOpCode) + 1; j++)
		{
			if(!ICOP_HAS_DEST(psInstr->eOpCode) && j == DEST) continue;
			
			for(k = 0; k < psFunctionDefinitionData->uNumParameters; k++)
			{
				if(pbNeedReplace[k])
				{
					if(psInstr->asOperand[j].uSymbolID == psFunctionDefinitionData->puParameterSymbolTableIDs[k])
					{
						/* Replace the inlined instruction operand with input of argument */
						ReplaceInlinedInstOperand(psCPD, psICProgram, &psInstr->asOperand[j], &psArgumentOperand[k]);
					}

					for(s = 0; s < psInstr->asOperand[j].uNumOffsets; s++)
					{
						if(psInstr->asOperand[j].psOffsets[s].uOffsetSymbolID == psFunctionDefinitionData->puParameterSymbolTableIDs[k])
						{
							/* if the argument is simple */
							if(!psArgumentOperand[k].eInstModifier && 
								psArgumentOperand[k].uNumOffsets == 0 &&
								psArgumentOperand[k].sSwizWMask.uNumComponents == 0)
							{
								psInstr->asOperand[j].psOffsets[s].uOffsetSymbolID = psArgumentOperand[k].uSymbolID;
							}
							else
							{
								/* The argment is not simple, INSERT a new MOV instruction */
								IMG_UINT32 uTempSymbolID;
								GLSLICInstruction *psNewInstr;
								GLSLPrecisionQualifier eTempPrecision;

								psNewInstr = DebugAllocHeapItem(psICContext->psInstructionHeap);
								if(psNewInstr == IMG_NULL)
								{
									LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to allocate memory for new instruction \n"));
									return;
								}

								#ifdef SRC_DEBUG
								psNewInstr->uSrcLine = psCPD->uCurSrcLine;
								#endif /* SRC_DEBUG */

								/* The precision for the temp is the same as the argument */
								eTempPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psArgumentOperand[k].uSymbolID);
								if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_INT, eTempPrecision, &uTempSymbolID))
								{
									LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to add a temp to the symbol table. \n"));
									return;
								}

								psNewInstr->eOpCode = GLSLIC_OP_MOV;
								ICInitICOperand(uTempSymbolID, &psNewInstr->asOperand[DEST]);
								psNewInstr->asOperand[SRCA] = psArgumentOperand[k];
								psNewInstr->pszOriginalLine = psInstr->pszOriginalLine;
								if(psArgumentOperand[k].uNumOffsets)
								{
									psNewInstr->asOperand[SRCA].psOffsets = DebugMemAlloc(psArgumentOperand[k].uNumOffsets * sizeof(GLSLICOperandOffset));
									if(psNewInstr->asOperand[SRCA].psOffsets == IMG_NULL) 
									{
										LOG_INTERNAL_ERROR(("ICProcessInlinedFunctionCall: Failed to get memory for offsets"));
										return;
									}
								}

								psInstr->asOperand[j].psOffsets[s].uOffsetSymbolID = uTempSymbolID;

								InsertInstructionsAfter(psICProgram, psInstr->psPrev, psNewInstr, psNewInstr);
							}

						}
					}
				}
			}

		}

		if(psInstr == psClonedEnd) break;
	}

	/* Replace the return value - we dont handle early return, so we just need to replace the last instruction dest */
	if(psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID)
	{
		DebugAssert(psClonedEnd->eOpCode == GLSLIC_OP_MOV);
		DebugAssert(psClonedEnd->asOperand[DEST].uSymbolID == psFunctionDefinitionData->uReturnDataSymbolID);
		
		psClonedEnd->asOperand[DEST].uSymbolID = psNode->uSymbolTableID;
		psNode->bEvaluated = IMG_TRUE;
	}

	/* Free some memory allocated */
	if(psArgumentOperand)
	{
		for(i = 0; i < psNode->uNumChildren; i++)
		{
			if(psArgumentOperand[i].uNumOffsets) DebugMemFree(psArgumentOperand[i].psOffsets);
		}
		DebugMemFree(psArgumentOperand);
	}
	if(pbNeedReplace) DebugMemFree(pbNeedReplace);
}

/******************************************************************************
 * Function Name: FindShaderChildFromFunctionID
 *
 * Inputs       : psICProgram, uFunctionDefinitionID
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Find a child of node SHADER with specific function ID
 *****************************************************************************/

static GLSLICShaderChild *FindShaderChildFromFunctionID(GLSLICProgram *psICProgram, 
														IMG_UINT32 uFunctionDefinitionID)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 i;
	for(i = 0; i < psICContext->uNumShaderChildren; i++)
	{
		if(psICContext->psShaderChildren[i].uFunctionDefinitionID == uFunctionDefinitionID)
		{
			return &psICContext->psShaderChildren[i];
		}
	}
	return IMG_NULL;
}
/******************************************************************************
 * Function Name: ICProcessUserDefinedFunctionCall
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate icode for usder defined function call
 *****************************************************************************/
static IMG_VOID ICProcessUserDefinedFunctionCall(GLSLCompilerPrivateData *psCPD, 
												 GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	IMG_UINT32					i;
	GLSLFunctionCallData		*psFunctionCallData;
	GLSLFunctionDefinitionData	*psFunctionDefinitionData;
	GLSLIdentifierData			*psParamData;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLICOperandInfo			*psArgumentOperand = IMG_NULL;
	GLSLICShaderChild			*psShaderChild;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psFunctionCallData = (GLSLFunctionCallData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: Failed to get symbol data for ID: %u\n", 
			psNode->uSymbolTableID));
		return;
	}

	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionCallData->uFunctionDefinitionSymbolID);
	if(psFunctionDefinitionData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: Failed to get symbol data for ID: %u\n", 
			psFunctionCallData->uFunctionDefinitionSymbolID));
		return;
	}
	DebugAssert(psFunctionDefinitionData->uNumParameters == psNode->uNumChildren);

	psShaderChild = FindShaderChildFromFunctionID(psICProgram, 
												  psFunctionCallData->uFunctionDefinitionSymbolID);
	if(psShaderChild == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: Failed to find function with function definition ID=%u\n", 
			psFunctionCallData->uFunctionDefinitionSymbolID));
		return;
	}

	if(!psShaderChild->bCodeGenerated)
	{
		LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: The code for function to be called has not been generated.\n"));
		return;
	}

	if(psShaderChild->bToBeInlined)
	{
		ICProcessInlinedFunctionCall(psCPD, psICProgram, psNode, psFunctionDefinitionData, psShaderChild);
		return;
	}

	if(psNode->uNumChildren)
	{
		psArgumentOperand = DebugMemAlloc(psNode->uNumChildren * sizeof(GLSLICOperandInfo));
		if(psArgumentOperand == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: cannot get memory\n"));
		}
	}

	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &psArgumentOperand[i]);

		psParamData = ( GLSLIdentifierData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionDefinitionData->puParameterSymbolTableIDs[i]); 
		if(psParamData == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: Failed to get symbol data for ID: %u\n",
				psFunctionDefinitionData->puParameterSymbolTableIDs[i]));
			return;
		}

		/* Copy data in */
		if (psParamData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_IN ||
			psParamData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INOUT )
		{

			ICAddICInstruction2a(psCPD, psICProgram,
								GLSLIC_OP_MOV,
								pszLineStart,
								psFunctionDefinitionData->puParameterSymbolTableIDs[i],
								&psArgumentOperand[i]);

		}

	}

	/* Actual function call  */
	ICAddICInstruction1a(psCPD, psICProgram,
						GLSLIC_OP_CALL,
						pszLineStart,
						psFunctionCallData->uFunctionDefinitionSymbolID);

	/* 
	** copy data out 
	*/
	for(i = 0; i < psFunctionDefinitionData->uNumParameters; i++)
	{
		psParamData = ( GLSLIdentifierData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionDefinitionData->puParameterSymbolTableIDs[i]); 
		if(psParamData == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ICProcessUserDefinedFunctionCall: Failed to get symbol data for ID: %u\n",
				psFunctionDefinitionData->puParameterSymbolTableIDs[i]));
			return;
		}

		if (psParamData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_OUT ||
			psParamData->sFullySpecifiedType.eParameterQualifier == GLSLPQ_INOUT )
		{

			ICAddICInstruction2c(psCPD, psICProgram,
								 GLSLIC_OP_MOV,
								 pszLineStart,
								 &psArgumentOperand[i],
								 psFunctionDefinitionData->puParameterSymbolTableIDs[i]);

		}
	}

	/* Save the function return value to the current node */
	if(psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID)
	{
		/* Move the funciton return in the function definition to this function call */
		ICAddICInstruction2b(psCPD, psICProgram, 
							 GLSLIC_OP_MOV,
							 pszLineStart,
							 psNode->uSymbolTableID,
							 psFunctionDefinitionData->uReturnDataSymbolID);
		psNode->bEvaluated = IMG_TRUE;
	}

	/* free the operands offsets */
	for(i = 0; i < psFunctionDefinitionData->uNumParameters; i++)
	{
		ICFreeOperandOffsetList(&psArgumentOperand[i]);
	}
	
	if(psArgumentOperand) DebugMemFree(psArgumentOperand);
}


/******************************************************************************
 * Function Name: ProcessNodeFUNCTIONCALL
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_FUNCTIONDEFINITION
 *****************************************************************************/
static IMG_VOID ProcessNodeFUNCTIONCALL(GLSLCompilerPrivateData *psCPD, 
										GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcess)
{
	GLSLFunctionCallData		*psFunctionCallData;
	GLSLFunctionDefinitionData	*psFunctionDefinitionData;
	GLSLICOperandInfo			 sDestOperand;
	GLSLICOperandInfo			*psDestOperand = &sDestOperand;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	psFunctionCallData = 
		(GLSLFunctionCallData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessNodeFUNCTIONCALL: Failed to get symbol Data for ID: %u\n", psNode->uSymbolTableID));
		return;
	}
	DebugAssert(psFunctionCallData->eSymbolTableDataType == GLSLSTDT_FUNCTION_CALL);

	/* Get Function Definition data */
	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionCallData->uFunctionDefinitionSymbolID);
	if(psFunctionDefinitionData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessNodeFUNCTIONCALL: Failed to get symbol Data for ID: %u\n", 
			psFunctionCallData->uFunctionDefinitionSymbolID));
		return;
	}

	/* 
	** If the parent is assignment and at the stage of front end node process, 
	** we want to delay the code generation until assignement node is visited (GLSLNT_EQUAL)
	** For user defined function, we treat it differently and never defer the function call code 
	*/
	if(!bPostProcess && IS_PARENT_ASSIGNMENT(psNode) && (psFunctionDefinitionData->eFunctionType != GLSLFT_USER))
	{
		return;
	}
	
	/* DEST Operand */
	if(bPostProcess)
	{
		/* At the post process stage, take the left node of assignment as the destination operand */

		GLSLNode *psDest;

		DebugAssert(IS_PARENT_ASSIGNMENT(psNode));
		DebugAssert(psNode->bEvaluated == IMG_FALSE);
		
		psDest = GET_PARENT_ASSIGNMENT_LEFT(psNode);

		ICProcessNodeOperand(psCPD, psICProgram, psDest, psDestOperand);
	}
	else
	{
		/* Not at post stage, take the current node as destination operand */
		
		if( psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID &&
			psFunctionDefinitionData->eFunctionType != GLSLFT_USER)
		{
			/* For constructors and builtin functions only */

			ICInitOperandInfo(psNode->uSymbolTableID, psDestOperand);

			/* Indicate the Symbol ID of this node is ready to use (as source operand) */
			psNode->bEvaluated = IMG_TRUE;
		}
		else
		{
			psDestOperand = IMG_NULL;
		}
	}

	switch(psFunctionDefinitionData->eFunctionType)
	{
		case GLSLFT_USER:
			DebugAssert(!bPostProcess);
			ICProcessUserDefinedFunctionCall(psCPD, psICProgram, psNode);
			break;
		case GLSLFT_CONSTRUCTOR:
			ICProcessConstructor(psCPD, psICProgram, psNode, psDestOperand);
			break;
		case GLSLFT_USERDEFINED_CONSTRUCTOR:
			ICProcessSturctureConstructor(psCPD, psICProgram, psNode, psDestOperand);
			break;
		case GLSLFT_BUILT_IN:
			ICProcessBuiltInFunctionCall(psCPD, psICProgram, psNode, psDestOperand);
			break;
		default:
			LOG_INTERNAL_ERROR(("ProcessNodeFUNCTIONCALL: Unknown function type in the function definition \n"));
			break;
	}

	if(psDestOperand)
	{
		ICFreeOperandOffsetList(psDestOperand);
	}
}

#if defined(GLSL_ES)
/******************************************************************************
 * Function Name: ICInsertCodeForGLPointSize
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate IC for clamping of gl_PointSize to OGLES2_POINT_SIZE_MAX
 *****************************************************************************/
static IMG_VOID ICInsertCodeForGLPointSize(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	
	/* The code will depends on whether shader writes to point size or not.

	1) if a shader doesn't written to gl_PointSize, then nothing will be done

	2) if a shader has written to gl_PointSize, then the code will be
	
		MIN gl_PointSize gl_PointSize, OGLES2_POINT_SIZE_MAX
					;  gl_PointSize = min(gl_PointSize, OGLES2_POINT_SIZE_MAX)
	*/
	
	if(psICContext->eBuiltInsWrittenTo & GLSLBVWT_POINTSIZE)
	{
		IMG_UINT32 uGLPointSizeID, uMaxPointSizeID;
		
		GLSLPrecisionQualifier ePointSizePrecision = psICContext->psInitCompilerContext->sRequestedPrecisions.eGLPointSize;
		
		/* Retrive gl_PointSize */
		uGLPointSizeID = ICFindBuiltInVariables(psCPD, psICProgram, "gl_PointSize");
		if(uGLPointSizeID == 0)
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to find gl_PointSize !\n"));
			return;
		}
		
		/* Append the clamping constant to symbol table */
		if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, OGLES2_POINT_SIZE_MAX, ePointSizePrecision, IMG_TRUE, &uMaxPointSizeID))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to add float constant -511.0f to the symbol table\n"));
			return;
		}
		
		/* Append the clamping instruction for maximum Point Size */
		/*==========================================================
			MIN gl_PointSize gl_PointSize OGLES2_POINT_SIZE_MAX
		==========================================================*/
		ICAddICInstruction3d(psCPD, psICProgram,
									GLSLIC_OP_MIN,
									IMG_NULL,
									uGLPointSizeID,
									uGLPointSizeID,
									uMaxPointSizeID);
	}
}
#else /* defined(GLSL_ES) */
/******************************************************************************
 * Function Name: ICInsertCodeForGLPointSize
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate IC for built-in functions	
 *****************************************************************************/
static IMG_VOID ICInsertCodeForGLPointSize(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{

	IMG_UINT32 uGLPointSizeID, uOneID, uPMXPointSizeEnableID, uPMXPointSizeID;

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	GLSLPrecisionQualifier ePointSizePrecision = psICContext->psInitCompilerContext->sRequestedPrecisions.eGLPointSize;
	GLSLPrecisionQualifier eEnablePrecision = BOOLEAN_PRECISION(psICProgram); 

	/* The code will depends on whether shader writes to point size or not.
	
		1) if a shader doesn't written to gl_PointSize, the i code will be

			MOV gl_PointSize gl_PMXPointSize;

		2) if a shader has written to gl_PointSize, then the code will be
		
			IFNOT gl_PMXPointSizeEnable
				MOV gl_PointSize gl_PMXPointSize
			ENDIF
	*/

	/* Add gl_PMXPointSizeEnable to the symbol table */
	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 "gl_PMXPointSizeEnable",
							 0,
							 GLSLBV_PMXPOINTSIZEENABLE,
							 GLSLTS_BOOL,
							 GLSLTQ_UNIFORM,
							 eEnablePrecision,
							 &uPMXPointSizeEnableID))
	{
		LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to add PMX builtin variable gl_PMXPointSizeEnable to the symbol table !\n"));
		return;
	}

	/* Add gl_PMXPointSize to the symbol table */
	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 "gl_PMXPointSize",
							 0,
							 GLSLBV_PMXPOINTSIZE,
							 GLSLTS_FLOAT,
							 GLSLTQ_UNIFORM,
							 ePointSizePrecision,
							 &uPMXPointSizeID))
	{
		LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to add PMX builtin variable gl_PMXPointSize to the symbol table !\n"));
		return;
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, ePointSizePrecision, IMG_TRUE, &uOneID))
	{
		LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to add float constant 0.0 to the table !\n"));
		return;
	}

	uGLPointSizeID = ICFindBuiltInVariables(psCPD, psICProgram, "gl_PointSize");
	if(uGLPointSizeID == 0)
	{
		LOG_INTERNAL_ERROR(("ICInsertCodeForGLPointSize: Failed to find gl_PointSize !\n"));
		return;
	}

	if(!(psICContext->eBuiltInsWrittenTo & GLSLBVWT_POINTSIZE))
	{
		/*=======================================
			MOV gl_PointSize gl_PMXPointSize 			
		=======================================*/
		ICAddICInstruction2b(psCPD, psICProgram, 
							 GLSLIC_OP_MOV,
							 NULL,
							 uGLPointSizeID,
							 uPMXPointSizeID);
	}
	else
	{
		/* IFNOT gl_PMXPointSizeEanable */
		ICAddICInstruction1a(psCPD, psICProgram, GLSLIC_OP_IFNOT, NULL, uPMXPointSizeEnableID);
	
			/*=======================================
				MOV gl_PointSize gl_PMXPointSize 			
			=======================================*/
			ICAddICInstruction2b(psCPD, psICProgram,
								 GLSLIC_OP_MOV,
								 NULL,
								 uGLPointSizeID,
								 uPMXPointSizeID);
	
		/* ENDIF */
		ADD_ICODE_INSTRUCTION_ENDIF(psCPD, psICProgram, IMG_NULL);
	}
}
#endif /* defined(GLSL_ES) */

#if 0 /* Color clamp is not supported */
/******************************************************************************
 * Function Name: ICClampColor
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : clamp color
 *****************************************************************************/
static IMG_VOID ICClampColor(GLSLCompilerPrivateData *psCPD, 
							 GLSLICProgram *psICProgram,
							IMG_UINT32 uColorSymID,
							IMG_UINT32 uMinValueSymID,
							IMG_UINT32 uMaxValueSymID)
{
	/* IC: MAX color color min */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MAX,
						 IMG_NULL,
						 uColorSymID,
						 uColorSymID,
						 uMinValueSymID);

	/* IC: MIN color color max */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MIN,
						 IMG_NULL,
						 uColorSymID,
						 uColorSymID,
						 uMaxValueSymID);

}

/******************************************************************************
 * Function Name: ICInsertCodeClampColors
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Check any built-in color has been written to, if so insert clamp
 *****************************************************************************/
static IMG_VOID ICInsertCodeClampColors(GLSLCompilerPrivateData *psCPD, 
										GLSLICProgram *psICProgram)
{
	IMG_UINT32 c0 = 0, c1 = 0, color;

	GLSLPrecisionQualifier ePrecision = DEFAULT_FLOAT_PRECISION(psICProgram);

	if( psICProgram->eProgramType == GLSLPT_FRAGMENT || 
		psICProgram->eBuiltInsWrittenTo & 
		(GLSLBVWT_FRONTCOLOR|GLSLBVWT_BACKCOLOR|GLSLBVWT_FRONTSECONDARYCOLOR|GLSLBVWT_BACKSECONDARYCOLOR) )

	{
		if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
								 "gl_PMXColourClampMin",
								 0,
								 GLSLBV_PMXCOLOURCLAMPMIN,
								 GLSLTS_FLOAT,
								 GLSLTQ_UNIFORM,
								 ePrecision,
								 &c0))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeClampColors: Failed to add gl_PMXColourClampMin to the symbol table\n"));
			return ;
		}

		if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
								 "gl_PMXColourClampMax",
								 0,
								 GLSLBV_PMXCOLOURCLAMPMAX,
								 GLSLTS_FLOAT,
								 GLSLTQ_UNIFORM,
								 ePrecision,
								 &c1))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeClampColors: Failed to add gl_PMXColourClampMax to the symbol table\n"));
			return ;
		}
	}

	if(psICProgram->eProgramType == GLSLPT_VERTEX)
	{
		/* gl_FrontColor */
		if(psICProgram->eBuiltInsWrittenTo & GLSLBVWT_FRONTCOLOR)
		{
			color = ICFindBuiltInVariables(psCPD, psICProgram, "gl_FrontColor");

			ICClampColor(psICProgram, color, c0, c1);
		}

		/* gl_FrontColor */
		if(psICProgram->eBuiltInsWrittenTo & GLSLBVWT_BACKCOLOR)
		{
			color = ICFindBuiltInVariables(psCPD, psICProgram, "gl_BackColor");

			ICClampColor(psICProgram, color, c0, c1);
		}


		/* gl_FrontSecondaryColor */
		if(psICProgram->eBuiltInsWrittenTo & GLSLBVWT_FRONTSECONDARYCOLOR)
		{
			color = ICFindBuiltInVariables(psCPD, psICProgram, "gl_FrontSecondaryColor");

			ICClampColor(psICProgram, color, c0, c1);
		}

		/* gl_BackSecondaryColor */
		if(psICProgram->eBuiltInsWrittenTo & GLSLBVWT_BACKSECONDARYCOLOR)
		{
			color = ICFindBuiltInVariables(psCPD, psICProgram, "gl_BackSecondaryColor");

			ICClampColor(psICProgram, color, c0, c1);
		}
	}
	else
	{
		color = ICFindBuiltInVariables(psCPD, psICProgram, "gl_FragColor");

		ICClampColor(psICProgram, color, c0, c1);
	}
}

#endif

/******************************************************************************
 * Function Name: ICPostProcessOnExitVP
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : extra code added on exit
 *****************************************************************************/
static IMG_VOID ICPostProcessOnExitVP(GLSLCompilerPrivateData *psCPD, 
									  GLSLICProgram *psICProgram)
{
#if !defined(GLSL_ES)
	ICInsertCodeForGLPointSize(psCPD, psICProgram);

#if 0 /* Color clamp is not supported */
#if !USE_INTERGER_COLORS
	ICInsertCodeClampColors(psICProgram);
#endif
#endif

	ICInsertCodeForFogFactor(psCPD, psICProgram);
#else
	ICInsertCodeForGLPointSize(psCPD, psICProgram);
#endif /* !defined GLSL_ES */

#if defined(GLSL_ES)
	PVR_UNREFERENCED_PARAMETER(psCPD);
	PVR_UNREFERENCED_PARAMETER(psICProgram);
#endif
}


#if 0 /* No color clamp */
/******************************************************************************
 * Function Name: ICPostProcessOnExitFP
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : extra code added on exit
 *****************************************************************************/
static IMG_VOID ICPostProcessOnExitFP(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	ICInsertCodeClampColors(psCPD, psICProgram);
}

#endif

/******************************************************************************
 * Function Name: ICPostProcessOnExit
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : extra code added on exit
 *****************************************************************************/

static IMG_VOID ICPostProcessOnExit(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if(psICContext->eProgramType == GLSLPT_VERTEX)
	{
		ICPostProcessOnExitVP(psCPD, psICProgram);
	}
#if 0 /* Color clamp is not supported */
	else
	{
		ICPostProcessOnExitFP(psCPD, psICProgram);
	}
#endif
}
/******************************************************************************
 * Function Name: ProcessNodeFUNCTIONDEFINITION
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type:	GLSLNT_FUNCTIONDEFINITION
 *****************************************************************************/
static IMG_VOID ProcessNodeFUNCTIONDEFINITION(GLSLCompilerPrivateData *psCPD, 
											  GLSLICProgram		*psICProgram,
											  GLSLNode			*psNode)
{

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32					i;
	GLSLFunctionDefinitionData	*psFunctionDefinitionData;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psICContext->bHadReturn = IMG_FALSE;

	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionDefinitionData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessNodeFUNCTIONDEFINITION: Failed to get symbol data for ID:%u\n", psNode->uSymbolTableID));
		return;
	}

	/* only issue LABEL if it is not main function */
	if(!psICContext->bMainFunction)
	{
		if(!psFunctionDefinitionData->uFunctionCalledCount)
		{
			return;
		}

		ICAddICInstruction1a(psCPD, psICProgram, GLSLIC_OP_LABEL, pszLineStart, psNode->uSymbolTableID);
	}

	/* Generate code for function body */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICTraverseAST(psCPD, psICProgram, psNode->ppsChildren[i]);
	}

	if(psICContext->bMainFunction)
	{
		ICPostProcessOnExit(psCPD, psICProgram);
	}

	if(!psICContext->bHadReturn)
	{
		/* Add RET op only if the last instruction is not an RET op */
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_RET, IMG_NULL);
	}

	psICContext->bMainFunction = IMG_FALSE;
}

/******************************************************************************
 * Function Name: GetInstructionsCount
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Count the number of instructions
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 GetInstructionsCount(GLSLICInstruction *psStart, GLSLICInstruction *psEnd)
{
	IMG_UINT32 uNumInstrs = 0;
	GLSLICInstruction *psInstr;

	psInstr = psStart;

	while(psInstr)
	{
		uNumInstrs++;

		if(psInstr == psEnd) break;
	
		psInstr = psInstr->psNext;
	}

	return uNumInstrs;
}

/******************************************************************************
 * Function Name: GetTypeNumMembers
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Count the number of memebers for a specific type specifier 
 *				: Any basic types such as vec4, matrix, int etc, count 1
 *				: For array, multiply its size.
 *				: For usder defined struct, count the number of members with basic types.
 *****************************************************************************/
static IMG_UINT32 GetTypeNumElements(GLSLCompilerPrivateData *psCPD, 
									 GLSLICProgram *psICProgram, GLSLFullySpecifiedType *psFullType)
{
	IMG_UINT32 uNumElements = 0, i;
	if(GLSL_IS_STRUCT(psFullType->eTypeSpecifier))
	{
		GLSLStructureDefinitionData *psStructureDefinitionData 
			= GetSymbolTableData(psICProgram->psSymbolTable, psFullType->uStructDescSymbolTableID);

		if(psStructureDefinitionData == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("GetTypeNumMembers: Failed to get structure definition data \n"));
			return 0;
		}

		for(i = 0; i < psStructureDefinitionData->uNumMembers; i++)
		{
			uNumElements += GetTypeNumElements(psCPD, psICProgram, &psStructureDefinitionData->psMembers[i].sIdentifierData.sFullySpecifiedType);
		}
	}
	else
	{
		uNumElements = TYPESPECIFIER_NUM_ELEMENTS(psFullType->eTypeSpecifier);
	}

	return uNumElements;
}

/******************************************************************************
 * Function Name: IsFunctionToBeInlined
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Is function to be inlined? 
 *****************************************************************************/
static IMG_BOOL DoesFunctionConstainEarlyReturns(GLSLICInstruction *psCodeStart, GLSLICInstruction *psCodeEnd)
{
	GLSLICInstruction *psInstr = psCodeStart;

	while(psInstr != psCodeEnd)
	{
		if(psInstr->eOpCode == GLSLIC_OP_RET)
		{
			return IMG_TRUE;
		}

		psInstr = psInstr->psNext;
	}

	return IMG_FALSE;
}

/******************************************************************************
 * Function Name: IsFunctionToBeInlined
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Is function to be inlined? 
 *****************************************************************************/
static IMG_BOOL IsFunctionToBeInlined(GLSLCompilerPrivateData *psCPD, 
									  GLSLICProgram				*psICProgram,
										GLSLFunctionDefinitionData	*psFunctionDefinitionData,
										GLSLICInstruction			*psCodeStart,
										GLSLICInstruction			*psCodeEnd)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLInlineFuncRules *psInlineFuncRules = &psICContext->psInitCompilerContext->sInlineFuncRules;

	/*
		Rules for inline

		1) If a function is called only once
		2) If a function contains less than the max number of IC instructions defined
		3) If a function use one or more samplers as paramenters.
		4) If the total number of paramenter members is greater than 8 
			( this will incur in multiple copies before executing the function body so...)

	*/
	IMG_UINT32 uNumInstructions;
	IMG_UINT32 uNumElements = 0, i, uNumElementsPerParam;

	/* The function is never called, of course is not inlined */
	if(psFunctionDefinitionData->uFunctionCalledCount == 0)
	{
		return IMG_FALSE;
	}

	/* If a function constains early returns, we can not inline it */
	if(DoesFunctionConstainEarlyReturns(psCodeStart, psCodeEnd))
	{
		return IMG_FALSE;
	}

	/* Override default rules if always-always flag is set. */
	if(psInlineFuncRules->bInlineAlwaysAlways)
	{
		return IMG_TRUE;
	}

	/* 1) If a function is called only once */
	if(psFunctionDefinitionData->uFunctionCalledCount == 1 && psInlineFuncRules->bInlineCalledOnceFunc)
	{
		return IMG_TRUE;
	}
	
	/* 2) If a function contains less than the max number of IC instructions defined */
	uNumInstructions = GetInstructionsCount(psCodeStart, psCodeEnd);
	if(uNumInstructions < psInlineFuncRules->uNumICInstrsBodyLessThan)
	{
		return IMG_TRUE;
	}
		
	for(i = 0; i < psFunctionDefinitionData->uNumParameters; i++)
	{
		GLSLIdentifierData *psIdentifierData = GetSymbolTableData(psICProgram->psSymbolTable,
																  psFunctionDefinitionData->puParameterSymbolTableIDs[i]);
		if(psIdentifierData == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("IsFunctionToBeInlined: Failed to get symbol identifier data !\n"));
			return IMG_FALSE;
		}

		/* 3) If a function use one or more samplers as paramenters. */
		if(GLSL_IS_SAMPLER(psIdentifierData->sFullySpecifiedType.eTypeSpecifier))
		{
			if(psInlineFuncRules->bInlineSamplerParamFunc)
			{
				return IMG_TRUE;
			}
			else
			{
				LOG_INTERNAL_ERROR(("IsFunctionToBeInlined: bInlineSamplerParamFunc should set true\n"));
				LOG_INTERNAL_ERROR(("IsFunctionToBeInlined: Force to inline if function has sampler parameters\n"));
				return IMG_TRUE;
			}
		}

		uNumElementsPerParam = GetTypeNumElements(psCPD, psICProgram, &psIdentifierData->sFullySpecifiedType);

		if(psIdentifierData->eArrayStatus != GLSLAS_NOT_ARRAY)
		{
			uNumElementsPerParam *= psIdentifierData->iActiveArraySize; 
		}

		uNumElements += uNumElementsPerParam;
	}
	

	/* 4) If the total number of paramenter members is greater than we defined  */
	if(uNumElements > psInlineFuncRules->uNumParamComponentsGreaterThan) return IMG_TRUE;
	
	return IMG_FALSE;	
}


/******************************************************************************
 * Function Name: ProcessNodeSHADER
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NodeType: GLSLNT_SHADER
 *****************************************************************************/
static IMG_VOID ProcessNodeSHADER(GLSLCompilerPrivateData *psCPD, 
								  GLSLICProgram *psICProgram, GLSLNode *psNode)
{

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode	*psChild;
	IMG_UINT32	i;
	GLSLICInstruction *psInsertMainAfter = IMG_NULL;
	GLSLICInstruction *psMainStart = IMG_NULL, *psMainEnd = IMG_NULL;
	GLSLICShaderChild *psShaderChild;
	IMG_BOOL		   bMainFunction;

	psICContext->uNumShaderChildren = psNode->uNumChildren;
	psICContext->psShaderChildren = DebugMemCalloc(psNode->uNumChildren * sizeof(GLSLICShaderChild));
	if(psICContext->psShaderChildren == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ProcessNodeSHADER: Failed to get memory for shader children information"));
		return;
	}

	/* Process all declarations before function definitions*/
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		bMainFunction = IMG_FALSE;

		psChild = psNode->ppsChildren[i];

		psShaderChild = &psICContext->psShaderChildren[i];

		psShaderChild->psCodeStart = psICProgram->psInstrTail;

		if(psChild->eNodeType == GLSLNT_DECLARATION)
		{
			ICTraverseAST(psCPD, psICProgram, psChild);
			psInsertMainAfter = psICProgram->psInstrTail;
			
			psShaderChild->bDeclaration = IMG_TRUE;
		}
		else if(psChild->eNodeType == GLSLNT_FUNCTION_DEFINITION)
		{
			IMG_CHAR *psFuncName;	
			psFuncName = GetSymbolName(psICProgram->psSymbolTable, psChild->uSymbolTableID);

			if(!strcmp(psFuncName, "fn_main@"))
			{
				/* The main function is always the last child */
				DebugAssert((i == psNode->uNumChildren - 1));

				bMainFunction = IMG_TRUE;
				psICContext->bMainFunction = bMainFunction;
			}

			ProcessNodeFUNCTIONDEFINITION(psCPD, psICProgram, psChild);

			psShaderChild->uFunctionDefinitionID = psChild->uSymbolTableID;
		}


		/* Keep a record of code start and code end for the current shader child */
		psShaderChild->psCodeEnd = psICProgram->psInstrTail;
		if(psShaderChild->psCodeStart == psShaderChild->psCodeEnd) 
		{
			psShaderChild->psCodeStart = psShaderChild->psCodeEnd = IMG_NULL;
		}
		else if(psShaderChild->psCodeStart)
		{
			psShaderChild->psCodeStart = psShaderChild->psCodeStart->psNext;
		}
		else
		{
			psShaderChild->psCodeStart = psICProgram->psInstrHead;
		}

		if(psShaderChild->psCodeStart) psShaderChild->bCodeGenerated = IMG_TRUE;

		/* If it is not a declaration */
		if(!bMainFunction && !psShaderChild->bDeclaration && psShaderChild->bCodeGenerated)
		{
			GLSLFunctionDefinitionData	*psFunctionDefinitionData;
			psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psChild->uSymbolTableID);

			if(psFunctionDefinitionData == IMG_NULL)
			{
				LOG_INTERNAL_ERROR(("ProcessNodeSHADER: Failed to get symbol data for ID:%u\n", psNode->uSymbolTableID));
				return;
			}

			psShaderChild->bToBeInlined = IsFunctionToBeInlined(psCPD, psICProgram, psFunctionDefinitionData, psShaderChild->psCodeStart, psShaderChild->psCodeEnd);

			if(psShaderChild->bToBeInlined)
			{
				if(psShaderChild->psCodeStart->psNext == psShaderChild->psCodeEnd)
				{
					psShaderChild->bEmptyBody = IMG_TRUE;
				}
				else
				{
					/* If it is to be inlined, remove the first (LABEL) and last(RET) instructions */
					GLSLICInstruction *psNewStart = psShaderChild->psCodeStart->psNext;
					GLSLICInstruction *psNewEnd = psShaderChild->psCodeEnd->psPrev;	

					DebugAssert(psShaderChild->psCodeStart->eOpCode == GLSLIC_OP_LABEL);
					DebugAssert(psShaderChild->psCodeEnd->eOpCode == GLSLIC_OP_RET);

					ICRemoveInstruction(psICProgram, psShaderChild->psCodeStart);
					ICRemoveInstruction(psICProgram, psShaderChild->psCodeEnd);

					psShaderChild->psCodeStart = psNewStart;
					psShaderChild->psCodeEnd = psNewEnd;
				}
			}
		}


	}

	for(i = 0; i < psNode->uNumChildren; i++)
	{
		if(psICContext->psShaderChildren[i].bToBeInlined)
		{
			ICRemoveInstructionRange(psICProgram, psICContext->psShaderChildren[i].psCodeStart,
									 psICContext->psShaderChildren[i].psCodeEnd);
		}
	}

	/* Main function is always the last child */
	psMainStart = psICContext->psShaderChildren[psNode->uNumChildren-1].psCodeStart;
	psMainEnd = psICContext->psShaderChildren[psNode->uNumChildren-1].psCodeEnd;
	DebugAssert(psMainEnd == psICProgram->psInstrTail);

	/* Move the main funcition to after the declaration */
	UnhookInstructions(psICProgram, psMainStart, psMainEnd);
	InsertInstructionsAfter(psICProgram, psInsertMainAfter, psMainStart, psMainEnd);

	/* Free the memory */
	DebugMemFree(psICContext->psShaderChildren);
	psICContext->psShaderChildren = IMG_NULL;

}

/******************************************************************************
 * Function Name: ProcessNodeRETURN
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NodeType: GLSLNT_RETURN
 *****************************************************************************/
static IMG_VOID ProcessNodeRETURN(GLSLCompilerPrivateData *psCPD, 
								  GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *psChild = IMG_NULL;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	if(psNode->uNumChildren) psChild = psNode->ppsChildren[0];

	if(psChild)
	{
		ADD_ICODE_INSTRUCTION_MOV(psCPD, psICProgram, psNode, psChild, pszLineStart);

		/* Indicate the Symbol ID of this node is ready to use. */
		psNode->bEvaluated = IMG_TRUE;
	}

	if(psICContext->bMainFunction)
	{
		ICPostProcessOnExit(psCPD, psICProgram);
	}

	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_RET, pszLineStart);

	if(psICContext->iConditionLevel == 0) 
	{
		psICContext->bHadReturn = IMG_TRUE;
	}
}

/******************************************************************************
 * Function Name: ProcessNodeDISCARD
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : NodeType: GLSLNT_DISCARD
 *****************************************************************************/
static IMG_VOID ProcessNodeDISCARD(GLSLCompilerPrivateData *psCPD, 
								   GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_DISCARD, pszLineStart);
}

/******************************************************************************
 * Function Name: ProcessNode
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Process the current node and generate IC instructions.
 *				  A node might be visited twice. bPostProcess is true on 
 *				  second visit.
 *****************************************************************************/
static IMG_VOID ProcessNode(GLSLCompilerPrivateData *psCPD, 
							GLSLICProgram *psICProgram, GLSLNode *psNode, IMG_BOOL bPostProcess)
{
	switch(psNode->eNodeType)
	{
	case GLSLNT_IDENTIFIER:
		/* Indicate this node has been evaluated, the Symbol ID of this node is ready to use. */
		psNode->bEvaluated = IMG_TRUE;
		break;
	case GLSLNT_FIELD_SELECTION:
		break;
	case GLSLNT_ARRAY_SPECIFIER:
		break;
	case GLSLNT_POST_INC:
	case GLSLNT_POST_DEC:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodePostIncDec(psICProgram, psNode);
		break;
	case GLSLNT_FUNCTION_CALL:
		ProcessNodeFUNCTIONCALL(psCPD, psICProgram, psNode, bPostProcess);
		break;
	case GLSLNT_PRE_INC:
	case GLSLNT_PRE_DEC:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodePreIncDec(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_NEGATE:
		break;
	case GLSLNT_POSITIVE:
		break;
	case GLSLNT_NOT:
		ProcessNodeNOT(psCPD, psICProgram, psNode, bPostProcess);
		break;
	case GLSLNT_MULTIPLY:
	case GLSLNT_DIVIDE:
	case GLSLNT_ADD:
	case GLSLNT_SUBTRACT:
		ProcessNodeBasicOp(psCPD, psICProgram, psNode, bPostProcess);
		break;

	case GLSLNT_LESS_THAN:
	case GLSLNT_GREATER_THAN:
	case GLSLNT_LESS_THAN_EQUAL:
	case GLSLNT_GREATER_THAN_EQUAL:
	case GLSLNT_EQUAL_TO:
	case GLSLNT_NOTEQUAL_TO:
		ProcessNodeComparisonOp(psCPD, psICProgram, psNode, bPostProcess);
		break;

	case GLSLNT_LOGICAL_OR:
	case GLSLNT_LOGICAL_XOR:
	case GLSLNT_LOGICAL_AND:
		LOG_INTERNAL_ERROR(("ProcessNode: node LOGICAL_OR, LOGICAL_XOR, LOGICAL_AND have been processed\n"));
		break;
	case GLSLNT_QUESTION:
		LOG_INTERNAL_ERROR(("ProcessNode: node QUESTION has been processed\n"));
		break;
	case GLSLNT_EQUAL:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeEQUAL(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_MUL_ASSIGN:
	case GLSLNT_DIV_ASSIGN:
	case GLSLNT_ADD_ASSIGN:
	case GLSLNT_SUB_ASSIGN:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeBasicOpAssign(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_CONTINUE:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeCONTINUE(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_BREAK:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeBREAK(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_RETURN:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeRETURN(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_DISCARD:
		/* Always generate code at the front end */
		DebugAssert(!bPostProcess);
		ProcessNodeDISCARD(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_FOR:
	case GLSLNT_WHILE:
	case GLSLNT_DO:
	case GLSLNT_IF:
		LOG_INTERNAL_ERROR(("ProcessNode: FOR, WHILE, DO, IF have been processed \n"));
		break;
	case GLSLNT_EXPRESSION:
		ProcessNodeEXPRESSION(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_SUBEXPRESSION:
		ProcessNodeSUBEXPRESSION(psCPD, psICProgram, psNode);
		break;
	case GLSLNT_STATEMENT_LIST:
		break;
	case GLSLNT_DECLARATION:
		break;
	case GLSLNT_FUNCTION_DEFINITION:
		LOG_INTERNAL_ERROR(("ProcessNode: FUNCTION_DEFINITION has been processed \n"));
		break;
	case GLSLNT_SHADER:
		LOG_INTERNAL_ERROR(("ProcessNode: FUNCTION_SHADER has been processed \n"));
		break;
	default:
		LOG_INTERNAL_NODE_ERROR((psNode,"ProcessNode: Unknown node type, %u", psNode->eNodeType));
		break;
	}
}

/* Return true if there is no modifier at all the the operand*/
static IMG_BOOL IsSimpleOperand(GLSLICOperandInfo *psOperandInfo)
{
	if( !psOperandInfo->sSwizWMask.uNumComponents && 
		!psOperandInfo->eInstModifier &&
		!psOperandInfo->psOffsetList)
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}



static IMG_VOID AddICodeLOOP(GLSLCompilerPrivateData *psCPD, 
							 GLSLICProgram *psICProgram, IMG_UINT32 uPredicateBoolSymbolID, IMG_BOOL bStartLoop, IMG_CHAR *psLineStart)
{

	GLSLICInstruction *psICInstr = ICGetNewInstruction(psCPD, psICProgram);

	psICInstr->uPredicateBoolSymID = uPredicateBoolSymbolID;

	psICInstr->eOpCode = bStartLoop ? GLSLIC_OP_LOOP : GLSLIC_OP_ENDLOOP;

	psICInstr->pszOriginalLine = psLineStart;

	ValidateICInstruction(psCPD, psICProgram, psICInstr);
}



#if 0
/******************************************************************************
 * Function Name: ProcessLoopTestCondition
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate code for loop condition and add test to break
 *****************************************************************************/
static IMG_VOID ProcessLoopTestCondition(GLSLICProgram *psICProgram, GLSLNode *psCondition)
{
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psCondition); 
	GLSLNode *psCompareLeft, *psCompareRight;
	GLSLNodeType eCompareNodeType;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psCondition);

	/* IF: generate IF code */
	if(IsSimpleConditionCheck(psCPD, psICProgram, psCondition, &eCompareNodeType, &psCompareLeft, &psCompareRight))
	{
		/* Simple case: one condition check, comparing two scalars */
		GenerateOptimisedConditionCode(psCPD, psICProgram, eCompareNodeType, psCompareLeft, psCompareRight, IMG_TRUE);
	}
	else
	{
		/* Generate code for condition expression */
		ICTraverseAST(psCPD, psICProgram, psCondition);

		/* IF (!psCondition) */
		ADD_ICODE_INSTRUCTION_IFNOT(psCPD, psICProgram, psCondition, psLineStart);
	}

		/* BREAK */
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_BREAK, psLineStart);

	/* ENDIF */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ENDIF, psLineStart);
}
#endif

/******************************************************************************
 * Function Name: ProcessNodeFOR
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type : GLSLNT_FOR, we might need to unroll the loop
 *****************************************************************************/
static IMG_VOID ProcessNodeFOR(GLSLCompilerPrivateData *psCPD, 
							   GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psInit, *psCondition, *psIncr, *psStatement;
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_BOOL bConstantCondition, bData = IMG_FALSE;
	GLSLICInstruction *psInitStart = IMG_NULL, *psInitEnd = IMG_NULL;
	GLSLICInstruction *psCondStart = IMG_NULL, *psCondEnd = IMG_NULL;
	GLSLICInstruction *psUpdateStart = IMG_NULL, *psUpdateEnd = IMG_NULL;
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_BOOL bUnrollLoop = psICContext->psInitCompilerContext->sUnrollLoopRules.bEnableUnroll;
	GLSLICInstruction *psDummyStart, *psDummyEnd;
	IMG_UINT32 uPredResultSymID = 0;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Increase for loop level */
	psICContext->iForLoopLevel++;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get init, codition, loop and statement nodes */
	DebugAssert(psNode->uNumChildren == 4);
	psInit		= psNode->ppsChildren[0];
	psCondition = psNode->ppsChildren[1];
	psIncr		= psNode->ppsChildren[2];
	psStatement = psNode->ppsChildren[3];

	/* Check if empty condition */
	if(psCondition)
	{
		bConstantCondition = 
			IsSymbolBoolConstant(psCPD, psICProgram->psSymbolTable, psCondition->uSymbolTableID, &bData);
	}
	else
	{
		bConstantCondition = IMG_TRUE;
		bData = IMG_TRUE;	
	}

	/* If constant condition and it is false, dont need to generate any instructions */
	if(bConstantCondition && !bData) return;
		
	/* init expression */
	psInitStart = psICProgram->psInstrTail;
	ICTraverseAST(psCPD, psICProgram, psInit);
	psInitEnd = psICProgram->psInstrTail;
	if(psInitStart)
	{
		psInitStart = psInitStart->psNext;
	}
	else
	{
		psInitStart = psICProgram->psInstrHead;
	}

	if(!bConstantCondition || bData)
	{
		if(!bConstantCondition)
		{
			/* Generate condition code */
			psCondStart = psICProgram->psInstrTail;
			
			ICTraverseCondition(psCPD, psICProgram, psCondition, &uPredResultSymID);

			psCondEnd = psICProgram->psInstrTail;
			psCondStart = psCondStart->psNext;
		}

		/* Increment condition level */
		IncrementConditionLevel(psICProgram);

		/* LOOP condition */
		ADD_ICODE_INSTRUCTION_LOOP(psCPD, psICProgram, uPredResultSymID, psLineStart);
	
			/* statement list */
			ICTraverseAST(psCPD, psICProgram, psStatement);
		
		/* CONTDEST */
		
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_CONTDEST, psLineStart);

			/* Loop update expression */
			psUpdateStart = psICProgram->psInstrTail;
			ICTraverseAST(psCPD, psICProgram, psIncr);
			psUpdateEnd = psICProgram->psInstrTail;
			psUpdateStart = psUpdateStart->psNext;

			if(!bConstantCondition)
			{
				/* Generate condition code again */
				CloneICodeInstructions(psCPD, psICProgram, psCondStart, psCondEnd, &psDummyStart, &psDummyEnd);
			}

		/* ENDLOOP condition */
		ADD_ICODE_INSTRUCTION_ENDLOOP(psCPD, psICProgram, uPredResultSymID, psLineStart);

		/* Decrement condition level */
		DecrementConditionLevel(psICProgram);
	}
	else
	{
		bUnrollLoop = IMG_FALSE;
	}

	/* Check whether it is a static loop, and whether able to unroll, if so, unroll it or mark it static */
	ICUnrollLoopFOR(psCPD, psICProgram, bUnrollLoop, psInitStart, psInitEnd, psCondStart, psCondEnd, psUpdateStart, psUpdateEnd);

	/* Decrease for loop level */
	psICContext->iForLoopLevel--;
}

/******************************************************************************
 * Function Name: ProcessNodeWHILE
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type: GLSLNT_WHILE
 *****************************************************************************/
static IMG_VOID ProcessNodeWHILE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psCondition, *psStatement;
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_BOOL bConstantCondition, bData = IMG_FALSE;
	IMG_UINT32 uPredResultSymID = 0;
	GLSLICInstruction *psCondStart = IMG_NULL, *psCondEnd = IMG_NULL, *psDummyStart, *psDummyEnd;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get condition and statement nodes */
	DebugAssert(psNode->uNumChildren == 2);
	psCondition = psNode->ppsChildren[0];
	psStatement = psNode->ppsChildren[1];

	if(psCondition)
	{
		bConstantCondition = 
			IsSymbolBoolConstant(psCPD, psICProgram->psSymbolTable, psCondition->uSymbolTableID, &bData);
	}
	else
	{
		bConstantCondition = IMG_TRUE;
		bData = IMG_TRUE;	
	}

	if(!bConstantCondition || bData)
	{
		/* Increment condition level */
		IncrementConditionLevel(psICProgram);

		if(!bConstantCondition)
		{
			/* Generate condition code */
			psCondStart = psICProgram->psInstrTail;
			
			ICTraverseCondition(psCPD, psICProgram, psCondition, &uPredResultSymID);

			psCondEnd = psICProgram->psInstrTail;
			if(psCondStart)
			{
				/* We were pointing to the instruction before the condition, so move to the next one. */
				psCondStart = psCondStart->psNext;
			}
			else
			{
				/* The condition instructions were the first in the program, so point to the head. */
				psCondStart = psICProgram->psInstrHead;
			}
		}

		/* LOOP */
		ADD_ICODE_INSTRUCTION_LOOP(psCPD, psICProgram, uPredResultSymID, psLineStart);
	
			/* statement */
			ICTraverseAST(psCPD, psICProgram, psStatement);

		/* CONTDEST */
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_CONTDEST, psLineStart);

			if(!bConstantCondition)
			{
				/* Execute condition code again */ 
				CloneICodeInstructions(psCPD, psICProgram, psCondStart, psCondEnd, &psDummyStart, &psDummyEnd);
			}

		/* ENDLOOP */
		ADD_ICODE_INSTRUCTION_ENDLOOP(psCPD, psICProgram, uPredResultSymID, psLineStart);

		/* Decrement condition level */
		DecrementConditionLevel(psICProgram);
	}
}

/******************************************************************************
 * Function Name: ProcessNodeDO
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type: GLSLNT_DO
 *****************************************************************************/
static IMG_VOID ProcessNodeDO(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psStatement, *psCondition;
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_BOOL bConstantCondition, bData = IMG_FALSE;
	IMG_UINT32 uPredResultSymID = 0;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get statement node and condition node */
	DebugAssert(psNode->uNumChildren == 2);
	psCondition = psNode->ppsChildren[0];
	psStatement = psNode->ppsChildren[1];

	if(psCondition)
	{
		bConstantCondition = 
			IsSymbolBoolConstant(psCPD, psICProgram->psSymbolTable, psCondition->uSymbolTableID, &bData);
	}
	else
	{
		bConstantCondition = IMG_TRUE;
		bData = IMG_TRUE;	
	}
	
	/* Increment condition level */
	IncrementConditionLevel(psICProgram);

	/* LOOP */
	ADD_ICODE_INSTRUCTION_LOOP(psCPD, psICProgram, 0, psLineStart);
	
	/* Always execute statement at least once. */
	ICTraverseAST(psCPD, psICProgram, psStatement);
	
	if(!(bConstantCondition && bData))/* We can only skip checking the condition if we know it to be a constant true */
	{
		/* CONTDEST */
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_CONTDEST, psLineStart);

		ICTraverseCondition(psCPD, psICProgram, psCondition, &uPredResultSymID);
	}
	
	/* ENDLOOP */
	ADD_ICODE_INSTRUCTION_ENDLOOP(psCPD, psICProgram, uPredResultSymID, psLineStart);

	/* Decrement condition level */
	DecrementConditionLevel(psICProgram);
}


/******************************************************************************
 * Function Name: ProcessNodeIF
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type: GLSLNT_IF
 *****************************************************************************/
static IMG_VOID ProcessNodeIF(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	GLSLNode *psCondition, *psTrueBlock, *psFalseBlock;
	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psNode);
#if 0
	GLSLNodeType eCompareNodeType;
	GLSLNode *psCompareLeft, *psCompareRight;
#else
	IMG_UINT32 uPredResultSymID;
#endif
	IMG_BOOL bConstantCondition, bData = IMG_FALSE;

	DUMP_PROCESSING_NODE_INFO(psICProgram, psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get condition, true block and false block nodes */
	psCondition = psNode->ppsChildren[0];
	psTrueBlock = psNode->ppsChildren[1];
	psFalseBlock = psNode->ppsChildren[2];

	/* If it is a constant condition expression, then we only need to evaluate the one block */
	bConstantCondition = 
		IsSymbolBoolConstant(psCPD, psICProgram->psSymbolTable, psCondition->uSymbolTableID, &bData);

	if(bConstantCondition)
	{
		if(bData)
		{
			if(psTrueBlock) ICTraverseAST(psCPD, psICProgram, psTrueBlock);
		}
		else
		{
			if(psFalseBlock) ICTraverseAST(psCPD, psICProgram, psFalseBlock);
		}

		return;
	}

#if 0
	/* IF: generate IF code */
	if(IsSimpleConditionCheck(psCPD, psICProgram, psCondition, &eCompareNodeType, &psCompareLeft, &psCompareRight))
	{
		/* Simple case: one condition check, comparing two scalars */
		GenerateOptimisedConditionCode(psCPD, psICProgram, eCompareNodeType, psCompareLeft, psCompareRight, IMG_FALSE);
	}
	else
	{
		/* Evaluate condition expression */
		ICTraverseAST(psCPD, psICProgram, psCondition);

		/* IF */
		ADD_ICODE_INSTRUCTION_IF(psCPD, psICProgram, psCondition, psLineStart);
	}

#else

	ICTraverseCondition(psCPD, psICProgram, psCondition, &uPredResultSymID);

	ADD_ICODE_INSTRUCTION_IFP(psCPD, psICProgram, uPredResultSymID, IMG_FALSE, psLineStart);
#endif

	/* Increase conditional nest count */
	IncrementConditionLevel(psICProgram);

	/* TRUE block */
	if(psTrueBlock) ICTraverseAST(psCPD, psICProgram, psTrueBlock);

	/* FALSE block */
	if(psFalseBlock)
	{
		/* ELSE */
		ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ELSE, psLineStart);

		/* evaluate false block */
		ICTraverseAST(psCPD, psICProgram, psFalseBlock);
	}

	/* ENDIF */
	ADD_ICODE_INSTRUCTION(psCPD, psICProgram, GLSLIC_OP_ENDIF, psLineStart);

	/* Increment crease conditional nest count */
	DecrementConditionLevel(psICProgram);
}


/******************************************************************************
 * Function Name: EvaluateNodeARRAYSPECIFIER
 *
 * Inputs       : psICProgram, psNode, 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node Type: GLSLNT_ARRAY_SPECIFIER
 *****************************************************************************/
static IMG_VOID EvaluateNodeARRAYSPECIFIER(GLSLCompilerPrivateData *psCPD, 
										   GLSLICProgram	*psICProgram,
										   GLSLNode			*psNode,
										   GLSLICOperandInfo	*psOperandInfo)
{

	IMG_CHAR *psLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLNode	*psLeft, *psRight;
	IMG_UINT32	uOffsetSymbolID = 0;
	IMG_INT32	iStaticOffset = 0 ;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/* 
	** Left hand side 
	*/
	if(psLeft->bEvaluated)
	{
		/* Evaludated ?, setup default settings */
		ICInitOperandInfo(psLeft->uSymbolTableID, psOperandInfo);
	}
	else
	{
		/* Examine left child further */
		PostEvaluateNode(psCPD, psICProgram, psLeft, psOperandInfo);
	}

	/* 
	** Right hand side, work out uStaticOffset and uOffsetSymbolID (symbol representing offset)
	*/
	if(!IsSymbolIntConstant(psCPD, psICProgram->psSymbolTable, psRight->uSymbolTableID, &iStaticOffset))
	{
		/* Not a static offset */
		GLSLICOperandInfo sRightOperand;
		
		PostEvaluateNode(psCPD, psICProgram, psRight, &sRightOperand);

		if(IsSimpleOperand(&sRightOperand))
		{
			/* 
			** Simple operand, we can directly use the symbol ID as offset 
			*/
			uOffsetSymbolID = sRightOperand.uSymbolID;
		}
		else
		{
			/* 
			** Complicated operand, for example, need to swizzle, or offsets attached, we need to move 
			** to a temp first
			*/
			ICAddICInstruction2a(psCPD, psICProgram,
							GLSLIC_OP_MOV,
							psLineStart,
							psRight->uSymbolTableID,
							&sRightOperand);

			uOffsetSymbolID = psRight->uSymbolTableID;

			/* Mark psRight->uSymbolTableID is ready to use to prevent duplicative instructions */
			psRight->bEvaluated = IMG_TRUE;
		}

		/* Free the offset list attached to sRightOperand */
		ICFreeOperandOffsetList(&sRightOperand);
	}

	/* Add the offset to the operand info */
	ICAddOperandOffset(psOperandInfo, (IMG_UINT32)iStaticOffset, uOffsetSymbolID);
}


/******************************************************************************
 * Function Name: EvaluateNodeFIELDSELECTION
 *
 * Inputs       : psICProgram, psNode, 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node type: GLSLNT_FIELD_SELECTION
 *****************************************************************************/
static IMG_VOID EvaluateNodeFIELDSELECTION(GLSLCompilerPrivateData *psCPD, 
										   GLSLICProgram		*psICProgram,
										   GLSLNode				*psNode,
										   GLSLICOperandInfo	*psOperandInfo)
{
	GLSLNode *psLeft, *psRight;
	GLSLGenericData *psGenericData;

	/* Get the left and right nodes */
	psLeft = psNode->ppsChildren[0];
	psRight = psNode->ppsChildren[1];

	/*
	** Left hand side 
	*/
	if(psLeft->bEvaluated)
	{
		ICInitOperandInfo(psLeft->uSymbolTableID, psOperandInfo);
	}
	else
	{
		PostEvaluateNode(psCPD, psICProgram, psLeft, psOperandInfo);
	}
	
	/*
	** Right hand side 
	*/
	psGenericData = (GLSLGenericData *) GetSymbolTableData(psICProgram->psSymbolTable, psRight->uSymbolTableID);	
	if(psGenericData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("EvaluateNodeFIELDSELECTION: Failed to get symbol data for ID:%u\n", psRight->uSymbolTableID));
		return;
	}

	switch(psGenericData->eSymbolTableDataType)
	{
	case GLSLSTDT_MEMBER_SELECTION:
		/* User defined structure. */
		{
			GLSLMemberSelectionData *psSelectionData = (GLSLMemberSelectionData *)psGenericData;
			IMG_UINT32 uStaticOffset = psSelectionData->uMemberOffset;

			/* Add the offset to the operand info */
			ICAddOperandOffset(psOperandInfo, uStaticOffset, 0);

			break;
		}
	case GLSLSTDT_SWIZZLE:
		{
			GLSLSwizzleData *psSwizzleData = (GLSLSwizzleData *)psGenericData;
			GLSLICVecSwizWMask sSwizzle;
			IMG_UINT32 i;

			/* Get swizzle mask */
			sSwizzle.uNumComponents = psSwizzleData->uNumComponents;
			for(i = 0; i < psSwizzleData->uNumComponents; i++)
			{
				sSwizzle.aeVecComponent[i] = (GLSLICVecComponent)psSwizzleData->uComponentIndex[i];
			}

			ICCombineTwoConstantSwizzles(&psOperandInfo->sSwizWMask, &sSwizzle);

			break;
		}
	default:
		LOG_INTERNAL_ERROR(("EvaluateNodeFIELDSELECTION: incorrect symbol data type for right child of FIELD_SELECTION\n"));
		break;
	}
}

/******************************************************************************
 * Function Name: PostEvaluateFunctionCall
 *
 * Inputs       : psICProgram, psNode, 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Node type: GLSLNT_FIELD_SELECTION
 *****************************************************************************/
static IMG_BOOL PostEvaluateFunctionCall(GLSLCompilerPrivateData *psCPD, 
										 GLSLICProgram		*psICProgram,
										 GLSLNode			*psNode,
										 GLSLICOperandInfo	*psOperandInfo)
{
	GLSLFunctionCallData		*psFunctionCallData;
	GLSLFunctionDefinitionData	*psFunctionDefinitionData;

	psFunctionCallData = 
		(GLSLFunctionCallData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);
	if(psFunctionCallData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("PostEvaluateFunctionCall: Failed to get symbol Data for ID: %u\n", psNode->uSymbolTableID));
		return IMG_FALSE;
	}
	DebugAssert(psFunctionCallData->eSymbolTableDataType == GLSLSTDT_FUNCTION_CALL);

	/* Get Function Definition data */
	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionCallData->uFunctionDefinitionSymbolID);
	if(psFunctionDefinitionData == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("PostEvaluateFunctionCall: Failed to get symbol Data for ID: %u\n", 
			psFunctionCallData->uFunctionDefinitionSymbolID));
		return IMG_FALSE;
	}

	switch(psFunctionDefinitionData->eFunctionType)
	{
		case GLSLFT_USER:
		{
			DebugAssert(psFunctionDefinitionData->sReturnFullySpecifiedType.eTypeSpecifier != GLSLTS_VOID);
			DebugAssert(psFunctionDefinitionData->uReturnDataSymbolID);

			ICInitOperandInfo(psFunctionDefinitionData->uReturnDataSymbolID, psOperandInfo);
			
			return IMG_TRUE;
		}
		case GLSLFT_CONSTRUCTOR:
		case GLSLFT_USERDEFINED_CONSTRUCTOR:
		case GLSLFT_BUILT_IN:
			/*  
				For constructor and builtin functions, we treat it differently. 
				We always use the symbol ID for function call to store the result, 
				it can have two processe stage, just return false and then force it to 
				post process the function call. 
			*/
			return IMG_FALSE;
		default:
			LOG_INTERNAL_ERROR(("ProcessNodeFUNCTIONCALL: Unknown function type in the function definition \n"));
			return IMG_FALSE;
	}

}

/******************************************************************************
 * Function Name: PostEvaluateNode
 *
 * Inputs       : psICProgram, psNode, 
 * Outputs      : psOperandInfo
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Evaluates a node from its offspring, work out the operand info 
 *				: (the base symbole ID and its assocaited offset list)
 *				: Note: While evaluating, new IC instructions might be inserted. 
 *****************************************************************************/
static IMG_BOOL PostEvaluateNode(GLSLCompilerPrivateData *psCPD, 
								 GLSLICProgram		*psICProgram,
								 GLSLNode			*psNode,
								 GLSLICOperandInfo	*psOperandInfo)
{
	if(!psNode) 
	{
		LOG_INTERNAL_ERROR(("PostEvaluateNode: why is the node NULL\n"));
		return IMG_FALSE;
	}

	if(psNode->bEvaluated || psNode->eNodeType == GLSLNT_IDENTIFIER ) 
	{
		ICInitOperandInfo(psNode->uSymbolTableID, psOperandInfo);

		return IMG_TRUE;
	}

	switch(psNode->eNodeType)
	{
	case GLSLNT_SUBEXPRESSION:
	case GLSLNT_POSITIVE:
	case GLSLNT_EXPRESSION:
	case GLSLNT_DECLARATION:
		{
			GLSLNode *child = psNode->ppsChildren[psNode->uNumChildren - 1];
			DebugAssert(child);
			return PostEvaluateNode(psCPD, psICProgram, child, psOperandInfo);
		}
	case GLSLNT_NEGATE:
	{
		if( PostEvaluateNode(psCPD, psICProgram, psNode->ppsChildren[0], psOperandInfo))
		{
			/* Flip negation status */
			if (psOperandInfo->eInstModifier & GLSLIC_MODIFIER_NEGATE)
			{
				psOperandInfo->eInstModifier &= ~GLSLIC_MODIFIER_NEGATE;
			}
			else
			{
				psOperandInfo->eInstModifier |= GLSLIC_MODIFIER_NEGATE;
			}

			return IMG_TRUE;
		}

		return IMG_FALSE;
	}
	case GLSLNT_ARRAY_SPECIFIER:
		EvaluateNodeARRAYSPECIFIER(psCPD, psICProgram, psNode, psOperandInfo);

		return IMG_TRUE;
	case GLSLNT_FIELD_SELECTION:
		EvaluateNodeFIELDSELECTION(psCPD, psICProgram, psNode, psOperandInfo);
		return IMG_TRUE;
	case GLSLNT_EQUAL:
	case GLSLNT_MUL_ASSIGN:
	case GLSLNT_DIV_ASSIGN:
	case GLSLNT_ADD_ASSIGN:
	case GLSLNT_SUB_ASSIGN:
		/* Inherit the result from the left hand child. */
		return PostEvaluateNode(psCPD, psICProgram, psNode->ppsChildren[0], psOperandInfo);

	case GLSLNT_POST_INC:
	case GLSLNT_POST_DEC:
	case GLSLNT_PRE_INC:
	case GLSLNT_PRE_DEC:
		/* Inherit the result from its child */
		return PostEvaluateNode(psCPD, psICProgram, psNode->ppsChildren[0], psOperandInfo);

	case GLSLNT_FUNCTION_CALL:
		return PostEvaluateFunctionCall(psCPD, psICProgram, psNode, psOperandInfo);

	default:
		return IMG_FALSE;
	}
}


/******************************************************************************
 * Function Name: IsNodeEvaluable
 *
 * Inputs       : psICProgram, psNode,
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Return true if a node is short and when traversing this node, 
				  no new code is needed. 
 *****************************************************************************/
static IMG_BOOL IsNodeDirectOperand(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode)
{
	if(psNode->bEvaluated || psNode->eNodeType == GLSLNT_IDENTIFIER ) 
	{
		return IMG_TRUE;
	}

	switch(psNode->eNodeType)
	{
	case GLSLNT_SUBEXPRESSION:
	case GLSLNT_POSITIVE:
	case GLSLNT_EXPRESSION:
	case GLSLNT_DECLARATION:
		{
			GLSLNode *child = psNode->ppsChildren[psNode->uNumChildren - 1];
			DebugAssert(child);
			return IsNodeDirectOperand(psCPD, psICProgram, child);
		}
	case GLSLNT_NEGATE:
	{
		if( IsNodeDirectOperand(psCPD, psICProgram, psNode->ppsChildren[0]))
		{
			return IMG_TRUE;
		}
		break;
	}
	case GLSLNT_ARRAY_SPECIFIER:
		if( IsNodeDirectOperand(psCPD, psICProgram, psNode->ppsChildren[0]))
		{
			GLSLNode *psRight = psNode->ppsChildren[1];

			if(psRight->bEvaluated || psRight->eNodeType == GLSLNT_IDENTIFIER)
			{
				return IMG_TRUE;
			}
		}
		break;
	case GLSLNT_FIELD_SELECTION:
		if(IsNodeDirectOperand(psCPD, psICProgram, psNode->ppsChildren[0]))
		{
			return IMG_TRUE;
		}
		break;
	default:
		break;
	}

	return IMG_FALSE;
}


/******************************************************************************
 * Function Name: ICTraverseAST
 *
 * Inputs       : psICProgram
 *				: psASTree - Abstract Syntax Tree
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Recursively traverse Abstract Syntax Tree and adding 
 *				: IC instructions to the IC program
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICTraverseAST(GLSLCompilerPrivateData *psCPD, 
									GLSLICProgram *psICProgram, GLSLNode *psASTree)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 i;

	if(!psASTree) return;

	/* Any code, after the top level return has been generated, is unreachable and can be removed */
	if(psICContext->bHadReturn) return;

	DebugAssert(psASTree->eNodeType == NODETYPE_NODETYPE(psASTree->eNodeType));

#if 0
	if(psASTree->uSymbolTableID)
	{
		DumpLogMessage(LOGFILE_ICODE, 0, "Traverse %s(%s):",
			NODETYPE_DESC(psASTree->eNodeType),
			GetSymbolName(psICProgram->psSymbolTable, psASTree->uSymbolTableID));
	}
	else
	{
		DumpLogMessage(LOGFILE_ICODE, 0, "Traverse %s:",
			NODETYPE_DESC(psASTree->eNodeType));
	}

	for(i = 0; i < psASTree->uNumChildren; i++)
	{
		if(psASTree->ppsChildren[i] == IMG_NULL)
		{
			DumpLogMessage(LOGFILE_ICODE, 0, "NULLchild ");
		}
		else
		{
			if(psASTree->ppsChildren[i]->uSymbolTableID)
			{
				DumpLogMessage(LOGFILE_ICODE, 0, "%s(%s) ", 
					NODETYPE_DESC(psASTree->ppsChildren[i]->eNodeType),
					GetSymbolName(psICProgram->psSymbolTable, psASTree->ppsChildren[i]->uSymbolTableID));
			}
			else
			{
				DumpLogMessage(LOGFILE_ICODE, 0, "%s ",  
					NODETYPE_DESC(psASTree->ppsChildren[i]->eNodeType));
			}
		}
	}
	DumpLogMessage(LOGFILE_ICODE, 0, "\n");
#endif

	switch(psASTree->eNodeType)
	{
	case GLSLNT_LOGICAL_OR:
		ProcessNodeLogicalOR(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_LOGICAL_XOR:
		ProcessNodeLogicalXOR(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_LOGICAL_AND:
		ProcessNodeLogicalAND(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_FOR:
		ProcessNodeFOR(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_WHILE:
		ProcessNodeWHILE(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_DO:
		ProcessNodeDO(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_IF:
		ProcessNodeIF(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_QUESTION:
		ProcessNodeQUESTION(psCPD, psICProgram, psASTree);
		return;
	case GLSLNT_FUNCTION_DEFINITION:
		LOG_INTERNAL_ERROR(("ICTraverseAST: FUNCTION_DEFINITION should not come here \n"));
		return;
	case GLSLNT_SHADER:
		ProcessNodeSHADER(psCPD, psICProgram, psASTree);
		return;
	default:
		{
			/* Visit all children first */
			for(i = 0; i < psASTree->uNumChildren; i++)
			{
				if(psASTree->ppsChildren[i])
				{
					ICTraverseAST(psCPD, psICProgram, psASTree->ppsChildren[i]);
				}
			}
		}
	} /* switch */

	/* Process the current node front end */
	ProcessNode(psCPD, psICProgram, psASTree, IMG_FALSE);

	/* Check for any postfix */
	InsertPostfix(psCPD, psICProgram, psASTree);

}

#undef USE_IFC 

/******************************************************************************
 End of file (icgen.c)
******************************************************************************/
