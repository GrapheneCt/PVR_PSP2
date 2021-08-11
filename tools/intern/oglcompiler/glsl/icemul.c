/******************************************************************************
 * Name         : icemul.c
 * Created      : 11/03/2005
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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
 * $Log: icemul.c $
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "glsltree.h"
#include "icode.h"
#include "common.h"
#include "icbuiltin.h"
#include "../parser/debug.h"
#include "../parser/symtab.h"
#include "error.h"
#include "icgen.h"
#include "astbuiltin.h"
#undef IS_MATRIX
#include "semantic.h"
#include "icemul.h"

#define SWIZ_X		1 | (GLSLIC_VECCOMP_X << 4)
#define SWIZ_Y		1 | (GLSLIC_VECCOMP_Y << 4)
#define SWIZ_Z		1 | (GLSLIC_VECCOMP_Z << 4)
#define SWIZ_W		1 | (GLSLIC_VECCOMP_W << 4)

#define SWIZ_XY		2 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Y << 6)
#define SWIZ_XZ		2 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Z << 6)
#define SWIZ_XXX	3 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_X << 8)
#define SWIZ_XXXY	4 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_X << 8) | (GLSLIC_VECCOMP_Y << 10)
#define SWIZ_XXZZ	4 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_XXZ	3 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_XYZ	3 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Y << 6) | (GLSLIC_VECCOMP_Z << 8)	
#define SWIZ_XW		2 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_W << 6)
#define SWIZ_XYZW	0
#define SWIZ_NA		0
#define SWIZ_XZXZ	4 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_X << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_XZZ	3 |	(GLSLIC_VECCOMP_X << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_YY		2 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_Y << 6)
#define SWIZ_YXYX	4 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Y << 8) | (GLSLIC_VECCOMP_X << 10)
#define SWIZ_YZZZ	4 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_YZW	3 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8)
#define SWIZ_YW		2 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_W << 6)
#define SWIZ_YZWW	4 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_YZZZ	4 |	(GLSLIC_VECCOMP_Y << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_ZXXX	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_X << 8) | (GLSLIC_VECCOMP_X << 10)
#define SWIZ_ZXZ	3 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_ZXZZ	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_ZXZX	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_X << 10)
#define SWIZ_ZX		2 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_X << 6)
#define SWIZ_ZZZW	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_ZZWZ	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_ZZWW	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_ZZW	3 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8)
#define SWIZ_ZW		2 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6)
#define SWIZ_ZY		2 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_Y << 6)
#define SWIZ_ZWZ	3 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_ZWZZ	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_ZWZW	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_ZWW	3 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_W << 8)
#define SWIZ_ZWWW	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_ZWWZ	4 |	(GLSLIC_VECCOMP_Z << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_WXXX	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_X << 6) | (GLSLIC_VECCOMP_X << 8) | (GLSLIC_VECCOMP_X << 10)
#define SWIZ_WZ		2 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6)
#define SWIZ_WZZ	3 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_WZZW	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_WZZZ	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_WWZ	3 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8)
#define SWIZ_WWWZ	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_WWZZ	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_WWZW	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_W << 6) | (GLSLIC_VECCOMP_Z << 8) | (GLSLIC_VECCOMP_W << 10)
#define SWIZ_WZW	3 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8)
#define SWIZ_WZWZ	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_Z << 10)
#define SWIZ_WZWW	4 |	(GLSLIC_VECCOMP_W << 4) | (GLSLIC_VECCOMP_Z << 6) | (GLSLIC_VECCOMP_W << 8) | (GLSLIC_VECCOMP_W << 10)

IMG_INTERNAL const IMG_UINT32 auDimSwiz[4] = {0, SWIZ_X, SWIZ_XY, SWIZ_XYZ};
IMG_INTERNAL const IMG_UINT32 auSwizzles[4] = {SWIZ_X, SWIZ_Y, SWIZ_Z, SWIZ_W};

#define SWIZ_NUM(swiz)		(swiz & 0x7)
#define SWIZ_0COMP(swiz)	(GLSLICVecComponent)((swiz >> 4) & 0x3)
#define SWIZ_1COMP(swiz)	(GLSLICVecComponent)((swiz >> 6) & 0x3)
#define SWIZ_2COMP(swiz)	(GLSLICVecComponent)((swiz >> 8) & 0x3)
#define SWIZ_3COMP(swiz)	(GLSLICVecComponent)((swiz >> 10) & 0x3)

#define OPRD_NEG(a)		a, -1, GLSLIC_MODIFIER_NEGATE
#define OPRD(a)			a, -1, GLSLIC_MODIFIER_NONE
#define OPRD_OFF(a, b)	a, b,  GLSLIC_MODIFIER_NONE

#ifndef TRUE
#define TRUE	IMG_TRUE
#endif
#ifndef FALSE
#define FALSE	IMG_FALSE
#endif

#define SQRT_2         (IMG_FLOAT)1.4142135623730950488016887
#define PI_OVER_2      (IMG_FLOAT)(OGL_PI / 2.0f)
#define PI_OVER_6      (IMG_FLOAT)(OGL_PI / 6.0f)
#define PI_OVER_12     (IMG_FLOAT)(OGL_PI / 12.0f)
#define TAN_PI_OVER_6  (IMG_FLOAT)tan(PI_OVER_6)
#define TAN_PI_OVER_12 (IMG_FLOAT)tan(PI_OVER_12)


#define Targ0 \
	IMG_UINT32 uID0, IMG_INT32 iOffset0, GLSLICModifier eInstModifier0, IMG_UINT32 uSwiz0
#define Targ1 \
	IMG_UINT32 uID1, IMG_INT32 iOffset1, GLSLICModifier eInstModifier1, IMG_UINT32 uSwiz1
#define Targ2 \
	IMG_UINT32 uID2, IMG_INT32 iOffset2, GLSLICModifier eInstModifier2, IMG_UINT32 uSwiz2
#define Targ3 \
	IMG_UINT32 uID3, IMG_INT32 iOffset3, GLSLICModifier eInstModifier3, IMG_UINT32 uSwiz3

#define arg0	uID0, iOffset0, eInstModifier0, uSwiz0
#define arg1	uID1, iOffset1, eInstModifier1, uSwiz1
#define arg2	uID2, iOffset2, eInstModifier2, uSwiz2
#define arg3	uID3, iOffset3, eInstModifier3, uSwiz3

#define _ADD_INSTR1(cpd, opcode, arg0)									\
{																	\
	GLSLICOperandInfo sOperand0;									\
																	\
	ICInitOperandInfoWithSwiz(arg0, &sOperand0);					\
																	\
	ICAddICInstruction1(cpd, psICProgram, opcode, NULL, &sOperand0);		\
																	\
	if(iOffset0 != -1) ICFreeOperandOffsetList(&sOperand0);			\
}

#define _ADD_INSTR2(cpd, opcode, arg0, arg1)													\
{																						\
	GLSLICOperandInfo sOperand0, sOperand1;												\
																						\
	ICInitOperandInfoWithSwiz(arg0, &sOperand0);										\
																						\
	ICInitOperandInfoWithSwiz(arg1, &sOperand1);										\
																						\
	ICAddICInstruction2(cpd, psICProgram, opcode, NULL, &sOperand0, &sOperand1);		\
																						\
	if(iOffset0 != -1) ICFreeOperandOffsetList(&sOperand0);								\
	if(iOffset1 != -1) ICFreeOperandOffsetList(&sOperand1);								\
}

#define _ADD_INSTR3(cpd, opcode, arg0, arg1, arg2)											\
{																						\
	GLSLICOperandInfo sOperand0, sOperand1, sOperand2;									\
																						\
	ICInitOperandInfoWithSwiz(arg0, &sOperand0);										\
																						\
	ICInitOperandInfoWithSwiz(arg1, &sOperand1);										\
																						\
	ICInitOperandInfoWithSwiz(arg2, &sOperand2);										\
																						\
	ICAddICInstruction3(cpd, psICProgram, opcode, NULL, &sOperand0, &sOperand1, &sOperand2); \
																						\
	if(iOffset0 != -1) ICFreeOperandOffsetList(&sOperand0);								\
	if(iOffset1 != -1) ICFreeOperandOffsetList(&sOperand1);								\
	if(iOffset2 != -1) ICFreeOperandOffsetList(&sOperand2);								\
}

#define _ADD_INSTR4(cpd, opcode, arg0, arg1, arg2, arg3)										\
{																						\
	GLSLICOperandInfo sOperand0, sOperand1, sOperand2, sOperand3;						\
																						\
	ICInitOperandInfoWithSwiz(arg0, &sOperand0);										\
																						\
	ICInitOperandInfoWithSwiz(arg1, &sOperand1);										\
																						\
	ICInitOperandInfoWithSwiz(arg2, &sOperand2);										\
																						\
	ICInitOperandInfoWithSwiz(arg3, &sOperand3);										\
																						\
	ICAddICInstruction4(cpd, psICProgram, opcode, NULL, &sOperand0, &sOperand1, &sOperand2, &sOperand3);\
																						\
	if(iOffset0 != -1) ICFreeOperandOffsetList(&sOperand0);								\
	if(iOffset1 != -1) ICFreeOperandOffsetList(&sOperand1);								\
	if(iOffset2 != -1) ICFreeOperandOffsetList(&sOperand2);								\
	if(iOffset3 != -1) ICFreeOperandOffsetList(&sOperand3);								\
}


#define _ADD_NODEST_INSTR2(cpd, opcode, arg0, arg1)											\
{																						\
	GLSLICOperandInfo sOperands[2];														\
																						\
	ICInitOperandInfoWithSwiz(arg0, &sOperands[0]);										\
																						\
	ICInitOperandInfoWithSwiz(arg1, &sOperands[1]);										\
																						\
	ICAddICInstructiona(cpd, psICProgram, opcode, 2, NULL, 0, &sOperands[0]);				\
																						\
	if(iOffset0 != -1) ICFreeOperandOffsetList(&sOperands[0]);							\
	if(iOffset1 != -1) ICFreeOperandOffsetList(&sOperands[1]);							\
}

/******************************************************************************
 * Function Name: ICInitOperandInfoWithSwiz
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Initilise an operand info structure with basic symbol ID. 
 *****************************************************************************/
static IMG_VOID ICInitOperandInfoWithSwiz(Targ0, GLSLICOperandInfo *psOperandInfo)
{
	psOperandInfo->uSymbolID = uID0;
	psOperandInfo->sSwizWMask.uNumComponents = SWIZ_NUM(uSwiz0);
	psOperandInfo->sSwizWMask.aeVecComponent[0] = SWIZ_0COMP(uSwiz0);
	psOperandInfo->sSwizWMask.aeVecComponent[1] = SWIZ_1COMP(uSwiz0);
	psOperandInfo->sSwizWMask.aeVecComponent[2] = SWIZ_2COMP(uSwiz0);
	psOperandInfo->sSwizWMask.aeVecComponent[3] = SWIZ_3COMP(uSwiz0);
	psOperandInfo->eInstModifier = eInstModifier0;

	psOperandInfo->psOffsetList = IMG_NULL;
	psOperandInfo->psOffsetListEnd = IMG_NULL;

	if(iOffset0 != -1)
	{
		ICAddOperandOffset(psOperandInfo, iOffset0, 0);
	}
}

static IMG_VOID ICApplyOperandMoreSwiz(IMG_UINT32 uSwiz, GLSLICOperandInfo *psOperandInfo)
{
	GLSLICVecSwizWMask sSwizMask;

	sSwizMask.uNumComponents = SWIZ_NUM(uSwiz);
	sSwizMask.aeVecComponent[0] = SWIZ_0COMP(uSwiz);
	sSwizMask.aeVecComponent[1] = SWIZ_1COMP(uSwiz);
	sSwizMask.aeVecComponent[2] = SWIZ_2COMP(uSwiz);
	sSwizMask.aeVecComponent[3] = SWIZ_3COMP(uSwiz);

	ICCombineTwoConstantSwizzles(&psOperandInfo->sSwizWMask, &sSwizMask);
}

/* XXX: TODO: The macros _ADD_NODEST_INSTRXXX should be replaced with actual functions if possible to reduce binary size */

#if 0
static IMG_VOID _IF(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0)
{
	_ADD_INSTR1(psCPD, GLSLIC_OP_IF, arg0)
}
#endif

static IMG_VOID _IFGE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_NODEST_INSTR2(psCPD, GLSLIC_OP_IFGE, arg0, arg1);
}

static IMG_VOID _IFGT(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_NODEST_INSTR2(psCPD, GLSLIC_OP_IFGT, arg0, arg1);
}

static IMG_VOID _IFLE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_NODEST_INSTR2(psCPD, GLSLIC_OP_IFLE, arg0, arg1);
}

static IMG_VOID _IFLT(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_NODEST_INSTR2(psCPD, GLSLIC_OP_IFLT, arg0, arg1);
}


static IMG_VOID _MUL(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_MUL, arg0, arg1, arg2);
}

static IMG_VOID _ABS(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1) 
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_ABS, arg0, arg1);
}

static IMG_VOID _ADD(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_ADD, arg0, arg1, arg2);
}

static IMG_VOID _SUB(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_SUB, arg0, arg1, arg2);
}

static IMG_VOID _SLE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_SLE, arg0, arg1, arg2);
}

static IMG_VOID _SGT(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_SGT, arg0, arg1, arg2);
}

static IMG_VOID _SGE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_SGE, arg0, arg1, arg2);
}


static IMG_VOID _SEQ(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_SEQ, arg0, arg1, arg2);
}

static IMG_VOID _MOV(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1) 
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_MOV, arg0, arg1);
}

static IMG_VOID _RSQ(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1) 
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_RSQ, arg0, arg1);
}

static IMG_VOID _RCP(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_RCP, arg0, arg1);
}

static IMG_VOID _EXP2(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_EXP2, arg0, arg1);
}

static IMG_VOID _MAD(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2, Targ3)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	
	if (psICContext->eExtensionsSupported & GLSLIC_EXT_MAD)
	{		
		_ADD_INSTR4(psCPD, GLSLIC_OP_MAD, arg0, arg1, arg2, arg3);
	}
	else
	{
		_MUL(psCPD, psICProgram, arg0, arg1, arg2);
		
		_ADD(psCPD, psICProgram, arg0, arg0, arg3);
	}
}

static IMG_VOID _FLR(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	_ADD_INSTR2(psCPD, GLSLIC_OP_FLOOR, arg0, arg1);
}

static IMG_VOID _FRC(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	
	if (psICContext->eExtensionsSupported & GLSLIC_EXT_FRC)
	{		
		_ADD_INSTR2(psCPD, GLSLIC_OP_FRC, arg0, arg1);
	}
	else
	{
		_FLR(psCPD, psICProgram, arg0, arg1);
		_SUB(psCPD, psICProgram, arg0, arg1, arg0);
	}
}

static IMG_VOID _DOT(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_DOT, arg0, arg1, arg2);
}

static IMG_VOID _ELSE(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	ICAddICInstruction0(psCPD, psICProgram, GLSLIC_OP_ELSE, NULL);
}

static IMG_VOID _ENDIF(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	ICAddICInstruction0(psCPD, psICProgram, GLSLIC_OP_ENDIF, NULL);
}

static IMG_VOID _RET(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	ICAddICInstruction0(psCPD, psICProgram, GLSLIC_OP_RET, NULL);
}

static IMG_VOID _LABEL(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 uFunDefID)
{
	GLSLICOperandInfo sOperand0;

	ICInitOperandInfo(uFunDefID, &sOperand0); 

	ICAddICInstruction1(psCPD, psICProgram, GLSLIC_OP_LABEL, NULL, &sOperand0);
}


static IMG_VOID _CALL(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 uFunDefID)
{
	GLSLICOperandInfo sOperand0;

	ICInitOperandInfo(uFunDefID, &sOperand0); 

	ICAddICInstruction1(psCPD, psICProgram, GLSLIC_OP_CALL, NULL, &sOperand0);
}

static IMG_VOID _TEXLD(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2) 
{
	_ADD_INSTR3(psCPD, GLSLIC_OP_TEXLD, arg0, arg1, arg2);
}


static IMG_VOID _MIX(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, Targ0, Targ1, Targ2, Targ3)
{
	_ADD_INSTR4(psCPD, GLSLIC_OP_MIX, arg0, arg1, arg2, arg3);
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNtan(GLSLCompilerPrivateData *psCPD,
									   GLSLICProgram *psICProgram, 
									   GLSLNode *psNode, 
									   GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
	  	SIN	temp1 angle			// temp1(genType) = sin(angle) 
	  	COS temp2 angle			// temp2(genType) = cos(angle)
	 	DIV	result temp1 temp2	// result = sin(angle)/cos(angle)
	=======================================*/

	GLSLNode *psChildNode = psNode->ppsChildren[0];
	GLSLICOperandInfo sChildOperand, sTemp1Operand, sTemp2Operand;
	IMG_UINT32 uTemp1, uTemp2;
	GLSLTypeSpecifier eChildType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psChildNode, &sChildOperand);

	/* Get child type */
	eChildType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psChildNode->uSymbolTableID);

	/* create temp ID for temp1 and temp2 */
	if( !ICAddTemporary(psCPD, psICProgram, eChildType, ePrecision, &uTemp1))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNtan: Can not add a result symbol for temp1 \n"));
		return;
	}
	ICInitOperandInfo(uTemp1, &sTemp1Operand);

	if( !ICAddTemporary(psCPD, psICProgram, eChildType, ePrecision, &uTemp2))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNtan: Can not add a result symbol for temp2 \n"));
		return;
	}
	ICInitOperandInfo(uTemp2, &sTemp2Operand);

	/* SIN	temp1 angle */
	ICAddICInstruction2(psCPD, psICProgram,
						GLSLIC_OP_SIN,
						pszLineStart,
						&sTemp1Operand,
						&sChildOperand);

	/* COS temp2 angle */
	ICAddICInstruction2(psCPD, psICProgram,
						GLSLIC_OP_COS,
						pszLineStart,
						&sTemp2Operand,
						&sChildOperand);

	/* DIV	result temp1 temp2 */
	ICAddICInstruction3(psCPD, psICProgram, 
						GLSLIC_OP_DIV, 
						pszLineStart,
						psDestOperand,
						&sTemp1Operand,
						&sTemp2Operand);

	/* Free the offset list of the child operand */
	ICFreeOperandOffsetList(&sChildOperand);
}
/******************************************************************************
 * Function Name: ICAddFunctionDefinitionToSymTab
 *
 * Inputs       : 
 * Outputs      : puFunctionDefinitionID, puParameterID, puReturnDataSymbolID
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function definition to symbol table.
 *****************************************************************************/

