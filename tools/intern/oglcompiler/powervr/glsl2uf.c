/******************************************************************************
 * Name         : glsl2uf.c
 * Author       : James McCarthy
 * Created      : 16/02/2006
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any for
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: glsl2uf.c $
 *****************************************************************************/

#include <string.h>

#include "error.h"
#include "glsl2uf.h"
#include "icgen.h"
#include "icodefns.h"
#include "debug.h"
#include "metrics.h"
#include "ic2uf.h"


/******************************************************************************
 * Function Name: GLSLDestroyCompiledUniflexProgram
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_VOID GLSLDestroyCompiledUniflexProgram(GLSLInitCompilerContext *psInitCompilerContext,
												  GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram,
												  IMG_BOOL                    bFreeUFInput,
												  IMG_BOOL                    bFreeUFOutput,
												  IMG_BOOL                    bFreeBindingSymbolList)
{
	GLSLUniFlexCode	   *psUniFlexCode = psGLSLCompiledUniflexProgram->psUniFlexCode;
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;
	IMG_VOID *pvUniFlexContext = psCPD->pvUniFlexContext;

	/* Destroy the requested parts of the uniflex code */
	DestroyUniFlexCode(psCPD, pvUniFlexContext, psUniFlexCode, bFreeUFInput, bFreeUFOutput);

	if (bFreeBindingSymbolList)
	{
		/* Destroy the binding symbol table */
		if (psGLSLCompiledUniflexProgram->psBindingSymbolList)
		{
			DestroyBindingSymbolList(psGLSLCompiledUniflexProgram->psBindingSymbolList);
		}
	}

	/* Free the structs */
	if (bFreeUFInput && bFreeUFOutput && bFreeBindingSymbolList)
	{
		psGLSLCompiledUniflexProgram->psUniFlexCode = IMG_NULL;
	}
}

/******************************************************************************
 * Function Name: GLSLFreeCompiledUniflexProgram
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
GLSL_EXPORT IMG_VOID IMG_CALLCONV GLSLFreeCompiledUniflexProgram(GLSLInitCompilerContext *psInitCompilerContext,
													 GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram)
{
	if (psGLSLCompiledUniflexProgram)
	{
		GLSLDestroyCompiledUniflexProgram(psInitCompilerContext, 
											psGLSLCompiledUniflexProgram, 
											IMG_TRUE, 
											IMG_TRUE, 
											IMG_TRUE);

		GLSLFreeInfoLog(&(psGLSLCompiledUniflexProgram->sInfoLog));

		DebugMemFree(psGLSLCompiledUniflexProgram);
	}
}


/******************************************************************************
 * Function Name: GLSLGenerateUniflexProgram
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      :
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
static IMG_BOOL GLSLGenerateUniflexProgram(GLSLCompileUniflexProgramContext *psCompileUniflexProgramContext,
										   GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram,
										   GLSLICProgram              *psICProgram,
										   GLSLUniFlexHWCodeInfo      *psUniFlexHWCodeInfo)
{
	GLSLInitCompilerContext *psInitCompilerContext = psCompileUniflexProgramContext->psCompileProgramContext->psInitCompilerContext;

	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	psGLSLCompiledUniflexProgram->psCompileUniflexProgramContext = psCompileUniflexProgramContext;

	if ((psGLSLCompiledUniflexProgram->eProgramFlags == GLSLPF_UNIFLEX_OUTPUT) ||
		(psGLSLCompiledUniflexProgram->eProgramFlags == GLSLPF_UNIPATCH_INPUT))
	{
		/* We need psUniFlexHWCodeInfo to pass it to USC */
		if (!psUniFlexHWCodeInfo)
		{
			LOG_INTERNAL_ERROR(("GLSLToUniflex: No uniflex hw code info supplied\n"));
			GLSLDestroyCompiledUniflexProgram(psInitCompilerContext, 
												psGLSLCompiledUniflexProgram,
												IMG_TRUE, 
												IMG_TRUE, 
												IMG_TRUE);
			return IMG_FALSE;
		}

#if !defined(GEN_HW_CODE)

		PVR_UNREFERENCED_PARAMETER(psUniFlexHWCodeInfo);

		LOG_INTERNAL_ERROR(("GLSLToUniflex: Cannot generate uniflex output code, build compiler with GEN_HW_CODE=1\n"));
		GLSLDestroyCompiledUniflexProgram(psInitCompilerContext, 
											psGLSLCompiledUniflexProgram,
											IMG_TRUE, 
											IMG_TRUE, 
											IMG_TRUE);
		return IMG_FALSE;
