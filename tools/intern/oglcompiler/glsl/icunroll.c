/******************************************************************************
 * Name         : icunroll.c
 * Created      : 11/03/2005
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means,electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: icunroll.c $
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

#define IS_COMPARE_OP(op) ( op == GLSLIC_OP_SLT || \
							op == GLSLIC_OP_SLE || \
							op == GLSLIC_OP_SGT || \
							op == GLSLIC_OP_SGE || \
							op == GLSLIC_OP_SEQ || \
							op == GLSLIC_OP_SNE )

#define IS_BREAK_OP(op)		(op == GLSLIC_OP_BREAK)
#define IS_ENDIF_OP(op)		(op == GLSLIC_OP_ENDIF)
#define IS_MOV_OP(op)		(op == GLSLIC_OP_MOV)
#define IS_NOP(op)			(op == GLSLIC_OP_NOP)

#define IS_LOOP_OP(op)		(op == GLSLIC_OP_LOOP)


typedef struct GLSLICInvariantTAG
{
	IMG_BOOL			bInvariant;
	IMG_INT32				iInvariantValue;
	IMG_INT32				iWriteToLine[MAX_OPRDS];
} GLSLICInvariant;

typedef struct 
{
	/* Init expression */
	IMG_UINT32		uLoopVarID;
	IMG_INT32		iInitValue;

	/* Condition expression */
	GLSLICOpcode	eCompareOp;
	IMG_INT32		iComparedValue;

	/* Updating expression */
	GLSLICOpcode	eUpdateOp;
	IMG_INT32		iUpdateValue;

	IMG_UINT32		uNumBodyInstrs;
	GLSLICInvariant	*apsInvariants;

	IMG_BOOL		bRelativeAddressing;
	IMG_UINT32		uNumIterations;

	IMG_BOOL		bLoopInside;

} GLSLICLoopInfo;

/******************************************************************************
 * Function Name: FindWrittenToForSymbolID
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Find out which line, a symbol ID is written to.
 *****************************************************************************/
static IMG_INT32 FindWrittenToLineForSymbolID(GLSLICInstruction		*psSearchStart,
												IMG_INT32				iStartIndex,
												IMG_UINT32				uSymID)
{
	IMG_INT32 iCodeConditionLevel = 0;
	GLSLICInstruction *psInstr;
	IMG_INT32 loc;

	psInstr = psSearchStart;
	for(loc = iStartIndex; loc >= 0; loc--, psInstr = psInstr->psPrev)
	{
		switch(psInstr->eOpCode)
		{
		case GLSLIC_OP_ENDIF:
		case GLSLIC_OP_ENDLOOP:
			iCodeConditionLevel++;
			break;
		case GLSLIC_OP_IF:
		case GLSLIC_OP_IFNOT:
		case GLSLIC_OP_IFLT:
		case GLSLIC_OP_IFGT:
		case GLSLIC_OP_IFLE:
		case GLSLIC_OP_IFGE:
		case GLSLIC_OP_IFEQ:
		case GLSLIC_OP_IFNE:
		case GLSLIC_OP_LOOP:
			iCodeConditionLevel--;
			break;
		default:
			break;
		}

		if(ICOP_HAS_DEST(psInstr->eOpCode) && (uSymID == psInstr->asOperand[DEST].uSymbolID))
		{
			if(iCodeConditionLevel == 0) 
			{
				return loc;
			}
			else return -1;
		}
	}
	return -1;
}

/******************************************************************************
 * Function Name: FindWrittenToForSymbolID
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Find out which line, a symbol ID is written to.
 *****************************************************************************/
static GLSLICInstruction *FindWrittenToInstrForSymbolID(GLSLICInstruction *psSearchStart,
														GLSLICInstruction *psSearchEnd,
														IMG_UINT32			uSymID) 
{
	IMG_INT32 iCodeConditionLevel = 0;
	GLSLICInstruction *psInstr;

	for(psInstr = psSearchStart; psInstr != psSearchEnd->psNext; psInstr = psInstr->psPrev)
	{
		switch(psInstr->eOpCode)
		{
		case GLSLIC_OP_ENDIF:
		case GLSLIC_OP_ENDLOOP:
			iCodeConditionLevel++;
			break;
		case GLSLIC_OP_IF:
		case GLSLIC_OP_IFNOT:
		case GLSLIC_OP_IFLT:
		case GLSLIC_OP_IFGT:
		case GLSLIC_OP_IFLE:
		case GLSLIC_OP_IFGE:
		case GLSLIC_OP_IFEQ:
		case GLSLIC_OP_IFNE:
		case GLSLIC_OP_LOOP:
			iCodeConditionLevel--;
			break;
		default:
			break;
		}

		if(ICOP_HAS_DEST(psInstr->eOpCode) && (uSymID == psInstr->asOperand[DEST].uSymbolID))
		{
			if(iCodeConditionLevel == 0) 
			{
				return psInstr;
			}

			else return IMG_NULL;
		}
	}
	return IMG_NULL;
}
/******************************************************************************
 * Function Name: ExamineLoopUpdateCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Examine updating code for loop 'for' to see if it is ok to be unrolled. 
 *****************************************************************************/

