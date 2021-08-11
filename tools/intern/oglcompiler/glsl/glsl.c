/******************************************************************************
 * Name         : glsl.c
 * Author       : James McCarthy
 * Created      : 29/07/2004
 *
 * Copyright    : 2000-2007 by Imagination Technologies Limited.
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
 * $Log: glsl.c $
 *****************************************************************************/

#include "glsl.h"
#include "error.h"
#include "astbuiltin.h"
#include "prepro.h"
#include "debug.h"
#include "metrics.h"
#include "ic2uf.h"
#include "icode.h"
#include "icodefns.h"
#include <string.h>
#include <stdarg.h>
#include <time.h>


#ifdef GEN_HW_CODE

static IMG_VOID * IMG_CALLCONV UniFlexAllocTrack(IMG_UINT32 uSize)
{
	return DebugMemAlloc(uSize);
}

static IMG_VOID IMG_CALLCONV UniFlexFreeTrack(IMG_VOID * pvData)
{
	DebugMemFree(pvData);
}

static IMG_VOID * IMG_CALLCONV UniFlexAlloc(IMG_UINT32 uSize)
{
	return DebugMemAllocNoTrack(uSize);
}

static IMG_VOID IMG_CALLCONV UniFlexFree(IMG_VOID * pvData)
{
	DebugMemFreeNoTrack(pvData);
}

static IMG_VOID UniFlexPrint(const IMG_CHAR* pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);
static IMG_VOID UniFlexPrint(const IMG_CHAR* pszFmt, ...)
{
	IMG_CHAR pszTemp[256];
	va_list ap;

	va_start(ap, pszFmt);
	vsprintf(pszTemp, pszFmt, ap);
	strcat(pszTemp, "\n");

#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_COMPILER, 0, pszTemp);
#endif

	va_end(ap);
}

#endif /* GEN_HW_CODE */

/******************************************************************************
 * Function Name: GLSLAddBuiltInVertexProgramState
 * Inputs       : psInitCompilerContext, psTokenList
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds builtin symbols to the symbol table of a vertex shader.
 *****************************************************************************/
static IMG_BOOL GLSLAddBuiltInVertexProgramState(GLSLInitCompilerContext *psInitCompilerContext, 
												 Token *psTokenList)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	SymTable *psVertexSymbolTable = CreateSymTable(psCPD->psSymbolTableContext,
												   "Built-in vertex state",
												   200, 
												   GLSL_NUM_BITS_FOR_SYMBOL_IDS, 
												   IMG_NULL);

	if (!ASTBIAddBuiltInData(psCPD, psVertexSymbolTable, 
							 psTokenList, 
							 GLSLPT_VERTEX,
							 GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS,
							 &(psInitCompilerContext->sRequestedPrecisions),
							 &(psInitCompilerContext->sCompilerResources)))
	{
		LOG_INTERNAL_ERROR(("GLSLInitCompiler: Failed to generate vertex built in functions/state\n"));

		return IMG_FALSE;
	}

	psCPD->psVertexSymbolTable = psVertexSymbolTable;

	psCPD->sVertexBuiltInsReferenced.uNumIdentifiersReferenced = 0;

	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GLSLAddBuiltInFragmentProgramState
 * Inputs       : psInitCompilerContext, psTokenList
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds builtin symbols to the symbol table of a fragment shader.
 *****************************************************************************/