#endif

	}

	/* Start timing */
	MetricStart((IMG_VOID*)psCPD, METRICS_UNIFLEXINPUTGEN);

	/* Generate the uniflex output code */
	psGLSLCompiledUniflexProgram->psUniFlexCode = GenerateUniFlexInput(psCPD, psICProgram, &psGLSLCompiledUniflexProgram->psBindingSymbolList);

	MetricFinish((IMG_VOID*)psCPD, METRICS_UNIFLEXINPUTGEN);

	if(!psGLSLCompiledUniflexProgram->psUniFlexCode)
	{
		LOG_INTERNAL_ERROR(("GLSLToUniflex: Failed to generate uniflex input code\n"));
		GLSLDestroyCompiledUniflexProgram(psInitCompilerContext, 
											psGLSLCompiledUniflexProgram,
											IMG_TRUE, 
											IMG_TRUE, 
											IMG_TRUE);
		return IMG_FALSE;
	}

	if(psGLSLCompiledUniflexProgram->psBindingSymbolList == IMG_NULL)
	{
		LOG_INTERNAL_ERROR(("GLSLToUniflex: Failed to generate binding symbol list \n"));
		GLSLDestroyCompiledUniflexProgram(psInitCompilerContext,
											psGLSLCompiledUniflexProgram,
											IMG_TRUE,
											IMG_TRUE,
											IMG_TRUE);
		return IMG_FALSE;
	}


	/* If we're not generating output code return the input code now */
	if (psGLSLCompiledUniflexProgram->eProgramFlags & GLSLPF_UNIFLEX_INPUT)
	{
		return IMG_TRUE;
	}
#ifdef GEN_HW_CODE

	/* Start timing */
	MetricStart((IMG_VOID*)psCPD, METRICS_UNIFLEXOUTPUTGEN);

#if defined(OUTPUT_USPBIN)
	/* Generate the output code */
	if (!GenerateUniPatchInput(psCPD,
							   psGLSLCompiledUniflexProgram->psUniFlexCode,
							   psCPD->pvUniFlexContext,
							   psGLSLCompiledUniflexProgram->psBindingSymbolList->pfConstantData,
							   psGLSLCompiledUniflexProgram->eProgramType,
							   &psGLSLCompiledUniflexProgram->eProgramFlags,
							   psCompileUniflexProgramContext->bCompileMSAATrans,
							   psUniFlexHWCodeInfo))
	{
		LOG_INTERNAL_ERROR(("GLSLToUniflex: Failed to generate unipatch input code\n"));
		return IMG_FALSE;
	}
#else
	/* Generate the output code */
	if (!GenerateUniFlexOutput(psCPD, psGLSLCompiledUniflexProgram->psUniFlexCode,
							   psCPD->pvUniFlexContext,
							   psGLSLCompiledUniflexProgram->psBindingSymbolList->pfConstantData,
							   psGLSLCompiledUniflexProgram->eProgramType,
							   psUniFlexHWCodeInfo))
	{
		LOG_INTERNAL_ERROR(("GLSLToUniflex: Failed to generate uniflex HW code\n"));
		return IMG_FALSE;
	}
#endif

	MetricFinish((IMG_VOID*)psCPD, METRICS_UNIFLEXOUTPUTGEN);

	/* Free any of the input data as it's no longer required */
	GLSLDestroyCompiledUniflexProgram(psInitCompilerContext,
										psGLSLCompiledUniflexProgram,
										IMG_TRUE,
										IMG_FALSE,
										IMG_FALSE);