static IMG_BOOL ExamineLoopUpdateCode(GLSLCompilerPrivateData *psCPD,
									  GLSLICProgram		*psICProgram,
									  GLSLICInstruction *psStart,
									  GLSLICInstruction *psEnd,
									  GLSLICLoopInfo	*psLoopInfo)
{
	IMG_UINT32 uNumLValues = 0;
	IMG_BOOL bSingleInteger;
	GLSLICInstruction *psInstr, *psInstr1;
	SymTable *psSymbolTab = psICProgram->psSymbolTable;

	/* 
		Restrictions for increment expression:

		1) Contain NOP/MOV/ADD/SUB instructions only
		2) Only one integer lvaue identifier is being written
		2) The code looks like: ADD/SUB i i c or ADD i c i
		3) The loop variable can not have a global scope.

	*/
	
	/* Remove redundant mov first (only for increment express) */
	for(psInstr = psEnd; psInstr != psStart->psPrev; psInstr = psInstr->psPrev)
	{
		switch(psInstr->eOpCode)
		{
		case GLSLIC_OP_NOP:
		case GLSLIC_OP_MOV:
		case GLSLIC_OP_ADD:
		case GLSLIC_OP_SUB:
			break;
		default:
			/* Reject any other ops for increment expression */
			return IMG_FALSE;
		}

		if(ICOP_HAS_DEST(psInstr->eOpCode) && IsSymbolLValue(psCPD, psSymbolTab, psInstr->asOperand[DEST].uSymbolID, &bSingleInteger))
		{
			IMG_INT32 iData;
			IMG_UINT32 uSrcASymID = psInstr->asOperand[SRCA].uSymbolID;

			if( bSingleInteger && 
				IS_MOV_OP(psInstr->eOpCode) && 
			   !IsSymbolLValue(psCPD, psSymbolTab, uSrcASymID, &bSingleInteger) &&
			   !IsSymbolIntConstant(psCPD, psSymbolTab, uSrcASymID, &iData))
			{
				GLSLICInstruction *psWriteTo;

				psWriteTo = FindWrittenToInstrForSymbolID(psInstr->psPrev, psStart, uSrcASymID);
				if(psWriteTo )
				{
					psInstr->eOpCode = psWriteTo->eOpCode;

					psInstr->asOperand[SRCA] = psWriteTo->asOperand[SRCA];
					psInstr->asOperand[SRCB] = psWriteTo->asOperand[SRCB];
					psInstr->asOperand[SRCC] = psWriteTo->asOperand[SRCC];
				}
			}
		}

	}

	/*
		Check how many lvalue varibles to be written,
		reject if more than one lvalue identifiers being written or it is not a single integer
	*/
	psInstr1 = IMG_NULL;

	for(psInstr = psStart; psInstr != psEnd->psNext; psInstr = psInstr->psNext)
	{
		if(ICOP_HAS_DEST(psInstr->eOpCode) && 
			IsSymbolLValue(psCPD, psSymbolTab, psInstr->asOperand[DEST].uSymbolID, &bSingleInteger))
		{
			uNumLValues++;
			if(bSingleInteger) 
			{
				psInstr1 = psInstr;
			}
		}
	}

	if(uNumLValues==1 && psInstr1)
	{
		/* this instruction should look like:
				ADD/SUB i i c (c is integer constant)
			or  ADD i c i
		*/

		if(psInstr1->eOpCode == GLSLIC_OP_ADD || psInstr1->eOpCode == GLSLIC_OP_SUB)
		{
			IMG_INT32 iData = 0;
			IMG_BOOL bOK = IMG_FALSE;
			IMG_UINT32 uScopeLevel = 0;
			IMG_UINT32 uLoopVarID = psInstr1->asOperand[DEST].uSymbolID;

			if( psInstr1->asOperand[SRCA].uSymbolID == uLoopVarID &&
				 IsSymbolIntConstant(psCPD, psSymbolTab, psInstr1->asOperand[SRCB].uSymbolID, &iData) )
			{
				/* Has a form of ADD/SUB i i c */
				bOK = IMG_TRUE;
			}
			else if(psInstr1->eOpCode == GLSLIC_OP_ADD &&
				 psInstr1->asOperand[SRCB].uSymbolID == uLoopVarID &&
				 IsSymbolIntConstant(psCPD, psSymbolTab, psInstr1->asOperand[SRCA].uSymbolID, &iData) ) 

			{
				/* Has a form of ADD i c i */
				bOK = IMG_TRUE;
			}

			if(bOK)
			{
				if(!GetSymbolScopeLevel(psICProgram->psSymbolTable, uLoopVarID, &uScopeLevel))
				{
					LOG_INTERNAL_ERROR(("ExamineLoopUpdateCode: Failed to get scope level for symbol ID: %u\n", uLoopVarID));
					return IMG_FALSE;
				}

				/* The loop variable can not have a global scope.*/
				if(uScopeLevel >0)
				{
					psLoopInfo->uLoopVarID = psInstr1->asOperand[DEST].uSymbolID;
					psLoopInfo->eUpdateOp = psInstr1->eOpCode;
					psLoopInfo->iUpdateValue = iData;

					return IMG_TRUE;
				}
				
			}
		}
	}

	return IMG_FALSE;
}