static IMG_BOOL GLSLAddBuiltInFragmentProgramState(GLSLInitCompilerContext *psInitCompilerContext, 
												   Token *psTokenList)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	SymTable *psFragmentSymbolTable = CreateSymTable(psCPD->psSymbolTableContext,
													 "Built-in fragment state",
													 200, 
													 GLSL_NUM_BITS_FOR_SYMBOL_IDS, 
													 IMG_NULL);
	
	if (!ASTBIAddBuiltInData(psCPD, psFragmentSymbolTable, 
							 psTokenList, 
							 GLSLPT_FRAGMENT,
							 (GLSLExtendFunctionality)(GLSL_EXTENDFUNC_ALLOW_TEXTURE_LOD_IN_FRAGMENT |
							 GLSL_EXTENDFUNC_SUPPORT_TEXTURE_STREAM | GLSL_EXTENDFUNC_SUPPORT_TEXTURE_EXTERNAL | 
							  GLSL_EXTENDFUNC_SUPPORT_TEXTURE_GRAD_FUNCTIONS), 
							 &(psInitCompilerContext->sRequestedPrecisions),
							 &(psInitCompilerContext->sCompilerResources)))
	{
		LOG_INTERNAL_ERROR(("GLSLInitCompiler: Failed to generate fragment built in functions/state\n"));

		return IMG_FALSE;
	}
	
	psCPD->psFragmentSymbolTable = psFragmentSymbolTable;

	psCPD->sFragmentBuiltInsReferenced.uNumIdentifiersReferenced = 0;


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GLSLAddBuiltInState
 * Inputs       : psInitCompilerContext, psTokenList, eProgramType
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Adds the builtin symbols to the shader.
 *****************************************************************************/
static IMG_BOOL GLSLAddBuiltInState(GLSLInitCompilerContext *psInitCompilerContext,
									Token                   *psTokenList,
									GLSLProgramType          eProgramType)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		if (psCPD->psFragmentSymbolTable)
		{
			/* Reset all data associated with the built in state and functions */
			return ASTBIResetBuiltInData(psCPD, psCPD->psFragmentSymbolTable, 
										 &psCPD->sFragmentBuiltInsReferenced);
		}
		else
		{
			return GLSLAddBuiltInFragmentProgramState(psInitCompilerContext, psTokenList);
		}
	}
	else
	{
		if (psCPD->psVertexSymbolTable)
		{
			/* Reset all data associated with the built in state and functions */
			return ASTBIResetBuiltInData(psCPD, psCPD->psVertexSymbolTable,
										 &psCPD->sVertexBuiltInsReferenced);
		}
		else
		{
			return GLSLAddBuiltInVertexProgramState(psInitCompilerContext, psTokenList);
		}
	}
}

/******************************************************************************
 * Function Name: GLSLFreeBuiltInState
 * Inputs       : psInitCompilerContext
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Frees builtin symbols from the symbol table of a shader.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IMG_CALLCONV GLSLFreeBuiltInState(GLSLInitCompilerContext *psInitCompilerContext)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	if (psCPD->psFragmentSymbolTable)
	{
		RemoveSymbolTableFromManager(psCPD->psSymbolTableContext, psCPD->psFragmentSymbolTable);
		DestroySymTable(psCPD->psFragmentSymbolTable);
		psCPD->psFragmentSymbolTable = IMG_NULL;
	}

	if (psCPD->psVertexSymbolTable)
	{
		RemoveSymbolTableFromManager(psCPD->psSymbolTableContext, psCPD->psVertexSymbolTable);
		DestroySymTable(psCPD->psVertexSymbolTable);
		psCPD->psVertexSymbolTable = IMG_NULL;
	}

	return IMG_TRUE;
}


#if defined(GEN_HW_CODE) && defined(METRICS)
/******************************************************************************
 * Function Name: GLSLUFStartMetric
 * Inputs       : pvCompilerPrivateData, eUFMetric
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Callback from Uniflex to update the metrics.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV GLSLUFStartMetric(IMG_VOID *pvCompilerPrivateData, USC_METRICS eUFMetric)
{
	MetricStart(pvCompilerPrivateData, METRICS_USC_BASE + eUFMetric);
}

/******************************************************************************
 * Function Name: GLSLUFStopMetric
 * Inputs       : pvCompilerPrivateData, eUFMetric
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Callback from Uniflex to update the metrics.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV GLSLUFFinishMetric(IMG_VOID *pvCompilerPrivateData, USC_METRICS eUFMetric)
{
	MetricFinish(pvCompilerPrivateData, METRICS_USC_BASE + eUFMetric);
}
#endif /* defined(GEN_HW_CODE) && defined(METRICS) */

