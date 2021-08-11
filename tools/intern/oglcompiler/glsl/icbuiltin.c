/******************************************************************************
 * Name         : icbuiltin.c
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
 * $Log: icbuiltin.c $
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "glsltree.h"
#include "icode.h"
#include "common.h"
#include "../parser/debug.h"
#include "../parser/symtab.h"
#include "error.h"
#include "icgen.h"
#include "astbuiltin.h"
#include "icbuiltin.h"
#include "icemul.h"
#undef IS_MATRIX
#include "semantic.h"


/* Built-in Functions 

	Trigonometry/angle (component-wise)
		radians, degrees, sin, cos, tan, asin, acos, atan
	Exponential (component-wise)
		pow, exp, log, exp2, log2, sqrt, inversesqrt
	Common (component-wise)
		abs, sign, floor, ceil, fract, mod, min, max, clamp, mix, step, smoothstep
	Geometric
		length, distance, dot, cross, normalize, ftransform, faceforward, reflect, refract, 
	Matrix
		matrixCompMult
		transpose
		outerProduct
	Vector relational
		lessThan, lessThanEqual, greaterThan, greaterThanEqual, equal, notEqual, any, all, not
	Texture lookup
		texture1D/2D/3D, texture1D/2D/3DProj, texture1D/2D/3DLod, texture1D/2D/3DProjLod,
		textureCube, textureCubeLod, 
		shadow1D/2D, shadow1D/2DProj, shadow1D/2DLod, shadow1D/2DProjLod
	Fragment shader only
		dFdx, dFdy, fwidth
	Noise
		noise1/2/3/4
*/

/* 
** Map directly a built-in function to a corresponding IC instruction.
** The function is for those functions which HW support directly and correspoding 
** IC instruction exists. 
**
** MAP_BIFN_TO_ICINSTR: built-in funcs with one to five arguments, 
*/

static void MAP_BIFN_TO_ICINSTR(GLSLCompilerPrivateData *psCPD, 
								GLSLICProgram *program, 
								GLSLNode *node, 
								GLSLICOperandInfo *dest, 
								GLSLICOpcode icopcode)
{
	GLSLICOperandInfo sOperands[MAX_OPRDS];
	IMG_UINT32 i;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(node);
	IMG_UINT32 numsources = node->uNumChildren;

	DebugAssert((numsources) < MAX_OPRDS);

	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(node);

	/* Process arguments */
	for(i = 0; i < numsources; i++)
	{
		ICProcessNodeOperand(psCPD, program, node->ppsChildren[i], &sOperands[i]);
	}

	/* OPCODE  result arg1 arg2 arg3, arg4 arg5 */
	ICAddICInstruction(psCPD, program,
					   icopcode,
					   numsources,
					   pszLineStart,
					   dest,
					   &sOperands[0]);

	/* Free all offsets attached */
	for(i = 0; i < numsources; i++)
	{
		ICFreeOperandOffsetList(&sOperands[i]);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNradians:
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for radians
 *				: Syntax		genType radians(genType degrees)
 *				: Description	Convert degrees to radians and returns the result,
 *								i.e., result = PII/180 * degree.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNradians(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	**	DIV	result degrees 57.29577951
	**=====================================*/

	IMG_UINT32			uRadianSymbolID;
	GLSLICOperandInfo	sChildOperand, sRadianOperand;
	GLSLNode			*psChildNode = psNode->ppsChildren[0];
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);
	
	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psChildNode, &sChildOperand);

	/* Degrees per radian */
	if (!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 57.29577951f, ePrecision, IMG_TRUE, &uRadianSymbolID))
	{
		LOG_INTERNAL_ERROR(("ProcessBuiltInRadians: Failed to add float constant 57.17446 !\n"));
		return;
	}
	ICInitOperandInfo(uRadianSymbolID, &sRadianOperand);

	/* DIV	result degrees 57.29577951 */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_DIV,
						pszLineStart,
						psDestOperand,
						&sChildOperand,
						&sRadianOperand);

	/* Free the offset list of the child operand */
	ICFreeOperandOffsetList(&sChildOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNdegrees
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for degrees
 *				: Syntax		genType degrees(genType radians)
 *				: Description	Convert radians to degrees and returns the result,
 *								ie., result = 180/PII * radians.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNdegrees(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	**	MUL	result radians 57.29577951
	**=====================================*/

	IMG_UINT32			uRadianSymbolID;
	GLSLICOperandInfo	sChildOperand, sRadianOperand;
	GLSLNode			*psChildNode = psNode->ppsChildren[0];
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);
	
	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psChildNode, &sChildOperand);

	/* Degrees per radian */
	if (!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 57.29577951f, ePrecision, IMG_TRUE, &uRadianSymbolID))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNdegrees: Failed to add float constant 57.29577951 !\n"));
		return;
	}
	ICInitOperandInfo(uRadianSymbolID, &sRadianOperand);

	/* MUL	result radians 57.29577951 */
	ICAddICInstruction3(psCPD, psICProgram, 
						GLSLIC_OP_MUL, 
						pszLineStart,
						psDestOperand,
						&sChildOperand,
						&sRadianOperand);

	/* Free the offset list of the child operand */
	ICFreeOperandOffsetList(&sChildOperand);

}

/******************************************************************************
 * Function Name: ICProcessBIFNsin
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for sin
 *				: Syntax		genType sin(genType angle)
 *				: Description	The standard trigonometric sine function
 *****************************************************************************/
static IMG_VOID ICProcessBIFNsin(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	**	SIN	result angle
	**=====================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SIN);
}


/******************************************************************************
 * Function Name: ICProcessBIFNcos
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for cos
 *				: Syntax		genType cos(genType angle)
 *				: Description	The standard trigonometric cosine function
 *****************************************************************************/
static IMG_VOID ICProcessBIFNcos(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
	**	COS	result angle
	**=====================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_COS);
}

/******************************************************************************
 * Function Name: ICProcessBIFNtan
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for tan
 *				: Syntax		genType tan(genType angle)
 *				: Description	The standard trigonometric tangent
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtan(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_TAN)
	{
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TAN);
	
	}
	else
	{
		/*=======================================
		  SIN	temp1 angle			// temp1(genType) = sin(angle) 
		  COS temp2 angle			// temp2(genType) = cos(angle)
		  DIV	result temp1 temp2	// result = sin(angle)/cos(angle)
		  =======================================*/
		
		ICEmulateBIFNtan(psCPD, psICProgram, psNode, psDestOperand);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNasin
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for asin
 *				: Syntax		genType asin(genType x)
 *				: Description	Arc sine. Returns an angle whose sine is x. The
 *								range of values returned by this function is 
 *								[-PII/2, PII/2]. Results are undefined if |x| > 1.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNasin(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_ASIN)
	{
		/*=======================================
		  ASIN result x		
		  =======================================*/
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_ASIN);
	}
	else
	{
		ICEmulateBIFNArcTrigs(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_ASIN);
	}
}



/******************************************************************************
 * Function Name: ICProcessBIFNacos
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for acos
 *				: Syntax		genType acos(genType x)
 *				: Description	Arc cosine. Returns angle whose cosine is x. The
 *								range of values returned by this function is 
 *								[0, PII]. Results are undefined if |x| > 1.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNacos(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_ACOS)
	{
		/*=======================================
		  ACOS result x		
		  =======================================*/
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_ACOS);
	}
	else
	{
		ICEmulateBIFNArcTrigs(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_ACOS);
	}

}

/******************************************************************************
 * Function Name: ICProcessBIFNatan
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for atan
 *				: Syntax(1)		genType atan(genType y, genType x)
 *				: Description	Arc tangent. Returns an angle whose tangent is 
 *				:				y/x. The signs of x and y are used to determind 
 *				:				what quadrant the angle is in. The range of 
 *				:				values returned by this function is [-PII, PII].
 *				:				Results are undefined if x and y are both 0
 *				: Syntax(2)		genType atan(genType y_over_x)
 *				: Description	Arc tangent. Returns an angle whose tangent is 
 *								y_over_x. The range of values returned by this 
 *								function is is[-PII/2, PII/2]. 
 *****************************************************************************/
static IMG_VOID ICProcessBIFNatan(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_ATAN)
	{
		if(psNode->uNumChildren == 1)
		{
			/*=======================================
			  ATAN result x		
			  =======================================*/
			MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_ATAN);
		}
		else
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNatan: We don't support atan2 with specialized hw instructions yet\n"));
		}
	}
	else
	{
		ICEmulateBIFNArcTrigs(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_ATAN);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNpow
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for pow
 *				: Syntax		genType pow(genType x, genType y)
 *				: Description	Returns x raised to the y power, i.e., xy.
 *								Results are undefined if x < 0.
 *								Results are undefined if x = 0 and y <= 0.
 *****************************************************************************/

static IMG_VOID ICProcessBIFNpow(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_POW)
	{
		/*=======================================
		  POW result x y	
		  =======================================*/
		
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_POW);
	}
	else
	{
		/*=======================================
		  LOG2 temp	x		// temp (genType) = log[2, x]
		  MUL  temp	temp  y	// temp (genType) = log[2, x] * y
		  EXP2 result temp	// result = exp2(temp) = pow(2, (log[2, x] * y))
		  =======================================*/
		ICEmulateBIFNpow(psCPD, psICProgram, psNode, psDestOperand);

	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNexp
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for exp
 *				: Syntax		genType exp(genType x)
 *				: Description	Returns the natural exponentiation of x, i.e., ex
 *****************************************************************************/
static IMG_VOID ICProcessBIFNexp(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
#if 1
	/* exp() has been given its own GLSLIC_OP enum so that it can be reused by other builtin functions. */
	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_EXP);
#else
	/*=======================================
	  	c = 1.0/log[2,e] = 0.69314718056 == ln(2) (constant value from Simmon Fenny)
		DIV	 temp  x c			// temp(genType) = x /ln(2) = x * log[2, e]
		EXP2 result temp		// result = exp2(temp) = exp2(x * log[2, e])
	=======================================*/

	GLSLNode *psXNode;
	GLSLICOperandInfo sXOperand, sTempOperand;
	IMG_UINT32 uConstant, uTemp;
	GLSLTypeSpecifier  eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x*/
	psXNode = psNode->ppsChildren[0];
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Process node operand */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 0.69314718056f, ePrecision, IMG_TRUE, &uConstant))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNexp: Failed to add float constant 0.69314718056f to the symbol table\n"));
		return;
	}

	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNexp: Failed to add a temp result type to symbol table\n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* IC: DIV temp x c */
	ICAddICInstruction3b(psCPD, psICProgram,
						 GLSLIC_OP_DIV,
						 pszLineStart,
						 uTemp,
						 &sXOperand,
						 uConstant);

	/* IC: EXP2 result temp */
	ICAddICInstruction2(psCPD, psICProgram, 
						GLSLIC_OP_EXP2,
						pszLineStart,
						psDestOperand,
						&sTempOperand);

	/* Free operand offset list */
	ICFreeOperandOffsetList(&sXOperand);
