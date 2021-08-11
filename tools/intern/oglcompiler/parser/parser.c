/******************************************************************************
 * Name         : parser.c
 * Author       : James McCarthy
 * Created      : 10/11/2003
 *
 * Copyright    : 2003-2007 by Imagination Technologies Limited.
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
 * $Log: parser.c $
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "debug.h"
#include "metrics.h"

#include "../glsl/error.h"
#include "../glsl/prepro.h"

/******************************************************************************
 * Function Name: CreateParseContext
 *
 * Inputs       : - LexRules Database, Syntax Table, Source Code, 
 *                  Starting SyntaxSymbol Name, Remove List, Remove List Size
 * Outputs      : -
 * Returns      : Parse context
 * Globals Used : -
 *
 * Description  : First it generates a list of preprocessing tokens based on the
 *                supplied source code. Second, this list of tokens is preprocessed.
 *                Third, whitespace is removed and keywords are detected. Finally,
 *                the list of syntactical tokens is parsed with the syntax grammar
 *                (SyntaxTable) and a parse tree is generated.
 *****************************************************************************/
IMG_INTERNAL ParseContext *CreateParseContext(IMG_VOID   *pvCompilerPrivateData,
											  IMG_CHAR	**ppszSourceCode,
											  IMG_UINT32  uNumSources)
{
	GLSLCompilerPrivateData *psCPD = (GLSLCompilerPrivateData *)pvCompilerPrivateData;
	ParseContext    *psParseContext;
	IMG_UINT32       i;

	psParseContext = DebugMemCalloc(sizeof(ParseContext));

	if (!psParseContext)
	{
		LOG_INTERNAL_TOKEN_ERROR((IMG_NULL,"CreateParseContext: Failed to alloc memory for parse context\n"));
		return IMG_NULL;
	}

	psParseContext->psCPD = psCPD;

	/* Alloc mem for source pointers */
	psParseContext->ppszSources = DebugMemAlloc(sizeof(IMG_CHAR *) * uNumSources);

	for (i = 0; i < uNumSources; i++)
	{
		psParseContext->uNumSources = i + 1;

		if (ppszSourceCode[i])
		{
			/* Make a copy of the source code */
			psParseContext->ppszSources[i] = DebugMemAlloc(strlen(ppszSourceCode[i]) + 1);

			if (!psParseContext->ppszSources[i])
			{
				LOG_INTERNAL_TOKEN_ERROR((IMG_NULL,"CreateParseContext: Failed to alloc memory for copy of source code\n"));
				DestroyParseContext(psParseContext);
				return IMG_NULL;
			}

			/* Copy source code */
			strcpy(psParseContext->ppszSources[i], ppszSourceCode[i]);
		}
		else
		{
			psParseContext->ppszSources[i] = IMG_NULL;
		}
	}

	MetricStart(pvCompilerPrivateData, METRICS_TOKENLIST);

	/* Generate a token list */
	psParseContext->psTokenList = LexCreateTokenList(pvCompilerPrivateData,
													 (const IMG_CHAR**)psParseContext->ppszSources,
													 uNumSources,
													 &psParseContext->uNumTokens,
													 &psParseContext->pvPreProcessorData);

	MetricFinish(pvCompilerPrivateData, METRICS_TOKENLIST);


	if (!psParseContext->psTokenList)
	{
		psParseContext->uNumTokens = 0;
		PPDestroyPreProcessorData(psParseContext->pvPreProcessorData);
		DestroyParseContext(psParseContext);
		return IMG_NULL;
	}

	MetricStart(pvCompilerPrivateData, METRICS_PARSETREE);

	MetricFinish(pvCompilerPrivateData, METRICS_PARSETREE);

	return psParseContext;
}


/******************************************************************************
 * Function Name: DestroyParseContext
 *
 * Inputs       : Parse Context
 * Outputs      : -
 * Returns      : -
 * Globals Used : -
 *
 * Description  : Frees memory associated with the parse context
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DestroyParseContext(ParseContext *psParseContext)
{
	IMG_UINT32 i;
#ifdef DUMP_LOGFILES
	DumpLogMessage(LOGFILE_PARSING_LOG, 0, "Destroying parse context\n");
#endif 
	if (psParseContext)
	{
		/* Free the linked list of allocs made by the bison rule handlers. */
		while(psParseContext->pvBisonAllocList)
		{
			IMG_VOID **psNext = psParseContext->pvBisonAllocList[0];

			DebugMemFree(psParseContext->pvBisonAllocList);

			psParseContext->pvBisonAllocList = psNext;
		}

		LexDestroyTokenList(psParseContext->psTokenList, psParseContext->uNumTokens);

		for (i = 0; i < psParseContext->uNumSources; i++)
		{
			if (psParseContext->ppszSources[i])
			{
				DebugMemFree(psParseContext->ppszSources[i]);
			}
		}

		DebugMemFree(psParseContext->ppszSources);

		DebugMemFree(psParseContext);
	}
}

/******************************************************************************
 End of file (parser.c)
******************************************************************************/
