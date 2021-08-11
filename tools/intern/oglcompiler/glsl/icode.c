/**************************************************************************
 * Name         : icode.c
 * Created      : 24/06/2004
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
 * $Log: icode.c $
 **************************************************************************/
#include <stdio.h>
#include <string.h>
#include "glsltree.h"
#include "../parser/debug.h"
#include "error.h"
#include "icode.h"
#include "icodefns.h"
#include "icgen.h"
#include "common.h"
#include "icbuiltin.h"
#include "icemul.h"

#ifdef DUMP_LOGFILES

/******************************************************************************
 * Function Name: DumpASTree
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Dump AST
 *****************************************************************************/
static IMG_VOID DumpASTree(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram, IMG_INT32 iIndent, GLSLNode *psASTree)
{
	IMG_UINT32 i = 0;
	IMG_CHAR str[2560];

	if(!psASTree) return;

	if(psASTree->uSymbolTableID)
	{
		i = sprintf(str, "%s(%s)",  
			NODETYPE_DESC(psASTree->eNodeType),
			GetSymbolName(psICProgram->psSymbolTable, psASTree->uSymbolTableID));

		if(psASTree->eNodeType == GLSLNT_FUNCTION_DEFINITION)
		{
			GLSLFunctionDefinitionData *psDefinitionData;
			IMG_UINT32 j;

			psDefinitionData = (GLSLFunctionDefinitionData *)
				GetSymbolTableData(psICProgram->psSymbolTable, psASTree->uSymbolTableID);

			i += sprintf(str + i, "(params: ");

			for(j = 0; j < psDefinitionData->uNumParameters; j++)
			{
				i += sprintf(str + i, "%s, ", GetSymbolName(psICProgram->psSymbolTable,
						psDefinitionData->puParameterSymbolTableIDs[j]));
			}

			i += sprintf(str + i, "ret:");

			i += sprintf(str + i, "%s)", GetSymbolName(psICProgram->psSymbolTable, 
						psDefinitionData->uReturnDataSymbolID));
		}
	}
	else
	{
		strcpy(str, NODETYPE_DESC(psASTree->eNodeType));
	}

	DumpLogMessage(LOGFILE_ICODE, iIndent, "%s: ", str);

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

	if(psASTree->psParent)
	{
		if(psASTree->psParent->uSymbolTableID)
		{
			DumpLogMessage(LOGFILE_ICODE, 0, "[parent: %s(%s)]",  
				NODETYPE_DESC(psASTree->psParent->eNodeType),
				GetSymbolName(psICProgram->psSymbolTable, psASTree->psParent->uSymbolTableID));
		}
		else
		{
			DumpLogMessage(LOGFILE_ICODE, 0, "[parent: %s]",  
				NODETYPE_DESC(psASTree->psParent->eNodeType));			
		}
	}
	else
	{
		DumpLogMessage(LOGFILE_ICODE, 0, "[parent: NULL] ");
	}

	DumpLogMessage(LOGFILE_ICODE, 0, "\n");

	for(i = 0; i < psASTree->uNumChildren; i++)
	{
		iIndent++;
		DumpASTree(psCPD, psICProgram, iIndent, psASTree->ppsChildren[i]);

		iIndent--;
	}

}

/******************************************************************************
 * Function Name: DumpICodeProgram
 *
 * Inputs       : psICContext
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Dump an intermediate program
 *****************************************************************************/
static IMG_VOID DumpICodeProgram(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICInstruction *psICInstr, *psICInstr0;
	IMG_INT32 iIndent = 0;
	IMG_BOOL bPrintOriginalLine;
	IMG_UINT32 uNumInstructions = 0;

	DumpLogMessage(LOGFILE_ICODE, 0, "\n============================ Intermediate Code  ============================\n");

	psICInstr = psICProgram->psInstrHead;
	while(psICInstr)
	{
		bPrintOriginalLine = IMG_TRUE;

		if(psICInstr->psPrev)
		{
			psICInstr0 = psICInstr->psPrev;
			if(psICInstr->pszOriginalLine == psICInstr0->pszOriginalLine) 
				bPrintOriginalLine = IMG_FALSE;
		}
				
		if(psICInstr->eOpCode == GLSLIC_OP_ELSE || 
			psICInstr->eOpCode == GLSLIC_OP_ENDIF ||
			psICInstr->eOpCode == GLSLIC_OP_ENDLOOP ||
			psICInstr->eOpCode == GLSLIC_OP_CONTDEST)
			iIndent--;

		if(iIndent < 0) 
		{
			LOG_INTERNAL_ERROR(("DumpICodeProgram: IF-ELSE_ENDIF or LOOP-CONTDEST-ENDLOOP do not match in I Code!\n"));
			iIndent = 0;
		}
		ICDumpICInstruction(LOGFILE_ICODE, psICProgram->psSymbolTable, (IMG_UINT32)iIndent, 
			psICInstr, uNumInstructions, bPrintOriginalLine, IMG_TRUE);

		if( psICInstr->eOpCode == GLSLIC_OP_IF || 
			psICInstr->eOpCode == GLSLIC_OP_IFNOT ||
			psICInstr->eOpCode == GLSLIC_OP_IFGT ||
			psICInstr->eOpCode == GLSLIC_OP_IFGE ||
			psICInstr->eOpCode == GLSLIC_OP_IFLT ||
			psICInstr->eOpCode == GLSLIC_OP_IFLE ||
			psICInstr->eOpCode == GLSLIC_OP_IFEQ ||
			psICInstr->eOpCode == GLSLIC_OP_IFNE ||
			psICInstr->eOpCode == GLSLIC_OP_ELSE ||
			psICInstr->eOpCode == GLSLIC_OP_LOOP ||
			psICInstr->eOpCode == GLSLIC_OP_STATICLOOP ||
			psICInstr->eOpCode == GLSLIC_OP_CONTDEST)
			iIndent++;

		uNumInstructions++;

		psICInstr = psICInstr->psNext;
	}

	if(iIndent)
	{
		LOG_INTERNAL_ERROR(("DumpICodeProgram: IF-ELSE_ENDIF or LOOP-CONTDEST-ENDLOOP do not match in I Code!\n"));
	}

	FlushLogFiles();
}