/******************************************************************************
 * Function Name: GLSLInitCompiler
 * Inputs       : psInitCompilerContext
 * Outputs      : psInitCompilerContext
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Initializes the compiler. This is the first function that must be called.
 *****************************************************************************/
GLSL_EXPORT IMG_BOOL IMG_CALLCONV GLSLInitCompiler(GLSLInitCompilerContext *psInitCompilerContext)
{
	GLSLCompilerPrivateData *psCPD = IMG_NULL;

	ErrorLog sInitErrorLog;

	psInitCompilerContext->bSuccessfulInit = IMG_FALSE;

	SetErrorLog(&sInitErrorLog, IMG_FALSE);

	psCPD = DebugMemAlloc(sizeof(GLSLCompilerPrivateData));

	if (!psCPD)
	{
		DEBUG_MESSAGE(("GLSLInitCompiler: Failed to create memory for private data\n"));
		return IMG_FALSE;
	}

	psCPD->psErrorLog = &sInitErrorLog;

#ifdef DUMP_LOGFILES
	if(psInitCompilerContext->eLogFiles == GLSLLF_LOG_ALL)
	{
		DEBUG_MESSAGE(("-- Enabling Log file generation --\n"));
		
		CreateLogFile(LOGFILE_PARSETREE,        "parsetree.txt");
		CreateLogFile(LOGFILE_TOKENS_GENERATED, "tokens.txt");
		CreateLogFile(LOGFILE_TOKENISING_LOG,   "tokenisinglog.txt");
		/* This log file is commented because it tends to grow huge (up to hundreds of MB) */
		//CreateLogFile(LOGFILE_PARSING_LOG,      "parsinglog.txt");
		CreateLogFile(LOGFILE_SYNTAX_GRAMMAR,   "syntaxgrammar.txt");
		CreateLogFile(LOGFILE_LEXICAL_RULES,    "lexrules.txt");
		CreateLogFile(LOGFILE_SYNTAXSYMBOLS,    "syntaxsymbols.txt");
		CreateLogFile(LOGFILE_SYMBOLTABLE,      "symboltable.txt");
		CreateLogFile(LOGFILE_COMPILER,         "compiler.txt");
		CreateLogFile(LOGFILE_ASTREE,           "astree.txt");
		CreateLogFile(LOGFILE_ICODE,			"icode.txt");
		CreateLogFile(LOGFILE_SYMTABLE_DATA,	"symtabledata.txt");
		CreateLogFile(LOGFILE_BINDINGSYMBOLS,	"bindingsym.txt");
		CreateLogFile(LOGFILE_PREPROCESSOR,     "preprocessor.txt");
		CreateLogFile(LOGFILE_MEMORY_STATS,     "memstats.txt");

	}
	else if(psInitCompilerContext->eLogFiles == GLSLLF_LOG_ICODE_ONLY)
	{
		CreateLogFile(LOGFILE_ICODE,			"icode.txt");
	}
#endif
	
	/* Set all fields to 0 */
	memset(psCPD, 0, sizeof(GLSLCompilerPrivateData));

	/* Store pointer to private data */
	psInitCompilerContext->pvCompilerPrivateData = (IMG_VOID *)psCPD;

	/* Init the symbol table manager */
	psCPD->psSymbolTableContext = InitSymbolTableManager();

	if (!psCPD->psSymbolTableContext)
	{
		LOG_INTERNAL_ERROR(("GLSLInitCompiler: Failed to initialise symbol table manager\n"));
		return IMG_FALSE;
	}

	/* Reset the stats that keep track of the used built in state */
	psCPD->sVertexBuiltInsReferenced.puIdentifiersReferenced          = IMG_NULL;
	psCPD->sVertexBuiltInsReferenced.uNumIdentifiersReferenced        = 0;
	psCPD->sVertexBuiltInsReferenced.uIdentifiersReferencedListSize   = 0;
	psCPD->sFragmentBuiltInsReferenced.puIdentifiersReferenced        = IMG_NULL;
	psCPD->sFragmentBuiltInsReferenced.uNumIdentifiersReferenced      = 0;
	psCPD->sFragmentBuiltInsReferenced.uIdentifiersReferencedListSize = 0;
	psCPD->psCompilerResources = &psInitCompilerContext->sCompilerResources;

#if !defined(COMPACT_MEMORY_MODEL)
	if (!GLSLAddBuiltInState(psInitCompilerContext, IMG_NULL, GLSLPT_VERTEX)|| 
		!GLSLAddBuiltInState(psInitCompilerContext, IMG_NULL, GLSLPT_FRAGMENT))
	{
#ifdef STANDALONE
		DisplayErrorMessages(&sInitErrorLog, ERRORTYPE_ALL);
#endif
		return IMG_FALSE;
	}
#endif
	
#ifdef GEN_HW_CODE
	psCPD->pvUniFlexContext = PVRUniFlexCreateContext(psInitCompilerContext->bEnableUSCMemoryTracking ? UniFlexAllocTrack : UniFlexAlloc, 
														psInitCompilerContext->bEnableUSCMemoryTracking ? UniFlexFreeTrack : UniFlexFree,
#if defined(DEBUG)
													  psInitCompilerContext->pfnPrintShaders ? psInitCompilerContext->pfnPrintShaders : UniFlexPrint,
#else
													  UniFlexPrint,
#endif
													  IMG_NULL, /* Pdump */
													  IMG_NULL, /* Pdump */
#ifdef METRICS
													  GLSLUFStartMetric, GLSLUFFinishMetric, (IMG_VOID*)psCPD
#else
													  IMG_NULL, IMG_NULL, IMG_NULL
#endif
													  );
#endif

#ifdef STANDALONE
	DisplayErrorMessages(&sInitErrorLog, ERRORTYPE_ALL);
#endif

	/* Free any mem allocated for the error messages */
	FreeErrorLogMessages(&sInitErrorLog);

	/* Initialize the performance counters */
	MetricsInit(psInitCompilerContext->pvCompilerPrivateData);

	psInitCompilerContext->bSuccessfulInit = IMG_TRUE;


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GLSLShutDownCompiler
 * Inputs       : psInitCompilerContext
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Destroys the compiler. After this function is called the compiler
                  context is invalid.
 *****************************************************************************/
GLSL_EXPORT IMG_BOOL IMG_CALLCONV GLSLShutDownCompiler(GLSLInitCompilerContext *psInitCompilerContext)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

#ifdef GEN_HW_CODE
	if (psCPD->pvUniFlexContext)
	{
		PVRUniFlexDestroyContext(psCPD->pvUniFlexContext);
	}
#endif
	
#if defined(STANDALONE) && !defined(COMPACT_MEMORY_MODEL)
	/* 
	   Reset all data associated with the built in state and functions 
	   Not required since we're shutting down but will allow us to 
	   test this functionality in standalone builds.
	*/
	ASTBIResetBuiltInData(psCPD, psCPD->psFragmentSymbolTable,
						  &(psCPD->sFragmentBuiltInsReferenced));

	ASTBIResetBuiltInData(psCPD, psCPD->psVertexSymbolTable,
						  &(psCPD->sVertexBuiltInsReferenced));
#endif

	/* Free the built ins referenced lists */
	DebugMemFree(psCPD->sVertexBuiltInsReferenced.puIdentifiersReferenced);
	DebugMemFree(psCPD->sFragmentBuiltInsReferenced.puIdentifiersReferenced);

	GLSLFreeBuiltInState(psInitCompilerContext);

	if (psCPD->psSymbolTableContext)
	{
		DestroySymbolTableManager(psCPD->psSymbolTableContext);
		psCPD->psSymbolTableContext = IMG_NULL;
	}

	/* free the private data */
	DebugMemFree(psCPD);

#ifdef DUMP_LOGFILES
	DebugDumpMemoryAllocLog();
#endif

#ifdef DUMP_LOGFILES
	if (psInitCompilerContext->eLogFiles)
	{
		CloseLogFile(LOGFILE_TOKENS_GENERATED);
		CloseLogFile(LOGFILE_TOKENISING_LOG);
		CloseLogFile(LOGFILE_PARSING_LOG);
		CloseLogFile(LOGFILE_SYNTAX_GRAMMAR);
		CloseLogFile(LOGFILE_LEXICAL_RULES);
		CloseLogFile(LOGFILE_COMPILER);
		CloseLogFile(LOGFILE_SYNTAXSYMBOLS);
		CloseLogFile(LOGFILE_SYMBOLTABLE);
		CloseLogFile(LOGFILE_ASTREE);
		CloseLogFile(LOGFILE_ICODE);
		CloseLogFile(LOGFILE_SYMTABLE_DATA);
		CloseLogFile(LOGFILE_BINDINGSYMBOLS);
		CloseLogFile(LOGFILE_PREPROCESSOR);
		CloseLogFile(LOGFILE_MEMORY_STATS);
	}
#endif


	return IMG_TRUE;
}