#endif

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GLSLCompileToUniflex
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      :
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
GLSL_EXPORT GLSLCompiledUniflexProgram * IMG_CALLCONV GLSLCompileToUniflex(GLSLCompileUniflexProgramContext *psCompileUniflexProgramContext)
{

	GLSLCompileProgramContext *psCompileProgramContext = psCompileUniflexProgramContext->psCompileProgramContext;

	GLSLCompiledUniflexProgram *psGLSLCompiledUniflexProgram;

	GLSLICProgram   *psICProgram   = IMG_NULL;

	ErrorLog         sErrorLog;

#if defined(DEBUG)
	ErrorType        eErrorsToReport = (ErrorType)(ERRORTYPE_PROGRAM_ERROR | ERRORTYPE_PROGRAM_WARNING | ERRORTYPE_INTERNAL);
#else
	ErrorType        eErrorsToReport = (ErrorType)(ERRORTYPE_PROGRAM_ERROR | ERRORTYPE_PROGRAM_WARNING);
#endif

	IMG_BOOL bSuccess;
	IMG_CHAR *pszCurrentLocale;
	IMG_CHAR *pszTemp;

	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psCompileProgramContext->psInitCompilerContext->pvCompilerPrivateData;

	SetErrorLog(&sErrorLog, IMG_FALSE);
	psCPD->psErrorLog = &sErrorLog;

	/* Alloc mem for compiled uniflex program */
	psGLSLCompiledUniflexProgram = DebugMemCalloc(sizeof(GLSLCompiledUniflexProgram));

	if (!psGLSLCompiledUniflexProgram)
	{
		LOG_INTERNAL_ERROR(("GLSLCompileUniflexProgram: Failed to alloc mem for GLSLCompiledUniflexProgram \n"));
		return IMG_NULL;
	}

	psGLSLCompiledUniflexProgram->eProgramType          = psCompileProgramContext->eProgramType;
	psGLSLCompiledUniflexProgram->eProgramFlags         = psCompileUniflexProgramContext->eOutputCodeType;

	/* Compiler to intermediate code */
	bSuccess = GLSLCompileToIntermediateCode(psCompileProgramContext,
											 &psICProgram,
											 &psGLSLCompiledUniflexProgram->eProgramFlags,
											 psCPD->psErrorLog);


	if (psCompileProgramContext->bValidateOnly || !bSuccess)
	{
		goto UniflexCleanUp;
	}

	/* Generate the uniflex code */
	bSuccess = GLSLGenerateUniflexProgram(psCompileUniflexProgramContext,
										  psGLSLCompiledUniflexProgram,
										  psICProgram,
										  psCompileUniflexProgramContext->psUniflexHWCodeInfo);

	/* Double-check that if any errors have been generated then we report a failure */
	if(sErrorLog.uNumInternalErrorMessages != 0 ||
	   sErrorLog.uNumProgramErrorMessages != 0)
	{
		bSuccess = IMG_FALSE;
	}


UniflexCleanUp:

	/* Free the intermediate code */
	GLSLFreeIntermediateCode(psCompileProgramContext, psICProgram);

#ifdef COMPACT_MEMORY_MODEL
	GLSLFreeBuiltInState(psCompileProgramContext->psInitCompilerContext);
#endif

	/* Generate the info log */
	GLSLGenerateInfoLog(&psGLSLCompiledUniflexProgram->sInfoLog,
						&sErrorLog,
						eErrorsToReport,
						bSuccess);

	/* Free any mem allocated for the error messages */
	FreeErrorLogMessages(&sErrorLog);

	psCPD->psErrorLog = IMG_NULL;

	/* Indicate success */
	psGLSLCompiledUniflexProgram->bSuccessfullyCompiled = bSuccess;

	return psGLSLCompiledUniflexProgram;
}

/******************************************************************************
 * Function Name: GLSLDisplayMetrics
 *
 * Inputs       : psInitCompilerContext
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
GLSL_EXPORT IMG_VOID IMG_CALLCONV GLSLDisplayMetrics(GLSLInitCompilerContext *psInitCompilerContext)
{
#if defined(DEBUG) || defined(DUMP_LOGFILES)
	PVR_UNREFERENCED_PARAMETER(psInitCompilerContext);
	DEBUG_MESSAGE(("\n-- Source Info (DEBUG Build) --\n"));
	DEBUG_MESSAGE(("-- Compiler built with debugging information.\n"));
	DEBUG_MESSAGE(("-- Timing metrics are meaningless so they will not be shown --\n\n"));

	DebugDisplayMemoryStats();
#else

#ifdef METRICS
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;
	IMG_UINT32 uNumTokens                   = MetricsGetCounter(psCPD, METRICCOUNT_NUMTOKENS);
	IMG_UINT32 uNumParseTreeBranches        = MetricsGetCounter(psCPD, METRICCOUNT_NUMPARSETREEBRANCHES);
	IMG_UINT32 uNumParseTreeBranchesCreated = MetricsGetCounter(psCPD, METRICCOUNT_NUMPARSETREEBRANCHESCREATED);
	IMG_UINT32 uMaxNumParseTreeBranches     = MetricsGetCounter(psCPD, METRICCOUNT_MAXNUMPARSETREEBRANCHES);
	IMG_UINT32 uNumICInstructions           = MetricsGetCounter(psCPD, METRICCOUNT_NUMICINSTRUCTIONS);
	IMG_UINT32 uNumCompilations             = MetricsGetCounter(psCPD, METRICCOUNT_NUMCOMPILATIONS);
	IMG_FLOAT  fTotalTimeScale; 

	fTotalTimeScale = (IMG_FLOAT)(
		MetricsGetTime(psCPD, METRICS_PARSECONTEXT)    + MetricsGetTime(psCPD, METRICS_AST) + 
		MetricsGetTime(psCPD, METRICS_ICODE)           + MetricsGetTime(psCPD, METRICS_UNIFLEXINPUTGEN) + 
		MetricsGetTime(psCPD, METRICS_UNIFLEXOUTPUTGEN) +
		MetricsGetTime(psCPD, METRICS_SHUTDOWN) );

	/* stop div by 0 */
	if (fTotalTimeScale == 0.0f)
	{
		fTotalTimeScale = 1.0f;
	}

	DEBUG_MESSAGE(("\n-- Source Info (RELEASE/TIMING Build)  --\n"));

	DEBUG_MESSAGE(("Num Tokens                        = %8d\n", uNumTokens));
	DEBUG_MESSAGE(("Num IC Instructions               = %8d\n", uNumICInstructions));
	DEBUG_MESSAGE(("Num Parse tree branches           = %8d (total = %d, max = %d)\n", uNumParseTreeBranches, uNumParseTreeBranchesCreated, uMaxNumParseTreeBranches));

	DEBUG_MESSAGE(("-- Compiler Metrics --\n"));
	DEBUG_MESSAGE(("Number of shaders compiled        = %7d\n\n", uNumCompilations));