static IMG_BOOL ICAddFunctionDefinitionToSymTab(GLSLCompilerPrivateData *psCPD,
												GLSLICProgram		   *psICProgram,
												IMG_CHAR               *pszFunctionName,
												IMG_CHAR              **ppszParamNames,
												IMG_UINT32              uNumParams,
												GLSLTypeSpecifier      *peParamTypeSpecifiers,
												GLSLParameterQualifier *peParamTypeQualifiers,
												GLSLTypeSpecifier		eReturnTypeSpecifier,
												GLSLPrecisionQualifier  eFunctionPrecision,
												IMG_UINT32			   *puFunctionDefinitionID,
												IMG_UINT32			   *puParameterID,
												IMG_UINT32			   *puReturnDataSymbolID)
{

	IMG_UINT32 j;

	GLSLFullySpecifiedType *psFullySpecifiedTypes = DebugMemAlloc(sizeof(GLSLFullySpecifiedType) * uNumParams);

	GLSLFunctionDefinitionData sFunctionDefinitionData;

	GLSLIdentifierData sParameterData;

	GLSLIdentifierData  sReturnData;

	IMG_CHAR *pszHashedFunctionName;

	SymTable  *psSymbolTable = psICProgram->psSymbolTable;


	/* Set up some default values for the parameters */
	sParameterData.eSymbolTableDataType                         = GLSLSTDT_IDENTIFIER;
	INIT_FULLY_SPECIFIED_TYPE(&sParameterData.sFullySpecifiedType);
	sParameterData.sFullySpecifiedType.eParameterQualifier      = GLSLPQ_IN;
	sParameterData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
	sParameterData.sFullySpecifiedType.ePrecisionQualifier	    = eFunctionPrecision;
	sParameterData.iActiveArraySize                             = -1;
	sParameterData.eArrayStatus                                 = GLSLAS_NOT_ARRAY;
	sParameterData.eLValueStatus                                = GLSLLV_L_VALUE;
	sParameterData.eBuiltInVariableID                           = GLSLBV_NOT_BTIN;	
	sParameterData.eIdentifierUsage                             = (GLSLIdentifierUsage)0;
	sParameterData.uConstantDataSize                            = 0;
	sParameterData.pvConstantData                               = IMG_NULL;
	sParameterData.uConstantAssociationSymbolID                 = 0;

	/* Allocate memory for parameters */
	sFunctionDefinitionData.puParameterSymbolTableIDs  = DebugMemAlloc(uNumParams * sizeof(IMG_UINT32));
	sFunctionDefinitionData.psFullySpecifiedTypes      = DebugMemAlloc(uNumParams * sizeof(GLSLFullySpecifiedType));
	
	/* Set up some default values for the function definition */
	sFunctionDefinitionData.eSymbolTableDataType  = GLSLSTDT_FUNCTION_DEFINITION;
	sFunctionDefinitionData.eFunctionType         = GLSLFT_BUILT_IN;
	sFunctionDefinitionData.eFunctionFlags        = GLSLFF_VALID_IN_ALL_CASES;
	sFunctionDefinitionData.bPrototype            = IMG_FALSE;
	sFunctionDefinitionData.eBuiltInFunctionID    = (GLSLBuiltInFunctionID)0;
	sFunctionDefinitionData.uFunctionCalledCount  = 0;
	sFunctionDefinitionData.uMaxFunctionCallDepth = 0;
	sFunctionDefinitionData.puCalledFunctionIDs   = IMG_NULL;
	sFunctionDefinitionData.uNumCalledFunctions   = 0;
	sFunctionDefinitionData.uReturnDataSymbolID   = 0;

	/* Build up a type specifier list */
	for (j = 0; j < uNumParams; j++)
	{
		psFullySpecifiedTypes[j].eTypeSpecifier           = peParamTypeSpecifiers[j];
		psFullySpecifiedTypes[j].eTypeQualifier           = GLSLTQ_TEMP;
		psFullySpecifiedTypes[j].eParameterQualifier	  = peParamTypeQualifiers[j];
		psFullySpecifiedTypes[j].ePrecisionQualifier      = eFunctionPrecision;
		psFullySpecifiedTypes[j].iArraySize               = 0;
		psFullySpecifiedTypes[j].uStructDescSymbolTableID = 0;
	}

	/* Get a hashed name for the function */
	pszHashedFunctionName = ASTSemCreateHashedFunctionName(psSymbolTable,
															pszFunctionName,
															uNumParams,
															psFullySpecifiedTypes);

	/* Set up some default values for the return type */
	sReturnData.eSymbolTableDataType                    = GLSLSTDT_IDENTIFIER;
	sReturnData.eLValueStatus                           = GLSLLV_L_VALUE;
	sReturnData.eArrayStatus                            = GLSLAS_NOT_ARRAY;
	sReturnData.iActiveArraySize                        = -1;
	INIT_FULLY_SPECIFIED_TYPE(&sReturnData.sFullySpecifiedType);
	sReturnData.eBuiltInVariableID                      = GLSLBV_NOT_BTIN;	
	sReturnData.eIdentifierUsage                        = (GLSLIdentifierUsage)0;
	sReturnData.uConstantDataSize                       = 0;
	sReturnData.pvConstantData                          = IMG_NULL;
	sReturnData.uConstantAssociationSymbolID            = 0;

	/* Increase the scope level */
	IncreaseScopeLevel(psSymbolTable);

	if(eReturnTypeSpecifier != GLSLTS_VOID)
	{
		IMG_CHAR pszReturnValueName[50];

		/* Construct the return name */
		sprintf(pszReturnValueName, "returnval_%s", pszFunctionName);

		/* Set up return type specifier */
		sReturnData.sFullySpecifiedType.eTypeSpecifier           = eReturnTypeSpecifier;
		sReturnData.sFullySpecifiedType.eTypeQualifier           = GLSLTQ_TEMP;
		sReturnData.sFullySpecifiedType.ePrecisionQualifier		 = eFunctionPrecision;

		if (!AddResultData(psCPD, psSymbolTable, 
						   pszReturnValueName, 
						   &(sReturnData), 
						   IMG_FALSE,
						   puReturnDataSymbolID))
		{
			LOG_INTERNAL_ERROR(("ICAddFunctionDefinitionToSymTab: Failed to add return value %s to symbol table", pszReturnValueName));
			
			return IMG_FALSE;
		}	

		sFunctionDefinitionData.uReturnDataSymbolID = *puReturnDataSymbolID;
	}

	/* Set up function definition */
	sFunctionDefinitionData.uNumParameters            = uNumParams;
	sFunctionDefinitionData.sReturnFullySpecifiedType = sReturnData.sFullySpecifiedType;
	sFunctionDefinitionData.pszOriginalFunctionName   = pszFunctionName;

	/* Add parameters to symbol table */
	for (j = 0; j < uNumParams; j++)
	{
		sParameterData.sFullySpecifiedType.eTypeSpecifier = psFullySpecifiedTypes[j].eTypeSpecifier;
		sParameterData.sFullySpecifiedType.eParameterQualifier = psFullySpecifiedTypes[j].eParameterQualifier;

		/* Add the data to the symbol table */
		if (!AddParameterData(psCPD, psSymbolTable, 
							  ppszParamNames[j], 
							  &sParameterData, 
							  IMG_FALSE,
							  &(sFunctionDefinitionData.puParameterSymbolTableIDs[j])))
		{
			LOG_INTERNAL_ERROR(("ASTBIAddFunction: Failed to add parameter %s to symbol table", ppszParamNames[j]));
			return IMG_FALSE;
		}

		/* Add this data for consistancies sake (even though it should never be used */
		sFunctionDefinitionData.psFullySpecifiedTypes[j] = sParameterData.sFullySpecifiedType;

		puParameterID[j] = sFunctionDefinitionData.puParameterSymbolTableIDs[j];

	}

	/* Add the function definition to the symbol table */
	if (!AddFunctionDefinitionData(psCPD, psSymbolTable, 
								   pszHashedFunctionName, 
								   &sFunctionDefinitionData, 
								   IMG_FALSE,
								   puFunctionDefinitionID))
	{
		LOG_INTERNAL_ERROR(("ASTBIAddFunction: Failed to add function %s to symbol table", pszHashedFunctionName));

		return IMG_FALSE;
	}

	/* Decrease the scope level */
	DecreaseScopeLevel(psSymbolTable);

	/* Free mem allocated for function name */
	DebugMemFree(pszHashedFunctionName);

	DebugMemFree(psFullySpecifiedTypes);

	DebugMemFree(sFunctionDefinitionData.puParameterSymbolTableIDs);
	DebugMemFree(sFunctionDefinitionData.psFullySpecifiedTypes);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddArcCos2FunctionDefinition
 *
 * Inputs       : psICProgram, psNode
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add pmx_arccos2 function definition to the symbol table.
 *				  Description: void pmx_arccos2(inout vec4 r0)
 *							   input:  r0.x
 *							   output: r0.x
 *****************************************************************************/
static IMG_BOOL ICAddArcCos2FunctionDefinition(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_CHAR pszParamName[] = "r0";
	GLSLTypeSpecifier eParameTypeSpecifier = GLSLTS_VEC4;
	GLSLParameterQualifier eParameterQualifier = GLSLPQ_INOUT;
	IMG_CHAR          *ppszParamNames[1];

	if(psICContext->psArcCos2Func)
	{
		return IMG_TRUE;
	}

	psICContext->psArcCos2Func = DebugMemAlloc(sizeof(GLSLICFunctionDefinition));
	if(psICContext->psArcCos2Func == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddArcCos2FunctionDefinition: Failed to allocation memory\n"));
		return IMG_FALSE;
	}

	ppszParamNames[0] = pszParamName;

	if( !ICAddFunctionDefinitionToSymTab(psCPD, psICProgram,
										"pmx_arccos2",
										ppszParamNames, 
										1,
										&eParameTypeSpecifier,
										&eParameterQualifier,
										GLSLTS_VOID,
										TRIG_FUNC_PRECISION,
										&psICContext->psArcCos2Func->uFunctionDefinitionID,
										&psICContext->psArcCos2Func->uParamSymbolID,
										IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICAddArcCos2FunctionDefinition: Failed to add function definition for ArcCos2.\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddArcSin2FunctionDefinition
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add pmx_arcsin2 function definition to the symbol table.
 *				  Description: void pmx_arcsin2(inout vec4 r0)
 *							   input:  r0.x
 *							   output: r0.x
 *****************************************************************************/
static IMG_BOOL ICAddArcSin2FunctionDefinition(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_CHAR pszParamName[] = "r0";
	GLSLTypeSpecifier eParameTypeSpecifier = GLSLTS_VEC4;
	GLSLParameterQualifier eParameterQualifier = GLSLPQ_INOUT;
	IMG_CHAR          *ppszParamNames[1];

	if(psICContext->psArcSin2Func)
	{	
		return IMG_TRUE;
	}

	psICContext->psArcSin2Func = DebugMemAlloc(sizeof(GLSLICFunctionDefinition));
	if(psICContext->psArcSin2Func == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddArcSin2FunctionDefinition: Failed to allocation memory\n"));
		return IMG_FALSE;
	}


	ppszParamNames[0] = pszParamName;

	if( !ICAddFunctionDefinitionToSymTab(psCPD, psICProgram,
										"pmx_arcsin2",
										ppszParamNames, 
										1,
										&eParameTypeSpecifier,
										&eParameterQualifier,
										GLSLTS_VOID,
										TRIG_FUNC_PRECISION,
										&psICContext->psArcSin2Func->uFunctionDefinitionID,
										&psICContext->psArcSin2Func->uParamSymbolID,
										IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICAddArcSin2FunctionDefinition: Failed to add function definition for ArcSin2.\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddArcTanFunctionDefinition
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add ArcTan to the symbol table.
 *				  Description: void pmx_arctan(inout vec4 r0)
 *							   input:  r0.x
 *							   output: r0.x
 *****************************************************************************/
static IMG_BOOL ICAddArcTanFunctionDefinition(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_CHAR pszParamName[] = "r0";
	GLSLTypeSpecifier eParameTypeSpecifier = GLSLTS_VEC4;
	GLSLParameterQualifier eParameterQualifier = GLSLPQ_INOUT;
	IMG_CHAR *ppszParamNames[1];

	if(psICContext->psArcTanFunc)
	{
		return IMG_TRUE;
	}

	psICContext->psArcTanFunc = DebugMemAlloc(sizeof(GLSLICFunctionDefinition));
	if(psICContext->psArcTanFunc == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionDefinition: Failed to allocation memory\n"));
		return IMG_FALSE;
	}

	ppszParamNames[0] = pszParamName;

	if( !ICAddFunctionDefinitionToSymTab(psCPD, psICProgram,
										"pmx_arctan",
										ppszParamNames, 
										1,
										&eParameTypeSpecifier,
										&eParameterQualifier,
										GLSLTS_VOID,
										TRIG_FUNC_PRECISION,
										&psICContext->psArcTanFunc->uFunctionDefinitionID,
										&psICContext->psArcTanFunc->uParamSymbolID,
										IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionDefinition: Failed to add function definition for ArcTan.\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddArcTan2FunctionDefinition
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add ArcTan to the symbol table.
 *				  Description: void pmx_arctan(inout vec4 r0)
 *							   input:  r0.x, r0.y
 *							   output: r0.x
 *****************************************************************************/
static IMG_BOOL ICAddArcTan2FunctionDefinition(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_CHAR pszParamName[] = "r0";
	GLSLTypeSpecifier eParameTypeSpecifier = GLSLTS_VEC4;
	GLSLParameterQualifier eParameterQualifier = GLSLPQ_INOUT;
	IMG_CHAR *ppszParamNames[1];

	if(psICContext->psArcTan2Func)
	{	
		return IMG_TRUE;
	}

	psICContext->psArcTan2Func = DebugMemAlloc(sizeof(GLSLICFunctionDefinition));
	if(psICContext->psArcTan2Func == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddArcTan2FunctionDefinition: Failed to allocation memory\n"));
		return IMG_FALSE;
	}

	ppszParamNames[0] = pszParamName;

	if( !ICAddFunctionDefinitionToSymTab(psCPD, psICProgram,
										"pmx_arctan2",
										ppszParamNames, 
										1,
										&eParameTypeSpecifier,
										&eParameterQualifier,
										GLSLTS_VOID,
										TRIG_FUNC_PRECISION,
										&psICContext->psArcTan2Func->uFunctionDefinitionID,
										&psICContext->psArcTan2Func->uParamSymbolID,
										IMG_NULL))
	{
		LOG_INTERNAL_ERROR(("ICAddArcTan2FunctionDefinition: Failed to add function definition for ArcTan2.\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddNoiseFunctionDefinition
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add following noise function to the symbol table.
 *				  Description: float pmx_noise1_1D(float P);
 *							   float pmx_noise1_2D(vec2 P);
 *							   float pmx_noise1_3D(vec3 P);
 *							   float pmx_noise1_4D(vec4 P);
 *****************************************************************************/
static IMG_BOOL ICAddNoiseFunctionDefinition(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_UINT32 uDim)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_CHAR pszParamName[] = "P";
	IMG_UINT32 uIndex = uDim - 1;
	GLSLTypeSpecifier eParameTypeSpecifier = (GLSLTypeSpecifier)(GLSLTS_FLOAT + uIndex);
	GLSLParameterQualifier eParameterQualifier = GLSLPQ_IN;
	GLSLTypeSpecifier eReturnTypeSpecifier = GLSLTS_FLOAT;
	IMG_CHAR *ppszParamNames[1];
	IMG_CHAR sFunctionName[16];

	if(psICContext->psNoiseFuncs[uIndex])
	{
		return IMG_TRUE;
	}

	psICContext->psNoiseFuncs[uIndex] = DebugMemAlloc(sizeof(GLSLICFunctionDefinition));
	if(psICContext->psNoiseFuncs[uIndex] == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionDefinition: Failed to allocation memory\n"));
		return IMG_FALSE;
	}

	ppszParamNames[0] = pszParamName;

	sprintf(sFunctionName, "pmx_noise1_%uD", uDim);

	if( !ICAddFunctionDefinitionToSymTab(psCPD, psICProgram,
										sFunctionName,
										ppszParamNames, 
										1,
										&eParameTypeSpecifier,
										&eParameterQualifier,
										eReturnTypeSpecifier,
										NOISE_FUNC_PRECISION,
										&psICContext->psNoiseFuncs[uIndex]->uFunctionDefinitionID,
										&psICContext->psNoiseFuncs[uIndex]->uParamSymbolID,
										&psICContext->psNoiseFuncs[uIndex]->uReturnDataSymbolID))
	{
		LOG_INTERNAL_ERROR(("ICAddNoiseFunctionDefinition: Failed to add function definition for noise1_%dD.\n", uDim));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICProcessBIFNArcTrigs
 *
 * Inputs       : psICProgram, psNode, eArcTrigFuncID
 * Outputs      : 
 * Returns      : 
 * Globals Used : -	

 *
 * Description  : common fuction body for processing asin, acos and atan
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICEmulateBIFNArcTrigs(GLSLCompilerPrivateData *psCPD,
											GLSLICProgram		*psICProgram, 
											GLSLNode				*psNode,
											GLSLICOperandInfo	*psDestOperand,
											GLSLBuiltInFunctionID eArcTrigFuncID)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *apsInputNode[2];
	GLSLICOperandInfo sInputOperand[2], sOperand[2];
	GLSLTypeSpecifier eInputType;
	IMG_CHAR *pszLine = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 i, r0;
	IMG_UINT32 uNumComponents;
	GLSLICVecSwizWMask sInputSwizMask, sDestSwizMask;
	IMG_UINT32 uComponentSwiz[4] = {SWIZ_X, SWIZ_Y, SWIZ_Z, SWIZ_W}; 
	IMG_UINT32 uFunctionDefinitionID;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	apsInputNode[0] = psNode->ppsChildren[0];

	switch(eArcTrigFuncID)
	{
	case GLSLBFID_ASIN:
		if( !ICAddArcSin2FunctionDefinition(psCPD, psICProgram))
		{
			LOG_INTERNAL_ERROR(("ICEmulateBIFNArcTrigs: Failed to add ArcSin2 function definition ID . \n"));
			return;
		}

		/* Get parameter id and function id */
		r0 = psICContext->psArcSin2Func->uParamSymbolID;
		uFunctionDefinitionID = psICContext->psArcSin2Func->uFunctionDefinitionID;

		break;
	case GLSLBFID_ACOS:
		if( !ICAddArcCos2FunctionDefinition(psCPD, psICProgram))
		{
			LOG_INTERNAL_ERROR(("ICEmulateBIFNArcTrigs: Failed to add ArcCos2 function definition ID . \n"));
			return;
		}

		/* Get parameter id and function id */
		r0 = psICContext->psArcCos2Func->uParamSymbolID;
		uFunctionDefinitionID = psICContext->psArcCos2Func->uFunctionDefinitionID;

		break;
	case GLSLBFID_ATAN:
		if(psNode->uNumChildren == 1)
		{
			if( !ICAddArcTanFunctionDefinition(psCPD, psICProgram))
			{
				LOG_INTERNAL_ERROR(("ICEmulateBIFNArcTrigs: Failed to add ArcTan function definition ID . \n"));
				return;
			}

			/* Get parameter id and function id */
			r0 = psICContext->psArcTanFunc->uParamSymbolID;
			uFunctionDefinitionID = psICContext->psArcTanFunc->uFunctionDefinitionID;
		}
		else
		{
			if( !ICAddArcTan2FunctionDefinition(psCPD, psICProgram))
			{
				LOG_INTERNAL_ERROR(("ICEmulateBIFNArcTrigs: Failed to add ArcTan2 function definition ID . \n"));
				return;
			}

			/* Get parameter id and function id */
			r0 = psICContext->psArcTan2Func->uParamSymbolID;
			uFunctionDefinitionID = psICContext->psArcTan2Func->uFunctionDefinitionID;
		}

		break;
	default:
		r0 = 0;
		uFunctionDefinitionID = 0;
		LOG_INTERNAL_ERROR(("ICEmulateBIFNArcTrigs: Not an arc trig function \n"));
		break;
	}

	ICProcessNodeOperand(psCPD, psICProgram, apsInputNode[0], &sInputOperand[0]);

	if(psNode->uNumChildren == 2)
	{
		apsInputNode[1] = psNode->ppsChildren[1];

		ICProcessNodeOperand(psCPD, psICProgram, apsInputNode[1], &sInputOperand[1]);
	}

	/* Get child type and number of components */
	/* FIXME: support calls to atan2 with swizzled inputs */
	eInputType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, apsInputNode[0]->uSymbolTableID);

	if(sInputOperand[0].sSwizWMask.uNumComponents)
	{
		uNumComponents = sInputOperand[0].sSwizWMask.uNumComponents;
	}
	else
	{
		uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eInputType);
	}

	sInputSwizMask = sInputOperand[0].sSwizWMask;
	sDestSwizMask  = psDestOperand->sSwizWMask;

	for(i = 0; i < uNumComponents; i++)
	{
		sInputOperand[0].sSwizWMask = sInputSwizMask;

		/* IC CODE: mov r0.x, input.x */
		ICApplyOperandMoreSwiz(uComponentSwiz[i], &sInputOperand[0]);
		ICInitOperandInfoWithSwiz(OPRD(r0), SWIZ_X, &sOperand[0]);
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, &sOperand[0], &sInputOperand[0]);

		if(psNode->uNumChildren == 2)
		{
			/* IC CODE: mov r0.y, input.y */
			ICApplyOperandMoreSwiz(uComponentSwiz[i], &sInputOperand[1]);
			ICInitOperandInfoWithSwiz(OPRD(r0), SWIZ_Y, &sOperand[1]);
			ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, &sOperand[1], &sInputOperand[1]);
		}

		/* IC CODE: CALL ArcTan */
		_CALL(psCPD, psICProgram, uFunctionDefinitionID);

		/* IC CODE: mov result.x(yzw), r0.x */
		psDestOperand->sSwizWMask = sDestSwizMask;
		ICApplyOperandMoreSwiz(uComponentSwiz[i], psDestOperand);
		ICInitOperandInfoWithSwiz(OPRD(r0), SWIZ_X, &sOperand[0]);
		ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, psDestOperand, &sOperand[0]);
	}

	ICFreeOperandOffsetList(&sInputOperand[0]);

	if(psNode->uNumChildren == 2)
	{
		ICFreeOperandOffsetList(&sInputOperand[1]);
	}
}

/******************************************************************************
 * Function Name: ICEmulateBIFNpow
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

IMG_INTERNAL IMG_VOID ICEmulateBIFNpow(GLSLCompilerPrivateData *psCPD, 
									   GLSLICProgram *psICProgram, 
									   GLSLNode *psNode, 
									   GLSLICOperandInfo *psDestOperand)
{

	/*=======================================
	  	LOG2 temp	x			// temp(genType) = log[2, x]
	  	MUL  temp	temp  y		// temp = log[2, x] * y
	 	EXP2 result temp		// result = exp2(temp) = pow(2, (log[2, x] * y))
	=======================================*/

	GLSLNode *psXNode, *psYNode;
	GLSLICOperandInfo sXOperand, sYOperand, sTempOperand;
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x, y */
	psXNode = psNode->ppsChildren[0];
	psYNode = psNode->ppsChildren[1];

	/* Process node operand */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psYNode, &sYOperand);

	/* Get type of x */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Add two new temps */
	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNpow: Failed to add a new temp \n"));
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* LOG2 temp x */
	ICAddICInstruction2(psCPD, psICProgram,
						GLSLIC_OP_LOG2,
						pszLineStart,
						&sTempOperand,
						&sXOperand);

	/* MUL  temp temp y */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MUL,
						pszLineStart,
						&sTempOperand,
						&sTempOperand,
						&sYOperand);

	/* EXP2 result temp */
	ICAddICInstruction2(psCPD, psICProgram, 
						GLSLIC_OP_EXP2,
						pszLineStart,
						psDestOperand,
						&sTempOperand);

	/* Free operand offset list */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sYOperand);
}





/******************************************************************************
 * Function Name: ICEmulateBIFNfract
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNfract(GLSLCompilerPrivateData *psCPD, 
										 GLSLICProgram *psICProgram, 
										 GLSLNode *psNode, 
										 GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		FLOOR temp x			// temp (genType) = floor(x)
		SUB   result x temp		// result = x - floor(x)
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
		LOG_INTERNAL_ERROR(("ICEmulateBIFNfract: Failed to add a temp \n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* IC: FLOOR temp x */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_FLOOR, pszLineStart, &sTempOperand, &sXOperand);

	/* IC: SUB  result x temp */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SUB,
						pszLineStart,
						psDestOperand,
						&sXOperand,
						&sTempOperand);

	/* Free offet list  */
	ICFreeOperandOffsetList(&sXOperand);

}

/******************************************************************************
 * Function Name: ICEmulateBIFNmod
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNmod(GLSLCompilerPrivateData *psCPD, 
									   GLSLICProgram *psICProgram, 
									   GLSLNode *psNode, 
									   GLSLICOperandInfo *psDestOperand)
{
	/*=======================================
		DIV	  temp x y			// temp(genType) = x/y
		FLOOR temp2 temp		// temp(genType) = floor(x/y)
		MUL	  temp y temp		// temp(genType) = y * floor(x/y)
		SUB   result x temp		// result = x - temp
	=======================================*/

	GLSLNode *psXNode, *psYNode;
	GLSLICOperandInfo sXOperand, sYOperand, sTempOperand;
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier eXType;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	GLSLPrecisionQualifier ePrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Get node x */
	psXNode = psNode->ppsChildren[0];
	psYNode = psNode->ppsChildren[1];
	
	/* Get x type specifier */
	eXType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);

	/* Process x node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);
	ICProcessNodeOperand(psCPD, psICProgram, psYNode, &sYOperand);

	/* Add a new temp */
	if( !ICAddTemporary(psCPD, psICProgram, eXType, ePrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNmod: Failed to add temp1 \n"));
		return;
	}
	ICInitOperandInfo(uTemp, &sTempOperand);

	/* IC: DIV temp x y */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_DIV,
						pszLineStart,
						&sTempOperand,
						&sXOperand,
						&sYOperand);

	/* IC: FLOOR temp temp */
	ICAddICInstruction2(psCPD, psICProgram,
						GLSLIC_OP_FLOOR,
						pszLineStart,
						&sTempOperand,
						&sTempOperand);

	/* IC: MUL	  temp y temp */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_MUL,
						pszLineStart,
						&sTempOperand,
						&sYOperand,
						&sTempOperand);

	/* IC: SUB   result x temp */
	ICAddICInstruction3(psCPD, psICProgram,
						GLSLIC_OP_SUB,
						pszLineStart,
						psDestOperand,
						&sXOperand,
						&sTempOperand);

	/* Free offet list  */
	ICFreeOperandOffsetList(&sXOperand);
	ICFreeOperandOffsetList(&sYOperand);

}