#endif
}

/******************************************************************************
 * Function Name: ICProcessBIFNlog
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for log
 *				: Syntax		genType log(genType x)
 *				: Description	Returns the natural logarithm of x, ie., returns
 *								the value y which satifies the equation x = ey.
 *								Results are undefined if x <= 0.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNlog(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
#if 1
	/* log() has been given its own GLSLIC_OP enum so that it can be reused by other builtin functions. */
	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_LOG);
#else
	/*=======================================
	  	c = 1.0/log[2,e] = 0.69314718056 == ln(2) (constant value from Simmon Fenny)
		LOG2 temp x				// temp(genType) = log[2, x]
		MUL result temp c		// result = temp * ln(2) = log[2, x] / log[2, e]
	=======================================*/

	GLSLNode *psXNode;
	GLSLICOperandInfo sXOperand, sTempOperand, sConstantOperand;
	IMG_UINT32 uConstant, uTemp;
	GLSLTypeSpecifier  eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x*/
	psXNode = psNode->ppsChildren[0];
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Process node operand */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 0.69314718056f, ePrecision, IMG_TRUE, &uConstant))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNlog: Failed to add float constant 0.69314718056(ln2) to the symbol table\n"));
		return;
	}
	ICInitOperandInfo(uConstant, &sConstantOperand);

	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNlog: Failed to add a temp result type to symbol table\n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* LOG2 temp x*/
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_LOG2, pszLineStart, &sTempOperand, &sXOperand);

	/* MUL result temp c */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MUL,
						pszLineStart,
						psDestOperand,
						&sTempOperand,
						&sConstantOperand);

	/* Free operand offset list */
	ICFreeOperandOffsetList(&sXOperand);
#endif
}

/******************************************************************************
 * Function Name: ICProcessBIFNexp2
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for exp2
 *				: Syntax		genType exp2(genType x)
 *				: Description	Returns 2 raised to the x power, ie., 2x
 *****************************************************************************/
static IMG_VOID ICProcessBIFNexp2(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
	  	EXP2 result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_EXP2);
}

/******************************************************************************
 * Function Name: ICProcessBIFNlog2
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for log2
 *				: Syntax		genType log2(genType x)
 *				: Description	Returns the case 2 logarithm of x, ie. returns the
 *								value y which satisfies the equation x = 2y
 *								Results are undefined if x <= 0.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNlog2(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	  	LOG2 result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_LOG2);
}

/******************************************************************************
 * Function Name: ICProcessBIFNsqrt
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for sqrt
 *				: Syntax		genType sqrt(genType)
 *				: Description	Returns the positive square root of x.
 *				:				Results are undefined if x < 0.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNsqrt(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	  	RSQ temp x			// temp(genType) = 1.0/sqrt(x)
		RCP result temp		// result = 1.0/temp = sqrt(x);
	=======================================*/
	GLSLNode *psXNode;
	GLSLICOperandInfo sXOperand, sTempOperand;
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x*/
	psXNode = psNode->ppsChildren[0];

	/* Get x type specifier */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Process x node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* Add a new temp */
	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNsqrt: Failed to add a temp \n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* RSQ temp x */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RSQ, pszLineStart, &sTempOperand, &sXOperand);

	/* RCP result temp */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RCP, pszLineStart, psDestOperand, &sTempOperand);

	/* Free offet list  */
	ICFreeOperandOffsetList(&sXOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNinversesqrt
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for inversesqrt
 *				: Syntax		genType inversesqrt(genType, x)
 *				: Description	Returns the reciprocal of the positive square root of x
 *				:				Results are undefined if x <= 0.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNinversesqrt(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	  	RSQ result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_RSQ);
}

/******************************************************************************
 * Function Name: ICProcessBIFNabs
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for abs
 *				: Syntax		genType abs(genType)
 *				: Description	Returns x if x>= 0, otherwise it returns -x
 *****************************************************************************/

static IMG_VOID ICProcessBIFNabs(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
	  	ABS result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_ABS);
}

/******************************************************************************
 * Function Name: ICProcessBIFNsign
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for sign
 *				: Syntax		genType sign(genType x)
 *				: Description	Returns 1.0 if x>0, 0.0 if x=0, or -1.0 if x<0
 *****************************************************************************/
static IMG_VOID ICProcessBIFNsign(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
	  	SGN result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SGN);
}

/******************************************************************************
 * Function Name: ICProcessBIFNfloor
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for floor
 *				: Syntax		genType floor(genType x)
 *				: Description	Returns a value equal to the nearest integer that is
 *								less than or equal to x
 *****************************************************************************/
static IMG_VOID ICProcessBIFNfloor(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
		FLOOR result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_FLOOR);
}

/******************************************************************************
 * Function Name: ICProcessBIFNceil
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for ceil
 *				: Syntax		genType ceil(genType)
 *				: Description	Returns a calue equal to the nearest integer that is
 *								greater than or equal to x
 *****************************************************************************/
static IMG_VOID ICProcessBIFNceil(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		CEIL result x
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_CEIL);
}

/******************************************************************************
 * Function Name: ICProcessBIFNfract
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for fract
 *				: Syntax		genType fract(genType x)
 *				: Description	Returns x - floor(x)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNfract(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	
	if (psICContext->eExtensionsSupported & GLSLIC_EXT_FRC)
	{
		/*=======================================
		  FRC result x
		  =======================================*/

		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_FRC);
	}
	else
	{
		/*=======================================
		  FLOOR temp x			// temp (genType) = floor(x)
		  SUB   result x temp		// result = x - floor(x)
		  =======================================*/

		ICEmulateBIFNfract(psCPD, psICProgram, psNode, psDestOperand);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNmod
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for mod
 *				: Syntax(1)		genType mod (genType x, float y)
 *				: Description	Modulus. Returns x - y *floor (x/y)
 *				: Syntax(2)		genType mod(genType x, genType y)
 *				: Description	Modulus. Returns x - y * floor(x/y)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNmod(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if (psICContext->eExtensionsSupported & GLSLIC_EXT_MOD)
	{
		/*=======================================
		  MOD result x y
		  =======================================*/

		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_MOD);
	}
	else
	{

		/*=======================================
		  DIV	  temp1 x y			// temp1(genType) = x/y
		  FLOOR temp2 temp1		// temp2(genType) = floor(x/y)
		  MUL	  temp3 y temp2		// temp3(genType) = y * floor(x/y)
		  SUB   result x temp3	// result = x - temp3
		  =======================================*/
		ICEmulateBIFNmod(psCPD, psICProgram, psNode, psDestOperand);
	}

}

/******************************************************************************
 * Function Name: ICProcessBIFNmin
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for min
 *				: Syntax		genType min (genType x,genType y)
 *				: Syntax		genType min (genType x,float y)
 *				: Description	Returns y if y < x, otherwise it returns x
 *****************************************************************************/
static IMG_VOID ICProcessBIFNmin(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
		MIN result x y
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_MIN);
}

/******************************************************************************
 * Function Name: ICProcessBIFNmax
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for max
 *				: Syntax		genType max (genType x,genType y)
 *				: Syntax		genType max (genType x,float y)
 *				: Description	Returns y if x < y, otherwise it returns x
 *****************************************************************************/
static IMG_VOID ICProcessBIFNmax(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		MAX result x y
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_MAX);

}

/******************************************************************************
 * Function Name: ICProcessBIFNclamp
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for clamp
 *				: Syntax(1)		genType clamp (genType x, genType minVal, genType maxVal)
 *				: Syntax(2)		genType clamp (genType x, float minVal, float maxVal)
 *				: Description	Returns min (max (x, minVal), maxVal)
 *								Note that colors and depths written by fragment
 *								shaders will be clamped by the implementation
 *								after the fragment shader runs.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNclamp(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		MAX temp x minVal		// temp(genType) = max(x, minVal)
		MIN result temp maxVal	// result = min(max(x, minVal), maxVal)
	=======================================*/
	GLSLNode *psXNode, *psMinValNode, *psMaxValNode;
	GLSLICOperandInfo sXOperand, sMinValOperand, sMaxValOperand, sTempOperand;
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x, minVal and maxVal */
	psXNode = psNode->ppsChildren[0];
	psMinValNode = psNode->ppsChildren[1];
	psMaxValNode = psNode->ppsChildren[2];

	/* Get x type specifier */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Process node x, minVal, maxVal  */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psMinValNode, &sMinValOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psMaxValNode, &sMaxValOperand);

	/* Add a new temp */
	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNclamp: Failed to add a temp \n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* IC: MAX temp x minVal */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MAX,
						pszLineStart,
						&sTempOperand,
						&sXOperand,
						&sMinValOperand);

	/* IC: MIN result temp maxVal */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MIN,
						pszLineStart,
						psDestOperand,
						&sTempOperand,
						&sMaxValOperand);

	/* Free offet list  */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sMinValOperand);
	ICFreeOperandOffsetList(&sMaxValOperand);	
}

/******************************************************************************
 * Function Name: ICProcessBIFNmix
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for mix
 *				: Syntax(1)		genType mix (genType x, genType y, genType a)
 *				: Syntax(2)		genType mix (genType x, genType y, float a)
 *				: Description   Returns x *(1 - a)+y *a, i.e., the linear blend
 *								of x and y
 *****************************************************************************/
static IMG_VOID ICProcessBIFNmix(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
#if 0
	/* x *(1 - a)+y *a = x - a * x + a * y = a * (y -x) + x */
	/*=======================================
		SUB temp y - x			
		MAD result a temp x 
	=======================================*/
	GLSLNode *psXNode, *psYNode, *psANode;
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier eGenType;
	GLSLICOperandInfo sXOperand, sYOperand, sAOperand, sTempOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x, y and a */
	psXNode = psNode->ppsChildren[0];
	psYNode = psNode->ppsChildren[1];
	psANode = psNode->ppsChildren[2];

	/* Get type for x and type for a */
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Add temp1 */
	if( !ICAddTemporary(psCPD, psICProgram, eGenType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNmix: Failed to add temp to the symbol table !\n"));
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* Process node x, y and a  */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psYNode, &sYOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psANode, &sAOperand);

	/* IC: SUB temp1 y x */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SUB,
						pszLineStart,
						&sTempOperand,
						&sYOperand,
						&sXOperand);

	/* IC: MAD result a temp x */
	ICAddICInstruction4(psCPD, psICProgram,
						GLSLIC_OP_MAD,
						pszLineStart,
						psDestOperand,
						&sAOperand,
						&sTempOperand,
						&sXOperand);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sYOperand);
	ICFreeOperandOffsetList(&sAOperand);