/******************************************************************************
 * Function Name: ExamineLoopBodyCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Examine loop body for loop 'for' to see if it is ok to be unrolled. 
 *****************************************************************************/
static IMG_BOOL ExamineLoopBodyCode(GLSLCompilerPrivateData *psCPD,
									GLSLICProgram		*psICProgram, 
									GLSLICInstruction	*psStart, 
									GLSLICInstruction	*psEnd, 
									GLSLICLoopInfo		*psLoopInfo)
{
	GLSLICInstruction *psInstr;
	IMG_UINT32 i, j, k;
	IMG_UINT32 uSymID;
	IMG_BOOL bInvariant;
	GLSLICInvariant *psInvariants ;
	IMG_INT32 iLoopLevel = 0;
	IMG_UINT32 uNumInstrs = 0;
	IMG_BOOL bStaticLoop = IMG_TRUE;

	/*
		Restrictions for loop body:

		1)	No continue, break, discard in the loop body
		2)	The loop variable cannot be written. 

		Also check whether the body contain relative addressing
	*/
	for(psInstr = psStart; psInstr != psEnd->psNext; psInstr = psInstr->psNext)
	{
		if(psInstr->eOpCode == GLSLIC_OP_LOOP || psInstr->eOpCode == GLSLIC_OP_STATICLOOP)
		{
			iLoopLevel++;

			psLoopInfo->bLoopInside = IMG_TRUE;
		}
		else if(psInstr->eOpCode == GLSLIC_OP_ENDLOOP)
		{
			iLoopLevel--;
		}

		/* Contains continue, break or discard? reject it */
		if( psInstr->eOpCode == GLSLIC_OP_CONTINUE ||
			psInstr->eOpCode == GLSLIC_OP_BREAK ||
			psInstr->eOpCode == GLSLIC_OP_DISCARD )
		{
			if(iLoopLevel == 0) 
			{
				bStaticLoop = IMG_FALSE;
			}
		}

		for(j = 0; j < ICOP_NUM_SRCS(psInstr->eOpCode) + 1; j++)
		{
			if(!ICOP_HAS_DEST(psInstr->eOpCode) && j==0) continue;

			for(k = 0; k < psInstr->asOperand[j].uNumOffsets; k++)
			{
				if(psInstr->asOperand[j].psOffsets[k].uOffsetSymbolID)
				{
					psLoopInfo->bRelativeAddressing = IMG_TRUE;
				}
			}
		}

		/* Loop variable has been written? reject it */
		if(ICOP_HAS_DEST(psInstr->eOpCode) && psInstr->asOperand[DEST].uSymbolID == psLoopInfo->uLoopVarID)
		{
			bStaticLoop = IMG_FALSE;
		}

		uNumInstrs++;

		if(psInstr == psEnd) break;
	}

	psLoopInfo->uNumBodyInstrs = uNumInstrs;

	/* Determine which lines are invariant */
	if(bStaticLoop && psLoopInfo->uNumBodyInstrs)
	{
		IMG_INT32 iCodeConditionLevel = 0;

		psLoopInfo->apsInvariants = DebugMemAlloc(psLoopInfo->uNumBodyInstrs * sizeof(GLSLICInvariant));
		if(psLoopInfo->apsInvariants == IMG_NULL)
		{
			LOG_INTERNAL_ERROR(("ExamineLoopBodyCode: Failed to allocate memory\n"));
			return IMG_FALSE;
		}
		psInvariants = psLoopInfo->apsInvariants;

		for(i=0, psInstr = psStart ; i < psLoopInfo->uNumBodyInstrs ; i++, psInstr = psInstr->psNext)
		{
			/* Initialise invariant info */
			psInvariants[i].bInvariant = IMG_FALSE;
			for(j = 0; j < MAX_OPRDS; j++)
			{
				psInvariants[i].iWriteToLine[j] = -1;  
			}

			switch(psInstr->eOpCode)
			{
			case GLSLIC_OP_IF:
			case GLSLIC_OP_IFNOT:
			case GLSLIC_OP_IFLT:
			case GLSLIC_OP_IFGT:
			case GLSLIC_OP_IFLE:
			case GLSLIC_OP_IFGE:
			case GLSLIC_OP_IFEQ:
			case GLSLIC_OP_IFNE:
			case GLSLIC_OP_LOOP:
			case GLSLIC_OP_STATICLOOP:
				iCodeConditionLevel++;
				continue;
			case GLSLIC_OP_ENDIF:
			case GLSLIC_OP_ENDLOOP:
				iCodeConditionLevel--;
				continue;
			case GLSLIC_OP_MOV:
			case GLSLIC_OP_ADD:
			case GLSLIC_OP_SUB:
			case GLSLIC_OP_MUL:
			case GLSLIC_OP_DIV:
				{
					if(iCodeConditionLevel) 
					{
						break;
					}

					bInvariant = IMG_TRUE;
					for(j = SRCA; j < ICOP_NUM_SRCS(psInstr->eOpCode) + 1; j++)
					{
						IMG_INT32 iData;

						if(psInstr->asOperand[j].eInstModifier & GLSLIC_MODIFIER_NEGATE ||
							psInstr->asOperand[j].uNumOffsets ||
							psInstr->asOperand[j].sSwizWMask.uNumComponents)
						{
							bInvariant = IMG_FALSE;
							break;
						}
						uSymID = psInstr->asOperand[j].uSymbolID;

						if(IsSymbolIntConstant(psCPD, psICProgram->psSymbolTable, uSymID, &iData))
						{
							continue;
						}
						
						else if(uSymID == psLoopInfo->uLoopVarID)
						{
							continue;
						}
						else
						{
							IMG_INT32 iWriteToLine;

							iWriteToLine = FindWrittenToLineForSymbolID(psInstr->psPrev, (IMG_INT32)(i - 1), uSymID);

							if(iWriteToLine != -1 && psInvariants[iWriteToLine].bInvariant)
							{
								psInvariants[i].iWriteToLine[j] = iWriteToLine;
							}
							else
							{
								bInvariant = IMG_FALSE;
								break;
							}
						}
					}

					psInvariants[i].bInvariant = bInvariant;
				}
			default:
				break;
			}

			if(psInstr == psEnd) break;
		}
	}

	return bStaticLoop;
}