/******************************************************************************
 * Function Name: ICAddDepthCompare
 *
 * Inputs       : psICProgram, coord, depthVal, depthTextureDesc, result
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add depth compare code for shadow mapping 
 *****************************************************************************/
static IMG_VOID ICAddDepthCompare(GLSLCompilerPrivateData *psCPD, 
								  GLSLICProgram			*psICProgram,
								  IMG_UINT32			coordz,
								  IMG_UINT32			depthVal4,
								  IMG_UINT32			depthTextureDesc,
								  IMG_UINT32			result,
								  GLSLPrecisionQualifier eDestPrecision)
{

/*
	// Get the actual depth value 
	depth = dot(depthValue4, depthTextureDesc[3]);

	// Test for LEQUAL
	test.x = float(coord.z <= depthVal);
	
	// Test for GEQUAL
	test.y = float(coord.z >= depthVal);
	
	// Test for EQUAL 
	test.z = float(coord.z == depthVal);

	// Mask results 
	test.xyz = test.xyz * depthTextureDesc[0].xyz;
	
	// Accumulate results 
	test.x = dot(test.xyz, test.xyz);

	// Convert result to boolean (1 or 0) values 
	result = vec4((test.x > 0.0));

	// THESE TWO LINES CAN BE REMOVED AND IMPLEMENTATION WILL BE CONFORMANT 
	
	// Zero result if TEXTURE_COMPARE_MODE != COMPARE_R_TO_TEXTURE 
	result *= depthTextureDesc[0].w;

	// Return the depth value if TEXTURE_COMPARE_MODE == NONE 
	result += depthVal * (1.0 - depthTextureDesc[0].w);

	// Format output result based on texture format (luminance, intensity, alpha) 
	result = (result * depthTextureDesc[1]) + depthTextureDesc[2];

*/

	IMG_UINT32 btest;
	IMG_UINT32 test, c0, c1;
	IMG_UINT32 fTemp, depthVal;

	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, eDestPrecision, &depthVal))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add bool temp to the symbol table !\n"));
		return;
	}

	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_BOOL, eDestPrecision, &btest))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add bool temp to the symbol table !\n"));
		return;
	}

	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, eDestPrecision, &test))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add bool temp to the symbol table !\n"));
		return;
	}

	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, eDestPrecision, &fTemp))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add vec4 temp to the symbol table !\n"));
		return;
	}
	
	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 0.0, eDestPrecision, IMG_TRUE, &c0))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add constant 0.0 to the symbol table !\n"));
		return;
	}

	if( !AddFloatConstant(psCPD, psICProgram->psSymbolTable, 1.0, eDestPrecision, IMG_TRUE, &c1))
	{
		LOG_INTERNAL_ERROR(("ICAddDepthCompare: Failed to add constant 0.0 to the symbol table !\n"));
		return;
	}

	/* depthVal = dot(depthValue4, depthTextureDesc[3]); */
	_DOT(psCPD, psICProgram, OPRD(depthVal), SWIZ_NA, OPRD(depthVal4), SWIZ_NA, OPRD_OFF(depthTextureDesc, 3), SWIZ_NA);

	/* test.x = float(coord.z <= depthVal); */
	_SLE(psCPD, psICProgram, OPRD(btest), SWIZ_NA,   OPRD(coordz), SWIZ_NA, OPRD(depthVal), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(test),  SWIZ_X,    OPRD(btest), SWIZ_NA);

	/* test.y = float(coord.z >= depthVal); */
	_SGE(psCPD, psICProgram, OPRD(btest), SWIZ_NA,   OPRD(coordz), SWIZ_NA, OPRD(depthVal), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(test),  SWIZ_Y,    OPRD(btest), SWIZ_NA);

	/* test.z = float(coord.z == depthVal) */
	_SEQ(psCPD, psICProgram, OPRD(btest), SWIZ_NA,   OPRD(coordz), SWIZ_NA, OPRD(depthVal), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(test),  SWIZ_Z,    OPRD(btest), SWIZ_NA);

	/* test.xyz = test.xyz * depthTextureDesc[0].xyz; */
	_MUL(psCPD, psICProgram, OPRD(test),  SWIZ_XYZ,  OPRD(test),  SWIZ_XYZ, OPRD_OFF(depthTextureDesc, 0), SWIZ_XYZ);

	/* test.x = dot(test.xyz, test.xyz); */
	_DOT(psCPD, psICProgram, OPRD(test),  SWIZ_X,    OPRD(test),  SWIZ_XYZ, OPRD(test), SWIZ_XYZ);

	/* result = vec4((test.x > 0.0)); */
	_SGT(psCPD, psICProgram, OPRD(btest),  SWIZ_NA,  OPRD(test),  SWIZ_X, OPRD(c0), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(result), SWIZ_NA,  OPRD(btest), SWIZ_NA);

	/*==================================================
	//  The FOLLOWING THREE LINES CAN BE REMOVED AND IMPLEMENTATION WILL BE CONFORMANT 
	*==================================================*/
#if 1
		/* result *= depthTextureDesc[0].w*/
		_MUL(psCPD, psICProgram, OPRD(result), SWIZ_NA, OPRD(result), SWIZ_NA, OPRD_OFF(depthTextureDesc, 0), SWIZ_W);

		/* result += depthVal * (1.0 - depthTextureDesc[0].w); */
		_MAD(psCPD, psICProgram, OPRD(depthVal), SWIZ_NA, OPRD_NEG(depthVal), SWIZ_NA, OPRD_OFF(depthTextureDesc, 0), SWIZ_W, OPRD(depthVal), SWIZ_NA);
		_ADD(psCPD, psICProgram, OPRD(result),   SWIZ_NA, OPRD(result), SWIZ_NA, OPRD(depthVal), SWIZ_NA);
#endif

	/* result = (result * depthTextureDesc[1]) + depthTextureDesc[2] */
	{
		/* Old shadow1/2D function. */
		_MAD(psCPD, psICProgram, OPRD(result),   SWIZ_NA, OPRD(result),   SWIZ_NA, OPRD_OFF(depthTextureDesc, 1), SWIZ_NA, OPRD_OFF(depthTextureDesc, 2), SWIZ_NA);
	}
}

/******************************************************************************
 * Function Name: GetDepthTextureDescID
 *
 * Inputs       : psICProgram, uSamplerSymID
 * Outputs      : puDepthTexDescSymID
 * Returns      : 
 * Globals Used : -
 *
 * Description  : From sampler symbol ID to find out builtin uniform gl_PMXDepthTextureDesc ID 
 *****************************************************************************/
static IMG_BOOL GetDepthTextureDescID(GLSLCompilerPrivateData *psCPD,
									  GLSLICProgram				*psICProgram,
									  IMG_UINT32				uSamplerSymID,
									  IMG_INT32					iOffset,
									  GLSLPrecisionQualifier	ePrecision,
									  IMG_UINT32				*puDepthTexDescSymID)
{
	IMG_CHAR pszDesc[50];
	IMG_UINT32 i;

	/* Search through depth texture list to see if this sampler has been added */
	for(i = 0; i < psICProgram->uNumDepthTextures; i++)
	{
		if(psICProgram->asDepthTexture[i].uTextureSymID == uSamplerSymID &&
		   psICProgram->asDepthTexture[i].iOffset == iOffset)
		{
			*puDepthTexDescSymID = psICProgram->asDepthTexture[i].uTexDescSymID;
			return IMG_TRUE;
		}
	}

	sprintf(pszDesc, "gl_PMXDepthTextureDesc%u", psICProgram->uNumDepthTextures);

	/* if not, add a new depthTextureDesc to the symbol table */
	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 pszDesc,
							 4,
							 (GLSLBuiltInVariableID)(GLSLBV_PMXDEPTHTEXTUREDESC0 + psICProgram->uNumDepthTextures),
							 GLSLTS_VEC4,
							 GLSLTQ_UNIFORM,
							 ePrecision, /* This precision is the same destination precision */
							 puDepthTexDescSymID))
	{
		LOG_INTERNAL_ERROR(("GetDepthTextureDescID: Failed to add %s to the symbol table !\n", pszDesc));
		return IMG_FALSE;
	}

	psICProgram->asDepthTexture[psICProgram->uNumDepthTextures].uTextureSymID = uSamplerSymID;
	psICProgram->asDepthTexture[psICProgram->uNumDepthTextures].iOffset = iOffset;
	psICProgram->asDepthTexture[psICProgram->uNumDepthTextures].uTexDescSymID = *puDepthTexDescSymID;

	psICProgram->uNumDepthTextures++;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICEmulateBIFNshadowFunc
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Process all shadow map functions, 
 *				  Algorithm is base on pmx\tests\OGL\OGLTests\shaders\simpleglsl.frag 1.10.
 
		vec4 shadow2DProj(sampler2D depthTexture, vec4 coord)
		{	
			float depthVal;
			vec4 depthVal4;

			vec4  test, result;

			// Peform projection of texture coordinates - REMOVE FOR NON PROJECTION VERSIONS
			coord.xyz = coord.xyz / coord.w;

			// Sample shadow map
			depthVal4 = texture2D(depthTexture, coord.xy);
			depthVal = dot(depthVal4, depthTextureDesc[3]);

			// Test for LEQUAL
			test.x = float(coord.z <= depthVal);

			// Test for GEQUAL
			test.y = float(coord.z >= depthVal);

			// Test for EQUAL
			test.z = float(coord.z == depthVal);

			// Mask results
			test.xyz = test.xyz * depthTextureDesc[0].xyz;

			// Accumulate results
			test.x = dot(test.xyz, test.xyz);

			// Convert result to boolean (1 or 0) values 
			result = vec4((test.x > 0.0));

			// THESE TWO LINES CAN BE REMOVED AND IMPLEMENTATION WILL BE CONFORMANT
			
				// Zero result if TEXTURE_COMPARE_MODE != COMPARE_R_TO_TEXTURE
				result *= depthTextureDesc[0].w;

				// Return the depth value if TEXTURE_COMPARE_MODE == NONE
				result += depthVal * (1.0 - depthTextureDesc[0].w);

			// Format output result based on texture format (luminance, intensity, alpha)
			result = (result * depthTextureDesc[1]) + depthTextureDesc[2];

			return result;
		} 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICEmulateBIFNshadowFunc(GLSLCompilerPrivateData *psCPD,
											  GLSLICProgram			*psICProgram,
												GLSLNode				*psNode,
												GLSLICOperandInfo		*psDestOperand,
												GLSLBuiltInFunctionID	eShadowFuncID)
{
	GLSLICOperandInfo sOperands[5];
	GLSLICOperandInfo *psSamplerOperand;
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 coordz, depthValue4, depthTextureDesc;
	IMG_INT32 iOffset = 0;
	IMG_UINT32 uTextureDim = 0, uNumSrcs = 0;
	GLSLICOpcode eLookupCode = GLSLIC_OP_TEXLD;
	GLSLICVecSwizWMask sOrigCoordSwizMask;
	IMG_BOOL bProjectiveDiv = IMG_FALSE;
	IMG_UINT32 i;
	IMG_UINT32 uTempID;
	GLSLPrecisionQualifier eDestPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* get node information for sampler and coord */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &sOperands[i]);
	}
	psSamplerOperand = &sOperands[0];

	/* Get ID for depthValue4 */
	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, eDestPrecision, &depthValue4))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNshadow2DProj: Failed to add vec4 temp to the symbol table !\n"));
		return;
	}

	if(psSamplerOperand->psOffsetList)
	{
		/* At most, it contains one offset */
		DebugAssert(psSamplerOperand->psOffsetList->psNext == IMG_NULL);

		DebugAssert(psSamplerOperand->psOffsetList->sOperandOffset.uOffsetSymbolID == 0);

		iOffset = psSamplerOperand->psOffsetList->sOperandOffset.uStaticOffset;
	}

	if(!GetDepthTextureDescID(psCPD, psICProgram, psSamplerOperand->uSymbolID, iOffset, eDestPrecision, &depthTextureDesc))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNshadow2DProj: Failed to Get depth texture desc symbol ID !\n"));
		return;
	}

	/* Get temp ID for coordz - we want to convert the same precision as the destination */
	if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT, eDestPrecision, &coordz))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNshadow2DProj: Failed to add temp to the symbol table \n"));
		return;
	}

	uTextureDim = GLSLTypeSpecifierDimensionTable(psSamplerType->eTypeSpecifier);
	switch(eShadowFuncID)
	{
		case GLSLBFID_TEXTURE:
		{
			if(psNode->uNumChildren == 2)
			{
				eLookupCode = GLSLIC_OP_TEXLD;
			}
			else
			{
				DebugAssert(psNode->uNumChildren == 3);
				eLookupCode = GLSLIC_OP_TEXLDB;
			}

			break;
		}
		case GLSLBFID_TEXTUREPROJ:
		{
			if(psNode->uNumChildren == 2) 
			{
				eLookupCode = GLSLIC_OP_TEXLDP;
			}
			else
			{
				DebugAssert(psNode->uNumChildren == 3);
				bProjectiveDiv = IMG_TRUE;
				eLookupCode = GLSLIC_OP_TEXLDB;
			}
			break;
		}
		
		case GLSLBFID_TEXTURELOD:
		{
			eLookupCode = GLSLIC_OP_TEXLDL;
			break;
		}
		
		
		case GLSLBFID_TEXTUREPROJLOD:
		{
			bProjectiveDiv = IMG_TRUE;
			eLookupCode = GLSLIC_OP_TEXLDL;
			break;
		}
	
		case GLSLBFID_TEXTUREPROJGRAD:
		{
			bProjectiveDiv = IMG_TRUE;
			eLookupCode = GLSLIC_OP_TEXLDD;
			break;
		}

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
		case GLSLBFID_TEXTURERECTPROJ:
			bProjectiveDiv = IMG_TRUE;
		case GLSLBFID_TEXTURERECT:
			eLookupCode = GLSLIC_OP_TEXLD;
			break;
#endif
	
		default:
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNshadow2DProj: Unknown ShadowFuncID (BCB) \n"));
			break;
		}
	}

	/* Do we need to perform projective division first ? */
	if(bProjectiveDiv)
	{
		GLSLICOperandInfo sCoords[2];
		GLSLPrecisionQualifier eTexCoordPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[1]->uSymbolTableID);

		/* Store the divided value in a temp */
		if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, eTexCoordPrecision, &uTempID))
		{
			LOG_INTERNAL_ERROR(("ICEmulateBIFNshadowFunc: Failed to add a temp to symbol table \n"));
			return;
		}

		/* temp = coord / coord.w */
		sCoords[0] = sOperands[1];
		sCoords[1] = sOperands[1];
		ICApplyOperandMoreSwiz(SWIZ_W, &sCoords[1]);
		ICAddICInstructiona(psCPD, psICProgram, GLSLIC_OP_DIV, 2, pszLineStart, uTempID, &sCoords[0]);

		/* Replace coord with temp */
		ICFreeOperandOffsetList(&sOperands[1]);
		ICInitOperandInfo(uTempID, &sOperands[1]);
	}

	/* Save a copy of swiz maks for source operand 1 (coord ) */
	sOrigCoordSwizMask = sOperands[1].sSwizWMask;

	/* Actual texure lookup */
	if(eLookupCode != GLSLIC_OP_TEXLDP)
	{
		ICApplyOperandMoreSwiz(auDimSwiz[uTextureDim], &sOperands[1]);
	}
	uNumSrcs = ICOP_NUM_SRCS(eLookupCode);
	ICAddICInstructiona(psCPD, psICProgram, eLookupCode, uNumSrcs, pszLineStart, depthValue4, psSamplerOperand);

	/* coordz = coord.z */
	sOperands[1].sSwizWMask = sOrigCoordSwizMask;
	{
		ICApplyOperandMoreSwiz(SWIZ_Z, &sOperands[1]);
	}
	ICAddICInstruction2a(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, coordz, &sOperands[1]);

	/* Add depth compare code and save the result to the current node */
	ICAddDepthCompare(psCPD, psICProgram, coordz, depthValue4, depthTextureDesc, psNode->uSymbolTableID, eDestPrecision);

	/* If the destination is not the current node */
	if(psNode->uSymbolTableID != psDestOperand->uSymbolID)
	{
		ICAddICInstruction2c(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart, psDestOperand, psNode->uSymbolTableID);
	}

	/* Free all operand offset lists */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICFreeOperandOffsetList(&sOperands[i]);
	}
}

/******************************************************************************
 * Function Name: ICEmulateBIFNnoise1
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNnoise1(GLSLCompilerPrivateData *psCPD, 
										  GLSLICProgram *psICProgram, 
										  GLSLNode *psNode, 
										  GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *psXNode = psNode->ppsChildren[0];
	GLSLICOperandInfo sXOperand;
	GLSLTypeSpecifier eGenType;
	IMG_CHAR *pszLine = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uNumComponents;
	GLSLICFunctionDefinition *psNoiseFunc;

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* Get child type and number of components */
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	if(sXOperand.sSwizWMask.uNumComponents)
	{
		uNumComponents = sXOperand.sSwizWMask.uNumComponents;
	}
	else
	{
		uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eGenType);
	}
	
	/* Add noise functin definition. */
	if(!ICAddNoiseFunctionDefinition(psCPD, psICProgram, uNumComponents))
	{
		LOG_INTERNAL_ERROR(("ICProcessBIFNnoise1: Failed to add noise function definition .\n"));
		return;
	}

	psNoiseFunc = psICContext->psNoiseFuncs[uNumComponents-1];
	DebugAssert(psNoiseFunc);

	/* Copy Source in */
	ICAddICInstruction2a(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, 
						 psNoiseFunc->uParamSymbolID,
						 &sXOperand);

	/* Call noise function */
	_CALL(psCPD, psICProgram, psNoiseFunc->uFunctionDefinitionID);

	/* Copy result out */
	ICAddICInstruction2c(psCPD, psICProgram, GLSLIC_OP_MOV, IMG_NULL, psDestOperand, psNoiseFunc->uReturnDataSymbolID);
}

/******************************************************************************
 * Function Name: ICEmulateBIFNnoise2
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNnoise2(GLSLCompilerPrivateData *psCPD, 
										  GLSLICProgram *psICProgram, 
										  GLSLNode *psNode, 
										  GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *psXNode = psNode->ppsChildren[0];
	GLSLICOperandInfo sXOperand;
	GLSLTypeSpecifier eGenType;
	IMG_CHAR *pszLine = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uNumComponents;
	GLSLICFunctionDefinition *psNoiseFunc;
	IMG_UINT32 p37, a[2], i;
	GLSLICVecSwizWMask sDestSwizMask;
	GLSLPrecisionQualifier eArgPrecisionQualifier = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* Get child type and number of components */
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	if(sXOperand.sSwizWMask.uNumComponents)
	{
		uNumComponents = sXOperand.sSwizWMask.uNumComponents;
	}
	else
	{
		uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eGenType);
	}
	
	/* Add noise functin definition. */
	if(!ICAddNoiseFunctionDefinition(psCPD, psICProgram, uNumComponents))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise2: Failed to add noise function definition .\n"));
		return;
	}

	psNoiseFunc = psICContext->psNoiseFuncs[uNumComponents-1];
	DebugAssert(psNoiseFunc);