#else

	PVR_UNREFERENCED_PARAMETER(psCPD);
	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_MIX);
#endif
}

/******************************************************************************
 * Function Name: ICProcessBIFNstep
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for step
 *				: Syntax(1)		genType step (genType edge, genType x)
 *				: Syntax(2)		genType step (float edge,genType x)
 *				: Description	Returns 0.0 if x < edge, otherwise it returns 1.0
 *****************************************************************************/
static IMG_VOID ICProcessBIFNstep(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SGE temp x edge		// temp (bvec the same size as genType) = (x >= edge)
		MOV result temp		// result = float(temp)
	=======================================*/

	GLSLNode *psEdgeNode, *psXNode;
	IMG_UINT32 uTemp; 
	GLSLICOperandInfo sEdgeOperand, sXOperand, sTempOperand;
	GLSLTypeSpecifier eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x, y and a */
	psEdgeNode = psNode->ppsChildren[0];
	psXNode = psNode->ppsChildren[1];

	/* Get type specifier for x */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Add temp of the same type as the X parameter */
	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNstep: Failed to add a temp to the symbol table !\n"));
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* Process node x, y and a  */
	ICProcessNodeOperand(psCPD, psICProgram, psEdgeNode, &sEdgeOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* IC: SGE temp x edge */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SGE,
						pszLineStart,
						&sTempOperand,
						&sXOperand,
						&sEdgeOperand);

	/* IC: MOV result temp */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, &sTempOperand);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sEdgeOperand);
	ICFreeOperandOffsetList(&sXOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNsmoothstep
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for smoothstep
 *				: Syntax(1)		genType smoothstep (genType edge0, genType edge1, genType x)
 *				: Syntax(2)		genType smoothstep (float edge0, float edge1, genType x)
 *				: Description	Returns 0.0 if x <= edge0 and 1.0 if x >= edge1
 *								and performs smooth Hermite interpolation
 *								between 0 and 1 when edge0 < x < edge1.This is
 *								useful in cases where you would want a threshold
 *								function with a smooth transition. This is
 *								equivalent to:
 *									genType t;
 *									t = clamp ((x - edge0)/(edge1 - edge0),0,1);
 *									return t * t * (3 - 2 * t);
 *****************************************************************************/
static IMG_VOID ICProcessBIFNsmoothstep(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/* FIXME: need to rework to reuse temporarys */
	/*=======================================
		1	SUB temp0 x edge0		// temp0 (genType) = x - edge0
		2	SUB temp1 edge1 edge0	// temp1 (eEdge0Type) = edge1 - edge0
		3	DIV	temp2 temp0 temp1	// temp2 (genType) = (x - edgo0)/(edge1 - edge0)
		4	MAX temp3 temp2 0,0		// temp3 (genType) = max(temp2, 0.0)
		5	MIN temp4 temp3 1.0		// temp4 (genType) = min(temp3, 1.0) = t
		6	MUL temp5 2.0 temp4		// temp5 (genType) = 2.0 * t 
		7	SUB temp6 3.0 temp5		// temp6 (genType) = 3.0 - 2.0 * t
		8	MUL temp7 temp4 temp4	// temp7 (genType) = t * t
		9	MUL result temp6 temp7	// result = t * t * (3 - 2 * t)
	=======================================*/

	GLSLNode *psEdge0Node, *psEdge1Node, *psXNode;
	IMG_UINT32 uTemp[8], i;
	IMG_UINT32 uZero, uOne, uTwo, uThree;
	GLSLICOperandInfo sEdge0Operand, sEdge1Operand, sXOperand, sTemp6Operand, sTemp7Operand;
	GLSLTypeSpecifier eXType, eEdge0Type;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node edge0, edge1 and x */
	psEdge0Node = psNode->ppsChildren[0];
	psEdge1Node = psNode->ppsChildren[1];
	psXNode = psNode->ppsChildren[2];

	/* Get type specifier for x and edge0 */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	eEdge0Type = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psEdge0Node->uSymbolTableID);

	/* Add temps */
	for(i = 0; i < 8; i++)
	{
		GLSLTypeSpecifier eType;

		// The type of the 2nd temp is the same as edge0 /
		if(i == 1)
		{
			eType = eEdge0Type;
		}
		else
		{
			eType = eXType;
		}
		if( !ICAddTemporary(psCPD, psICProgram, eType, ePrecision, &uTemp[i]))
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNsmoothstep: Failed to add a temp to the symbol table !\n"));
		}
	}

	/* Add constant 0.0, 1.0, 2.0 and 3.0 */
	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 0.0, ePrecision, IMG_TRUE, &uZero))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNsmoothstep: Failed to add float constant 0.0 to the symbole table!\n"));
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, ePrecision, IMG_TRUE, &uOne))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNsmoothstep: Failed to add float constant 1.0 to the table!\n"));
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 2.0, ePrecision, IMG_TRUE, &uTwo))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNsmoothstep: Failed to add float constant 2.0 to the symbole table!\n"));
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 3.0, ePrecision, IMG_TRUE, &uThree))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNsmoothstep: Failed to add float constant 3.0 to the table!\n"));
	}

	/* Process node x, y and a  */
	ICProcessNodeOperand(psCPD, psICProgram, psEdge0Node, &sEdge0Operand);
	ICProcessNodeOperand(psCPD, psICProgram, psEdge1Node, &sEdge1Operand);
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* IC:	1	SUB temp0 x edge0 */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 uTemp[0],
						 &sXOperand,
						 &sEdge0Operand);

	/* IC:	2	SUB temp1 edge1 edge0 */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 uTemp[1],
						 &sEdge1Operand,
						 &sEdge0Operand);


	/* IC:	3	DIV	temp2 temp0 temp1 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_DIV,
						 pszLineStart,
						 uTemp[2],
						 uTemp[0],
						 uTemp[1]);

	/* IC:	4	MAX temp3 temp2 0,0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MAX,
						 pszLineStart,
						 uTemp[3],
						 uTemp[2],
						 uZero);

	/* IC:	5	MIN temp4 temp3 1.0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MIN,
						 pszLineStart,
						 uTemp[4],
						 uTemp[3],
						 uOne);

	/* IC:	6	MUL temp5 2.0 temp4 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[5],
						 uTwo,
						 uTemp[4]);


	/* IC:	7	SUB temp6 3.0 temp5 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 uTemp[6],
						 uThree,
						 uTemp[5]);

	/* IC:	8	MUL temp7 temp4 temp4 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[7],
						 uTemp[4],
						 uTemp[4]);

	/* IC:	9	MUL result temp6 temp7 */
	ICInitOperandInfo(uTemp[6], &sTemp6Operand);
	ICInitOperandInfo(uTemp[7], &sTemp7Operand);
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MUL,
						pszLineStart,
						psDestOperand,
						&sTemp6Operand,
						&sTemp7Operand);


	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sEdge0Operand);
	ICFreeOperandOffsetList(&sEdge1Operand);
	ICFreeOperandOffsetList(&sXOperand);
}

/*
** Geometric Functions 
*/

/******************************************************************************
 * Function Name: ICProcessBIFNlength
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for length
 *				: Syntax		float length (genType x)
 *				: Description	Returns the length of vector x, i.e.,
 *								sqrt (x[0] *x[0] + x[1] *x[1] + ...)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNlength(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DOT temp x x		// temp(float) = dot(x, x)
		RSQ	temp temp		// temp(float) = 1.0/sqrt(temp)
		RCP result temp		// remp = sqrt(temp)
	=======================================*/

	GLSLNode *psXNode;
	IMG_UINT32 uTemp; 
	GLSLICOperandInfo sXOperand, sTempOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psXNode = psNode->ppsChildren[0];

	/* Add temp */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNlength: Failed to add temp 0 to the symbol table !\n"));
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* Process node x */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* IC: DOT temp x x */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_DOT,
						pszLineStart,
						&sTempOperand,
						&sXOperand,
						&sXOperand);

	/* IC: RSQ	temp temp */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RSQ, pszLineStart, &sTempOperand, &sTempOperand);

	/* IC: RCP result temp */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RCP, pszLineStart, psDestOperand, &sTempOperand);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sXOperand);	
}

/******************************************************************************
 * Function Name: ICProcessBIFNdistance
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for distance
 *				: Syntax		float distance (genType p0, genType p1)
 *				: Description	Returns the distance between p0 and p1,i.e.
 *				:				length (p0 - p1)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNdistance(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SUB temp0 p0 p1			// temp0 (genType) = p0 - p1
		DOT temp1 temp0 temp0	// temp1 (float) = dot(temp0, temp0)
		RSQ	temp1 temp1			// temp2 (float) = 1.0/(sqrt(temp1))
		RCP result temp1		// result(float) = sqrt(temp1)
	=======================================*/

	GLSLNode *psP0Node, *psP1Node;
	IMG_UINT32 uTemp0, uTemp1; 
	GLSLICOperandInfo sP0Operand, sP1Operand, sTemp0Operand, sTemp1Operand;
	GLSLTypeSpecifier eP0Type;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psP0Node = psNode->ppsChildren[0];
	psP1Node = psNode->ppsChildren[1];
	eP0Type = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psP0Node->uSymbolTableID);

	/* Add temp 0 */
	if( !ICAddTemporary(psCPD, psICProgram, eP0Type, ePrecision, &uTemp0))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNdistance: Failed to add temp 0 to the symbol table !\n"));
	}
	ICInitOperandInfo(uTemp0, &sTemp0Operand);

	/* Add temp 1 */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp1))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNdistance: Failed to add temp 1to the symbol table !\n"));
	}
	ICInitOperandInfo(uTemp1, &sTemp1Operand);


	/* Process node p0, p1 */
	ICProcessNodeOperand(psCPD, psICProgram, psP0Node, &sP0Operand);
	ICProcessNodeOperand(psCPD, psICProgram, psP1Node, &sP1Operand);

	/* IC: SUB temp0 p0 p1 */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SUB,
						pszLineStart,
						&sTemp0Operand,
						&sP0Operand,
						&sP1Operand);

	/* IC: DOT temp1 temp0 temp0 */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_DOT,
						pszLineStart,
						&sTemp1Operand,
						&sTemp0Operand,
						&sTemp0Operand);

	/* IC: RSQ	temp1 temp1 */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RSQ, pszLineStart, &sTemp1Operand, &sTemp1Operand); 

	/* IC: RCP result temp1 */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RCP, pszLineStart, psDestOperand, &sTemp1Operand);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sP0Operand);
	ICFreeOperandOffsetList(&sP1Operand);

}