/******************************************************************************
 * Function Name: GLSLGenerateInfoLog 
 * Inputs       : psInfoLog, psErrorLog, eErrorTypes, bSuccess
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Creates an error log (usually after compilation). It includes
                  compilation errors, warnings and/or internal errors.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV GLSLGenerateInfoLog( GLSLInfoLog *psInfoLog,
														ErrorLog    *psErrorLog,
														ErrorType    eErrorTypes,
														IMG_BOOL     bSuccess)
{
	IMG_UINT32 i, j, uTotalStringSize = 0;

#ifdef DEBUG
	if (psErrorLog->uNumInternalErrorMessages)
	{
		PrintInternalErrorFunc("%d internal errors.\n\n", psErrorLog->uNumInternalErrorMessages);
	}
#endif

	if (psErrorLog->uNumProgramWarningMessages)
	{
		LogProgramWarning(psErrorLog, "%d compilation warnings.\n\n", psErrorLog->uNumProgramWarningMessages);
	}
	
	if (psErrorLog->uNumProgramErrorMessages)
	{
		LogProgramError(psErrorLog, "%d compilation errors. No code generated.\n\n", psErrorLog->uNumProgramErrorMessages);
	}

	/* Space for sucess / failure message */
	uTotalStringSize = 20;

	/* Count total number of chars */
	for (i = 0; i < psErrorLog->uTotalNumErrorMessages; i++)
	{
		if (psErrorLog->sErrorMessages[i].eErrorType & eErrorTypes)
		{
			/**
			 * OGL64 Review.
			 * Use size_t for strlen?
			 */
			uTotalStringSize += (IMG_UINT32)strlen(psErrorLog->sErrorMessages[i].pszErrorMessageString);
		}
	}

	uTotalStringSize++;

	/* Allocate memory for info log string */
	psInfoLog->pszInfoLogString = DebugMemAlloc(uTotalStringSize);

	if(!psInfoLog->pszInfoLogString)
	{
		DEBUG_MESSAGE(("GLSLGenerateInfoLog: Could not allocate memory for the infolog.\n"));
		return;
	}

	psInfoLog->pszInfoLogString[0] = '\0';

	if (bSuccess)
	{
		strcat(psInfoLog->pszInfoLogString, "Success.\n");
	}
	else
	{
		strcat(psInfoLog->pszInfoLogString, "Compile failed.\n");
	}

	/* Loop over all error types, so all messages of a type are collected together in the output */
	for (j = 0; j < 3; j++)
	{
		IMG_UINT32 uErrorTypeToPrint = (1 << j);
		
		if(eErrorTypes & uErrorTypeToPrint)
		{
			/* Concatenate all error messages into one string */
			for (i = 0; i < psErrorLog->uTotalNumErrorMessages; i++)
			{
				if (psErrorLog->sErrorMessages[i].eErrorType & uErrorTypeToPrint)
				{
					strcat(psInfoLog->pszInfoLogString, psErrorLog->sErrorMessages[i].pszErrorMessageString);
				}
			}
		}
	}

	psInfoLog->uInfoLogStringLength = uTotalStringSize;
}