/*
	noise2_1D(float)
	{
		vec2 retVal;
		float b=a; 
		b += p[37];
		retVal.x = noise1_1D(a);
		retVal.y = noise1_1D(b);
		return retVal;
	};

	vec2 noise2_3D(vec3  a) 
	{ 
		vec2 retVal;
		vec3 b=a; 
		b.x += p[37];
		retVal.x = noise1_3D(a);
		retVal.y = noise1_3D(b);
		return retVal;
	}

	vec2 noise2_4D(vec4  a) 
	{ 
		vec2 retVal;
		vec4 b=a; 
		b.x += p[37];
		retVal.x = noise1_4D(a);
		retVal.y = noise1_4D(b);
		return retVal;
	}
*/
	/* p[37] = 26 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 26.0, eArgPrecisionQualifier, IMG_TRUE, &p37))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise2: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	for(i = 0; i < 2; i++)
	{
		if(!ICAddTemporary(psCPD, psICProgram, eGenType, eArgPrecisionQualifier, &a[i]))
		{
			LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise2: Failed to add a temp to the symbol table \n"));
			return;
		}
	}

	/* Move the input to a temp */
	ICAddICInstruction2a(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, a[0], &sXOperand);

	/* Save the original swiz mask of the destination */
	sDestSwizMask = psDestOperand->sSwizWMask;

	/* Copy Source in */
	for (i = 0; i < 2; i++)
	{
		if(i==1)
		{
			/* a[1] = a[0] */
			_MOV(psCPD, psICProgram, OPRD(a[1]), SWIZ_NA, OPRD(a[0]), SWIZ_NA);

			/* a[1] = a[0].x + p[37] */
			_ADD(psCPD, psICProgram, OPRD(a[1]), SWIZ_X, OPRD(a[0]), SWIZ_X, OPRD(p37), SWIZ_NA);
		}

		_MOV(psCPD, psICProgram, OPRD(psNoiseFunc->uParamSymbolID), SWIZ_NA, OPRD(a[i]), SWIZ_NA);

		/* Call noise function */
		_CALL(psCPD, psICProgram, psNoiseFunc->uFunctionDefinitionID);

		/* Copy result out */
		psDestOperand->sSwizWMask = sDestSwizMask;
		ICApplyOperandMoreSwiz(auSwizzles[i], psDestOperand);
		ICAddICInstruction2c(psCPD, psICProgram, GLSLIC_OP_MOV, IMG_NULL, psDestOperand, psNoiseFunc->uReturnDataSymbolID);
	}
}

/******************************************************************************
 * Function Name: ICEmulateBIFNnoise3
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNnoise3(GLSLCompilerPrivateData *psCPD, 
										  GLSLICProgram *psICProgram, 
										  GLSLNode *psNode, 
										  GLSLICOperandInfo *psDestOperand)
{

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *psXNode = psNode->ppsChildren[0];
	GLSLICOperandInfo sXOperand;
	GLSLTypeSpecifier eGenType;
	IMG_CHAR *pszLine = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uNumComponents;
	GLSLICFunctionDefinition *psNoiseFunc;
	IMG_UINT32 p37, p38, a[3], i;
	GLSLICVecSwizWMask sDestSwizMask;
	GLSLPrecisionQualifier eArgPrecisionQualifier = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* Get child type and number of components */
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	if(sXOperand.sSwizWMask.uNumComponents)
	{
		uNumComponents = sXOperand.sSwizWMask.uNumComponents;
	}
	else
	{
		uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eGenType);
	}
	
	/* Add noise functin definition. */
	if(!ICAddNoiseFunctionDefinition(psCPD, psICProgram, uNumComponents))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise3: Failed to add noise function definition .\n"));
		return;
	}

	psNoiseFunc = psICContext->psNoiseFuncs[uNumComponents-1];
	DebugAssert(psNoiseFunc);
/*
	vec3 noise3_1D(float a) 
	{ 
		vec3 retVal;
		float b=a; 
		float c=a; 
		b += p[37];
		c += p[38];
		retVal.x = noise1_1D(a);
		retVal.y = noise1_1D(b);
		retVal.z = noise1_1D(c);
		return retVal;
	}

	vec3 noise3_2D(vec2  a) 
	{ 
		vec3 retVal;
		vec2 b=a; 
		vec2 c=a; 
		b.x += p[37];
		c.x += p[38];
		retVal.x = noise1_2D(a);
		retVal.y = noise1_2D(b);
		retVal.z = noise1_2D(c);
		return retVal;
	}

	vec3 noise3_3D(vec3  a) 
	{
		vec3 retVal;
		vec3 b=a; 
		vec3 c=a; 
		b.x += p[37];
		c.x += p[38];
		retVal.x = noise1_3D(a);
		retVal.y = noise1_3D(b);
		retVal.z = noise1_3D(c);
		return retVal;
	}

	vec3 noise3_4D(vec4  a) 
	{
		vec3 retVal;
		vec4 b=a; 
		vec4 c=a; 
		b.x += p[37];
		c.x += p[38];
		retVal.x = noise1_4D(a);
		retVal.y = noise1_4D(b);
		retVal.z = noise1_4D(c);
		return retVal;
	}
*/
	/* p[37] = 26.0 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 26.0, eArgPrecisionQualifier, IMG_TRUE, &p37))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise3: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	/* p[38] = 197.0 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 197.0, eArgPrecisionQualifier, IMG_TRUE, &p38))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise3: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	for(i = 0; i < 3; i++)
	{
		if(!ICAddTemporary(psCPD, psICProgram, eGenType, eArgPrecisionQualifier, &a[i]))
		{
			LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise3: Failed to add temp to the symbol table \n"));
			return;
		}
	}

	/* Move the input to a temp */
	ICAddICInstruction2a(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, a[0], &sXOperand);

	/* Save the original swiz mask of the destination */
	sDestSwizMask = psDestOperand->sSwizWMask;

	/* Copy Source in */
	for (i = 0; i < 3; i++)
	{
		if(i != 0)
		{
			/* a[i] = a[0] */
			_MOV(psCPD, psICProgram, OPRD(a[i]), SWIZ_NA, OPRD(a[0]), SWIZ_NA);
		}

		switch(i)
		{ 
		case 0:
			break;
		case 1:
			/* a[1] += p[37] */
			_ADD(psCPD, psICProgram, OPRD(a[i]), SWIZ_X, OPRD(a[i]), SWIZ_X, OPRD(p37), SWIZ_NA);
			break;
		case 2:
			/* a[2] += p[38] */
			_ADD(psCPD, psICProgram, OPRD(a[i]), SWIZ_X, OPRD(a[i]), SWIZ_X, OPRD(p38), SWIZ_NA);
			break;
		}

		_MOV(psCPD, psICProgram, OPRD(psNoiseFunc->uParamSymbolID), SWIZ_NA, OPRD(a[i]), SWIZ_NA);

		/* Call noise function */
		_CALL(psCPD, psICProgram, psNoiseFunc->uFunctionDefinitionID);

		/* Copy result out */
		psDestOperand->sSwizWMask = sDestSwizMask;
		ICApplyOperandMoreSwiz(auSwizzles[i], psDestOperand);
		ICAddICInstruction2c(psCPD, psICProgram, GLSLIC_OP_MOV, IMG_NULL, psDestOperand, psNoiseFunc->uReturnDataSymbolID);
	}

}

/******************************************************************************
 * Function Name: ICEmulateBIFNnoise4
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
IMG_INTERNAL IMG_VOID ICEmulateBIFNnoise4(GLSLCompilerPrivateData *psCPD,
										  GLSLICProgram *psICProgram,
										  GLSLNode *psNode,
										  GLSLICOperandInfo *psDestOperand)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLNode *psXNode = psNode->ppsChildren[0];
	GLSLICOperandInfo sXOperand;
	GLSLTypeSpecifier eGenType;
	IMG_CHAR *pszLine = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uNumComponents;
	GLSLICFunctionDefinition *psNoiseFunc;
	IMG_UINT32 p37, p38, p39, a[4], i;
	GLSLICVecSwizWMask sDestSwizMask;
	GLSLPrecisionQualifier eArgPrecisionQualifier = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->uSymbolTableID);

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process child node */
	ICProcessNodeOperand(psCPD, psICProgram, psXNode, &sXOperand);

	/* Get child type and number of components */
	eGenType = ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psXNode->uSymbolTableID);
	if(sXOperand.sSwizWMask.uNumComponents)
	{
		uNumComponents = sXOperand.sSwizWMask.uNumComponents;
	}
	else
	{
		uNumComponents = TYPESPECIFIER_NUM_COMPONENTS(eGenType);
	}
	
	/* Add noise functin definition. */
	if(!ICAddNoiseFunctionDefinition(psCPD, psICProgram, uNumComponents))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise4: Failed to add noise function definition .\n"));
		return;
	}

	psNoiseFunc = psICContext->psNoiseFuncs[uNumComponents-1];
	DebugAssert(psNoiseFunc);
/*
	vec4 noise4_1D(float a) 
	{
		vec4 retVal;
		float b=a; 
		float c=a; 
		float d=a; 
		b += p[37]; 
		c += p[38];
		d += p[39];
		retVal.x = noise1_1D(a);
		retVal.y = noise1_1D(b);
		retVal.z = noise1_1D(c);
		retVal.w = noise1_1D(d);
		return retVal;
	}

	vec4 noise4_2D(vec2  a) 
	{
		vec4 retVal;
		vec2 b=a; 
		vec2 c=a; 
		vec2 d=a; 
		b.x += p[37]; 
		c.x += p[38];
		d.x += p[39];
		retVal.x = noise1_2D(a);
		retVal.y = noise1_2D(b);
		retVal.z = noise1_2D(c);
		retVal.w = noise1_2D(d);
		return retVal;
	}

	vec4 noise4_3D(vec3  a)
	{
		vec4 retVal;
		vec3 b=a; 
		vec3 c=a; 
		vec3 d=a; 
		b.x += p[37]; 
		c.x += p[38];
		d.x += p[39];
		retVal.x = noise1_3D(a);
		retVal.y = noise1_3D(b);
		retVal.z = noise1_3D(c);
		retVal.w = noise1_3D(d);
		return retVal;
	}

	vec4 noise4_4D(vec4  a) 
	{
		vec4 retVal;
		vec4 b=a; 
		vec4 c=a; 
		vec4 d=a; 
		b.x += p[37]; 
		c.x += p[38];
		d.x += p[39];
		retVal.x = noise1_4D(a);
		retVal.y = noise1_4D(b);
		retVal.z = noise1_4D(c);
		retVal.w = noise1_4D(d);
		return retVal;
}
*/
	/* p[37] = 26.0 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 26.0f, eArgPrecisionQualifier, IMG_TRUE, &p37))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise4: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	/* p[37] = 197.0 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 197.0f, eArgPrecisionQualifier, IMG_TRUE, &p38))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise4: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	/* p[39] = 62.0 */
	if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, 62.0f, eArgPrecisionQualifier, IMG_TRUE, &p39))
	{
		LOG_INTERNAL_ERROR(("ICEmulateBIFNnoise4: Failed to add constant 26.0 to the symbol table \n"));
		return;
	}

	for(i = 0; i < 4; i++)
	{
		if(!ICAddTemporary(psCPD, psICProgram, eGenType, eArgPrecisionQualifier, &a[i]))
		{
			LOG_INTERNAL_ERROR(("ICProcessBIFNnoise3: Failed to add temp to the symbol table \n"));
			return;
		}
	}

	/* Move the input to a temp */
	ICAddICInstruction2a(psCPD, psICProgram, GLSLIC_OP_MOV, pszLine, a[0], &sXOperand);

	/* Save the original swiz mask of the destination */
	sDestSwizMask = psDestOperand->sSwizWMask;

	/* Copy Source in */
	for (i = 0; i < 4; i++)
	{
		if(i != 0)
		{
			/* a[i] = a[0] */
			_MOV(psCPD, psICProgram, OPRD(a[i]), SWIZ_NA, OPRD(a[0]), SWIZ_NA);
		}

		switch(i)
		{ 
		case 0:
			break;
		case 1:
			/* a[1].x += p[37] */
			_ADD(psCPD, psICProgram, OPRD(a[i]), SWIZ_X, OPRD(a[i]), SWIZ_X, OPRD(p37), SWIZ_NA);
			break;
		case 2:
			/* a[2].x += p[38] */
			_ADD(psCPD, psICProgram, OPRD(a[i]), SWIZ_X, OPRD(a[i]), SWIZ_X, OPRD(p38), SWIZ_NA);
			break;
		case 3: 
			/* a[3].x += p[39] */
			_ADD(psCPD, psICProgram, OPRD(a[i]), SWIZ_X, OPRD(a[i]), SWIZ_X, OPRD(p39), SWIZ_NA);
			break;
		}

		_MOV(psCPD, psICProgram, OPRD(psNoiseFunc->uParamSymbolID), SWIZ_NA, OPRD(a[i]), SWIZ_NA);

		/* Call noise function */
		_CALL(psCPD, psICProgram, psNoiseFunc->uFunctionDefinitionID);

		/* Copy result out */
		psDestOperand->sSwizWMask = sDestSwizMask;
		ICApplyOperandMoreSwiz(auSwizzles[i], psDestOperand);
		ICAddICInstruction2c(psCPD, psICProgram, GLSLIC_OP_MOV, IMG_NULL, psDestOperand, psNoiseFunc->uReturnDataSymbolID);
	}
}


/******************************************************************************
 * Function Name: ICAddArcSinCos2FunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: void pmx_arccos2(vec4 r0) and void pmx_arcsin2(vec4 r0)	
				  Algorithm is based on functin ArcSinCos2 in compiler\misc\trig\trig.c 1.9
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddArcSinCos2FunctionBody(GLSLCompilerPrivateData *psCPD, 
												  GLSLICProgram *psICProgram, IMG_BOOL bCos)
{
/*
IMG_FLOAT ArcSinCos2(IMG_FLOAT fTestVal, IMG_BOOL bCos)
{
	IMG_FLOAT  r0[4], r1[4], r2[4];
	IMG_FLOAT  c0[4], c1[4], c2[4];

	if (bCos)
	{
		c0[0] = 1.0f;                   c0[1] = PI;                     c0[2] = 0.0f;                     c0[3] = 0.0f;
		c1[0] = -SQRT_2;                c1[1] = -SQRT_2/12.0f;          c1[2] = -3.0f/160.f*SQRT_2;       c1[3] = -5.0f/896.0f*SQRT_2;
		c2[0] = -35.0f/18432.0f*SQRT_2; c2[1] = -63.0f/90112.0f*SQRT_2; c2[2] = -231.0f/851968.0f*SQRT_2; c2[3] = -143.0f/1310720.0f*SQRT_2;
	}
	else
	{
		c0[0] = 1.0f;                  c0[1] = -PI_OVER_2;            c0[2] = 0.0f;                    c0[3] = PI_OVER_2;
		c1[0] = SQRT_2;                c1[1] = SQRT_2/12.0f;          c1[2] = 3.0f/160.f*SQRT_2;       c1[3] = 5.0f/896.0f*SQRT_2;
		c2[0] = 35.0f/18432.0f*SQRT_2; c2[1] = 63.0f/90112.0f*SQRT_2; c2[2] = 231.0f/851968.0f*SQRT_2; c2[3] = 143.0f/1310720.0f*SQRT_2;
	}


	r0[indx(X)] = fTestVal;
	
	// if (r0.x <= c0.z)	                                         // if (r0.x <= 0)
 	if (r0[indx(X)] <= c0[indx(Z)])
	{
		// mad r0.x, r0.xxxx, c0.zxxx, c0.xxxx                       // r0 = 1, (x+1), (x+1), (x+1) 
		MAD (r0,X,Y,Z,W, r0,X,X,X,X, c0,Z,X,X,X, c0,X,X,X,X);  

		//sqrt r2.z, r0.y                                            // r2.z = SQRT(x+1)
		SQRT (r2,Z,0,0,0, r0,Y);
		
		//	mul r0.zw,  r0.yyyy, r0.yyyy                             // r0  = 1,       (x+1),   (x+1)^2,  (x+1)^2
		MUL (r0,Z,W,0,0, r0,Y,Y,Y,Y, r0,Y,Y,Y,Y);
		//	mul r1,     r0.yzzz, r0.zzzz                             // r1  = (x+1)^3, (x+1)^4, (x+1)^4 , (x+1)^4 
		MUL (r1,X,Y,Z,W, r0,Y,Z,Z,Z, r0,Z,Z,Z,Z); 
		//	mov r0.w,   r1.x                                         // r0  = 1,       (x+1),   (x+1)^2,  (x+1)^3
		MOV (r0,W,0,0,0, r1,X,X,X,X);
		//	mul r1,     r1.xxxy, r0.yzww                             // r1  = (x+1)^4, (x+1)^5, (x+1)^6 , (x+1)^7 
		MUL (r1,X,Y,Z,W, r1,X,X,X,Y, r0,Y,Z,W,W);
		
		//  dot r0.x, r0, c1;                                        // multiply the first 4 terms by their coeffs and add
		DOT (r0,X,0,0,0, r0,X,Y,Z,W, c1,X,Y,Z,W);
		//  dp4 r0.y, r1, c2;                                        // multiply the second 4 terms by their coeffs and add
		DOT (r0,Y,0,0,0, r1,X,Y,Z,W, c2,X,Y,Z,W);
		
		//  add r0.x, r0.x, r0.y                                     // add the 1st and 2nd 4 terms together
		ADD (r0,X,0,0,0, r0,X,X,X,X, r0,Y,Y,Y,Y);
		
		//  mad r0.x, r0.x, r2.z, c0.y                               // multiply by SQRT(x+1) and Add PI/-PI_OVER_2 
		MAD (r0,X,0,0,0, r0,X,X,X,X, r2,Z,Z,Z,Z, c0,Y,Y,Y,Y);
	}
	// else
	else
	{
		// mad r0.x, -r0.xxxx, c0.zxxx, c0.xxxx                      // r0 = 1, (1-x), (1-x), (1-x)  
		MAD (r0,X,Y,Z,W, NEG(r0),X,X,X,X, c0,Z,X,X,X, c0,X,X,X,X);  

		//sqrt r2.z, r0.y                                            // r2.z = SQRT(1-x)
		SQRT (r2,Z,0,0,0, r0,Y);
		
		//	mul r0.zw,  r0.yyyy, r0.yyyy                             // r0  = 1,       (1-x),   (1-x)^2,  (1-x)^2
		MUL (r0,Z,W,0,0, r0,Y,Y,Y,Y, r0,Y,Y,Y,Y);
		//	mul r1,     r0.yzzz, r0.zzzz                             // r1  = (1-x)^3, (1-x)^4, (1-x)^4 , (1-x)^4 
		MUL (r1,X,Y,Z,W, r0,Y,Z,Z,Z, r0,Z,Z,Z,Z); 
		//	mov r0.w,   r1.x                                         // r0  = 1,       (1-x),   (1-x)^2,  (1-x)^3
		MOV (r0,W,0,0,0, r1,X,X,X,X);              
		//	mul r1,     r1.xxxy, r0.yzww                             // r1  = (1-x)^4, (1-x)^5, (1-x)^6 , (1-x)^7 
		MUL (r1,X,Y,Z,W, r1,X,X,X,Y, r0,Y,Z,W,W);
		
		//  dp4 r0.x, r0, c1;                                        // multiply the first 4 terms by their coeffs and add
		DP4 (r0,X,0,0,0, r0,X,Y,Z,W, c1,X,Y,Z,W);              
		//  dp4 r0.y, r1, c2;                                        // multiply the second 4 terms by their coeffs and add
		DP4 (r0,Y,0,0,0, r1,X,Y,Z,W, c2,X,Y,Z,W);
		
		//  add r0.x, r0.x, r0.y                                     // add the 1st and 2nd 4 terms together
		ADD (r0,X,0,0,0, r0,X,X,X,X, r0,Y,Y,Y,Y);
		
		//  mad r0.x, r0.x, r2.z, c0.w                               // multiply by -SQRT(1-x) and add 0/PI_OVER_2
		MAD (r0,X,0,0,0, r0,X,X,X,X, NEG(r2),Z,Z,Z,Z, c0,W,W,W,W);
	}

	return r0[indx(X)];
}
*/

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_FLOAT a0[4], a1[4], a2[4]; /* hold actual constant value */
	IMG_UINT32 c0,c1,c2;
	IMG_UINT32 r0, r1, r2, r3;
	IMG_UINT32 test;

	/* Three acos constants */
	if(bCos)
	{
		a0[0] = 1.0f;                   a0[1] = OGL_PI;                 a0[2] = 0.0f;						a0[3] = 0.0f;
		a1[0] = -SQRT_2;                a1[1] = -SQRT_2/12.0f;          a1[2] = -3.0f/160.f*SQRT_2;			a1[3] = -5.0f/896.0f*SQRT_2;
		a2[0] = -35.0f/18432.0f*SQRT_2; a2[1] = -63.0f/90112.0f*SQRT_2; a2[2] = -231.0f/851968.0f*SQRT_2;	a2[3] = -143.0f/1310720.0f*SQRT_2;

		r0 = psICContext->psArcCos2Func->uParamSymbolID;
	}
	else
	{
		a0[0] = 1.0f;					a0[1] = -PI_OVER_2;				a0[2] = 0.0f;						a0[3] = PI_OVER_2;
		a1[0] = SQRT_2;					a1[1] = SQRT_2/12.0f;			a1[2] = 3.0f/160.f*SQRT_2;			a1[3] = 5.0f/896.0f*SQRT_2;
		a2[0] = 35.0f/18432.0f*SQRT_2;	a2[1] = 63.0f/90112.0f*SQRT_2;	a2[2] = 231.0f/851968.0f*SQRT_2;	a2[3] = 143.0f/1310720.0f*SQRT_2;

		r0 = psICContext->psArcSin2Func->uParamSymbolID;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "acosConstant0", a0, 4, NOISE_FUNC_PRECISION, IMG_TRUE, &c0) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "acosConstant1", a1, 4, NOISE_FUNC_PRECISION, IMG_TRUE, &c1) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "acosConstant2", a2, 4, NOISE_FUNC_PRECISION, IMG_TRUE, &c2) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, NOISE_FUNC_PRECISION, &r1))
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add a result symbol for temp r0\n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, NOISE_FUNC_PRECISION, &r2))
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add a result symbol for temp r0\n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, NOISE_FUNC_PRECISION, &r3))
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add a result symbol for temp r0\n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_BOOL, NOISE_FUNC_PRECISION, &test))
	{
		LOG_INTERNAL_ERROR(("ICAddArcSinCos2FunctionBody: Can not add a result symbol for temp \n"));
		return IMG_FALSE;
	}

	if(bCos)
	{
		_LABEL(psCPD, psICProgram, psICContext->psArcCos2Func->uFunctionDefinitionID);
	}
	else
	{
		_LABEL(psCPD, psICProgram, psICContext->psArcSin2Func->uFunctionDefinitionID);
	}