/******************************************************************************
 * Function Name: ICProcessBIFNdot
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for dot
 *				: Syntax		float dot (genType x, genType y)
 *				: Description	Returns the dot product of x and y, i.e.,
 *								result = x[0] *y[0] + x[1] *y[1] + ...
 *****************************************************************************/
static IMG_VOID ICProcessBIFNdot(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DOT result x y
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_DOT);
}

/******************************************************************************
 * Function Name: ICProcessBIFNcross
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for cross
 *				: Syntax		vec3 cross (vec3 x,vec3 y)
 *				: Description	Returns the cross product of x and y, i.e.
 *								result.0 = x[1] *y[2] - y[1] *x[2]
 *								result.1 = x[2] *y[0] - y[2] *x[0]
 *								result.2 = x[0] *y[1] - y[0] *x[1]
 *****************************************************************************/
static IMG_VOID ICProcessBIFNcross(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		CROSS result x y
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_CROSS);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnormalize
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for normalize
 *				: Syntax		genType normalize(genType x)
 *				: Description	Returns a vector in the same direction as x but
 *								with a length of 1.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnormalize(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DOT temp x x			// temp(float) = dot(x, x)
		RSQ temp temp			// temp(float) = 1.0/sqrt(dot(x,x))
		MUL result x temp		// result = x * temp = x / sqrt(dot(x,x))
	=======================================*/

	GLSLNode *psXNode;
	GLSLICOperandInfo sXOperand, sTempOperand;
	IMG_UINT32 uTemp;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);
	GLSLTypeSpecifier eType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	if (eType == GLSLTS_VEC3)
	{
		/*
			Map directly to the intermediate instruction.
		*/
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_NRM3);
	}
	else
	{
		/* Get the source line number */
		psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

		/* Get node x */
		psXNode = psNode->ppsChildren[0];

		/* Add temp 0 */
		if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp))
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNnormalize: Failed to add temp to the symbol table !\n"));
		}
		ICInitOperandInfo(uTemp, &sTempOperand);

		/* Process node p0, p1 */
		ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

		/* IC: DOT temp x x */
		ICAddICInstruction3(psCPD, psICProgram,
							GLSLIC_OP_DOT,
							pszLineStart,
							&sTempOperand,
							&sXOperand,
							&sXOperand);

		/* IC: RSQ temp temp */
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_RSQ, pszLineStart, &sTempOperand, &sTempOperand);

		/* IC: MUL result x temp1 */
		ICAddICInstruction3(psCPD, psICProgram,
							GLSLIC_OP_MUL,
							pszLineStart,
							psDestOperand,
							&sXOperand,
							&sTempOperand);

		/* Free all operand offset lists */
		ICFreeOperandOffsetList(&sXOperand);
	}
}


/******************************************************************************
 * Function Name: ICGetBuiltInVariables
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Find the symbol ID for a built-in variable according to the name
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 ICFindBuiltInVariables(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_CHAR *pszBIVaribleName)
{
	IMG_UINT32 uVaribleSymbolID;

	if (!FindSymbol(psICProgram->psSymbolTable, pszBIVaribleName, &uVaribleSymbolID, IMG_FALSE))
	{
		LOG_INTERNAL_ERROR(("ICFindBuiltInVariables: Failed to find built-in %s !\n", pszBIVaribleName));
		return 0;
	}

	return uVaribleSymbolID;
}

/******************************************************************************
 * Function Name: ICProcessBIFNftransform
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for ftransform
 *				: Syntax		vec4 ftransform()
 *				: Description	For vertex shaders only. This function will
 *								ensure that the incoming vertex value will be
 *				 				transformed in a way that produces exactly the
 *								same result as would be produced by OpenGL's
 *								fixed functionality transform. It is intended to be
 *								used to compute gl_Position, e.g.,
 *									gl_Position = ftransform()
 *								This function should be used, for example, when
 *								an application is rendering the same geometry in
 *								separate passes, and one pass uses the fixed
 *								functionality path to render and another pass uses
 *								programmable shaders.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNftransform(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		MUL result gl_ModelViewProjectionMatrix gl_Vertex
	=======================================*/

	IMG_UINT32 uGLVertex;						// Symbol ID for gl_Vertex
	IMG_UINT32 uGLModelViewProjectionMatrix;	// Symbol IF for gl_ModelViewProjectMatrix
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get built-in variable gl_Vertex and gl_ModelViewProjectMatrix */
	uGLVertex					 = ICFindBuiltInVariables(psCPD, psICProgram, "gl_Vertex");
	uGLModelViewProjectionMatrix = ICFindBuiltInVariables(psCPD, psICProgram, "gl_ModelViewProjectionMatrix");

	/* Add instruction */
	ICAddICInstruction3e(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 psDestOperand,
						 uGLModelViewProjectionMatrix,
						 uGLVertex);
}

/******************************************************************************
 * Function Name: ICProcessBIFNfaceforward
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for faceforward
 *				: Syntax		genType faceforward(genType N, genType I, genType Nref)
 *				: Description	If dot (Nref, I)<0 return N otherwise return -N
 *****************************************************************************/
static IMG_VOID ICProcessBIFNfaceforward(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DOT temp0 Nref I		// temp0 (float) = dot(Nref, I)
		SLT	temp1 temp0 0.0		// temp1 (bool) = (dot(Nref, I) < 0.0)
		IF temp1	
			MOV result N		// result = N
		ELSE
			MOV result -N		// result = -N
		ENDIF
	=======================================*/

	GLSLNode *psNNode, *psINode, *psNrefNode;
	GLSLICOperandInfo sNOperand, sIOperand, sNrefOperand;
	IMG_UINT32 uTemp0, uTemp1;
	IMG_UINT32 uZero;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psNNode = psNode->ppsChildren[0];
	psINode = psNode->ppsChildren[1];
	psNrefNode = psNode->ppsChildren[2];

	/* Add temp 0 */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp0))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNfaceforward: Failed to add temp 0 to the symbol table !\n"));
	}

	/* Add temp 1 */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_BOOL, ePrecision, &uTemp1))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNfaceforward: Failed to add temp 1to the symbol table !\n"));
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable,  0.0, ePrecision, IMG_TRUE, &uZero))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNfaceforward: Failed to add float constant 0.0 to the table !\n"));
	}

	/* Process node N, I and Nref */
	ICProcessNodeOperand(psCPD, psICProgram, psNNode, &sNOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psINode, &sIOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psNrefNode, &sNrefOperand);

	/* IC: DOT temp0 Nref I */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_DOT,
						 pszLineStart,
						 uTemp0,
						 &sNrefOperand,
						 &sIOperand);

	/* IC: SLT	temp1 temp0 0.0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_SLT,
						 pszLineStart,
						 uTemp1,
						 uTemp0,
						 uZero);

	/* IC: IF temp1 */
	ICAddICInstruction1a(psCPD, psICProgram,
						 GLSLIC_OP_IF,
						 pszLineStart,
						 uTemp1);

		/* IC: MOV result N */
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, &sNOperand);

	/* IC: ELSE */
	ICAddICInstruction0(psCPD, psICProgram, GLSLIC_OP_ELSE, pszLineStart);

		/* IC: MOV result -N */
		sNOperand.eInstModifier ^= GLSLIC_MODIFIER_NEGATE;
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, &sNOperand);

	/* IC: ENDIF */
	ICAddICInstruction0(psCPD, psICProgram, GLSLIC_OP_ENDIF, pszLineStart);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sNOperand);
	ICFreeOperandOffsetList(&sIOperand);
	ICFreeOperandOffsetList(&sNrefOperand);

}