#endif


/*****************************************************************************
 FUNCTION	: GetSupportedExtensions

 PURPOSE	: Indiciates which extensions are supported

 PARAMETERS	: Nothing

 RETURNS	: Extentensions enum
*****************************************************************************/
static GLSLICExtensionsSupported GetSupportedExtensions(IMG_VOID)
{
	return (GLSLICExtensionsSupported)(GLSLIC_EXT_POW |
									   GLSLIC_EXT_FRC |
									   GLSLIC_EXT_MAD |
									   GLSLIC_EXT_TAN |
									   GLSLIC_EXT_NEG_MOD);
}


/******************************************************************************
 * Function Name: CreateICodeContext
 *
 * Inputs       : 
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Create an intermediate code program
 *****************************************************************************/
static GLSLICContext *CreateICodeContext(GLSLCompilerPrivateData *psCPD, GLSLTreeContext *psTreeContext)
{
	GLSLICContext *psICContext;
	IMG_INT32 i;
	IMG_UINT32 uEstimatedInstructions;

	/* ALlocate memory for the structure */
	psICContext = (GLSLICContext *)DebugMemAlloc(sizeof(GLSLICContext));

	if(psICContext == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("CreateICodeProgram: Failed to malloc memory\n"));
		return IMG_NULL;
	}

	psICContext->bMainFunction			= IMG_FALSE;
	psICContext->uTempIssusedNo			= 0;
	psICContext->iConditionLevel		= 0;
	psICContext->bHadReturn				= IMG_FALSE;
	psICContext->iForLoopLevel			= 0;
	psICContext->psArcCos2Func			= IMG_NULL;
	psICContext->psArcSin2Func			= IMG_NULL;
	psICContext->psArcTanFunc			= IMG_NULL;
	psICContext->psArcTan2Func			= IMG_NULL;
	
	for(i = 0; i < 4; i++)
	{
		psICContext->psNoiseFuncs[i]	= IMG_NULL;
	}

	uEstimatedInstructions              = psTreeContext->psParseContext->uNumTokens / 4;

	psICContext->psTreeContext	     	= psTreeContext;
	psICContext->eProgramType		    = psTreeContext->eProgramType;
	psICContext->eBuiltInsWrittenTo	    = psTreeContext->eBuiltInsWrittenTo;
	psICContext->psInstructionHeap      = DebugCreateHeap(sizeof(GLSLICInstruction), uEstimatedInstructions);
	psICContext->eDefaultFloatPrecision	= psTreeContext->eDefaultFloatPrecision;

	psICContext->psInitCompilerContext  = psTreeContext->psInitCompilerContext;

	psICContext->eBooleanPrecision		= (psICContext->eProgramType == GLSLPT_VERTEX) ?
		psTreeContext->psInitCompilerContext->sRequestedPrecisions.eVertBooleanPrecision :
		psTreeContext->psInitCompilerContext->sRequestedPrecisions.eFragBooleanPrecision;

	if(psICContext->psInstructionHeap == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GenerateICodeProgram: Failed to create heap for IC instructions \n"));
		return IMG_NULL;
	}
	
	/* get the supported extensions */
	psICContext->eExtensionsSupported	= GetSupportedExtensions();

	return psICContext;
}


/******************************************************************************
 * Function Name: GenerateICodeProgram
 *
 * Inputs       : psASTree - Root of Abstract Syntax Tree 
 *				: psSymbolTable - symbol table
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Generate intermediate program from an AST
 *****************************************************************************/