#if 0
	// if (r0.x <= c0.z)
	_SLE(psCPD, psICProgram, OPRD(test), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_Z);  

	/* IF test */
	_IF(psICProgram, OPRD(test), SWIZ_X);
#else
	// if (r0.x <= c0.z)
	_IFLE(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_Z);
#endif

		// mad r0.xyzw, r0.xxxx, c0.zxxx, c0.xxxx                       // r0 = 1, (x+1), (x+1), (x+1) 
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_XYZW, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_ZXXX, OPRD(c0), SWIZ_X);  

		//sqrt r2.z, r0.y                                            // r2.z = SQRT(x+1)
		_RSQ(psCPD, psICProgram, OPRD(r2), SWIZ_Z, OPRD(r0), SWIZ_Y);

		// rcp r2.z r2.z
		_RCP(psCPD, psICProgram, OPRD(r2), SWIZ_Z, OPRD(r2), SWIZ_Z);
				
		//	mul r0.zw,  r0.yyyy, r0.yyyy                             // r0  = 1,       (x+1),   (x+1)^2,  (x+1)^2
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_ZW, OPRD(r0), SWIZ_YY, OPRD(r0), SWIZ_YY);

		//	mul r1,     r0.yzzz, r0.zzzz                             // r1  = (x+1)^3, (x+1)^4, (x+1)^4 , (x+1)^4 
		_MUL(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r0), SWIZ_YZZZ, OPRD(r0), SWIZ_Z); 

		//	mov r0.w,   r1.x                                         // r0  = 1,       (x+1),   (x+1)^2,  (x+1)^3
		_MOV(psCPD, psICProgram, OPRD(r0),  SWIZ_W, OPRD(r1), SWIZ_X);

		//	mul r1,     r1.xxxy, r0.yzww                             // r1  = (x+1)^4, (x+1)^5, (x+1)^6 , (x+1)^7 
		_MUL(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r1), SWIZ_XXXY, OPRD(r0), SWIZ_YZWW);

		//  dp4 r0.x, r0, c1;                                        // multiply the first 4 terms by their coeffs and add
		_DOT(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_XYZW, OPRD(c1), SWIZ_XYZW);

		//  dp4 r0.y, r1, c2;                                        // multiply the second 4 terms by their coeffs and add
		_DOT(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r1), SWIZ_XYZW, OPRD(c2), SWIZ_XYZW);
				
		//  add r0.x, r0.x, r0.y                                     // add the 1st and 2nd 4 terms together
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_Y);
				
		//  mad r0.x, r0.x, r2.z, c0.y                               // multiply by SQRT(x+1) and Add PI/-PI_OVER_2 
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(r2), SWIZ_Z, OPRD(c0), SWIZ_Y);

	_ELSE(psCPD, psICProgram);

		// mad r0.x, -r0.xxxx, c0.zxxx, c0.xxxx                      // r0 = 1, (1-x), (1-x), (1-x)  
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_XYZW, OPRD_NEG(r0), SWIZ_X, OPRD(c0), SWIZ_ZXXX, OPRD(c0), SWIZ_X);  

		//sqrt r2.z, r0.y                                            // r2.z = SQRT(1-x)
		_RSQ(psCPD, psICProgram, OPRD(r2), SWIZ_Z, OPRD(r0), SWIZ_Y);

		// rcp r2.z r2.z
		_RCP(psCPD, psICProgram, OPRD(r2), SWIZ_Z, OPRD(r2), SWIZ_Z);
			
		//	mul r0.zw,  r0.yyyy, r0.yyyy                             // r0  = 1,       (1-x),   (1-x)^2,  (1-x)^2
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_ZW, OPRD(r0), SWIZ_YY, OPRD(r0), SWIZ_YY);

		//	mul r1,     r0.yzzz, r0.zzzz                             // r1  = (1-x)^3, (1-x)^4, (1-x)^4 , (1-x)^4
		_MUL(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r0), SWIZ_YZZZ, OPRD(r0), SWIZ_Z);

		//	mov r0.w,   r1.x                                         // r0  = 1,       (1-x),   (1-x)^2,  (1-x)^3
		_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r1), SWIZ_X);

		//	mul r1,     r1.xxxy, r0.yzww                             // r1  = (1-x)^4, (1-x)^5, (1-x)^6 , (1-x)^7 
		_MUL(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r1), SWIZ_XXXY, OPRD(r0), SWIZ_YZWW);

		//  dp4 r0.x, r0, c1;                                        // multiply the first 4 terms by their coeffs and add
		_DOT(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_XYZW, OPRD(c1), SWIZ_XYZW);

		//  dp4 r0.y, r1, c2;                                        // multiply the second 4 terms by their coeffs and add
		_DOT(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r1), SWIZ_XYZW, OPRD(c2), SWIZ_XYZW);

		//  add r0.x, r0.x, r0.y                                     // add the 1st and 2nd 4 terms together
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_Y);

		//  mad r0.x, r0.x, -r2.z, c0.w                               // multiply by -SQRT(1-x) and add 0/PI_OVER_2
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD_NEG(r2), SWIZ_Z, OPRD(c0), SWIZ_W);

	_ENDIF(psCPD, psICProgram);

	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddArcTanFunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: void pmx_arctan(vec4 r0);
				  Algorithm is based on functin ArcTan in compiler\misc\trig\trig.c 1.9
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddArcTanFunctionBody(GLSLCompilerPrivateData *psCPD,
											  GLSLICProgram *psICProgram)
{
/*
	IMG_FLOAT ArcTan(IMG_FLOAT fTestVal)
	{
		IMG_FLOAT  r0[4], r1[4], r2[4];
		IMG_FLOAT  c0[4], c1[4], c2[4], c3[4];

		c0[0] = 0.0f;
		c0[1] = 1.0f;
		c0[2] = -1.0f;
		c0[3] = PI_OVER_2;

		c1[0] = TAN_PI_OVER_6;
		c1[1] = TAN_PI_OVER_6 + 1.0f;
		c1[2] = PI_OVER_6;
		c1[3] = TAN_PI_OVER_12;

		c2[0] = (IMG_FLOAT)48.70107004404898384; // c1
		c2[1] = (IMG_FLOAT)49.5326263772254345;  // c2
		c2[2] = (IMG_FLOAT) 9.40604244231624;    // c3
		c2[3] = (IMG_FLOAT) 0.0f;                // 

		c3[0] = (IMG_FLOAT)48.70107004404996166; // c4
		c3[1] = (IMG_FLOAT)65.7663163908956299;  // c5
		c3[2] = (IMG_FLOAT)21.587934067020262;   // c6
		c3[3] = (IMG_FLOAT)0.0f;                 //
	
		r0[indx(X)] = fTestVal;

		// mov r2, c0.yxyx                                         // r2 = (1, 0, 1, 0) 
																   // r2.x = multiplier, r2.y = addition, r2.z = multiplier, r2.w = value to subtract from
		MOV(r2,X,Y,Z,W, c0,Y,X,Y,X);

		// if (r0.x < c0.x)                                        // if r0.x < 0
		if (r0[indx(X)] < c0[indx(X)])
		{
			// mov r0.x, -r0.x                                     // r0.x = -r0.x
			MOV(r0,X,0,0,0, NEG(r0),X,X,X,X);
			// mov r2.x, -c0.y                                     // r2.x = -1.0f (multiplier to invert final result)
			MOV(r2,X,0,0,0, NEG(c0),Y,Y,Y,Y);
		}

		// if (r0.x > c0.y)                                        // if r0.x > 1
		if (r0[indx(X)] > c0[indx(Y)]) 
		{
			// rcp r0.x, r0.x                                      // r0.x = 1 / r0.x
			RCP(r0,X,0,0,0, r0,X,X,X,X);
			// mov r2.zw, c0.zwwww                                 // r2.z = -1, r2.w = PI / 2 (value to subtract result from) 
			MOV(r2,Z,W,0,0, c0,Z,W,W,W);
		}

		// if (r0.x > c1.w)                                        // if r0.x > TAN_PI_OVER_12
		if (r0[indx(X)] > c1[indx(W)])  
		{
			// add r1.x, r0.x, -c1.x                               // r1.x = r0.x - TAN_PI_OVER_6
			ADD(r1,X,0,0,0, r0,X,X,X,X, NEG(c1),X,X,X,X); 
			// mad r1.y, r0.x, c1.x, c0.y                          // r1.y = (r0.x * TAN_PI_OVER_6) + 1
			MAD(r1,Y,0,0,0, r0,X,X,X,X, c1,X,X,X,X, c0,Y,Y,Y,Y);
			// rcp r1.y, r1.y                                      // r1.y = 1 / r1.y
			RCP(r1,Y,0,0,0, r1,Y,Y,Y,Y);
			// mul r0.x, r1.x, r1.y                                // r0.x = r1.x * r1.y 
			MUL(r0,X,0,0,0, r1,X,X,X,X, r1,Y,Y,Y,Y);
			// mov r2.y, c1.z                                      // r2.y = PI_OVER_6 (addition value)  
			MOV(r2,Y,0,0,0, c1,Z,Z,Z,Z); 
		}

		// mul r0.yzw, r0.x, r0.x                                  // r0 = (x, x^2, x^2, x^2)
		MUL(r0,Y,Z,W,0, r0,X,X,X,X, r0,X,X,X,X);
		// mad r1.x, r0.y, c2.z, c2.y                              // r1.x = c2 + (x^2 * c3)
		MAD(r1,X,0,0,0, r0,Y,Y,Y,Y, c2,Z,Z,Z,Z, c2,Y,Y,Y,Y);
		// mad r1.x, r0.y, r1.x, c2.x                              // r1.x = c1 + (x^2 * (c2 + (x^2 * c3)))
		MAD(r1,X,0,0,0, r0,Y,Y,Y,Y, r1,X,X,X,X, c2,X,X,X,X);
		// mul r1.x, r1.x, r0.x                                    // r1.x = x * (c1 + (x^2 * (c2 + (x^2 * c3))))
		MUL(r1,X,0,0,0, r1,X,0,0,0, r0,X,X,X,X);             

		// add r1.y, r0.y, c3.z                                    // r1.y = x^2 + c6
		ADD(r1,Y,0,0,0, r0,Y,Y,Y,Y, c3,Z,Z,Z,Z);
		// mad r1.y, r1.y, r0.y, c3.y                              // r1.y = c5 + (x^2 * (x^2 + c6))
		MAD(r1,Y,0,0,0, r1,Y,0,0,0, r0,Y,Y,Y,Y, c3,Y,Y,Y,Y);
		// mad r1.y, r1.y, r0.y, c3.x                              // r1.y = c4 + x^2 * ((c5 + (x^2 * (x^2 + c6)))
		MAD(r1,Y,0,0,0, r1,Y,0,0,0, r0,Y,Y,Y,Y, c3,X,X,X,X);

		// rcp r1.y, r1.y                                          // r1.y = 1 / r1.y
		RCP(r1,Y,0,0,0, r1,Y,Y,Y,Y);                         

		// mad r0.x, r1.x, r1.y, r2.y                              // r.0x = (r1.x * r1.y) + 0 or PI_OVER_6
		MAD(r0,X,0,0,0, r1,X,X,X,X, r1,Y,Y,Y,Y, r2,Y,Y,Y,Y);  

		// mad r0.x, r0.x, r2.z, r2.w                              // r0.x = (r0.x * -1 or 1) + PI_OVER_2 or 0
		MAD(r0,X,0,0,0, r0,X,X,X,X, r2,Z,Z,Z,Z, r2,W,W,W,W);

		// mul r0.x, r0.x, r2.x                                    // r0.x = r0.x * 1 or -1
		MUL(r0,X,0,0,0, r0,X,X,X,X, r2,X,X,X,X); 
		
		return r0[indx(X)];
	}
*/

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_FLOAT a0[4], a1[4], a2[4], a3[4]; /* hold actual constant value */
	IMG_UINT32 c0,c1,c2,c3;
	IMG_UINT32 r0 = psICContext->psArcTanFunc->uParamSymbolID, r1, r2;
	IMG_UINT32 test;

	/* Three acos constants */
	a0[0] = 0.0f;
	a0[1] = 1.0f;
	a0[2] = -1.0f;
	a0[3] = PI_OVER_2;

	a1[0] = TAN_PI_OVER_6;
	a1[1] = TAN_PI_OVER_6 + 1.0f;
	a1[2] = PI_OVER_6;
	a1[3] = TAN_PI_OVER_12;

	a2[0] = (IMG_FLOAT)48.70107004404898384; // c1
	a2[1] = (IMG_FLOAT)49.5326263772254345;  // c2
	a2[2] = (IMG_FLOAT) 9.40604244231624;    // c3
	a2[3] = (IMG_FLOAT) 0.0f;                // 

	a3[0] = (IMG_FLOAT)48.70107004404996166; // c4
	a3[1] = (IMG_FLOAT)65.7663163908956299;  // c5
	a3[2] = (IMG_FLOAT)21.587934067020262;   // c6
	a3[3] = (IMG_FLOAT)0.0f;                 // 

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atanConstant0", a0, 4, TRIG_FUNC_PRECISION, IMG_TRUE, &c0) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atanConstant1", a1, 4, TRIG_FUNC_PRECISION, IMG_TRUE, &c1) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atanConstant2", a2, 4, TRIG_FUNC_PRECISION, IMG_TRUE, &c2) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atanConstant3", a3, 4, TRIG_FUNC_PRECISION, IMG_TRUE, &c3) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add acos constants \n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, TRIG_FUNC_PRECISION, &r1))
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add a result symbol for temp r0\n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, TRIG_FUNC_PRECISION, &r2))
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add a result symbol for temp r0\n"));
		return IMG_FALSE;
	}

	if( !ICAddTemporary(psCPD, psICProgram, GLSLTS_BOOL, TRIG_FUNC_PRECISION, &test))
	{
		LOG_INTERNAL_ERROR(("ICAddArcTanFunctionBody: Can not add a result symbol for temp \n"));
		return IMG_FALSE;
	}

	_LABEL(psCPD, psICProgram, psICContext->psArcTanFunc->uFunctionDefinitionID);

	// mov r2, c0.yxyx                                         // r2 = (1, 0, 1, 0) 															   // r2.x = multiplier, r2.y = addition, r2.z = multiplier, r2.w = value to subtract from
	_MOV(psCPD, psICProgram, OPRD(r2), SWIZ_XYZW, OPRD(c0), SWIZ_YXYX);                         
															
	// if (r0.x < c0.x)                                        // if r0.x < 0
#if 0
	_SLE(psCPD, psICProgram, OPRD(test), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_X);
	_IF(psICProgram, OPRD(test), SWIZ_X);
#else
	_IFLE(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_X);
#endif

		// mov r0.x, -r0.x                                     // r0.x = -r0.x
		_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD_NEG(r0), SWIZ_X);

		// mov r2.x, -c0.y                                     // r2.x = -1.0f (multiplier to invert final result)
		_MOV(psCPD, psICProgram, OPRD(r2), SWIZ_X, OPRD_NEG(c0), SWIZ_Y);                

	_ENDIF(psCPD, psICProgram);

	// if (r0.x > c0.y)   // if r0.x > 1
#if 0
	_SGT(psCPD, psICProgram, OPRD(test), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_Y);
	_IF(psICProgram, OPRD(test), SWIZ_X);
#else
	_IFGT(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(c0), SWIZ_Y);
#endif

		// rcp r0.x, r0.x                                      // r0.x = 1 / r0.x
		_RCP(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X);

		// mov r2.zw, c0.zwwww                                 // r2.z = -1, r2.w = PI / 2 (value to subtract result from) 
		_MOV(psCPD, psICProgram, OPRD(r2), SWIZ_ZW, OPRD(c0), SWIZ_ZW);

	_ENDIF(psCPD, psICProgram);
		
	// if (r0.x > c1.w)  // if r0.x > TAN_PI_OVER_12
#if 0
	_SGT(psCPD, psICProgram, OPRD(test), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(c1), SWIZ_W);
	_IF(psICProgram, OPRD(test), SWIZ_X);
#else
	_IFGT(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(c1), SWIZ_W);