/******************************************************************************
 * Function Name: ICProcessBIFNreflect
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for reflect
 *				: Syntax		genType reflect(genType I, genType N)
 *				: Description	For the incident vector I and surface orientation
 *								N, returns the reflection direction:
 *								result = I - 2 dot(N, I) * N
 *								N must already be normalized in order to achieve
 *								the desired result.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNreflect(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DOT temp0 N I			// temp0(float) = dot(N, I)
		MUL	temp1 2.0 temp0		// temp1(float) = 2.0 * dot(N, I)
		MUL temp2 temp1 N		// temp2(genType) = 2.0 * dot(N, I) * N
		SUB	result I temp2		// result = I - temp2
	=======================================*/

	GLSLNode *psINode, *psNNode;
	GLSLICOperandInfo sIOperand, sNOperand, sTemp2Operand;
	IMG_UINT32 uTemp0, uTemp1, uTemp2;
	IMG_UINT32 uTwo;
	GLSLTypeSpecifier eIType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psINode = psNode->ppsChildren[0];
	psNNode = psNode->ppsChildren[1];
	eIType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psINode->uSymbolTableID);

	/* Add temp 0 */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp0))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNreflect: Failed to add temp 0 to the symbol table !\n"));
	}

	/* Add temp 1 */
	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, ePrecision, &uTemp1))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNreflect: Failed to add temp 1 to the symbol table !\n"));
	}

	/* Add temp 2 */
	if( !ICAddTemporary(psCPD, psICProgram, eIType, ePrecision, &uTemp2))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNreflect: Failed to add temp 2 to the symbol table !\n"));
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 2.0, ePrecision, IMG_TRUE, &uTwo))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNreflect: Failed to add float constant 0.0 to the table !\n"));
	}

	/* Process node I, N */
	ICProcessNodeOperand(psCPD, psICProgram, psINode, &sIOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psNNode, &sNOperand);

	/* IC: DOT temp0 N I */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_DOT,
						 pszLineStart,
						 uTemp0,
						 &sNOperand,
						 &sIOperand);

	/* IC: MUL	temp1 2.0 temp0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp1,
						 uTwo,
						 uTemp0);

	/* IC: MUL temp2 temp1 N */
	ICAddICInstruction3c(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp2,
						 uTemp1,
						 &sNOperand);

	/* IC: SUB	result I temp2 */
	ICInitOperandInfo(uTemp2, &sTemp2Operand);
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SUB,
						pszLineStart,
						psDestOperand,
						&sIOperand,
						&sTemp2Operand);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sIOperand);
	ICFreeOperandOffsetList(&sNOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNrefract
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for refract
 *				: Syntax		genType refract(genType I, genType N, float eta)
 *				: Description	For the incident vector I and surface normal N,
 *								and the ratio of indices of refraction eta, return
 *								the refraction vector. The returned result is
 *								computed by
 *								k =1.0-eta * eta *(1.0 -dot(N, I)*dot(N, I))
 *								if (k < 0.0)
 *									result = genType(0.0)
 *								else
 *									result = eta * I -(eta * dot(N, I)+sqrt(k)) * N
 *								The input parameters for the incident vector I and
 *								the surface normal N must already be normalized
 *								to get the desired results.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNrefract(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		1	DOT temp0 N I			// temp0 (float) = dot(N, I)
		2	MUL temp1 temp0 temp0	// temp1 (float) = temp0 * temp0 = dot(N, I) * dot(N,I)
		3	SUB temp2 1.0 temp1		// temp2 (float) = 1.0 - temp1 = 1.0 - dot(N, I) * dot(N,I)
		4	MUL temp3 eta eta		// temp3 (float) = eta * eta
		5	MUL	temp4 temp3 temp2	// temp4 (float) = temp3 * temp2 = eta * eta *(1.0 -dot(N, I)*dot(N, I))
		6	SUB	temp5 1.0 temp4		// temp5 (float) = 1.0 - temp4 = k
		7	SLT temp6 temp5 0.0		// temp6 (bool) = (k < 0.0)
		8	IF temp6
		9		MOV result 0.0
		10	ELSE
		11		MUL temp7 eta temp0			// temp7 (float) = eta * dot(N, I)
		12		RSQ temp8 temp5				// temp8 (float) = 1.0/sqrt(temp5) = 1.0/sqrt(k)
		13		RCP temp9 temp8				// temp9 (flaot) = 1.0/temp8 = sqrt(k)
		14		ADD temp10 temp7 temp9		// temp10 (float) = temp7 + temp9 = eta * dot(N,I) + sqrt(k)
		15		MUL temp11 temp10 N			// temp11 (genType) = temp10 * N
		16		MUL temp12 eta I			// temp12 (genType) = eta * I
		17		SUB result temp12 temp11	// result = temp12 - temp11
		18	ENDIF
	=======================================*/

	GLSLNode *psINode, *psNNode, *psEtaNode;
	GLSLICOperandInfo sIOperand, sNOperand, sEtaOperand;
	IMG_UINT32 uTemp[13];
	IMG_UINT32 uZero, uOne, i;
	GLSLTypeSpecifier eGenType, eType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psINode = psNode->ppsChildren[0];
	psNNode = psNode->ppsChildren[1];
	psEtaNode = psNode->ppsChildren[2];
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psINode->uSymbolTableID);

	/* Add temp 0 - 13 */
	for(i = 0; i < 13; i++)
	{
		eType = GLSLTS_FLOAT;

		if(i == 6)
		{
			eType = GLSLTS_BOOL;
		}
		else if(i == 11 || i == 12) 
		{
			eType = eGenType;
		}
		
		if( !ICAddTemporary(psCPD, psICProgram, eType, ePrecision, &uTemp[i]))
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNrefract: Failed to add temp %d to the symbol table !\n", i));
		}
	}

	/* Add constant 0.0 */
	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 0.0, ePrecision, IMG_TRUE, &uZero))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNrefract: Failed to add float constant 0.0 to the table !\n"));
	}

	/* add constant 1.0 */
	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, ePrecision, IMG_TRUE, &uOne))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNrefract: Failed to add float constant 1.0 to the table !\n"));
	}

	/* Process node I, N */
	ICProcessNodeOperand(psCPD, psICProgram, psINode, &sIOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psNNode, &sNOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psEtaNode, &sEtaOperand);

	/*	1	DOT temp0 N I */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_DOT,
						 pszLineStart,
						 uTemp[0],
						 &sNOperand,
						 &sIOperand);

	/*	2	MUL temp1 temp0 temp0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[1],
						 uTemp[0],
						 uTemp[0]);

	/*	3	SUB temp2 1.0 temp1 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 uTemp[2],
						 uOne,
						 uTemp[1]);

	/*	4	MUL temp3 eta eta */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[3],
						 &sEtaOperand,
						 &sEtaOperand);

	/*	5	MUL	temp4 temp3 temp2 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[4],
						 uTemp[3],
						 uTemp[2]);

	/*	6	SUB	temp5 1.0 temp4 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 uTemp[5],
						 uOne,
						 uTemp[4]);

	/*	7	SLT temp6 temp5 0.0 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_SLT,
						 pszLineStart,
						 uTemp[6],
						 uTemp[5],
						 uZero);

	/*	8	IF temp6 */
	ICAddICInstruction1a(psCPD, psICProgram,
						 GLSLIC_OP_IF,
						 pszLineStart,
						 uTemp[6]);

	/*	9	MOV result 0.0 */
	ICAddICInstruction2c(psCPD, psICProgram,
						 GLSLIC_OP_MOV,
						 pszLineStart,
						 psDestOperand,
						 uZero);

	/*	10	ELSE */
	ICAddICInstruction0(psCPD, psICProgram,
						 GLSLIC_OP_ELSE,
						 pszLineStart);

	/*	11		MUL temp7 eta temp0 */
	ICAddICInstruction3b(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[7],
						 &sEtaOperand,
						 uTemp[0]);

	/*	12		RSQ temp8 temp5	 */
	ICAddICInstruction2b(psCPD, psICProgram,
						 GLSLIC_OP_RSQ,
						 pszLineStart,
						 uTemp[8],
						 uTemp[5]);

	/*	13		RCP temp9 temp8 */
	ICAddICInstruction2b(psCPD, psICProgram,
						 GLSLIC_OP_RCP,
						 pszLineStart,
						 uTemp[9],
						 uTemp[8]);

	/*	14		ADD temp10 temp7 temp9 */
	ICAddICInstruction3d(psCPD, psICProgram,
						 GLSLIC_OP_ADD,
						 pszLineStart,
						 uTemp[10],
						 uTemp[7],
						 uTemp[9]);

	/*	15		MUL temp11 temp10 N */
	ICAddICInstruction3c(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[11],
						 uTemp[10],
						 &sNOperand);

	/*	16		MUL temp12 eta I */
	ICAddICInstruction3a(psCPD, psICProgram,
						 GLSLIC_OP_MUL,
						 pszLineStart,
						 uTemp[12],
						 &sEtaOperand,
						 &sIOperand);

	/*	17		SUB result temp12 temp11 */
	ICAddICInstruction3e(psCPD, psICProgram,
						 GLSLIC_OP_SUB,
						 pszLineStart,
						 psDestOperand,
						 uTemp[12],
						 uTemp[11]);

	/*	18	ENDIF */
	ICAddICInstruction0(psCPD, psICProgram,
						 GLSLIC_OP_ENDIF,
						 pszLineStart);

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sIOperand);
	ICFreeOperandOffsetList(&sNOperand);
	ICFreeOperandOffsetList(&sEtaOperand);

}

/******************************************************************************
 * Function Name: ICProcessBIFNmatrixCompMult
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for matrixCompMult
 *				: Syntax		mat matrixCompMult(mat x, mat y)
 *				: Description	Multiply matrix x by matrix y component-wise,
 *								i.e., result[i][j] is the scalar product of x[i][j] and
 *								y[i][j].
 *								Note: to get linear algebraic matrix
 *								multiplication, use the multiply operator (*).
 *****************************************************************************/