IMG_INTERNAL GLSLICProgram *GenerateICodeProgram(GLSLCompilerPrivateData *psCPD,
												 GLSLTreeContext	*psTreeContext, 
												SymTable		*psSymbolTable,
												ErrorLog        *psErrorLog)
{
	GLSLICProgram *psICProgram;

	/* Create the context */
	GLSLICContext *psICContext = CreateICodeContext(psCPD, psTreeContext);

	GLSLNode *psASTree         = psTreeContext->psAbstractSyntaxTree;

	psICProgram = (GLSLICProgram *)DebugMemAlloc(sizeof(GLSLICProgram));

	if(psICContext == IMG_NULL || psICProgram == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("CreateICodeProgram: Failed to malloc memory\n"));
		return IMG_NULL;
	}

	psICProgram->psInstrHead			= IMG_NULL;
	psICProgram->psInstrTail			= IMG_NULL;
	psICProgram->uNumDepthTextures		= 0;
	psICProgram->psErrorLog             = psErrorLog;
	psICProgram->psSymbolTable		    = psSymbolTable;
	psICProgram->pvContextData          = (IMG_VOID *)psICContext;

#ifdef DUMP_LOGFILES
	/* Dump AST tree */
	DumpLogMessage(LOGFILE_ICODE, 0, "===== Representation of Abstract Syntax Tree =====\n");
	DumpASTree(psCPD, psICProgram, 0, psASTree);
#endif


#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_ICODE, 0, "\nProcessing Abstract Syntax Tree ...\n");
#endif

	/*
	** Recursively traverse Abstract Syntax Tree and adding IC instructions to the IC program
	*/
	ICTraverseAST(psCPD, psICProgram, psASTree);

	DebugAssert(psICContext->iConditionLevel == 0);

	/* Add some function body if used */

	/* if ArcCos2 is used */
	if(psICContext->psArcCos2Func) ICAddArcSinCos2FunctionBody(psCPD, psICProgram, IMG_TRUE);

	/* if ArcSin2 is used */
	if(psICContext->psArcSin2Func) ICAddArcSinCos2FunctionBody(psCPD, psICProgram, IMG_FALSE);

	/* if ArcTan is used */
	if(psICContext->psArcTanFunc) ICAddArcTanFunctionBody(psCPD, psICProgram);

	/* if ArcTan2 is used */
	if(psICContext->psArcTan2Func) ICAddArcTan2FunctionBody(psCPD, psICProgram);

	/* Noise functions */
	if(psICContext->psNoiseFuncs[NOISE1D]) ICAddNoise1DFunctionBody(psCPD, psICProgram);
	if(psICContext->psNoiseFuncs[NOISE2D]) ICAddNoise2DFunctionBody(psCPD, psICProgram);
	if(psICContext->psNoiseFuncs[NOISE3D]) ICAddNoise3DFunctionBody(psCPD, psICProgram);
	if(psICContext->psNoiseFuncs[NOISE4D]) ICAddNoise4DFunctionBody(psCPD, psICProgram);

#ifdef DUMP_LOGFILES
	DumpICodeProgram(psCPD, psICProgram);
#endif

	return psICProgram;	
}

/******************************************************************************
 * Function Name: DestroyICodeProgram
 *
 * Inputs       : psICContext
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Destroy intermediate program
 *****************************************************************************/
static IMG_VOID DestroyICodeContext(GLSLICContext *psICContext)
{
	IMG_UINT32			i;

	/* Free all function definition structure */
	if(psICContext->psArcCos2Func) 
		DebugMemFree(psICContext->psArcCos2Func);
	
	if(psICContext->psArcSin2Func) 
		DebugMemFree(psICContext->psArcSin2Func);

	if(psICContext->psArcTanFunc) 
		DebugMemFree(psICContext->psArcTanFunc);

	if(psICContext->psArcTan2Func) 
		DebugMemFree(psICContext->psArcTan2Func);

	for(i = 0; i < 4; i++)
	{
		if(psICContext->psNoiseFuncs[i])
			DebugMemFree(psICContext->psNoiseFuncs[i]);
	}

	DebugDestroyHeap(psICContext->psInstructionHeap);

	/* Free program */
	DebugMemFree(psICContext);
}

/******************************************************************************
 * Function Name: DestroyICodeProgram
 *
 * Inputs       : psICContext
 * Outputs      : 
 * Returns      : 
 * Globals Used : -
 *
 * Description  : Destroy intermediate program
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyICodeProgram(GLSLCompilerPrivateData *psCPD, GLSLICProgram *psICProgram)
{
	GLSLICContext *psICContext = GET_IC_CONTEXTDATA(psICProgram);

	PVR_UNREFERENCED_PARAMETER(psCPD);

	/* Free offset array attached to each instruction */
	ICRemoveInstructionRange(psICProgram, psICProgram->psInstrHead, psICProgram->psInstrTail);

	DebugAssert(psICProgram->psInstrHead == IMG_NULL);
	DebugAssert(psICProgram->psInstrTail == IMG_NULL);

	DestroyICodeContext(psICContext);

	/* Free program */
	DebugMemFree(psICProgram);
}


IMG_INTERNAL IMG_UINT32 GetICInstructionCount(GLSLICProgram *psICProgram)
{
	return GetInstructionsCount(psICProgram->psInstrHead, psICProgram->psInstrTail);
}