#endif

		// add r1.x, r0.x, -c1.x                               // r1.x = r0.x - TAN_PI_OVER_6
		_ADD(psCPD, psICProgram, OPRD(r1), SWIZ_X, OPRD(r0), SWIZ_X, OPRD_NEG(c1), SWIZ_X);

		// mad r1.y, r0.x, c1.x, c0.y                          // r1.y = (r0.x * TAN_PI_OVER_6) + 1
		_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r0), SWIZ_X, OPRD(c1), SWIZ_X, OPRD(c0), SWIZ_Y);

		// rcp r1.y, r1.y                                      // r1.y = 1 / r1.y
		_RCP(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r1), SWIZ_Y);

		// mul r0.x, r1.x, r1.y                                // r0.x = r1.x * r1.y 
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r1), SWIZ_X, OPRD(r1), SWIZ_Y);

		// mov r2.y, c1.z                                      // r2.y = PI_OVER_6 (addition value)  
		_MOV(psCPD, psICProgram, OPRD(r2), SWIZ_Y, OPRD(c1), SWIZ_Z); 

	_ENDIF(psCPD, psICProgram);

	// mul r0.yzw, r0.x, r0.x                                  // r0 = (x, x^2, x^2, x^2)
	_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_YZW, OPRD(r0), SWIZ_XXX, OPRD(r0), SWIZ_XXX);

	// mad r1.x, r0.y, c2.z, c2.y                              // r1.x = c2 + (x^2 * c3)
	_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_X, OPRD(r0), SWIZ_Y, OPRD(c2), SWIZ_Z, OPRD(c2), SWIZ_Y);

	// mad r1.x, r0.y, r1.x, c2.x                              // r1.x = c1 + (x^2 * (c2 + (x^2 * c3)))
	_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_X, OPRD(r0), SWIZ_Y, OPRD(r1), SWIZ_X, OPRD(c2), SWIZ_X);

	// mul r1.x, r1.x, r0.x                                    // r1.x = x * (c1 + (x^2 * (c2 + (x^2 * c3))))
	_MUL(psCPD, psICProgram, OPRD(r1), SWIZ_X, OPRD(r1), SWIZ_X, OPRD(r0), SWIZ_X);             

	// add r1.y, r0.y, c3.z                                    // r1.y = x^2 + c6
	_ADD(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(c3), SWIZ_Z);

	// mad r1.y, r1.y, r0.y, c3.y                              // r1.y = c5 + (x^2 * (x^2 + c6))
	_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r1), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(c3), SWIZ_Y);

	// mad r1.y, r1.y, r0.y, c3.x                              // r1.y = c4 + x^2 * ((c5 + (x^2 * (x^2 + c6)))
	_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r1), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(c3), SWIZ_X); 

	// rcp r1.y, r1.y                                          // r1.y = 1 / r1.y
	_RCP(psCPD, psICProgram, OPRD(r1), SWIZ_Y, OPRD(r1), SWIZ_Y);

	// mad r0.x, r1.x, r1.y, r2.y                              // r.0x = (r1.x * r1.y) + 0 or PI_OVER_6
	_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r1), SWIZ_X, OPRD(r1), SWIZ_Y, OPRD(r2), SWIZ_Y);  

	// mad r0.x, r0.x, r2.z, r2.w                              // r0.x = (r0.x * -1 or 1) + PI_OVER_2 or 0
	_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(r2), SWIZ_Z, OPRD(r2), SWIZ_W);

	// mul r0.x, r0.x, r2.x                                    // r0.x = r0.x * 1 or -1
	_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_X, OPRD(r2), SWIZ_X); 

	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICAddArcTan2FunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: void pmx_arctan2(vec4 r0);
				  Algorithm is based on functin ArcTan2 in oglcompiler\misc\trig\trig.c 1.11
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddArcTan2FunctionBody(GLSLCompilerPrivateData *psCPD,
											   GLSLICProgram *psICProgram)
{
/*
	// Maximum error is roughly 0.01 radians
	// http://dspguru.com/comp.dsp/tricks/alg/fxdatan2.htm
	static IMG_FLOAT ArcTan2(IMG_FLOAT y, IMG_FLOAT x)
	{
		IMG_FLOAT  r0[4];
		IMG_FLOAT  c0[4], hw[4];

		// Set up the constants
		hw[0] = 0.0f;         // Hardware constant 000000b
		hw[1] = PI/4.0f;      // Hardware constant 100000b

		c0[0] = 3.0f*PI/4.0f; // No hardware constant for you today
		c0[1] = 0.1963f;      // No idea of where did this come from
		c0[2] = 0.9817f;      // Ditto.

		// Set up the input registers
		r0[0] = y;
		r0[1] = x;

		// Compute the arc tangent
		r0[2] = (IMG_FLOAT)fabs(r0[0]);

		if(r0[1] >= hw[0])
		{
			// r0[3] = (r0[1] - r0[2]) / (r0[1] + r0[2]);
			ADD(r0, W,0,0,0, r0, Y,0,0,0, r0, Z,0,0,0);                   // r0[3] = r0[1] + r0[2]
			RCP(r0, W,0,0,0, r0, W,0,0,0);	                              // r0[3] = 1.0f / r0[3]
			ADD(r0, Y,0,0,0, r0, Y,0,0,0, NEG(r0), Z,0,0,0);              // r0[1] = r0[1] - r0[2]
			MUL(r0, W,0,0,0, r0, Y,0,0,0, r0, W,0,0,0);                   // r0[3] = r0[1] * r0[3]

			// r0[1] = (c0[1]*r0[3]*r0[3] - c0[2]) * r0[3] + hw[1];
			MUL(r0, Y,0,0,0, r0, W,0,0,0, r0, W,0,0,0);                   // r0[1] = r0[3]*r0[3]
			MAD(r0, Y,0,0,0, r0, Y,0,0,0, c0, Y,0,0,0, NEG(c0), Z,0,0,0); // r0[1] = r0[1]*c0[1] - c0[2]
			MAD(r0, Y,0,0,0, r0, Y,0,0,0, r0, W,0,0,0, hw, Y,0,0,0);      // r0[1] = r0[1] * r0[3] + hw[1]
		}
		else
		{
			// r0[3] = (r0[1] + r0[2]) / (r0[2] - r0[1]);
			ADD(r0, W,0,0,0, r0, Z,0,0,0, NEG(r0), Y,0,0,0);              // r0[3] = r0[2] - r0[1]
			RCP(r0, W,0,0,0, r0, W,0,0,0);	                              // r0[3] = 1.0f / r0[3]
			ADD(r0, Y,0,0,0, r0, Y,0,0,0, r0, Z,0,0,0);                   // r0[1] = r0[1] + r0[2]
			MUL(r0, W,0,0,0, r0, Y,0,0,0, r0, W,0,0,0);                   // r0[3] = r0[1] * r0[3]

			//r0[1] = (c0[1]*r0[3]*r0[3] - c0[2]) * r0[3] + c0[0];
			MUL(r0, Y,0,0,0, r0, W,0,0,0, r0, W,0,0,0);                   // r0[1] = r0[3]*r0[3]
			MAD(r0, Y,0,0,0, r0, Y,0,0,0, c0, Y,0,0,0, NEG(c0), Z,0,0,0); // r0[1] = r0[1]*c0[1] - c0[2]
			MAD(r0, Y,0,0,0, r0, Y,0,0,0, r0, W,0,0,0, c0, X,0,0,0);      // r0[1] = r0[1] * r0[3] + c0[0]		
		}

		if(r0[0] < hw[0])
		{
			// r0[0] = -r0[1];
			MOV(r0, X,0,0,0, NEG(r0), Y,0,0,0);
		}
		else
		{
			// r0[0] = r0[1];
			MOV(r0, X,0,0,0, r0, Y,0,0,0);
		}

		return r0[0];
	}
*/
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_FLOAT a0[2], a1[3]; /* hold actual constant values */
	IMG_UINT32 c0, hw;
	IMG_UINT32 r0 = psICContext->psArcTan2Func->uParamSymbolID;

	/* These are actually hardware constants */
	a0[0] = 0.0f;
	a0[1] = OGL_PI/4.0f;

	/* These are not hardware constants */
	a1[0] = 3.0f*OGL_PI/4.0f;
	a1[1] = 0.1963f;
	a1[2] = 0.9817f;

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atan2Constant0", a0, 2, TRIG_FUNC_PRECISION, IMG_TRUE, &hw) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTan2FunctionBody: Can not add atan2 constants \n"));
		return IMG_FALSE;
	}

	if(! AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "atan2Constant1", a1, 3, TRIG_FUNC_PRECISION, IMG_TRUE, &c0) )
	{
		LOG_INTERNAL_ERROR(("ICAddArcTan2FunctionBody: Can not add atan2 constants \n"));
		return IMG_FALSE;
	}

	_LABEL(psCPD, psICProgram, psICContext->psArcTan2Func->uFunctionDefinitionID);

	// r0[2] = (IMG_FLOAT)fabs(r0[0]);
	_ABS(psCPD, psICProgram, OPRD(r0), SWIZ_Z,  OPRD(r0), SWIZ_X);

	// if(r0[1] >= hw[0])
	_IFGE(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(hw), SWIZ_X);
	{
		// r0[3] = (r0[1] - r0[2]) / (r0[1] + r0[2]);
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Z);                   // r0[3] = r0[1] + r0[2]
		_RCP(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W);	                                   // r0[3] = 1.0f / r0[3]
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD_NEG(r0), SWIZ_Z);               // r0[1] = r0[1] - r0[2]
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W);                   // r0[3] = r0[1] * r0[3]

		// r0[1] = (c0[1]*r0[3]*r0[3] - c0[2]) * r0[3] + hw[1];
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W);                       // r0[1] = r0[3]*r0[3]
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(c0), SWIZ_Y, OPRD_NEG(c0), SWIZ_Z); // r0[1] = r0[1]*c0[1] - c0[2]
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W, OPRD(hw), SWIZ_Y);     // r0[1] = r0[1] * r0[3] + hw[1]
	}
	// else
	_ELSE(psCPD, psICProgram);
	{
		// r0[3] = (r0[1] + r0[2]) / (r0[2] - r0[1]);
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_Z, OPRD_NEG(r0), SWIZ_Y);               // r0[3] = r0[2] - r0[1]
		_RCP(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W);	                                   // r0[3] = 1.0f / r0[3]
		_ADD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Z);                   // r0[1] = r0[1] + r0[2]
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W);                   // r0[3] = r0[1] * r0[3]

		//r0[1] = (c0[1]*r0[3]*r0[3] - c0[2]) * r0[3] + c0[0];
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W);                       // r0[1] = r0[3]*r0[3]
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(c0), SWIZ_Y, OPRD_NEG(c0), SWIZ_Z); // r0[1] = r0[1]*c0[1] - c0[2]
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_Y, OPRD(r0), SWIZ_W, OPRD(c0), SWIZ_X);     // r0[1] = r0[1] * r0[3] + c0[0]		
	}
	_ENDIF(psCPD, psICProgram);

	// if(r0[0] < hw[0])
	_IFLT(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(hw), SWIZ_X);
	{
		// r0[0] = -r0[1];
		_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD_NEG(r0), SWIZ_Y);
	}
	// else
	_ELSE(psCPD, psICProgram);
	{
		// r0[0] = r0[1];
		_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(r0), SWIZ_Y);
	}
	_ENDIF(psCPD, psICProgram);

	// return r0[0]
	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}


#define ONE 0.00390625f
#define ONEHALF 0.001953125f
#define _X_	0
#define _Y_ 1
#define _Z_ 2
#define _W_ 3

typedef struct NoiseRegistersTAG
{
	IMG_UINT32 r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11;
	IMG_UINT32 c0, c1, c2;
}NoiseRegisters;

static IMG_VOID ICInitNoiseFuncRegisters(GLSLCompilerPrivateData *psCPD, 
										 GLSLICProgram *psICProgram, NoiseRegisters *psNRegs)
{
	IMG_FLOAT a0[4], a1[2], a2[3];
	IMG_UINT32 temp[12];
	IMG_INT32 i;

	a0[_X_] = ONE;                              // ONE
	a0[_Y_] = ONEHALF;                          // ONEHALF
	a0[_Z_] = 0.0;
	a0[_W_] = 1.0;

	a1[_X_] = 4.0f;
	a1[_Y_] = -1.0f;

	a2[_X_] = 6.0f;
	a2[_Y_] = -15.0f;
	a2[_Z_] = 10.0f;

	if(!AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "noiseConstant0", a0, 4, NOISE_FUNC_PRECISION, IMG_TRUE, &psNRegs->c0) )
	{
		LOG_INTERNAL_ERROR(("ICInitNoiseFuncRegisters: Failed to add noiseConstant0 to the symbol table \n"));
		return ;
	}

	if(!AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "noiseConstant1", a1, 2, NOISE_FUNC_PRECISION, IMG_TRUE, &psNRegs->c1) )
	{
		LOG_INTERNAL_ERROR(("ICInitNoiseFuncRegisters: Failed to add noiseConstant0 to the symbol table \n"));
		return ;
	}

	if(!AddFloatVecConstant(psCPD, psICProgram->psSymbolTable, "noiseConstant2", a2, 3, NOISE_FUNC_PRECISION, IMG_TRUE, &psNRegs->c2) )
	{
		LOG_INTERNAL_ERROR(("ICInitNoiseFuncRegisters: Failed to add noiseConstant0 to the symbol table \n"));
		return ;
	}

	/* Add 11 temps: r1, r2, r3, r4, r5 */
	for(i = 0; i < 12; i++)
	{
		if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, NOISE_FUNC_PRECISION, &temp[i]))
		{
			LOG_INTERNAL_ERROR(("ICInitNoiseFuncRegisters: Failed to add temp to the symbol table \n"));
			return ;
		}
	}

	psNRegs->r0 = temp[0];
	psNRegs->r1 = temp[1];
	psNRegs->r2 = temp[2];
	psNRegs->r3 = temp[3];
	psNRegs->r4 = temp[4];
	psNRegs->r5 = temp[5];
	psNRegs->r6 = temp[6];
	psNRegs->r7 = temp[7];
	psNRegs->r8 = temp[8];
	psNRegs->r9 = temp[9];
	psNRegs->r10 = temp[10];
	psNRegs->r11 = temp[11];
}

#define r0 sNRegs.r0
#define r1 sNRegs.r1
#define r2 sNRegs.r2
#define r3 sNRegs.r3
#define r4 sNRegs.r4
#define r5 sNRegs.r5
#define r6 sNRegs.r6
#define r7 sNRegs.r7
#define r8 sNRegs.r8
#define r9 sNRegs.r9
#define r10 sNRegs.r10
#define r11 sNRegs.r11
#define c0 sNRegs.c0
#define c1 sNRegs.c1
#define c2 sNRegs.c2

// basic on glslnoiseptfast.c 1.5 */

/*
  4D Fade Macro
  return t*t*t*(t*(t*6.0-15.0)+10.0);                      // Improved fade, yields C2-continuous noise 
  Uses: r8, r9, c2
    mul r8, t.tSwiz, t.tSwiz                               // r8 = t*t
	mad r9, t.tSwiz, c2.x, c2.y                            // r9 = (t*6.0-15.0)
	mul r8, r8, t.tSwiz                                    // r8 = t*t*t
	mad r9, r2, r9, c2.z                                   // r9 = (t*(t*6.0-15.0) + 10.0f)
	mul Result.ResultMask, r8, r9                          // Result = t*t*t*(t*(t*6.0-15.0) + 10.0f)
*/
#define FADE(Result,ResultMask, t,tSwiz) \
	_MUL(psCPD, psICProgram, OPRD(r8), SWIZ_XYZW, OPRD(t), tSwiz,   OPRD(t),tSwiz); \
    _MAD(psCPD, psICProgram, OPRD(r9), SWIZ_XYZW, OPRD(t), tSwiz,   OPRD(c2), SWIZ_X, OPRD(c2), SWIZ_Y); \
	_MUL(psCPD, psICProgram, OPRD(r8), SWIZ_XYZW, OPRD(r8),SWIZ_XYZW, OPRD(t),tSwiz); \
	_MAD(psCPD, psICProgram, OPRD(r9), SWIZ_XYZW, OPRD(t), tSwiz,   OPRD(r9), SWIZ_XYZW, OPRD(c2), SWIZ_Z); \
	_MUL(psCPD, psICProgram, OPRD(Result),ResultMask, OPRD(r8),SWIZ_XYZW, OPRD(r9),SWIZ_XYZW); 


/*
  4D mix macro
  mix4d(v1,v2,fade) = v1 * (1.0f - fade) + (v2 * fade);

    add r9.y, c0.w, -fade.fadeSwiz                 // r9.y = 1.0f - fade

  Can replace these two instructions below with WSUM if supported 

    mul r10, v2.v2Swiz, fade.fadeSwiz              // r5 = v2 * a
    mad result.resultMask, v1.v1Swiz, r9.y, r10    // r6 = v1 * (1.0f - a) + (v2 * a)
*/
#if 0
/* Use MAD for mix */ 
#define MIX1D(result, resultMask, v1, v1Swiz, v2, v2Swiz, fade, fadeSwiz) \
	_ADD(psCPD, psICProgram, OPRD(r9), SWIZ_Y, OPRD(c0), SWIZ_W, OPRD_NEG(fade),fadeSwiz); \
	_MUL(psCPD, psICProgram, OPRD(r10),SWIZ_X, OPRD(v2), v2Swiz, OPRD(fade), fadeSwiz); \
	_MAD(psCPD, psICProgram, OPRD(result), resultMask, OPRD(v1), v1Swiz, OPRD(r9), SWIZ_Y, OPRD(r10), SWIZ_X); 

#define MIX2D(result, resultMask, v1, v1Swiz, v2, v2Swiz, fade, fadeSwiz) \
	_ADD(psCPD, psICProgram, OPRD(r9), SWIZ_Y, OPRD(c0), SWIZ_W, OPRD_NEG(fade),fadeSwiz); \
	_MUL(psCPD, psICProgram, OPRD(r10),SWIZ_XY, OPRD(v2), v2Swiz, OPRD(fade), fadeSwiz); \
	_MAD(psCPD, psICProgram, OPRD(result), resultMask, OPRD(v1), v1Swiz, OPRD(r9), SWIZ_Y, OPRD(r10), SWIZ_XY); 

#define MIX3D(result, resultMask, v1, v1Swiz, v2, v2Swiz, fade, fadeSwiz) \
	_ADD(psCPD, psICProgram, OPRD(r9), SWIZ_Y, OPRD(c0), SWIZ_W, OPRD_NEG(fade),fadeSwiz); \
	_MUL(psCPD, psICProgram, OPRD(r10),SWIZ_XYZ, OPRD(v2), v2Swiz, OPRD(fade), fadeSwiz); \
	_MAD(psCPD, psICProgram, OPRD(result), resultMask, OPRD(v1), v1Swiz, OPRD(r9), SWIZ_Y, OPRD(r10), SWIZ_XYZ); 

#define MIX4D(result, resultMask, v1, v1Swiz, v2, v2Swiz, fade, fadeSwiz) \
	_ADD(psCPD, psICProgram, OPRD(r9), SWIZ_Y, OPRD(c0), SWIZ_W, OPRD_NEG(fade),fadeSwiz); \
	_MUL(psCPD, psICProgram, OPRD(r10),SWIZ_XYZW, OPRD(v2), v2Swiz, OPRD(fade), fadeSwiz); \
	_MAD(psCPD, psICProgram, OPRD(result), resultMask, OPRD(v1), v1Swiz, OPRD(r9), SWIZ_Y, OPRD(r10), SWIZ_XYZW); 
#else

/* Use LRP for mix */
#define MIX(result, resultMask, v1, v1Swiz, v2, v2Swiz, fade, fadeSwiz) \
	_MIX(psCPD, psICProgram, OPRD(result), resultMask, OPRD(v1), v1Swiz, OPRD(v2), v2Swiz, OPRD(fade), fadeSwiz);

#define MIX1D MIX
#define MIX2D MIX
#define MIX3D MIX
#define MIX4D MIX

#endif

//   tex2d r7, permTexture, texCoord.texCoordSwiz          // r7 = texture2D(permTexture, texCoordSwiz))
//   mad r7, r7, c1.x, c1.y                                // r7 = grad = texture2D(permTexture, texCoord) * 4.0 - 1.0;
//   mul, r8, offsetPf.offsetPfSwiz, r10                   // mask components we do not wish to be in dot product
//   dot Result.ResultMask, r7, r8                         // r7 = n = dot(grad00, (Pf - PfAdjust));
#define SAMPLE_CORNER1D(Result, ResultMask, texture, texCoord,texCoordSwiz, offsetPf,offsetPfSwiz) \
    _TEXLD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(texture), SWIZ_NA, OPRD(texCoord), texCoordSwiz); \
	_MAD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(r7), SWIZ_XYZW, OPRD(c1), SWIZ_X, OPRD(c1), SWIZ_Y); \
	_MUL(psCPD, psICProgram, OPRD(Result),ResultMask, OPRD(r7),SWIZ_X, OPRD(offsetPf),offsetPfSwiz);

#define SAMPLE_CORNER2D(Result, ResultMask, texture, texCoord,texCoordSwiz, offsetPf,offsetPfSwiz) \
    _TEXLD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(texture), SWIZ_NA, OPRD(texCoord), texCoordSwiz); \
	_MAD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(r7), SWIZ_XYZW, OPRD(c1), SWIZ_X, OPRD(c1), SWIZ_Y); \
	_DOT(psCPD, psICProgram, OPRD(Result),ResultMask, OPRD(r7),SWIZ_XY, OPRD(offsetPf),offsetPfSwiz);

#define SAMPLE_CORNER3D(Result, ResultMask, texture, texCoord,texCoordSwiz, offsetPf,offsetPfSwiz) \
    _TEXLD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(texture), SWIZ_NA, OPRD(texCoord), texCoordSwiz); \
	_MAD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(r7), SWIZ_XYZW, OPRD(c1), SWIZ_X, OPRD(c1), SWIZ_Y); \
	_DOT(psCPD, psICProgram, OPRD(Result),ResultMask, OPRD(r7),SWIZ_XYZ, OPRD(offsetPf),offsetPfSwiz);

#define SAMPLE_CORNER4D(Result, ResultMask, texture, texCoord,texCoordSwiz, offsetPf,offsetPfSwiz) \
	_TEXLD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(texture), SWIZ_NA, OPRD(texCoord), texCoordSwiz); \
	_MAD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZW, OPRD(r7), SWIZ_XYZW, OPRD(c1), SWIZ_X, OPRD(c1), SWIZ_Y); \
	_DOT(psCPD, psICProgram, OPRD(Result),ResultMask, OPRD(r7),SWIZ_XYZW, OPRD(offsetPf),offsetPfSwiz);

//   add   r7, PermPi.PermPiSwiz, PermdPiOffset.PermPiOffsetSwiz    // r7.xy = (perm + PermPiOffset.x, Pi.z + PermPiOffset.y) 
//   add   r8, Pf, - PfOffset.PfOffsetSwiz                          // r8.xyz = Pf.xyz - (PfOffset.PfOffsetSwiz)
//   Call  sampleCorner
//   mov   Result.ResultMask, r7.x                                  // Result.ResultMask = r7.x
#define SAMPLE_CORNER3DA(Result, ResultMask, texture, PermPi,PermPiSwiz, PermPiOffset, PermPiOffsetSwiz, Pf, PfOffset, PfOffsetSwiz) \
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XY, OPRD(PermPi),PermPiSwiz, OPRD(PermPiOffset),PermPiOffsetSwiz); \
	_SUB(psCPD, psICProgram, OPRD(r8),SWIZ_XYZ, OPRD(Pf),SWIZ_XYZ, OPRD(PfOffset),PfOffsetSwiz); \
	SAMPLE_CORNER3D(Result,ResultMask, texture, r7,SWIZ_XY, r8,SWIZ_XYZ); 

/*
  Noise Setup function
  Sets up registers with values required to do noise calc
  Input:  r0 = P
  Output: r1 = Pi = ONE*floor(P)+ONEHALF
          r2 = Pf = fract(P)
		  r3 = fade(Pf)
    flr r1, r0                                            // r1 = floor(P) 
	mad r1, r1, c0.x, c0.y                                // r1 = Pi = ONE*floor(P)+ONEHALF;
	frc r2, r0                                            // r2 = Pf = fract(P)
	r3 = fade(r2)                                         // r3 = fade(Pf)                               
*/
#define NOISE_SETUP() \
	_FLR(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r0), SWIZ_XYZW); \
	_MAD(psCPD, psICProgram, OPRD(r1), SWIZ_XYZW, OPRD(r1), SWIZ_XYZW, OPRD(c0), SWIZ_X, OPRD(c0), SWIZ_Y); \
	_FRC(psCPD, psICProgram, OPRD(r2), SWIZ_XYZW, OPRD(r0), SWIZ_XYZW); \
	FADE(r3, SWIZ_XYZW, r2, SWIZ_XYZW);