static IMG_VOID ICProcessBIFNmatrixCompMult(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		MUL result[0] x[0] y[0]			
		MUL result[1] x[1] y[1]			
		MUL result[2] x[2] y[2]
		MUL result[3] x[3] y[3]
	=======================================*/

	GLSLNode *psXNode, *psYNode;
	GLSLICOperandInfo sXOperand, sYOperand;
	IMG_UINT32 uNumColumns;
	GLSLTypeSpecifier eXType;
	IMG_UINT32 i;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x and y */
	psXNode = psNode->ppsChildren[0];
	psYNode = psNode->ppsChildren[1];
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	DebugAssert(GLSL_IS_MATRIX(eXType));

	/* Process node x, y */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psYNode, &sYOperand);

	uNumColumns = TYPESPECIFIER_NUM_COLS(eXType);

	/* Add an offset to indicate colum access of a matrix */
	ICAddOperandOffset(&sXOperand, 0, 0);
	ICAddOperandOffset(&sYOperand, 0, 0);
	ICAddOperandOffset(psDestOperand, 0, 0);

	for(i = 0; i < uNumColumns; i++)
	{
		/* Modify the last offset */
		ICAdjustOperandLastOffset(&sXOperand, i, 0);
		ICAdjustOperandLastOffset(&sYOperand, i, 0);
		ICAdjustOperandLastOffset(psDestOperand, i, 0);

		/* IC: MUL result[i] x[i] y[i]	 */
		ICAddICInstruction3(psCPD, psICProgram,
							 GLSLIC_OP_MUL,
							 pszLineStart,
							 psDestOperand,
							 &sXOperand,
							 &sYOperand);

	}

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sYOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNtranspose
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for transpose()
 *				: Syntax		mat transpose(mat x)
 *				: Description	Provides transpose of matrix
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtranspose(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		for(i = 0; i < uSourceRows; i++)
		{
			for(j = 0; j < uSourceCols; j++)
			{
				MOV result[[i][j] x[j][i]
			}
		}
	=======================================*/

	GLSLNode *psXNode;
	GLSLICOperandInfo sXOperand;
	IMG_UINT32 uSourceCols, uSourceRows;
	GLSLTypeSpecifier eXType;
	IMG_UINT32 i, j;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLICOperandOffset *psXLastOffset, *psResultLastOffset; 
	GLSLICOperandOffset *psXSecOffsetFromLast, *psResultSecOffsetFromLast;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x and y */
	psXNode = psNode->ppsChildren[0];
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	DebugAssert(GLSL_IS_MATRIX(eXType));

	/* Process node x, y */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	uSourceCols = TYPESPECIFIER_NUM_COLS(eXType);
	uSourceRows = TYPESPECIFIER_NUM_ROWS(eXType);

	/* Add an offset to indicate colum access of a matrix */
	ICAddOperandOffset(&sXOperand, 0, 0);
	ICAddOperandOffset(psDestOperand, 0, 0);
	psXSecOffsetFromLast = &sXOperand.psOffsetListEnd->sOperandOffset;
	psResultSecOffsetFromLast = &psDestOperand->psOffsetListEnd->sOperandOffset;

	/* Add an offset to indicate component access of a column */
	ICAddOperandOffset(&sXOperand, 0, 0);
	ICAddOperandOffset(psDestOperand, 0, 0);
	psXLastOffset = &sXOperand.psOffsetListEnd->sOperandOffset;
	psResultLastOffset = &psDestOperand->psOffsetListEnd->sOperandOffset;

	for(i = 0; i < uSourceRows; i++)
	{
		for(j = 0; j < uSourceCols; j++)
		{
			/* Modify the last two offsets */
			psResultSecOffsetFromLast->uStaticOffset = i;
			psResultLastOffset->uStaticOffset = j;

			/* x[i][j] */
			psXSecOffsetFromLast->uStaticOffset = j;
			psXLastOffset->uStaticOffset = i;

			/* IC: MOV result[i][j] x[j][i]	 */
			ICAddICInstruction2(psCPD, psICProgram,
								 GLSLIC_OP_MOV,
								 pszLineStart,
								 psDestOperand,
								 &sXOperand);
		}
	}

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sXOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNouterProduct
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for outerProduct()
 *				: Syntax		mat transpose(vec c, vec r)
 *				: Description	Treats the first parameter c as a column vector (matrix
 *                              with one column) and the second parameter r as a row
 *                              vector (matrix with one row) and does a linear algebraic
 *                              matrix multiply c * r, yielding a matrix whose number of
 *                              rows is the number of components in c and whose
 *                              number of columns is the number of components in r.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNouterProduct(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		uCols = num of components in r
		uRows = num of components in c
		for(i = 0; i < uCols; i++)
		{
			for(j = 0; j < uRows; j++)
			{
				MUL result[i][j] x[j] y[i]
			}
		}
	=======================================*/

	GLSLNode *psXNode, *psYNode;
	GLSLICOperandInfo sXOperand, sYOperand;
	IMG_UINT32 uCols, uRows;
	GLSLTypeSpecifier eXType, eYType;
	IMG_UINT32 i, j;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLICOperandOffset *psXLastOffset, *psYLastOffset; 
	GLSLICOperandOffset *psResultSecOffsetFromLast, *psResultLastOffset;

	PVR_UNREFERENCED_PARAMETER(psCPD);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Operand X */
	psXNode = psNode->ppsChildren[0];
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	uRows = TYPESPECIFIER_NUM_COMPONENTS(eXType);

	/* Operand Y */
	psYNode = psNode->ppsChildren[1];
	ICProcessNodeOperand(psCPD, psICProgram, psYNode, &sYOperand);
	eYType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psYNode->uSymbolTableID);
	uCols = TYPESPECIFIER_NUM_COMPONENTS(eYType);

	/* Add two offsets to indicate component access of the result */
	ICAddOperandOffset(psDestOperand, 0, 0);
	psResultSecOffsetFromLast = &psDestOperand->psOffsetListEnd->sOperandOffset;
	ICAddOperandOffset(psDestOperand, 0, 0);
	psResultLastOffset = &psDestOperand->psOffsetListEnd->sOperandOffset;

	/* Add offset to indicate component access of X */
	ICAddOperandOffset(&sXOperand, 0, 0);
	psXLastOffset = &sXOperand.psOffsetListEnd->sOperandOffset;

	ICAddOperandOffset(&sYOperand, 0, 0);
	psYLastOffset = &sYOperand.psOffsetListEnd->sOperandOffset;

	for(i = 0; i < uCols; i++)
	{
		for(j = 0; j < uRows; j++)
		{
			/* result[i][j] */
			psResultSecOffsetFromLast->uStaticOffset = i;
			psResultLastOffset->uStaticOffset = j;

			/* x[j] */
			psXLastOffset->uStaticOffset = j;

			/* y[i] */
			psYLastOffset->uStaticOffset = i;

			/* IC: MUL result[i][j] x[j][i]	 */
			ICAddICInstruction3(psCPD, psICProgram,
								GLSLIC_OP_MUL,
								pszLineStart,
								psDestOperand,
								&sXOperand,
								&sYOperand);
		}

	}

	/* Free all operand offset lists */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sYOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNlessThan
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for lessThan
 *				: Syntax(1)		bvec lessThan(vec x, vec y)
 *				: Syntax(2)		bvec lessThan(ivec x, ivec y)
 *				: Description	Returns the component-wise compare of x < y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNlessThan(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
		SLT result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SLT);

}

/******************************************************************************
 * Function Name: ICProcessBIFNlessThanEqual
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for lessThanEqual
 *				: Syntax(1)		bvec lessThanEqual(vec x, vec y)
 *				: Syntax(2)		bvec lessThanEqual(ivec x, ivec y)
 *				: Description	Returns the component-wise compare of x <= y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNlessThanEqual(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SLE result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SLE);
}

/******************************************************************************
 * Function Name: ICProcessBIFNgreaterThan
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for greaterThan
 *				: Syntax(1)		bvec greaterThan(vec x, vec y)
 *				: Syntax(2)		bvec greaterThan(ivec x, ivec y)
 *				: Description	Returns the component-wise compare of x > y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNgreaterThan(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SGT result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SGT);
}

/******************************************************************************
 * Function Name: ICProcessBIFNgreaterThanEqual
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for greaterThanEqual
 *				: Syntax(1)		bvec greaterThanEqual(vec x, vec y)
 *				: Syntax(2)		bvec greaterThanEqual(ivec x, ivec y)
 *				: Description	Returns the component-wise compare of x >= y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNgreaterThanEqual(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SGE result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SGE);
}

/******************************************************************************
 * Function Name: ICProcessBIFNequal
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for equal
 *				: Syntax(1)		bvec equal(vec x, vec y)
 *				: Syntax(2)		bvec equal(ivec x, ivec y)
 *				: Syntax(3)		bvec equal(bvec x,bvec y)
 *				: Description	Returns the component-wise compare of x == y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNequal(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SEQ result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SEQ);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnotEqual
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for notEqual
 *				: Syntax(1)		bvec notEqual(vec x, vec y)
 *				: Syntax(2)		bvec notEqual(ivec x,ivec y)
 *				: Syntax(3)		bvec notEqual(bvec x, bvec y)
 *				: Description	Returns the component-wise compare of x != y.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnotEqual(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SNE result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SNE);
}

/******************************************************************************
 * Function Name: ICProcessBIFNany
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for any
 *				: Syntax		bool any(bvec x)
 *				: Description	Returns true if any component of x is true.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNany(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SANY result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SANY);
}

/******************************************************************************
 * Function Name: ICProcessBIFNall
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for all
 *				: Syntax		bool all(bvec x)
 *				: Description	Returns true only if all components of x are true.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNall(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		SALL result x y			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_SALL);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnot
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for not
 *				: Syntax		bvec not(bvec x)
 *				: Description	Returns the component-wise logical complement of x.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnot(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		NOT result x			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_NOT);
}



/******************************************************************************
 * Function Name: ICProcessBIFNtextureProj
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureProj and related functions
 * Syntax		: vec4 texture1DProj (sampler1D sampler, vec2 coord [, float bias] )
 *				: vec4 texture1DProj (sampler1D sampler, vec4 coord [, float bias] )
 *				: vec4 texture2DProj (sampler2D sampler, vec3 coord [, float bias] )
 *				: vec4 texture2DProj (sampler2D sampler, vec4 coord [, float bias] )
 *				: vec4 texture3DProj (sampler3D sampler, vec4 coord [, float bias] )
 *				: gvec4 textureProj (gsampler1D sampler, vec2 P [, float bias] )
 *				: gvec4 textureProj (gsampler1D sampler, vec4 P [, float bias] )
 *				: gvec4 textureProj (gsampler2D sampler, vec3 P [, float bias] )
 *				: gvec4 textureProj (gsampler2D sampler, vec4 P [, float bias] )
 *				: gvec4 textureProj (gsampler3D sampler, vec4 P [, float bias] )
 *				: float textureProj (sampler1DShadow sampler, vec4 P[, float bias] )
 *				: float textureProj (sampler2DShadow sampler, vec4 P[, float bias] )
 *				: vec4 shadow1DProj (sampler1DShadow sampler,vec4 coord [, float bias])
 *				: vec4 shadow2DProj (sampler2DShadow sampler,vec4 coord [, float bias])
 *				: vec4 textureStreamProjIMG (samplerStreamIMG sampler, vec3 coord)
 *				: vec4 textureStreamProjIMG (samplerStreamIMG sampler, vec4 coord)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureProj(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTUREPROJ);
	}
	else
	{
		if(psNode->uNumChildren == 2)
		{
			/*=======================================
				TEXLDP  result sampler coord -- no bias
			=======================================*/

			/* No bias provided */
			MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDP);
		}
		else
		{
			/*=======================================
				DIV temp coord.swiz1 coord.swiz2 			// temp (float vec type, size = texture dim)
				TEXLDB result sampler temp bias
			=======================================*/
			ICEmulateTextureProj(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDB);
		}
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNtextureLod
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureLod and related functions
 * Syntax		: vec4 texture1DLod (sampler1D sampler, float coord, float lod)
 *				: vec4 texture2DLod (sampler2D sampler, vec2 coord, float lod)
 *				: vec4 texture3DLod (sampler3D sampler, vec3 coord, float lod)
 *				: vec4 textureCubeLod (samplerCube sampler, vec3 coord, float lod)
 *				: gvec4 textureLod (gsampler1D sampler, float P, float lod)
 *				: gvec4 textureLod (gsampler2D sampler, vec2 P, float lod)
 *				: gvec4 textureLod (gsampler3D sampler, vec3 P, float lod)
 *				: gvec4 textureLod (gsamplerCube sampler, vec3 P, float lod)
 *				: float textureLod (sampler1DShadow sampler, vec3 P, float lod)
 *				: float textureLod (sampler2DShadow sampler, vec3 P, float lod)
 *				: gvec4 textureLod (gsampler1DArray sampler, vec2 P, float lod)
 *				: gvec4 textureLod (gsampler2DArray sampler, vec3 P, float lod)
 *				: float textureLod (sampler1DArrayShadow sampler, vec3 P, float lod)
 *				: vec4 shadow1DLod (sampler1DShadow sampler,vec3 coord,float lod)
 *				: vec4 shadow2DLod (sampler2DShadow sampler,vec3 coord,float lod)

 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureLod(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTURELOD);
	}
	else
	{
		/*=======================================
			TEXLDL result sampler coord lod
		=======================================*/

		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDL);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNtextureProjLod
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureProjLod etc
 *				: Syntax		vec4 texture1DProjLod (sampler1D sampler, vec2 coord,float lod)
 *				:				vec4 texture1DProjLod (sampler1D sampler, vec4 coord,float lod)
 *				: 				vec4 texture2DProjLod (sampler2D sampler, vec3 coord,float lod)
 *				:				vec4 texture2DProjLod (sampler2D sampler,vec4 coord,float lod)
 *				:				vec4 texture3DProjLod (sampler3D sampler,vec4 coord,float lod)
 *				:				gvec4 textureProjLod (gsampler1D sampler, vec2 P, float lod)
 *				:				gvec4 textureProjLod (gsampler1D sampler, vec4 P, float lod)
 *				:				gvec4 textureProjLod (gsampler2D sampler, vec3 P, float lod)
 *				:				gvec4 textureProjLod (gsampler2D sampler, vec4 P, float lod)
 *				:				gvec4 textureProjLod (gsampler3D sampler, vec4 P, float lod)
 *				:				float textureProjLod (sampler1DShadow sampler, vec4 P, float lod)
 *				:				float textureProjLod (sampler2DShadow sampler, vec4 P, float lod)
 *				:				vec4 shadow1DProjLod(sampler1DShadow sampler, vec4 coord,float lod)
 *				:				vec4 shadow2DProjLod(sampler2DShadow sampler, vec4 coord, float lod)
 *				: Description	refer to textureProjLod
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureProjLod(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTUREPROJLOD);
	}
	else
	{
		/*=======================================
			DIV		temp coord.swiz1 coord.swiz2 
			TEXLDL	result sampler temp lod
		=======================================*/

		ICEmulateTextureProj(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDL);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNtexture
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for texture sampling code that is common to several GLSL function forms
 *				: Syntax		vec4 shadow1D (sampler1DShadow sampler,vec3 coord [, float bias])
 *				:				vec4 shadow2D (sampler2DShadow sampler,vec3 coord [, float bias])
 *				:				float texture (samplerCubeShadow sampler, vec4 P [, float bias] )
 *				:				float texture (sampler1DArrayShadow sampler, vec3 P [, float bias] )
 *				:				float texture (sampler2DArrayShadow sampler, vec4 P [, float bias])
 *				:				vec4 textureStreamIMG (samplerStream sampler, vec2 coord)
 *				:				
 *				: Description	Use texture coordinate coord to do a depth
 *								comparison lookup on the depth texture
 *								bound to sampler, as described in section
 *								3.8.14 of version 1.4 of the OpenGL
 *								specification. The 3rd component of coord
 *								(coord.p) is used as the R value. The texture
 *								bound to sampler must be a depth texture,
 *								or results are undefined. For the projective
 *								("Proj") version of each built-in, the texture
 *								coordinate is divide by coord.q, giving a
 *								depth value R of coord.p/coord.q. The
 *								second component of coord is ignored for
 *								the "1D" variants.

 *****************************************************************************/
static IMG_VOID ICProcessBIFNtexture(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTURE);
	}
	else
	{
		if(psNode->uNumChildren == 2)
		{
			/*=======================================
				TEXLD result sampler coord
			=======================================*/		

			/* No bias provided */
			MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLD);
		}
		else
		{
			/*=======================================
				TEXLDB result sampler coord bias
			=======================================*/

			/* Lod bias provided */
			DebugAssert(psNode->uNumChildren == 3);
			MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDB);
		}
	}
}

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
/******************************************************************************
 * Function Name: ICProcessBIFNtextureRect
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for texture sampling code that is common to several GLSL function forms
 *				: Syntax		vec4 texture2DRect(sampler2DRect sampler, vec2 coord)
 *				:				vec4 shadow2DRect(sampler2DRectShadow sampler, vec3 coord)
 *				:				
 *				: Description	Use texture coordinate coord to do a depth
 *								comparison lookup on the depth texture
 *								bound to sampler, as described in section
 *								3.8.14 of version 1.4 of the OpenGL
 *								specification. The 3rd component of coord
 *								(coord.p) is used as the R value. The texture
 *								bound to sampler must be a depth texture,
 *								or results are undefined. For the projective
 *								("Proj") version of each built-in, the texture
 *								coordinate is divide by coord.q, giving a
 *								depth value R of coord.p/coord.q. The
 *								second component of coord is ignored for
 *								the "1D" variants.

 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureRect(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTURERECT);
	}
	else
	{
		DebugAssert(psNode->uNumChildren == 2)
		/*=======================================
			DIV		temp coord dims
			TEXLD	result sampler temp
		=======================================*/		

		/* No bias provided */
		ICEmulateTextureRect(psCPD, psICProgram, psNode, psDestOperand);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNtextureRectProj
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureProj and related functions
 * Syntax		: vec4 texture2DRectProj (sampler2D sampler, vec3 coord)
 *				: vec4 texture2DRectProj (sampler2D sampler, vec4 coord)
 *				: vec4 shadow2DRectProj(sampler2DRectShadow sampler, vec4 coord)
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureRectProj(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTURERECTPROJ);
	}
	else
	{
		DebugAssert(psNode->uNumChildren == 2);
		/*=======================================
			DIV		temp coord dims
			TEXLDP  result sampler temp -- no bias
		=======================================*/
		
		/* No bias provided */
		//ICEmulateTextureRectProj(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTURERECT);
		ICEmulateTextureRect(psCPD, psICProgram, psNode, psDestOperand);
	}
}