/******************************************************************************
 * Function Name: ExamineLoopConditionCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Examine condition expression for loop 'for'to see if it is ok to be unrolled. 
 *****************************************************************************/

static IMG_BOOL ExamineLoopConditionCode(GLSLCompilerPrivateData *psCPD,
										 GLSLICProgram		*psICProgram,
										 GLSLICInstruction	*psStart,
										 GLSLICInstruction	*psEnd,
										 GLSLICLoopInfo		*psLoopInfo)
{
	GLSLICInstruction *psInstr, *psLOOPInstr;

	/* The condition code is normally like this, and we reject any other patterns of condition code 				
		SLT pred a c
		pred LOOP

	*/
	psInstr = psStart;
	psLOOPInstr = psInstr->psNext;

	/* Has only three instructions */
	if(psEnd == psStart) /* one instruction for condition code */
	{
		if( IS_COMPARE_OP(psInstr->eOpCode) &&		/* Need to be compare op */
			IS_LOOP_OP(psLOOPInstr->eOpCode) &&		/* LOOP after condition */
			ICIsSymbolScalar(psCPD, psICProgram, psInstr->asOperand[SRCA].uSymbolID) &&	/* Compare two scalars */
			ICIsSymbolScalar(psCPD, psICProgram, psInstr->asOperand[SRCB].uSymbolID) &&	/* Compare two scalars */
			psInstr->asOperand[DEST].uSymbolID == psLOOPInstr->uPredicateBoolSymID	/* Compare result to be the loop predicate */
		)
		{
			IMG_INT32 iData;

			if(psLoopInfo->uLoopVarID == psInstr->asOperand[SRCA].uSymbolID && 
				IsSymbolIntConstant(psCPD, psICProgram->psSymbolTable, psInstr->asOperand[SRCB].uSymbolID, &iData))
			{
				psLoopInfo->eCompareOp = psInstr->eOpCode;
				psLoopInfo->iComparedValue = iData;
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}


/******************************************************************************
 * Function Name: ExamineLoopInitCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Examine initialisation expression for loop 'for' to see if it is ok to be unrolled. 
 *****************************************************************************/
static IMG_BOOL ExamineLoopInitCode(GLSLCompilerPrivateData *psCPD,
									GLSLICProgram		*psICProgram,
									GLSLICInstruction	*psStart,
									GLSLICInstruction	*psEnd,
									GLSLICLoopInfo		*psLoopInfo)
{
	GLSLICInstruction *psInstr;
	IMG_INT32 iData;

	/* Restrictions:

		The loop variable has to be assigned an int constant 
	*/
	PVR_UNREFERENCED_PARAMETER(psStart);

	for(psInstr = psEnd; psInstr; psInstr = psInstr->psPrev)
	{
		if(psInstr->asOperand[DEST].uSymbolID == psLoopInfo->uLoopVarID)
		{
			if( IS_MOV_OP(psInstr->eOpCode) &&
				IsSymbolIntConstant(psCPD, psICProgram->psSymbolTable, psInstr->asOperand[SRCA].uSymbolID, &iData) )
			{
				psLoopInfo->iInitValue = iData;
				return IMG_TRUE;
			}
			else
			{
				/* Not initialised to a constant */
				return IMG_FALSE;
			}
		}
	}
	
	return IMG_FALSE;
}


/******************************************************************************
 * Function Name: RewriteLoopCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Rewrite loop code with invariant loop variable from uStart to uStart + uCout
 *****************************************************************************/

static IMG_VOID RewriteLoopCode(GLSLCompilerPrivateData *psCPD,
								GLSLICProgram			*psICProgram, 
								GLSLICInstruction		*psStart, 
								GLSLICInstruction		*psEnd,
								GLSLICLoopInfo			*psLoopInfo, 
								IMG_INT32				iLoopValue,
								GLSLPrecisionQualifier	eLoopVarPrecision)
{
	GLSLICInstruction *psInstr;
	IMG_UINT32 i, j, k;
	IMG_UINT32 uLoopValueID = 0;
	SymTable *psSymTab = psICProgram->psSymbolTable;
	IMG_UINT32 uSymID;
	GLSLICInvariant *psInvariants = psLoopInfo->apsInvariants;

	psInstr = psStart;
	
	for(i = 0 ; i < psLoopInfo->uNumBodyInstrs; i++, psInstr = psInstr->psNext)
	{
		if(psInvariants[i].bInvariant)
		{
			IMG_INT32 iValue[MAX_OPRDS], iData;

			/* Evaluate Invariant value */
			for(j = SRCA; j < ICOP_NUM_SRCS(psInstr->eOpCode) + 1; j++)
			{
				uSymID = psInstr->asOperand[j].uSymbolID;
				if(uSymID == psLoopInfo->uLoopVarID)
				{
					iValue[j] = iLoopValue;
				}
				else if(IsSymbolIntConstant(psCPD, psSymTab, uSymID, &iData))
				{
					iValue[j] = iData;
				}
				else
				{
					DebugAssert(psInvariants[i].iWriteToLine[j] != -1);	

					iValue[j] = psInvariants[psInvariants[i].iWriteToLine[j]].iInvariantValue;
				}
			}

			switch(psInstr->eOpCode)
			{
			case GLSLIC_OP_MOV:
				psInvariants[i].iInvariantValue = iValue[SRCA];
				break;
			case GLSLIC_OP_ADD:
				psInvariants[i].iInvariantValue = iValue[SRCA] + iValue[SRCB];
				break;
			case GLSLIC_OP_SUB:
				psInvariants[i].iInvariantValue = iValue[SRCA] - iValue[SRCB];
				break;
			case GLSLIC_OP_MUL:
				psInvariants[i].iInvariantValue = iValue[SRCA] * iValue[SRCB];
				break;
			case GLSLIC_OP_DIV:
				psInvariants[i].iInvariantValue = iValue[SRCA] / iValue[SRCB];
				break;
			default:
				LOG_INTERNAL_ERROR(("RewriteLoopCode: Need to be one of bacic ops to be invariant line"));
				psInvariants[i].iInvariantValue = 0;
				break;
			}
		}

		for(j = 0; j < ICOP_NUM_SRCS(psInstr->eOpCode) + 1; j++)
		{
			if(!ICOP_HAS_DEST(psInstr->eOpCode) && j == DEST) continue;

			if(psInstr->asOperand[j].uSymbolID == psLoopInfo->uLoopVarID)
			{
				if(!uLoopValueID)
				{
					/* Add constant loop value */
					if(!AddIntConstant(psCPD, psICProgram->psSymbolTable, iLoopValue, eLoopVarPrecision, IMG_TRUE, &uLoopValueID))
					{
						LOG_INTERNAL_ERROR(("RewriteLoopCode: Failed to add constant %u", iLoopValue));
					}
				}
	
				psInstr->asOperand[j].uSymbolID = uLoopValueID;
			}

			for(k = 0; k < psInstr->asOperand[j].uNumOffsets; k++)
			{
				if(psInstr->asOperand[j].psOffsets[k].uOffsetSymbolID == psLoopInfo->uLoopVarID)
				{
					psInstr->asOperand[j].psOffsets[k].uOffsetSymbolID = 0;
					psInstr->asOperand[j].psOffsets[k].uStaticOffset = iLoopValue;
				}
				else
				{
					IMG_INT32 iWriteToLine = FindWrittenToLineForSymbolID(psInstr->psPrev, (IMG_INT32)(i - 1), psInstr->asOperand[j].psOffsets[k].uOffsetSymbolID);
					if(iWriteToLine != -1 && psInvariants[iWriteToLine].bInvariant)
					{
						if(psInvariants[iWriteToLine].iInvariantValue < 0)
						{
							LOG_INTERNAL_ERROR(("RewriteLoopCode: Index to an array is less than 0\n"));
							psInvariants[iWriteToLine].iInvariantValue = 0;
						}
							
						psInstr->asOperand[j].psOffsets[k].uOffsetSymbolID = 0;
						psInstr->asOperand[j].psOffsets[k].uStaticOffset = psInvariants[iWriteToLine].iInvariantValue;
					}
				}
			}	
		}

		if(psInstr == psEnd) break;
	}
}

/******************************************************************************
 * Function Name: EvaluateCondition
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : evaluate the condition of loop 'for'
 *****************************************************************************/
static IMG_BOOL EvaluateCondition(GLSLCompilerPrivateData *psCPD, IMG_INT32 iLoopValue, GLSLICLoopInfo *psLoopInfo)
{
	IMG_BOOL condition = IMG_FALSE;

	switch(psLoopInfo->eCompareOp)
	{
	case GLSLIC_OP_SLT:
		condition = (IMG_BOOL)(iLoopValue < psLoopInfo->iComparedValue);
		break;
	case GLSLIC_OP_SLE:
		condition = (IMG_BOOL)(iLoopValue <= psLoopInfo->iComparedValue);
		break;
	case GLSLIC_OP_SGT:
		condition = (IMG_BOOL)(iLoopValue > psLoopInfo->iComparedValue);
		break;
	case GLSLIC_OP_SGE:
		condition = (IMG_BOOL)(iLoopValue >= psLoopInfo->iComparedValue);
		break;
	case GLSLIC_OP_SEQ:
		condition = (IMG_BOOL)(iLoopValue == psLoopInfo->iComparedValue);
		break;
	case GLSLIC_OP_SNE:
		condition = (IMG_BOOL)(iLoopValue != psLoopInfo->iComparedValue);
		break;
	default:
		LOG_INTERNAL_ERROR(("EvaluateCondition: Unsupported compare op for unrolling loops \n"));
		break;
	}

	return condition;
}


static IMG_VOID FreeInvariantInfo(GLSLICLoopInfo *psLoopInfo)
{
	if(psLoopInfo->apsInvariants)
	{
		DebugMemFree(psLoopInfo->apsInvariants);
		psLoopInfo->apsInvariants = IMG_NULL;
	}
}
/******************************************************************************
 * Function Name: CalculateLoopNumIterations
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Pre calculate the number of iterations for a loop
 *****************************************************************************/
static IMG_UINT32 CalculateLoopNumIterations(GLSLCompilerPrivateData *psCPD, GLSLICLoopInfo *psLoopInfo)
{
	IMG_INT32 iLoopValue = psLoopInfo->iInitValue;
	IMG_UINT32 uNumIterations = 0;

	/* 
		If condition is true, we jump out of the loop.  
		Please note, we have inversed the condition when the ic instructions was generated. 
	*/
	while(EvaluateCondition(psCPD, iLoopValue, psLoopInfo))
	{
		switch(psLoopInfo->eUpdateOp)
		{
		case GLSLIC_OP_ADD:
			iLoopValue = iLoopValue + psLoopInfo->iUpdateValue;
			break;
		case GLSLIC_OP_SUB:
			iLoopValue = iLoopValue - psLoopInfo->iUpdateValue;
			break;
		default:
			LOG_INTERNAL_ERROR(("CalculateLoopNumIterations: Unsuported increment op for unrolling loops \n"));
		}

		uNumIterations++;

		if(uNumIterations > 4096)
		{
			break;
		}
	}

	return uNumIterations;
}

/******************************************************************************
 * Function Name: UnrollLoopFORCode
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Unroll loop 'for'
 *****************************************************************************/
static IMG_VOID UnrollLoopFORCode(GLSLCompilerPrivateData *psCPD,
								  GLSLICProgram		*psICProgram,
								  GLSLICInstruction	*psStart,
								  GLSLICInstruction	*psEnd,
								  GLSLICLoopInfo	*psLoopInfo)
{

	IMG_INT32 iLoopValue;
	IMG_UINT32 uLoopValueID = 0;
	IMG_UINT32 uNumIterations = 0;
	GLSLICInstruction *psClonedStart, *psCloneEnd;
	GLSLPrecisionQualifier eLoopVarPrecision;

	if(!psLoopInfo->uNumBodyInstrs)
		return; /* Protect against unrolling empty-body for loops. */
	
	eLoopVarPrecision = ICGetSymbolPrecision(psCPD, psICProgram->psSymbolTable, psLoopInfo->uLoopVarID);
	iLoopValue = psLoopInfo->iInitValue;


	/*
		If condition is true, we jump out of the loop.
		Please note, we have inversed the condition when the ic instructions was generated.
	*/
	while(EvaluateCondition(psCPD, iLoopValue, psLoopInfo))
	{
		if(uNumIterations)
		{
			/* We don't want to rewrite the first loop for the moment because it will be used as base code */
			CloneICodeInstructions(psCPD, psICProgram, psStart, psEnd, &psClonedStart, &psCloneEnd);
			RewriteLoopCode(psCPD, psICProgram, psClonedStart, psCloneEnd, psLoopInfo, iLoopValue, eLoopVarPrecision);
		}

		switch(psLoopInfo->eUpdateOp)
		{
		case GLSLIC_OP_ADD:
			iLoopValue = iLoopValue + psLoopInfo->iUpdateValue;
			break;
		case GLSLIC_OP_SUB:
			iLoopValue = iLoopValue - psLoopInfo->iUpdateValue;
			break;
		default:
			LOG_INTERNAL_ERROR(("UnrollLoopFORCode: Unsuported increment op for unrolling loops \n"));
		}

		uNumIterations++;

		if(uNumIterations > 4096)
		{
			LOG_INTERNAL_ERROR(("UnrollLoopFORCode: The program appears to contain endless looping\n"));
			break;
		}
	}

	/* Check for pre calculated number of iterations for peace of mind */
	if(uNumIterations != psLoopInfo->uNumIterations)
	{
		LOG_INTERNAL_ERROR(("UnrollLoopFORCode: The number of iteration is different form pre-calculation\n"));
	}

	if(uNumIterations)
	{
		/* Re-write the first loop */
		RewriteLoopCode(psCPD, psICProgram, psStart, psEnd, psLoopInfo, psLoopInfo->iInitValue, eLoopVarPrecision);

		/* At the end of loop, the loop variable is always assigned to its final value */
		if(!AddIntConstant(psCPD, psICProgram->psSymbolTable, iLoopValue, eLoopVarPrecision, IMG_TRUE, &uLoopValueID))
		{
			LOG_INTERNAL_ERROR(("RewriteLoopCode: Failed to add constant %u", iLoopValue));
		}
		ICAddICInstruction2b(psCPD, psICProgram, GLSLIC_OP_MOV, IMG_NULL, psLoopInfo->uLoopVarID, uLoopValueID);
	}
	else
	{
		ICRemoveInstructionRange(psICProgram, psStart, psEnd);
	}

}


/******************************************************************************
 * Function Name: ProcessNodeFOR
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ICUnrollLoopFOR(GLSLCompilerPrivateData *psCPD,
									  GLSLICProgram		*psICProgram,
									  IMG_BOOL			bUnrollLoop,
									  GLSLICInstruction	*psInitStart,
									  GLSLICInstruction	*psInitEnd,
									  GLSLICInstruction	*psCondStart,
									  GLSLICInstruction	*psCondEnd,
									  GLSLICInstruction	*psUpdateStart,
									  GLSLICInstruction	*psUpdateEnd)
{
	GLSLICInstruction *psLoopStart, *psLoopEnd;
	GLSLICLoopInfo sLoopInfo;
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);
	GLSLUnrollLoopRules *psUnrollLoopRules = &psICContext->psInitCompilerContext->sUnrollLoopRules;
	IMG_BOOL bStaticLoop;

	/* Initialise the loop information, initially assume the body does not contain relative addressing  */
	memset(&sLoopInfo, 0, sizeof(GLSLICLoopInfo));

	/*
		Regardless of the requested unrolling rules, the following restrictions should apply:

		1)	Only consider loops using for

		2)	Only consider loops where the loop variable is a scalar integer, and not 
			a float or part of a more complex type, can not have a global scope

		3)	Only consider loops where the expression to initialise the loop variable is 
			an assignment of a integer constant. The initialisation can happen before the loop. 
			The initialisation expression can contain other initialisation code.

		4)	Only consider loops where the expression to update the loop variable is an 
			increment/decrement. Also allow an addition/subtraction of a literal integer 
			constant. Only ONE loop variable can be updated. Typical allowed examples are: 
			i++, i += 5, i = i+5 and descrement variations.

		5)  Only consider loops where the condition to terminate the loop is ONE comparison 
			of the loop variable with a integer constant. Allow equality and non-equality tests. 
			If the condition is never met, the code for loops will be removed.  

		6)  Only consider loops where the loop variable is not changed within the body of the 
			loop( the loop variable is not be a destionation operand). The loop variable can not 
			be a global in case it gets updated within a subroutine. 

		7)  Only consider loops which contain no continue, no break, no discard.

		At the end of loops, the loop variable is always assigned to its final value. 

	*/


	/* a typical for loop code looks like this 

		ICODE					Instr No.  (Instr count and notes for how to work out )
		===================================================
		MOV i 0					psInitStart 
		..
		..						psInitEnd
		SLT pred i #10			psCondStart psCondEnd
		pred LOOP				psCondEnd->psNext;
		-						-----------------> psLoopStart = psCondEnd->psNext->psNext
		.						
		.
		.
		.						-----------------> psLoopEnd = psUpdateStart->psPrev->psPrev
		CONTDEST				-----------------> psUpdateStart->psPrev
			ADD i i 1			psUpdateStart
			.					psUpdateEnd
			SLT pred i #10		
		pred ENDLOOP					-----------------> psUpdateEnd->psNext->psNext
	*/

	if(!psCondEnd || !psCondEnd->psNext || !psUpdateStart || !psUpdateStart->psPrev)
	{
		bStaticLoop = IMG_FALSE;

		return;
	}
	else
	{
		/* Get body instructions */
		psLoopStart = psCondEnd->psNext->psNext;
		psLoopEnd = psUpdateStart->psPrev->psPrev;
	}

	/* Examine update code */
	bStaticLoop = ExamineLoopUpdateCode(psCPD, psICProgram, psUpdateStart, psUpdateEnd, &sLoopInfo);
	if(!bStaticLoop) goto TidyUp;

	/* Examine body code */
	bStaticLoop = ExamineLoopBodyCode(psCPD, psICProgram, psLoopStart, psLoopEnd, &sLoopInfo);
	if(!bStaticLoop) goto TidyUp;

	/* Examine jump out condition */
	bStaticLoop = ExamineLoopConditionCode(psCPD, psICProgram, psCondStart, psCondEnd, &sLoopInfo);
	if(!bStaticLoop) goto TidyUp;

	/* Examine init expression */
	bStaticLoop = ExamineLoopInitCode(psCPD, psICProgram, psInitStart, psInitEnd, &sLoopInfo);	
	if(!bStaticLoop) goto TidyUp;


	/* Futher check for requested unrolling rules */
	if(bUnrollLoop)
	{
		if(psICContext->iForLoopLevel > 1)
		{
			/* It is a nested loop, reject it */
			bUnrollLoop = IMG_FALSE;
		}
		else if(sLoopInfo.bLoopInside)
		{
			bUnrollLoop = IMG_FALSE;
		}
		else
		{
			/* Pre calculate number of iterations */
			sLoopInfo.uNumIterations = CalculateLoopNumIterations(psCPD, &sLoopInfo);

			/* Check for maximum of iterations allowed for unrolling */
			if(sLoopInfo.uNumIterations > psUnrollLoopRules->uMaxNumIterations)
			{
				bUnrollLoop = IMG_FALSE;
			}
			/* Check for relative addressing unrolling only */
			else if(psUnrollLoopRules->bUnrollRelativeAddressingOnly && !sLoopInfo.bRelativeAddressing)
			{
				bUnrollLoop = IMG_FALSE;
			}
		}
	}

	/* If finally we decide we still want to go ahead */
	if(bUnrollLoop)
	{	
		/* Remove LOOP instr */
		ICRemoveInstruction(psICProgram, psCondEnd->psNext);

		/* Remove CONDEST instr */
		ICRemoveInstruction(psICProgram, psUpdateStart->psPrev);

		/* Remove ENDLOOP instr */
		ICRemoveInstruction(psICProgram, psUpdateEnd->psNext->psNext);

		/* Remove condition code before ENDLOOP instr */
		ICRemoveInstruction(psICProgram, psUpdateEnd->psNext);

		/* Remove condition code */
		ICRemoveInstructionRange(psICProgram, psCondStart, psCondEnd);

		/* Remove increment code */
		ICRemoveInstructionRange(psICProgram, psUpdateStart, psUpdateEnd);

		/* Actually generate the unrolled code */
		UnrollLoopFORCode(psCPD, psICProgram, psLoopStart, psLoopEnd, &sLoopInfo);
	}
	else
	{
		/* Static loop: replace it with static */

		if(bStaticLoop)
		{
			GLSLICInstruction *psLoopInst = psCondEnd->psNext;
			
			DebugAssert(psLoopInst->eOpCode == GLSLIC_OP_LOOP);

			psLoopInst->eOpCode = GLSLIC_OP_STATICLOOP;
		}
	}

TidyUp:

	/* Free the memory for invariant array */
	FreeInvariantInfo(&sLoopInfo);

}
/******************************************************************************
 End of file (icunroll.c)
******************************************************************************/