/******************************************************************************
 * Function Name: ICAddNoise1DFunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: float pmx_noise1_1D(float P);
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddNoise1DFunctionBody(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 permTexture;
	NoiseRegisters sNRegs;

	ICInitNoiseFuncRegisters(psCPD, psICProgram, &sNRegs);

	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable, 
							 "gl_PMXPermTexture", 
							 0, 
							 GLSLBV_PMXPERMTEXTURE,
							 GLSLTS_SAMPLER2D, 
							 GLSLTQ_UNIFORM, 
							 NOISE_FUNC_PRECISION,
							 &permTexture))
	{
		LOG_INTERNAL_ERROR(("ICAddNoise1DFunctionBody: Failed to add gl_PMXPermTexture to the symbol table\n"));
		return IMG_FALSE;
	}

	/* LABEL noise1_2D */
	_LABEL(psCPD, psICProgram, psICContext->psNoiseFuncs[NOISE1D]->uFunctionDefinitionID);

	/*
		r0[_X_] = P;
		r0[_Y_] = 0.0f;
		r0[_Z_] = 0.0f;
		r0[_W_] = 0.0f;
	*/
	_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(psICContext->psNoiseFuncs[NOISE1D]->uParamSymbolID), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_YZW, OPRD(c0), SWIZ_Z);

	// call noiseSetup
	NOISE_SETUP();
	
	// mov r10, c0.wzzz                                      // setup dot mask so that a DP1 (i.e. MUL)  is performed
	_MOV(psCPD, psICProgram, OPRD(r10), SWIZ_XYZW, OPRD(c0), SWIZ_WZZZ);

	// add r4, r1, c0.z                                      // r4.x = Pi + 0.0f - can replace with MOV 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_X, OPRD(r1), SWIZ_X, OPRD(c0),SWIZ_Z);

	// mov r4.y, c0.z                                        // zero the y coordinate
	_MOV(psCPD, psICProgram, OPRD(r4), SWIZ_Y, OPRD(c0), SWIZ_Z);

	/*
	  Calculate n0
	  Result = r5.x,  texCoord = r4.xy,  offsetPf = r2.x
	*/
	SAMPLE_CORNER1D(r5, SWIZ_X, permTexture, r4, SWIZ_XY, r2, SWIZ_X);
	
	// add r4, r1, c0.z                                      // r4.x = Pi + ONE 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_X, OPRD(r1), SWIZ_X, OPRD(c0), SWIZ_X);

	// add r6, r2, -c0.z                                     // r6.x = Pf - 1.0f 
	_ADD(psCPD, psICProgram, OPRD(r6), SWIZ_X, OPRD(r2), SWIZ_X, OPRD_NEG(c0), SWIZ_W);

	/*
	  Calculate n1
	  Result = r5.y,  texCoord = r4.xy,  offsetPf = r6.x
	*/
	SAMPLE_CORNER1D(r5, SWIZ_Y, permTexture, r4, SWIZ_XY, r6, SWIZ_X);


	// Blend contributions along x
	// r5.x = mix(n0, n1, fade(Pf.x));
	MIX1D(r5, SWIZ_X, r5, SWIZ_X, r5, SWIZ_Y, r3, SWIZ_X);

	/* Move the result out */
	_MOV(psCPD, psICProgram, OPRD(psICContext->psNoiseFuncs[NOISE1D]->uReturnDataSymbolID), SWIZ_NA,
		OPRD(r5), SWIZ_X);

	// ret
	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddNoise2DFunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: float pmx_noise1_2D(vec2 P);
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddNoise2DFunctionBody(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram *psICProgram)
{

	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 permTexture;
	NoiseRegisters sNRegs;

	ICInitNoiseFuncRegisters(psCPD, psICProgram, &sNRegs);

	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 "gl_PMXPermTexture",
							 0,
							 GLSLBV_PMXPERMTEXTURE,
							 GLSLTS_SAMPLER2D,
							 GLSLTQ_UNIFORM,
							 NOISE_FUNC_PRECISION,
							 &permTexture))
	{
		LOG_INTERNAL_ERROR(("ICAddNoise2DFunctionBody: Failed to add gl_PMXPermTexture to the symbol table\n"));
		return IMG_FALSE;
	}

	/* LABEL noise1_2D */
	_LABEL(psCPD, psICProgram, psICContext->psNoiseFuncs[NOISE2D]->uFunctionDefinitionID);

	/*
		r0[_X_] = P.x;
		r0[_Y_] = P.y;
		r0[_Z_] = 0.0f;
		r0[_W_] = 0.0f;
	*/
	_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_XY, OPRD(psICContext->psNoiseFuncs[NOISE2D]->uParamSymbolID), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(r1), SWIZ_ZW, OPRD(c0), SWIZ_Z);
		
	// call noiseSetup
	NOISE_SETUP();

	// mov r10, c0.wwzz                                      // setup dot mask so that a DP2 is performed
	//_MOV(psCPD, psICProgram, OPRD(r10), SWIZ_XYZW, OPRD(c0), SWIZ_WWZZ);


	// n00 - Sample lower left corner 

	// add r4, r1, c0.z                                      // r4.xy = Pi.xy + (0.0f, 0.0f) - can replace with MOV 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_XYZW, OPRD(r1), SWIZ_XYZW, OPRD(c0), SWIZ_Z);

	/*
	  Calculate n00
	  Result = r5.x,  texCoord = r4.xy,  offsetPf = r2.x
	*/
	SAMPLE_CORNER2D(r5, SWIZ_X, permTexture, r4, SWIZ_XY, r2, SWIZ_XY);

	// n01 - Sample upper left corner 

	// add r4.xy, r1.xy, c0.zx                                // r4.xy = Pi.xy + (0.0f, ONE) 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_XY, OPRD(r1), SWIZ_XY, OPRD(c0), SWIZ_ZX);

	// add r6.xy, r2.xy, -c0.zw                               // r6.xy = Pf.xy - (0.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r6), SWIZ_XY, OPRD(r2), SWIZ_XY, OPRD_NEG(c0), SWIZ_ZW);

	/*
	  Calculate n01
	  Result = r5.y,  texCoord = r4.xy,  offsetPf = r6.x
	*/
	SAMPLE_CORNER2D(r5, SWIZ_Y, permTexture, r4, SWIZ_XY, r6, SWIZ_XY);


	// n10 - Sample lower right corner 

	// add r4.xy, r1.xy, c0.xz                                // r4.xy = Pi.xy + (ONE, 0.0f) 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_XY, OPRD(r1), SWIZ_XY, OPRD(c0), SWIZ_XZ);

	// add r6.xy, r2.xy, -c0.zw                               // r6.xy = Pf.xy - (1.0f, 0.0f) 
	_ADD(psCPD, psICProgram, OPRD(r6), SWIZ_XY, OPRD(r2), SWIZ_XY, OPRD_NEG(c0), SWIZ_WZ);

	/*
	  Calculate n10
	  Result = r5.z,  texCoord = r4.xy,  offsetPf = r6.x
	*/
	SAMPLE_CORNER2D(r5, SWIZ_Z, permTexture, r4, SWIZ_XY, r6, SWIZ_XY);


	// n11 - Sample upper right corner 

	// add r4.xy, r1.xy, c0.x                                // r4.xy = Pi.xy + (ONE, ONE) 
	_ADD(psCPD, psICProgram, OPRD(r4), SWIZ_XY, OPRD(r1), SWIZ_XY, OPRD(c0), SWIZ_X);

	// add r6.xy, r2.xy, -c0.w                               // r6.xy = Pf.xy - (1.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r6), SWIZ_XY, OPRD(r2), SWIZ_XY, OPRD_NEG(c0),SWIZ_W);

	/*
	  Calculate n11
	  Result = r5.w,  texCoord = r4.xy,  offsetPf = r6.x
	*/
	SAMPLE_CORNER2D(r5, SWIZ_W, permTexture, r4, SWIZ_XY, r6, SWIZ_XY);


	// Blend contributions along x
	// r5.xy = n_x = mix(n00, n01, n10, n11, fade(Pf.x));
	MIX2D(r5, SWIZ_XY, r5, SWIZ_XY, r5, SWIZ_ZW, r3, SWIZ_X);


	// Blend contributions along y
	// r5.x = mix(nx.x, nx.y, fade(Pf.y));
	MIX1D(r5, SWIZ_X, r5, SWIZ_X, r5, SWIZ_Y, r3, SWIZ_Y);

	/* Move the result out */
	_MOV(psCPD, psICProgram, OPRD(psICContext->psNoiseFuncs[NOISE2D]->uReturnDataSymbolID), SWIZ_NA, OPRD(r5), SWIZ_X);
	// ret
	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}
/******************************************************************************
 * Function Name: ICAddNoise3DFunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: float pmx_noise1_3D(vec3 P);
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddNoise3DFunctionBody(GLSLCompilerPrivateData *psCPD,
											   GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 permTexture;
	NoiseRegisters sNRegs;

	ICInitNoiseFuncRegisters(psCPD, psICProgram, &sNRegs);

	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable, 
							 "gl_PMXPermTexture", 
							 0, 
							 GLSLBV_PMXPERMTEXTURE,
							 GLSLTS_SAMPLER2D, 
							 GLSLTQ_UNIFORM, 
							 NOISE_FUNC_PRECISION,
							 &permTexture))
	{
		LOG_INTERNAL_ERROR(("ICAddNoise3DFunctionBody: Failed to add gl_PMXPermTexture to the symbol table\n"));
		return IMG_FALSE;
	}

	_LABEL(psCPD, psICProgram, psICContext->psNoiseFuncs[NOISE3D]->uFunctionDefinitionID);

	/* 	r0[_X_] = P.x;
		r0[_Y_] = P.y;
		r0[_Z_] = P.z;
		r0[_W_] = 0.0f;
	*/
	_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_XYZ, OPRD(psICContext->psNoiseFuncs[NOISE3D]->uParamSymbolID), SWIZ_NA);
	_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(c0), SWIZ_Z);

	// call noiseSetup
	NOISE_SETUP();

	// mov r10, c0.wwwz                                      // setup dot mask so that a DP3 is performed
	//_MOV(psCPD, psICProgram, OPRD(r10),SWIZ_XYZW, OPRD(c0), SWIZ_WWWZ);

	// Get Perm00
	// add r7, r1, c0.z                                      // r7.xyz = Pi.xyz + (0.0f, 0.0f, 0.0f) - can replace with MOV 
	_ADD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZ, OPRD(r1), SWIZ_XYZ, OPRD(c0), SWIZ_Z);

	// tex2d r7, permTexture, r7                             // r7.w = Perm00 = texture2D(permTexture, r7.xy)
	_TEXLD(psCPD, psICProgram, OPRD(r8), SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_XY);
	_MOV(psCPD, psICProgram, OPRD(r7), SWIZ_W, OPRD(r8), SWIZ_W);

	// mov r4.xy, r7.wz                                      // r6.xy = (Perm00, Pi.z)
	_MOV(psCPD, psICProgram, OPRD(r4), SWIZ_XY, OPRD(r7), SWIZ_WZ);

	// Get Perm01
	// add r7, r1, c0.zx                                     // r7.xyz = Pi.xyz + (0.0f, ONE, 0.0f)  
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XYZ, OPRD(r1), SWIZ_XYZ, OPRD(c0), SWIZ_ZXZ);

	// tex2d r7, permTexture, r7                             // r7.w = Perm01 = texture2D(permTexture, r7.xy)
	_TEXLD(psCPD, psICProgram, OPRD(r8), SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7),SWIZ_XY);
	_MOV(psCPD, psICProgram, OPRD(r7), SWIZ_W, OPRD(r8), SWIZ_W);

	// mov r4.zw, r7.xyzw                                    // r4.zw = (Perm01, Pi.z)
	_MOV(psCPD, psICProgram, OPRD(r4), SWIZ_ZW, OPRD(r7), SWIZ_WZ);

	// Get Perm10
	// add r7, r1, c0.xz                                     // r7.xyz = Pi.xyz + (ONE, 0.0f, 0.0f)  
	_ADD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZ, OPRD(r1), SWIZ_XYZ, OPRD(c0), SWIZ_XZZ);
	
	// tex2d r7, permTexture, r7                             // r7.w = Perm10 = texture2D(permTexture, r7.xy)
	_TEXLD(psCPD, psICProgram, OPRD(r8), SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_XY);
	_MOV(psCPD, psICProgram, OPRD(r7), SWIZ_W, OPRD(r8), SWIZ_W);

	// mov r5.xy, r7.wz                                      // r5.xy = (Perm10, Pi.z)
	_MOV(psCPD, psICProgram, OPRD(r5), SWIZ_XY, OPRD(r7), SWIZ_WZ);

	// Get Perm11
	// add r7, r1, c0.xxzz                                   // r7.xyz = Pi.xyz + (ONE, ONE, 0.0f)  
	_ADD(psCPD, psICProgram, OPRD(r7), SWIZ_XYZ, OPRD(r1), SWIZ_XYZ, OPRD(c0), SWIZ_XXZ);

	// tex2d r7, permTexture, r7                             // r7.w = Perm11 = texture2D(permTexture, r7.xy)
	_TEXLD(psCPD, psICProgram, OPRD(r8), SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_XY);
	_MOV(psCPD, psICProgram, OPRD(r7), SWIZ_W, OPRD(r8), SWIZ_W);

	// mov r5.zw, r7.xyzw                                    // r5.zw = (Perm11, Pi.z)
	_MOV(psCPD, psICProgram, OPRD(r5), SWIZ_ZW, OPRD(r7), SWIZ_WZ);

	/*
	  Calculate n000
	  Result = r1.x PermPi = r4.xy,  PermPiOffsett = c0.z = (0.0f, 0.0f),  Pf = r2, PfOffset = c0.z = (0.0f, 0.0f, 0.0f)  
	*/
	SAMPLE_CORNER3DA(r1, SWIZ_X, permTexture, r4,SWIZ_XY, c0, SWIZ_Z, r2, c0, SWIZ_Z);
	/*
	  Calculate n001
	  Result = r1.y PermPi = r4.xy,  PermPiOffsett = c0.zx = (0.0f, ONE),  Pf = r2, PfOffset = c0.zzwz = (0.0f, 0.0f, 1.0f)  
	*/
	SAMPLE_CORNER3DA(r1, SWIZ_Y, permTexture, r4,SWIZ_XY, c0, SWIZ_ZX, r2, c0, SWIZ_ZZW);
	/*
	  Calculate n010
	  Result = r1.z PermPi = r4.zw,  PermPiOffsett = c0.z = (0.0f, 0.0f),  Pf = r2, PfOffset = c0.zwzz = (0.0f, 1.0f, 0.0f)  
	*/
	SAMPLE_CORNER3DA(r1, SWIZ_Z, permTexture, r4, SWIZ_ZW, c0, SWIZ_Z, r2, c0, SWIZ_ZWZ);
	/*
	  Calculate n011
	  Result = r1.w PermPi = r4.zw,  PermPiOffsett = c0.zx = (0.0f, ONE),  Pf = r2, PfOffset = c0.zwwz = (0.0f, 1.0f, 1.0f)  
	*/
	SAMPLE_CORNER3DA(r1, SWIZ_W, permTexture, r4,SWIZ_ZW, c0, SWIZ_ZX, r2, c0, SWIZ_ZWW);
	/*
	  Calculate n100
	  Result = r4.x PermPi = r5.xy,  PermPiOffsett = c0.z = (0.0f, 0.0f),  Pf = r2, PfOffset = c0.wz = (1.0f, 0.0f, 0.0f)  
	*/
	SAMPLE_CORNER3DA(r4, SWIZ_X, permTexture, r5,SWIZ_XY, c0,SWIZ_Z, r2, c0, SWIZ_WZZ);
	/*
	  Calculate n101
	  Result = r4.y PermPi = r5.xy,  PermPiOffsett = c0.zx = (0.0f, ONE),  Pf = r2, PfOffset = c0.wzwz = (1.0f, 0.0f, 1.0f)  
	*/
	SAMPLE_CORNER3DA(r4, SWIZ_Y, permTexture, r5, SWIZ_XY, c0, SWIZ_ZX, r2, c0, SWIZ_WZW);

	/*
	  Calculate n110
	  Result = r4.z PermPi = r5.zw,  PermPiOffsett = c0.z = (0.0f, 0.0f),  Pf = r2, PfOffset = c0.wwzz = (1.0f, 1.0f, 0.0f)  
	*/
	SAMPLE_CORNER3DA(r4, SWIZ_Z, permTexture, r5, SWIZ_ZW, c0, SWIZ_Z, r2, c0, SWIZ_WWZ);
	/*
	  Calculate n111
	  Result = r4.w PermPi = r5.zw,  PermPiOffsett = c0.zx = (0.0f, ONE),  Pf = r2, PfOffset = c0.wwwz = (1.0f, 1.0f, 1.0f)  
	*/
	SAMPLE_CORNER3DA(r4, SWIZ_W, permTexture, r5, SWIZ_ZW, c0, SWIZ_ZX, r2, c0,SWIZ_W);


	// Blend contributions along x
	// r5 = n_x = mix(vec4(n000, n001, n010, n011), vec4(n100, n101, n110, n111), fade(Pf.x));
	MIX4D(r5, SWIZ_XYZW, r1,SWIZ_XYZW, r4,SWIZ_XYZW, r3, SWIZ_X);

	// Blend contributions along y
	// r5.xy =  n_xy = mix(n_x.xy, n_x.zw, fade(Pf.y));
	MIX2D(r5, SWIZ_XY, r5, SWIZ_XY, r5,SWIZ_ZW, r3, SWIZ_Y);

	// Blend contributions along z
	// r5.x = n_xyz = mix(n_xy.x, n_xy.y, fade(Pf.z));
	MIX1D(r5, SWIZ_X, r5, SWIZ_X, r5, SWIZ_Y, r3, SWIZ_Z);

	/* Move the result out */
	_MOV(psCPD, psICProgram, OPRD(psICContext->psNoiseFuncs[NOISE3D]->uReturnDataSymbolID), SWIZ_NA,
		OPRD(r5), SWIZ_X);

	// ret
	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: ICAddNoise4DFunctionBody
 *
 * Inputs       : psICProgram
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Add function body for: float pmx_noise1_4D(vec4 P);
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL ICAddNoise4DFunctionBody(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	IMG_UINT32 permTexture, gradTexture;
	NoiseRegisters sNRegs;

	ICInitNoiseFuncRegisters(psCPD, psICProgram, &sNRegs);

	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 "gl_PMXPermTexture",
							 0,
							 GLSLBV_PMXPERMTEXTURE,
							 GLSLTS_SAMPLER2D,
							 GLSLTQ_UNIFORM,
							 NOISE_FUNC_PRECISION,
							 &permTexture))
	{
		LOG_INTERNAL_ERROR(("ICAddNoise2DFunctionBody: Failed to add gl_PMXPermTexture to the symbol table\n"));
		return IMG_FALSE;
	}

	if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable, 
							 "gl_PMXGradTexture", 
							 0, 
							 GLSLBV_PMXGRADTEXTURE,
							 GLSLTS_SAMPLER2D, 
							 GLSLTQ_UNIFORM, 
							 NOISE_FUNC_PRECISION,
							 &gradTexture))
	{
		LOG_INTERNAL_ERROR(("ICAddNoise2DFunctionBody: Failed to add gl_PMXPermTexture to the symbol table\n"));
		return IMG_FALSE;
	}

	_LABEL(psCPD, psICProgram, psICContext->psNoiseFuncs[NOISE4D]->uFunctionDefinitionID);

	/*
		r0[_X_] = P.x;
		r0[_Y_] = P.y;
		r0[_Z_] = P.z;
		r0[_W_] = P.w;
	*/
	r0 = psICContext->psNoiseFuncs[NOISE4D]->uParamSymbolID;
	//_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_NA, OPRD(psICContext->psNoiseFuncs[NOISE4D]->uParamSymbolID), SWIZ_NA);

	// call noiseSetup
	NOISE_SETUP();

	// mov r10, c0.wwww                                      // setup dot mask so that a DP4 is performed
	//_MOV(psCPD, psICProgram, OPRD(r10),SWIZ_XYZW, OPRD(c0), SWIZ_W);

	// Get Perm00
	// add r7, r1, c0.z                                      // r7   = Pi + (0.0f, 0.0f, 0.0f, 0.0f) - can replace with MOV 
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XYZW, OPRD(r1),SWIZ_XYZW, OPRD(c0),SWIZ_Z);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm00.x = texture2D(permTexture, r7.xy).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7),SWIZ_XY);

	// mov r4.x, r8.w                                        // r4.x = Perm00.x
	_MOV(psCPD, psICProgram, OPRD(r4),SWIZ_X, OPRD(r8),SWIZ_W);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm00.y = texture2D(permTexture, r7.zw).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA,OPRD(r7), SWIZ_ZW);

	// mov r4.y, r8.w                                        // r4.y = Perm00.y
	_MOV(psCPD, psICProgram, OPRD(r4), SWIZ_Y, OPRD(r8),SWIZ_W);
	

	// Get Perm01
	// add r7, r1, c0.zxzx                                   // r7   = Pi + (0.0f, ONE, 0.0f, ONE) 
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XYZW, OPRD(r1),SWIZ_XYZW, OPRD(c0),SWIZ_ZXZX);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm01.x = texture2D(permTexture, r7.xy).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7),SWIZ_XY);

	// mov r4.z, r8.w                                        // r4.z = Perm01.x
	_MOV(psCPD, psICProgram, OPRD(r4),SWIZ_Z, OPRD(r8),SWIZ_W);

	// tex2d r4.W, permTexture, r7                           // r4.w = Perm01.y = texture2D(permTexture, r7.zw).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_ZW);
	_MOV(psCPD, psICProgram, OPRD(r4),SWIZ_W, OPRD(r8),SWIZ_W);

	// Get Perm10
	// add r7, r1, c0.xzxz                                   // r7   = Pi + (ONE, 0.0f, ONE, 0.0f) 
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XYZW, OPRD(r1),SWIZ_XYZW, OPRD(c0), SWIZ_XZXZ);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm10.x = texture2D(permTexture, r7.xy).w
	_TEXLD(psCPD, psICProgram, OPRD(r8), SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7),SWIZ_XY);

	// mov r5.x, r8.w                                        // r5.x = Perm10.x
	_MOV(psCPD, psICProgram, OPRD(r5),SWIZ_X, OPRD(r8),SWIZ_W);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm10.y = texture2D(permTexture, r7.zw).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_ZW);

	// mov r5.y, r8.w                                        // r5.y = Perm10.y
	_MOV(psCPD, psICProgram, OPRD(r5),SWIZ_Y, OPRD(r8),SWIZ_W);


	// Get Perm11
	// add r7, r1, c0.xxxx                                   // r7   = Pi + (ONE, ONE, ONE, ONE) 
	_ADD(psCPD, psICProgram, OPRD(r7),SWIZ_XYZW, OPRD(r1),SWIZ_XYZW, OPRD(c0), SWIZ_X);

	// tex2d r8.w, permTexture, r7                           // r8.w = Perm11.x = texture2D(permTexture, r7.xy).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7),SWIZ_XY);

	// mov r5.z, r8.w                                        // r5.z = Perm11.x
	_MOV(psCPD, psICProgram, OPRD(r5),SWIZ_Z, OPRD(r8),SWIZ_W);

	// tex2d r5.W, permTexture, r7                           // r5.w = Perm11.y = texture2D(permTexture, r7.zw).w
	_TEXLD(psCPD, psICProgram, OPRD(r8),SWIZ_NA, OPRD(permTexture), SWIZ_NA, OPRD(r7), SWIZ_ZW);
	_MOV(psCPD, psICProgram, OPRD(r5),SWIZ_W, OPRD(r8),SWIZ_W);

	// Sample n0000 
	// mov r9.xy, r4.xy/                                        / r9.xy = (perm00.x, perm00.y, perm01.x, perm01.y)
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_XYZW, OPRD(r4),SWIZ_XYZW);

	/*
	  Calculate n0000
	  Result = r6.x,  texCoord = r9.xy,  offsetPf = r2.xyzw
	*/
	SAMPLE_CORNER4D(r6, SWIZ_X, gradTexture, r9,SWIZ_XY, r2, SWIZ_XYZW);


	// Sample n0001 
	// add r8, r2, -c0.zzzw                                 // r8 = Pf - (0.0f, 0.0f, 0.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_ZZZW);

	/*
	  Calculate n0001
	  Result = r6.y,  texCoord = r9.xw,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r6, SWIZ_Y, gradTexture, r9, SWIZ_XW, r8, SWIZ_XYZW);


	// Sample n0010 
	// mov r9.y, r5.y,                                        // r9.xy = (perm00.x, Perm10.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_Y);

	// add r8, r2, -c0.zzwz                                  // r8 = Pf - (0.0f, 0.0f, 1.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_ZZWZ);

	/*
	  Calculate n0010
	  Result = r6.z,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r6, SWIZ_Z, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);



	// Sample n0011 
	// mov r9, r5.w,                                        // r9.xy = (perm00.x, Perm11.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_W);

	// add r8, r2, -c0.zzww                                 // r8 = Pf - (0.0f, 0.0f, 1.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_ZZWW);

	/*
	  Calculate n0011
	  Result = r6.w,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r6, SWIZ_W, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n0100 
	// mov r9, r4.zy,                                        // r9.xy = (perm01.x, Perm00.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_XY, OPRD(r4), SWIZ_ZY);

	// add r8, r2, -c0.zwzz                                  // r8 = Pf - (0.0f, 1.0f, 0.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_ZWZZ);

	/*
	  Calculate n0100
	  Result = r1.x,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r1, SWIZ_X, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n0101 
	// mov r9, r4.w,                                        // r9.xy = (perm01.x, Perm01.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r4),SWIZ_W);
	
	// add r8, r2, -c0.zwzw                                 // r8 = Pf - (0.0f, 1.0f, 0.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_ZWZW);

	/*
	  Calculate n0101
	  Result = r1.y,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r1, SWIZ_Y, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n0110 
	// mov r9, r5.y,                                        // r9.xy = (perm01.x, Perm10.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_Y);
	
	// add r8, r2, -c0.zwwz                                 // r8 = Pf - (0.0f, 1.0f, 1.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_ZWWZ);

	/*
	  Calculate n0110
	  Result = r1.z,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r1, SWIZ_Z, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n0111 
	// mov r9, r5.w,                                        // r9.xy = (perm01.x, Perm11.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_W);

	// add r8, r2, -c0.zwww                                 // r8 = Pf - (0.0f, 1.0f, 1.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_ZWWW);

	/*
	  Calculate n0111
	  Result = r1.w,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r1, SWIZ_W, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1000 
	
	// mov r9.x, r5.x,                                       // r9.x  = perm10.x 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_X, OPRD(r5),SWIZ_X);

	// mov r9.y, r4.y,                                       // r9.xy = (perm10.x, Perm00.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r4),SWIZ_Y);

	// add r8, r2, -c0.wz                                    // r8 = Pf - (1.0f, 0.0f, 0.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WZZZ);

	/*
	  Calculate n1000
	  Result = r0.x,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r0, SWIZ_X, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1001 
	// mov r9, r4.w,                                        // r9.xy = (perm10.x, Perm01.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r4),SWIZ_W);
	
	// add r8, r2, -c0.wzzw                                 // r8 = Pf - (1.0f, 0.0f, 0.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WZZW);

	/*
	  Calculate n1001
	  Result = r0.y,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r0, SWIZ_Y, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1010 
	// mov r9, r5.y,                                         // r9.xy = (perm10.x, Perm10.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_Y);

	// add r8, r2, -c0.wzwz                                  // r8 = Pf - (1.0f, 0.0f, 1.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WZWZ);

	/*
	  Calculate n1010
	  Result = r0.z,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r0, SWIZ_Z, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1011 
	// mov r9, r5.w,                                         // r9.xy = (perm10.x, Perm11.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_W);

	// add r8, r2, -c0.wzww                                  // r8 = Pf - (1.0f, 0.0f, 1.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WZWW);

	/*
	  Calculate n1011
	  Result = r0.w,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r0, SWIZ_W, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1100 

	// mov r9.x, r5.z,                                       // r9.x  = perm11.x 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_X, OPRD(r5),SWIZ_Z);

	// mov r9.y, r4.y,                                       // r9.xy = (perm11.x, Perm00.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r4),SWIZ_Y);

	// add r8, r2, -c0.wwzz                                  // r8 = Pf - (1.0f, 1.0f, 0.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WWZZ);

	/*
	  Calculate n1100
	  Result = r11.x,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r11, SWIZ_X, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1101 
	// mov r9, r4.w,                                        // r9.xy = (perm11.x, Perm01.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r4),SWIZ_W);

	// add r8, r2, -c0.wwzw                                 // r8 = Pf - (1.0f, 1.0f, 0.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_WWZW);

	/*
	  Calculate n1101
	  Result = r11.y,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r11, SWIZ_Y, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1110 
	// mov r9, r5.y,                                        // r9.xy = (perm11.x, Perm10.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_Y);

	// add r8, r2, -c0.wwwz                                 // r8 = Pf - (1.0f, 1.0f, 1.0f, 0.0f)
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0), SWIZ_WWWZ);

	/*
	  Calculate n1110
	  Result = r11.z,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r11, SWIZ_Z, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Sample n1111 
	// mov r9, r5.w,                                        // r9.xy = (perm11.x, Perm11.y) 
	_MOV(psCPD, psICProgram, OPRD(r9),SWIZ_Y, OPRD(r5),SWIZ_W);

	// add r8, r2, -c0.wwww                                 // r8 = Pf - (1.0f, 1.0f, 1.0f, 1.0f) 
	_ADD(psCPD, psICProgram, OPRD(r8),SWIZ_XYZW, OPRD(r2),SWIZ_XYZW, OPRD_NEG(c0),SWIZ_W);

	/*
	  Calculate n1111
	  Result = r11.w,  texCoord = r9.xy,  offsetPf = r8.xyzw
	*/
	SAMPLE_CORNER4D(r11, SWIZ_W, gradTexture, r9,SWIZ_XY, r8, SWIZ_XYZW);


	// Blend contributions along x
	// n_x0 = mix4D(n0000, n0001, n0010, n0011,
	//              n1000, n1001, n1010, n1011, 
	//              fade.x);
	MIX4D(r0, SWIZ_XYZW, r6,SWIZ_XYZW, r0,SWIZ_XYZW, r3,SWIZ_X);

	// Blend contributions along x
	// n_x1 = mix4D(n0100, n0101, n0110, n0111,
	//              n1100, n1101, n1110, n1111, 
	//              fade.x);
	MIX4D(r6, SWIZ_XYZW, r1,SWIZ_XYZW, r11,SWIZ_XYZW, r3,SWIZ_X);

	// Blend contributions along y
	// n_xy = mix4D(n_x0.x, n_x0.y, n_x0.z, n_x0.w, 
	//			    n_x1.x, n_x1.y, n_x1.z, n_x1.w,
	//			    fade.y);
	MIX4D(r6, SWIZ_XYZW, r0,SWIZ_XYZW, r6,SWIZ_XYZW, r3,SWIZ_Y);

	// Blend contributions along z
	// n_xyz = mix2D(n_xy.x, n_xy.y, 
	//	 		     n_xy.z, n_xy.w,
	//			     fade.z);
	MIX2D(r6, SWIZ_XY, r6,SWIZ_XY, r6, SWIZ_ZW, r3,SWIZ_Z);

	// Blend contributions along w
	// n_xyzw = mix1D(n_xyz.x, n_xyz.y, fade.w);
	MIX1D(r6, SWIZ_X, r6,SWIZ_X, r6,SWIZ_Y, r3,SWIZ_W);

	/* Move the result out */
	_MOV(psCPD, psICProgram, OPRD(psICContext->psNoiseFuncs[NOISE4D]->uReturnDataSymbolID), SWIZ_NA,
		OPRD(r6), SWIZ_X);

	_RET(psCPD, psICProgram);

	return IMG_TRUE;
}