#endif

/******************************************************************************
 * Function Name: ICProcessBIFNdFdx
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for dFdx
 *				: Syntax		genType dFdx (genType p)
 *				: Description	Returns the derivative in x using local
 *								differencing for the input argument p.
 *****************************************************************************/

static IMG_VOID ICProcessBIFNdFdx(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DFDX result p			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_DFDX);
}

/******************************************************************************
 * Function Name: ICProcessBIFNdFdy
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for dFdy
 *				: Syntax		genType dFdy (genType p)
 *				: Description	Returns the derivative in y using local
 *								differencing for the input argument p.
 *								These two functions are commonly used to estimate
 *								the filter width used to anti-alias procedural
 *								textures.We are assuming that the expression is being
 *								evaluated in parallelon a SIMDarray so thatatany
 *								given point in time the value of the function is known
 *								at the grid points represented by the SIMD array.
 *								Local differencing between SIMD array elements can
 *								therefore be used to derive dFdx, dFdy, etc.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNdFdy(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DFDY result p			
	=======================================*/

	PVR_UNREFERENCED_PARAMETER(psCPD);

	MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_DFDY);
}

/******************************************************************************
 * Function Name: ICProcessBIFNfwidth
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for fwidth
 *				: Syntax		genType fwidth (genType p)
 *				: Description	Returns the sum of the absolute derivative in x
 *								and y using local differencing for the input
 *								argument p, i.e.:
 *								return = abs (dFdx (p)) + abs (dFdy (p));
 *****************************************************************************/