#define METRICS_MSG_ARGS(e) 1000*MetricsGetTime(psCPD,e)/uNumCompilations, 1000*MetricsGetTime(psCPD,e), 100*MetricsGetTime(psCPD,e) / fTotalTimeScale

	if(uNumCompilations)
	{
		DEBUG_MESSAGE(("          Operation               | ms/shader | Total ms | percent\n"));
		DEBUG_MESSAGE(("----------------------------------+-----------+----------+--------\n"));
		DEBUG_MESSAGE(("Parse context creation            |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_PARSECONTEXT)));
		DEBUG_MESSAGE(("( Token list creation             |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_TOKENLIST)));
		DEBUG_MESSAGE(("(   - Preprocessor                |   %7.3f |  %7.3f |       %6.2f)\n", METRICS_MSG_ARGS(METRICS_PREPROCESSOR)));
		DEBUG_MESSAGE(("( Tree creation                   |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_PARSETREE)));
		DEBUG_MESSAGE(("Abstract Syntax tree creation     |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_AST)));
		DEBUG_MESSAGE(("Intermediate code generation      |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_ICODE)));
		DEBUG_MESSAGE(("Uniflex input code generation     |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_UNIFLEXINPUTGEN)));
		DEBUG_MESSAGE(("Uniflex output code generation    |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_UNIFLEXOUTPUTGEN)));
		DEBUG_MESSAGE(("( UF intermediate code generation |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_INTERMEDIATE_CODE_GENERATION)));
		DEBUG_MESSAGE(("( Value numbering                 |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_VALUE_NUMBERING)));
		DEBUG_MESSAGE(("( Assign index registers          |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(USC_METRICS_ASSIGN_INDEX_REGISTERS)));
		DEBUG_MESSAGE(("( Flatten cond. & eliminate moves |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES)));
		DEBUG_MESSAGE(("( Integer optimizations           |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_INTEGER_OPTIMIZATIONS)));
		DEBUG_MESSAGE(("( Merge identical instructions    |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_MERGE_IDENTICAL_INSTRUCTIONS)));
		DEBUG_MESSAGE(("( Instruction selection           |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_INSTRUCTION_SELECTION)));
		DEBUG_MESSAGE(("( Register allocation             |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_REGISTER_ALLOCATION)));
		DEBUG_MESSAGE(("( C10 register allocation         |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_C10_REGISTER_ALLOCATION)));
		DEBUG_MESSAGE(("( Finalise shader                 |   %7.3f |  %7.3f |    %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_FINALISE_SHADER)));
		DEBUG_MESSAGE(("( * Custom timer A                |   %7.3f |  %7.3f |  * %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_CUSTOM_TIMER_A)));
		DEBUG_MESSAGE(("( * Custom timer B                |   %7.3f |  %7.3f |  * %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_CUSTOM_TIMER_B)));
		DEBUG_MESSAGE(("( * Custom timer C                |   %7.3f |  %7.3f |  * %6.2f)\n",    METRICS_MSG_ARGS(METRICS_USC_CUSTOM_TIMER_C)));
		DEBUG_MESSAGE(("Shutting down compiler            |   %7.3f |  %7.3f | %6.2f\n",        METRICS_MSG_ARGS(METRICS_SHUTDOWN)));
		DEBUG_MESSAGE(("----------------------------------+-----------+----------+--------\n"));
		DEBUG_MESSAGE(("Total compilation time                        |  %7.3f\n", 1000*fTotalTimeScale));
	}

#undef METRICS_MESSAGE_ARGS

	DebugDisplayMemoryStats();
#else
	PVR_UNREFERENCED_PARAMETER(psInitCompilerContext);
	DEBUG_MESSAGE(("\nMetrics not enabled, build compiler with METRICS=1\n"));
#endif
#endif
}

/******************************************************************************
 End of file (glsl2uf.c)
******************************************************************************/