/******************************************************************************
 * Function Name: GLSLFreeInfoLog
 * Inputs       : psInfoLog
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Destroys the info log.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV GLSLFreeInfoLog(GLSLInfoLog *psInfoLog)
{
	if (psInfoLog)
	{
		if (psInfoLog->pszInfoLogString)
		{
			DebugMemFree(psInfoLog->pszInfoLogString);
		}
	}
}

/******************************************************************************
 * Function Name: GLSLDumpUniFlexProgram
 * Inputs       : pvCode, fstream
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 * Description  : Wrapper for DumpUniFlextInstData.
 *****************************************************************************/
IMG_INTERNAL IMG_VOID IMG_CALLCONV GLSLDumpUniFlexProgram(IMG_VOID *pvCode, FILE *fstream)
{
	DumpUniFlextInstData(fstream, pvCode);
}


/******************************************************************************
 * Function Name: GLSLCompileToIntermediateCode
 * Inputs       : psCompileProgramContext, 
 * Outputs      : ppsICProgram, peProgramFlags, psErrorLog
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Compiles a GLSL shader into intermediate code (UniFlex)
                  that can be further compiled into USSE machine code.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL GLSLCompileToIntermediateCode(GLSLCompileProgramContext  *psCompileProgramContext,
													GLSLICProgram             **ppsICProgram,
													GLSLProgramFlags		   *peProgramFlags,
													ErrorLog                   *psErrorLog)
{
	GLSLInitCompilerContext *psInitCompilerContext = psCompileProgramContext->psInitCompilerContext;
	IMG_CHAR               **ppszSourceCodeStrings = psCompileProgramContext->ppszSourceCodeStrings;
	IMG_UINT32               uNumSourceCodeStrings = psCompileProgramContext->uNumSourceCodeStrings;
	GLSLProgramType          eProgramType          = psCompileProgramContext->eProgramType;
	IMG_BOOL                 bCompleteProgram      = psCompileProgramContext->bCompleteProgram;
	IMG_BOOL                 bValidateOnly         = psCompileProgramContext->bValidateOnly;
	GLSLCompilerWarnings     eEnabledWarnings      = psCompileProgramContext->eEnabledWarnings;

	IMG_BOOL                 bSuccess              = IMG_TRUE;

	GLSLCompilerPrivateData *psCPD = IMG_NULL;
	ParseContext			*psParseContext		   = IMG_NULL;
	GLSLTreeContext			*psGLSLTreeContext	   = IMG_NULL;
	GLSLICProgram			*psICProgram		   = IMG_NULL;
	SymTable				*psSymbolTable		   = IMG_NULL;
	
	/* Get compile private data */
	psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	if (!psCPD)
	{
		DEBUG_MESSAGE(("GLSLCompileProgram: Compiler private data not initialised\n"));
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	psCPD->psErrorLog = psErrorLog;

	MetricsAccumCounter((IMG_VOID*)psCPD, METRICCOUNT_NUMCOMPILATIONS, 1);

	MetricStart((IMG_VOID*)psCPD, METRICS_TOTAL);

	if (!psInitCompilerContext || !psInitCompilerContext->bSuccessfulInit)
	{
		LOG_INTERNAL_ERROR(("GLSLCompileProgram: Compiler not initialised\n"));
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	if (!ppszSourceCodeStrings || !uNumSourceCodeStrings)
	{
		LogProgramError(psCPD->psErrorLog, "No source code supplied.\n");
	}

	MetricStart((IMG_VOID*)psCPD, METRICS_PARSECONTEXT);

	/* Parse the source file and generate a Parse Tree */
	psParseContext = CreateParseContext((IMG_VOID*)psCPD,
										ppszSourceCodeStrings,
										uNumSourceCodeStrings);
	
	MetricFinish((IMG_VOID*)psCPD, METRICS_PARSECONTEXT);	
	
	if (!psParseContext)
	{
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* Store some parsing stats */
	MetricsAccumCounter((IMG_VOID*)psCPD, METRICCOUNT_NUMTOKENS,                   psParseContext->uNumTokens);

	MetricStart((IMG_VOID*)psCPD, METRICS_AST);
	
	/* Add the built in state required by this program */
	if (!GLSLAddBuiltInState(psInitCompilerContext, psParseContext->psTokenList, eProgramType))
	{
		bSuccess = IMG_FALSE;
		MetricFinish((IMG_VOID*)psCPD, METRICS_AST);
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* Create a symbol table */
	if (eProgramType == GLSLPT_VERTEX)
	{
		psSymbolTable = CreateSymTable(psCPD->psSymbolTableContext, 
									   "Vertex Program",
									   250, 
									   GLSL_NUM_BITS_FOR_SYMBOL_IDS, 
									   psCPD->psVertexSymbolTable);
	}
	else
	{
		psSymbolTable = CreateSymTable(psCPD->psSymbolTableContext, 
									   "Fragment Program",
									   250, 
									   GLSL_NUM_BITS_FOR_SYMBOL_IDS, 
									   psCPD->psFragmentSymbolTable);
	}

	if (!psSymbolTable)
	{
		LOG_INTERNAL_ERROR(("GLSLCompileProgram: Failed to create symbol table"));
		bSuccess = IMG_FALSE;
		MetricFinish((IMG_VOID*)psCPD, METRICS_AST);
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* Generate the abstract syntax tree */
	psGLSLTreeContext = CreateGLSLTreeContext(psParseContext,
											  psSymbolTable,
											  eProgramType,
											  eEnabledWarnings,
											  psInitCompilerContext);

	MetricFinish((IMG_VOID*)psCPD, METRICS_AST);

	/* For bottom up rules, the top node of the tree is not set if there was a parse error. */
	if(!psGLSLTreeContext->psAbstractSyntaxTree)
	{
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* If all the source code for this shader was present then check for completeness */
	if (bCompleteProgram)
	{
		if (!CheckGLSLTreeCompleteness(psGLSLTreeContext))
		{
			bSuccess = IMG_FALSE;
			goto GLSLCompileToIntermediateCodeCleanUp;
		}
	}

	/* Check for any error messages */
	if (!psGLSLTreeContext                   ||
		psErrorLog->uNumProgramErrorMessages ||
		psErrorLog->uNumInternalErrorMessages)
	{
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	if (bValidateOnly)
	{
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* Destroy parse tree to conserve memory */
	MetricStart((IMG_VOID*)psCPD, METRICS_SHUTDOWN);

	MetricFinish((IMG_VOID*)psCPD, METRICS_SHUTDOWN);

	if(psGLSLTreeContext->bDiscardExecuted) 
	{
		*peProgramFlags |= GLSLPF_DISCARD_EXECUTED;
	}

	if(psGLSLTreeContext->eBuiltInsWrittenTo & GLSLBVWT_FRAGDEPTH)
	{
		*peProgramFlags |= GLSLPF_FRAGDEPTH_WRITTENTO;
	}

	MetricStart((IMG_VOID*)psCPD, METRICS_ICODE);

	/* generate intermediate program from abstract syntax tree */
	psICProgram = GenerateICodeProgram(psCPD, psGLSLTreeContext,
									   psSymbolTable,
									   psErrorLog);

	MetricFinish((IMG_VOID*)psCPD, METRICS_ICODE);

	if(!psICProgram)
	{
		LOG_INTERNAL_ERROR(("GLSLCompileProgram: Failed to generate intermediate code program"));
		bSuccess = IMG_FALSE;
		goto GLSLCompileToIntermediateCodeCleanUp;
	}

	/* Record number of instructions */
	MetricsAccumCounter((IMG_VOID*)psCPD, METRICCOUNT_NUMICINSTRUCTIONS, GetICInstructionCount(psICProgram));

GLSLCompileToIntermediateCodeCleanUp:

	MetricStart((IMG_VOID*)psCPD, METRICS_SHUTDOWN);

	if(psGLSLTreeContext)
	{
		DestroyGLSLTreeContext(psGLSLTreeContext);
		psGLSLTreeContext = IMG_NULL;
	}

	if(psParseContext)
	{
		PPDestroyPreProcessorData(psParseContext->pvPreProcessorData);
		DestroyParseContext(psParseContext);
		psParseContext = IMG_NULL;
	}

	if (psSymbolTable && (!bSuccess || bValidateOnly))
	{
		RemoveSymbolTableFromManager(psCPD->psSymbolTableContext, psSymbolTable);
		DestroySymTable(psSymbolTable);
	}

	MetricFinish((IMG_VOID*)psCPD, METRICS_SHUTDOWN);

	MetricFinish((IMG_VOID*)psCPD, METRICS_TOTAL);

	*ppsICProgram   = psICProgram;

	/* Double-check that if any errors have been generated then we report a failure */
	if(psErrorLog->uNumInternalErrorMessages != 0 ||
	   psErrorLog->uNumProgramErrorMessages != 0)
	{
		bSuccess = IMG_FALSE;
	}
	
	return bSuccess;
}


/******************************************************************************
 * Function Name: GLSLFreeIntermediateCode
 * Inputs       : psCompileProgramContext, psICProgram
 * Outputs      : -
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : Destroys some intermediate code previously generated by GLSLCompileToIntermediateCode.
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL GLSLFreeIntermediateCode(GLSLCompileProgramContext *psCompileProgramContext,
															GLSLICProgram             *psICProgram)
{
	GLSLInitCompilerContext *psInitCompilerContext = psCompileProgramContext->psInitCompilerContext;

	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)psInitCompilerContext->pvCompilerPrivateData;

	if (psICProgram)
	{
		/* Free the symbol table */
		if(psICProgram->psSymbolTable) 
		{
			RemoveSymbolTableFromManager(psCPD->psSymbolTableContext, psICProgram->psSymbolTable);
			DestroySymTable(psICProgram->psSymbolTable);
		}

		/* free the intermediate code */
		DestroyICodeProgram(psCPD, psICProgram);
	}

	return IMG_TRUE;
}

#ifndef GLSL_ES
/******************************************************************************
 * Function Name: GLSLGetLanguageVersion
 * Inputs       : uStringLength - maximum number of characters can be written to.
 * Outputs      : pszVersionString - the version string
 * Returns      : IMG_TRUE if successfull. IMG_FALSE otherwise.
 * Globals Used : -
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_BOOL IMG_CALLCONV GLSLGetLanguageVersion(IMG_UINT32 uStringLength, IMG_CHAR *pszVersionString)
{
#define MAX_LANGUAGE_VERSION_STRING_LENGTH 5
	if(uStringLength < MAX_LANGUAGE_VERSION_STRING_LENGTH )
	{
		DEBUG_MESSAGE(("GLSLGetCompilerVersion: String length is not enough. Minimum length is %d\n",
			MAX_LANGUAGE_VERSION_STRING_LENGTH));

		return IMG_FALSE;
	}

#ifdef GLSL_ES
	strncpy(pszVersionString, "1.00", uStringLength);
#else
		strncpy(pszVersionString, "1.20", uStringLength);
#endif

	return IMG_TRUE;
}

#endif
/******************************************************************************
 End of file (glsl.c)
******************************************************************************/