static IMG_VOID ICProcessBIFNfwidth(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		1	DFDX temp0 p			// temp0(genType) = dFdx(p)
		2	ABS	 temp1 temp0		// temp1 (genType) = abs(dFdx(p))
		3	DFDY temp2 p			// temp2 (genType) = dFdy(p)
		4	ABS	 temp3 temp2		// temp3 (genType) = abs(dFdy(p))
		5	ADD  result temp1 temp3
	=======================================*/

	GLSLNode *psPNode;
	GLSLICOperandInfo sPOperand;
	IMG_UINT32 uTemp[4], i;
	GLSLTypeSpecifier eGenType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	psPNode = psNode->ppsChildren[0];
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psPNode->uSymbolTableID);

	/* Add temps */
	for(i = 0; i < 4; i++)
	{
		if( !ICAddTemporary(psCPD, psICProgram, eGenType, ePrecision, &uTemp[i]))
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNfwidth: Failed to add temp %d to the symbol table !\n", i));
			return;
		}
	}

	/* Process paramenter x, and y operands */
	ICProcessNodeOperand(psCPD, psICProgram, psPNode, &sPOperand);

	/*	1	DFDX temp0 p */
	ICAddICInstruction2a(psCPD, psICProgram,
						 GLSLIC_OP_DFDX,
						 pszLineStart,
						 uTemp[0],
						 &sPOperand);

	/*	2	ABS	 temp1 temp0 */
	ICAddICInstruction2b(psCPD, psICProgram,
						 GLSLIC_OP_ABS,
						 pszLineStart,
						 uTemp[1],
						 uTemp[0]);

	/*	3	DFDY temp2 p */
	ICAddICInstruction2a(psCPD, psICProgram,
						 GLSLIC_OP_DFDY,
						 pszLineStart,
						 uTemp[2],
						 &sPOperand);

	/*	4	ABS	 temp3 temp2 */
	ICAddICInstruction2b(psCPD, psICProgram,
						 GLSLIC_OP_ABS,
						 pszLineStart,
						 uTemp[3],
						 uTemp[2]);

	/*	5	ADD  result temp1 temp3 */
	ICAddICInstruction3e(psCPD, psICProgram,
						 GLSLIC_OP_ADD,
						 pszLineStart,
						 psDestOperand, 
						 uTemp[1],
						 uTemp[3]);


	/* free all offsets attached */
	ICFreeOperandOffsetList(&sPOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnoise1
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for noise1
 *				: Syntax		float noise1 (genType x)
 *				: Description   Returns a 1D noise value based on the input value x.
 *****************************************************************************/

static IMG_VOID ICProcessBIFNnoise1(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	ICEmulateBIFNnoise1(psCPD, psICProgram, psNode, psDestOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnoise2
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for noise2
 *				: Syntax		vec2 noise2 (genType x)
 *				: Description	Returns a 2D noise value based on the input value x.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnoise2(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	ICEmulateBIFNnoise2(psCPD, psICProgram, psNode, psDestOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnoise3
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for noise3
 *				: Syntax		vec3 noise3 (genType x)
 *				: Description   Returns a 3D noise value based on the input value x.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnoise3(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	ICEmulateBIFNnoise3(psCPD, psICProgram, psNode, psDestOperand);
}

/******************************************************************************
 * Function Name: ICProcessBIFNnoise4
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for noise4
 *				: Syntax		vec4 noise4 (genType x)
 *				: Description	Returns a 4D noise value based on the input value x.
 *****************************************************************************/
static IMG_VOID ICProcessBIFNnoise4(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	ICEmulateBIFNnoise4(psCPD, psICProgram, psNode, psDestOperand);
}


/******************************************************************************
 * Function Name: ICProcessBIFNtextureGrad
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureGrad
 * Syntax		: gvec4 textureGrad (gsampler1D sampler, float P,float dPdx, float dPdy)
 *				: gvec4 textureGrad (gsampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy)
 *				: gvec4 textureGrad (gsampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy)
 *				: gvec4 textureGrad (gsamplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy)
 *				: float textureGrad (sampler1DShadow sampler, vec3 P, float dPdx, float dPdy)
 *				: float textureGrad (sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy)
 *				: float textureGrad (samplerCubeShadow sampler, vec4 P, vec3 dPdx, vec3 dPdy)
 *				: gvec4 textureGrad (gsampler1DArray sampler, vec2 P, float dPdx, float dPdy)
 *				: gvec4 textureGrad (gsampler2DArray sampler, vec3 P, vec2 dPdx, vec2 dPdy)
 *				: float textureGrad (sampler1DArrayShadow sampler, vec3 P, float dPdx, float dPdy)
 *				: float textureGrad (sampler2DArrayShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)
 *				: vec4 shadow1DGradARB(sampler1DShadow sampler, vec3 P, float dPdx, float dPdy);
 *				: vec4 shadow2DGradARB(sampler2DShadow sampler, vec3 P, vec2 dPdx, vec2 dPdy);
 *				: vec4 texture1DGradARB(sampler1D sampler, float P, float dPdx, float dPdy);
 *				: vec4 texture2DGradARB(sampler2D sampler, vec2 P, vec2 dPdx, vec2 dPdy);
 *				: vec4 texture3DGradARB(sampler3D sampler, vec3 P, vec3 dPdx, vec3 dPdy );
 *				: vec4 textureCubeGradARB(samplerCube sampler, vec3 P, vec3 dPdx, vec3 dPdy);

 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureGrad(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTUREGRAD);
	}
	else
	{
		MAP_BIFN_TO_ICINSTR(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDD);
	}
}

/******************************************************************************
 * Function Name: ICProcessBIFNtextureProjGrad
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generates IC instructions for textureProjGrad
 * Syntax		: gvec4 textureProjGrad (gsampler1D sampler, vec2 P, float dPdx, float dPdy)
 *				: gvec4 textureProjGrad (gsampler1D sampler, vec4 P, float dPdx, float dPdy)
 *				: gvec4 textureProjGrad (gsampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy)
 *				: gvec4 textureProjGrad (gsampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy)
 *				: gvec4 textureProjGrad (gsampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy)
 *				: float textureProjGrad (sampler1DShadow sampler, vec4 P, float dPdx, float dPdy)
 *				: float textureProjGrad (sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy)
 *				: vec4 texture1DProjGradARB(sampler2D sampler, vec2  P, float dPdx, float dPdy);
 *				: vec4 texture1DProjGradARB(sampler2D sampler, vec4  P, float dPdx, float dPdy);
 *				: vec4 texture2DProjGradARB(sampler2D sampler, vec3 P, vec2 dPdx, vec2 dPdy);
 *				: vec4 texture2DProjGradARB(sampler2D sampler, vec4 P, vec2 dPdx, vec2 dPdy);
 *				: vec4 texture3DProjGradARB(sampler3D sampler, vec4 P, vec3 dPdx, vec3 dPdy );
 *				: vec4 shadow1DProjGradARB(sampler1DShadow sampler, vec4 P, float dPdx, float dPdy);
 *				: vec4 shadow2DProjGradARB(sampler2DShadow sampler, vec4 P, vec2 dPdx, vec2 dPdy);
 *****************************************************************************/
static IMG_VOID ICProcessBIFNtextureProjGrad(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, GLSLNode *psNode, GLSLICOperandInfo *psDestOperand)
{
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);
	if(GLSL_IS_DEPTH_TEXTURE(psSamplerType->eTypeSpecifier))
	{
		ICEmulateBIFNshadowFunc(psCPD, psICProgram, psNode, psDestOperand, GLSLBFID_TEXTUREPROJGRAD);
	}
	else
	{
		ICEmulateTextureProj(psCPD, psICProgram, psNode, psDestOperand, GLSLIC_OP_TEXLDD);
	}
}

/* 
** Built-in function table, any enum name order changes(GLSLBuiltInFunctionID) in glsltree.h 
** should be ported in this table 
*/
IMG_INTERNAL const GLSLBuiltInFunctionInfo asGLSLBuiltInFunctionInfoTable[GLSLBFID_NOT_BUILT_IN + 1] = 
{
	/*									bReducibleToConst	bUseGradients		bTextureSampler		pfnProcessBuiltInFunction	*/
	{GLSLBFID_(RADIANS)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNradians,				},
	{GLSLBFID_(DEGREES)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNdegrees,				},
	{GLSLBFID_(SIN)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNsin,					},
	{GLSLBFID_(COS)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNcos,					},
	{GLSLBFID_(TAN)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNtan,					},
	{GLSLBFID_(ASIN)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNasin,					},
	{GLSLBFID_(ACOS)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNacos,					},
	{GLSLBFID_(ATAN)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNatan,					},
	{GLSLBFID_(POW)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNpow,					},
	{GLSLBFID_(EXP)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNexp,					},
	{GLSLBFID_(LOG)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNlog,					},
	{GLSLBFID_(EXP2)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNexp2,					},
	{GLSLBFID_(LOG2)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNlog2,					},
	{GLSLBFID_(SQRT)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNsqrt,					},
	{GLSLBFID_(INVERSESQRT)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNinversesqrt,			},
	{GLSLBFID_(ABS)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNabs,					},
	{GLSLBFID_(SIGN)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNsign,					},
	{GLSLBFID_(FLOOR)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNfloor,					},
	{GLSLBFID_(CEIL)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNceil,					},
	{GLSLBFID_(FRACT)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNfract,					},
	{GLSLBFID_(MOD)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNmod,					},
	{GLSLBFID_(MIN)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNmin,					},
	{GLSLBFID_(MAX)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNmax,					},
	{GLSLBFID_(CLAMP)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNclamp,					},
	{GLSLBFID_(MIX)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNmix,					},
	{GLSLBFID_(STEP)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNstep,					},
	{GLSLBFID_(SMOOTHSTEP)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNsmoothstep,			},
	{GLSLBFID_(LENGTH)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNlength,				},
	{GLSLBFID_(DISTANCE)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNdistance,				},
	{GLSLBFID_(DOT)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNdot,					},
	{GLSLBFID_(CROSS)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNcross,					},
	{GLSLBFID_(NORMALIZE)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnormalize,				},
	{GLSLBFID_(FTRANSFORM)				IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNftransform,			},
	{GLSLBFID_(FACEFORWARD)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNfaceforward,			},
	{GLSLBFID_(REFLECT)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNreflect,				},
	{GLSLBFID_(REFRACT)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNrefract,				},
	{GLSLBFID_(MATRIXCOMPMULT)			IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNmatrixCompMult,		},
	{GLSLBFID_(TRANSPOSE)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNtranspose,     		},
	{GLSLBFID_(OUTERPRODUCT)			IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNouterProduct,			},
	{GLSLBFID_(LESSTHAN)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNlessThan,				},
	{GLSLBFID_(LESSTHANEQUAL)			IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNlessThanEqual,			},
	{GLSLBFID_(GREATERTHAN)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNgreaterThan,			},
	{GLSLBFID_(GREATERTHANEQUAL)		IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNgreaterThanEqual,		},
	{GLSLBFID_(EQUAL)					IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNequal,					},
	{GLSLBFID_(NOTEQUAL)				IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnotEqual,				},
	{GLSLBFID_(ANY)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNany,					},
	{GLSLBFID_(ALL)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNall,					},
	{GLSLBFID_(NOT)						IMG_TRUE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnot,					},
	{GLSLBFID_(TEXTURE)					IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtexture,				},
	{GLSLBFID_(TEXTUREPROJ)				IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtextureProj,			},
	{GLSLBFID_(TEXTURELOD)				IMG_FALSE,			IMG_FALSE,			IMG_TRUE,			ICProcessBIFNtextureLod,			},
	{GLSLBFID_(TEXTUREPROJLOD)			IMG_FALSE,			IMG_FALSE,			IMG_TRUE,			ICProcessBIFNtextureProjLod,		},
	{GLSLBFID_(TEXTUREGRAD)				IMG_FALSE,			IMG_FALSE,			IMG_TRUE,			ICProcessBIFNtextureGrad,			},
	{GLSLBFID_(TEXTUREPROJGRAD)			IMG_FALSE,			IMG_FALSE,			IMG_TRUE,			ICProcessBIFNtextureProjGrad,		},
	{GLSLBFID_(TEXTURESTREAM)			IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtexture,				},
	{GLSLBFID_(TEXTURESTREAMPROJ)		IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtextureProj,			},
#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
	{GLSLBFID_(TEXTURERECT)				IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtextureRect,			},
	{GLSLBFID_(TEXTURERECTPROJ)			IMG_FALSE,			IMG_TRUE,			IMG_TRUE,			ICProcessBIFNtextureRectProj,		},
#endif
	{GLSLBFID_(DFDX)					IMG_TRUE,			IMG_TRUE,			IMG_FALSE,			ICProcessBIFNdFdx,					},
	{GLSLBFID_(DFDY)					IMG_TRUE,			IMG_TRUE,			IMG_FALSE,			ICProcessBIFNdFdy,					},
	{GLSLBFID_(FWIDTH)					IMG_TRUE,			IMG_TRUE,			IMG_FALSE,			ICProcessBIFNfwidth,				},
	{GLSLBFID_(NOISE1)					IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnoise1,				},
	{GLSLBFID_(NOISE2)					IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnoise2,				},
	{GLSLBFID_(NOISE3)					IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnoise3,				},
	{GLSLBFID_(NOISE4)					IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			ICProcessBIFNnoise4,				},
	{GLSLBFID_(NOT_BUILT_IN)			IMG_FALSE,			IMG_FALSE,			IMG_FALSE,			IMG_NULL,							},
};

/******************************************************************************
 * Function Name: ICProcessBuiltInFunctionCall
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate IC for built-in functions	
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICProcessBuiltInFunctionCall(GLSLCompilerPrivateData *psCPD,
												   GLSLICProgram *psICProgram, 
												   GLSLNode *psNode, 
												   GLSLICOperandInfo *psDestOperand)
{
	GLSLFunctionCallData		*psFunctionCallData;
	GLSLFunctionDefinitionData	*psFunctionDefinitionData;
	PFNProcessBuiltInFunction	BuiltInFuncProc;

	/* Get function call data */
	psFunctionCallData = (GLSLFunctionCallData *)GetSymbolTableData(psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get function definition data */
	psFunctionDefinitionData = (GLSLFunctionDefinitionData *)GetSymbolTableData(psICProgram->psSymbolTable, psFunctionCallData->uFunctionDefinitionSymbolID);

	/* Pick which built in function to process */
	DebugAssert(GLSLBuiltInFunctionID(psFunctionDefinitionData->eBuiltInFunctionID) == psFunctionDefinitionData->eBuiltInFunctionID);
	BuiltInFuncProc = GLSLBuiltInFunctionProcessFunction(psFunctionDefinitionData->eBuiltInFunctionID);

	/* Generate inline IC code for built-in function call */
	(*BuiltInFuncProc)(psCPD, psICProgram, psNode, psDestOperand);
}

/******************************************************************************
 End of file (icbuiltin.c)
******************************************************************************/