#undef ONE 
#undef ONEHALF
#undef _X_
#undef _Y_
#undef _Z_
#undef _W_

#undef r0
#undef r1
#undef r2
#undef r3
#undef r4
#undef r5
#undef r6
#undef r7
#undef r8
#undef r9
#undef r10
#undef r11
#undef c0
#undef c1 
#undef c2

/* 
	
	Emulate fog factor calculation :

		// Pass through
		MOV r0.x gl_FogFragCoord

		// LINEAR
		// Multiple eye distance by (-1.0f / (end - start)) and add to (end / (end - start))
		MAD r0.y gl_FogFragCoord gl_PMXFogParam.z gl_PMXFogParam.w

		// EXP
		MUL r0.z gl_FogFragCoord gl_PMXFogParam.x
		EXP	r0.z -r0.z
	
		// EXP2
		MUL r0.w gl_FogFragCoord gl_PMXFogParam.y
		MUL r0.w r0.w r0.w
		EXP r0.w -r0.w

		// DOT
		DOT gl_FogFragCoord gl_PMXFogMode r0

*/


IMG_INTERNAL IMG_BOOL ICInsertCodeForFogFactor(GLSLCompilerPrivateData *psCPD, 
											   GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	if((psICContext->eBuiltInsWrittenTo & GLSLBVWT_FOGFRAGCOORD))
	{
		IMG_UINT32 uFogModeID, uFogParamID, uFogFragCoordID, r0, uConstantID;
		GLSLPrecisionQualifier eGLFogFragCoordPrecision;

		/* gl_FogFragCoord */
		uFogFragCoordID = ICFindBuiltInVariables(psCPD, psICProgram, "gl_FogFragCoord");
		if(uFogFragCoordID == 0)
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForFogFactor: Failed to find gl_FogFragCoord !\n"));
			return IMG_FALSE;
		}
		eGLFogFragCoordPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, uFogFragCoordID);

		/* Add gl_PMXFogMode to the symbol table */
		if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
								 "gl_PMXFogMode",
								 0,
								 GLSLBV_PMXFOGMODE,
								 GLSLTS_VEC4,
								 GLSLTQ_UNIFORM,
								 eGLFogFragCoordPrecision,
								 &uFogModeID))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForFogFactor: Failed to add builtin variable gl_PMXFogMode to the symbol table !\n"));
			return IMG_FALSE;
		}

		/* Add gl_PMXFogParam to the symbol table */
		if(!AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
								 "gl_PMXFogParam",
								 0,
								 GLSLBV_PMXFOGPARAM,
								 GLSLTS_VEC4,
								 GLSLTQ_UNIFORM,
								 eGLFogFragCoordPrecision,
								 &uFogParamID))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForFogFactor: Failed to add builtin variable gl_PMXFogPARAM to the symbol table !\n"));
			return IMG_FALSE;
		}

		if(!ICAddTemporary(psCPD, psICProgram, GLSLTS_VEC4, eGLFogFragCoordPrecision, &r0))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForFogFactor: Failed to add vec4 temp to the symbol table \n"));
			return IMG_FALSE;
		}
		
		if(!AddFloatConstant(psCPD, psICProgram->psSymbolTable, -1.442695041f, eGLFogFragCoordPrecision, IMG_TRUE, &uConstantID))
		{
			LOG_INTERNAL_ERROR(("ICInsertCodeForFogFactor: Failed to add float constant -1.442695041 to the symbol table\n"));
			return IMG_FALSE;
		}

		/* MOV r0.x gl_FogFragCoord */
		_MOV(psCPD, psICProgram, OPRD(r0), SWIZ_X, OPRD(uFogFragCoordID), SWIZ_NA);

		// LINEAR
		// Multiple eye distance by (-1.0f / (end - start)) and add to (end / (end - start))
		//MAD r0.y gl_FogFragCoord gl_PMXFogParam.z gl_PMXFogParam.w
		_MAD(psCPD, psICProgram, OPRD(r0), SWIZ_Y, OPRD(uFogFragCoordID), SWIZ_NA, OPRD(uFogParamID), SWIZ_Z, OPRD(uFogParamID), SWIZ_W);

		// EXP
		// MUL r0.z gl_FogFragCoord gl_PMXFogParam.x
		// EXP	r0.z -r0.z
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_Z, OPRD(uFogFragCoordID), SWIZ_NA, OPRD(uFogParamID), SWIZ_X);
		_EXP2(psCPD, psICProgram, OPRD(r0), SWIZ_Z, OPRD_NEG(r0), SWIZ_Z); 

		// EXP2
		//MUL r0.w gl_FogFragCoord gl_PMXFogParam.y
		//MUL r0.w r0.w r0.w
		//EXP r0.w -r0.w
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(uFogFragCoordID), SWIZ_NA, OPRD(uFogParamID), SWIZ_Y);
		_MUL(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W, OPRD(r0), SWIZ_W); 
		_EXP2(psCPD, psICProgram, OPRD(r0), SWIZ_W, OPRD_NEG(r0), SWIZ_W); 

		// DOT
		//DOT gl_FogFragCoord gl_PMXFogMode r0
		_DOT(psCPD, psICProgram, OPRD(uFogFragCoordID), SWIZ_NA, OPRD(uFogModeID), SWIZ_NA, OPRD(r0), SWIZ_NA);

	}

	return IMG_TRUE;
}


/******************************************************************************
 * Function Name: ICEmulateTextureProj
 *
 * Inputs       : psICProgram, psNode, uTextureDim, eTextureLookupOp
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Projective division and then texture look up.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICEmulateTextureProj(GLSLCompilerPrivateData *psCPD, 
										   GLSLICProgram		*psICProgram, 
											GLSLNode			*psNode, 
											GLSLICOperandInfo *psDestOperand,
											GLSLICOpcode		eTextureLookupOp)
{
	GLSLICOperandInfo	sOperands[5]; 
	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uTemp;
	GLSLTypeSpecifier  eCoordType;
	IMG_UINT32 uCoordDim;
	IMG_UINT32 i;
	IMG_UINT32 auLastSwiz[] = {0, SWIZ_X, SWIZ_Y, SWIZ_Z, SWIZ_W};
	GLSLICOperandInfo sTempCoord[2];
	GLSLPrecisionQualifier eTexCoordPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[1]->uSymbolTableID);
	IMG_UINT32 uTextureDim = GLSLTypeSpecifierDimensionTable(
		ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[0]->uSymbolTableID));

	/* Get the source line number */
	psCPD->uCurSrcLine = GET_SHADERCODE_LINE_NUMBER(psNode);

	/* Process all children */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &sOperands[i]);
	}

	/* Get the number of components for coord argument */
	eCoordType	= ICGetSymbolTypeSpecifier(psCPD, psICProgram->psSymbolTable, psNode->ppsChildren[1]->uSymbolTableID);
	if(sOperands[1].sSwizWMask.uNumComponents)
	{
		uCoordDim = sOperands[1].sSwizWMask.uNumComponents;
	}
	else
	{
		uCoordDim = TYPESPECIFIER_NUM_COMPONENTS(eCoordType);
	}
		
	/* Add a temp to store the divided coordinate, have the same type as coord */
	if( !ICAddTemporary(psCPD, psICProgram, (GLSLTypeSpecifier)(GLSLTS_FLOAT + uTextureDim - 1), eTexCoordPrecision, &uTemp))
	{
		LOG_INTERNAL_ERROR(("ICEmulateTextureProj: Failed to add new temp !\n"));
		return;
	}

	/* DIV temp coord.x(xy/xyz) coord.w */
	sTempCoord[0] = sOperands[1];
	sTempCoord[1] = sOperands[1];
	ICApplyOperandMoreSwiz(auDimSwiz[uTextureDim], &sTempCoord[0]);
	ICApplyOperandMoreSwiz(auLastSwiz[uCoordDim], &sTempCoord[1]);
	ICAddICInstructiona(psCPD, psICProgram, GLSLIC_OP_DIV, 2, pszLineStart, uTemp, &sTempCoord[0]);
	
	/* Overwrite the operand 1 with the temp */
	ICFreeOperandOffsetList(&sOperands[1]);
	ICInitOperandInfo(uTemp, &sOperands[1]);

	/* Actual texture lookup */
	ICAddICInstruction(psCPD, psICProgram, eTextureLookupOp, psNode->uNumChildren, pszLineStart, psDestOperand, &sOperands[0]);

	/* Free all offsets attached */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICFreeOperandOffsetList(&sOperands[i]);
	}
}

#if defined(SUPPORT_GL_TEXTURE_RECTANGLE)
/******************************************************************************
 * Function Name: ICEmulateTextureRect
 *
 * Inputs       : psICProgram, psNode, uTextureDim, eTextureLookupOp
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Dimension division and then texture look up.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICEmulateTextureRect(GLSLCompilerPrivateData	*psCPD, 
										   GLSLICProgram			*psICProgram, 
										   GLSLNode					*psNode, 
										   GLSLICOperandInfo		*psDestOperand)
{

	IMG_CHAR *pszLineStart = GET_SHADERCODE_LINE(psNode);
	IMG_UINT32 uTextureSizeArrayId, uTemp, i;
	GLSLICOperandInfo	sTextureSizeArrayOperand, sFloatCoordOperand;
	GLSLICOperandInfo asOperands[MAX_OPRDS];
	GLSLICOperandInfo *psSamplerOperand = &asOperands[0];
	GLSLICOperandInfo *psCoordOperand = &asOperands[1];

	GLSLNode *psSamplerNode = psNode->ppsChildren[0];
	GLSLFullySpecifiedType *psSamplerType = ICGetSymbolFullType(psCPD, psICProgram->psSymbolTable, psSamplerNode->uSymbolTableID);
	IMG_UINT32 uDimension = GLSLTypeSpecifierDimensionTable(psSamplerType->eTypeSpecifier);
	
	/* Process arguments */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICProcessNodeOperand(psCPD, psICProgram, psNode->ppsChildren[i], &asOperands[i]);
	}

	AddBuiltInIdentifier(psCPD, psICProgram->psSymbolTable,
							 "gl_IMGTextureSize",
							 /*UF_MAX_TEXTURE*/16,	/* FIXME!!! */
							 GLSLBV_TEXTURESIZE,
							 GLSLTS_IVEC4,
							 GLSLTQ_UNIFORM,
							 GLSLPRECQ_HIGH,
							 &uTextureSizeArrayId);

	ICInitOperandInfo(uTextureSizeArrayId, &sTextureSizeArrayOperand);
	ICAddOperandOffset(&sTextureSizeArrayOperand, 0, psSamplerNode->uSymbolTableID);

	ICApplyOperandMoreSwiz(auDimSwiz[uDimension], 
		&sTextureSizeArrayOperand);

	/**
	 * We store the final normalized texture coordinate inside a temp VEC2 
	 * variable.
	 */
	ICAddTemporary(psCPD, psICProgram, GLSLTS_FLOAT + uDimension - 1, GLSLPRECQ_HIGH, &uTemp);
	ICInitOperandInfo(uTemp, &sFloatCoordOperand);

	/** MOV temp, dims. */
	ICAddICInstruction2(psCPD, psICProgram, GLSLIC_OP_MOV, pszLineStart,
		&sFloatCoordOperand, &sTextureSizeArrayOperand);

	/* DIV temp coord dimensions */
	ICAddICInstruction3(psCPD, psICProgram, GLSLIC_OP_DIV, pszLineStart,
		&sFloatCoordOperand, psCoordOperand, &sFloatCoordOperand);

	/** LD dest, texture, coords. */
	ICAddICInstruction3(psCPD, psICProgram, GLSLIC_OP_TEXLD, pszLineStart,
		psDestOperand, psSamplerOperand, &sFloatCoordOperand);
	
	/* Free all offsets attached */
	for(i = 0; i < psNode->uNumChildren; i++)
	{
		ICFreeOperandOffsetList(&asOperands[i]);
	}
	ICFreeOperandOffsetList(&sTextureSizeArrayOperand);
}

#endif

/******************************************************************************
 End of file (icemul.c)
******************************************************************************/
